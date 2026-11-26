// Modified by Elixir.
#include "runtime/webview_helper_launcher.h"

#if defined(_WIN32)
#include "runtime/url_split.h"

#include <windows.h>
#include <bcrypt.h>

#include <cstdint>
#include <cwchar>
#else
#include "runtime/helper_environment.h"

#include <arpa/inet.h>
#include <curl/curl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#if !defined(_WIN32)
extern char **environ;
#endif

namespace mocktail {
namespace runtime {

struct WebViewHelperProcess::State {
  std::atomic<pid_t> child{-1};
  std::mutex control_mutex;
  int control_descriptor = -1;
  std::deque<WebViewHelperEvent> pending_events;
  std::string receive_buffer;
  bool tcp = false;
  bool ready = false;
#if defined(_WIN32)
  void* pipe = nullptr;
  void* process = nullptr;
#endif

  ~State() {
#if defined(_WIN32)
    if (pipe != nullptr) {
      CloseHandle(pipe);
    }
    if (process != nullptr) {
      CloseHandle(process);
    }
#else
    if (control_descriptor >= 0) {
      close(control_descriptor);
    }
#endif
  }
};

class SensitiveStringGuard final {
 public:
  explicit SensitiveStringGuard(std::string* value) : value_(value) {}
  ~SensitiveStringGuard() {
    if (value_ != nullptr) {
      std::fill(value_->begin(), value_->end(), '\0');
      value_->clear();
    }
  }

  SensitiveStringGuard(const SensitiveStringGuard&) = delete;
  SensitiveStringGuard& operator=(const SensitiveStringGuard&) = delete;

 private:
  std::string* value_;
};

WebViewHelperProcess::WebViewHelperProcess(std::shared_ptr<State> state)
    : state_(std::move(state)) {}

bool WebViewHelperProcess::running() const {
  if (state_ == nullptr) {
    return false;
  }
  if (!state_->tcp) {
    return process_id() > 0;
  }
#if defined(_WIN32)
  return false;
#else
  std::lock_guard<std::mutex> lock(state_->control_mutex);
  if (state_->control_descriptor < 0 ||
      state_->child.load(std::memory_order_acquire) <= 0) {
    return false;
  }
  pollfd probe = {state_->control_descriptor, POLLIN, 0};
  if (poll(&probe, 1, 0) > 0) {
    if ((probe.revents & (POLLHUP | POLLERR | POLLNVAL)) != 0) {
      state_->child.store(-1, std::memory_order_release);
      return false;
    }
    if ((probe.revents & POLLIN) != 0) {
      char peek;
      const ssize_t count = recv(state_->control_descriptor, &peek, 1,
                                 MSG_PEEK | MSG_DONTWAIT);
      if (count == 0) {
        state_->child.store(-1, std::memory_order_release);
        return false;
      }
    }
  }
  return true;
#endif
}

pid_t WebViewHelperProcess::process_id() const {
  return state_ != nullptr ? state_->child.load(std::memory_order_acquire) : -1;
}

namespace {

constexpr std::string_view kProtocolHeader = "MOCKTAIL-WEBVIEW 1\n";
constexpr std::string_view kControlMagic = "MWVC";
constexpr std::string_view kEventMagic = "MWVE";
constexpr unsigned char kControlProtocolVersion = 1;
constexpr std::size_t kControlHeaderBytes = 12;
constexpr std::size_t kMaximumRequestBytes =
    kProtocolHeader.size() + 20 + 1 + kMaximumWebViewUrlBytes;
constexpr std::string_view kInternalRoutePrefix = "www:";
constexpr std::string_view kRobloxWebOrigin = "https://www.roblox.com/";

#if !defined(_WIN32)
class ScopedFileDescriptor final {
public:
  explicit ScopedFileDescriptor(int descriptor = -1)
      : descriptor_(descriptor) {}
  ~ScopedFileDescriptor() {
    if (descriptor_ >= 0) {
      close(descriptor_);
    }
  }

  ScopedFileDescriptor(const ScopedFileDescriptor &) = delete;
  ScopedFileDescriptor &operator=(const ScopedFileDescriptor &) = delete;

  int get() const { return descriptor_; }

  int release() {
    const int descriptor = descriptor_;
    descriptor_ = -1;
    return descriptor;
  }

private:
  int descriptor_ = -1;
};

class ScopedCurlUrl final {
public:
  ScopedCurlUrl() : handle_(curl_url()) {}
  ~ScopedCurlUrl() {
    if (handle_ != nullptr) {
      curl_url_cleanup(handle_);
    }
  }

  CURLU *get() const { return handle_; }

private:
  CURLU *handle_ = nullptr;
};

class ScopedCurlString final {
public:
  ~ScopedCurlString() { curl_free(value_); }
  char **output() { return &value_; }
  const char *get() const { return value_; }

private:
  char *value_ = nullptr;
};

bool HostIsFreeBsd() {
  char buffer[512] = {};
  std::ifstream file("/proc/version", std::ios::binary);
  file.read(buffer, sizeof(buffer) - 1);
  const std::string_view text(buffer, static_cast<std::size_t>(file.gcount()));
  return text.find("FreeBSD") != std::string_view::npos ||
         text.find("freebsd.org") != std::string_view::npos;
}

std::string FramePacket(std::string_view packet) {
  std::string out(4, '\0');
  const std::size_t size = packet.size();
  out[0] = static_cast<char>((size >> 24) & 0xff);
  out[1] = static_cast<char>((size >> 16) & 0xff);
  out[2] = static_cast<char>((size >> 8) & 0xff);
  out[3] = static_cast<char>(size & 0xff);
  out.append(packet);
  return out;
}

bool ExtractFrame(std::string *buffer, std::string *frame) {
  if (buffer->size() < 4) {
    return false;
  }
  const std::size_t size =
      (static_cast<std::size_t>(static_cast<unsigned char>((*buffer)[0])) << 24) |
      (static_cast<std::size_t>(static_cast<unsigned char>((*buffer)[1])) << 16) |
      (static_cast<std::size_t>(static_cast<unsigned char>((*buffer)[2])) << 8) |
      static_cast<std::size_t>(static_cast<unsigned char>((*buffer)[3]));
  if (size > kMaximumWebViewEventPacketBytes) {
    buffer->clear();
    return false;
  }
  if (buffer->size() < 4 + size) {
    return false;
  }
  frame->assign(*buffer, 4, size);
  buffer->erase(0, 4 + size);
  return true;
}

int ConnectToFreeBsdHost() {
  const int descriptor = socket(AF_INET, SOCK_STREAM, 0);
  if (descriptor < 0) {
    return -1;
  }
  fcntl(descriptor, F_SETFD, FD_CLOEXEC);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(2137);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  int enabled = 1;
  setsockopt(descriptor, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled));
  if (connect(descriptor, reinterpret_cast<sockaddr *>(&address),
              sizeof(address)) != 0) {
    close(descriptor);
    return -1;
  }
  const int flags = fcntl(descriptor, F_GETFL);
  if (flags < 0 || fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) != 0) {
    close(descriptor);
    return -1;
  }
  return descriptor;
}

ssize_t ReceivePacket(WebViewHelperProcess::State *state,
                      std::string *packet) {
  if (!state->tcp) {
    packet->assign(kMaximumWebViewEventPacketBytes, '\0');
    const ssize_t count = recv(state->control_descriptor, packet->data(),
                               packet->size(), MSG_DONTWAIT | MSG_TRUNC);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return -1;
    }
    if (count < 0) {
      return -2;
    }
    if (count > 0 && static_cast<std::size_t>(count) <= packet->size()) {
      packet->resize(static_cast<std::size_t>(count));
    }
    return count;
  }
  while (true) {
    if (ExtractFrame(&state->receive_buffer, packet)) {
      return static_cast<ssize_t>(packet->size());
    }
    char chunk[4096];
    const ssize_t count = read(state->control_descriptor, chunk, sizeof(chunk));
    if (count < 0 && errno == EINTR) {
      continue;
    }
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return -1;
    }
    if (count < 0) {
      return -2;
    }
    if (count == 0) {
      return 0;
    }
    state->receive_buffer.append(chunk, static_cast<std::size_t>(count));
    if (state->receive_buffer.size() > kMaximumWebViewEventPacketBytes * 2) {
      return -2;
    }
  }
}
#endif

