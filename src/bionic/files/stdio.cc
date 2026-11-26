// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "bionic/files/stdio.h"

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <mutex>

#include "bionic/files/stdio_internal.h"
#include "bionic/memory.h"
#include "bionic/text.h"

namespace elixir::bionic::files {
namespace {

constexpr std::uint64_t kFileMagic = 0x454c4946584c45ull;
constexpr std::size_t kDefaultBufferSize = 4096;

alignas(8) guest::File g_sF[3];
guest::File* g_stdin = &g_sF[0];
guest::File* g_stdout = &g_sF[1];
guest::File* g_stderr = &g_sF[2];

struct Registry {
  std::mutex lock;
  std::vector<std::shared_ptr<Stream>> streams;
};
Registry& Streams() {
  static Registry* registry = new Registry();
  return *registry;
}

std::shared_ptr<Stream> g_std[3];
std::once_flag g_std_once;

class FdBackend final : public StreamBackend {
 public:
  explicit FdBackend(int fd) : fd_(fd) {}
  std::int64_t Read(void* buffer, std::size_t count) override {
    DescriptionPtr d = GetDescription(fd_);
    if (d == nullptr) return -1;
    return DescRead(*d, buffer, std::min<std::size_t>(count, 0x7ffff000));
  }
  std::int64_t Write(const void* buffer, std::size_t count) override {
    DescriptionPtr d = GetDescription(fd_);
    if (d == nullptr) return -1;
    return DescWrite(*d, buffer, std::min<std::size_t>(count, 0x7ffff000));
  }
  std::int64_t Seek(std::int64_t offset, int whence) override {
    DescriptionPtr d = GetDescription(fd_);
    if (d == nullptr) return -1;
    return DescSeek(*d, offset, whence);
  }
  int Close() override { return CloseFd(fd_); }
  int Fd() const override { return fd_; }
  bool IsConsole() const override {
    DescriptionPtr d = FindDescription(fd_);
    return d != nullptr && d->provider == Provider::kHandle && d->kind == vfs::FdKind::kConsole;
  }

