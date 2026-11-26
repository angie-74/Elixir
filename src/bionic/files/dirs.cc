// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <algorithm>
#include <cstring>
#include <set>

#include "bionic/files/files_internal.h"
#include "bionic/memory.h"

namespace elixir::bionic::files {
namespace {

constexpr std::uint64_t kDirMagic = 0x5249444c45584900ull;

struct DirEntry {
  std::string name;
  std::uint64_t ino;
  std::uint8_t type;
};

struct GuestDir {
  std::uint64_t magic = kDirMagic;
  int fd = -1;
  DescriptionPtr description;
  std::mutex lock;
  bool loaded = false;
  std::vector<DirEntry> entries;
  std::size_t next = 0;
  guest::Dirent current{};
};

GuestDir* FromHandle(void* dir) {
  auto* d = static_cast<GuestDir*>(dir);
  if (d == nullptr || d->magic != kDirMagic) return nullptr;
  return d;
}

std::uint8_t TypeFromAttributes(DWORD attributes, DWORD ea_or_tag) {
  if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 && IsSymlinkTag(ea_or_tag)) return guest::kDT_LNK;
  if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) return guest::kDT_DIR;
  return guest::kDT_REG;
}

int ListHost(HANDLE handle, const std::string& guest_dir, std::vector<DirEntry>* out) {
  std::vector<std::uint8_t> buffer(64 * 1024);
  bool use_full = false;
  bool first = true;
  for (;;) {
    FILE_INFO_BY_HANDLE_CLASS info_class;
    if (use_full) {
      info_class = first ? FileFullDirectoryRestartInfo : FileFullDirectoryInfo;
    } else {
      info_class = first ? FileIdBothDirectoryRestartInfo : FileIdBothDirectoryInfo;
    }
    if (!GetFileInformationByHandleEx(handle, info_class, buffer.data(), static_cast<DWORD>(buffer.size()))) {
      const DWORD error = GetLastError();
      if (error == ERROR_NO_MORE_FILES) return 0;
      if (first && !use_full && (error == ERROR_INVALID_PARAMETER || error == ERROR_NOT_SUPPORTED)) {
        use_full = true;
        continue;
      }
      return error == ERROR_FILE_NOT_FOUND ? 0 : ErrnoFromWin32(error);
    }
    first = false;
    std::size_t offset = 0;
    for (;;) {
      std::wstring_view name;
      DWORD attributes = 0, ea = 0, next = 0;
      std::uint64_t ino = 0;
      if (use_full) {
        const auto* info = reinterpret_cast<const FILE_FULL_DIR_INFO*>(buffer.data() + offset);
        name = std::wstring_view(info->FileName, info->FileNameLength / sizeof(WCHAR));
        attributes = info->FileAttributes;
        ea = info->EaSize;
        next = info->NextEntryOffset;
      } else {
        const auto* info = reinterpret_cast<const FILE_ID_BOTH_DIR_INFO*>(buffer.data() + offset);
        name = std::wstring_view(info->FileName, info->FileNameLength / sizeof(WCHAR));
        attributes = info->FileAttributes;
        ea = info->EaSize;
        next = info->NextEntryOffset;
        ino = static_cast<std::uint64_t>(info->FileId.QuadPart);
      }
      if (name != L"." && name != L"..") {
        DirEntry entry;
        entry.name = UnescapeHostName(name);
        entry.ino = ino != 0 ? ino : (HashString(guest_dir + "/" + entry.name) | 1);
        entry.type = TypeFromAttributes(attributes, ea);
        out->push_back(std::move(entry));
      }
      if (next == 0) break;
      offset += next;
    }
  }
}

