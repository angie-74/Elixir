// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#ifndef ELIXIR_RUNTIME_NETWORK_TRACE_H_
#define ELIXIR_RUNTIME_NETWORK_TRACE_H_

#include <optional>
#include <string>
#include <string_view>

namespace elixir::runtime {

std::optional<std::string> FormatGuestHttpOutcomeTrace(std::string_view body) noexcept;

}

#endif
