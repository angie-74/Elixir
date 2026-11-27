// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/roblox_session_store.h"

#include <windows.h>
#include <dpapi.h>
#include <sddl.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "jnivm/jnivm.h"

namespace elixir::runtime {
namespace {

constexpr std::size_t kMagicBytes = 4;
constexpr std::uint32_t kVersion = 1;
constexpr std::string_view kPrefix = ".ROBLOSECURITY=";
constexpr std::size_t kMaximumCredentialBytes = 64 * 1024;
constexpr std::size_t kMaximumFileBytes = 128 * 1024;

struct Format {
  char magic[kMagicBytes];
  std::string_view entropy;
  const wchar_t* description;
};

constexpr Format kSessionFormat{{'E', 'L', 'X', 'S'}, "Elixir/com.roblox.client/session/v1", L"Elixir Roblox session"};
constexpr Format kCookiesFormat{{'E', 'L', 'X', 'C'}, "Elixir/com.roblox.client/cookies/v1", L"Elixir Roblox cookies"};

const Format& FormatOf(RobloxSessionStore::Content content) {
  return content == RobloxSessionStore::Content::kCookies ? kCookiesFormat : kSessionFormat;
}

void WipeSecret(std::string* value) {
  if (!value->empty()) SecureZeroMemory(value->data(), value->size());
  value->clear();
}

void WipeSecret(std::vector<BYTE>* value) {
  if (!value->empty()) SecureZeroMemory(value->data(), value->size());
  value->clear();
}

std::wstring PrivateSddl() {
  std::wstring sddl = L"D:P";
  HANDLE token = nullptr;
  if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<BYTE> buffer(size);
    wchar_t* sid = nullptr;
    if (size > 0 && GetTokenInformation(token, TokenUser, buffer.data(), size, &size) &&
        ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid, &sid)) {
      sddl += L"(A;OICI;FA;;;" + std::wstring(sid) + L")";
      LocalFree(sid);
    }
    CloseHandle(token);
  }
  sddl += L"(A;OICI;FA;;;SY)";
  return sddl;
}

bool IsReparsePoint(const std::filesystem::path& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

DATA_BLOB EntropyBlob(const Format& format) {
  DATA_BLOB blob{};
  blob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(format.entropy.data()));
  blob.cbData = static_cast<DWORD>(format.entropy.size());
  return blob;
}

}

RobloxSessionStore::RobloxSessionStore(std::filesystem::path file, Content content)
    : file_(std::move(file)), content_(content) {}

bool RobloxSessionStore::IsCookieList(std::string_view cookies) {
  return !cookies.empty() && cookies.size() <= jnivm::kMaximumRobloxKeptCookieBytes &&
         jnivm::RobloxKeptCookiesFromHeader(cookies) == cookies;
}

bool RobloxSessionStore::Accepts(std::string_view value) const {
  return content_ == Content::kCookies ? IsCookieList(value) : IsCanonical(value);
}

bool RobloxSessionStore::IsCanonical(std::string_view credential) {
  if (credential.size() <= kPrefix.size() || credential.size() > kMaximumCredentialBytes) return false;
  if (credential.substr(0, kPrefix.size()) != kPrefix) return false;
  for (const char character : credential.substr(kPrefix.size())) {
    const auto byte = static_cast<unsigned char>(character);
    if (byte <= 0x20 || byte >= 0x7f || byte == ';') return false;
  }
  return true;
}

bool RobloxSessionStore::EnsureFolder() const { return EnsurePrivateFolder(file_.parent_path()); }

bool RobloxSessionStore::EnsurePrivateFolder(const std::filesystem::path& folder) {
  if (folder.empty()) return false;
  std::error_code error;
  if (std::filesystem::is_directory(folder, error)) return !IsReparsePoint(folder);
  if (!folder.parent_path().empty()) std::filesystem::create_directories(folder.parent_path(), error);
  SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, FALSE};
  const std::wstring sddl = PrivateSddl();
  if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1,
                                                            &attributes.lpSecurityDescriptor, nullptr)) {
    return false;
  }
  const BOOL created = CreateDirectoryW(folder.c_str(), &attributes);
  LocalFree(attributes.lpSecurityDescriptor);
  if (!created && GetLastError() != ERROR_ALREADY_EXISTS) return false;
  return std::filesystem::is_directory(folder, error) && !IsReparsePoint(folder);
}

RobloxSessionStore::LoadResult RobloxSessionStore::Load(std::string* credential) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return LoadLocked(credential);
}

