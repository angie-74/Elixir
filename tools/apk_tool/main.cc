// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <iostream>
#include <string>
#include <vector>

#include "apk_tool.h"

namespace {

std::string ToUtf8(const wchar_t* text) {
  const int size =
      WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
  if (size <= 1) return {};
  std::string result(static_cast<std::size_t>(size - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr,
                      nullptr);
  return result;
}

}

int wmain(int argc, wchar_t** argv) {
  std::vector<std::string> arguments;
  for (int index = 1; index < argc; ++index) {
    arguments.push_back(ToUtf8(argv[index]));
  }
  std::ios::sync_with_stdio(false);
  return elixir::apk_tool::Run(arguments, std::cout, std::cerr);
}
