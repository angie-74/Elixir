// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/runtime.h"
#include "bionic/text/text_internal.h"

namespace elixir::bionic {

void RegisterStringExports(ExportTable& table) {
  text_internal::RegisterMessageExports(table);
  text_internal::RegisterStringFunctionExports(table);
  text_internal::RegisterFortifyExports(table);
  text_internal::RegisterCtypeExports(table);
  text_internal::RegisterUnicodeExports(table);
  text_internal::RegisterWcharExports(table);
  text_internal::RegisterLocaleExports(table);
  text_internal::RegisterIconvExports(table);
}

void RegisterStdlibExports(ExportTable& table) {
  text_internal::RegisterIntegerParseExports(table);
  text_internal::RegisterFloatParseExports(table);
  text_internal::RegisterStdlibMiscExports(table);
  text_internal::RegisterEnvironmentExports(table);
  text_internal::RegisterMallocExports(table);
}

}
