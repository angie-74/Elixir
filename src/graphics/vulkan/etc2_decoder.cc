// Copyright 2026 Mocktail Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from Mocktail's src/graphics/etc2_decoder.cc.
// Modified by Elixir.
#include "graphics/vulkan/etc2_decoder.h"

#include <emmintrin.h>
#include <intrin.h>
#include <tmmintrin.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>

#define ELIXIR_SSSE3 __attribute__((target("ssse3")))
#define ELIXIR_SSSE3_INLINE __attribute__((target("ssse3"), always_inline)) inline
#define ELIXIR_SSSE3_RARE __attribute__((target("ssse3"), noinline))

namespace elixir::vulkan {
namespace {

constexpr std::array<int, 8> kDistance = {3, 6, 11, 16, 23, 32, 41, 64};

constexpr int kEtc1Modifiers[8][4] = {
    {2, 8, -2, -8},       {5, 17, -5, -17},   {9, 29, -9, -29},
    {13, 42, -13, -42},   {18, 60, -18, -60}, {24, 80, -24, -80},
    {33, 106, -33, -106}, {47, 183, -47, -183},
};

constexpr int kEacModifiers[16][8] = {
    {-3, -6, -9, -15, 2, 5, 8, 14}, {-3, -7, -10, -13, 2, 6, 9, 12},
    {-2, -5, -8, -13, 1, 4, 7, 12}, {-2, -4, -6, -13, 1, 3, 5, 12},
    {-3, -6, -8, -12, 2, 5, 7, 11}, {-3, -7, -9, -11, 2, 6, 8, 10},
    {-4, -7, -8, -11, 3, 6, 7, 10}, {-3, -5, -8, -11, 2, 4, 7, 10},
    {-2, -6, -8, -10, 1, 5, 7, 9},  {-2, -5, -8, -10, 1, 4, 7, 9},
    {-2, -4, -8, -10, 1, 3, 7, 9},  {-2, -5, -7, -10, 1, 4, 6, 9},
    {-3, -4, -7, -10, 2, 3, 6, 9},  {-1, -2, -3, -10, 0, 1, 2, 9},
    {-4, -6, -8, -9, 3, 5, 7, 8},   {-3, -5, -7, -9, 2, 4, 6, 8},
};

constexpr int kDelta[8] = {0, 1, 2, 3, -4, -3, -2, -1};

std::uint8_t Clamp255(int value) { return static_cast<std::uint8_t>(std::clamp(value, 0, 255)); }

constexpr int Expand4(int value) { return (value << 4) | value; }
constexpr int Expand5(int value) { return (value << 3) | (value >> 2); }
constexpr int Expand6(int value) { return (value << 2) | (value >> 4); }
constexpr int Expand7(int value) { return (value << 1) | (value >> 6); }

void PaintColors(const std::uint8_t* b, bool t_mode, int paint[4][3]) {
  if (t_mode) {
    const int c1[3] = {Expand4((((b[0] >> 3) & 3) << 2) | (b[0] & 3)), Expand4(b[1] >> 4), Expand4(b[1] & 15)};
    const int c2[3] = {Expand4(b[2] >> 4), Expand4(b[2] & 15), Expand4(b[3] >> 4)};
    const int d = kDistance[(((b[3] >> 2) & 3) << 1) | (b[3] & 1)];
    for (int c = 0; c < 3; ++c) {
      paint[0][c] = c1[c];
      paint[1][c] = c2[c] + d;
      paint[2][c] = c2[c];
      paint[3][c] = c2[c] - d;
    }
    return;
  }
  const int c1[3] = {Expand4((b[0] >> 3) & 15), Expand4(((b[0] & 7) << 1) | ((b[1] >> 4) & 1)),
                     Expand4((b[1] & 8) | ((b[1] & 3) << 1) | (b[2] >> 7))};
  const int c2[3] = {Expand4((b[2] >> 3) & 15), Expand4(((b[2] & 7) << 1) | (b[3] >> 7)), Expand4((b[3] >> 3) & 15)};
  const int v1 = (c1[0] << 16) | (c1[1] << 8) | c1[2];
  const int v2 = (c2[0] << 16) | (c2[1] << 8) | c2[2];
  const int d = kDistance[(b[3] & 4) | ((b[3] & 1) << 1) | (v1 >= v2 ? 1 : 0)];
  for (int c = 0; c < 3; ++c) {
    paint[0][c] = c1[c] + d;
    paint[1][c] = c1[c] - d;
    paint[2][c] = c2[c] + d;
    paint[3][c] = c2[c] - d;
  }
}

struct PlanarColors {
  int origin[3];
  int horizontal[3];
  int vertical[3];
};

PlanarColors ReadPlanarColors(const std::uint8_t* b) {
  PlanarColors colors;
  colors.origin[0] = Expand6((b[0] >> 1) & 63);
  colors.origin[1] = Expand7(((b[0] & 1) << 6) | ((b[1] >> 1) & 63));
  colors.origin[2] = Expand6(((b[1] & 1) << 5) | (b[2] & 0x18) | ((b[2] & 3) << 1) | (b[3] >> 7));
  colors.horizontal[0] = Expand6(((b[3] & 0x7c) >> 1) | (b[3] & 1));
  colors.horizontal[1] = Expand7(b[4] >> 1);
  colors.horizontal[2] = Expand6(((b[4] & 1) << 5) | (b[5] >> 3));
  colors.vertical[0] = Expand6(((b[5] & 7) << 3) | (b[6] >> 5));
  colors.vertical[1] = Expand7(((b[6] & 31) << 2) | (b[7] >> 6));
  colors.vertical[2] = Expand6(b[7] & 63);
  return colors;
}

void SubblockBases(const std::uint8_t* b, bool individual, int base[2][3]) {
  for (int c = 0; c < 3; ++c) {
    if (individual) {
      base[0][c] = Expand4(b[c] >> 4);
      base[1][c] = Expand4(b[c] & 15);
    } else {
      base[0][c] = Expand5(b[c] >> 3);
      base[1][c] = Expand5((b[c] >> 3) + kDelta[b[c] & 7]);
    }
  }
}

void DecodeRgbBlock(const std::uint8_t* b, bool punchthrough, std::array<std::uint8_t, 64>* out) {
  const bool diff_or_opaque = (b[3] & 2) != 0;
  const bool opaque = !punchthrough || diff_or_opaque;
  const int msb = (b[4] << 8) | b[5];
  const int lsb = (b[6] << 8) | b[7];
  auto index_of = [msb, lsb](int texel) { return (((msb >> texel) & 1) << 1) | ((lsb >> texel) & 1); };
  auto write = [out](int texel, int r, int g, int bl, int a) {
    const std::size_t o = static_cast<std::size_t>(texel) * 4;
    (*out)[o] = Clamp255(r);
    (*out)[o + 1] = Clamp255(g);
    (*out)[o + 2] = Clamp255(bl);
    (*out)[o + 3] = static_cast<std::uint8_t>(a);
  };

  const int r_sum = (b[0] >> 3) + kDelta[b[0] & 7];
  const int g_sum = (b[1] >> 3) + kDelta[b[1] & 7];
  const int b_sum = (b[2] >> 3) + kDelta[b[2] & 7];
  const bool individual = !punchthrough && !diff_or_opaque;

  if (!individual && (r_sum < 0 || r_sum > 31 || g_sum < 0 || g_sum > 31)) {
    int paint[4][3];
    PaintColors(b, r_sum < 0 || r_sum > 31, paint);
    for (int texel = 0; texel < 16; ++texel) {
      const int index = index_of(texel);
      if (!opaque && index == 2) {
        write(texel, 0, 0, 0, 0);
      } else {
        write(texel, paint[index][0], paint[index][1], paint[index][2], 255);
      }
    }
    return;
  }

  if (!individual && (b_sum < 0 || b_sum > 31)) {
    const PlanarColors p = ReadPlanarColors(b);
    for (int x = 0; x < 4; ++x) {
      for (int y = 0; y < 4; ++y) {
        write(x * 4 + y, (x * (p.horizontal[0] - p.origin[0]) + y * (p.vertical[0] - p.origin[0]) + 4 * p.origin[0] + 2) >> 2,
              (x * (p.horizontal[1] - p.origin[1]) + y * (p.vertical[1] - p.origin[1]) + 4 * p.origin[1] + 2) >> 2,
              (x * (p.horizontal[2] - p.origin[2]) + y * (p.vertical[2] - p.origin[2]) + 4 * p.origin[2] + 2) >> 2, 255);
      }
    }
    return;
  }

  int base[2][3];
  SubblockBases(b, individual, base);
  const int tables[2] = {b[3] >> 5, (b[3] >> 2) & 7};
  const bool flipped = (b[3] & 1) != 0;
  for (int x = 0; x < 4; ++x) {
    for (int y = 0; y < 4; ++y) {
      const int texel = x * 4 + y;
      const int index = index_of(texel);
      if (!opaque && index == 2) {
        write(texel, 0, 0, 0, 0);
        continue;
      }
      const int sub = flipped ? (y >= 2 ? 1 : 0) : (x >= 2 ? 1 : 0);
      const int modifier = (!opaque && index == 0) ? 0 : kEtc1Modifiers[tables[sub]][index];
      write(texel, base[sub][0] + modifier, base[sub][1] + modifier, base[sub][2] + modifier, 255);
    }
  }
}

int EacIndex(const std::uint8_t* b, int texel) {
  std::uint64_t bits = 0;
  for (int i = 2; i < 8; ++i) {
    bits = (bits << 8) | b[i];
  }
  return static_cast<int>((bits >> (45 - texel * 3)) & 7);
}

int EacModifier(const std::uint8_t* b, int texel) { return kEacModifiers[b[1] & 15][EacIndex(b, texel)]; }

std::uint16_t DecodeR11(const std::uint8_t* b, int texel, bool is_signed) {
  const int multiplier = b[1] >> 4;
  const int modifier = EacModifier(b, texel);
  const int scaled = multiplier != 0 ? modifier * multiplier * 8 : modifier;
  if (!is_signed) {
    const int value = std::clamp(b[0] * 8 + 4 + scaled, 0, 2047);
    return static_cast<std::uint16_t>((value << 5) | (value >> 6));
  }
  const int base = std::max(-127, static_cast<int>(static_cast<std::int8_t>(b[0])));
  const int value = std::clamp(base * 8 + scaled, -1023, 1023);
  const int magnitude = value < 0 ? -value : value;
  const int expanded = (magnitude << 5) | (magnitude >> 5);
  return static_cast<std::uint16_t>(static_cast<std::int16_t>(value < 0 ? -expanded : expanded));
}

void DecodeBlockPortable(EtcFormat format, const std::uint8_t* block, std::uint64_t bx, std::uint64_t by,
                         std::uint32_t width, std::uint32_t height, std::uint8_t* destination,
                         std::size_t texel_bytes) {
  std::array<std::uint8_t, 64> rgba{};
  if (format == EtcFormat::kEtc2Rgb8 || format == EtcFormat::kEtc2Rgb8A1) {
    DecodeRgbBlock(block, format == EtcFormat::kEtc2Rgb8A1, &rgba);
  } else if (format == EtcFormat::kEtc2Rgba8) {
    DecodeRgbBlock(block + 8, false, &rgba);
  }
  for (std::uint32_t y = 0; y < 4 && by * 4 + y < height; ++y) {
    for (std::uint32_t x = 0; x < 4 && bx * 4 + x < width; ++x) {
      const int texel = static_cast<int>(x * 4 + y);
      std::uint8_t* out = destination + ((by * 4 + y) * width + bx * 4 + x) * texel_bytes;
      switch (format) {
        case EtcFormat::kEtc2Rgb8:
        case EtcFormat::kEtc2Rgb8A1:
        case EtcFormat::kEtc2Rgba8:
          std::copy_n(rgba.data() + texel * 4, 4, out);
          if (format == EtcFormat::kEtc2Rgba8) {
            const int alpha = block[0] + EacModifier(block, texel) * (block[1] >> 4);
            out[3] = Clamp255(alpha);
          }
          break;
        case EtcFormat::kEacR11:
        case EtcFormat::kEacR11Signed:
        case EtcFormat::kEacRg11:
        case EtcFormat::kEacRg11Signed: {
          const bool is_signed = format == EtcFormat::kEacR11Signed || format == EtcFormat::kEacRg11Signed;
          const int channels = static_cast<int>(texel_bytes / 2);
          for (int channel = 0; channel < channels; ++channel) {
            const std::uint16_t value = DecodeR11(block + channel * 8, texel, is_signed);
            out[channel * 2] = static_cast<std::uint8_t>(value);
            out[channel * 2 + 1] = static_cast<std::uint8_t>(value >> 8);
          }
          break;
        }
      }
    }
  }
}

struct Etc1Table16 {
  alignas(16) std::int16_t rows[8][4];
};

constexpr Etc1Table16 MakeEtc1Table16() {
  Etc1Table16 table{};
  for (int row = 0; row < 8; ++row) {
    for (int column = 0; column < 4; ++column) {
      table.rows[row][column] = static_cast<std::int16_t>(kEtc1Modifiers[row][column]);
    }
  }
  return table;
}

constexpr Etc1Table16 kEtc1Table16 = MakeEtc1Table16();

struct EacTable16 {
  alignas(16) std::int16_t rows[16][8];
};

constexpr EacTable16 MakeEacTable16() {
  EacTable16 table{};
  for (int row = 0; row < 16; ++row) {
    for (int column = 0; column < 8; ++column) {
      table.rows[row][column] = static_cast<std::int16_t>(kEacModifiers[row][column]);
    }
  }
  return table;
}

constexpr EacTable16 kEacTable16 = MakeEacTable16();

struct BaseTables {
  std::uint16_t pairs[2][256];
  std::uint8_t overflow[256];
};

constexpr BaseTables MakeBaseTables() {
  BaseTables tables{};
  for (int value = 0; value < 256; ++value) {
    const int sum = (value >> 3) + kDelta[value & 7];
    tables.overflow[value] = static_cast<std::uint8_t>(sum < 0 || sum > 31 ? 1 : 0);
    tables.pairs[0][value] = static_cast<std::uint16_t>(Expand5(value >> 3) | (Expand5(sum & 31) << 8));
    tables.pairs[1][value] = static_cast<std::uint16_t>(Expand4(value >> 4) | (Expand4(value & 15) << 8));
  }
  return tables;
}

constexpr BaseTables kBaseTables = MakeBaseTables();

alignas(16) constexpr std::int8_t kSubblockSplit[2][16] = {
    {0, 0, 4, 4, 0, 0, 4, 4, 0, 0, 4, 4, 0, 0, 4, 4},
    {0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4},
};

alignas(16) constexpr std::int16_t kModifierMask[2][8] = {
    {0, -1, -1, -1, 0, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1},
};

alignas(16) constexpr std::int8_t kKeptEntries[2][16] = {
    {-1, -1, 0, -1, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0},
    {-1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0},
};

bool DetectSsse3() {
  int info[4] = {};
  __cpuid(info, 1);
  return (info[2] & (1 << 9)) != 0;
}

bool HasSsse3() {
  static const bool present = DetectSsse3();
  return present;
}

ELIXIR_SSSE3_INLINE __m128i ColorSelectors(const std::uint8_t* block) {
  std::uint32_t planes;
  std::memcpy(&planes, block + 4, sizeof(planes));
  const __m128i word = _mm_cvtsi32_si128(static_cast<int>(planes));
  const __m128i bit = _mm_setr_epi8(1, 16, 1, 16, 2, 32, 2, 32, 4, 64, 4, 64, 8, -128, 8, -128);
  const __m128i low = _mm_shuffle_epi8(word, _mm_setr_epi8(3, 3, 2, 2, 3, 3, 2, 2, 3, 3, 2, 2, 3, 3, 2, 2));
  const __m128i high = _mm_shuffle_epi8(word, _mm_setr_epi8(1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0));
  const __m128i low_set = _mm_cmpeq_epi8(_mm_and_si128(low, bit), bit);
  const __m128i high_set = _mm_cmpeq_epi8(_mm_and_si128(high, bit), bit);
  return _mm_or_si128(_mm_and_si128(low_set, _mm_set1_epi8(1)), _mm_and_si128(high_set, _mm_set1_epi8(2)));
}

ELIXIR_SSSE3_INLINE __m128i EacSelectors(const std::uint8_t* block) {
  const __m128i bytes = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(block));
  const __m128i upper = _mm_shuffle_epi8(bytes, _mm_setr_epi8(3, 2, 4, 3, 6, 5, 7, 6, 3, 2, 4, 3, 6, 5, 7, 6));
  const __m128i lower = _mm_shuffle_epi8(bytes, _mm_setr_epi8(3, 2, 5, 4, 6, 5, -128, 7, 4, 3, 5, 4, 7, 6, -128, 7));
  return _mm_packus_epi16(
      _mm_srli_epi16(_mm_mullo_epi16(upper, _mm_setr_epi16(1, 16, 1, 16, 8, 128, 8, 128)), 13),
      _mm_srli_epi16(_mm_mullo_epi16(lower, _mm_setr_epi16(64, 4, 64, 4, 2, 32, 2, 32)), 13));
}

ELIXIR_SSSE3_INLINE __m128i EacModifiers16(const std::uint8_t* block) {
  return _mm_load_si128(reinterpret_cast<const __m128i*>(kEacTable16.rows[block[1] & 15]));
}

ELIXIR_SSSE3_INLINE __m128i AlphaValues(const std::uint8_t* block) {
  const __m128i values =
      _mm_add_epi16(_mm_set1_epi16(static_cast<short>(block[0])),
                    _mm_mullo_epi16(EacModifiers16(block), _mm_set1_epi16(static_cast<short>(block[1] >> 4))));
  return _mm_shuffle_epi8(_mm_packus_epi16(values, values), EacSelectors(block));
}

ELIXIR_SSSE3_INLINE __m128i ElevenBitTable(const std::uint8_t* block, bool is_signed) {
  const int multiplier = block[1] >> 4;
  const __m128i scaled = _mm_mullo_epi16(
      EacModifiers16(block), _mm_set1_epi16(static_cast<short>(multiplier != 0 ? multiplier * 8 : 1)));
  if (!is_signed) {
    __m128i value = _mm_add_epi16(_mm_set1_epi16(static_cast<short>(block[0] * 8 + 4)), scaled);
    value = _mm_max_epi16(_mm_min_epi16(value, _mm_set1_epi16(2047)), _mm_setzero_si128());
    return _mm_or_si128(_mm_slli_epi16(value, 5), _mm_srli_epi16(value, 6));
  }
  const int base = std::max(-127, static_cast<int>(static_cast<std::int8_t>(block[0])));
  __m128i value = _mm_add_epi16(_mm_set1_epi16(static_cast<short>(base * 8)), scaled);
  value = _mm_max_epi16(_mm_min_epi16(value, _mm_set1_epi16(1023)), _mm_set1_epi16(-1023));
  const __m128i magnitude = _mm_abs_epi16(value);
  const __m128i expanded = _mm_or_si128(_mm_slli_epi16(magnitude, 5), _mm_srli_epi16(magnitude, 5));
  return _mm_sign_epi16(expanded, value);
}

struct ElevenBitRows {
  __m128i upper;
  __m128i lower;
};

ELIXIR_SSSE3_INLINE ElevenBitRows DecodeElevenBitBlock(const std::uint8_t* block, bool is_signed) {
  const __m128i table = ElevenBitTable(block, is_signed);
  const __m128i selectors = EacSelectors(block);
  const __m128i zero = _mm_setzero_si128();
  const __m128i low = _mm_shuffle_epi8(_mm_packus_epi16(_mm_and_si128(table, _mm_set1_epi16(255)), zero), selectors);
  const __m128i high = _mm_shuffle_epi8(_mm_packus_epi16(_mm_srli_epi16(table, 8), zero), selectors);
  return {_mm_unpacklo_epi8(low, high), _mm_unpackhi_epi8(low, high)};
}

ELIXIR_SSSE3_INLINE void StoreRow(std::uint8_t* out, __m128i row) {
  _mm_storeu_si128(reinterpret_cast<__m128i*>(out), row);
}

ELIXIR_SSSE3_INLINE void StoreHalfRow(std::uint8_t* out, __m128i row) {
  _mm_storel_epi64(reinterpret_cast<__m128i*>(out), row);
}

ELIXIR_SSSE3_INLINE void StoreColorRows(__m128i r, __m128i g, __m128i b, __m128i a, std::uint8_t* out,
                                       std::size_t stride) {
  const __m128i rg_upper = _mm_unpacklo_epi8(r, g);
  const __m128i rg_lower = _mm_unpackhi_epi8(r, g);
  const __m128i ba_upper = _mm_unpacklo_epi8(b, a);
  const __m128i ba_lower = _mm_unpackhi_epi8(b, a);
  StoreRow(out, _mm_unpacklo_epi16(rg_upper, ba_upper));
  StoreRow(out + stride, _mm_unpackhi_epi16(rg_upper, ba_upper));
  StoreRow(out + stride * 2, _mm_unpacklo_epi16(rg_lower, ba_lower));
  StoreRow(out + stride * 3, _mm_unpackhi_epi16(rg_lower, ba_lower));
}

ELIXIR_SSSE3_RARE void StorePlanarRows(const std::uint8_t* b, const __m128i* alpha, std::uint8_t* out,
                                      std::size_t stride) {
  const PlanarColors p = ReadPlanarColors(b);
  short start[3];
  short across[3];
  short down[3];
  for (int c = 0; c < 3; ++c) {
    start[c] = static_cast<short>(4 * p.origin[c] + 2);
    across[c] = static_cast<short>(p.horizontal[c] - p.origin[c]);
    down[c] = static_cast<short>(p.vertical[c] - p.origin[c]);
  }
  const short opaque = alpha == nullptr ? 1020 : 0;
  __m128i left = _mm_setr_epi16(start[0], start[1], start[2], opaque, static_cast<short>(start[0] + across[0]),
                                static_cast<short>(start[1] + across[1]), static_cast<short>(start[2] + across[2]),
                                opaque);
  const __m128i two_across =
      _mm_setr_epi16(static_cast<short>(2 * across[0]), static_cast<short>(2 * across[1]),
                     static_cast<short>(2 * across[2]), 0, static_cast<short>(2 * across[0]),
                     static_cast<short>(2 * across[1]), static_cast<short>(2 * across[2]), 0);
  const __m128i one_down = _mm_setr_epi16(down[0], down[1], down[2], 0, down[0], down[1], down[2], 0);
  const __m128i spread = _mm_setr_epi8(-128, -128, -128, 0, -128, -128, -128, 1, -128, -128, -128, 2, -128, -128, -128, 3);
  for (int y = 0; y < 4; ++y) {
    __m128i row = _mm_packus_epi16(_mm_srai_epi16(left, 2), _mm_srai_epi16(_mm_add_epi16(left, two_across), 2));
    if (alpha != nullptr) {
      row = _mm_or_si128(row, _mm_shuffle_epi8(*alpha, _mm_add_epi8(spread, _mm_set1_epi8(static_cast<char>(4 * y)))));
    }
    StoreRow(out + stride * static_cast<std::size_t>(y), row);
    left = _mm_add_epi16(left, one_down);
  }
}

ELIXIR_SSSE3_INLINE __m128i FourBytes(int a, int b, int c, int d) {
  return _mm_cvtsi32_si128(static_cast<int>(static_cast<std::uint32_t>(Clamp255(a)) |
                                            (static_cast<std::uint32_t>(Clamp255(b)) << 8) |
                                            (static_cast<std::uint32_t>(Clamp255(c)) << 16) |
                                            (static_cast<std::uint32_t>(Clamp255(d)) << 24)));
}

ELIXIR_SSSE3_RARE void PaintTables(const std::uint8_t* b, bool t_mode, __m128i* reds, __m128i* greens,
                                  __m128i* blues) {
  int paint[4][3];
  PaintColors(b, t_mode, paint);
  *reds = FourBytes(paint[0][0], paint[1][0], paint[2][0], paint[3][0]);
  *greens = FourBytes(paint[0][1], paint[1][1], paint[2][1], paint[3][1]);
  *blues = FourBytes(paint[0][2], paint[1][2], paint[2][2], paint[3][2]);
}

ELIXIR_SSSE3_INLINE void DecodeColorBlockSsse3(const std::uint8_t* b, bool punchthrough, const __m128i* alpha,
                                              std::uint8_t* out, std::size_t stride) {
  const int opaque = (!punchthrough || (b[3] & 2) != 0) ? 1 : 0;
  const int individual = (!punchthrough && (b[3] & 2) == 0) ? 1 : 0;
  const unsigned special =
      individual != 0 ? 0U
                      : static_cast<unsigned>(kBaseTables.overflow[b[0]] | (kBaseTables.overflow[b[1]] << 1) |
                                              (kBaseTables.overflow[b[2]] << 2));
  __m128i reds;
  __m128i greens;
  __m128i blues;
  __m128i split = _mm_setzero_si128();
  if (special != 0) {
    if ((special & 3) == 0) {
      StorePlanarRows(b, alpha, out, stride);
      return;
    }
    PaintTables(b, (special & 1) != 0, &reds, &greens, &blues);
  } else {
    const std::uint64_t pairs = static_cast<std::uint64_t>(kBaseTables.pairs[individual][b[0]]) |
                                (static_cast<std::uint64_t>(kBaseTables.pairs[individual][b[1]]) << 16) |
                                (static_cast<std::uint64_t>(kBaseTables.pairs[individual][b[2]]) << 32);
    const __m128i bases = _mm_cvtsi64_si128(static_cast<long long>(pairs));
    const __m128i modifiers = _mm_and_si128(
        _mm_unpacklo_epi64(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(kEtc1Table16.rows[b[3] >> 5])),
                           _mm_loadl_epi64(reinterpret_cast<const __m128i*>(kEtc1Table16.rows[(b[3] >> 2) & 7]))),
        _mm_load_si128(reinterpret_cast<const __m128i*>(kModifierMask[opaque])));
    const __m128i zero = _mm_setzero_si128();
    reds = _mm_packus_epi16(
        _mm_add_epi16(_mm_shuffle_epi8(bases, _mm_setr_epi8(0, -128, 0, -128, 0, -128, 0, -128, 1, -128, 1, -128, 1,
                                                            -128, 1, -128)),
                      modifiers),
        zero);
    greens = _mm_packus_epi16(
        _mm_add_epi16(_mm_shuffle_epi8(bases, _mm_setr_epi8(2, -128, 2, -128, 2, -128, 2, -128, 3, -128, 3, -128, 3,
                                                            -128, 3, -128)),
                      modifiers),
        zero);
    blues = _mm_packus_epi16(
        _mm_add_epi16(_mm_shuffle_epi8(bases, _mm_setr_epi8(4, -128, 4, -128, 4, -128, 4, -128, 5, -128, 5, -128, 5,
                                                            -128, 5, -128)),
                      modifiers),
        zero);
    split = _mm_load_si128(reinterpret_cast<const __m128i*>(kSubblockSplit[b[3] & 1]));
  }
  const __m128i selectors = _mm_or_si128(ColorSelectors(b), split);
  const __m128i kept = _mm_load_si128(reinterpret_cast<const __m128i*>(kKeptEntries[opaque]));
  const __m128i alphas = alpha != nullptr ? *alpha : _mm_shuffle_epi8(kept, selectors);
  StoreColorRows(_mm_shuffle_epi8(_mm_and_si128(reds, kept), selectors),
                 _mm_shuffle_epi8(_mm_and_si128(greens, kept), selectors),
                 _mm_shuffle_epi8(_mm_and_si128(blues, kept), selectors), alphas, out, stride);
}

