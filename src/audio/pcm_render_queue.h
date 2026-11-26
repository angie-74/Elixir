// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_AUDIO_PCM_RENDER_QUEUE_H_
#define ELIXIR_AUDIO_PCM_RENDER_QUEUE_H_

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "audio/pcm_converter.h"
#include "mocktail/audio/audio_sink.h"
#include "mocktail/status.h"

namespace mocktail::audio {

using AudioDataNeededCallback = bool (*)(void* context, const void** data,
                                         std::size_t* size_bytes);

struct PcmRenderQueueStats {
  std::uint64_t enqueued_buffers = 0;
  std::uint64_t enqueued_bytes = 0;
  std::uint64_t consumed_bytes = 0;
  std::uint64_t released_buffers = 0;
  std::uint64_t discarded_buffers = 0;
  std::uint64_t rendered_frames = 0;
  std::uint64_t data_frames = 0;
  std::uint64_t underrun_frames = 0;
  std::uint64_t pull_calls = 0;
};

class PcmRenderQueue final {
 public:
  explicit PcmRenderQueue(const PcmSpec& source_spec);
  ~PcmRenderQueue();

  PcmRenderQueue(const PcmRenderQueue&) = delete;
  PcmRenderQueue& operator=(const PcmRenderQueue&) = delete;

  const PcmSpec& source_spec() const { return source_spec_; }

  Status Enqueue(const PcmBuffer& buffer);
  Status SetGain(float linear_gain);
  float gain() const { return gain_.load(std::memory_order_relaxed); }
  void Flush();
  void Clear();
  std::size_t QueuedBytes() const;
  void SetDataNeededCallback(AudioDataNeededCallback callback, void* context);
  void SetPlaying(bool playing);
  bool playing() const { return playing_.load(std::memory_order_acquire); }

  Status SetOutputLayout(const PcmLayout& layout);
  const PcmLayout& output_layout() const { return output_layout_; }
  std::size_t Render(void* output, std::size_t frames);
  std::size_t ConverterBufferedFrames() const;

  void ReleaseAll();
  bool closed() const;

  PcmRenderQueueStats stats() const;

 private:
  struct Entry {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    std::size_t offset = 0;
    AudioBufferReleaseCallback release = nullptr;
    void* release_context = nullptr;
    std::vector<std::uint8_t> owned;
  };
  struct Release {
    AudioBufferReleaseCallback callback;
    void* context;
    const void* data;
    std::size_t size;
  };

  Status EnqueueLocked(const PcmBuffer& buffer);
  void Pull(std::size_t frames);
  static void InvokeReleases(std::vector<Release>& releases);
  void WriteSilence(void* output, std::size_t frames) const;
  void WaitForStartedReleasesLocked(std::unique_lock<std::mutex>& lock);

  const PcmSpec source_spec_;
  const std::size_t source_frame_bytes_;
  mutable std::mutex mutex_;
  std::deque<Entry> entries_;
  std::size_t queued_bytes_ = 0;
  PcmStreamConverter converter_;
  PcmLayout output_layout_;
  bool output_configured_ = false;
  bool flushed_ = false;
  bool closed_ = false;
  std::atomic<float> gain_{1.0F};
  std::atomic<bool> playing_{false};
  AudioDataNeededCallback data_needed_ = nullptr;
  void* data_needed_context_ = nullptr;
  std::vector<Release> pending_releases_;
  std::uint64_t release_batches_started_ = 0;
  std::uint64_t release_batches_finished_ = 0;
  std::thread::id releasing_thread_;
  std::condition_variable releases_finished_;
  PcmRenderQueueStats stats_;
};

}

#endif
