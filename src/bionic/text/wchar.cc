// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The multibyte conversions are adapted from AOSP bionic
// (references/platform_bionic/libc/bionic/wchar.cpp, mbrtoc32.cpp,
// c32rtomb.cpp, mbrtoc16.cpp, c16rtomb.cpp, private/bionic_mbstate.h):
//
//   Copyright (C) 2008-2014 The Android Open Source Project
//   All rights reserved.
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in
//      the documentation and/or other materials provided with the
//      distribution.
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
//
// The wcs*/wmem* functions follow FreeBSD libc/string (wcscmp, wcsstr,
// wcstok, wcslcat, ...; Copyright (c) 1989, 1990, 1993 The Regents of the
// University of California, Copyright (c) 1999 Citrus Project, BSD) and the
// OpenBSD locale wrappers (mbtowc, wctomb, btowc, wctob, mbstowcs, wcstombs;
// Copyright (c) 2002-2004 Tim J. Robbins, BSD-2-Clause) shipped in bionic.
#include <cstring>

#include "bionic/guest_constants.h"
#include "bionic/memory.h"
#include "bionic/runtime.h"
#include "bionic/text/text_internal.h"

namespace {

namespace ti = elixir::bionic::text_internal;
using ti::g_size;
using ti::g_wchar;
using ti::g_wint;
using elixir::bionic::SetGuestErrno;
namespace guest = elixir::bionic::guest;

struct MbState {
  std::uint8_t seq[4];
  std::uint8_t reserved[4];
};
static_assert(sizeof(MbState) == sizeof(guest::Mbstate), "mbstate_t is 8 bytes");

bool IsInitial(const MbState* ps) noexcept {
  return (ps->seq[0] | ps->seq[1] | ps->seq[2] | ps->seq[3]) == 0;
}
std::size_t BytesSoFar(const MbState* ps) noexcept {
  return ps->seq[2] != 0 ? 3 : ps->seq[1] != 0 ? 2 : ps->seq[0] != 0 ? 1 : 0;
}
void Reset(MbState* ps) noexcept { std::memset(ps->seq, 0, sizeof(ps->seq)); }
std::size_t ResetAndReturnIllegal(int error, MbState* ps) noexcept {
  SetGuestErrno(error);
  Reset(ps);
  return ti::kMultibyteIllegal;
}
std::size_t ResetAndReturn(std::size_t result, MbState* ps) noexcept {
  Reset(ps);
  return result;
}

MbState* StateOr(void* ps, MbState* fallback) noexcept {
  return ps != nullptr ? static_cast<MbState*>(ps) : fallback;
}

std::size_t Mbrtoc32(std::uint32_t* pc32, const char* s, std::size_t n, MbState* state) noexcept {
  if (state->seq[3] != 0) return ResetAndReturnIllegal(guest::kEINVAL, state);
  if (s == nullptr) {
    s = "";
    n = 1;
    pc32 = nullptr;
  }
  if (n == 0) return ti::kMultibyteIncomplete;

  std::uint8_t ch = static_cast<std::uint8_t>(*s);
  if (IsInitial(state) && (ch & ~0x7f) == 0) {
    if (pc32 != nullptr) *pc32 = ch;
    return ch != '\0' ? 1 : 0;
  }

  std::size_t length;
  int mask;
  std::uint32_t lower_bound;
  const std::size_t bytes_so_far = BytesSoFar(state);
  ch = bytes_so_far > 0 ? state->seq[0] : static_cast<std::uint8_t>(*s);
  if ((ch & 0xe0) == 0xc0) {
    mask = 0x1f;
    length = 2;
    lower_bound = 0x80;
  } else if ((ch & 0xf0) == 0xe0) {
    mask = 0x0f;
    length = 3;
    lower_bound = 0x800;
  } else if ((ch & 0xf8) == 0xf0) {
    mask = 0x07;
    length = 4;
    lower_bound = 0x10000;
  } else {
    return ResetAndReturnIllegal(guest::kEILSEQ, state);
  }

  const std::size_t bytes_wanted = length - bytes_so_far;
  std::size_t i;
  for (i = 0; i < (bytes_wanted < n ? bytes_wanted : n); ++i) {
    if (!IsInitial(state) && ((*s & 0xc0) != 0x80)) return ResetAndReturnIllegal(guest::kEILSEQ, state);
    state->seq[bytes_so_far + i] = static_cast<std::uint8_t>(*s++);
  }
  if (i < bytes_wanted) return ti::kMultibyteIncomplete;

  std::uint32_t c32 = state->seq[0] & mask;
  for (i = 1; i < length; ++i) {
    c32 <<= 6;
    c32 |= state->seq[i] & 0x3f;
  }
  if (c32 < lower_bound) return ResetAndReturnIllegal(guest::kEILSEQ, state);
  if ((c32 >= 0xd800 && c32 <= 0xdfff) || c32 > 0x10ffff) return ResetAndReturnIllegal(guest::kEILSEQ, state);
  if (pc32 != nullptr) *pc32 = c32;
  return ResetAndReturn(c32 == 0 ? 0 : bytes_wanted, state);
}

std::size_t C32rtomb(char* s, std::uint32_t c32, MbState* state) noexcept {
  if (s == nullptr) return ResetAndReturn(1, state);
  if (c32 == 0) {
    *s = '\0';
    return ResetAndReturn(1, state);
  }
  if (!IsInitial(state)) return ResetAndReturnIllegal(guest::kEILSEQ, state);
  if ((c32 & ~0x7fu) == 0) {
    *s = static_cast<char>(c32);
    return 1;
  }
  std::uint8_t lead;
  std::size_t length;
  if ((c32 & ~0x7ffu) == 0) {
    lead = 0xc0;
    length = 2;
  } else if ((c32 & ~0xffffu) == 0) {
    lead = 0xe0;
    length = 3;
  } else if ((c32 & ~0x1fffffu) == 0) {
    lead = 0xf0;
    length = 4;
  } else {
    SetGuestErrno(guest::kEILSEQ);
    return ti::kMultibyteIllegal;
  }
  for (std::size_t i = length - 1; i > 0; --i) {
    s[i] = static_cast<char>((c32 & 0x3f) | 0x80);
    c32 >>= 6;
  }
  *s = static_cast<char>((c32 & 0xff) | lead);
  return length;
}

MbState g_mbrtowc_state, g_mbsnrtowcs_state, g_wcrtomb_state, g_wcsnrtombs_state, g_mbrtoc32_state,
    g_c32rtomb_state, g_mbrtoc16_state, g_c16rtomb_state, g_mbrlen_state, g_mbtowc_state, g_wctomb_state;

std::size_t Mbsnrtowcs(g_wchar* dst, const char** src, std::size_t nmc, std::size_t len, MbState* state) noexcept {
  std::size_t i, o, r;
  if (nmc > 0 && BytesSoFar(state) > 0 && static_cast<std::uint8_t>((*src)[0]) < 0x80) {
    return ResetAndReturnIllegal(guest::kEILSEQ, state);
  }
  if (dst == nullptr) {
    for (i = o = 0; i < nmc; i += r, o++) {
      if (static_cast<std::uint8_t>((*src)[i]) < 0x80) {
        if ((*src)[i] == '\0') return ResetAndReturn(o, state);
        r = 1;
      } else {
        r = Mbrtoc32(nullptr, *src + i, nmc - i, state);
        if (r == ti::kMultibyteIllegal || r == ti::kMultibyteIncomplete) {
          return ResetAndReturnIllegal(guest::kEILSEQ, state);
        }
        if (r == 0) return ResetAndReturn(o, state);
      }
    }
    return ResetAndReturn(o, state);
  }
  for (i = o = 0; i < nmc && o < len; i += r, o++) {
    if (static_cast<std::uint8_t>((*src)[i]) < 0x80) {
      dst[o] = static_cast<std::uint8_t>((*src)[i]);
      r = 1;
      if ((*src)[i] == '\0') {
        *src = nullptr;
        return ResetAndReturn(o, state);
      }
    } else {
      r = Mbrtoc32(dst + o, *src + i, nmc - i, state);
      if (r == ti::kMultibyteIllegal) {
        *src += i;
        return ResetAndReturnIllegal(guest::kEILSEQ, state);
      }
      if (r == ti::kMultibyteIncomplete) {
        *src += nmc;
        return ResetAndReturnIllegal(guest::kEILSEQ, state);
      }
      if (r == 0) {
        *src = nullptr;
        return ResetAndReturn(o, state);
      }
    }
  }
  *src += i;
  return ResetAndReturn(o, state);
}

std::size_t Wcsnrtombs(char* dst, const g_wchar** src, std::size_t nwc, std::size_t len, MbState* state) noexcept {
  if (!IsInitial(state)) return ResetAndReturnIllegal(guest::kEILSEQ, state);
  char buf[4];
  std::size_t i, o, r;
  if (dst == nullptr) {
    for (i = o = 0; i < nwc; i++, o += r) {
      const g_wchar wc = (*src)[i];
      if (wc < 0x80) {
        if (wc == 0) return o;
        r = 1;
      } else {
        r = C32rtomb(buf, wc, state);
        if (r == ti::kMultibyteIllegal) return r;
      }
    }
    return o;
  }
  for (i = o = 0; i < nwc && o < len; i++, o += r) {
    const g_wchar wc = (*src)[i];
    if (wc < 0x80) {
      dst[o] = static_cast<char>(wc);
      if (wc == 0) {
        *src = nullptr;
        return o;
      }
      r = 1;
    } else if (len - o >= sizeof(buf)) {
      r = C32rtomb(dst + o, wc, state);
      if (r == ti::kMultibyteIllegal) {
        *src += i;
        return r;
      }
    } else {
      r = C32rtomb(buf, wc, state);
      if (r == ti::kMultibyteIllegal) {
        *src += i;
        return r;
      }
      if (r > len - o) break;
      std::memcpy(dst + o, buf, r);
    }
  }
  *src += i;
  return o;
}

g_size Wcslen(const g_wchar* s) noexcept {
  const g_wchar* p = s;
  while (*p != 0) ++p;
  return static_cast<g_size>(p - s);
}

int Wcsncmp(const g_wchar* s1, const g_wchar* s2, g_size n) noexcept {
  if (n == 0) return 0;
  do {
    if (*s1 != *s2++) return static_cast<int>(*s1 - *--s2);
    if (*s1++ == 0) break;
  } while (--n != 0);
  return 0;
}

}

