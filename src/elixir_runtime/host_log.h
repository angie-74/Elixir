// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_HOST_LOG_H_
#define ELIXIR_RUNTIME_HOST_LOG_H_

#include <string>
#include <string_view>

namespace elixir::runtime {

void Log(std::string_view message) noexcept;
void LogError(std::string_view message) noexcept;

void GuestLogSink(int priority, const char* tag, const char* message) noexcept;
void ResetConnectionDiagnostics() noexcept;
void LoaderLogSink(int priority, const char* tag, const char* message) noexcept;

unsigned long GuestLogLineCount();

using GuestLineObserver = void (*)(void* context, const char* tag, std::string_view message);
void SetGuestLineObserver(GuestLineObserver observer, void* context) noexcept;

using EventSink = void (*)(void* context, unsigned kind, const char* json);
void SetEventSink(EventSink sink, void* context) noexcept;
void EmitEvent(unsigned kind, std::string_view event, std::string_view detail) noexcept;

}

#endif
