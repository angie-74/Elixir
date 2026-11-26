// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The conversion logic follows AOSP bionic libc/stdio/vfprintf.cpp,
// vfwprintf.cpp and printf_common.h, which carry this notice:
//
//   Copyright (c) 1990 The Regents of the University of California.
//   All rights reserved.
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
#include <vector>

#include "bionic/format.h"
#include "bionic/format/format_internal.h"
#include "bionic/guest_constants.h"
#include "bionic/runtime.h"

namespace elixir::bionic::format {
namespace {

using abi::g_wchar;
using internal::DecodedFloat;

constexpr int kAlt = 0x0001;
constexpr int kLadjust = 0x0004;
constexpr int kLongDbl = 0x0008;
constexpr int kLongInt = 0x0010;
constexpr int kLLongInt = 0x0020;
constexpr int kShortInt = 0x0040;
constexpr int kZeroPad = 0x0080;
constexpr int kFpt = 0x0100;
constexpr int kPtrInt = 0x0200;
constexpr int kSizeInt = 0x0400;
constexpr int kCharInt = 0x0800;
constexpr int kMaxInt = 0x1000;
constexpr int kWideInt = kMaxInt | kLLongInt | kLongInt | kPtrInt | kSizeInt;

constexpr int kDefaultPrecision = 6;
constexpr int kMaxPositionalArgs = 65536;
constexpr int kNumberBuffer = 100;

enum class Failure { kNone, kOverflow, kInvalid, kIllegalSequence, kPercentN, kSink };

enum class ArgClass : std::uint8_t { kUnused, kGp, kDouble, kLongDouble };

template <typename CharT>
constexpr bool IsDigitChar(CharT c) {
  return c >= '0' && c <= '9';
}

template <typename CharT>
bool AppendDigit(int* value, CharT digit) {
  if (*value > INT_MAX / 10) return false;
  *value *= 10;
  const int d = static_cast<int>(digit - '0');
  if (*value > INT_MAX - d) return false;
  *value += d;
  return true;
}

int WidthToFlag(int size, bool fast) {
  if (size == 8) return kCharInt;
  if (size == 16) return fast ? kLLongInt : kShortInt;
  if (size == 32) return fast ? kLLongInt : 0;
  if (size == 64) return kLLongInt;
  return -1;
}

struct ArgPlan {
  static constexpr int kInline = 32;
  bool positional = false;
  int max_index = 0;
  ArgClass inline_types[kInline] = {};
  std::vector<ArgClass> more_types;

  ArgClass Get(int index) const {
    if (index < kInline) return inline_types[index];
    const auto slot = static_cast<std::size_t>(index - kInline);
    return slot < more_types.size() ? more_types[slot] : ArgClass::kUnused;
  }
  void Set(int index, ArgClass type) {
    if (index < kInline) {
      inline_types[index] = type;
      return;
    }
    const auto slot = static_cast<std::size_t>(index - kInline);
    if (slot >= more_types.size()) more_types.resize(slot + 1 + kInline, ArgClass::kUnused);
    more_types[slot] = type;
  }
};

class TypeRecorder {
 public:
  explicit TypeRecorder(ArgPlan* plan) : plan_(plan) {}
  bool Add(ArgClass type) {
    if (next_ < 1 || next_ > kMaxPositionalArgs) return false;
    plan_->Set(next_, type);
    if (next_ > plan_->max_index) plan_->max_index = next_;
    ++next_;
    return true;
  }
  int next_ = 1;