namespace elixir::bionic::text_internal {

std::size_t Mbrtoc32WithState(std::uint32_t* pc32, const char* s, std::size_t n, void* state) noexcept {
  return Mbrtoc32(pc32, s, n, static_cast<MbState*>(state));
}

std::size_t C32rtombWithState(char* s, std::uint32_t c32, void* state) noexcept {
  return C32rtomb(s, c32, static_cast<MbState*>(state));
}

}

ELIXIR_BIONIC_FN(g_size, wcslen)(const g_wchar* s) noexcept { return Wcslen(s); }

ELIXIR_BIONIC_FN(g_size, wcsnlen)(const g_wchar* s, g_size max_length) noexcept {
  g_size length = 0;
  while (length < max_length && s[length] != 0) ++length;
  return length;
}

ELIXIR_BIONIC_FN(g_wchar*, wcscpy)(g_wchar* dst, const g_wchar* src) noexcept {
  std::memcpy(dst, src, (Wcslen(src) + 1) * sizeof(g_wchar));
  return dst;
}

ELIXIR_BIONIC_FN(g_wchar*, wcsncpy)(g_wchar* dst, const g_wchar* src, g_size n) noexcept {
  g_size length = 0;
  while (length < n && src[length] != 0) ++length;
  std::memcpy(dst, src, length * sizeof(g_wchar));
  for (g_size i = length; i < n; ++i) dst[i] = 0;
  return dst;
}