bool IsRobloxHost(std::string_view host) {
  constexpr std::string_view kRoot = "roblox.com";
  std::string normalized(host);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return normalized == kRoot ||
         (normalized.size() > kRoot.size() &&
          normalized.compare(normalized.size() - kRoot.size(), kRoot.size(),
                             kRoot) == 0 &&
          normalized[normalized.size() - kRoot.size() - 1] == '.');
}

bool ContainsControlBytes(std::string_view value) {
  return std::any_of(value.begin(), value.end(), [](unsigned char character) {
    return character < 0x20 || character == 0x7f;
  });
}

int HexDigitValue(char character) {
  if (character >= '0' && character <= '9') {
    return character - '0';
  }
  if (character >= 'a' && character <= 'f') {
    return character - 'a' + 10;
  }
  if (character >= 'A' && character <= 'F') {
    return character - 'A' + 10;
  }
  return -1;
}

bool HasUnsafePath(std::string_view path) {
  std::string segment;
  segment.reserve(path.size());
  for (std::size_t index = 0; index <= path.size(); ++index) {
    if (index == path.size() || path[index] == '/') {
      if (segment == "." || segment == "..") {
        return true;
      }
      segment.clear();
      continue;
    }

    unsigned char decoded = static_cast<unsigned char>(path[index]);
    if (path[index] == '%') {
      if (index + 2 >= path.size()) {
        return true;
      }
      const int high = HexDigitValue(path[index + 1]);
      const int low = HexDigitValue(path[index + 2]);
      if (high < 0 || low < 0) {
        return true;
      }
      decoded = static_cast<unsigned char>((high << 4) | low);
      index += 2;
      if (decoded == '/' || decoded == '\\') {
        return true;
      }
    }
    if (decoded < 0x20 || decoded == 0x7f || decoded == '\\') {
      return true;
    }
    segment.push_back(static_cast<char>(decoded));
  }
  return false;
}

std::string EncodeRequest(std::string_view url) {
  std::string request(kProtocolHeader);
  request += std::to_string(url.size());
  request += '\n';
  request.append(url);
  return request;
}

std::string EncodeControlPacket(WebViewHelperControlOperation operation,
                                std::string_view payload) {
  std::string packet(kControlHeaderBytes, '\0');
  std::copy(kControlMagic.begin(), kControlMagic.end(), packet.begin());
  packet[4] = static_cast<char>(kControlProtocolVersion);
  packet[5] = static_cast<char>(operation);
  const std::size_t size = payload.size();
  packet[8] = static_cast<char>((size >> 24) & 0xff);
  packet[9] = static_cast<char>((size >> 16) & 0xff);
  packet[10] = static_cast<char>((size >> 8) & 0xff);
  packet[11] = static_cast<char>(size & 0xff);
  packet.append(payload);
  return packet;
}

#if defined(_WIN32)
std::atomic<bool> g_helper_closed_by_user{false};
constexpr DWORD kPipeBufferBytes = 256 * 1024;
constexpr DWORD kControlWriteTimeoutMs = 2000;

bool WritePipeMessage(HANDLE pipe, std::string_view bytes, DWORD timeout_ms) {
  OVERLAPPED overlapped{};
  overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (overlapped.hEvent == nullptr) {
    return false;
  }
  DWORD written = 0;
  bool ok = WriteFile(pipe, bytes.data(), static_cast<DWORD>(bytes.size()),
                      &written, &overlapped) != 0;
  if (!ok && GetLastError() == ERROR_IO_PENDING) {
    ok = GetOverlappedResultEx(pipe, &overlapped, &written, timeout_ms,
                               FALSE) != 0;
    if (!ok) {
      CancelIoEx(pipe, &overlapped);
      GetOverlappedResult(pipe, &overlapped, &written, TRUE);
    }
  }
  CloseHandle(overlapped.hEvent);
  return ok && written == bytes.size();
}

int ReceivePacket(WebViewHelperProcess::State* state, std::string* packet) {
  packet->clear();
  DWORD total = 0;
  DWORD message_bytes = 0;
  if (!PeekNamedPipe(state->pipe, nullptr, 0, nullptr, &total,
                     &message_bytes)) {
    return -1;
  }
  if (total == 0) {
    return 0;
  }
  const bool oversized = message_bytes > kMaximumWebViewEventPacketBytes;
  std::string buffer(oversized ? kMaximumWebViewEventPacketBytes
                               : std::max<DWORD>(message_bytes, 1),
                     '\0');
  OVERLAPPED overlapped{};
  overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (overlapped.hEvent == nullptr) {
    return -1;
  }
  DWORD read = 0;
  bool ok = ReadFile(state->pipe, buffer.data(),
                     static_cast<DWORD>(buffer.size()), &read,
                     &overlapped) != 0;
  if (!ok && GetLastError() == ERROR_IO_PENDING) {
    ok = GetOverlappedResult(state->pipe, &overlapped, &read, TRUE) != 0;
  }
  const DWORD status = ok ? ERROR_SUCCESS : GetLastError();
  CloseHandle(overlapped.hEvent);
  if (!ok && status != ERROR_MORE_DATA) {
    return -1;
  }
  if (ok && !oversized) {
    buffer.resize(read);
    *packet = std::move(buffer);
  }
  return 1;
}

