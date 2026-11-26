// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The directive handling, the integer scanner, the floating-point prefix
// scanner (parsefloat) and the EOF/match-failure semantics follow AOSP
// bionic libc/stdio/vfscanf.cpp, vfwscanf.cpp, scanf_common.h and
// parsefloat.c, which carry this notice:
//
//   Copyright (c) 1990, 1993
//        The Regents of the University of California.  All rights reserved.
//
//   This code is derived from software contributed to Berkeley by
//   Chris Torek.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//   1. Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//   2. Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//   3. Neither the name of the University nor the names of its contributors
//      may be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
//   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
//   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//   ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
//   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
//   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
//   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
//   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
//   SUCH DAMAGE.
#include <climits>
#include <cstring>
#include <new>
#include <string>
#include <type_traits>

#include "bionic/format.h"
#include "bionic/format/format_internal.h"
#include "bionic/guest_constants.h"
#include "bionic/memory.h"
#include "bionic/runtime.h"

namespace elixir::bionic::format {
namespace {

using abi::g_wchar;

constexpr int kLong = 0x00001;
constexpr int kLongDbl = 0x00002;
constexpr int kShort = 0x00004;
constexpr int kShortShort = 0x00008;
constexpr int kLLong = 0x00010;
constexpr int kPointer = 0x00020;
constexpr int kSizeInt = 0x00040;
constexpr int kMaxInt = 0x00080;
constexpr int kPtrInt = 0x00100;
constexpr int kNoSkip = 0x00200;
constexpr int kSuppress = 0x00400;
constexpr int kUnsigned = 0x00800;
constexpr int kAllocate = 0x01000;
constexpr int kSignOk = 0x02000;
constexpr int kHaveSign = 0x04000;
constexpr int kNDigits = 0x08000;
constexpr int kPfxOk = 0x10000;
constexpr int kPfbOk = 0x20000;
constexpr int kNzDigits = 0x40000;

enum Conversion { kCtChar, kCtCcl, kCtString, kCtInt, kCtFloat };

constexpr std::size_t kNumericBuffer = 513;
constexpr std::int64_t kEof = -1;

template <typename SourceT>
class Reader {
 public:
  explicit Reader(SourceT& source) : source_(source) {}
  ~Reader() { Finish(); }

  std::int64_t Peek() {
    if (!have_) {
      lookahead_ = source_.Get();
      have_ = true;
    }
    return lookahead_;
  }
  void Consume() { have_ = false; }
  std::int64_t Next() {
    const std::int64_t c = Peek();
    if (c != kEof) have_ = false;
    return c;
  }
  void Unread(std::int64_t c) {
    if (have_) {
      if (lookahead_ != kEof) Unget(lookahead_);
      have_ = false;
    }
    Unget(c);
  }
  void Finish() {
    if (have_ && lookahead_ != kEof) Unget(lookahead_);
    have_ = false;
  }

 private:
  void Unget(std::int64_t c) {
    if constexpr (std::is_same_v<SourceT, Source>) {
      source_.Unget(static_cast<int>(c));
    } else {
      source_.Unget(c);
    }
  }

