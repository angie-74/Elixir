// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "platform/windows/raw_syscall_scan.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <utility>

namespace elixir::platform {
namespace {

enum class OpcodeMap : std::uint8_t { kOne, k0F, k0F38, k0F3A };

bool IsLegacyPrefix(std::uint8_t byte) noexcept {
  switch (byte) {
    case 0x26: case 0x2E: case 0x36: case 0x3E: case 0x64: case 0x65:
    case 0x66: case 0x67: case 0xF0: case 0xF2: case 0xF3:
      return true;
    default:
      return false;
  }
}

}

X86Instruction DecodeX86(const std::uint8_t* code, std::size_t available) noexcept {
  const X86Instruction unknown;
  if (available > 15) available = 15;
  std::size_t p = 0;
  bool operand16 = false, address32 = false, rex_w = false;
  while (p < available && IsLegacyPrefix(code[p])) {
    if (code[p] == 0x66) operand16 = true;
    if (code[p] == 0x67) address32 = true;
    ++p;
  }
  if (p >= available) return unknown;
  if ((code[p] & 0xF0) == 0x40) {
    rex_w = (code[p] & 0x08) != 0;
    if (++p >= available) return unknown;
  }
  OpcodeMap map = OpcodeMap::kOne;
  bool vector_encoding = false;
  if (code[p] == 0xC5) {
    if (p + 2 >= available) return unknown;
    p += 2;
    map = OpcodeMap::k0F;
    vector_encoding = true;
  } else if (code[p] == 0xC4) {
    if (p + 3 >= available) return unknown;
    switch (code[p + 1] & 0x1F) {
      case 1: map = OpcodeMap::k0F; break;
      case 2: map = OpcodeMap::k0F38; break;
      case 3: map = OpcodeMap::k0F3A; break;
      default: return unknown;
    }
    p += 3;
    vector_encoding = true;
  } else if (code[p] == 0x62) {
    if (p + 4 >= available) return unknown;
    switch (code[p + 1] & 0x07) {
      case 1: case 5: map = OpcodeMap::k0F; break;
      case 2: case 6: map = OpcodeMap::k0F38; break;
      case 3: map = OpcodeMap::k0F3A; break;
      default: return unknown;
    }
    p += 4;
    vector_encoding = true;
  } else if (code[p] == 0x0F) {
    if (++p >= available) return unknown;
    map = OpcodeMap::k0F;
    if (code[p] == 0x38) {
      if (++p >= available) return unknown;
      map = OpcodeMap::k0F38;
    } else if (code[p] == 0x3A) {
      if (++p >= available) return unknown;
      map = OpcodeMap::k0F3A;
    }
  }
  const std::uint8_t opcode = code[p++];

  bool modrm = false;
  std::size_t immediate = 0;
  const std::size_t immediate_z = operand16 ? 2 : 4;
  std::size_t group_immediate = 0;
  bool group = false;
  bool relative = false;
  X86Flow flow = X86Flow::kNone;

  if (map == OpcodeMap::kOne) {
    if (opcode < 0x40) {
      switch (opcode & 7) {
        case 0: case 1: case 2: case 3: modrm = true; break;
        case 4: immediate = 1; break;
        case 5: immediate = immediate_z; break;
        default: return unknown;
      }
    } else if (opcode >= 0x50 && opcode <= 0x5F) {
    } else if (opcode == 0x63) {
      modrm = true;
    } else if (opcode == 0x68) {
      immediate = immediate_z;
    } else if (opcode == 0x69) {
      modrm = true;
      immediate = immediate_z;
    } else if (opcode == 0x6A) {
      immediate = 1;
    } else if (opcode == 0x6B) {
      modrm = true;
      immediate = 1;
    } else if (opcode >= 0x6C && opcode <= 0x6F) {
    } else if (opcode >= 0x70 && opcode <= 0x7F) {
      immediate = 1;
      relative = true;
      flow = X86Flow::kConditional;
    } else if (opcode == 0x80 || opcode == 0x83) {
      modrm = true;
      immediate = 1;
    } else if (opcode == 0x81) {
      modrm = true;
      immediate = immediate_z;
    } else if (opcode >= 0x84 && opcode <= 0x8F) {
      modrm = true;
    } else if ((opcode >= 0x90 && opcode <= 0x99) || (opcode >= 0x9B && opcode <= 0x9F)) {
    } else if (opcode >= 0xA0 && opcode <= 0xA3) {
      immediate = address32 ? 4 : 8;
    } else if ((opcode >= 0xA4 && opcode <= 0xA7) || (opcode >= 0xAA && opcode <= 0xAF)) {
    } else if (opcode == 0xA8) {
      immediate = 1;
    } else if (opcode == 0xA9) {
      immediate = immediate_z;
    } else if (opcode >= 0xB0 && opcode <= 0xB7) {
      immediate = 1;
    } else if (opcode >= 0xB8 && opcode <= 0xBF) {
      immediate = rex_w ? 8 : immediate_z;
    } else if (opcode == 0xC0 || opcode == 0xC1 || opcode == 0xC6) {
      modrm = true;
      immediate = 1;
    } else if (opcode == 0xC2 || opcode == 0xCA) {
      immediate = 2;
      flow = X86Flow::kReturn;
    } else if (opcode == 0xC3 || opcode == 0xCB || opcode == 0xCF) {
      flow = X86Flow::kReturn;
    } else if (opcode == 0xC7) {
      modrm = true;
      immediate = immediate_z;
    } else if (opcode == 0xC8) {
      immediate = 3;
    } else if (opcode == 0xC9) {
    } else if (opcode == 0xCC) {
      flow = X86Flow::kHalt;
    } else if (opcode == 0xCD) {
      immediate = 1;
    } else if ((opcode >= 0xD0 && opcode <= 0xD3) || (opcode >= 0xD8 && opcode <= 0xDF)) {
      modrm = true;
    } else if (opcode == 0xD7) {
    } else if (opcode >= 0xE0 && opcode <= 0xE3) {
      immediate = 1;
      relative = true;
      flow = X86Flow::kConditional;
    } else if (opcode >= 0xE4 && opcode <= 0xE7) {
      immediate = 1;
    } else if (opcode == 0xE8) {
      immediate = 4;
      relative = true;
      flow = X86Flow::kCall;
    } else if (opcode == 0xE9) {
      immediate = 4;
      relative = true;
      flow = X86Flow::kJump;
    } else if (opcode == 0xEB) {
      immediate = 1;
      relative = true;
      flow = X86Flow::kJump;
    } else if (opcode >= 0xEC && opcode <= 0xEF) {
    } else if (opcode == 0xF1 || opcode == 0xF5 || (opcode >= 0xF8 && opcode <= 0xFD)) {
    } else if (opcode == 0xF4) {
      flow = X86Flow::kHalt;
    } else if (opcode == 0xF6) {
      modrm = true;
      group = true;
      group_immediate = 1;
    } else if (opcode == 0xF7) {
      modrm = true;
      group = true;
      group_immediate = immediate_z;
    } else if (opcode == 0xFE || opcode == 0xFF) {
      modrm = true;
    } else {
      return unknown;
    }
  } else if (map == OpcodeMap::k0F) {
    if (opcode <= 0x03 || opcode == 0x0D) {
      modrm = true;
    } else if (opcode == 0x05) {
      if (!vector_encoding) flow = X86Flow::kSyscall;
    } else if (opcode == 0x0B) {
      if (!vector_encoding) flow = X86Flow::kHalt;
    } else if (opcode == 0x06 || opcode == 0x07 || opcode == 0x08 || opcode == 0x09 || opcode == 0x0E) {
    } else if (opcode == 0x0F) {
      modrm = true;
      immediate = 1;
    } else if ((opcode >= 0x10 && opcode <= 0x23) || (opcode >= 0x28 && opcode <= 0x2F)) {
      modrm = true;
    } else if (opcode >= 0x30 && opcode <= 0x37) {
    } else if (opcode >= 0x40 && opcode <= 0x6F) {
      modrm = true;
    } else if (opcode >= 0x70 && opcode <= 0x73) {
      modrm = true;
      immediate = 1;
    } else if ((opcode >= 0x74 && opcode <= 0x76) || (opcode >= 0x78 && opcode <= 0x7F)) {
      modrm = true;
    } else if (opcode == 0x77) {
    } else if (opcode >= 0x80 && opcode <= 0x8F) {
      if (vector_encoding) return unknown;
      immediate = 4;
      relative = true;
      flow = X86Flow::kConditional;
    } else if (opcode >= 0x90 && opcode <= 0x9F) {
      modrm = true;
    } else if (opcode == 0xA0 || opcode == 0xA1 || opcode == 0xA2 || opcode == 0xA8 || opcode == 0xA9 ||
               opcode == 0xAA) {
    } else if (opcode == 0xA4 || opcode == 0xAC || opcode == 0xBA || opcode == 0xC2 || opcode == 0xC4 ||
               opcode == 0xC5 || opcode == 0xC6) {
      modrm = true;
      immediate = 1;
    } else if (opcode == 0xA3 || opcode == 0xA5 || opcode == 0xAB || opcode == 0xAD || opcode == 0xAE ||
               opcode == 0xAF || (opcode >= 0xB0 && opcode <= 0xB9) || (opcode >= 0xBB && opcode <= 0xC1) ||
               opcode == 0xC3 || opcode == 0xC7) {
      modrm = true;
      if (opcode == 0xB9 && !vector_encoding) flow = X86Flow::kHalt;
    } else if (opcode >= 0xC8 && opcode <= 0xCF) {
    } else if (opcode >= 0xD0) {
      modrm = true;
      if (opcode == 0xFF && !vector_encoding) flow = X86Flow::kHalt;
    } else {
      return unknown;
    }
  } else if (map == OpcodeMap::k0F38) {
    modrm = true;
  } else {
    modrm = true;
    immediate = 1;
  }

  if (modrm) {
    if (p >= available) return unknown;
    const std::uint8_t byte = code[p++];
    const unsigned mod = byte >> 6, reg = (byte >> 3) & 7, rm = byte & 7;
    if (group && reg <= 1) immediate = group_immediate;
    if (map == OpcodeMap::kOne && opcode == 0xFF) {
      if (reg == 2 || reg == 3) flow = X86Flow::kIndirectCall;
      if (reg == 4 || reg == 5) flow = X86Flow::kIndirectJump;
    }
    if (mod != 3) {
      if (rm == 4) {
        if (p >= available) return unknown;
        const std::uint8_t sib = code[p++];
        if (mod == 0 && (sib & 7) == 5) p += 4;
      }
      if (mod == 0 && rm == 5) {
        p += 4;
      } else if (mod == 1) {
        p += 1;
      } else if (mod == 2) {
        p += 4;
      }
    }
  }
  p += immediate;
  if (p > available) return unknown;

  X86Instruction instruction;
  instruction.length = static_cast<std::uint8_t>(p);
  instruction.flow = flow;
  if (relative) {
    if (immediate == 1) {
      instruction.displacement = static_cast<std::int8_t>(code[p - 1]);
    } else {
      std::memcpy(&instruction.displacement, code + p - 4, 4);
    }
  }
  return instruction;
}

std::ptrdiff_t RawSyscallFunction::IndexOf(std::uintptr_t address) const noexcept {
  if (address < start || address >= end) return -1;
  const auto offset = static_cast<std::uint32_t>(address - start);
  const auto found = std::lower_bound(offsets.begin(), offsets.end(), offset);
  if (found == offsets.end() || *found != offset) return -1;
  return found - offsets.begin();
}

RawSyscallAnalysis AnalyzeRawSyscallFunction(const std::uint8_t* code, std::uintptr_t start, std::size_t size,
                                             bool exception_table, const std::vector<RawSyscallLandingPad>* pads,
                                             RawSyscallFunction* out) {
  if (size == 0 || size > 0x7fffffffu) return RawSyscallAnalysis::kUndecodable;
  RawSyscallFunction f;
  f.start = start;
  f.end = start + size;
  constexpr std::int64_t kNoTarget = -1, kOutside = -2;
  std::vector<std::int64_t> target_offset;
  for (std::size_t at = 0; at < size;) {
    const X86Instruction instruction = DecodeX86(code + at, size - at);
    if (instruction.length == 0) return RawSyscallAnalysis::kUndecodable;
    f.offsets.push_back(static_cast<std::uint32_t>(at));
    f.lengths.push_back(instruction.length);
    f.flows.push_back(instruction.flow);
    std::int64_t target = kNoTarget;
    if (instruction.flow == X86Flow::kConditional || instruction.flow == X86Flow::kJump ||
        instruction.flow == X86Flow::kCall) {
      const std::int64_t to = static_cast<std::int64_t>(at) + instruction.length + instruction.displacement;
      target = to >= 0 && to < static_cast<std::int64_t>(size) ? to : kOutside;
    }
    target_offset.push_back(target);
    if (instruction.flow == X86Flow::kSyscall) ++f.syscalls;
    at += instruction.length;
  }
  if (f.syscalls == 0) return RawSyscallAnalysis::kNoSyscall;
  const std::size_t count = f.offsets.size();

  constexpr std::uint8_t kNoStop = 0xFF;
  std::vector<std::uint8_t> stop_bit(count, kNoStop);
  for (std::size_t i = 0; i < count; ++i) {
    if (f.flows[i] != X86Flow::kSyscall && f.flows[i] != X86Flow::kIndirectJump) continue;
    if (f.stops.size() == kMaxRawSyscallStops) return RawSyscallAnalysis::kTooManyStops;
    stop_bit[i] = static_cast<std::uint8_t>(f.stops.size());
    f.stops.push_back(static_cast<std::uint32_t>(i));
  }

  constexpr std::int32_t kNone = -1;
  std::vector<std::int32_t> target(count, kNone);
  std::vector<std::uint8_t> exits(count, 0);
  for (std::size_t i = 0; i < count; ++i) {
    if (target_offset[i] == kOutside) {
      if (f.flows[i] != X86Flow::kCall) exits[i] = 1;
    } else if (target_offset[i] >= 0) {
      const std::ptrdiff_t index = f.IndexOf(start + static_cast<std::uintptr_t>(target_offset[i]));
      if (index < 0) return RawSyscallAnalysis::kUndecodable;
      target[i] = static_cast<std::int32_t>(index);
    }
    if (f.flows[i] == X86Flow::kReturn) exits[i] = 1;
  }
  const auto falls_through = [&f](std::size_t i) {
    const X86Flow flow = f.flows[i];
    return flow == X86Flow::kNone || flow == X86Flow::kConditional || flow == X86Flow::kCall ||
           flow == X86Flow::kIndirectCall || flow == X86Flow::kSyscall;
  };
  const auto is_call = [&f](std::size_t i) {
    return f.flows[i] == X86Flow::kCall || f.flows[i] == X86Flow::kIndirectCall;
  };

  std::vector<std::vector<std::uint32_t>> pad_edges;
  if (exception_table) {
    pad_edges.resize(count);
    bool known = pads != nullptr;
    if (known) {
      for (std::size_t i = 0; i < count && known; ++i) {
        if (!is_call(i)) continue;
        const std::uint32_t last = f.offsets[i] + f.lengths[i] - 1;
        for (const RawSyscallLandingPad& pad : *pads) {
          if (last < pad.begin || last >= pad.end) continue;
          const std::ptrdiff_t index = f.IndexOf(start + pad.pad);
          if (index < 0) {
            known = false;
            break;
          }
          pad_edges[i].push_back(static_cast<std::uint32_t>(index));
        }
      }
    }
    if (!known) {
      std::vector<std::uint8_t> reached(count, 0);
      reached[0] = 1;
      for (std::size_t i = 0; i < count; ++i) {
        if (falls_through(i) && i + 1 < count) reached[i + 1] = 1;
        if (target[i] != kNone) reached[static_cast<std::size_t>(target[i])] = 1;
      }
      std::vector<std::uint32_t> unreached;
      for (std::size_t i = 0; i < count; ++i) {
        if (reached[i] == 0) unreached.push_back(static_cast<std::uint32_t>(i));
      }
      for (std::size_t i = 0; i < count; ++i) {
        if (is_call(i)) pad_edges[i] = unreached;
      }
    }
  }

  f.next.assign(count, 0);
  f.leaves.assign(count, 0);
  for (std::size_t bit = 0; bit < f.stops.size(); ++bit) f.next[f.stops[bit]] = std::uint64_t{1} << bit;
  for (bool changed = true; changed;) {
    changed = false;
    for (std::size_t i = count; i-- > 0;) {
      if (stop_bit[i] != kNoStop) continue;
      std::uint64_t mask = 0;
      bool leaves = exits[i] != 0;
      const auto follow = [&](std::size_t to) {
        mask |= f.next[to];
        leaves = leaves || f.leaves[to] != 0;
      };
      if (falls_through(i)) {
        if (i + 1 < count) {
          follow(i + 1);
        } else {
          leaves = true;
        }
      }
      if (target[i] != kNone) follow(static_cast<std::size_t>(target[i]));
      if (!pad_edges.empty()) {
        for (const std::uint32_t pad : pad_edges[i]) follow(pad);
      }
      if (mask != f.next[i] || (leaves ? 1 : 0) != f.leaves[i]) {
        f.next[i] = mask;
        f.leaves[i] = leaves ? 1 : 0;
        changed = true;
      }
    }
  }
  *out = std::move(f);
  return RawSyscallAnalysis::kOk;
}

namespace {

class Reader {
 public:
  explicit Reader(const RawSyscallImage& image) : image_(image) {}

