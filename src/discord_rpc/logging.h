// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#ifndef DISCORD_RPC_LOGGING_H_
#define DISCORD_RPC_LOGGING_H_

#include <mutex>
#include <string>
#include <vector>

namespace discord_rpc::logging {

enum class LogLevel {
  Trace = 1,
  Info = 2,
  Warning = 3,
  Error = 4,
  None = 256,
};

using Arguments = std::vector<std::string>;

class ILogger {
 public:
  virtual ~ILogger() = default;

  virtual LogLevel Level() const = 0;
  virtual void SetLevel(LogLevel level) = 0;

  virtual void Trace(const std::string& message, const Arguments& args = {}) = 0;
  virtual void Info(const std::string& message, const Arguments& args = {}) = 0;
  virtual void Warning(const std::string& message, const Arguments& args = {}) = 0;
  virtual void Error(const std::string& message, const Arguments& args = {}) = 0;
};

class NullLogger final : public ILogger {
 public:
  LogLevel Level() const override { return level_; }
  void SetLevel(LogLevel level) override { level_ = level; }

  void Trace(const std::string&, const Arguments&) override {}
  void Info(const std::string&, const Arguments&) override {}
  void Warning(const std::string&, const Arguments&) override {}
  void Error(const std::string&, const Arguments&) override {}

 private:
  LogLevel level_ = static_cast<LogLevel>(0);
};

class ConsoleLogger final : public ILogger {
 public:
  ConsoleLogger() = default;
  explicit ConsoleLogger(LogLevel level) : level_(level) {}
  ConsoleLogger(LogLevel level, bool coloured) : level_(level), coloured_(coloured) {}

  LogLevel Level() const override { return level_; }
  void SetLevel(LogLevel level) override { level_ = level; }
  bool Coloured() const { return coloured_; }
  void SetColoured(bool coloured) { coloured_ = coloured; }
  bool Colored() const { return coloured_; }
  void SetColored(bool colored) { coloured_ = colored; }

  void Trace(const std::string& message, const Arguments& args = {}) override;
  void Info(const std::string& message, const Arguments& args = {}) override;
  void Warning(const std::string& message, const Arguments& args = {}) override;
  void Error(const std::string& message, const Arguments& args = {}) override;

 private:
  void Write(unsigned short colour, const char* prefix, const std::string& message, const Arguments& args);

  LogLevel level_ = LogLevel::Info;
  bool coloured_ = false;
};

class FileLogger final : public ILogger {
 public:
  explicit FileLogger(std::string path) : FileLogger(std::move(path), LogLevel::Info) {}
  FileLogger(std::string path, LogLevel level) : level_(level), file_(std::move(path)) {}

  LogLevel Level() const override { return level_; }
  void SetLevel(LogLevel level) override { level_ = level; }
  const std::string& File() const { return file_; }
  void SetFile(std::string path) { file_ = std::move(path); }

  void Trace(const std::string& message, const Arguments& args = {}) override;
  void Info(const std::string& message, const Arguments& args = {}) override;
  void Warning(const std::string& message, const Arguments& args = {}) override;
  void Error(const std::string& message, const Arguments& args = {}) override;

 private:
  void Append(const char* prefix, const std::string& message, const Arguments& args);

  LogLevel level_;
  std::string file_;
  std::mutex filelock_;
};

}

#endif
