// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_WINHTTP_CLIENT_H_
#define ELIXIR_RUNTIME_WINHTTP_CLIENT_H_

#include "services/http_client.h"

namespace elixir::runtime {

class WinHttpClient final : public mocktail::services::HttpClient {
 public:
  mocktail::services::HttpResponse Get(const mocktail::services::HttpRequest& request) override;
};

}

#endif
