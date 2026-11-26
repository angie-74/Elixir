// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/sign_in_identity.h"

#include <condition_variable>
#include <system_error>
#include <utility>

#include "elixir_runtime/roblox_session_store.h"
#include "jnivm/jnivm.h"
#include "services/auth_service.h"
#include "services/http_client.h"

#include <windows.h>

namespace elixir::runtime {
namespace {

namespace ms = mocktail::services;

constexpr std::string_view kCredentialPrefix = ".ROBLOSECURITY=";
constexpr std::size_t kMaximumCredentialBytes = 64 * 1024;

bool IsCanonicalCredential(const char* data, std::size_t size) {
  if (data == nullptr || size <= kCredentialPrefix.size() || size > kMaximumCredentialBytes) return false;
  const std::string_view credential(data, size);
  if (credential.substr(0, kCredentialPrefix.size()) != kCredentialPrefix) return false;
  for (const char character : credential.substr(kCredentialPrefix.size())) {
    const auto byte = static_cast<unsigned char>(character);
    if (byte <= 0x20 || byte >= 0x7f || byte == ';') return false;
  }
  return true;
}

void ClearSecret(std::string* value) {
  if (!value->empty()) SecureZeroMemory(value->data(), value->size());
  value->clear();
}

void AssignSecret(std::string* target, std::string_view value) {
  ClearSecret(target);
  if (target->capacity() < value.size()) target->reserve(value.size());
  target->assign(value.data(), value.size());
}

ms::AuthSession ResolveAccount(ms::HttpClient& http, const std::string& credential) {
  try {
    ms::AuthService service(http);
    return service.ResolveSession(credential, false);
  } catch (...) {
    ms::AuthSession failed;
    failed.status = ms::AuthSessionStatus::kUnavailable;
    failed.error = "the request could not be made";
    return failed;
  }
}

std::string HttpStatusText(long status) {
  return status > 0 ? " (HTTP " + std::to_string(status) + ")" : std::string();
}

std::string DelayText(std::chrono::milliseconds delay) {
  const long long millis = delay.count();
  return millis % 1000 == 0 ? std::to_string(millis / 1000) + " s" : std::to_string(millis) + " ms";
}

enum class Outcome {
  kPending,
  kResolved,
  kRejected,
  kUnavailable,
};

}

struct SignInIdentity::State {
  std::mutex mutex;
  std::condition_variable wake;
  std::condition_variable changed;
  jnivm::VM* vm = nullptr;
  std::shared_ptr<ms::HttpClient> http;
  std::function<void(std::string_view)> log;
  std::vector<std::chrono::milliseconds> retry_delays;
  std::string pending;
  bool has_pending = false;
  std::string last;
  Outcome last_outcome = Outcome::kPending;
  std::shared_ptr<RobloxSessionStore> store;
  std::string saved;
  bool busy = false;
  bool stopping = false;
  bool exited = false;

  void Log(const std::string& line) {
    if (!log) return;
    try {
      log(line);
    } catch (...) {
    }
  }

  std::string IdentityNote() const {
    return vm != nullptr && vm->GetRobloxAuthIdentitySnapshot().user_id > 0 ? "; the account identity is unchanged"
                                                                             : "; no account identity";
  }
};

SignInIdentity::SignInIdentity(jnivm::VM& vm, std::shared_ptr<State> state, std::chrono::milliseconds shutdown_wait)
    : vm_(&vm), state_(std::move(state)), shutdown_wait_(shutdown_wait) {}

std::unique_ptr<SignInIdentity> SignInIdentity::Install(jnivm::VM& vm, Options options) {
  if (options.http_client == nullptr) return nullptr;
  auto state = std::make_shared<State>();
  state->vm = &vm;
  state->http = std::move(options.http_client);
  state->log = std::move(options.log);
  state->retry_delays = std::move(options.retry_delays);
  state->store = std::move(options.store);
  std::unique_ptr<SignInIdentity> identity(new SignInIdentity(vm, state, options.shutdown_wait));
  try {
    identity->worker_ = std::thread([state] { Run(state); });
  } catch (const std::system_error&) {
    identity->vm_ = nullptr;
    return nullptr;
  }
  vm.SetRobloxCredentialSink(state,
                             jnivm::RobloxCredentialSinkCallbacks{&SignInIdentity::Store, &SignInIdentity::SignedOut});
  return identity;
}

void SignInIdentity::AcceptRestored(std::string_view credential) {
  if (state_ == nullptr || !IsCanonicalCredential(credential.data(), credential.size())) return;
  std::lock_guard<std::mutex> lock(state_->mutex);
  if (!state_->stopping) AssignSecret(&state_->saved, credential);
}

void SignInIdentity::SignedOut(void* context) {
  auto* state = static_cast<State*>(context);
  if (state == nullptr) return;
  std::shared_ptr<RobloxSessionStore> store;
  {
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->stopping) return;
    ClearSecret(&state->pending);
    state->has_pending = false;
    ClearSecret(&state->last);
    state->last_outcome = Outcome::kPending;
    ClearSecret(&state->saved);
    store = state->store;
    state->Log(store != nullptr ? "sign-in: Roblox signed out; the saved session was removed"
                                : "sign-in: Roblox signed out");
  }
  if (store != nullptr) store->Clear();
  state->wake.notify_all();
}

void SignInIdentity::SaveIfNew(State& state, const std::string& credential, std::unique_lock<std::mutex>& lock) {
  if (state.store == nullptr || credential == state.saved) return;
  const std::shared_ptr<RobloxSessionStore> store = state.store;
  lock.unlock();
  const bool saved = store->Save(credential);
  lock.lock();
  if (state.stopping) return;
  if (saved) {
    AssignSecret(&state.saved, credential);
    state.Log("sign-in: the Roblox session was saved");
  } else {
    state.Log("sign-in: the Roblox session could not be saved; signing in will be needed again next time");
  }
}

