// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/shortcuts.h"

#include <windows.h>

#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <algorithm>
#include <cwctype>

#include "core/link_request.h"
#include "core/util.h"

namespace elixir::launcher {

namespace {

constexpr wchar_t kGameLinkPrefix[] = L"roblox://experiences/start?placeId=";

class ComScope {
 public:
  ComScope() : result_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)) {}
  ~ComScope() {
    if (SUCCEEDED(result_)) CoUninitialize();
  }
  bool ok() const { return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE; }

 private:
  HRESULT result_;
};

template <typename T>
class Released {
 public:
  ~Released() {
    if (pointer_ != nullptr) pointer_->Release();
  }
  T** out() { return &pointer_; }
  T* operator->() const { return pointer_; }
  T* get() const { return pointer_; }

 private:
  T* pointer_ = nullptr;
};

std::wstring KnownFolder(REFKNOWNFOLDERID id) {
  PWSTR path = nullptr;
  std::wstring out;
  if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &path)) && path != nullptr) out = path;
  if (path != nullptr) CoTaskMemFree(path);
  return out;
}

std::wstring Unquoted(std::wstring_view text) {
  std::wstring out = TrimWide(text);
  if (out.size() >= 2 && out.front() == L'"' && out.back() == L'"') out = out.substr(1, out.size() - 2);
  return out;
}

}

std::wstring DesktopDirectory() { return KnownFolder(FOLDERID_Desktop); }
std::wstring StartMenuProgramsDirectory() { return KnownFolder(FOLDERID_Programs); }

bool CreateShortcut(const std::wstring& link_path, const ShortcutSpec& spec, std::wstring* error) {
  const auto fail = [error](std::wstring_view what, HRESULT hr) {
    if (error != nullptr) *error = std::wstring(what) + L" " + HResultText(hr);
    return false;
  };
  ComScope com;
  if (!com.ok()) return fail(L"The shortcut couldn’t be created.", E_FAIL);
  Released<IShellLinkW> link;
  HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(link.out()));
  if (FAILED(hr)) return fail(L"The shortcut couldn’t be created.", hr);
  hr = link->SetPath(spec.target.c_str());
  if (SUCCEEDED(hr)) hr = link->SetArguments(spec.arguments.c_str());
  if (SUCCEEDED(hr)) hr = link->SetWorkingDirectory(ParentDirectory(spec.target).c_str());
  if (SUCCEEDED(hr)) hr = link->SetIconLocation(spec.icon.empty() ? spec.target.c_str() : spec.icon.c_str(), 0);
  if (SUCCEEDED(hr)) hr = link->SetDescription(spec.description.substr(0, INFOTIPSIZE - 1).c_str());
  if (FAILED(hr)) return fail(L"The shortcut couldn’t be created.", hr);
  Released<IPersistFile> file;
  hr = link->QueryInterface(IID_PPV_ARGS(file.out()));
  if (FAILED(hr)) return fail(L"The shortcut couldn’t be created.", hr);
  hr = file->Save(link_path.c_str(), TRUE);
  if (FAILED(hr)) return fail(L"The shortcut couldn’t be saved.", hr);
  return true;
}

bool ReadShortcut(const std::wstring& link_path, ShortcutInfo* info) {
  if (info == nullptr || !PathIsFile(link_path)) return false;
  ComScope com;
  if (!com.ok()) return false;
  Released<IShellLinkW> link;
  if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(link.out())))) return false;
  Released<IPersistFile> file;
  if (FAILED(link->QueryInterface(IID_PPV_ARGS(file.out())))) return false;
  if (FAILED(file->Load(link_path.c_str(), STGM_READ | STGM_SHARE_DENY_NONE))) return false;
  wchar_t target[MAX_PATH * 2] = {};
  if (FAILED(link->GetPath(target, static_cast<int>(std::size(target)), nullptr, SLGP_RAWPATH))) return false;
  wchar_t arguments[2048] = {};
  if (FAILED(link->GetArguments(arguments, static_cast<int>(std::size(arguments))))) return false;
  info->target = target;
  info->arguments = arguments;
  wchar_t description[INFOTIPSIZE] = {};
  if (SUCCEEDED(link->GetDescription(description, static_cast<int>(std::size(description))))) {
    info->description = description;
  }
  wchar_t icon[MAX_PATH * 2] = {};
  int icon_index = 0;
  if (SUCCEEDED(link->GetIconLocation(icon, static_cast<int>(std::size(icon)), &icon_index)) && icon[0] != L'\0' &&
      !PathsEqual(icon, info->target)) {
    info->icon = icon;
  }
  return true;
}

