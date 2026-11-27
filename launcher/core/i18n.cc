// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "core/i18n.h"

#include <windows.h>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_set>

#include "core/json.h"
#include "core/notices.h"
#include "core/util.h"
#include "resource.h"

namespace elixir::launcher {
namespace {

constexpr wchar_t kContextMark[] = L"##";
constexpr wchar_t kLeftToRightMark = 0x200E;
constexpr std::size_t kMaxLocaleFileBytes = 4 * 1024 * 1024;

struct Table {
  std::map<std::wstring, std::wstring, std::less<>> strings;
};

struct State {
  std::shared_mutex mutex;
  std::map<std::string, std::unique_ptr<Table>, std::less<>> tables;
  std::vector<std::unique_ptr<Table>> retired;
  std::unordered_set<std::wstring> interned;
  std::string language = kDefaultLocale;
  std::atomic<const Table*> active{nullptr};
  std::atomic<const Table*> english{nullptr};
  std::atomic<bool> right_to_left{false};
  std::atomic<std::uint64_t> generation{1};
};

State& GetState() {
  static State* state = new State;
  return *state;
}

const std::wstring& Intern(State& state, std::wstring_view text) {
  std::wstring key(text);
  {
    std::shared_lock<std::shared_mutex> lock(state.mutex);
    const auto found = state.interned.find(key);
    if (found != state.interned.end()) return *found;
  }
  std::unique_lock<std::shared_mutex> lock(state.mutex);
  return *state.interned.insert(std::move(key)).first;
}

const std::wstring* FindIn(const Table* table, std::wstring_view text) {
  if (table == nullptr) return nullptr;
  const auto found = table->strings.find(text);
  return found != table->strings.end() ? &found->second : nullptr;
}

std::string PrimaryLanguage(std::string_view code) { return ToLowerAscii(code.substr(0, code.find('-'))); }

bool EndsWith(std::string_view text, std::string_view suffix) {
  return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

void ActivateLocked(State& state) {
  const auto found = state.tables.find(state.language);
  state.active.store(found != state.tables.end() ? found->second.get() : nullptr, std::memory_order_release);
  const auto english = state.tables.find(std::string_view(kDefaultLocale));
  state.english.store(english != state.tables.end() ? english->second.get() : nullptr, std::memory_order_release);
  const LocaleInfo* locale = FindLocale(state.language);
  state.right_to_left.store(locale != nullptr && locale->right_to_left, std::memory_order_release);
  state.generation.fetch_add(1, std::memory_order_acq_rel);
}

}

const std::vector<LocaleInfo>& SupportedLocales() {
  static const std::vector<LocaleInfo> locales = {
      {"en-US", L"English", false},
      {"pt-BR", L"Português (Brasil)", false},
      {"es-ES", L"Español", false},
      {"ru-RU", L"Русский", false},
      {"id-ID", L"Bahasa Indonesia", false},
      {"fr-FR", L"Français", false},
      {"de-DE", L"Deutsch", false},
      {"tr-TR", L"Türkçe", false},
      {"pl-PL", L"Polski", false},
      {"vi-VN", L"Tiếng Việt", false},
      {"th-TH", L"ไทย", false},
      {"ja-JP", L"日本語", false},
      {"ko-KR", L"한국어", false},
      {"zh-CN", L"简体中文", false},
      {"zh-TW", L"繁體中文", false},
      {"ar-SA", L"العربية", true},
  };
  return locales;
}

const LocaleInfo* FindLocale(std::string_view code) {
  const std::string wanted = ToLowerAscii(code);
  for (const LocaleInfo& locale : SupportedLocales()) {
    if (ToLowerAscii(locale.code) == wanted) return &locale;
  }
  return nullptr;
}

std::string MatchLocale(const std::vector<std::wstring>& preferred) {
  for (const std::wstring& name : preferred) {
    const std::string wanted = ToLowerAscii(WideToUtf8(name));
    if (wanted.empty()) continue;
    if (const LocaleInfo* exact = FindLocale(wanted)) return exact->code;
    const std::string language = PrimaryLanguage(wanted);
    if (language == "zh") {
      const bool simplified = wanted.find("-hans") != std::string::npos;
      const bool traditional = !simplified && (wanted.find("-hant") != std::string::npos || EndsWith(wanted, "-tw") ||
                                               EndsWith(wanted, "-hk") || EndsWith(wanted, "-mo"));
      return traditional ? "zh-TW" : "zh-CN";
    }
    for (const LocaleInfo& locale : SupportedLocales()) {
      if (PrimaryLanguage(locale.code) == language) return locale.code;
    }
  }
  return kDefaultLocale;
}

std::vector<std::wstring> WindowsPreferredLanguages() {
  std::vector<std::wstring> names;
  ULONG count = 0;
  ULONG size = 0;
  if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, nullptr, &size) || size == 0) return names;
  std::wstring buffer(size, L'\0');
  if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, buffer.data(), &size)) return names;
  std::size_t start = 0;
  while (start < buffer.size()) {
    const std::size_t end = buffer.find(L'\0', start);
    if (end == std::wstring::npos || end == start) break;
    names.emplace_back(buffer.substr(start, end - start));
    start = end + 1;
  }
  return names;
}

