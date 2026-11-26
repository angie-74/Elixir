// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "audio/pcm_render_queue.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace mocktail::audio {
namespace {

constexpr std::size_t kMaximumPullBuffersPerRequest = 32;

}

PcmRenderQueue::PcmRenderQueue(const PcmSpec& source_spec)
    : source_spec_(source_spec), source_frame_bytes_(BytesPerFrame(source_spec)) {
  pending_releases_.reserve(64);
}

PcmRenderQueue::~PcmRenderQueue() { ReleaseAll(); }

Status PcmRenderQueue::EnqueueLocked(const PcmBuffer& buffer) {
  if (closed_) {
    return Status::Error(StatusCode::kFailedPrecondition, "audio sink is shut down");
  }
  Entry entry;
  entry.size = buffer.size_bytes;
  if (buffer.release_callback == nullptr) {
    try {
      entry.owned.assign(static_cast<const std::uint8_t*>(buffer.data),
                         static_cast<const std::uint8_t*>(buffer.data) + buffer.size_bytes);
    } catch (const std::bad_alloc&) {
      return Status::Error(StatusCode::kUnavailable, "unable to copy PCM buffer");
    }
    entry.data = entry.owned.data();
  } else {
    entry.data = static_cast<const std::uint8_t*>(buffer.data);
    entry.release = buffer.release_callback;
    entry.release_context = buffer.release_context;
  }
  try {
    entries_.push_back(std::move(entry));
  } catch (const std::bad_alloc&) {
    return Status::Error(StatusCode::kUnavailable, "unable to queue PCM buffer");
  }
  queued_bytes_ += buffer.size_bytes;
  flushed_ = false;
  ++stats_.enqueued_buffers;
  stats_.enqueued_bytes += buffer.size_bytes;
  return Status::Ok();
}

Status PcmRenderQueue::Enqueue(const PcmBuffer& buffer) {
  if (buffer.data == nullptr || buffer.size_bytes == 0) {
    return Status::Error(StatusCode::kInvalidArgument, "PCM buffer must contain data");
  }
  if (source_frame_bytes_ == 0 || buffer.size_bytes % source_frame_bytes_ != 0) {
    return Status::Error(StatusCode::kInvalidArgument, "PCM buffer size must contain complete frames");
  }
  if (buffer.size_bytes > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return Status::Error(StatusCode::kInvalidArgument, "PCM buffer exceeds the per-call byte limit");
  }
  std::lock_guard<std::mutex> lock(mutex_);
  return EnqueueLocked(buffer);
}

Status PcmRenderQueue::SetGain(float linear_gain) {
  if (!std::isfinite(linear_gain) || linear_gain < 0.0F) {
    return Status::Error(StatusCode::kInvalidArgument, "audio gain must be finite and non-negative");
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (closed_) return Status::Error(StatusCode::kFailedPrecondition, "audio sink is shut down");
  gain_.store(linear_gain, std::memory_order_relaxed);
  return Status::Ok();
}

void PcmRenderQueue::Flush() {
  std::lock_guard<std::mutex> lock(mutex_);
  flushed_ = true;
}

void PcmRenderQueue::WaitForStartedReleasesLocked(std::unique_lock<std::mutex>& lock) {
  const std::uint64_t started = release_batches_started_;
  if (release_batches_finished_ >= started) return;
  if (releasing_thread_ == std::this_thread::get_id()) return;
  releases_finished_.wait(lock, [this, started] { return release_batches_finished_ >= started; });
}

void PcmRenderQueue::Clear() {
  std::vector<Release> releases;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    for (Entry& entry : entries_) {
      if (entry.release != nullptr) {
        releases.push_back({entry.release, entry.release_context, entry.data, entry.size});
        ++stats_.discarded_buffers;
      }
    }
    entries_.clear();
    queued_bytes_ = 0;
    flushed_ = false;
    if (converter_.configured()) converter_.Reset();
    WaitForStartedReleasesLocked(lock);
  }
  InvokeReleases(releases);
}

void PcmRenderQueue::ReleaseAll() {
  std::vector<Release> releases;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    closed_ = true;
    for (Entry& entry : entries_) {
      if (entry.release != nullptr) {
        releases.push_back({entry.release, entry.release_context, entry.data, entry.size});
        ++stats_.discarded_buffers;
      }
    }
    entries_.clear();
    queued_bytes_ = 0;
    data_needed_ = nullptr;
    data_needed_context_ = nullptr;
    WaitForStartedReleasesLocked(lock);
  }
  InvokeReleases(releases);
}

bool PcmRenderQueue::closed() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return closed_;
}

std::size_t PcmRenderQueue::QueuedBytes() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return queued_bytes_;
}

void PcmRenderQueue::SetDataNeededCallback(AudioDataNeededCallback callback, void* context) {
  std::lock_guard<std::mutex> lock(mutex_);
  data_needed_ = callback;
  data_needed_context_ = context;
}

