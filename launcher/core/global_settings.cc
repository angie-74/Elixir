// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "core/global_settings.h"

#include <shlwapi.h>
#include <windows.h>
#include <wrl/client.h>
#include <xmllite.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <string_view>
#include <vector>

#include "core/instances.h"
#include "core/util.h"
#include "core/i18n.h"

namespace elixir::launcher {
namespace {

using Microsoft::WRL::ComPtr;

constexpr LONGLONG kMaxBytes = 4ll << 20;

struct Spec {
  GlobalSetting field;
  const char* type;
  const char* name;
  const wchar_t* xml_type;
  const wchar_t* xml_name;
};

constexpr std::array<Spec, 6> kSpecs{{
    {GlobalSetting::kQualityLevel, "token", "SavedQualityLevel", L"token", L"SavedQualityLevel"},
    {GlobalSetting::kFramerateCap, "int", "FramerateCap", L"int", L"FramerateCap"},
    {GlobalSetting::kTextSize, "token", "PreferredTextSize", L"token", L"PreferredTextSize"},
    {GlobalSetting::kTransparency, "float", "PreferredTransparency", L"float", L"PreferredTransparency"},
    {GlobalSetting::kMouseSensitivity, "float", "MouseSensitivity", L"float", L"MouseSensitivity"},
    {GlobalSetting::kReducedMotion, "bool", "ReducedMotion", L"bool", L"ReducedMotion"},
}};

bool Fail(std::wstring* error, std::wstring message) {
  if (error) *error = std::move(message);
  return false;
}

struct Handle {
  HANDLE value = INVALID_HANDLE_VALUE;
  ~Handle() {
    if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value);
  }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  explicit Handle(HANDLE raw = INVALID_HANDLE_VALUE) : value(raw) {}
};

struct Scalar {
  unsigned occurrences = 0;
  std::size_t begin = std::string::npos;
  std::size_t end = 0;
  bool self_closing = false;

  bool editable() const { return occurrences == 1 && begin != std::string::npos; }
};

struct Property {
  std::string name;
  std::size_t begin = 0;
  std::size_t end = std::string::npos;
};

struct Document {
  std::string bytes;
  std::array<Scalar, kSpecs.size()> fields{};
  std::vector<Property> properties;
  std::size_t properties_end = std::string::npos;
  bool empty_properties = false;
};

struct Markup {
  std::size_t begin = 0, end = 0;
};

bool NextTag(const std::string& bytes, std::size_t& cursor, Markup& tag) {
  for (;;) {
    tag.begin = bytes.find('<', cursor);
    if (tag.begin == std::string::npos) return false;
    const std::string_view rest(bytes.data() + tag.begin, bytes.size() - tag.begin);
    std::string_view stop;
    if (rest.substr(0, 4) == "<!--")
      stop = "-->";
    else if (rest.substr(0, 9) == "<![CDATA[")
      stop = "]]>";
    else if (rest.substr(0, 2) == "<?")
      stop = "?>";
    if (!stop.empty()) {
      const auto end = bytes.find(stop, tag.begin + 2);
      if (end == std::string::npos) return false;
      cursor = end + stop.size();
      continue;
    }
    char quote = 0;
    for (auto pos = tag.begin + 1; pos < bytes.size(); ++pos) {
      const char c = bytes[pos];
      if (quote) {
        if (c == quote) quote = 0;
      } else if (c == '\'' || c == '"') {
        quote = c;
      } else if (c == '>') {
        cursor = tag.end = pos + 1;
        return true;
      }
    }
    return false;
  }
}

std::wstring_view Attribute(IXmlReader* reader, const wchar_t* name) {
  if (reader->MoveToAttributeByName(name, nullptr) != S_OK) return {};
  const wchar_t* value = nullptr;
  UINT length = 0;
  std::wstring_view result;
  if (SUCCEEDED(reader->GetValue(&value, &length))) result = {value, length};
  return result;
}

bool Parse(Document& doc) {
  if (doc.bytes.empty() || doc.bytes.find('\0') != std::string::npos) return false;
  ComPtr<IXmlReader> reader;
  if (FAILED(CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(reader.GetAddressOf()), nullptr)))
    return false;
  ComPtr<IStream> stream;
  stream.Attach(
      SHCreateMemStream(reinterpret_cast<const BYTE*>(doc.bytes.data()), static_cast<UINT>(doc.bytes.size())));
  if (!stream || FAILED(reader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit)) ||
      FAILED(reader->SetInput(stream.Get())))
    return false;

  bool root = false, user_item = false, properties = false, have_properties = false, open_property = false;
  std::size_t cursor = 0;
  XmlNodeType kind;
  HRESULT hr;
  while ((hr = reader->Read(&kind)) == S_OK) {
    if (kind != XmlNodeType_Element && kind != XmlNodeType_EndElement) continue;
    Markup tag;
    UINT depth = 0, length = 0;
    const wchar_t* name_data = nullptr;
    if (!NextTag(doc.bytes, cursor, tag) || FAILED(reader->GetDepth(&depth)) ||
        FAILED(reader->GetQualifiedName(&name_data, &length)))
      return false;
    const std::wstring_view name(name_data, length);
    if (kind == XmlNodeType_EndElement) {
      if (depth == 3 && properties && name == L"Properties") {
        doc.properties_end = tag.begin;
        properties = false;
      } else if (depth == 2 && name == L"Item") {
        user_item = false;
      } else if (depth == 4 && properties && open_property) {
        doc.properties.back().end = tag.end;
        open_property = false;
      }
      continue;
    }
    const bool empty = reader->IsEmptyElement() != FALSE;
    if (depth == 0) {
      root = name == L"roblox";
    } else if (depth == 1) {
      user_item = root && !empty && name == L"Item" && Attribute(reader.Get(), L"class") == L"UserGameSettings";
      reader->MoveToElement();
    } else if (depth == 2 && user_item && name == L"Properties") {
      if (have_properties) return false;
      have_properties = true;
      properties = !empty;
      if (empty) {
        doc.properties_end = tag.end - 2;
        doc.empty_properties = true;
      }
    } else if (depth == 3 && properties) {
      const std::wstring type(name);
      const auto key = Attribute(reader.Get(), L"name");
      if (open_property) return false;
      doc.properties.push_back(Property{WideToUtf8(key), tag.begin, empty ? tag.end : std::string::npos});
      open_property = !empty;
      for (std::size_t i = 0; i < kSpecs.size(); ++i) {
        if (key != kSpecs[i].xml_name) continue;
        Scalar& field = doc.fields[i];
        ++field.occurrences;
        if (type != kSpecs[i].xml_type) break;
        if (empty) {
          field.begin = tag.end - 2;
          field.end = tag.end;
          field.self_closing = true;
        } else {
          const auto close = doc.bytes.find('<', tag.end);
          if (close != std::string::npos && doc.bytes.compare(close, 2, "</") == 0) {
            field.begin = tag.end;
            field.end = close;
          }
        }
        break;
      }
      reader->MoveToElement();
    }
  }
  return hr == S_FALSE && root && have_properties && !open_property && doc.properties_end != std::string::npos;
}