RobloxSessionStore::LoadResult RobloxSessionStore::LoadLocked(std::string* credential) const {
  if (credential != nullptr) WipeSecret(credential);
  const DWORD attributes = GetFileAttributesW(file_.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    const DWORD error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? LoadResult::kMissing
                                                                          : LoadResult::kUnreadable;
  }
  if ((attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) != 0 ||
      IsReparsePoint(file_.parent_path())) {
    return LoadResult::kUnreadable;
  }
  HANDLE handle = CreateFileW(file_.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return LoadResult::kUnreadable;
  BY_HANDLE_FILE_INFORMATION info{};
  LARGE_INTEGER size{};
  std::vector<BYTE> bytes;
  const Format& format = FormatOf(content_);
  bool ok = GetFileInformationByHandle(handle, &info) && info.nNumberOfLinks == 1 && GetFileSizeEx(handle, &size) &&
            size.QuadPart > static_cast<LONGLONG>(kMagicBytes + sizeof(kVersion)) &&
            size.QuadPart <= static_cast<LONGLONG>(kMaximumFileBytes);
  if (ok) {
    bytes.resize(static_cast<std::size_t>(size.QuadPart));
    DWORD read = 0;
    ok = ReadFile(handle, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) && read == bytes.size();
  }
  CloseHandle(handle);
  std::uint32_t version = 0;
  if (ok) {
    std::memcpy(&version, bytes.data() + kMagicBytes, sizeof(version));
    ok = std::memcmp(bytes.data(), format.magic, kMagicBytes) == 0 && version == kVersion;
  }
  if (!ok) {
    WipeSecret(&bytes);
    return LoadResult::kUnreadable;
  }
  DATA_BLOB input{};
  input.pbData = bytes.data() + kMagicBytes + sizeof(version);
  input.cbData = static_cast<DWORD>(bytes.size() - kMagicBytes - sizeof(version));
  DATA_BLOB entropy = EntropyBlob(format);
  DATA_BLOB output{};
  const BOOL decrypted =
      CryptUnprotectData(&input, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output);
  WipeSecret(&bytes);
  if (!decrypted) return LoadResult::kUnreadable;
  std::string value(reinterpret_cast<const char*>(output.pbData), output.cbData);
  SecureZeroMemory(output.pbData, output.cbData);
  LocalFree(output.pbData);
  if (!Accepts(value)) {
    WipeSecret(&value);
    return LoadResult::kUnreadable;
  }
  if (credential != nullptr) {
    credential->reserve(value.size());
    credential->assign(value);
  }
  WipeSecret(&value);
  return LoadResult::kFound;
}

bool RobloxSessionStore::Save(std::string_view credential) {
  if (!Accepts(credential)) return false;
  std::lock_guard<std::mutex> lock(mutex_);
  if (!EnsureFolder()) return false;
  if (IsReparsePoint(file_)) return false;

  const Format& format = FormatOf(content_);
  DATA_BLOB input{};
  input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(credential.data()));
  input.cbData = static_cast<DWORD>(credential.size());
  DATA_BLOB entropy = EntropyBlob(format);
  DATA_BLOB output{};
  if (!CryptProtectData(&input, format.description, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                        &output)) {
    return false;
  }
  std::vector<BYTE> bytes(kMagicBytes + sizeof(kVersion) + output.cbData);
  std::memcpy(bytes.data(), format.magic, kMagicBytes);
  std::memcpy(bytes.data() + kMagicBytes, &kVersion, sizeof(kVersion));
  std::memcpy(bytes.data() + kMagicBytes + sizeof(kVersion), output.pbData, output.cbData);
  LocalFree(output.pbData);

  std::filesystem::path temporary = file_;
  temporary += L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
  HANDLE handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                              FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    WipeSecret(&bytes);
    return false;
  }
  DWORD written = 0;
  bool ok = WriteFile(handle, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
            written == bytes.size() && FlushFileBuffers(handle);
  CloseHandle(handle);
  WipeSecret(&bytes);
  if (ok) ok = MoveFileExW(temporary.c_str(), file_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
  if (!ok) DeleteFileW(temporary.c_str());
  return ok;
}

void RobloxSessionStore::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!IsReparsePoint(file_)) DeleteFileW(file_.c_str());
}

void RobloxSessionStore::ClearIfEquals(std::string_view credential) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string saved;
  if (LoadLocked(&saved) == LoadResult::kFound && saved == credential) DeleteFileW(file_.c_str());
  WipeSecret(&saved);
}

}