bool SendControlCommand(
    const std::shared_ptr<WebViewHelperProcess::State>& state,
    WebViewHelperControlOperation operation, std::string_view payload) {
  if (state == nullptr) {
    return false;
  }
  std::string packet = EncodeControlPacket(operation, payload);
  bool sent = false;
  {
    std::lock_guard<std::mutex> lock(state->control_mutex);
    if (state->child.load(std::memory_order_acquire) > 0 &&
        state->pipe != nullptr) {
      sent = WritePipeMessage(state->pipe, packet, kControlWriteTimeoutMs);
    }
  }
  std::fill(packet.begin(), packet.end(), '\0');
  return sent;
}

void WatchHelperProcess(HANDLE process,
                        std::shared_ptr<WebViewHelperProcess::State> state,
                        WebViewHelperExitObserver exit_observer) {
  std::thread([process, state = std::move(state),
               exit_observer = std::move(exit_observer)]() mutable {
    WaitForSingleObject(process, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(process, &exit_code);
    g_helper_closed_by_user.store(
        exit_code == static_cast<DWORD>(kWebViewHelperClosedByUserExitCode),
        std::memory_order_release);
    CloseHandle(process);
    state->child.store(-1, std::memory_order_release);
    {
      std::lock_guard<std::mutex> lock(state->control_mutex);
      if (state->pipe != nullptr) {
        CloseHandle(state->pipe);
        state->pipe = nullptr;
      }
      if (state->process != nullptr) {
        CloseHandle(state->process);
        state->process = nullptr;
      }
    }
    state.reset();
    if (exit_observer.valid()) {
      exit_observer.on_exit(exit_observer.context.get());
    }
  }).detach();
}

struct ScopedHandle final {
  HANDLE handle = nullptr;
  ScopedHandle() = default;
  explicit ScopedHandle(HANDLE value) : handle(value) {}
  ScopedHandle(const ScopedHandle&) = delete;
  ScopedHandle& operator=(const ScopedHandle&) = delete;
  ~ScopedHandle() { reset(); }
  bool valid() const {
    return handle != nullptr && handle != INVALID_HANDLE_VALUE;
  }
  void reset() {
    if (valid()) {
      CloseHandle(handle);
    }
    handle = nullptr;
  }
  HANDLE release() {
    const HANDLE value = handle;
    handle = nullptr;
    return value;
  }
};

std::wstring PrivatePipeName() {
  unsigned char random[16] = {};
  if (BCryptGenRandom(nullptr, random, sizeof(random),
                      BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
    return {};
  }
  wchar_t name[128] = {};
  int length = swprintf(name, 128, L"\\\\.\\pipe\\Elixir.WebView.%lu.",
                        static_cast<unsigned long>(GetCurrentProcessId()));
  for (const unsigned char byte : random) {
    length += swprintf(name + length, 128 - static_cast<std::size_t>(length),
                       L"%02x", byte);
  }
  return name;
}

struct WindowsHelperLaunch {
  std::shared_ptr<WebViewHelperProcess::State> state;
  pid_t process_id = -1;
  std::string error;
};

WindowsHelperLaunch LaunchWindowsWebViewHelper(
    const std::filesystem::path& helper, const std::string& normalized_url,
    WebViewHelperExitObserver exit_observer) {
  WindowsHelperLaunch result;
  std::error_code filesystem_error;
  if (!helper.is_absolute() ||
      !std::filesystem::is_regular_file(helper, filesystem_error) ||
      filesystem_error) {
    result.error = "webview helper is not an absolute regular file";
    return result;
  }

  const std::wstring pipe_name = PrivatePipeName();
  if (pipe_name.empty()) {
    result.error = "cannot name the private webview control channel";
    return result;
  }
  ScopedHandle server(CreateNamedPipeW(
      pipe_name.c_str(),
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT |
          PIPE_REJECT_REMOTE_CLIENTS,
      1, kPipeBufferBytes, kPipeBufferBytes, 0, nullptr));
  SECURITY_ATTRIBUTES inheritable{sizeof(inheritable), nullptr, TRUE};
  ScopedHandle client;
  if (server.valid()) {
    client.handle = CreateFileW(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE,
                                0, &inheritable, OPEN_EXISTING,
                                FILE_FLAG_OVERLAPPED, nullptr);
  }
  if (!server.valid() || !client.valid()) {
    result.error = "cannot create private webview control channel";
    return result;
  }

  HANDLE request_read_raw = nullptr;
  HANDLE request_write_raw = nullptr;
  if (!CreatePipe(&request_read_raw, &request_write_raw, &inheritable, 0)) {
    result.error = "cannot create bounded webview request";
    return result;
  }
  ScopedHandle request_read(request_read_raw);
  ScopedHandle request_write(request_write_raw);
  SetHandleInformation(request_write.handle, HANDLE_FLAG_INHERIT, 0);

  ScopedHandle log;
  const HANDLE own_log = GetStdHandle(STD_ERROR_HANDLE);
  if (own_log != nullptr && own_log != INVALID_HANDLE_VALUE) {
    HANDLE duplicate = nullptr;
    if (DuplicateHandle(GetCurrentProcess(), own_log, GetCurrentProcess(),
                        &duplicate, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
      log.handle = duplicate;
    }
  }

  HANDLE inherited[3] = {request_read.handle, client.handle, nullptr};
  DWORD inherited_count = 2;
  if (log.valid()) {
    inherited[inherited_count++] = log.handle;
  }
  SIZE_T attribute_bytes = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_bytes);
  std::string attribute_storage(attribute_bytes, '\0');
  auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
      attribute_storage.data());
  if (attribute_bytes == 0 ||
      !InitializeProcThreadAttributeList(attributes, 1, 0, &attribute_bytes)) {
    result.error = "cannot initialize webview helper process";
    return result;
  }
  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof(startup);
  startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  startup.StartupInfo.hStdInput = request_read.handle;
  startup.StartupInfo.hStdOutput = log.valid() ? log.handle : nullptr;
  startup.StartupInfo.hStdError = log.valid() ? log.handle : nullptr;
  startup.lpAttributeList = attributes;
  std::wstring command = L"\"" + helper.wstring() + L"\" --elixir-webview --control " +
                         std::to_wstring(reinterpret_cast<std::uintptr_t>(client.handle));
  PROCESS_INFORMATION process{};
  const bool listed = UpdateProcThreadAttribute(
                          attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                          inherited, inherited_count * sizeof(HANDLE), nullptr,
                          nullptr) != 0;
  const bool started =
      listed && CreateProcessW(helper.c_str(), command.data(), nullptr, nullptr,
                               TRUE, EXTENDED_STARTUPINFO_PRESENT, nullptr,
                               helper.parent_path().c_str(),
                               &startup.StartupInfo, &process) != 0;
  const DWORD start_error = started ? ERROR_SUCCESS : GetLastError();
  DeleteProcThreadAttributeList(attributes);
  if (!started) {
    result.error = "cannot start webview helper (Windows error " +
                   std::to_string(start_error) + ")";
    return result;
  }
  CloseHandle(process.hThread);
  AllowSetForegroundWindow(process.dwProcessId);
  request_read.reset();
  client.reset();
  log.reset();

  std::string request = EncodeRequest(normalized_url);
  SensitiveStringGuard request_guard(&request);
  DWORD request_written = 0;
  const bool requested =
      WriteFile(request_write.handle, request.data(),
                static_cast<DWORD>(request.size()), &request_written,
                nullptr) != 0 &&
      request_written == request.size();
  request_write.reset();
  if (!requested) {
    TerminateProcess(process.hProcess, 1);
    CloseHandle(process.hProcess);
    result.error = "cannot send the bounded webview request";
    return result;
  }

  HANDLE watched = nullptr;
  if (!DuplicateHandle(GetCurrentProcess(), process.hProcess,
                       GetCurrentProcess(), &watched,
                       SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                       0)) {
    TerminateProcess(process.hProcess, 1);
    CloseHandle(process.hProcess);
    result.error = "cannot own webview helper process";
    return result;
  }
  auto process_state = std::make_shared<WebViewHelperProcess::State>();
  process_state->child.store(static_cast<pid_t>(process.dwProcessId),
                             std::memory_order_release);
  process_state->pipe = server.release();
  process_state->process = process.hProcess;
  WatchHelperProcess(watched, process_state, std::move(exit_observer));
  result.process_id = static_cast<pid_t>(process.dwProcessId);
  result.state = std::move(process_state);
  return result;
}
#else
bool WriteAll(int descriptor, std::string_view bytes) {
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const ssize_t written =
        write(descriptor, bytes.data() + offset, bytes.size() - offset);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (written == 0) {
      return false;
    }
    offset += static_cast<std::size_t>(written);
  }
  return true;
}

