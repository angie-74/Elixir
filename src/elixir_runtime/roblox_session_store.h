// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_ROBLOX_SESSION_STORE_H_
#define ELIXIR_RUNTIME_ROBLOX_SESSION_STORE_H_

#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>

namespace elixir::runtime {

class RobloxSessionStore final {
 public:
  enum class LoadResult {
    kFound,
    kMissing,
    kUnreadable,
  };

  enum class Content {
    kSession,
    kCookies,
  };

  explicit RobloxSessionStore(std::filesystem::path file, Content content = Content::kSession);

  RobloxSessionStore(const RobloxSessionStore&) = delete;
  RobloxSessionStore& operator=(const RobloxSessionStore&) = delete;

  LoadResult Load(std::string* credential) const;
  bool Save(std::string_view credential);
  void Clear();
  void ClearIfEquals(std::string_view credential);

  const std::filesystem::path& file() const { return file_; }

  static bool IsCanonical(std::string_view credential);
  static bool IsCookieList(std::string_view cookies);
  static bool EnsurePrivateFolder(const std::filesystem::path& folder);

 private:
  bool Accepts(std::string_view value) const;
  bool EnsureFolder() const;
  LoadResult LoadLocked(std::string* credential) const;

  std::filesystem::path file_;
  Content content_;
  mutable std::mutex mutex_;
};

}

#endif
