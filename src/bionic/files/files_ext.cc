// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/files/files_internal.h"

namespace elixir::bionic::files {
namespace {

int CloseRange(unsigned first, unsigned last, unsigned flags) {
  if ((flags & ~(kCLOSE_RANGE_UNSHARE | kCLOSE_RANGE_CLOEXEC)) != 0) return FailWith(guest::kEINVAL, -1);
  if (first > last) return FailWith(guest::kEINVAL, -1);
  const int saved = GuestErrno();
  for (const int fd : OpenFds()) {
    const auto number = static_cast<unsigned>(fd);
    if (number < first || number > last) continue;
    if ((flags & kCLOSE_RANGE_CLOEXEC) != 0) {
      SetCloexec(fd, true);
    } else {
      CloseFd(fd);
    }
  }
  SetGuestErrno(saved);
  return 0;
}

}

ELIXIR_BIONIC_FN(int, close_range)(unsigned first, unsigned last, int flags) noexcept {
  return Guarded(-1, [&] { return CloseRange(first, last, static_cast<unsigned>(flags)); });
}

void RegisterFileExtExports(ExportTable& table) {
  table.Add(kLibc, "close_range", reinterpret_cast<void*>(&elixir_bionic_close_range));
}

}