int Load(GuestDir& dir) {
  Description& d = *dir.description;
  std::vector<DirEntry> entries;
  const std::string& guest_dir = d.guest_path;
  const std::uint64_t self_ino = HashString(guest_dir) | 1;
  std::string parent = guest_dir.substr(0, guest_dir.rfind('/'));
  if (parent.empty()) parent = "/";
  entries.push_back({".", self_ino, guest::kDT_DIR});
  entries.push_back({"..", HashString(parent) | 1, guest::kDT_DIR});
  if (d.provider == Provider::kVirtualDir) {
    if (d.synthetic_dir) {
      for (auto& [name, type] : SynthList(guest_dir)) {
        entries.push_back({name, HashString(guest_dir + "/" + name) | 1, type});
      }
    }
  } else {
    HostFileInfo info;
    if (QueryHandleInfo(d.handle, &info)) entries[0].ino = info.file_id;
    const int error = ListHost(d.handle, guest_dir, &entries);
    if (error != 0) return error;
  }
  std::set<std::string> seen;
  for (const DirEntry& e : entries) seen.insert(e.name);
  for (const std::string& name : MountChildren(guest_dir)) {
    if (seen.insert(name).second) {
      entries.push_back({name, HashString((guest_dir == "/" ? std::string() : guest_dir) + "/" + name) | 1,
                         guest::kDT_DIR});
    }
  }
  dir.entries = std::move(entries);
  dir.loaded = true;
  return 0;
}

guest::Dirent* Next(GuestDir& dir, int* error) {
  *error = 0;
  if (!dir.loaded) {
    *error = Load(dir);
    if (*error != 0) return nullptr;
  }
  if (dir.next >= dir.entries.size()) return nullptr;
  const DirEntry& e = dir.entries[dir.next++];
  guest::Dirent& out = dir.current;
  std::memset(&out, 0, sizeof(out));
  out.d_ino = e.ino;
  out.d_off = static_cast<std::int64_t>(dir.next);
  const std::size_t length = std::min<std::size_t>(e.name.size(), sizeof(out.d_name) - 1);
  std::memcpy(out.d_name, e.name.data(), length);
  out.d_reclen = static_cast<std::uint16_t>((offsetof(guest::Dirent, d_name) + length + 1 + 7) & ~std::size_t{7});
  out.d_type = e.type;
  return &out;
}

GuestDir* MakeDir(int fd) {
  DescriptionPtr description = GetDescription(fd);
  if (description == nullptr) return nullptr;
  if (!IsDirectoryDescription(*description)) return FailWith(guest::kENOTDIR, static_cast<GuestDir*>(nullptr));
  auto* dir = new GuestDir();
  dir->fd = fd;
  dir->description = std::move(description);
  return dir;
}

void* OpenDirAt(int dirfd, const char* path) {
  const int fd = OpenAt(dirfd, path, guest::kO_RDONLY | guest::kO_DIRECTORY | guest::kO_CLOEXEC, 0);
  if (fd < 0) return nullptr;
  GuestDir* dir = MakeDir(fd);
  if (dir == nullptr) {
    CloseFd(fd);
    return nullptr;
  }
  const int error = Load(*dir);
  if (error != 0) {
    delete dir;
    CloseFd(fd);
    return FailWith(error, static_cast<void*>(nullptr));
  }
  return dir;
}

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

int VersionCompare(const char* a, const char* b) {
  for (;;) {
    if (IsDigit(*a) && IsDigit(*b)) {
      const char* ra = a;
      const char* rb = b;
      while (IsDigit(*a)) ++a;
      while (IsDigit(*b)) ++b;
      const std::string_view x(ra, static_cast<std::size_t>(a - ra));
      const std::string_view y(rb, static_cast<std::size_t>(b - rb));
      const bool fx = x.size() > 1 && x[0] == '0';
      const bool fy = y.size() > 1 && y[0] == '0';
      if (fx != fy) return fx ? -1 : 1;
      if (!fx) {
        if (x.size() != y.size()) return x.size() < y.size() ? -1 : 1;
        const int c = x.compare(y);
        if (c != 0) return c < 0 ? -1 : 1;
      } else {
        const std::size_t zx = x.find_first_not_of('0') == std::string_view::npos ? x.size() : x.find_first_not_of('0');
        const std::size_t zy = y.find_first_not_of('0') == std::string_view::npos ? y.size() : y.find_first_not_of('0');
        if (zx != zy) return zx > zy ? -1 : 1;
        const int c = x.substr(zx).compare(y.substr(zy));
        if (c != 0) return c < 0 ? -1 : 1;
      }
      continue;
    }
    const unsigned char ca = static_cast<unsigned char>(*a);
    const unsigned char cb = static_cast<unsigned char>(*b);
    if (ca != cb) return ca < cb ? -1 : 1;
    if (ca == 0) return 0;
    ++a;
    ++b;
  }
}

