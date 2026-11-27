// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_I18N_H_
#define ELIXIR_LAUNCHER_CORE_I18N_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#define ELIXIR_TR(text) text

namespace elixir::launcher {

struct LocaleInfo {
  const char* code;
  const wchar_t* name;
  bool right_to_left;
};

inline constexpr char kDefaultLocale[] = "en-US";
inline constexpr char kAutomaticLanguage[] = "auto";

const std::vector<LocaleInfo>& SupportedLocales();
const LocaleInfo* FindLocale(std::string_view code);

std::string MatchLocale(const std::vector<std::wstring>& preferred);
std::vector<std::wstring> WindowsPreferredLanguages();
std::string ResolveLanguage(std::string_view configured);

bool LoadLocaleJson(std::string_view code, std::string_view json, std::string* error);
bool LoadLocalesFromResources();
bool LoadLocalesFromDirectory(const std::wstring& directory, std::wstring* error);
bool LocaleLoaded(std::string_view code);

void SetLanguage(std::string_view code);
std::string CurrentLanguage();
std::wstring CurrentLanguageTag();
bool CurrentLanguageIsRightToLeft();
std::uint64_t LanguageGeneration();

const std::wstring& T(std::wstring_view text);
std::wstring FormatText(std::wstring_view pattern, const std::wstring* arguments, std::size_t count);
std::wstring TN(std::uint64_t count, std::wstring_view one, std::wstring_view other);
const wchar_t* PluralCategory(std::string_view code, std::uint64_t count);
bool ReadsRightToLeft(std::wstring_view text);
std::wstring FormatDate(int year, int month, int day);

namespace i18n_detail {

inline std::wstring Text(const std::wstring& value) { return value; }
inline std::wstring Text(std::wstring_view value) { return std::wstring(value); }
inline std::wstring Text(const wchar_t* value) { return value != nullptr ? std::wstring(value) : std::wstring(); }
template <typename Number, std::enable_if_t<std::is_integral_v<Number>, int> = 0>
std::wstring Text(Number value) {
  return std::to_wstring(value);
}

}

template <typename... Arguments>
std::wstring TF(std::wstring_view pattern, const Arguments&... arguments) {
  const std::array<std::wstring, sizeof...(Arguments)> texts = {i18n_detail::Text(arguments)...};
  return FormatText(T(pattern), texts.data(), texts.size());
}

}

#endif
