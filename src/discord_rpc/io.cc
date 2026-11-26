// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#include "discord_rpc/io.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstring>

#include "discord_rpc/exceptions.h"
#include "discord_rpc/json.h"

namespace discord_rpc::io {
namespace {

using helper::Args;

std::string Win32Message(DWORD error) {
  switch (error) {
    case ERROR_BROKEN_PIPE: return "Pipe is broken.";
    case ERROR_NO_DATA: return "The pipe is being closed.";
    case ERROR_PIPE_NOT_CONNECTED: return "No process is on the other end of the pipe.";
    case ERROR_SEM_TIMEOUT: return "The semaphore timeout period has expired.";
    case ERROR_ACCESS_DENIED: return "Access to the path is denied.";
    case ERROR_OPERATION_ABORTED: return "The I/O operation has been aborted.";
    default: return "Win32 error " + std::to_string(error) + ".";
  }
}

bool IsEndOfPipe(DWORD error) {
  return error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED || error == ERROR_NO_DATA;
}

int Min(int a, std::uint32_t b) {
  if (b >= static_cast<std::uint32_t>(a)) return a;
  return static_cast<int>(b);
}

bool TryReadUInt32(MemoryStream* stream, std::uint32_t* value) {
  std::uint8_t bytes[4];
  const int cnt = stream->Read(bytes, 0, 4);

  if (cnt != 4) {
    *value = 0;
    return false;
  }

  std::memcpy(value, bytes, 4);
  return true;
}

}

std::string ToString(Opcode opcode) {
  switch (opcode) {
    case Opcode::Handshake: return "Handshake";
    case Opcode::Frame: return "Frame";
    case Opcode::Close: return "Close";
    case Opcode::Ping: return "Ping";
    case Opcode::Pong: return "Pong";
  }
  return std::to_string(static_cast<std::uint32_t>(opcode));
}

std::string Handshake::ToJson() const {
  json::Writer writer;
  writer.StartObject();
  writer.Name("v");
  writer.Int(Version);
  writer.Name("client_id");
  writer.TextOrNull(ClientID);
  writer.EndObject();
  return writer.Take();
}

int MemoryStream::Read(std::uint8_t* buffer, int offset, int count) {
  if (count <= 0) return 0;
  const std::size_t n = std::min(size_ - position_, static_cast<std::size_t>(count));
  if (n > 0) std::memcpy(buffer + offset, data_ + position_, n);
  position_ += n;
  return static_cast<int>(n);
}

PipeFrame::PipeFrame(io::Opcode opcode, const std::string& json) : Opcode(opcode) {
  SetObject(json);
}

std::uint32_t PipeFrame::Length() const {
  if (Data == nullptr) throw NullReferenceException();
  return static_cast<std::uint32_t>(Data->size());
}

std::string PipeFrame::Message() const {
  if (Data == nullptr) throw ArgumentNullException("bytes");
  return helper::WellFormed(std::string_view(reinterpret_cast<const char*>(Data->data()), Data->size()));
}

void PipeFrame::SetMessage(std::string_view str) {
  const std::string text = helper::WellFormed(str);
  Data = std::make_shared<std::vector<std::uint8_t>>(text.begin(), text.end());
}

void PipeFrame::SetObject(io::Opcode opcode, const std::string& json) {
  Opcode = opcode;
  SetObject(json);
}

bool PipeFrame::ReadStream(MemoryStream* stream) {
  std::uint32_t op;
  if (!TryReadUInt32(stream, &op)) return false;

  std::uint32_t len;
  if (!TryReadUInt32(stream, &len)) return false;

  std::uint32_t readsRemaining = len;

  std::vector<std::uint8_t> mem;
  const std::uint32_t chunkSize = static_cast<std::uint32_t>(Min(2048, len));
  std::vector<std::uint8_t> buffer(chunkSize);
  int bytesRead;
  while ((bytesRead = stream->Read(buffer.data(), 0, Min(static_cast<int>(buffer.size()), readsRemaining))) > 0) {
    readsRemaining -= chunkSize;
    mem.insert(mem.end(), buffer.begin(), buffer.begin() + bytesRead);
  }

  if (mem.size() != len) return false;

  Opcode = static_cast<io::Opcode>(op);
  Data = std::make_shared<std::vector<std::uint8_t>>(std::move(mem));
  return true;
}

std::vector<std::uint8_t> PipeFrame::ToBytes() const {
  const std::uint32_t op = static_cast<std::uint32_t>(Opcode);
  const std::uint32_t len = Length();

  std::vector<std::uint8_t> buff(8 + Data->size());
  std::memcpy(buff.data(), &op, 4);
  std::memcpy(buff.data() + 4, &len, 4);
  if (!Data->empty()) std::memcpy(buff.data() + 8, Data->data(), Data->size());
  return buff;
}

bool PipeFrame::Equals(const PipeFrame& other) const {
  return Opcode == other.Opcode && Length() == other.Length() && Data == other.Data;
}

std::vector<std::string> PipeLocation::GetPipes(int start_pipe) {
  constexpr int MaximumPipeVariations = 10;
  std::vector<std::string> pipes;
  for (int index = start_pipe; index < start_pipe + MaximumPipeVariations; ++index) {
    pipes.push_back("discord-ipc-" + std::to_string(index));
  }
  return pipes;
}

class ManagedNamedPipeClient::Stream {
 public:
  enum class State { kWaitingToConnect, kConnected, kBroken, kClosed };

