// Modified by Elixir.
#ifndef MOCKTAIL_AUDIO_WEBRTC_JNI_AUDIO_BRIDGE_H_
#define MOCKTAIL_AUDIO_WEBRTC_JNI_AUDIO_BRIDGE_H_

#include "mocktail/status.h"

#if defined(_WIN32)
#include <functional>
#include <string>
#include <string_view>
#endif

namespace jnivm {
class VM;
}

namespace mocktail::audio {

Status InstallWebRtcJniAudioBridge(jnivm::VM* vm);
#if defined(_WIN32)
struct WebRtcMicrophonePicker {
  std::function<Status(std::wstring_view, std::string*)> select;
  bool route_configured = false;
};

Status InstallWebRtcJniAudioBridge(jnivm::VM* vm, std::string_view input_device, bool allow_device_name = true,
                                  WebRtcMicrophonePicker* picker = nullptr);
#endif
Status ShutdownWebRtcJniAudioBridge(jnivm::VM* vm);

}

#endif
