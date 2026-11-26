// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_CORE_INSTANCE_H_
#define ELIXIR_LAUNCHER_CORE_INSTANCE_H_

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

namespace elixir::launcher {

inline constexpr std::size_t kMaxInstancePayloadBytes = 64 * 1024;

std::wstring InstanceKey(const std::wstring& data_root);

enum class InstanceMessageKind { kActivate, kLink, kOpenPage };

struct InstanceMessage {
  InstanceMessageKind kind = InstanceMessageKind::kActivate;
  std::string payload;
};

enum class InstanceReply : std::uint8_t {
  kAccepted = 0,
  kInvalid = 1,
  kQueueFull = 2,
  kDenied = 3,
  kStopping = 4,
};

std::string EncodeInstanceMessage(const InstanceMessage& message);
bool DecodeInstanceMessage(std::string_view bytes, InstanceMessage* message);

class InstanceOwner final {
 public:
  InstanceOwner() = default;
  ~InstanceOwner();
  InstanceOwner(const InstanceOwner&) = delete;
  InstanceOwner& operator=(const InstanceOwner&) = delete;

  bool Acquire(const std::wstring& key);
  bool Listen(std::function<InstanceReply(const InstanceMessage&)> handler, std::wstring* error);
  void Stop();
  bool owns() const { return mutex_ != nullptr; }

 private:
  void Serve();

  std::wstring key_;
  HANDLE mutex_ = nullptr;
  HANDLE pipe_ = nullptr;
  HANDLE stop_ = nullptr;
  std::thread thread_;
  std::function<InstanceReply(const InstanceMessage&)> handler_;
};

bool SendToInstanceOwner(const std::wstring& key, const InstanceMessage& message, unsigned timeout_ms,
                         InstanceReply* reply, std::wstring* error);

}

#endif
