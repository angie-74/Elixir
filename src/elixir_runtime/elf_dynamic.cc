// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/elf_dynamic.h"

#include <windows.h>

#include <algorithm>
#include <cstring>

namespace elixir::runtime {
namespace {

constexpr std::uint32_t kPtLoad = 1;
constexpr std::uint32_t kPtDynamic = 2;
constexpr std::uint32_t kPtNote = 4;
constexpr std::int64_t kDtNull = 0;
constexpr std::int64_t kDtNeeded = 1;
constexpr std::int64_t kDtHash = 4;
constexpr std::int64_t kDtStrtab = 5;
constexpr std::int64_t kDtSymtab = 6;
constexpr std::int64_t kDtStrsz = 10;
constexpr std::int64_t kDtSoname = 14;
constexpr std::int64_t kDtGnuHash = 0x6ffffef5;

template <typename T>
bool ReadAt(std::string_view bytes, std::uint64_t offset, T* out) {
  if (offset > bytes.size() || bytes.size() - offset < sizeof(T)) return false;
  std::memcpy(out, bytes.data() + offset, sizeof(T));
  return true;
}

struct Load {
  std::uint64_t vaddr, memsz, offset, filesz;
};

bool VaddrToOffset(const std::vector<Load>& loads, std::uint64_t vaddr, std::uint64_t* offset) {
  for (const Load& load : loads) {
    if (vaddr >= load.vaddr && vaddr - load.vaddr < load.filesz) {
      *offset = load.offset + (vaddr - load.vaddr);
      return true;
    }
  }
  return false;
}

bool ReadCString(std::string_view bytes, std::uint64_t table, std::uint64_t table_size, std::uint64_t index,
                 std::string* out) {
  if (index >= table_size || table > bytes.size() || bytes.size() - table < table_size) return false;
  const char* begin = bytes.data() + table + index;
  const std::size_t limit = static_cast<std::size_t>(table_size - index);
  const void* end = std::memchr(begin, '\0', limit);
  if (end == nullptr) return false;
  out->assign(begin, static_cast<const char*>(end) - begin);
  return true;
}

bool CountSymbols(std::string_view bytes, std::uint64_t gnu_hash, std::uint64_t sysv_hash, std::uint64_t* count) {
  if (gnu_hash != 0) {
    std::uint32_t header[4];
    if (!ReadAt(bytes, gnu_hash, &header)) return false;
    const std::uint32_t buckets = header[0], symoffset = header[1], bloom_words = header[2];
    const std::uint64_t bucket_table = gnu_hash + 16 + static_cast<std::uint64_t>(bloom_words) * 8;
    std::uint32_t last = 0;
    for (std::uint32_t index = 0; index < buckets; ++index) {
      std::uint32_t value = 0;
      if (!ReadAt(bytes, bucket_table + static_cast<std::uint64_t>(index) * 4, &value)) return false;
      last = std::max(last, value);
    }
    if (last < symoffset) {
      *count = symoffset;
      return true;
    }
    const std::uint64_t chains = bucket_table + static_cast<std::uint64_t>(buckets) * 4;
    for (std::uint64_t symbol = last;; ++symbol) {
      std::uint32_t hash = 0;
      if (!ReadAt(bytes, chains + (symbol - symoffset) * 4, &hash)) return false;
      if ((hash & 1U) != 0) {
        *count = symbol + 1;
        return true;
      }
      if (symbol - last > (1U << 24)) return false;
    }
  }
  if (sysv_hash != 0) {
    std::uint32_t header[2];
    if (!ReadAt(bytes, sysv_hash, &header)) return false;
    *count = header[1];
    return true;
  }
  return false;
}

std::string BuildIdFromNotes(std::string_view bytes, std::uint64_t offset, std::uint64_t size) {
  std::uint64_t cursor = offset;
  const std::uint64_t end = std::min<std::uint64_t>(offset + size, bytes.size());
  while (cursor + 12 <= end) {
    std::uint32_t header[3];
    if (!ReadAt(bytes, cursor, &header)) break;
    const std::uint64_t name_size = header[0], desc_size = header[1], type = header[2];
    const std::uint64_t name_offset = cursor + 12;
    const std::uint64_t desc_offset = name_offset + ((name_size + 3) & ~3ULL);
    const std::uint64_t next = desc_offset + ((desc_size + 3) & ~3ULL);
    if (next > end) break;
    if (type == 3 && name_size == 4 && std::memcmp(bytes.data() + name_offset, "GNU", 4) == 0 && desc_size > 0) {
      static const char kHex[] = "0123456789abcdef";
      std::string hex;
      for (std::uint64_t i = 0; i < desc_size; ++i) {
        const auto byte = static_cast<unsigned char>(bytes[desc_offset + i]);
        hex.push_back(kHex[byte >> 4]);
        hex.push_back(kHex[byte & 15]);
      }
      return hex;
    }
    cursor = next;
  }
  return {};
}

}

std::vector<std::string> ElfDynamicInfo::ExportedNames() const {
  std::vector<std::string> names;
  for (const ElfDynamicSymbol& symbol : symbols) {
    if (symbol.undefined() || symbol.name.empty()) continue;
    if (symbol.binding != 1 && symbol.binding != 2) continue;
    names.push_back(symbol.name);
  }
  return names;
}

bool ParseElfDynamic(std::string_view bytes, ElfDynamicInfo* info, std::string* error) {
  *info = ElfDynamicInfo();
  if (bytes.size() < 64 || std::memcmp(bytes.data(), "\x7f" "ELF", 4) != 0) {
    *error = "not an ELF file";
    return false;
  }
  if (bytes[4] != 2 || bytes[5] != 1) {
    *error = "not a 64-bit little-endian ELF file";
    return false;
  }
  std::uint16_t type = 0, machine = 0, phentsize = 0, phnum = 0;
  std::uint64_t phoff = 0;
  ReadAt(bytes, 16, &type);
  ReadAt(bytes, 18, &machine);
  ReadAt(bytes, 32, &phoff);
  ReadAt(bytes, 54, &phentsize);
  ReadAt(bytes, 56, &phnum);
  info->machine = machine;
  if (type != 3) {
    *error = "not a shared object (ET_DYN)";
    return false;
  }
  if (phentsize != 56 || phnum == 0) {
    *error = "invalid program header table";
    return false;
  }
  std::vector<Load> loads;
  std::uint64_t dynamic_offset = 0, dynamic_size = 0;
  bool have_dynamic = false;
  for (std::uint16_t index = 0; index < phnum; ++index) {
    const std::uint64_t entry = phoff + static_cast<std::uint64_t>(index) * 56;
    std::uint32_t p_type = 0;
    std::uint64_t p_offset = 0, p_vaddr = 0, p_filesz = 0, p_memsz = 0;
    if (!ReadAt(bytes, entry, &p_type) || !ReadAt(bytes, entry + 8, &p_offset) ||
        !ReadAt(bytes, entry + 16, &p_vaddr) || !ReadAt(bytes, entry + 32, &p_filesz) ||
        !ReadAt(bytes, entry + 40, &p_memsz)) {
      *error = "program header table is truncated";
      return false;
    }
    if (p_type == kPtLoad) loads.push_back({p_vaddr, p_memsz, p_offset, p_filesz});
    if (p_type == kPtDynamic) {
      have_dynamic = true;
      dynamic_offset = p_offset;
      dynamic_size = p_filesz;
    }
    if (p_type == kPtNote && info->build_id.empty()) info->build_id = BuildIdFromNotes(bytes, p_offset, p_filesz);
  }
  if (!have_dynamic) {
    *error = "no PT_DYNAMIC segment";
    return false;
  }
  std::uint64_t strtab = 0, strsz = 0, symtab = 0, gnu_hash = 0, sysv_hash = 0, soname = 0;
  bool have_soname = false;
  std::vector<std::uint64_t> needed;
  for (std::uint64_t cursor = dynamic_offset; cursor + 16 <= dynamic_offset + dynamic_size; cursor += 16) {
    std::int64_t tag = 0;
    std::uint64_t value = 0;
    if (!ReadAt(bytes, cursor, &tag) || !ReadAt(bytes, cursor + 8, &value)) {
      *error = "dynamic segment is truncated";
      return false;
    }
    if (tag == kDtNull) break;
    switch (tag) {
      case kDtNeeded: needed.push_back(value); break;
      case kDtStrtab: strtab = value; break;
      case kDtStrsz: strsz = value; break;
      case kDtSymtab: symtab = value; break;
      case kDtGnuHash: gnu_hash = value; break;
      case kDtHash: sysv_hash = value; break;
      case kDtSoname: soname = value; have_soname = true; break;
      default: break;
    }
  }
  std::uint64_t strtab_offset = 0, symtab_offset = 0, gnu_offset = 0, sysv_offset = 0;
  if (!VaddrToOffset(loads, strtab, &strtab_offset) || !VaddrToOffset(loads, symtab, &symtab_offset)) {
    *error = "DT_STRTAB or DT_SYMTAB is outside the file";
    return false;
  }
  if (gnu_hash != 0 && !VaddrToOffset(loads, gnu_hash, &gnu_offset)) gnu_offset = 0;
  if (sysv_hash != 0 && !VaddrToOffset(loads, sysv_hash, &sysv_offset)) sysv_offset = 0;
  for (const std::uint64_t name : needed) {
    std::string text;
    if (!ReadCString(bytes, strtab_offset, strsz, name, &text)) {
      *error = "DT_NEEDED name is outside the string table";
      return false;
    }
    info->needed.push_back(std::move(text));
  }
  if (have_soname) ReadCString(bytes, strtab_offset, strsz, soname, &info->soname);
  std::uint64_t count = 0;
  if (!CountSymbols(bytes, gnu_offset, sysv_offset, &count)) {
    if (strtab_offset <= symtab_offset) {
      *error = "cannot size the dynamic symbol table (no DT_GNU_HASH or DT_HASH)";
      return false;
    }
    count = (strtab_offset - symtab_offset) / 24;
  }
  if (count > (bytes.size() / 24) + 1) {
    *error = "dynamic symbol count is implausible";
    return false;
  }
  info->symbols.reserve(static_cast<std::size_t>(count));
  for (std::uint64_t index = 1; index < count; ++index) {
    const std::uint64_t entry = symtab_offset + index * 24;
    std::uint32_t st_name = 0;
    std::uint8_t st_info = 0;
    std::uint16_t st_shndx = 0;
    std::uint64_t st_value = 0, st_size = 0;
    if (!ReadAt(bytes, entry, &st_name) || !ReadAt(bytes, entry + 4, &st_info) ||
        !ReadAt(bytes, entry + 6, &st_shndx) || !ReadAt(bytes, entry + 8, &st_value) ||
        !ReadAt(bytes, entry + 16, &st_size)) {
      *error = "dynamic symbol table is truncated";
      return false;
    }
    ElfDynamicSymbol symbol;
    ReadCString(bytes, strtab_offset, strsz, st_name, &symbol.name);
    symbol.binding = static_cast<std::uint8_t>(st_info >> 4);
    symbol.type = static_cast<std::uint8_t>(st_info & 15);
    symbol.section = st_shndx;
    symbol.value = st_value;
    symbol.size = st_size;
    info->symbols.push_back(std::move(symbol));
  }
  return true;
}

bool ReadElfDynamicFile(const std::filesystem::path& path, ElfDynamicInfo* info, std::string* error) {
  const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    *error = "cannot open (Windows error " + std::to_string(GetLastError()) + ")";
    return false;
  }
  LARGE_INTEGER size{};
  if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart >= (4LL << 30)) {
    CloseHandle(file);
    *error = "cannot read the file (empty, or 4 GiB or more)";
    return false;
  }
  const HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
  const void* view = mapping != nullptr ? MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0) : nullptr;
  const DWORD map_error = view == nullptr ? GetLastError() : 0;
  if (mapping != nullptr) CloseHandle(mapping);
  bool ok = false;
  if (view == nullptr) {
    *error = "cannot map the file (Windows error " + std::to_string(map_error) + ")";
  } else {
    ok = ParseElfDynamic(std::string_view(static_cast<const char*>(view), static_cast<std::size_t>(size.QuadPart)),
                         info, error);
    UnmapViewOfFile(view);
  }
  CloseHandle(file);
  return ok;
}

}
