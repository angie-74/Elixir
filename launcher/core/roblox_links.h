// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// The key layout follows Fishstrap's WindowsRegistry.RegisterProtocol (MIT).
#ifndef ELIXIR_LAUNCHER_CORE_ROBLOX_LINKS_H_
#define ELIXIR_LAUNCHER_CORE_ROBLOX_LINKS_H_

#include <windows.h>

#include <string>

namespace elixir::launcher {

struct LinkRegistry {
  HKEY root = HKEY_CURRENT_USER;
  std::wstring classes = L"Software\\Classes";
  std::wstring player_uninstall = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\roblox-player";
  std::wstring user_choice = L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations";
};

inline constexpr const wchar_t* kLinkSchemes[] = {L"roblox-player", L"roblox"};

enum class LinkHandler {
  kNone,
  kElixir,
  kOther,
};

struct LinkStatus {
  LinkHandler roblox_player = LinkHandler::kNone;
  LinkHandler roblox = LinkHandler::kNone;
  bool overridden_by_user_choice = false;

  bool all_elixir() const { return roblox_player == LinkHandler::kElixir && roblox == LinkHandler::kElixir; }
};

std::wstring LinkCommand(const std::wstring& exe);

LinkStatus QueryLinkHandlers(const LinkRegistry& registry, const std::wstring& exe);

std::wstring LinkRegistrationBlocker();

bool RegisterLinkHandlers(const LinkRegistry& registry, const std::wstring& exe, const std::wstring& backup_file,
                          std::wstring* error);

bool UnregisterLinkHandlers(const LinkRegistry& registry, const std::wstring& exe, const std::wstring& backup_file,
                            std::wstring* error);

}

#endif
