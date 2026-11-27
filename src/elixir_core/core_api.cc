// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir/core_api.h"

#include <windows.h>
#include <fcntl.h>
#include <io.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "elixir_runtime/cli.h"
#include "elixir_runtime/host_log.h"
#include "elixir_runtime/link_joins.h"
#include "elixir_runtime/runtime_main.h"
#include "runtime/session_log.h"

static_assert(static_cast<unsigned>(elixir::runtime::kExitRestart) == ELIXIR_EXIT_RESTART);

#ifndef ELIXIR_VERSION
#define ELIXIR_VERSION "0.0.0"
#endif
#ifndef ELIXIR_CORE_BUILD
#define ELIXIR_CORE_BUILD "unknown"
#endif

namespace {

ElixirCoreHostInfo g_host{};

bool HasUsableHandle(DWORD which) {
  const HANDLE handle = GetStdHandle(which);
  return handle != nullptr && handle != INVALID_HANDLE_VALUE && GetFileType(handle) != FILE_TYPE_UNKNOWN;
}

void PrepareStandardStreams() {
  HANDLE console = nullptr;
  if (!HasUsableHandle(STD_OUTPUT_HANDLE) && !HasUsableHandle(STD_ERROR_HANDLE) &&
      AttachConsole(ATTACH_PARENT_PROCESS)) {
    console = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                          OPEN_EXISTING, 0, nullptr);
    if (console == INVALID_HANDLE_VALUE) console = nullptr;
  }
  mocktail::runtime::BindStandardStreams(console);
  if (console != nullptr) CloseHandle(console);
  for (FILE* stream : {stdout, stderr}) {
    if (_fileno(stream) >= 0) _setmode(_fileno(stream), _O_BINARY);
  }
}

void ForwardEvent(void* context, unsigned kind, const char* json) {
  (void)context;
  if (g_host.report != nullptr) g_host.report(g_host.report_context, kind, json);
}

int Initialize(const ElixirCoreHostInfo* host) {
  g_host = ElixirCoreHostInfo{};
  if (host == nullptr || host->struct_size < offsetof(ElixirCoreHostInfo, report)) return 2;
  std::memcpy(&g_host, host, std::min<std::size_t>(host->struct_size, sizeof(g_host)));
  PrepareStandardStreams();
  elixir::runtime::SetEventSink(g_host.report != nullptr ? &ForwardEvent : nullptr, nullptr);
  return 0;
}

int Run(int argc, const char* const* argv) {
  try {
    std::vector<std::string> arguments;
    arguments.reserve(argc > 0 ? static_cast<std::size_t>(argc) : 0);
    for (int i = 0; i < argc; ++i) arguments.emplace_back(argv[i] != nullptr ? argv[i] : "");
    return elixir::runtime::RunElixirRuntime(arguments);
  } catch (...) {
    elixir::runtime::LogError("elixir-core: unexpected C++ exception; the session ends");
    elixir::runtime::EmitEvent(2, "error", "unexpected C++ exception in the runtime");
    return 4;
  }
}

void RequestStop() { elixir::runtime::RequestStop(); }

int SubmitLaunch(const char* json, uint32_t size) {
  if (json == nullptr || size == 0 || size > ELIXIR_LAUNCH_MAX_BYTES) return ELIXIR_LAUNCH_INVALID;
  try {
    return elixir::runtime::SubmitLinkJoin(std::string_view(json, size));
  } catch (...) {
    return ELIXIR_LAUNCH_UNAVAILABLE;
  }
}

void Shutdown() {
  elixir::runtime::SetEventSink(nullptr, nullptr);
  std::fflush(stdout);
  std::fflush(stderr);
  g_host = ElixirCoreHostInfo{};
}

const ElixirCoreApi kApi = {
    sizeof(ElixirCoreApi), ELIXIR_CORE_ABI_VERSION, ELIXIR_VERSION, ELIXIR_CORE_BUILD,
    &Initialize,           &Run,                    &RequestStop,   &Shutdown,
    &SubmitLaunch,
};

}

extern "C" ELIXIR_CORE_API const ElixirCoreApi* elixir_core_get_api(uint32_t requested_abi_version) {
  return requested_abi_version == ELIXIR_CORE_ABI_VERSION ? &kApi : nullptr;
}
