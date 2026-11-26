// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_TEXT_H_
#define ELIXIR_RUNTIME_TEXT_H_

#include <filesystem>
#include <string>
#include <string_view>

namespace elixir::runtime {

std::wstring WideFromUtf8(std::string_view text);
std::string Utf8FromWide(std::wstring_view text);
std::string GenericUtf8(const std::filesystem::path& path);
std::string NativeUtf8(const std::filesystem::path& path);
std::filesystem::path PathFromUtf8(std::string_view text);
std::string JsonQuote(std::string_view text);

}

#endif
