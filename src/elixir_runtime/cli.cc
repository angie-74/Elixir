// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "elixir_runtime/cli.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <set>
#include <sstream>
#include <string_view>

#include "elixir_runtime/text.h"

#ifndef ELIXIR_VERSION
#define ELIXIR_VERSION "unknown"
#endif

namespace elixir::runtime {

bool IsAndroidPackageName(std::string_view name) {
  if (name.empty() || name.size() > 255) return false;
  std::size_t segments = 0;
  std::size_t start = 0;
  while (start <= name.size()) {
    const std::size_t dot = name.find('.', start);
    const std::string_view segment = name.substr(start, dot == std::string_view::npos ? name.npos : dot - start);
    if (segment.empty()) return false;
    const char first = segment.front();
    if (!((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z'))) return false;
    for (const char c : segment) {
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    ++segments;
    if (dot == std::string_view::npos) break;
    start = dot + 1;
  }
  return segments >= 2;
}

namespace {

bool IsValidResolutionScale(double scale) { return scale >= 0.25 && scale <= 1.0; }
bool IsValidStretch(double stretch) { return stretch >= 0.0 && stretch <= 0.5; }

bool ParseInt64(std::string_view text, std::int64_t* value) {
  if (text.empty() || text.size() > 19) return false;
  std::int64_t result = 0;
  for (const char c : text) {
    if (c < '0' || c > '9') return false;
    const int digit = c - '0';
    if (result > (std::numeric_limits<std::int64_t>::max() - digit) / 10) return false;
    result = result * 10 + digit;
  }
  *value = result;
  return true;
}

bool ParseOnOff(std::string_view text, bool* value) {
  if (text == "on") {
    *value = true;
    return true;
  }
  if (text == "off") {
    *value = false;
    return true;
  }
  return false;
}

bool HasControlCharacters(std::string_view text) {
  for (const char c : text) {
    if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) return true;
  }
  return false;
}

std::string Quoted(std::string_view value) { return "'" + std::string(value) + "'"; }

struct OptionSpec {
  const char* name;
  bool takes_value;
  std::function<std::string(LaunchOptions*, const std::string&)> apply;
};

std::string PathOption(std::filesystem::path* target, const std::string& value) {
  if (value.empty() || HasControlCharacters(value)) return "expected a path";
  *target = PathFromUtf8(value);
  return {};
}

std::string Choice(std::string* target, const std::string& value, std::initializer_list<const char*> allowed) {
  for (const char* option : allowed) {
    if (value == option) {
      *target = value;
      return {};
    }
  }
  std::string message = "expected ";
  std::size_t index = 0;
  for (const char* option : allowed) {
    if (index > 0) message += index + 1 == allowed.size() ? " or " : ", ";
    message += option;
    ++index;
  }
  return message + ", got " + Quoted(value);
}

std::string OnOff(std::optional<bool>* target, const std::string& value) {
  bool parsed = false;
  if (!ParseOnOff(value, &parsed)) return "expected on or off, got " + Quoted(value);
  *target = parsed;
  return {};
}

std::string AudioEndpoint(std::string* target, const std::string& value) {
  if (value.empty() || value.size() > 512 || HasControlCharacters(value)) {
    return "expected default or an audio endpoint id (at most 512 characters)";
  }
  *target = value;
  return {};
}

const std::vector<OptionSpec>& Options() {
  static const std::vector<OptionSpec> options = {
      {"--package", true,
       [](LaunchOptions* o, const std::string& v) -> std::string {
         if (!IsAndroidPackageName(v)) return "expected an Android package name such as com.roblox.client, got " + Quoted(v);
         o->package = v;
         return {};
       }},
      {"--version-code", true,
       [](LaunchOptions* o, const std::string& v) -> std::string {
         std::int64_t parsed = 0;
         if (!ParseInt64(v, &parsed)) return "expected a non-negative integer (the APK's versionCode), got " + Quoted(v);
         o->version_code = parsed;
         return {};
       }},
      {"--app-dir", true, [](LaunchOptions* o, const std::string& v) { return PathOption(&o->app_dir, v); }},
      {"--data-dir", true, [](LaunchOptions* o, const std::string& v) { return PathOption(&o->data_dir, v); }},
      {"--config", true, [](LaunchOptions* o, const std::string& v) { return PathOption(&o->config_file, v); }},
      {"--crash-dir", true, [](LaunchOptions* o, const std::string& v) { return PathOption(&o->crash_dir, v); }},
      {"--fflags", true, [](LaunchOptions* o, const std::string& v) { return PathOption(&o->fflags_file, v); }},
      {"--guest-runtime", true,
       [](LaunchOptions* o, const std::string& v) { return PathOption(&o->guest_runtime_dir, v); }},
      {"--roblox-lib", true,
       [](LaunchOptions* o, const std::string& v) { return PathOption(&o->roblox_library, v); }},
      {"--renderer", true,
       [](LaunchOptions* o, const std::string& v) -> std::string {
         if (v != "vulkan") return "only vulkan is supported (Roblox's Android x86-64 client renders with Vulkan), got " + Quoted(v);
         o->renderer = v;
         return {};
       }},
      {"--present-mode", true,
       [](LaunchOptions* o, const std::string& v) { return Choice(&o->present_mode, v, {"fifo", "mailbox", "immediate"}); }},
      {"--frame-rate-cap", true,
       [](LaunchOptions* o, const std::string& v) -> std::string {
         errno = 0;
         char* end = nullptr;
         const long parsed = std::strtol(v.c_str(), &end, 10);
         if (v.empty() || end != v.c_str() + v.size() || errno != 0 || parsed < 1 || parsed > 100000) {
           return "expected a frame rate from 1 to 100000, got " + Quoted(v);
         }
         o->frame_rate_cap = static_cast<int>(parsed);
         return {};
       }},
      {"--resolution-scale", true,
       [](LaunchOptions* o, const std::string& v) -> std::string {
         errno = 0;
         char* end = nullptr;
         const double parsed = std::strtod(v.c_str(), &end);
         if (v.empty() || end != v.c_str() + v.size() || errno != 0 || !IsValidResolutionScale(parsed)) {
           return "expected a scale from 0.25 to 1.00, got " + Quoted(v);
         }
         o->resolution_scale = parsed;
         return {};
       }},
      {"--stretch", true,
       [](LaunchOptions* o, const std::string& v) -> std::string {
         errno = 0;
         char* end = nullptr;
         const double parsed = std::strtod(v.c_str(), &end);
         if (v.empty() || end != v.c_str() + v.size() || errno != 0 || !IsValidStretch(parsed)) {
           return "expected a stretch from 0.00 to 0.50, got " + Quoted(v);
         }
         o->stretch = parsed;
         return {};
       }},
      {"--mods-dir", true, [](LaunchOptions* o, const std::string& v) { return PathOption(&o->mods_dir, v); }},
      {"--accounts-dir", true,
       [](LaunchOptions* o, const std::string& v) { return PathOption(&o->accounts_dir, v); }},
      {"--activity-dir", true,
       [](LaunchOptions* o, const std::string& v) { return PathOption(&o->activity_dir, v); }},
      {"--shaders-dir", true,
       [](LaunchOptions* o, const std::string& v) { return PathOption(&o->shaders_dir, v); }},
      {"--upscale", true, [](LaunchOptions* o, const std::string& v) { return OnOff(&o->upscale, v); }},
      {"--dynamic-resolution", true,
       [](LaunchOptions* o, const std::string& v) { return OnOff(&o->dynamic_resolution, v); }},
      {"--sharpness", true,
       [](LaunchOptions* o, const std::string& v) -> std::string {
         errno = 0;
         char* end = nullptr;
         const double parsed = std::strtod(v.c_str(), &end);
         if (v.empty() || end != v.c_str() + v.size() || errno != 0 || !(parsed >= 0.0 && parsed <= 1.0)) {
           return "expected a sharpness from 0.00 to 1.00, got " + Quoted(v);
         }
         o->sharpness = parsed;
         return {};
       }},
      {"--target-fps", true,
       [](LaunchOptions* o, const std::string& v) -> std::string {
         errno = 0;
         char* end = nullptr;
         const long parsed = std::strtol(v.c_str(), &end, 10);
         if (v.empty() || end != v.c_str() + v.size() || errno != 0 || parsed < 20 || parsed > 480) {
           return "expected a target frame rate from 20 to 480, got " + Quoted(v);
         }
         o->target_fps = static_cast<int>(parsed);
         return {};
       }},
      {"--wait-spin", true,
       [](LaunchOptions* o, const std::string& v) -> std::string {
         errno = 0;
         char* end = nullptr;
         const long parsed = std::strtol(v.c_str(), &end, 10);
         if (v.empty() || end != v.c_str() + v.size() || errno != 0 || parsed < 0 || parsed > 200) {
           return "expected 0 to 200 microseconds, got " + Quoted(v);
         }
         o->wait_spin_us = static_cast<int>(parsed);
         return {};
       }},
      {"--controller", true, [](LaunchOptions* o, const std::string& v) { return OnOff(&o->controller, v); }},
      {"--user-fflags", true, [](LaunchOptions* o, const std::string& v) { return OnOff(&o->user_fflags, v); }},
      {"--discord", true,
       [](LaunchOptions* o, const std::string& v) -> std::string {
         if (v.size() < 17 || v.size() > 20 || v.find_first_not_of("0123456789") != std::string::npos) {
           return "expected a Discord application ID (17 to 20 digits), got " + Quoted(v);
         }
         o->discord_app = v;
         return {};
       }},
      {"--instance", true,
       [](LaunchOptions* o, const std::string& v) -> std::string {
         if (v.size() != 1 || v[0] < '2' || v[0] > '8') return "expected a window number from 2 to 8, got " + Quoted(v);
         o->instance = v[0] - '0';
         return {};
       }},
      {"--discord-game", true, [](LaunchOptions* o, const std::string& v) { return OnOff(&o->discord_game, v); }},
      {"--discord-join", true, [](LaunchOptions* o, const std::string& v) { return OnOff(&o->discord_join, v); }},
      {"--discord-rpc", true, [](LaunchOptions* o, const std::string& v) { return OnOff(&o->discord_rpc, v); }},
      {"--audio-device", true,
       [](LaunchOptions* o, const std::string& v) { return AudioEndpoint(&o->audio_device, v); }},
      {"--audio-input-device", true,
       [](LaunchOptions* o, const std::string& v) { return AudioEndpoint(&o->audio_input_device, v); }},
      {"--timer-resolution-1ms", true,
       [](LaunchOptions* o, const std::string& v) { return OnOff(&o->timer_resolution_1ms, v); }},
      {"--mmcss", true, [](LaunchOptions* o, const std::string& v) { return OnOff(&o->mmcss, v); }},
      {"--priority", true,
       [](LaunchOptions* o, const std::string& v) -> std::string {
         if (v == "normal") {
           o->priority = PriorityRequest::kNormal;
         } else if (v == "above-normal") {
           o->priority = PriorityRequest::kAboveNormal;
         } else if (v == "high") {
           o->priority = PriorityRequest::kHigh;
         } else if (v == "realtime") {
           return "realtime is not allowed: a realtime-priority game can starve the input, audio and "
                  "graphics drivers; use high";
         } else {
           return "expected normal, above-normal or high, got " + Quoted(v);
         }
         return {};
       }},
      {"--power-throttling", true,
       [](LaunchOptions* o, const std::string& v) { return OnOff(&o->power_throttling, v); }},
      {"--no-session-log", false,
       [](LaunchOptions* o, const std::string&) -> std::string {
         o->session_log = false;
         return {};
       }},
      {"--stop-after", true,
       [](LaunchOptions* o, const std::string& v) -> std::string {
         std::int64_t parsed = 0;
         if (!ParseInt64(v, &parsed) || parsed <= 0) {
           return "expected a positive number of milliseconds, got " + Quoted(v);
         }
         o->stop_after_ms = parsed;
         return {};
       }},
      {"--trace", true,
       [](LaunchOptions* o, const std::string& v) -> std::string {
         std::vector<std::string> areas;
         std::size_t begin = 0;
         while (begin <= v.size()) {
           const std::size_t comma = v.find(',', begin);
           const std::string area = v.substr(begin, comma == std::string::npos ? std::string::npos : comma - begin);
           if (area != "jni" && area != "jni-strings" && area != "window") {
             return "expected a comma-separated list of jni, jni-strings and window, got " + Quoted(v);
           }
           areas.push_back(area);
           if (comma == std::string::npos) break;
           begin = comma + 1;
         }
         o->trace = std::move(areas);
         return {};
       }},
  };
  return options;
}

}

CommandLineResult ParseRuntimeCommandLine(const std::vector<std::string>& arguments) {
  CommandLineResult result;
  std::set<std::string> seen;
  bool mode_set = false;
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    const std::string& argument = arguments[index];
    if (argument == "--help" || argument == "-h" || argument == "/?") {
      result.options.mode = RuntimeMode::kHelp;
      return result;
    }
    if (argument == "--version") {
      result.options.mode = RuntimeMode::kVersion;
      return result;
    }
    if (argument == "--check-imports" || argument == "--print-runtime-flags") {
      const RuntimeMode mode =
          argument == "--check-imports" ? RuntimeMode::kCheckImports : RuntimeMode::kPrintRuntimeFlags;
      if (mode_set && result.options.mode != mode) {
        result.error = "--check-imports and --print-runtime-flags cannot be combined";
        return result;
      }
      mode_set = true;
      result.options.mode = mode;
      continue;
    }
    std::string name = argument;
    std::string value;
    bool inline_value = false;
    if (const std::size_t equals = argument.find('='); argument.rfind("--", 0) == 0 && equals != std::string::npos) {
      name = argument.substr(0, equals);
      value = argument.substr(equals + 1);
      inline_value = true;
    }
    const OptionSpec* spec = nullptr;
    for (const OptionSpec& candidate : Options()) {
      if (name == candidate.name) spec = &candidate;
    }
    if (spec == nullptr) {
      if (argument.rfind("-", 0) == 0) {
        result.error = "unknown option " + Quoted(name);
      } else {
        result.error = "unexpected argument " + std::to_string(index + 1) +
                       ": elixir-runtime takes no bare arguments (Roblox website links reach a session through "
                       "Elixir.exe)";
      }
      return result;
    }
    if (!seen.insert(name).second) {
      result.error = name + " is given more than once";
      return result;
    }
    if (spec->takes_value && !inline_value) {
      if (index + 1 >= arguments.size() || arguments[index + 1].rfind("--", 0) == 0) {
        result.error = name + " needs a value";
        return result;
      }
      value = arguments[++index];
    } else if (!spec->takes_value && inline_value) {
      result.error = name + " does not take a value";
      return result;
    }
    const std::string problem = spec->apply(&result.options, value);
    if (!problem.empty()) {
      result.error = name + ": " + problem;
      return result;
    }
  }
  return result;
}

std::vector<std::string> ArgumentsFromWide(int argc, const wchar_t* const argv[]) {
  std::vector<std::string> arguments;
  for (int index = 1; index < argc; ++index) {
    arguments.push_back(argv[index] != nullptr ? Utf8FromWide(argv[index]) : std::string());
  }
  return arguments;
}

std::string RuntimeVersionText() {
  return std::string("elixir-runtime ") + ELIXIR_VERSION +
         " (Elixir: Native Android Gaming for Windows; based on Mocktail)\n";
}

std::string RuntimeUsage() {
  std::ostringstream usage;
  usage << "Usage: elixir-runtime [options]\n"
           "\n"
           "Runs the Android x86-64 Roblox client installed by Elixir.exe. Elixir.exe passes\n"
           "these options when you press Play; they can also be given by hand.\n"
           "\n"
           "Installed app:\n"
           "  --package <name>            Android package (default: config.json installed.package)\n"
           "  --version-code <n>          versionCode the install must have\n"
           "  --app-dir <dir>             install folder (apps\\<package>\\<versionCode>)\n"
           "  --roblox-lib <file>         development override: load this libroblox.so\n"
           "\n"
           "Locations (default: %LOCALAPPDATA%\\Elixir, or .\\data next to the exe in portable mode):\n"
           "  --data-dir <dir>  --config <config.json>  --crash-dir <dir>  --fflags <fflags.json>\n"
           "  --user-fflags on|off        off: start Roblox with none of the Fast Flags file's flags\n"
           "  --instance <2-8>            one of several Roblox windows (its own --data-dir; title \"Elixir (n)\")\n"
           "  --accounts-dir <dir>        saved accounts shared by every Roblox window (default: auth\\accounts)\n"
           "  --activity-dir <dir>        where the games you play are noted for the launcher (default: not noted)\n"
           "  --guest-runtime <dir>       Android-side runtime libraries (default: guest-runtime next to the exe)\n"
           "\n"
           "Settings (default: config.json):\n"
           "  --renderer vulkan  --present-mode fifo|mailbox|immediate  --frame-rate-cap <fps>\n"
           "  --resolution-scale <0.25-1.00>  --stretch <0.00-0.50>  --controller on|off\n"
           "  (no frame-rate cap or window mode: Roblox's own settings decide both)\n"
           "  --shaders-dir <dir>         GLSL shaders drawn over the game (shaders.json says which are on)\n"
           "  --upscale on|off  --sharpness <0.00-1.00>   Elixir's own upscaling of a smaller game picture\n"
           "  --dynamic-resolution on|off  --target-fps <20-480>   lower the scale to hold a frame rate\n"
           "  --wait-spin <0-200>         microseconds a thread watches for its wake before it sleeps (0: never)\n"
           "  --audio-device default|<endpoint id>\n"
           "  --audio-input-device default|disabled|<capture endpoint id>\n"
           "\n"
           "Discord Rich Presence (off unless --discord is given):\n"
           "  --discord <application id>  show what you play in Discord, under this Discord application\n"
           "  --discord-game on|off       the experience's name, icon and buttons (default on)\n"
           "  --discord-join on|off       a Join button on public servers (default off)\n"
           "  --discord-rpc on|off        games may change the text with BloxstrapRPC (default on)\n"
           "\n"
           "Process tuning:\n"
           "  --timer-resolution-1ms on|off   1 ms timer resolution while running\n"
           "  --mmcss on|off                  MMCSS Games/Pro Audio for render/audio threads\n"
           "  --priority normal|above-normal|high   (realtime is refused)\n"
           "  --power-throttling on|off       off opts the process out of power throttling\n"
           "\n"
           "Modes:\n"
           "  --check-imports        list every import of libroblox.so and its libraries that\n"
           "                         Elixir does not provide, then exit\n"
           "  --print-runtime-flags  print the Fast Flags Elixir's runtime policies set (JSON), then exit\n"
           "  --no-session-log       do not write logs\\sessions\\<time>.log\n"
           "  --stop-after <ms>      end the session <ms> after Roblox's start-up returned\n"
           "  --trace <areas>        log more: jni (every pseudo-JVM call), jni-strings, window\n"
           "  --help, --version\n"
           "\n"
           "Exit codes: 0 ok, 2 invalid command line, 3 configuration, 4 Android runtime start-up,\n"
           "  5 no usable installed app, 6 libroblox.so failed to load, 7 unresolved imports,\n"
           "  10 Roblox started up to its window, but the rest of the runtime is not implemented yet.\n";
  return usage.str();
}

std::vector<std::string> FormatRuntimeArguments(const LaunchOptions& options) {
  std::vector<std::string> args;
  const auto add = [&args](const char* name, const std::string& value) {
    args.emplace_back(name);
    args.push_back(value);
  };
  const auto on_off = [](bool value) { return std::string(value ? "on" : "off"); };
  if (!options.package.empty()) add("--package", options.package);
  if (options.version_code) add("--version-code", std::to_string(*options.version_code));
  if (!options.app_dir.empty()) add("--app-dir", NativeUtf8(options.app_dir));
  if (!options.data_dir.empty()) add("--data-dir", NativeUtf8(options.data_dir));
  if (!options.config_file.empty()) add("--config", NativeUtf8(options.config_file));
  if (!options.crash_dir.empty()) add("--crash-dir", NativeUtf8(options.crash_dir));
  if (!options.fflags_file.empty()) add("--fflags", NativeUtf8(options.fflags_file));
  if (!options.mods_dir.empty()) add("--mods-dir", NativeUtf8(options.mods_dir));
  if (!options.accounts_dir.empty()) add("--accounts-dir", NativeUtf8(options.accounts_dir));
  if (!options.activity_dir.empty()) add("--activity-dir", NativeUtf8(options.activity_dir));
  if (!options.shaders_dir.empty()) add("--shaders-dir", NativeUtf8(options.shaders_dir));
  if (options.upscale) add("--upscale", *options.upscale ? "on" : "off");
  if (options.sharpness) {
    char sharpness[32];
    std::snprintf(sharpness, sizeof(sharpness), "%.2f", *options.sharpness);
    add("--sharpness", sharpness);
  }
  if (options.dynamic_resolution) add("--dynamic-resolution", *options.dynamic_resolution ? "on" : "off");
  if (options.target_fps) add("--target-fps", std::to_string(*options.target_fps));
  if (options.wait_spin_us) add("--wait-spin", std::to_string(*options.wait_spin_us));
  if (!options.guest_runtime_dir.empty()) add("--guest-runtime", NativeUtf8(options.guest_runtime_dir));
  if (!options.renderer.empty()) add("--renderer", options.renderer);
  if (!options.present_mode.empty()) add("--present-mode", options.present_mode);
  if (options.frame_rate_cap > 0) add("--frame-rate-cap", std::to_string(options.frame_rate_cap));
  if (options.resolution_scale) {
    char scale[32];
    const double value = *options.resolution_scale;
    std::snprintf(scale, sizeof(scale), "%.2f", IsValidResolutionScale(value) ? value : 1.0);
    add("--resolution-scale", scale);
  }
  if (options.stretch) {
    char stretch[32];
    const double value = *options.stretch;
    std::snprintf(stretch, sizeof(stretch), "%.2f", IsValidStretch(value) ? value : 0.0);
    add("--stretch", stretch);
  }
  if (options.controller) add("--controller", on_off(*options.controller));
  if (options.user_fflags) add("--user-fflags", on_off(*options.user_fflags));
  if (options.instance) add("--instance", std::to_string(*options.instance));
  if (!options.discord_app.empty()) add("--discord", options.discord_app);
  if (options.discord_game) add("--discord-game", on_off(*options.discord_game));
  if (options.discord_join) add("--discord-join", on_off(*options.discord_join));
  if (options.discord_rpc) add("--discord-rpc", on_off(*options.discord_rpc));
  if (!options.audio_device.empty()) add("--audio-device", options.audio_device);
  if (options.timer_resolution_1ms) add("--timer-resolution-1ms", on_off(*options.timer_resolution_1ms));
  if (options.mmcss) add("--mmcss", on_off(*options.mmcss));
  if (!options.audio_input_device.empty()) add("--audio-input-device", options.audio_input_device);
  return args;
}

}
