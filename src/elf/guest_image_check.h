// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_ELF_GUEST_IMAGE_CHECK_H_
#define ELIXIR_ELF_GUEST_IMAGE_CHECK_H_

#include <cstdint>
#include <string>
#include <vector>

namespace elixir::elf {

struct GuestCodeCheck {
  std::string path;
  std::uint64_t compared_bytes = 0;
  std::uint64_t differing_bytes = 0;
  std::uint64_t differing_pages = 0;
  std::uintptr_t first_difference = 0;
  bool unreadable = false;
};

std::vector<GuestCodeCheck> CheckGuestCodeAgainstFiles();

bool GuestCodeMatchesFiles(const std::vector<GuestCodeCheck>& checks) noexcept;

}

#endif
