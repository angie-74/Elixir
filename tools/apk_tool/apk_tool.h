// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_TOOLS_APK_TOOL_H_
#define ELIXIR_TOOLS_APK_TOOL_H_

#include <ostream>
#include <string>
#include <vector>

namespace elixir::apk_tool {

int Run(const std::vector<std::string>& arguments, std::ostream& output,
        std::ostream& errors);

}

#endif
