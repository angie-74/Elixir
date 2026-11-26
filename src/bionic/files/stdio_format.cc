// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include <windows.h>

#include <string>

#include "abi/variadic_thunk.h"
#include "bionic/files/stdio_internal.h"
#include "bionic/format.h"
#include "bionic/text.h"

namespace elixir::bionic::files {
namespace {

using abi::SysvVaList;
using GFile = guest::File;

class StreamSink final : public format::Sink {
 public:
  StreamSink(Stream* stream, std::string* staging) : stream_(stream), staging_(staging) {}
  bool Write(const char* data, std::size_t length) override {
    if (staging_ != nullptr) {
      staging_->append(data, length);
      return true;
    }
    return StreamWriteLocked(stream_, data, length) == length;
  }

 private:
  Stream* stream_;
  std::string* staging_;
};

class StreamWideSink final : public format::WideSink {
 public:
  StreamWideSink(Stream* stream, std::string* staging) : stream_(stream), staging_(staging) {}
  bool Write(const abi::g_wchar* data, std::size_t length) override {
    for (std::size_t i = 0; i < length; ++i) {
      char bytes[4];
      const std::size_t n = text::EncodeUtf8(data[i], bytes);
      if (n == static_cast<std::size_t>(-1)) {
        stream_->error = true;
        SetGuestErrno(guest::kEILSEQ);
        return false;
      }
      if (staging_ != nullptr) {
        staging_->append(bytes, n);
      } else if (StreamWriteLocked(stream_, bytes, n) != n) {
        return false;
      }
    }
    return true;
  }

 private:
  Stream* stream_;
  std::string* staging_;
};

class StreamSource final : public format::Source {
 public:
  explicit StreamSource(Stream* stream) : stream_(stream) {}
  int Get() override {
    const int c = StreamGetcLocked(stream_);
    return c == guest::kEOF ? -1 : c;
  }
  void Unget(int c) override {
    if (c >= 0) StreamUngetcLocked(stream_, c);
  }

 private:
  Stream* stream_;
};

class StreamWideSource final : public format::WideSource {
 public:
  explicit StreamWideSource(Stream* stream) : stream_(stream) {}
  std::int64_t Get() override {
    const std::uint32_t c = StreamGetwcLocked(stream_);
    return c == kWEOF ? -1 : static_cast<std::int64_t>(c);
  }
  void Unget(std::int64_t c) override {
    if (c >= 0) StreamUngetwcLocked(stream_, static_cast<std::uint32_t>(c));
  }

 private:
  Stream* stream_;
};

class StringSink final : public format::Sink {
 public:
  bool Write(const char* data, std::size_t length) override {
    text.append(data, length);
    return true;
  }
  std::string text;
};

bool CheckWritable(Stream* s) {
  if (s->writable) return true;
  s->error = true;
  SetGuestErrno(guest::kEBADF);
  return false;
}

bool CheckReadable(Stream* s) {
  if (s->readable) return true;
  s->error = true;
  SetGuestErrno(guest::kEBADF);
  return false;
}

int FormatToStream(GFile* file, const char* format, SysvVaList* args) {
  Stream* s = StreamOf(file);
  if (s == nullptr) return -1;
  StreamLock lock(s);
  if (!CheckWritable(s)) return -1;
  if (s->orientation == 0) s->orientation = -1;
  std::string staging;
  const bool stage = StreamIsUnbuffered(s);
  StreamSink sink(s, stage ? &staging : nullptr);
  int result = format::Format(sink, format, args);
  if (stage && result >= 0 && StreamWriteLocked(s, staging.data(), staging.size()) != staging.size()) result = -1;
  return result;
}

int FormatWideToStream(GFile* file, const abi::g_wchar* format, SysvVaList* args) {
  Stream* s = StreamOf(file);
  if (s == nullptr) return -1;
  StreamLock lock(s);
  if (!CheckWritable(s)) return -1;
  if (s->orientation == 0) s->orientation = 1;
  std::string staging;
  const bool stage = StreamIsUnbuffered(s);
  StreamWideSink sink(s, stage ? &staging : nullptr);
  int result = format::FormatWide(sink, format, args);
  if (stage && result >= 0 && StreamWriteLocked(s, staging.data(), staging.size()) != staging.size()) result = -1;
  return result;
}

int ScanStream(GFile* file, const char* format, SysvVaList* args) {
  Stream* s = StreamOf(file);
  if (s == nullptr) return guest::kEOF;
  StreamLock lock(s);
  if (!CheckReadable(s)) return guest::kEOF;
  StreamSource source(s);
  return format::Scan(source, format, args);
}

int ScanWideStream(GFile* file, const abi::g_wchar* format, SysvVaList* args) {
  Stream* s = StreamOf(file);
  if (s == nullptr) return guest::kEOF;
  StreamLock lock(s);
  if (!CheckReadable(s)) return guest::kEOF;
  StreamWideSource source(s);
  return format::ScanWide(source, format, args);
}

int FormatToFd(int fd, const char* format, SysvVaList* args) {
  DescriptionPtr d = GetDescription(fd);
  if (d == nullptr) return -1;
  StringSink sink;
  const int result = format::Format(sink, format, args);
  if (result < 0) return result;
  std::size_t done = 0;
  while (done < sink.text.size()) {
    const std::int64_t written = DescWrite(*d, sink.text.data() + done, sink.text.size() - done);
    if (written <= 0) return -1;
    done += static_cast<std::size_t>(written);
  }
  return result;
}

}
}

