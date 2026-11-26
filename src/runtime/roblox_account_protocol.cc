// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "runtime/roblox_account_protocol.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <utility>

namespace mocktail {
namespace runtime {
namespace {

bool Read(JNIEnv* env, RobloxAccountProtocolStringFn native, std::string* name) {
  if (env == nullptr || native == nullptr) return false;
  const jstring answer = native(env, nullptr);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    if (answer != nullptr) env->DeleteLocalRef(answer);
    return false;
  }
  if (answer == nullptr) return false;
  bool read = false;
  if (const char* text = env->GetStringUTFChars(answer, nullptr)) {
    if (text[0] != '\0') {
      *name = text;
      read = true;
    }
    env->ReleaseStringUTFChars(answer, text);
  }
  env->DeleteLocalRef(answer);
  return read;
}

}

RobloxAccountProtocolNames ReadRobloxAccountProtocolNames(JNIEnv* env, const RobloxAccountProtocolSymbols& symbols) {
  RobloxAccountProtocolNames names;
  names.from_library += Read(env, symbols.get_protocol_name, &names.protocol) ? 1 : 0;
  names.from_library +=
      Read(env, symbols.get_device_integrity_available_method_name, &names.device_integrity_available_method) ? 1 : 0;
  names.from_library += Read(env, symbols.get_get_integrity_token_method_name, &names.integrity_token_method) ? 1 : 0;
  names.from_library += Read(env, symbols.get_support_key, &names.support_key) ? 1 : 0;
  names.from_library += Read(env, symbols.get_token_key, &names.token_key) ? 1 : 0;
  names.from_library += Read(env, symbols.get_result_key, &names.result_key) ? 1 : 0;
  names.from_library += Read(env, symbols.get_request_hash_key, &names.request_hash_key) ? 1 : 0;
  return names;
}

std::string DeviceIntegrityAvailableReply(const RobloxAccountProtocolNames& names) {
  nlohmann::ordered_json reply = nlohmann::ordered_json::object();
  reply[names.support_key] = true;
  return reply.dump();
}

RobloxIntegrityTokenReply IntegrityTokenReplyWithoutProvider(const RobloxAccountProtocolNames& names,
                                                             std::string_view request_json) {
  bool has_request_hash = false;
  const nlohmann::json request = nlohmann::json::parse(request_json, nullptr, false);
  if (request.is_object()) {
    const auto hash = request.find(names.request_hash_key);
    if (hash != request.end()) {
      has_request_hash = hash->is_string() ? !hash->get_ref<const std::string&>().empty() : !hash->is_null();
    }
  }
  RobloxIntegrityTokenReply reply;
  reply.result = has_request_hash ? "TOKEN_PROVIDER_UNINITIALIZED" : "INVALID_NONCE";
  nlohmann::ordered_json body = nlohmann::ordered_json::object();
  body[names.token_key] = "";
  body[names.result_key] = reply.result;
  reply.json = body.dump();
  return reply;
}

struct RobloxAccountProtocolBridge::Target {
  RobloxAccountProtocolNames names;
  std::atomic<int> availability_requests{0};
  std::atomic<int> token_requests{0};
};

RobloxAccountProtocolBridge::RobloxAccountProtocolBridge(JniEnvironmentProvider environment,
                                                         RobloxAccountProtocolBridgeSymbols symbols,
                                                         RobloxAccountProtocolBridgeObjects objects,
                                                         RobloxAccountProtocolNames names)
    : environment_(environment), symbols_(symbols), objects_(objects), target_(std::make_shared<Target>()) {
  target_->names = std::move(names);
}

RobloxAccountProtocolBridge::~RobloxAccountProtocolBridge() { (void)Shutdown(); }

int RobloxAccountProtocolBridge::availability_requests() const { return target_->availability_requests.load(); }
int RobloxAccountProtocolBridge::token_requests() const { return target_->token_requests.load(); }

std::string RobloxAccountProtocolBridge::OnDeviceIntegrityAvailable(void* context, JNIEnv*, jstring) {
  auto* target = static_cast<Target*>(context);
  const int count = target->availability_requests.fetch_add(1) + 1;
  std::fprintf(stderr, "  [messagebus] %s method=%s result=supported transport=sync count=%d\n",
               target->names.protocol.c_str(), target->names.device_integrity_available_method.c_str(), count);
  return DeviceIntegrityAvailableReply(target->names);
}

std::string RobloxAccountProtocolBridge::OnGetIntegrityToken(void* context, JNIEnv* env, jstring request) {
  auto* target = static_cast<Target*>(context);
  std::string body;
  if (env != nullptr && request != nullptr) {
    if (const char* text = env->GetStringUTFChars(request, nullptr)) {
      body = text;
      env->ReleaseStringUTFChars(request, text);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
  }
  const RobloxIntegrityTokenReply reply = IntegrityTokenReplyWithoutProvider(target->names, body);
  const int count = target->token_requests.fetch_add(1) + 1;
  std::fprintf(stderr, "  [messagebus] %s method=%s result=%s transport=sync count=%d\n",
               target->names.protocol.c_str(), target->names.integrity_token_method.c_str(), reply.result, count);
  return reply.json;
}

Status RobloxAccountProtocolBridge::Initialize() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (initialized_) {
    return Status::Error(StatusCode::kFailedPrecondition, "the Account protocol bridge is already initialized");
  }
  const RobloxAccountProtocolNames& names = target_->names;
  if (!environment_.valid() || !symbols_.complete() || !objects_.complete() || names.protocol.empty() ||
      names.device_integrity_available_method.empty() || names.integrity_token_method.empty()) {
    return Status::Error(StatusCode::kUnavailable, "the Account protocol bridge's prerequisites are incomplete");
  }
  JNIEnv* env = nullptr;
  Status status = environment_.Acquire(&env);
  if (!status.ok()) return status;

  const auto global_string = [env](const std::string& text) -> jstring {
    const jstring local = env->NewStringUTF(text.c_str());
    if (local == nullptr) return nullptr;
    const auto global = static_cast<jstring>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    return global;
  };
  message_bus_ = env->NewGlobalRef(objects_.message_bus);
  protocol_ = global_string(names.protocol);
  methods_[0] = global_string(names.device_integrity_available_method);
  methods_[1] = global_string(names.integrity_token_method);
  std::string (*const run[2])(void*, JNIEnv*, jstring) = {&OnDeviceIntegrityAvailable, &OnGetIntegrityToken};
  for (int i = 0; i < 2; ++i) {
    const jobject local = objects_.create_request_handler(objects_.factory_context, target_, run[i]);
    if (local != nullptr) {
      handlers_[i] = env->NewGlobalRef(local);
      if (handlers_[i] == nullptr) objects_.clear_request_handler(objects_.factory_context, local);
      env->DeleteLocalRef(local);
    }
  }
  const bool exception = env->ExceptionCheck();
  if (exception) env->ExceptionClear();
  if (exception || message_bus_ == nullptr || protocol_ == nullptr || methods_[0] == nullptr ||
      methods_[1] == nullptr || handlers_[0] == nullptr || handlers_[1] == nullptr) {
    (void)ShutdownLocked(env);
    return Status::Error(StatusCode::kUnavailable, "could not create the Account protocol's request handlers");
  }
  for (int i = 0; i < 2; ++i) {
    symbols_.set_request_handler_raw(env, message_bus_, protocol_, methods_[i], handlers_[i]);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      (void)ShutdownLocked(env);
      return Status::Error(StatusCode::kUnavailable, "could not register the Account protocol's request handlers");
    }
    registered_[i] = true;
  }
  initialized_ = true;
  std::fprintf(stderr, "  [messagebus] %s ready sync=2\n", names.protocol.c_str());
  return Status::Ok();
}

