// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <algorithm>
#include <cstring>

#include "bionic/files/stdio_internal.h"
#include "bionic/memory.h"
#include "bionic/text.h"

namespace elixir::bionic::files {
namespace {

std::int64_t Fail(int error) {
  SetGuestErrno(error);
  return -1;
}

class FmemBackend final : public StreamBackend {
 public:
  FmemBackend(char* buffer, std::size_t capacity, char* owned)
      : buffer_(buffer), owned_(owned), capacity_(capacity) {}
  ~FmemBackend() override { delete[] owned_; }

  void Configure(char kind) {
    if (kind == 'a') {
      size_ = strnlen(buffer_, capacity_);
      offset_ = size_;
      append_ = true;
    } else if (kind == 'r') {
      size_ = capacity_;
      offset_ = 0;
    } else {
      size_ = 0;
      offset_ = 0;
      if (capacity_ > 0) buffer_[0] = '\0';
    }
  }

  std::int64_t Read(void* out, std::size_t count) override {
    if (offset_ >= size_) return 0;
    const std::size_t n = std::min(count, size_ - offset_);
    std::memmove(out, buffer_ + offset_, n);
    offset_ += n;
    return static_cast<std::int64_t>(n);
  }

  std::int64_t Write(const void* data, std::size_t count) override {
    const auto* bytes = static_cast<const char*>(data);
    if (count == 0) return 0;
    if (append_) offset_ = size_;
    const std::size_t space_for_nul = count > 0 && bytes[count - 1] != '\0' ? 1 : 0;
    std::int64_t n = static_cast<std::int64_t>(count);
    const std::int64_t room =
        static_cast<std::int64_t>(capacity_) - static_cast<std::int64_t>(offset_) - static_cast<std::int64_t>(space_for_nul);
    if (n > room) {
      n = room;
      if (n <= 0) return Fail(guest::kENOSPC);
    }
    std::memmove(buffer_ + offset_, bytes, static_cast<std::size_t>(n));
    offset_ += static_cast<std::size_t>(n);
    if (offset_ >= size_) {
      if (bytes[n - 1] != '\0' && offset_ < capacity_) buffer_[offset_] = '\0';
      size_ = offset_;
    }
    return n;
  }

  std::int64_t Seek(std::int64_t offset, int whence) override {
    std::int64_t target = -1;
    if (whence == guest::kSEEK_SET) {
      target = offset;
    } else if (whence == guest::kSEEK_CUR) {
      target = static_cast<std::int64_t>(offset_) + offset;
    } else if (whence == guest::kSEEK_END) {
      if (offset > 0) return Fail(guest::kEINVAL);
      target = static_cast<std::int64_t>(size_) + offset;
    }
    if (target < 0 || target > static_cast<std::int64_t>(capacity_)) return Fail(guest::kEINVAL);
    offset_ = static_cast<std::size_t>(target);
    return target;
  }

  int Close() override { return 0; }

 private:
  char* buffer_;
  char* owned_;
  std::size_t capacity_;
  std::size_t size_ = 0;
  std::size_t offset_ = 0;
  bool append_ = false;
};

class MemstreamBackend final : public StreamBackend {
 public:
  MemstreamBackend(char** pbuf, abi::g_size* psize) : pbuf_(pbuf), psize_(psize) {}

  bool Init() {
    allocated_ = 64;
    string_ = static_cast<char*>(GuestCalloc(1, allocated_));
    if (string_ == nullptr) return false;
    *pbuf_ = string_;
    *psize_ = 0;
    return true;
  }

  std::int64_t Read(void*, std::size_t) override { return Fail(guest::kEBADF); }

  std::int64_t Write(const void* data, std::size_t count) override {
    const std::size_t end = position_ + count;
    if (end < position_) return Fail(guest::kEOVERFLOW);
    if (end + 1 > allocated_) {
      std::size_t next = std::max(allocated_ * 8 / 5, end + 1);
      auto* grown = static_cast<char*>(GuestRealloc(string_, next));
      if (grown == nullptr) return Fail(guest::kENOMEM);
      std::memset(grown + allocated_, 0, next - allocated_);
      string_ = grown;
      allocated_ = next;
      *pbuf_ = string_;
    }
    std::memcpy(string_ + position_, data, count);
    position_ = end;
    if (position_ > length_) {
      length_ = position_;
      string_[length_] = '\0';
    }
    *psize_ = position_;
    return static_cast<std::int64_t>(count);
  }