std::string ResolveLanguage(std::string_view configured) {
  if (const LocaleInfo* chosen = FindLocale(configured)) return chosen->code;
  return MatchLocale(WindowsPreferredLanguages());
}

bool LoadLocaleJson(std::string_view code, std::string_view json, std::string* error) {
  const auto fail = [error](std::string message) {
    if (error != nullptr) *error = std::move(message);
    return false;
  };
  const LocaleInfo* locale = FindLocale(code);
  if (locale == nullptr) return fail("unknown language " + std::string(code));
  if (json.size() >= 3 && json.substr(0, 3) == "\xEF\xBB\xBF") json.remove_prefix(3);
  std::string parse_error;
  const std::optional<Json> document = Json::Parse(json, &parse_error);
  if (!document) return fail(parse_error);
  if (!document->is_object()) return fail("the file is not a JSON object");
  const std::string declared = document->GetString("code");
  if (!declared.empty() && ToLowerAscii(declared) != ToLowerAscii(locale->code)) {
    return fail("the file is for " + declared + ", not " + locale->code);
  }
  const Json* strings = document->Find("strings");
  if (strings == nullptr || !strings->is_object()) return fail("the file has no \"strings\" object");
  auto table = std::make_unique<Table>();
  for (const Json::Member& member : strings->members()) {
    if (!member.second.is_string() || member.second.string_ref().empty() || member.first.empty()) continue;
    table->strings[Utf8ToWide(member.first)] = Utf8ToWide(member.second.string_ref());
  }
  State& state = GetState();
  std::unique_lock<std::shared_mutex> lock(state.mutex);
  std::unique_ptr<Table>& slot = state.tables[locale->code];
  if (slot != nullptr) state.retired.push_back(std::move(slot));
  slot = std::move(table);
  ActivateLocked(state);
  return true;
}

bool LoadLocalesFromResources() {
  const std::vector<LocaleInfo>& locales = SupportedLocales();
  for (std::size_t index = 0; index < locales.size(); ++index) {
    std::string bytes;
    if (!LoadResourceText(IDR_LOCALE_FIRST + static_cast<int>(index), &bytes)) continue;
    LoadLocaleJson(locales[index].code, bytes, nullptr);
  }
  return LocaleLoaded(kDefaultLocale);
}

bool LoadLocalesFromDirectory(const std::wstring& directory, std::wstring* error) {
  bool complete = true;
  for (const LocaleInfo& locale : SupportedLocales()) {
    const std::wstring path = JoinPath(directory, Utf8ToWide(locale.code) + L".json");
    std::string bytes;
    std::string problem;
    if (!ReadFileBytes(path, &bytes, nullptr, kMaxLocaleFileBytes) ||
        !LoadLocaleJson(locale.code, bytes, &problem)) {
      complete = false;
      if (error != nullptr && error->empty()) *error = path + (problem.empty() ? L"" : L": " + Utf8ToWide(problem));
    }
  }
  return complete;
}

bool LocaleLoaded(std::string_view code) {
  State& state = GetState();
  std::shared_lock<std::shared_mutex> lock(state.mutex);
  const LocaleInfo* locale = FindLocale(code);
  return locale != nullptr && state.tables.find(std::string_view(locale->code)) != state.tables.end();
}

void SetLanguage(std::string_view code) {
  const LocaleInfo* locale = FindLocale(code);
  State& state = GetState();
  std::unique_lock<std::shared_mutex> lock(state.mutex);
  const std::string wanted = locale != nullptr ? locale->code : kDefaultLocale;
  if (wanted == state.language) return;
  state.language = wanted;
  ActivateLocked(state);
}

std::string CurrentLanguage() {
  State& state = GetState();
  std::shared_lock<std::shared_mutex> lock(state.mutex);
  return state.language;
}

std::wstring CurrentLanguageTag() { return Utf8ToWide(ToLowerAscii(CurrentLanguage())); }

bool CurrentLanguageIsRightToLeft() { return GetState().right_to_left.load(std::memory_order_acquire); }

std::uint64_t LanguageGeneration() { return GetState().generation.load(std::memory_order_acquire); }

const std::wstring& T(std::wstring_view text) {
  State& state = GetState();
  if (const std::wstring* found = FindIn(state.active.load(std::memory_order_acquire), text)) return *found;
  if (const std::wstring* found = FindIn(state.english.load(std::memory_order_acquire), text)) return *found;
  return Intern(state, text.substr(0, text.find(kContextMark)));
}

