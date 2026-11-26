// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_PLATFORM_TRACE_CLOCK_H_
#define ELIXIR_PLATFORM_TRACE_CLOCK_H_

#include <chrono>

namespace elixir {

inline long long DiagnosticUtcMillis() noexcept {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
}

}
#endif
