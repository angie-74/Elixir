// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "bionic/files/files_internal.h"

#include <winsock2.h>
#include <windows.h>

#include <algorithm>
#include <cstring>

namespace elixir::bionic::files {
namespace {

struct FdEntry {
  DescriptionPtr description;
  bool cloexec = false;
  std::uint64_t owner_tag = 0;
};

struct FdTable {
  std::mutex lock;
  std::vector<FdEntry> entries;
};

HANDLE OpenNulDevice() {
  return CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                     nullptr, OPEN_EXISTING, 0, nullptr);
}

bool IsConsoleHandle(HANDLE handle) {
  DWORD mode = 0;
  return GetFileType(handle) == FILE_TYPE_CHAR && GetConsoleMode(handle, &mode) != FALSE;
}

DescriptionPtr MakeStdDescription(int fd) {
  static const DWORD kIds[] = {STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE};
  const int flags = fd == 0 ? guest::kO_RDONLY : guest::kO_WRONLY;
  HANDLE source = GetStdHandle(kIds[fd]);
  HANDLE own = nullptr;
  if (source != nullptr && source != INVALID_HANDLE_VALUE &&
      !DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(), &own, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
    own = nullptr;
  }
  if (own == nullptr) {
    own = OpenNulDevice();
    auto d = MakeHandleDescription(own == INVALID_HANDLE_VALUE ? nullptr : own, vfs::FdKind::kNull, flags);
    d->guest_path = "/dev/null";
    return d;
  }
  vfs::FdKind kind = vfs::FdKind::kFile;
  switch (GetFileType(own)) {
    case FILE_TYPE_CHAR: kind = IsConsoleHandle(own) ? vfs::FdKind::kConsole : vfs::FdKind::kNull; break;
    case FILE_TYPE_PIPE: kind = vfs::FdKind::kPipe; break;
    default: kind = vfs::FdKind::kFile; break;
  }
  auto d = MakeHandleDescription(own, kind, flags);
  if (kind == vfs::FdKind::kFile) d->use_os_pointer = true;
  if (kind == vfs::FdKind::kConsole) d->guest_path = "/dev/pts/0";
  if (kind == vfs::FdKind::kNull) d->guest_path = "/dev/null";
  return d;
}

FdTable& Table() {
  static FdTable* table = [] {
    auto* t = new FdTable();
    t->entries.resize(3);
    for (int fd = 0; fd < 3; ++fd) t->entries[static_cast<std::size_t>(fd)].description = MakeStdDescription(fd);
    return t;
  }();
  return *table;
}

constexpr DWORD kMaxChunk = 1u << 30;

std::int64_t Fail(int error) {
  SetGuestErrno(error);
  return -1;
}

std::int64_t ReadAt(const Description& description, void* buffer, std::size_t count, std::int64_t offset) {
  const HANDLE handle = description.handle;
  std::size_t total = 0;
  while (total < count) {
    const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(count - total, kMaxChunk));
    const std::uint64_t at = static_cast<std::uint64_t>(offset) + total;
    OVERLAPPED overlapped{};
    overlapped.Offset = static_cast<DWORD>(at);
    overlapped.OffsetHigh = static_cast<DWORD>(at >> 32);
    DWORD got = 0;
    if (!ReadFile(handle, static_cast<char*>(buffer) + total, chunk, &got, &overlapped)) {
      DWORD error = GetLastError();
      if (error == ERROR_IO_PENDING) {
        if (GetOverlappedResult(handle, &overlapped, &got, TRUE)) {
          error = ERROR_SUCCESS;
        } else {
          error = GetLastError();
        }
      }
      if (error == ERROR_HANDLE_EOF) break;
      if (error != ERROR_SUCCESS) {
        const int guest_error = ErrnoFromWin32(error);
        RecordContentCacheIoFailure(ContentCacheOperation::kRead, guest_error, description.guest_path);
        if (total > 0) break;
        return Fail(guest_error);
      }
    }
    total += got;
    if (got < chunk) break;
  }
  return static_cast<std::int64_t>(total);
}

