// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Ported from Mocktail stubs/libandroid_stub.cc (Apache-2.0, Copyright 2026
// Mocktail Project Authors). Modified by Elixir.
#include <windows.h>

#include <zlib.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "android/android_internal.h"
#include "bionic/runtime.h"
#include "bionic/vfs.h"
#include "libc_shim/vulkan_etc1_sky_transcoder.h"

namespace guest = elixir::bionic::guest;

namespace elixir::android {
namespace {

std::wstring Widen(std::string_view utf8) {
  if (utf8.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()),
                                         nullptr, 0);
  if (length <= 0) return {};
  std::wstring wide(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
  return wide;
}

std::uint16_t Le16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | p[1] << 8); }
std::uint32_t Le32(const std::uint8_t* p) {
  return p[0] | static_cast<std::uint32_t>(p[1]) << 8 | static_cast<std::uint32_t>(p[2]) << 16 |
         static_cast<std::uint32_t>(p[3]) << 24;
}

class MappedFile {
 public:
  static std::shared_ptr<MappedFile> Open(const std::wstring& path, std::string* error) {
    auto file = std::shared_ptr<MappedFile>(new MappedFile());
    file->handle_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr);
    if (file->handle_ == INVALID_HANDLE_VALUE) {
      if (error != nullptr) *error = "cannot open (Win32 error " + std::to_string(GetLastError()) + ")";
      return nullptr;
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file->handle_, &size)) {
      if (error != nullptr) *error = "cannot query the size";
      return nullptr;
    }
    file->size_ = static_cast<std::uint64_t>(size.QuadPart);
    if (file->size_ == 0) return file;
    file->mapping_ = CreateFileMappingW(file->handle_, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (file->mapping_ != nullptr) {
      file->view_ = static_cast<const std::uint8_t*>(MapViewOfFile(file->mapping_, FILE_MAP_READ, 0, 0, 0));
    }
    if (file->view_ == nullptr) {
      if (error != nullptr) *error = "cannot map (Win32 error " + std::to_string(GetLastError()) + ")";
      return nullptr;
    }
    return file;
  }

  ~MappedFile() {
    if (view_ != nullptr) UnmapViewOfFile(view_);
    if (mapping_ != nullptr) CloseHandle(mapping_);
    if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
  }

  const std::uint8_t* data() const { return view_; }
  std::uint64_t size() const { return size_; }

 private:
  MappedFile() = default;
  HANDLE handle_ = INVALID_HANDLE_VALUE;
  HANDLE mapping_ = nullptr;
  const std::uint8_t* view_ = nullptr;
  std::uint64_t size_ = 0;
};

struct ZipEntry {
  std::uint64_t local_offset = 0;
  std::uint64_t data_offset = 0;
  std::uint32_t compressed_size = 0;
  std::uint32_t uncompressed_size = 0;
  std::uint32_t crc32 = 0;
  std::uint16_t method = 0;
  bool usable = false;
};

class ZipIndex {
 public:
  static std::shared_ptr<ZipIndex> Open(const std::string& utf8_path, std::string* error) {
    const std::wstring path = Widen(utf8_path);
    if (path.empty()) {
      *error = "invalid path";
      return nullptr;
    }
    auto index = std::shared_ptr<ZipIndex>(new ZipIndex());
    index->path_ = path;
    index->file_ = MappedFile::Open(path, error);
    if (index->file_ == nullptr) return nullptr;
    if (!index->Build(error)) return nullptr;
    return index;
  }

  const ZipEntry* Find(const std::string& name) const {
    const auto it = entries_.find(name);
    return it != entries_.end() && it->second.usable ? &it->second : nullptr;
  }

  const std::uint8_t* Data(const ZipEntry& entry) const { return file_->data() + entry.data_offset; }