  Stream(std::wstring path, HANDLE released) : path_(std::move(path)), released_(released) {
    read_.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    read_started_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    stop_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    write_done_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  }

  ~Stream() {
    if (handle_ != INVALID_HANDLE_VALUE) {
      CancelIoEx(handle_, nullptr);
      if (read_pending_.load() && read_.hEvent != nullptr) WaitForSingleObject(read_.hEvent, INFINITE);
      CloseHandle(handle_);
    }
    for (const HANDLE event : {read_.hEvent, read_started_, stop_, write_done_}) {
      if (event != nullptr) CloseHandle(event);
    }
  }

  Stream(const Stream&) = delete;
  Stream& operator=(const Stream&) = delete;

  bool IsConnected() const { return state_.load() == State::kConnected; }
  const std::uint8_t* buffer() const { return buffer_.data(); }

  void Connect() {
    if (read_.hEvent == nullptr || read_started_ == nullptr || stop_ == nullptr || write_done_ == nullptr) {
      throw IOException("Win32 error " + std::to_string(ERROR_NO_SYSTEM_RESOURCES) + ".");
    }
    do {
      if (!WaitNamedPipeW(path_.c_str(), NMPWAIT_USE_DEFAULT_WAIT)) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) continue;
        if (error == ERROR_SUCCESS) break;
        throw IOException(Win32Message(error));
      }
      const HANDLE handle =
          CreateFileW(path_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                      FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
      if (handle == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        if (error == ERROR_PIPE_BUSY) continue;
        throw IOException(Win32Message(error));
      }
      handle_ = handle;
      state_ = State::kConnected;
      return;
    } while (false);
    throw TimeoutException();
  }