  SourceT& source_;
  std::int64_t lookahead_ = kEof;
  bool have_ = false;
};

int WidthToFlag(int size, bool fast) {
  if (size == 8) return kShortShort;
  if (size == 16) return fast ? kLLong : kShort;
  if (size == 32) return fast ? kLLong : 0;
  if (size == 64) return kLLong;
  return -1;
}

std::uint64_t ConvertInteger(const char* text, int base, bool is_unsigned, bool* range_error) {
  const char* p = text;
  bool negative = false;
  if (*p == '+' || *p == '-') {
    negative = *p == '-';
    ++p;
  }
  if (base == 16 && p[0] == '0' && (p[1] | 0x20) == 'x' && internal::IsXdigit(p[2])) p += 2;
  if (base == 2 && p[0] == '0' && (p[1] | 0x20) == 'b' && (p[2] == '0' || p[2] == '1')) p += 2;
  std::uint64_t value = 0;
  bool overflow = false;
  for (; *p != 0; ++p) {
    int digit;
    if (internal::IsDigit(*p)) {
      digit = *p - '0';
    } else if (*p >= 'a' && *p <= 'z') {
      digit = *p - 'a' + 10;
    } else if (*p >= 'A' && *p <= 'Z') {
      digit = *p - 'A' + 10;
    } else {
      break;
    }
    if (digit >= base) break;
    if (value > (UINT64_MAX - static_cast<std::uint64_t>(digit)) / static_cast<std::uint64_t>(base)) {
      overflow = true;
    } else {
      value = value * static_cast<std::uint64_t>(base) + static_cast<std::uint64_t>(digit);
    }
  }
  if (is_unsigned) {
    if (overflow) {
      *range_error = true;
      return UINT64_MAX;
    }
    return negative ? 0 - value : value;
  }
  const std::uint64_t limit = negative ? (std::uint64_t{1} << 63) : (std::uint64_t{1} << 63) - 1;
  if (overflow || value > limit) {
    *range_error = true;
    return negative ? (std::uint64_t{1} << 63) : (std::uint64_t{1} << 63) - 1;
  }
  return negative ? 0 - value : value;
}

void StoreBytes(void* target, const void* value, std::size_t size) {
  if (target != nullptr) std::memcpy(target, value, size);
}

void StoreInteger(void* target, int flags, std::uint64_t value) {
  if (flags & (kPointer | kMaxInt | kLLong | kSizeInt | kPtrInt | kLong)) {
    StoreBytes(target, &value, 8);
  } else if (flags & kShort) {
    const auto v = static_cast<std::uint16_t>(value);
    StoreBytes(target, &v, 2);
  } else if (flags & kShortShort) {
    const auto v = static_cast<std::uint8_t>(value);
    StoreBytes(target, &v, 1);
  } else {
    const auto v = static_cast<std::uint32_t>(value);
    StoreBytes(target, &v, 4);
  }
}

void StoreCount(void* target, int flags, int count) {
  const auto value = static_cast<std::int64_t>(count);
  if (flags & kShortShort) {
    const auto v = static_cast<std::int8_t>(count);
    StoreBytes(target, &v, 1);
  } else if (flags & kShort) {
    const auto v = static_cast<std::int16_t>(count);
    StoreBytes(target, &v, 2);
  } else if (flags & (kLong | kSizeInt | kPtrInt | kLLong | kMaxInt)) {
    StoreBytes(target, &value, 8);
  } else {
    StoreBytes(target, &count, 4);
  }
}

template <typename ReaderT>
bool ScanInteger(ReaderT& in, std::size_t width, int flags, int* base, char* buffer, std::size_t* length) {
  if (width == 0 || width > kNumericBuffer - 1) width = kNumericBuffer - 1;
  flags |= kSignOk | kNDigits | kNzDigits;
  char* p = buffer;
  for (; width != 0; --width) {
    const std::int64_t c = in.Peek();
    bool ok = false;
    switch (c) {
      case '0':
        if (*base == 0) {
          *base = 8;
          flags |= kPfbOk | kPfxOk;
        }
        if (flags & kNzDigits) {
          flags &= ~(kSignOk | kNzDigits | kNDigits);
        } else {
          flags &= ~(kSignOk | kPfbOk | kPfxOk | kNDigits);
        }
        ok = true;
        break;
      case 'B':
      case 'b':
        if ((flags & kPfbOk) && p == buffer + 1 + ((flags & kHaveSign) ? 1 : 0)) {
          *base = 2;
          flags &= ~kPfbOk;
          ok = true;
          break;
        }
        [[fallthrough]];
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case 'A':
      case 'C':
      case 'D':
      case 'E':
      case 'F':
      case 'a':
      case 'c':
      case 'd':
      case 'e':
      case 'f':
        if (*base == 0) *base = 10;
        if (*base != 16 && (c - '0') >= *base) break;
        flags &= ~(kSignOk | kPfbOk | kPfxOk | kNDigits);
        ok = true;
        break;
      case '+':
      case '-':
        if (flags & kSignOk) {
          flags &= ~kSignOk;
          flags |= kHaveSign;
          ok = true;
        }
        break;
      case 'x':
      case 'X':
        if ((flags & kPfxOk) && p == buffer + 1 + ((flags & kHaveSign) ? 1 : 0)) {
          *base = 16;
          flags &= ~kPfxOk;
          ok = true;
        }
        break;
      default:
        break;
    }
    if (!ok) break;
    *p++ = static_cast<char>(c);
    in.Consume();
  }
  if (flags & kNDigits) {
    if (p > buffer) in.Unread(static_cast<unsigned char>(*--p));
    return false;
  }
  const char last = p[-1];
  if ((*base == 2 && (last == 'b' || last == 'B')) || last == 'x' || last == 'X') {
    --p;
    in.Unread(static_cast<unsigned char>(last));
  }
  *p = 0;
  *length = static_cast<std::size_t>(p - buffer);
  return true;
}

template <typename ReaderT>
std::size_t ScanFloat(ReaderT& in, std::size_t width, char* buffer) {
  if (width == 0 || width > kNumericBuffer - 1) width = kNumericBuffer - 1;
  enum { kStart, kGotSign, kInf, kNan, kMaybeHex, kDigits, kFrac, kExp, kExpDigits } state = kStart;
  int infnanpos = 0;
  bool gotmantdig = false;
  bool ishex = false;
  std::ptrdiff_t commit = -1;
  std::size_t p = 0;
  while (p < width) {
    const std::int64_t c = in.Peek();
    if (c == kEof) break;
    bool accept = true;
    bool done = false;
    for (bool again = true; again;) {
      again = false;
      switch (state) {
        case kStart:
          state = kGotSign;
          if (c == '-' || c == '+') break;
          again = true;
          break;
        case kGotSign:
          if (c == '0') {
            state = kMaybeHex;
            commit = static_cast<std::ptrdiff_t>(p);
          } else if (c == 'I' || c == 'i') {
            state = kInf;
          } else if (c == 'N' || c == 'n') {
            state = kNan;
          } else {
            state = kDigits;
            again = true;
          }
          break;
        case kInf:
          if (infnanpos > 6 || (c != "nfinity"[infnanpos] && c != "NFINITY"[infnanpos])) {
            done = true;
            break;
          }
          if (infnanpos == 1 || infnanpos == 6) commit = static_cast<std::ptrdiff_t>(p);
          ++infnanpos;
          break;
        case kNan:
          switch (infnanpos) {
            case -1:
              done = true;
              break;
            case 0:
              if (c != 'A' && c != 'a') done = true;
              break;
            case 1:
              if (c != 'N' && c != 'n') {
                done = true;
              } else {
                commit = static_cast<std::ptrdiff_t>(p);
              }
              break;
            case 2:
              if (c != '(') done = true;
              break;
            default:
              if (c == ')') {
                commit = static_cast<std::ptrdiff_t>(p);
                infnanpos = -2;
              } else if (!internal::IsAlnum(c) && c != '_') {
                done = true;
              }
              break;
          }
          if (!done) ++infnanpos;
          break;
        case kMaybeHex:
          state = kDigits;
          if (c == 'X' || c == 'x') {
            ishex = true;
            break;
          }
          gotmantdig = true;
          again = true;
          break;
        case kDigits:
          if ((ishex && internal::IsXdigit(c)) || internal::IsDigit(c)) {
            gotmantdig = true;
          } else {
            state = kFrac;
            if (c != '.') {
              again = true;
              break;
            }
          }
          if (gotmantdig) commit = static_cast<std::ptrdiff_t>(p);
          break;
        case kFrac:
          if (((c == 'E' || c == 'e') && !ishex) || ((c == 'P' || c == 'p') && ishex)) {
            if (!gotmantdig) {
              done = true;
            } else {
              state = kExp;
            }
          } else if ((ishex && internal::IsXdigit(c)) || internal::IsDigit(c)) {
            commit = static_cast<std::ptrdiff_t>(p);
            gotmantdig = true;
          } else {
            done = true;
          }
          break;
        case kExp:
          state = kExpDigits;
          if (c == '-' || c == '+') break;
          again = true;
          break;
        case kExpDigits:
          if (internal::IsDigit(c)) {
            commit = static_cast<std::ptrdiff_t>(p);
          } else {
            done = true;
          }
          break;
      }
      if (done) {
        accept = false;
        break;
      }
    }
    if (!accept) break;
    buffer[p++] = static_cast<char>(c);
    in.Consume();
  }
  while (static_cast<std::ptrdiff_t>(p) - 1 > commit) {
    --p;
    in.Unread(static_cast<unsigned char>(buffer[p]));
  }
  const std::size_t length = static_cast<std::size_t>(commit + 1);
  buffer[length] = 0;
  return length;
}

void StoreFloat(void* target, int flags, const char* text, std::size_t length) {
  internal::FloatKind kind = internal::FloatKind::kFloat;
  if (flags & kLongDbl) {
    kind = internal::FloatKind::kQuad;
  } else if (flags & kLong) {
    kind = internal::FloatKind::kDouble;
  }
  std::uint64_t high = 0;
  std::uint64_t low = 0;
  bool range_error = false;
  internal::ParseFloat(text, length, kind, &high, &low, &range_error);
  if (range_error) SetGuestErrno(guest::kERANGE);
  switch (kind) {
    case internal::FloatKind::kFloat: {
      const auto bits = static_cast<std::uint32_t>(low);
      StoreBytes(target, &bits, 4);
      break;
    }
    case internal::FloatKind::kDouble:
      StoreBytes(target, &low, 8);
      break;
    case internal::FloatKind::kQuad: {
      const abi::GuestLongDouble value{low, high};
      StoreBytes(target, &value, 16);
      break;
    }
  }
}

enum class ScanFault { kNone, kInvalid };

void LogInvalid(const char* what) {
  std::string message = "scanf: ";
  message += what;
  message += "; returning -1 (EINVAL)";
  HostLog(internal::kLogError, "libc", message.c_str());
}

const unsigned char* BuildScanset(bool* table, const unsigned char* fmt) {
  int c = *fmt++;
  bool value;
  if (c == '^') {
    value = true;
    c = *fmt++;
  } else {
    value = false;
  }
  for (int i = 0; i < 256; ++i) table[i] = value;
  if (c == 0) return fmt - 1;
  value = !value;
  for (;;) {
    table[c] = value;
    for (;;) {
      const int n = *fmt++;
      if (n == 0) return fmt - 1;
      if (n == ']') return fmt;
      if (n == '-') {
        const int next = *fmt;
        if (next == ']' || next < c) {
          c = '-';
          break;
        }
        ++fmt;
        do {
          table[++c] = value;
        } while (c < next);
        continue;
      }
      c = n;
      break;
    }
  }
}

int ScanNarrow(Source& source, const char* format, abi::SysvVaList* list) {
  abi::SysvVaCursor args(list);
  Reader<Source> in(source);
  const auto* fmt = reinterpret_cast<const unsigned char*>(format);
  int nassigned = 0;
  int nread = 0;
  bool ccltab[256];
  char buf[kNumericBuffer];
  void* allocation = nullptr;

  auto input_failure = [&]() {
    GuestFree(allocation);
    allocation = nullptr;
    return nassigned == 0 ? -1 : nassigned;
  };

  for (;;) {
    int c = *fmt++;
    if (c == 0) return nassigned;
    if (internal::IsSpace(c)) {
      for (;;) {
        const std::int64_t next = in.Peek();
        if (next == kEof || !internal::IsSpace(static_cast<int>(next))) break;
        in.Consume();
        ++nread;
      }
      continue;
    }
    std::size_t width = 0;
    int flags = 0;
    int base = 10;
    int conversion = -1;
    if (c != '%') {
      const std::int64_t next = in.Peek();
      if (next == kEof) return input_failure();
      if (next != c) return nassigned;
      in.Consume();
      ++nread;
      continue;
    }
    bool literal_percent = false;
    bool skip_rest = false;
    for (bool again = true; again;) {
      again = false;
      c = *fmt++;
      bool reswitch;
      do {
        reswitch = false;
        switch (c) {
          case '%':
            literal_percent = true;
            break;
          case '*':
            flags |= kSuppress;
            again = true;
            break;
          case 'j':
            flags |= kMaxInt;
            again = true;
            break;
          case 'L':
            flags |= kLongDbl;
            again = true;
            break;
          case 'h':
            if (*fmt == 'h') {
              ++fmt;
              flags |= kShortShort;
            } else {
              flags |= kShort;
            }
            again = true;
            break;
          case 'l':
            if (*fmt == 'l') {
              ++fmt;
              flags |= kLLong;
            } else {
              flags |= kLong;
            }
            again = true;
            break;
          case 'm':
            flags |= kAllocate;
            again = true;
            break;
          case 'q':
            flags |= kLLong;
            again = true;
            break;
          case 't':
            flags |= kPtrInt;
            again = true;
            break;
          case 'z':
            flags |= kSizeInt;
            again = true;
            break;
          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
            width = width * 10 + static_cast<std::size_t>(c - '0');
            again = true;
            break;
          case 'b':
            conversion = kCtInt;
            base = 2;
            flags |= kPfbOk;
            break;
          case 'D':
            flags |= kLong;
            [[fallthrough]];
          case 'd':
            conversion = kCtInt;
            base = 10;
            break;
          case 'i':
            conversion = kCtInt;
            base = 0;
            break;
          case 'O':
            flags |= kLong;
            [[fallthrough]];
          case 'o':
            conversion = kCtInt;
            flags |= kUnsigned;
            base = 8;
            break;
          case 'u':
            conversion = kCtInt;
            flags |= kUnsigned;
            base = 10;
            break;
          case 'w': {
            int size = 0;
            bool fast = false;
            c = *fmt++;
            if (c == 'f') {
              fast = true;
              c = *fmt++;
            }
            while (internal::IsDigit(c)) {
              if (size < 1000000) size = size * 10 + (c - '0');
              c = *fmt++;
            }
            const int extra = WidthToFlag(size, fast);
            if (extra < 0) {
              LogInvalid("unsupported %wN size");
              return FailWith(guest::kEINVAL, -1);
            }
            flags |= extra;
            reswitch = true;
            break;
          }
          case 'X':
          case 'x':
            flags |= kPfxOk | kUnsigned;
            conversion = kCtInt;
            base = 16;
            break;
          case 'e':
          case 'E':
          case 'f':
          case 'F':
          case 'g':
          case 'G':
          case 'a':
          case 'A':
            conversion = kCtFloat;
            break;
          case 's':
            for (bool& entry : ccltab) entry = true;
            ccltab['\t'] = ccltab['\n'] = ccltab['\v'] = ccltab['\f'] = ccltab['\r'] = ccltab[' '] = false;
            conversion = kCtString;
            break;
          case '[':
            fmt = BuildScanset(ccltab, fmt);
            flags |= kNoSkip;
            conversion = kCtCcl;
            break;
          case 'c':
            flags |= kNoSkip;
            conversion = kCtChar;
            break;
          case 'p':
            flags |= kPointer | kPfxOk | kUnsigned;
            conversion = kCtInt;
            base = 16;
            break;
          case 'n':
            if (!(flags & kSuppress)) StoreCount(args.NextPointer<void*>(), flags, nread);
            skip_rest = true;
            break;
          case '\0':
            return -1;
          default:
            if (internal::IsUpper(c)) flags |= kLong;
            conversion = kCtInt;
            base = 10;
            break;
        }
      } while (reswitch);
    }
    if (skip_rest) continue;
    if (literal_percent) {
      const std::int64_t next = in.Peek();
      if (next == kEof) return input_failure();
      if (next != '%') return nassigned;
      in.Consume();
      ++nread;
      continue;
    }

    if ((flags & kAllocate) && conversion > kCtString) {
      LogInvalid("'m' only works with %c/%s/%[");
      return FailWith(guest::kEINVAL, -1);
    }
    if ((flags & (kAllocate | kSuppress)) == (kAllocate | kSuppress)) {
      LogInvalid("'m' makes no sense with '*'");
      return FailWith(guest::kEINVAL, -1);
    }

    if (in.Peek() == kEof) return input_failure();
    if ((flags & kNoSkip) == 0) {
      while (internal::IsSpace(static_cast<int>(in.Peek()))) {
        ++nread;
        in.Consume();
        if (in.Peek() == kEof) return input_failure();
      }
    }

    switch (conversion) {
      case kCtChar: {
        if (width == 0) width = 1;
        if (flags & kLong) {
          g_wchar* wcp = nullptr;
          if (flags & kAllocate) {
            allocation = GuestMalloc(width * sizeof(g_wchar));
            if (allocation == nullptr) return input_failure();
            wcp = static_cast<g_wchar*>(allocation);
          } else if (!(flags & kSuppress)) {
            wcp = args.NextPointer<g_wchar*>();
          }
          char bytes[8];
          std::size_t count = 0;
          while (width != 0) {
            if (count == internal::kMbCurMax) return input_failure();
            bytes[count++] = static_cast<char>(in.Peek());
            in.Consume();
            std::uint32_t wc = 0;
            const std::size_t r = internal::DecodeUtf8(bytes, count, &wc);
            if (r == internal::kMbIllegal) return input_failure();
            if (r != internal::kMbIncomplete) {
              if (wcp != nullptr) {
                StoreBytes(wcp, &wc, sizeof(wc));
                ++wcp;
              }
              nread += static_cast<int>(count);
              --width;
              count = 0;
            }
            if (in.Peek() == kEof) {
              if (count != 0) return input_failure();
              break;
            }
          }
          if (allocation != nullptr) {
            *args.NextPointer<g_wchar**>() = static_cast<g_wchar*>(allocation);
            allocation = nullptr;
          }
          if (!(flags & kSuppress)) ++nassigned;
        } else if (flags & kSuppress) {
          std::size_t sum = 0;
          while (sum < width && in.Peek() != kEof) {
            in.Consume();
            ++sum;
          }
          nread += static_cast<int>(sum);
        } else {
          char* p;
          if (flags & kAllocate) {
            allocation = GuestMalloc(width);
            if (allocation == nullptr) return input_failure();
            p = static_cast<char*>(allocation);
          } else {
            p = args.NextPointer<char*>();
          }
          std::size_t count = 0;
          while (count < width && in.Peek() != kEof) {
            p[count++] = static_cast<char>(in.Peek());
            in.Consume();
          }
          if (count == 0) return input_failure();
          if (allocation != nullptr) {
            *args.NextPointer<char**>() = static_cast<char*>(allocation);
            allocation = nullptr;
          }
          nread += static_cast<int>(count);
          ++nassigned;
        }
        break;
      }

      case kCtCcl:
      case kCtString: {
        if (width == 0) width = SIZE_MAX;
        std::size_t n = 0;
        char* p = nullptr;
        g_wchar* wcp = nullptr;
        if (flags & kLong) {
          std::size_t capacity = 0;
          if (flags & kAllocate) {
            capacity = width < 32 ? width : 32;
            allocation = GuestMalloc(sizeof(g_wchar) * capacity);
            if (allocation == nullptr) return input_failure();
            wcp = static_cast<g_wchar*>(allocation);
          } else if (!(flags & kSuppress)) {
            wcp = args.NextPointer<g_wchar*>();
          }
          char bytes[8];
          std::size_t count = 0;
          while ((conversion == kCtCcl || !internal::IsSpace(static_cast<int>(in.Peek()))) && width != 0) {
            if (count == internal::kMbCurMax) return input_failure();
            bytes[count++] = static_cast<char>(in.Peek());
            in.Consume();
            std::uint32_t wc = 0;
            const std::size_t r = internal::DecodeUtf8(bytes, count, &wc);
            if (r == internal::kMbIllegal) return input_failure();
            if (r != internal::kMbIncomplete) {
              const bool reject = (conversion == kCtCcl && wc < 0x80 && !ccltab[wc]) ||
                                  (conversion == kCtString && internal::IsWideSpace(wc));
              if (reject) {
                while (count != 0) in.Unread(static_cast<unsigned char>(bytes[--count]));
                break;
              }
              if (wcp != nullptr) StoreBytes(wcp + n, &wc, sizeof(wc));
              ++n;
              if (allocation != nullptr && n == capacity) {
                capacity *= 2;
                void* grown = GuestRealloc(allocation, sizeof(g_wchar) * capacity);
                if (grown == nullptr) return input_failure();
                allocation = grown;
                wcp = static_cast<g_wchar*>(grown);
              }
              nread += static_cast<int>(count);
              --width;
              count = 0;
            }
            if (in.Peek() == kEof) {
              if (count != 0) return input_failure();
              break;
            }
          }
          if (conversion == kCtCcl && count != 0) return input_failure();
          if (allocation != nullptr) {
            *args.NextPointer<g_wchar**>() = static_cast<g_wchar*>(allocation);
            allocation = nullptr;
          }
        } else if (flags & kSuppress) {
          for (;;) {
            const std::int64_t next = in.Peek();
            if (next == kEof || !ccltab[next]) break;
            ++n;
            in.Consume();
            if (--width == 0) break;
          }
          nread += static_cast<int>(n);
        } else {
          std::size_t capacity = 0;
          if (flags & kAllocate) {
            capacity = width < 32 ? width : 32;
            allocation = GuestMalloc(capacity);
            if (allocation == nullptr) return input_failure();
            p = static_cast<char*>(allocation);
          } else {
            p = args.NextPointer<char*>();
          }
          for (;;) {
            const std::int64_t next = in.Peek();
            if (next == kEof || !ccltab[next]) break;
            in.Consume();
            p[n++] = static_cast<char>(next);
            if (allocation != nullptr && n == capacity) {
              capacity *= 2;
              void* grown = GuestRealloc(allocation, capacity);
              if (grown == nullptr) return input_failure();
              allocation = grown;
              p = static_cast<char*>(grown);
            }
            if (--width == 0) break;
          }
          nread += static_cast<int>(n);
          if (allocation != nullptr) {
            *args.NextPointer<char**>() = static_cast<char*>(allocation);
            allocation = nullptr;
          }
        }
        if (conversion == kCtCcl && n == 0) return nassigned;
        if (!(flags & kSuppress)) {
          if (flags & kLong) {
            const g_wchar zero = 0;
            StoreBytes(wcp + n, &zero, sizeof(zero));
          } else {
            p[n] = 0;
          }
          ++nassigned;
        }
        break;
      }

      case kCtInt: {
        std::size_t length = 0;
        if (!ScanInteger(in, width, flags, &base, buf, &length)) return nassigned;
        if (!(flags & kSuppress)) {
          bool range_error = false;
          const std::uint64_t value = ConvertInteger(buf, base, (flags & kUnsigned) != 0, &range_error);
          if (range_error) SetGuestErrno(guest::kERANGE);
          StoreInteger(args.NextPointer<void*>(), flags, value);
          ++nassigned;
        }
        nread += static_cast<int>(length);
        break;
      }

      case kCtFloat: {
        const std::size_t length = ScanFloat(in, width, buf);
        if (length == 0) return nassigned;
        if (!(flags & kSuppress)) {
          StoreFloat(args.NextPointer<void*>(), flags, buf, length);
          ++nassigned;
        }
        nread += static_cast<int>(length);
        break;
      }

      default:
        break;
    }
  }
}

bool InScanset(g_wchar wc, const g_wchar* ccl) {
  bool member_result = true;
  if (*ccl == '^') {
    member_result = false;
    ++ccl;
  }
  if (*ccl == '-' || *ccl == ']') {
    if (*ccl == wc) return member_result;
    ++ccl;
  }
  while (*ccl != 0 && *ccl != ']') {
    if (*ccl == '-' && ccl[1] != 0 && ccl[1] != ']') {
      const g_wchar first = ccl[-1];
      const g_wchar last = ccl[1];
      if (first <= last) {
        if (wc >= first && wc <= last) return member_result;
        ccl += 2;
        continue;
      }
    }
    if (*ccl == wc) return member_result;
    ++ccl;
  }
  return !member_result;
}

int ScanWideImpl(WideSource& source, const g_wchar* fmt, abi::SysvVaList* list) {
  abi::SysvVaCursor args(list);
  Reader<WideSource> in(source);
  int nassigned = 0;
  int nconversions = 0;
  int nread = 0;
  char buf[kNumericBuffer];

  auto input_failure = [&]() { return nconversions != 0 ? nassigned : -1; };

  for (;;) {
    g_wchar c = *fmt++;
    if (c == 0) return nassigned;
    if (internal::IsWideSpace(c)) {
      while (internal::IsWideSpace(in.Peek())) in.Consume();
      continue;
    }
    std::size_t width = 0;
    int flags = 0;
    int base = 10;
    int conversion = -1;
    const g_wchar* ccl = nullptr;
    bool literal = c != '%';
    bool skip_rest = false;
    if (!literal) {
      for (bool again = true; again;) {
        again = false;
        c = *fmt++;
        bool reswitch;
        do {
          reswitch = false;
          switch (c) {
            case '%':
              literal = true;
              break;
            case '*':
              flags |= kSuppress;
              again = true;
              break;
            case 'j':
              flags |= kMaxInt;
              again = true;
              break;
            case 'L':
              flags |= kLongDbl;
              again = true;
              break;
            case 'h':
              if (*fmt == 'h') {
                ++fmt;
                flags |= kShortShort;
              } else {
                flags |= kShort;
              }
              again = true;
              break;
            case 'l':
              if (*fmt == 'l') {
                ++fmt;
                flags |= kLLong;
              } else {
                flags |= kLong;
              }
              again = true;
              break;
            case 'q':
              flags |= kLLong;
              again = true;
              break;
            case 't':
              flags |= kPtrInt;
              again = true;
              break;
            case 'z':
              flags |= kSizeInt;
              again = true;
              break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
              width = width * 10 + (c - '0');
              again = true;
              break;
            case 'b':
              conversion = kCtInt;
              base = 2;
              flags |= kPfbOk;
              break;
            case 'D':
              flags |= kLong;
              [[fallthrough]];
            case 'd':
              conversion = kCtInt;
              base = 10;
              break;
            case 'i':
              conversion = kCtInt;
              base = 0;
              break;
            case 'O':
              flags |= kLong;
              [[fallthrough]];
            case 'o':
              conversion = kCtInt;
              flags |= kUnsigned;
              base = 8;
              break;
            case 'u':
              conversion = kCtInt;
              flags |= kUnsigned;
              base = 10;
              break;
            case 'w': {
              int size = 0;
              bool fast = false;
              c = *fmt++;
              if (c == 'f') {
                fast = true;
                c = *fmt++;
              }
              while (internal::IsDigit(c)) {
                if (size < 1000000) size = size * 10 + static_cast<int>(c - '0');
                c = *fmt++;
              }
              const int extra = WidthToFlag(size, fast);
              if (extra < 0) {
                LogInvalid("unsupported %wN size");
                return FailWith(guest::kEINVAL, -1);
              }
              flags |= extra;
              reswitch = true;
              break;
            }
            case 'X':
            case 'x':
              flags |= kPfxOk | kUnsigned;
              conversion = kCtInt;
              base = 16;
              break;
            case 'e':
            case 'E':
            case 'f':
            case 'F':
            case 'g':
            case 'G':
            case 'a':
            case 'A':
              conversion = kCtFloat;
              break;
            case 's':
              conversion = kCtString;
              break;
            case '[':
              ccl = fmt;
              if (*fmt == '^') ++fmt;
              if (*fmt == ']') ++fmt;
              while (*fmt != 0 && *fmt != ']') ++fmt;
              if (*fmt == ']') ++fmt;
              flags |= kNoSkip;
              conversion = kCtCcl;
              break;
            case 'c':
              flags |= kNoSkip;
              conversion = kCtChar;
              break;
            case 'p':
              flags |= kPointer | kPfxOk | kUnsigned;
              conversion = kCtInt;
              base = 16;
              break;
            case 'n':
              ++nconversions;
              if (!(flags & kSuppress)) StoreCount(args.NextPointer<void*>(), flags, nread);
              skip_rest = true;
              break;
            case 0:
              return -1;
            default:
              if (internal::IsUpper(c)) flags |= kLong;
              conversion = kCtInt;
              base = 10;
              break;
          }
        } while (reswitch);
      }
    }
    if (skip_rest) continue;
    if (literal) {
      const std::int64_t next = in.Next();
      if (next == kEof) return input_failure();
      if (next != c) {
        in.Unread(next);
        return nassigned;
      }
      ++nread;
      continue;
    }

    if ((flags & kNoSkip) == 0) {
      while (internal::IsWideSpace(in.Peek())) {
        in.Consume();
        ++nread;
      }
      if (in.Peek() == kEof) return input_failure();
    }

    switch (conversion) {
      case kCtChar: {
        if (width == 0) width = 1;
        int n = 0;
        if (flags & kLong) {
          g_wchar* p = (flags & kSuppress) ? nullptr : args.NextPointer<g_wchar*>();
          while (width-- != 0) {
            const std::int64_t wi = in.Next();
            if (wi == kEof) break;
            if (p != nullptr) {
              const auto value = static_cast<g_wchar>(wi);
              StoreBytes(p++, &value, sizeof(value));
            }
            ++n;
          }
        } else {
          char* mbp = (flags & kSuppress) ? nullptr : args.NextPointer<char*>();
          while (width != 0) {
            const std::int64_t wi = in.Next();
            if (wi == kEof) break;
            char encoded[8];
            const std::size_t length = internal::EncodeUtf8(static_cast<std::uint32_t>(wi), encoded);
            if (length == internal::kMbIllegal) return input_failure();
            if (length > width) {
              in.Unread(wi);
              break;
            }
            if (mbp != nullptr) {
              std::memcpy(mbp, encoded, length);
              mbp += length;
            }
            width -= length;
            ++n;
          }
        }
        if (n == 0) return input_failure();
        nread += n;
        if (!(flags & kSuppress)) ++nassigned;
        ++nconversions;
        break;
      }

      case kCtCcl:
      case kCtString: {
        if (width == 0) width = SIZE_MAX;
        auto matches = [&](std::int64_t wi) {
          return conversion == kCtCcl ? InScanset(static_cast<g_wchar>(wi), ccl) : !internal::IsWideSpace(wi);
        };
        int n = 0;
        g_wchar* p = nullptr;
        char* mbp = nullptr;
        if (flags & kLong) {
          if (!(flags & kSuppress)) p = args.NextPointer<g_wchar*>();
          for (;;) {
            const std::int64_t wi = in.Peek();
            if (wi == kEof || width == 0 || !matches(wi)) break;
            --width;
            in.Consume();
            if (p != nullptr) {
              const auto value = static_cast<g_wchar>(wi);
              StoreBytes(p + n, &value, sizeof(value));
            }
            ++n;
          }
        } else {
          if (!(flags & kSuppress)) mbp = args.NextPointer<char*>();
          for (;;) {
            const std::int64_t wi = in.Peek();
            if (wi == kEof || width == 0 || !matches(wi)) break;
            char encoded[8];
            const std::size_t length = internal::EncodeUtf8(static_cast<std::uint32_t>(wi), encoded);
            if (length == internal::kMbIllegal) return input_failure();
            if (length > width) break;
            in.Consume();
            if (mbp != nullptr) {
              std::memcpy(mbp, encoded, length);
              mbp += length;
            }
            width -= length;
            ++n;
          }
        }
        if (conversion == kCtCcl && n == 0) return nassigned;
        if (!(flags & kSuppress)) {
          if (flags & kLong) {
            const g_wchar zero = 0;
            StoreBytes(p + n, &zero, sizeof(zero));
          } else {
            *mbp = 0;
          }
          ++nassigned;
        }
        nread += n;
        ++nconversions;
        break;
      }

      case kCtInt: {
        std::size_t length = 0;
        if (!ScanInteger(in, width, flags, &base, buf, &length)) return nassigned;
        if (!(flags & kSuppress)) {
          bool range_error = false;
          const std::uint64_t value = ConvertInteger(buf, base, (flags & kUnsigned) != 0, &range_error);
          if (range_error) SetGuestErrno(guest::kERANGE);
          StoreInteger(args.NextPointer<void*>(), flags, value);
          ++nassigned;
        }
        nread += static_cast<int>(length);
        ++nconversions;
        break;
      }

      case kCtFloat: {
        const std::size_t length = ScanFloat(in, width, buf);
        if (length == 0) return nassigned;
        if (!(flags & kSuppress)) {
          StoreFloat(args.NextPointer<void*>(), flags, buf, length);
          ++nassigned;
        }
        nread += static_cast<int>(length);
        ++nconversions;
        break;
      }

      default:
        break;
    }
  }
}

}

namespace {

abi::SysvVaList* EmptyVaList(abi::SysvVaList* storage) {
  alignas(16) static const unsigned char kZeros[1024] = {};
  storage->gp_offset = abi::kSysvGpSaveBytes;
  storage->fp_offset = abi::kSysvFpSaveEnd;
  storage->overflow_arg_area = const_cast<unsigned char*>(kZeros);
  storage->reg_save_area = const_cast<unsigned char*>(kZeros);
  return storage;
}

}

int Scan(Source& source, const char* format, abi::SysvVaList* args) noexcept {
  if (format == nullptr) return FailWith(guest::kEINVAL, -1);
  abi::SysvVaList empty_list;
  if (args == nullptr) args = EmptyVaList(&empty_list);
  try {
    return ScanNarrow(source, format, args);
  } catch (...) {
    return FailWith(guest::kENOMEM, -1);
  }
}

int ScanWide(WideSource& source, const g_wchar* format, abi::SysvVaList* args) noexcept {
  if (format == nullptr) return FailWith(guest::kEINVAL, -1);
  abi::SysvVaList empty_list;
  if (args == nullptr) args = EmptyVaList(&empty_list);
  try {
    return ScanWideImpl(source, format, args);
  } catch (...) {
    return FailWith(guest::kENOMEM, -1);
  }
}

}
