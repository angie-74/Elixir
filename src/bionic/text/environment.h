// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_TEXT_ENVIRONMENT_H_
#define ELIXIR_BIONIC_TEXT_ENVIRONMENT_H_

#include <string>
#include <vector>

namespace elixir::bionic::text {

void SetGuestEnvironment(const std::vector<std::string>& entries);

char*** GuestEnvironLocation() noexcept;

void SetGuestProgramName(const char* name);

}

#endif
