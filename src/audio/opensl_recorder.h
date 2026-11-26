// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_AUDIO_OPENSL_RECORDER_H_
#define ELIXIR_AUDIO_OPENSL_RECORDER_H_

#include "mocktail/audio/opensl_abi.h"
#include "mocktail/audio/opensl_runtime_abi.h"

namespace mocktail::audio {

inline constexpr opensl_abi::Uint32 kOpenSlRecorderInterfaceCount = 3;

opensl_abi::Result CreateOpenSlRecorder(const opensl_abi::DataSource* source, const opensl_abi::DataSink* sink,
                                        opensl_abi::Object* recorder);

}

#endif