ELIXIR_BIONIC_FN(g_wchar*, wcpcpy)(g_wchar* dst, const g_wchar* src) noexcept {
  const g_size length = Wcslen(src);
  std::memcpy(dst, src, (length + 1) * sizeof(g_wchar));
  return dst + length;
}

ELIXIR_BIONIC_FN(g_wchar*, wcpncpy)(g_wchar* dst, const g_wchar* src, g_size n) noexcept {
  g_size length = 0;
  while (length < n && src[length] != 0) ++length;
  std::memcpy(dst, src, length * sizeof(g_wchar));
  for (g_size i = length; i < n; ++i) dst[i] = 0;
  return dst + length;
}

ELIXIR_BIONIC_FN(g_wchar*, wcscat)(g_wchar* dst, const g_wchar* src) noexcept {
  g_wchar* end = dst + Wcslen(dst);
  std::memcpy(end, src, (Wcslen(src) + 1) * sizeof(g_wchar));
  return dst;
}

ELIXIR_BIONIC_FN(g_wchar*, wcsncat)(g_wchar* dst, const g_wchar* src, g_size n) noexcept {
  g_wchar* q = dst + Wcslen(dst);
  while (n != 0 && *src != 0) {
    *q++ = *src++;
    --n;
  }
  *q = 0;
  return dst;
}

