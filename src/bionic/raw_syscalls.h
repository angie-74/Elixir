// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_RAW_SYSCALLS_H_
#define ELIXIR_BIONIC_RAW_SYSCALLS_H_

#include <cstdint>
#include <string>

namespace elixir::bionic {

class ExportTable;

void InstallRawSyscalls(const ExportTable& table);

std::int64_t RawSyscall(std::uint64_t number, const std::uint64_t arguments[6]) noexcept;

const char* RawSyscallName(std::uint64_t number) noexcept;

std::string RawSyscallSummary();

}

#endif
