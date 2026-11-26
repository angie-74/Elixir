// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_ROBLOX_ACCOUNT_PROTOCOL_H_
#define ELIXIR_RUNTIME_ROBLOX_ACCOUNT_PROTOCOL_H_

#include <jni.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include "mocktail/status.h"
#include "runtime/roblox_game_session_native_adapter.h"

namespace mocktail {
namespace runtime {

struct RobloxAccountProtocolNames {
  std::string protocol = "Account";
  std::string device_integrity_available_method = "deviceIntegrityAvailable";
  std::string integrity_token_method = "getIntegrityToken";
  std::string support_key = "support";
  std::string token_key = "token";
  std::string result_key = "result";
  std::string request_hash_key = "requestHash";
  int from_library = 0;
};

using RobloxAccountProtocolStringFn = jstring (JNICALL*)(JNIEnv*, jclass);

struct RobloxAccountProtocolSymbols {
  RobloxAccountProtocolStringFn get_protocol_name = nullptr;
  RobloxAccountProtocolStringFn get_device_integrity_available_method_name = nullptr;
  RobloxAccountProtocolStringFn get_get_integrity_token_method_name = nullptr;
  RobloxAccountProtocolStringFn get_support_key = nullptr;
  RobloxAccountProtocolStringFn get_token_key = nullptr;
  RobloxAccountProtocolStringFn get_result_key = nullptr;
  RobloxAccountProtocolStringFn get_request_hash_key = nullptr;
};

RobloxAccountProtocolSymbols ResolveRobloxAccountProtocolSymbols(void* roblox_library);

RobloxAccountProtocolNames ReadRobloxAccountProtocolNames(JNIEnv* env, const RobloxAccountProtocolSymbols& symbols);

std::string DeviceIntegrityAvailableReply(const RobloxAccountProtocolNames& names);

struct RobloxIntegrityTokenReply {
  std::string json;
  const char* result;
};

RobloxIntegrityTokenReply IntegrityTokenReplyWithoutProvider(const RobloxAccountProtocolNames& names,
                                                             std::string_view request_json);

struct RobloxAccountProtocolBridgeSymbols {
  void (JNICALL* set_request_handler_raw)(JNIEnv*, jobject, jstring, jstring, jobject) = nullptr;
  void (JNICALL* clear_request_handler)(JNIEnv*, jobject, jstring, jstring) = nullptr;

  bool complete() const { return set_request_handler_raw != nullptr && clear_request_handler != nullptr; }
};

struct RobloxAccountProtocolBridgeObjects {
  jobject message_bus = nullptr;
  void* factory_context = nullptr;
  jobject (*create_request_handler)(void* context, std::shared_ptr<void> callback_context,
                                    std::string (*run)(void*, JNIEnv*, jstring)) = nullptr;
  void (*clear_request_handler)(void* context, jobject handler) = nullptr;

  bool complete() const {
    return message_bus != nullptr && factory_context != nullptr && create_request_handler != nullptr &&
           clear_request_handler != nullptr;
  }
};

class RobloxAccountProtocolBridge final {
 public:
  RobloxAccountProtocolBridge(JniEnvironmentProvider environment, RobloxAccountProtocolBridgeSymbols symbols,
                              RobloxAccountProtocolBridgeObjects objects, RobloxAccountProtocolNames names);
  ~RobloxAccountProtocolBridge();
  RobloxAccountProtocolBridge(const RobloxAccountProtocolBridge&) = delete;
  RobloxAccountProtocolBridge& operator=(const RobloxAccountProtocolBridge&) = delete;

  Status Initialize();
  Status Shutdown();
  int availability_requests() const;
  int token_requests() const;

 private:
  struct Target;
  static std::string OnDeviceIntegrityAvailable(void* context, JNIEnv* env, jstring request);
  static std::string OnGetIntegrityToken(void* context, JNIEnv* env, jstring request);
  Status ShutdownLocked(JNIEnv* env);

  const JniEnvironmentProvider environment_;
  const RobloxAccountProtocolBridgeSymbols symbols_;
  const RobloxAccountProtocolBridgeObjects objects_;
  const std::shared_ptr<Target> target_;
  std::mutex mutex_;
  jobject message_bus_ = nullptr;
  jstring protocol_ = nullptr;
  jstring methods_[2] = {nullptr, nullptr};
  jobject handlers_[2] = {nullptr, nullptr};
  bool registered_[2] = {false, false};
  bool initialized_ = false;
};

}
}

#endif
