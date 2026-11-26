// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "core/instance.h"

#include <windows.h>
#include <sddl.h>

#include <cstring>
#include <string>
#include <vector>

#include "core/sha256.h"
#include "core/util.h"

namespace elixir::launcher {
namespace {

constexpr char kActivatePrefix[] = "ELIXIR-ACTIVATE/1\n";
constexpr char kPagePrefix[] = "ELIXIR-PAGE/1\n";
constexpr char kLinkPrefix[] = "ELIXIR-LINK/1\n";
constexpr char kReplyMagic[] = "ELX1";
constexpr DWORD kIoTimeoutMs = 5000;
constexpr DWORD kMessageBufferBytes = static_cast<DWORD>(kMaxInstancePayloadBytes + 64);

std::wstring PipeName(const std::wstring& key) { return L"\\\\.\\pipe\\Elixir-" + key + L"-links-v1"; }
std::wstring MutexName(const std::wstring& key) { return L"Local\\Elixir-" + key; }

class Handle final {
 public:
  explicit Handle(HANDLE handle = nullptr) : handle_(handle) {}
  ~Handle() {
    if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
  }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  HANDLE get() const { return handle_; }
  bool valid() const { return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE; }

 private:
  HANDLE handle_;
};

std::vector<BYTE> TokenUserOf(HANDLE process) {
  HANDLE token = nullptr;
  if (!OpenProcessToken(process, TOKEN_QUERY, &token)) return {};
  DWORD size = 0;
  GetTokenInformation(token, TokenUser, nullptr, 0, &size);
  std::vector<BYTE> buffer(size);
  if (size == 0 || !GetTokenInformation(token, TokenUser, buffer.data(), size, &size)) buffer.clear();
  CloseHandle(token);
  return buffer;
}

PSID SidOf(std::vector<BYTE>& user) {
  return user.empty() ? nullptr : reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid;
}

bool SameUserAndSession(DWORD pid) {
  Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
  if (!process.valid()) return false;
  std::vector<BYTE> theirs = TokenUserOf(process.get());
  std::vector<BYTE> ours = TokenUserOf(GetCurrentProcess());
  DWORD their_session = 0;
  DWORD our_session = 0;
  return SidOf(theirs) != nullptr && SidOf(ours) != nullptr && EqualSid(SidOf(theirs), SidOf(ours)) &&
         ProcessIdToSessionId(pid, &their_session) && ProcessIdToSessionId(GetCurrentProcessId(), &our_session) &&
         their_session == our_session;
}

std::wstring ImagePathOf(DWORD pid) {
  Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
  if (!process.valid()) return {};
  std::wstring path(32768, L'\0');
  DWORD size = static_cast<DWORD>(path.size());
  if (!QueryFullProcessImageNameW(process.get(), 0, path.data(), &size)) return {};
  path.resize(size);
  return path;
}

bool SameText(const std::wstring& a, const std::wstring& b) {
  return !a.empty() &&
         CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()), b.c_str(), static_cast<int>(b.size()), TRUE) ==
             CSTR_EQUAL;
}

class UserOnlySecurity final {
 public:
  UserOnlySecurity() {
    std::vector<BYTE> user = TokenUserOf(GetCurrentProcess());
    LPWSTR sid_text = nullptr;
    if (SidOf(user) == nullptr || !ConvertSidToStringSidW(SidOf(user), &sid_text)) return;
    const std::wstring sddl = L"D:P(A;;GA;;;" + std::wstring(sid_text) + L")(A;;GA;;;SY)";
    LocalFree(sid_text);
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor_, nullptr)) {
      descriptor_ = nullptr;
      return;
    }
    attributes_.nLength = sizeof(attributes_);
    attributes_.lpSecurityDescriptor = descriptor_;
    attributes_.bInheritHandle = FALSE;
  }
  ~UserOnlySecurity() {
    if (descriptor_ != nullptr) LocalFree(descriptor_);
  }
  UserOnlySecurity(const UserOnlySecurity&) = delete;
  UserOnlySecurity& operator=(const UserOnlySecurity&) = delete;
  SECURITY_ATTRIBUTES* get() { return descriptor_ != nullptr ? &attributes_ : nullptr; }

 private:
  PSECURITY_DESCRIPTOR descriptor_ = nullptr;
  SECURITY_ATTRIBUTES attributes_{};
};

bool Complete(HANDLE handle, OVERLAPPED* overlapped, BOOL started, HANDLE stop, DWORD timeout_ms, DWORD* transferred,
              DWORD* error = nullptr) {
  *transferred = 0;
  if (!started) {
    const DWORD code = GetLastError();
    if (code != ERROR_IO_PENDING) {
      if (error != nullptr) *error = code;
      return false;
    }
    HANDLE waits[2] = {overlapped->hEvent, stop};
    const DWORD count = stop != nullptr ? 2 : 1;
    if (WaitForMultipleObjects(count, waits, FALSE, timeout_ms) != WAIT_OBJECT_0) {
      CancelIoEx(handle, overlapped);
      DWORD ignored = 0;
      GetOverlappedResult(handle, overlapped, &ignored, TRUE);
      if (error != nullptr) *error = ERROR_TIMEOUT;
      return false;
    }
  }
  if (!GetOverlappedResult(handle, overlapped, transferred, FALSE)) {
    if (error != nullptr) *error = GetLastError();
    return false;
  }
  return true;
}

}

