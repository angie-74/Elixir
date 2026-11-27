// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/game_shortcuts.h"

#include <windows.h>

#include <objbase.h>
#include <shlobj.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <map>
#include <optional>

#include "core/json.h"
#include "core/sha256.h"
#include "core/shortcuts.h"
#include "core/util.h"
#include "core/i18n.h"

namespace elixir::launcher {

namespace {

using Microsoft::WRL::ComPtr;

constexpr std::uint64_t kJsonLimit = 256 * 1024;
constexpr std::uint64_t kImageLimit = 4u << 20;

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

bool SameName(std::wstring_view a, std::wstring_view b) {
  return a.size() == b.size() &&
         CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE) ==
             CSTR_EQUAL;
}

std::optional<Json> GetJson(const HttpGetFunction& get, const std::wstring& url) {
  if (!get) return std::nullopt;
  const HttpResult answer = get(url, kJsonLimit);
  if (!answer.ok) return std::nullopt;
  std::string error;
  std::optional<Json> json = Json::Parse(answer.body, &error);
  if (!json || !json->is_object()) return std::nullopt;
  return json;
}

const Json* FirstData(const std::optional<Json>& answer) {
  if (!answer) return nullptr;
  const Json* data = answer->Find("data");
  if (data == nullptr || !data->is_array() || data->items().empty() || !data->items().front().is_object()) {
    return nullptr;
  }
  return &data->items().front();
}

std::vector<std::wstring> LoadAutomaticNames(const std::wstring& state_file) {
  std::vector<std::wstring> names;
  std::string text;
  std::wstring error;
  if (!PathIsFile(state_file) || !ReadFileBytes(state_file, &text, &error, 1u << 20)) return names;
  std::string parse_error;
  const std::optional<Json> json = Json::Parse(text, &parse_error);
  const Json* list = json && json->is_object() ? json->Find("automatic_names") : nullptr;
  if (list == nullptr || !list->is_array()) return names;
  for (const Json& item : list->items()) {
    if (item.is_string() && !item.string_ref().empty()) names.push_back(Utf8ToWide(item.string_ref()));
  }
  return names;
}

bool SaveAutomaticNames(const std::wstring& state_file, const std::vector<std::wstring>& names) {
  Json list = Json::MakeArray();
  for (const std::wstring& name : names) list.Push(Json(WideToUtf8(name)));
  Json root = Json::MakeObject();
  root.Set("automatic_names", std::move(list));
  std::wstring error;
  EnsureDirectory(ParentDirectory(state_file), nullptr);
  return AtomicWriteFile(state_file, root.Dump(2), AtomicWriteBackup::kNone, &error);
}

bool Contains(const std::vector<std::wstring>& names, std::wstring_view file) {
  return std::any_of(names.begin(), names.end(), [file](const std::wstring& name) { return SameName(name, file); });
}

void Remove(std::vector<std::wstring>* names, std::wstring_view file) {
  names->erase(std::remove_if(names->begin(), names->end(),
                              [file](const std::wstring& name) { return SameName(name, file); }),
               names->end());
}

std::wstring FallbackName(std::uint64_t place_id) { return L"Roblox place " + std::to_wstring(place_id); }

std::wstring IconPrefix(std::uint64_t place_id) { return std::to_wstring(place_id) + L"-"; }

std::wstring EnsureIcon(const GameShortcutPlaces& places, std::uint64_t place_id, const std::string& url,
                        const HttpGetFunction& get) {
  if (!IsRobloxImageUrl(url) || places.icons_dir.empty()) return {};
  const std::wstring file = IconPrefix(place_id) + Utf8ToWide(Sha256Hex(url).substr(0, 12)) + L".ico";
  const std::wstring path = JoinPath(places.icons_dir, file);
  if (PathIsFile(path)) return path;
  if (!get) return {};
  const HttpResult image = get(Utf8ToWide(url), kImageLimit);
  if (!image.ok || image.body.empty()) return {};
  std::wstring error;
  if (!EnsureDirectory(places.icons_dir, &error) || !WriteIconFile(image.body, path, &error)) return {};
  return path;
}

void DeleteUnusedIcons(const GameShortcutPlaces& places, const std::vector<GameShortcutEntry>& entries) {
  if (places.icons_dir.empty()) return;
  WIN32_FIND_DATAW found{};
  const HANDLE find = FindFirstFileExW(JoinPath(places.icons_dir, L"*.ico").c_str(), FindExInfoBasic, &found,
                                       FindExSearchNameMatch, nullptr, 0);
  if (find == INVALID_HANDLE_VALUE) return;
  do {
    if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
    const std::wstring path = JoinPath(places.icons_dir, found.cFileName);
    const bool used = std::any_of(entries.begin(), entries.end(),
                                  [&path](const GameShortcutEntry& entry) { return PathsEqual(entry.icon, path); });
    if (!used) DeleteFileW(path.c_str());
  } while (FindNextFileW(find, &found));
  FindClose(find);
}

std::wstring FreeLinkPath(const std::wstring& folder, const std::wstring& name) {
  for (int number = 1; number <= 20; ++number) {
    const std::wstring candidate =
        JoinPath(folder, name + (number == 1 ? std::wstring() : L" (" + std::to_wstring(number) + L")") + L".lnk");
    if (GetFileAttributesW(candidate.c_str()) == INVALID_FILE_ATTRIBUTES) return candidate;
  }
  return {};
}

std::wstring LinkName(const std::wstring& path) {
  std::wstring name = FileNameOf(path);
  if (name.size() > 4) name.resize(name.size() - 4);
  return name;
}

std::wstring MoveLink(const GameShortcutPlaces& places, const std::wstring& path, const std::wstring& wanted) {
  const std::wstring current = LinkName(path);
  if (wanted.empty() || wanted == current) return path;
  std::wstring target = JoinPath(places.folder, wanted + L".lnk");
  if (!SameName(wanted, current)) {
    if (GetFileAttributesW(target.c_str()) != INVALID_FILE_ATTRIBUTES) target = FreeLinkPath(places.folder, wanted);
    if (target.empty()) return path;
  }
  if (!MoveFileExW(path.c_str(), target.c_str(), 0)) return path;
  SHChangeNotify(SHCNE_RENAMEITEM, SHCNF_PATHW | SHCNF_FLUSHNOWAIT, path.c_str(), target.c_str());
  return target;
}

}

