// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#ifndef DISCORD_RPC_IO_H_
#define DISCORD_RPC_IO_H_

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "discord_rpc/helper.h"
#include "discord_rpc/logging.h"

namespace discord_rpc::io {

enum class Opcode : std::uint32_t {
  Handshake = 0,
  Frame = 1,
  Close = 2,
  Ping = 3,
  Pong = 4,
};
std::string ToString(Opcode opcode);

struct Handshake {
  int Version = 0;
  String ClientID;

  std::string ToJson() const;
};

class MemoryStream {
 public:
  MemoryStream(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

  int Read(std::uint8_t* buffer, int offset, int count);

 private:
  const std::uint8_t* data_;
  std::size_t size_;
  std::size_t position_ = 0;
};

struct PipeFrame {
  static constexpr int MAX_SIZE = 16 * 1024;

  io::Opcode Opcode = io::Opcode::Handshake;
  std::shared_ptr<std::vector<std::uint8_t>> Data;

  PipeFrame() = default;
  PipeFrame(io::Opcode opcode, const std::string& json);

  std::uint32_t Length() const;
  std::string Message() const;
  void SetMessage(std::string_view str);
  void SetObject(const std::string& json) { SetMessage(json); }
  void SetObject(io::Opcode opcode, const std::string& json);

  bool ReadStream(MemoryStream* stream);
  std::vector<std::uint8_t> ToBytes() const;

  bool Equals(const PipeFrame& other) const;
};

class INamedPipeClient {
 public:
  virtual ~INamedPipeClient() = default;

  virtual std::shared_ptr<logging::ILogger> Logger() const = 0;
  virtual void SetLogger(std::shared_ptr<logging::ILogger> logger) = 0;

  virtual bool IsConnected() = 0;
  virtual int ConnectedPipe() const = 0;

  virtual bool Connect(int pipe) = 0;
  virtual bool ReadFrame(PipeFrame* frame) = 0;
  virtual bool WriteFrame(const PipeFrame& frame) = 0;
  virtual void Close() = 0;
  virtual void Dispose() = 0;

  virtual void Release() {}
};

struct PipeLocation {
  static std::vector<std::string> GetPipes(int start_pipe = 0);
};

class ManagedNamedPipeClient final : public INamedPipeClient {
 public:
  ManagedNamedPipeClient();
  explicit ManagedNamedPipeClient(std::wstring pipe_prefix);
  ~ManagedNamedPipeClient() override;
  ManagedNamedPipeClient(const ManagedNamedPipeClient&) = delete;
  ManagedNamedPipeClient& operator=(const ManagedNamedPipeClient&) = delete;

  std::shared_ptr<logging::ILogger> Logger() const override;
  void SetLogger(std::shared_ptr<logging::ILogger> logger) override;

  bool IsConnected() override;
  int ConnectedPipe() const override { return connected_pipe_.load(); }

  bool Connect(int pipe) override;
  bool ReadFrame(PipeFrame* frame) override;
  bool WriteFrame(const PipeFrame& frame) override;
  void Close() override;
  void Dispose() override;
  void Release() override;

 private:
  class Stream;
  struct Reader {
    std::thread thread;
    std::shared_ptr<std::atomic<bool>> done;
  };

  bool AttemptConnection(const std::string& pipename, const std::wstring& path);
  void BeginReadStream();
  void EndReadStream(const std::shared_ptr<Stream>& stream, unsigned long error, unsigned long bytes);
  void JoinReaders(bool all);

  mutable std::mutex logger_lock_;
  std::shared_ptr<logging::ILogger> logger_;
  std::atomic<int> connected_pipe_{0};

  std::shared_ptr<Stream> stream_;
  std::deque<PipeFrame> framequeue_;
  std::recursive_mutex framequeuelock_;

  std::atomic<bool> is_disposed_{false};
  std::atomic<bool> is_closed_{true};

  std::recursive_mutex l_stream_;

  const std::wstring pipe_prefix_;
  void* released_ = nullptr;
  std::mutex readers_lock_;
  std::vector<Reader> readers_;
};

}

#endif
