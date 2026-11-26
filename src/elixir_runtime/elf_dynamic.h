// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_ELF_DYNAMIC_H_
#define ELIXIR_RUNTIME_ELF_DYNAMIC_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace elixir::runtime {

inline constexpr std::uint16_t kElfMachineX86_64 = 62;

struct ElfDynamicSymbol {
  std::string name;
  std::uint8_t binding = 0;
  std::uint8_t type = 0;
  std::uint16_t section = 0;
  std::uint64_t value = 0;
  std::uint64_t size = 0;

  bool undefined() const { return section == 0; }
  bool weak() const { return binding == 2; }
};

struct ElfDynamicInfo {
  std::uint16_t machine = 0;
  std::string soname;
  std::vector<std::string> needed;
  std::vector<ElfDynamicSymbol> symbols;
  std::string build_id;

  std::vector<std::string> ExportedNames() const;
};

bool ParseElfDynamic(std::string_view bytes, ElfDynamicInfo* info, std::string* error);
bool ReadElfDynamicFile(const std::filesystem::path& path, ElfDynamicInfo* info, std::string* error);

}

#endif
