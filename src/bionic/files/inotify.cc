// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/files/files_internal.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <climits>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace elixir::bionic::files {
namespace {

constexpr std::uint32_t kIN_ACCESS = 0x00000001;
constexpr std::uint32_t kIN_MODIFY = 0x00000002;
constexpr std::uint32_t kIN_ATTRIB = 0x00000004;
constexpr std::uint32_t kIN_CLOSE_WRITE = 0x00000008;
constexpr std::uint32_t kIN_CLOSE_NOWRITE = 0x00000010;
constexpr std::uint32_t kIN_OPEN = 0x00000020;
constexpr std::uint32_t kIN_MOVED_FROM = 0x00000040;
constexpr std::uint32_t kIN_MOVED_TO = 0x00000080;
constexpr std::uint32_t kIN_CREATE = 0x00000100;
constexpr std::uint32_t kIN_DELETE = 0x00000200;
constexpr std::uint32_t kIN_DELETE_SELF = 0x00000400;
constexpr std::uint32_t kIN_MOVE_SELF = 0x00000800;
constexpr std::uint32_t kIN_UNMOUNT = 0x00002000;
constexpr std::uint32_t kIN_Q_OVERFLOW = 0x00004000;
constexpr std::uint32_t kIN_IGNORED = 0x00008000;
constexpr std::uint32_t kIN_ONLYDIR = 0x01000000;
constexpr std::uint32_t kIN_DONT_FOLLOW = 0x02000000;
constexpr std::uint32_t kIN_EXCL_UNLINK = 0x04000000;
constexpr std::uint32_t kIN_MASK_CREATE = 0x10000000;
constexpr std::uint32_t kIN_MASK_ADD = 0x20000000;
constexpr std::uint32_t kIN_ISDIR = 0x40000000;
constexpr std::uint32_t kIN_ONESHOT = 0x80000000;
constexpr std::uint32_t kIN_ALL_EVENTS = kIN_ACCESS | kIN_MODIFY | kIN_ATTRIB | kIN_CLOSE_WRITE | kIN_CLOSE_NOWRITE |
                                         kIN_OPEN | kIN_MOVED_FROM | kIN_MOVED_TO | kIN_DELETE | kIN_CREATE |
                                         kIN_DELETE_SELF | kIN_MOVE_SELF;
constexpr std::uint32_t kAllInotifyBits = kIN_ALL_EVENTS | kIN_UNMOUNT | kIN_Q_OVERFLOW | kIN_IGNORED | kIN_ONLYDIR |
                                          kIN_DONT_FOLLOW | kIN_EXCL_UNLINK | kIN_MASK_CREATE | kIN_MASK_ADD |
                                          kIN_ISDIR | kIN_ONESHOT;
constexpr std::uint32_t kStoredBits = kIN_ALL_EVENTS | kIN_ONESHOT | kIN_EXCL_UNLINK;
constexpr std::uint32_t kUnmaskable = kIN_IGNORED | kIN_Q_OVERFLOW | kIN_UNMOUNT;

constexpr int kMaxInstances = 128;
constexpr int kMaxWatches = 8192;
constexpr std::size_t kMaxQueuedEvents = 16384;
constexpr std::size_t kEventHeader = 16;
constexpr DWORD kNotifyBufferBytes = 64 * 1024;

struct GuestInotifyEvent {
  std::int32_t wd;
  std::uint32_t mask;
  std::uint32_t cookie;
  std::uint32_t len;
};
static_assert(sizeof(GuestInotifyEvent) == kEventHeader, "struct inotify_event");

struct NotifyExtendedInformation {
  DWORD NextEntryOffset;
  DWORD Action;
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastModificationTime;
  LARGE_INTEGER LastChangeTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER AllocatedLength;
  LARGE_INTEGER FileSize;
  DWORD FileAttributes;
  DWORD ReparsePointTagOrEaSize;
  LARGE_INTEGER FileId;
  LARGE_INTEGER ParentFileId;
  DWORD FileNameLength;
  WCHAR FileName[1];
};
static_assert(offsetof(NotifyExtendedInformation, FileName) == 84, "FILE_NOTIFY_EXTENDED_INFORMATION");
constexpr int kReadDirectoryNotifyExtendedInformation = 2;
using ReadDirectoryChangesExFn = BOOL(WINAPI*)(HANDLE, LPVOID, DWORD, BOOL, DWORD, LPDWORD, LPOVERLAPPED,
                                               LPOVERLAPPED_COMPLETION_ROUTINE, int);

ReadDirectoryChangesExFn ReadDirectoryChangesEx() {
  static const ReadDirectoryChangesExFn fn = reinterpret_cast<ReadDirectoryChangesExFn>(
      reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "ReadDirectoryChangesExW")));
  return fn;
}

constexpr ULONG_PTR kMessageKey = 1;
constexpr ULONGLONG kCleanupEchoMs = 1500;
constexpr LONG kStatusPending = 0x103;
constexpr wchar_t kTmpfilePrefix[] = L".elixir-tmpfile-";

struct Event {
  int wd;
  std::uint32_t mask;
  std::uint32_t cookie;
  std::string name;
};