 private:
  ArgPlan* plan_;
};

template <typename CharT>
Failure FindArguments(const CharT* fmt, ArgPlan* plan) {
  TypeRecorder rec(plan);
  auto add_aster = [&](const CharT** cursor) -> Failure {
    int n2 = 0;
    const CharT* cp = *cursor;
    while (IsDigitChar(*cp)) {
      if (!AppendDigit(&n2, *cp)) return Failure::kOverflow;
      ++cp;
    }
    if (*cp == '$') {
      plan->positional = true;
      const int hold = rec.next_;
      rec.next_ = n2;
      if (!rec.Add(ArgClass::kGp)) return Failure::kInvalid;
      rec.next_ = hold;
      *cursor = cp + 1;
    } else if (!rec.Add(ArgClass::kGp)) {
      return Failure::kInvalid;
    }
    return Failure::kNone;
  };

  for (;;) {
    while (*fmt != 0 && *fmt != '%') ++fmt;
    if (*fmt == 0) return Failure::kNone;
    ++fmt;
    int flags = 0;
    CharT ch = *fmt++;
    for (;;) {
      bool consumed = false;
      bool reswitch = false;
      switch (ch) {
        case ' ':
        case '#':
        case '\'':
        case '-':
        case '+':
        case '0':
          break;
        case '*': {
          const Failure f = add_aster(&fmt);
          if (f != Failure::kNone) return f;
          break;
        }
        case '.':
          ch = *fmt++;
          if (ch == '*') {
            const Failure f = add_aster(&fmt);
            if (f != Failure::kNone) return f;
            break;
          }
          while (IsDigitChar(ch)) ch = *fmt++;
          reswitch = true;
          break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
          int n = 0;
          do {
            if (!AppendDigit(&n, ch)) return Failure::kOverflow;
            ch = *fmt++;
          } while (IsDigitChar(ch));
          if (ch == '$') {
            plan->positional = true;
            rec.next_ = n;
            break;
          }
          reswitch = true;
          break;
        }
        case 'L':
          flags |= kLongDbl;
          break;
        case 'h':
          if (*fmt == 'h') {
            ++fmt;
            flags |= kCharInt;
          } else {
            flags |= kShortInt;
          }
          break;
        case 'j':
          flags |= kMaxInt;
          break;
        case 'l':
          if (*fmt == 'l') {
            ++fmt;
            flags |= kLLongInt;
          } else {
            flags |= kLongInt;
          }
          break;
        case 'q':
          flags |= kLLongInt;
          break;
        case 't':
          flags |= kPtrInt;
          break;
        case 'z':
          flags |= kSizeInt;
          break;
        case 'w': {
          int n = 0;
          bool fast = false;
          ch = *fmt++;
          if (ch == 'f') {
            fast = true;
            ch = *fmt++;
          }
          while (IsDigitChar(ch)) {
            if (!AppendDigit(&n, ch)) return Failure::kOverflow;
            ch = *fmt++;
          }
          const int extra = WidthToFlag(n, fast);
          if (extra < 0) return Failure::kInvalid;
          flags |= extra;
          reswitch = true;
          break;
        }
        case 'a':
        case 'A':
        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
          if (!rec.Add((flags & kLongDbl) != 0 ? ArgClass::kLongDouble : ArgClass::kDouble)) {
            return Failure::kInvalid;
          }
          consumed = true;
          break;
        case 'n':
          return Failure::kPercentN;
        case 'C':
        case 'c':
        case 'D':
        case 'd':
        case 'i':
        case 'O':
        case 'o':
        case 'p':
        case 'S':
        case 's':
        case 'U':
        case 'u':
        case 'X':
        case 'x':
        case 'B':
        case 'b':
          if (!rec.Add(ArgClass::kGp)) return Failure::kInvalid;
          consumed = true;
          break;
        default:
          if (ch == 0) return Failure::kNone;
          consumed = true;
          break;
      }
      if (consumed) break;
      if (!reswitch) ch = *fmt++;
    }
  }
}

struct alignas(16) ArgSlot {
  unsigned char bytes[16];
};

class Arguments {
 public:
  explicit Arguments(abi::SysvVaList* list) : list_(list), cursor_(list) {}

  void BuildTable(const ArgPlan& plan) {
    table_.assign(static_cast<std::size_t>(plan.max_index) + 1, ArgSlot{});
    abi::SysvVaCursor cursor(list_);
    for (int n = 1; n <= plan.max_index; ++n) {
      ArgSlot& slot = table_[static_cast<std::size_t>(n)];
      switch (plan.Get(n)) {
        case ArgClass::kDouble: {
          const double value = cursor.NextDouble();
          std::memcpy(slot.bytes, &value, sizeof(value));
          break;
        }
        case ArgClass::kLongDouble: {
          const abi::GuestLongDouble value = cursor.NextLongDouble();
          std::memcpy(slot.bytes, &value, sizeof(value));
          break;
        }
        case ArgClass::kUnused:
        case ArgClass::kGp:
        default: {
          const std::uint64_t value = cursor.NextGp();
          std::memcpy(slot.bytes, &value, sizeof(value));
          break;
        }
      }
    }
    positional_ = true;
  }

  bool bad() const { return bad_; }

  std::uint64_t Gp() {
    if (positional_) {
      std::uint64_t value = 0;
      if (const ArgSlot* slot = Slot()) std::memcpy(&value, slot->bytes, sizeof(value));
      ++next_;
      return value;
    }
    ++next_;
    return cursor_.NextGp();
  }

  double Double() {
    if (positional_) {
      double value = 0;
      if (const ArgSlot* slot = Slot()) std::memcpy(&value, slot->bytes, sizeof(value));
      ++next_;
      return value;
    }
    ++next_;
    return cursor_.NextDouble();
  }

