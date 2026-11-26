// Modified by Elixir.
#ifndef MOCKTAIL_RUNTIME_FRAME_RATE_POLICY_H_
#define MOCKTAIL_RUNTIME_FRAME_RATE_POLICY_H_

#include <string>
#include <string_view>

namespace mocktail {
namespace runtime {

inline constexpr int kMaximumSupportedRobloxSchedulerFps = 240;

enum class FrameRateLimitMode {
  kUnmanaged,
  kDisplay,
  kFixed,
  kUnlimited,
  kInvalid,
};

struct FrameRatePolicy {
  FrameRateLimitMode mode = FrameRateLimitMode::kUnmanaged;
  int fixed_fps = 0;

  bool valid() const { return mode != FrameRateLimitMode::kInvalid; }
};

FrameRatePolicy ParseFrameRatePolicy(std::string_view value);

bool MergeFrameRateClientSettingsOverrides(const FrameRatePolicy &policy,
                                           std::string_view base_json,
                                           std::string *merged_json,
                                           std::string *error);

}
}

#endif
