// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "core/activity.h"

#include <windows.h>

#include <algorithm>
#include <ctime>
#include <mutex>
#include <optional>
#include <utility>

#include "core/i18n.h"
#include "core/json.h"
#include "core/util.h"

namespace elixir::launcher {

namespace {

constexpr std::uint64_t kMaxJournalBytes = 16ull << 20;
constexpr std::uint64_t kMaxStoreBytes = 4ull << 20;
constexpr std::uint64_t kMaxIconBytes = 2ull << 20;
constexpr std::string_view kPngSignature = "\x89PNG\r\n\x1a\n";
constexpr std::int64_t kEarliest = 1600000000;
constexpr std::int64_t kLongestSession = 7 * 24 * 60 * 60;
constexpr std::int64_t kRetryAfterFailureSeconds = 60 * 60;
constexpr std::size_t kMaxDone = 256;
constexpr std::size_t kMaxOpen = 64;
constexpr std::size_t kMaxPlacesPerGame = 32;
constexpr std::size_t kMaxNameChars = 100;
constexpr std::size_t kMaxCreatorChars = 60;
constexpr std::size_t kMaxDescriptionChars = 600;
constexpr wchar_t kIconsFolder[] = L"icons";

struct StoredGame {
  ActivityGame game;
  std::vector<std::uint64_t> places;
  std::string icon_url;
  std::int64_t checked = 0;
};

struct OpenSession {
  std::string id;
  std::uint64_t place = 0;
  std::string job;
  std::string kind;
  std::int64_t user = 0;
  std::int64_t start = 0;
  std::int64_t last = 0;
  std::int64_t counted = 0;
  bool ended = false;
};

struct Store {
  std::vector<StoredGame> games;
  std::vector<OpenSession> open;
  std::vector<std::string> done;
};

std::mutex& StoreMutex() {
  static std::mutex mutex;
  return mutex;
}

bool IsHexDigit(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }

bool IsSessionId(std::string_view text) {
  return text.size() == 16 && std::all_of(text.begin(), text.end(), IsHexDigit);
}

bool IsKind(std::string_view text) {
  return text == "public" || text == "private" || text == "reserved" || text == "unknown";
}

std::uint64_t Positive(std::int64_t value) { return value > 0 ? static_cast<std::uint64_t>(value) : 0; }

std::wstring CleanText(std::wstring text, std::size_t limit) {
  std::wstring out;
  out.reserve(text.size());
  bool space = false;
  for (const wchar_t c : text) {
    if (c < 0x20 || c == 0x7F || c == 0x2028 || c == 0x2029 || c == L' ') {
      space = !out.empty();
      continue;
    }
    if (space) out.push_back(L' ');
    space = false;
    out.push_back(c);
  }
  if (out.size() > limit) {
    out.resize(limit);
    if (!out.empty() && out.back() >= 0xD800 && out.back() <= 0xDBFF) out.pop_back();
    while (!out.empty() && out.back() == L' ') out.pop_back();
    out += L"\x2026";
  }
  return out;
}

std::wstring StorePath(const std::wstring& folder) { return JoinPath(folder, kActivityStoreFile); }

std::wstring IconPath(const std::wstring& folder, std::uint64_t universe_id) {
  return JoinPath(JoinPath(folder, kIconsFolder), std::to_wstring(universe_id) + L".png");
}

bool IsSmallFile(const std::wstring& path, std::uint64_t max_bytes) {
  WIN32_FILE_ATTRIBUTE_DATA data{};
  if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return false;
  if ((data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) return false;
  const std::uint64_t size = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
  return size > 0 && size <= max_bytes;
}

Store LoadStore(const std::wstring& folder) {
  Store store;
  std::string text;
  if (!IsSmallFile(StorePath(folder), kMaxStoreBytes) || !ReadFileBytes(StorePath(folder), &text, nullptr, kMaxStoreBytes)) {
    return store;
  }
  const std::optional<Json> json = Json::Parse(text, nullptr);
  if (!json || !json->is_object() || json->GetInt("version", 0) != 1) return store;
  if (const Json* games = json->Find("games"); games != nullptr && games->is_array()) {
    for (const Json& item : games->items()) {
      if (!item.is_object() || store.games.size() >= kMaxActivityGames) continue;
      StoredGame stored;
      ActivityGame& game = stored.game;
      game.place_id = Positive(item.GetInt("place", 0));
      if (game.place_id == 0) continue;
      game.root_place_id = Positive(item.GetInt("root_place", 0));
      game.universe_id = Positive(item.GetInt("universe", 0));
      game.name = CleanText(Utf8ToWide(item.GetString("name")), kMaxNameChars);
      game.description = CleanText(Utf8ToWide(item.GetString("description")), kMaxDescriptionChars);
      game.creator = CleanText(Utf8ToWide(item.GetString("creator")), kMaxCreatorChars);
      game.last_played = std::max<std::int64_t>(0, item.GetInt("last_played", 0));
      game.seconds = std::max<std::int64_t>(0, item.GetInt("seconds", 0));
      game.user_id = std::max<std::int64_t>(0, item.GetInt("user", 0));
      const std::string job = item.GetString("job");
      if (IsActivityServerId(job)) game.job_id = job;
      const std::string icon_url = item.GetString("icon_url");
      if (IsRobloxImageUrl(icon_url)) stored.icon_url = icon_url;
      stored.checked = std::max<std::int64_t>(0, item.GetInt("checked", 0));
      if (const Json* places = item.Find("places"); places != nullptr && places->is_array()) {
        for (const Json& place : places->items()) {
          const std::uint64_t id = Positive(place.AsInt(0));
          if (id != 0 && stored.places.size() < kMaxPlacesPerGame) stored.places.push_back(id);
        }
      }
      if (std::find(stored.places.begin(), stored.places.end(), game.place_id) == stored.places.end()) {
        stored.places.push_back(game.place_id);
      }
      store.games.push_back(std::move(stored));
    }
  }
  if (const Json* open = json->Find("open"); open != nullptr && open->is_array()) {
    for (const Json& item : open->items()) {
      if (!item.is_object() || store.open.size() >= kMaxOpen) continue;
      OpenSession session;
      session.id = item.GetString("id");
      if (!IsSessionId(session.id)) continue;
      session.place = Positive(item.GetInt("place", 0));
      const std::string job = item.GetString("job");
      if (IsActivityServerId(job)) session.job = job;
      const std::string kind = item.GetString("kind");
      session.kind = IsKind(kind) ? kind : std::string("unknown");
      session.user = std::max<std::int64_t>(0, item.GetInt("user", 0));
      session.start = item.GetInt("start", 0);
      session.last = std::max(session.start, item.GetInt("last", 0));
      session.counted = std::clamp<std::int64_t>(item.GetInt("counted", 0), 0, kLongestSession);
      store.open.push_back(std::move(session));
    }
  }
  if (const Json* done = json->Find("done"); done != nullptr && done->is_array()) {
    for (const Json& item : done->items()) {
      if (item.is_string() && IsSessionId(item.string_ref()) && store.done.size() < kMaxDone) {
        store.done.push_back(item.string_ref());
      }
    }
  }
  return store;
}

bool SaveStore(const std::wstring& folder, const Store& store) {
  Json root = Json::MakeObject();
  root.Set("version", 1);
  Json games = Json::MakeArray();
  for (const StoredGame& stored : store.games) {
    const ActivityGame& game = stored.game;
    Json item = Json::MakeObject();
    item.Set("place", static_cast<std::int64_t>(game.place_id));
    item.Set("root_place", static_cast<std::int64_t>(game.root_place_id));
    item.Set("universe", static_cast<std::int64_t>(game.universe_id));
    item.Set("name", WideToUtf8(game.name));
    item.Set("description", WideToUtf8(game.description));
    item.Set("creator", WideToUtf8(game.creator));
    item.Set("icon_url", stored.icon_url);
    item.Set("last_played", game.last_played);
    item.Set("seconds", game.seconds);
    item.Set("user", game.user_id);
    item.Set("job", game.job_id);
    item.Set("checked", stored.checked);
    Json places = Json::MakeArray();
    for (const std::uint64_t place : stored.places) places.Push(static_cast<std::int64_t>(place));
    item.Set("places", std::move(places));
    games.Push(std::move(item));
  }
  root.Set("games", std::move(games));
  Json open = Json::MakeArray();
  for (const OpenSession& session : store.open) {
    Json item = Json::MakeObject();
    item.Set("id", session.id);
    item.Set("place", static_cast<std::int64_t>(session.place));
    item.Set("job", session.job);
    item.Set("kind", session.kind);
    item.Set("user", session.user);
    item.Set("start", session.start);
    item.Set("last", session.last);
    item.Set("counted", session.counted);
    open.Push(std::move(item));
  }
  root.Set("open", std::move(open));
  Json done = Json::MakeArray();
  for (const std::string& id : store.done) done.Push(id);
  root.Set("done", std::move(done));
  if (!EnsureDirectory(folder, nullptr)) return false;
  return AtomicWriteFile(StorePath(folder), root.Dump(), AtomicWriteBackup::kNone, nullptr);
}

StoredGame* GameWithPlace(Store* store, std::uint64_t place) {
  for (StoredGame& stored : store->games) {
    if (std::find(stored.places.begin(), stored.places.end(), place) != stored.places.end()) return &stored;
  }
  return nullptr;
}

StoredGame& GameFor(Store* store, std::uint64_t place) {
  if (StoredGame* found = GameWithPlace(store, place)) return *found;
  StoredGame stored;
  stored.game.place_id = place;
  stored.places.push_back(place);
  store->games.push_back(std::move(stored));
  return store->games.back();
}

OpenSession* OpenWithId(Store* store, std::string_view id) {
  for (OpenSession& session : store->open) {
    if (session.id == id) return &session;
  }
  return nullptr;
}

bool ApplyLine(std::string_view line, Store* store, std::int64_t now) {
  if (line.empty() || line.size() > 512) return false;
  const std::optional<Json> json = Json::Parse(line, nullptr);
  if (!json || !json->is_object() || json->GetInt("v", 0) != 1) return false;
  const std::string id = json->GetString("s");
  const std::string event = json->GetString("e");
  const std::int64_t when = json->GetInt("t", 0);
  if (!IsSessionId(id) || when < kEarliest || when > now + 24 * 60 * 60) return false;
  if (event == "start") {
    if (OpenWithId(store, id) != nullptr || store->open.size() >= kMaxOpen ||
        std::find(store->done.begin(), store->done.end(), id) != store->done.end()) {
      return false;
    }
    OpenSession session;
    session.id = id;
    session.place = Positive(json->GetInt("place", 0));
    const std::string job = json->GetString("job");
    if (IsActivityServerId(job)) session.job = job;
    const std::string kind = json->GetString("kind");
    session.kind = IsKind(kind) ? kind : std::string("unknown");
    session.user = std::max<std::int64_t>(0, json->GetInt("user", 0));
    session.start = when;
    session.last = when;
    store->open.push_back(std::move(session));
    return true;
  }
  OpenSession* session = OpenWithId(store, id);
  if (session == nullptr) return false;
  session->last = std::max(session->last, when);
  if (event == "server") {
    const std::uint64_t place = Positive(json->GetInt("place", 0));
    const std::string job = json->GetString("job");
    if (place != 0) session->place = place;
    if (IsActivityServerId(job)) session->job = job;
  } else if (event == "end") {
    session->ended = true;
  } else if (event != "alive") {
    return false;
  }
  return true;
}

std::vector<std::wstring> TakenJournals(const std::wstring& folder) {
  std::vector<std::wstring> files;
  WIN32_FIND_DATAW entry{};
  const HANDLE find = FindFirstFileExW(JoinPath(folder, L"journal-*.jsonl").c_str(), FindExInfoBasic, &entry,
                                       FindExSearchNameMatch, nullptr, 0);
  if (find == INVALID_HANDLE_VALUE) return files;
  do {
    if ((entry.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) continue;
    files.push_back(JoinPath(folder, entry.cFileName));
  } while (FindNextFileW(find, &entry) && files.size() < 64);
  FindClose(find);
  std::sort(files.begin(), files.end());
  return files;
}

std::vector<std::wstring> FoldJournals(const std::wstring& folder, Store* store, std::int64_t now) {
  const std::wstring journal = JoinPath(folder, kActivityJournalFile);
  if (IsSmallFile(journal, kMaxJournalBytes * 4)) {
    for (int attempt = 0; attempt < 4; ++attempt) {
      const std::wstring taken = JoinPath(folder, L"journal-" + UtcFileStamp() + L"-" +
                                                      std::to_wstring(GetCurrentProcessId()) + L"-" +
                                                      std::to_wstring(attempt) + L".jsonl");
      if (MoveFileExW(journal.c_str(), taken.c_str(), 0)) {
        Sleep(20);
        break;
      }
      const DWORD error = GetLastError();
      if (error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS) break;
    }
  }
  std::vector<std::wstring> files = TakenJournals(folder);
  for (const std::wstring& file : files) {
    std::string text;
    if (!IsSmallFile(file, kMaxJournalBytes) || !ReadFileBytes(file, &text, nullptr, kMaxJournalBytes)) continue;
    std::size_t begin = 0;
    while (begin < text.size()) {
      std::size_t end = text.find('\n', begin);
      if (end == std::string::npos) end = text.size();
      std::string_view line(text.data() + begin, end - begin);
      if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
      ApplyLine(line, store, now);
      begin = end + 1;
    }
  }
  return files;
}

void Settle(Store* store, std::int64_t now) {
  for (OpenSession& session : store->open) {
    if (session.place == 0) continue;
    StoredGame& stored = GameFor(store, session.place);
    const std::int64_t length = std::clamp<std::int64_t>(session.last - session.start, 0, kLongestSession);
    if (length > session.counted) {
      stored.game.seconds += length - session.counted;
      session.counted = length;
    }
    if (session.last >= stored.game.last_played) {
      stored.game.last_played = session.last;
      stored.game.place_id = session.place;
      stored.game.user_id = session.user;
      stored.game.job_id = session.kind == "public" ? session.job : std::string();
    }
  }
  for (auto it = store->open.begin(); it != store->open.end();) {
    if (it->ended || now - it->last > kActivitySessionQuietSeconds) {
      store->done.push_back(it->id);
      it = store->open.erase(it);
    } else {
      ++it;
    }
  }
  if (store->done.size() > kMaxDone) {
    store->done.erase(store->done.begin(),
                      store->done.begin() + static_cast<std::ptrdiff_t>(store->done.size() - kMaxDone));
  }
  std::stable_sort(store->games.begin(), store->games.end(), [](const StoredGame& a, const StoredGame& b) {
    return a.game.last_played > b.game.last_played;
  });
  if (store->games.size() > kMaxActivityGames) store->games.resize(kMaxActivityGames);
}

void MergeSameUniverse(Store* store) {
  for (std::size_t i = 0; i < store->games.size(); ++i) {
    StoredGame& keep = store->games[i];
    if (keep.game.universe_id == 0) continue;
    for (std::size_t j = i + 1; j < store->games.size();) {
      StoredGame& other = store->games[j];
      if (other.game.universe_id != keep.game.universe_id) {
        ++j;
        continue;
      }
      const std::int64_t seconds = keep.game.seconds + other.game.seconds;
      std::vector<std::uint64_t> places = keep.places;
      for (const std::uint64_t place : other.places) {
        if (std::find(places.begin(), places.end(), place) == places.end() && places.size() < kMaxPlacesPerGame) {
          places.push_back(place);
        }
      }
      if (other.game.last_played > keep.game.last_played) keep = std::move(other);
      keep.game.seconds = seconds;
      keep.places = std::move(places);
      store->games.erase(store->games.begin() + static_cast<std::ptrdiff_t>(j));
    }
  }
}

std::vector<ActivityGame> Shown(const std::wstring& folder, const Store& store) {
  std::vector<ActivityGame> games;
  for (const StoredGame& stored : store.games) {
    ActivityGame game = stored.game;
    if (game.last_played <= 0) continue;
    if (game.universe_id != 0) {
      const std::wstring icon = IconPath(folder, game.universe_id);
      if (IsSmallFile(icon, kMaxIconBytes)) game.icon_file = icon;
    }
    game.playing = std::any_of(store.open.begin(), store.open.end(), [&stored](const OpenSession& session) {
      return std::find(stored.places.begin(), stored.places.end(), session.place) != stored.places.end();
    });
    games.push_back(std::move(game));
  }
  return games;
}

}

bool IsActivityServerId(std::string_view text) {
  if (text.size() != 36) return false;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const bool dash = i == 8 || i == 13 || i == 18 || i == 23;
    if (dash ? text[i] != '-' : !IsHexDigit(text[i])) return false;
  }
  return true;
}

std::vector<ActivityGame> ReadActivity(const std::wstring& folder, std::int64_t now) {
  if (folder.empty() || !PathIsDirectory(folder)) return {};
  std::lock_guard<std::mutex> lock(StoreMutex());
  Store store = LoadStore(folder);
  const std::size_t open_before = store.open.size();
  const std::vector<std::wstring> consumed = FoldJournals(folder, &store, now);
  Settle(&store, now);
  if (!consumed.empty() || store.open.size() != open_before) {
    if (SaveStore(folder, store)) {
      for (const std::wstring& file : consumed) DeleteFileW(file.c_str());
    }
  }
  return Shown(folder, store);
}

bool RemoveActivityGame(const std::wstring& folder, std::uint64_t place_id) {
  if (folder.empty() || place_id == 0) return false;
  std::lock_guard<std::mutex> lock(StoreMutex());
  Store store = LoadStore(folder);
  StoredGame* stored = GameWithPlace(&store, place_id);
  if (stored == nullptr) return false;
  const std::vector<std::uint64_t> places = stored->places;
  const std::uint64_t universe = stored->game.universe_id;
  store.games.erase(store.games.begin() + (stored - store.games.data()));
  for (auto it = store.open.begin(); it != store.open.end();) {
    if (std::find(places.begin(), places.end(), it->place) != places.end()) {
      store.done.push_back(it->id);
      it = store.open.erase(it);
    } else {
      ++it;
    }
  }
  if (!SaveStore(folder, store)) return false;
  if (universe != 0) DeleteFileW(IconPath(folder, universe).c_str());
  return true;
}

bool ClearActivity(const std::wstring& folder) {
  if (folder.empty() || !PathIsDirectory(folder)) return true;
  std::lock_guard<std::mutex> lock(StoreMutex());
  Store store = LoadStore(folder);
  for (const std::wstring& file : TakenJournals(folder)) DeleteFileW(file.c_str());
  DeleteFileW(JoinPath(folder, kActivityJournalFile).c_str());
  for (const StoredGame& stored : store.games) {
    if (stored.game.universe_id != 0) DeleteFileW(IconPath(folder, stored.game.universe_id).c_str());
  }
  Store empty;
  for (const OpenSession& session : store.open) empty.done.push_back(session.id);
  return SaveStore(folder, empty);
}

int UpdateActivityDetails(const std::wstring& folder, const HttpGetFunction& get, std::int64_t now,
                          std::size_t limit) {
  if (folder.empty() || !get || limit == 0 || !PathIsDirectory(folder)) return 0;
  struct Wanted {
    std::uint64_t place = 0;
    std::string icon_url;
    bool has_icon = false;
    bool ok = false;
    GameDetails details;
    std::string icon;
  };
  std::vector<Wanted> wanted;
  {
    std::lock_guard<std::mutex> lock(StoreMutex());
    const Store store = LoadStore(folder);
    for (const StoredGame& stored : store.games) {
      const ActivityGame& game = stored.game;
      const bool has_icon = game.universe_id != 0 && IsSmallFile(IconPath(folder, game.universe_id), kMaxIconBytes);
      const bool old = stored.checked <= 0 || now - stored.checked > kActivityDetailsMaxAgeSeconds ||
                       stored.checked > now + 60;
      if (!old && (has_icon || stored.icon_url.empty())) continue;
      Wanted item;
      item.place = game.place_id;
      item.icon_url = stored.icon_url;
      item.has_icon = has_icon;
      wanted.push_back(std::move(item));
      if (wanted.size() >= limit) break;
    }
  }
  if (wanted.empty()) return 0;
  for (Wanted& item : wanted) {
    item.ok = FetchGameDetails(item.place, get, &item.details);
    if (!item.ok || !IsRobloxImageUrl(item.details.icon_url)) continue;
    if (item.has_icon && item.details.icon_url == item.icon_url) continue;
    HttpResult image = get(Utf8ToWide(item.details.icon_url), kMaxIconBytes);
    if (image.ok && image.body.size() > kPngSignature.size() &&
        std::string_view(image.body).substr(0, kPngSignature.size()) == kPngSignature) {
      item.icon = std::move(image.body);
    }
  }
  int changed = 0;
  std::lock_guard<std::mutex> lock(StoreMutex());
  Store store = LoadStore(folder);
  for (Wanted& item : wanted) {
    StoredGame* stored = GameWithPlace(&store, item.place);
    if (stored == nullptr) continue;
    if (!item.ok) {
      stored->checked = now - kActivityDetailsMaxAgeSeconds + kRetryAfterFailureSeconds;
      continue;
    }
    ActivityGame& game = stored->game;
    const std::wstring name = CleanText(item.details.name, kMaxNameChars);
    const std::wstring description = CleanText(item.details.description, kMaxDescriptionChars);
    const std::wstring creator = CleanText(item.details.creator, kMaxCreatorChars);
    bool different = game.universe_id != item.details.universe_id || game.name != name ||
                     game.description != description || game.creator != creator ||
                     game.root_place_id != item.details.root_place_id;
    game.universe_id = item.details.universe_id;
    game.root_place_id = item.details.root_place_id;
    game.name = name;
    game.description = description;
    game.creator = creator;
    stored->checked = now;
    if (!item.icon.empty() && game.universe_id != 0 &&
        EnsureDirectory(JoinPath(folder, kIconsFolder), nullptr) &&
        AtomicWriteFile(IconPath(folder, game.universe_id), item.icon, AtomicWriteBackup::kNone, nullptr)) {
      stored->icon_url = item.details.icon_url;
      different = true;
    } else if (stored->icon_url.empty() && IsRobloxImageUrl(item.details.icon_url)) {
      stored->icon_url = item.details.icon_url;
    }
    if (different) ++changed;
  }
  MergeSameUniverse(&store);
  SaveStore(folder, store);
  return changed;
}

std::wstring ActivityJoinLink(const ActivityGame& game, bool same_server) {
  const std::uint64_t place = same_server || game.root_place_id == 0 ? game.place_id : game.root_place_id;
  if (place == 0) return {};
  std::wstring link = L"roblox://experiences/start?placeId=" + std::to_wstring(place);
  if (same_server) {
    if (!IsActivityServerId(game.job_id)) return {};
    link += L"&gameInstanceId=" + Utf8ToWide(game.job_id);
  }
  return link;
}

std::wstring ActivityDuration(std::int64_t seconds) {
  if (seconds <= 0) return {};
  const std::int64_t minutes = std::max<std::int64_t>(1, (seconds + 30) / 60);
  if (minutes < 60) return TF(L"{0} min", std::to_wstring(minutes));
  return TF(L"{0} h {1} min", std::to_wstring(minutes / 60), std::to_wstring(minutes % 60));
}

std::wstring ActivityDay(std::int64_t when, std::int64_t now) {
  if (when <= 0) return {};
  const auto local = [](std::int64_t seconds, std::tm* out) {
    const __time64_t value = seconds;
    return _localtime64_s(out, &value) == 0;
  };
  std::tm then{};
  std::tm today{};
  if (!local(when, &then) || !local(now, &today)) return {};
  if (then.tm_year == today.tm_year && then.tm_yday == today.tm_yday) return T(L"Today");
  std::tm yesterday{};
  if (local(now - 24 * 60 * 60, &yesterday) && then.tm_year == yesterday.tm_year &&
      then.tm_yday == yesterday.tm_yday) {
    return T(L"Yesterday");
  }
  return FormatDate(then.tm_year + 1900, then.tm_mon + 1, then.tm_mday);
}

}