std::int64_t WriteAt(const Description& description, const void* buffer, std::size_t count, std::int64_t offset) {
  const HANDLE handle = description.handle;
  std::size_t total = 0;
  while (total < count) {
    const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(count - total, kMaxChunk));
    OVERLAPPED overlapped{};
    if (offset < 0) {
      overlapped.Offset = 0xFFFFFFFFu;
      overlapped.OffsetHigh = 0xFFFFFFFFu;
    } else {
      const std::uint64_t at = static_cast<std::uint64_t>(offset) + total;
      overlapped.Offset = static_cast<DWORD>(at);
      overlapped.OffsetHigh = static_cast<DWORD>(at >> 32);
    }
    DWORD written = 0;
    if (!WriteFile(handle, static_cast<const char*>(buffer) + total, chunk, &written, &overlapped)) {
      DWORD error = GetLastError();
      if (error == ERROR_IO_PENDING) {
        error = GetOverlappedResult(handle, &overlapped, &written, TRUE) ? ERROR_SUCCESS : GetLastError();
      }
      if (error != ERROR_SUCCESS) {
        const int guest_error = error == ERROR_DISK_FULL || error == ERROR_HANDLE_DISK_FULL
                                    ? guest::kENOSPC : ErrnoFromWin32(error);
        RecordContentCacheIoFailure(ContentCacheOperation::kWrite, guest_error, description.guest_path);
        if (total > 0) break;
        return Fail(guest_error);
      }
    }
    total += written;
    if (written < chunk) break;
  }
  return static_cast<std::int64_t>(total);
}

std::int64_t FileSize(HANDLE handle) {
  LARGE_INTEGER size;
  if (!GetFileSizeEx(handle, &size)) return -1;
  return size.QuadPart;
}

std::int64_t StreamRead(HANDLE handle, void* buffer, std::size_t count, bool pipe) {
  const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(count, kMaxChunk));
  DWORD got = 0;
  if (!ReadFile(handle, buffer, chunk, &got, nullptr)) {
    const DWORD error = GetLastError();
    if (error == ERROR_BROKEN_PIPE || error == ERROR_HANDLE_EOF || error == ERROR_PIPE_NOT_CONNECTED) return 0;
    if (error == ERROR_MORE_DATA) return got;
    if (pipe && error == ERROR_NO_DATA) return Fail(guest::kEAGAIN);
    return Fail(ErrnoFromWin32(error));
  }
  return got;
}

std::int64_t StreamWrite(HANDLE handle, const void* buffer, std::size_t count, bool pipe, bool nonblocking) {
  std::size_t total = 0;
  while (total < count) {
    const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(count - total, kMaxChunk));
    DWORD written = 0;
    if (!WriteFile(handle, static_cast<const char*>(buffer) + total, chunk, &written, nullptr)) {
      const DWORD error = GetLastError();
      if (total > 0) break;
      if (pipe && (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA || error == ERROR_PIPE_NOT_CONNECTED)) {
        return Fail(guest::kEPIPE);
      }
      return Fail(ErrnoFromWin32(error));
    }
    total += written;
    if (written < chunk) {
      if (total == 0 && nonblocking) return Fail(guest::kEAGAIN);
      break;
    }
  }
  return static_cast<std::int64_t>(total);
}

std::size_t CompleteUtf8Prefix(const std::string& data) {
  std::size_t end = data.size();
  std::size_t back = 0;
  while (back < 4 && back < end) {
    const unsigned char c = static_cast<unsigned char>(data[end - 1 - back]);
    if ((c & 0xC0) != 0x80) {
      std::size_t need = 1;
      if ((c & 0xE0) == 0xC0) need = 2;
      else if ((c & 0xF0) == 0xE0) need = 3;
      else if ((c & 0xF8) == 0xF0) need = 4;
      if (back + 1 < need) return end - 1 - back;
      return end;
    }
    ++back;
  }
  return end;
}

std::int64_t ConsoleWrite(Description& d, const void* buffer, std::size_t count) {
  std::lock_guard<std::mutex> guard(d.lock);
  std::string data = std::move(d.console_out_carry);
  d.console_out_carry.clear();
  data.append(static_cast<const char*>(buffer), count);
  const std::size_t complete = CompleteUtf8Prefix(data);
  d.console_out_carry = data.substr(complete);
  data.resize(complete);
  std::size_t offset = 0;
  while (offset < data.size()) {
    const std::size_t piece = std::min<std::size_t>(data.size() - offset, 16384);
    std::size_t cut = offset + piece;
    if (cut < data.size()) {
      while (cut > offset && (static_cast<unsigned char>(data[cut]) & 0xC0) == 0x80) --cut;
      if (cut == offset) cut = offset + piece;
    }
    const std::wstring wide = Utf8ToWide(std::string_view(data).substr(offset, cut - offset));
    std::size_t done = 0;
    while (done < wide.size()) {
      DWORD written = 0;
      if (!WriteConsoleW(d.handle, wide.data() + done, static_cast<DWORD>(wide.size() - done), &written, nullptr)) {
        DWORD raw = 0;
        if (!WriteFile(d.handle, data.data() + offset, static_cast<DWORD>(cut - offset), &raw, nullptr)) {
          return offset > 0 ? static_cast<std::int64_t>(offset) : Fail(ErrnoFromWin32(GetLastError()));
        }
        done = wide.size();
        break;
      }
      if (written == 0) break;
      done += written;
    }
    offset = cut;
  }
  return static_cast<std::int64_t>(count);
}

