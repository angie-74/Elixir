// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_NET_H_
#define ELIXIR_BIONIC_NET_H_

#include <cstddef>
#include <cstdint>

#include "abi/sysv_abi.h"

namespace elixir::bionic {
class ExportTable;
}

namespace elixir::bionic::net {

bool EnsureWinsock() noexcept;

abi::g_ssize FdRead(void* handle, std::uint8_t kind, void* buffer, std::size_t size, bool nonblocking,
                      int* guest_errno) noexcept;
abi::g_ssize FdWrite(void* handle, std::uint8_t kind, const void* buffer, std::size_t size, bool nonblocking,
                       int* guest_errno) noexcept;
int FdClose(void* handle, std::uint8_t kind, int* guest_errno) noexcept;
int FdSetNonBlocking(void* handle, std::uint8_t kind, bool nonblocking, int* guest_errno) noexcept;
int FdBytesAvailable(void* handle, std::uint8_t kind, int* available, int* guest_errno) noexcept;

void RegisterNetExports(ExportTable& table);

}

#endif