  abi::GuestLongDouble LongDouble() {
    if (positional_) {
      abi::GuestLongDouble value{};
      if (const ArgSlot* slot = Slot()) std::memcpy(&value, slot->bytes, sizeof(value));
      ++next_;
      return value;
    }
    ++next_;
    return cursor_.NextLongDouble();
  }

  int next_ = 1;

 private:
  const ArgSlot* Slot() {
    if (next_ >= 1 && static_cast<std::size_t>(next_) < table_.size()) return &table_[static_cast<std::size_t>(next_)];
    bad_ = true;
    return nullptr;
  }

  abi::SysvVaList* list_;
  abi::SysvVaCursor cursor_;
  std::vector<ArgSlot> table_;
  bool positional_ = false;
  bool bad_ = false;
};

template <typename CharT, typename SinkT>
class Writer {
 public:
  explicit Writer(SinkT& sink) : sink_(sink) {}

  bool ok() const { return ok_; }

  void Write(const CharT* data, std::size_t length) {
    if (!ok_ || length == 0) return;
    if (length > kCapacity - used_) {
      Flush();
      if (!ok_) return;
      if (length >= kCapacity) {
        ok_ = sink_.Write(data, length);
        return;
      }
    }
    std::memcpy(buffer_ + used_, data, length * sizeof(CharT));
    used_ += length;
  }

  void Put(CharT c) { Write(&c, 1); }

  void WriteAscii(const char* text, std::size_t length) {
    if constexpr (sizeof(CharT) == 1) {
      Write(text, length);
    } else {
      for (std::size_t i = 0; i < length; ++i) Put(static_cast<CharT>(static_cast<unsigned char>(text[i])));
    }
  }

  void Pad(CharT c, long long count) {
    while (ok_ && count > 0) {
      if (used_ == kCapacity) Flush();
      std::size_t chunk = kCapacity - used_;
      if (static_cast<long long>(chunk) > count) chunk = static_cast<std::size_t>(count);
      for (std::size_t i = 0; i < chunk; ++i) buffer_[used_ + i] = c;
      used_ += chunk;
      count -= static_cast<long long>(chunk);
    }
  }

  bool Flush() {
    if (ok_ && used_ != 0) ok_ = sink_.Write(buffer_, used_);
    used_ = 0;
    return ok_;
  }

