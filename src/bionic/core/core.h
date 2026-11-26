// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_CORE_CORE_H_
#define ELIXIR_BIONIC_CORE_CORE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace elixir::bionic::core {

struct GuestIdentity {
  std::uint32_t uid = 10123;
  std::uint32_t gid = 10123;
  std::vector<std::uint32_t> groups = {3003, 9997, 20123, 50123};
};
void SetGuestIdentity(const GuestIdentity& identity);
GuestIdentity CurrentGuestIdentity();

std::string GuestAuxiliaryVector();

using ExitHook = void (*)();
void AddExitHook(ExitHook hook) noexcept;

void SetTimeZoneOverride(const char* tz);
std::string HostTimeZoneIanaName();

using FaultFilter = bool (*)(std::uintptr_t rip) noexcept;
void SetFaultDeliveryFilter(FaultFilter filter) noexcept;
std::uint64_t CurrentSignalMask() noexcept;
void SetSignalMask(std::uint64_t mask) noexcept;

std::uint64_t DeliveredFaultCount() noexcept;
int RaiseGuestSignal(int sig) noexcept;
int SendGuestSignalToThread(std::int32_t tid, int sig) noexcept;

}

#endif
