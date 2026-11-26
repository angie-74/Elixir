// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/sha256.h"

#include <windows.h>
#include <bcrypt.h>

#include <vector>

#include "core/util.h"

namespace elixir::launcher {

namespace {

BCRYPT_ALG_HANDLE Provider() {
  static BCRYPT_ALG_HANDLE provider = [] {
    BCRYPT_ALG_HANDLE handle = nullptr;
    if (BCryptOpenAlgorithmProvider(&handle, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) handle = nullptr;
    return handle;
  }();
  return provider;
}

}

Sha256::Sha256() {
  BCRYPT_ALG_HANDLE provider = Provider();
  BCRYPT_HASH_HANDLE hash = nullptr;
  if (provider != nullptr && BCryptCreateHash(provider, &hash, nullptr, 0, nullptr, 0, 0) == 0) hash_ = hash;
}

Sha256::~Sha256() {
  if (hash_ != nullptr) BCryptDestroyHash(static_cast<BCRYPT_HASH_HANDLE>(hash_));
}

void Sha256::Update(const void* data, std::size_t size) {
  if (hash_ == nullptr) return;
  const auto* bytes = static_cast<const std::uint8_t*>(data);
  while (size > 0) {
    const ULONG chunk = static_cast<ULONG>(size > (1u << 30) ? (1u << 30) : size);
    if (BCryptHashData(static_cast<BCRYPT_HASH_HANDLE>(hash_), const_cast<PUCHAR>(bytes), chunk, 0) != 0) {
      failed_ = true;
      return;
    }
    bytes += chunk;
    size -= chunk;
  }
}

std::array<std::uint8_t, 32> Sha256::Finish() {
  std::array<std::uint8_t, 32> digest{};
  if (hash_ != nullptr && !failed_) {
    if (BCryptFinishHash(static_cast<BCRYPT_HASH_HANDLE>(hash_), digest.data(), 32, 0) != 0) failed_ = true;
  }
  return digest;
}

std::string Sha256::FinishHex() {
  const auto digest = Finish();
  if (hash_ == nullptr || failed_) return {};
  return ToHex(digest.data(), digest.size());
}

std::string ToHex(const std::uint8_t* data, std::size_t size) {
  static const char kDigits[] = "0123456789abcdef";
  std::string hex;
  hex.reserve(size * 2);
  for (std::size_t i = 0; i < size; ++i) {
    hex.push_back(kDigits[data[i] >> 4]);
    hex.push_back(kDigits[data[i] & 0x0F]);
  }
  return hex;
}

std::string Sha256Hex(std::string_view data) {
  Sha256 hash;
  hash.Update(data.data(), data.size());
  return hash.FinishHex();
}

bool Sha256File(const std::wstring& path, std::string* hex, std::wstring* error) {
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                            FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    if (error) *error = L"Cannot open " + path + L": " + Win32ErrorText(GetLastError());
    return false;
  }
  Sha256 hash;
  if (!hash.ok()) {
    CloseHandle(file);
    if (error) *error = L"SHA-256 is unavailable (BCryptCreateHash failed)";
    return false;
  }
  std::vector<std::uint8_t> buffer(1u << 20);
  for (;;) {
    DWORD read = 0;
    if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
      if (error) *error = L"Cannot read " + path + L": " + Win32ErrorText(GetLastError());
      CloseHandle(file);
      return false;
    }
    if (read == 0) break;
    hash.Update(buffer.data(), read);
  }
  CloseHandle(file);
  *hex = hash.FinishHex();
  if (hex->empty()) {
    if (error) *error = L"SHA-256 computation failed";
    return false;
  }
  return true;
}

bool IsSha256Hex(std::string_view text) {
  if (text.size() != 64) return false;
  for (const char c : text) {
    const bool digit = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (!digit) return false;
  }
  return true;
}

std::string NormalizeSha256(std::string_view text) {
  std::string result;
  for (const char c : text) {
    if (c == ':' || c == ' ' || c == '-') continue;
    result.push_back(c >= 'A' && c <= 'F' ? static_cast<char>(c - 'A' + 'a') : c);
  }
  return result;
}

bool Sha256Equal(std::string_view a, std::string_view b) {
  if (!IsSha256Hex(a) || !IsSha256Hex(b)) return false;
  return ToLowerAscii(a) == ToLowerAscii(b);
}

}
