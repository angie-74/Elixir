// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/files/files_internal.h"

namespace elixir::bionic {

void RegisterFileExports(ExportTable& table) {
  files::RegisterFdExports(table);
  files::RegisterMetadataExports(table);
  files::RegisterPathExports(table);
  files::RegisterDirExports(table);
  files::RegisterExtraFileExports(table);
  files::RegisterFdsanExports(table);
  files::RegisterFileExtExports(table);
  files::RegisterInotifyExports(table);
}

void RegisterStdioExports(ExportTable& table) {
  files::RegisterStdioCoreExports(table);
  files::RegisterStdioMemExports(table);
  files::RegisterStdioWideExports(table);
  files::RegisterStdioFormatExports(table);
  files::RegisterStdioUnlockedExports(table);
}

}