ELIXIR_BIONIC_FN(g_size, wcslcpy)(g_wchar* dst, const g_wchar* src, g_size size) noexcept {
  const g_size length = Wcslen(src);
  if (size != 0) {
    const g_size copy = length < size - 1 ? length : size - 1;
    std::memcpy(dst, src, copy * sizeof(g_wchar));
    dst[copy] = 0;
  }
  return length;
}

ELIXIR_BIONIC_FN(g_size, wcslcat)(g_wchar* dst, const g_wchar* src, g_size size) noexcept {
  g_wchar* d = dst;
  const g_wchar* s = src;
  g_size n = size;
  while (n-- != 0 && *d != 0) d++;
  const g_size dlen = static_cast<g_size>(d - dst);
  n = size - dlen;
  if (n == 0) return dlen + Wcslen(s);
  while (*s != 0) {
    if (n != 1) {
      *d++ = *s;
      n--;
    }
    s++;
  }
  *d = 0;
  return dlen + static_cast<g_size>(s - src);
}

ELIXIR_BIONIC_FN(g_wchar*, wcsdup)(const g_wchar* s) noexcept {
  const g_size bytes = (Wcslen(s) + 1) * sizeof(g_wchar);
  auto* copy = static_cast<g_wchar*>(elixir::bionic::GuestMalloc(bytes));
  if (copy == nullptr) {
    SetGuestErrno(guest::kENOMEM);
    return nullptr;
  }
  std::memcpy(copy, s, bytes);
  return copy;
}

ELIXIR_BIONIC_FN(int, wcscmp)(const g_wchar* s1, const g_wchar* s2) noexcept {
  while (*s1 == *s2++) {
    if (*s1++ == 0) return 0;
  }
  return static_cast<int>(*s1 - *--s2);
}

ELIXIR_BIONIC_FN(int, wcsncmp)(const g_wchar* s1, const g_wchar* s2, g_size n) noexcept {
  return Wcsncmp(s1, s2, n);
}

ELIXIR_BIONIC_FN(int, wcscasecmp)(const g_wchar* s1, const g_wchar* s2) noexcept {
  for (; *s1 != 0; s1++, s2++) {
    const g_wint c1 = ti::WideToLower(*s1);
    const g_wint c2 = ti::WideToLower(*s2);
    if (c1 != c2) return static_cast<int>(c1) - static_cast<int>(c2);
  }
  return -static_cast<int>(*s2);
}

ELIXIR_BIONIC_FN(int, wcsncasecmp)(const g_wchar* s1, const g_wchar* s2, g_size n) noexcept {
  if (n == 0) return 0;
  for (; *s1 != 0; s1++, s2++) {
    const g_wint c1 = ti::WideToLower(*s1);
    const g_wint c2 = ti::WideToLower(*s2);
    if (c1 != c2) return static_cast<int>(c1) - static_cast<int>(c2);
    if (--n == 0) return 0;
  }
  return -static_cast<int>(*s2);
}

