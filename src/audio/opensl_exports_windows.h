// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_AUDIO_OPENSL_EXPORTS_WINDOWS_H_
#define ELIXIR_AUDIO_OPENSL_EXPORTS_WINDOWS_H_

#include <string>
#include <unordered_map>

namespace elixir::audio {

inline constexpr const char kOpenSlSoname[] = "libOpenSLES.so";

void RegisterOpenSlExports(std::unordered_map<std::string, void*>& exports);

}

#endif
