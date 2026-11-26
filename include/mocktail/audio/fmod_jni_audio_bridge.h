// Modified by Elixir.
#ifndef MOCKTAIL_AUDIO_FMOD_JNI_AUDIO_BRIDGE_H_
#define MOCKTAIL_AUDIO_FMOD_JNI_AUDIO_BRIDGE_H_

#include "mocktail/status.h"

#if defined(_WIN32)
#include <string_view>
#endif

namespace jnivm {
class VM;
}

namespace mocktail::audio {

Status InstallFmodJniAudioBridge(jnivm::VM* vm);

#if defined(_WIN32)
Status InstallFmodJniAudioBridge(jnivm::VM* vm, std::string_view output_device,
                                bool fallback_to_default = false);
#endif

Status ShutdownFmodJniAudioBridge(jnivm::VM* vm);

}

#endif