bool SetShortcutAppearance(const std::wstring& link_path, const std::wstring& icon, const std::wstring& description) {
  if (!PathIsFile(link_path)) return false;
  ComScope com;
  if (!com.ok()) return false;
  Released<IShellLinkW> link;
  if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(link.out())))) return false;
  Released<IPersistFile> file;
  if (FAILED(link->QueryInterface(IID_PPV_ARGS(file.out())))) return false;
  if (FAILED(file->Load(link_path.c_str(), STGM_READWRITE | STGM_SHARE_DENY_WRITE))) return false;
  wchar_t target[MAX_PATH * 2] = {};
  if (FAILED(link->GetPath(target, static_cast<int>(std::size(target)), nullptr, SLGP_RAWPATH))) return false;
  if (FAILED(link->SetIconLocation(icon.empty() ? target : icon.c_str(), 0))) return false;
  if (FAILED(link->SetDescription(description.substr(0, INFOTIPSIZE - 1).c_str()))) return false;
  if (FAILED(file->Save(link_path.c_str(), TRUE))) return false;
  SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATHW | SHCNF_FLUSHNOWAIT, link_path.c_str(), nullptr);
  return true;
}

bool ShortcutRuns(const std::wstring& link_path, const std::wstring& target) {
  ShortcutInfo info;
  return ReadShortcut(link_path, &info) && PathsEqual(info.target, target);
}

bool RemoveShortcut(const std::wstring& link_path, const std::wstring& target) {
  if (!PathIsFile(link_path)) return true;
  if (!ShortcutRuns(link_path, target)) return false;
  return DeleteFileW(link_path.c_str()) != 0 || GetLastError() == ERROR_FILE_NOT_FOUND;
}

std::wstring GameShortcutArguments(std::uint64_t place_id) {
  return std::wstring(kOpenLinkFlag) + L" \"" + kGameLinkPrefix + std::to_wstring(place_id) + L"\"";
}

std::uint64_t PlaceOfGameShortcut(std::wstring_view arguments) {
  std::wstring text = TrimWide(arguments);
  const std::wstring_view flag = kOpenLinkFlag;
  if (text.size() > flag.size() && StartsWithI(text, flag) && text[flag.size()] == L' ') {
    text = TrimWide(std::wstring_view(text).substr(flag.size()));
  }
  const std::wstring link = Unquoted(text);
  const std::size_t prefix = std::size(kGameLinkPrefix) - 1;
  if (link.size() <= prefix || !StartsWithI(link, kGameLinkPrefix)) return 0;
  std::uint64_t place_id = 0;
  return ParsePlaceId(std::wstring_view(link).substr(prefix), &place_id) ? place_id : 0;
}