ELIXIR_BIONIC_FN(int, wcscoll)(const g_wchar* s1, const g_wchar* s2) noexcept {
  return elixir_bionic_wcscmp(s1, s2);
}

ELIXIR_BIONIC_FN(g_size, wcsxfrm)(g_wchar* dst, const g_wchar* src, g_size n) noexcept {
  if (n == 0) return Wcslen(src);
  return elixir_bionic_wcslcpy(dst, src, n);
}

ELIXIR_BIONIC_FN(g_wchar*, wcschr)(const g_wchar* s, g_wchar c) noexcept {
  while (*s != c && *s != 0) ++s;
  return *s == c ? const_cast<g_wchar*>(s) : nullptr;
}

ELIXIR_BIONIC_FN(g_wchar*, wcsrchr)(const g_wchar* s, g_wchar c) noexcept {
  const g_wchar* last = nullptr;
  for (;; ++s) {
    if (*s == c) last = s;
    if (*s == 0) break;
  }
  return const_cast<g_wchar*>(last);
}

ELIXIR_BIONIC_FN(g_wchar*, wcsstr)(const g_wchar* s, const g_wchar* find) noexcept {
  g_wchar c = *find++;
  if (c != 0) {
    const g_size length = Wcslen(find);
    do {
      g_wchar sc;
      do {
        if ((sc = *s++) == 0) return nullptr;
      } while (sc != c);
    } while (Wcsncmp(s, find, length) != 0);
    s--;
  }
  return const_cast<g_wchar*>(s);
}

ELIXIR_BIONIC_FN(g_size, wcsspn)(const g_wchar* s, const g_wchar* accept) noexcept {
  const g_wchar* p = s;
  for (; *p != 0; ++p) {
    const g_wchar* q = accept;
    while (*q != 0 && *q != *p) ++q;
    if (*q == 0) break;
  }
  return static_cast<g_size>(p - s);
}

ELIXIR_BIONIC_FN(g_size, wcscspn)(const g_wchar* s, const g_wchar* reject) noexcept {
  const g_wchar* p = s;
  for (; *p != 0; ++p) {
    for (const g_wchar* q = reject; *q != 0; ++q) {
      if (*q == *p) return static_cast<g_size>(p - s);
    }
  }
  return static_cast<g_size>(p - s);
}

ELIXIR_BIONIC_FN(g_wchar*, wcspbrk)(const g_wchar* s, const g_wchar* accept) noexcept {
  for (; *s != 0; ++s) {
    for (const g_wchar* q = accept; *q != 0; ++q) {
      if (*q == *s) return const_cast<g_wchar*>(s);
    }
  }
  return nullptr;
}

ELIXIR_BIONIC_FN(g_wchar*, wcstok)(g_wchar* s, const g_wchar* delim, g_wchar** last) noexcept {
  if (s == nullptr && (s = *last) == nullptr) return nullptr;
  g_wchar c, sc;
  const g_wchar* spanp;
cont:
  c = *s++;
  for (spanp = delim; (sc = *spanp++) != 0;) {
    if (c == sc) goto cont;
  }
  if (c == 0) {
    *last = nullptr;
    return nullptr;
  }
  g_wchar* token = s - 1;
  for (;;) {
    c = *s++;
    spanp = delim;
    do {
      if ((sc = *spanp++) == c) {
        if (c == 0) {
          s = nullptr;
        } else {
          s[-1] = 0;
        }
        *last = s;
        return token;
      }
    } while (sc != 0);
  }
}

ELIXIR_BIONIC_FN(g_wchar*, wmemchr)(const g_wchar* s, g_wchar c, g_size n) noexcept {
  for (; n != 0; --n, ++s) {
    if (*s == c) return const_cast<g_wchar*>(s);
  }
  return nullptr;
}

ELIXIR_BIONIC_FN(int, wmemcmp)(const g_wchar* s1, const g_wchar* s2, g_size n) noexcept {
  for (; n != 0; --n, ++s1, ++s2) {
    if (*s1 != *s2) {
      return static_cast<std::int32_t>(*s1) > static_cast<std::int32_t>(*s2) ? 1 : -1;
    }
  }
  return 0;
}

