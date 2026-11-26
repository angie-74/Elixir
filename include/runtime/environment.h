// Modified by Elixir.
#ifndef MOCKTAIL_RUNTIME_ENVIRONMENT_H_
#define MOCKTAIL_RUNTIME_ENVIRONMENT_H_

#include <optional>
#include <string>
#include <string_view>

namespace mocktail {
namespace runtime {

class Environment {
 public:
  virtual ~Environment() = default;

  virtual std::optional<std::string> Get(std::string_view name) const = 0;

  bool HasNonEmpty(std::string_view name) const;
  std::string GetOr(std::string_view name,
                    std::string_view default_value) const;
};

class ProcessEnvironment final : public Environment {
 public:
  std::optional<std::string> Get(std::string_view name) const override;
};

}
}

#endif