  bool Extract(const ZipEntry& entry, std::vector<std::uint8_t>* out) const {
    out->resize(entry.uncompressed_size);
    if (entry.method == 0) {
      std::memcpy(out->data(), Data(entry), entry.uncompressed_size);
    } else {
      z_stream stream{};
      if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) return false;
      stream.next_in = const_cast<Bytef*>(Data(entry));
      stream.avail_in = entry.compressed_size;
      stream.next_out = out->data();
      stream.avail_out = entry.uncompressed_size;
      const int status = inflate(&stream, Z_FINISH);
      const bool complete = status == Z_STREAM_END && stream.total_out == entry.uncompressed_size;
      inflateEnd(&stream);
      if (!complete) return false;
    }
    return crc32(0L, out->data(), static_cast<uInt>(out->size())) == entry.crc32;
  }

  void ListFiles(const std::string& prefix, std::set<std::string>* names) const {
    for (const auto& [name, entry] : entries_) {
      if (!entry.usable || name.size() <= prefix.size() || name.compare(0, prefix.size(), prefix) != 0) continue;
      const std::string_view rest(name.data() + prefix.size(), name.size() - prefix.size());
      if (rest.find('/') == std::string_view::npos) names->insert(std::string(rest));
    }
  }

  const std::wstring& path() const { return path_; }
  const std::shared_ptr<MappedFile>& file() const { return file_; }

 private:
  ZipIndex() = default;

  bool Build(std::string* error) {
    constexpr std::uint64_t kEndRecordSize = 22;
    constexpr std::uint64_t kMaximumComment = 0xffff;
    const std::uint8_t* bytes = file_->data();
    const std::uint64_t size = file_->size();
    if (bytes == nullptr || size < kEndRecordSize) {
      *error = "not a ZIP archive (no end of central directory record)";
      return false;
    }
    const std::uint64_t low = size - kEndRecordSize > kMaximumComment ? size - kEndRecordSize - kMaximumComment : 0;
    std::uint64_t end_record = UINT64_MAX;
    for (std::uint64_t position = size - kEndRecordSize;; --position) {
      const std::uint8_t* record = bytes + position;
      if (Le32(record) == 0x06054b50U && position + kEndRecordSize + Le16(record + 20) == size) {
        end_record = position;
        break;
      }
      if (position == low) break;
    }
    if (end_record == UINT64_MAX) {
      *error = "not a ZIP archive (no end of central directory record)";
      return false;
    }
    const std::uint8_t* eocd = bytes + end_record;
    const std::uint16_t disk = Le16(eocd + 4);
    const std::uint16_t cd_disk = Le16(eocd + 6);
    const std::uint16_t disk_entries = Le16(eocd + 8);
    const std::uint16_t total_entries = Le16(eocd + 10);
    const std::uint32_t cd_size = Le32(eocd + 12);
    const std::uint32_t cd_offset = Le32(eocd + 16);
    if (total_entries == 0xffff || cd_size == 0xffffffffU || cd_offset == 0xffffffffU) {
      *error = "ZIP64 archives are not supported";
      return false;
    }
    if (disk != 0 || cd_disk != 0 || disk_entries != total_entries) {
      *error = "multi-disk ZIP archives are not supported";
      return false;
    }
    if (static_cast<std::uint64_t>(cd_offset) + cd_size > end_record) {
      *error = "central directory lies outside the archive";
      return false;
    }
    std::uint64_t position = cd_offset;
    const std::uint64_t cd_end = static_cast<std::uint64_t>(cd_offset) + cd_size;
    entries_.reserve(total_entries);
    for (std::uint32_t i = 0; i < total_entries; ++i) {
      if (position + 46 > cd_end || Le32(bytes + position) != 0x02014b50U) {
        *error = "corrupt central directory entry " + std::to_string(i);
        return false;
      }
      const std::uint8_t* header = bytes + position;
      const std::uint16_t flags = Le16(header + 8);
      const std::uint16_t method = Le16(header + 10);
      const std::uint16_t name_length = Le16(header + 28);
      const std::uint16_t extra_length = Le16(header + 30);
      const std::uint16_t comment_length = Le16(header + 32);
      const std::uint64_t next = position + 46 + name_length + extra_length + comment_length;
      if (next > cd_end) {
        *error = "corrupt central directory entry " + std::to_string(i);
        return false;
      }
      ZipEntry entry;
      entry.crc32 = Le32(header + 16);
      entry.compressed_size = Le32(header + 20);
      entry.uncompressed_size = Le32(header + 24);
      entry.local_offset = Le32(header + 42);
      entry.method = method;
      if (entry.compressed_size == 0xffffffffU || entry.uncompressed_size == 0xffffffffU ||
          entry.local_offset == 0xffffffffU) {
        *error = "ZIP64 entries are not supported";
        return false;
      }
      std::string name(reinterpret_cast<const char*>(header + 46), name_length);
      entry.usable = (flags & 1U) == 0 && (method == 0 || method == 8) && ResolveLocal(name, &entry, cd_offset);
      if (!entries_.emplace(std::move(name), entry).second) {
        *error = "duplicate entry name in archive";
        return false;
      }
      position = next;
    }
    return true;
  }

  bool ResolveLocal(const std::string& name, ZipEntry* entry, std::uint64_t cd_offset) const {
    const std::uint8_t* bytes = file_->data();
    if (entry->local_offset + 30 > cd_offset || Le32(bytes + entry->local_offset) != 0x04034b50U) return false;
    const std::uint8_t* local = bytes + entry->local_offset;
    const std::uint16_t name_length = Le16(local + 26);
    const std::uint16_t extra_length = Le16(local + 28);
    if (name_length != name.size() || entry->local_offset + 30 + name_length > cd_offset ||
        std::memcmp(local + 30, name.data(), name.size()) != 0 || Le16(local + 8) != entry->method) {
      return false;
    }
    entry->data_offset = entry->local_offset + 30 + name_length + extra_length;
    if (entry->data_offset + entry->compressed_size > cd_offset) return false;
    if (entry->method == 0 && entry->compressed_size != entry->uncompressed_size) return false;
    return true;
  }

  std::wstring path_;
  std::shared_ptr<MappedFile> file_;
  std::unordered_map<std::string, ZipEntry> entries_;
};