std::wstring InstanceKey(const std::wstring& data_root) {
  std::wstring lowered = data_root;
  if (!lowered.empty()) CharLowerBuffW(lowered.data(), static_cast<DWORD>(lowered.size()));
  return Utf8ToWide(Sha256Hex(WideToUtf8(lowered)).substr(0, 16));
}

std::string EncodeInstanceMessage(const InstanceMessage& message) {
  if (message.kind == InstanceMessageKind::kActivate) return kActivatePrefix;
  if (message.kind == InstanceMessageKind::kOpenPage) return std::string(kPagePrefix) + message.payload;
  return std::string(kLinkPrefix) + message.payload;
}

bool DecodeInstanceMessage(std::string_view bytes, InstanceMessage* message) {
  if (bytes == kActivatePrefix) {
    message->kind = InstanceMessageKind::kActivate;
    message->payload.clear();
    return true;
  }
  const std::string_view page_prefix = kPagePrefix;
  if (bytes.substr(0, page_prefix.size()) == page_prefix) {
    const auto page = bytes.substr(page_prefix.size());
    if (page.size() > 32 || page.find_first_not_of("abcdefghijklmnopqrstuvwxyz-") != std::string_view::npos)
      return false;
    message->kind = InstanceMessageKind::kOpenPage;
    message->payload.assign(page);
    return true;
  }
  const std::string_view prefix = kLinkPrefix;
  if (bytes.substr(0, prefix.size()) != prefix) return false;
  const std::string_view payload = bytes.substr(prefix.size());
  if (payload.empty() || payload.size() > kMaxInstancePayloadBytes ||
      payload.find_first_of(std::string_view("\n\r\0", 3)) != std::string_view::npos) {
    return false;
  }
  message->kind = InstanceMessageKind::kLink;
  message->payload.assign(payload);
  return true;
}

InstanceOwner::~InstanceOwner() { Stop(); }

bool InstanceOwner::Acquire(const std::wstring& key) {
  if (mutex_ != nullptr) return true;
  UserOnlySecurity security;
  HANDLE mutex = CreateMutexW(security.get(), FALSE, MutexName(key).c_str());
  const DWORD error = GetLastError();
  if (mutex == nullptr) return false;
  if (error == ERROR_ALREADY_EXISTS) {
    CloseHandle(mutex);
    return false;
  }
  mutex_ = mutex;
  key_ = key;
  return true;
}

bool InstanceOwner::Listen(std::function<InstanceReply(const InstanceMessage&)> handler, std::wstring* error) {
  if (mutex_ == nullptr || pipe_ != nullptr) {
    if (error != nullptr) *error = L"not the owner of this data folder";
    return false;
  }
  UserOnlySecurity security;
  if (security.get() == nullptr) {
    if (error != nullptr) *error = L"cannot describe the current user";
    return false;
  }
  HANDLE pipe = CreateNamedPipeW(PipeName(key_).c_str(),
                                 PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                 PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1,
                                 4096, kMessageBufferBytes, 0, security.get());
  if (pipe == INVALID_HANDLE_VALUE) {
    if (error != nullptr) *error = L"cannot create the instance pipe: " + Win32ErrorText(GetLastError());
    return false;
  }
  stop_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (stop_ == nullptr) {
    CloseHandle(pipe);
    if (error != nullptr) *error = L"cannot create an event";
    return false;
  }
  pipe_ = pipe;
  handler_ = std::move(handler);
  thread_ = std::thread([this] { Serve(); });
  return true;
}

