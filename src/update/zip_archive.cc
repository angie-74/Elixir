// Modified by Elixir.
#include "update/zip_archive.h"

#include <minizip/unzip.h>
#if defined(_WIN32)
#include <windows.h>

#include "update/host_file_windows.h"
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace mocktail::update {
namespace {

struct ArchiveCloser {
  void operator()(void* archive) const {
    if (archive != nullptr) unzClose(archive);
  }
};

using Archive = std::unique_ptr<void, ArchiveCloser>;

#if defined(_WIN32)
std::string PathText(const std::filesystem::path& path) {
  return host_file::Utf8(path);
}
#else
std::string PathText(const std::filesystem::path& path) { return path.string(); }
#endif

constexpr std::uint32_t kEndRecordSignature = 0x06054b50U;
constexpr std::uint32_t kZip64LocatorSignature = 0x07064b50U;
constexpr std::uint64_t kEndRecordSize = 22;
constexpr std::uint64_t kZip64LocatorSize = 20;
constexpr std::uint64_t kMaximumComment = 0xffff;
constexpr std::uint64_t kMinizipSearchBytes = 0xffff;
constexpr std::uint64_t kSearchSlack = 8;

std::uint32_t LittleU32(const unsigned char* bytes) {
  return bytes[0] | static_cast<std::uint32_t>(bytes[1]) << 8U |
         static_cast<std::uint32_t>(bytes[2]) << 16U |
         static_cast<std::uint32_t>(bytes[3]) << 24U;
}

using ReadAt = std::function<bool(std::uint64_t offset, unsigned char* buffer,
                                  std::size_t size)>;

bool CheckEndRecords(std::uint64_t size, const ReadAt& read_at,
                     std::string* error) {
  if (size < kEndRecordSize) {
    *error = "ZIP archive has no valid end of central directory record";
    return false;
  }
  const std::uint64_t span = kMaximumComment + kEndRecordSize +
                             kZip64LocatorSize + kSearchSlack;
  const std::uint64_t tail_begin = size > span ? size - span : 0;
  std::vector<unsigned char> tail(static_cast<std::size_t>(size - tail_begin));
  if (!read_at(tail_begin, tail.data(), tail.size())) {
    *error = "cannot read ZIP end records";
    return false;
  }
  const auto at = [&](std::uint64_t position) {
    return tail.data() + static_cast<std::size_t>(position - tail_begin);
  };

  std::optional<std::uint64_t> strict;
  const std::uint64_t strict_low = size - kEndRecordSize > kMaximumComment
                                       ? size - kEndRecordSize - kMaximumComment
                                       : 0;
  for (std::uint64_t position = size - kEndRecordSize;; --position) {
    const unsigned char* record = at(position);
    if (LittleU32(record) == kEndRecordSignature &&
        position + kEndRecordSize + (record[20] | record[21] << 8U) == size) {
      strict = position;
      break;
    }
    if (position == strict_low) break;
  }
  if (!strict.has_value()) {
    *error = "ZIP archive has no valid end of central directory record";
    return false;
  }

  const std::uint64_t search_low = size > kMinizipSearchBytes + kSearchSlack
                                       ? size - kMinizipSearchBytes - kSearchSlack
                                       : 0;
  const auto last = [&](std::uint32_t signature) -> std::optional<std::uint64_t> {
    for (std::uint64_t position = size - 4;; --position) {
      if (LittleU32(at(position)) == signature) return position;
      if (position == search_low) break;
    }
    return std::nullopt;
  };
  if (last(kEndRecordSignature) != strict) {
    *error =
        "ZIP archive has more than one end of central directory record "
        "candidate";
    return false;
  }
  const std::optional<std::uint64_t> locator = last(kZip64LocatorSignature);
  if (locator.has_value()) {
    const bool in_place = *strict >= kZip64LocatorSize &&
                          *locator == *strict - kZip64LocatorSize;
    const bool followed = *locator + kZip64LocatorSize <= size &&
                          LittleU32(at(*locator + 4)) == 0 &&
                          LittleU32(at(*locator + 16)) == 1;
    if (followed && !in_place) {
      *error =
          "ZIP archive has a ZIP64 end of central directory locator outside "
          "its end records";
      return false;
    }
  }
  return true;
}

#if defined(_WIN32)
struct HeldStream final {
  HANDLE handle = nullptr;
  int error = 0;
};

voidpf ZCALLBACK HeldOpen(voidpf, const void* stream, int mode) {
  if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER) != ZLIB_FILEFUNC_MODE_READ) {
    return nullptr;
  }
  return const_cast<void*>(stream);
}