ELIXIR_SSSE3 void DecodeFullBlocksSsse3(EtcFormat format, const std::uint8_t* blocks, std::uint64_t count,
                                       std::uint8_t* out, std::size_t stride) {
  switch (format) {
    case EtcFormat::kEtc2Rgb8:
      for (std::uint64_t index = 0; index < count; ++index) {
        DecodeColorBlockSsse3(blocks + index * 8, false, nullptr, out + index * 16, stride);
      }
      break;
    case EtcFormat::kEtc2Rgb8A1:
      for (std::uint64_t index = 0; index < count; ++index) {
        DecodeColorBlockSsse3(blocks + index * 8, true, nullptr, out + index * 16, stride);
      }
      break;
    case EtcFormat::kEtc2Rgba8:
      for (std::uint64_t index = 0; index < count; ++index) {
        const std::uint8_t* block = blocks + index * 16;
        const __m128i alpha = AlphaValues(block);
        DecodeColorBlockSsse3(block + 8, false, &alpha, out + index * 16, stride);
      }
      break;
    case EtcFormat::kEacR11:
    case EtcFormat::kEacR11Signed: {
      const bool is_signed = format == EtcFormat::kEacR11Signed;
      for (std::uint64_t index = 0; index < count; ++index) {
        const ElevenBitRows red = DecodeElevenBitBlock(blocks + index * 8, is_signed);
        std::uint8_t* block_out = out + index * 8;
        StoreHalfRow(block_out, red.upper);
        StoreHalfRow(block_out + stride, _mm_srli_si128(red.upper, 8));
        StoreHalfRow(block_out + stride * 2, red.lower);
        StoreHalfRow(block_out + stride * 3, _mm_srli_si128(red.lower, 8));
      }
      break;
    }
    case EtcFormat::kEacRg11:
    case EtcFormat::kEacRg11Signed: {
      const bool is_signed = format == EtcFormat::kEacRg11Signed;
      for (std::uint64_t index = 0; index < count; ++index) {
        const std::uint8_t* block = blocks + index * 16;
        const ElevenBitRows red = DecodeElevenBitBlock(block, is_signed);
        const ElevenBitRows green = DecodeElevenBitBlock(block + 8, is_signed);
        std::uint8_t* block_out = out + index * 16;
        StoreRow(block_out, _mm_unpacklo_epi16(red.upper, green.upper));
        StoreRow(block_out + stride, _mm_unpackhi_epi16(red.upper, green.upper));
        StoreRow(block_out + stride * 2, _mm_unpacklo_epi16(red.lower, green.lower));
        StoreRow(block_out + stride * 3, _mm_unpackhi_epi16(red.lower, green.lower));
      }
      break;
    }
  }
}

}

