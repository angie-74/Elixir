// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/net/net_internal.h"

#include <chrono>

namespace elixir::bionic::net::internal {

PairChannel::Reserve PairChannel::TryReserve(int to, std::size_t length) noexcept {
  std::lock_guard<std::mutex> guard(lock_);
  if (closed_[to]) return kPeerClosed;
  if (queued_[to] >= kBudget) return kFull;
  const std::size_t charge = (length > 0xffff ? 0xffff : length) + kOverhead;
  try {
    charges_[to].push_back(static_cast<std::uint32_t>(charge));
  } catch (...) {
    return kReserved;
  }
  queued_[to] += charge;
  return kReserved;
}

void PairChannel::Attach(int end, const std::shared_ptr<SocketState>& state) noexcept {
  std::lock_guard<std::mutex> guard(lock_);
  ends_[end] = state;
}

void PairChannel::Cancel(int to) noexcept {
  std::lock_guard<std::mutex> guard(lock_);
  if (charges_[to].empty()) return;
  queued_[to] -= charges_[to].back();
  charges_[to].pop_back();
}

void PairChannel::Consumed(int to) noexcept {
  {
    std::lock_guard<std::mutex> guard(lock_);
    if (charges_[to].empty()) return;
    queued_[to] -= charges_[to].front();
    charges_[to].pop_front();
  }
  space_.notify_all();
}

bool PairChannel::Writable(int to) noexcept {
  std::lock_guard<std::mutex> guard(lock_);
  return closed_[to] || queued_[to] < kBudget;
}

int PairChannel::WaitWritable(int to, const Deadline& deadline) noexcept {
  std::unique_lock<std::mutex> guard(lock_);
  while (!closed_[to] && queued_[to] >= kBudget) {
    if (deadline.infinite()) {
      space_.wait(guard);
      continue;
    }
    const std::int64_t left = deadline.RemainingNs();
    if (left <= 0) return guest::kEAGAIN;
    space_.wait_for(guard, std::chrono::nanoseconds(left));
  }
  return 0;
}

void PairChannel::Close(int end) noexcept {
  std::shared_ptr<SocketState> peer;
  {
    std::lock_guard<std::mutex> guard(lock_);
    closed_[end] = true;
    charges_[end].clear();
    queued_[end] = 0;
    peer = ends_[1 - end].lock();
  }
  space_.notify_all();
  if (peer != nullptr && peer->type == guest::kSOCK_SEQPACKET) {
    peer->shut_rd.store(true);
    peer->shut_wr.store(true);
    NoteShutdown(peer.get());
  }
}

}
