// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_ABI_SYSV_VA_LIST_H_
#define ELIXIR_ABI_SYSV_VA_LIST_H_

#include <cstdint>
#include <cstring>

#include "abi/sysv_abi.h"

namespace elixir::abi {

struct SysvVaList {
  std::uint32_t gp_offset;
  std::uint32_t fp_offset;
  void* overflow_arg_area;
  void* reg_save_area;
};
static_assert(sizeof(SysvVaList) == 24, "SysV va_list layout");

inline constexpr std::uint32_t kSysvGpSaveBytes = 6 * 8;
inline constexpr std::uint32_t kSysvFpSaveEnd = kSysvGpSaveBytes + 8 * 16;

class SysvVaCursor {
 public:
  explicit SysvVaCursor(SysvVaList* list) noexcept : list_(list), state_(*list) {}

  std::uint64_t NextGp() noexcept {
    if (state_.gp_offset < kSysvGpSaveBytes) {
      std::uint64_t value;
      std::memcpy(&value, static_cast<char*>(state_.reg_save_area) + state_.gp_offset, 8);
      state_.gp_offset += 8;
      return value;
    }
    return NextStackWord();
  }

  double NextDouble() noexcept {
    if (state_.fp_offset < kSysvFpSaveEnd) {
      double value;
      std::memcpy(&value, static_cast<char*>(state_.reg_save_area) + state_.fp_offset, 8);
      state_.fp_offset += 16;
      return value;
    }
    std::uint64_t bits = NextStackWord();
    double value;
    std::memcpy(&value, &bits, 8);
    return value;
  }

  GuestLongDouble NextLongDouble() noexcept {
    GuestLongDouble value;
    if (state_.fp_offset < kSysvFpSaveEnd) {
      std::memcpy(&value, static_cast<char*>(state_.reg_save_area) + state_.fp_offset, sizeof(value));
      state_.fp_offset += 16;
      return value;
    }
    auto address = reinterpret_cast<std::uintptr_t>(state_.overflow_arg_area);
    address = (address + 15) & ~static_cast<std::uintptr_t>(15);
    std::memcpy(&value, reinterpret_cast<void*>(address), sizeof(value));
    state_.overflow_arg_area = reinterpret_cast<void*>(address + 16);
    return value;
  }

  template <typename T>
  T NextPointer() noexcept {
    return reinterpret_cast<T>(static_cast<std::uintptr_t>(NextGp()));
  }

  std::int32_t NextInt32() noexcept { return static_cast<std::int32_t>(NextGp()); }
  std::int64_t NextInt64() noexcept { return static_cast<std::int64_t>(NextGp()); }

  void Commit() noexcept { *list_ = state_; }
  const SysvVaList& state() const noexcept { return state_; }

 private:
  std::uint64_t NextStackWord() noexcept {
    std::uint64_t value;
    std::memcpy(&value, state_.overflow_arg_area, 8);
    state_.overflow_arg_area = static_cast<char*>(state_.overflow_arg_area) + 8;
    return value;
  }

  SysvVaList* list_;
  SysvVaList state_;
};

}

#endif