std::size_t EtcBlockBytes(EtcFormat format) {
  switch (format) {
    case EtcFormat::kEtc2Rgba8:
    case EtcFormat::kEacRg11:
    case EtcFormat::kEacRg11Signed:
      return 16;
    default:
      return 8;
  }
}

std::size_t EtcDecodedTexelBytes(EtcFormat format) {
  switch (format) {
    case EtcFormat::kEacR11:
    case EtcFormat::kEacR11Signed:
      return 2;
    default:
      return 4;
  }
}

namespace {

constexpr std::uint64_t kBlocksPerBand = 8192;
constexpr unsigned kHelperThreads = kEtcMaximumDecodeThreads - 1;

bool ImageFits(EtcFormat format, const std::uint8_t* source, std::size_t source_bytes, std::uint32_t width,
               std::uint32_t height, const std::uint8_t* destination, std::size_t destination_bytes) {
  if (source == nullptr || destination == nullptr || width == 0 || height == 0) {
    return false;
  }
  const std::uint64_t blocks_wide = (static_cast<std::uint64_t>(width) + 3) / 4;
  const std::uint64_t blocks_high = (static_cast<std::uint64_t>(height) + 3) / 4;
  return blocks_wide * blocks_high * EtcBlockBytes(format) <= source_bytes &&
         static_cast<std::uint64_t>(width) * height * EtcDecodedTexelBytes(format) <= destination_bytes;
}

bool DecodeRows(EtcFormat format, const std::uint8_t* source, std::size_t source_bytes, std::uint32_t width,
                std::uint32_t height, std::uint32_t first_block_row, std::uint32_t block_row_count,
                std::uint8_t* destination, std::size_t destination_bytes, bool fast) {
  if (!ImageFits(format, source, source_bytes, width, height, destination, destination_bytes)) {
    return false;
  }
  const std::uint64_t blocks_wide = (static_cast<std::uint64_t>(width) + 3) / 4;
  const std::uint64_t blocks_high = (static_cast<std::uint64_t>(height) + 3) / 4;
  if (static_cast<std::uint64_t>(first_block_row) + block_row_count > blocks_high) {
    return false;
  }
  const std::size_t texel_bytes = EtcDecodedTexelBytes(format);
  const std::size_t block_bytes = EtcBlockBytes(format);
  const std::size_t stride = static_cast<std::size_t>(width) * texel_bytes;
  const std::uint64_t whole_columns = fast ? width / 4 : 0;

  const std::uint64_t end_block_row = static_cast<std::uint64_t>(first_block_row) + block_row_count;
  for (std::uint64_t by = first_block_row; by < end_block_row; ++by) {
    const std::uint8_t* row = source + by * blocks_wide * block_bytes;
    std::uint64_t bx = 0;
    if (whole_columns != 0 && by * 4 + 4 <= height) {
      DecodeFullBlocksSsse3(format, row, whole_columns, destination + by * 4 * stride, stride);
      bx = whole_columns;
    }
    for (; bx < blocks_wide; ++bx) {
      DecodeBlockPortable(format, row + bx * block_bytes, bx, by, width, height, destination, texel_bytes);
    }
  }
  return true;
}

}

