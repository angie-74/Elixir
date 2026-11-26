// Copyright 2026 Mocktail Project Authors
// Licensed under the Apache License, Version 2.0
// Modified by Elixir.
#include "runtime/session_log.h"

#if defined(_WIN32)

#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <intrin.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#ifndef MOCKTAIL_PROJECT_VERSION
#ifdef ELIXIR_VERSION
#define MOCKTAIL_PROJECT_VERSION ELIXIR_VERSION
#else
#define MOCKTAIL_PROJECT_VERSION "unknown"
#endif
#endif
#ifndef MOCKTAIL_BUILD_GIT_COMMIT
#define MOCKTAIL_BUILD_GIT_COMMIT "unknown"
#endif
#ifndef MOCKTAIL_BUILD_GIT_DIRTY
#define MOCKTAIL_BUILD_GIT_DIRTY "unknown"
#endif
#ifndef MOCKTAIL_BUILD_TYPE
#define MOCKTAIL_BUILD_TYPE "unknown"
#endif

namespace mocktail {
namespace runtime {

struct SessionLog::WindowsTee {
  HANDLE log_file = INVALID_HANDLE_VALUE;
  CRITICAL_SECTION log_lock;
  HANDLE stop_event = nullptr;
  std::atomic<bool> abandon{false};
  struct Stream {
    int crt_fd = -1;
    DWORD std_handle_id = 0;
    int saved_fd = -1;
    bool redirected = false;
    HANDLE original_std = nullptr;
    HANDLE echo = nullptr;
    HANDLE read_end = nullptr;
    HANDLE read_event = nullptr;
    HANDLE thread = nullptr;
    WindowsTee* owner = nullptr;
  };
  std::array<Stream, 2> streams;
};

namespace {

void IgnoreInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {}

class QuietCrtValidation final {
 public:
  QuietCrtValidation() : previous_(_set_thread_local_invalid_parameter_handler(&IgnoreInvalidParameter)) {}
  ~QuietCrtValidation() { _set_thread_local_invalid_parameter_handler(previous_); }
  QuietCrtValidation(const QuietCrtValidation&) = delete;
  QuietCrtValidation& operator=(const QuietCrtValidation&) = delete;

