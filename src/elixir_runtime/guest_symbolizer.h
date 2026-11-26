// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_GUEST_SYMBOLIZER_H_
#define ELIXIR_RUNTIME_GUEST_SYMBOLIZER_H_

#include <cstdint>

#include "platform/windows/crash_reporter.h"

namespace elixir::runtime {

struct GuestSymbolizerOptions {
  unsigned long loader_timeout_ms = 500;
};

bool StartGuestSymbolizer(const GuestSymbolizerOptions& options = {});
bool StopGuestSymbolizer();
bool GuestSymbolizerRunning();

bool SymbolizeGuestAddress(std::uintptr_t address, platform::SymbolInfo* info);

bool SymbolizeFromLoadedImage(std::uintptr_t address, platform::SymbolInfo* info);

struct GuestSymbolizerStats {
  unsigned long loader_answers = 0;
  unsigned long loader_misses = 0;
  unsigned long loader_timeouts = 0;
  unsigned long loader_skipped = 0;
  unsigned long image_answers = 0;
};
GuestSymbolizerStats GuestSymbolizerStatistics();

}

#endif
