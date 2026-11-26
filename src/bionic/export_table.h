// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_EXPORT_TABLE_H_
#define ELIXIR_BIONIC_EXPORT_TABLE_H_

#include <cstddef>

#include "bionic/runtime.h"

namespace elixir::bionic {

bool InitializeGuestLibc(const RuntimeOptions& options, ExportTable* table);

std::size_t InstallSyntheticLibraries(const ExportTable& table);

}

#endif