 private:
  _invalid_parameter_handler previous_;
};

bool UsableHandle(HANDLE handle) { return handle != nullptr && handle != INVALID_HANDLE_VALUE; }

HANDLE DescriptorHandle(int fd) {
  if (fd < 0) return nullptr;
  const intptr_t handle = _get_osfhandle(fd);
  if (handle == -1 || handle == -2 || handle == 0) return nullptr;
  return reinterpret_cast<HANDLE>(handle);
}

HANDLE WritableCopy(HANDLE target) {
  HANDLE copy = nullptr;
  if (UsableHandle(target)) {
    if (!DuplicateHandle(GetCurrentProcess(), target, GetCurrentProcess(), &copy, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
      return nullptr;
    }
    return copy;
  }
  copy = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0,
                     nullptr);
  return copy == INVALID_HANDLE_VALUE ? nullptr : copy;
}

bool AdoptHandleAsDescriptor(HANDLE handle, int fd) {
  const int temporary = _open_osfhandle(reinterpret_cast<intptr_t>(handle), _O_WRONLY | _O_BINARY);
  if (temporary < 0) {
    CloseHandle(handle);
    return false;
  }
  bool ok = true;
  if (temporary != fd) {
    ok = _dup2(temporary, fd) == 0;
    _close(temporary);
  }
  if (ok) SetHandleInformation(DescriptorHandle(fd), HANDLE_FLAG_INHERIT, 0);
  return ok;
}

int DuplicateAboveStandard(int fd) {
  int parked[3];
  int parked_count = 0;
  int copy = _dup(fd);
  while (copy >= 0 && copy <= 2 && parked_count < 3) {
    parked[parked_count++] = copy;
    copy = _dup(fd);
  }
  for (int i = 0; i < parked_count; ++i) _close(parked[i]);
  return copy;
}

bool BindStream(FILE* stream, int fd, HANDLE destination) {
  const HANDLE copy = WritableCopy(destination);
  if (copy == nullptr) return false;
  std::fflush(stream);
  if (_fileno(stream) != fd) {
    for (int lower = 0; lower < fd; ++lower) {
      if (_get_osfhandle(lower) == -1) {
        const int occupied = _wopen(L"NUL", _O_RDONLY | _O_BINARY | _O_NOINHERIT);
        if (occupied >= 0 && occupied != lower) _close(occupied);
      }
    }
    _close(fd);
    FILE* reopened = nullptr;
    if (_wfreopen_s(&reopened, L"NUL", L"wb", stream) != 0 || _fileno(stream) != fd) {
      CloseHandle(copy);
      return false;
    }
  }
  return AdoptHandleAsDescriptor(copy, fd);
}

}

bool BindStandardStreams(void* destination) {
  QuietCrtValidation quiet;
  bool ok = true;
  const struct {
    FILE* stream;
    int fd;
    DWORD id;
  } streams[] = {{stdout, 1, STD_OUTPUT_HANDLE}, {stderr, 2, STD_ERROR_HANDLE}};
  for (const auto& entry : streams) {
    if (_fileno(entry.stream) == entry.fd && DescriptorHandle(entry.fd) != nullptr) continue;
    const auto target = static_cast<HANDLE>(destination);
    if (!BindStream(entry.stream, entry.fd, target)) {
      ok = false;
      continue;
    }
    if (UsableHandle(target)) SetStdHandle(entry.id, DescriptorHandle(entry.fd));
  }
  return ok;
}

namespace {

bool Enabled(const Environment& environment, std::string_view name) {
  const auto value = environment.Get(name);
  return value.has_value() && !value->empty() && *value != "0";
}

std::string LocalTimestamp(std::chrono::system_clock::time_point timestamp,
                           const char* format) {
  const std::time_t raw = std::chrono::system_clock::to_time_t(timestamp);
  std::tm local = {};
  if (localtime_s(&local, &raw) != 0) return "unknown";
  std::array<char, 64> output{};
  if (std::strftime(output.data(), output.size(), format, &local) == 0) {
    return "unknown";
  }
  return output.data();
}

std::string IsoLocalTimestamp(std::chrono::system_clock::time_point timestamp) {
  std::string formatted = LocalTimestamp(timestamp, "%Y-%m-%dT%H:%M:%S%z");
  if (formatted.size() >= 5) {
    const std::size_t offset = formatted.size() - 5;
    if ((formatted[offset] == '+' || formatted[offset] == '-') &&
        formatted.find_first_not_of("0123456789", offset + 1) == std::string::npos) {
      formatted.insert(formatted.size() - 2, ":");
    }
  }
  return formatted;
}

std::string Utf8(const std::wstring& wide) {
  if (wide.empty()) return {};
  const int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0,
                                         nullptr, nullptr);
  std::string narrow(static_cast<std::size_t>(length > 0 ? length : 0), '\0');
  if (length > 0) {
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), narrow.data(), length, nullptr,
                        nullptr);
  }
  return narrow;
}

