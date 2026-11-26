// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "compat/elf64_reader.h"

#include <cstring>
#include <string>
#include <string_view>

namespace mocktail::compat::elf64 {
namespace {

constexpr unsigned char kClass64 = 2;
constexpr unsigned char kLittleEndian = 1;
constexpr unsigned char kCurrentVersion = 1;

std::uint16_t U16(const unsigned char* bytes) {
  return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8U));
}

std::uint32_t U32(const unsigned char* bytes) {
  return static_cast<std::uint32_t>(bytes[0]) |
         static_cast<std::uint32_t>(bytes[1]) << 8U |
         static_cast<std::uint32_t>(bytes[2]) << 16U |
         static_cast<std::uint32_t>(bytes[3]) << 24U;
}

std::uint64_t U64(const unsigned char* bytes) {
  return static_cast<std::uint64_t>(U32(bytes)) |
         static_cast<std::uint64_t>(U32(bytes + 4)) << 32U;
}

const unsigned char* Raw(std::string_view bytes, std::size_t offset) {
  return reinterpret_cast<const unsigned char*>(bytes.data()) + offset;
}

SectionHeader ReadSectionHeader(const unsigned char* raw) {
  SectionHeader header;
  header.name = U32(raw + 0);
  header.type = U32(raw + 4);
  header.flags = U64(raw + 8);
  header.address = U64(raw + 16);
  header.offset = U64(raw + 24);
  header.size = U64(raw + 32);
  header.link = U32(raw + 40);
  header.info = U32(raw + 44);
  header.alignment = U64(raw + 48);
  header.entry_size = U64(raw + 56);
  return header;
}

std::uint64_t Align(std::uint64_t value, bool eight) {
  const std::uint64_t mask = eight ? 7U : 3U;
  return (value + mask) & ~mask;
}

}

bool HasElfMagic(std::string_view bytes) {
  return bytes.size() >= 4 && bytes[0] == '\x7f' && bytes[1] == 'E' &&
         bytes[2] == 'L' && bytes[3] == 'F';
}

bool Image::Parse(std::string_view bytes, std::string* error) {
  bytes_ = {};
  sections_.clear();
  type_ = 0;
  machine_ = 0;
  section_name_index_ = 0;
  if (!HasElfMagic(bytes) || bytes.size() < kHeaderSize) {
    *error = "not an ELF object";
    return false;
  }
  const unsigned char* header = Raw(bytes, 0);
  if (header[4] != kClass64) {
    *error = "not an ELFCLASS64 object";
    return false;
  }
  if (header[5] != kLittleEndian) {
    *error = "not a little-endian ELF object";
    return false;
  }
  if (header[6] != kCurrentVersion) {
    *error = "unsupported ELF identification version";
    return false;
  }
  const std::uint64_t section_offset = U64(header + 40);
  const std::uint16_t section_entry_size = U16(header + 58);
  const std::uint16_t declared_count = U16(header + 60);
  std::uint32_t name_index = U16(header + 62);
  if (section_offset == 0) {
    if (declared_count != 0) {
      *error = "ELF declares sections without a section header table";
      return false;
    }
    bytes_ = bytes;
    type_ = U16(header + 16);
    machine_ = U16(header + 18);
    return true;
  }
  if (section_entry_size != kSectionHeaderSize) {
    *error = "ELF section header size is not 64 bytes";
    return false;
  }
  if (section_offset > bytes.size() ||
      bytes.size() - section_offset < kSectionHeaderSize) {
    *error = "ELF section header table lies outside the image";
    return false;
  }
  const SectionHeader first =
      ReadSectionHeader(Raw(bytes, static_cast<std::size_t>(section_offset)));
  std::uint64_t count = declared_count;
  if (count == 0) count = first.size;
  if (name_index == kSectionIndexExtended) name_index = first.link;
  const std::uint64_t available =
      (bytes.size() - section_offset) / kSectionHeaderSize;
  if (count > available) {
    *error = "ELF section header table lies outside the image";
    return false;
  }
  sections_.reserve(static_cast<std::size_t>(count));
  for (std::uint64_t index = 0; index < count; ++index) {
    sections_.push_back(ReadSectionHeader(Raw(
        bytes,
        static_cast<std::size_t>(section_offset + index * kSectionHeaderSize))));
  }
  bytes_ = bytes;
  type_ = U16(header + 16);
  machine_ = U16(header + 18);
  section_name_index_ = name_index;
  return true;
}

