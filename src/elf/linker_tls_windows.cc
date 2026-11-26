// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elf/linker_tls_windows.h"

#include "linker_globals.h"
#include "linker_soinfo.h"
#include "platform/windows/guest_tls.h"

#ifndef DF_STATIC_TLS
#define DF_STATIC_TLS 0x00000010
#endif

bool elixir_register_soinfo_tls(soinfo* si) {
  soinfo_tls* tls = si->get_tls();
  if (tls == nullptr || tls->module_id != kTlsUninitializedModuleId) return true;
  bool want_static = false;
  for (const ElfW(Dyn)* d = si->dynamic; d != nullptr && d->d_tag != DT_NULL; ++d) {
    if (d->d_tag == DT_FLAGS && (d->d_un.d_val & DF_STATIC_TLS) != 0) want_static = true;
  }
  elixir::platform::TlsSegmentImage image;
  image.init_image = tls->segment.init_ptr;
  image.init_size = tls->segment.init_size;
  image.memory_size = tls->segment.size;
  image.alignment = tls->segment.alignment;
  const elixir::platform::TlsModuleRegistration registration =
      elixir::platform::RegisterTlsModule(image, want_static);
  if (registration.module_id == 0) {
    DL_ERR("\"%s\" has a TLS segment but guest TLS is unavailable (was the runtime initialized?)",
           si->get_realpath());
    return false;
  }
  tls->module_id = registration.module_id;
  tls->elixir_is_static = registration.is_static;
  tls->elixir_tp_offset = registration.tp_offset;
  return true;
}

void elixir_unregister_soinfo_tls(soinfo* si) {
  soinfo_tls* tls = si->get_tls();
  if (tls == nullptr || tls->module_id == kTlsUninitializedModuleId) return;
  elixir::platform::UnregisterTlsModule(tls->module_id);
  tls->module_id = kTlsUninitializedModuleId;
}

void* elixir_tls_symbol_address(soinfo* si, const ElfW(Sym)* sym) {
  soinfo_tls* tls = si->get_tls();
  if (tls == nullptr || tls->module_id == kTlsUninitializedModuleId) return nullptr;
  auto* block = static_cast<char*>(elixir::platform::GetTlsBlock(tls->module_id));
  return block != nullptr ? block + sym->st_value : nullptr;
}

void* elixir_tls_block_if_allocated(size_t module_id) {
  return elixir::platform::FindTlsBlock(module_id);
}
