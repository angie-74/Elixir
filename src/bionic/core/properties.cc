// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/core/properties.h"

#include <windows.h>

#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "abi/sysv_abi.h"
#include "bionic/core/core.h"
#include "bionic/core/core_internal.h"
#include "bionic/runtime.h"

namespace elixir::bionic::properties {
namespace {

struct PropRecord {
  std::string name;
  std::string value;
  alignas(4) std::atomic<std::uint32_t> serial{0};
  std::uint32_t counter = 0;
};

struct Store {
  SRWLOCK lock = SRWLOCK_INIT;
  std::vector<PropRecord*> records;
  std::unordered_map<std::string, PropRecord*> index;
  alignas(4) std::atomic<std::uint32_t> area_serial{0};
};

std::uint32_t MakeSerial(std::uint32_t counter, std::size_t value_length) noexcept {
  const std::uint32_t length = value_length > 0xff ? 0xff : static_cast<std::uint32_t>(value_length);
  return (length << 24) | ((counter << 1) & 0x00fffffeu);
}

bool ValidName(std::string_view name) noexcept {
  if (name.empty()) return false;
  for (char c : name) {
    if (c == '\0' || c == '\n') return false;
  }
  return true;
}

bool ValidValue(std::string_view name, std::string_view value) noexcept {
  if (value.find('\0') != std::string_view::npos) return false;
  if (value.size() < kPropValueMax) return true;
  return name.substr(0, 3) == "ro.";
}

void SetLocked(Store& store, std::string_view name, std::string_view value) {
  const std::string key(name);
  auto found = store.index.find(key);
  PropRecord* record = nullptr;
  if (found == store.index.end()) {
    auto created = std::make_unique<PropRecord>();
    created->name = key;
    record = created.get();
    store.records.push_back(record);
    store.index.emplace(key, record);
    created.release();
  } else {
    record = found->second;
  }
  record->value.assign(value.data(), value.size());
  ++record->counter;
  record->serial.store(MakeSerial(record->counter, value.size()), std::memory_order_release);
  store.area_serial.fetch_add(1, std::memory_order_acq_rel);
  WakeByAddressAll(&record->serial);
  WakeByAddressAll(&store.area_serial);
}

std::string HostLocaleTag() {
  wchar_t buffer[LOCALE_NAME_MAX_LENGTH] = {};
  if (GetUserDefaultLocaleName(buffer, LOCALE_NAME_MAX_LENGTH) <= 0) return "en-US";
  std::string tag;
  for (const wchar_t* p = buffer; *p != 0; ++p) {
    if (*p > 0x7f) return "en-US";
    tag.push_back(static_cast<char>(*p));
  }
  return tag.empty() ? std::string("en-US") : tag;
}

void Seed(Store& store) {
  const int sdk = Options().sdk_version > 0 ? Options().sdk_version : 35;
  const std::string sdk_text = std::to_string(sdk);
  const std::string build_id = "AP4A.250305.002";
  const std::string incremental = "elixir.1";
  const std::string fingerprint =
      "Elixir/elixir_x86_64/elixir_x86_64:15/" + build_id + "/" + incremental + ":user/release-keys";
  const std::string locale = HostLocaleTag();
  const std::pair<const char*, std::string> defaults[] = {
      {"ro.build.version.sdk", sdk_text},
      {"ro.build.version.release", "15"},
      {"ro.build.version.release_or_codename", "15"},
      {"ro.build.version.release_or_preview_display", "15"},
      {"ro.build.version.codename", "REL"},
      {"ro.build.version.all_codenames", "REL"},
      {"ro.build.version.incremental", incremental},
      {"ro.build.version.preview_sdk", "0"},
      {"ro.build.version.preview_sdk_fingerprint", "REL"},
      {"ro.build.version.security_patch", "2025-03-05"},
      {"ro.build.version.min_supported_target_sdk", "24"},
      {"ro.build.id", build_id},
      {"ro.build.display.id", build_id},
      {"ro.build.type", "user"},
      {"ro.build.tags", "release-keys"},
      {"ro.build.user", "elixir"},
      {"ro.build.host", "elixir"},
      {"ro.build.flavor", "elixir_x86_64-user"},
      {"ro.build.product", "elixir_x86_64"},
      {"ro.build.description", "elixir_x86_64-user 15 " + build_id + " " + incremental + " release-keys"},
      {"ro.build.fingerprint", fingerprint},
      {"ro.build.date", "Wed Mar  5 00:00:00 UTC 2025"},
      {"ro.build.date.utc", "1741132800"},
      {"ro.build.characteristics", "default"},
      {"ro.product.brand", "Elixir"},
      {"ro.product.manufacturer", "Elixir"},
      {"ro.product.model", "Elixir x86_64"},
      {"ro.product.name", "elixir_x86_64"},
      {"ro.product.device", "elixir_x86_64"},
      {"ro.product.board", "elixir"},
      {"ro.product.cpu.abi", "x86_64"},
      {"ro.product.cpu.abilist", "x86_64"},
      {"ro.product.cpu.abilist64", "x86_64"},
      {"ro.product.cpu.abilist32", ""},
      {"ro.product.cpu.pagesize.max", "4096"},
      {"ro.system.product.cpu.abilist", "x86_64"},
      {"ro.system.product.cpu.abilist64", "x86_64"},
      {"ro.system.product.cpu.abilist32", ""},
      {"ro.vendor.product.cpu.abilist", "x86_64"},
      {"ro.vendor.product.cpu.abilist64", "x86_64"},
      {"ro.vendor.product.cpu.abilist32", ""},
      {"ro.product.first_api_level", sdk_text},
      {"ro.board.first_api_level", sdk_text},
      {"ro.board.api_level", sdk_text},
      {"ro.vendor.api_level", sdk_text},
      {"ro.vndk.version", sdk_text},
      {"ro.treble.enabled", "true"},
      {"ro.product.locale", locale},
      {"ro.hardware", "elixir"},
      {"ro.boot.hardware", "elixir"},
      {"ro.board.platform", "elixir"},
      {"ro.arch", "x86_64"},
      {"ro.debuggable", "0"},
      {"ro.secure", "1"},
      {"ro.adb.secure", "1"},
      {"ro.zygote", "zygote64"},
      {"ro.dalvik.vm.native.bridge", "0"},
      {"dalvik.vm.isa.x86_64.variant", "x86_64"},
      {"dalvik.vm.isa.x86_64.features", "default"},
      {"ro.opengles.version", "196610"},
      {"ro.config.low_ram", "false"},
      {"ro.crypto.state", "encrypted"},
      {"ro.revision", "0"},
      {"ro.carrier", "unknown"},
      {"net.bt.name", "Android"},
      {"sys.boot_completed", "1"},
      {"dev.bootcomplete", "1"},
      {"persist.sys.locale", locale},
      {"persist.sys.timezone", core::HostTimeZoneIanaName()},
  };
  for (const auto& [name, value] : defaults) SetLocked(store, name, value);
  static const char* const kPartitions[] = {"system", "system_ext", "vendor", "product", "odm"};
  for (const char* partition : kPartitions) {
    const std::string prefix = std::string("ro.product.") + partition + ".";
    SetLocked(store, prefix + "brand", "Elixir");
    SetLocked(store, prefix + "manufacturer", "Elixir");
    SetLocked(store, prefix + "model", "Elixir x86_64");
    SetLocked(store, prefix + "name", "elixir_x86_64");
    SetLocked(store, prefix + "device", "elixir_x86_64");
  }
}

Store& GetStore() {
  static Store* store = [] {
    auto* created = new Store();
    AcquireSRWLockExclusive(&created->lock);
    try {
      Seed(*created);
    } catch (...) {
    }
    ReleaseSRWLockExclusive(&created->lock);
    return created;
  }();
  return *store;
}

class SharedLock {
 public:
  explicit SharedLock(Store& store) noexcept : store_(store) { AcquireSRWLockShared(&store_.lock); }
  ~SharedLock() { ReleaseSRWLockShared(&store_.lock); }
  SharedLock(const SharedLock&) = delete;
  SharedLock& operator=(const SharedLock&) = delete;