 private:
  static constexpr std::size_t kCapacity = 512;
  SinkT& sink_;
  CharT buffer_[kCapacity];
  std::size_t used_ = 0;
  bool ok_ = true;
};

template <typename CharT>
std::size_t StringLength(const CharT* text, long long limit) {
  std::size_t n = 0;
  if (limit < 0) {
    while (text[n] != 0) ++n;
  } else {
    while (static_cast<long long>(n) < limit && text[n] != 0) ++n;
  }
  return n;
}

int WriteExponent(char* out, int exp, char marker) {
  char* p = out;
  *p++ = marker;
  if (exp < 0) {
    exp = -exp;
    *p++ = '-';
  } else {
    *p++ = '+';
  }
  if (exp > 9) {
    char digits[16];
    int n = 0;
    do {
      digits[n++] = static_cast<char>('0' + exp % 10);
      exp /= 10;
    } while (exp > 0);
    while (n > 0) *p++ = digits[--n];
  } else {
    if (marker == 'e' || marker == 'E') *p++ = '0';
    *p++ = static_cast<char>('0' + exp);
  }
  return static_cast<int>(p - out);
}

bool WideToMultibyte(const g_wchar* wide, long long prec, std::string* out) {
  out->clear();
  char buffer[8];
  for (std::size_t i = 0;; ++i) {
    const g_wchar wc = wide[i];
    if (wc == 0) return true;
    if (prec >= 0 && static_cast<long long>(out->size()) >= prec) return true;
    const std::size_t n = internal::EncodeUtf8(wc, buffer);
    if (n == internal::kMbIllegal) return false;
    if (prec >= 0 && static_cast<long long>(out->size() + n) > prec) return true;
    out->append(buffer, n);
  }
}

bool MultibyteToWide(const char* text, long long prec, std::basic_string<g_wchar>* out) {
  out->clear();
  std::size_t position = 0;
  for (;;) {
    if (prec >= 0 && static_cast<long long>(out->size()) == prec) return true;
    std::uint32_t wc = 0;
    const std::size_t r = internal::DecodeUtf8(text + position, internal::kMbCurMax, &wc);
    if (r == internal::kMbIllegal || r == internal::kMbIncomplete) return false;
    if (r == 0) return true;
    out->push_back(wc);
    position += r;
  }
}

template <typename CharT>
const CharT* NullString() {
  static const CharT kText[] = {'(', 'n', 'u', 'l', 'l', ')', 0};
  return kText;
}

abi::SysvVaList* EmptyVaList(abi::SysvVaList* storage) {
  alignas(16) static const unsigned char kZeros[1024] = {};
  storage->gp_offset = abi::kSysvGpSaveBytes;
  storage->fp_offset = abi::kSysvFpSaveEnd;
  storage->overflow_arg_area = const_cast<unsigned char*>(kZeros);
  storage->reg_save_area = const_cast<unsigned char*>(kZeros);
  return storage;
}

template <typename CharT, typename SinkT>
int FormatImpl(SinkT& sink, const CharT* fmt0, abi::SysvVaList* list) noexcept {
  const int caller_errno = GuestErrno();
  if (fmt0 == nullptr) return FailWith(guest::kEINVAL, -1);
  abi::SysvVaList empty_list;
  if (list == nullptr) list = EmptyVaList(&empty_list);
  Failure failure = Failure::kNone;
  int ret = 0;
  try {
    ArgPlan plan;
    failure = FindArguments(fmt0, &plan);
    if (failure == Failure::kNone) {
      Arguments args(list);
      if (plan.positional) args.BuildTable(plan);
      Writer<CharT, SinkT> out(sink);

      static const char kLowerDigits[] = "0123456789abcdef";
      static const char kUpperDigits[] = "0123456789ABCDEF";

      const CharT* fmt = fmt0;
      std::string dtoa;
      std::string narrow_conv;
      std::basic_string<g_wchar> wide_conv;
      for (;;) {
        const CharT* literal = fmt;
        while (*fmt != 0 && *fmt != '%') ++fmt;
        if (fmt != literal) {
          const long long m = fmt - literal;
          if (m > INT_MAX - ret) {
            failure = Failure::kOverflow;
            break;
          }
          out.Write(literal, static_cast<std::size_t>(m));
          ret += static_cast<int>(m);
        }
        if (*fmt == 0) break;
        ++fmt;

        int flags = 0;
        int dprec = 0;
        int width = 0;
        int prec = -1;
        CharT sign = 0;
        CharT ox1 = 0;
        const char* xdigs = kLowerDigits;
        std::uint64_t umax = 0;
        int base = 10;
        CharT buf[kNumberBuffer];
        const CharT* cp = nullptr;
        long long size = 0;
        char expchar = 0;
        int expt = 0;
        int ndig = 0;
        int lead = 0;
        char expstr[16];
        int expsize = 0;
        bool signflag = false;
        bool stop = false;

        auto get_aster = [&](int* value) -> Failure {
          int n2 = 0;
          const CharT* p = fmt;
          while (IsDigitChar(*p)) {
            if (!AppendDigit(&n2, *p)) return Failure::kOverflow;
            ++p;
          }
          if (*p == '$') {
            const int hold = args.next_;
            args.next_ = n2;
            *value = static_cast<int>(args.Gp());
            args.next_ = hold;
            fmt = p + 1;
          } else {
            *value = static_cast<int>(args.Gp());
          }
          return args.bad() ? Failure::kInvalid : Failure::kNone;
        };

        CharT ch = *fmt++;
        for (;;) {
          bool reswitch = false;
          bool conversion = false;
          switch (ch) {
            case ' ':
              if (sign == 0) sign = ' ';
              break;
            case '#':
              flags |= kAlt;
              break;
            case '\'':
              break;
            case '*': {
              int value = 0;
              failure = get_aster(&value);
              if (failure != Failure::kNone) break;
              if (value >= 0) {
                width = value;
                break;
              }
              if (value == INT_MIN) {
                failure = Failure::kOverflow;
                break;
              }
              width = -value;
              flags |= kLadjust;
              break;
            }
            case '-':
              flags |= kLadjust;
              break;
            case '+':
              sign = '+';
              break;
            case '.': {
              ch = *fmt++;
              if (ch == '*') {
                int value = 0;
                failure = get_aster(&value);
                if (failure != Failure::kNone) break;
                prec = value < 0 ? -1 : value;
                break;
              }
              int n = 0;
              while (IsDigitChar(ch)) {
                if (!AppendDigit(&n, ch)) {
                  failure = Failure::kOverflow;
                  break;
                }
                ch = *fmt++;
              }
              prec = n;
              reswitch = true;
              break;
            }
            case '0':
              flags |= kZeroPad;
              break;
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9': {
              int n = 0;
              do {
                if (!AppendDigit(&n, ch)) {
                  failure = Failure::kOverflow;
                  break;
                }
                ch = *fmt++;
              } while (IsDigitChar(ch));
              if (failure != Failure::kNone) break;
              if (ch == '$') {
                args.next_ = n;
                break;
              }
              width = n;
              reswitch = true;
              break;
            }
            case 'L':
              flags |= kLongDbl;
              break;
            case 'h':
              if (*fmt == 'h') {
                ++fmt;
                flags |= kCharInt;
              } else {
                flags |= kShortInt;
              }
              break;
            case 'j':
              flags |= kMaxInt;
              break;
            case 'l':
              if (*fmt == 'l') {
                ++fmt;
                flags |= kLLongInt;
              } else {
                flags |= kLongInt;
              }
              break;
            case 'q':
              flags |= kLLongInt;
              break;
            case 't':
              flags |= kPtrInt;
              break;
            case 'z':
              flags |= kSizeInt;
              break;
            case 'w': {
              int n = 0;
              bool fast = false;
              ch = *fmt++;
              if (ch == 'f') {
                fast = true;
                ch = *fmt++;
              }
              while (IsDigitChar(ch)) {
                if (!AppendDigit(&n, ch)) {
                  failure = Failure::kOverflow;
                  break;
                }
                ch = *fmt++;
              }
              const int extra = WidthToFlag(n, fast);
              if (extra < 0) {
                failure = Failure::kInvalid;
                break;
              }
              flags |= extra;
              reswitch = true;
              break;
            }
            default:
              conversion = true;
              break;
          }
          if (failure != Failure::kNone || conversion) break;
          if (!reswitch) ch = *fmt++;
        }
        if (failure != Failure::kNone) break;

        auto sarg = [&]() -> std::int64_t {
          const std::uint64_t raw = args.Gp();
          if (flags & kWideInt) return static_cast<std::int64_t>(raw);
          if (flags & kShortInt) return static_cast<short>(raw);
          if (flags & kCharInt) return static_cast<signed char>(raw);
          return static_cast<int>(raw);
        };
        auto uarg = [&]() -> std::uint64_t {
          const std::uint64_t raw = args.Gp();
          if (flags & kWideInt) return raw;
          if (flags & kShortInt) return static_cast<unsigned short>(raw);
          if (flags & kCharInt) return static_cast<unsigned char>(raw);
          return static_cast<unsigned int>(raw);
        };
        auto set_string = [&](const CharT* text) {
          cp = text;
          size = static_cast<long long>(StringLength(text, prec));
          sign = 0;
        };

        enum class Kind { kText, kNumber, kFloat } kind = Kind::kText;
        bool signed_decimal = false;
        switch (ch) {
          case 'B':
          case 'b':
            umax = uarg();
            base = 2;
            if ((flags & kAlt) && umax != 0) ox1 = ch;
            sign = 0;
            kind = Kind::kNumber;
            break;
          case 'C':
            flags |= kLongInt;
            [[fallthrough]];
          case 'c':
            if constexpr (sizeof(CharT) == 1) {
              if (flags & kLongInt) {
                char encoded[8];
                const std::size_t n = internal::EncodeUtf8(static_cast<std::uint32_t>(args.Gp()), encoded);
                if (n == internal::kMbIllegal) {
                  failure = Failure::kIllegalSequence;
                  break;
                }
                std::memcpy(buf, encoded, n);
                cp = buf;
                size = static_cast<long long>(n);
              } else {
                buf[0] = static_cast<CharT>(static_cast<int>(args.Gp()));
                cp = buf;
                size = 1;
              }
            } else {
              if (flags & kLongInt) {
                buf[0] = static_cast<CharT>(static_cast<std::uint32_t>(args.Gp()));
              } else {
                const int c = static_cast<int>(args.Gp());
                const auto byte = static_cast<unsigned char>(static_cast<char>(c));
                buf[0] = (c == -1 || byte >= 0x80) ? static_cast<CharT>(0xffffffffu) : static_cast<CharT>(byte);
              }
              cp = buf;
              size = 1;
            }
            sign = 0;
            break;
          case 'D':
            flags |= kLongInt;
            [[fallthrough]];
          case 'd':
          case 'i':
            umax = static_cast<std::uint64_t>(sarg());
            signed_decimal = true;
            break;
          case 'a':
          case 'A': {
            if (ch == 'a') {
              ox1 = 'x';
              xdigs = kLowerDigits;
              expchar = 'p';
            } else {
              ox1 = 'X';
              xdigs = kUpperDigits;
              expchar = 'P';
            }
            if (prec == INT_MAX) {
              failure = Failure::kOverflow;
              break;
            }
            if (prec >= 0) ++prec;
            const DecodedFloat value =
                (flags & kLongDbl) ? internal::DecodeQuad(args.LongDouble()) : internal::DecodeDouble(args.Double());
            signflag = value.negative;
            if (value.cls == DecodedFloat::kInfinite || value.cls == DecodedFloat::kNan) {
              expt = INT_MAX;
              dtoa = value.cls == DecodedFloat::kNan ? "NaN" : "Infinity";
            } else {
              internal::HexDigits(value, prec, &dtoa, &expt);
              for (char& digit : dtoa) digit = xdigs[static_cast<unsigned char>(digit)];
            }
            if (prec < 0) prec = static_cast<int>(dtoa.size());
            if (expt == INT_MAX) ox1 = 0;
            kind = Kind::kFloat;
            break;
          }
          case 'e':
          case 'E':
          case 'f':
          case 'F':
          case 'g':
          case 'G': {
            if (ch == 'e' || ch == 'E') {
              if (prec == INT_MAX) {
                failure = Failure::kOverflow;
                break;
              }
              expchar = static_cast<char>(ch);
              prec = prec < 0 ? kDefaultPrecision + 1 : prec + 1;
            } else if (ch == 'f' || ch == 'F') {
              expchar = 0;
            } else {
              expchar = static_cast<char>(ch - ('g' - 'e'));
              if (prec == 0) prec = 1;
            }
            if (prec < 0) prec = kDefaultPrecision;
            const DecodedFloat value =
                (flags & kLongDbl) ? internal::DecodeQuad(args.LongDouble()) : internal::DecodeDouble(args.Double());
            signflag = value.negative;
            if (value.cls == DecodedFloat::kInfinite || value.cls == DecodedFloat::kNan) {
              expt = INT_MAX;
              dtoa = value.cls == DecodedFloat::kNan ? "NaN" : "Infinity";
            } else {
              internal::DecimalDigits(value, expchar != 0 ? 2 : 3, prec, &dtoa, &expt);
            }
            kind = Kind::kFloat;
            break;
          }
          case 'm':
            if (flags & kAlt) {
              const char* name = internal::ErrnoName(caller_errno);
              if (name == nullptr) {
                umax = static_cast<std::uint64_t>(static_cast<std::int64_t>(caller_errno));
                signed_decimal = true;
                break;
              }
              if constexpr (sizeof(CharT) == 1) {
                set_string(name);
              } else {
                if (!MultibyteToWide(name, prec, &wide_conv)) {
                  failure = Failure::kIllegalSequence;
                  break;
                }
                set_string(wide_conv.c_str());
              }
            } else {
              const char* message = internal::ErrnoMessage(caller_errno);
              if constexpr (sizeof(CharT) == 1) {
                set_string(message);
              } else {
                if (!MultibyteToWide(message, prec, &wide_conv)) {
                  failure = Failure::kIllegalSequence;
                  break;
                }
                set_string(wide_conv.c_str());
              }
            }
            break;
          case 'O':
            flags |= kLongInt;
            [[fallthrough]];
          case 'o':
            umax = uarg();
            base = 8;
            sign = 0;
            kind = Kind::kNumber;
            break;
          case 'p':
            umax = args.Gp();
            base = 16;
            xdigs = kLowerDigits;
            ox1 = 'x';
            sign = 0;
            kind = Kind::kNumber;
            break;
          case 'S':
            flags |= kLongInt;
            [[fallthrough]];
          case 's':
            if constexpr (sizeof(CharT) == 1) {
              if (flags & kLongInt) {
                const auto* wide = reinterpret_cast<const g_wchar*>(static_cast<std::uintptr_t>(args.Gp()));
                if (wide == nullptr) {
                  set_string(NullString<CharT>());
                } else {
                  if (!WideToMultibyte(wide, prec, &narrow_conv)) {
                    failure = Failure::kIllegalSequence;
                    break;
                  }
                  set_string(narrow_conv.c_str());
                }
              } else {
                const auto* text = reinterpret_cast<const char*>(static_cast<std::uintptr_t>(args.Gp()));
                set_string(text != nullptr ? text : NullString<CharT>());
              }
            } else {
              if (flags & kLongInt) {
                const auto* wide = reinterpret_cast<const g_wchar*>(static_cast<std::uintptr_t>(args.Gp()));
                set_string(wide != nullptr ? wide : NullString<CharT>());
              } else {
                const char* text = reinterpret_cast<const char*>(static_cast<std::uintptr_t>(args.Gp()));
                if (text == nullptr) text = "(null)";
                if (!MultibyteToWide(text, prec, &wide_conv)) {
                  failure = Failure::kIllegalSequence;
                  break;
                }
                set_string(wide_conv.c_str());
              }
            }
            if (size > INT_MAX) failure = Failure::kOverflow;
            break;
          case 'U':
            flags |= kLongInt;
            [[fallthrough]];
          case 'u':
            umax = uarg();
            base = 10;
            sign = 0;
            kind = Kind::kNumber;
            break;
          case 'X':
          case 'x':
            xdigs = ch == 'X' ? kUpperDigits : kLowerDigits;
            umax = uarg();
            base = 16;
            if ((flags & kAlt) && umax != 0) ox1 = ch;
            sign = 0;
            kind = Kind::kNumber;
            break;
          default:
            if (ch == 0) {
              stop = true;
              break;
            }
            buf[0] = ch;
            cp = buf;
            size = 1;
            sign = 0;
            break;
        }
        if (failure != Failure::kNone || stop) break;
        if (args.bad()) {
          failure = Failure::kInvalid;
          break;
        }

        if (signed_decimal) {
          if (static_cast<std::int64_t>(umax) < 0) {
            umax = 0 - umax;
            sign = '-';
          }
          base = 10;
          kind = Kind::kNumber;
        }

        if (kind == Kind::kNumber) {
          if ((dprec = prec) >= 0) flags &= ~kZeroPad;
          CharT* end = buf + kNumberBuffer;
          CharT* p = end;
          if (umax != 0 || prec != 0) {
            switch (base) {
              case 2:
                do {
                  *--p = static_cast<CharT>('0' + (umax & 1));
                  umax >>= 1;
                } while (umax != 0);
                break;
              case 8:
                do {
                  *--p = static_cast<CharT>('0' + (umax & 7));
                  umax >>= 3;
                } while (umax != 0);
                if ((flags & kAlt) && *p != '0') *--p = '0';
                break;
              case 10:
                while (umax >= 10) {
                  *--p = static_cast<CharT>('0' + umax % 10);
                  umax /= 10;
                }
                *--p = static_cast<CharT>('0' + umax);
                break;
              default:
                do {
                  *--p = static_cast<CharT>(xdigs[umax & 15]);
                  umax >>= 4;
                } while (umax != 0);
                break;
            }
          }
          cp = p;
          size = end - p;
        } else if (kind == Kind::kFloat) {
          if (signflag) sign = '-';
          if (expt == INT_MAX) {
            const bool lower = ch >= 'a';
            static const CharT kNanLower[] = {'n', 'a', 'n'};
            static const CharT kNanUpper[] = {'N', 'A', 'N'};
            static const CharT kInfLower[] = {'i', 'n', 'f'};
            static const CharT kInfUpper[] = {'I', 'N', 'F'};
            if (dtoa[0] == 'N') {
              cp = lower ? kNanLower : kNanUpper;
            } else {
              cp = lower ? kInfLower : kInfUpper;
            }
            size = 3;
            flags &= ~kZeroPad;
            kind = Kind::kText;
          } else {
            flags |= kFpt;
            ndig = static_cast<int>(dtoa.size());
            if (ch == 'g' || ch == 'G') {
              if (expt > -4 && expt <= prec) {
                expchar = 0;
                if (flags & kAlt) {
                  prec -= expt;
                } else {
                  prec = ndig - expt;
                }
                if (prec < 0) prec = 0;
              } else if (!(flags & kAlt)) {
                prec = ndig;
              }
            }
            if (expchar != 0) {
              expsize = WriteExponent(expstr, expt - 1, expchar);
              size = static_cast<long long>(expsize) + prec;
              if (prec > 1 || (flags & kAlt)) ++size;
            } else {
              size = expt > 0 ? expt : 1;
              if (prec != 0 || (flags & kAlt)) size += static_cast<long long>(prec) + 1;
              lead = expt;
            }
          }
        }

        long long realsz = dprec > size ? dprec : size;
        if (sign != 0) ++realsz;
        if (ox1 != 0) realsz += 2;
        if (realsz > INT_MAX) {
          failure = Failure::kOverflow;
          break;
        }
        if ((flags & (kLadjust | kZeroPad)) == 0) out.Pad(' ', width - realsz);
        if (sign != 0) out.Put(sign);
        if (ox1 != 0) {
          out.Put('0');
          out.Put(ox1);
        }
        if ((flags & (kLadjust | kZeroPad)) == kZeroPad) out.Pad('0', width - realsz);
        out.Pad('0', dprec - size);
        if ((flags & kFpt) == 0) {
          out.Write(cp, static_cast<std::size_t>(size));
        } else {
          auto print_and_pad = [&](long long position, long long length) {
            long long available = static_cast<long long>(ndig) - position;
            if (available > length) available = length;
            if (available > 0) out.WriteAscii(dtoa.data() + position, static_cast<std::size_t>(available));
            out.Pad('0', length - (available > 0 ? available : 0));
          };
          if (expchar == 0) {
            if (expt <= 0) {
              out.Put('0');
              if (prec != 0 || (flags & kAlt)) out.Put('.');
              out.Pad('0', -static_cast<long long>(expt));
              print_and_pad(0, static_cast<long long>(prec) + expt);
            } else {
              print_and_pad(0, lead);
              if (prec != 0 || (flags & kAlt)) out.Put('.');
              print_and_pad(lead, prec);
            }
          } else {
            if (prec > 1 || (flags & kAlt)) {
              out.WriteAscii(dtoa.data(), 1);
              out.Put('.');
              out.WriteAscii(dtoa.data() + 1, static_cast<std::size_t>(ndig - 1));
              out.Pad('0', static_cast<long long>(prec) - ndig);
            } else {
              out.WriteAscii(dtoa.data(), 1);
            }
            out.WriteAscii(expstr, static_cast<std::size_t>(expsize));
          }
        }
        if (flags & kLadjust) out.Pad(' ', width - realsz);
        long long counted = width < realsz ? realsz : width;
        if (counted > INT_MAX - ret) {
          failure = Failure::kOverflow;
          break;
        }
        ret += static_cast<int>(counted);
        if (!out.ok()) {
          failure = Failure::kSink;
          break;
        }
      }
      if (!out.Flush() && failure == Failure::kNone) failure = Failure::kSink;
    }
  } catch (const std::bad_alloc&) {
    return FailWith(guest::kENOMEM, -1);
  } catch (...) {
    return FailWith(guest::kEINVAL, -1);
  }
  switch (failure) {
    case Failure::kNone:
      return ret;
    case Failure::kOverflow:
      return FailWith(guest::kEOVERFLOW, -1);
    case Failure::kIllegalSequence:
      return FailWith(guest::kEILSEQ, -1);
    case Failure::kPercentN:
      HostLog(internal::kLogError, "libc", "printf: %n is not allowed on Android; returning -1 (EINVAL)");
      return FailWith(guest::kEINVAL, -1);
    case Failure::kInvalid:
      HostLog(internal::kLogError, "libc", "printf: unsupported format (bad %w size or argument index)");
      return FailWith(guest::kEINVAL, -1);
    case Failure::kSink:
    default:
      return -1;
  }
}

