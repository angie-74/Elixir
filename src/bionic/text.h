// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_TEXT_H_
#define ELIXIR_BIONIC_TEXT_H_

#include <cstddef>
#include <string>

#include "abi/sysv_abi.h"

namespace elixir::bionic::text {

const char* ErrorMessage(int guest_errno) noexcept;
const char* SignalMessage(int guest_signal) noexcept;

std::size_t DecodeUtf8(const char* input, std::size_t length, abi::g_wchar* out) noexcept;
std::size_t EncodeUtf8(abi::g_wchar code_point, char* out) noexcept;

std::wstring Utf8ToWide(const char* text);
std::string WideToUtf8(const wchar_t* text, int length = -1);

}

#endif