bool IsRobloxImageUrl(std::string_view url) {
  constexpr std::string_view kScheme = "https://";
  constexpr std::string_view kHost = ".rbxcdn.com";
  if (url.size() > 512 || url.substr(0, kScheme.size()) != kScheme) return false;
  for (const char c : url) {
    if (static_cast<unsigned char>(c) <= 0x20 || static_cast<unsigned char>(c) >= 0x7F) return false;
  }
  const std::string_view rest = url.substr(kScheme.size());
  const std::string_view host = rest.substr(0, rest.find('/'));
  for (const char c : host) {
    if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '.' && c != '-') return false;
  }
  return host.size() > kHost.size() && host.substr(host.size() - kHost.size()) == kHost;
}

bool FetchGameDetails(std::uint64_t place_id, const HttpGetFunction& get, GameDetails* details) {
  if (place_id == 0 || details == nullptr) return false;
  const std::optional<Json> universe =
      GetJson(get, L"https://apis.roblox.com/universes/v1/places/" + std::to_wstring(place_id) + L"/universe");
  const std::int64_t universe_id = universe ? universe->GetInt("universeId", 0) : 0;
  if (universe_id <= 0) return false;
  const std::wstring id = std::to_wstring(universe_id);
  const std::optional<Json> games = GetJson(get, L"https://games.roblox.com/v1/games?universeIds=" + id);
  const Json* game = FirstData(games);
  if (game == nullptr) return false;
  GameDetails found;
  found.universe_id = static_cast<std::uint64_t>(universe_id);
  found.name = TrimWide(Utf8ToWide(game->GetString("name")));
  if (found.name.empty()) return false;
  const std::optional<Json> icons =
      GetJson(get, L"https://thumbnails.roblox.com/v1/games/icons?universeIds=" + id +
                       L"&returnPolicy=PlaceHolder&size=256x256&format=Png&isCircular=false");
  if (const Json* icon = FirstData(icons); icon != nullptr && icon->GetString("state") != "Pending") {
    const std::string url = icon->GetString("imageUrl");
    if (IsRobloxImageUrl(url)) found.icon_url = url;
  }
  *details = std::move(found);
  return true;
}

