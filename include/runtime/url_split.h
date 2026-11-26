// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef MOCKTAIL_RUNTIME_URL_SPLIT_H_
#define MOCKTAIL_RUNTIME_URL_SPLIT_H_

#include <string>
#include <string_view>

namespace mocktail {
namespace runtime {

struct UrlComponents {
  std::string scheme;
  bool has_user_info = false;
  std::string host;
  bool has_port = false;
  std::string port;
  std::string path;
  bool has_query = false;
  std::string query;
  bool has_fragment = false;
  std::string fragment;
};

bool SplitUrl(std::string_view url, UrlComponents* components);

}
}

#endif