std::size_t EventBytes(const Event& e) {
  if (e.name.empty()) return kEventHeader;
  return kEventHeader + (e.name.size() + 1 + kEventHeader - 1) / kEventHeader * kEventHeader;
}

struct Monitor;

struct Instance;

struct Watch {
  Instance* instance = nullptr;
  int wd = 0;
  std::uint32_t mask = 0;
  Monitor* monitor = nullptr;
  std::wstring name;
  std::wstring name_key;
  bool directory = false;
  std::string identity;
  std::uint32_t move_cookie = 0;
};

struct Instance {
  std::mutex lock;
  std::condition_variable ready;
  std::deque<Event> queue;
  std::size_t queued_bytes = 0;
  std::map<int, Watch*> watches;
  std::map<std::string, int> by_identity;
  int last_wd = 0;
};

struct Channel {
  Monitor* monitor = nullptr;
  bool attributes = false;
  HANDLE handle = INVALID_HANDLE_VALUE;
  OVERLAPPED overlapped{};
  std::unique_ptr<DWORD[]> buffer;
  int packets = 0;
  int stale = 0;
  bool pending = false;
  bool extended = true;
  std::uint32_t rename_cookie = 0;
};

struct Monitor {
  std::wstring dir;
  std::vector<std::wstring> keys;
  Channel main;
  Channel attrib;
  std::vector<Watch*> watches;
  std::unordered_map<std::wstring, ULONGLONG> guest_writes;
  bool dead = false;
  bool busy = false;
};

struct Message {
  std::wstring dir_key;
  std::wstring name;
  std::wstring self_key;
  std::uint32_t mask = 0;
  bool directory = false;
};

struct Registry {
  std::mutex lock;
  std::unordered_map<std::wstring, Monitor*> by_key;
  int instances = 0;
  int watches = 0;
  std::uint32_t cookie = 0;
  HANDLE port = nullptr;
};

Registry& Reg() {
  static Registry* registry = new Registry();
  return *registry;
}

std::atomic<int> g_monitors{0};

std::wstring Upper(std::wstring_view text) {
  std::wstring out(text);
  if (!out.empty()) {
    LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, text.data(), static_cast<int>(text.size()), out.data(),
                  static_cast<int>(out.size()), nullptr, nullptr, 0);
  }
  return out;
}

std::wstring KeyOf(std::wstring_view path) {
  std::wstring p(path);
  if (p.rfind(L"\\\\?\\UNC\\", 0) == 0) {
    p = L"\\\\" + p.substr(8);
  } else if (p.rfind(L"\\\\?\\", 0) == 0) {
    p = p.substr(4);
  }
  std::replace(p.begin(), p.end(), L'/', L'\\');
  while (p.size() > 3 && p.back() == L'\\') p.pop_back();
  return Upper(p);
}

std::pair<std::wstring, std::wstring> SplitParent(const std::wstring& path) {
  std::wstring trimmed = path;
  while (trimmed.size() > 1 && (trimmed.back() == L'\\' || trimmed.back() == L'/')) trimmed.pop_back();
  const std::size_t slash = trimmed.find_last_of(L"\\/");
  if (slash == std::wstring::npos) return {std::wstring(), trimmed};
  return {trimmed.substr(0, slash), trimmed.substr(slash + 1)};
}