void PcmRenderQueue::SetPlaying(bool playing) {
  std::lock_guard<std::mutex> lock(mutex_);
  playing_.store(playing, std::memory_order_release);
}

Status PcmRenderQueue::SetOutputLayout(const PcmLayout& layout) {
  std::lock_guard<std::mutex> lock(mutex_);
  const Status status = converter_.Configure(LayoutFromPcmSpec(source_spec_), layout,
                                             true);
  if (status.ok()) {
    output_layout_ = layout;
    output_configured_ = true;
  }
  return status;
}

std::size_t PcmRenderQueue::ConverterBufferedFrames() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return converter_.buffered_input_frames();
}

PcmRenderQueueStats PcmRenderQueue::stats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return stats_;
}

void PcmRenderQueue::InvokeReleases(std::vector<Release>& releases) {
  for (const Release& release : releases) {
    release.callback(release.context, release.data, release.size);
  }
  releases.clear();
}

void PcmRenderQueue::WriteSilence(void* output, std::size_t frames) const {
  const std::size_t bytes = frames * BytesPerFrame(output_layout_);
  std::memset(output, output_layout_.type == PcmSampleType::kU8 ? 0x80 : 0, bytes);
}

void PcmRenderQueue::Pull(std::size_t frames) {
  AudioDataNeededCallback callback = nullptr;
  void* context = nullptr;
  std::size_t deficit_bytes = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_ || data_needed_ == nullptr || !output_configured_) return;
    const std::size_t wanted_frames = converter_.InputFramesWanted(frames);
    const std::size_t wanted_bytes = wanted_frames * source_frame_bytes_;
    if (wanted_bytes <= queued_bytes_) return;
    deficit_bytes = wanted_bytes - queued_bytes_;
    callback = data_needed_;
    context = data_needed_context_;
  }
  std::size_t supplied = 0;
  for (std::size_t index = 0; supplied < deficit_bytes && index < kMaximumPullBuffersPerRequest;
       ++index) {
    const void* data = nullptr;
    std::size_t size_bytes = 0;
    if (!callback(context, &data, &size_bytes) || data == nullptr || size_bytes == 0) break;
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.pull_calls;
    if (size_bytes % source_frame_bytes_ != 0 ||
        size_bytes > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        !EnqueueLocked(PcmBuffer{data, size_bytes, nullptr, nullptr}).ok()) {
      break;
    }
    supplied += size_bytes;
  }
}

std::size_t PcmRenderQueue::Render(void* output, std::size_t frames) {
  if (output == nullptr || frames == 0) return 0;
  if (!playing_.load(std::memory_order_acquire)) {
    std::lock_guard<std::mutex> lock(mutex_);
    WriteSilence(output, frames);
    stats_.rendered_frames += frames;
    return 0;
  }
  Pull(frames);

  std::vector<Release> releases;
  releases.swap(pending_releases_);
  std::size_t produced = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!output_configured_) return 0;
    if (!playing_.load(std::memory_order_relaxed) || closed_) {
      WriteSilence(output, frames);
      stats_.rendered_frames += frames;
      pending_releases_.swap(releases);
      return 0;
    }
    auto* bytes = static_cast<std::uint8_t*>(output);
    const std::size_t frame_bytes = BytesPerFrame(output_layout_);
    const float gain = gain_.load(std::memory_order_relaxed);
    for (;;) {
      const bool drain = flushed_ && entries_.empty();
      produced += converter_.Pull(bytes + produced * frame_bytes, frames - produced, gain, drain);
      if (produced >= frames || entries_.empty()) break;
      std::size_t wanted = converter_.InputFramesWanted(frames - produced);
      if (wanted == 0) wanted = 1;
      Entry& head = entries_.front();
      const std::size_t available = (head.size - head.offset) / source_frame_bytes_;
      const std::size_t take = std::min(available, wanted);
      converter_.Push(head.data + head.offset, take);
      head.offset += take * source_frame_bytes_;
      queued_bytes_ -= take * source_frame_bytes_;
      stats_.consumed_bytes += take * source_frame_bytes_;
      if (head.offset >= head.size) {
        if (head.release != nullptr) {
          releases.push_back({head.release, head.release_context, head.data, head.size});
          ++stats_.released_buffers;
        }
        entries_.pop_front();
      }
    }
    if (produced < frames) {
      WriteSilence(bytes + produced * frame_bytes, frames - produced);
      stats_.underrun_frames += frames - produced;
    }
    stats_.rendered_frames += frames;
    stats_.data_frames += produced;
    if (!releases.empty()) {
      ++release_batches_started_;
      releasing_thread_ = std::this_thread::get_id();
    }
  }
  const bool delivered_batch = !releases.empty();
  InvokeReleases(releases);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (delivered_batch) {
      ++release_batches_finished_;
      releasing_thread_ = std::thread::id();
    }
    if (pending_releases_.capacity() < releases.capacity()) pending_releases_.swap(releases);
  }
  if (delivered_batch) releases_finished_.notify_all();
  return produced;
}

}