bool EnsureDirectory(const std::filesystem::path& directory, std::string* error) {
  std::error_code filesystem_error;
  std::filesystem::create_directories(directory, filesystem_error);
  if (filesystem_error) {
    *error = "cannot create session log directory: " + filesystem_error.message();
    return false;
  }
  const DWORD attributes = GetFileAttributesW(directory.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
      (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
    *error = "session log directory is invalid or a symlink";
    return false;
  }
  return true;
}

HANDLE CreateSessionFile(const std::filesystem::path& sessions,
                         std::chrono::system_clock::time_point started_at,
                         std::filesystem::path* path, std::string* error) {
  const std::string timestamp = LocalTimestamp(started_at, "%Y-%m-%d_%H-%M-%S");
  for (int collision = 1; collision <= 999; ++collision) {
    const std::string suffix = collision == 1 ? std::string() : "-" + std::to_string(collision);
    const std::filesystem::path candidate = sessions / std::filesystem::u8path(timestamp + suffix + ".log");
    const HANDLE file = CreateFileW(candidate.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE,
                                    nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
      *path = candidate;
      return file;
    }
    const DWORD code = GetLastError();
    if (code != ERROR_FILE_EXISTS) {
      *error = "cannot create session log (Windows error " + std::to_string(code) + ")";
      return INVALID_HANDLE_VALUE;
    }
  }
  *error = "cannot create a unique session log name";
  return INVALID_HANDLE_VALUE;
}

bool UpdateLatestLink(const std::filesystem::path& latest, const std::filesystem::path& session,
                      std::string* warning) {
  DeleteFileW(latest.c_str());
  if (CreateHardLinkW(latest.c_str(), session.c_str(), nullptr)) return true;
  *warning = "cannot update latest.log (Windows error " + std::to_string(GetLastError()) + ")";
  return false;
}

bool WriteAllHandle(HANDLE handle, const char* bytes, DWORD size) {
  while (size > 0) {
    DWORD written = 0;
    if (!WriteFile(handle, bytes, size, &written, nullptr) || written == 0) return false;
    bytes += written;
    size -= written;
  }
  return true;
}

constexpr std::size_t kMaximumPendingLine = 64 * 1024;

void AppendToLog(SessionLog::WindowsTee* tee, const char* bytes, std::size_t size) {
  if (size == 0) return;
  EnterCriticalSection(&tee->log_lock);
  WriteAllHandle(tee->log_file, bytes, static_cast<DWORD>(size));
  LeaveCriticalSection(&tee->log_lock);
}

bool CreateSessionPipe(int index, HANDLE* read_end, HANDLE* write_end) {
  static std::atomic<unsigned> counter{0};
  const std::wstring name = std::wstring(LR"(\\.\pipe\elixir-session-)") + std::to_wstring(GetCurrentProcessId()) +
                            L"-" + std::to_wstring(counter.fetch_add(1)) + L"-" + std::to_wstring(index);
  *read_end = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                               PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 0,
                               64 * 1024, 0, nullptr);
  if (*read_end == INVALID_HANDLE_VALUE) {
    *read_end = nullptr;
    return false;
  }
  *write_end = CreateFileW(name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                           SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
  if (*write_end == INVALID_HANDLE_VALUE) {
    const DWORD error = GetLastError();
    *write_end = nullptr;
    CloseHandle(*read_end);
    *read_end = nullptr;
    SetLastError(error);
    return false;
  }
  return true;
}

bool ReadPipe(SessionLog::WindowsTee::Stream* stream, char* buffer, DWORD size, DWORD* read) {
  OVERLAPPED overlapped{};
  overlapped.hEvent = stream->read_event;
  ResetEvent(stream->read_event);
  *read = 0;
  if (ReadFile(stream->read_end, buffer, size, read, &overlapped)) return *read != 0;
  if (GetLastError() != ERROR_IO_PENDING) return false;
  const HANDLE waits[2] = {stream->read_event, stream->owner->stop_event};
  if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) != WAIT_OBJECT_0) {
    CancelIoEx(stream->read_end, &overlapped);
  }
  return GetOverlappedResult(stream->read_end, &overlapped, read, TRUE) && *read != 0;
}

