// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_FFLAGS_FILE_H_
#define ELIXIR_LAUNCHER_CORE_FFLAGS_FILE_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace elixir::launcher {

struct RuntimeFlagsReport {
  std::vector<std::pair<std::string, std::string>> mandatory;
  std::vector<std::pair<std::string, std::string>> defaults;
};
bool ParseRuntimeFlagsReport(std::string_view json, RuntimeFlagsReport* report, std::string* error);

std::string StripJsonComments(std::string_view text, bool* unterminated_comment = nullptr);

struct FflagsCheck {
  bool ok = false;
  std::size_t count = 0;
  std::string error;
};

FflagsCheck CheckFflagsText(std::string_view text);

}

#endif
