// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#ifndef DISCORD_RPC_REGISTRY_H_
#define DISCORD_RPC_REGISTRY_H_

#include <memory>
#include <string>

#include "discord_rpc/helper.h"
#include "discord_rpc/logging.h"

namespace discord_rpc::registry {

struct SchemeInfo {
  String ApplicationID;
  String SteamAppID;
  String ExecutablePath;

  bool UsingSteamApp() const { return !helper::IsNullOrEmpty(SteamAppID); }
};

class IRegisterUriScheme {
 public:
  virtual ~IRegisterUriScheme() = default;

  virtual bool Register(const SchemeInfo& info) = 0;
};

class WindowsUriScheme final : public IRegisterUriScheme {
 public:
  explicit WindowsUriScheme(std::shared_ptr<logging::ILogger> logger) : WindowsUriScheme(std::move(logger), nullptr) {}
  WindowsUriScheme(std::shared_ptr<logging::ILogger> logger, void* root) : logger_(std::move(logger)), root_(root) {}

  bool Register(const SchemeInfo& info) override;

  String GetSteamLocation();

 private:
  void CreateUriScheme(const std::string& scheme, const std::string& friendly_name, const std::string& default_icon,
                       const std::string& command);

  std::shared_ptr<logging::ILogger> logger_;
  void* root_;
};

struct UriScheme {
  static bool Register(const SchemeInfo& info, std::shared_ptr<logging::ILogger> logger = nullptr);
};

}

#endif
