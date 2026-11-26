// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
//
// Adapted from AOSP bionic libc/bionic/pthread_key.cpp and Mocktail
// src/compat/bionic_pthread_key_runtime.cc (Apache-2.0):
//
//   Copyright (C) 2008 The Android Open Source Project
//   All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in
//      the documentation and/or other materials provided with the
//      distribution.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
//   COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//   BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
//   OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
//   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
//   OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
//   SUCH DAMAGE.
#include <cstdint>

#include "bionic/threads/thread_internal.h"

namespace elixir::bionic::threads::internal {
namespace {

using KeyDestructor = abi::SysvFn<void(void*)>;

struct KeyMapEntry {
  std::uintptr_t seq;
  std::uintptr_t destructor;
};

KeyMapEntry g_key_map[kKeyCount] = {{1, 0}, {1, 0}};

struct KeyData {
  std::uintptr_t seq;
  void* data;
};
thread_local KeyData t_key_data[kKeyCount];

inline bool SeqOfKeyInUse(std::uintptr_t seq) noexcept { return (seq & 1) != 0; }

inline bool DecodeKey(guest::PthreadKey key, std::uint32_t* index) noexcept {
  const std::uint32_t candidate = static_cast<std::uint32_t>(key) ^ kKeyValidFlag;
  if (candidate >= kKeyCount) return false;
  *index = candidate;
  return true;
}

}

void RunKeyDestructors() noexcept {
  KeyData* key_data = t_key_data;
  for (int rounds = guest::kPTHREAD_DESTRUCTOR_ITERATIONS; rounds > 0; --rounds) {
    std::size_t called_destructor_count = 0;
    for (std::size_t i = 0; i < kKeyCount; ++i) {
      const std::uintptr_t seq = __atomic_load_n(&g_key_map[i].seq, __ATOMIC_RELAXED);
      if (SeqOfKeyInUse(seq) && seq == key_data[i].seq) {
        if (key_data[i].data == nullptr) continue;
        const auto destructor =
            reinterpret_cast<KeyDestructor>(__atomic_load_n(&g_key_map[i].destructor, __ATOMIC_RELAXED));
        if (destructor == nullptr) continue;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (__atomic_load_n(&g_key_map[i].seq, __ATOMIC_RELAXED) != seq) continue;
        void* data = key_data[i].data;
        key_data[i].data = nullptr;
        destructor(data);
        ++called_destructor_count;
      }
    }
    if (called_destructor_count == 0) break;
  }
}

bool HasKeyValues() noexcept {
  for (std::size_t i = 0; i < kKeyCount; ++i) {
    if (t_key_data[i].data != nullptr) return true;
  }
  return false;
}

}

namespace {
namespace internal = elixir::bionic::threads::internal;
namespace guest = elixir::bionic::guest;
using internal::kKeyCount;
}

ELIXIR_BIONIC_FN(int, pthread_key_create)(guest::PthreadKey* key,
                                          elixir::abi::SysvFn<void(void*)> key_destructor) noexcept {
  for (std::size_t i = internal::kKeyReservedCount; i < kKeyCount; ++i) {
    std::uintptr_t seq = __atomic_load_n(&internal::g_key_map[i].seq, __ATOMIC_RELAXED);
    while (!internal::SeqOfKeyInUse(seq)) {
      if (__atomic_compare_exchange_n(&internal::g_key_map[i].seq, &seq, seq + 1, true, __ATOMIC_SEQ_CST,
                                      __ATOMIC_RELAXED)) {
        __atomic_store_n(&internal::g_key_map[i].destructor, reinterpret_cast<std::uintptr_t>(key_destructor),
                         __ATOMIC_SEQ_CST);
        *key = static_cast<guest::PthreadKey>(static_cast<std::uint32_t>(i) | internal::kKeyValidFlag);
        return 0;
      }
    }
  }
  return guest::kEAGAIN;
}

ELIXIR_BIONIC_FN(int, pthread_key_delete)(guest::PthreadKey key) noexcept {
  std::uint32_t index = 0;
  if (!internal::DecodeKey(key, &index) || index < internal::kKeyReservedCount) return guest::kEINVAL;
  std::uintptr_t seq = __atomic_load_n(&internal::g_key_map[index].seq, __ATOMIC_RELAXED);
  if (internal::SeqOfKeyInUse(seq)) {
    if (__atomic_compare_exchange_n(&internal::g_key_map[index].seq, &seq, seq + 1, false, __ATOMIC_SEQ_CST,
                                    __ATOMIC_RELAXED)) {
      return 0;
    }
  }
  return guest::kEINVAL;
}

ELIXIR_BIONIC_FN(void*, pthread_getspecific)(guest::PthreadKey key) noexcept {
  std::uint32_t index = 0;
  if (__builtin_expect(!internal::DecodeKey(key, &index), 0)) return nullptr;
  const std::uintptr_t seq = __atomic_load_n(&internal::g_key_map[index].seq, __ATOMIC_RELAXED);
  internal::KeyData* data = &internal::t_key_data[index];
  if (__builtin_expect(internal::SeqOfKeyInUse(seq) && data->seq == seq, 1)) return data->data;
  data->data = nullptr;
  return nullptr;
}

ELIXIR_BIONIC_FN(int, pthread_setspecific)(guest::PthreadKey key, const void* value) noexcept {
  std::uint32_t index = 0;
  if (__builtin_expect(!internal::DecodeKey(key, &index), 0)) return guest::kEINVAL;
  const std::uintptr_t seq = __atomic_load_n(&internal::g_key_map[index].seq, __ATOMIC_RELAXED);
  if (__builtin_expect(!internal::SeqOfKeyInUse(seq), 0)) return guest::kEINVAL;
  if (value != nullptr && internal::CurrentThreadIfAny() == nullptr) internal::Self();
  internal::KeyData* data = &internal::t_key_data[index];
  data->seq = seq;
  data->data = const_cast<void*>(value);
  return 0;
}

namespace elixir::bionic::threads::internal {

void RegisterKeyExports(ExportTable& table) {
  table.Add(kLibc, "pthread_key_create", reinterpret_cast<void*>(&elixir_bionic_pthread_key_create));
  table.Add(kLibc, "pthread_key_delete", reinterpret_cast<void*>(&elixir_bionic_pthread_key_delete));
  table.Add(kLibc, "pthread_getspecific", reinterpret_cast<void*>(&elixir_bionic_pthread_getspecific));
  table.Add(kLibc, "pthread_setspecific", reinterpret_cast<void*>(&elixir_bionic_pthread_setspecific));
}

}