  std::int64_t Seek(std::int64_t offset, int whence) override {
    std::int64_t base = 0;
    if (whence == guest::kSEEK_CUR) base = static_cast<std::int64_t>(position_);
    else if (whence == guest::kSEEK_END) base = static_cast<std::int64_t>(length_);
    else if (whence != guest::kSEEK_SET) return Fail(guest::kEINVAL);
    if ((offset > 0 && offset > INT64_MAX - base) || base + offset < 0) return Fail(guest::kEINVAL);
    position_ = static_cast<std::size_t>(base + offset);
    *psize_ = std::min(position_, length_);
    return static_cast<std::int64_t>(position_);
  }

  int Close() override {
    *pbuf_ = string_;
    *psize_ = std::min(position_, length_);
    return 0;
  }

 private:
  char** pbuf_;
  abi::g_size* psize_;
  char* string_ = nullptr;
  std::size_t allocated_ = 0;
  std::size_t position_ = 0;
  std::size_t length_ = 0;
};

class WideMemstreamBackend final : public StreamBackend {
 public:
  WideMemstreamBackend(abi::g_wchar** pbuf, abi::g_size* psize) : pbuf_(pbuf), psize_(psize) {}

  bool Init() {
    allocated_ = 64;
    string_ = static_cast<abi::g_wchar*>(GuestCalloc(allocated_, sizeof(abi::g_wchar)));
    if (string_ == nullptr) return false;
    *pbuf_ = string_;
    *psize_ = 0;
    return true;
  }

  std::int64_t Read(void*, std::size_t) override { return Fail(guest::kEBADF); }

  std::int64_t Write(const void* data, std::size_t count) override {
    pending_.append(static_cast<const char*>(data), count);
    std::size_t used = 0;
    while (used < pending_.size()) {
      abi::g_wchar c = 0;
      const std::size_t n = text::DecodeUtf8(pending_.data() + used, pending_.size() - used, &c);
      if (n == static_cast<std::size_t>(-2)) break;
      if (n == static_cast<std::size_t>(-1)) {
        pending_.clear();
        return Fail(guest::kEILSEQ);
      }
      if (!Put(c)) return Fail(guest::kENOMEM);
      used += n == 0 ? 1 : n;
    }
    pending_.erase(0, used);
    *psize_ = position_;
    return static_cast<std::int64_t>(count);
  }

  std::int64_t Seek(std::int64_t offset, int whence) override {
    pending_.clear();
    std::int64_t base = 0;
    if (whence == guest::kSEEK_CUR) base = static_cast<std::int64_t>(position_);
    else if (whence == guest::kSEEK_END) base = static_cast<std::int64_t>(length_);
    else if (whence != guest::kSEEK_SET) return Fail(guest::kEINVAL);
    if ((offset > 0 && offset > INT64_MAX - base) || base + offset < 0) return Fail(guest::kEINVAL);
    position_ = static_cast<std::size_t>(base + offset);
    *psize_ = std::min(position_, length_);
    return static_cast<std::int64_t>(position_);
  }

  int Close() override {
    *pbuf_ = string_;
    *psize_ = std::min(position_, length_);
    return 0;
  }

 private:
  bool Put(abi::g_wchar c) {
    if (position_ + 2 > allocated_) {
      const std::size_t next = std::max(allocated_ * 8 / 5, position_ + 2);
      auto* grown = static_cast<abi::g_wchar*>(GuestRealloc(string_, next * sizeof(abi::g_wchar)));
      if (grown == nullptr) return false;
      std::memset(grown + allocated_, 0, (next - allocated_) * sizeof(abi::g_wchar));
      string_ = grown;
      allocated_ = next;
      *pbuf_ = string_;
    }
    string_[position_++] = c;
    if (position_ > length_) {
      length_ = position_;
      string_[length_] = 0;
    }
    return true;
  }

  abi::g_wchar** pbuf_;
  abi::g_size* psize_;
  abi::g_wchar* string_ = nullptr;
  std::size_t allocated_ = 0;
  std::size_t position_ = 0;
  std::size_t length_ = 0;
  std::string pending_;
};

using CookieRead = abi::SysvFn<int(void*, char*, int)>;
using CookieWrite = abi::SysvFn<int(void*, const char*, int)>;
using CookieSeek = abi::SysvFn<std::int64_t(void*, std::int64_t, int)>;
using CookieClose = abi::SysvFn<int(void*)>;

class CookieBackend final : public StreamBackend {
 public:
  CookieBackend(void* cookie, CookieRead read, CookieWrite write, CookieSeek seek, CookieClose close)
      : cookie_(cookie), read_(read), write_(write), seek_(seek), close_(close) {}

