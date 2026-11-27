// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/kept_cookies.h"

#include <utility>

#include "elixir_runtime/roblox_session_store.h"
#include "jnivm/jnivm.h"

#include <windows.h>

namespace elixir::runtime {
namespace {

void Wipe(std::string* value) {
  if (!value->empty()) SecureZeroMemory(value->data(), value->size());
  value->clear();
}

std::string CountText(std::string_view cookies) {
  const std::size_t count = jnivm::CountRobloxKeptCookies(cookies);
  return std::to_string(count) + (count == 1 ? " Roblox cookie" : " Roblox cookies");
}

}

KeptCookies::KeptCookies(Options options) : store_(std::move(options.store)), log_(std::move(options.log)) {}

KeptCookies::~KeptCookies() { Detach(); }

std::shared_ptr<KeptCookies> KeptCookies::Install(jnivm::VM& vm, Options options) {
  if (options.store == nullptr) return nullptr;
  std::shared_ptr<KeptCookies> kept(new KeptCookies(std::move(options)));
  kept->vm_ = &vm;
  std::string loaded;
  switch (kept->store_->Load(&loaded)) {
    case RobloxSessionStore::LoadResult::kFound:
      vm.SetRobloxKeptCookies(loaded);
      kept->saved_ = loaded;
      kept->Log("cookies: " + CountText(loaded) + " from the last session will be given back to Roblox");
      break;
    case RobloxSessionStore::LoadResult::kMissing:
      break;
    case RobloxSessionStore::LoadResult::kUnreadable:
      kept->Log("cookies: the saved Roblox cookies could not be read; Roblox starts without them");
      break;
  }
  Wipe(&loaded);
  vm.SetRobloxKeptCookieSink(kept, jnivm::RobloxKeptCookieCallbacks{&KeptCookies::Store});
  return kept;
}

void KeptCookies::Store(void* context, const char* data, std::size_t size) {
  auto* kept = static_cast<KeptCookies*>(context);
  if (kept == nullptr) return;
  if (data == nullptr || size == 0) {
    std::lock_guard<std::mutex> lock(kept->mutex_);
    if (kept->detached_ || kept->saved_.empty()) return;
    kept->store_->Clear();
    Wipe(&kept->saved_);
    kept->Log("cookies: Roblox removed its cookies; the saved ones were removed");
    return;
  }
  const std::string_view cookies(data, size);
  if (!RobloxSessionStore::IsCookieList(cookies)) return;
  std::lock_guard<std::mutex> lock(kept->mutex_);
  if (kept->detached_ || cookies == kept->saved_) return;
  if (kept->store_->Save(cookies)) {
    Wipe(&kept->saved_);
    kept->saved_.assign(cookies);
    kept->Log("cookies: " + CountText(cookies) + " saved for the next start");
  } else {
    kept->Log("cookies: Roblox's cookies could not be saved");
  }
}

void KeptCookies::Detach() {
  jnivm::VM* vm = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (detached_) return;
    detached_ = true;
    vm = vm_;
    vm_ = nullptr;
    Wipe(&saved_);
  }
  if (vm != nullptr) vm->ClearRobloxKeptCookieSink();
}

void KeptCookies::Log(const std::string& line) {
  if (!log_) return;
  try {
    log_(line);
  } catch (...) {
  }
}

}