bool SendControlCommand(
    const std::shared_ptr<WebViewHelperProcess::State>& state,
    WebViewHelperControlOperation operation, std::string_view payload) {
  if (state == nullptr) {
    return false;
  }
  std::string packet = EncodeControlPacket(operation, payload);
  bool sent = false;
  {
    std::lock_guard<std::mutex> lock(state->control_mutex);
    if (state->child.load(std::memory_order_acquire) > 0 &&
        state->control_descriptor >= 0) {
      if (state->tcp) {
        const std::string framed = FramePacket(packet);
        std::size_t offset = 0;
        sent = true;
        while (offset < framed.size()) {
          const ssize_t count =
              send(state->control_descriptor, framed.data() + offset,
                   framed.size() - offset, MSG_NOSIGNAL);
          if (count < 0 && errno == EINTR) {
            continue;
          }
          if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            pollfd writable = {state->control_descriptor, POLLOUT, 0};
            if (poll(&writable, 1, 1000) <= 0) {
              sent = false;
              break;
            }
            continue;
          }
          if (count <= 0) {
            sent = false;
            break;
          }
          offset += static_cast<std::size_t>(count);
        }
      } else {
        ssize_t count = -1;
        do {
          count = send(state->control_descriptor, packet.data(), packet.size(),
                       MSG_DONTWAIT | MSG_NOSIGNAL | MSG_EOR);
        } while (count < 0 && errno == EINTR);
        sent = count == static_cast<ssize_t>(packet.size());
      }
    }
  }
  std::fill(packet.begin(), packet.end(), '\0');
  return sent;
}

void CloseControlDescriptor(WebViewHelperProcess::State* state) {
  if (state == nullptr) {
    return;
  }
  std::lock_guard<std::mutex> lock(state->control_mutex);
  if (state->control_descriptor >= 0) {
    close(state->control_descriptor);
    state->control_descriptor = -1;
  }
}

struct ChildReaperContext {
  pid_t child = -1;
  std::shared_ptr<WebViewHelperProcess::State> process_state;
  WebViewHelperExitObserver exit_observer;
};

void *ReapChild(void *opaque_context) {
  std::unique_ptr<ChildReaperContext> context(
      static_cast<ChildReaperContext *>(opaque_context));
  int child_status = 0;
  while (waitpid(context->child, &child_status, 0) < 0 && errno == EINTR) {
  }
  if (context->process_state != nullptr) {
    context->process_state->child.store(-1, std::memory_order_release);
    CloseControlDescriptor(context->process_state.get());
  }
  if (context->exit_observer.valid()) {
    context->exit_observer.on_exit(context->exit_observer.context.get());
  }
  return nullptr;
}

int StartDetachedChildReaper(pid_t child,
                             std::shared_ptr<WebViewHelperProcess::State> state,
                             WebViewHelperExitObserver exit_observer) {
  std::unique_ptr<ChildReaperContext> context(
      new (std::nothrow) ChildReaperContext{child, std::move(state),
                                            std::move(exit_observer)});
  if (context == nullptr) {
    return ENOMEM;
  }

  pthread_attr_t attributes;
  int status = pthread_attr_init(&attributes);
  if (status != 0) {
    return status;
  }
  status = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
  pthread_t thread{};
  if (status == 0) {
    status = pthread_create(&thread, &attributes, &ReapChild, context.get());
  }
  pthread_attr_destroy(&attributes);
  if (status == 0) {
    context.release();
  }
  return status;
}

void TerminateAndReapChild(pid_t child) {
  if (kill(child, SIGKILL) < 0 && errno != ESRCH) {
    return;
  }
  int child_status = 0;
  while (waitpid(child, &child_status, 0) < 0 && errno == EINTR) {
  }
}
#endif

}