using DirentFilter = abi::SysvFn<int(const guest::Dirent*)>;
using DirentCompare = abi::SysvFn<int(const guest::Dirent**, const guest::Dirent**)>;

int ScanDir(void* opened, guest::Dirent*** name_list, DirentFilter filter, DirentCompare compare) {
  GuestDir* dir = FromHandle(opened);
  if (dir == nullptr) return -1;
  std::vector<guest::Dirent*> names;
  int error = 0;
  auto release = [&] {
    for (guest::Dirent* entry : names) GuestFree(entry);
    CloseFd(dir->fd);
    delete dir;
  };
  for (;;) {
    guest::Dirent* entry = Next(*dir, &error);
    if (entry == nullptr) break;
    if (filter != nullptr && filter(entry) == 0) continue;
    auto* copy = static_cast<guest::Dirent*>(GuestMalloc(sizeof(guest::Dirent)));
    if (copy == nullptr) {
      release();
      return FailWith(guest::kENOMEM, -1);
    }
    std::memcpy(copy, entry, sizeof(guest::Dirent));
    names.push_back(copy);
  }
  if (error != 0) {
    release();
    return FailWith(error, -1);
  }
  if (compare != nullptr) {
    std::stable_sort(names.begin(), names.end(), [&](guest::Dirent* x, guest::Dirent* y) {
      const guest::Dirent* px = x;
      const guest::Dirent* py = y;
      return compare(&px, &py) < 0;
    });
  }
  auto* array = static_cast<guest::Dirent**>(GuestMalloc(std::max<std::size_t>(1, names.size()) * sizeof(guest::Dirent*)));
  if (array == nullptr) {
    release();
    return FailWith(guest::kENOMEM, -1);
  }
  std::copy(names.begin(), names.end(), array);
  const int count = static_cast<int>(names.size());
  names.clear();
  CloseFd(dir->fd);
  delete dir;
  *name_list = array;
  return count;
}

}

