// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_PLATFORM_WINDOWS_GUEST_UNWIND_BARRIER_H_
#define ELIXIR_PLATFORM_WINDOWS_GUEST_UNWIND_BARRIER_H_

#include <cstddef>
#include <cstdint>

namespace elixir::platform {

bool InstallGuestUnwindBarrier(std::uintptr_t start, std::uintptr_t end) noexcept;
void RemoveGuestUnwindBarriers(std::uintptr_t start, std::uintptr_t end) noexcept;
bool IsBehindGuestUnwindBarrier(std::uintptr_t address) noexcept;
std::size_t GuestUnwindBarrierCount() noexcept;

}

#endif
