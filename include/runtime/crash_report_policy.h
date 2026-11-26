// Modified by Elixir.
#ifndef MOCKTAIL_RUNTIME_CRASH_REPORT_POLICY_H_
#define MOCKTAIL_RUNTIME_CRASH_REPORT_POLICY_H_

#include <string>
#include <string_view>

namespace mocktail {
namespace runtime {

bool MergeCrashReportClientSettingsOverrides(std::string_view base_json,
                                             std::string* merged_json,
                                             std::string* error);

bool MergeCrashReportFastFlagsOverrides(std::string_view base_json,
                                        std::string* merged_json,
                                        std::string* error);

}
}

#endif