bool ValidateWebViewUrl(std::string_view url, std::string *error) {
  const auto fail = [error](const char *message) {
    if (error != nullptr) {
      *error = message;
    }
    return false;
  };
  if (url.empty() || url.size() > kMaximumWebViewUrlBytes ||
      ContainsControlBytes(url)) {
    return fail("webview URL is empty, oversized, or contains control bytes");
  }

#if defined(_WIN32)
  UrlComponents parsed;
  if (!SplitUrl(url, &parsed)) {
    return fail("webview URL is malformed");
  }
  if (parsed.scheme != "https" || !IsRobloxHost(parsed.host)) {
    return fail("webview URL must use HTTPS on roblox.com");
  }
  if (parsed.has_user_info) {
    return fail("webview URL must not contain user information");
  }
  if (parsed.has_port && parsed.port != "443") {
    return fail("webview URL must use the default HTTPS port");
  }
  if (HasUnsafePath(parsed.path)) {
    return fail("webview URL contains an unsafe path");
  }
  return true;
#else
  const ScopedCurlUrl parsed;
  if (parsed.get() == nullptr ||
      curl_url_set(parsed.get(), CURLUPART_URL, std::string(url).c_str(),
                   CURLU_NON_SUPPORT_SCHEME | CURLU_PATH_AS_IS) != CURLUE_OK) {
    return fail("webview URL is malformed");
  }
  ScopedCurlString scheme;
  ScopedCurlString host;
  ScopedCurlString user;
  ScopedCurlString password;
  ScopedCurlString port;
  ScopedCurlString path;
  if (curl_url_get(parsed.get(), CURLUPART_SCHEME, scheme.output(), 0) !=
          CURLUE_OK ||
      curl_url_get(parsed.get(), CURLUPART_HOST, host.output(), 0) !=
          CURLUE_OK ||
      scheme.get() == nullptr || std::string_view(scheme.get()) != "https" ||
      host.get() == nullptr || !IsRobloxHost(host.get())) {
    return fail("webview URL must use HTTPS on roblox.com");
  }
  if (curl_url_get(parsed.get(), CURLUPART_USER, user.output(), 0) ==
          CURLUE_OK ||
      curl_url_get(parsed.get(), CURLUPART_PASSWORD, password.output(), 0) ==
          CURLUE_OK) {
    return fail("webview URL must not contain user information");
  }
  const CURLUcode port_status =
      curl_url_get(parsed.get(), CURLUPART_PORT, port.output(), 0);
  if (port_status == CURLUE_OK &&
      (port.get() == nullptr || std::string_view(port.get()) != "443")) {
    return fail("webview URL must use the default HTTPS port");
  }
  if (port_status != CURLUE_OK && port_status != CURLUE_NO_PORT) {
    return fail("webview URL contains an invalid port");
  }
  if (curl_url_get(parsed.get(), CURLUPART_PATH, path.output(), 0) !=
          CURLUE_OK ||
      path.get() == nullptr || HasUnsafePath(path.get())) {
    return fail("webview URL contains an unsafe path");
  }
  return true;
#endif
}

bool NormalizeWebViewUrl(std::string_view url, std::string *normalized_url,
                         std::string *error) {
  const auto fail = [error](const char *message) {
    if (error != nullptr) {
      *error = message;
    }
    return false;
  };
  if (normalized_url == nullptr) {
    return fail("webview URL has no output destination");
  }
  if (url.empty() || url.size() > kMaximumWebViewUrlBytes ||
      ContainsControlBytes(url)) {
    return fail("webview URL is empty, oversized, or contains control bytes");
  }

  if (url.compare(0, kInternalRoutePrefix.size(), kInternalRoutePrefix) == 0) {
    const std::string_view route = url.substr(kInternalRoutePrefix.size());
    if (route.empty() || route.front() == '/') {
      return fail("webview internal Roblox route is invalid");
    }
    normalized_url->assign(kRobloxWebOrigin);
    normalized_url->append(route);
  } else {
    normalized_url->assign(url);
  }
  if (!ValidateWebViewUrl(*normalized_url, error)) {
    normalized_url->clear();
    return false;
  }
  return true;
}

bool DecodeWebViewRequest(std::string_view request, std::string *url,
                          std::string *error) {
  if (url == nullptr) {
    if (error != nullptr) {
      *error = "webview request has no output destination";
    }
    return false;
  }
  if (request.size() > kMaximumRequestBytes ||
      request.size() < kProtocolHeader.size() ||
      request.compare(0, kProtocolHeader.size(), kProtocolHeader) != 0) {
    if (error != nullptr) {
      *error = "webview request has an invalid protocol header";
    }
    return false;
  }
  const std::size_t length_end = request.find('\n', kProtocolHeader.size());
  if (length_end == std::string_view::npos) {
    if (error != nullptr) {
      *error = "webview request has no bounded URL length";
    }
    return false;
  }
  const std::string_view length_text = request.substr(
      kProtocolHeader.size(), length_end - kProtocolHeader.size());
  std::size_t expected_length = 0;
  const auto conversion =
      std::from_chars(length_text.data(),
                      length_text.data() + length_text.size(), expected_length);
  const std::string_view decoded = request.substr(length_end + 1);
  if (length_text.empty() || conversion.ec != std::errc() ||
      conversion.ptr != length_text.data() + length_text.size() ||
      expected_length != decoded.size() ||
      expected_length > kMaximumWebViewUrlBytes ||
      !ValidateWebViewUrl(decoded, error)) {
    if (error != nullptr && error->empty()) {
      *error = "webview request URL length is invalid";
    }
    return false;
  }
  url->assign(decoded);
  return true;
}