std::int64_t ConsoleRead(Description& d, void* buffer, std::size_t count) {
  std::lock_guard<std::mutex> guard(d.lock);
  if (d.console_in_pending.empty()) {
    wchar_t wide[4096];
    DWORD got = 0;
    if (!ReadConsoleW(d.handle, wide, 4096, &got, nullptr)) {
      return StreamRead(d.handle, buffer, count, false);
    }
    std::string text = WideToUtf8(std::wstring_view(wide, got));
    std::string normalized;
    normalized.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
      if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') continue;
      normalized.push_back(text[i]);
    }
    if (!normalized.empty() && normalized[0] == '\x1a') return 0;
    d.console_in_pending = std::move(normalized);
  }
  const std::size_t n = std::min(count, d.console_in_pending.size());
  std::memcpy(buffer, d.console_in_pending.data(), n);
  d.console_in_pending.erase(0, n);
  return static_cast<std::int64_t>(n);
}

bool WaitSocket(SOCKET socket, short events) {
  WSAPOLLFD pfd{};
  pfd.fd = socket;
  pfd.events = events;
  return WSAPoll(&pfd, 1, -1) >= 0;
}

std::int64_t SocketRead(Description& d, void* buffer, std::size_t count) {
  const auto socket = reinterpret_cast<SOCKET>(d.handle);
  const int length = static_cast<int>(std::min<std::size_t>(count, 0x7fffffff));
  for (;;) {
    const int got = recv(socket, static_cast<char*>(buffer), length, 0);
    if (got >= 0) return got;
    const int error = WSAGetLastError();
    if (error == WSAEWOULDBLOCK && (d.status_flags.load() & guest::kO_NONBLOCK) == 0 && WaitSocket(socket, POLLRDNORM)) {
      continue;
    }
    if (error == WSAESHUTDOWN || error == WSAECONNRESET) return error == WSAESHUTDOWN ? 0 : Fail(guest::kECONNRESET);
    return Fail(ErrnoFromWinsock(error));
  }
}

std::int64_t SocketWrite(Description& d, const void* buffer, std::size_t count) {
  const auto socket = reinterpret_cast<SOCKET>(d.handle);
  const int length = static_cast<int>(std::min<std::size_t>(count, 0x7fffffff));
  for (;;) {
    const int sent = send(socket, static_cast<const char*>(buffer), length, 0);
    if (sent >= 0) return sent;
    const int error = WSAGetLastError();
    if (error == WSAEWOULDBLOCK && (d.status_flags.load() & guest::kO_NONBLOCK) == 0 && WaitSocket(socket, POLLWRNORM)) {
      continue;
    }
    if (error == WSAESHUTDOWN || error == WSAECONNABORTED) return Fail(guest::kEPIPE);
    return Fail(ErrnoFromWinsock(error));
  }
}

}

Description::~Description() {
  if (const FdOps* hooked = HookOps(*this); hooked != nullptr) {
    HookedClose(*this, *hooked);
    return;
  }
  if (handle == nullptr || handle == INVALID_HANDLE_VALUE) return;
  if (kind == vfs::FdKind::kSocket) {
    closesocket(reinterpret_cast<SOCKET>(handle));
    return;
  }
  CloseHandle(handle);
  if (provider == Provider::kHandle && (kind == vfs::FdKind::kFile || kind == vfs::FdKind::kDirectory) &&
      !host_path.empty() && !IsPathOnly(*this)) {
    InotifyNoteGuestClose(host_path, kind == vfs::FdKind::kDirectory, AccessMode(*this) != guest::kO_RDONLY);
  }
}

DescriptionPtr MakeHandleDescription(HANDLE handle, vfs::FdKind kind, int flags) {
  auto d = std::make_shared<Description>();
  d->handle = handle;
  d->kind = kind;
  d->provider = Provider::kHandle;
  d->status_flags.store(flags & ~guest::kO_CLOEXEC);
  return d;
}

DescriptionPtr FindDescription(int fd) {
  if (fd < 0 || fd >= kMaxFds) return nullptr;
  FdTable& table = Table();
  std::lock_guard<std::mutex> guard(table.lock);
  if (static_cast<std::size_t>(fd) >= table.entries.size()) return nullptr;
  return table.entries[static_cast<std::size_t>(fd)].description;
}

DescriptionPtr GetDescription(int fd) {
  DescriptionPtr d = FindDescription(fd);
  if (d == nullptr) SetGuestErrno(guest::kEBADF);
  return d;
}

