// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/runtime.h"
#include "bionic/threads/thread_internal.h"

namespace elixir::bionic {

void RegisterThreadExports(ExportTable& table) {
  threads::internal::RegisterLifecycleExports(table);
  threads::internal::RegisterKeyExports(table);
  threads::internal::RegisterMutexExports(table);
  threads::internal::RegisterCondExports(table);
  threads::internal::RegisterRwlockExports(table);
  threads::internal::RegisterSyncMiscExports(table);
  threads::internal::RegisterSchedExports(table);
  threads::internal::RegisterC11Exports(table);
  threads::internal::RegisterLibcInternalExports(table);
}

}