void InstanceOwner::Serve() {
  std::vector<char> buffer(kMessageBufferBytes);
  Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
  if (!event.valid()) return;
  for (;;) {
    OVERLAPPED connect{};
    connect.hEvent = event.get();
    ResetEvent(event.get());
    DWORD transferred = 0;
    DWORD code = 0;
    const BOOL connected_now = ConnectNamedPipe(pipe_, &connect);
    if (!connected_now && GetLastError() == ERROR_PIPE_CONNECTED) {
    } else if (!Complete(pipe_, &connect, connected_now, stop_, INFINITE, &transferred, &code)) {
      if (WaitForSingleObject(stop_, 0) == WAIT_OBJECT_0) return;
      DisconnectNamedPipe(pipe_);
      continue;
    }

    InstanceReply reply = InstanceReply::kDenied;
    ULONG client = 0;
    if (GetNamedPipeClientProcessId(pipe_, &client) && SameUserAndSession(client)) {
      OVERLAPPED read{};
      read.hEvent = event.get();
      ResetEvent(event.get());
      DWORD bytes = 0;
      const BOOL read_now = ReadFile(pipe_, buffer.data(), kMessageBufferBytes, nullptr, &read);
      if (Complete(pipe_, &read, read_now, stop_, kIoTimeoutMs, &bytes, &code)) {
        InstanceMessage message;
        reply = DecodeInstanceMessage(std::string_view(buffer.data(), bytes), &message) ? handler_(message)
                                                                                         : InstanceReply::kInvalid;
        SecureZeroMemory(message.payload.data(), message.payload.size());
        SecureZeroMemory(buffer.data(), bytes);
      } else {
        reply = InstanceReply::kInvalid;
      }
    }
    const char answer[5] = {kReplyMagic[0], kReplyMagic[1], kReplyMagic[2], kReplyMagic[3],
                             static_cast<char>(reply)};
    OVERLAPPED write{};
    write.hEvent = event.get();
    ResetEvent(event.get());
    const BOOL written_now = WriteFile(pipe_, answer, sizeof(answer), nullptr, &write);
    if (Complete(pipe_, &write, written_now, stop_, kIoTimeoutMs, &transferred)) {
      char drain = 0;
      OVERLAPPED wait{};
      wait.hEvent = event.get();
      ResetEvent(event.get());
      const BOOL drained_now = ReadFile(pipe_, &drain, 1, nullptr, &wait);
      Complete(pipe_, &wait, drained_now, stop_, kIoTimeoutMs, &transferred);
    }
    DisconnectNamedPipe(pipe_);
    if (WaitForSingleObject(stop_, 0) == WAIT_OBJECT_0) return;
  }
}

void InstanceOwner::Stop() {
  if (stop_ != nullptr) SetEvent(stop_);
  if (thread_.joinable()) thread_.join();
  if (pipe_ != nullptr) {
    CloseHandle(pipe_);
    pipe_ = nullptr;
  }
  if (stop_ != nullptr) {
    CloseHandle(stop_);
    stop_ = nullptr;
  }
  if (mutex_ != nullptr) {
    CloseHandle(mutex_);
    mutex_ = nullptr;
  }
}

bool SendToInstanceOwner(const std::wstring& key, const InstanceMessage& message, unsigned timeout_ms,
                         InstanceReply* reply, std::wstring* error) {
  std::wstring local_error;
  std::wstring& failure = error != nullptr ? *error : local_error;
  const std::wstring name = PipeName(key);
  const ULONGLONG deadline = GetTickCount64() + timeout_ms;
  HANDLE raw = INVALID_HANDLE_VALUE;
  for (;;) {
    raw = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                      FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
    if (raw != INVALID_HANDLE_VALUE) break;
    const DWORD code = GetLastError();
    if (GetTickCount64() >= deadline) {
      failure = L"no running Elixir answered";
      return false;
    }
    if (code == ERROR_PIPE_BUSY) {
      WaitNamedPipeW(name.c_str(), 250);
    } else {
      Sleep(100);
    }
  }
  Handle pipe(raw);
  DWORD mode = PIPE_READMODE_MESSAGE;
  SetNamedPipeHandleState(pipe.get(), &mode, nullptr, nullptr);
  ULONG server = 0;
  if (!GetNamedPipeServerProcessId(pipe.get(), &server) || !SameUserAndSession(server) ||
      !SameText(ImagePathOf(server), ExecutablePath())) {
    failure = L"the running copy of Elixir could not be verified";
    return false;
  }
  AllowSetForegroundWindow(server);

  std::string bytes = EncodeInstanceMessage(message);
  Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
  OVERLAPPED write{};
  write.hEvent = event.get();
  DWORD transferred = 0;
  const BOOL written_now = WriteFile(pipe.get(), bytes.data(), static_cast<DWORD>(bytes.size()), nullptr, &write);
  const bool written = Complete(pipe.get(), &write, written_now, nullptr, kIoTimeoutMs, &transferred) &&
                       transferred == bytes.size();
  SecureZeroMemory(bytes.data(), bytes.size());
  if (!written) {
    failure = L"the request could not be sent";
    return false;
  }
  char answer[8] = {};
  OVERLAPPED read{};
  read.hEvent = event.get();
  ResetEvent(event.get());
  const BOOL read_now = ReadFile(pipe.get(), answer, sizeof(answer), nullptr, &read);
  if (!Complete(pipe.get(), &read, read_now, nullptr, kIoTimeoutMs, &transferred) || transferred != 5 ||
      std::memcmp(answer, kReplyMagic, 4) != 0 || static_cast<unsigned char>(answer[4]) > 4) {
    failure = L"no answer from the running copy of Elixir";
    return false;
  }
  if (reply != nullptr) *reply = static_cast<InstanceReply>(answer[4]);
  return true;
}

}