ELIXIR_BIONIC_FN(g_wchar*, wmemcpy)(g_wchar* dst, const g_wchar* src, g_size n) noexcept {
  return static_cast<g_wchar*>(std::memcpy(dst, src, n * sizeof(g_wchar)));
}

ELIXIR_BIONIC_FN(g_wchar*, wmempcpy)(g_wchar* dst, const g_wchar* src, g_size n) noexcept {
  return static_cast<g_wchar*>(std::memcpy(dst, src, n * sizeof(g_wchar))) + n;
}

ELIXIR_BIONIC_FN(g_wchar*, wmemmove)(g_wchar* dst, const g_wchar* src, g_size n) noexcept {
  return static_cast<g_wchar*>(std::memmove(dst, src, n * sizeof(g_wchar)));
}

ELIXIR_BIONIC_FN(g_wchar*, wmemset)(g_wchar* dst, g_wchar c, g_size n) noexcept {
  for (g_size i = 0; i < n; ++i) dst[i] = c;
  return dst;
}

ELIXIR_BIONIC_FN(int, mbsinit)(const void* ps) noexcept {
  return ps == nullptr || IsInitial(static_cast<const MbState*>(ps));
}

ELIXIR_BIONIC_FN(g_size, mbrtoc32)(std::uint32_t* pc32, const char* s, g_size n, void* ps) noexcept {
  return Mbrtoc32(pc32, s, n, StateOr(ps, &g_mbrtoc32_state));
}

ELIXIR_BIONIC_FN(g_size, mbrtowc)(g_wchar* pwc, const char* s, g_size n, void* ps) noexcept {
  return Mbrtoc32(pwc, s, n, StateOr(ps, &g_mbrtowc_state));
}

ELIXIR_BIONIC_FN(g_size, mbrlen)(const char* s, g_size n, void* ps) noexcept {
  return Mbrtoc32(nullptr, s, n, StateOr(ps, &g_mbrlen_state));
}

ELIXIR_BIONIC_FN(g_size, c32rtomb)(char* s, std::uint32_t c32, void* ps) noexcept {
  return C32rtomb(s, c32, StateOr(ps, &g_c32rtomb_state));
}

ELIXIR_BIONIC_FN(g_size, wcrtomb)(char* s, g_wchar wc, void* ps) noexcept {
  return C32rtomb(s, wc, StateOr(ps, &g_wcrtomb_state));
}

ELIXIR_BIONIC_FN(g_size, mbrtoc16)(std::uint16_t* pc16, const char* s, g_size n, void* ps) noexcept {
  MbState* state = StateOr(ps, &g_mbrtoc16_state);
  std::uint16_t private_pc16;
  if (pc16 == nullptr) pc16 = &private_pc16;
  if (state->seq[3] != 0) {
    *pc16 = static_cast<std::uint16_t>(state->seq[1] << 8 | state->seq[0]);
    Reset(state);
    return static_cast<g_size>(-3);
  }
  std::uint32_t c32;
  const std::size_t nconv = Mbrtoc32(&c32, s, n, state);
  if (nconv == ti::kMultibyteIllegal || nconv == ti::kMultibyteIncomplete) return nconv;
  if (nconv == 0) return ResetAndReturn(nconv, state);
  if (c32 < 0x10000) {
    *pc16 = static_cast<std::uint16_t>(c32);
    return ResetAndReturn(nconv, state);
  }
  if (c32 > 0x10ffff) return ResetAndReturnIllegal(guest::kEILSEQ, state);
  c32 -= 0x10000;
  const auto trail = static_cast<std::uint16_t>((c32 & 0x3ff) | 0xdc00);
  state->seq[0] = static_cast<std::uint8_t>(trail & 0x00ff);
  state->seq[1] = static_cast<std::uint8_t>((trail & 0xff00) >> 8);
  state->seq[3] = static_cast<std::uint8_t>(nconv & 0xff);
  *pc16 = static_cast<std::uint16_t>(((c32 & 0xffc00) >> 10) | 0xd800);
  return nconv;
}

