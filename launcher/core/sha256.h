// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_SHA256_H_
#define ELIXIR_LAUNCHER_CORE_SHA256_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace elixir::launcher {

class Sha256 {
 public:
  Sha256();
  ~Sha256();
  Sha256(const Sha256&) = delete;
  Sha256& operator=(const Sha256&) = delete;

  bool ok() const { return hash_ != nullptr; }
  void Update(const void* data, std::size_t size);
  std::array<std::uint8_t, 32> Finish();
  std::string FinishHex();

 private:
  void* hash_ = nullptr;
  bool failed_ = false;
};

std::string ToHex(const std::uint8_t* data, std::size_t size);
std::string Sha256Hex(std::string_view data);
bool Sha256File(const std::wstring& path, std::string* hex, std::wstring* error);

bool IsSha256Hex(std::string_view text);
bool Sha256Equal(std::string_view a, std::string_view b);
std::string NormalizeSha256(std::string_view text);

}

#endif
