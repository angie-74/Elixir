// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_GUEST_EXT_H_
#define ELIXIR_BIONIC_GUEST_EXT_H_

#include <string>
#include <vector>

namespace elixir::bionic {

inline constexpr const char* kGuestLibcExtensionName = "libelixir-extension.so";

bool LoadGuestLibcExtension(const std::string& library_path, std::string* error);

std::vector<std::string> GuestLibcExtensionExports();

}

#endif
