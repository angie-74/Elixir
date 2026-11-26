// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "runtime/url_split.h"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace mocktail {
namespace runtime {
namespace {

bool IsAsciiLetter(char character) {
  return (character >= 'a' && character <= 'z') ||
         (character >= 'A' && character <= 'Z');
}

bool IsAsciiDigit(char character) {
  return character >= '0' && character <= '9';
}

bool IsSchemeCharacter(char character, bool first) {
  if (first) {
    return IsAsciiLetter(character);
  }
  return IsAsciiLetter(character) || IsAsciiDigit(character) ||
         character == '+' || character == '-' || character == '.';
}

bool IsHostCharacter(char character) {
  return IsAsciiLetter(character) || IsAsciiDigit(character) ||
         character == '-' || character == '.' || character == '_';
}

char AsciiLower(char character) {
  return character >= 'A' && character <= 'Z'
             ? static_cast<char>(character - 'A' + 'a')
             : character;
}

}

bool SplitUrl(std::string_view url, UrlComponents* components) {
  if (components == nullptr) {
    return false;
  }
  *components = {};
  if (url.empty()) {
    return false;
  }
  for (const char character : url) {
    const unsigned char byte = static_cast<unsigned char>(character);
    if (byte <= 0x20 || byte == 0x7f || character == '\\') {
      return false;
    }
  }

  UrlComponents parts;
  const std::size_t colon = url.find(':');
  if (colon == std::string_view::npos || colon == 0) {
    return false;
  }
  for (std::size_t index = 0; index < colon; ++index) {
    if (!IsSchemeCharacter(url[index], index == 0)) {
      return false;
    }
    parts.scheme.push_back(AsciiLower(url[index]));
  }

  std::string_view rest = url.substr(colon + 1);
  if (rest.size() < 2 || rest[0] != '/' || rest[1] != '/') {
    return false;
  }
  rest.remove_prefix(2);

  const std::size_t authority_end = rest.find_first_of("/?#");
  std::string_view authority = rest.substr(0, authority_end);
  rest = authority_end == std::string_view::npos
             ? std::string_view()
             : rest.substr(authority_end);

  const std::size_t at = authority.rfind('@');
  if (at != std::string_view::npos) {
    parts.has_user_info = true;
    authority.remove_prefix(at + 1);
  }

  const std::size_t port_colon = authority.find(':');
  const std::string_view host = authority.substr(0, port_colon);
  if (host.empty()) {
    return false;
  }
  for (const char character : host) {
    if (!IsHostCharacter(character)) {
      return false;
    }
  }
  parts.host.assign(host);

  if (port_colon != std::string_view::npos) {
    std::string_view port = authority.substr(port_colon + 1);
    for (const char character : port) {
      if (!IsAsciiDigit(character)) {
        return false;
      }
    }
    if (!port.empty()) {
      while (port.size() > 1 && port.front() == '0') {
        port.remove_prefix(1);
      }
      if (port.size() > 5) {
        return false;
      }
      uint32_t value = 0;
      for (const char character : port) {
        value = value * 10 + static_cast<uint32_t>(character - '0');
      }
      if (value > 65535) {
        return false;
      }
      parts.has_port = true;
      parts.port.assign(port);
    }
  }

  const std::size_t path_end = rest.find_first_of("?#");
  const std::string_view path = rest.substr(0, path_end);
  parts.path = path.empty() ? std::string("/") : std::string(path);
  rest = path_end == std::string_view::npos ? std::string_view()
                                            : rest.substr(path_end);

  if (!rest.empty() && rest.front() == '?') {
    const std::size_t fragment = rest.find('#');
    parts.has_query = true;
    parts.query.assign(rest.substr(
        1, fragment == std::string_view::npos ? std::string_view::npos
                                              : fragment - 1));
    rest = fragment == std::string_view::npos ? std::string_view()
                                              : rest.substr(fragment);
  }
  if (!rest.empty() && rest.front() == '#') {
    parts.has_fragment = true;
    parts.fragment.assign(rest.substr(1));
  }

  *components = std::move(parts);
  return true;
}

}
}
