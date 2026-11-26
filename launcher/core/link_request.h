// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_LINK_REQUEST_H_
#define ELIXIR_LAUNCHER_CORE_LINK_REQUEST_H_

#include <cstdint>
#include <string>
#include <string_view>

namespace elixir::launcher {

inline constexpr wchar_t kOpenLinkFlag[] = L"--open-link";

enum class LinkRequestKind {
  kJoin,
  kOpenApp,
  kInvalid,
};

struct LinkRequest {
  LinkRequestKind kind = LinkRequestKind::kInvalid;
  std::string join;
  std::int64_t place_id = 0;
};

LinkRequest ParseLink(std::wstring_view link);

bool ValidateJoinRequest(std::string* join, std::int64_t* place_id);

void Wipe(std::string* text);
void Wipe(std::wstring* text);

void WipeLinkFromCommandLine();

}

#endif
