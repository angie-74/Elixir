// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/net/dns_native.h"
#include "bionic/net/net_internal.h"

namespace elixir::bionic::net {

void RegisterNetExports(ExportTable& table) {
  internal::InstallFilesHooks();
  internal::RegisterSocketExports(table);
  internal::RegisterSockoptExports(table);
  internal::RegisterIoExports(table);
  internal::RegisterNetdbExports(table);
  internal::RegisterServiceExports(table);
  internal::RegisterInetExports(table);
  internal::RegisterIfaddrsExports(table);
  internal::RegisterPollExports(table);
  internal::RegisterEpollExports(table);
  internal::RegisterObjectExports(table);
  internal::RegisterDnsNativeExports(table);
}

}