bool DecodeWebViewHelperControlPacket(std::string_view packet,
                                      WebViewHelperControlCommand* command,
                                      std::string* error) {
  if (error != nullptr) {
    error->clear();
  }
  const auto fail = [command, error](const char* message) {
    if (command != nullptr) {
      command->operation = WebViewHelperControlOperation::kClose;
      command->payload.clear();
      command->visible = false;
    }
    if (error != nullptr) {
      *error = message;
    }
    return false;
  };
  if (command == nullptr) {
    return fail("webview control command has no output destination");
  }
  if (packet.size() < kControlHeaderBytes ||
      packet.size() > kMaximumWebViewControlPacketBytes ||
      packet.compare(0, kControlMagic.size(), kControlMagic) != 0 ||
      static_cast<unsigned char>(packet[4]) != kControlProtocolVersion ||
      packet[6] != '\0' || packet[7] != '\0') {
    return fail("webview control packet has an invalid header");
  }
  const std::size_t payload_size =
      (static_cast<std::size_t>(static_cast<unsigned char>(packet[8])) << 24) |
      (static_cast<std::size_t>(static_cast<unsigned char>(packet[9])) << 16) |
      (static_cast<std::size_t>(static_cast<unsigned char>(packet[10])) << 8) |
      static_cast<std::size_t>(static_cast<unsigned char>(packet[11]));
  if (payload_size != packet.size() - kControlHeaderBytes) {
    return fail("webview control packet has an invalid payload length");
  }

  const auto operation = static_cast<WebViewHelperControlOperation>(
      static_cast<unsigned char>(packet[5]));
  const std::string_view payload = packet.substr(kControlHeaderBytes);
  const bool contains_nul = payload.find('\0') != std::string_view::npos;
  switch (operation) {
    case WebViewHelperControlOperation::kLoadUrl:
      if (!ValidateWebViewUrl(payload, error)) {
        command->operation = WebViewHelperControlOperation::kClose;
        command->payload.clear();
        command->visible = false;
        return false;
      }
      break;
    case WebViewHelperControlOperation::kSetTitle:
      if (payload.size() > kMaximumWebViewTitleBytes ||
          ContainsControlBytes(payload)) {
        return fail("webview control title is oversized or unsafe");
      }
      break;
    case WebViewHelperControlOperation::kSetVisible:
    case WebViewHelperControlOperation::kSetBackNavigationDisabled:
    case WebViewHelperControlOperation::kSetShowDomainAsTitle:
      if (payload.size() != 1 ||
          (static_cast<unsigned char>(payload.front()) != 0 &&
           static_cast<unsigned char>(payload.front()) != 1)) {
        return fail("webview control boolean payload is invalid");
      }
      break;
    case WebViewHelperControlOperation::kEvaluateJavaScript:
      if (payload.size() > kMaximumWebViewJavaScriptBytes || contains_nul) {
        return fail("webview control JavaScript is oversized or contains NUL");
      }
      break;
    case WebViewHelperControlOperation::kSetRobloxCookie:
      if (payload.empty() || payload.size() > kMaximumWebViewCookieBytes ||
          !std::all_of(payload.begin(), payload.end(), [](char byte) {
            const unsigned char value = static_cast<unsigned char>(byte);
            return value == 0x21 || (value >= 0x23 && value <= 0x2b) ||
                   (value >= 0x2d && value <= 0x3a) ||
                   (value >= 0x3c && value <= 0x5b) ||
                   (value >= 0x5d && value <= 0x7e);
          })) {
        return fail("webview Roblox cookie is oversized or unsafe");
      }
      break;
    case WebViewHelperControlOperation::kClearRobloxCookie:
      if (!payload.empty()) {
        return fail("webview Roblox cookie clear payload must be empty");
      }
      break;
    case WebViewHelperControlOperation::kRetainPersistentRobloxCookie:
      if (!payload.empty()) {
        return fail("webview Roblox cookie retention payload must be empty");
      }
      break;
    case WebViewHelperControlOperation::kClose:
      if (!payload.empty()) {
        return fail("webview control close payload must be empty");
      }
      break;
    default:
      return fail("webview control operation is unsupported");
  }

  command->operation = operation;
  command->payload.assign(payload);
  command->visible = !payload.empty() && payload.front() != '\0';
  return true;
}

bool EncodeWebViewHelperEventPacket(WebViewHelperEventType type,
                                    std::string_view payload,
                                    std::string* packet) {
  const bool ready = type == WebViewHelperEventType::kReady;
  const bool cookie = type == WebViewHelperEventType::kRobloxCookie;
  if (packet == nullptr || (!ready && payload.empty()) ||
      payload.size() > (cookie ? kMaximumWebViewCookieBytes
                               : kMaximumWebViewHybridEventBytes) ||
      payload.find('\0') != std::string_view::npos ||
      (type != WebViewHelperEventType::kExecuteRoblox &&
       type != WebViewHelperEventType::kRobloxWkHybrid && !ready && !cookie)) {
    return false;
  }
  packet->assign(kControlHeaderBytes, '\0');
  std::copy(kEventMagic.begin(), kEventMagic.end(), packet->begin());
  (*packet)[4] = static_cast<char>(kControlProtocolVersion);
  (*packet)[5] = static_cast<char>(type);
  const std::size_t size = payload.size();
  (*packet)[8] = static_cast<char>((size >> 24) & 0xff);
  (*packet)[9] = static_cast<char>((size >> 16) & 0xff);
  (*packet)[10] = static_cast<char>((size >> 8) & 0xff);
  (*packet)[11] = static_cast<char>(size & 0xff);
  packet->append(payload);
  return true;
}

bool DecodeWebViewHelperEventPacket(std::string_view packet,
                                    WebViewHelperEvent* event,
                                    std::string* error) {
  if (error != nullptr) {
    error->clear();
  }
  const auto fail = [event, error](const char* message) {
    if (event != nullptr) {
      event->type = WebViewHelperEventType::kExecuteRoblox;
      event->payload.clear();
    }
    if (error != nullptr) {
      *error = message;
    }
    return false;
  };
  if (event == nullptr) {
    return fail("webview event has no output destination");
  }
  if (packet.size() < kControlHeaderBytes ||
      packet.size() > kMaximumWebViewEventPacketBytes ||
      packet.compare(0, kEventMagic.size(), kEventMagic) != 0 ||
      static_cast<unsigned char>(packet[4]) != kControlProtocolVersion ||
      packet[6] != '\0' || packet[7] != '\0') {
    return fail("webview event packet has an invalid header");
  }
  const std::size_t payload_size =
      (static_cast<std::size_t>(static_cast<unsigned char>(packet[8])) << 24) |
      (static_cast<std::size_t>(static_cast<unsigned char>(packet[9])) << 16) |
      (static_cast<std::size_t>(static_cast<unsigned char>(packet[10])) << 8) |
      static_cast<std::size_t>(static_cast<unsigned char>(packet[11]));
  const std::string_view payload = packet.substr(kControlHeaderBytes);
  if (payload_size != payload.size() ||
      payload.find('\0') != std::string_view::npos) {
    return fail("webview event packet has an invalid payload");
  }
  const auto type = static_cast<WebViewHelperEventType>(
      static_cast<unsigned char>(packet[5]));
  if (type != WebViewHelperEventType::kExecuteRoblox &&
      type != WebViewHelperEventType::kRobloxWkHybrid &&
      type != WebViewHelperEventType::kReady &&
      type != WebViewHelperEventType::kRobloxCookie) {
    return fail("webview event type is unsupported");
  }
  if (payload.empty() != (type == WebViewHelperEventType::kReady) ||
      (type == WebViewHelperEventType::kRobloxCookie &&
       payload.size() > kMaximumWebViewCookieBytes)) {
    return fail("webview event payload does not match its type");
  }
  event->type = type;
  event->payload.assign(payload);
  return true;
}