int InstallDescription(DescriptionPtr description, bool cloexec, int min_fd) {
  if (min_fd < 0 || min_fd >= kMaxFds) return FailWith(guest::kEINVAL, -1);
  FdTable& table = Table();
  std::lock_guard<std::mutex> guard(table.lock);
  for (int fd = min_fd; fd < kMaxFds; ++fd) {
    const auto index = static_cast<std::size_t>(fd);
    if (index >= table.entries.size()) table.entries.resize(index + 1);
    if (table.entries[index].description == nullptr) {
      table.entries[index].description = std::move(description);
      table.entries[index].cloexec = cloexec;
      return fd;
    }
  }
  return FailWith(guest::kEMFILE, -1);
}

int InstallDescriptionAt(DescriptionPtr description, int fd, bool cloexec) {
  if (fd < 0 || fd >= kMaxFds) return FailWith(guest::kEBADF, -1);
  DescriptionPtr previous;
  {
    FdTable& table = Table();
    std::lock_guard<std::mutex> guard(table.lock);
    const auto index = static_cast<std::size_t>(fd);
    if (index >= table.entries.size()) table.entries.resize(index + 1);
    previous = std::move(table.entries[index].description);
    table.entries[index].description = std::move(description);
    table.entries[index].cloexec = cloexec;
  }
  previous.reset();
  return fd;
}

int CloseFd(int fd) {
  DescriptionPtr previous;
  {
    if (fd < 0 || fd >= kMaxFds) return FailWith(guest::kEBADF, -1);
    FdTable& table = Table();
    std::lock_guard<std::mutex> guard(table.lock);
    const auto index = static_cast<std::size_t>(fd);
    if (index >= table.entries.size() || table.entries[index].description == nullptr) {
      return FailWith(guest::kEBADF, -1);
    }
    previous = std::move(table.entries[index].description);
    table.entries[index].cloexec = false;
  }
  previous.reset();
  return 0;
}

bool GetCloexec(int fd, bool* cloexec) {
  if (fd < 0 || fd >= kMaxFds) return false;
  FdTable& table = Table();
  std::lock_guard<std::mutex> guard(table.lock);
  const auto index = static_cast<std::size_t>(fd);
  if (index >= table.entries.size() || table.entries[index].description == nullptr) return false;
  *cloexec = table.entries[index].cloexec;
  return true;
}

bool SetCloexec(int fd, bool cloexec) {
  if (fd < 0 || fd >= kMaxFds) return false;
  FdTable& table = Table();
  std::lock_guard<std::mutex> guard(table.lock);
  const auto index = static_cast<std::size_t>(fd);
  if (index >= table.entries.size() || table.entries[index].description == nullptr) return false;
  table.entries[index].cloexec = cloexec;
  return true;
}

bool ExchangeFdOwnerTag(int fd, std::uint64_t expected, std::uint64_t desired, std::uint64_t* actual) {
  if (fd < 0 || fd >= kMaxFds) return false;
  FdTable& table = Table();
  std::lock_guard<std::mutex> guard(table.lock);
  const auto index = static_cast<std::size_t>(fd);
  if (index >= table.entries.size()) {
    if (expected == 0 && desired == 0) {
      *actual = 0;
      return true;
    }
    table.entries.resize(index + 1);
  }
  std::uint64_t& tag = table.entries[index].owner_tag;
  *actual = tag;
  if (tag == expected) tag = desired;
  return true;
}

std::uint64_t FdOwnerTag(int fd) {
  if (fd < 0 || fd >= kMaxFds) return 0;
  FdTable& table = Table();
  std::lock_guard<std::mutex> guard(table.lock);
  const auto index = static_cast<std::size_t>(fd);
  return index < table.entries.size() ? table.entries[index].owner_tag : 0;
}

std::vector<int> OpenFds() {
  std::vector<int> fds;
  FdTable& table = Table();
  std::lock_guard<std::mutex> guard(table.lock);
  for (std::size_t i = 0; i < table.entries.size(); ++i) {
    if (table.entries[i].description != nullptr) fds.push_back(static_cast<int>(i));
  }
  return fds;
}