  void BeginRead() {
    switch (state_.load()) {
      case State::kWaitingToConnect: throw InvalidOperationException("Pipe hasn't been connected yet.");
      case State::kClosed: throw ObjectDisposedException("", "Cannot access a closed pipe.");
      default: break;
    }
    if (read_pending_.exchange(true)) return;
    end_of_pipe_ = false;
    ResetEvent(read_.hEvent);
    read_.Internal = 0;
    read_.InternalHigh = 0;
    read_.Offset = 0;
    read_.OffsetHigh = 0;
    if (!ReadFile(handle_, buffer_.data(), static_cast<DWORD>(buffer_.size()), nullptr, &read_)) {
      const DWORD error = GetLastError();
      if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
        state_ = State::kBroken;
        end_of_pipe_ = true;
        SetEvent(read_.hEvent);
      } else if (error != ERROR_IO_PENDING) {
        read_pending_ = false;
        throw IOException(Win32Message(error));
      }
    }
    SetEvent(read_started_);
  }

  bool WaitRead(DWORD* error, DWORD* bytes) {
    const HANDLE started[2] = {stop_, read_started_};
    const DWORD which = WaitForMultipleObjects(2, started, FALSE, INFINITE);
    if (which != WAIT_OBJECT_0 + 1) {
      return false;
    }
    WaitForSingleObject(read_.hEvent, INFINITE);
    *error = 0;
    *bytes = 0;
    if (!end_of_pipe_ && !GetOverlappedResult(handle_, &read_, bytes, FALSE)) {
      *error = GetLastError();
      *bytes = 0;
      if (IsEndOfPipe(*error) || *error == ERROR_MORE_DATA) *error = 0;
    }
    read_pending_ = false;
    return true;
  }

  void Write(const std::vector<std::uint8_t>& bytes) {
    CheckWriteOperations();
    const std::uint8_t* data = bytes.data();
    std::size_t left = bytes.size();
    while (left > 0) {
      OVERLAPPED overlapped{};
      overlapped.hEvent = write_done_;
      ResetEvent(write_done_);
      DWORD written = 0;
      DWORD error = 0;
      if (!WriteFile(handle_, data, static_cast<DWORD>(left), nullptr, &overlapped)) {
        error = GetLastError();
        if (error == ERROR_IO_PENDING) {
          const HANDLE waits[2] = {write_done_, released_};
          if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) != WAIT_OBJECT_0) {
            CancelIoEx(handle_, &overlapped);
            WaitForSingleObject(write_done_, INFINITE);
          }
          error = GetOverlappedResult(handle_, &overlapped, &written, FALSE) ? 0 : GetLastError();
        }
      } else if (!GetOverlappedResult(handle_, &overlapped, &written, FALSE)) {
        error = GetLastError();
      }
      if (error == 0 && written == 0) error = ERROR_BROKEN_PIPE;
      if (error != 0) {
        if (IsEndOfPipe(error)) {
          State connected = State::kConnected;
          state_.compare_exchange_strong(connected, State::kBroken);
        }
        throw IOException(Win32Message(error));
      }
      data += written;
      left -= written;
    }
  }

  void Flush() { CheckWriteOperations(); }

  void Dispose() {
    state_ = State::kClosed;
    if (handle_ != INVALID_HANDLE_VALUE) CancelIoEx(handle_, nullptr);
    if (stop_ != nullptr) SetEvent(stop_);
  }

 private:
  void CheckWriteOperations() const {
    switch (state_.load()) {
      case State::kWaitingToConnect: throw InvalidOperationException("Pipe hasn't been connected yet.");
      case State::kBroken: throw IOException("Pipe is broken.");
      case State::kClosed: throw ObjectDisposedException("", "Cannot access a closed pipe.");
      default: break;
    }
  }

  const std::wstring path_;
  const HANDLE released_;
  HANDLE handle_ = INVALID_HANDLE_VALUE;
  std::atomic<State> state_{State::kWaitingToConnect};

  OVERLAPPED read_{};
  HANDLE read_started_ = nullptr;
  HANDLE stop_ = nullptr;
  HANDLE write_done_ = nullptr;
  std::atomic<bool> read_pending_{false};
  std::atomic<bool> end_of_pipe_{false};
  std::array<std::uint8_t, PipeFrame::MAX_SIZE> buffer_{};
};

ManagedNamedPipeClient::ManagedNamedPipeClient() : ManagedNamedPipeClient(std::wstring()) {}

ManagedNamedPipeClient::ManagedNamedPipeClient(std::wstring pipe_prefix)
    : logger_(std::make_shared<logging::NullLogger>()), pipe_prefix_(std::move(pipe_prefix)) {
  released_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
}

ManagedNamedPipeClient::~ManagedNamedPipeClient() {
  Release();
  try {
    Dispose();
  } catch (const Exception&) {
  }
  JoinReaders(true);
  if (released_ != nullptr) CloseHandle(static_cast<HANDLE>(released_));
}

std::shared_ptr<logging::ILogger> ManagedNamedPipeClient::Logger() const {
  std::lock_guard<std::mutex> lock(logger_lock_);
  return logger_;
}

void ManagedNamedPipeClient::SetLogger(std::shared_ptr<logging::ILogger> logger) {
  std::lock_guard<std::mutex> lock(logger_lock_);
  logger_ = std::move(logger);
}

bool ManagedNamedPipeClient::IsConnected() {
  if (is_closed_) return false;
  std::lock_guard<std::recursive_mutex> lock(l_stream_);
  return stream_ != nullptr && stream_->IsConnected();
}

