// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_BIONIC_FILES_STDIO_INTERNAL_H_
#define ELIXIR_BIONIC_FILES_STDIO_INTERNAL_H_

#include <windows.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "bionic/files/files_internal.h"

namespace elixir::bionic::files {

class StreamBackend {
 public:
  virtual ~StreamBackend() = default;
  virtual std::int64_t Read(void* buffer, std::size_t count) = 0;
  virtual std::int64_t Write(const void* buffer, std::size_t count) = 0;
  virtual std::int64_t Seek(std::int64_t offset, int whence) = 0;
  virtual int Close() = 0;
  virtual int Fd() const { return -1; }
  virtual bool IsConsole() const { return false; }
};

std::unique_ptr<StreamBackend> MakeFdBackend(int fd);

struct Stream {
  Stream();
  ~Stream();
  Stream(const Stream&) = delete;
  Stream& operator=(const Stream&) = delete;

  std::uint64_t magic;
  guest::File* file = nullptr;
  int std_index = -1;
  CRITICAL_SECTION lock;
  std::unique_ptr<StreamBackend> backend;
  bool readable = false, writable = false, append = false;
  bool eof = false, error = false, closed = false;
  int buffer_mode = -1;
  unsigned char* buffer = nullptr;
  std::size_t buffer_size = 0;
  bool own_buffer = false;
  unsigned char tiny[1] = {};
  enum class Mode : std::uint8_t { kIdle, kReading, kWriting } mode = Mode::kIdle;
  std::size_t rpos = 0, rend = 0;
  std::size_t wlen = 0;
  std::vector<unsigned char> pushback;
  int orientation = 0;
  bool caller_locking = false;
  std::string line;
};

inline constexpr std::uint64_t kStreamMagic = 0x4d52545358454c45ull;

Stream* StreamOf(void* file);
guest::File* NewStream(std::unique_ptr<StreamBackend> backend, bool readable, bool writable, bool append);
guest::File* StdStreamFile(int index);
guest::File* StdVariable(int index);

struct StreamLock {
  explicit StreamLock(Stream* stream) : stream_(stream) { EnterCriticalSection(&stream_->lock); }
  ~StreamLock() { LeaveCriticalSection(&stream_->lock); }
  StreamLock(const StreamLock&) = delete;
  StreamLock& operator=(const StreamLock&) = delete;
  Stream* stream_;
};

std::size_t StreamReadLocked(Stream* s, void* buffer, std::size_t count);
std::size_t StreamWriteLocked(Stream* s, const void* buffer, std::size_t count);
int StreamGetcLocked(Stream* s);
int StreamPutcLocked(Stream* s, int c);
int StreamUngetcLocked(Stream* s, int c);
int StreamFlushLocked(Stream* s);
bool StreamIsUnbuffered(Stream* s);

std::uint32_t StreamGetwcLocked(Stream* s);
std::uint32_t StreamPutwcLocked(Stream* s, std::uint32_t wc);
std::uint32_t StreamUngetwcLocked(Stream* s, std::uint32_t wc);

bool ParseStdioMode(const char* mode, int* open_flags, bool* readable, bool* writable, bool* append);

}

#endif