std::string FdTargetPath(int fd) {
  DescriptionPtr d = FindDescription(fd);
  if (d == nullptr) return {};
  const auto inode = static_cast<unsigned long long>(HashString(std::to_string(reinterpret_cast<std::uintptr_t>(d.get()))) & 0xffffffu);
  switch (d->kind) {
    case vfs::FdKind::kPipe: return "pipe:[" + std::to_string(inode) + "]";
    case vfs::FdKind::kSocket: return "socket:[" + std::to_string(inode) + "]";
    case vfs::FdKind::kEvent: return d->guest_path.empty() ? "anon_inode:[eventfd]" : d->guest_path;
    case vfs::FdKind::kConsole: return "/dev/pts/0";
    default: break;
  }
  if (!d->guest_path.empty()) return d->guest_path;
  if (d->provider == Provider::kHandle && d->handle != nullptr) {
    wchar_t buffer[1024];
    const DWORD length =
        GetFinalPathNameByHandleW(d->handle, buffer, 1024, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (length > 0 && length < 1024) return vfs::HostToGuest(WideToUtf8(std::wstring_view(buffer, length)));
  }
  return "/dev/null";
}

void RecordContentCacheMapFailure(int fd, const void* handle, int guest_errno) noexcept {
  const DWORD saved_last_error = GetLastError();
  try {
    const DescriptionPtr description = FindDescription(fd);
    if (description != nullptr && description->handle == handle) {
      RecordContentCacheIoFailure(ContentCacheOperation::kMap, guest_errno, description->guest_path);
    }
  } catch (...) {
  }
  SetLastError(saved_last_error);
}

bool DescIsSeekable(const Description& d) {
  switch (d.provider) {
    case Provider::kMemory:
    case Provider::kZero:
    case Provider::kRandom:
    case Provider::kVirtualDir:
      return true;
    case Provider::kHandle:
      return d.kind == vfs::FdKind::kFile || d.kind == vfs::FdKind::kDirectory || d.kind == vfs::FdKind::kNull;
  }
  return false;
}

static std::int64_t ReadSnapshot(Description& d, void* buffer, std::size_t count, std::int64_t offset) {
  if (d.sample && (offset == 0 || d.memory.empty())) d.memory = d.sample();
  if (offset >= static_cast<std::int64_t>(d.memory.size())) return 0;
  const std::size_t bytes = std::min(count, d.memory.size() - static_cast<std::size_t>(offset));
  std::memcpy(buffer, d.memory.data() + offset, bytes);
  return static_cast<std::int64_t>(bytes);
}

static std::int64_t DescReadImpl(Description& d, void* buffer, std::size_t count) {
  if (IsPathOnly(d) || AccessMode(d) == guest::kO_WRONLY) return Fail(guest::kEBADF);
  if (const FdOps* hooked = HookOps(d); hooked != nullptr) return HookedRead(d, *hooked, buffer, count);
  if (IsDirectoryDescription(d)) return Fail(guest::kEISDIR);
  if (count == 0) return 0;
  switch (d.provider) {
    case Provider::kZero:
      std::memset(buffer, 0, count);
      return static_cast<std::int64_t>(std::min<std::size_t>(count, 0x7ffff000));
    case Provider::kRandom: {
      const std::size_t n = std::min<std::size_t>(count, 0x7ffff000);
      if (!FillRandom(buffer, n)) return Fail(guest::kEIO);
      return static_cast<std::int64_t>(n);
    }
    case Provider::kMemory: {
      std::lock_guard<std::mutex> guard(d.lock);
      const auto bytes = ReadSnapshot(d, buffer, count, d.position);
      d.position += bytes;
      return bytes;
    }
    case Provider::kVirtualDir:
      return Fail(guest::kEISDIR);
    case Provider::kHandle:
      break;
  }
  switch (d.kind) {
    case vfs::FdKind::kFile: {
      if (d.use_os_pointer) return StreamRead(d.handle, buffer, count, false);
      std::lock_guard<std::mutex> guard(d.lock);
      const std::int64_t got = ReadAt(d, buffer, count, d.position);
      if (got > 0) d.position += got;
      return got;
    }
    case vfs::FdKind::kPipe: {
      if ((d.status_flags.load() & guest::kO_NONBLOCK) != 0) {
        DWORD available = 0;
        if (!PeekNamedPipe(d.handle, nullptr, 0, nullptr, &available, nullptr)) {
          const DWORD error = GetLastError();
          if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) return 0;
          return Fail(ErrnoFromWin32(error));
        }
        if (available == 0) return Fail(guest::kEAGAIN);
        count = std::min<std::size_t>(count, available);
      }
      return StreamRead(d.handle, buffer, count, true);
    }
    case vfs::FdKind::kConsole:
      return ConsoleRead(d, buffer, count);
    case vfs::FdKind::kSocket:
      return SocketRead(d, buffer, count);
    case vfs::FdKind::kNull:
      return 0;
    case vfs::FdKind::kEvent:
      return Fail(guest::kEINVAL);
    case vfs::FdKind::kDirectory:
      return Fail(guest::kEISDIR);
  }
  return Fail(guest::kEBADF);
}

std::int64_t DescRead(Description& d, void* buffer, std::size_t count) {
  const std::int64_t result = DescReadImpl(d, buffer, count);
  RecordProcReadResult(d, false, count, result);
  return result;
}

