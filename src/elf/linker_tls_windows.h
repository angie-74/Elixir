// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_ELF_LINKER_TLS_WINDOWS_H_
#define ELIXIR_ELF_LINKER_TLS_WINDOWS_H_

#include <stddef.h>

#include <elf.h>
#include <link.h>

struct soinfo;

bool elixir_register_soinfo_tls(soinfo* si);
void elixir_unregister_soinfo_tls(soinfo* si);
void* elixir_tls_symbol_address(soinfo* si, const ElfW(Sym)* sym);
void* elixir_tls_block_if_allocated(size_t module_id);

#endif