bool WebViewHelperProcess::LoadUrl(std::string_view url) const {
  std::string normalized_url;
  if (!NormalizeWebViewUrl(url, &normalized_url, nullptr)) {
    return false;
  }
  const bool sent = SendControlCommand(
      state_, WebViewHelperControlOperation::kLoadUrl, normalized_url);
  std::fill(normalized_url.begin(), normalized_url.end(), '\0');
  return sent;
}

bool WebViewHelperProcess::SetTitle(std::string_view title) const {
  if (title.size() > kMaximumWebViewTitleBytes || ContainsControlBytes(title)) {
    return false;
  }
  return SendControlCommand(state_, WebViewHelperControlOperation::kSetTitle,
                            title);
}

bool WebViewHelperProcess::SetVisible(bool visible) const {
  const char encoded = visible ? '\1' : '\0';
  return SendControlCommand(state_, WebViewHelperControlOperation::kSetVisible,
                            std::string_view(&encoded, 1));
}

bool WebViewHelperProcess::EvaluateJavaScript(std::string_view script) const {
  if (script.size() > kMaximumWebViewJavaScriptBytes ||
      script.find('\0') != std::string_view::npos) {
    return false;
  }
  return SendControlCommand(
      state_, WebViewHelperControlOperation::kEvaluateJavaScript, script);
}

bool WebViewHelperProcess::SetBackNavigationDisabled(bool disabled) const {
  const char encoded = disabled ? '\1' : '\0';
  return SendControlCommand(
      state_, WebViewHelperControlOperation::kSetBackNavigationDisabled,
      std::string_view(&encoded, 1));
}

bool WebViewHelperProcess::SetShowDomainAsTitle(bool enabled) const {
  const char encoded = enabled ? '\1' : '\0';
  return SendControlCommand(
      state_, WebViewHelperControlOperation::kSetShowDomainAsTitle,
      std::string_view(&encoded, 1));
}

bool WebViewHelperProcess::SetRobloxCookie(std::string_view value) const {
  if (value.size() > kMaximumWebViewCookieBytes) {
    return false;
  }
  return SendControlCommand(
      state_,
      value.empty()
          ? WebViewHelperControlOperation::kRetainPersistentRobloxCookie
          : WebViewHelperControlOperation::kSetRobloxCookie,
      value);
}

bool WebViewHelperProcess::ClearRobloxCookie() const {
  return SendControlCommand(
      state_, WebViewHelperControlOperation::kClearRobloxCookie, {});
}