 private:
  Store& store_;
};

PropRecord* Find(const char* name) noexcept {
  if (name == nullptr) return nullptr;
  try {
    Store& store = GetStore();
    SharedLock lock(store);
    auto found = store.index.find(name);
    return found == store.index.end() ? nullptr : found->second;
  } catch (...) {
    return nullptr;
  }
}

int CopyValue(const PropRecord* record, char* value) noexcept {
  Store& store = GetStore();
  SharedLock lock(store);
  std::size_t length = record->value.size();
  if (length > kPropValueMax - 1) length = kPropValueMax - 1;
  std::memcpy(value, record->value.data(), length);
  value[length] = '\0';
  return static_cast<int>(length);
}

bool WaitForChange(std::atomic<std::uint32_t>* word, std::uint32_t old, const guest::Timespec* timeout) noexcept {
  const std::int64_t timeout_ns = timeout != nullptr ? core::TimespecToNanoseconds(*timeout) : -1;
  const std::int64_t deadline = timeout_ns >= 0 ? core::MonotonicNanoseconds() + timeout_ns : INT64_MAX;
  for (;;) {
    if (word->load(std::memory_order_acquire) != old) return true;
    DWORD wait_ms = INFINITE;
    if (timeout_ns >= 0) {
      const std::int64_t remaining = deadline - core::MonotonicNanoseconds();
      if (remaining <= 0) return false;
      const std::int64_t ms = (remaining + 999999) / 1000000;
      wait_ms = ms >= 0x7fffffff ? 0x7fffffff : static_cast<DWORD>(ms);
    }
    std::uint32_t expected = old;
    WaitOnAddress(word, &expected, sizeof(expected), wait_ms);
  }
}

}

bool SetProperty(std::string_view name, std::string_view value) {
  if (!ValidName(name) || !ValidValue(name, value)) return false;
  Store& store = GetStore();
  AcquireSRWLockExclusive(&store.lock);
  try {
    SetLocked(store, name, value);
  } catch (...) {
    ReleaseSRWLockExclusive(&store.lock);
    return false;
  }
  ReleaseSRWLockExclusive(&store.lock);
  return true;
}

void SetProperties(const std::vector<std::pair<std::string, std::string>>& values) {
  for (const auto& [name, value] : values) SetProperty(name, value);
}

std::optional<std::string> GetProperty(std::string_view name) {
  Store& store = GetStore();
  SharedLock lock(store);
  auto found = store.index.find(std::string(name));
  if (found == store.index.end()) return std::nullopt;
  return found->second->value;
}

std::vector<std::pair<std::string, std::string>> AllProperties() {
  Store& store = GetStore();
  SharedLock lock(store);
  std::vector<std::pair<std::string, std::string>> out;
  out.reserve(store.records.size());
  for (const PropRecord* record : store.records) out.emplace_back(record->name, record->value);
  return out;
}

}