struct Sources {
  std::vector<std::string> directories;
  std::vector<std::shared_ptr<ZipIndex>> archives;
  bool transcode = true;
  std::string overlay_directory;
  bool (*overlay_filter)(std::string_view name) = nullptr;
};

struct SourceState {
  std::mutex mutex;
  std::shared_ptr<const Sources> sources = std::make_shared<Sources>();
};
SourceState& GetSourceState() {
  static SourceState* state = new SourceState();
  return *state;
}

std::shared_ptr<const Sources> CurrentSources() {
  SourceState& state = GetSourceState();
  std::lock_guard<std::mutex> lock(state.mutex);
  return state.sources;
}

std::string_view StripAssetUriPrefix(std::string_view path) {
  static constexpr const char* kPrefixes[] = {"file:///android_asset/", "rbxasset://"};
  for (const char* prefix : kPrefixes) {
    const std::size_t length = std::strlen(prefix);
    if (path.size() >= length && path.substr(0, length) == prefix) return path.substr(length);
  }
  return path;
}

bool HasUnsafePathSegment(std::string_view path) {
  if (path.empty() || path[0] == '/') return true;
  if (path.find('\\') != std::string_view::npos || path.find(':') != std::string_view::npos) return true;
  return path == "." || path == ".." || path.find("/../") != std::string_view::npos || path.find("../") == 0 ||
         (path.size() >= 3 && path.rfind("/..") == path.size() - 3) || path.find("//") != std::string_view::npos;
}

std::string_view StripAndroidAssetPrefix(std::string_view path) {
  static constexpr const char* kPrefixes[] = {"assets/", "content/"};
  for (const char* prefix : kPrefixes) {
    const std::size_t length = std::strlen(prefix);
    if (path.size() >= length && path.substr(0, length) == prefix) return path.substr(length);
  }
  return path;
}

std::vector<std::string> Candidates(std::string_view requested) {
  const std::string full(requested);
  const std::string stripped(StripAndroidAssetPrefix(requested));
  std::vector<std::string> candidates = {full, stripped, "ExtraContent/" + full, "ExtraContent/" + stripped};
  std::vector<std::string> unique;
  for (std::string& candidate : candidates) {
    if (std::find(unique.begin(), unique.end(), candidate) == unique.end()) unique.push_back(std::move(candidate));
  }
  return unique;
}