bool WebViewHelperProcess::WaitUntilReady(
    std::chrono::milliseconds timeout) const {
  if (state_ == nullptr || timeout.count() < 0) {
    return false;
  }
#if defined(_WIN32)
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  for (;;) {
    {
      std::lock_guard<std::mutex> lock(state_->control_mutex);
      if (state_->ready) {
        return true;
      }
      if (state_->pipe == nullptr ||
          state_->child.load(std::memory_order_acquire) <= 0) {
        return false;
      }
      std::string packet;
      for (;;) {
        const int status = ReceivePacket(state_.get(), &packet);
        if (status < 0) {
          return state_->ready;
        }
        if (status == 0) {
          break;
        }
        WebViewHelperEvent event;
        if (!DecodeWebViewHelperEventPacket(packet, &event)) {
          continue;
        }
        if (event.type == WebViewHelperEventType::kReady) {
          state_->ready = true;
          return true;
        }
        state_->pending_events.push_back(std::move(event));
      }
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    Sleep(5);
  }
#else
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  std::lock_guard<std::mutex> lock(state_->control_mutex);
  while (!state_->ready && state_->control_descriptor >= 0 &&
         state_->child.load(std::memory_order_acquire) > 0) {
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
    if (remaining.count() <= 0) {
      break;
    }
    std::string packet;
    bool closed = false;
    while (!state_->ready) {
      const ssize_t count = ReceivePacket(state_.get(), &packet);
      if (count == 0 || count == -2) {
        closed = true;
        break;
      }
      if (count < 0) {
        break;
      }
      WebViewHelperEvent event;
      if (!DecodeWebViewHelperEventPacket(packet, &event)) {
        continue;
      }
      if (event.type == WebViewHelperEventType::kReady) {
        state_->ready = true;
      } else {
        state_->pending_events.push_back(std::move(event));
      }
    }
    if (closed || state_->ready) {
      break;
    }
    pollfd descriptor = {state_->control_descriptor, POLLIN, 0};
    int poll_status = -1;
    do {
      poll_status = poll(&descriptor, 1, static_cast<int>(remaining.count()));
    } while (poll_status < 0 && errno == EINTR);
    if (poll_status < 0) {
      break;
    }
    if (poll_status > 0 && (descriptor.revents & (POLLERR | POLLNVAL)) != 0) {
      break;
    }
  }
  return state_->ready;
#endif
}

bool WebViewHelperProcess::DrainEvents(
    std::vector<WebViewHelperEvent>* events) const {
  if (state_ == nullptr || events == nullptr) {
    return false;
  }
#if defined(_WIN32)
  std::lock_guard<std::mutex> lock(state_->control_mutex);
  while (!state_->pending_events.empty()) {
    events->push_back(std::move(state_->pending_events.front()));
    state_->pending_events.pop_front();
  }
  if (state_->pipe == nullptr) {
    return !events->empty();
  }
  std::string packet;
  for (;;) {
    const int status = ReceivePacket(state_.get(), &packet);
    if (status <= 0) {
      return true;
    }
    WebViewHelperEvent event;
    if (DecodeWebViewHelperEventPacket(packet, &event)) {
      if (event.type == WebViewHelperEventType::kReady) {
        state_->ready = true;
      } else {
        events->push_back(std::move(event));
      }
    }
  }
#else
  std::lock_guard<std::mutex> lock(state_->control_mutex);
  if (state_->control_descriptor < 0) {
    return false;
  }
  while (!state_->pending_events.empty()) {
    events->push_back(std::move(state_->pending_events.front()));
    state_->pending_events.pop_front();
  }
  std::string packet;
  while (true) {
    const ssize_t count = ReceivePacket(state_.get(), &packet);
    if (count == -1 || count == 0) {
      return true;
    }
    if (count < 0) {
      return false;
    }
    WebViewHelperEvent event;
    if (DecodeWebViewHelperEventPacket(packet, &event)) {
      if (event.type == WebViewHelperEventType::kReady) {
        state_->ready = true;
      } else {
        events->push_back(std::move(event));
      }
    }
  }
#endif
}

bool WebViewHelperProcess::RequestClose() const {
  const pid_t child = process_id();
  if (child <= 0) {
    return true;
  }
  if (SendControlCommand(state_, WebViewHelperControlOperation::kClose, {})) {
    return true;
  }
  if (state_->tcp) {
    return false;
  }
  const pid_t current_child = process_id();
  if (current_child <= 0) {
    return true;
  }
#if defined(_WIN32)
  std::lock_guard<std::mutex> lock(state_->control_mutex);
  return state_->process == nullptr ||
         TerminateProcess(state_->process, 0) != 0;
#else
  return kill(current_child, SIGTERM) == 0 || errno == ESRCH;
#endif
}

bool ConsumeWebViewHelperClosedByUser() {
#if defined(_WIN32)
  return g_helper_closed_by_user.exchange(false, std::memory_order_acq_rel);
#else
  return false;
#endif
}

WebViewHelperLaunchResult LaunchWebViewHelper(
    const std::filesystem::path& helper, std::string_view url,
    WebViewHelperExitObserver exit_observer) {
  WebViewHelperLaunchResult result;
  std::string normalized_url;
  SensitiveStringGuard normalized_url_guard(&normalized_url);
  if (!NormalizeWebViewUrl(url, &normalized_url, &result.error)) {
    return result;
  }
#if defined(_WIN32)
  WindowsHelperLaunch launched = LaunchWindowsWebViewHelper(
      helper, normalized_url, std::move(exit_observer));
  if (launched.state == nullptr) {
    result.error = launched.error.empty() ? "cannot start webview helper"
                                          : launched.error;
    return result;
  }
  result.spawned = true;
  result.process_id = launched.process_id;
  result.process = std::shared_ptr<WebViewHelperProcess>(
      new WebViewHelperProcess(std::move(launched.state)));
  return result;
#else
  if (HostIsFreeBsd()) {
    const int descriptor = ConnectToFreeBsdHost();
    if (descriptor < 0) {
      result.error = "cannot connect to the FreeBSD webview host";
      return result;
    }
    auto tcp_state = std::make_shared<WebViewHelperProcess::State>();
    tcp_state->tcp = true;
    tcp_state->control_descriptor = descriptor;
    tcp_state->child.store(1, std::memory_order_release);
    result.process = std::shared_ptr<WebViewHelperProcess>(
        new WebViewHelperProcess(std::move(tcp_state)));
    result.spawned = true;
    result.process_id = 1;
    if (!result.process->LoadUrl(normalized_url)) {
      result.spawned = false;
      result.error = "cannot send the initial URL to the FreeBSD webview host";
    }
    return result;
  }
  std::error_code filesystem_error;
  const std::filesystem::file_status helper_status =
      std::filesystem::symlink_status(helper, filesystem_error);
  if (!helper.is_absolute() || filesystem_error ||
      !std::filesystem::is_regular_file(helper_status) ||
      std::filesystem::is_symlink(helper_status) ||
      access(helper.c_str(), X_OK) != 0) {
    result.error = "webview helper is not an executable absolute regular file";
    return result;
  }

  int control_descriptors[2] = {-1, -1};
  if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0,
                 control_descriptors) != 0) {
    result.error = "cannot create private webview control channel";
    return result;
  }
  ScopedFileDescriptor parent_control(control_descriptors[0]);
  const ScopedFileDescriptor child_control(control_descriptors[1]);
  if (child_control.get() == kWebViewHelperControlDescriptor) {
    const int descriptor_flags = fcntl(child_control.get(), F_GETFD);
    if (descriptor_flags < 0 || fcntl(child_control.get(), F_SETFD,
                                      descriptor_flags & ~FD_CLOEXEC) != 0) {
      result.error = "cannot prepare private webview control channel";
      return result;
    }
  }

  std::string request = EncodeRequest(normalized_url);
  SensitiveStringGuard request_guard(&request);
  const int request_descriptor =
      memfd_create("mocktail-webview-request", MFD_CLOEXEC);
  if (request_descriptor < 0) {
    result.error = "cannot create bounded webview request";
    return result;
  }
  const ScopedFileDescriptor request_file(request_descriptor);
  if (!WriteAll(request_file.get(), request) ||
      lseek(request_file.get(), 0, SEEK_SET) < 0) {
    result.error = "cannot prepare bounded webview request";
    return result;
  }

  posix_spawn_file_actions_t actions;
  int spawn_status = posix_spawn_file_actions_init(&actions);
  if (spawn_status != 0) {
    result.error = "cannot initialize webview helper process: " +
                   std::string(std::strerror(spawn_status));
    return result;
  }
  spawn_status = posix_spawn_file_actions_adddup2(&actions, request_file.get(),
                                                  STDIN_FILENO);
  if (spawn_status == 0 && request_file.get() != STDIN_FILENO) {
    spawn_status =
        posix_spawn_file_actions_addclose(&actions, request_file.get());
  }
  if (spawn_status == 0) {
    spawn_status =
        posix_spawn_file_actions_addclose(&actions, parent_control.get());
  }
  if (spawn_status == 0) {
    spawn_status = posix_spawn_file_actions_adddup2(
        &actions, child_control.get(), kWebViewHelperControlDescriptor);
  }
  if (spawn_status == 0 &&
      child_control.get() != kWebViewHelperControlDescriptor) {
    spawn_status =
        posix_spawn_file_actions_addclose(&actions, child_control.get());
  }

  pid_t child = -1;
  std::string helper_string = helper.string();
  char *arguments[] = {helper_string.data(), nullptr};
  if (spawn_status == 0) {
    const auto child_environment = BuildHelperEnvironment(environ);
    spawn_status = posix_spawn(&child, helper.c_str(), &actions, nullptr,
                               arguments, child_environment.data());
  }
  posix_spawn_file_actions_destroy(&actions);
  if (spawn_status != 0) {
    result.error = "cannot start webview helper: " +
                   std::string(std::strerror(spawn_status));
    return result;
  }
  auto process_state = std::make_shared<WebViewHelperProcess::State>();
  process_state->child.store(child, std::memory_order_release);
  process_state->control_descriptor = parent_control.release();
  const int reaper_status =
      StartDetachedChildReaper(child, process_state, std::move(exit_observer));
  if (reaper_status != 0) {
    TerminateAndReapChild(child);
    result.error = "cannot own webview helper process: " +
                   std::string(std::strerror(reaper_status));
    return result;
  }
  result.spawned = true;
  result.process_id = child;
  result.process = std::shared_ptr<WebViewHelperProcess>(
      new WebViewHelperProcess(std::move(process_state)));
  return result;
#endif
}

}
}
