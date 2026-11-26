// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_IMPORT_CHECK_H_
#define ELIXIR_RUNTIME_IMPORT_CHECK_H_

#include <filesystem>
#include <string>
#include <vector>

namespace elixir::runtime {

struct ImportCheckLibrary {
  std::string soname;
  std::string kind;
  std::filesystem::path path;
  std::size_t imports = 0;
  std::size_t weak_imports = 0;
  std::vector<std::string> needed;
};

struct UnresolvedImport {
  std::string library;
  std::string symbol;
  std::string hint;
};

struct ImportCheckReport {
  std::string error;
  std::vector<ImportCheckLibrary> libraries;
  std::vector<UnresolvedImport> unresolved;
  std::vector<UnresolvedImport> weak_unresolved;
  std::size_t missing_libraries = 0;
  bool ok() const { return error.empty() && unresolved.empty() && missing_libraries == 0; }
};

ImportCheckReport CheckImports(const std::filesystem::path& library,
                               const std::vector<std::filesystem::path>& search_path);

std::string FormatImportCheckReport(const ImportCheckReport& report, const std::filesystem::path& library);

}

#endif