 private:
  int fd_;
};

void BindBlock(guest::File* file, Stream* stream) {
  std::memset(file->opaque, 0, sizeof(file->opaque));
  std::memcpy(file->opaque, &kFileMagic, sizeof(kFileMagic));
  std::memcpy(file->opaque + 8, &stream, sizeof(stream));
}

void EnsureStd() {
  std::call_once(g_std_once, [] {
    for (int i = 0; i < 3; ++i) {
      auto stream = std::make_shared<Stream>();
      stream->backend = MakeFdBackend(i);
      stream->readable = i == 0;
      stream->writable = i != 0;
      stream->std_index = i;
      stream->file = &g_sF[i];
      BindBlock(&g_sF[i], stream.get());
      g_std[i] = stream;
      Registry& registry = Streams();
      std::lock_guard<std::mutex> guard(registry.lock);
      registry.streams.push_back(stream);
    }
  });
}

Stream* StreamOfAny(void* file) {
  if (file == nullptr) return FailWith(guest::kEBADF, static_cast<Stream*>(nullptr));
  auto* block = static_cast<guest::File*>(file);
  if (block >= g_sF && block < g_sF + 3) EnsureStd();
  std::uint64_t magic = 0;
  std::memcpy(&magic, block->opaque, sizeof(magic));
  if (magic != kFileMagic) return FailWith(guest::kEBADF, static_cast<Stream*>(nullptr));
  Stream* stream = nullptr;
  std::memcpy(&stream, block->opaque + 8, sizeof(stream));
  if (stream == nullptr || stream->magic != kStreamMagic) return FailWith(guest::kEBADF, static_cast<Stream*>(nullptr));
  return stream;
}

void DetermineMode(Stream* s) {
  if (s->buffer_mode >= 0) return;
  if (s->std_index == 2) {
    s->buffer_mode = guest::k_IONBF;
  } else if (s->backend != nullptr && s->backend->IsConsole()) {
    s->buffer_mode = guest::k_IOLBF;
  } else {
    s->buffer_mode = guest::k_IOFBF;
  }
}

void EnsureBuffer(Stream* s) {
  DetermineMode(s);
  if (s->buffer != nullptr) return;
  if (s->buffer_mode == guest::k_IONBF) {
    s->buffer = s->tiny;
    s->buffer_size = 1;
    s->own_buffer = false;
    return;
  }
  const std::size_t size = s->buffer_size != 0 ? s->buffer_size : kDefaultBufferSize;
  s->buffer = new (std::nothrow) unsigned char[size];
  if (s->buffer == nullptr) {
    s->buffer_mode = guest::k_IONBF;
    s->buffer = s->tiny;
    s->buffer_size = 1;
    s->own_buffer = false;
    return;
  }
  s->buffer_size = size;
  s->own_buffer = true;
}

void ReleaseBuffer(Stream* s) {
  if (s->own_buffer) delete[] s->buffer;
  s->buffer = nullptr;
  s->buffer_size = 0;
  s->own_buffer = false;
}

std::size_t WriteAll(Stream* s, const unsigned char* data, std::size_t count) {
  std::size_t done = 0;
  while (done < count) {
    const std::int64_t written = s->backend->Write(data + done, count - done);
    if (written <= 0) {
      s->error = true;
      break;
    }
    done += static_cast<std::size_t>(written);
  }
  return done;
}

void DiscardRead(Stream* s) {
  const std::size_t unread = (s->rend - s->rpos) + s->pushback.size();
  if (s->mode == Stream::Mode::kReading && unread > 0) {
    const int saved = GuestErrno();
    s->backend->Seek(-static_cast<std::int64_t>(unread), guest::kSEEK_CUR);
    SetGuestErrno(saved);
  }
  s->rpos = s->rend = 0;
  s->pushback.clear();
  s->mode = Stream::Mode::kIdle;
}

bool BeginRead(Stream* s) {
  if (!s->readable) {
    s->error = true;
    SetGuestErrno(guest::kEBADF);
    return false;
  }
  if (s->mode == Stream::Mode::kWriting) {
    if (StreamFlushLocked(s) != 0) return false;
    s->rpos = s->rend = 0;
  }
  s->mode = Stream::Mode::kReading;
  if (s->orientation == 0) s->orientation = -1;
  return true;
}

bool BeginWrite(Stream* s) {
  if (!s->writable) {
    s->error = true;
    SetGuestErrno(guest::kEBADF);
    return false;
  }
  if (s->mode == Stream::Mode::kReading) DiscardRead(s);
  if (s->mode != Stream::Mode::kWriting) {
    s->mode = Stream::Mode::kWriting;
    s->wlen = 0;
  }
  if (s->orientation == 0) s->orientation = -1;
  return true;
}

void FlushInteractiveOutput(Stream* reader) {
  if (reader->buffer_mode != guest::k_IOLBF && reader->buffer_mode != guest::k_IONBF) return;
  EnsureStd();
  Stream* out = g_std[1].get();
  if (out == nullptr || out == reader) return;
  if (!TryEnterCriticalSection(&out->lock)) return;
  if (!out->closed && out->buffer_mode == guest::k_IOLBF && out->mode == Stream::Mode::kWriting && out->wlen > 0) {
    StreamFlushLocked(out);
  }
  LeaveCriticalSection(&out->lock);
}

bool Refill(Stream* s) {
  if (s->eof) return false;
  EnsureBuffer(s);
  FlushInteractiveOutput(s);
  const std::int64_t got = s->backend->Read(s->buffer, s->buffer_size);
  if (got > 0) {
    s->rpos = 0;
    s->rend = static_cast<std::size_t>(got);
    return true;
  }
  s->rpos = s->rend = 0;
  if (got == 0) {
    s->eof = true;
  } else {
    s->error = true;
  }
  return false;
}

std::int64_t TellLocked(Stream* s) {
  if (s->mode == Stream::Mode::kWriting && s->append && StreamFlushLocked(s) != 0) return -1;
  const std::int64_t base = s->backend->Seek(0, guest::kSEEK_CUR);
  if (base < 0) return -1;
  std::int64_t position = base;
  if (s->mode == Stream::Mode::kReading) {
    position -= static_cast<std::int64_t>((s->rend - s->rpos) + s->pushback.size());
    if (position < 0) position = 0;
  } else if (s->mode == Stream::Mode::kWriting) {
    position += static_cast<std::int64_t>(s->wlen);
  }
  return position;
}

int SeekLocked(Stream* s, std::int64_t offset, int whence) {
  if (whence != guest::kSEEK_SET && whence != guest::kSEEK_CUR && whence != guest::kSEEK_END) {
    return FailWith(guest::kEINVAL, -1);
  }
  if (s->mode == Stream::Mode::kWriting && StreamFlushLocked(s) != 0) return -1;
  if (whence == guest::kSEEK_CUR) {
    const std::int64_t current = TellLocked(s);
    if (current < 0) return -1;
    if (offset > 0 && current > INT64_MAX - offset) return FailWith(guest::kEOVERFLOW, -1);
    offset += current;
    whence = guest::kSEEK_SET;
    if (offset < 0) return FailWith(guest::kEINVAL, -1);
  }
  if (s->mode == Stream::Mode::kReading && s->pushback.empty() && whence == guest::kSEEK_SET && s->rend > 0) {
    const std::int64_t base = s->backend->Seek(0, guest::kSEEK_CUR);
    const std::int64_t start = base - static_cast<std::int64_t>(s->rend);
    if (base >= 0 && offset >= start && offset <= base) {
      s->rpos = static_cast<std::size_t>(offset - start);
      s->eof = false;
      return 0;
    }
  }
  if (s->backend->Seek(offset, whence) < 0) return -1;
  s->rpos = s->rend = 0;
  s->pushback.clear();
  s->mode = Stream::Mode::kIdle;
  s->eof = false;
  return 0;
}

void FlushAll(bool never_block) {
  std::vector<std::shared_ptr<Stream>> snapshot;
  {
    Registry& registry = Streams();
    std::lock_guard<std::mutex> guard(registry.lock);
    snapshot = registry.streams;
  }
  for (const auto& stream : snapshot) {
    if (never_block) {
      if (!TryEnterCriticalSection(&stream->lock)) continue;
    } else {
      EnterCriticalSection(&stream->lock);
    }
    if (!stream->closed && stream->writable) StreamFlushLocked(stream.get());
    LeaveCriticalSection(&stream->lock);
  }
}

int CloseStream(Stream* s) {
  std::shared_ptr<Stream> keep;
  {
    Registry& registry = Streams();
    std::lock_guard<std::mutex> guard(registry.lock);
    auto it = std::find_if(registry.streams.begin(), registry.streams.end(),
                           [&](const std::shared_ptr<Stream>& p) { return p.get() == s; });
    if (it != registry.streams.end()) {
      keep = *it;
      if (s->std_index < 0) registry.streams.erase(it);
    }
  }
  int result = 0;
  {
    StreamLock lock(s);
    if (s->closed) return FailWith(guest::kEBADF, guest::kEOF);
    if (s->writable && StreamFlushLocked(s) != 0) result = guest::kEOF;
    if (s->backend != nullptr && s->backend->Close() != 0) result = guest::kEOF;
    s->backend.reset();
    ReleaseBuffer(s);
    s->closed = true;
    s->pushback.clear();
    s->mode = Stream::Mode::kIdle;
  }
  if (s->std_index < 0) {
    guest::File* block = s->file;
    std::memset(block->opaque, 0, sizeof(block->opaque));
    GuestFree(block);
  }
  return result;
}

std::int64_t GetDelim(char** line, abi::g_size* capacity, int delimiter, Stream* s) {
  if (line == nullptr || capacity == nullptr) {
    s->error = true;
    return FailWith(guest::kEINVAL, static_cast<std::int64_t>(-1));
  }
  if (*line == nullptr) *capacity = 0;
  std::size_t length = 0;
  auto ensure = [&](std::size_t needed) {
    if (needed <= *capacity) return true;
    std::size_t next = std::max<std::size_t>(*capacity * 2, 120);
    while (next < needed) next *= 2;
    char* grown = static_cast<char*>(GuestRealloc(*line, next));
    if (grown == nullptr) {
      s->error = true;
      SetGuestErrno(guest::kENOMEM);
      return false;
    }
    *line = grown;
    *capacity = next;
    return true;
  };
  for (;;) {
    if (!BeginRead(s)) return -1;
    if (s->pushback.empty() && s->rpos < s->rend) {
      const unsigned char* start = s->buffer + s->rpos;
      const std::size_t available = s->rend - s->rpos;
      const void* hit = std::memchr(start, delimiter, available);
      const std::size_t take = hit != nullptr ? static_cast<std::size_t>(static_cast<const unsigned char*>(hit) - start) + 1
                                              : available;
      if (!ensure(length + take + 1)) return -1;
      std::memcpy(*line + length, start, take);
      length += take;
      s->rpos += take;
      if (hit != nullptr) break;
      continue;
    }
    const int c = StreamGetcLocked(s);
    if (c == guest::kEOF) break;
    if (!ensure(length + 2)) return -1;
    (*line)[length++] = static_cast<char>(c);
    if (c == (delimiter & 0xff)) break;
  }
  if (length == 0) {
    if (!ensure(1)) return -1;
    (*line)[0] = '\0';
    return -1;
  }
  (*line)[length] = '\0';
  return static_cast<std::int64_t>(length);
}

char* FgetsLocked(char* buffer, int size, Stream* s) {
  if (buffer == nullptr || size <= 0) return FailWith(guest::kEINVAL, static_cast<char*>(nullptr));
  if (size == 1) {
    buffer[0] = '\0';
    return buffer;
  }
  std::size_t length = 0;
  const std::size_t limit = static_cast<std::size_t>(size) - 1;
  while (length < limit) {
    if (s->mode == Stream::Mode::kReading && s->pushback.empty() && s->rpos < s->rend) {
      const unsigned char* start = s->buffer + s->rpos;
      const std::size_t available = std::min(s->rend - s->rpos, limit - length);
      const void* hit = std::memchr(start, '\n', available);
      const std::size_t take = hit != nullptr ? static_cast<std::size_t>(static_cast<const unsigned char*>(hit) - start) + 1
                                              : available;
      std::memcpy(buffer + length, start, take);
      length += take;
      s->rpos += take;
      if (hit != nullptr) break;
      continue;
    }
    const int c = StreamGetcLocked(s);
    if (c == guest::kEOF) break;
    buffer[length++] = static_cast<char>(c);
    if (c == '\n') break;
  }
  if (length == 0) return nullptr;
  buffer[length] = '\0';
  return buffer;
}

int SetvbufLocked(Stream* s, char* buffer, int mode, std::size_t size) {
  if (mode != guest::k_IOFBF && mode != guest::k_IOLBF && mode != guest::k_IONBF) return FailWith(guest::kEINVAL, -1);
  if (s->mode == Stream::Mode::kWriting) StreamFlushLocked(s);
  if (s->mode == Stream::Mode::kReading) DiscardRead(s);
  ReleaseBuffer(s);
  s->buffer_mode = mode;
  if (mode == guest::k_IONBF) {
    s->buffer = s->tiny;
    s->buffer_size = 1;
    return 0;
  }
  if (buffer != nullptr && size > 0) {
    s->buffer = reinterpret_cast<unsigned char*>(buffer);
    s->buffer_size = size;
    s->own_buffer = false;
    return 0;
  }
  s->buffer_size = size > 0 && size <= (64u << 20) ? size : kDefaultBufferSize;
  EnsureBuffer(s);
  return s->buffer_mode == mode ? 0 : guest::kEOF;
}

guest::File* OpenFdStream(int fd, bool readable, bool writable, bool append) {
  guest::File* file = NewStream(MakeFdBackend(fd), readable, writable, append);
  if (file == nullptr) {
    const int saved = GuestErrno();
    CloseFd(fd);
    SetGuestErrno(saved);
    return nullptr;
  }
  if (append) {
    Stream* s = StreamOf(file);
    StreamLock lock(s);
    s->backend->Seek(0, guest::kSEEK_END);
  }
  return file;
}

}

Stream::Stream() : magic(kStreamMagic) { InitializeCriticalSectionAndSpinCount(&lock, 400); }
Stream::~Stream() {
  magic = 0;
  ReleaseBuffer(this);
  DeleteCriticalSection(&lock);
}

std::unique_ptr<StreamBackend> MakeFdBackend(int fd) { return std::make_unique<FdBackend>(fd); }

Stream* StreamOf(void* file) {
  Stream* stream = StreamOfAny(file);
  if (stream != nullptr && stream->closed) return FailWith(guest::kEBADF, static_cast<Stream*>(nullptr));
  return stream;
}

guest::File* StdStreamFile(int index) { return &g_sF[index]; }

guest::File* StdVariable(int index) {
  switch (index) {
    case 0: return g_stdin;
    case 1: return g_stdout;
    default: return g_stderr;
  }
}

guest::File* NewStream(std::unique_ptr<StreamBackend> backend, bool readable, bool writable, bool append) {
  auto* block = static_cast<guest::File*>(GuestCalloc(1, sizeof(guest::File)));
  if (block == nullptr) return FailWith(guest::kENOMEM, static_cast<guest::File*>(nullptr));
  std::shared_ptr<Stream> stream;
  try {
    stream = std::make_shared<Stream>();
    stream->backend = std::move(backend);
    stream->readable = readable;
    stream->writable = writable;
    stream->append = append;
    stream->file = block;
    Registry& registry = Streams();
    std::lock_guard<std::mutex> guard(registry.lock);
    registry.streams.push_back(stream);
  } catch (...) {
    GuestFree(block);
    return FailWith(guest::kENOMEM, static_cast<guest::File*>(nullptr));
  }
  BindBlock(block, stream.get());
  return block;
}

bool StreamIsUnbuffered(Stream* s) {
  DetermineMode(s);
  return s->buffer_mode == guest::k_IONBF;
}

int StreamFlushLocked(Stream* s) {
  if (s->mode != Stream::Mode::kWriting || s->wlen == 0) return 0;
  const std::size_t written = WriteAll(s, s->buffer, s->wlen);
  if (written < s->wlen) {
    std::memmove(s->buffer, s->buffer + written, s->wlen - written);
    s->wlen -= written;
    return guest::kEOF;
  }
  s->wlen = 0;
  return 0;
}

std::size_t StreamReadLocked(Stream* s, void* out, std::size_t count) {
  if (count == 0) return 0;
  if (!BeginRead(s)) return 0;
  auto* buffer = static_cast<unsigned char*>(out);
  std::size_t total = 0;
  while (total < count && !s->pushback.empty()) {
    buffer[total++] = s->pushback.back();
    s->pushback.pop_back();
  }
  EnsureBuffer(s);
  while (total < count) {
    const std::size_t available = s->rend - s->rpos;
    if (available > 0) {
      const std::size_t take = std::min(available, count - total);
      std::memcpy(buffer + total, s->buffer + s->rpos, take);
      s->rpos += take;
      total += take;
      continue;
    }
    if (s->eof) break;
    if (count - total >= s->buffer_size) {
      FlushInteractiveOutput(s);
      const std::int64_t got = s->backend->Read(buffer + total, count - total);
      if (got > 0) {
        total += static_cast<std::size_t>(got);
        continue;
      }
      if (got == 0) {
        s->eof = true;
      } else {
        s->error = true;
      }
      break;
    }
    if (!Refill(s)) break;
  }
  return total;
}

std::size_t StreamWriteLocked(Stream* s, const void* data, std::size_t count) {
  if (count == 0) return 0;
  if (!BeginWrite(s)) return 0;
  EnsureBuffer(s);
  const auto* bytes = static_cast<const unsigned char*>(data);
  if (s->buffer_mode == guest::k_IONBF) return WriteAll(s, bytes, count);
  std::size_t copied = 0;
  while (copied < count) {
    if (s->wlen == 0 && count - copied >= s->buffer_size && s->buffer_mode == guest::k_IOFBF) {
      return copied + WriteAll(s, bytes + copied, count - copied);
    }
    const std::size_t take = std::min(s->buffer_size - s->wlen, count - copied);
    std::memcpy(s->buffer + s->wlen, bytes + copied, take);
    s->wlen += take;
    copied += take;
    if (s->wlen == s->buffer_size && StreamFlushLocked(s) != 0) {
      return copied > s->wlen ? copied - s->wlen : 0;
    }
  }
  if (s->buffer_mode == guest::k_IOLBF && std::memchr(bytes, '\n', count) != nullptr && StreamFlushLocked(s) != 0) {
    return copied > s->wlen ? copied - s->wlen : 0;
  }
  return copied;
}

int StreamGetcLocked(Stream* s) {
  if (s->mode == Stream::Mode::kReading && s->pushback.empty() && s->rpos < s->rend) return s->buffer[s->rpos++];
  if (!BeginRead(s)) return guest::kEOF;
  if (!s->pushback.empty()) {
    const int c = s->pushback.back();
    s->pushback.pop_back();
    return c;
  }
  if (s->rpos >= s->rend && !Refill(s)) return guest::kEOF;
  return s->buffer[s->rpos++];
}

int StreamPutcLocked(Stream* s, int c) {
  const unsigned char byte = static_cast<unsigned char>(c);
  if (s->mode == Stream::Mode::kWriting && s->buffer_mode == guest::k_IOFBF && s->buffer != nullptr &&
      s->wlen < s->buffer_size) {
    s->buffer[s->wlen++] = byte;
    if (s->wlen == s->buffer_size && StreamFlushLocked(s) != 0) return guest::kEOF;
    return byte;
  }
  return StreamWriteLocked(s, &byte, 1) == 1 ? byte : guest::kEOF;
}

int StreamUngetcLocked(Stream* s, int c) {
  if (c == guest::kEOF) return guest::kEOF;
  if (!s->readable) return guest::kEOF;
  if (s->mode == Stream::Mode::kWriting) {
    if (StreamFlushLocked(s) != 0) return guest::kEOF;
    s->rpos = s->rend = 0;
  }
  s->mode = Stream::Mode::kReading;
  const unsigned char byte = static_cast<unsigned char>(c);
  if (s->pushback.empty() && s->rpos > 0 && s->buffer != nullptr && s->buffer[s->rpos - 1] == byte) {
    --s->rpos;
  } else {
    s->pushback.push_back(byte);
  }
  s->eof = false;
  return byte;
}

bool ParseStdioMode(const char* mode, int* open_flags, bool* readable, bool* writable, bool* append) {
  if (mode == nullptr) return FailWith(guest::kEINVAL, false);
  int access = 0, extra = 0;
  switch (*mode) {
    case 'r':
      access = guest::kO_RDONLY;
      *readable = true;
      *writable = false;
      *append = false;
      break;
    case 'w':
      access = guest::kO_WRONLY;
      extra = guest::kO_CREAT | guest::kO_TRUNC;
      *readable = false;
      *writable = true;
      *append = false;
      break;
    case 'a':
      access = guest::kO_WRONLY;
      extra = guest::kO_CREAT | guest::kO_APPEND;
      *readable = false;
      *writable = true;
      *append = true;
      break;
    default:
      return FailWith(guest::kEINVAL, false);
  }
  for (const char* p = mode + 1; *p != '\0'; ++p) {
    switch (*p) {
      case '+':
        access = guest::kO_RDWR;
        *readable = *writable = true;
        break;
      case 'e': extra |= guest::kO_CLOEXEC; break;
      case 'x':
        if ((extra & guest::kO_CREAT) != 0) extra |= guest::kO_EXCL;
        break;
      default: break;
    }
  }
  *open_flags = access | extra;
  return true;
}

using GFile = guest::File;

ELIXIR_BIONIC_FN(GFile*, fopen)(const char* path, const char* mode) noexcept {
  return Guarded(static_cast<GFile*>(nullptr), [&]() -> GFile* {
    int flags = 0;
    bool readable = false, writable = false, append = false;
    if (!ParseStdioMode(mode, &flags, &readable, &writable, &append)) return nullptr;
    const int fd = OpenAt(guest::kAT_FDCWD, path, flags, 0666);
    if (fd < 0) return nullptr;
    return OpenFdStream(fd, readable, writable, append);
  });
}

ELIXIR_BIONIC_FN(GFile*, fdopen)(int fd, const char* mode) noexcept {
  return Guarded(static_cast<GFile*>(nullptr), [&]() -> GFile* {
    int flags = 0;
    bool readable = false, writable = false, append = false;
    if (!ParseStdioMode(mode, &flags, &readable, &writable, &append)) return nullptr;
    DescriptionPtr d = GetDescription(fd);
    if (d == nullptr) return nullptr;
    const int current = d->status_flags.load();
    const int access = current & guest::kO_ACCMODE;
    if (access != guest::kO_RDWR && access != (flags & guest::kO_ACCMODE)) {
      return FailWith(guest::kEINVAL, static_cast<GFile*>(nullptr));
    }
    if (append && (current & guest::kO_APPEND) == 0) DescSetStatusFlags(*d, current | guest::kO_APPEND);
    if ((flags & guest::kO_CLOEXEC) != 0) SetCloexec(fd, true);
    return NewStream(MakeFdBackend(fd), readable, writable, append);
  });
}

ELIXIR_BIONIC_FN(GFile*, freopen)(const char* path, const char* mode, GFile* file) noexcept {
  return Guarded(static_cast<GFile*>(nullptr), [&]() -> GFile* {
    Stream* s = StreamOfAny(file);
    if (s == nullptr) return nullptr;
    StreamLock lock(s);
    int flags = 0;
    bool readable = false, writable = false, append = false;
    const bool mode_ok = ParseStdioMode(mode, &flags, &readable, &writable, &append);
    const int old_fd = s->backend != nullptr ? s->backend->Fd() : -1;
    std::string target = path != nullptr ? std::string(path) : std::string();
    if (target.empty() && old_fd >= 0) target = FdTargetPath(old_fd);
    if (!s->closed) {
      if (s->writable) StreamFlushLocked(s);
      if (s->backend != nullptr) s->backend->Close();
      s->backend.reset();
    }
    ReleaseBuffer(s);
    s->closed = true;
    s->pushback.clear();
    s->mode = Stream::Mode::kIdle;
    s->rpos = s->rend = s->wlen = 0;
    if (!mode_ok) return nullptr;
    if (target.empty()) return FailWith(guest::kEBADF, static_cast<GFile*>(nullptr));
    int fd = OpenAt(guest::kAT_FDCWD, target.c_str(), flags, 0666);
    if (fd < 0) return nullptr;
    if (old_fd >= 0 && fd != old_fd) {
      DescriptionPtr d = FindDescription(fd);
      if (d != nullptr && InstallDescriptionAt(d, old_fd, (flags & guest::kO_CLOEXEC) != 0) == old_fd) {
        CloseFd(fd);
        fd = old_fd;
      }
    }
    s->backend = MakeFdBackend(fd);
    s->readable = readable;
    s->writable = writable;
    s->append = append;
    s->eof = s->error = false;
    s->closed = false;
    s->orientation = 0;
    s->buffer_mode = -1;
    if (append) s->backend->Seek(0, guest::kSEEK_END);
    return file;
  });
}

ELIXIR_BIONIC_FN(int, fclose)(GFile* file) noexcept {
  return Guarded(guest::kEOF, [&] {
    Stream* s = StreamOf(file);
    if (s == nullptr) return guest::kEOF;
    return CloseStream(s);
  });
}

ELIXIR_BIONIC_FN(int, fflush)(GFile* file) noexcept {
  return Guarded(guest::kEOF, [&] {
    if (file == nullptr) {
      FlushAll(false);
      return 0;
    }
    Stream* s = StreamOf(file);
    if (s == nullptr) return guest::kEOF;
    StreamLock lock(s);
    if (!s->writable) return FailWith(guest::kEBADF, guest::kEOF);
    return StreamFlushLocked(s);
  });
}

ELIXIR_BIONIC_FN(abi::g_size, fread)(void* buffer, abi::g_size size, abi::g_size count, GFile* file) noexcept {
  return Guarded<abi::g_size>(0, [&]() -> abi::g_size {
    Stream* s = StreamOf(file);
    if (s == nullptr) return 0;
    if (size == 0 || count == 0) return 0;
    StreamLock lock(s);
    if (count > static_cast<abi::g_size>(INT64_MAX) / size) {
      s->error = true;
      return FailWith(guest::kEOVERFLOW, static_cast<abi::g_size>(0));
    }
    return StreamReadLocked(s, buffer, size * count) / size;
  });
}

ELIXIR_BIONIC_FN(abi::g_size, fwrite)(const void* buffer, abi::g_size size, abi::g_size count, GFile* file) noexcept {
  return Guarded<abi::g_size>(0, [&]() -> abi::g_size {
    Stream* s = StreamOf(file);
    if (s == nullptr) return 0;
    if (size == 0 || count == 0) return 0;
    StreamLock lock(s);
    if (count > static_cast<abi::g_size>(INT64_MAX) / size) {
      s->error = true;
      return FailWith(guest::kEOVERFLOW, static_cast<abi::g_size>(0));
    }
    return StreamWriteLocked(s, buffer, size * count) / size;
  });
}

ELIXIR_BIONIC_FN(int, fgetc)(GFile* file) noexcept {
  return Guarded(guest::kEOF, [&] {
    Stream* s = StreamOf(file);
    if (s == nullptr) return guest::kEOF;
    StreamLock lock(s);
    return StreamGetcLocked(s);
  });
}
ELIXIR_BIONIC_FN(int, getc_unlocked)(GFile* file) noexcept {
  return Guarded(guest::kEOF, [&] {
    Stream* s = StreamOf(file);
    return s == nullptr ? guest::kEOF : StreamGetcLocked(s);
  });
}
ELIXIR_BIONIC_FN(int, getchar)() noexcept { return elixir_bionic_fgetc(g_stdin); }
ELIXIR_BIONIC_FN(int, getchar_unlocked)() noexcept { return elixir_bionic_getc_unlocked(g_stdin); }

ELIXIR_BIONIC_FN(char*, fgets)(char* buffer, int size, GFile* file) noexcept {
  return Guarded(static_cast<char*>(nullptr), [&]() -> char* {
    Stream* s = StreamOf(file);
    if (s == nullptr) return nullptr;
    StreamLock lock(s);
    return FgetsLocked(buffer, size, s);
  });
}
ELIXIR_BIONIC_FN(char*, __fgets_chk)(char* buffer, int size, GFile* file, abi::g_size buffer_size) noexcept {
  if (size < 0) FortifyFatal("fgets", "buffer size < 0");
  if (static_cast<abi::g_size>(size) > buffer_size) FortifyFatal("fgets", "prevented write past end of buffer");
  return elixir_bionic_fgets(buffer, size, file);
}
ELIXIR_BIONIC_FN(char*, gets)(char* buffer) noexcept {
  return Guarded(static_cast<char*>(nullptr), [&]() -> char* {
    Stream* s = StreamOf(g_stdin);
    if (s == nullptr || buffer == nullptr) return nullptr;
    StreamLock lock(s);
    std::size_t length = 0;
    int c;
    while ((c = StreamGetcLocked(s)) != guest::kEOF && c != '\n') buffer[length++] = static_cast<char>(c);
    if (c == guest::kEOF && length == 0) return nullptr;
    buffer[length] = '\0';
    return buffer;
  });
}

ELIXIR_BIONIC_FN(int, fputc)(int c, GFile* file) noexcept {
  return Guarded(guest::kEOF, [&] {
    Stream* s = StreamOf(file);
    if (s == nullptr) return guest::kEOF;
    StreamLock lock(s);
    return StreamPutcLocked(s, c);
  });
}
ELIXIR_BIONIC_FN(int, putc_unlocked)(int c, GFile* file) noexcept {
  return Guarded(guest::kEOF, [&] {
    Stream* s = StreamOf(file);
    return s == nullptr ? guest::kEOF : StreamPutcLocked(s, c);
  });
}
ELIXIR_BIONIC_FN(int, putchar)(int c) noexcept { return elixir_bionic_fputc(c, g_stdout); }
ELIXIR_BIONIC_FN(int, putchar_unlocked)(int c) noexcept { return elixir_bionic_putc_unlocked(c, g_stdout); }

ELIXIR_BIONIC_FN(int, fputs)(const char* text, GFile* file) noexcept {
  return Guarded(guest::kEOF, [&] {
    Stream* s = StreamOf(file);
    if (s == nullptr) return guest::kEOF;
    if (text == nullptr) return FailWith(guest::kEFAULT, guest::kEOF);
    StreamLock lock(s);
    const std::size_t length = std::strlen(text);
    return StreamWriteLocked(s, text, length) == length ? 0 : guest::kEOF;
  });
}
ELIXIR_BIONIC_FN(int, puts)(const char* text) noexcept {
  return Guarded(guest::kEOF, [&] {
    Stream* s = StreamOf(g_stdout);
    if (s == nullptr) return guest::kEOF;
    if (text == nullptr) return FailWith(guest::kEFAULT, guest::kEOF);
    StreamLock lock(s);
    const std::size_t length = std::strlen(text);
    if (StreamWriteLocked(s, text, length) != length) return guest::kEOF;
    return StreamPutcLocked(s, '\n') == guest::kEOF ? guest::kEOF : 1;
  });
}

ELIXIR_BIONIC_FN(int, ungetc)(int c, GFile* file) noexcept {
  return Guarded(guest::kEOF, [&] {
    Stream* s = StreamOf(file);
    if (s == nullptr) return guest::kEOF;
    StreamLock lock(s);
    return StreamUngetcLocked(s, c);
  });
}

ELIXIR_BIONIC_FN(int, fseeko)(GFile* file, abi::g_off offset, int whence) noexcept {
  return Guarded(-1, [&] {
    Stream* s = StreamOf(file);
    if (s == nullptr) return -1;
    StreamLock lock(s);
    return SeekLocked(s, offset, whence);
  });
}
ELIXIR_BIONIC_FN(abi::g_off, ftello)(GFile* file) noexcept {
  return Guarded<abi::g_off>(-1, [&]() -> abi::g_off {
    Stream* s = StreamOf(file);
    if (s == nullptr) return -1;
    StreamLock lock(s);
    return TellLocked(s);
  });
}
ELIXIR_BIONIC_FN(void, rewind)(GFile* file) noexcept {
  Guarded(0, [&] {
    Stream* s = StreamOf(file);
    if (s == nullptr) return 0;
    StreamLock lock(s);
    SeekLocked(s, 0, guest::kSEEK_SET);
    s->error = false;
    return 0;
  });
}
ELIXIR_BIONIC_FN(int, fgetpos)(GFile* file, std::int64_t* position) noexcept {
  return Guarded(-1, [&] {
    if (position == nullptr) return FailWith(guest::kEINVAL, -1);
    const std::int64_t value = elixir_bionic_ftello(file);
    if (value < 0) return -1;
    *position = value;
    return 0;
  });
}
ELIXIR_BIONIC_FN(int, fsetpos)(GFile* file, const std::int64_t* position) noexcept {
  if (position == nullptr) return FailWith(guest::kEINVAL, -1);
  return elixir_bionic_fseeko(file, *position, guest::kSEEK_SET);
}

ELIXIR_BIONIC_FN(int, feof)(GFile* file) noexcept {
  return Guarded(0, [&] {
    Stream* s = StreamOf(file);
    if (s == nullptr) return 0;
    StreamLock lock(s);
    return s->eof ? 1 : 0;
  });
}
ELIXIR_BIONIC_FN(int, feof_unlocked)(GFile* file) noexcept {
  Stream* s = StreamOf(file);
  return s != nullptr && s->eof ? 1 : 0;
}
ELIXIR_BIONIC_FN(int, ferror)(GFile* file) noexcept {
  return Guarded(0, [&] {
    Stream* s = StreamOf(file);
    if (s == nullptr) return 0;
    StreamLock lock(s);
    return s->error ? 1 : 0;
  });
}
ELIXIR_BIONIC_FN(int, ferror_unlocked)(GFile* file) noexcept {
  Stream* s = StreamOf(file);
  return s != nullptr && s->error ? 1 : 0;
}
ELIXIR_BIONIC_FN(void, clearerr)(GFile* file) noexcept {
  Guarded(0, [&] {
    Stream* s = StreamOf(file);
    if (s == nullptr) return 0;
    StreamLock lock(s);
    s->eof = s->error = false;
    return 0;
  });
}
ELIXIR_BIONIC_FN(void, clearerr_unlocked)(GFile* file) noexcept {
  Stream* s = StreamOf(file);
  if (s != nullptr) s->eof = s->error = false;
}
ELIXIR_BIONIC_FN(int, fileno_unlocked)(GFile* file) noexcept {
  Stream* s = StreamOf(file);
  if (s == nullptr) return -1;
  const int fd = s->backend->Fd();
  return fd >= 0 ? fd : FailWith(guest::kEBADF, -1);
}
ELIXIR_BIONIC_FN(int, fileno)(GFile* file) noexcept {
  return Guarded(-1, [&] {
    Stream* s = StreamOf(file);
    if (s == nullptr) return -1;
    StreamLock lock(s);
    return elixir_bionic_fileno_unlocked(file);
  });
}

ELIXIR_BIONIC_FN(int, setvbuf)(GFile* file, char* buffer, int mode, abi::g_size size) noexcept {
  return Guarded(-1, [&] {
    Stream* s = StreamOf(file);
    if (s == nullptr) return -1;
    StreamLock lock(s);
    return SetvbufLocked(s, buffer, mode, static_cast<std::size_t>(size));
  });
}
ELIXIR_BIONIC_FN(void, setbuf)(GFile* file, char* buffer) noexcept {
  elixir_bionic_setvbuf(file, buffer, buffer != nullptr ? guest::k_IOFBF : guest::k_IONBF, guest::kBUFSIZ);
}
ELIXIR_BIONIC_FN(void, setbuffer)(GFile* file, char* buffer, int size) noexcept {
  elixir_bionic_setvbuf(file, buffer, buffer != nullptr ? guest::k_IOFBF : guest::k_IONBF,
                        static_cast<abi::g_size>(size < 0 ? 0 : size));
}
ELIXIR_BIONIC_FN(int, setlinebuf)(GFile* file) noexcept {
  return elixir_bionic_setvbuf(file, nullptr, guest::k_IOLBF, 0);
}

ELIXIR_BIONIC_FN(void, flockfile)(GFile* file) noexcept {
  Stream* s = StreamOfAny(file);
  if (s != nullptr) EnterCriticalSection(&s->lock);
}
ELIXIR_BIONIC_FN(int, ftrylockfile)(GFile* file) noexcept {
  Stream* s = StreamOfAny(file);
  if (s == nullptr) return -1;
  return TryEnterCriticalSection(&s->lock) ? 0 : -1;
}
ELIXIR_BIONIC_FN(void, funlockfile)(GFile* file) noexcept {
  Stream* s = StreamOfAny(file);
  if (s != nullptr) LeaveCriticalSection(&s->lock);
}

ELIXIR_BIONIC_FN(abi::g_ssize, getdelim)(char** line, abi::g_size* capacity, int delimiter, GFile* file) noexcept {
  return Guarded<abi::g_ssize>(-1, [&]() -> abi::g_ssize {
    Stream* s = StreamOf(file);
    if (s == nullptr) return -1;
    StreamLock lock(s);
    return GetDelim(line, capacity, delimiter, s);
  });
}
ELIXIR_BIONIC_FN(abi::g_ssize, getline)(char** line, abi::g_size* capacity, GFile* file) noexcept {
  return elixir_bionic_getdelim(line, capacity, '\n', file);
}
ELIXIR_BIONIC_FN(char*, fgetln)(GFile* file, abi::g_size* length) noexcept {
  return Guarded(static_cast<char*>(nullptr), [&]() -> char* {
    Stream* s = StreamOf(file);
    if (s == nullptr) return nullptr;
    StreamLock lock(s);
    s->line.clear();
    int c;
    while ((c = StreamGetcLocked(s)) != guest::kEOF) {
      s->line.push_back(static_cast<char>(c));
      if (c == '\n') break;
    }
    if (s->line.empty()) return nullptr;
    if (length != nullptr) *length = s->line.size();
    return s->line.data();
  });
}

ELIXIR_BIONIC_FN(void, perror)(const char* message) noexcept {
  Guarded(0, [&] {
    const int saved = GuestErrno();
    std::string text;
    if (message != nullptr && message[0] != '\0') {
      text = message;
      text += ": ";
    }
    text += text::ErrorMessage(saved);
    text += "\n";
    Stream* s = StreamOf(g_stderr);
    if (s != nullptr) {
      StreamLock lock(s);
      StreamWriteLocked(s, text.data(), text.size());
    }
    SetGuestErrno(saved);
    return 0;
  });
}

ELIXIR_BIONIC_FN(int, fwide)(GFile* file, int mode) noexcept {
  return Guarded(0, [&] {
    Stream* s = StreamOf(file);
    if (s == nullptr) return 0;
    StreamLock lock(s);
    if (mode != 0 && s->orientation == 0) s->orientation = mode > 0 ? 1 : -1;
    return s->orientation;
  });
}

ELIXIR_BIONIC_FN(GFile*, tmpfile)() noexcept {
  return Guarded(static_cast<GFile*>(nullptr), [&]() -> GFile* {
    int fd = OpenAt(guest::kAT_FDCWD, "/data/local/tmp", guest::kO_TMPFILE | guest::kO_RDWR, 0600);
    if (fd < 0) {
      const std::string host = WideToUtf8(StripLongPrefixPath(HostTempDirectory()));
      fd = OpenAt(guest::kAT_FDCWD, host.c_str(), guest::kO_TMPFILE | guest::kO_RDWR, 0600);
    }
    if (fd < 0) return nullptr;
    return OpenFdStream(fd, true, true, false);
  });
}

ELIXIR_BIONIC_FN(char*, ctermid)(char* buffer) noexcept {
  static char storage[] = "/dev/tty";
  if (buffer == nullptr) return storage;
  std::memcpy(buffer, storage, sizeof(storage));
  return buffer;
}

ELIXIR_BIONIC_FN(abi::g_size, __fbufsize)(GFile* file) noexcept {
  Stream* s = StreamOf(file);
  return s != nullptr && s->buffer != nullptr ? s->buffer_size : 0;
}
ELIXIR_BIONIC_FN(abi::g_size, __fpending)(GFile* file) noexcept {
  Stream* s = StreamOf(file);
  return s != nullptr && s->mode == Stream::Mode::kWriting ? s->wlen : 0;
}
ELIXIR_BIONIC_FN(abi::g_size, __freadahead)(GFile* file) noexcept {
  Stream* s = StreamOf(file);
  return s != nullptr && s->mode == Stream::Mode::kReading ? (s->rend - s->rpos) + s->pushback.size() : 0;
}
ELIXIR_BIONIC_FN(int, __flbf)(GFile* file) noexcept {
  Stream* s = StreamOf(file);
  if (s == nullptr) return 0;
  DetermineMode(s);
  return s->buffer_mode == guest::k_IOLBF ? 1 : 0;
}
ELIXIR_BIONIC_FN(int, __freadable)(GFile* file) noexcept {
  Stream* s = StreamOf(file);
  return s != nullptr && s->readable ? 1 : 0;
}
ELIXIR_BIONIC_FN(int, __fwritable)(GFile* file) noexcept {
  Stream* s = StreamOf(file);
  return s != nullptr && s->writable ? 1 : 0;
}
ELIXIR_BIONIC_FN(int, __freading)(GFile* file) noexcept {
  Stream* s = StreamOf(file);
  return s != nullptr && (!s->writable || s->mode == Stream::Mode::kReading) ? 1 : 0;
}
ELIXIR_BIONIC_FN(int, __fwriting)(GFile* file) noexcept {
  Stream* s = StreamOf(file);
  return s != nullptr && (!s->readable || s->mode == Stream::Mode::kWriting) ? 1 : 0;
}
ELIXIR_BIONIC_FN(void, __fpurge)(GFile* file) noexcept {
  Stream* s = StreamOf(file);
  if (s == nullptr) return;
  StreamLock lock(s);
  s->wlen = 0;
  s->rpos = s->rend = 0;
  s->pushback.clear();
  s->mode = Stream::Mode::kIdle;
}
ELIXIR_BIONIC_FN(int, fpurge)(GFile* file) noexcept {
  if (StreamOf(file) == nullptr) return guest::kEOF;
  elixir_bionic___fpurge(file);
  return 0;
}
ELIXIR_BIONIC_FN(void, __fseterr)(GFile* file) noexcept {
  Stream* s = StreamOf(file);
  if (s != nullptr) s->error = true;
}
ELIXIR_BIONIC_FN(int, __fsetlocking)(GFile* file, int type) noexcept {
  Stream* s = StreamOf(file);
  if (s == nullptr) return kFSETLOCKING_INTERNAL;
  const int previous = s->caller_locking ? kFSETLOCKING_BYCALLER : kFSETLOCKING_INTERNAL;
  if (type == kFSETLOCKING_QUERY) return previous;
  if (type != kFSETLOCKING_INTERNAL && type != kFSETLOCKING_BYCALLER) {
    FortifyFatal("__fsetlocking", "bad type passed to __fsetlocking");
  }
  s->caller_locking = type == kFSETLOCKING_BYCALLER;
  return previous;
}
ELIXIR_BIONIC_FN(void, _flushlbf)() noexcept {
  Guarded(0, [] {
    FlushAll(false);
    return 0;
  });
}

ELIXIR_BIONIC_FN(abi::g_size, __fread_chk)(void* buffer, abi::g_size size, abi::g_size count, GFile* file,
                                           abi::g_size buffer_size) noexcept {
  if (size != 0 && count > static_cast<abi::g_size>(-1) / size) FortifyFatal("fread", "size * count overflow");
  if (size * count > buffer_size) FortifyFatal("fread", "prevented write past end of buffer");
  return elixir_bionic_fread(buffer, size, count, file);
}
ELIXIR_BIONIC_FN(abi::g_size, __fwrite_chk)(const void* buffer, abi::g_size size, abi::g_size count, GFile* file,
                                            abi::g_size buffer_size) noexcept {
  if (size != 0 && count > static_cast<abi::g_size>(-1) / size) FortifyFatal("fwrite", "size * count overflow");
  if (size * count > buffer_size) FortifyFatal("fwrite", "prevented read past end of buffer");
  return elixir_bionic_fwrite(buffer, size, count, file);
}

void RegisterStdioCoreExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  table.Add(kLibc, "__sF", static_cast<void*>(g_sF));
  table.Add(kLibc, "stdin", static_cast<void*>(&g_stdin));
  table.Add(kLibc, "stdout", static_cast<void*>(&g_stdout));
  table.Add(kLibc, "stderr", static_cast<void*>(&g_stderr));
  add("fopen", &elixir_bionic_fopen);
  add("fopen64", &elixir_bionic_fopen);
  add("fdopen", &elixir_bionic_fdopen);
  add("freopen", &elixir_bionic_freopen);
  add("freopen64", &elixir_bionic_freopen);
  add("fclose", &elixir_bionic_fclose);
  add("fflush", &elixir_bionic_fflush);
  add("fread", &elixir_bionic_fread);
  add("fwrite", &elixir_bionic_fwrite);
  add("fgetc", &elixir_bionic_fgetc);
  add("getc", &elixir_bionic_fgetc);
  add("getc_unlocked", &elixir_bionic_getc_unlocked);
  add("getchar", &elixir_bionic_getchar);
  add("getchar_unlocked", &elixir_bionic_getchar_unlocked);
  add("fgets", &elixir_bionic_fgets);
  add("__fgets_chk", &elixir_bionic___fgets_chk);
  add("gets", &elixir_bionic_gets);
  add("fputc", &elixir_bionic_fputc);
  add("putc", &elixir_bionic_fputc);
  add("putc_unlocked", &elixir_bionic_putc_unlocked);
  add("putchar", &elixir_bionic_putchar);
  add("putchar_unlocked", &elixir_bionic_putchar_unlocked);
  add("fputs", &elixir_bionic_fputs);
  add("puts", &elixir_bionic_puts);
  add("ungetc", &elixir_bionic_ungetc);
  add("fseek", &elixir_bionic_fseeko);
  add("fseeko", &elixir_bionic_fseeko);
  add("fseeko64", &elixir_bionic_fseeko);
  add("ftell", &elixir_bionic_ftello);
  add("ftello", &elixir_bionic_ftello);
  add("ftello64", &elixir_bionic_ftello);
  add("rewind", &elixir_bionic_rewind);
  add("fgetpos", &elixir_bionic_fgetpos);
  add("fsetpos", &elixir_bionic_fsetpos);
  add("fgetpos64", &elixir_bionic_fgetpos);
  add("fsetpos64", &elixir_bionic_fsetpos);
  add("feof", &elixir_bionic_feof);
  add("feof_unlocked", &elixir_bionic_feof_unlocked);
  add("ferror", &elixir_bionic_ferror);
  add("ferror_unlocked", &elixir_bionic_ferror_unlocked);
  add("clearerr", &elixir_bionic_clearerr);
  add("clearerr_unlocked", &elixir_bionic_clearerr_unlocked);
  add("fileno", &elixir_bionic_fileno);
  add("fileno_unlocked", &elixir_bionic_fileno_unlocked);
  add("setvbuf", &elixir_bionic_setvbuf);
  add("setbuf", &elixir_bionic_setbuf);
  add("setbuffer", &elixir_bionic_setbuffer);
  add("setlinebuf", &elixir_bionic_setlinebuf);
  add("flockfile", &elixir_bionic_flockfile);
  add("ftrylockfile", &elixir_bionic_ftrylockfile);
  add("funlockfile", &elixir_bionic_funlockfile);
  add("getdelim", &elixir_bionic_getdelim);
  add("getline", &elixir_bionic_getline);
  add("fgetln", &elixir_bionic_fgetln);
  add("perror", &elixir_bionic_perror);
  add("fwide", &elixir_bionic_fwide);
  add("tmpfile", &elixir_bionic_tmpfile);
  add("tmpfile64", &elixir_bionic_tmpfile);
  add("ctermid", &elixir_bionic_ctermid);
  add("__fbufsize", &elixir_bionic___fbufsize);
  add("__fpending", &elixir_bionic___fpending);
  add("__freadahead", &elixir_bionic___freadahead);
  add("__flbf", &elixir_bionic___flbf);
  add("__freadable", &elixir_bionic___freadable);
  add("__fwritable", &elixir_bionic___fwritable);
  add("__freading", &elixir_bionic___freading);
  add("__fwriting", &elixir_bionic___fwriting);
  add("__fpurge", &elixir_bionic___fpurge);
  add("fpurge", &elixir_bionic_fpurge);
  add("__fseterr", &elixir_bionic___fseterr);
  add("__fsetlocking", &elixir_bionic___fsetlocking);
  add("_flushlbf", &elixir_bionic__flushlbf);
  add("__fread_chk", &elixir_bionic___fread_chk);
  add("__fwrite_chk", &elixir_bionic___fwrite_chk);
}

}

namespace elixir::bionic::stdio {

void FlushAllStreams() noexcept {
  try {
    files::FlushAll(true);
  } catch (...) {
  }
}

}

