// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_SIGN_IN_IDENTITY_H_
#define ELIXIR_RUNTIME_SIGN_IN_IDENTITY_H_

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace jnivm {
class VM;
}

namespace mocktail::services {
class HttpClient;
}

namespace elixir::runtime {

class AccountBook;
class RobloxSessionStore;

class SignInIdentity final {
 public:
  struct Options {
    std::shared_ptr<mocktail::services::HttpClient> http_client;
    std::function<void(std::string_view line)> log;
    std::vector<std::chrono::milliseconds> retry_delays{std::chrono::seconds(2), std::chrono::seconds(10),
                                                        std::chrono::seconds(30)};
    std::chrono::milliseconds shutdown_wait{2000};
    std::shared_ptr<RobloxSessionStore> store;
    std::shared_ptr<AccountBook> accounts;
    std::function<long long()> now_seconds;
    std::function<void(long long user_id)> account_changed;
  };

  static std::unique_ptr<SignInIdentity> Install(jnivm::VM& vm, Options options);

  ~SignInIdentity();
  SignInIdentity(const SignInIdentity&) = delete;
  SignInIdentity& operator=(const SignInIdentity&) = delete;

  void Shutdown();

  void AcceptRestored(std::string_view credential);

  bool WaitUntilIdle(std::chrono::milliseconds timeout);

 private:
  struct State;

  SignInIdentity(jnivm::VM& vm, std::shared_ptr<State> state, std::chrono::milliseconds shutdown_wait);

  static bool Store(void* context, const char* data, std::size_t size);
  static void SignedOut(void* context);
  static void SaveIfNew(State& state, const std::string& credential, std::unique_lock<std::mutex>& lock);
  static void Run(const std::shared_ptr<State>& state);
  static void LookUp(State& state, const std::string& credential, std::unique_lock<std::mutex>& lock);

  jnivm::VM* vm_;
  std::shared_ptr<State> state_;
  std::thread worker_;
  std::chrono::milliseconds shutdown_wait_;
};

}

#endif