bool RegularFileExists(const std::wstring& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES &&
         (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0;
}

struct LookupCache {
  std::mutex mutex;
  std::unordered_map<std::string, std::pair<std::wstring, std::string>> resolved;
};
LookupCache& GetLookupCache() {
  static LookupCache* cache = new LookupCache();
  return *cache;
}
constexpr std::size_t kMaxCachedAssetPaths = 1024 * 5;

}
}

struct AAssetManager {};

struct AAsset {
  std::string logical_path;
  std::shared_ptr<elixir::android::ZipIndex> archive;
  const elixir::android::ZipEntry* entry = nullptr;
  std::shared_ptr<elixir::android::MappedFile> file;
  std::wstring file_path;
  std::uint64_t length = 0;
  std::uint64_t position = 0;
  const std::uint8_t* data = nullptr;
  std::vector<std::uint8_t> owned;
  bool loaded = false;
  bool transcoded = false;
  bool compressed = false;
};

struct AAssetDir {
  std::vector<std::string> names;
  std::size_t next = 0;
};

namespace elixir::android {
namespace {

AAssetManager g_asset_manager;
std::atomic<std::uint64_t> g_extracted_asset_read_failures{0};
std::atomic<std::uint64_t> g_archive_asset_read_failures{0};

bool EnsureLoaded(AAsset* asset) {
  if (asset->loaded) return true;
  if (asset->archive != nullptr) {
    if (asset->entry->method == 0) {
      asset->data = asset->archive->Data(*asset->entry);
    } else {
      if (!asset->archive->Extract(*asset->entry, &asset->owned)) {
        g_archive_asset_read_failures.fetch_add(1, std::memory_order_relaxed);
        Log(kLogError, "asset %s: corrupt deflate stream or CRC mismatch", asset->logical_path.c_str());
        std::vector<std::uint8_t>().swap(asset->owned);
        return false;
      }
      asset->data = asset->owned.data();
    }
  } else {
    asset->data = asset->file->data();
  }
  asset->loaded = true;
  return true;
}

bool Transcode(AAsset* asset) {
  if (!EnsureLoaded(asset)) return false;
  std::vector<unsigned char> bytes(asset->data, asset->data + asset->length);
  if (!libc_shim::TranscodeEtc1SkyTextureForVulkan(asset->logical_path.c_str(), &bytes)) return true;
  asset->owned.assign(bytes.begin(), bytes.end());
  asset->data = asset->owned.data();
  asset->length = asset->owned.size();
  asset->transcoded = true;
  return true;
}

AAsset* OpenAsset(const char* filename) {
  if (filename == nullptr) return nullptr;
  const std::string_view requested = StripAssetUriPrefix(filename);
  if (HasUnsafePathSegment(requested)) return nullptr;
  const std::shared_ptr<const Sources> sources = CurrentSources();
  const std::vector<std::string> candidates = Candidates(requested);
  auto asset = std::make_unique<AAsset>();

  if (!sources->overlay_directory.empty() && sources->overlay_filter != nullptr) {
    for (const std::string& candidate : candidates) {
      if (!sources->overlay_filter(candidate)) continue;
      const std::wstring path = Widen(sources->overlay_directory + "/" + candidate);
      if (path.empty() || !RegularFileExists(path)) continue;
      std::string error;
      asset->file = MappedFile::Open(path, &error);
      if (asset->file == nullptr) break;
      asset->file_path = path;
      asset->length = asset->file->size();
      asset->logical_path = "/assets/" + candidate;
      static std::atomic<int> logged{0};
      if (logged.fetch_add(1, std::memory_order_relaxed) < 24) {
        std::fprintf(stderr, "[elixir][mods] %s is served from the Mods folder\n", asset->logical_path.c_str());
      }
      break;
    }
  }

  LookupCache& cache = GetLookupCache();
  for (const std::string& directory : sources->directories) {
    if (asset->file != nullptr) break;
    const std::string cache_key = directory + '\0' + std::string(requested);
    std::wstring resolved;
    std::string relative;
    {
      std::lock_guard<std::mutex> lock(cache.mutex);
      const auto it = cache.resolved.find(cache_key);
      if (it != cache.resolved.end()) {
        if (RegularFileExists(it->second.first)) {
          resolved = it->second.first;
          relative = it->second.second;
        } else {
          cache.resolved.erase(it);
        }
      }
    }
    if (resolved.empty()) {
      for (const std::string& candidate : candidates) {
        const std::wstring path = Widen(directory + "/" + candidate);
        if (!path.empty() && RegularFileExists(path)) {
          resolved = path;
          relative = candidate;
          std::lock_guard<std::mutex> lock(cache.mutex);
          if (cache.resolved.size() >= kMaxCachedAssetPaths) cache.resolved.clear();
          cache.resolved.insert_or_assign(cache_key, std::make_pair(path, candidate));
          break;
        }
      }
    }
    if (resolved.empty()) continue;
    std::string error;
    asset->file = MappedFile::Open(resolved, &error);
    if (asset->file == nullptr) {
      g_extracted_asset_read_failures.fetch_add(1, std::memory_order_relaxed);
      return nullptr;
    }
    asset->file_path = resolved;
    asset->length = asset->file->size();
    asset->logical_path = "/assets/" + relative;
    break;
  }
  if (asset->file == nullptr) {
    for (const std::shared_ptr<ZipIndex>& archive : sources->archives) {
      for (const std::string& candidate : candidates) {
        const ZipEntry* entry = archive->Find("assets/" + candidate);
        if (entry == nullptr) continue;
        asset->archive = archive;
        asset->entry = entry;
        asset->length = entry->uncompressed_size;
        asset->compressed = entry->method != 0;
        asset->logical_path = "/assets/" + candidate;
        break;
      }
      if (asset->archive != nullptr) break;
    }
  }
  if (asset->file == nullptr && asset->archive == nullptr) return nullptr;
  if (sources->transcode && libc_shim::IsBundledSkyTexture(asset->logical_path.c_str()) && !Transcode(asset.get())) {
    return nullptr;
  }
  return asset.release();
}

AAssetDir* OpenDirectory(const char* directory_name) {
  std::string_view requested = StripAssetUriPrefix(directory_name != nullptr ? directory_name : "");
  while (!requested.empty() && requested.back() == '/') requested.remove_suffix(1);
  if (!requested.empty() && HasUnsafePathSegment(requested)) return nullptr;
  const std::shared_ptr<const Sources> sources = CurrentSources();
  std::set<std::string> names;
  for (const std::string& directory : sources->directories) {
    const std::wstring pattern =
        Widen(directory + "/" + std::string(requested) + (requested.empty() ? "*" : "/*"));
    WIN32_FIND_DATAW data;
    HANDLE find = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &data, FindExSearchNameMatch, nullptr, 0);
    if (find == INVALID_HANDLE_VALUE) continue;
    do {
      if ((data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) continue;
      const int length = WideCharToMultiByte(CP_UTF8, 0, data.cFileName, -1, nullptr, 0, nullptr, nullptr);
      if (length <= 1) continue;
      std::string name(static_cast<std::size_t>(length - 1), '\0');
      WideCharToMultiByte(CP_UTF8, 0, data.cFileName, -1, name.data(), length, nullptr, nullptr);
      names.insert(std::move(name));
    } while (FindNextFileW(find, &data));
    FindClose(find);
  }
  const std::string prefix = requested.empty() ? "assets/" : "assets/" + std::string(requested) + "/";
  for (const std::shared_ptr<ZipIndex>& archive : sources->archives) archive->ListFiles(prefix, &names);
  auto* dir = new AAssetDir();
  dir->names.assign(names.begin(), names.end());
  return dir;
}

}

AssetReadFailures GetAssetReadFailures() noexcept {
  return {g_extracted_asset_read_failures.load(std::memory_order_relaxed),
          g_archive_asset_read_failures.load(std::memory_order_relaxed)};
}

bool SetAssetSources(const AssetSources& sources, std::string* error) {
  try {
    auto next = std::make_shared<Sources>();
    next->directories = sources.directories;
    for (std::string& directory : next->directories) {
      while (!directory.empty() && (directory.back() == '/' || directory.back() == '\\')) directory.pop_back();
    }
    next->transcode = sources.transcode_etc1_sky_for_vulkan;
    next->overlay_directory = sources.overlay_directory;
    while (!next->overlay_directory.empty() &&
           (next->overlay_directory.back() == '/' || next->overlay_directory.back() == '\\')) {
      next->overlay_directory.pop_back();
    }
    next->overlay_filter = sources.overlay_filter;
    for (const std::string& path : sources.archives) {
      std::string reason;
      std::shared_ptr<ZipIndex> index = ZipIndex::Open(path, &reason);
      if (index == nullptr) {
        if (error != nullptr) *error = path + ": " + reason;
        return false;
      }
      next->archives.push_back(std::move(index));
    }
    {
      LookupCache& cache = GetLookupCache();
      std::lock_guard<std::mutex> lock(cache.mutex);
      cache.resolved.clear();
    }
    std::shared_ptr<const Sources> previous;
    {
      SourceState& state = GetSourceState();
      std::lock_guard<std::mutex> lock(state.mutex);
      previous = std::move(state.sources);
      state.sources = std::move(next);
    }
    return true;
  } catch (const std::exception& exception) {
    if (error != nullptr) *error = exception.what();
    return false;
  }
}

AAssetManager* GetAssetManager() { return &g_asset_manager; }

}

