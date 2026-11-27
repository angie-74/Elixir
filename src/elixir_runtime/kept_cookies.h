// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_KEPT_COOKIES_H_
#define ELIXIR_RUNTIME_KEPT_COOKIES_H_

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace jnivm {
class VM;
}

namespace elixir::runtime {

class RobloxSessionStore;

class KeptCookies final {
 public:
  struct Options {
    std::shared_ptr<RobloxSessionStore> store;
    std::function<void(std::string_view line)> log;
  };

  static std::shared_ptr<KeptCookies> Install(jnivm::VM& vm, Options options);

  ~KeptCookies();
  KeptCookies(const KeptCookies&) = delete;
  KeptCookies& operator=(const KeptCookies&) = delete;

  void Detach();

 private:
  explicit KeptCookies(Options options);

  static void Store(void* context, const char* data, std::size_t size);
  void Log(const std::string& line);

  std::mutex mutex_;
  jnivm::VM* vm_ = nullptr;
  std::shared_ptr<RobloxSessionStore> store_;
  std::function<void(std::string_view)> log_;
  std::string saved_;
  bool detached_ = false;
};

}

#endif