bool WriteIconFile(std::string_view image, const std::wstring& ico_path, std::wstring* error) {
  const auto fail = [error] {
    if (error != nullptr) *error = T(L"The game’s icon couldn’t be used.");
    return false;
  };
  if (image.empty() || image.size() > kImageLimit) return fail();
  ComScope com;
  if (!com.ok()) return fail();
  ComPtr<IWICImagingFactory> wic;
  ComPtr<IWICStream> stream;
  ComPtr<IWICBitmapDecoder> decoder;
  ComPtr<IWICBitmapFrameDecode> frame;
  ComPtr<IWICFormatConverter> converter;
  ComPtr<IWICBitmap> picture;
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))) ||
      FAILED(wic->CreateStream(&stream)) ||
      FAILED(stream->InitializeFromMemory(reinterpret_cast<BYTE*>(const_cast<char*>(image.data())),
                                          static_cast<DWORD>(image.size()))) ||
      FAILED(wic->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) ||
      FAILED(decoder->GetFrame(0, &frame)) || FAILED(wic->CreateFormatConverter(&converter)) ||
      FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeCustom)) ||
      FAILED(wic->CreateBitmapFromSource(converter.Get(), WICBitmapCacheOnLoad, &picture))) {
    return fail();
  }
  UINT width = 0;
  UINT height = 0;
  if (FAILED(picture->GetSize(&width, &height)) || width == 0 || height == 0 || width > 8192 || height > 8192) {
    return fail();
  }
  ComPtr<IWICBitmapSource> square = picture;
  if (width != height) {
    const UINT side = std::min(width, height);
    ComPtr<IWICBitmapClipper> clipper;
    const WICRect middle{static_cast<INT>((width - side) / 2), static_cast<INT>((height - side) / 2),
                         static_cast<INT>(side), static_cast<INT>(side)};
    if (FAILED(wic->CreateBitmapClipper(&clipper)) || FAILED(clipper->Initialize(picture.Get(), &middle))) return fail();
    square = clipper;
  }

  static constexpr UINT kSizes[] = {16, 20, 24, 32, 40, 48, 64, 256};
  std::vector<std::string> images;
  for (const UINT size : kSizes) {
    ComPtr<IWICBitmapScaler> scaler;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(size) * size * 4);
    if (FAILED(wic->CreateBitmapScaler(&scaler)) ||
        FAILED(scaler->Initialize(square.Get(), size, size, WICBitmapInterpolationModeFant)) ||
        FAILED(scaler->CopyPixels(nullptr, size * 4, static_cast<UINT>(pixels.size()), pixels.data()))) {
      return fail();
    }
    std::string data;
    if (size == 256) {
      ComPtr<IStream> memory;
      ComPtr<IWICBitmapEncoder> encoder;
      ComPtr<IWICBitmapFrameEncode> out;
      WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
      if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &memory)) ||
          FAILED(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) ||
          FAILED(encoder->Initialize(memory.Get(), WICBitmapEncoderNoCache)) ||
          FAILED(encoder->CreateNewFrame(&out, nullptr)) || FAILED(out->Initialize(nullptr)) ||
          FAILED(out->SetSize(size, size)) || FAILED(out->SetPixelFormat(&format)) ||
          !IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA) ||
          FAILED(out->WritePixels(size, size * 4, static_cast<UINT>(pixels.size()), pixels.data())) ||
          FAILED(out->Commit()) || FAILED(encoder->Commit())) {
        return fail();
      }
      STATSTG stat{};
      HGLOBAL block = nullptr;
      if (FAILED(memory->Stat(&stat, STATFLAG_NONAME)) || FAILED(GetHGlobalFromStream(memory.Get(), &block)) ||
          stat.cbSize.QuadPart == 0 || stat.cbSize.QuadPart > (8u << 20)) {
        return fail();
      }
      const void* bytes = GlobalLock(block);
      if (bytes == nullptr) return fail();
      data.assign(static_cast<const char*>(bytes), static_cast<std::size_t>(stat.cbSize.QuadPart));
      GlobalUnlock(block);
    } else {
      const std::size_t mask_stride = ((size + 31) / 32) * 4;
      BITMAPINFOHEADER header{};
      header.biSize = sizeof(header);
      header.biWidth = static_cast<LONG>(size);
      header.biHeight = static_cast<LONG>(size) * 2;
      header.biPlanes = 1;
      header.biBitCount = 32;
      header.biCompression = BI_RGB;
      header.biSizeImage = static_cast<DWORD>(pixels.size() + mask_stride * size);
      data.assign(reinterpret_cast<const char*>(&header), sizeof(header));
      for (UINT row = size; row-- > 0;) {
        data.append(reinterpret_cast<const char*>(pixels.data()) + static_cast<std::size_t>(row) * size * 4, size * 4);
      }
      for (UINT row = size; row-- > 0;) {
        std::string mask(mask_stride, '\0');
        for (UINT x = 0; x < size; ++x) {
          const std::uint8_t alpha = pixels[(static_cast<std::size_t>(row) * size + x) * 4 + 3];
          if (alpha == 0) mask[x / 8] = static_cast<char>(mask[x / 8] | (0x80 >> (x % 8)));
        }
        data += mask;
      }
    }
    images.push_back(std::move(data));
  }