using elixir::android::Guarded;

ELIXIR_ANDROID_FN(AAssetManager*, AAssetManager_fromJava)(void* env, void* asset_manager) noexcept {
  return &elixir::android::g_asset_manager;
}

ELIXIR_ANDROID_FN(AAsset*, AAssetManager_open)(AAssetManager* manager, const char* filename, int mode) noexcept {
  if (manager == nullptr) return nullptr;
  return Guarded<AAsset*>(nullptr, [&] { return elixir::android::OpenAsset(filename); });
}

ELIXIR_ANDROID_FN(AAssetDir*, AAssetManager_openDir)(AAssetManager* manager, const char* directory) noexcept {
  if (manager == nullptr) return nullptr;
  return Guarded<AAssetDir*>(nullptr, [&] { return elixir::android::OpenDirectory(directory); });
}

ELIXIR_ANDROID_FN(const char*, AAssetDir_getNextFileName)(AAssetDir* dir) noexcept {
  if (dir == nullptr || dir->next >= dir->names.size()) return nullptr;
  return dir->names[dir->next++].c_str();
}

ELIXIR_ANDROID_FN(void, AAssetDir_rewind)(AAssetDir* dir) noexcept {
  if (dir != nullptr) dir->next = 0;
}

ELIXIR_ANDROID_FN(void, AAssetDir_close)(AAssetDir* dir) noexcept { delete dir; }

