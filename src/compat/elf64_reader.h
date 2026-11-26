// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef MOCKTAIL_COMPAT_ELF64_READER_H_
#define MOCKTAIL_COMPAT_ELF64_READER_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mocktail::compat::elf64 {

inline constexpr std::uint32_t kSectionStringTable = 3;
inline constexpr std::uint32_t kSectionNote = 7;
inline constexpr std::uint32_t kSectionNoBits = 8;
inline constexpr std::uint32_t kSectionDynamicSymbols = 11;
inline constexpr std::uint16_t kSectionUndefined = 0;
inline constexpr std::uint16_t kSectionIndexExtended = 0xffff;
inline constexpr std::uint32_t kNoteGnuBuildId = 3;
inline constexpr std::size_t kHeaderSize = 64;
inline constexpr std::size_t kSectionHeaderSize = 64;
inline constexpr std::size_t kSymbolSize = 24;

struct SectionHeader {
  std::uint32_t name = 0;
  std::uint32_t type = 0;
  std::uint64_t flags = 0;
  std::uint64_t address = 0;
  std::uint64_t offset = 0;
  std::uint64_t size = 0;
  std::uint32_t link = 0;
  std::uint32_t info = 0;
  std::uint64_t alignment = 0;
  std::uint64_t entry_size = 0;
};

struct Symbol {
  std::uint32_t name = 0;
  std::uint8_t info = 0;
  std::uint8_t other = 0;
  std::uint16_t section_index = 0;
  std::uint64_t value = 0;
  std::uint64_t size = 0;
};

struct Note {
  std::uint32_t type = 0;
  std::string_view name;
  std::string_view descriptor;
};

bool HasElfMagic(std::string_view bytes);

class Image final {
 public:
  bool Parse(std::string_view bytes, std::string* error);

  std::uint16_t type() const { return type_; }
  std::uint16_t machine() const { return machine_; }
  std::size_t section_count() const { return sections_.size(); }
  const SectionHeader& section(std::size_t index) const {
    return sections_[index];
  }
  std::size_t section_name_index() const { return section_name_index_; }

  bool SectionData(const SectionHeader& section, std::string_view* data) const;

  const char* String(std::size_t section_index, std::uint64_t offset) const;
  const char* SectionName(const SectionHeader& section) const;

  static bool ReadSymbol(std::string_view table, std::size_t index,
                         Symbol* symbol);

 private:
  std::string_view bytes_;
  std::uint16_t type_ = 0;
  std::uint16_t machine_ = 0;
  std::size_t section_name_index_ = 0;
  std::vector<SectionHeader> sections_;
};

std::size_t NextNote(std::string_view data, std::size_t offset,
                     std::uint64_t section_alignment, Note* note);

std::string FindGnuBuildId(const Image& image);

}

#endif
