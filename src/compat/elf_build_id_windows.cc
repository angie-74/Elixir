// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "compat/elf_build_id.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

#include "compat/elf64_reader.h"
#include "update/host_file_windows.h"

namespace mocktail::compat {
namespace {

BuildIdResult Failure(std::string error) {
  return BuildIdResult{{}, std::move(error)};
}

}

BuildIdResult ReadElfBuildId(const std::string& path) {
  if (path.empty()) {
    return Failure("ELF path is empty");
  }
  std::filesystem::path native;
  if (!update::host_file::PathFromUtf8(path, &native)) {
    return Failure("ELF path is not valid UTF-8");
  }
  update::host_file::ReadOnlyMapping mapping;
  switch (mapping.Map(native)) {
    case update::host_file::ReadOnlyMapping::Status::kOk:
      break;
    case update::host_file::ReadOnlyMapping::Status::kUnavailable:
      return Failure("open(" + path + "): cannot open file");
    case update::host_file::ReadOnlyMapping::Status::kNotRegular:
    case update::host_file::ReadOnlyMapping::Status::kTooLarge:
      return Failure(path + " is not an ELF object");
    case update::host_file::ReadOnlyMapping::Status::kMapFailed:
      return Failure("cannot map " + path);
  }
  const std::string_view bytes(reinterpret_cast<const char*>(mapping.data()),
                               mapping.size());
  if (!elf64::HasElfMagic(bytes)) {
    return Failure(path + " is not an ELF object");
  }
  elf64::Image image;
  std::string error;
  if (!image.Parse(bytes, &error)) {
    return Failure(path + ": " + error);
  }
  std::string build_id = elf64::FindGnuBuildId(image);
  if (build_id.empty()) {
    return Failure(path + " has no GNU build ID");
  }
  return BuildIdResult{std::move(build_id), {}};
}

}