  bool Readable(std::uintptr_t address, std::size_t size) const noexcept {
    for (const RawSyscallImage::Range& range : image_.loaded) {
      if (address >= range.start && address <= range.end && size <= range.end - address) return true;
    }
    return false;
  }

  template <typename T>
  bool Read(std::uintptr_t address, T* out) const noexcept {
    if (!Readable(address, sizeof(T))) return false;
    std::memcpy(out, reinterpret_cast<const void*>(address), sizeof(T));
    return true;
  }

  bool Uleb(std::uintptr_t* at, std::uint64_t* out) const noexcept {
    std::uint64_t value = 0;
    for (unsigned shift = 0; shift < 64; shift += 7) {
      std::uint8_t byte = 0;
      if (!Read((*at)++, &byte)) return false;
      value |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
      if ((byte & 0x80) == 0) {
        *out = value;
        return true;
      }
    }
    return false;
  }

  bool Sleb(std::uintptr_t* at, std::int64_t* out) const noexcept {
    std::uint64_t value = 0;
    for (unsigned shift = 0; shift < 64; shift += 7) {
      std::uint8_t byte = 0;
      if (!Read((*at)++, &byte)) return false;
      value |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
      if ((byte & 0x80) == 0) {
        if ((byte & 0x40) != 0 && shift + 7 < 64) value |= ~std::uint64_t{0} << (shift + 7);
        *out = static_cast<std::int64_t>(value);
        return true;
      }
    }
    return false;
  }