bool DecodeEtcImage(EtcFormat format, const std::uint8_t* source, std::size_t source_bytes, std::uint32_t width,
                    std::uint32_t height, std::uint8_t* destination, std::size_t destination_bytes) {
  return DecodeEtcImageBlockRows(format, source, source_bytes, width, height, 0, (height + 3) / 4, destination,
                                 destination_bytes);
}

namespace {

struct DecodeBand {
  EtcDecodeJob* job;
  std::uint32_t first_block_row;
  std::uint32_t block_row_count;
};

struct DecodeBandQueue {
  std::vector<DecodeBand> bands;
  std::atomic<std::size_t> next{0};
  std::mutex mutex;
  std::condition_variable completed;
  unsigned workers = 0;

  void Run() {
    for (;;) {
      const std::size_t index = next.fetch_add(1, std::memory_order_relaxed);
      if (index >= bands.size()) {
        return;
      }
      const DecodeBand& band = bands[index];
      const EtcDecodeJob& job = *band.job;
      DecodeEtcImageBlockRows(job.format, job.source, job.source_bytes, job.width, job.height,
                              band.first_block_row, band.block_row_count, job.destination,
                              job.destination_bytes);
    }
  }
};

class DecodeWorkerPool {
 public:
  DecodeWorkerPool() {
    const unsigned hardware = std::max(1U, std::thread::hardware_concurrency());
    const unsigned limit = std::min(kHelperThreads, hardware - 1);
    while (worker_count_ < limit) {
      try {
        workers_[worker_count_] = std::thread(&DecodeWorkerPool::RunWorker, this);
      } catch (const std::system_error&) {
        break;
      }
      ++worker_count_;
    }
  }

