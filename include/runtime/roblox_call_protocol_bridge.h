// Modified by Elixir.
#ifndef MOCKTAIL_RUNTIME_ROBLOX_CALL_PROTOCOL_BRIDGE_H_
#define MOCKTAIL_RUNTIME_ROBLOX_CALL_PROTOCOL_BRIDGE_H_

#include "runtime/roblox_message_bus_request_bridge.h"

namespace mocktail {
namespace runtime {

class RobloxCallProtocolBridge final {
 public:
  RobloxCallProtocolBridge(JniEnvironmentProvider environment,
                           RobloxMessageBusSymbols symbols,
                           RobloxMessageBusObjects objects);
  Status Initialize();
  Status Shutdown();

 private:
  RobloxMessageBusRequestBridge bridge_;
};

}
}
#endif
