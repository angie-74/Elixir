// Modified by Elixir.
#ifndef MOCKTAIL_COMPAT_ELF_BUILD_ID_H_
#define MOCKTAIL_COMPAT_ELF_BUILD_ID_H_

#include <string>
#include <string_view>

namespace mocktail::compat {

struct BuildIdResult {
  std::string build_id;
  std::string error;

  explicit operator bool() const noexcept { return error.empty(); }
};

BuildIdResult ReadElfBuildId(const std::string& path);

bool IsValidBuildId(std::string_view build_id) noexcept;

}

#endif