DWORD WINAPI ReaderThread(void* context) {
  auto* stream = static_cast<SessionLog::WindowsTee::Stream*>(context);
  SessionLog::WindowsTee* tee = stream->owner;
  std::array<char, 8192> buffer{};
  std::string pending;
  bool echo_ok = stream->echo != nullptr;
  while (!tee->abandon.load()) {
    DWORD read = 0;
    if (!ReadPipe(stream, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) break;
    if (echo_ok) echo_ok = WriteAllHandle(stream->echo, buffer.data(), read);
    pending.append(buffer.data(), read);
    const std::size_t last_newline = pending.rfind('\n');
    if (last_newline != std::string::npos) {
      AppendToLog(tee, pending.data(), last_newline + 1);
      pending.erase(0, last_newline + 1);
    } else if (pending.size() >= kMaximumPendingLine) {
      AppendToLog(tee, pending.data(), pending.size());
      pending.clear();
    }
  }
  AppendToLog(tee, pending.data(), pending.size());
  return 0;
}

std::string CpuModel() {
  int registers[4] = {};
  __cpuid(registers, static_cast<int>(0x80000000));
  if (static_cast<unsigned>(registers[0]) < 0x80000004U) return "unknown";
  char brand[49] = {};
  for (int leaf = 0; leaf < 3; ++leaf) {
    __cpuid(registers, static_cast<int>(0x80000002 + leaf));
    std::memcpy(brand + leaf * 16, registers, 16);
  }
  std::string model(brand);
  const std::size_t begin = model.find_first_not_of(' ');
  const std::size_t end = model.find_last_not_of(' ');
  return begin == std::string::npos ? "unknown" : model.substr(begin, end - begin + 1);
}

std::string TotalMemory() {
  MEMORYSTATUSEX status{};
  status.dwLength = sizeof(status);
  if (!GlobalMemoryStatusEx(&status)) return "unknown";
  return std::to_string(status.ullTotalPhys / (1024ULL * 1024ULL)) + "MiB";
}

std::string WindowsVersion() {
  using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
  const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  auto* get_version =
      ntdll != nullptr ? reinterpret_cast<RtlGetVersionFn>(
                             reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")))
                       : nullptr;
  OSVERSIONINFOW info{};
  info.dwOSVersionInfoSize = sizeof(info);
  if (get_version == nullptr || get_version(&info) != 0) return "Windows unknown";
  return "Windows " + std::to_string(info.dwMajorVersion) + "." + std::to_string(info.dwMinorVersion) + "." +
         std::to_string(info.dwBuildNumber);
}

std::string GraphicsAdapters() {
  std::string adapters;
  DISPLAY_DEVICEW device{};
  device.cb = sizeof(device);
  for (DWORD index = 0; EnumDisplayDevicesW(nullptr, index, &device, 0); ++index) {
    const std::string name = Utf8(device.DeviceString);
    if (!name.empty() && adapters.find(name) == std::string::npos) {
      if (!adapters.empty()) adapters += "; ";
      adapters += name;
    }
    device = {};
    device.cb = sizeof(device);
  }
  return adapters.empty() ? "unknown" : adapters;
}

std::filesystem::path ExecutablePath() {
  std::wstring buffer(MAX_PATH, L'\0');
  for (;;) {
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) return {};
    if (length < buffer.size()) {
      buffer.resize(length);
      return buffer;
    }
    buffer.resize(buffer.size() * 2);
  }
}

}

SessionBuildInformation CurrentSessionBuildInformation() {
  SessionBuildInformation information;
  information.version = MOCKTAIL_PROJECT_VERSION;
  information.git_commit = MOCKTAIL_BUILD_GIT_COMMIT;
  information.git_dirty = MOCKTAIL_BUILD_GIT_DIRTY;
  information.build_type = MOCKTAIL_BUILD_TYPE;
#if defined(__clang__)
  information.compiler = std::string("Clang ") + __clang_version__;
#else
  information.compiler = "unknown";
#endif
  information.target = "Windows/x86_64";
  return information;
}

SessionLog::~SessionLog() { Stop(); }

SessionLog::SessionLog(SessionLog&& other) noexcept { *this = std::move(other); }

SessionLog& SessionLog::operator=(SessionLog&& other) noexcept {
  if (this == &other) return *this;
  Stop();
  attempted_ = other.attempted_;
  active_ = other.active_;
  started_at_ = other.started_at_;
  path_ = std::move(other.path_);
  latest_path_ = std::move(other.latest_path_);
  error_ = std::move(other.error_);
  warning_ = std::move(other.warning_);
  tee_ = other.tee_;
  other.attempted_ = false;
  other.active_ = false;
  other.tee_ = nullptr;
  return *this;
}

