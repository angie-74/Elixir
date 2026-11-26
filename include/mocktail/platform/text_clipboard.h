// Modified by Elixir.
#ifndef MOCKTAIL_PLATFORM_TEXT_CLIPBOARD_H_
#define MOCKTAIL_PLATFORM_TEXT_CLIPBOARD_H_

#include <memory>
#include <string>

#include "mocktail/status.h"

namespace mocktail {
namespace platform {

class TextClipboard {
 public:
  virtual ~TextClipboard() = default;

  virtual Status ReadText(std::string* text) = 0;
  virtual Status WriteText(const std::string& text) = 0;
};

std::unique_ptr<TextClipboard> CreateSdlTextClipboard();

}
}

#endif
