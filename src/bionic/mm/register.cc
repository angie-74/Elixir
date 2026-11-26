// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/mm/mm_internal.h"

namespace elixir::bionic {

void RegisterMemoryMapExports(ExportTable& table) {
  mm::Space::Get();
  mm::RegisterMappingExports(table);
  mm::RegisterAdviceExports(table);
  mm::RegisterMemfdExports(table);
  mm::RegisterBrkExports(table);
  mm::RegisterMallocExtraExports(table);
}

}
