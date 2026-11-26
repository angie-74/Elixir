// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_ELF_LOADER_LOG_H_
#define ELIXIR_ELF_LOADER_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*elixir_loader_log_sink)(int priority, const char* tag, const char* message);
void elixir_loader_set_log_sink(elixir_loader_log_sink sink);

#ifdef __cplusplus
}
#endif

#endif
