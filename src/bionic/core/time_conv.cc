// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

#include "abi/sysv_abi.h"
#include "bionic/core/core.h"
#include "bionic/core/core_internal.h"
#include "bionic/runtime.h"

#pragma comment(lib, "advapi32.lib")

namespace elixir::bionic {

ELIXIR_BIONIC_FN(abi::g_time, mktime)(guest::Tm* tm) noexcept;

namespace core {
namespace {

using guest::Tm;

constexpr std::int64_t kSecondsPerDay = 86400;

std::int64_t FloorDiv(std::int64_t a, std::int64_t b) noexcept {
  std::int64_t q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
  return q;
}
std::int64_t FloorMod(std::int64_t a, std::int64_t b) noexcept { return a - FloorDiv(a, b) * b; }

bool IsLeap(std::int64_t year) noexcept { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

int DaysInMonth(std::int64_t year, int month) noexcept {
  static const int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return month == 2 && IsLeap(year) ? 29 : kDays[month - 1];
}

// Howard Hinnant's days_from_civil / civil_from_days (public domain).
std::int64_t DaysFromCivil(std::int64_t y, int m, int d) noexcept {
  y -= m <= 2;
  const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
  const std::int64_t yoe = y - era * 400;
  const std::int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const std::int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

void CivilFromDays(std::int64_t z, std::int64_t* year, int* month, int* day) noexcept {
  z += 719468;
  const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const std::int64_t doe = z - era * 146097;
  const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const std::int64_t mp = (5 * doy + 2) / 153;
  *day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  *month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  *year = yoe + era * 400 + (*month <= 2);
}

int WeekdayOfDays(std::int64_t days) noexcept { return static_cast<int>(FloorMod(days + 4, 7)); }

std::int64_t YearOfSeconds(std::int64_t seconds) noexcept {
  std::int64_t year;
  int month, day;
  CivilFromDays(FloorDiv(seconds, kSecondsPerDay), &year, &month, &day);
  return year;
}

const char* Intern(const std::string& text) {
  static std::mutex lock;
  static auto* pool = new std::unordered_set<std::string>();
  std::lock_guard<std::mutex> guard(lock);
  return pool->insert(text).first->c_str();
}

std::string NumericAbbreviation(std::int32_t offset) {
  char buffer[16];
  const char sign = offset < 0 ? '-' : '+';
  const std::int32_t magnitude = offset < 0 ? -offset : offset;
  const int hours = magnitude / 3600;
  const int minutes = (magnitude / 60) % 60;
  if (minutes == 0) {
    std::snprintf(buffer, sizeof(buffer), "%c%02d", sign, hours);
  } else {
    std::snprintf(buffer, sizeof(buffer), "%c%02d%02d", sign, hours, minutes);
  }
  return buffer;
}

struct YearRule {
  std::int32_t std_offset = 0;
  std::int32_t dst_offset = 0;
  bool has_dst = false;
  std::int64_t dst_start = 0;
  std::int64_t dst_end = 0;
};

struct PosixRule {
  int kind = 2;
  int month = 0, week = 0, weekday = 0, day = 0;
  std::int32_t time = 7200;
};

enum class ZoneKind { kUtc, kPosix, kWindows };

struct Zone {
  ZoneKind kind = ZoneKind::kUtc;
  const char* std_abbr = "UTC";
  const char* dst_abbr = "UTC";
  std::int32_t std_offset = 0;
  std::int32_t dst_offset = 0;
  bool has_dst = false;
  PosixRule start, end;
  DYNAMIC_TIME_ZONE_INFORMATION windows{};
  bool fixed_abbreviations = false;
  std::string iana = "Etc/UTC";
  mutable SRWLOCK cache_lock = SRWLOCK_INIT;
  mutable std::map<std::int64_t, YearRule> cache;
};

struct ZoneName {
  const wchar_t* windows;
  const char* iana;
  const char* std_abbr;
  const char* dst_abbr;
};

const ZoneName kZoneNames[] = {
    {L"Dateline Standard Time", "Etc/GMT+12", nullptr, nullptr},
    {L"UTC-11", "Etc/GMT+11", nullptr, nullptr},
    {L"Aleutian Standard Time", "America/Adak", "HST", "HDT"},
    {L"Hawaiian Standard Time", "Pacific/Honolulu", "HST", "HDT"},
    {L"Marquesas Standard Time", "Pacific/Marquesas", nullptr, nullptr},
    {L"Alaskan Standard Time", "America/Anchorage", "AKST", "AKDT"},
    {L"UTC-09", "Etc/GMT+9", nullptr, nullptr},
    {L"Pacific Standard Time (Mexico)", "America/Tijuana", "PST", "PDT"},
    {L"UTC-08", "Etc/GMT+8", nullptr, nullptr},
    {L"Pacific Standard Time", "America/Los_Angeles", "PST", "PDT"},
    {L"US Mountain Standard Time", "America/Phoenix", "MST", "MDT"},
    {L"Mountain Standard Time (Mexico)", "America/Mazatlan", "MST", "MDT"},
    {L"Mountain Standard Time", "America/Denver", "MST", "MDT"},
    {L"Yukon Standard Time", "America/Whitehorse", "MST", "MDT"},
    {L"Central America Standard Time", "America/Guatemala", "CST", "CDT"},
    {L"Central Standard Time", "America/Chicago", "CST", "CDT"},
    {L"Easter Island Standard Time", "Pacific/Easter", nullptr, nullptr},
    {L"Central Standard Time (Mexico)", "America/Mexico_City", "CST", "CDT"},
    {L"Canada Central Standard Time", "America/Regina", "CST", "CDT"},
    {L"SA Pacific Standard Time", "America/Bogota", nullptr, nullptr},
    {L"Eastern Standard Time (Mexico)", "America/Cancun", "EST", "EDT"},
    {L"Eastern Standard Time", "America/New_York", "EST", "EDT"},
    {L"Haiti Standard Time", "America/Port-au-Prince", "EST", "EDT"},
    {L"Cuba Standard Time", "America/Havana", "CST", "CDT"},
    {L"US Eastern Standard Time", "America/Indiana/Indianapolis", "EST", "EDT"},
    {L"Turks And Caicos Standard Time", "America/Grand_Turk", "EST", "EDT"},
    {L"Paraguay Standard Time", "America/Asuncion", nullptr, nullptr},
    {L"Atlantic Standard Time", "America/Halifax", "AST", "ADT"},
    {L"Venezuela Standard Time", "America/Caracas", nullptr, nullptr},
    {L"Central Brazilian Standard Time", "America/Cuiaba", nullptr, nullptr},
    {L"SA Western Standard Time", "America/La_Paz", nullptr, nullptr},
    {L"Pacific SA Standard Time", "America/Santiago", nullptr, nullptr},
    {L"Newfoundland Standard Time", "America/St_Johns", "NST", "NDT"},
    {L"Tocantins Standard Time", "America/Araguaina", nullptr, nullptr},
    {L"E. South America Standard Time", "America/Sao_Paulo", nullptr, nullptr},
    {L"SA Eastern Standard Time", "America/Cayenne", nullptr, nullptr},
    {L"Argentina Standard Time", "America/Argentina/Buenos_Aires", nullptr, nullptr},
    {L"Greenland Standard Time", "America/Nuuk", nullptr, nullptr},
    {L"Montevideo Standard Time", "America/Montevideo", nullptr, nullptr},
    {L"Magallanes Standard Time", "America/Punta_Arenas", nullptr, nullptr},
    {L"Saint Pierre Standard Time", "America/Miquelon", nullptr, nullptr},
    {L"Bahia Standard Time", "America/Bahia", nullptr, nullptr},
    {L"UTC-02", "Etc/GMT+2", nullptr, nullptr},
    {L"Azores Standard Time", "Atlantic/Azores", nullptr, nullptr},
    {L"Cape Verde Standard Time", "Atlantic/Cape_Verde", nullptr, nullptr},
    {L"UTC", "Etc/UTC", "UTC", "UTC"},
    {L"GMT Standard Time", "Europe/London", "GMT", "BST"},
    {L"Greenwich Standard Time", "Atlantic/Reykjavik", "GMT", "GMT"},
    {L"Sao Tome Standard Time", "Africa/Sao_Tome", "GMT", "GMT"},
    {L"Morocco Standard Time", "Africa/Casablanca", nullptr, nullptr},
    {L"W. Europe Standard Time", "Europe/Berlin", "CET", "CEST"},
    {L"Central Europe Standard Time", "Europe/Budapest", "CET", "CEST"},
    {L"Romance Standard Time", "Europe/Paris", "CET", "CEST"},
    {L"Central European Standard Time", "Europe/Warsaw", "CET", "CEST"},
    {L"W. Central Africa Standard Time", "Africa/Lagos", "WAT", "WAST"},
    {L"Jordan Standard Time", "Asia/Amman", nullptr, nullptr},
    {L"GTB Standard Time", "Europe/Bucharest", "EET", "EEST"},
    {L"Middle East Standard Time", "Asia/Beirut", "EET", "EEST"},
    {L"Egypt Standard Time", "Africa/Cairo", "EET", "EEST"},
    {L"E. Europe Standard Time", "Europe/Chisinau", "EET", "EEST"},
    {L"Syria Standard Time", "Asia/Damascus", nullptr, nullptr},
    {L"West Bank Standard Time", "Asia/Hebron", "EET", "EEST"},
    {L"South Africa Standard Time", "Africa/Johannesburg", "SAST", "SAST"},
    {L"FLE Standard Time", "Europe/Kyiv", "EET", "EEST"},
    {L"Israel Standard Time", "Asia/Jerusalem", "IST", "IDT"},
    {L"South Sudan Standard Time", "Africa/Juba", "CAT", "CAT"},
    {L"Kaliningrad Standard Time", "Europe/Kaliningrad", "EET", "EEST"},
    {L"Sudan Standard Time", "Africa/Khartoum", "CAT", "CAT"},
    {L"Libya Standard Time", "Africa/Tripoli", "EET", "EEST"},
    {L"Namibia Standard Time", "Africa/Windhoek", "CAT", "CAT"},
    {L"Arabic Standard Time", "Asia/Baghdad", nullptr, nullptr},
    {L"Turkey Standard Time", "Europe/Istanbul", nullptr, nullptr},
    {L"Arab Standard Time", "Asia/Riyadh", nullptr, nullptr},
    {L"Belarus Standard Time", "Europe/Minsk", nullptr, nullptr},
    {L"Russian Standard Time", "Europe/Moscow", "MSK", "MSK"},
    {L"E. Africa Standard Time", "Africa/Nairobi", "EAT", "EAT"},
    {L"Volgograd Standard Time", "Europe/Volgograd", "MSK", "MSK"},
    {L"Iran Standard Time", "Asia/Tehran", nullptr, nullptr},
    {L"Arabian Standard Time", "Asia/Dubai", nullptr, nullptr},
    {L"Astrakhan Standard Time", "Europe/Astrakhan", nullptr, nullptr},
    {L"Azerbaijan Standard Time", "Asia/Baku", nullptr, nullptr},
    {L"Russia Time Zone 3", "Europe/Samara", nullptr, nullptr},
    {L"Mauritius Standard Time", "Indian/Mauritius", nullptr, nullptr},
    {L"Saratov Standard Time", "Europe/Saratov", nullptr, nullptr},
    {L"Georgian Standard Time", "Asia/Tbilisi", nullptr, nullptr},
    {L"Caucasus Standard Time", "Asia/Yerevan", nullptr, nullptr},
    {L"Afghanistan Standard Time", "Asia/Kabul", nullptr, nullptr},
    {L"West Asia Standard Time", "Asia/Tashkent", nullptr, nullptr},
    {L"Qyzylorda Standard Time", "Asia/Qyzylorda", nullptr, nullptr},
    {L"Ekaterinburg Standard Time", "Asia/Yekaterinburg", nullptr, nullptr},
    {L"Pakistan Standard Time", "Asia/Karachi", "PKT", "PKST"},
    {L"India Standard Time", "Asia/Kolkata", "IST", "IST"},
    {L"Sri Lanka Standard Time", "Asia/Colombo", nullptr, nullptr},
    {L"Nepal Standard Time", "Asia/Kathmandu", nullptr, nullptr},
    {L"Central Asia Standard Time", "Asia/Almaty", nullptr, nullptr},
    {L"Bangladesh Standard Time", "Asia/Dhaka", nullptr, nullptr},
    {L"Omsk Standard Time", "Asia/Omsk", nullptr, nullptr},
    {L"Myanmar Standard Time", "Asia/Yangon", nullptr, nullptr},
    {L"SE Asia Standard Time", "Asia/Bangkok", nullptr, nullptr},
    {L"Altai Standard Time", "Asia/Barnaul", nullptr, nullptr},
    {L"W. Mongolia Standard Time", "Asia/Hovd", nullptr, nullptr},
    {L"North Asia Standard Time", "Asia/Krasnoyarsk", nullptr, nullptr},
    {L"N. Central Asia Standard Time", "Asia/Novosibirsk", nullptr, nullptr},
    {L"Tomsk Standard Time", "Asia/Tomsk", nullptr, nullptr},
    {L"China Standard Time", "Asia/Shanghai", "CST", "CDT"},
    {L"North Asia East Standard Time", "Asia/Irkutsk", nullptr, nullptr},
    {L"Singapore Standard Time", "Asia/Singapore", nullptr, nullptr},
    {L"W. Australia Standard Time", "Australia/Perth", "AWST", "AWDT"},
    {L"Taipei Standard Time", "Asia/Taipei", "CST", "CDT"},
    {L"Ulaanbaatar Standard Time", "Asia/Ulaanbaatar", nullptr, nullptr},
    {L"Aus Central W. Standard Time", "Australia/Eucla", nullptr, nullptr},
    {L"Transbaikal Standard Time", "Asia/Chita", nullptr, nullptr},
    {L"Tokyo Standard Time", "Asia/Tokyo", "JST", "JDT"},
    {L"North Korea Standard Time", "Asia/Pyongyang", "KST", "KST"},
    {L"Korea Standard Time", "Asia/Seoul", "KST", "KDT"},
    {L"Yakutsk Standard Time", "Asia/Yakutsk", nullptr, nullptr},
    {L"Cen. Australia Standard Time", "Australia/Adelaide", "ACST", "ACDT"},
    {L"AUS Central Standard Time", "Australia/Darwin", "ACST", "ACDT"},
    {L"E. Australia Standard Time", "Australia/Brisbane", "AEST", "AEDT"},
    {L"AUS Eastern Standard Time", "Australia/Sydney", "AEST", "AEDT"},
    {L"West Pacific Standard Time", "Pacific/Port_Moresby", nullptr, nullptr},
    {L"Tasmania Standard Time", "Australia/Hobart", "AEST", "AEDT"},
    {L"Vladivostok Standard Time", "Asia/Vladivostok", nullptr, nullptr},
    {L"Lord Howe Standard Time", "Australia/Lord_Howe", nullptr, nullptr},
    {L"Bougainville Standard Time", "Pacific/Bougainville", nullptr, nullptr},
    {L"Russia Time Zone 10", "Asia/Srednekolymsk", nullptr, nullptr},
    {L"Magadan Standard Time", "Asia/Magadan", nullptr, nullptr},
    {L"Norfolk Standard Time", "Pacific/Norfolk", nullptr, nullptr},
    {L"Sakhalin Standard Time", "Asia/Sakhalin", nullptr, nullptr},
    {L"Central Pacific Standard Time", "Pacific/Guadalcanal", nullptr, nullptr},
    {L"Russia Time Zone 11", "Asia/Kamchatka", nullptr, nullptr},
    {L"New Zealand Standard Time", "Pacific/Auckland", "NZST", "NZDT"},
    {L"UTC+12", "Etc/GMT-12", nullptr, nullptr},
    {L"Fiji Standard Time", "Pacific/Fiji", nullptr, nullptr},
    {L"Chatham Islands Standard Time", "Pacific/Chatham", nullptr, nullptr},
    {L"UTC+13", "Etc/GMT-13", nullptr, nullptr},
    {L"Tonga Standard Time", "Pacific/Tongatapu", nullptr, nullptr},
    {L"Samoa Standard Time", "Pacific/Apia", nullptr, nullptr},
    {L"Line Islands Standard Time", "Pacific/Kiritimati", nullptr, nullptr},
};

const ZoneName* FindByWindowsName(const wchar_t* key) noexcept {
  for (const ZoneName& entry : kZoneNames) {
    if (_wcsicmp(entry.windows, key) == 0) return &entry;
  }
  return nullptr;
}

const ZoneName* FindByIana(const std::string& name) noexcept {
  for (const ZoneName& entry : kZoneNames) {
    if (_stricmp(entry.iana, name.c_str()) == 0) return &entry;
  }
  static const std::pair<const char*, const char*> kAliases[] = {
      {"US/Pacific", "America/Los_Angeles"}, {"US/Eastern", "America/New_York"},
      {"US/Central", "America/Chicago"},     {"US/Mountain", "America/Denver"},
      {"Asia/Calcutta", "Asia/Kolkata"},     {"Europe/Kiev", "Europe/Kyiv"},
      {"America/Buenos_Aires", "America/Argentina/Buenos_Aires"},
      {"Asia/Rangoon", "Asia/Yangon"},       {"Asia/Katmandu", "Asia/Kathmandu"},
      {"America/Godthab", "America/Nuuk"},
  };
  for (const auto& alias : kAliases) {
    if (_stricmp(alias.first, name.c_str()) == 0) return FindByIana(alias.second);
  }
  return nullptr;
}

bool IsUtcName(std::string name) noexcept {
  if (!name.empty() && name[0] == ':') name.erase(0, 1);
  static const char* const kNames[] = {"",          "UTC",       "UTC0",       "GMT",     "GMT0",     "Etc/UTC",
                                       "Etc/GMT",   "Etc/UCT",   "Etc/Universal", "Etc/Zulu", "UCT", "Universal",
                                       "Zulu",      "Etc/GMT0",  "Etc/GMT+0",  "Etc/GMT-0", "GMT+0", "GMT-0",
                                       "Etc/Greenwich", "Greenwich"};
  for (const char* candidate : kNames) {
    if (_stricmp(candidate, name.c_str()) == 0) return true;
  }
  return false;
}

std::shared_ptr<Zone> MakeUtcZone() {
  auto zone = std::make_shared<Zone>();
  zone->kind = ZoneKind::kUtc;
  zone->std_abbr = zone->dst_abbr = "UTC";
  zone->iana = "Etc/UTC";
  return zone;
}

void ApplyAbbreviations(Zone& zone, const ZoneName* name) {
  if (name != nullptr && name->std_abbr != nullptr) {
    zone.std_abbr = name->std_abbr;
    zone.dst_abbr = name->dst_abbr;
    zone.fixed_abbreviations = true;
  } else {
    zone.std_abbr = Intern(NumericAbbreviation(zone.std_offset));
    zone.dst_abbr = Intern(NumericAbbreviation(zone.dst_offset));
  }
}

std::shared_ptr<Zone> MakeWindowsZone(const DYNAMIC_TIME_ZONE_INFORMATION& information) {
  auto zone = std::make_shared<Zone>();
  zone->kind = ZoneKind::kWindows;
  zone->windows = information;
  zone->std_offset = -(information.Bias + information.StandardBias) * 60;
  zone->dst_offset = -(information.Bias + information.DaylightBias) * 60;
  zone->has_dst = !information.DynamicDaylightTimeDisabled && information.DaylightDate.wMonth != 0 &&
                  information.StandardDate.wMonth != 0 && zone->std_offset != zone->dst_offset;
  const ZoneName* name = FindByWindowsName(information.TimeZoneKeyName);
  if (name != nullptr) {
    zone->iana = name->iana;
  } else if (zone->std_offset % 3600 == 0) {
    const int hours = -zone->std_offset / 3600;
    zone->iana = hours == 0 ? "Etc/UTC" : (std::string("Etc/GMT") + (hours > 0 ? "+" : "") + std::to_string(hours));
  } else {
    zone->iana = "Etc/UTC";
  }
  ApplyAbbreviations(*zone, name);
  return zone;
}

std::shared_ptr<Zone> MakeHostZone() {
  DYNAMIC_TIME_ZONE_INFORMATION information{};
  if (GetDynamicTimeZoneInformation(&information) == TIME_ZONE_ID_INVALID) return MakeUtcZone();
  return MakeWindowsZone(information);
}

std::shared_ptr<Zone> MakeZoneForWindowsKey(const wchar_t* key) {
  DYNAMIC_TIME_ZONE_INFORMATION information{};
  for (DWORD index = 0; EnumDynamicTimeZoneInformation(index, &information) == ERROR_SUCCESS; ++index) {
    if (_wcsicmp(information.TimeZoneKeyName, key) == 0) return MakeWindowsZone(information);
  }
  return nullptr;
}

bool ParseAbbreviation(const char*& p, std::string* out) {
  out->clear();
  if (*p == '<') {
    ++p;
    while (*p != '\0' && *p != '>') out->push_back(*p++);
    if (*p != '>') return false;
    ++p;
  } else {
    while (std::isalpha(static_cast<unsigned char>(*p))) out->push_back(*p++);
  }
  return out->size() >= 3;
}

bool ParseOffset(const char*& p, std::int32_t* seconds, bool allow_sign) {
  int sign = 1;
  if (allow_sign && (*p == '+' || *p == '-')) {
    if (*p == '-') sign = -1;
    ++p;
  }
  if (!std::isdigit(static_cast<unsigned char>(*p))) return false;
  int parts[3] = {0, 0, 0};
  for (int i = 0; i < 3; ++i) {
    if (!std::isdigit(static_cast<unsigned char>(*p))) return false;
    int value = 0;
    int digits = 0;
    while (std::isdigit(static_cast<unsigned char>(*p)) && digits < 3) {
      value = value * 10 + (*p++ - '0');
      ++digits;
    }
    parts[i] = value;
    if (*p != ':') break;
    ++p;
  }
  if (parts[0] > 167 || parts[1] > 59 || parts[2] > 59) return false;
  *seconds = sign * (parts[0] * 3600 + parts[1] * 60 + parts[2]);
  return true;
}

bool ParseRule(const char*& p, PosixRule* rule) {
  if (*p == 'J') {
    ++p;
    rule->kind = 0;
    rule->day = std::atoi(p);
    while (std::isdigit(static_cast<unsigned char>(*p))) ++p;
    if (rule->day < 1 || rule->day > 365) return false;
  } else if (*p == 'M') {
    ++p;
    rule->kind = 2;
    if (std::sscanf(p, "%d.%d.%d", &rule->month, &rule->week, &rule->weekday) != 3) return false;
    while (std::isdigit(static_cast<unsigned char>(*p)) || *p == '.') ++p;
    if (rule->month < 1 || rule->month > 12 || rule->week < 1 || rule->week > 5 || rule->weekday < 0 ||
        rule->weekday > 6) {
      return false;
    }
  } else if (std::isdigit(static_cast<unsigned char>(*p))) {
    rule->kind = 1;
    rule->day = std::atoi(p);
    while (std::isdigit(static_cast<unsigned char>(*p))) ++p;
    if (rule->day > 365) return false;
  } else {
    return false;
  }
  rule->time = 7200;
  if (*p == '/') {
    ++p;
    if (!ParseOffset(p, &rule->time, true)) return false;
  }
  return true;
}

std::shared_ptr<Zone> ParsePosixZone(const std::string& text) {
  const char* p = text.c_str();
  if (*p == ':') return nullptr;
  auto zone = std::make_shared<Zone>();
  zone->kind = ZoneKind::kPosix;
  std::string std_name, dst_name;
  if (!ParseAbbreviation(p, &std_name)) return nullptr;
  std::int32_t std_west = 0;
  if (!ParseOffset(p, &std_west, true)) return nullptr;
  zone->std_offset = -std_west;
  zone->std_abbr = Intern(std_name);
  zone->dst_abbr = zone->std_abbr;
  zone->dst_offset = zone->std_offset;
  if (*p != '\0') {
    if (!ParseAbbreviation(p, &dst_name)) return nullptr;
    zone->dst_abbr = Intern(dst_name);
    zone->dst_offset = zone->std_offset + 3600;
    if (*p != '\0' && *p != ',') {
      std::int32_t dst_west = 0;
      if (!ParseOffset(p, &dst_west, true)) return nullptr;
      zone->dst_offset = -dst_west;
    }
    zone->has_dst = true;
    if (*p == ',') {
      ++p;
      if (!ParseRule(p, &zone->start) || *p != ',') return nullptr;
      ++p;
      if (!ParseRule(p, &zone->end)) return nullptr;
    } else {
      const char* defaults = "M3.2.0,M11.1.0";
      ParseRule(defaults, &zone->start);
      ++defaults;
      ParseRule(defaults, &zone->end);
    }
    if (*p != '\0') return nullptr;
  }
  zone->iana = text;
  zone->fixed_abbreviations = true;
  return zone;
}

std::shared_ptr<Zone> ZoneFromName(const std::string& raw) {
  std::string name = raw;
  if (IsUtcName(name)) return MakeUtcZone();
  if (!name.empty() && name[0] == ':') name.erase(0, 1);
  if (auto posix = ParsePosixZone(name)) return posix;
  if (const ZoneName* entry = FindByIana(name)) {
    if (auto zone = MakeZoneForWindowsKey(entry->windows)) {
      zone->iana = name;
      return zone;
    }
  }
  return nullptr;
}

std::int64_t WindowsTransition(std::int64_t year, const SYSTEMTIME& when, std::int32_t offset) noexcept {
  std::int64_t days;
  if (when.wYear == 0) {
    const int month = when.wMonth;
    const int week = when.wDay;
    const int weekday = when.wDayOfWeek;
    const std::int64_t first = DaysFromCivil(year, month, 1);
    int day = 1 + (weekday - WeekdayOfDays(first) + 7) % 7 + (week - 1) * 7;
    while (day > DaysInMonth(year, month)) day -= 7;
    days = DaysFromCivil(year, month, day);
  } else {
    days = DaysFromCivil(when.wYear, when.wMonth, when.wDay);
  }
  std::int64_t local = days * kSecondsPerDay + when.wHour * 3600 + when.wMinute * 60 + when.wSecond;
  if (when.wMilliseconds != 0) ++local;
  return local - offset;
}

std::int64_t PosixTransition(std::int64_t year, const PosixRule& rule, std::int32_t offset) noexcept {
  std::int64_t days;
  if (rule.kind == 0) {
    days = DaysFromCivil(year, 1, 1) + rule.day - 1;
    if (IsLeap(year) && rule.day >= 60) ++days;
  } else if (rule.kind == 1) {
    days = DaysFromCivil(year, 1, 1) + rule.day;
  } else {
    const std::int64_t first = DaysFromCivil(year, rule.month, 1);
    int day = 1 + (rule.weekday - WeekdayOfDays(first) + 7) % 7 + (rule.week - 1) * 7;
    while (day > DaysInMonth(year, rule.month)) day -= 7;
    days = DaysFromCivil(year, rule.month, day);
  }
  return days * kSecondsPerDay + rule.time - offset;
}

YearRule RuleForYear(const Zone& zone, std::int64_t year) {
  YearRule rule;
  rule.std_offset = zone.std_offset;
  rule.dst_offset = zone.dst_offset;
  if (zone.kind == ZoneKind::kUtc) return rule;
  if (zone.kind == ZoneKind::kPosix) {
    rule.has_dst = zone.has_dst;
    if (rule.has_dst) {
      rule.dst_start = PosixTransition(year, zone.start, zone.std_offset);
      rule.dst_end = PosixTransition(year, zone.end, zone.dst_offset);
    }
    return rule;
  }
  const std::int64_t clamped = year < 1601 ? 1601 : (year > 30827 ? 30827 : year);
  if (clamped == year) {
    AcquireSRWLockShared(&zone.cache_lock);
    auto cached = zone.cache.find(year);
    const bool hit = cached != zone.cache.end();
    if (hit) rule = cached->second;
    ReleaseSRWLockShared(&zone.cache_lock);
    if (hit) return rule;
  }
  TIME_ZONE_INFORMATION information{};
  auto dynamic = zone.windows;
  if (!GetTimeZoneInformationForYear(static_cast<USHORT>(clamped), &dynamic, &information)) {
    information.Bias = zone.windows.Bias;
    information.StandardBias = zone.windows.StandardBias;
    information.DaylightBias = zone.windows.DaylightBias;
    information.StandardDate = zone.windows.StandardDate;
    information.DaylightDate = zone.windows.DaylightDate;
  }
  rule.std_offset = -(information.Bias + information.StandardBias) * 60;
  rule.dst_offset = -(information.Bias + information.DaylightBias) * 60;
  rule.has_dst = !zone.windows.DynamicDaylightTimeDisabled && information.DaylightDate.wMonth != 0 &&
                 information.StandardDate.wMonth != 0 && rule.std_offset != rule.dst_offset;
  if (rule.has_dst) {
    rule.dst_start = WindowsTransition(year, information.DaylightDate, rule.std_offset);
    rule.dst_end = WindowsTransition(year, information.StandardDate, rule.dst_offset);
  }
  if (clamped == year) {
    AcquireSRWLockExclusive(&zone.cache_lock);
    zone.cache[clamped] = rule;
    ReleaseSRWLockExclusive(&zone.cache_lock);
  }
  return rule;
}

bool InDst(const YearRule& rule, std::int64_t t) noexcept {
  if (!rule.has_dst) return false;
  if (rule.dst_start < rule.dst_end) return t >= rule.dst_start && t < rule.dst_end;
  return t >= rule.dst_start || t < rule.dst_end;
}

struct LocalInfo {
  std::int32_t offset;
  bool dst;
  const char* abbr;
};

LocalInfo InfoAt(const Zone& zone, std::int64_t t) {
  const YearRule rule = RuleForYear(zone, YearOfSeconds(t + zone.std_offset));
  const bool dst = InDst(rule, t);
  LocalInfo info;
  info.offset = dst ? rule.dst_offset : rule.std_offset;
  info.dst = dst;
  if (zone.fixed_abbreviations || zone.kind != ZoneKind::kWindows) {
    info.abbr = dst ? zone.dst_abbr : zone.std_abbr;
  } else if (info.offset == (dst ? zone.dst_offset : zone.std_offset)) {
    info.abbr = dst ? zone.dst_abbr : zone.std_abbr;
  } else {
    info.abbr = Intern(NumericAbbreviation(info.offset));
  }
  return info;
}

SRWLOCK g_zone_lock = SRWLOCK_INIT;
std::shared_ptr<Zone>* g_zone = nullptr;
std::string* g_zone_key = nullptr;
std::string* g_override = nullptr;
bool g_override_set = false;
std::int64_t g_zone_refreshed_ns = 0;

char* g_tzname[2] = {const_cast<char*>("UTC"), const_cast<char*>("UTC")};
abi::g_long g_timezone = 0;
int g_daylight = 0;

std::string CurrentZoneKey() {
  AcquireSRWLockShared(&g_zone_lock);
  const bool overridden = g_override_set;
  const std::string override_value = overridden ? *g_override : std::string();
  ReleaseSRWLockShared(&g_zone_lock);
  if (overridden) return "override:" + override_value;
  char buffer[256];
  const DWORD length = GetEnvironmentVariableA("TZ", buffer, sizeof(buffer));
  if (length == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND) return "host";
  if (length >= sizeof(buffer)) return "host";
  return std::string("tz:") + std::string(buffer, length);
}

std::shared_ptr<Zone> BuildZone(const std::string& key) {
  if (key.rfind("override:", 0) == 0 || key.rfind("tz:", 0) == 0) {
    const std::string name = key.substr(key.find(':') + 1);
    if (auto zone = ZoneFromName(name)) return zone;
  }
  return MakeHostZone();
}

void PublishTzVariables(const Zone& zone) noexcept {
  g_tzname[0] = const_cast<char*>(zone.std_abbr);
  g_tzname[1] = const_cast<char*>(zone.has_dst ? zone.dst_abbr : zone.std_abbr);
  g_timezone = -static_cast<abi::g_long>(zone.std_offset);
  g_daylight = zone.has_dst ? 1 : 0;
}

std::shared_ptr<Zone> ActiveZone(bool force) {
  const std::int64_t now = MonotonicNanoseconds();
  std::string key = CurrentZoneKey();
  AcquireSRWLockShared(&g_zone_lock);
  const bool usable = g_zone != nullptr && *g_zone_key == key && !force &&
                      (key != "host" || now - g_zone_refreshed_ns < 1000000000);
  std::shared_ptr<Zone> current = usable ? *g_zone : nullptr;
  ReleaseSRWLockShared(&g_zone_lock);
  if (current) return current;
  std::shared_ptr<Zone> built = BuildZone(key);
  AcquireSRWLockExclusive(&g_zone_lock);
  if (g_zone == nullptr) {
    g_zone = new std::shared_ptr<Zone>();
    g_zone_key = new std::string();
  }
  *g_zone = built;
  *g_zone_key = key;
  g_zone_refreshed_ns = now;
  PublishTzVariables(*built);
  ReleaseSRWLockExclusive(&g_zone_lock);
  return built;
}

bool BreakDown(std::int64_t utc, std::int32_t offset, bool dst, const char* abbr, Tm* out) noexcept {
  const std::int64_t local = utc + offset;
  const std::int64_t days = FloorDiv(local, kSecondsPerDay);
  const std::int64_t seconds = local - days * kSecondsPerDay;
  std::int64_t year;
  int month, day;
  CivilFromDays(days, &year, &month, &day);
  if (year - 1900 > INT32_MAX || year - 1900 < INT32_MIN) return false;
  Tm tm{};
  tm.tm_sec = static_cast<int>(seconds % 60);
  tm.tm_min = static_cast<int>((seconds / 60) % 60);
  tm.tm_hour = static_cast<int>(seconds / 3600);
  tm.tm_mday = day;
  tm.tm_mon = month - 1;
  tm.tm_year = static_cast<int>(year - 1900);
  tm.tm_wday = WeekdayOfDays(days);
  tm.tm_yday = static_cast<int>(days - DaysFromCivil(year, 1, 1));
  tm.tm_isdst = dst ? 1 : 0;
  tm.tm_gmtoff = offset;
  tm.tm_zone = abbr;
  *out = tm;
  return true;
}

bool WallSeconds(const Tm& tm, std::int64_t* out) noexcept {
  const std::int64_t months = static_cast<std::int64_t>(tm.tm_mon);
  const std::int64_t year = static_cast<std::int64_t>(tm.tm_year) + 1900 + FloorDiv(months, 12);
  const int month = static_cast<int>(FloorMod(months, 12)) + 1;
  if (year > 100000000000ll || year < -100000000000ll) return false;
  const std::int64_t days = DaysFromCivil(year, month, 1) + static_cast<std::int64_t>(tm.tm_mday) - 1;
  *out = days * kSecondsPerDay + static_cast<std::int64_t>(tm.tm_hour) * 3600 +
         static_cast<std::int64_t>(tm.tm_min) * 60 + tm.tm_sec;
  return true;
}

std::int64_t LocalToUtc(const Zone& zone, std::int64_t wall, int isdst_hint) {
  const YearRule rule = RuleForYear(zone, YearOfSeconds(wall));
  if (!rule.has_dst) return wall - rule.std_offset;
  const std::int64_t as_std = wall - rule.std_offset;
  const std::int64_t as_dst = wall - rule.dst_offset;
  if (isdst_hint == 0) return as_std;
  if (isdst_hint > 0) return as_dst;
  const bool std_valid = !InfoAt(zone, as_std).dst;
  const bool dst_valid = InfoAt(zone, as_dst).dst;
  if (dst_valid) return as_dst;
  if (std_valid) return as_std;
  return as_std;
}

abi::g_time MakeTime(const Zone* zone, Tm* tm) noexcept {
  if (tm == nullptr) return FailWith(guest::kEINVAL, abi::g_time{-1});
  std::int64_t wall;
  if (!WallSeconds(*tm, &wall)) return FailWith(guest::kEOVERFLOW, abi::g_time{-1});
  try {
    std::int64_t utc;
    Tm result;
    if (zone == nullptr) {
      utc = wall;
      if (!BreakDown(utc, 0, false, "UTC", &result)) return FailWith(guest::kEOVERFLOW, abi::g_time{-1});
    } else {
      utc = LocalToUtc(*zone, wall, tm->tm_isdst);
      const LocalInfo info = InfoAt(*zone, utc);
      if (!BreakDown(utc, info.offset, info.dst, info.abbr, &result)) {
        return FailWith(guest::kEOVERFLOW, abi::g_time{-1});
      }
    }
    *tm = result;
    return utc;
  } catch (...) {
    return FailWith(guest::kENOMEM, abi::g_time{-1});
  }
}

Tm* LocalTime(const Zone& zone, abi::g_time t, Tm* out) noexcept {
  try {
    const LocalInfo info = InfoAt(zone, t);
    if (!BreakDown(t, info.offset, info.dst, info.abbr, out)) return FailWith(guest::kEOVERFLOW, static_cast<Tm*>(nullptr));
    return out;
  } catch (...) {
    return FailWith(guest::kENOMEM, static_cast<Tm*>(nullptr));
  }
}

constexpr int kForceLower = -1;

const char* const kWeekdays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char* const kWeekdaysShort[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char* const kMonths[12] = {"January", "February", "March",     "April",   "May",      "June",
                                  "July",    "August",   "September", "October", "November", "December"};
const char* const kMonthsShort[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

class Output {
 public:
  Output(char* buffer, std::size_t size) noexcept : p_(buffer), end_(buffer + size) {}
  void Add(const char* text, int modifier) noexcept {
    for (; *text != '\0'; ++text) {
      if (p_ == end_) return;
      int c = static_cast<unsigned char>(*text);
      if (modifier == kForceLower) {
        c = std::tolower(c);
      } else if (modifier == '^') {
        c = std::toupper(c);
      } else if (modifier == '#') {
        c = std::isupper(c) ? std::tolower(c) : (std::islower(c) ? std::toupper(c) : c);
      }
      *p_++ = static_cast<char>(c);
    }
  }
  void Put(char c) noexcept {
    if (p_ != end_) *p_++ = c;
  }
  void Number(std::int64_t value, const char* format) noexcept {
    const char fill = format[0];
    const int width = format[1] == ' ' ? 0 : format[1] - '0';
    char digits[32];
    int count = 0;
    const bool negative = value < 0;
    std::uint64_t magnitude = negative ? 0 - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
    do {
      digits[count++] = static_cast<char>('0' + magnitude % 10);
      magnitude /= 10;
    } while (magnitude != 0);
    if (negative) digits[count++] = '-';
    while (count < width && count < 31) digits[count++] = fill;
    while (count > 0) Put(digits[--count]);
  }
  bool full() const noexcept { return p_ == end_; }
  char* position() const noexcept { return p_; }

 private:
  char* p_;
  char* end_;
};

const char* Pad(int modifier, const char* normal, const char* underscore, const char* dash, const char* zero) noexcept {
  switch (modifier) {
    case '_': return underscore;
    case '-': return dash;
    case '0': return zero;
    default: return normal;
  }
}

void YearConv(Output& out, std::int64_t year, bool top, bool bottom, int modifier) noexcept {
  std::int64_t lead = year / 100;
  std::int64_t trail = year % 100;
  if (trail < 0 && lead > 0) {
    trail += 100;
    --lead;
  } else if (lead < 0 && trail > 0) {
    trail -= 100;
    ++lead;
  }
  if (top) {
    if (lead == 0 && trail < 0) {
      out.Add("-0", modifier);
    } else {
      out.Number(lead, Pad(modifier, "02", " 2", "  ", "02"));
    }
  }
  if (bottom) out.Number(trail < 0 ? -trail : trail, Pad(modifier, "02", " 2", "  ", "02"));
}

void Format(Output& out, const char* format, const Tm& t, int depth);

void IsoWeek(const Tm& t, std::int64_t* iso_year, int* week) noexcept {
  std::int64_t year = static_cast<std::int64_t>(t.tm_year) + 1900;
  int yday = t.tm_yday;
  const int wday = t.tm_wday;
  for (;;) {
    const int length = IsLeap(year) ? 366 : 365;
    const int bottom = ((yday + 11 - wday) % 7) - 3;
    int top = bottom - (length % 7);
    if (top < -3) top += 7;
    top += length;
    if (yday >= top) {
      ++year;
      *week = 1;
      break;
    }
    if (yday >= bottom) {
      *week = 1 + (yday - bottom) / 7;
      break;
    }
    --year;
    yday += IsLeap(year) ? 366 : 365;
  }
  *iso_year = year;
}

const char* SafeZone(const Tm& t) noexcept {
  const char* zone = t.tm_zone;
  if (zone == nullptr || *zone == '\0') {
    if (t.tm_isdst == 0) {
      zone = g_tzname[0];
    } else if (t.tm_isdst > 0) {
      zone = g_tzname[1];
    }
    if (zone == nullptr || *zone == '\0') zone = "";
  }
  return zone;
}

void Format(Output& out, const char* format, const Tm& t, int depth) {
  if (depth > 4) return;
  for (; *format != '\0'; ++format) {
    if (out.full()) return;
    if (*format != '%') {
      out.Put(*format);
      continue;
    }
    int modifier = 0;
    bool done = false;
    while (!done) {
      const char c = *++format;
      done = true;
      switch (c) {
        case '\0': --format; break;
        case 'E':
        case 'O': done = false; break;
        case '_':
        case '-':
        case '0':
        case '^':
        case '#':
          modifier = c;
          done = false;
          break;
        case 'A': out.Add(t.tm_wday < 0 || t.tm_wday > 6 ? "?" : kWeekdays[t.tm_wday], modifier); break;
        case 'a': out.Add(t.tm_wday < 0 || t.tm_wday > 6 ? "?" : kWeekdaysShort[t.tm_wday], modifier); break;
        case 'B': out.Add(t.tm_mon < 0 || t.tm_mon > 11 ? "?" : kMonths[t.tm_mon], modifier); break;
        case 'b':
        case 'h': out.Add(t.tm_mon < 0 || t.tm_mon > 11 ? "?" : kMonthsShort[t.tm_mon], modifier); break;
        case 'C': YearConv(out, static_cast<std::int64_t>(t.tm_year) + 1900, true, false, modifier); break;
        case 'c': Format(out, "%a %b %e %H:%M:%S %Y", t, depth + 1); break;
        case 'D': Format(out, "%m/%d/%y", t, depth + 1); break;
        case 'd': out.Number(t.tm_mday, Pad(modifier, "02", " 2", "  ", "02")); break;
        case 'e': out.Number(t.tm_mday, Pad(modifier, " 2", " 2", "  ", "02")); break;
        case 'F': Format(out, "%Y-%m-%d", t, depth + 1); break;
        case 'H': out.Number(t.tm_hour, Pad(modifier, "02", " 2", "  ", "02")); break;
        case 'I': out.Number(t.tm_hour % 12 != 0 ? t.tm_hour % 12 : 12, Pad(modifier, "02", " 2", "  ", "02")); break;
        case 'j': out.Number(t.tm_yday + 1, Pad(modifier, "03", " 3", "  ", "03")); break;
        case 'k': out.Number(t.tm_hour, Pad(modifier, " 2", " 2", "  ", "02")); break;
        case 'l': out.Number(t.tm_hour % 12 != 0 ? t.tm_hour % 12 : 12, Pad(modifier, " 2", " 2", "  ", "02")); break;
        case 'M': out.Number(t.tm_min, Pad(modifier, "02", " 2", "  ", "02")); break;
        case 'm': out.Number(t.tm_mon + 1, Pad(modifier, "02", " 2", "  ", "02")); break;
        case 'n': out.Add("\n", modifier); break;
        case 'P':
        case 'p': out.Add(t.tm_hour >= 12 ? "PM" : "AM", c == 'P' ? kForceLower : modifier); break;
        case 'R': Format(out, "%H:%M", t, depth + 1); break;
        case 'r': Format(out, "%I:%M:%S %p", t, depth + 1); break;
        case 'S': out.Number(t.tm_sec, Pad(modifier, "02", " 2", "  ", "02")); break;
        case 's': {
          Tm copy{};
          copy.tm_sec = t.tm_sec;
          copy.tm_min = t.tm_min;
          copy.tm_hour = t.tm_hour;
          copy.tm_mday = t.tm_mday;
          copy.tm_mon = t.tm_mon;
          copy.tm_year = t.tm_year;
          copy.tm_isdst = t.tm_isdst;
          copy.tm_gmtoff = t.tm_gmtoff;
          const int saved = GuestErrno();
          const abi::g_time value = elixir_bionic_mktime(&copy);
          SetGuestErrno(saved);
          char buffer[32];
          std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
          out.Add(buffer, modifier);
          break;
        }
        case 'T': Format(out, "%H:%M:%S", t, depth + 1); break;
        case 't': out.Add("\t", modifier); break;
        case 'U': out.Number((t.tm_yday + 7 - t.tm_wday) / 7, Pad(modifier, "02", " 2", "  ", "02")); break;
        case 'u': out.Number(t.tm_wday == 0 ? 7 : t.tm_wday, "  "); break;
        case 'V':
        case 'G':
        case 'g': {
          std::int64_t iso_year;
          int week;
          IsoWeek(t, &iso_year, &week);
          if (c == 'V') {
            out.Number(week, Pad(modifier, "02", " 2", "  ", "02"));
          } else {
            YearConv(out, iso_year, c == 'G', true, modifier);
          }
          break;
        }
        case 'v': Format(out, "%e-%b-%Y", t, depth + 1); break;
        case 'W':
          out.Number((t.tm_yday + 7 - (t.tm_wday != 0 ? t.tm_wday - 1 : 6)) / 7, Pad(modifier, "02", " 2", "  ", "02"));
          break;
        case 'w': out.Number(t.tm_wday, "  "); break;
        case 'X': Format(out, "%H:%M:%S", t, depth + 1); break;
        case 'x': Format(out, "%m/%d/%y", t, depth + 1); break;
        case 'y': YearConv(out, static_cast<std::int64_t>(t.tm_year) + 1900, false, true, modifier); break;
        case 'Y': YearConv(out, static_cast<std::int64_t>(t.tm_year) + 1900, true, true, modifier); break;
        case 'Z': out.Add(SafeZone(t), modifier); break;
        case 'z': {
          std::int64_t diff = t.tm_gmtoff;
          bool negative = diff < 0;
          if (diff == 0) negative = SafeZone(t)[0] == '-';
          if (diff < 0) diff = -diff;
          out.Add(negative ? "-" : "+", modifier);
          diff /= 60;
          diff = (diff / 60) * 100 + diff % 60;
          out.Number(diff, Pad(modifier, "04", " 4", "  ", "04"));
          break;
        }
        case '+': Format(out, "%a %b %e %H:%M:%S %Z %Y", t, depth + 1); break;
        default:
          out.Put(c);
          break;
      }
    }
  }
}

std::size_t FormatTime(char* buffer, std::size_t size, const char* format, const Tm* t) noexcept {
  if (buffer == nullptr || format == nullptr || t == nullptr || size == 0) return 0;
  try {
    ActiveZone(false);
    Output out(buffer, size);
    Format(out, format, *t, 0);
    if (out.full()) {
      SetGuestErrno(guest::kERANGE);
      return 0;
    }
    *out.position() = '\0';
    return static_cast<std::size_t>(out.position() - buffer);
  } catch (...) {
    return 0;
  }
}

struct ParseState {
  int century = 1900;
  int relyear = -1;
  unsigned fields = 0;
};
constexpr unsigned kFieldMon = 1, kFieldMday = 2, kFieldWday = 4, kFieldYday = 8, kFieldYear = 16;

bool ConvNum(const unsigned char** text, int* out, int low, int high) noexcept {
  if (**text < '0' || **text > '9') return false;
  int result = 0;
  int remaining = high;
  do {
    result = result * 10 + (*(*text)++ - '0');
    remaining /= 10;
  } while (result * 10 <= high && remaining != 0 && **text >= '0' && **text <= '9');
  if (result < low || result > high) return false;
  *out = result;
  return true;
}

bool MatchName(const unsigned char** text, const char* const* names, const char* const* short_names, int count,
               int* index) noexcept {
  for (int i = 0; i < count; ++i) {
    std::size_t length = std::strlen(names[i]);
    if (_strnicmp(names[i], reinterpret_cast<const char*>(*text), length) == 0) {
      *index = i;
      *text += length;
      return true;
    }
    length = std::strlen(short_names[i]);
    if (_strnicmp(short_names[i], reinterpret_cast<const char*>(*text), length) == 0) {
      *index = i;
      *text += length;
      return true;
    }
  }
  return false;
}

const unsigned char* Parse(const unsigned char* text, const char* format, Tm* tm, ParseState& state, int depth) {
  if (depth > 4) return nullptr;
  while (*format != '\0') {
    char c = *format;
    if (std::isspace(static_cast<unsigned char>(c))) {
      while (std::isspace(*text)) ++text;
      ++format;
      continue;
    }
    ++format;
    if (c != '%') {
      if (static_cast<unsigned char>(c) != *text++) return nullptr;
      continue;
    }
    c = *format++;
    while (c == 'E' || c == 'O') c = *format++;
    int value = 0;
    switch (c) {
      case '%':
        if (*text++ != '%') return nullptr;
        break;
      case 'c': text = Parse(text, "%a %b %e %H:%M:%S %Y", tm, state, depth + 1); break;
      case 'D': text = Parse(text, "%m/%d/%y", tm, state, depth + 1); break;
      case 'F': text = Parse(text, "%Y-%m-%d", tm, state, depth + 1); break;
      case 'R': text = Parse(text, "%H:%M", tm, state, depth + 1); break;
      case 'r': text = Parse(text, "%I:%M:%S %p", tm, state, depth + 1); break;
      case 'T': text = Parse(text, "%H:%M:%S", tm, state, depth + 1); break;
      case 'v': text = Parse(text, "%e-%b-%Y", tm, state, depth + 1); break;
      case 'X': text = Parse(text, "%H:%M:%S", tm, state, depth + 1); break;
      case 'x': text = Parse(text, "%m/%d/%y", tm, state, depth + 1); break;
      case 'A':
      case 'a':
        if (!MatchName(&text, kWeekdays, kWeekdaysShort, 7, &value)) return nullptr;
        tm->tm_wday = value;
        state.fields |= kFieldWday;
        break;
      case 'B':
      case 'b':
      case 'h':
        if (!MatchName(&text, kMonths, kMonthsShort, 12, &value)) return nullptr;
        tm->tm_mon = value;
        state.fields |= kFieldMon;
        break;
      case 'C':
        if (!ConvNum(&text, &value, 0, 99)) return nullptr;
        state.century = value * 100;
        break;
      case 'e':
        if (std::isspace(*text)) ++text;
        [[fallthrough]];
      case 'd':
        if (!ConvNum(&text, &tm->tm_mday, 1, 31)) return nullptr;
        state.fields |= kFieldMday;
        break;
      case 'k':
      case 'H':
        if (!ConvNum(&text, &tm->tm_hour, 0, 23)) return nullptr;
        break;
      case 'l':
      case 'I':
        if (!ConvNum(&text, &tm->tm_hour, 1, 12)) return nullptr;
        break;
      case 'j':
        if (!ConvNum(&text, &tm->tm_yday, 1, 366)) return nullptr;
        --tm->tm_yday;
        state.fields |= kFieldYday;
        break;
      case 'M':
        if (!ConvNum(&text, &tm->tm_min, 0, 59)) return nullptr;
        break;
      case 'm':
        if (!ConvNum(&text, &tm->tm_mon, 1, 12)) return nullptr;
        --tm->tm_mon;
        state.fields |= kFieldMon;
        break;
      case 'P':
      case 'p':
        if (_strnicmp("AM", reinterpret_cast<const char*>(text), 2) == 0) {
          if (tm->tm_hour > 12) return nullptr;
          if (tm->tm_hour == 12) tm->tm_hour = 0;
          text += 2;
        } else if (_strnicmp("PM", reinterpret_cast<const char*>(text), 2) == 0) {
          if (tm->tm_hour > 12) return nullptr;
          if (tm->tm_hour < 12) tm->tm_hour += 12;
          text += 2;
        } else {
          return nullptr;
        }
        break;
      case 'S':
        if (!ConvNum(&text, &tm->tm_sec, 0, 60)) return nullptr;
        break;
      case 's': {
        char* end = nullptr;
        const long long seconds = std::strtoll(reinterpret_cast<const char*>(text), &end, 10);
        if (end == reinterpret_cast<const char*>(text) || seconds < 0 || seconds == LLONG_MAX) return nullptr;
        text = reinterpret_cast<const unsigned char*>(end);
        if (LocalTime(*ActiveZone(false), seconds, tm) == nullptr) return nullptr;
        state.fields = 0xffff;
        break;
      }
      case 'U':
      case 'W':
        if (!ConvNum(&text, &value, 0, 53)) return nullptr;
        break;
      case 'w':
        if (!ConvNum(&text, &tm->tm_wday, 0, 6)) return nullptr;
        state.fields |= kFieldWday;
        break;
      case 'u':
        if (!ConvNum(&text, &value, 1, 7)) return nullptr;
        tm->tm_wday = value % 7;
        state.fields |= kFieldWday;
        break;
      case 'g':
        if (!ConvNum(&text, &value, 0, 99)) return nullptr;
        break;
      case 'G':
        do {
          ++text;
        } while (std::isdigit(*text));
        break;
      case 'V':
        if (!ConvNum(&text, &value, 0, 53)) return nullptr;
        break;
      case 'Y':
        if (!ConvNum(&text, &value, 0, 9999)) return nullptr;
        state.relyear = -1;
        tm->tm_year = value - 1900;
        state.fields |= kFieldYear;
        break;
      case 'y':
        if (!ConvNum(&text, &state.relyear, 0, 99)) return nullptr;
        break;
      case 'Z': {
        ActiveZone(false);
        if (std::strncmp(reinterpret_cast<const char*>(text), "GMT", 3) == 0 ||
            std::strncmp(reinterpret_cast<const char*>(text), "UTC", 3) == 0) {
          tm->tm_isdst = 0;
          tm->tm_gmtoff = 0;
          tm->tm_zone = text[0] == 'G' ? "GMT" : "UTC";
          text += 3;
        } else {
          int found = -1;
          for (int i = 0; i < 2 && found < 0; ++i) {
            const std::size_t length = std::strlen(g_tzname[i]);
            if (length > 0 && _strnicmp(g_tzname[i], reinterpret_cast<const char*>(text), length) == 0) {
              found = i;
              text += length;
            }
          }
          if (found < 0) return nullptr;
          tm->tm_isdst = found;
          tm->tm_gmtoff = -g_timezone;
          tm->tm_zone = g_tzname[found];
        }
        break;
      }
      case 'z': {
        while (std::isspace(*text)) ++text;
        bool negative = false;
        switch (*text++) {
          case 'G':
            if (*text++ != 'M') return nullptr;
            [[fallthrough]];
          case 'U':
            if (*text++ != 'T') return nullptr;
            [[fallthrough]];
          case 'Z':
            tm->tm_isdst = 0;
            tm->tm_gmtoff = 0;
            tm->tm_zone = "UTC";
            continue;
          case '+': break;
          case '-': negative = true; break;
          default: {
            --text;
            static const char* const kStandard[] = {"EST", "CST", "MST", "PST"};
            static const char* const kDaylight[] = {"EDT", "CDT", "MDT", "PDT"};
            for (int i = 0; i < 4; ++i) {
              if (_strnicmp(kStandard[i], reinterpret_cast<const char*>(text), 3) == 0) {
                tm->tm_gmtoff = (-5 - i) * 3600;
                tm->tm_zone = kStandard[i];
                text += 3;
                goto next_conversion;
              }
              if (_strnicmp(kDaylight[i], reinterpret_cast<const char*>(text), 3) == 0) {
                tm->tm_isdst = 1;
                tm->tm_gmtoff = (-4 - i) * 3600;
                tm->tm_zone = kDaylight[i];
                text += 3;
                goto next_conversion;
              }
            }
            return nullptr;
          }
        }
        if (!std::isdigit(text[0]) || !std::isdigit(text[1])) return nullptr;
        {
          int offset = ((text[0] - '0') * 10 + (text[1] - '0')) * 3600;
          text += 2;
          if (*text == ':') ++text;
          if (std::isdigit(*text)) {
            offset += (*text++ - '0') * 600;
            if (!std::isdigit(*text)) return nullptr;
            offset += (*text++ - '0') * 60;
          }
          tm->tm_isdst = 0;
          tm->tm_gmtoff = negative ? -offset : offset;
          tm->tm_zone = nullptr;
        }
        break;
      }
      case 'n':
      case 't':
        while (std::isspace(*text)) ++text;
        break;
      default: return nullptr;
    }
    if (text == nullptr) return nullptr;
  next_conversion:;
  }
  return text;
}

char* ParseTime(const char* buffer, const char* format, Tm* tm) noexcept {
  if (buffer == nullptr || format == nullptr || tm == nullptr) return nullptr;
  try {
    ParseState state;
    const unsigned char* end = Parse(reinterpret_cast<const unsigned char*>(buffer), format, tm, state, 0);
    if (end == nullptr) return nullptr;
    if (state.relyear != -1) {
      if (state.century == 1900) {
        tm->tm_year = state.relyear <= 68 ? state.relyear + 100 : state.relyear;
      } else {
        tm->tm_year = state.relyear + state.century - 1900;
      }
      state.fields |= kFieldYear;
    }
    if ((state.fields & kFieldYear) != 0) {
      const std::int64_t year = static_cast<std::int64_t>(tm->tm_year) + 1900;
      if ((state.fields & kFieldYday) == 0 && (state.fields & kFieldMon) != 0 && (state.fields & kFieldMday) != 0) {
        tm->tm_yday = tm->tm_mday - 1;
        for (int month = 0; month < tm->tm_mon; ++month) tm->tm_yday += DaysInMonth(year, month + 1);
        state.fields |= kFieldYday;
      }
      if ((state.fields & kFieldYday) != 0) {
        int days = tm->tm_yday;
        if ((state.fields & kFieldWday) == 0) {
          tm->tm_wday = WeekdayOfDays(DaysFromCivil(year, 1, 1) + tm->tm_yday);
        }
        if ((state.fields & kFieldMon) == 0) {
          tm->tm_mon = 0;
          while (tm->tm_mon < 12 && days >= DaysInMonth(year, tm->tm_mon + 1)) days -= DaysInMonth(year, ++tm->tm_mon);
        }
        if ((state.fields & kFieldMday) == 0) tm->tm_mday = days + 1;
      }
    }
    return const_cast<char*>(reinterpret_cast<const char*>(end));
  } catch (...) {
    return nullptr;
  }
}

std::string EncodeUtf8(const abi::g_wchar* text) {
  std::string out;
  for (; *text != 0; ++text) {
    const std::uint32_t c = *text;
    if (c < 0x80) {
      out.push_back(static_cast<char>(c));
    } else if (c < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (c >> 6)));
      out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else if (c < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (c >> 12)));
      out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | ((c >> 18) & 0x07)));
      out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
  }
  return out;
}

}

void SetTimeZoneOverride(const char* tz) {
  AcquireSRWLockExclusive(&g_zone_lock);
  if (g_override == nullptr) g_override = new std::string();
  g_override_set = tz != nullptr && *tz != '\0';
  *g_override = g_override_set ? tz : "";
  ReleaseSRWLockExclusive(&g_zone_lock);
  ActiveZone(true);
}

std::string HostTimeZoneIanaName() {
  try {
    return MakeHostZone()->iana;
  } catch (...) {
    return "Etc/UTC";
  }
}

}

using guest::Tm;

ELIXIR_BIONIC_FN(void, tzset)() noexcept {
  try {
    core::ActiveZone(true);
  } catch (...) {
  }
}

ELIXIR_BIONIC_FN(Tm*, gmtime_r)(const abi::g_time* t, Tm* out) noexcept {
  if (t == nullptr || out == nullptr) return FailWith(guest::kEINVAL, static_cast<Tm*>(nullptr));
  if (!core::BreakDown(*t, 0, false, "UTC", out)) return FailWith(guest::kEOVERFLOW, static_cast<Tm*>(nullptr));
  return out;
}

ELIXIR_BIONIC_FN(Tm*, gmtime)(const abi::g_time* t) noexcept {
  static thread_local Tm result;
  return elixir_bionic_gmtime_r(t, &result);
}

ELIXIR_BIONIC_FN(Tm*, localtime_r)(const abi::g_time* t, Tm* out) noexcept {
  if (t == nullptr || out == nullptr) return FailWith(guest::kEINVAL, static_cast<Tm*>(nullptr));
  try {
    return core::LocalTime(*core::ActiveZone(false), *t, out);
  } catch (...) {
    return FailWith(guest::kENOMEM, static_cast<Tm*>(nullptr));
  }
}

ELIXIR_BIONIC_FN(Tm*, localtime)(const abi::g_time* t) noexcept {
  static thread_local Tm result;
  return elixir_bionic_localtime_r(t, &result);
}

ELIXIR_BIONIC_FN(abi::g_time, mktime)(Tm* tm) noexcept {
  try {
    std::shared_ptr<core::Zone> zone = core::ActiveZone(false);
    return core::MakeTime(zone.get(), tm);
  } catch (...) {
    return FailWith(guest::kENOMEM, abi::g_time{-1});
  }
}

ELIXIR_BIONIC_FN(abi::g_time, timegm)(Tm* tm) noexcept { return core::MakeTime(nullptr, tm); }

ELIXIR_BIONIC_FN(char*, asctime_r)(const Tm* tm, char* buffer) noexcept {
  if (buffer == nullptr) return FailWith(guest::kEINVAL, static_cast<char*>(nullptr));
  if (tm == nullptr) {
    std::strcpy(buffer, "??? ??? ?? ??:??:?? ????\n");
    return FailWith(guest::kEINVAL, buffer);
  }
  const char* weekday = tm->tm_wday < 0 || tm->tm_wday > 6 ? "???" : core::kWeekdaysShort[tm->tm_wday];
  const char* month = tm->tm_mon < 0 || tm->tm_mon > 11 ? "???" : core::kMonthsShort[tm->tm_mon];
  char year[32];
  std::snprintf(year, sizeof(year), "%lld", static_cast<long long>(tm->tm_year) + 1900);
  char result[64];
  std::snprintf(result, sizeof(result),
                std::strlen(year) <= 4 ? "%s %s%3d %.2d:%.2d:%.2d %-4s\n" : "%s %s%3d %.2d:%.2d:%.2d     %s\n",
                weekday, month, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, year);
  if (std::strlen(result) >= 26) return FailWith(guest::kEOVERFLOW, static_cast<char*>(nullptr));
  std::strcpy(buffer, result);
  return buffer;
}

ELIXIR_BIONIC_FN(char*, asctime)(const Tm* tm) noexcept {
  static thread_local char buffer[64];
  if (tm == nullptr) return elixir_bionic_asctime_r(tm, buffer);
  const char* weekday = tm->tm_wday < 0 || tm->tm_wday > 6 ? "???" : core::kWeekdaysShort[tm->tm_wday];
  const char* month = tm->tm_mon < 0 || tm->tm_mon > 11 ? "???" : core::kMonthsShort[tm->tm_mon];
  char year[32];
  std::snprintf(year, sizeof(year), "%lld", static_cast<long long>(tm->tm_year) + 1900);
  std::snprintf(buffer, sizeof(buffer),
                std::strlen(year) <= 4 ? "%s %s%3d %.2d:%.2d:%.2d %-4s\n" : "%s %s%3d %.2d:%.2d:%.2d     %s\n",
                weekday, month, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, year);
  return buffer;
}

ELIXIR_BIONIC_FN(char*, ctime_r)(const abi::g_time* t, char* buffer) noexcept {
  Tm tm;
  if (elixir_bionic_localtime_r(t, &tm) == nullptr) return nullptr;
  return elixir_bionic_asctime_r(&tm, buffer);
}

ELIXIR_BIONIC_FN(char*, ctime)(const abi::g_time* t) noexcept {
  Tm tm;
  if (elixir_bionic_localtime_r(t, &tm) == nullptr) return nullptr;
  return elixir_bionic_asctime(&tm);
}

ELIXIR_BIONIC_FN(abi::g_size, strftime)(char* buffer, abi::g_size size, const char* format, const Tm* tm) noexcept {
  return core::FormatTime(buffer, size, format, tm);
}

ELIXIR_BIONIC_FN(abi::g_size, strftime_l)(char* buffer, abi::g_size size, const char* format, const Tm* tm,
                                          void* ) noexcept {
  return core::FormatTime(buffer, size, format, tm);
}

ELIXIR_BIONIC_FN(char*, strptime)(const char* buffer, const char* format, Tm* tm) noexcept {
  return core::ParseTime(buffer, format, tm);
}

ELIXIR_BIONIC_FN(char*, strptime_l)(const char* buffer, const char* format, Tm* tm, void* ) noexcept {
  return core::ParseTime(buffer, format, tm);
}

ELIXIR_BIONIC_FN(abi::g_size, wcsftime)(abi::g_wchar* buffer, abi::g_size size, const abi::g_wchar* format,
                                        const Tm* tm) noexcept {
  if (buffer == nullptr || format == nullptr || tm == nullptr || size == 0) return 0;
  try {
    const std::string narrow_format = core::EncodeUtf8(format);
    std::string narrow(size * 4 + 1, '\0');
    const std::size_t length = core::FormatTime(narrow.data(), narrow.size(), narrow_format.c_str(), tm);
    if (length == 0 && !narrow_format.empty()) return 0;
    std::size_t out = 0;
    for (std::size_t i = 0; i < length;) {
      const auto byte = static_cast<unsigned char>(narrow[i]);
      std::uint32_t code;
      int extra;
      if (byte < 0x80) {
        code = byte;
        extra = 0;
      } else if ((byte & 0xE0) == 0xC0) {
        code = byte & 0x1F;
        extra = 1;
      } else if ((byte & 0xF0) == 0xE0) {
        code = byte & 0x0F;
        extra = 2;
      } else {
        code = byte & 0x07;
        extra = 3;
      }
      ++i;
      for (int k = 0; k < extra && i < length; ++k) code = (code << 6) | (static_cast<unsigned char>(narrow[i++]) & 0x3F);
      if (out + 1 >= size) return 0;
      buffer[out++] = code;
    }
    buffer[out] = 0;
    return out;
  } catch (...) {
    return 0;
  }
}

namespace {
struct TimezoneHandle {
  std::shared_ptr<core::Zone> zone;
};
}

ELIXIR_BIONIC_FN(void*, tzalloc)(const char* name) noexcept {
  try {
    std::shared_ptr<core::Zone> zone;
    if (name == nullptr) {
      zone = core::ActiveZone(false);
    } else {
      zone = core::ZoneFromName(name);
      if (!zone) return FailWith(guest::kEINVAL, static_cast<void*>(nullptr));
    }
    return new TimezoneHandle{zone};
  } catch (...) {
    return FailWith(guest::kENOMEM, static_cast<void*>(nullptr));
  }
}

ELIXIR_BIONIC_FN(void, tzfree)(void* handle) noexcept { delete static_cast<TimezoneHandle*>(handle); }

ELIXIR_BIONIC_FN(Tm*, localtime_rz)(void* handle, const abi::g_time* t, Tm* out) noexcept {
  if (handle == nullptr) return elixir_bionic_gmtime_r(t, out);
  if (t == nullptr || out == nullptr) return FailWith(guest::kEINVAL, static_cast<Tm*>(nullptr));
  return core::LocalTime(*static_cast<TimezoneHandle*>(handle)->zone, *t, out);
}

ELIXIR_BIONIC_FN(abi::g_time, mktime_z)(void* handle, Tm* tm) noexcept {
  if (handle == nullptr) return core::MakeTime(nullptr, tm);
  return core::MakeTime(static_cast<TimezoneHandle*>(handle)->zone.get(), tm);
}

namespace core {

void RegisterTimeConversionExports(ExportTable& table) {
  ActiveZone(false);
#define ELIXIR_ADD(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_ADD(tzset);
  ELIXIR_ADD(gmtime);
  ELIXIR_ADD(gmtime_r);
  ELIXIR_ADD(localtime);
  ELIXIR_ADD(localtime_r);
  ELIXIR_ADD(mktime);
  ELIXIR_ADD(timegm);
  table.Add(kLibc, "timelocal", reinterpret_cast<void*>(&elixir_bionic_mktime));
  ELIXIR_ADD(asctime);
  ELIXIR_ADD(asctime_r);
  ELIXIR_ADD(ctime);
  ELIXIR_ADD(ctime_r);
  ELIXIR_ADD(strftime);
  ELIXIR_ADD(strftime_l);
  ELIXIR_ADD(strptime);
  ELIXIR_ADD(strptime_l);
  ELIXIR_ADD(wcsftime);
  table.Add(kLibc, "wcsftime_l", reinterpret_cast<void*>(&elixir_bionic_wcsftime));
  ELIXIR_ADD(tzalloc);
  ELIXIR_ADD(tzfree);
  ELIXIR_ADD(localtime_rz);
  ELIXIR_ADD(mktime_z);
#undef ELIXIR_ADD
  table.Add(kLibc, "tzname", reinterpret_cast<void*>(g_tzname));
  table.Add(kLibc, "timezone", &g_timezone);
  table.Add(kLibc, "daylight", &g_daylight);
}

}
}