bool ManagedNamedPipeClient::Connect(int pipe) {
  Logger()->Trace("ManagedNamedPipeClient.Connection({0})", Args(pipe));

  if (is_disposed_) throw ObjectDisposedException("NamedPipe");

  if (pipe > 9) throw ArgumentOutOfRangeException("pipe", "Argument cannot be greater than 9");

  int startPipe = 0;
  if (pipe >= 0) startPipe = pipe;

  JoinReaders(false);

  if (pipe_prefix_.empty()) {
    for (const std::string& pipename : PipeLocation::GetPipes(startPipe)) {
      if (AttemptConnection(pipename, LR"(\\.\pipe\)" + helper::ToWide(pipename))) {
        BeginReadStream();
        return true;
      }
    }
  } else {
    for (int index = startPipe; index < startPipe + 10; ++index) {
      const std::wstring path = pipe_prefix_ + std::to_wstring(index);
      if (AttemptConnection(helper::FromWide(path), path)) {
        BeginReadStream();
        return true;
      }
    }
  }

  return false;
}

bool ManagedNamedPipeClient::AttemptConnection(const std::string& pipename, const std::wstring& path) {
  if (is_disposed_) throw ObjectDisposedException("_stream");

  try {
    std::shared_ptr<Stream> connected;
    {
      std::lock_guard<std::recursive_mutex> lock(l_stream_);
      Logger()->Info("Attempting to connect to '{0}'", Args(pipename));
      if (stream_ != nullptr) stream_->Dispose();
      stream_ = std::make_shared<Stream>(path, static_cast<HANDLE>(released_));

      stream_->Connect();

      Logger()->Trace("Waiting for connection...");
      do {
        Sleep(10);
      } while (!stream_->IsConnected());
      connected = stream_;
    }

    Logger()->Info("Connected to '{0}'", Args(pipename));
    const std::size_t dash = pipename.rfind('-');
    if (dash == std::string::npos) {
      throw ArgumentOutOfRangeException("startIndex", "StartIndex cannot be less than zero.");
    }
    std::int32_t number = 0;
    if (!json::TryParseInt32(std::string_view(pipename).substr(dash), &number)) throw FormatException();
    connected_pipe_ = number;
    is_closed_ = false;

    auto done = std::make_shared<std::atomic<bool>>(false);
    std::thread thread([this, connected, done] {
      DWORD error = 0;
      DWORD bytes = 0;
      try {
        while (connected->WaitRead(&error, &bytes)) EndReadStream(connected, error, bytes);
      } catch (...) {
      }
      done->store(true);
    });
    std::lock_guard<std::mutex> lock(readers_lock_);
    readers_.push_back(Reader{std::move(thread), std::move(done)});
  } catch (const Exception& e) {
    Logger()->Error("Failed connection to {0}. {1}", Args(pipename, e.Message()));
    Close();
  }

  Logger()->Trace("Done. Result: {0}", Args(is_closed_.load()));
  return !is_closed_;
}

void ManagedNamedPipeClient::BeginReadStream() {
  if (is_closed_) return;
  try {
    std::lock_guard<std::recursive_mutex> lock(l_stream_);
    if (stream_ == nullptr || !stream_->IsConnected()) return;

    Logger()->Trace("Begining Read of {0} bytes", Args(static_cast<int>(PipeFrame::MAX_SIZE)));
    stream_->BeginRead();
  } catch (const ObjectDisposedException&) {
    Logger()->Warning("Attempted to start reading from a disposed pipe");
    return;
  } catch (const InvalidOperationException&) {
    Logger()->Warning("Attempted to start reading from a closed pipe");
    return;
  } catch (const Exception& e) {
    Logger()->Error("An exception occured while starting to read a stream: {0}", Args(e.Message()));
  }
}