std::string_view TextOf(const Document& doc, const Property& property) {
  return std::string_view(doc.bytes).substr(property.begin, property.end - property.begin);
}

template <class T>
std::optional<T> Number(std::string_view text) {
  T result{};
  auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) return {};
  return result;
}

struct ScalarText {
  std::string_view tag;
  std::string text;
};

std::optional<ScalarText> ScalarOf(std::string_view element) {
  const std::size_t open = element.find('>');
  const std::size_t close = element.rfind("</");
  if (open == std::string_view::npos || close == std::string_view::npos || close <= open) return std::nullopt;
  ScalarText scalar{element.substr(0, open + 1), TrimAscii(element.substr(open + 1, close - open - 1))};
  if (scalar.text.empty() || scalar.text.find('<') != std::string::npos) return std::nullopt;
  return scalar;
}

bool SameSetting(std::string_view a, std::string_view b) {
  if (a == b) return true;
  const std::optional<ScalarText> left = ScalarOf(a);
  const std::optional<ScalarText> right = ScalarOf(b);
  if (!left || !right || left->tag != right->tag) return false;
  const std::optional<double> x = Number<double>(left->text);
  const std::optional<double> y = Number<double>(right->text);
  if (!x || !y) return false;
  const bool decimal = (left->text + right->text).find_first_of(".eE") != std::string::npos;
  return decimal ? static_cast<float>(*x) == static_cast<float>(*y) : *x == *y;
}

const Property* OnlyProperty(const Document& doc, const std::string& name, int* occurrences) {
  const Property* found = nullptr;
  int count = 0;
  for (const Property& property : doc.properties) {
    if (property.name != name) continue;
    ++count;
    found = &property;
  }
  if (occurrences != nullptr) *occurrences = count;
  return count == 1 ? found : nullptr;
}

