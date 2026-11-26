// A C++ translation of discord-rpc-csharp 1.6.1
// (https://github.com/Lachee/discord-rpc-csharp), Copyright (c) 2021 Lachee,
// MIT License: see LICENSE in this folder.
#ifndef DISCORD_RPC_EXCEPTIONS_H_
#define DISCORD_RPC_EXCEPTIONS_H_

#include <stdexcept>
#include <string>

namespace discord_rpc {

class Exception : public std::runtime_error {
 public:
  Exception(const char* type_name, const std::string& message) : std::runtime_error(message), type_name_(type_name) {}

  const char* TypeName() const noexcept { return type_name_; }
  std::string Message() const { return what(); }

 private:
  const char* type_name_;
};

class NullReferenceException final : public Exception {
 public:
  NullReferenceException()
      : Exception("NullReferenceException", "Object reference not set to an instance of an object.") {}
};

class ArgumentException : public Exception {
 public:
  explicit ArgumentException(const std::string& message) : Exception("ArgumentException", message) {}

 protected:
  ArgumentException(const char* type_name, const std::string& message) : Exception(type_name, message) {}
};

class ArgumentNullException final : public ArgumentException {
 public:
  explicit ArgumentNullException(const std::string& parameter)
      : ArgumentException("ArgumentNullException", "Value cannot be null.\r\nParameter name: " + parameter) {}
};

class ArgumentOutOfRangeException final : public ArgumentException {
 public:
  ArgumentOutOfRangeException(const std::string& parameter, const std::string& message)
      : ArgumentException("ArgumentOutOfRangeException", message + "\r\nParameter name: " + parameter) {}
};

class InvalidOperationException : public Exception {
 public:
  explicit InvalidOperationException(const std::string& message) : Exception("InvalidOperationException", message) {}

 protected:
  InvalidOperationException(const char* type_name, const std::string& message) : Exception(type_name, message) {}
};

class ObjectDisposedException final : public InvalidOperationException {
 public:
  explicit ObjectDisposedException(const std::string& object_name)
      : InvalidOperationException("ObjectDisposedException",
                                  "Cannot access a disposed object.\r\nObject name: '" + object_name + "'.") {}
  ObjectDisposedException(const std::string& object_name, const std::string& message)
      : InvalidOperationException("ObjectDisposedException", message + "\r\nObject name: '" + object_name + "'.") {}
};

class IndexOutOfRangeException final : public Exception {
 public:
  IndexOutOfRangeException() : Exception("IndexOutOfRangeException", "Index was outside the bounds of the array.") {}
};

class OverflowException final : public Exception {
 public:
  OverflowException() : Exception("OverflowException", "Arithmetic operation resulted in an overflow.") {}
};

class FormatException final : public Exception {
 public:
  FormatException() : Exception("FormatException", "Input string was not in a correct format.") {}
};

class InvalidCastException final : public Exception {
 public:
  explicit InvalidCastException(const std::string& message) : Exception("InvalidCastException", message) {}
};

class BadImageFormatException final : public Exception {
 public:
  explicit BadImageFormatException(const std::string& message) : Exception("BadImageFormatException", message) {}
};

class IOException final : public Exception {
 public:
  explicit IOException(const std::string& message) : Exception("IOException", message) {}
};

class TimeoutException final : public Exception {
 public:
  TimeoutException() : Exception("TimeoutException", "The operation has timed out.") {}
};

class JsonException : public Exception {
 public:
  JsonException(const char* type_name, const std::string& message) : Exception(type_name, message) {}
};

class JsonReaderException final : public JsonException {
 public:
  explicit JsonReaderException(const std::string& message) : JsonException("JsonReaderException", message) {}
};

class JsonSerializationException final : public JsonException {
 public:
  explicit JsonSerializationException(const std::string& message)
      : JsonException("JsonSerializationException", message) {}
};

class BadPresenceException final : public Exception {
 public:
  explicit BadPresenceException(const std::string& message) : Exception("BadPresenceException", message) {}
};

class InvalidConfigurationException final : public Exception {
 public:
  explicit InvalidConfigurationException(const std::string& message)
      : Exception("InvalidConfigurationException", message) {}
};

class InvalidPipeException final : public Exception {
 public:
  explicit InvalidPipeException(const std::string& message) : Exception("InvalidPipeException", message) {}
};

class StringOutOfRangeException final : public Exception {
 public:
  StringOutOfRangeException(const std::string& message, int min, int max)
      : Exception("StringOutOfRangeException", message), minimum_length_(min), maximum_length_(max) {}
  StringOutOfRangeException(int minimum, int max)
      : StringOutOfRangeException("Length of string is out of range. Expected a value between " +
                                      std::to_string(minimum) + " and " + std::to_string(max),
                                  minimum, max) {}
  explicit StringOutOfRangeException(int max)
      : StringOutOfRangeException(
            "Length of string is out of range. Expected a value with a maximum length of " + std::to_string(max), 0,
            max) {}

  int MaximumLength() const { return maximum_length_; }
  int MinimumLength() const { return minimum_length_; }

 private:
  int minimum_length_;
  int maximum_length_;
};

class UninitializedException final : public Exception {
 public:
  explicit UninitializedException(const std::string& message) : Exception("UninitializedException", message) {}
  UninitializedException()
      : UninitializedException(
            "Cannot perform action because the client has not been initialized yet or has been deinitialized.") {}
};

}

#endif