#pragma pack(push, 1)
  struct IconHeader {
    std::uint16_t reserved;
    std::uint16_t type;
    std::uint16_t count;
  };
  struct IconEntry {
    std::uint8_t width;
    std::uint8_t height;
    std::uint8_t colors;
    std::uint8_t reserved;
    std::uint16_t planes;
    std::uint16_t bits;
    std::uint32_t bytes;
    std::uint32_t offset;
  };
#pragma pack(pop)
  const IconHeader header{0, 1, static_cast<std::uint16_t>(images.size())};
  std::string file(reinterpret_cast<const char*>(&header), sizeof(header));
  std::uint32_t offset = static_cast<std::uint32_t>(sizeof(IconHeader) + sizeof(IconEntry) * images.size());
  for (std::size_t i = 0; i < images.size(); ++i) {
    const std::uint8_t side = kSizes[i] >= 256 ? 0 : static_cast<std::uint8_t>(kSizes[i]);
    const IconEntry entry{side, side, 0, 0, 1, 32, static_cast<std::uint32_t>(images[i].size()), offset};
    file.append(reinterpret_cast<const char*>(&entry), sizeof(entry));
    offset += static_cast<std::uint32_t>(images[i].size());
  }
  for (const std::string& data : images) file += data;
  std::wstring write_error;
  if (!AtomicWriteFile(ico_path, file, AtomicWriteBackup::kNone, &write_error)) return fail();
  return true;
}

std::vector<GameShortcutEntry> ListGameShortcutEntries(const GameShortcutPlaces& places) {
  std::vector<GameShortcutEntry> entries;
  const std::vector<std::wstring> automatic = LoadAutomaticNames(places.state_file);
  for (const GameShortcut& shortcut : ListGameShortcuts(places.folder, places.program)) {
    GameShortcutEntry entry;
    entry.path = shortcut.path;
    entry.name = shortcut.name;
    entry.place_id = shortcut.place_id;
    ShortcutInfo info;
    if (ReadShortcut(shortcut.path, &info)) {
      entry.title = info.description;
      if (!info.icon.empty() && PathIsFile(info.icon)) entry.icon = info.icon;
    }
    entry.automatic_name = Contains(automatic, FileNameOf(shortcut.path));
    entries.push_back(std::move(entry));
  }
  return entries;
}

GameShortcutChange AddGameShortcut(const GameShortcutPlaces& places, std::uint64_t place_id,
                                   const HttpGetFunction& get) {
  GameShortcutChange change;
  if (places.folder.empty()) {
    change.error = T(L"Windows didn’t say where your desktop is.");
    return change;
  }
  if (place_id == 0) {
    change.error = T(L"That isn’t a Roblox game link.");
    return change;
  }
  for (const GameShortcutEntry& entry : ListGameShortcutEntries(places)) {
    if (entry.place_id == place_id && entry.automatic_name) {
      UpdateGameShortcuts(places, get);
      for (const GameShortcutEntry& now : ListGameShortcutEntries(places)) {
        if (now.place_id == place_id && now.automatic_name) change.path = now.path;
      }
      change.ok = !change.path.empty();
      return change;
    }
  }
  GameDetails details;
  const bool known = FetchGameDetails(place_id, get, &details);
  std::wstring name = known ? ShortcutFileName(details.name) : std::wstring();
  if (name.empty()) name = FallbackName(place_id);
  if (!EnsureDirectory(places.folder, &change.error)) return change;
  const std::wstring link = FreeLinkPath(places.folder, name);
  if (link.empty()) {
    change.error = T(L"Your desktop already has shortcuts with that name.");
    return change;
  }
  ShortcutSpec spec;
  spec.target = places.program;
  spec.arguments = GameShortcutArguments(place_id);
  spec.description = known ? details.name : std::wstring();
  spec.icon = known ? EnsureIcon(places, place_id, details.icon_url, get) : std::wstring();
  if (!CreateShortcut(link, spec, &change.error)) return change;
  std::vector<std::wstring> automatic = LoadAutomaticNames(places.state_file);
  Remove(&automatic, FileNameOf(link));
  automatic.push_back(FileNameOf(link));
  SaveAutomaticNames(places.state_file, automatic);
  change.ok = true;
  change.path = link;
  return change;
}