bool SetParsed(GlobalSettings& out, GlobalSetting field, std::string_view raw) {
  const std::string text = TrimAscii(raw);
  if (text.empty()) return false;
  if (field == GlobalSetting::kReducedMotion) {
    const std::string lower = ToLowerAscii(text);
    if (lower != "true" && lower != "false") return false;
    out.reduced_motion = lower == "true";
    return true;
  }
  if (field == GlobalSetting::kTransparency || field == GlobalSetting::kMouseSensitivity) {
    const bool transparency = field == GlobalSetting::kTransparency;
    auto n = Number<float>(text);
    if (!n || !std::isfinite(*n) || *n < 0 || *n > (transparency ? 1.f : 4.f)) return false;
    (transparency ? out.transparency : out.mouse_sensitivity) = n;
    return true;
  }
  auto n = Number<int>(text);
  if (!n) return false;
  switch (field) {
    case GlobalSetting::kQualityLevel:
      if (*n < 0 || *n > 10) return false;
      out.quality_level = n;
      return true;
    case GlobalSetting::kFramerateCap:
      if (*n != -1 && *n < 1) return false;
      out.framerate_cap = n;
      return true;
    case GlobalSetting::kTextSize:
      if (*n < 1 || *n > 4) return false;
      out.text_size = n;
      return true;
    default:
      return false;
  }
}

struct SettingsFile {
  std::wstring path;
  Handle handle;
  DWORD attributes = 0;
  bool Open(const LauncherPaths& paths, bool edit, bool change_attribute, std::wstring* error) {
    path = GlobalSettingsFile(paths);
    const auto absent =
        T(L"Open Roblox once in this Elixir profile to create "
        L"its global settings.");
    if (!IsRealDirectoryBelow(paths.data_root, ParentDirectory(path))) return Fail(error, absent);
    const DWORD access = GENERIC_READ | (change_attribute ? FILE_WRITE_ATTRIBUTES : 0);
    handle.value =
        CreateFileW(path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_DELETE | (edit ? 0 : FILE_SHARE_WRITE), nullptr,
                    OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle.value == INVALID_HANDLE_VALUE) {
      const auto code = GetLastError();
      if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND) return Fail(error, absent);
      return Fail(error, WithErrorCause(T(L"Couldn’t open the Roblox settings file."), code));
    }
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(handle.value, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) ||
        (edit && info.nNumberOfLinks != 1))
      return Fail(error, T(L"Roblox settings must be a regular file in this profile."));
    attributes = info.dwFileAttributes;
    return true;
  }

  bool Read(Document& doc, std::wstring* error) {
    LARGE_INTEGER size;
    if (!GetFileSizeEx(handle.value, &size) || size.QuadPart <= 0 || size.QuadPart > kMaxBytes)
      return Fail(error, T(L"The Roblox settings file is empty or too large to read."));
    doc.bytes.resize(static_cast<std::size_t>(size.QuadPart));
    DWORD got = 0;
    if (!ReadFile(handle.value, doc.bytes.data(), static_cast<DWORD>(doc.bytes.size()), &got, nullptr) ||
        got != doc.bytes.size())
      return Fail(error, T(L"Couldn’t read the Roblox settings file."));
    if (!Parse(doc)) return Fail(error, T(L"The file doesn’t contain a readable Roblox user settings section."));
    return true;
  }
};

constexpr wchar_t kInUse[] = ELIXIR_TR(L"Close Roblox in this profile before changing its global settings.");

}

std::wstring GlobalSettingsFile(const LauncherPaths& paths) {
  return JoinPath(JoinPath(AndroidAppDataDirectory(paths, kRobloxPackage), L"files\\appData"),
                  L"GlobalBasicSettings_13.xml");
}

bool IsGlobalSettingsText(const std::string& text) {
  Document doc;
  doc.bytes = text;
  return Parse(doc);
}

std::optional<std::string> MergeGlobalSettings(const std::string& base, const std::string& changed,
                                               const std::string& current) {
  Document theirs;
  theirs.bytes = changed;
  if (!Parse(theirs)) return std::nullopt;
  Document ours;
  ours.bytes = current;
  if (!Parse(ours)) return changed;
  Document before;
  before.bytes = base;
  const bool have_before = Parse(before);

  struct Edit {
    std::size_t begin;
    std::size_t end;
    std::string text;
  };
  std::vector<Edit> edits;
  std::string added;
  for (const Property& property : theirs.properties) {
    int occurrences = 0;
    if (property.name.empty() || OnlyProperty(theirs, property.name, &occurrences) == nullptr) continue;
    const std::string_view value = TextOf(theirs, property);
    if (have_before) {
      const Property* old = OnlyProperty(before, property.name, &occurrences);
      if (occurrences > 1 || (old != nullptr && SameSetting(TextOf(before, *old), value))) continue;
    }
    const Property* target = OnlyProperty(ours, property.name, &occurrences);
    if (occurrences == 0) {
      added.append(value);
    } else if (target != nullptr && !SameSetting(TextOf(ours, *target), value)) {
      edits.push_back(Edit{target->begin, target->end, std::string(value)});
    }
  }
  if (!added.empty()) {
    if (ours.empty_properties) {
      edits.push_back(Edit{ours.properties_end, ours.properties_end + 2, ">" + added + "</Properties>"});
    } else {
      edits.push_back(Edit{ours.properties_end, ours.properties_end, added});
    }
  }
  std::sort(edits.begin(), edits.end(), [](const Edit& a, const Edit& b) { return a.begin > b.begin; });
  Document result;
  result.bytes = current;
  for (const Edit& edit : edits) result.bytes.replace(edit.begin, edit.end - edit.begin, edit.text);
  std::string merged = result.bytes;
  if (!Parse(result)) return std::nullopt;
  return merged;
}