std::int64_t DescWrite(Description& d, const void* buffer, std::size_t count) {
  if (IsPathOnly(d) || AccessMode(d) == guest::kO_RDONLY) return Fail(guest::kEBADF);
  if (const FdOps* hooked = HookOps(d); hooked != nullptr) return HookedWrite(d, *hooked, buffer, count);
  if (IsDirectoryDescription(d)) return Fail(guest::kEISDIR);
  if (count == 0) return 0;
  switch (d.provider) {
    case Provider::kZero:
    case Provider::kRandom:
      return static_cast<std::int64_t>(count);
    case Provider::kMemory:
    case Provider::kVirtualDir:
      return Fail(guest::kEBADF);
    case Provider::kHandle:
      break;
  }
  switch (d.kind) {
    case vfs::FdKind::kFile: {
      std::int64_t written = 0;
      if (d.use_os_pointer) {
        written = StreamWrite(d.handle, buffer, count, false, false);
      } else {
        std::lock_guard<std::mutex> guard(d.lock);
        if ((d.status_flags.load() & guest::kO_APPEND) != 0) {
          written = WriteAt(d, buffer, count, -1);
          if (written >= 0) {
            const std::int64_t size = FileSize(d.handle);
            if (size >= 0) d.position = size;
          }
        } else {
          written = WriteAt(d, buffer, count, d.position);
          if (written > 0) d.position += written;
        }
      }
      if (written > 0) InotifyNoteGuestWrite(d);
      return written;
    }
    case vfs::FdKind::kPipe:
      return StreamWrite(d.handle, buffer, count, true, (d.status_flags.load() & guest::kO_NONBLOCK) != 0);
    case vfs::FdKind::kConsole:
      return ConsoleWrite(d, buffer, count);
    case vfs::FdKind::kSocket:
      return SocketWrite(d, buffer, count);
    case vfs::FdKind::kNull:
      return static_cast<std::int64_t>(count);
    case vfs::FdKind::kEvent:
      return Fail(guest::kEINVAL);
    case vfs::FdKind::kDirectory:
      return Fail(guest::kEISDIR);
  }
  return Fail(guest::kEBADF);
}

static std::int64_t DescPreadImpl(Description& d, void* buffer, std::size_t count, std::int64_t offset) {
  if (IsPathOnly(d) || AccessMode(d) == guest::kO_WRONLY) return Fail(guest::kEBADF);
  if (offset < 0) return Fail(guest::kEINVAL);
  if (IsDirectoryDescription(d)) return Fail(guest::kEISDIR);
  if (!DescIsSeekable(d) || d.use_os_pointer) return Fail(guest::kESPIPE);
  if (count == 0) return 0;
  switch (d.provider) {
    case Provider::kZero:
    case Provider::kRandom:
      return DescReadImpl(d, buffer, count);
    case Provider::kMemory: {
      std::lock_guard<std::mutex> guard(d.lock);
      return ReadSnapshot(d, buffer, count, offset);
    }
    default:
      break;
  }
  if (d.kind == vfs::FdKind::kNull) return 0;
  return ReadAt(d, buffer, count, offset);
}

std::int64_t DescPread(Description& d, void* buffer, std::size_t count, std::int64_t offset) {
  const std::int64_t result = DescPreadImpl(d, buffer, count, offset);
  RecordProcReadResult(d, true, count, result);
  return result;
}

std::int64_t DescPwrite(Description& d, const void* buffer, std::size_t count, std::int64_t offset) {
  if (IsPathOnly(d) || AccessMode(d) == guest::kO_RDONLY) return Fail(guest::kEBADF);
  if (offset < 0) return Fail(guest::kEINVAL);
  if (IsDirectoryDescription(d)) return Fail(guest::kEISDIR);
  if (!DescIsSeekable(d) || d.use_os_pointer) return Fail(guest::kESPIPE);
  if (count == 0) return 0;
  if (d.provider == Provider::kZero || d.provider == Provider::kRandom || d.kind == vfs::FdKind::kNull) {
    return static_cast<std::int64_t>(count);
  }
  if (d.provider != Provider::kHandle) return Fail(guest::kEBADF);
  const bool append = (d.status_flags.load() & guest::kO_APPEND) != 0;
  const std::int64_t written = WriteAt(d, buffer, count, append ? -1 : offset);
  if (written > 0) InotifyNoteGuestWrite(d);
  return written;
}

