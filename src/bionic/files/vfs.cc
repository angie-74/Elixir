// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The longest-prefix mapping and the default /data/data/<pkg> layout follow
// Mocktail's libc_shim (src/libc_shim/libc_shim.cc, Apache-2.0).
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <set>
#include <string>

#include "bionic/files/files.h"
#include "bionic/files/files_internal.h"
#include "platform/trace_clock.h"

namespace elixir::bionic::files {
namespace {

struct MountEntry {
  std::string guest;
  std::vector<std::string> components;
  std::wstring host_root;
  std::wstring final_root;
  bool read_only = false;
  std::size_t order = 0;
  std::wstring overlay_root;
  bool (*overlay_filter)(std::string_view relative) = nullptr;
};

std::atomic<std::uint64_t> g_overlay_lookups{0};
constexpr std::uint64_t kLoggedOverlayLookups = 24;

struct VfsState {
  SRWLOCK lock = SRWLOCK_INIT;
  std::vector<MountEntry> mounts;
  bool configured = false;
  std::string cwd = "/";
};

VfsState& State() {
  static VfsState* state = new VfsState();
  return *state;
}

constexpr const char* kSyntheticRoots[] = {"proc", "sys", "dev"};

std::atomic<std::uint64_t> g_content_cache_failures[] = {{0}, {0}, {0}, {0}, {0}};
std::atomic<std::uint64_t> g_content_cache_open_hits{0};
std::atomic<std::uint64_t> g_content_cache_open_misses{0};
constexpr const char* kContentCacheOperations[] = {"open", "read", "write", "rename", "map"};

std::atomic<std::uint64_t>& ContentCacheCounter(ContentCacheOperation operation) noexcept {
  return g_content_cache_failures[static_cast<unsigned>(operation)];
}

std::string_view AppRelativePath(std::string_view guest_path) noexcept {
  constexpr std::string_view data_prefix = "/data/data/";
  constexpr std::string_view user_prefix = "/data/user/0/";
  if (guest_path.compare(0, data_prefix.size(), data_prefix) == 0) {
    guest_path.remove_prefix(data_prefix.size());
  } else if (guest_path.compare(0, user_prefix.size(), user_prefix) == 0) {
    guest_path.remove_prefix(user_prefix.size());
  } else {
    return {};
  }
  const std::size_t package_end = guest_path.find('/');
  if (package_end == 0 || package_end == std::string_view::npos) return {};
  return guest_path.substr(package_end + 1);
}

bool IsContentCachePath(std::string_view guest_path) noexcept {
  guest_path = AppRelativePath(guest_path);
  constexpr std::string_view cache_prefix = "cache/";
  if (guest_path.compare(0, cache_prefix.size(), cache_prefix) == 0) {
    guest_path.remove_prefix(cache_prefix.size());
  }
  constexpr std::string_view provider_prefix = "ContentProvider_";
  if (guest_path.compare(0, provider_prefix.size(), provider_prefix) != 0) return false;
  guest_path.remove_prefix(provider_prefix.size());
  const std::string_view number = guest_path.substr(0, guest_path.find('/'));
  return !number.empty() && std::all_of(number.begin(), number.end(), [](char c) { return c >= '0' && c <= '9'; });
}

bool IsSyntheticRoot(std::string_view component) {
  for (const char* root : kSyntheticRoots) {
    if (component == root) return true;
  }
  return false;
}

bool IsAsciiAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

bool IsHostAbsolute(std::string_view path) {
  if (path.size() >= 3 && IsAsciiAlpha(path[0]) && path[1] == ':' && (path[2] == '/' || path[2] == '\\')) return true;
  if (path.size() >= 4 && (path[0] == '\\' || path[0] == '/') && (path[1] == '\\' || path[1] == '/') &&
      path[2] == '?' && (path[3] == '\\' || path[3] == '/'))
    return true;
  return false;
}

std::vector<std::string> SplitComponents(std::string_view path) {
  std::vector<std::string> out;
  std::size_t i = 0;
  while (i < path.size()) {
    while (i < path.size() && path[i] == '/') ++i;
    std::size_t j = i;
    while (j < path.size() && path[j] != '/') ++j;
    if (j > i) out.emplace_back(path.substr(i, j - i));
    i = j;
  }
  return out;
}

std::string JoinComponents(const std::vector<std::string>& components) {
  if (components.empty()) return "/";
  std::string out;
  for (const auto& c : components) {
    out.push_back('/');
    out += c;
  }
  return out;
}

bool EqualsIgnoreCase(std::wstring_view a, std::wstring_view b) {
  if (a.size() != b.size()) return false;
  if (a.empty()) return true;
  return CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE) ==
         CSTR_EQUAL;
}

std::wstring StripLongPrefix(std::wstring path) {
  for (auto& c : path) {
    if (c == L'/') c = L'\\';
  }
  if (path.rfind(L"\\\\?\\UNC\\", 0) == 0) return L"\\\\" + path.substr(8);
  if (path.rfind(L"\\\\?\\", 0) == 0) return path.substr(4);
  return path;
}

std::wstring WithLongPrefix(const std::wstring& plain) {
  if (plain.rfind(L"\\\\?\\", 0) == 0) return plain;
  if (plain.rfind(L"\\\\", 0) == 0) return L"\\\\?\\UNC\\" + plain.substr(2);
  return L"\\\\?\\" + plain;
}

std::wstring FullHostPath(const std::wstring& plain) {
  DWORD needed = GetFullPathNameW(plain.c_str(), 0, nullptr, nullptr);
  if (needed == 0) return plain;
  std::wstring out(needed, L'\0');
  DWORD written = GetFullPathNameW(plain.c_str(), needed, out.data(), nullptr);
  if (written == 0 || written >= needed) return plain;
  out.resize(written);
  return out;
}

std::wstring NormalizeHostRoot(std::string_view host_utf8) {
  std::wstring plain = StripLongPrefix(Utf8ToWide(host_utf8));
  plain = FullHostPath(plain);
  while (plain.size() > 3 && plain.back() == L'\\') plain.pop_back();
  return plain;
}

std::wstring FinalRoot(const std::wstring& root) {
  HANDLE handle = CreateFileW(WithLongPrefix(root).c_str(), FILE_READ_ATTRIBUTES,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                              FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return {};
  std::wstring buffer(1024, L'\0');
  DWORD length = GetFinalPathNameByHandleW(handle, buffer.data(), static_cast<DWORD>(buffer.size()),
                                           FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (length >= buffer.size()) {
    buffer.resize(length + 1);
    length = GetFinalPathNameByHandleW(handle, buffer.data(), static_cast<DWORD>(buffer.size()),
                                       FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  }
  CloseHandle(handle);
  if (length == 0 || length >= buffer.size()) return {};
  buffer.resize(length);
  std::wstring plain = StripLongPrefix(buffer);
  while (plain.size() > 3 && plain.back() == L'\\') plain.pop_back();
  if (EqualsIgnoreCase(plain, root)) return {};
  return plain;
}

MountEntry MakeEntry(const vfs::Mount& mount, std::size_t order) {
  MountEntry entry;
  std::string prefix = mount.guest_prefix;
  if (prefix.empty() || prefix[0] != '/') prefix.insert(prefix.begin(), '/');
  entry.components = SplitComponents(prefix);
  std::vector<std::string> clean;
  for (auto& c : entry.components) {
    if (c == ".") continue;
    if (c == "..") {
      if (!clean.empty()) clean.pop_back();
      continue;
    }
    clean.push_back(c);
  }
  entry.components = clean;
  entry.guest = JoinComponents(entry.components);
  entry.host_root = NormalizeHostRoot(mount.host_path);
  entry.final_root = FinalRoot(entry.host_root);
  entry.read_only = mount.read_only;
  entry.order = order;
  if (mount.read_only && mount.overlay_filter != nullptr && !mount.overlay_host_path.empty()) {
    entry.overlay_root = NormalizeHostRoot(mount.overlay_host_path);
    entry.overlay_filter = mount.overlay_filter;
  }
  return entry;
}

void InstallMountsLocked(VfsState& state, const vfs::Mount* mounts, std::size_t count) {
  std::vector<MountEntry> entries;
  entries.reserve(count);
  for (std::size_t i = 0; i < count; ++i) entries.push_back(MakeEntry(mounts[i], i));
  state.mounts = std::move(entries);
  state.configured = true;
}

void EnsureConfigured() {
  VfsState& state = State();
  AcquireSRWLockShared(&state.lock);
  const bool configured = state.configured;
  ReleaseSRWLockShared(&state.lock);
  if (configured) return;
  const std::vector<vfs::Mount> defaults = DefaultMounts(Options().package_name);
  EnsureMountDirectories(defaults);
  AcquireSRWLockExclusive(&state.lock);
  if (!state.configured) InstallMountsLocked(state, defaults.data(), defaults.size());
  ReleaseSRWLockExclusive(&state.lock);
}

struct SharedLock {
  explicit SharedLock(SRWLOCK* lock) : lock_(lock) { AcquireSRWLockShared(lock_); }
  ~SharedLock() { ReleaseSRWLockShared(lock_); }
  SRWLOCK* lock_;
};

const MountEntry* LongestMountLocked(const VfsState& state, const std::vector<std::string>& components) {
  const MountEntry* best = nullptr;
  for (const MountEntry& mount : state.mounts) {
    if (mount.components.size() > components.size()) continue;
    if (!std::equal(mount.components.begin(), mount.components.end(), components.begin())) continue;
    if (best == nullptr || mount.components.size() > best->components.size()) best = &mount;
  }
  return best;
}

std::wstring BuildHostPath(const std::wstring& root, const std::vector<std::string>& components, std::size_t first) {
  std::wstring out = WithLongPrefix(root);
  for (std::size_t i = first; i < components.size(); ++i) {
    if (out.back() != L'\\') out.push_back(L'\\');
    AppendEscapedComponent(&out, components[i]);
  }
  return out;
}

bool HasMountBelowLocked(const VfsState& state, const std::vector<std::string>& components) {
  if (components.empty()) return true;
  for (const MountEntry& mount : state.mounts) {
    if (mount.components.size() <= components.size()) continue;
    if (std::equal(components.begin(), components.end(), mount.components.begin())) return true;
  }
  return false;
}

bool ResolveHostPassthrough(std::string_view path, bool trailing_hint, Resolved* out, int* err) {
  std::wstring plain = StripLongPrefix(Utf8ToWide(path));
  const bool trailing = trailing_hint || (!plain.empty() && plain.back() == L'\\' && plain.size() > 3);
  std::wstring full = FullHostPath(plain);
  while (full.size() > 3 && full.back() == L'\\') full.pop_back();
  if (full.size() < 3) {
    *err = guest::kENOENT;
    return false;
  }
  out->kind = NodeKind::kHost;
  out->host_absolute = true;
  out->host = WithLongPrefix(full);
  std::string guest = WideToUtf8(full);
  std::replace(guest.begin(), guest.end(), '\\', '/');
  out->guest = guest;
  out->trailing_slash = trailing;
  out->read_only = false;
  out->mount_root = false;
  VfsState& state = State();
  SharedLock lock(&state.lock);
  for (const MountEntry& mount : state.mounts) {
    if (!mount.read_only) continue;
    if (full.size() >= mount.host_root.size() &&
        EqualsIgnoreCase(std::wstring_view(full).substr(0, mount.host_root.size()), mount.host_root) &&
        (full.size() == mount.host_root.size() || full[mount.host_root.size()] == L'\\')) {
      out->read_only = true;
    }
  }
  return true;
}

bool ResolveComponents(std::vector<std::string> components, bool trailing, bool last_dot, Resolved* out, int* err) {
  out->guest = JoinComponents(components);
  out->trailing_slash = trailing;
  out->last_is_dot = last_dot;
  out->host_absolute = false;
  bool have_mount = false;
  std::size_t mount_depth = 0;
  std::wstring host;
  std::wstring overlay;
  bool read_only = false;
  bool virtual_dir = false;
  {
    VfsState& state = State();
    SharedLock lock(&state.lock);
    const MountEntry* mount = LongestMountLocked(state, components);
    if (mount != nullptr) {
      have_mount = true;
      mount_depth = mount->components.size();
      host = BuildHostPath(mount->host_root, components, mount_depth);
      read_only = mount->read_only;
      if (!mount->overlay_root.empty() && components.size() > mount_depth) {
        std::string relative;
        for (std::size_t i = mount_depth; i < components.size(); ++i) {
          if (!relative.empty()) relative.push_back('/');
          relative += components[i];
        }
        if (mount->overlay_filter(relative)) overlay = BuildHostPath(mount->overlay_root, components, mount_depth);
      }
    } else {
      virtual_dir = HasMountBelowLocked(state, components);
    }
  }
  if (!overlay.empty()) {
    const DWORD attributes = GetFileAttributesW(overlay.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0) {
      host = std::move(overlay);
      if (g_overlay_lookups.fetch_add(1, std::memory_order_relaxed) < kLoggedOverlayLookups) {
        const DWORD saved_error = GetLastError();
        std::fprintf(stderr, "[elixir][mods] %s is served from the Mods folder\n", out->guest.c_str());
        SetLastError(saved_error);
      }
    }
  }
  if (!components.empty() && IsSyntheticRoot(components[0]) && (!have_mount || mount_depth <= 1)) {
    SynthNode node;
    if (SynthLookup(out->guest, &node)) {
      out->kind = NodeKind::kSynthetic;
      out->read_only = true;
      out->mount_root = false;
      return true;
    }
  }
  if (have_mount) {
    out->kind = NodeKind::kHost;
    out->host = std::move(host);
    out->read_only = read_only;
    out->mount_root = mount_depth == components.size();
    return true;
  }
  if (virtual_dir) {
    out->kind = NodeKind::kVirtualDir;
    out->read_only = true;
    out->mount_root = false;
    return true;
  }
  *err = guest::kENOENT;
  return false;
}

bool ResolveWithBase(std::string_view base, std::string_view path, Resolved* out, int* err) {
  if (path.empty()) {
    *err = guest::kENOENT;
    return false;
  }
  if (path.size() >= 4096) {
    *err = guest::kENAMETOOLONG;
    return false;
  }
  EnsureConfigured();
  if (IsHostAbsolute(path)) return ResolveHostPassthrough(path, false, out, err);
  const bool relative = path[0] != '/';
  if (relative && IsHostAbsolute(base)) {
    std::string joined(base);
    joined.push_back('/');
    joined.append(path);
    const bool trailing = path.back() == '/' || path == "." || path == ".." ||
                          (path.size() >= 2 && path.substr(path.size() - 2) == "/.") ||
                          (path.size() >= 3 && path.substr(path.size() - 3) == "/..");
    return ResolveHostPassthrough(joined, trailing, out, err);
  }
  std::vector<std::string> components = relative ? SplitComponents(base) : std::vector<std::string>{};
  bool trailing = path.back() == '/';
  bool last_dot = false;
  std::size_t i = 0;
  while (i < path.size()) {
    while (i < path.size() && path[i] == '/') ++i;
    if (i >= path.size()) break;
    std::size_t j = i;
    while (j < path.size() && path[j] != '/') ++j;
    const std::string_view component = path.substr(i, j - i);
    const bool is_last = j >= path.size() || path.find_first_not_of('/', j) == std::string_view::npos;
    if (component == "." || component == "..") {
      if (component == ".." && !components.empty()) components.pop_back();
      if (is_last) {
        trailing = true;
        last_dot = true;
      }
    } else {
      if (component.size() > 255) {
        *err = guest::kENAMETOOLONG;
        return false;
      }
      components.emplace_back(component);
    }
    i = j;
  }
  return ResolveComponents(std::move(components), trailing, last_dot, out, err);
}

}

ContentCacheIoCounters GetContentCacheIoCounters() noexcept {
  return {ContentCacheCounter(ContentCacheOperation::kOpen).load(std::memory_order_relaxed),
          ContentCacheCounter(ContentCacheOperation::kRead).load(std::memory_order_relaxed),
          ContentCacheCounter(ContentCacheOperation::kWrite).load(std::memory_order_relaxed),
          ContentCacheCounter(ContentCacheOperation::kRename).load(std::memory_order_relaxed),
          ContentCacheCounter(ContentCacheOperation::kMap).load(std::memory_order_relaxed),
          g_content_cache_open_hits.load(std::memory_order_relaxed),
          g_content_cache_open_misses.load(std::memory_order_relaxed)};
}

void RecordContentCacheOpenResult(std::string_view path, int guest_errno) noexcept {
  if (guest_errno != 0 && guest_errno != guest::kENOENT) {
    RecordContentCacheIoFailure(ContentCacheOperation::kOpen, guest_errno, path);
    return;
  }
  if (!IsContentCachePath(path)) return;
  auto& counter = guest_errno == 0 ? g_content_cache_open_hits : g_content_cache_open_misses;
  counter.fetch_add(1, std::memory_order_relaxed);
}

void RecordAppStorageQuery(const Resolved& path, const char* operation, int error) noexcept {
  const std::string_view relative = AppRelativePath(path.guest);
  const bool known_cache_file = relative == "files/ixp_cache_random_id";
  const bool open = std::strcmp(operation, "open") == 0;
  if (!error && !(open && known_cache_file)) return;
  const char* category = nullptr;
  if (known_cache_file) category = "startup-cache";
  else if (relative == "cache" || relative.compare(0, 6, "cache/") == 0) category = "app-cache";
  else if (relative == "files") category = "files-root";
  else return;

  static std::atomic<unsigned> samples[4]{};
  auto& budget = samples[!open ? 3 : !known_cache_file ? 2 : error ? 1 : 0];
  unsigned count = budget.load(std::memory_order_relaxed);
  do {
    if (count >= 16) return;
  } while (!budget.compare_exchange_weak(count, count + 1, std::memory_order_relaxed));

  const DWORD saved_last = GetLastError();
  const int saved_errno = errno;
  const int saved_guest = GuestErrno();
  const char* parent = "unknown";
  if (error && !path.host.empty()) {
    try {
      const auto slash = path.host.find_last_of(L"\\/");
      if (slash != std::wstring::npos) {
        const DWORD attrs = GetFileAttributesW(path.host.substr(0, slash + 1).c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES) parent = (attrs & FILE_ATTRIBUTE_DIRECTORY) ? "yes" : "no";
        else if (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND) parent = "no";
      }
    } catch (...) {
    }
  }
  std::fprintf(stderr, "[elixir][app-storage] operation=%s category=%s errno=%d parent_dir=%s tid=%lu utc_ms=%lld\n",
               operation, category, error, parent, GetCurrentThreadId(), DiagnosticUtcMillis());
  SetGuestErrno(saved_guest);
  errno = saved_errno;
  SetLastError(saved_last);
}

void RecordContentCacheIoFailure(ContentCacheOperation operation, int guest_errno,
                                 std::string_view path, std::string_view other_path) noexcept {
  if (guest_errno == 0 || guest_errno == guest::kENOENT || guest_errno == guest::kEEXIST ||
      guest_errno == guest::kEINTR || guest_errno == guest::kEAGAIN) {
    return;
  }
  if (!IsContentCachePath(path) && !IsContentCachePath(other_path)) return;

  ContentCacheCounter(operation).fetch_add(1, std::memory_order_relaxed);
  const DWORD saved_last_error = GetLastError();
  const int saved_host_errno = errno;
  const int saved_guest_errno = GuestErrno();
  std::fprintf(stderr, "[elixir][content-cache] operation=%s errno=%d utc_ms=%lld\n",
               kContentCacheOperations[static_cast<unsigned>(operation)], guest_errno, DiagnosticUtcMillis());
  SetGuestErrno(saved_guest_errno);
  errno = saved_host_errno;
  SetLastError(saved_last_error);
}

const char* ProcResourceLabel(std::string_view path) noexcept {
  if (path.compare(0, 6, "/proc/") != 0) return path == "/proc" ? "proc-other" : nullptr;
  path.remove_prefix(6);
  if (path == "cpuinfo") return "cpuinfo";
  if (path == "meminfo") return "meminfo";
  if (path == "stat") return "stat";
  if (path == "version") return "version";
  if (path == "uptime") return "uptime";
  if (path == "loadavg") return "loadavg";

  const std::size_t slash = path.find('/');
  const std::string_view group = path.substr(0, slash);
  if (group == "self" || group == "thread-self" ||
      (!group.empty() && std::all_of(group.begin(), group.end(), [](char c) { return c >= '0' && c <= '9'; }))) {
    if (slash == std::string_view::npos) return "process-other";
    path.remove_prefix(slash + 1);
    if (path == "status") return "process-status";
    if (path == "stat") return "process-stat";
    if (path == "statm") return "process-statm";
    if (path == "fd" || path.compare(0, 3, "fd/") == 0) return "fd";
    return "process-other";
  }
  return "proc-other";
}

namespace {

class SavedIoErrors {
 public:
  ~SavedIoErrors() {
    SetGuestErrno(guest_errno);
    errno = host_errno;
    SetLastError(last_error);
  }

  const DWORD last_error = GetLastError();
  const int host_errno = errno;
  const int guest_errno = GuestErrno();
};

std::string EncodeProcLogPath(std::string_view path) {
  constexpr char hex[] = "0123456789ABCDEF";
  std::string encoded;
  encoded.reserve(path.size());
  for (unsigned char c : path) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
        c == '/' || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded.push_back(static_cast<char>(c));
    } else {
      encoded.push_back('%');
      encoded.push_back(hex[c >> 4]);
      encoded.push_back(hex[c & 15]);
    }
  }
  return encoded;
}

}

std::atomic<int> g_proc_trace_override{-1};

bool ProcTraceEnabled() noexcept {
  const int override_value = g_proc_trace_override.load(std::memory_order_relaxed);
  if (override_value >= 0) return override_value != 0;
  static const bool enabled = [] {
    char value[8] = {};
    const DWORD length = GetEnvironmentVariableA("ELIXIR_PROC_TRACE", value, sizeof(value));
    return length > 0 && length < sizeof(value) && value[0] == '1';
  }();
  return enabled;
}

bool FirstMissingProcPath(std::string_view path) noexcept {
  static std::mutex lock;
  static std::set<std::string>* seen = nullptr;
  try {
    std::lock_guard<std::mutex> guard(lock);
    if (seen == nullptr) seen = new std::set<std::string>();
    if (seen->size() >= 64) return false;
    return seen->emplace(path).second;
  } catch (...) {
    return false;
  }
}

void SetProcTrace(bool enabled) noexcept { g_proc_trace_override.store(enabled ? 1 : 0, std::memory_order_relaxed); }

void RecordMissingSystemPath(std::string_view guest_path, const char* operation, int guest_errno) noexcept {
  if (guest_errno != guest::kENOENT || guest_path.size() < 2 || guest_path[0] != '/') return;
  for (const std::string_view own : {"/data/data/", "/data/user/", "/data/local/tmp", "/sdcard", "/storage/", "/mnt/"}) {
    if (guest_path.compare(0, own.size(), own) == 0) return;
  }
  const bool proc = guest_path == "/proc" || guest_path.compare(0, 6, "/proc/") == 0;
  if (proc && std::strcmp(operation, "open") == 0) return;
  static std::atomic<bool> full{false};
  if (full.load(std::memory_order_relaxed)) return;
  static std::mutex lock;
  static std::set<std::string>* seen = nullptr;
  const SavedIoErrors errors;
  try {
    {
      std::lock_guard<std::mutex> guard(lock);
      if (seen == nullptr) seen = new std::set<std::string>();
      if (seen->size() >= 256) {
        full.store(true, std::memory_order_relaxed);
        return;
      }
      if (!seen->emplace(guest_path).second) return;
    }
    const std::string encoded = EncodeProcLogPath(guest_path);
    std::fprintf(stderr, "[elixir][missing-path] operation=%s utc_ms=%lld path=%s\n", operation, DiagnosticUtcMillis(),
                 encoded.c_str());
  } catch (...) {
  }
}

void RecordProcOpenResult(const char* resource, int guest_errno, std::string_view requested_guest) noexcept {
  if (resource == nullptr) return;
  const bool trace = ProcTraceEnabled();
  const std::string_view group(resource);
  const bool missing = guest_errno == guest::kENOENT && (group == "proc-other" || group == "process-other") &&
                       (requested_guest == "/proc" || requested_guest.compare(0, 6, "/proc/") == 0);
  if (!trace && !missing) return;
  const SavedIoErrors errors;
  const auto now = DiagnosticUtcMillis();
  if (trace && guest_errno == 0) {
    std::fprintf(stderr, "[elixir][proc-io] operation=open resource=%s result=ok utc_ms=%lld\n", resource,
                 now);
  } else if (trace) {
    std::fprintf(stderr, "[elixir][proc-io] operation=open resource=%s result=error errno=%d utc_ms=%lld\n",
                 resource, guest_errno, now);
  }
  if (missing && (trace || FirstMissingProcPath(requested_guest))) {
    try {
      const auto encoded = EncodeProcLogPath(requested_guest);
      std::fprintf(stderr, "[elixir][proc-miss-path] resource=%s utc_ms=%lld path=%s\n",
                   resource, now, encoded.c_str());
    } catch (...) {
    }
  }
}

void RecordProcReadResult(const Description& description, bool positional, std::optional<std::size_t> requested,
                          std::int64_t result) noexcept {
  if (!ProcTraceEnabled()) return;
  const char* resource = ProcResourceLabel(description.guest_path);
  if (resource == nullptr) return;
  const SavedIoErrors errors;
  const char* operation = positional ? "pread" : "read";
  const auto count = static_cast<unsigned long long>(requested.value_or(0));
  if (result < 0 && !requested) {
    std::fprintf(stderr,
                 "[elixir][proc-io] operation=%s resource=%s result=error requested=unknown errno=%d utc_ms=%lld\n",
                 operation, resource, errors.guest_errno, DiagnosticUtcMillis());
  } else if (result < 0) {
    std::fprintf(stderr,
                 "[elixir][proc-io] operation=%s resource=%s result=error requested=%llu errno=%d utc_ms=%lld\n",
                 operation, resource, count, errors.guest_errno, DiagnosticUtcMillis());
  } else {
    const char* outcome = result > 0 ? "ok" : (count == 0 ? "empty" : "eof");
    std::fprintf(stderr,
                 "[elixir][proc-io] operation=%s resource=%s result=%s requested=%llu bytes=%llu utc_ms=%lld\n",
                 operation, resource, outcome, count, static_cast<unsigned long long>(result), DiagnosticUtcMillis());
  }
}

bool Resolve(std::string_view path, Resolved* out, int* err) {
  return ResolveWithBase(GuestCwd(), path, out, err);
}

bool ResolveFrom(std::string_view base, std::string_view path, Resolved* out, int* err) {
  return ResolveWithBase(base, path, out, err);
}

bool ResolveAt(int dirfd, const char* path, Resolved* out, int* err) {
  if (path == nullptr) {
    *err = guest::kEFAULT;
    return false;
  }
  const std::string_view view(path);
  if (dirfd == guest::kAT_FDCWD || view.empty() || view[0] == '/' || IsHostAbsolute(view)) {
    return Resolve(view, out, err);
  }
  DescriptionPtr description = GetDescription(dirfd);
  if (description == nullptr) {
    *err = guest::kEBADF;
    return false;
  }
  if (!IsDirectoryDescription(*description)) {
    *err = guest::kENOTDIR;
    return false;
  }
  return ResolveWithBase(description->guest_path, view, out, err);
}

std::string GuestCwd() {
  VfsState& state = State();
  SharedLock lock(&state.lock);
  return state.cwd;
}

void SetGuestCwd(std::string path) {
  VfsState& state = State();
  AcquireSRWLockExclusive(&state.lock);
  state.cwd = std::move(path);
  ReleaseSRWLockExclusive(&state.lock);
}

std::vector<std::string> MountChildren(const std::string& guest_dir) {
  EnsureConfigured();
  const std::vector<std::string> base = SplitComponents(guest_dir);
  std::vector<std::string> names;
  VfsState& state = State();
  SharedLock lock(&state.lock);
  auto add = [&](const std::string& name) {
    if (std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);
  };
  for (const MountEntry& mount : state.mounts) {
    if (mount.components.size() <= base.size()) continue;
    if (!std::equal(base.begin(), base.end(), mount.components.begin())) continue;
    add(mount.components[base.size()]);
  }
  if (base.empty()) {
    for (const char* root : kSyntheticRoots) add(root);
  }
  return names;
}

bool HasMountBelow(const std::string& guest) {
  EnsureConfigured();
  VfsState& state = State();
  SharedLock lock(&state.lock);
  return HasMountBelowLocked(state, SplitComponents(guest));
}

std::wstring HostTempDirectory() {
  wchar_t buffer[MAX_PATH + 2];
  DWORD length = GetTempPathW(MAX_PATH + 1, buffer);
  std::wstring plain = length > 0 && length <= MAX_PATH ? std::wstring(buffer, length) : std::wstring(L"C:\\Windows\\Temp");
  while (plain.size() > 3 && plain.back() == L'\\') plain.pop_back();
  return WithLongPrefix(FullHostPath(plain));
}

std::vector<std::pair<std::string, bool>> MountList() {
  EnsureConfigured();
  VfsState& state = State();
  SharedLock lock(&state.lock);
  std::vector<std::pair<std::string, bool>> out;
  for (const MountEntry& mount : state.mounts) out.emplace_back(mount.guest, mount.read_only);
  return out;
}

std::wstring StripLongPrefixPath(std::wstring path) { return StripLongPrefix(std::move(path)); }

std::vector<vfs::Mount> DefaultMounts(std::string_view package, std::string_view host_root) {
  std::string root(host_root);
  if (root.empty()) {
    wchar_t buffer[MAX_PATH];
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
      root = WideToUtf8(std::wstring_view(buffer, length)) + "\\Elixir";
    } else {
      root = WideToUtf8(StripLongPrefix(HostTempDirectory())) + "\\Elixir";
    }
  }
  std::replace(root.begin(), root.end(), '\\', '/');
  const std::string pkg = package.empty() ? std::string("com.elixir.guest") : std::string(package);
  const std::string data = root + "/data/" + pkg;
  const std::string sdcard = root + "/sdcard";
  return {
      {"/data/data/" + pkg, data, false},
      {"/data/user/0/" + pkg, data, false},
      {"/sdcard", sdcard, false},
      {"/storage/emulated/0", sdcard, false},
      {"/storage/self/primary", sdcard, false},
      {"/data/local/tmp", root + "/tmp", false},
  };
}