ELIXIR_BIONIC_FN(g_size, c16rtomb)(char* s, std::uint16_t c16, void* ps) noexcept {
  MbState* state = StateOr(ps, &g_c16rtomb_state);
  const bool high = c16 >= 0xd800 && c16 < 0xdc00;
  const bool low = c16 >= 0xdc00 && c16 < 0xe000;
  if (IsInitial(state)) {
    if (high) {
      const std::uint32_t c32 = static_cast<std::uint32_t>(c16 & ~0xd800) << 10;
      state->seq[3] = static_cast<std::uint8_t>((c32 & 0xff0000) >> 16);
      state->seq[2] = static_cast<std::uint8_t>((c32 & 0x00ff00) >> 8);
      return 0;
    }
    if (low) return ResetAndReturnIllegal(guest::kEINVAL, state);
    return C32rtomb(s, c16, state);
  }
  if (!low) return ResetAndReturnIllegal(guest::kEINVAL, state);
  const std::uint32_t c32 =
      ((static_cast<std::uint32_t>(state->seq[3]) << 16) | (static_cast<std::uint32_t>(state->seq[2]) << 8) |
       static_cast<std::uint32_t>(c16 & ~0xdc00)) + 0x10000;
  MbState scratch = {};
  return ResetAndReturn(C32rtomb(s, c32, &scratch), state);
}

ELIXIR_BIONIC_FN(g_size, mbsnrtowcs)(g_wchar* dst, const char** src, g_size nmc, g_size len, void* ps) noexcept {
  return Mbsnrtowcs(dst, src, nmc, len, StateOr(ps, &g_mbsnrtowcs_state));
}

ELIXIR_BIONIC_FN(g_size, mbsrtowcs)(g_wchar* dst, const char** src, g_size len, void* ps) noexcept {
  return Mbsnrtowcs(dst, src, static_cast<std::size_t>(-1), len, StateOr(ps, &g_mbsnrtowcs_state));
}

ELIXIR_BIONIC_FN(g_size, wcsnrtombs)(char* dst, const g_wchar** src, g_size nwc, g_size len, void* ps) noexcept {
  return Wcsnrtombs(dst, src, nwc, len, StateOr(ps, &g_wcsnrtombs_state));
}

ELIXIR_BIONIC_FN(g_size, wcsrtombs)(char* dst, const g_wchar** src, g_size len, void* ps) noexcept {
  return Wcsnrtombs(dst, src, static_cast<std::size_t>(-1), len, StateOr(ps, &g_wcsnrtombs_state));
}

ELIXIR_BIONIC_FN(g_size, mbstowcs)(g_wchar* dst, const char* src, g_size n) noexcept {
  MbState state = {};
  const char* cursor = src;
  return Mbsnrtowcs(dst, &cursor, static_cast<std::size_t>(-1), n, &state);
}

ELIXIR_BIONIC_FN(g_size, wcstombs)(char* dst, const g_wchar* src, g_size n) noexcept {
  MbState state = {};
  const g_wchar* cursor = src;
  return Wcsnrtombs(dst, &cursor, static_cast<std::size_t>(-1), n, &state);
}

ELIXIR_BIONIC_FN(int, mbtowc)(g_wchar* pwc, const char* s, g_size n) noexcept {
  if (s == nullptr) {
    Reset(&g_mbtowc_state);
    return 0;
  }
  const std::size_t result = Mbrtoc32(pwc, s, n, &g_mbtowc_state);
  if (result == ti::kMultibyteIncomplete) {
    SetGuestErrno(guest::kEILSEQ);
    return -1;
  }
  if (result == ti::kMultibyteIllegal) return -1;
  return static_cast<int>(result);
}

ELIXIR_BIONIC_FN(int, wctomb)(char* s, g_wchar wc) noexcept {
  if (s == nullptr) {
    Reset(&g_wctomb_state);
    return 0;
  }
  const std::size_t result = C32rtomb(s, wc, &g_wctomb_state);
  return result == ti::kMultibyteIllegal ? -1 : static_cast<int>(result);
}

