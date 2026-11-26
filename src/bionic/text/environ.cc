// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// getenv/setenv/putenv/unsetenv are adapted from OpenBSD libc/stdlib/getenv.c
// and setenv.c as shipped in AOSP bionic
// (references/platform_bionic/libc/upstream-openbsd/lib/libc/stdlib/):
//
//   Copyright (c) 1987, 1993 The Regents of the University of California.
//   All rights reserved.
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//   1. Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//   2. Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//   3. Neither the name of the University nor the names of its contributors
//      may be used to endorse or promote products derived from this software
//      without specific prior written permission.
//   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
//   ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
//
// clearenv() follows libc/bionic/clearenv.cpp (BSD-2-Clause, Copyright (C)
// 2013 The Android Open Source Project); setprogname() follows OpenBSD
// libc/gen/setprogname.c (Copyright (c) 2013 Antoine Jacoutot, ISC).
#include "bionic/text/environment.h"

#include <windows.h>

#include <atomic>
#include <cstring>

#include "bionic/guest_constants.h"
#include "bionic/memory.h"
#include "bionic/runtime.h"
#include "bionic/text/text_internal.h"

namespace elixir::bionic::text {
namespace {

char** g_environ = nullptr;
const char* g_progname = nullptr;

char** g_lastenv = nullptr;
SRWLOCK g_environment_lock = SRWLOCK_INIT;
std::atomic<bool> g_environment_ready{false};
char* g_progname_storage = nullptr;

class ExclusiveLock {
 public:
  ExclusiveLock() noexcept { AcquireSRWLockExclusive(&g_environment_lock); }
  ~ExclusiveLock() { ReleaseSRWLockExclusive(&g_environment_lock); }
  ExclusiveLock(const ExclusiveLock&) = delete;
  ExclusiveLock& operator=(const ExclusiveLock&) = delete;
};

class SharedLock {
 public:
  SharedLock() noexcept { AcquireSRWLockShared(&g_environment_lock); }
  ~SharedLock() { ReleaseSRWLockShared(&g_environment_lock); }
  SharedLock(const SharedLock&) = delete;
  SharedLock& operator=(const SharedLock&) = delete;
};

constexpr const char* kDefaultEnvironment[] = {
    ("PATH=/product/bin:/apex/com.android.runtime/bin:/apex/com.android.art/bin:/system_ext/bin:/system/bin:"
     "/system/xbin:/odm/bin:/vendor/bin:/vendor/xbin"),
    "ANDROID_BOOTLOGO=1",
    "ANDROID_ROOT=/system",
    "ANDROID_ASSETS=/system/app",
    "ANDROID_DATA=/data",
    "ANDROID_STORAGE=/storage",
    "ANDROID_ART_ROOT=/apex/com.android.art",
    "ANDROID_I18N_ROOT=/apex/com.android.i18n",
    "ANDROID_TZDATA_ROOT=/apex/com.android.tzdata",
    "EXTERNAL_STORAGE=/sdcard",
    "ASEC_MOUNTPOINT=/mnt/asec",
    "DOWNLOAD_CACHE=/data/cache",
};

char** BuildEnvironment(const char* const* entries, std::size_t count) noexcept {
  auto** array = static_cast<char**>(GuestCalloc(count + 1, sizeof(char*)));
  if (array == nullptr) return nullptr;
  for (std::size_t i = 0; i < count; ++i) {
    array[i] = GuestStrdup(entries[i]);
    if (array[i] == nullptr) {
      for (std::size_t j = 0; j < i; ++j) GuestFree(array[j]);
      GuestFree(array);
      return nullptr;
    }
  }
  return array;
}

void InstallProgramName(const char* name) noexcept {
  const char* slash = std::strrchr(name, '/');
  char* copy = GuestStrdup(slash != nullptr ? slash + 1 : name);
  if (copy == nullptr) return;
  g_progname_storage = copy;
  g_progname = copy;
}

void EnsureEnvironment() noexcept {
  if (g_environment_ready.load(std::memory_order_acquire)) return;
  ExclusiveLock lock;
  if (g_environment_ready.load(std::memory_order_relaxed)) return;
  if (g_environ == nullptr) {
    g_environ = BuildEnvironment(kDefaultEnvironment, sizeof(kDefaultEnvironment) / sizeof(kDefaultEnvironment[0]));
    g_lastenv = g_environ;
  }
  if (g_progname == nullptr) {
    const std::string& package = Options().package_name;
    InstallProgramName(package.empty() ? "<unknown>" : package.c_str());
  }
  g_environment_ready.store(true, std::memory_order_release);
}

char* FindEnv(const char* name, std::size_t length, std::size_t* offset) noexcept {
  if (name == nullptr || g_environ == nullptr) return nullptr;
  for (char** p = g_environ + *offset; *p != nullptr; ++p) {
    const char* cp = *p;
    if (std::strncmp(cp, name, length) == 0 && cp[length] == '=') {
      *offset = static_cast<std::size_t>(p - g_environ);
      return const_cast<char*>(cp + length + 1);
    }
  }
  return nullptr;
}

void RemoveDuplicates(const char* name, std::size_t length, std::size_t offset) noexcept {
  while (FindEnv(name, length, &offset) != nullptr) {
    for (char** p = &g_environ[offset];; ++p) {
      if ((*p = *(p + 1)) == nullptr) break;
    }
  }
}

std::ptrdiff_t AppendSlot() noexcept {
  std::size_t count = 0;
  if (g_environ != nullptr) {
    while (g_environ[count] != nullptr) ++count;
  }
  auto** grown = static_cast<char**>(GuestRealloc(g_lastenv, (count + 2) * sizeof(char*)));
  if (grown == nullptr) {
    SetGuestErrno(guest::kENOMEM);
    return -1;
  }
  if (g_lastenv != g_environ && g_environ != nullptr) std::memcpy(grown, g_environ, count * sizeof(char*));
  g_lastenv = g_environ = grown;
  g_environ[count] = nullptr;
  g_environ[count + 1] = nullptr;
  return static_cast<std::ptrdiff_t>(count);
}

std::size_t NameLength(const char* name) noexcept {
  const char* np = name;
  while (*np != '\0' && *np != '=') ++np;
  return static_cast<std::size_t>(np - name);
}

}

void SetGuestEnvironment(const std::vector<std::string>& entries) {
  std::vector<const char*> pointers;
  pointers.reserve(entries.size());
  for (const std::string& entry : entries) pointers.push_back(entry.c_str());
  ExclusiveLock lock;
  char** array = BuildEnvironment(pointers.data(), pointers.size());
  if (array == nullptr) return;
  g_environ = array;
  g_lastenv = array;
  if (g_progname == nullptr) {
    const std::string& package = Options().package_name;
    InstallProgramName(package.empty() ? "<unknown>" : package.c_str());
  }
  g_environment_ready.store(true, std::memory_order_release);
}

char*** GuestEnvironLocation() noexcept {
  EnsureEnvironment();
  return &g_environ;
}

void SetGuestProgramName(const char* name) {
  EnsureEnvironment();
  ExclusiveLock lock;
  InstallProgramName(name != nullptr ? name : "<unknown>");
}

}

