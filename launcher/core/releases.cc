// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0

#include "core/releases.h"

#include <algorithm>
#include <cctype>

#include "core/config.h"
#include "core/json.h"
#include "core/project.h"
#include "core/sha256.h"
#include "core/util.h"
#include "core/i18n.h"

namespace elixir::launcher {

namespace {

constexpr std::size_t kMaxNotesBytes = 6000;

std::string RepositoryUrl() { return std::string("https://github.com/") + kProjectRepository + "/"; }

bool StartsWith(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

bool IsVersionText(std::string_view text) {
  if (text.empty() || text.size() > 40) return false;
  for (const char c : text) {
    const bool allowed = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' ||
                         c == '-' || c == '+';
    if (!allowed) return false;
  }
  return text.front() >= '0' && text.front() <= '9';
}

std::string StripInline(std::string_view line) {
  std::string out;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (c == '`') continue;
    if (c == '*' || c == '_') {
      const bool doubled = i + 1 < line.size() && line[i + 1] == c;
      const bool word_inside = c == '_' && i > 0 && i + 1 < line.size() && std::isalnum(static_cast<unsigned char>(line[i - 1])) &&
                               std::isalnum(static_cast<unsigned char>(line[i + 1]));
      if (word_inside) {
        out.push_back(c);
        continue;
      }
      if (doubled) ++i;
      continue;
    }
    if (c == '[') {
      const std::size_t close = line.find(']', i + 1);
      if (close != std::string_view::npos && close + 1 < line.size() && line[close + 1] == '(') {
        const std::size_t end = line.find(')', close + 2);
        if (end != std::string_view::npos) {
          out += StripInline(line.substr(i + 1, close - i - 1));
          i = end;
          continue;
        }
      }
    }
    out.push_back(c);
  }
  return out;
}

}

std::wstring ReleasesApiUrl() {
  return L"https://api.github.com/repos/" + Utf8ToWide(kProjectRepository) + L"/releases?per_page=30";
}

std::string PlainReleaseNotes(std::string_view markdown) {
  std::string out;
  bool blank_pending = false;
  bool in_comment = false;
  std::size_t start = 0;
  while (start <= markdown.size()) {
    std::size_t end = markdown.find('\n', start);
    if (end == std::string_view::npos) end = markdown.size();
    std::string_view line = markdown.substr(start, end - start);
    start = end + 1;
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (in_comment) {
      if (line.find("-->") != std::string_view::npos) in_comment = false;
      continue;
    }
    if (StartsWith(TrimAscii(line), "<!--")) {
      if (line.find("-->") == std::string_view::npos) in_comment = true;
      continue;
    }
    std::size_t indent = 0;
    while (indent < line.size() && (line[indent] == ' ' || line[indent] == '\t')) ++indent;
    std::string_view body = line.substr(indent);
    while (!body.empty() && (body.back() == ' ' || body.back() == '\t')) body.remove_suffix(1);
    if (StartsWith(body, "```")) continue;
    std::string text;
    if (!body.empty() && body.front() == '#') {
      while (!body.empty() && body.front() == '#') body.remove_prefix(1);
      text = StripInline(TrimAscii(body));
    } else if (body.size() >= 2 && (body[0] == '-' || body[0] == '*' || body[0] == '+') && body[1] == ' ') {
      text = "\xE2\x80\xA2 " + StripInline(TrimAscii(body.substr(2)));
    } else {
      text = StripInline(body);
    }
    if (text.empty()) {
      blank_pending = !out.empty();
      continue;
    }
    if (!out.empty()) out += blank_pending ? "\n\n" : "\n";
    blank_pending = false;
    out += text;
    if (out.size() > kMaxNotesBytes) break;
  }
  if (out.size() > kMaxNotesBytes) {
    std::size_t cut = kMaxNotesBytes;
    while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) --cut;
    out.resize(cut);
    out += "\xE2\x80\xA6";
  }
  return out;
}