class BufferSink final : public Sink {
 public:
  BufferSink(char* buffer, std::size_t capacity) : buffer_(buffer), capacity_(capacity) {}
  bool Write(const char* data, std::size_t length) override {
    const std::size_t room = capacity_ - used_;
    const std::size_t n = length < room ? length : room;
    if (n != 0) std::memcpy(buffer_ + used_, data, n);
    used_ += n;
    return true;
  }
  std::size_t used() const { return used_; }

 private:
  char* buffer_;
  std::size_t capacity_;
  std::size_t used_ = 0;
};

class CollectWideSink final : public WideSink {
 public:
  bool Write(const g_wchar* data, std::size_t length) override {
    text.append(data, length);
    return true;
  }
  std::basic_string<g_wchar> text;
};

}

int Format(Sink& sink, const char* format, abi::SysvVaList* args) noexcept {
  return FormatImpl<char, Sink>(sink, format, args);
}

int FormatWide(WideSink& sink, const g_wchar* format, abi::SysvVaList* args) noexcept {
  return FormatImpl<g_wchar, WideSink>(sink, format, args);
}

int FormatToBuffer(char* buffer, std::size_t size, const char* format, abi::SysvVaList* args) noexcept {
  BufferSink sink(buffer, size == 0 ? 0 : size - 1);
  const int result = Format(sink, format, args);
  if (size != 0) buffer[sink.used()] = '\0';
  return result;
}