SignInIdentity::~SignInIdentity() { Shutdown(); }

bool SignInIdentity::Store(void* context, const char* data, std::size_t size) {
  auto* state = static_cast<State*>(context);
  if (state == nullptr || !IsCanonicalCredential(data, size)) return false;
  const std::string_view credential(data, size);
  std::lock_guard<std::mutex> lock(state->mutex);
  if (state->stopping) return false;
  if (credential == state->last && state->last_outcome != Outcome::kUnavailable) return true;
  AssignSecret(&state->last, credential);
  state->last_outcome = Outcome::kPending;
  AssignSecret(&state->pending, credential);
  state->has_pending = true;
  state->wake.notify_all();
  return true;
}

void SignInIdentity::Run(const std::shared_ptr<State>& state) {
  std::string credential;
  std::unique_lock<std::mutex> lock(state->mutex);
  for (;;) {
    state->wake.wait(lock, [&] { return state->stopping || state->has_pending; });
    if (state->stopping) break;
    AssignSecret(&credential, state->pending);
    ClearSecret(&state->pending);
    state->has_pending = false;
    state->busy = true;
    SaveIfNew(*state, credential, lock);
    if (state->stopping) break;
    state->Log("sign-in: Roblox reported a signed-in session; looking up its account");
    LookUp(*state, credential, lock);
    ClearSecret(&credential);
    state->busy = false;
    state->changed.notify_all();
  }
  ClearSecret(&credential);
  state->busy = false;
  state->exited = true;
  state->changed.notify_all();
}

void SignInIdentity::LookUp(State& state, const std::string& credential, std::unique_lock<std::mutex>& lock) {
  for (std::size_t attempt = 0;; ++attempt) {
    lock.unlock();
    const ms::AuthSession session = ResolveAccount(*state.http, credential);
    lock.lock();
    if (state.stopping || state.has_pending) return;
    switch (session.status) {
      case ms::AuthSessionStatus::kAuthenticated: {
        jnivm::RobloxAuthIdentity identity;
        identity.user_id = session.identity.user_id;
        identity.username = session.identity.username;
        identity.display_name = session.identity.display_name;
        state.vm->SetRobloxAuthIdentity(identity);
        state.last_outcome = Outcome::kResolved;
        state.Log("sign-in: account identity set");
        return;
      }
      case ms::AuthSessionStatus::kInvalid: {
        state.last_outcome = Outcome::kRejected;
        state.Log("sign-in: Roblox did not accept the session" + HttpStatusText(session.http_status) +
                  state.IdentityNote());
        if (state.store != nullptr && credential == state.saved) {
          const std::shared_ptr<RobloxSessionStore> store = state.store;
          ClearSecret(&state.saved);
          lock.unlock();
          store->ClearIfEquals(credential);
          lock.lock();
          if (!state.stopping) state.Log("sign-in: the saved session was removed");
        }
        return;
      }
      case ms::AuthSessionStatus::kGuest:
      case ms::AuthSessionStatus::kUnavailable:
        break;
    }
    const std::string reason = session.error + HttpStatusText(session.http_status);
    if (attempt >= state.retry_delays.size()) {
      state.last_outcome = Outcome::kUnavailable;
      state.Log("sign-in: the account lookup failed (" + reason + ")" + state.IdentityNote() +
                " until Roblox reports the sign-in again");
      return;
    }
    const std::chrono::milliseconds delay = state.retry_delays[attempt];
    state.Log("sign-in: the account lookup failed (" + reason + "); retrying in " + DelayText(delay));
    static_cast<void>(state.wake.wait_for(lock, delay, [&] { return state.stopping || state.has_pending; }));
    if (state.stopping || state.has_pending) return;
  }
}

void SignInIdentity::Shutdown() {
  if (state_ == nullptr) return;
  {
    std::string unsaved;
    std::shared_ptr<RobloxSessionStore> store;
    {
      std::lock_guard<std::mutex> lock(state_->mutex);
      if (!state_->stopping && state_->has_pending && state_->store != nullptr && state_->pending != state_->saved) {
        AssignSecret(&unsaved, state_->pending);
        store = state_->store;
      }
    }
    if (store != nullptr && store->Save(unsaved)) {
      std::lock_guard<std::mutex> lock(state_->mutex);
      if (!state_->stopping) AssignSecret(&state_->saved, unsaved);
    }
    ClearSecret(&unsaved);
  }
  {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->stopping = true;
    state_->vm = nullptr;
    ClearSecret(&state_->pending);
    state_->has_pending = false;
    ClearSecret(&state_->last);
    ClearSecret(&state_->saved);
  }
  state_->wake.notify_all();
  if (vm_ != nullptr) {
    vm_->ClearRobloxCredentialSink();
    vm_->ClearRobloxCredentialProvider();
    vm_ = nullptr;
  }
  if (!worker_.joinable()) return;
  bool exited = false;
  {
    std::unique_lock<std::mutex> lock(state_->mutex);
    exited = state_->changed.wait_for(lock, shutdown_wait_, [&] { return state_->exited; });
  }
  if (exited) {
    worker_.join();
  } else {
    worker_.detach();
  }
}

bool SignInIdentity::WaitUntilIdle(std::chrono::milliseconds timeout) {
  if (state_ == nullptr) return true;
  std::unique_lock<std::mutex> lock(state_->mutex);
  return state_->changed.wait_for(lock, timeout,
                                  [&] { return state_->exited || (!state_->has_pending && !state_->busy); });
}

}