uLong ZCALLBACK HeldRead(voidpf, voidpf stream, void* buffer, uLong size) {
  auto* held = static_cast<HeldStream*>(stream);
  DWORD read = 0;
  if (ReadFile(held->handle, buffer, size, &read, nullptr) == 0) {
    const DWORD code = GetLastError();
    if (code != ERROR_HANDLE_EOF) held->error = static_cast<int>(code);
    return 0;
  }
  return read;
}

uLong ZCALLBACK HeldWrite(voidpf, voidpf, const void*, uLong) { return 0; }

ZPOS64_T ZCALLBACK HeldTell(voidpf, voidpf stream) {
  auto* held = static_cast<HeldStream*>(stream);
  LARGE_INTEGER zero = {};
  LARGE_INTEGER position = {};
  if (SetFilePointerEx(held->handle, zero, &position, FILE_CURRENT) == 0) {
    held->error = static_cast<int>(GetLastError());
    return static_cast<ZPOS64_T>(-1);
  }
  return static_cast<ZPOS64_T>(position.QuadPart);
}

long ZCALLBACK HeldSeek(voidpf, voidpf stream, ZPOS64_T offset, int origin) {
  auto* held = static_cast<HeldStream*>(stream);
  DWORD method = FILE_BEGIN;
  switch (origin) {
    case ZLIB_FILEFUNC_SEEK_SET:
      method = FILE_BEGIN;
      break;
    case ZLIB_FILEFUNC_SEEK_CUR:
      method = FILE_CURRENT;
      break;
    case ZLIB_FILEFUNC_SEEK_END:
      method = FILE_END;
      break;
    default:
      return -1;
  }
  LARGE_INTEGER distance = {};
  distance.QuadPart = static_cast<LONGLONG>(offset);
  if (SetFilePointerEx(held->handle, distance, nullptr, method) == 0) {
    held->error = static_cast<int>(GetLastError());
    return -1;
  }
  return 0;
}

int ZCALLBACK HeldClose(voidpf, voidpf) { return 0; }

int ZCALLBACK HeldError(voidpf, voidpf stream) {
  return static_cast<HeldStream*>(stream)->error;
}
#endif

class OpenArchive final {
 public:
  void* handle() const { return archive_.get(); }

 private:
  friend std::unique_ptr<OpenArchive> Open(const std::filesystem::path& path,
                                           std::string* error);
#if defined(_WIN32)
  host_file::ReadOnlyFile file_;
  HeldStream stream_;
#endif
  Archive archive_;
};