namespace elixir::bionic {
namespace {
using properties::PropRecord;
using PropertyCallback = abi::SysvFn<void(void*, const char*, const char*, std::uint32_t)>;
using ForeachCallback = abi::SysvFn<void(const void*, void*)>;
}

ELIXIR_BIONIC_FN(int, __system_property_get)(const char* name, char* value) noexcept {
  if (value == nullptr) return 0;
  const PropRecord* record = properties::Find(name);
  if (record == nullptr) {
    value[0] = '\0';
    return 0;
  }
  return properties::CopyValue(record, value);
}

ELIXIR_BIONIC_FN(const void*, __system_property_find)(const char* name) noexcept { return properties::Find(name); }

ELIXIR_BIONIC_FN(const void*, __system_property_find_nth)(unsigned index) noexcept {
  try {
    properties::Store& store = properties::GetStore();
    properties::SharedLock lock(store);
    return index < store.records.size() ? store.records[index] : nullptr;
  } catch (...) {
    return nullptr;
  }
}

ELIXIR_BIONIC_FN(int, __system_property_read)(const void* info, char* name, char* value) noexcept {
  const auto* record = static_cast<const PropRecord*>(info);
  if (record == nullptr) return 0;
  if (name != nullptr) {
    properties::Store& store = properties::GetStore();
    properties::SharedLock lock(store);
    std::size_t length = record->name.size();
    if (length > properties::kPropNameMax - 1) length = properties::kPropNameMax - 1;
    std::memcpy(name, record->name.data(), length);
    name[length] = '\0';
  }
  if (value == nullptr) return 0;
  return properties::CopyValue(record, value);
}

ELIXIR_BIONIC_FN(void, __system_property_read_callback)(const void* info, PropertyCallback callback,
                                                         void* cookie) noexcept {
  const auto* record = static_cast<const PropRecord*>(info);
  if (record == nullptr || callback == nullptr) return;
  std::string name;
  std::string value;
  std::uint32_t serial = 0;
  try {
    properties::Store& store = properties::GetStore();
    properties::SharedLock lock(store);
    name = record->name;
    value = record->value;
    serial = record->serial.load(std::memory_order_acquire);
  } catch (...) {
    return;
  }
  callback(cookie, name.c_str(), value.c_str(), serial);
}

ELIXIR_BIONIC_FN(int, __system_property_foreach)(ForeachCallback callback, void* cookie) noexcept {
  if (callback == nullptr) return -1;
  std::vector<const PropRecord*> snapshot;
  try {
    properties::Store& store = properties::GetStore();
    properties::SharedLock lock(store);
    snapshot.assign(store.records.begin(), store.records.end());
  } catch (...) {
    return -1;
  }
  for (const PropRecord* record : snapshot) callback(record, cookie);
  return 0;
}

ELIXIR_BIONIC_FN(std::uint32_t, __system_property_serial)(const void* info) noexcept {
  const auto* record = static_cast<const PropRecord*>(info);
  return record != nullptr ? record->serial.load(std::memory_order_acquire) : 0;
}

ELIXIR_BIONIC_FN(std::uint32_t, __system_property_area_serial)() noexcept {
  return properties::GetStore().area_serial.load(std::memory_order_acquire);
}

ELIXIR_BIONIC_FN(int, __system_property_set)(const char* name, const char* value) noexcept {
  (void)name;
  (void)value;
  return -1;
}

ELIXIR_BIONIC_FN(bool, __system_property_wait)(const void* info, std::uint32_t old_serial,
                                              std::uint32_t* new_serial, const guest::Timespec* timeout) noexcept {
  if (timeout != nullptr && !core::ValidTimespec(*timeout)) return false;
  properties::Store& store = properties::GetStore();
  auto* record = const_cast<PropRecord*>(static_cast<const PropRecord*>(info));
  std::atomic<std::uint32_t>* word = record != nullptr ? &record->serial : &store.area_serial;
  if (!properties::WaitForChange(word, old_serial, timeout)) return false;
  if (new_serial != nullptr) *new_serial = word->load(std::memory_order_acquire);
  return true;
}

ELIXIR_BIONIC_FN(std::uint32_t, __system_property_wait_any)(std::uint32_t old_serial) noexcept {
  properties::Store& store = properties::GetStore();
  properties::WaitForChange(&store.area_serial, old_serial, nullptr);
  return store.area_serial.load(std::memory_order_acquire);
}

ELIXIR_BIONIC_FN(int, android_get_device_api_level)() noexcept {
  char value[properties::kPropValueMax];
  if (elixir_bionic___system_property_get("ro.build.version.sdk", value) <= 0) return -1;
  return std::atoi(value) > 0 ? std::atoi(value) : -1;
}

namespace core {

void RegisterPropertyExports(ExportTable& table) {
#define ELIXIR_ADD(name) table.Add(kLibc, #name, reinterpret_cast<void*>(&elixir_bionic_##name))
  ELIXIR_ADD(__system_property_get);
  ELIXIR_ADD(__system_property_find);
  ELIXIR_ADD(__system_property_find_nth);
  ELIXIR_ADD(__system_property_read);
  ELIXIR_ADD(__system_property_read_callback);
  ELIXIR_ADD(__system_property_foreach);
  ELIXIR_ADD(__system_property_serial);
  ELIXIR_ADD(__system_property_area_serial);
  ELIXIR_ADD(__system_property_set);
  ELIXIR_ADD(__system_property_wait);
  ELIXIR_ADD(__system_property_wait_any);
  ELIXIR_ADD(android_get_device_api_level);
#undef ELIXIR_ADD
}

}
}
