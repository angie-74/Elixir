// Modified by Elixir.
#ifndef MOCKTAIL_PLATFORM_DISPLAY_REFRESH_CAPABILITIES_H_
#define MOCKTAIL_PLATFORM_DISPLAY_REFRESH_CAPABILITIES_H_

#include <vector>

struct SDL_Window;

namespace mocktail {
namespace platform {

struct DisplayRefreshCapabilities {
  float current_hz = 0.0f;
  std::vector<float> supported_hz;

  bool valid() const { return current_hz > 0.0f; }
};

DisplayRefreshCapabilities
QuerySdlDisplayRefreshCapabilities(SDL_Window *window);

DisplayRefreshCapabilities
NormalizeDisplayRefreshCapabilities(float current_hz,
                                    std::vector<float> supported_hz);

}
}

#endif