Status RobloxAccountProtocolBridge::Shutdown() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!initialized_ && message_bus_ == nullptr) return Status::Ok();
  JNIEnv* env = nullptr;
  const Status status = environment_.Acquire(&env);
  if (!status.ok()) return status;
  return ShutdownLocked(env);
}

Status RobloxAccountProtocolBridge::ShutdownLocked(JNIEnv* env) {
  Status result = Status::Ok();
  for (int i = 0; i < 2; ++i) {
    if (registered_[i]) {
      symbols_.clear_request_handler(env, message_bus_, protocol_, methods_[i]);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        result = Status::Error(StatusCode::kUnavailable, "could not clear an Account protocol request handler");
      }
      registered_[i] = false;
    }
    if (handlers_[i] != nullptr) {
      objects_.clear_request_handler(objects_.factory_context, handlers_[i]);
      env->DeleteGlobalRef(handlers_[i]);
      handlers_[i] = nullptr;
    }
    if (methods_[i] != nullptr) {
      env->DeleteGlobalRef(methods_[i]);
      methods_[i] = nullptr;
    }
  }
  if (protocol_ != nullptr) {
    env->DeleteGlobalRef(protocol_);
    protocol_ = nullptr;
  }
  if (message_bus_ != nullptr) {
    env->DeleteGlobalRef(message_bus_);
    message_bus_ = nullptr;
  }
  initialized_ = false;
  return result;
}

}
}
