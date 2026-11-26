// Modified by Elixir.
#ifndef MOCKTAIL_RUNTIME_RUNTIME_CONFIG_BOOTSTRAP_H_
#define MOCKTAIL_RUNTIME_RUNTIME_CONFIG_BOOTSTRAP_H_

#include <filesystem>
#include <string>
#include <string_view>

namespace mocktail {
namespace runtime {

enum class RuntimeConfigBootstrapStatus {
  kCreated,
  kPreservedExisting,
  kAbsent,
};

struct RuntimeConfigBootstrapResult {
  RuntimeConfigBootstrapStatus status =
      RuntimeConfigBootstrapStatus::kPreservedExisting;
  std::string error;

  explicit operator bool() const { return error.empty(); }
  bool created() const {
    return status == RuntimeConfigBootstrapStatus::kCreated;
  }
};

std::string_view DefaultRuntimeConfigYaml();

RuntimeConfigBootstrapResult EnsureRuntimeConfigFile(
    const std::filesystem::path& config_file);

}
}

#endif
