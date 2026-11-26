// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/link_joins.h"

#include <windows.h>

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <utility>

#include "elixir/core_api.h"
#include "elixir_runtime/host_log.h"

#if defined(ELIXIR_RUNTIME_HAS_EXPERIENCE)
#include "runtime/external_launch_broker.h"
#include "runtime/roblox_experience_launch_request.h"
#endif

namespace elixir::runtime {

#if defined(ELIXIR_RUNTIME_HAS_EXPERIENCE)
namespace {

namespace mr = mocktail::runtime;

void WipeRequest(mr::RobloxExperienceLaunchRequest* request) {
  for (std::string* text :
       {&request->game_instance_id, &request->reserved_server_access_code, &request->call_id, &request->referral_page,
        &request->access_code, &request->link_code, &request->launch_data, &request->event_id,
        &request->game_join_context, &request->join_attempt_id, &request->join_attempt_origin, &request->iso_context,
        &request->canonical_json}) {
    if (!text->empty()) SecureZeroMemory(text->data(), text->size());
    text->clear();
  }
}

struct LinkJoins {
  std::mutex mutex;
  std::deque<mr::RobloxExperienceLaunchRequest> waiting;
  std::shared_ptr<mr::ExternalLaunchBroker> broker;
  bool stopping = false;
  bool unavailable = false;
};

LinkJoins& State() {
  static LinkJoins* state = new LinkJoins();
  return *state;
}

int QueueInBroker(mr::ExternalLaunchBroker& broker, mr::RobloxExperienceLaunchRequest request) {
  const mocktail::Status status = broker.QueueInitialRequest(std::move(request));
  if (status.ok()) return ELIXIR_LAUNCH_ACCEPTED;
  return status.code() == mocktail::StatusCode::kUnavailable ? ELIXIR_LAUNCH_BUSY : ELIXIR_LAUNCH_STOPPING;
}

std::size_t DropWaiting(LinkJoins& state) {
  const std::size_t dropped = state.waiting.size();
  for (mr::RobloxExperienceLaunchRequest& request : state.waiting) WipeRequest(&request);
  state.waiting.clear();
  return dropped;
}

}

int SubmitLinkJoin(std::string_view json) {
  if (json.empty() || json.size() > mr::kMaximumExperienceLaunchJsonBytes) return ELIXIR_LAUNCH_INVALID;
  std::string copy(json);
  mr::RobloxExperienceLaunchRequest request;
  const mocktail::Status parsed = mr::ParseRobloxExperienceLaunchJson(copy, &request);
  SecureZeroMemory(copy.data(), copy.size());
  if (!parsed.ok()) {
    WipeRequest(&request);
    LogError("link join: the request was refused (" + parsed.message() + ")");
    return ELIXIR_LAUNCH_INVALID;
  }
  const std::int64_t place_id = request.place_id;
  int result = ELIXIR_LAUNCH_ACCEPTED;
  {
    LinkJoins& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.unavailable) {
      result = ELIXIR_LAUNCH_UNAVAILABLE;
    } else if (state.stopping) {
      result = ELIXIR_LAUNCH_STOPPING;
    } else if (state.broker != nullptr) {
      result = QueueInBroker(*state.broker, std::move(request));
    } else if (state.waiting.size() >= kMaximumQueuedLinkJoins) {
      result = ELIXIR_LAUNCH_BUSY;
    } else {
      state.waiting.push_back(std::move(request));
    }
  }
  WipeRequest(&request);
  if (result == ELIXIR_LAUNCH_ACCEPTED) {
    Log("link join: place " + std::to_string(place_id) + " queued; Roblox joins once the player is signed in");
    EmitEvent(3, "join", "queued");
  }
  return result;
}

void AttachLinkJoinBroker(std::shared_ptr<mr::ExternalLaunchBroker> broker) {
  if (broker == nullptr) return;
  LinkJoins& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);
  if (state.unavailable) return;
  state.broker = std::move(broker);
  state.stopping = false;
  while (!state.waiting.empty()) {
    mr::RobloxExperienceLaunchRequest request = std::move(state.waiting.front());
    state.waiting.pop_front();
    static_cast<void>(QueueInBroker(*state.broker, std::move(request)));
    WipeRequest(&request);
  }
}

void DetachLinkJoinBroker() {
  LinkJoins& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.broker.reset();
  state.stopping = true;
  DropWaiting(state);
}

std::size_t RefuseLinkJoins() {
  LinkJoins& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.broker.reset();
  state.unavailable = true;
  return DropWaiting(state);
}

void ResetLinkJoinsForTest() {
  LinkJoins& state = State();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.broker.reset();
  state.stopping = false;
  state.unavailable = false;
  DropWaiting(state);
}

#else

int SubmitLinkJoin(std::string_view) { return ELIXIR_LAUNCH_UNAVAILABLE; }
void AttachLinkJoinBroker(std::shared_ptr<mocktail::runtime::ExternalLaunchBroker>) {}
void DetachLinkJoinBroker() {}
std::size_t RefuseLinkJoins() { return 0; }
void ResetLinkJoinsForTest() {}

#endif

}