std::unique_ptr<OpenArchive> Open(const std::filesystem::path& path,
                                  std::string* error) {
  std::error_code filesystem_error;
  const std::filesystem::file_status status =
      std::filesystem::symlink_status(path, filesystem_error);
  if (filesystem_error || !std::filesystem::is_regular_file(status) ||
      std::filesystem::is_symlink(status)) {
    *error = "ZIP archive is not a regular file: " + PathText(path);
    return nullptr;
  }
  auto opened = std::make_unique<OpenArchive>();
#if defined(_WIN32)
  std::uint64_t size = 0;
  if (opened->file_.Open(path, host_file::ReadOnlyFile::Sharing::kReadOnly) !=
          host_file::OpenStatus::kOk ||
      !opened->file_.Size(&size)) {
    *error = "cannot open ZIP archive: " + PathText(path);
    return nullptr;
  }
  if (!CheckEndRecords(
          size,
          [&](std::uint64_t offset, unsigned char* buffer, std::size_t bytes) {
            return opened->file_.ReadAt(offset, buffer, bytes);
          },
          error)) {
    *error += ": " + PathText(path);
    return nullptr;
  }
  opened->stream_.handle = static_cast<HANDLE>(opened->file_.native_handle());
  zlib_filefunc64_def functions = {};
  functions.zopen64_file = HeldOpen;
  functions.zread_file = HeldRead;
  functions.zwrite_file = HeldWrite;
  functions.ztell64_file = HeldTell;
  functions.zseek64_file = HeldSeek;
  functions.zclose_file = HeldClose;
  functions.zerror_file = HeldError;
  opened->archive_.reset(unzOpen2_64(&opened->stream_, &functions));
#else
  const int descriptor = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  struct stat metadata = {};
  if (descriptor < 0 || fstat(descriptor, &metadata) != 0 ||
      !S_ISREG(metadata.st_mode) || metadata.st_size < 0) {
    if (descriptor >= 0) close(descriptor);
    *error = "cannot open ZIP archive: " + PathText(path);
    return nullptr;
  }
  const bool consistent = CheckEndRecords(
      static_cast<std::uint64_t>(metadata.st_size),
      [&](std::uint64_t offset, unsigned char* buffer, std::size_t bytes) {
        while (bytes > 0) {
          const ssize_t count = pread(descriptor, buffer, bytes,
                                      static_cast<off_t>(offset));
          if (count < 0 && errno == EINTR) continue;
          if (count <= 0) return false;
          buffer += count;
          bytes -= static_cast<std::size_t>(count);
          offset += static_cast<std::uint64_t>(count);
        }
        return true;
      },
      error);
  close(descriptor);
  if (!consistent) {
    *error += ": " + PathText(path);
    return nullptr;
  }
  opened->archive_.reset(unzOpen64(path.c_str()));
#endif
  if (!opened->archive_) {
    *error = "cannot open ZIP archive: " + PathText(path);
    return nullptr;
  }
  return opened;
}

bool CurrentInfo(void* archive, ZipEntry* entry, std::string* error) {
  unz_file_info64 info = {};
  if (unzGetCurrentFileInfo64(archive, &info, nullptr, 0, nullptr, 0, nullptr,
                              0) != UNZ_OK ||
      info.size_filename == 0 || info.size_filename > 64U * 1024U) {
    *error = "cannot read ZIP entry metadata";
    return false;
  }
  std::vector<char> name(static_cast<std::size_t>(info.size_filename) + 1U);
  if (unzGetCurrentFileInfo64(archive, &info, name.data(), name.size(), nullptr,
                              0, nullptr, 0) != UNZ_OK) {
    *error = "cannot read ZIP entry name";
    return false;
  }
  entry->name.assign(name.data(), static_cast<std::size_t>(info.size_filename));
  entry->uncompressed_size = info.uncompressed_size;
  entry->directory = !entry->name.empty() && entry->name.back() == '/';
#if defined(_WIN32)
  constexpr std::uint32_t kUnixTypeMask = 0170000U;
  constexpr std::uint32_t kUnixSymbolicLink = 0120000U;
  const std::uint32_t unix_mode =
      static_cast<std::uint32_t>((info.external_fa >> 16U) & 0xffffU);
  entry->symbolic_link = (unix_mode & kUnixTypeMask) == kUnixSymbolicLink;
#else
  const mode_t unix_mode =
      static_cast<mode_t>((info.external_fa >> 16U) & 0xffffU);
  entry->symbolic_link = (unix_mode & S_IFMT) == S_IFLNK;
#endif
  return true;
}

bool First(void* archive, std::string* error) {
  const int status = unzGoToFirstFile(archive);
  if (status != UNZ_OK) *error = "cannot enumerate ZIP archive";
  return status == UNZ_OK;
}