namespace env = elixir::bionic::text;
using elixir::bionic::SetGuestErrno;
using elixir::bionic::text_internal::g_size;

ELIXIR_BIONIC_FN(char*, getenv)(const char* name) noexcept {
  env::EnsureEnvironment();
  if (name == nullptr) return nullptr;
  env::SharedLock lock;
  std::size_t offset = 0;
  return env::FindEnv(name, env::NameLength(name), &offset);
}

ELIXIR_BIONIC_FN(char*, secure_getenv)(const char* name) noexcept { return elixir_bionic_getenv(name); }

ELIXIR_BIONIC_FN(int, setenv)(const char* name, const char* value, int rewrite) noexcept {
  env::EnsureEnvironment();
  if (name == nullptr || *name == '\0' || value == nullptr) {
    SetGuestErrno(elixir::bionic::guest::kEINVAL);
    return -1;
  }
  const std::size_t name_length = env::NameLength(name);
  if (name[name_length] != '\0') {
    SetGuestErrno(elixir::bionic::guest::kEINVAL);
    return -1;
  }
  env::ExclusiveLock lock;
  std::size_t offset = 0;
  if (env::FindEnv(name, name_length, &offset) != nullptr) {
    if (rewrite == 0) return 0;
    env::RemoveDuplicates(name, name_length, offset + 1);
  } else {
    const std::ptrdiff_t slot = env::AppendSlot();
    if (slot < 0) return -1;
    offset = static_cast<std::size_t>(slot);
  }
  const std::size_t value_length = std::strlen(value);
  auto* entry = static_cast<char*>(elixir::bionic::GuestMalloc(name_length + value_length + 2));
  if (entry == nullptr) {
    SetGuestErrno(elixir::bionic::guest::kENOMEM);
    return -1;
  }
  std::memcpy(entry, name, name_length);
  entry[name_length] = '=';
  std::memcpy(entry + name_length + 1, value, value_length + 1);
  env::g_environ[offset] = entry;
  return 0;
}