using elixir::abi::SysvVaList;
using elixir::bionic::files::Guarded;

ELIXIR_BIONIC_FN(int, vfprintf)(elixir::bionic::guest::File* file, const char* format, SysvVaList* args) noexcept {
  return Guarded(-1, [&] { return elixir::bionic::files::FormatToStream(file, format, args); });
}
ELIXIR_BIONIC_FN(int, vprintf)(const char* format, SysvVaList* args) noexcept {
  return Guarded(-1, [&] {
    return elixir::bionic::files::FormatToStream(elixir::bionic::files::StdVariable(1), format, args);
  });
}
ELIXIR_BIONIC_FN(int, vdprintf)(int fd, const char* format, SysvVaList* args) noexcept {
  return Guarded(-1, [&] { return elixir::bionic::files::FormatToFd(fd, format, args); });
}
ELIXIR_BIONIC_FN(int, vfscanf)(elixir::bionic::guest::File* file, const char* format, SysvVaList* args) noexcept {
  return Guarded(-1, [&] { return elixir::bionic::files::ScanStream(file, format, args); });
}
ELIXIR_BIONIC_FN(int, vscanf)(const char* format, SysvVaList* args) noexcept {
  return Guarded(-1, [&] {
    return elixir::bionic::files::ScanStream(elixir::bionic::files::StdVariable(0), format, args);
  });
}
ELIXIR_BIONIC_FN(int, vfwprintf)(elixir::bionic::guest::File* file, const elixir::abi::g_wchar* format,
                                 SysvVaList* args) noexcept {
  return Guarded(-1, [&] { return elixir::bionic::files::FormatWideToStream(file, format, args); });
}
ELIXIR_BIONIC_FN(int, vwprintf)(const elixir::abi::g_wchar* format, SysvVaList* args) noexcept {
  return Guarded(-1, [&] {
    return elixir::bionic::files::FormatWideToStream(elixir::bionic::files::StdVariable(1), format, args);
  });
}
ELIXIR_BIONIC_FN(int, vfwscanf)(elixir::bionic::guest::File* file, const elixir::abi::g_wchar* format,
                                SysvVaList* args) noexcept {
  return Guarded(-1, [&] { return elixir::bionic::files::ScanWideStream(file, format, args); });
}
ELIXIR_BIONIC_FN(int, vwscanf)(const elixir::abi::g_wchar* format, SysvVaList* args) noexcept {
  return Guarded(-1, [&] {
    return elixir::bionic::files::ScanWideStream(elixir::bionic::files::StdVariable(0), format, args);
  });
}

ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_printf, elixir_bionic_vprintf, 1)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_fprintf, elixir_bionic_vfprintf, 2)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_dprintf, elixir_bionic_vdprintf, 2)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_scanf, elixir_bionic_vscanf, 1)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_fscanf, elixir_bionic_vfscanf, 2)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_wprintf, elixir_bionic_vwprintf, 1)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_fwprintf, elixir_bionic_vfwprintf, 2)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_wscanf, elixir_bionic_vwscanf, 1)
ELIXIR_SYSV_VARIADIC_THUNK(elixir_bionic_fwscanf, elixir_bionic_vfwscanf, 2)

namespace elixir::bionic::files {

void RegisterStdioFormatExports(ExportTable& table) {
  auto add = [&](const char* name, void* fn) { table.Add(kLibc, name, fn); };
  add("vfprintf", reinterpret_cast<void*>(&elixir_bionic_vfprintf));
  add("vprintf", reinterpret_cast<void*>(&elixir_bionic_vprintf));
  add("vdprintf", reinterpret_cast<void*>(&elixir_bionic_vdprintf));
  add("vfscanf", reinterpret_cast<void*>(&elixir_bionic_vfscanf));
  add("vscanf", reinterpret_cast<void*>(&elixir_bionic_vscanf));
  add("vfwprintf", reinterpret_cast<void*>(&elixir_bionic_vfwprintf));
  add("vwprintf", reinterpret_cast<void*>(&elixir_bionic_vwprintf));
  add("vfwscanf", reinterpret_cast<void*>(&elixir_bionic_vfwscanf));
  add("vwscanf", reinterpret_cast<void*>(&elixir_bionic_vwscanf));
  add("printf", reinterpret_cast<void*>(&elixir_bionic_printf));
  add("fprintf", reinterpret_cast<void*>(&elixir_bionic_fprintf));
  add("dprintf", reinterpret_cast<void*>(&elixir_bionic_dprintf));
  add("scanf", reinterpret_cast<void*>(&elixir_bionic_scanf));
  add("fscanf", reinterpret_cast<void*>(&elixir_bionic_fscanf));
  add("wprintf", reinterpret_cast<void*>(&elixir_bionic_wprintf));
  add("fwprintf", reinterpret_cast<void*>(&elixir_bionic_fwprintf));
  add("wscanf", reinterpret_cast<void*>(&elixir_bionic_wscanf));
  add("fwscanf", reinterpret_cast<void*>(&elixir_bionic_fwscanf));
}

}
