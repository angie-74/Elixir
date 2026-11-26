// Modified by Elixir.
#ifndef MOCKTAIL_RUNTIME_TEXTURE_MEMORY_POLICY_H_
#define MOCKTAIL_RUNTIME_TEXTURE_MEMORY_POLICY_H_

#include <cstdint>
#include <string>
#include <string_view>

namespace mocktail {
namespace runtime {

std::uint64_t DetectHostMemoryBytes();

std::uint64_t CalculateTextureMemoryBudgetBytes(std::uint64_t host_memory_bytes);

bool MergeTextureMemoryClientSettingsOverrides(std::uint64_t budget_bytes,
                                               std::string_view base_json,
                                               std::string* merged_json,
                                               std::string* error);

}
}

#endif
