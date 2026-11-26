/* Copyright 2026 Elixir Project Authors
 * Licensed under the Apache License, Version 2.0 */
#ifndef ELIXIR_CORE_API_H_
#define ELIXIR_CORE_API_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ELIXIR_CORE_ABI_VERSION 1u

#define ELIXIR_CORE_API

#define ELIXIR_EXIT_CORE_MISSING 11u
#define ELIXIR_EXIT_CORE_ABI 12u

typedef enum ElixirCoreEventKind {
  ELIXIR_CORE_EVENT_STAGE = 1,
  ELIXIR_CORE_EVENT_ERROR = 2,
  ELIXIR_CORE_EVENT_INFO = 3
} ElixirCoreEventKind;

typedef void (*ElixirCoreReportFn)(void* context, uint32_t kind, const char* json);

#define ELIXIR_LAUNCH_ACCEPTED 0
#define ELIXIR_LAUNCH_INVALID 1
#define ELIXIR_LAUNCH_BUSY 2
#define ELIXIR_LAUNCH_STOPPING 3
#define ELIXIR_LAUNCH_UNAVAILABLE 4
#define ELIXIR_LAUNCH_MAX_BYTES (64u * 1024u)

typedef struct ElixirCoreHostInfo {
  uint32_t struct_size;
  uint32_t abi_version;
  const char* host_version;
  ElixirCoreReportFn report;
  void* report_context;
} ElixirCoreHostInfo;

typedef struct ElixirCoreApi {
  uint32_t struct_size;
  uint32_t abi_version;
  const char* core_version;
  const char* build;

  int (*initialize)(const ElixirCoreHostInfo* host);
  int (*run)(int argc, const char* const* argv);
  void (*request_stop)(void);
  void (*shutdown)(void);

  int (*submit_launch)(const char* json, uint32_t size);
} ElixirCoreApi;

ELIXIR_CORE_API const ElixirCoreApi* elixir_core_get_api(uint32_t requested_abi_version);
typedef const ElixirCoreApi* (*ElixirCoreGetApiFn)(uint32_t requested_abi_version);

#ifdef __cplusplus
}
#endif

#endif