ELIXIR_ANDROID_FN(void, AAsset_close)(AAsset* asset) noexcept { delete asset; }

ELIXIR_ANDROID_FN(int, AAsset_read)(AAsset* asset, void* buffer, std::uint64_t count) noexcept {
  if (asset == nullptr || (buffer == nullptr && count != 0)) return -1;
  return Guarded(-1, [&] {
    if (!elixir::android::EnsureLoaded(asset)) return -1;
    const std::uint64_t remaining = asset->position < asset->length ? asset->length - asset->position : 0;
    std::uint64_t to_read = std::min<std::uint64_t>(count, remaining);
    to_read = std::min<std::uint64_t>(to_read, 0x7fffffff);
    if (to_read > 0) {
      std::memcpy(buffer, asset->data + asset->position, static_cast<std::size_t>(to_read));
      asset->position += to_read;
    }
    return static_cast<int>(to_read);
  });
}

ELIXIR_ANDROID_FN(const void*, AAsset_getBuffer)(AAsset* asset) noexcept {
  if (asset == nullptr) return nullptr;
  return Guarded<const void*>(nullptr, [&]() -> const void* {
    if (!elixir::android::EnsureLoaded(asset)) return nullptr;
    return asset->data != nullptr ? static_cast<const void*>(asset->data) : static_cast<const void*>("");
  });
}

ELIXIR_ANDROID_FN(std::int64_t, AAsset_getLength)(AAsset* asset) noexcept {
  return asset != nullptr ? static_cast<std::int64_t>(asset->length) : 0;
}