  ~DecodeWorkerPool() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopping_ = true;
    }
    ready_.notify_all();
    for (unsigned index = 0; index < worker_count_; ++index) {
      workers_[index].join();
    }
  }

  void Run(DecodeBandQueue* queue, unsigned workers) {
    const unsigned helpers = std::min(workers, worker_count_);
    if (helpers == 0) {
      queue->Run();
      return;
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue->workers = helpers;
      for (unsigned index = 0; index < helpers; ++index) {
        pending_.push_back(queue);
      }
    }
    for (unsigned index = 0; index < helpers; ++index) {
      ready_.notify_one();
    }
    queue->Run();
    std::unique_lock<std::mutex> lock(queue->mutex);
    queue->completed.wait(lock, [queue] { return queue->workers == 0; });
  }

 private:
  void RunWorker() {
    for (;;) {
      DecodeBandQueue* queue;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        ready_.wait(lock, [this] { return stopping_ || !pending_.empty(); });
        if (pending_.empty()) {
          return;
        }
        queue = pending_.front();
        pending_.pop_front();
      }
      queue->Run();
      std::lock_guard<std::mutex> lock(queue->mutex);
      if (--queue->workers == 0) {
        queue->completed.notify_one();
      }
    }
  }

  std::mutex mutex_;
  std::condition_variable ready_;
  std::deque<DecodeBandQueue*> pending_;
  std::array<std::thread, kHelperThreads> workers_{};
  unsigned worker_count_ = 0;
  bool stopping_ = false;
};