bool ParseReleases(std::string_view json_text, std::vector<ElixirRelease>* releases, std::string* error) {
  releases->clear();
  std::string parse_error;
  const auto json = Json::Parse(json_text, &parse_error);
  if (!json || !json->is_array()) {
    if (error != nullptr) *error = "The list of versions couldn’t be read.";
    return false;
  }
  const std::string repository = RepositoryUrl();
  const std::string downloads = repository + "releases/download/";
  for (const Json& item : json->items()) {
    if (!item.is_object() || item.GetBool("draft", false)) continue;
    ElixirRelease release;
    release.tag = TrimAscii(item.GetString("tag_name"));
    std::string_view version = release.tag;
    if (!version.empty() && (version.front() == 'v' || version.front() == 'V')) version.remove_prefix(1);
    if (!IsVersionText(version)) continue;
    release.version = std::string(version);
    release.name = TrimAscii(item.GetString("name"));
    release.notes = PlainReleaseNotes(item.GetString("body"));
    release.published = item.GetString("published_at").substr(0, 10);
    release.prerelease = item.GetBool("prerelease", false);
    const std::string page = item.GetString("html_url");
    if (StartsWith(page, repository) && IsHttpsUrl(page)) release.page_url = page;
    if (const Json* assets = item.Find("assets"); assets != nullptr && assets->is_array()) {
      for (const Json& asset : assets->items()) {
        if (!asset.is_object() || asset.GetString("name") != kReleaseAssetName) continue;
        const std::string url = asset.GetString("browser_download_url");
        if (!StartsWith(url, downloads) || !IsHttpsUrl(url)) continue;
        std::string digest = asset.GetString("digest");
        if (StartsWith(digest, "sha256:")) digest.erase(0, 7);
        digest = NormalizeSha256(digest);
        release.download_url = url;
        release.sha256 = IsSha256Hex(digest) ? digest : std::string();
        release.size = static_cast<std::uint64_t>(std::max<std::int64_t>(0, asset.GetInt("size", 0)));
        break;
      }
    }
    releases->push_back(std::move(release));
  }
  std::stable_sort(releases->begin(), releases->end(), [](const ElixirRelease& a, const ElixirRelease& b) {
    return CompareVersions(a.version, b.version) > 0;
  });
  return true;
}

bool ReleaseCanBeInstalled(const ElixirRelease& release) {
  return !release.download_url.empty() && IsSha256Hex(release.sha256);
}

const ElixirRelease* NewestRelease(const std::vector<ElixirRelease>& releases) {
  for (const ElixirRelease& release : releases) {
    if (!release.prerelease && ReleaseCanBeInstalled(release)) return &release;
  }
  return nullptr;
}

UpdateManifest ManifestForRelease(const ElixirRelease& release) {
  UpdateManifest manifest;
  manifest.version = release.version;
  manifest.url = release.download_url;
  manifest.sha256 = release.sha256;
  manifest.notes = release.notes;
  return manifest;
}

ReleaseList FetchReleases(const HttpGetFunction& get) {
  ReleaseList list;
  if (!get) {
    list.error = T(L"Couldn’t check for updates.");
    return list;
  }
  const HttpResult http = get(ReleasesApiUrl(), 2u << 20);
  if (!http.ok) {
    if (http.status == 404) {
      list.ok = true;
      return list;
    }
    list.error = http.status == 403 || http.status == 429
                     ? std::wstring(T(L"GitHub is limiting requests right now. Try again in a few minutes."))
                     : http.status != 0 ? std::wstring(T(L"GitHub didn’t answer. Try again later.")) : http.error;
    if (list.error.empty()) list.error = T(L"Couldn’t check for updates. Try again later.");
    return list;
  }
  std::string error;
  if (!ParseReleases(http.body, &list.releases, &error)) {
    list.error = Utf8ToWide(error);
    return list;
  }
  list.ok = true;
  return list;
}

ReleaseList FetchReleasesCached(const HttpGetFunction& get, const std::wstring& cache_file, std::int64_t now,
                                bool fresh) {
  if (!fresh && !cache_file.empty()) {
    std::string bytes;
    std::string parse_error;
    if (ReadFileBytes(cache_file, &bytes, nullptr, 4u << 20)) {
      const auto cached = Json::Parse(bytes, &parse_error);
      if (cached && cached->is_object()) {
        const std::int64_t fetched = cached->GetInt("fetched", 0);
        ReleaseList list;
        if (fetched <= now && now - fetched < kReleaseListMaxAgeSeconds &&
            ParseReleases(cached->GetString("body"), &list.releases, &parse_error)) {
          list.ok = true;
          return list;
        }
      }
    }
  }
  if (!get) return FetchReleases(get);
  std::string body;
  const HttpGetFunction keeping = [&get, &body](const std::wstring& url, std::uint64_t max_bytes) {
    HttpResult result = get(url, max_bytes);
    if (result.ok) body = result.body;
    return result;
  };
  ReleaseList list = FetchReleases(keeping);
  if (list.ok && !body.empty() && !cache_file.empty()) {
    Json cache = Json::MakeObject();
    cache.Set("fetched", now);
    cache.Set("body", body);
    EnsureDirectory(ParentDirectory(cache_file), nullptr);
    AtomicWriteFile(cache_file, cache.Dump(), AtomicWriteBackup::kNone, nullptr);
  }
  return list;
}

}