int FormatWideToBuffer(g_wchar* buffer, std::size_t size, const g_wchar* format,
                       abi::SysvVaList* args) noexcept {
  if (size == 0) return FailWith(guest::kEINVAL, -1);
  try {
    CollectWideSink sink;
    const int result = FormatWide(sink, format, args);
    if (result < 0) return -1;
    if (result == 0) {
      buffer[0] = 0;
      return 0;
    }
    const std::basic_string<g_wchar>& text = sink.text;
    for (const g_wchar c : text) {
      if (c > 0x1fffff) return FailWith(guest::kEILSEQ, -1);
    }
    const std::size_t limit = text.size() < size ? text.size() : size;
    for (std::size_t i = 0; i < limit; ++i) {
      const g_wchar c = text[i];
      if (c == 0) {
        buffer[i] = 0;
        return result;
      }
      if ((c >= 0xd800 && c <= 0xdfff) || c > 0x10ffff) return FailWith(guest::kEILSEQ, -1);
      buffer[i] = c;
    }
    if (text.size() < size) {
      buffer[text.size()] = 0;
      return result;
    }
    buffer[size - 1] = 0;
    return FailWith(guest::kEOVERFLOW, -1);
  } catch (...) {
    return FailWith(guest::kENOMEM, -1);
  }
}

}