  std::int64_t Read(void* buffer, std::size_t count) override {
    if (read_ == nullptr) return Fail(guest::kEBADF);
    const int n = read_(cookie_, static_cast<char*>(buffer), static_cast<int>(std::min<std::size_t>(count, 0x7fffffff)));
    return n < 0 ? -1 : n;
  }
  std::int64_t Write(const void* buffer, std::size_t count) override {
    if (write_ == nullptr) return Fail(guest::kEBADF);
    const int n =
        write_(cookie_, static_cast<const char*>(buffer), static_cast<int>(std::min<std::size_t>(count, 0x7fffffff)));
    return n <= 0 ? -1 : n;
  }
  std::int64_t Seek(std::int64_t offset, int whence) override {
    if (seek_ == nullptr) return Fail(guest::kESPIPE);
    const std::int64_t result = seek_(cookie_, offset, whence);
    return result < 0 ? -1 : result;
  }
  int Close() override { return close_ != nullptr ? close_(cookie_) : 0; }

 private:
  void* cookie_;
  CookieRead read_;
  CookieWrite write_;
  CookieSeek seek_;
  CookieClose close_;
};

}

ELIXIR_BIONIC_FN(guest::File*, fmemopen)(void* buffer, abi::g_size capacity, const char* mode) noexcept {
  return Guarded(static_cast<guest::File*>(nullptr), [&]() -> guest::File* {
    int flags = 0;
    bool readable = false, writable = false, append = false;
    if (!ParseStdioMode(mode, &flags, &readable, &writable, &append)) return nullptr;
    char* owned = nullptr;
    char* target = static_cast<char*>(buffer);
    if (target == nullptr) {
      owned = new (std::nothrow) char[capacity > 0 ? capacity : 1]();
      if (owned == nullptr) return FailWith(guest::kENOMEM, static_cast<guest::File*>(nullptr));
      target = owned;
    }
    auto backend = std::make_unique<FmemBackend>(target, static_cast<std::size_t>(capacity), owned);
    backend->Configure(mode[0]);
    return NewStream(std::move(backend), readable, writable, false);
  });
}

ELIXIR_BIONIC_FN(guest::File*, open_memstream)(char** pbuf, abi::g_size* psize) noexcept {
  return Guarded(static_cast<guest::File*>(nullptr), [&]() -> guest::File* {
    if (pbuf == nullptr || psize == nullptr) return FailWith(guest::kEINVAL, static_cast<guest::File*>(nullptr));
    auto backend = std::make_unique<MemstreamBackend>(pbuf, psize);
    if (!backend->Init()) return FailWith(guest::kENOMEM, static_cast<guest::File*>(nullptr));
    return NewStream(std::move(backend), false, true, false);
  });
}

ELIXIR_BIONIC_FN(guest::File*, open_wmemstream)(abi::g_wchar** pbuf, abi::g_size* psize) noexcept {
  return Guarded(static_cast<guest::File*>(nullptr), [&]() -> guest::File* {
    if (pbuf == nullptr || psize == nullptr) return FailWith(guest::kEINVAL, static_cast<guest::File*>(nullptr));
    auto backend = std::make_unique<WideMemstreamBackend>(pbuf, psize);
    if (!backend->Init()) return FailWith(guest::kENOMEM, static_cast<guest::File*>(nullptr));
    guest::File* file = NewStream(std::move(backend), false, true, false);
    if (file != nullptr) StreamOf(file)->orientation = 1;
    return file;
  });
}

ELIXIR_BIONIC_FN(guest::File*, funopen)(void* cookie, CookieRead read, CookieWrite write, CookieSeek seek,
                                        CookieClose close) noexcept {
  return Guarded(static_cast<guest::File*>(nullptr), [&]() -> guest::File* {
    if (read == nullptr && write == nullptr) return FailWith(guest::kEINVAL, static_cast<guest::File*>(nullptr));
    return NewStream(std::make_unique<CookieBackend>(cookie, read, write, seek, close), read != nullptr,
                     write != nullptr, false);
  });
}

void RegisterStdioMemExports(ExportTable& table) {
  auto add = [&](const char* name, auto* fn) { table.Add(kLibc, name, reinterpret_cast<void*>(fn)); };
  add("fmemopen", &elixir_bionic_fmemopen);
  add("open_memstream", &elixir_bionic_open_memstream);
  add("open_wmemstream", &elixir_bionic_open_wmemstream);
  add("funopen", &elixir_bionic_funopen);
  add("funopen64", &elixir_bionic_funopen);
}

}