#if defined(_WIN32)
bool WriteCurrent(void* archive, host_file::NewFile* output, std::size_t maximum,
                  std::size_t* written, std::string* error) {
  if (unzOpenCurrentFile(archive) != UNZ_OK) {
    *error = "cannot open compressed ZIP entry";
    return false;
  }
  std::vector<char> buffer(128U * 1024U);
  bool success = true;
  while (success) {
    const int bytes = unzReadCurrentFile(
        archive, buffer.data(), static_cast<unsigned>(buffer.size()));
    if (bytes == 0) break;
    if (bytes < 0) {
      *error = "cannot decompress ZIP entry";
      success = false;
      break;
    }
    const std::size_t chunk = static_cast<std::size_t>(bytes);
    if (chunk > maximum || *written > maximum - chunk) {
      *error = "decompressed ZIP data exceeds its size limit";
      success = false;
      break;
    }
    if (!output->WriteAll(buffer.data(), chunk)) {
      *error = "cannot write decompressed ZIP entry: " +
               host_file::ErrorText(output->last_error());
      success = false;
      break;
    }
    *written += chunk;
  }
  const int close_status = unzCloseCurrentFile(archive);
  if (success && close_status != UNZ_OK) {
    *error = "ZIP entry failed its CRC check";
    success = false;
  }
  return success;
}
#else
bool WriteCurrent(void* archive, int descriptor, std::size_t maximum,
                  std::size_t* written, std::string* error) {
  if (unzOpenCurrentFile(archive) != UNZ_OK) {
    *error = "cannot open compressed ZIP entry";
    return false;
  }
  std::array<char, 128U * 1024U> buffer{};
  bool success = true;
  while (success) {
    const int bytes = unzReadCurrentFile(archive, buffer.data(), buffer.size());
    if (bytes == 0) break;
    if (bytes < 0) {
      *error = "cannot decompress ZIP entry";
      success = false;
      break;
    }
    const std::size_t chunk = static_cast<std::size_t>(bytes);
    if (chunk > maximum || *written > maximum - chunk) {
      *error = "decompressed ZIP data exceeds its size limit";
      success = false;
      break;
    }
    std::size_t offset = 0;
    while (offset < chunk) {
      const ssize_t output =
          write(descriptor, buffer.data() + offset, chunk - offset);
      if (output < 0) {
        if (errno == EINTR) continue;
        *error = "cannot write decompressed ZIP entry: " +
                 std::string(std::strerror(errno));
        success = false;
        break;
      }
      offset += static_cast<std::size_t>(output);
    }
    *written += chunk;
  }
  const int close_status = unzCloseCurrentFile(archive);
  if (success && close_status != UNZ_OK) {
    *error = "ZIP entry failed its CRC check";
    success = false;
  }
  return success;
}
#endif

bool Locate(void* archive, std::string_view entry, ZipEntry* metadata,
            std::string* error) {
  if (entry.empty() || entry.find('\0') != std::string_view::npos ||
      unzLocateFile(archive, std::string(entry).c_str(), 1) != UNZ_OK) {
    *error = "ZIP entry is missing: " + std::string(entry);
    return false;
  }
  if (!CurrentInfo(archive, metadata, error)) return false;
  if (metadata->name != entry) {
    *error = "ZIP entry name is ambiguous: " + std::string(entry);
    return false;
  }
  return true;
}

#if defined(_WIN32)
bool AsciiEqualIgnoreCase(std::string_view left, std::string_view right) {
  if (left.size() != right.size()) return false;
  for (std::size_t index = 0; index < left.size(); ++index) {
    char first = left[index];
    char second = right[index];
    if (first >= 'a' && first <= 'z') first = static_cast<char>(first - 'a' + 'A');
    if (second >= 'a' && second <= 'z') second = static_cast<char>(second - 'a' + 'A');
    if (first != second) return false;
  }
  return true;
}