ELIXIR_BIONIC_FN(void*, opendir)(const char* path) noexcept {
  return Guarded(static_cast<void*>(nullptr), [&] { return OpenDirAt(guest::kAT_FDCWD, path); });
}
ELIXIR_BIONIC_FN(void*, fdopendir)(int fd) noexcept {
  return Guarded(static_cast<void*>(nullptr), [&]() -> void* { return MakeDir(fd); });
}
ELIXIR_BIONIC_FN(guest::Dirent*, readdir)(void* opened) noexcept {
  return Guarded(static_cast<guest::Dirent*>(nullptr), [&]() -> guest::Dirent* {
    GuestDir* dir = FromHandle(opened);
    if (dir == nullptr) return FailWith(guest::kEBADF, static_cast<guest::Dirent*>(nullptr));
    std::lock_guard<std::mutex> guard(dir->lock);
    int error = 0;
    guest::Dirent* entry = Next(*dir, &error);
    if (entry == nullptr && error != 0) SetGuestErrno(error);
    return entry;
  });
}
ELIXIR_BIONIC_FN(int, readdir_r)(void* opened, guest::Dirent* entry, guest::Dirent** result) noexcept {
  return Guarded(guest::kENOMEM, [&] {
    GuestDir* dir = FromHandle(opened);
    if (dir == nullptr || entry == nullptr || result == nullptr) return guest::kEBADF;
    std::lock_guard<std::mutex> guard(dir->lock);
    int error = 0;
    guest::Dirent* next = Next(*dir, &error);
    if (next == nullptr) {
      *result = nullptr;
      return error;
    }
    std::memcpy(entry, next, sizeof(*entry));
    *result = entry;
    return 0;
  });
}
ELIXIR_BIONIC_FN(int, closedir)(void* opened) noexcept {
  return Guarded(-1, [&] {
    GuestDir* dir = FromHandle(opened);
    if (dir == nullptr) return FailWith(guest::kEINVAL, -1);
    dir->magic = 0;
    const int fd = dir->fd;
    delete dir;
    return CloseFd(fd);
  });
}
ELIXIR_BIONIC_FN(void, rewinddir)(void* opened) noexcept {
  Guarded(0, [&] {
    GuestDir* dir = FromHandle(opened);
    if (dir == nullptr) return 0;
    std::lock_guard<std::mutex> guard(dir->lock);
    dir->loaded = false;
    dir->entries.clear();
    dir->next = 0;
    return 0;
  });
}
ELIXIR_BIONIC_FN(abi::g_long, telldir)(void* opened) noexcept {
  return Guarded<abi::g_long>(-1, [&]() -> abi::g_long {
    GuestDir* dir = FromHandle(opened);
    if (dir == nullptr) return FailWith(guest::kEBADF, static_cast<abi::g_long>(-1));
    std::lock_guard<std::mutex> guard(dir->lock);
    return static_cast<abi::g_long>(dir->next);
  });
}
ELIXIR_BIONIC_FN(void, seekdir)(void* opened, abi::g_long position) noexcept {
  Guarded(0, [&] {
    GuestDir* dir = FromHandle(opened);
    if (dir == nullptr || position < 0) return 0;
    std::lock_guard<std::mutex> guard(dir->lock);
    if (!dir->loaded && Load(*dir) != 0) return 0;
    dir->next = std::min<std::size_t>(static_cast<std::size_t>(position), dir->entries.size());
    return 0;
  });
}
ELIXIR_BIONIC_FN(int, dirfd)(void* opened) noexcept {
  GuestDir* dir = FromHandle(opened);
  if (dir == nullptr) return FailWith(guest::kEINVAL, -1);
  return dir->fd;
}
ELIXIR_BIONIC_FN(int, scandir)(const char* path, guest::Dirent*** name_list, DirentFilter filter,
                               DirentCompare compare) noexcept {
  return Guarded(-1, [&] {
    if (name_list == nullptr) return FailWith(guest::kEFAULT, -1);
    void* dir = OpenDirAt(guest::kAT_FDCWD, path);
    if (dir == nullptr) return -1;
    return ScanDir(dir, name_list, filter, compare);
  });
}
ELIXIR_BIONIC_FN(int, scandirat)(int dirfd, const char* path, guest::Dirent*** name_list, DirentFilter filter,
                                 DirentCompare compare) noexcept {
  return Guarded(-1, [&] {
    if (name_list == nullptr) return FailWith(guest::kEFAULT, -1);
    void* dir = OpenDirAt(dirfd, path);
    if (dir == nullptr) return -1;
    return ScanDir(dir, name_list, filter, compare);
  });
}
ELIXIR_BIONIC_FN(int, alphasort)(const guest::Dirent** a, const guest::Dirent** b) noexcept {
  return std::strcmp((*a)->d_name, (*b)->d_name);
}
ELIXIR_BIONIC_FN(int, versionsort)(const guest::Dirent** a, const guest::Dirent** b) noexcept {
  return VersionCompare((*a)->d_name, (*b)->d_name);
}

void RegisterDirExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  add("opendir", &elixir_bionic_opendir);
  add("fdopendir", &elixir_bionic_fdopendir);
  add("readdir", &elixir_bionic_readdir);
  add("readdir64", &elixir_bionic_readdir);
  add("readdir_r", &elixir_bionic_readdir_r);
  add("readdir64_r", &elixir_bionic_readdir_r);
  add("closedir", &elixir_bionic_closedir);
  add("rewinddir", &elixir_bionic_rewinddir);
  add("telldir", &elixir_bionic_telldir);
  add("seekdir", &elixir_bionic_seekdir);
  add("dirfd", &elixir_bionic_dirfd);
  add("scandir", &elixir_bionic_scandir);
  add("scandir64", &elixir_bionic_scandir);
  add("scandirat", &elixir_bionic_scandirat);
  add("scandirat64", &elixir_bionic_scandirat);
  add("alphasort", &elixir_bionic_alphasort);
  add("alphasort64", &elixir_bionic_alphasort);
  add("versionsort", &elixir_bionic_versionsort);
  add("versionsort64", &elixir_bionic_versionsort);
}

}