std::int64_t DescWriteAppend(Description& d, const void* buffer, std::size_t count, std::int64_t offset) {
  if (d.provider != Provider::kHandle || d.kind != vfs::FdKind::kFile || d.use_os_pointer) {
    return offset < 0 ? DescWrite(d, buffer, count) : DescPwrite(d, buffer, count, offset);
  }
  if (IsPathOnly(d) || AccessMode(d) == guest::kO_RDONLY) return Fail(guest::kEBADF);
  if (count == 0) return 0;
  std::int64_t written = 0;
  {
    std::lock_guard<std::mutex> guard(d.lock);
    written = WriteAt(d, buffer, count, -1);
    if (written >= 0 && offset < 0) {
      const std::int64_t size = FileSize(d.handle);
      if (size >= 0) d.position = size;
    }
  }
  if (written > 0) InotifyNoteGuestWrite(d);
  return written;
}

std::int64_t DescSeek(Description& d, std::int64_t offset, int whence) {
  if (whence < guest::kSEEK_SET || whence > guest::kSEEK_HOLE) return Fail(guest::kEINVAL);
  if (d.provider == Provider::kHandle) {
    if (d.kind == vfs::FdKind::kPipe || d.kind == vfs::FdKind::kSocket || d.kind == vfs::FdKind::kConsole) {
      return Fail(guest::kESPIPE);
    }
    if (d.kind == vfs::FdKind::kNull || d.kind == vfs::FdKind::kEvent) return 0;
  }
  if (d.provider == Provider::kZero || d.provider == Provider::kRandom) return 0;
  if (d.use_os_pointer) {
    if (whence > guest::kSEEK_END) return Fail(guest::kEINVAL);
    LARGE_INTEGER distance;
    distance.QuadPart = offset;
    LARGE_INTEGER result;
    const DWORD method = whence == guest::kSEEK_SET ? FILE_BEGIN : whence == guest::kSEEK_CUR ? FILE_CURRENT : FILE_END;
    if (!SetFilePointerEx(d.handle, distance, &result, method)) return Fail(ErrnoFromWin32(GetLastError()));
    return result.QuadPart;
  }
  std::lock_guard<std::mutex> guard(d.lock);
  std::int64_t size = 0;
  if (whence == guest::kSEEK_END || whence == guest::kSEEK_DATA || whence == guest::kSEEK_HOLE) {
    if (d.provider == Provider::kMemory) {
      if (d.sample && d.memory.empty()) d.memory = d.sample();
      size = static_cast<std::int64_t>(d.memory.size());
    } else if (d.provider == Provider::kHandle && d.kind == vfs::FdKind::kFile) {
      size = FileSize(d.handle);
      if (size < 0) return Fail(ErrnoFromWin32(GetLastError()));
    } else if (whence != guest::kSEEK_END) {
      return Fail(guest::kEINVAL);
    }
  }
  std::int64_t target = 0;
  switch (whence) {
    case guest::kSEEK_SET: target = offset; break;
    case guest::kSEEK_CUR:
      if ((offset > 0 && d.position > INT64_MAX - offset)) return Fail(guest::kEOVERFLOW);
      target = d.position + offset;
      break;
    case guest::kSEEK_END:
      if ((offset > 0 && size > INT64_MAX - offset)) return Fail(guest::kEOVERFLOW);
      target = size + offset;
      break;
    case guest::kSEEK_DATA:
      if (offset < 0 || offset >= size) return Fail(guest::kENXIO);
      target = offset;
      break;
    case guest::kSEEK_HOLE:
      if (offset < 0 || offset >= size) return Fail(guest::kENXIO);
      target = size;
      break;
    default: return Fail(guest::kEINVAL);
  }
  if (target < 0) return Fail(guest::kEINVAL);
  d.position = target;
  return target;
}

void DescSetStatusFlags(Description& d, int flags) {
  const int changeable = guest::kO_APPEND | guest::kO_NONBLOCK | guest::kO_ASYNC | guest::kO_DIRECT | guest::kO_NOATIME;
  const int previous = d.status_flags.load();
  const int next = (previous & ~changeable) | (flags & changeable);
  d.status_flags.store(next);
  if (const FdOps* hooked = HookOps(d); hooked != nullptr) {
    if (((previous ^ next) & guest::kO_NONBLOCK) != 0) {
      HookedSetNonBlocking(d, *hooked, (next & guest::kO_NONBLOCK) != 0);
    }
    return;
  }
  if (d.provider == Provider::kHandle && d.kind == vfs::FdKind::kPipe && AccessMode(d) != guest::kO_RDONLY &&
      ((previous ^ next) & guest::kO_NONBLOCK) != 0) {
    DWORD mode = PIPE_READMODE_BYTE | ((next & guest::kO_NONBLOCK) != 0 ? PIPE_NOWAIT : PIPE_WAIT);
    SetNamedPipeHandleState(d.handle, &mode, nullptr, nullptr);
  }
}