void ManagedNamedPipeClient::EndReadStream(const std::shared_ptr<Stream>& stream, unsigned long error,
                                           unsigned long bytes_read) {
  Logger()->Trace("Ending Read");
  int bytes = 0;
  {
    std::lock_guard<std::recursive_mutex> lock(l_stream_);
    if (stream_ == nullptr || !stream_->IsConnected()) return;
    if (stream_ != stream) return;

    if (error != 0) {
      Logger()->Warning("Attempted to end reading from a closed pipe");
      return;
    }
    bytes = static_cast<int>(bytes_read);
  }

  Logger()->Trace("Read {0} bytes", Args(bytes));

  if (bytes > 0) {
    MemoryStream memory(stream->buffer(), static_cast<std::size_t>(bytes));
    try {
      PipeFrame frame;
      if (frame.ReadStream(&memory)) {
        Logger()->Trace("Read a frame: {0}", Args(frame.Opcode));

        std::lock_guard<std::recursive_mutex> lock(framequeuelock_);
        framequeue_.push_back(frame);
      } else {
        Logger()->Error("Pipe failed to read from the data received by the stream.");
        Close();
      }
    } catch (const Exception& e) {
      Logger()->Error("A exception has occured while trying to parse the pipe data: {0}", Args(e.Message()));
      Close();
    }
  } else {
    Logger()->Error("Empty frame was read on {0}, aborting.", Args("Microsoft Windows NT"));
    Close();
  }

  if (!is_closed_ && IsConnected()) {
    Logger()->Trace("Starting another read");
    BeginReadStream();
  }
}

bool ManagedNamedPipeClient::ReadFrame(PipeFrame* frame) {
  if (is_disposed_) throw ObjectDisposedException("_stream");

  std::lock_guard<std::recursive_mutex> lock(framequeuelock_);
  if (framequeue_.empty()) {
    *frame = PipeFrame();
    return false;
  }

  *frame = framequeue_.front();
  framequeue_.pop_front();
  return true;
}

bool ManagedNamedPipeClient::WriteFrame(const PipeFrame& frame) {
  if (is_disposed_) throw ObjectDisposedException("_stream");

  if (is_closed_ || !IsConnected()) {
    Logger()->Error("Failed to write frame because the stream is closed");
    return false;
  }

  try {
    const std::vector<std::uint8_t> bytes = frame.ToBytes();
    std::shared_ptr<Stream> stream;
    {
      std::lock_guard<std::recursive_mutex> lock(l_stream_);
      stream = stream_;
    }
    if (stream == nullptr) throw NullReferenceException();
    stream->Write(bytes);
    return true;
  } catch (const IOException& io) {
    Logger()->Error("Failed to write frame because of a IO Exception: {0}", Args(io.Message()));
  } catch (const ObjectDisposedException&) {
    Logger()->Warning("Failed to write frame as the stream was already disposed");
  } catch (const InvalidOperationException&) {
    Logger()->Warning("Failed to write frame because of a invalid operation");
  }

  return false;
}

void ManagedNamedPipeClient::Close() {
  if (is_closed_) {
    Logger()->Warning("Tried to close a already closed pipe.");
    return;
  }

  {
    std::lock_guard<std::recursive_mutex> lock(l_stream_);
    if (stream_ != nullptr) {
      try {
        stream_->Flush();
      } catch (const Exception&) {
      }
      stream_->Dispose();

      stream_ = nullptr;
      is_closed_ = true;
    } else {
      Logger()->Warning("Stream was closed, but no stream was available to begin with!");
    }
  }

  is_closed_ = true;
}

void ManagedNamedPipeClient::Dispose() {
  if (is_disposed_) return;

  if (!is_closed_) Close();

  {
    std::lock_guard<std::recursive_mutex> lock(l_stream_);
    if (stream_ != nullptr) {
      stream_->Dispose();
      stream_ = nullptr;
    }
  }

  is_disposed_ = true;
}

void ManagedNamedPipeClient::Release() {
  if (released_ != nullptr) SetEvent(static_cast<HANDLE>(released_));
}

void ManagedNamedPipeClient::JoinReaders(bool all) {
  std::vector<Reader> finished;
  {
    std::lock_guard<std::mutex> lock(readers_lock_);
    for (auto reader = readers_.begin(); reader != readers_.end();) {
      if (all || reader->done->load()) {
        finished.push_back(std::move(*reader));
        reader = readers_.erase(reader);
      } else {
        ++reader;
      }
    }
  }
  for (Reader& reader : finished) {
    if (reader.thread.joinable()) reader.thread.join();
  }
}

}