SessionLog SessionLog::Start(const Environment& environment, const RuntimePaths& paths,
                             std::chrono::system_clock::time_point started_at) {
  SessionLog result;
  result.started_at_ = started_at;
  if (Enabled(environment, "MOCKTAIL_DISABLE_SESSION_LOG") ||
      Enabled(environment, "ELIXIR_DISABLE_SESSION_LOG") ||
      Enabled(environment, "MOCKTAIL_ISOLATED_CANARY")) {
    return result;
  }
  result.attempted_ = true;
  std::cout.flush();
  std::cerr.flush();
  (void)std::fflush(nullptr);

  const std::filesystem::path sessions = paths.logs_root() / L"sessions";
  if (!EnsureDirectory(paths.logs_root(), &result.error_) || !EnsureDirectory(sessions, &result.error_)) {
    return result;
  }
  const HANDLE log_file = CreateSessionFile(sessions, started_at, &result.path_, &result.error_);
  if (log_file == INVALID_HANDLE_VALUE) return result;
  result.latest_path_ = paths.logs_root() / L"latest.log";

  auto* tee = new WindowsTee();
  tee->log_file = log_file;
  InitializeCriticalSection(&tee->log_lock);
  const int fds[2] = {1, 2};
  const DWORD ids[2] = {STD_OUTPUT_HANDLE, STD_ERROR_HANDLE};
  bool ok = true;
  DWORD failure = 0;
  tee->stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (tee->stop_event == nullptr) {
    failure = GetLastError();
    ok = false;
  }
  if (ok) {
    QuietCrtValidation quiet;
    for (int index = 0; index < 2; ++index) {
      WindowsTee::Stream& stream = tee->streams[static_cast<std::size_t>(index)];
      FILE* const file = index == 0 ? stdout : stderr;
      stream.owner = tee;
      stream.crt_fd = fds[index];
      stream.std_handle_id = ids[index];
      stream.original_std = GetStdHandle(ids[index]);
      const int current = _fileno(file);
      HANDLE destination = DescriptorHandle(current >= 0 ? current : stream.crt_fd);
      if (destination == nullptr && UsableHandle(stream.original_std)) destination = stream.original_std;
      if (destination != nullptr) {
        DuplicateHandle(GetCurrentProcess(), destination, GetCurrentProcess(), &stream.echo, 0, FALSE,
                        DUPLICATE_SAME_ACCESS);
      }
      if ((current != stream.crt_fd || DescriptorHandle(stream.crt_fd) == nullptr) &&
          !BindStream(file, stream.crt_fd, destination)) {
        result.warning_ += std::string(result.warning_.empty() ? "" : "; ") + (index == 0 ? "stdout" : "stderr") +
                           " could not be bound to its descriptor and is not captured";
      }
    }
    for (int index = 0; index < 2 && ok; ++index) {
      WindowsTee::Stream& stream = tee->streams[static_cast<std::size_t>(index)];
      stream.saved_fd = DescriptorHandle(stream.crt_fd) != nullptr ? DuplicateAboveStandard(stream.crt_fd) : -1;
      HANDLE write_end = nullptr;
      stream.read_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
      if (stream.read_event == nullptr || !CreateSessionPipe(index, &stream.read_end, &write_end)) {
        failure = GetLastError();
        ok = false;
        break;
      }
      ok = AdoptHandleAsDescriptor(write_end, stream.crt_fd);
      if (!ok) {
        failure = GetLastError();
        break;
      }
      stream.redirected = true;
      SetStdHandle(stream.std_handle_id, DescriptorHandle(stream.crt_fd));
      stream.thread = CreateThread(nullptr, 64 * 1024, &ReaderThread, &stream, 0, nullptr);
      ok = stream.thread != nullptr;
      if (!ok) failure = GetLastError();
    }
  }
  result.tee_ = tee;
  result.active_ = true;
  if (!ok) {
    result.error_ = "cannot redirect stdout/stderr into the session log (Windows error " + std::to_string(failure) +
                    ")";
    result.Stop();
    std::error_code ignored;
    std::filesystem::remove(result.path_, ignored);
    return result;
  }
  (void)std::setvbuf(stdout, nullptr, _IONBF, 0);
  (void)std::setvbuf(stderr, nullptr, _IONBF, 0);
  (void)UpdateLatestLink(result.latest_path_, result.path_, &result.warning_);
  return result;
}