std::int64_t DescAvailable(Description& d) {
  if (const FdOps* hooked = HookOps(d); hooked != nullptr) {
    int value = 0;
    if (HookedBytesAvailable(d, *hooked, &value) != 0) return -1;
    return value;
  }
  switch (d.provider) {
    case Provider::kMemory: {
      std::lock_guard<std::mutex> guard(d.lock);
      if (d.sample && d.memory.empty()) d.memory = d.sample();
      return std::max<std::int64_t>(0, static_cast<std::int64_t>(d.memory.size()) - d.position);
    }
    case Provider::kZero:
    case Provider::kRandom:
    case Provider::kVirtualDir:
      return 0;
    case Provider::kHandle:
      break;
  }
  switch (d.kind) {
    case vfs::FdKind::kFile: {
      if (d.use_os_pointer) return 0;
      std::lock_guard<std::mutex> guard(d.lock);
      const std::int64_t size = FileSize(d.handle);
      return size < 0 ? -1 : std::max<std::int64_t>(0, size - d.position);
    }
    case vfs::FdKind::kPipe: {
      DWORD available = 0;
      if (!PeekNamedPipe(d.handle, nullptr, 0, nullptr, &available, nullptr)) return 0;
      return available;
    }
    case vfs::FdKind::kSocket: {
      u_long available = 0;
      if (ioctlsocket(reinterpret_cast<SOCKET>(d.handle), FIONREAD, &available) != 0) return -1;
      return static_cast<std::int64_t>(available);
    }
    case vfs::FdKind::kConsole: {
      std::lock_guard<std::mutex> guard(d.lock);
      return static_cast<std::int64_t>(d.console_in_pending.size());
    }
    default:
      return 0;
  }
}

DescriptionPtr OpenSyntheticDescription(const std::string& guest, const SynthNode& node, int flags, int* err) {
  const int access = flags & guest::kO_ACCMODE;
  switch (node.type) {
    case SynthType::kNull: {
      HANDLE handle = OpenNulDevice();
      if (handle == INVALID_HANDLE_VALUE) {
        *err = ErrnoFromWin32(GetLastError());
        return nullptr;
      }
      auto d = MakeHandleDescription(handle, vfs::FdKind::kNull, flags);
      d->guest_path = guest;
      return d;
    }
    case SynthType::kZero:
    case SynthType::kRandom: {
      auto d = MakeHandleDescription(nullptr, vfs::FdKind::kNull, flags);
      d->provider = node.type == SynthType::kZero ? Provider::kZero : Provider::kRandom;
      d->guest_path = guest;
      return d;
    }
    case SynthType::kFile: {
      if (access != guest::kO_RDONLY || (flags & guest::kO_TRUNC) != 0) {
        *err = guest::kEACCES;
        return nullptr;
      }
      if ((flags & guest::kO_DIRECTORY) != 0) {
        *err = guest::kENOTDIR;
        return nullptr;
      }
      auto d = MakeHandleDescription(nullptr, vfs::FdKind::kFile, flags);
      d->provider = Provider::kMemory;
      d->sample = node.sample;
      if (!d->sample) d->memory = SynthContent(guest);
      d->guest_path = guest;
      return d;
    }
    case SynthType::kDir: {
      if (access != guest::kO_RDONLY) {
        *err = guest::kEISDIR;
        return nullptr;
      }
      auto d = MakeHandleDescription(nullptr, vfs::FdKind::kDirectory, flags);
      d->provider = Provider::kVirtualDir;
      d->synthetic_dir = true;
      d->guest_path = guest;
      return d;
    }
    case SynthType::kFdAlias: {
      DescriptionPtr target = FindDescription(node.alias_fd);
      if (target == nullptr) *err = guest::kENOENT;
      return target;
    }
    default:
      *err = guest::kENOENT;
      return nullptr;
  }
}

}

namespace elixir::bionic::vfs {

int InstallHandle(void* handle, FdKind kind, int open_flags) {
  files::DescriptionPtr d;
  try {
    d = files::MakeHandleDescription(static_cast<HANDLE>(handle), kind, open_flags);
  } catch (...) {
    return FailWith(guest::kENOMEM, -1);
  }
  int fd = -1;
  try {
    fd = files::InstallDescription(d, (open_flags & guest::kO_CLOEXEC) != 0);
  } catch (...) {
    fd = FailWith(guest::kENOMEM, -1);
  }
  if (fd < 0) {
    d->handle = nullptr;
    d->kind = FdKind::kNull;
  }
  return fd;
}

void* HandleOf(int fd, FdKind* kind) {
  try {
    files::DescriptionPtr d = files::FindDescription(fd);
    if (d == nullptr) {
      SetGuestErrno(guest::kEBADF);
      return nullptr;
    }
    if (kind != nullptr) *kind = d->kind;
    return d->handle;
  } catch (...) {
    SetGuestErrno(guest::kENOMEM);
    return nullptr;
  }
}

}