bool Image::SectionData(const SectionHeader& section,
                        std::string_view* data) const {
  if (section.type == kSectionNoBits) {
    *data = {};
    return true;
  }
  if (section.offset > bytes_.size() ||
      section.size > bytes_.size() - section.offset) {
    return false;
  }
  *data = bytes_.substr(static_cast<std::size_t>(section.offset),
                        static_cast<std::size_t>(section.size));
  return true;
}

const char* Image::String(std::size_t section_index,
                          std::uint64_t offset) const {
  if (section_index >= sections_.size()) return nullptr;
  const SectionHeader& table = sections_[section_index];
  std::string_view data;
  if (table.type != kSectionStringTable || !SectionData(table, &data) ||
      offset >= data.size()) {
    return nullptr;
  }
  const std::size_t start = static_cast<std::size_t>(offset);
  if (std::memchr(data.data() + start, '\0', data.size() - start) == nullptr) {
    return nullptr;
  }
  return data.data() + start;
}

const char* Image::SectionName(const SectionHeader& section) const {
  return String(section_name_index_, section.name);
}

bool Image::ReadSymbol(std::string_view table, std::size_t index,
                       Symbol* symbol) {
  if (table.size() < kSymbolSize ||
      index > (table.size() - kSymbolSize) / kSymbolSize) {
    return false;
  }
  const unsigned char* raw = Raw(table, index * kSymbolSize);
  symbol->name = U32(raw + 0);
  symbol->info = raw[4];
  symbol->other = raw[5];
  symbol->section_index = U16(raw + 6);
  symbol->value = U64(raw + 8);
  symbol->size = U64(raw + 16);
  return true;
}

std::size_t NextNote(std::string_view data, std::size_t offset,
                     std::uint64_t section_alignment, Note* note) {
  constexpr std::size_t kNoteHeaderSize = 12;
  const bool eight = section_alignment == 8;
  if (offset > data.size() || data.size() - offset < kNoteHeaderSize) return 0;
  const unsigned char* header = Raw(data, offset);
  const std::uint32_t name_size = U32(header + 0);
  const std::uint32_t descriptor_size = U32(header + 4);
  const std::uint32_t type = U32(header + 8);
  offset += kNoteHeaderSize;
  const std::size_t name_offset = offset;
  if (name_size > data.size() || offset > data.size() - name_size) return 0;
  const std::uint64_t padded_descriptor = Align(descriptor_size, eight);
  const std::uint64_t descriptor_offset = Align(offset + name_size, eight);
  if (descriptor_offset > data.size() ||
      data.size() - descriptor_offset < padded_descriptor) {
    return 0;
  }
  note->type = type;
  note->name = data.substr(name_offset, name_size);
  note->descriptor = data.substr(static_cast<std::size_t>(descriptor_offset),
                                 descriptor_size);
  return static_cast<std::size_t>(descriptor_offset + padded_descriptor);
}

std::string FindGnuBuildId(const Image& image) {
  constexpr char kHex[] = "0123456789abcdef";
  for (std::size_t index = 0; index < image.section_count(); ++index) {
    const SectionHeader& section = image.section(index);
    if (section.type != kSectionNote) continue;
    std::string_view data;
    if (!image.SectionData(section, &data)) continue;
    std::size_t offset = 0;
    while (offset < data.size()) {
      Note note;
      const std::size_t next = NextNote(data, offset, section.alignment, &note);
      if (next == 0 || next <= offset) break;
      if (note.type == kNoteGnuBuildId && note.name.size() >= 3 &&
          !note.descriptor.empty() && note.name.substr(0, 3) == "GNU") {
        std::string hex(note.descriptor.size() * 2U, '0');
        for (std::size_t byte = 0; byte < note.descriptor.size(); ++byte) {
          const auto value = static_cast<unsigned char>(note.descriptor[byte]);
          hex[byte * 2U] = kHex[value >> 4U];
          hex[byte * 2U + 1U] = kHex[value & 0x0fU];
        }
        return hex;
      }
      offset = next;
    }
  }
  return {};
}

}
