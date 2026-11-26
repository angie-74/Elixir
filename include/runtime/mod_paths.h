// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_MOD_PATHS_H_
#define ELIXIR_RUNTIME_MOD_PATHS_H_

#include <cstddef>
#include <string_view>

namespace elixir::mods {

inline constexpr std::string_view kReplaceableFolders[] = {
    "content/textures/", "content/sounds/", "content/fonts/", "content/sky/", "ExtraContent/textures/",
};

inline constexpr wchar_t kModsFolderName[] = L"Modifications";

namespace detail {
constexpr char Lower(char c) noexcept { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }
}

inline bool IsReplaceable(std::string_view relative) noexcept {
  if (relative.empty() || relative.front() == '/' || relative.back() == '/') return false;
  std::size_t begin = 0;
  while (begin <= relative.size()) {
    std::size_t end = relative.find('/', begin);
    if (end == std::string_view::npos) end = relative.size();
    const std::string_view part = relative.substr(begin, end - begin);
    if (part.empty() || part == "." || part == "..") return false;
    if (part.find('\\') != std::string_view::npos || part.find(':') != std::string_view::npos) return false;
    begin = end + 1;
  }
  for (const std::string_view folder : kReplaceableFolders) {
    if (relative.size() <= folder.size()) continue;
    bool same = true;
    for (std::size_t i = 0; i < folder.size() && same; ++i) {
      same = detail::Lower(relative[i]) == detail::Lower(folder[i]);
    }
    if (same) return true;
  }
  return false;
}

}

#endif