ELIXIR_BIONIC_FN(int, putenv)(char* string) noexcept {
  env::EnsureEnvironment();
  const std::size_t name_length = string != nullptr ? env::NameLength(string) : 0;
  if (string == nullptr || name_length == 0 || string[name_length] != '=') {
    SetGuestErrno(elixir::bionic::guest::kEINVAL);
    return -1;
  }
  env::ExclusiveLock lock;
  std::size_t offset = 0;
  if (env::FindEnv(string, name_length, &offset) != nullptr) {
    env::g_environ[offset] = string;
    env::RemoveDuplicates(string, name_length, offset + 1);
    return 0;
  }
  const std::ptrdiff_t slot = env::AppendSlot();
  if (slot < 0) return -1;
  env::g_environ[slot] = string;
  return 0;
}

ELIXIR_BIONIC_FN(int, unsetenv)(const char* name) noexcept {
  env::EnsureEnvironment();
  if (name == nullptr || *name == '\0') {
    SetGuestErrno(elixir::bionic::guest::kEINVAL);
    return -1;
  }
  const std::size_t name_length = env::NameLength(name);
  if (name[name_length] != '\0') {
    SetGuestErrno(elixir::bionic::guest::kEINVAL);
    return -1;
  }
  env::ExclusiveLock lock;
  env::RemoveDuplicates(name, name_length, 0);
  return 0;
}

ELIXIR_BIONIC_FN(int, clearenv)() noexcept {
  env::EnsureEnvironment();
  env::ExclusiveLock lock;
  if (char** e = env::g_environ) {
    for (; *e != nullptr; ++e) *e = nullptr;
  }
  return 0;
}

ELIXIR_BIONIC_FN(const char*, getprogname)() noexcept {
  env::EnsureEnvironment();
  return env::g_progname;
}

ELIXIR_BIONIC_FN(void, setprogname)(const char* name) noexcept {
  env::EnsureEnvironment();
  const char* slash = std::strrchr(name, '/');
  env::g_progname = slash != nullptr ? slash + 1 : name;
}

namespace elixir::bionic::text_internal {

void RegisterEnvironmentExports(ExportTable& table) {
  env::EnsureEnvironment();
  table.Add(kLibc, "environ", reinterpret_cast<void*>(&env::g_environ));
  table.Add(kLibc, "__progname", reinterpret_cast<void*>(&env::g_progname));
  ELIXIR_TEXT_EXPORT(table, kLibc, getenv);
  ELIXIR_TEXT_EXPORT(table, kLibc, secure_getenv);
  ELIXIR_TEXT_EXPORT(table, kLibc, setenv);
  ELIXIR_TEXT_EXPORT(table, kLibc, putenv);
  ELIXIR_TEXT_EXPORT(table, kLibc, unsetenv);
  ELIXIR_TEXT_EXPORT(table, kLibc, clearenv);
  ELIXIR_TEXT_EXPORT(table, kLibc, getprogname);
  ELIXIR_TEXT_EXPORT(table, kLibc, setprogname);
}

}