  bool Encoded(std::uint8_t encoding, std::uintptr_t* at, std::uintptr_t* out) const noexcept {
    if (encoding == 0xFF || (encoding & 0x80) != 0) return false;
    const std::uintptr_t field = *at;
    std::uint64_t value = 0;
    switch (encoding & 0x0F) {
      case 0x00:
      case 0x04: {
        if (!Read(field, &value)) return false;
        *at += 8;
        break;
      }
      case 0x01: {
        if (!Uleb(at, &value)) return false;
        break;
      }
      case 0x02: {
        std::uint16_t raw = 0;
        if (!Read(field, &raw)) return false;
        value = raw;
        *at += 2;
        break;
      }
      case 0x03: {
        std::uint32_t raw = 0;
        if (!Read(field, &raw)) return false;
        value = raw;
        *at += 4;
        break;
      }
      case 0x09: {
        std::int64_t raw = 0;
        if (!Sleb(at, &raw)) return false;
        value = static_cast<std::uint64_t>(raw);
        break;
      }
      case 0x0A: {
        std::int16_t raw = 0;
        if (!Read(field, &raw)) return false;
        value = static_cast<std::uint64_t>(static_cast<std::int64_t>(raw));
        *at += 2;
        break;
      }
      case 0x0B: {
        std::int32_t raw = 0;
        if (!Read(field, &raw)) return false;
        value = static_cast<std::uint64_t>(static_cast<std::int64_t>(raw));
        *at += 4;
        break;
      }
      case 0x0C: {
        if (!Read(field, &value)) return false;
        *at += 8;
        break;
      }
      default:
        return false;
    }
    switch (encoding & 0x70) {
      case 0x00: break;
      case 0x10:
        if (value != 0) value += field;
        break;
      default:
        return false;
    }
    *out = static_cast<std::uintptr_t>(value);
    return true;
  }