bool LoadGlobalSettings(const LauncherPaths& paths, GlobalSettings* settings, std::wstring* error) {
  if (!settings) return false;
  *settings = {};
  Document doc;
  SettingsFile file;
  if (!file.Open(paths, false, false, error) || !file.Read(doc, error)) return false;
  settings->read_only = (file.attributes & FILE_ATTRIBUTE_READONLY) != 0;
  for (std::size_t i = 0; i < kSpecs.size(); ++i) {
    const auto& field = doc.fields[i];
    if (field.editable() && !field.self_closing) {
      SetParsed(*settings, kSpecs[i].field, std::string_view(doc.bytes).substr(field.begin, field.end - field.begin));
    }
  }
  return true;
}

bool SaveGlobalSetting(const LauncherPaths& paths, GlobalSetting setting, std::string_view raw, std::wstring* error) {
  GlobalSettings parsed;
  if (!SetParsed(parsed, setting, raw)) return Fail(error, T(L"That value is outside the range for this setting."));
  const std::string value =
      setting == GlobalSetting::kReducedMotion ? (*parsed.reduced_motion ? "true" : "false") : TrimAscii(raw);
  Handle reservation(TryReserveDataFolder(paths.data_root));
  if (!reservation.value) return Fail(error, T(kInUse));
  Document doc;
  SettingsFile file;
  if (!file.Open(paths, true, false, error) || !file.Read(doc, error)) return false;
  if (file.attributes & FILE_ATTRIBUTE_READONLY) return Fail(error, T(L"Turn off Read-only before editing this file."));
  for (std::size_t i = 0; i < kSpecs.size(); ++i) {
    const Spec& spec = kSpecs[i];
    if (spec.field != setting) continue;
    const Scalar& field = doc.fields[i];
    if (field.editable()) {
      const std::string replacement = field.self_closing ? ">" + value + "</" + spec.type + ">" : value;
      doc.bytes.replace(field.begin, field.end - field.begin, replacement);
    } else if (field.occurrences == 0) {
      const std::string element =
          std::string("<") + spec.type + " name=\"" + spec.name + "\">" + value + "</" + spec.type + ">";
      if (doc.empty_properties)
        doc.bytes.replace(doc.properties_end, 2, ">" + element + "</Properties>");
      else
        doc.bytes.insert(doc.properties_end, element);
    } else {
      return Fail(error,
                  T(L"This property occurs more than once or has an "
                  L"unexpected XML type or value structure."));
    }
    return AtomicWriteFile(file.path, doc.bytes, AtomicWriteBackup::kNone, error);
  }
  return false;
}

bool SetGlobalSettingsReadOnly(const LauncherPaths& paths, bool read_only, std::wstring* error) {
  Handle reservation(TryReserveDataFolder(paths.data_root));
  if (!reservation.value) return Fail(error, T(kInUse));
  SettingsFile file;
  if (!file.Open(paths, true, true, error)) return false;
  FILE_BASIC_INFO info{};
  if (!GetFileInformationByHandleEx(file.handle.value, FileBasicInfo, &info, sizeof(info)))
    return Fail(error, WithErrorCause(T(L"Couldn’t read the file attributes."), GetLastError()));
  if (((info.FileAttributes & FILE_ATTRIBUTE_READONLY) != 0) == read_only) return true;
  info.FileAttributes &= ~FILE_ATTRIBUTE_NORMAL;
  if (read_only)
    info.FileAttributes |= FILE_ATTRIBUTE_READONLY;
  else
    info.FileAttributes &= ~FILE_ATTRIBUTE_READONLY;
  if (!info.FileAttributes) info.FileAttributes = FILE_ATTRIBUTE_NORMAL;
  if (!SetFileInformationByHandle(file.handle.value, FileBasicInfo, &info, sizeof(info)))
    return Fail(error, WithErrorCause(T(L"Couldn’t change the file attributes."), GetLastError()));
  return true;
}

}
