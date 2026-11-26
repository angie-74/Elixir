// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#ifndef DISCORD_RPC_HELPER_H_
#define DISCORD_RPC_HELPER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace discord_rpc {

using String = std::optional<std::string>;

namespace helper {

int Length(std::string_view text);
int Utf8ByteCount(std::string_view text);
std::string Trim(std::string_view text);
bool IsNullOrEmpty(const String& text);
bool EqualsIgnoreCase(std::string_view a, std::string_view b);
std::string ToUpperInvariant(std::string_view text);
std::string ToLowerInvariant(std::string_view text);
std::string WellFormed(std::string_view text);
std::wstring ToWide(std::string_view text);
std::string FromWide(std::wstring_view text);

std::string Format(std::string_view format, const std::vector<std::string>& args);

std::string ToString(const std::string& value);
std::string ToString(std::string_view value);
std::string ToString(const char* value);
std::string ToString(bool value);
std::string ToString(int value);
std::string ToString(unsigned value);
std::string ToString(long value);
std::string ToString(unsigned long value);
std::string ToString(long long value);
std::string ToString(unsigned long long value);
std::string ToString(double value);
template <typename T>
std::string ToString(const std::optional<T>& value) {
  return value ? ToString(*value) : std::string();
}

template <typename... T>
std::vector<std::string> Args(const T&... args) {
  return std::vector<std::string>{ToString(args)...};
}

class BackoffDelay {
 public:
  BackoffDelay(int min, int max) : maximum_(max), minimum_(min), current_(min), fails_(0) {}

  int Maximum() const { return maximum_; }
  int Minimum() const { return minimum_; }
  int Current() const { return current_; }
  int Fails() const { return fails_; }

  void Reset() {
    fails_ = 0;
    current_ = minimum_;
  }

  int NextDelay();

 private:
  int maximum_;
  int minimum_;
  int current_;
  int fails_;
};

struct StringTools {
  static String GetNullOrString(const String& str);
  static bool WithinLength(const String& str, int bytes);
  static String ToCamelCase(const String& str);
  static String ToSnakeCase(const String& str);
};

}
}

#endif