ELIXIR_ANDROID_FN(std::int64_t, AAsset_getRemainingLength)(AAsset* asset) noexcept {
  if (asset == nullptr || asset->position > asset->length) return 0;
  return static_cast<std::int64_t>(asset->length - asset->position);
}

ELIXIR_ANDROID_FN(std::int64_t, AAsset_seek)(AAsset* asset, std::int64_t offset, int whence) noexcept {
  if (asset == nullptr) return -1;
  std::int64_t base = 0;
  if (whence == guest::kSEEK_SET) {
    base = 0;
  } else if (whence == guest::kSEEK_CUR) {
    base = static_cast<std::int64_t>(asset->position);
  } else if (whence == guest::kSEEK_END) {
    base = static_cast<std::int64_t>(asset->length);
  } else {
    return elixir::bionic::FailWith<std::int64_t>(guest::kEINVAL, -1);
  }
  const std::int64_t length = static_cast<std::int64_t>(asset->length);
  if (offset < -base || offset > length - base) return elixir::bionic::FailWith<std::int64_t>(guest::kEINVAL, -1);
  const std::int64_t next = base + offset;
  asset->position = static_cast<std::uint64_t>(next);
  return next;
}

ELIXIR_ANDROID_FN(int, AAsset_isAllocated)(AAsset* asset) noexcept {
  return asset != nullptr && (asset->compressed || asset->transcoded) ? 1 : 0;
}

ELIXIR_ANDROID_FN(int, AAsset_openFileDescriptor)(AAsset* asset, std::int64_t* out_start,
                                                  std::int64_t* out_length) noexcept {
  if (asset == nullptr || asset->compressed || asset->transcoded) return -1;
  return Guarded(-1, [&] {
    const std::wstring& path = asset->archive != nullptr ? asset->archive->path() : asset->file_path;
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
      return elixir::bionic::FailWith(elixir::bionic::ErrnoFromWin32(GetLastError()), -1);
    }
    const int fd = elixir::bionic::vfs::InstallHandle(handle, elixir::bionic::vfs::FdKind::kFile,
                                                      guest::kO_RDONLY | guest::kO_CLOEXEC);
    if (fd < 0) {
      CloseHandle(handle);
      return -1;
    }
    if (out_start != nullptr) {
      *out_start = asset->archive != nullptr ? static_cast<std::int64_t>(asset->entry->data_offset) : 0;
    }
    if (out_length != nullptr) *out_length = static_cast<std::int64_t>(asset->length);
    return fd;
  });
}

namespace elixir::android {

void RegisterAssetExports(bionic::ExportTable& table) {
#define ELIXIR_ADD(name) table.Add(kLibandroid, #name, reinterpret_cast<void*>(&elixir_android_##name))
#define ELIXIR_ADD_AS(name, impl) table.Add(kLibandroid, #name, reinterpret_cast<void*>(&elixir_android_##impl))
  ELIXIR_ADD(AAssetManager_fromJava);
  ELIXIR_ADD(AAssetManager_open);
  ELIXIR_ADD(AAssetManager_openDir);
  ELIXIR_ADD(AAssetDir_getNextFileName);
  ELIXIR_ADD(AAssetDir_rewind);
  ELIXIR_ADD(AAssetDir_close);
  ELIXIR_ADD(AAsset_close);
  ELIXIR_ADD(AAsset_read);
  ELIXIR_ADD(AAsset_getBuffer);
  ELIXIR_ADD(AAsset_getLength);
  ELIXIR_ADD_AS(AAsset_getLength64, AAsset_getLength);
  ELIXIR_ADD(AAsset_getRemainingLength);
  ELIXIR_ADD_AS(AAsset_getRemainingLength64, AAsset_getRemainingLength);
  ELIXIR_ADD(AAsset_seek);
  ELIXIR_ADD_AS(AAsset_seek64, AAsset_seek);
  ELIXIR_ADD(AAsset_isAllocated);
  ELIXIR_ADD(AAsset_openFileDescriptor);
  ELIXIR_ADD_AS(AAsset_openFileDescriptor64, AAsset_openFileDescriptor);
#undef ELIXIR_ADD_AS
#undef ELIXIR_ADD
}

}