std::string SessionLog::Header(const Environment& environment, const RuntimePaths& paths,
                               std::string_view graphics_backend) const {
  (void)environment;
  const SessionBuildInformation build = CurrentSessionBuildInformation();
  const unsigned int threads = std::thread::hardware_concurrency();
  std::ostringstream output;
  output << "[elixir] version=" << build.version << " commit=" << build.git_commit;
  if (build.git_dirty == "true") output << " dirty=true";
  output << " build=" << build.build_type << " compiler=\"" << build.compiler << "\"\n"
         << "[elixir] started=" << IsoLocalTimestamp(started_at_) << " package="
         << (paths.portable() ? "portable" : "installed") << " target=" << build.target << " host=\""
         << WindowsVersion() << "\" pid=" << GetCurrentProcessId() << '\n'
         << "[elixir] cpu=\"" << CpuModel() << "\" threads=";
  if (threads == 0) {
    output << "unknown";
  } else {
    output << threads;
  }
  output << " ram=" << TotalMemory() << " gpu=\"" << GraphicsAdapters() << "\" graphics="
         << (graphics_backend.empty() ? "unknown" : graphics_backend) << '\n'
         << "[elixir] executable=" << ExecutablePath().u8string() << " config=" << paths.config_file().u8string()
         << '\n'
         << "[elixir] log=" << path_.u8string() << '\n';
  if (!warning_.empty()) output << "[elixir] warning=" << warning_ << '\n';
  return output.str();
}

void SessionLog::Stop() {
  if (!active_ || tee_ == nullptr) {
    active_ = false;
    return;
  }
  std::cout.flush();
  std::cerr.flush();
  (void)std::fflush(nullptr);
  WindowsTee* tee = tee_;
  {
    QuietCrtValidation quiet;
    for (WindowsTee::Stream& stream : tee->streams) {
      if (stream.crt_fd < 0) continue;
      if (stream.redirected) {
        bool restored = false;
        if (stream.saved_fd >= 0) {
          restored = _dup2(stream.saved_fd, stream.crt_fd) == 0;
        } else if (const HANDLE nul = WritableCopy(nullptr); nul != nullptr) {
          restored = AdoptHandleAsDescriptor(nul, stream.crt_fd);
        }
        if (!restored) _close(stream.crt_fd);
        stream.redirected = false;
        SetStdHandle(stream.std_handle_id, stream.original_std);
      }
      if (stream.saved_fd >= 0) {
        _close(stream.saved_fd);
        stream.saved_fd = -1;
      }
    }
  }
  if (tee->stop_event != nullptr) SetEvent(tee->stop_event);
  for (WindowsTee::Stream& stream : tee->streams) {
    if (stream.thread == nullptr) continue;
    if (WaitForSingleObject(stream.thread, 3000) == WAIT_TIMEOUT) {
      tee->abandon.store(true);
      CancelIoEx(stream.read_end, nullptr);
      WaitForSingleObject(stream.thread, INFINITE);
    }
    CloseHandle(stream.thread);
    stream.thread = nullptr;
  }
  for (WindowsTee::Stream& stream : tee->streams) {
    if (stream.read_end != nullptr) CloseHandle(stream.read_end);
    if (stream.read_event != nullptr) CloseHandle(stream.read_event);
    if (stream.echo != nullptr) CloseHandle(stream.echo);
  }
  if (tee->stop_event != nullptr) CloseHandle(tee->stop_event);
  if (tee->log_file != INVALID_HANDLE_VALUE) {
    FlushFileBuffers(tee->log_file);
    CloseHandle(tee->log_file);
  }
  DeleteCriticalSection(&tee->log_lock);
  delete tee;
  tee_ = nullptr;
  active_ = false;
}

}
}

#endif