bool IsReservedDeviceName(std::string_view component) {
  std::string_view stem = component.substr(0, component.find('.'));
  while (!stem.empty() && stem.back() == ' ') stem.remove_suffix(1);
  static constexpr std::string_view kNames[] = {
      "CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$", "CLOCK$"};
  for (const std::string_view name : kNames) {
    if (AsciiEqualIgnoreCase(stem, name)) return true;
  }
  if (stem.size() >= 4 && (AsciiEqualIgnoreCase(stem.substr(0, 3), "COM") ||
                           AsciiEqualIgnoreCase(stem.substr(0, 3), "LPT"))) {
    const std::string_view suffix = stem.substr(3);
    if (suffix.size() == 1 && suffix[0] >= '0' && suffix[0] <= '9') return true;
    if (suffix == "\xc2\xb9" || suffix == "\xc2\xb2" || suffix == "\xc2\xb3") {
      return true;
    }
  }
  return false;
}

std::wstring CaseFoldKey(const std::filesystem::path& path) {
  std::wstring key = path.generic_wstring();
  if (!key.empty()) {
    const int size = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE,
                                   key.data(), static_cast<int>(key.size()),
                                   nullptr, 0, nullptr, nullptr, 0);
    if (size > 0) {
      std::wstring upper(static_cast<std::size_t>(size), L'\0');
      LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, key.data(),
                    static_cast<int>(key.size()), upper.data(), size, nullptr,
                    nullptr, 0);
      key = std::move(upper);
    }
  }
  return key;
}
#endif

}

bool IsSafeArchivePath(std::string_view path) {
  if (path.empty() || path.front() == '/' || path.find('\0') != path.npos ||
      path.find('\\') != path.npos) {
    return false;
  }
  std::size_t begin = 0;
  while (begin < path.size()) {
    std::size_t end = path.find('/', begin);
    if (end == path.npos) end = path.size();
    const std::string_view component = path.substr(begin, end - begin);
    if (component.empty() || component == "." || component == "..") {
      return end == path.size() && component.empty();
    }
    begin = end + 1;
  }
  return true;
}

#if defined(_WIN32)
bool IsSafeWindowsExtractionPath(std::string_view path) {
  if (!IsSafeArchivePath(path) || !host_file::IsValidUtf8(path)) return false;
  std::size_t begin = 0;
  while (begin < path.size()) {
    std::size_t end = path.find('/', begin);
    if (end == path.npos) end = path.size();
    const std::string_view component = path.substr(begin, end - begin);
    begin = end + 1;
    if (component.empty()) continue;
    for (const char character : component) {
      const unsigned char value = static_cast<unsigned char>(character);
      if (value < 0x20U || std::strchr("<>:\"|?*", character) != nullptr) {
        return false;
      }
    }
    if (component.back() == '.' || component.back() == ' ') return false;
    if (IsReservedDeviceName(component)) return false;
  }
  return true;
}
#endif

ZipListResult ListZipEntries(const std::filesystem::path& archive_path,
                             std::size_t maximum_entries) {
  ZipListResult result;
  const std::unique_ptr<OpenArchive> archive = Open(archive_path, &result.error);
  if (!archive) return result;
  unz_global_info64 global = {};
  if (unzGetGlobalInfo64(archive->handle(), &global) != UNZ_OK ||
      global.number_entry > maximum_entries) {
    result.error = "ZIP archive has an invalid or excessive entry count";
    return result;
  }
  if (global.number_entry == 0) return result;
  if (!First(archive->handle(), &result.error)) return result;
  std::set<std::string> names;
  for (std::uint64_t index = 0; index < global.number_entry; ++index) {
    ZipEntry entry;
    if (!CurrentInfo(archive->handle(), &entry, &result.error)) return result;
    if (!IsSafeArchivePath(entry.name) || entry.symbolic_link) {
      result.error = "ZIP archive contains an unsafe entry: " + entry.name;
      return result;
    }
    if (!names.insert(entry.name).second) {
      result.error = "ZIP archive contains a duplicate entry: " + entry.name;
      return result;
    }
    result.entries.push_back(std::move(entry));
    if (index + 1 < global.number_entry &&
        unzGoToNextFile(archive->handle()) != UNZ_OK) {
      result.error = "ZIP archive ended before its declared entry count";
      return result;
    }
  }
  return result;
}