bool EnsureMountDirectories(const std::vector<vfs::Mount>& mounts) {
  bool ok = true;
  for (const vfs::Mount& mount : mounts) {
    const std::wstring root = WithLongPrefix(NormalizeHostRoot(mount.host_path));
    std::size_t start = root.rfind(L"\\\\?\\UNC\\", 0) == 0 ? 8 : 4;
    std::size_t position = root.find(L'\\', start + 3);
    while (true) {
      const std::wstring partial = position == std::wstring::npos ? root : root.substr(0, position);
      if (!CreateDirectoryW(partial.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS &&
          GetLastError() != ERROR_ACCESS_DENIED) {
        if (position == std::wstring::npos) ok = false;
      }
      if (position == std::wstring::npos) break;
      position = root.find(L'\\', position + 1);
    }
    const DWORD attributes = GetFileAttributesW(root.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) ok = false;
  }
  return ok;
}

}

namespace elixir::bionic::vfs {

void SetMounts(const Mount* mounts, std::size_t count) {
  files::VfsState& state = files::State();
  std::vector<files::MountEntry> entries;
  entries.reserve(count);
  for (std::size_t i = 0; i < count; ++i) entries.push_back(files::MakeEntry(mounts[i], i));
  AcquireSRWLockExclusive(&state.lock);
  state.mounts = std::move(entries);
  state.configured = true;
  ReleaseSRWLockExclusive(&state.lock);
}

bool ResolveToHost(std::string_view guest_path, std::string* host_path, int* guest_errno, bool* read_only) {
  int error = 0;
  files::Resolved resolved;
  bool ok = false;
  try {
    ok = files::Resolve(guest_path, &resolved, &error);
    if (ok && resolved.kind != files::NodeKind::kHost) {
      ok = false;
      error = guest::kENOENT;
    }
    if (ok) {
      std::string text = files::WideToUtf8(files::StripLongPrefix(resolved.host));
      std::replace(text.begin(), text.end(), '\\', '/');
      if (host_path != nullptr) *host_path = std::move(text);
      if (read_only != nullptr) *read_only = resolved.read_only;
    }
  } catch (...) {
    ok = false;
    error = guest::kENOMEM;
  }
  if (!ok && guest_errno != nullptr) *guest_errno = error;
  return ok;
}

std::uint64_t OverlayLookupCount() noexcept {
  return files::g_overlay_lookups.load(std::memory_order_relaxed);
}

std::string HostToGuest(std::string_view host_path) {
  const std::wstring plain = files::StripLongPrefix(files::Utf8ToWide(host_path));
  const bool absolute = (plain.size() >= 3 && plain[1] == L':' && plain[2] == L'\\') || plain.rfind(L"\\\\", 0) == 0;
  if (absolute) {
    files::EnsureConfigured();
    files::VfsState& state = files::State();
    files::SharedLock lock(&state.lock);
    const files::MountEntry* best = nullptr;
    std::size_t best_length = 0;
    for (const files::MountEntry& mount : state.mounts) {
      for (const std::wstring* root : {&mount.host_root, &mount.final_root, &mount.overlay_root}) {
        if (root->empty() || plain.size() < root->size()) continue;
        if (!files::EqualsIgnoreCase(std::wstring_view(plain).substr(0, root->size()), *root)) continue;
        if (plain.size() != root->size() && plain[root->size()] != L'\\' && root->back() != L'\\') continue;
        if (best == nullptr || root->size() > best_length ||
            (root->size() == best_length && mount.order < best->order)) {
          best = &mount;
          best_length = root->size();
        }
      }
    }
    if (best != nullptr) {
      std::string guest = best->guest == "/" ? std::string() : best->guest;
      std::size_t i = best_length;
      while (i < plain.size()) {
        while (i < plain.size() && plain[i] == L'\\') ++i;
        if (i >= plain.size()) break;
        std::size_t j = plain.find(L'\\', i);
        if (j == std::wstring::npos) j = plain.size();
        guest.push_back('/');
        guest += files::UnescapeHostName(std::wstring_view(plain).substr(i, j - i));
        i = j;
      }
      return guest.empty() ? std::string("/") : guest;
    }
  }
  std::string out = files::UnescapeHostName(plain);
  std::replace(out.begin(), out.end(), '\\', '/');
  return out;
}

}