std::wstring ShortcutFileName(std::wstring_view name) {
  std::wstring out;
  for (const wchar_t c : name) {
    const bool forbidden = c < 0x20 || c == L'<' || c == L'>' || c == L':' || c == L'"' || c == L'/' || c == L'\\' ||
                           c == L'|' || c == L'?' || c == L'*';
    out.push_back(forbidden ? L' ' : c);
  }
  while (!out.empty() && (out.back() == L' ' || out.back() == L'.')) out.pop_back();
  std::size_t start = 0;
  while (start < out.size() && (out[start] == L' ' || out[start] == L'.')) ++start;
  out = out.substr(start);
  if (out.size() > 80) out.resize(80);
  while (!out.empty() && (out.back() == L' ' || out.back() == L'.')) out.pop_back();
  std::wstring stem = out.substr(0, out.find(L'.'));
  for (wchar_t& c : stem) c = static_cast<wchar_t>(std::towupper(c));
  static constexpr const wchar_t* kReserved[] = {L"CON", L"PRN", L"AUX", L"NUL"};
  for (const wchar_t* reserved : kReserved) {
    if (stem == reserved) return {};
  }
  if (stem.size() == 4 && (stem.rfind(L"COM", 0) == 0 || stem.rfind(L"LPT", 0) == 0) && std::iswdigit(stem[3])) {
    return {};
  }
  return out;
}

namespace {

bool LeadingPlaceId(std::wstring_view text, std::uint64_t* place_id) {
  std::size_t length = 0;
  std::uint64_t value = 0;
  while (length < text.size() && text[length] >= L'0' && text[length] <= L'9') {
    if (length == 18) return false;
    value = value * 10 + static_cast<std::uint64_t>(text[length] - L'0');
    ++length;
  }
  if (length == 0 || value == 0) return false;
  if (length < text.size() && std::iswalnum(text[length])) return false;
  if (place_id != nullptr) *place_id = value;
  return true;
}

}

bool ParsePlaceId(std::wstring_view text, std::uint64_t* place_id) {
  const std::wstring typed = TrimWide(text);
  if (typed.empty() || typed.size() > 2048) return false;
  const bool digits_only =
      std::all_of(typed.begin(), typed.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; });
  if (digits_only) return LeadingPlaceId(typed, place_id);
  std::wstring lower = typed;
  for (wchar_t& c : lower) c = static_cast<wchar_t>(std::towlower(c));
  const bool roblox = lower.rfind(L"roblox://", 0) == 0 || lower.rfind(L"roblox:", 0) == 0 ||
                      lower.find(L"roblox.com/") != std::wstring::npos || lower.find(L"ro.blox.com/") != std::wstring::npos;
  if (!roblox) return false;
  static constexpr std::wstring_view kMarks[] = {L"placeid=", L"/games/", L"/experiences/"};
  for (const std::wstring_view mark : kMarks) {
    const std::size_t found = lower.find(mark);
    if (found == std::wstring::npos) continue;
    if (LeadingPlaceId(std::wstring_view(typed).substr(found + mark.size()), place_id)) return true;
  }
  return false;
}

std::vector<GameShortcut> ListGameShortcuts(const std::wstring& directory, const std::wstring& target) {
  std::vector<GameShortcut> shortcuts;
  if (directory.empty()) return shortcuts;
  WIN32_FIND_DATAW entry{};
  const HANDLE find = FindFirstFileExW(JoinPath(directory, L"*.lnk").c_str(), FindExInfoBasic, &entry,
                                       FindExSearchNameMatch, nullptr, 0);
  if (find == INVALID_HANDLE_VALUE) return shortcuts;
  do {
    if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
    const std::wstring path = JoinPath(directory, entry.cFileName);
    ShortcutInfo info;
    if (!ReadShortcut(path, &info) || !PathsEqual(info.target, target)) continue;
    const std::uint64_t place_id = PlaceOfGameShortcut(info.arguments);
    if (place_id == 0) continue;
    GameShortcut shortcut;
    shortcut.path = path;
    shortcut.name = entry.cFileName;
    if (shortcut.name.size() > 4) shortcut.name.resize(shortcut.name.size() - 4);
    shortcut.place_id = place_id;
    shortcuts.push_back(std::move(shortcut));
  } while (FindNextFileW(find, &entry));
  FindClose(find);
  std::sort(shortcuts.begin(), shortcuts.end(),
            [](const GameShortcut& a, const GameShortcut& b) { return a.name < b.name; });
  return shortcuts;
}

}