DecodeWorkerPool& Pool() {
  static DecodeWorkerPool* pool = new DecodeWorkerPool;
  return *pool;
}

}

unsigned DecodeWorkerCount() {
  static const unsigned count =
      std::min(kEtcMaximumDecodeThreads, std::max(1U, std::thread::hardware_concurrency() / 2));
  return count;
}

void DecodeEtcJobs(EtcDecodeJob* jobs, std::size_t count, unsigned worker_count) {
  if (jobs == nullptr) {
    return;
  }
  DecodeBandQueue queue;
  std::vector<DecodeBand>& bands = queue.bands;
  std::uint64_t total_blocks = 0;
  for (std::size_t index = 0; index < count; ++index) {
    EtcDecodeJob& job = jobs[index];
    job.ok = ImageFits(job.format, job.source, job.source_bytes, job.width, job.height, job.destination,
                       job.destination_bytes);
    if (!job.ok) {
      continue;
    }
    const auto blocks_wide = static_cast<std::uint32_t>((static_cast<std::uint64_t>(job.width) + 3) / 4);
    const auto blocks_high = static_cast<std::uint32_t>((static_cast<std::uint64_t>(job.height) + 3) / 4);
    const auto rows_per_band =
        static_cast<std::uint32_t>(std::max<std::uint64_t>(1, kBlocksPerBand / blocks_wide));
    for (std::uint32_t row = 0; row < blocks_high; row += rows_per_band) {
      bands.push_back({&job, row, std::min(rows_per_band, blocks_high - row)});
    }
    total_blocks += static_cast<std::uint64_t>(blocks_wide) * blocks_high;
  }

  const std::size_t threads = total_blocks < kEtcParallelMinimumBlocks
                                  ? 1
                                  : std::min<std::size_t>(std::max(worker_count, 1U), bands.size());
  if (threads <= 1) {
    queue.Run();
  } else {
    Pool().Run(&queue, static_cast<unsigned>(threads - 1));
  }
}

bool DecodeEtcImageBlockRows(EtcFormat format, const std::uint8_t* source, std::size_t source_bytes,
                             std::uint32_t width, std::uint32_t height, std::uint32_t first_block_row,
                             std::uint32_t block_row_count, std::uint8_t* destination,
                             std::size_t destination_bytes) {
  return DecodeRows(format, source, source_bytes, width, height, first_block_row, block_row_count, destination,
                    destination_bytes, HasSsse3());
}

bool DecodeEtcImageBlockRowsPortable(EtcFormat format, const std::uint8_t* source, std::size_t source_bytes,
                                     std::uint32_t width, std::uint32_t height, std::uint32_t first_block_row,
                                     std::uint32_t block_row_count, std::uint8_t* destination,
                                     std::size_t destination_bytes) {
  return DecodeRows(format, source, source_bytes, width, height, first_block_row, block_row_count, destination,
                    destination_bytes, false);
}

bool EtcDecoderIsVectorized() { return HasSsse3(); }

}
