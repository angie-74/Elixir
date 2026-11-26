// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#include "discord_rpc/helper.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "discord_rpc/exceptions.h"

namespace discord_rpc::helper {
namespace {

char32_t Next(std::string_view text, std::size_t* index) {
  const auto byte = [&text](std::size_t at) { return static_cast<unsigned char>(text[at]); };
  const std::size_t start = *index;
  const unsigned char lead = byte(start);
  *index = start + 1;
  if (lead < 0x80) return lead;
  const std::size_t length = lead > 0xF4 ? 0 : lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC2 ? 2 : 0;
  if (length == 0 || start + length > text.size()) return 0xFFFD;
  char32_t value = lead & (0xFFu >> (length + 1));
  for (std::size_t k = 1; k < length; ++k) {
    if ((byte(start + k) & 0xC0) != 0x80) return 0xFFFD;
    value = (value << 6) | (byte(start + k) & 0x3Fu);
  }
  const bool overlong = (length == 3 && value < 0x800) || (length == 4 && value < 0x10000);
  if (overlong || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) return 0xFFFD;
  *index = start + length;
  return value;
}

bool IsWhiteSpace(char32_t c) {
  return c == 0x20 || (c >= 0x09 && c <= 0x0D) || c == 0x85 || c == 0xA0 || c == 0x1680 ||
         (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000;
}

std::wstring MapCase(std::wstring text, DWORD flags) {
  if (text.empty()) return text;
  std::wstring mapped(text.size(), L'\0');
  const int written = LCMapStringEx(LOCALE_NAME_INVARIANT, flags, text.data(), static_cast<int>(text.size()),
                                    mapped.data(), static_cast<int>(mapped.size()), nullptr, nullptr, 0);
  return written == static_cast<int>(text.size()) ? mapped : text;
}

bool IsUpper(wchar_t c) {
  if (c < 0x100) return (c >= L'A' && c <= L'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7);
  WORD type = 0;
  return GetStringTypeW(CT_CTYPE1, &c, 1, &type) != 0 && (type & C1_UPPER) != 0;
}

}

int Length(std::string_view text) {
  int units = 0;
  for (std::size_t index = 0; index < text.size();) units += Next(text, &index) >= 0x10000 ? 2 : 1;
  return units;
}

int Utf8ByteCount(std::string_view text) {
  int bytes = 0;
  for (std::size_t index = 0; index < text.size();) {
    const char32_t c = Next(text, &index);
    bytes += c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
  }
  return bytes;
}

std::string Trim(std::string_view text) {
  std::size_t start = 0;
  while (start < text.size()) {
    std::size_t next = start;
    if (!IsWhiteSpace(Next(text, &next))) break;
    start = next;
  }
  std::size_t end = text.size();
  while (end > start) {
    std::size_t at = end - 1;
    while (at > start && end - at < 4 && (static_cast<unsigned char>(text[at]) & 0xC0) == 0x80) --at;
    std::size_t next = at;
    const char32_t c = Next(text, &next);
    if (next != end || !IsWhiteSpace(c)) break;
    end = at;
  }
  return std::string(text.substr(start, end - start));
}

bool IsNullOrEmpty(const String& text) { return !text.has_value() || text->empty(); }

bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  const auto upper = [](char c) { return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c; };
  for (std::size_t index = 0; index < a.size(); ++index) {
    if (upper(a[index]) != upper(b[index])) return false;
  }
  return true;
}

std::string ToUpperInvariant(std::string_view text) { return FromWide(MapCase(ToWide(text), LCMAP_UPPERCASE)); }

std::string ToLowerInvariant(std::string_view text) { return FromWide(MapCase(ToWide(text), LCMAP_LOWERCASE)); }

std::string WellFormed(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (std::size_t index = 0; index < text.size();) {
    const std::size_t start = index;
    if (Next(text, &index) == 0xFFFD && !(index - start == 3 && text.substr(start, 3) == "\xEF\xBF\xBD")) {
      out += "\xEF\xBF\xBD";
    } else {
      out.append(text.substr(start, index - start));
    }
  }
  return out;
}

std::wstring ToWide(std::string_view text) {
  if (text.empty()) return {};
  const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  std::wstring wide(static_cast<std::size_t>(std::max(size, 0)), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
  return wide;
}

std::string FromWide(std::wstring_view text) {
  if (text.empty()) return {};
  const int size =
      WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  std::string narrow(static_cast<std::size_t>(std::max(size, 0)), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(), size, nullptr, nullptr);
  return narrow;
}

std::string Format(std::string_view format, const std::vector<std::string>& args) {
  std::string out;
  for (std::size_t index = 0; index < format.size(); ++index) {
    const char c = format[index];
    if (c == '{') {
      if (index + 1 < format.size() && format[index + 1] == '{') {
        out += '{';
        ++index;
        continue;
      }
      const std::size_t close = format.find('}', index);
      if (close == std::string_view::npos) throw FormatException();
      std::size_t argument = 0;
      std::size_t at = index + 1;
      while (at < close && format[at] >= '0' && format[at] <= '9' && argument < 100000) {
        argument = argument * 10 + static_cast<std::size_t>(format[at] - '0');
        ++at;
      }
      if (at == index + 1 || argument >= args.size()) throw FormatException();
      out += args[argument];
      index = close;
    } else if (c == '}') {
      if (index + 1 >= format.size() || format[index + 1] != '}') throw FormatException();
      out += '}';
      ++index;
    } else {
      out += c;
    }
  }
  return out;
}

std::string ToString(const std::string& value) { return value; }
std::string ToString(std::string_view value) { return std::string(value); }
std::string ToString(const char* value) { return value != nullptr ? std::string(value) : std::string(); }
std::string ToString(bool value) { return value ? "True" : "False"; }
std::string ToString(int value) { return std::to_string(value); }
std::string ToString(unsigned value) { return std::to_string(value); }
std::string ToString(long value) { return std::to_string(value); }
std::string ToString(unsigned long value) { return std::to_string(value); }
std::string ToString(long long value) { return std::to_string(value); }
std::string ToString(unsigned long long value) { return std::to_string(value); }

std::string ToString(double value) {
  if (value == 0) return "0";
  char buffer[48];
  std::snprintf(buffer, sizeof(buffer), "%.15g", value);
  std::string text = buffer;
  std::replace(text.begin(), text.end(), 'e', 'E');
  return text;
}

int BackoffDelay::NextDelay() {
  fails_++;

  const double diff = static_cast<double>(static_cast<float>(maximum_ - minimum_) / 100.0f);
  current_ = static_cast<int>(std::floor(diff * fails_)) + minimum_;

  return std::min(std::max(current_, minimum_), maximum_);
}

String StringTools::GetNullOrString(const String& str) {
  if (!str.has_value()) throw NullReferenceException();
  return str->empty() || Trim(*str).empty() ? String() : str;
}

bool StringTools::WithinLength(const String& str, int bytes) {
  if (!str.has_value()) throw ArgumentNullException("s");
  return Utf8ByteCount(*str) <= bytes;
}

String StringTools::ToCamelCase(const String& str) {
  if (!str.has_value()) return std::nullopt;
  const std::wstring lower = MapCase(ToWide(*str), LCMAP_LOWERCASE);
  std::wstring result;
  std::size_t index = 0;
  while (index < lower.size()) {
    std::size_t end = index;
    while (end < lower.size() && lower[end] != L'_' && lower[end] != L' ') ++end;
    if (end > index) {
      result += MapCase(lower.substr(index, 1), LCMAP_UPPERCASE);
      result += lower.substr(index + 1, end - index - 1);
    }
    index = end + 1;
  }
  return FromWide(result);
}

String StringTools::ToSnakeCase(const String& str) {
  if (!str.has_value()) return std::nullopt;
  const std::wstring text = ToWide(*str);
  std::wstring concat;
  for (std::size_t index = 0; index < text.size(); ++index) {
    if (index > 0 && IsUpper(text[index])) concat += L'_';
    concat += text[index];
  }
  return FromWide(MapCase(concat, LCMAP_UPPERCASE));
}

}