ZipReadResult ReadZipEntry(const std::filesystem::path& archive_path,
                           std::string_view entry, std::size_t maximum_bytes) {
  ZipReadResult result;
  const std::unique_ptr<OpenArchive> archive = Open(archive_path, &result.error);
  if (!archive) return result;
  ZipEntry metadata;
  if (!Locate(archive->handle(), entry, &metadata, &result.error) ||
      metadata.directory || metadata.symbolic_link ||
      metadata.uncompressed_size > maximum_bytes) {
    if (result.error.empty())
      result.error = "ZIP entry is invalid or too large";
    return result;
  }
  if (unzOpenCurrentFile(archive->handle()) != UNZ_OK) {
    result.error = "cannot open compressed ZIP entry";
    return result;
  }
  result.bytes.reserve(static_cast<std::size_t>(metadata.uncompressed_size));
  std::array<char, 64U * 1024U> buffer{};
  while (true) {
    const int bytes =
        unzReadCurrentFile(archive->handle(), buffer.data(), buffer.size());
    if (bytes == 0) break;
    if (bytes < 0 || static_cast<std::size_t>(bytes) > maximum_bytes ||
        result.bytes.size() > maximum_bytes - static_cast<std::size_t>(bytes)) {
      unzCloseCurrentFile(archive->handle());
      result.bytes.clear();
      result.error = bytes < 0 ? "cannot decompress ZIP entry"
                               : "ZIP entry exceeds its size limit";
      return result;
    }
    result.bytes.append(buffer.data(), static_cast<std::size_t>(bytes));
  }
  if (unzCloseCurrentFile(archive->handle()) != UNZ_OK) {
    result.bytes.clear();
    result.error = "ZIP entry failed its CRC check";
  }
  return result;
}