ELIXIR_BIONIC_FN(int, mblen)(const char* s, g_size n) noexcept {
  MbState state = {};
  return static_cast<int>(Mbrtoc32(nullptr, s, n, &state));
}

ELIXIR_BIONIC_FN(g_wint, btowc)(int c) noexcept {
  if (c == -1) return ti::kWeof;
  MbState state = {};
  const char byte = static_cast<char>(c);
  std::uint32_t wc = 0;
  if (Mbrtoc32(&wc, &byte, 1, &state) > 1) return ti::kWeof;
  return wc;
}

ELIXIR_BIONIC_FN(int, wctob)(g_wint c) noexcept {
  if (c == ti::kWeof) return -1;
  MbState state = {};
  char buffer[4];
  if (C32rtomb(buffer, c, &state) != 1) return -1;
  return static_cast<unsigned char>(buffer[0]);
}

namespace elixir::bionic::text_internal {

void RegisterWcharExports(ExportTable& table) {
  ELIXIR_TEXT_EXPORT(table, kLibc, wcslen);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcsnlen);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcscpy);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcsncpy);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcpcpy);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcpncpy);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcscat);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcsncat);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcslcpy);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcslcat);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcsdup);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcscmp);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcsncmp);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcscasecmp);
  ELIXIR_TEXT_ALIAS(table, kLibc, wcscasecmp_l, wcscasecmp);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcsncasecmp);
  ELIXIR_TEXT_ALIAS(table, kLibc, wcsncasecmp_l, wcsncasecmp);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcscoll);
  ELIXIR_TEXT_ALIAS(table, kLibc, wcscoll_l, wcscoll);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcsxfrm);
  ELIXIR_TEXT_ALIAS(table, kLibc, wcsxfrm_l, wcsxfrm);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcschr);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcsrchr);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcsstr);
  ELIXIR_TEXT_ALIAS(table, kLibc, wcswcs, wcsstr);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcsspn);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcscspn);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcspbrk);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcstok);
  ELIXIR_TEXT_EXPORT(table, kLibc, wmemchr);
  ELIXIR_TEXT_EXPORT(table, kLibc, wmemcmp);
  ELIXIR_TEXT_EXPORT(table, kLibc, wmemcpy);
  ELIXIR_TEXT_EXPORT(table, kLibc, wmempcpy);
  ELIXIR_TEXT_EXPORT(table, kLibc, wmemmove);
  ELIXIR_TEXT_EXPORT(table, kLibc, wmemset);
  ELIXIR_TEXT_EXPORT(table, kLibc, mbsinit);
  ELIXIR_TEXT_EXPORT(table, kLibc, mbrtowc);
  ELIXIR_TEXT_EXPORT(table, kLibc, mbrlen);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcrtomb);
  ELIXIR_TEXT_EXPORT(table, kLibc, mbrtoc32);
  ELIXIR_TEXT_EXPORT(table, kLibc, c32rtomb);
  ELIXIR_TEXT_EXPORT(table, kLibc, mbrtoc16);
  ELIXIR_TEXT_EXPORT(table, kLibc, c16rtomb);
  ELIXIR_TEXT_EXPORT(table, kLibc, mbsnrtowcs);
  ELIXIR_TEXT_EXPORT(table, kLibc, mbsrtowcs);
  ELIXIR_TEXT_ALIAS(table, kLibc, mbsrtowcs_l, mbsrtowcs);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcsnrtombs);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcsrtombs);
  ELIXIR_TEXT_ALIAS(table, kLibc, wcsrtombs_l, wcsrtombs);
  ELIXIR_TEXT_EXPORT(table, kLibc, mbstowcs);
  ELIXIR_TEXT_EXPORT(table, kLibc, wcstombs);
  ELIXIR_TEXT_EXPORT(table, kLibc, mbtowc);
  ELIXIR_TEXT_EXPORT(table, kLibc, wctomb);
  ELIXIR_TEXT_EXPORT(table, kLibc, mblen);
  ELIXIR_TEXT_EXPORT(table, kLibc, btowc);
  ELIXIR_TEXT_EXPORT(table, kLibc, wctob);
}

}
