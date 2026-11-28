// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "shaders/shader_text.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace elixir::shaders {

namespace {

bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r'; }

bool IsLetter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

std::string Lowered(std::string_view text) {
  std::string out(text);
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return out;
}

class Words {
 public:
  explicit Words(std::string_view line) : rest_(line) {}

  bool Next(std::string* word) {
    Skip();
    if (rest_.empty()) return false;
    std::size_t end = 0;
    while (end < rest_.size() && !IsSpace(rest_[end])) ++end;
    *word = std::string(rest_.substr(0, end));
    rest_.remove_prefix(end);
    return true;
  }

  bool Quoted(std::string* text) {
    Skip();
    if (rest_.empty() || rest_.front() != '"') return false;
    const std::size_t end = rest_.find('"', 1);
    if (end == std::string_view::npos) return false;
    *text = std::string(rest_.substr(1, end - 1));
    rest_.remove_prefix(end + 1);
    return true;
  }

  bool Number(float* value) {
    std::string word;
    if (!Next(&word) || word.empty() || word.size() > 32) return false;
    errno = 0;
    char* end = nullptr;
    const double parsed = std::strtod(word.c_str(), &end);
    if (end != word.c_str() + word.size() || errno != 0 || !std::isfinite(parsed) ||
        std::fabs(parsed) > 1.0e6) {
      return false;
    }
    *value = static_cast<float>(parsed);
    return true;
  }

  bool AtEnd() {
    Skip();
    return rest_.empty();
  }

 private:
  void Skip() {
    while (!rest_.empty() && IsSpace(rest_.front())) rest_.remove_prefix(1);
  }

  std::string_view rest_;
};

void Report(std::vector<Diagnostic>* diagnostics, int line, std::string message) {
  if (diagnostics != nullptr) diagnostics->push_back(Diagnostic{line, std::move(message), true});
}

}

bool IsShaderIdentifier(std::string_view text) {
  if (text.empty() || text.size() > 48 || !IsLetter(text.front())) return false;
  if (text.size() >= 3 && text.substr(0, 3) == "gl_") return false;
  return std::all_of(text.begin(), text.end(), [](char c) { return IsLetter(c) || IsDigit(c); });
}

bool IsPlainFileName(std::string_view text) {
  if (text.empty() || text.size() > 96 || text.front() == '.' || text.front() == ' ' || text.back() == '.' ||
      text.back() == ' ') {
    return false;
  }
  for (const char c : text) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (u < 0x20 || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' ||
        c == '|') {
      return false;
    }
  }
  return true;
}

bool StageOfFileName(std::string_view file_name, ShaderStage* stage) {
  const std::size_t dot = file_name.rfind('.');
  if (dot == std::string_view::npos) return false;
  const std::string extension = Lowered(file_name.substr(dot));
  if (extension == ".frag" || extension == ".glsl") {
    *stage = ShaderStage::kFragment;
  } else if (extension == ".vert") {
    *stage = ShaderStage::kVertex;
  } else if (extension == ".comp") {
    *stage = ShaderStage::kCompute;
  } else {
    return false;
  }
  return true;
}

float ClampParameter(const ShaderParameter& parameter, float value) {
  if (!std::isfinite(value)) return parameter.initial;
  return std::min(std::max(value, parameter.minimum), parameter.maximum);
}

bool ParseShaderPragmas(std::string_view source, ShaderPragmas* pragmas, std::vector<Diagnostic>* diagnostics) {
  *pragmas = ShaderPragmas{};
  if (source.size() > kMaxSourceBytes) {
    Report(diagnostics, 0, "the file is larger than 256 KB");
    return false;
  }
  bool ok = true;
  int number = 0;
  std::size_t begin = 0;
  while (begin <= source.size()) {
    std::size_t end = source.find('\n', begin);
    if (end == std::string_view::npos) end = source.size();
    ++number;
    std::string_view line = source.substr(begin, end - begin);
    begin = end + 1;
    while (!line.empty() && IsSpace(line.front())) line.remove_prefix(1);
    if (line.empty() || line.front() != '#') continue;
    line.remove_prefix(1);
    Words words(line);
    std::string word;
    if (!words.Next(&word) || word != "pragma" || !words.Next(&word)) continue;
    if (word == "parameter") {
      ShaderParameter parameter;
      if (!words.Next(&parameter.name) || !IsShaderIdentifier(parameter.name) || !words.Quoted(&parameter.label) ||
          !words.Number(&parameter.initial) || !words.Number(&parameter.minimum) ||
          !words.Number(&parameter.maximum)) {
        Report(diagnostics, number, "expected: #pragma parameter name \"Label\" default minimum maximum [step]");
        ok = false;
        continue;
      }
      if (!words.AtEnd() && (!words.Number(&parameter.step) || !words.AtEnd())) {
        Report(diagnostics, number, "expected: #pragma parameter name \"Label\" default minimum maximum [step]");
        ok = false;
        continue;
      }
      if (parameter.maximum < parameter.minimum || parameter.step < 0.0f || parameter.label.size() > 64) {
        Report(diagnostics, number, "the parameter's range is not usable");
        ok = false;
        continue;
      }
      if (std::any_of(pragmas->parameters.begin(), pragmas->parameters.end(),
                      [&parameter](const ShaderParameter& other) { return other.name == parameter.name; })) {
        Report(diagnostics, number, "the parameter " + parameter.name + " is declared twice");
        ok = false;
        continue;
      }
      if (pragmas->parameters.size() >= kMaxParameters) {
        Report(diagnostics, number, "a shader can have at most 32 parameters");
        ok = false;
        continue;
      }
      parameter.initial = ClampParameter(parameter, parameter.initial);
      if (parameter.label.empty()) parameter.label = parameter.name;
      pragmas->parameters.push_back(std::move(parameter));
    } else if (word == "scale") {
      float scale = 0.0f;
      if (!words.Number(&scale) || !words.AtEnd() || scale < kMinPassScale || scale > 1.0f) {
        Report(diagnostics, number, "expected: #pragma scale with a number from 0.0625 to 1");
        ok = false;
        continue;
      }
      pragmas->scale = scale;
      pragmas->has_scale = true;
    } else if (word == "format") {
      std::string format;
      if (!words.Next(&format) || !words.AtEnd() || (format != "rgba8" && format != "rgba16f")) {
        Report(diagnostics, number, "expected: #pragma format rgba8 or rgba16f");
        ok = false;
        continue;
      }
      pragmas->format = format == "rgba16f" ? TargetFormat::kRgba16f : TargetFormat::kRgba8;
    } else if (word == "texture") {
      ShaderTexture texture;
      if (!words.Next(&texture.name) || !IsShaderIdentifier(texture.name) || !words.Quoted(&texture.file) ||
          !words.AtEnd() || !IsPlainFileName(texture.file)) {
        Report(diagnostics, number, "expected: #pragma texture name \"file.png\" with a file beside the shader");
        ok = false;
        continue;
      }
      if (pragmas->textures.size() >= kMaxTextures) {
        Report(diagnostics, number, "a shader can load at most 4 textures");
        ok = false;
        continue;
      }
      pragmas->textures.push_back(std::move(texture));
    }
  }
  return ok;
}

}