 private:
  const RawSyscallImage& image_;
};

struct Cie {
  bool valid = false;
  bool sized_augmentation = false;
  std::uint8_t fde_encoding = 0;
  std::uint8_t lsda_encoding = 0xFF;
};

Cie ParseCie(const Reader& reader, std::uintptr_t cie) noexcept {
  Cie out;
  std::uint32_t length = 0, id = 1;
  if (!reader.Read(cie, &length) || length == 0 || length == 0xFFFFFFFFu) return out;
  if (!reader.Read(cie + 4, &id) || id != 0) return out;
  std::uintptr_t at = cie + 8;
  std::uint8_t version = 0;
  if (!reader.Read(at++, &version) || (version != 1 && version != 3 && version != 4)) return out;
  char augmentation[16] = {};
  for (std::size_t i = 0;; ++i) {
    if (i + 1 >= sizeof(augmentation)) return out;
    if (!reader.Read(at++, &augmentation[i])) return out;
    if (augmentation[i] == '\0') break;
  }
  if (version == 4) at += 2;
  std::uint64_t code_alignment = 0, return_register = 0;
  std::int64_t data_alignment = 0;
  if (!reader.Uleb(&at, &code_alignment) || !reader.Sleb(&at, &data_alignment)) return out;
  if (version == 1) {
    ++at;
  } else if (!reader.Uleb(&at, &return_register)) {
    return out;
  }
  if (augmentation[0] == 'z') {
    out.sized_augmentation = true;
    std::uint64_t augmentation_length = 0;
    if (!reader.Uleb(&at, &augmentation_length)) return out;
    for (const char* letter = augmentation + 1; *letter != '\0'; ++letter) {
      if (*letter == 'R') {
        if (!reader.Read(at++, &out.fde_encoding)) return out;
      } else if (*letter == 'L') {
        if (!reader.Read(at++, &out.lsda_encoding)) return out;
      } else if (*letter == 'P') {
        std::uint8_t encoding = 0;
        if (!reader.Read(at++, &encoding)) return out;
        std::uintptr_t personality = 0;
        if (!reader.Encoded(encoding & 0x0F, &at, &personality)) return out;
      } else if (*letter == 'S' || *letter == 'B' || *letter == 'G') {
      } else {
        return out;
      }
    }
  } else if (augmentation[0] != '\0') {
    return out;
  }
  out.valid = true;
  return out;
}

struct Fde {
  std::uintptr_t begin = 0;
  std::uintptr_t end = 0;
  std::uintptr_t lsda = 0;
};

bool ParseFde(const Reader& reader, std::uintptr_t fde, std::map<std::uintptr_t, Cie>* cies, Fde* out) {
  std::uint32_t length = 0, delta = 0;
  if (!reader.Read(fde, &length) || length == 0 || length == 0xFFFFFFFFu) return false;
  if (!reader.Read(fde + 4, &delta) || delta == 0 || delta > fde + 4) return false;
  const std::uintptr_t cie_address = fde + 4 - delta;
  auto found = cies->find(cie_address);
  if (found == cies->end()) found = cies->emplace(cie_address, ParseCie(reader, cie_address)).first;
  const Cie& cie = found->second;
  if (!cie.valid) return false;
  std::uintptr_t at = fde + 8;
  std::uintptr_t range = 0;
  if (!reader.Encoded(cie.fde_encoding, &at, &out->begin)) return false;
  if (!reader.Encoded(cie.fde_encoding & 0x0F, &at, &range)) return false;
  out->end = out->begin + range;
  out->lsda = 0;
  if (cie.sized_augmentation) {
    std::uint64_t augmentation_length = 0;
    if (!reader.Uleb(&at, &augmentation_length)) return false;
    if (cie.lsda_encoding != 0xFF && augmentation_length != 0 && !reader.Encoded(cie.lsda_encoding, &at, &out->lsda)) {
      return false;
    }
  }
  return out->begin != 0 && out->end > out->begin;
}

bool ParseLandingPads(const Reader& reader, std::uintptr_t lsda, std::uintptr_t function, std::size_t size,
                      std::vector<RawSyscallLandingPad>* out) {
  std::uintptr_t at = lsda;
  std::uint8_t base_encoding = 0, type_encoding = 0, site_encoding = 0;
  if (!reader.Read(at++, &base_encoding)) return false;
  std::uintptr_t base = function;
  if (base_encoding != 0xFF && !reader.Encoded(base_encoding, &at, &base)) return false;
  if (!reader.Read(at++, &type_encoding)) return false;
  std::uint64_t ignored = 0, table_length = 0;
  if (type_encoding != 0xFF && !reader.Uleb(&at, &ignored)) return false;
  if (!reader.Read(at++, &site_encoding) || (site_encoding & 0x70) != 0) return false;
  if (!reader.Uleb(&at, &table_length) || !reader.Readable(at, table_length)) return false;
  const std::uintptr_t end = at + static_cast<std::uintptr_t>(table_length);
  while (at < end) {
    std::uintptr_t begin = 0, length = 0, pad = 0;
    if (!reader.Encoded(site_encoding, &at, &begin) || !reader.Encoded(site_encoding, &at, &length) ||
        !reader.Encoded(site_encoding, &at, &pad) || !reader.Uleb(&at, &ignored)) {
      return false;
    }
    if (pad == 0) continue;
    const std::uintptr_t address = base + pad;
    if (address < function || address >= function + size || begin > size || length > size - begin) return false;
    RawSyscallLandingPad range;
    range.begin = static_cast<std::uint32_t>(begin);
    range.end = static_cast<std::uint32_t>(begin + length);
    range.pad = static_cast<std::uint32_t>(address - function);
    out->push_back(range);
  }
  return true;
}

}

RawSyscallScan ScanRawSyscalls(const RawSyscallImage& image) {
  RawSyscallScan scan;
  const Reader reader(image);

  const std::uintptr_t header = image.eh_frame_hdr;
  std::uintptr_t table = 0;
  std::uint32_t entries = 0;
  std::uint8_t head[4] = {};
  if (header != 0 && reader.Readable(header, 4)) {
    std::memcpy(head, reinterpret_cast<const void*>(header), 4);
    std::uintptr_t at = header + 4, frames = 0, count = 0;
    if (head[0] == 1 && head[3] == 0x3B && reader.Encoded(head[1], &at, &frames) &&
        reader.Encoded(head[2], &at, &count) && count != 0 && count <= 0x7fffffffu &&
        reader.Readable(at, static_cast<std::size_t>(count) * 8)) {
      table = at;
      entries = static_cast<std::uint32_t>(count);
    }
  }
  scan.function_table = entries != 0;
  const auto start_of = [&](std::uint32_t entry) {
    std::int32_t relative = 0;
    std::memcpy(&relative, reinterpret_cast<const void*>(table + std::size_t{entry} * 8), 4);
    return header + static_cast<std::uintptr_t>(static_cast<std::intptr_t>(relative));
  };
  const auto fde_of = [&](std::uint32_t entry) {
    std::int32_t relative = 0;
    std::memcpy(&relative, reinterpret_cast<const void*>(table + std::size_t{entry} * 8 + 4), 4);
    return header + static_cast<std::uintptr_t>(static_cast<std::intptr_t>(relative));
  };

  std::map<std::uintptr_t, Cie> cies;
  struct Function {
    bool parsed = false;
    Fde fde;
  };
  std::map<std::uint32_t, Function> functions;
  const auto function_at = [&](std::uint32_t entry) -> const Function& {
    auto found = functions.find(entry);
    if (found == functions.end()) {
      Function function;
      function.parsed = ParseFde(reader, fde_of(entry), &cies, &function.fde);
      found = functions.emplace(entry, function).first;
    }
    return found->second;
  };
  std::uintptr_t span_begin = 0, span_end = 0;
  if (entries != 0) {
    span_begin = start_of(0);
    const Function& last = function_at(entries - 1);
    span_end = last.parsed ? last.fde.end : start_of(entries - 1);
  }

  std::vector<std::uint32_t> holders;
  for (const RawSyscallImage::Range& range : image.executable) {
    if (range.end <= range.start || !reader.Readable(range.start, range.end - range.start)) continue;
    const auto* const begin = reinterpret_cast<const std::uint8_t*>(range.start);
    const auto* const end = reinterpret_cast<const std::uint8_t*>(range.end);
    for (const std::uint8_t* p = begin; p + 1 < end; ++p) {
      p = static_cast<const std::uint8_t*>(std::memchr(p, 0x0F, static_cast<std::size_t>(end - 1 - p)));
      if (p == nullptr) break;
      if (p[1] != 0x05) continue;
      ++scan.byte_pairs;
      if (entries == 0) continue;
      const auto address = reinterpret_cast<std::uintptr_t>(p);
      std::uint32_t low = 0, high = entries;
      while (low < high) {
        const std::uint32_t middle = low + (high - low) / 2;
        if (start_of(middle) <= address) {
          low = middle + 1;
        } else {
          high = middle;
        }
      }
      bool covered = false;
      if (low != 0) {
        const Function& function = function_at(low - 1);
        covered = function.parsed && address >= function.fde.begin && address < function.fde.end;
        if (covered && (holders.empty() || holders.back() != low - 1)) holders.push_back(low - 1);
      }
      if (!covered && address >= span_begin && address < span_end) ++scan.outside_functions;
    }
  }
  std::sort(holders.begin(), holders.end());
  holders.erase(std::unique(holders.begin(), holders.end()), holders.end());

  for (const std::uint32_t entry : holders) {
    const Fde& fde = function_at(entry).fde;
    const std::size_t size = fde.end - fde.begin;
    bool executable = false;
    for (const RawSyscallImage::Range& range : image.executable) {
      if (fde.begin >= range.start && fde.end <= range.end) executable = true;
    }
    if (!executable) {
      ++scan.undecodable;
      continue;
    }
    std::vector<RawSyscallLandingPad> pads;
    const bool pads_known = fde.lsda != 0 && ParseLandingPads(reader, fde.lsda, fde.begin, size, &pads);
    RawSyscallFunction function;
    switch (AnalyzeRawSyscallFunction(reinterpret_cast<const std::uint8_t*>(fde.begin), fde.begin, size,
                                      fde.lsda != 0, pads_known ? &pads : nullptr, &function)) {
      case RawSyscallAnalysis::kOk:
        scan.syscalls += function.syscalls;
        scan.functions.push_back(std::move(function));
        break;
      case RawSyscallAnalysis::kNoSyscall:
        break;
      case RawSyscallAnalysis::kUndecodable:
        ++scan.undecodable;
        break;
      case RawSyscallAnalysis::kTooManyStops:
        ++scan.too_many_stops;
        break;
    }
  }
  std::stable_sort(scan.functions.begin(), scan.functions.end(),
                   [](const RawSyscallFunction& a, const RawSyscallFunction& b) { return a.syscalls > b.syscalls; });
  return scan;
}

}