bool ExtractZipEntry(const std::filesystem::path& archive_path,
                     std::string_view entry,
                     const std::filesystem::path& destination,
                     std::size_t maximum_bytes, std::string* error) {
  const std::unique_ptr<OpenArchive> archive = Open(archive_path, error);
  if (!archive) return false;
  ZipEntry metadata;
  if (!Locate(archive->handle(), entry, &metadata, error) || metadata.directory ||
      metadata.symbolic_link || metadata.uncompressed_size > maximum_bytes) {
    if (error->empty()) *error = "ZIP entry is invalid or too large";
    return false;
  }
  std::error_code filesystem_error;
  std::filesystem::create_directories(destination.parent_path(),
                                      filesystem_error);
  if (filesystem_error) {
    *error = "cannot create ZIP extraction directory";
    return false;
  }
#if defined(_WIN32)
  host_file::NewFile output;
  if (!output.Create(destination)) {
    *error = "cannot create extracted ZIP entry";
    return false;
  }
  std::size_t written = 0;
  const bool extracted =
      WriteCurrent(archive->handle(), &output, maximum_bytes, &written, error);
  const bool synced = extracted && output.Sync();
  output.Close();
#else
  const int descriptor =
      open(destination.c_str(),
           O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
  if (descriptor < 0) {
    *error = "cannot create extracted ZIP entry";
    return false;
  }
  std::size_t written = 0;
  const bool extracted =
      WriteCurrent(archive->handle(), descriptor, maximum_bytes, &written, error);
  const bool synced = fsync(descriptor) == 0;
  close(descriptor);
#endif
  if (!extracted || !synced || written != metadata.uncompressed_size) {
    std::filesystem::remove(destination, filesystem_error);
    if (extracted && (!synced || written != metadata.uncompressed_size)) {
      *error = "extracted ZIP entry is truncated";
    }
    return false;
  }
  return true;
}

bool ExtractZipPrefix(const std::filesystem::path& archive_path,
                      std::string_view prefix,
                      const std::filesystem::path& destination,
                      std::size_t maximum_total_bytes,
                      std::size_t* extracted_files, std::string* error) {
  const ZipListResult listed = ListZipEntries(archive_path);
  if (!listed) {
    *error = listed.error;
    return false;
  }
  const std::unique_ptr<OpenArchive> archive = Open(archive_path, error);
  if (!archive) return false;
  if (!listed.entries.empty() && !First(archive->handle(), error)) return false;
  std::size_t total = 0;
  std::size_t files = 0;
#if defined(_WIN32)
  std::set<std::wstring> materialised;
#endif
  for (std::size_t index = 0; index < listed.entries.size(); ++index) {
    ZipEntry current;
    if (!CurrentInfo(archive->handle(), &current, error)) return false;
    const ZipEntry& entry = listed.entries[index];
    if (current.name != entry.name ||
        current.uncompressed_size != entry.uncompressed_size ||
        current.directory != entry.directory ||
        current.symbolic_link != entry.symbolic_link) {
      *error = "ZIP archive changed during extraction";
      return false;
    }
    const auto advance = [&]() {
      if (index + 1 >= listed.entries.size()) return true;
      if (unzGoToNextFile(archive->handle()) == UNZ_OK) return true;
      *error = "ZIP archive ended during extraction";
      return false;
    };
    if (entry.name.substr(0, prefix.size()) != prefix) {
      if (!advance()) return false;
      continue;
    }
    const std::string_view relative(entry.name.data() + prefix.size(),
                                    entry.name.size() - prefix.size());
    if (relative.empty()) {
      if (!advance()) return false;
      continue;
    }
    if (!IsSafeArchivePath(relative) || entry.symbolic_link ||
        entry.uncompressed_size > maximum_total_bytes ||
        total > maximum_total_bytes - entry.uncompressed_size) {
      *error = "ZIP prefix contains an unsafe or oversized entry";
      return false;
    }
#if defined(_WIN32)
    std::filesystem::path relative_path;
    if (!IsSafeWindowsExtractionPath(relative) ||
        !host_file::PathFromUtf8(relative, &relative_path)) {
      *error = "ZIP prefix contains an entry Windows cannot store: " +
               entry.name;
      return false;
    }
    const std::filesystem::path output = destination / relative_path;
    if (!entry.directory &&
        !materialised.insert(CaseFoldKey(relative_path)).second) {
      *error = "ZIP prefix contains entries that collide on a "
               "case-insensitive file system: " + entry.name;
      return false;
    }
#else
    const std::filesystem::path output = destination / std::string(relative);
#endif
    std::error_code filesystem_error;
    if (entry.directory) {
      std::filesystem::create_directories(output, filesystem_error);
      if (filesystem_error) {
        *error = "cannot create extracted ZIP directory";
        return false;
      }
    } else {
      std::filesystem::create_directories(output.parent_path(),
                                          filesystem_error);
      if (filesystem_error) {
        *error = "cannot create extracted ZIP directory";
        return false;
      }
#if defined(_WIN32)
      host_file::NewFile file;
      if (!file.Create(output)) {
        *error = "cannot create extracted ZIP entry";
        return false;
      }
      std::size_t written = 0;
      const bool extracted = WriteCurrent(
          archive->handle(), &file,
          static_cast<std::size_t>(entry.uncompressed_size), &written, error);
      file.Close();
#else
      const int descriptor =
          open(output.c_str(),
               O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
      if (descriptor < 0) {
        *error = "cannot create extracted ZIP entry";
        return false;
      }
      std::size_t written = 0;
      const bool extracted = WriteCurrent(
          archive->handle(), descriptor,
          static_cast<std::size_t>(entry.uncompressed_size), &written, error);
      close(descriptor);
#endif
      if (!extracted || written != entry.uncompressed_size) {
        std::filesystem::remove(output, filesystem_error);
        if (extracted && written != entry.uncompressed_size) {
          *error = "extracted ZIP entry is truncated";
        }
        return false;
      }
      total += static_cast<std::size_t>(entry.uncompressed_size);
      ++files;
    }
    if (!advance()) return false;
  }
  if (extracted_files != nullptr) *extracted_files = files;
  return true;
}

}
