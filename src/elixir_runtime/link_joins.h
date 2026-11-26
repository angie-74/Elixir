// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_LINK_JOINS_H_
#define ELIXIR_RUNTIME_LINK_JOINS_H_

#include <cstddef>
#include <memory>
#include <string_view>

namespace mocktail::runtime {
class ExternalLaunchBroker;
}

namespace elixir::runtime {

inline constexpr std::size_t kMaximumQueuedLinkJoins = 8;

int SubmitLinkJoin(std::string_view json);

void AttachLinkJoinBroker(std::shared_ptr<mocktail::runtime::ExternalLaunchBroker> broker);
void DetachLinkJoinBroker();

std::size_t RefuseLinkJoins();

void ResetLinkJoinsForTest();

}

#endif
