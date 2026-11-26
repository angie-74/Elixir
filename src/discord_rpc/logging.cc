// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#include "discord_rpc/logging.h"

#include <windows.h>

#include <cstdio>

#include "discord_rpc/helper.h"

namespace discord_rpc::logging {
namespace {

constexpr unsigned short kGray = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr unsigned short kWhite = kGray | FOREGROUND_INTENSITY;
constexpr unsigned short kYellow = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr unsigned short kRed = FOREGROUND_RED | FOREGROUND_INTENSITY;

std::string Text(const std::string& message, const Arguments& args) {
  return args.empty() ? message : helper::Format(message, args);
}

}

void ConsoleLogger::Write(unsigned short colour, const char* prefix, const std::string& message,
                          const Arguments& args) {
  if (coloured_) {
    const HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (console != nullptr && console != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(console, &info)) {
      SetConsoleTextAttribute(console, static_cast<WORD>((info.wAttributes & 0xFFF0) | colour));
    }
  }
  const std::string line = Text(prefix + message, args) + "\n";
  std::fwrite(line.data(), 1, line.size(), stdout);
  std::fflush(stdout);
}

void ConsoleLogger::Trace(const std::string& message, const Arguments& args) {
  if (level_ > LogLevel::Trace) return;
  Write(kGray, "TRACE: ", message, args);
}

void ConsoleLogger::Info(const std::string& message, const Arguments& args) {
  if (level_ > LogLevel::Info) return;
  Write(kWhite, "INFO: ", message, args);
}

void ConsoleLogger::Warning(const std::string& message, const Arguments& args) {
  if (level_ > LogLevel::Warning) return;
  Write(kYellow, "WARN: ", message, args);
}

void ConsoleLogger::Error(const std::string& message, const Arguments& args) {
  if (level_ > LogLevel::Error) return;
  Write(kRed, "ERR : ", message, args);
}

void FileLogger::Append(const char* prefix, const std::string& message, const Arguments& args) {
  const std::string text = prefix + Text(message, args);
  std::lock_guard<std::mutex> lock(filelock_);
  const HANDLE file = CreateFileW(helper::ToWide(file_).c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                                  OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return;
  DWORD written = 0;
  WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
  CloseHandle(file);
}

void FileLogger::Trace(const std::string& message, const Arguments& args) {
  if (level_ > LogLevel::Trace) return;
  Append("\r\nTRCE: ", message, args);
}

void FileLogger::Info(const std::string& message, const Arguments& args) {
  if (level_ > LogLevel::Info) return;
  Append("\r\nINFO: ", message, args);
}

void FileLogger::Warning(const std::string& message, const Arguments& args) {
  if (level_ > LogLevel::Warning) return;
  Append("\r\nWARN: ", message, args);
}

void FileLogger::Error(const std::string& message, const Arguments& args) {
  if (level_ > LogLevel::Error) return;
  Append("\r\nERR : ", message, args);
}

}