std::wstring FinalPath(HANDLE handle) {
  wchar_t buffer[1024];
  DWORD length = GetFinalPathNameByHandleW(handle, buffer, 1024, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (length == 0) return {};
  if (length < 1024) return std::wstring(buffer, length);
  std::wstring big(length, L'\0');
  length = GetFinalPathNameByHandleW(handle, big.data(), length, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  big.resize(length);
  return big;
}

bool IdentityOf(HANDLE handle, std::string* out) {
  FILE_ID_INFO info{};
  if (GetFileInformationByHandleEx(handle, FileIdInfo, &info, sizeof(info))) {
    out->assign(reinterpret_cast<const char*>(&info.VolumeSerialNumber), sizeof(info.VolumeSerialNumber));
    out->append(reinterpret_cast<const char*>(&info.FileId), sizeof(info.FileId));
    return true;
  }
  BY_HANDLE_FILE_INFORMATION by_handle{};
  if (!GetFileInformationByHandle(handle, &by_handle)) return false;
  const std::uint64_t parts[2] = {by_handle.dwVolumeSerialNumber,
                                  (static_cast<std::uint64_t>(by_handle.nFileIndexHigh) << 32) | by_handle.nFileIndexLow};
  out->assign(reinterpret_cast<const char*>(parts), sizeof(parts));
  return true;
}

std::uint32_t NextCookie() {
  Registry& reg = Reg();
  if (++reg.cookie == 0) ++reg.cookie;
  return reg.cookie;
}

void Enqueue(Instance* instance, int wd, std::uint32_t mask, std::uint32_t cookie, std::string name) {
  std::lock_guard<std::mutex> guard(instance->lock);
  if (!instance->queue.empty()) {
    const Event& last = instance->queue.back();
    if (last.wd == wd && last.mask == mask && last.cookie == cookie && last.name == name &&
        (last.mask & kIN_IGNORED) == 0) {
      return;
    }
  }
  if (instance->queue.size() >= kMaxQueuedEvents) {
    if (instance->queue.back().mask != kIN_Q_OVERFLOW) {
      instance->queue.push_back(Event{-1, kIN_Q_OVERFLOW, 0, {}});
      instance->queued_bytes += kEventHeader;
      instance->ready.notify_all();
    }
    return;
  }
  instance->queue.push_back(Event{wd, mask, cookie, std::move(name)});
  instance->queued_bytes += EventBytes(instance->queue.back());
  instance->ready.notify_all();
}

void MaybeFree(Monitor* m) {
  if (m->dead && !m->busy && m->main.packets == 0 && m->attrib.packets == 0) delete m;
}

void CloseChannel(Channel* c) {
  if (c->handle == INVALID_HANDLE_VALUE) return;
  CancelIoEx(c->handle, nullptr);
  CloseHandle(c->handle);
  c->handle = INVALID_HANDLE_VALUE;
}

void RetireMonitor(Monitor* m) {
  if (m->dead) return;
  m->dead = true;
  Registry& reg = Reg();
  for (const std::wstring& key : m->keys) {
    auto it = reg.by_key.find(key);
    if (it != reg.by_key.end() && it->second == m) reg.by_key.erase(it);
  }
  CloseChannel(&m->main);
  CloseChannel(&m->attrib);
  g_monitors.fetch_sub(1);
  MaybeFree(m);
}

bool Arm(Channel* c) {
  if (c->handle == INVALID_HANDLE_VALUE) return false;
  const DWORD filter = c->attributes ? (FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SECURITY)
                                     : (FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                                        FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE);
  for (;;) {
    std::memset(&c->overlapped, 0, sizeof(c->overlapped));
    c->overlapped.Internal = static_cast<ULONG_PTR>(kStatusPending);
    BOOL ok = FALSE;
    const ReadDirectoryChangesExFn extended = ReadDirectoryChangesEx();
    if (c->extended && extended != nullptr) {
      ok = extended(c->handle, c->buffer.get(), kNotifyBufferBytes, FALSE, filter, nullptr, &c->overlapped, nullptr,
                    kReadDirectoryNotifyExtendedInformation);
    } else {
      c->extended = false;
      ok = ReadDirectoryChangesW(c->handle, c->buffer.get(), kNotifyBufferBytes, FALSE, filter, nullptr,
                                 &c->overlapped, nullptr);
    }
    const DWORD error = ok ? ERROR_SUCCESS : GetLastError();
    if (ok || error == ERROR_IO_PENDING) {
      ++c->packets;
      c->pending = true;
      return true;
    }
    if (c->extended && (error == ERROR_INVALID_PARAMETER || error == ERROR_INVALID_FUNCTION ||
                        error == ERROR_NOT_SUPPORTED || error == ERROR_INVALID_LEVEL)) {
      c->extended = false;
      continue;
    }
    SetLastError(error);
    return false;
  }
}

bool OpenChannel(Channel* c, int* err) {
  Registry& reg = Reg();
  if (reg.port == nullptr) {
    *err = guest::kENOMEM;
    return false;
  }
  HANDLE handle = CreateFileW(c->monitor->dir.c_str(), FILE_LIST_DIRECTORY,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                              FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    const DWORD error = GetLastError();
    *err = error == ERROR_ACCESS_DENIED ? guest::kEACCES : PathErrnoFromWin32(error, c->monitor->dir);
    return false;
  }
  if (CreateIoCompletionPort(handle, reg.port, reinterpret_cast<ULONG_PTR>(c), 0) == nullptr) {
    *err = ErrnoFromWin32(GetLastError());
    CloseHandle(handle);
    return false;
  }
  c->handle = handle;
  if (c->buffer == nullptr) c->buffer = std::make_unique<DWORD[]>(kNotifyBufferBytes / sizeof(DWORD));
  if (!Arm(c)) {
    *err = ErrnoFromWin32(GetLastError());
    CloseHandle(handle);
    c->handle = INVALID_HANDLE_VALUE;
    return false;
  }
  return true;
}

void RemoveWatch(Watch* w, bool ignored) {
  Instance* instance = w->instance;
  if (ignored) Enqueue(instance, w->wd, kIN_IGNORED, 0, {});
  instance->watches.erase(w->wd);
  instance->by_identity.erase(w->identity);
  Monitor* m = w->monitor;
  if (m != nullptr) m->watches.erase(std::remove(m->watches.begin(), m->watches.end(), w), m->watches.end());
  --Reg().watches;
  delete w;
  if (m != nullptr && m->watches.empty()) RetireMonitor(m);
}

bool Emit(Watch* w, std::uint32_t mask, std::uint32_t cookie, const std::string& name) {
  const std::uint32_t events = mask & ~kIN_ISDIR;
  if ((events & w->mask) == 0 && (events & kUnmaskable) == 0) return true;
  Enqueue(w->instance, w->wd, mask, cookie, name);
  if ((w->mask & kIN_ONESHOT) != 0 && (events & kUnmaskable) == 0) {
    RemoveWatch(w, true);
    return false;
  }
  return true;
}

void Deliver(Monitor* m, const std::wstring& name, std::uint32_t mask, bool directory, std::uint32_t cookie) {
  const std::string guest_name = UnescapeHostName(name);
  const std::wstring key = Upper(name);
  const std::uint32_t isdir = directory ? kIN_ISDIR : 0;
  const std::vector<Watch*> watches = m->watches;
  for (Watch* w : watches) {
    if (w->name_key.empty()) {
      Emit(w, mask | isdir, cookie, guest_name);
      continue;
    }
    if (w->name_key != key) {
      if (mask == kIN_MOVED_TO && w->move_cookie != 0 && w->move_cookie == cookie) {
        w->name = name;
        w->name_key = key;
        w->move_cookie = 0;
      }
      continue;
    }
    switch (mask) {
      case kIN_DELETE:
        if (Emit(w, kIN_ATTRIB | isdir, 0, {}) && Emit(w, kIN_DELETE_SELF | isdir, 0, {})) RemoveWatch(w, true);
        break;
      case kIN_MOVED_FROM:
        w->move_cookie = cookie;
        Emit(w, kIN_MOVE_SELF | isdir, 0, {});
        break;
      case kIN_CREATE:
      case kIN_MOVED_TO:
        break;
      default:
        Emit(w, mask | isdir, 0, {});
        break;
    }
  }
}

void DeliverSelf(Monitor* m, std::uint32_t mask) {
  const std::vector<Watch*> watches = m->watches;
  for (Watch* w : watches) {
    if (w->name_key.empty()) Emit(w, mask, 0, {});
  }
}

void Overflow(Monitor* m) {
  std::vector<Instance*> seen;
  for (Watch* w : m->watches) {
    if (std::find(seen.begin(), seen.end(), w->instance) != seen.end()) continue;
    seen.push_back(w->instance);
    Enqueue(w->instance, -1, kIN_Q_OVERFLOW, 0, {});
  }
}

bool DirectoryGone(Monitor* m) {
  if (m->main.handle != INVALID_HANDLE_VALUE) {
    FILE_STANDARD_INFO info{};
    if (GetFileInformationByHandleEx(m->main.handle, FileStandardInfo, &info, sizeof(info)) && info.DeletePending) {
      return true;
    }
  }
  if (GetFileAttributesW(m->dir.c_str()) != INVALID_FILE_ATTRIBUTES) return false;
  const DWORD error = GetLastError();
  return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND || error == ERROR_ACCESS_DENIED ||
         error == ERROR_DELETE_PENDING;
}

void MonitorLost(Monitor* m, bool deleted) {
  const std::vector<Watch*> watches = m->watches;
  for (Watch* w : watches) {
    bool alive = true;
    if (w->name_key.empty()) alive = Emit(w, deleted ? (kIN_DELETE_SELF | kIN_ISDIR) : kIN_UNMOUNT, 0, {});
    if (alive) RemoveWatch(w, true);
  }
}

void ChildChanged(Monitor* m, const std::wstring& name, bool moved) {
  Registry& reg = Reg();
  auto it = reg.by_key.find(KeyOf(m->dir + L"\\" + name));
  if (it == reg.by_key.end() || it->second == m || it->second->dead) return;
  Monitor* child = it->second;
  if (moved) {
    DeliverSelf(child, kIN_MOVE_SELF | kIN_ISDIR);
  } else if (DirectoryGone(child)) {
    child->busy = true;
    MonitorLost(child, true);
    child->busy = false;
    MaybeFree(child);
  }
}

void HandleRecord(Channel* c, DWORD action, const std::wstring& name, bool attributes_known, DWORD attributes) {
  if (name.rfind(kTmpfilePrefix, 0) == 0) return;
  Monitor* m = c->monitor;
  bool directory = attributes_known && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  if (!attributes_known && action != FILE_ACTION_REMOVED && action != FILE_ACTION_RENAMED_OLD_NAME) {
    const DWORD now = GetFileAttributesW((m->dir + L"\\" + name).c_str());
    directory = now != INVALID_FILE_ATTRIBUTES && (now & FILE_ATTRIBUTE_DIRECTORY) != 0;
  }
  if (c->attributes) {
    if (action == FILE_ACTION_MODIFIED) Deliver(m, name, kIN_ATTRIB, directory, 0);
    return;
  }
  switch (action) {
    case FILE_ACTION_ADDED:
      Deliver(m, name, kIN_CREATE, directory, 0);
      break;
    case FILE_ACTION_REMOVED:
      Deliver(m, name, kIN_DELETE, directory, 0);
      if (!m->dead && directory) ChildChanged(m, name, false);
      break;
    case FILE_ACTION_MODIFIED: {
      if (directory) break;
      if (auto echo = m->guest_writes.find(Upper(name)); echo != m->guest_writes.end()) {
        if (echo->second == 0 || GetTickCount64() < echo->second) break;
        m->guest_writes.erase(echo);
      }
      Deliver(m, name, kIN_MODIFY, false, 0);
      break;
    }
    case FILE_ACTION_RENAMED_OLD_NAME:
      c->rename_cookie = NextCookie();
      Deliver(m, name, kIN_MOVED_FROM, directory, c->rename_cookie);
      if (!m->dead && directory) ChildChanged(m, name, true);
      break;
    case FILE_ACTION_RENAMED_NEW_NAME: {
      const std::uint32_t cookie = c->rename_cookie != 0 ? c->rename_cookie : NextCookie();
      c->rename_cookie = 0;
      Deliver(m, name, kIN_MOVED_TO, directory, cookie);
      break;
    }
    default:
      break;
  }
}

void Process(Channel* c, DWORD transferred, DWORD error) {
  Monitor* m = c->monitor;
  if (error == ERROR_NOTIFY_ENUM_DIR || (error == ERROR_SUCCESS && transferred == 0)) {
    if (!c->attributes && DirectoryGone(m)) {
      MonitorLost(m, true);
    } else {
      Overflow(m);
    }
    return;
  }
  if (error != ERROR_SUCCESS && c->extended &&
      (error == ERROR_INVALID_PARAMETER || error == ERROR_INVALID_FUNCTION || error == ERROR_NOT_SUPPORTED)) {
    c->extended = false;
    return;
  }
  if (error != ERROR_SUCCESS) {
    if (c->attributes) {
      CloseChannel(c);
    } else {
      MonitorLost(m, DirectoryGone(m) || error == ERROR_ACCESS_DENIED || error == ERROR_DELETE_PENDING);
    }
    return;
  }
  const auto* base = reinterpret_cast<const unsigned char*>(c->buffer.get());
  const std::size_t limit = std::min<std::size_t>(transferred, kNotifyBufferBytes);
  const std::size_t header = c->extended ? offsetof(NotifyExtendedInformation, FileName)
                                         : offsetof(FILE_NOTIFY_INFORMATION, FileName);
  std::size_t offset = 0;
  while (!m->dead && offset + header <= limit) {
    DWORD next = 0;
    if (c->extended) {
      const auto* record = reinterpret_cast<const NotifyExtendedInformation*>(base + offset);
      if (offset + offsetof(NotifyExtendedInformation, FileName) + record->FileNameLength > limit) break;
      HandleRecord(c, record->Action, std::wstring(record->FileName, record->FileNameLength / sizeof(WCHAR)), true,
                   record->FileAttributes);
      next = record->NextEntryOffset;
    } else {
      const auto* record = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(base + offset);
      if (offset + offsetof(FILE_NOTIFY_INFORMATION, FileName) + record->FileNameLength > limit) break;
      HandleRecord(c, record->Action, std::wstring(record->FileName, record->FileNameLength / sizeof(WCHAR)), false,
                   0);
      next = record->NextEntryOffset;
    }
    if (next == 0) break;
    offset += next;
  }
}

void Rearm(Channel* c) {
  Monitor* m = c->monitor;
  if (m->dead || c->handle == INVALID_HANDLE_VALUE) return;
  if (Arm(c)) return;
  if (c->attributes) {
    CloseChannel(c);
  } else {
    MonitorLost(m, DirectoryGone(m));
  }
}

void Drain(Monitor* m) {
  for (Channel* c : {&m->main, &m->attrib}) {
    for (int round = 0; round < 8 && !m->dead; ++round) {
      if (c->handle == INVALID_HANDLE_VALUE || !c->pending) break;
      MemoryBarrier();
      if (!HasOverlappedIoCompleted(&c->overlapped)) break;
      c->pending = false;
      ++c->stale;
      DWORD transferred = 0;
      DWORD error = ERROR_SUCCESS;
      if (!GetOverlappedResult(c->handle, &c->overlapped, &transferred, FALSE)) error = GetLastError();
      Process(c, transferred, error);
      Rearm(c);
    }
  }
}

void OnPacket(Channel* c) {
  Monitor* m = c->monitor;
  --c->packets;
  if (c->stale > 0) {
    --c->stale;
  } else if (!m->dead && c->handle != INVALID_HANDLE_VALUE) {
    c->pending = false;
    m->busy = true;
    DWORD transferred = 0;
    DWORD error = ERROR_SUCCESS;
    if (!GetOverlappedResult(c->handle, &c->overlapped, &transferred, FALSE)) error = GetLastError();
    Process(c, transferred, error);
    Rearm(c);
    m->busy = false;
  } else {
    c->pending = false;
  }
  MaybeFree(m);
}

Monitor* FindMonitor(const std::wstring& key) {
  Registry& reg = Reg();
  auto it = reg.by_key.find(key);
  return it == reg.by_key.end() || it->second->dead ? nullptr : it->second;
}

void NoteGuestWriter(Monitor* m, const Message& message) {
  const ULONGLONG now = GetTickCount64();
  for (auto it = m->guest_writes.begin(); it != m->guest_writes.end();) {
    it = it->second != 0 && it->second <= now ? m->guest_writes.erase(it) : std::next(it);
  }
  if (message.mask == kIN_MODIFY) {
    m->guest_writes[Upper(message.name)] = 0;
  } else if (message.mask == kIN_CLOSE_WRITE) {
    if (auto it = m->guest_writes.find(Upper(message.name)); it != m->guest_writes.end()) {
      it->second = now + kCleanupEchoMs;
    }
  }
}

void OnMessage(const Message& message) {
  if (Monitor* m = FindMonitor(message.dir_key); m != nullptr) {
    m->busy = true;
    Drain(m);
    if (!m->dead) {
      if (!message.directory) NoteGuestWriter(m, message);
      Deliver(m, message.name, message.mask, message.directory, 0);
    }
    m->busy = false;
    MaybeFree(m);
  }
  if (message.directory && !message.self_key.empty()) {
    if (Monitor* self = FindMonitor(message.self_key); self != nullptr) {
      self->busy = true;
      Drain(self);
      if (!self->dead) DeliverSelf(self, message.mask | kIN_ISDIR);
      self->busy = false;
      MaybeFree(self);
    }
  }
}

DWORD WINAPI WorkerMain(void*) {
  OVERLAPPED_ENTRY entries[64];
  for (;;) {
    ULONG count = 0;
    if (!GetQueuedCompletionStatusEx(Reg().port, entries, 64, &count, INFINITE, FALSE)) {
      if (GetLastError() == ERROR_ABANDONED_WAIT_0) return 0;
      continue;
    }
    std::lock_guard<std::mutex> guard(Reg().lock);
    for (ULONG i = 0; i < count; ++i) {
      try {
        if (entries[i].lpCompletionKey == kMessageKey) {
          std::unique_ptr<Message> message(reinterpret_cast<Message*>(entries[i].lpOverlapped));
          OnMessage(*message);
        } else {
          OnPacket(reinterpret_cast<Channel*>(entries[i].lpCompletionKey));
        }
      } catch (...) {
      }
    }
  }
}

bool EnsureWorker() {
  Registry& reg = Reg();
  if (reg.port != nullptr) return true;
  HANDLE port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
  if (port == nullptr) return false;
  reg.port = port;
  HANDLE thread = CreateThread(nullptr, 256 * 1024, &WorkerMain, nullptr, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
  if (thread == nullptr) {
    CloseHandle(port);
    reg.port = nullptr;
    return false;
  }
  CloseHandle(thread);
  return true;
}

Monitor* AcquireMonitor(const std::wstring& dir, const std::wstring& alias_key, int* err) {
  Registry& reg = Reg();
  const std::wstring key = KeyOf(dir);
  Monitor* m = FindMonitor(key);
  if (m == nullptr && !alias_key.empty()) m = FindMonitor(alias_key);
  if (m == nullptr) {
    if (!EnsureWorker()) {
      *err = guest::kENOMEM;
      return nullptr;
    }
    auto created = std::make_unique<Monitor>();
    created->dir = dir;
    created->main.monitor = created.get();
    created->attrib.monitor = created.get();
    created->attrib.attributes = true;
    if (!OpenChannel(&created->main, err)) return nullptr;
    m = created.release();
    g_monitors.fetch_add(1);
  }
  for (const std::wstring* k : {&key, &alias_key}) {
    if (k->empty() || std::find(m->keys.begin(), m->keys.end(), *k) != m->keys.end()) continue;
    m->keys.push_back(*k);
    reg.by_key[*k] = m;
  }
  return m;
}

void EnsureAttributeStream(Monitor* m) {
  if (m == nullptr || m->dead || m->attrib.handle != INVALID_HANDLE_VALUE) return;
  int ignored = 0;
  OpenChannel(&m->attrib, &ignored);
}

struct Target {
  bool host = false;
  bool directory = false;
  std::string identity;
  std::wstring monitor_dir;
  std::wstring alias_key;
  std::wstring name;
};

bool ResolveTarget(const char* path, std::uint32_t mask, Target* t, int* err) {
  if (path[0] == '\0') {
    *err = guest::kENOENT;
    return false;
  }
  const bool follow = (mask & kIN_DONT_FOLLOW) == 0;
  Resolved r;
  if (!Resolve(path, &r, err)) return false;
  if (r.kind == NodeKind::kSynthetic) {
    SynthNode node;
    if (!SynthLookup(r.guest, &node)) {
      *err = guest::kENOENT;
      return false;
    }
    if (node.type != SynthType::kLink || follow) {
      if (!FollowSynthetic(&r, &node, err)) return false;
    }
    if (r.kind == NodeKind::kSynthetic) {
      t->directory = node.type == SynthType::kDir;
      t->identity = "synthetic:" + r.guest;
    }
  }
  if (r.kind == NodeKind::kVirtualDir) {
    t->directory = true;
    t->identity = "virtual:" + r.guest;
  }
  if (r.kind == NodeKind::kHost) {
    const DWORD flags = FILE_FLAG_BACKUP_SEMANTICS | (follow ? 0 : FILE_FLAG_OPEN_REPARSE_POINT);
    HANDLE handle = CreateFileW(r.host.c_str(), FILE_READ_ATTRIBUTES,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, flags,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
      *err = PathErrnoFromWin32(GetLastError(), r.host);
      return false;
    }
    FILE_ATTRIBUTE_TAG_INFO tag{};
    const bool have_tag = GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag, sizeof(tag)) != FALSE;
    const bool link = have_tag && !follow && (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 &&
                      IsSymlinkTag(tag.ReparseTag);
    t->directory = have_tag && (tag.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 && !link;
    const bool identified = IdentityOf(handle, &t->identity);
    const std::wstring final_path = FinalPath(handle);
    CloseHandle(handle);
    if (!identified || final_path.empty()) {
      *err = guest::kEACCES;
      return false;
    }
    t->host = true;
    if (t->directory) {
      t->monitor_dir = final_path;
      t->alias_key = KeyOf(r.host);
    } else {
      auto [dir, name] = SplitParent(final_path);
      t->monitor_dir = dir;
      t->name = name;
      t->alias_key = KeyOf(SplitParent(r.host).first);
    }
  }
  if ((mask & kIN_ONLYDIR) != 0 && !t->directory) {
    *err = guest::kENOTDIR;
    return false;
  }
  if (r.trailing_slash && !t->directory) {
    *err = guest::kENOTDIR;
    return false;
  }
  return true;
}

abi::g_ssize InotifyRead(void* handle, std::uint8_t, void* buffer, std::size_t size, bool nonblocking,
                         int* guest_errno) noexcept {
  auto* instance = static_cast<Instance*>(handle);
  try {
    std::unique_lock<std::mutex> lock(instance->lock);
    while (instance->queue.empty()) {
      if (nonblocking) {
        *guest_errno = guest::kEAGAIN;
        return -1;
      }
      instance->ready.wait(lock);
    }
    auto* out = static_cast<unsigned char*>(buffer);
    std::size_t copied = 0;
    while (!instance->queue.empty()) {
      const Event& event = instance->queue.front();
      const std::size_t bytes = EventBytes(event);
      if (bytes > size - copied) {
        if (copied == 0) {
          *guest_errno = guest::kEINVAL;
          return -1;
        }
        break;
      }
      const GuestInotifyEvent header{event.wd, event.mask, event.cookie,
                                     static_cast<std::uint32_t>(bytes - kEventHeader)};
      std::memcpy(out + copied, &header, sizeof(header));
      if (bytes > kEventHeader) {
        std::memset(out + copied + kEventHeader, 0, bytes - kEventHeader);
        std::memcpy(out + copied + kEventHeader, event.name.data(), event.name.size());
      }
      copied += bytes;
      instance->queued_bytes -= bytes;
      instance->queue.pop_front();
    }
    return static_cast<abi::g_ssize>(copied);
  } catch (...) {
    *guest_errno = guest::kENOMEM;
    return -1;
  }
}

int InotifyAvailable(void* handle, std::uint8_t, int* available, int* guest_errno) noexcept {
  auto* instance = static_cast<Instance*>(handle);
  try {
    std::lock_guard<std::mutex> guard(instance->lock);
    *available = static_cast<int>(std::min<std::size_t>(instance->queued_bytes, INT_MAX));
    return 0;
  } catch (...) {
    *guest_errno = guest::kENOMEM;
    return -1;
  }
}

std::uint32_t InotifyPoll(void* handle, std::uint8_t, std::uint32_t) noexcept {
  auto* instance = static_cast<Instance*>(handle);
  try {
    std::lock_guard<std::mutex> guard(instance->lock);
    return instance->queue.empty() ? 0u : static_cast<std::uint32_t>(guest::kPOLLIN | guest::kPOLLRDNORM);
  } catch (...) {
    return static_cast<std::uint32_t>(guest::kPOLLERR);
  }
}

int InotifyClose(void* handle, std::uint8_t, int*) noexcept {
  auto* instance = static_cast<Instance*>(handle);
  try {
    {
      std::lock_guard<std::mutex> guard(Reg().lock);
      while (!instance->watches.empty()) RemoveWatch(instance->watches.begin()->second, false);
      --Reg().instances;
    }
    delete instance;
  } catch (...) {
  }
  return 0;
}

const FdOps kInotifyOps{&InotifyRead, nullptr, &InotifyClose, nullptr, &InotifyAvailable, &InotifyPoll};

int Init(int flags) {
  if ((flags & ~(guest::kO_CLOEXEC | guest::kO_NONBLOCK)) != 0) return FailWith(guest::kEINVAL, -1);
  {
    std::lock_guard<std::mutex> guard(Reg().lock);
    if (Reg().instances >= kMaxInstances) return FailWith(guest::kEMFILE, -1);
    ++Reg().instances;
  }
  auto* instance = new (std::nothrow) Instance();
  const int fd = instance == nullptr
                     ? FailWith(guest::kENOMEM, -1)
                     : InstallHookedFd(instance, &kInotifyOps, guest::kO_RDONLY | flags, "anon_inode:inotify");
  if (fd < 0) {
    delete instance;
    std::lock_guard<std::mutex> guard(Reg().lock);
    --Reg().instances;
  }
  return fd;
}

int AddWatch(int fd, const char* path, std::uint32_t mask) {
  if ((mask & kAllInotifyBits) == 0) return FailWith(guest::kEINVAL, -1);
  if ((mask & kIN_MASK_ADD) != 0 && (mask & kIN_MASK_CREATE) != 0) return FailWith(guest::kEINVAL, -1);
  DescriptionPtr d = GetDescription(fd);
  if (d == nullptr) return -1;
  if (d->ops != &kInotifyOps) return FailWith(guest::kEINVAL, -1);
  auto* instance = static_cast<Instance*>(d->handle);
  if (path == nullptr) return FailWith(guest::kEFAULT, -1);
  Target target;
  int err = 0;
  if (!ResolveTarget(path, mask, &target, &err)) return FailWith(err, -1);
  const std::uint32_t stored = mask & kStoredBits;

  std::lock_guard<std::mutex> guard(Reg().lock);
  if (auto existing = instance->by_identity.find(target.identity); existing != instance->by_identity.end()) {
    if ((mask & kIN_MASK_CREATE) != 0) return FailWith(guest::kEEXIST, -1);
    Watch* w = instance->watches.at(existing->second);
    w->mask = (mask & kIN_MASK_ADD) != 0 ? (w->mask | stored) : stored;
    if ((w->mask & kIN_ATTRIB) != 0) EnsureAttributeStream(w->monitor);
    return w->wd;
  }
  if (Reg().watches >= kMaxWatches) return FailWith(guest::kENOSPC, -1);
  Monitor* monitor = nullptr;
  if (target.host) {
    monitor = AcquireMonitor(target.monitor_dir, target.alias_key, &err);
    if (monitor == nullptr) return FailWith(err, -1);
  }
  auto w = std::make_unique<Watch>();
  w->instance = instance;
  w->mask = stored;
  w->monitor = monitor;
  w->directory = target.directory;
  w->identity = target.identity;
  if (!target.directory) {
    w->name = target.name;
    w->name_key = Upper(target.name);
  }
  int wd = instance->last_wd;
  do {
    wd = wd == INT_MAX ? 1 : wd + 1;
  } while (instance->watches.count(wd) != 0);
  instance->last_wd = wd;
  w->wd = wd;
  Watch* raw = w.get();
  instance->watches[wd] = raw;
  instance->by_identity[target.identity] = wd;
  ++Reg().watches;
  if (monitor != nullptr) {
    monitor->watches.push_back(raw);
    if ((stored & kIN_ATTRIB) != 0) EnsureAttributeStream(monitor);
  }
  w.release();
  return wd;
}

int RemoveWatchDescriptor(int fd, int wd) {
  DescriptionPtr d = GetDescription(fd);
  if (d == nullptr) return -1;
  if (d->ops != &kInotifyOps) return FailWith(guest::kEINVAL, -1);
  auto* instance = static_cast<Instance*>(d->handle);
  std::lock_guard<std::mutex> guard(Reg().lock);
  auto it = instance->watches.find(wd);
  if (it == instance->watches.end()) return FailWith(guest::kEINVAL, -1);
  RemoveWatch(it->second, true);
  return 0;
}

void PostGuestEvent(const std::wstring& path, std::uint32_t mask, bool directory) {
  auto message = std::make_unique<Message>();
  auto [dir, name] = SplitParent(path);
  if (name.empty()) return;
  message->dir_key = KeyOf(dir);
  message->name = std::move(name);
  message->mask = mask;
  message->directory = directory;
  if (directory) message->self_key = KeyOf(path);
  Registry& reg = Reg();
  {
    std::lock_guard<std::mutex> guard(reg.lock);
    const bool parent = reg.by_key.count(message->dir_key) != 0;
    const bool self = directory && reg.by_key.count(message->self_key) != 0;
    if ((!parent && !self) || reg.port == nullptr) return;
    if (!PostQueuedCompletionStatus(reg.port, 0, kMessageKey, reinterpret_cast<OVERLAPPED*>(message.get()))) return;
  }
  message.release();
}

}

void InotifyNoteGuestWrite(const Description& d) noexcept {
  if (g_monitors.load(std::memory_order_relaxed) == 0 || d.host_path.empty()) return;
  try {
    PostGuestEvent(d.host_path, kIN_MODIFY, false);
  } catch (...) {
  }
}

void InotifyNoteGuestClose(const std::wstring& host_path, bool directory, bool writable) noexcept {
  if (g_monitors.load(std::memory_order_relaxed) == 0) return;
  try {
    PostGuestEvent(host_path, writable ? kIN_CLOSE_WRITE : kIN_CLOSE_NOWRITE, directory);
  } catch (...) {
  }
}

ELIXIR_BIONIC_FN(int, inotify_init1)(int flags) noexcept {
  return Guarded(-1, [&] { return Init(flags); });
}
ELIXIR_BIONIC_FN(int, inotify_init)() noexcept {
  return Guarded(-1, [&] { return Init(0); });
}
ELIXIR_BIONIC_FN(int, inotify_add_watch)(int fd, const char* path, std::uint32_t mask) noexcept {
  return Guarded(-1, [&] { return AddWatch(fd, path, mask); });
}
ELIXIR_BIONIC_FN(int, inotify_rm_watch)(int fd, std::uint32_t wd) noexcept {
  return Guarded(-1, [&] { return RemoveWatchDescriptor(fd, static_cast<int>(wd)); });
}

void RegisterInotifyExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  add("inotify_init", &elixir_bionic_inotify_init);
  add("inotify_init1", &elixir_bionic_inotify_init1);
  add("inotify_add_watch", &elixir_bionic_inotify_add_watch);
  add("inotify_rm_watch", &elixir_bionic_inotify_rm_watch);
}

}