bool ReadsRightToLeft(std::wstring_view text) {
  for (const wchar_t c : text) {
    if ((c >= 0x0590 && c <= 0x08FF) || (c >= 0xFB1D && c <= 0xFDFF) || (c >= 0xFE70 && c <= 0xFEFF)) return true;
  }
  return false;
}

std::wstring FormatText(std::wstring_view pattern, const std::wstring* arguments, std::size_t count) {
  const bool guard = CurrentLanguageIsRightToLeft();
  std::wstring out;
  out.reserve(pattern.size() + 18 * count);
  for (std::size_t i = 0; i < pattern.size(); ++i) {
    const wchar_t c = pattern[i];
    if (c == L'{' && i + 2 < pattern.size() && pattern[i + 1] >= L'0' && pattern[i + 1] <= L'9' &&
        pattern[i + 2] == L'}') {
      const std::size_t index = static_cast<std::size_t>(pattern[i + 1] - L'0');
      if (index < count && arguments != nullptr) {
        const std::wstring& argument = arguments[index];
        const bool foreign = guard && !argument.empty() && !ReadsRightToLeft(argument);
        if (foreign) out.push_back(kLeftToRightMark);
        out += argument;
        if (foreign) out.push_back(kLeftToRightMark);
        i += 2;
        continue;
      }
    }
    out.push_back(c);
  }
  return out;
}

std::wstring FormatDate(int year, int month, int day) {
  if (month < 1 || month > 12 || day < 1 || day > 31) return {};
  const std::wstring& pattern = T(L"MMM d, yyyy##date");
  const auto padded = [](int value) { return (value < 10 ? L"0" : L"") + std::to_wstring(value); };
  std::wstring text;
  for (std::size_t i = 0; i < pattern.size();) {
    const std::wstring_view rest = std::wstring_view(pattern).substr(i);
    if (rest.front() == L'\'') {
      const std::size_t end = rest.find(L'\'', 1);
      text.append(rest.substr(1, end == std::wstring_view::npos ? std::wstring_view::npos : end - 1));
      i += end == std::wstring_view::npos ? rest.size() : end + 1;
    } else if (rest.substr(0, 4) == L"yyyy") {
      text += std::to_wstring(year);
      i += 4;
    } else if (rest.substr(0, 3) == L"MMM") {
      wchar_t name[64] = {};
      const std::wstring tag = CurrentLanguageTag();
      const CALTYPE which = CAL_SABBREVMONTHNAME1 + static_cast<CALTYPE>(month - 1);
      if (GetCalendarInfoEx(tag.c_str(), CAL_GREGORIAN, nullptr, which, name, 64, nullptr) > 0 && name[0] != L'\0') {
        text += name;
      } else {
        text += std::to_wstring(month);
      }
      i += 3;
    } else if (rest.substr(0, 2) == L"MM") {
      text += padded(month);
      i += 2;
    } else if (rest.front() == L'M') {
      text += std::to_wstring(month);
      i += 1;
    } else if (rest.substr(0, 2) == L"dd") {
      text += padded(day);
      i += 2;
    } else if (rest.front() == L'd') {
      text += std::to_wstring(day);
      i += 1;
    } else {
      text.push_back(rest.front());
      i += 1;
    }
  }
  return text;
}

const wchar_t* PluralCategory(std::string_view code, std::uint64_t count) {
  const std::string language = PrimaryLanguage(code);
  const std::uint64_t tens = count % 10;
  const std::uint64_t hundreds = count % 100;
  if (language == "ru") {
    if (tens == 1 && hundreds != 11) return L"one";
    if (tens >= 2 && tens <= 4 && (hundreds < 12 || hundreds > 14)) return L"few";
    return L"many";
  }
  if (language == "pl") {
    if (count == 1) return L"one";
    if (tens >= 2 && tens <= 4 && (hundreds < 12 || hundreds > 14)) return L"few";
    return L"many";
  }
  if (language == "ar") {
    if (count == 0) return L"zero";
    if (count == 1) return L"one";
    if (count == 2) return L"two";
    if (hundreds >= 3 && hundreds <= 10) return L"few";
    if (hundreds >= 11) return L"many";
    return L"other";
  }
  if (language == "fr" || language == "pt") return count <= 1 ? L"one" : L"other";
  if (language == "id" || language == "ja" || language == "ko" || language == "th" || language == "vi" ||
      language == "zh") {
    return L"other";
  }
  return count == 1 ? L"one" : L"other";
}

std::wstring TN(std::uint64_t count, std::wstring_view one, std::wstring_view other) {
  State& state = GetState();
  const std::wstring_view category = PluralCategory(CurrentLanguage(), count);
  std::wstring key(other);
  key += kContextMark;
  key += category;
  const std::wstring* pattern = FindIn(state.active.load(std::memory_order_acquire), key);
  if (pattern == nullptr) pattern = &T(category == L"one" ? one : other);
  const std::wstring number = std::to_wstring(count);
  return FormatText(*pattern, &number, 1);
}

}
