// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_CORE_PROPERTIES_H_
#define ELIXIR_BIONIC_CORE_PROPERTIES_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace elixir::bionic::properties {

inline constexpr std::size_t kPropNameMax = 32;
inline constexpr std::size_t kPropValueMax = 92;

bool SetProperty(std::string_view name, std::string_view value);
void SetProperties(const std::vector<std::pair<std::string, std::string>>& values);
std::optional<std::string> GetProperty(std::string_view name);
std::vector<std::pair<std::string, std::string>> AllProperties();

}

#endif