GameShortcutChange RenameGameShortcut(const GameShortcutPlaces& places, const std::wstring& path,
                                      std::wstring_view name) {
  GameShortcutChange change;
  change.path = path;
  ShortcutInfo info;
  if (!ReadShortcut(path, &info) || !PathsEqual(info.target, places.program) ||
      PlaceOfGameShortcut(info.arguments) == 0) {
    change.error = T(L"That shortcut is gone.");
    return change;
  }
  const bool automatic = TrimWide(name).empty();
  std::wstring wanted;
  if (automatic) {
    wanted = ShortcutFileName(info.description);
    if (wanted.empty()) wanted = FallbackName(PlaceOfGameShortcut(info.arguments));
  } else {
    wanted = ShortcutFileName(name);
    if (wanted.empty()) {
      change.error = T(L"A shortcut can’t have that name.");
      return change;
    }
  }
  std::vector<std::wstring> names = LoadAutomaticNames(places.state_file);
  Remove(&names, FileNameOf(path));
  const std::wstring current = LinkName(path);
  if (wanted != current) {
    const std::wstring target = JoinPath(places.folder, wanted + L".lnk");
    if (!SameName(wanted, current) && GetFileAttributesW(target.c_str()) != INVALID_FILE_ATTRIBUTES) {
      change.error = T(L"Your desktop already has a shortcut with that name.");
      return change;
    }
    if (!MoveFileExW(path.c_str(), target.c_str(), 0)) {
      change.error = WithErrorCause(T(L"The shortcut couldn’t be renamed."), GetLastError());
      return change;
    }
    SHChangeNotify(SHCNE_RENAMEITEM, SHCNF_PATHW | SHCNF_FLUSHNOWAIT, path.c_str(), target.c_str());
    change.path = target;
  }
  if (automatic) names.push_back(FileNameOf(change.path));
  SaveAutomaticNames(places.state_file, names);
  change.ok = true;
  return change;
}

bool DeleteGameShortcut(const GameShortcutPlaces& places, const std::wstring& path) {
  if (!RemoveShortcut(path, places.program)) return false;
  std::vector<std::wstring> names = LoadAutomaticNames(places.state_file);
  Remove(&names, FileNameOf(path));
  SaveAutomaticNames(places.state_file, names);
  DeleteUnusedIcons(places, ListGameShortcutEntries(places));
  return true;
}

int UpdateGameShortcuts(const GameShortcutPlaces& places, const HttpGetFunction& get) {
  std::vector<GameShortcutEntry> entries = ListGameShortcutEntries(places);
  if (entries.empty()) {
    DeleteUnusedIcons(places, entries);
    return 0;
  }
  std::vector<std::wstring> names = LoadAutomaticNames(places.state_file);
  const std::vector<std::wstring> names_before = names;
  std::map<std::uint64_t, std::optional<GameDetails>> asked;
  int changed = 0;
  for (GameShortcutEntry& entry : entries) {
    auto [found, first] = asked.try_emplace(entry.place_id);
    if (first) {
      GameDetails details;
      if (FetchGameDetails(entry.place_id, get, &details)) found->second = std::move(details);
    }
    if (!found->second) continue;
    const GameDetails& details = *found->second;
    bool touched = false;
    if (entry.automatic_name) {
      const std::wstring wanted = ShortcutFileName(details.name);
      const std::wstring moved = MoveLink(places, entry.path, wanted);
      if (moved != entry.path) {
        Remove(&names, FileNameOf(entry.path));
        names.push_back(FileNameOf(moved));
        entry.path = moved;
        entry.name = LinkName(moved);
        touched = true;
      }
    }
    std::wstring icon = EnsureIcon(places, entry.place_id, details.icon_url, get);
    if (icon.empty()) icon = entry.icon;
    if (!PathsEqual(icon, entry.icon) || details.name != entry.title) {
      if (SetShortcutAppearance(entry.path, icon, details.name)) {
        entry.icon = icon;
        entry.title = details.name;
        touched = true;
      }
    }
    if (touched) ++changed;
  }
  names.erase(std::remove_if(names.begin(), names.end(),
                             [&entries](const std::wstring& name) {
                               return std::none_of(entries.begin(), entries.end(), [&name](const GameShortcutEntry& e) {
                                 return SameName(FileNameOf(e.path), name);
                               });
                             }),
              names.end());
  if (names != names_before) SaveAutomaticNames(places.state_file, names);
  DeleteUnusedIcons(places, entries);
  return changed;
}

}
