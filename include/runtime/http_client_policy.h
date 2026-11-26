// Modified by Elixir.
#ifndef MOCKTAIL_RUNTIME_HTTP_CLIENT_POLICY_H_
#define MOCKTAIL_RUNTIME_HTTP_CLIENT_POLICY_H_

#include <string>
#include <string_view>

namespace mocktail {
namespace runtime {

bool MergeHttpClientSettingsOverrides(std::string_view base_json,
                                      std::string* merged_json,
                                      std::string* error);

}
}

#endif
