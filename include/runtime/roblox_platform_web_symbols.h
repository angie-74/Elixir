// Modified by Elixir.
#ifndef MOCKTAIL_RUNTIME_ROBLOX_PLATFORM_WEB_SYMBOLS_H_
#define MOCKTAIL_RUNTIME_ROBLOX_PLATFORM_WEB_SYMBOLS_H_

#include "runtime/roblox_browser_service_bridge.h"
#include "runtime/roblox_permissions_bridge.h"
#include "runtime/roblox_web_view_bridge.h"

namespace mocktail {
namespace runtime {

struct RobloxPlatformWebSymbols {
  RobloxWebViewMessageBusSymbols web_view;
  RobloxBrowserServiceSymbols browser_service;
  RobloxPermissionsMessageBusSymbols permissions;

  bool complete() const {
    return web_view.complete() && browser_service.complete() &&
           permissions.complete();
  }
};

RobloxPlatformWebSymbols ResolveRobloxPlatformWebSymbols(
    void* roblox_library, SubscribeWebViewRawFn subscribe_raw,
    DeleteWebViewConnectionFn delete_connection);

}
}

#endif
