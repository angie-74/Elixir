// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#include "audio/opensl_exports_windows.h"

#include "mocktail/audio/opensl_playback_runtime.h"

namespace elixir::audio {

void RegisterOpenSlExports(std::unordered_map<std::string, void*>& exports) {
  exports["slCreateEngine"] = reinterpret_cast<void*>(&slCreateEngine);
  exports["slQueryNumSupportedEngineInterfaces"] =
      reinterpret_cast<void*>(&slQueryNumSupportedEngineInterfaces);
  exports["slQuerySupportedEngineInterfaces"] =
      reinterpret_cast<void*>(&slQuerySupportedEngineInterfaces);
  exports["SL_IID_ANDROIDCONFIGURATION"] =
      const_cast<void*>(static_cast<const void*>(&SL_IID_ANDROIDCONFIGURATION));
  exports["SL_IID_ANDROIDSIMPLEBUFFERQUEUE"] =
      const_cast<void*>(static_cast<const void*>(&SL_IID_ANDROIDSIMPLEBUFFERQUEUE));
  exports["SL_IID_BUFFERQUEUE"] = const_cast<void*>(static_cast<const void*>(&SL_IID_BUFFERQUEUE));
  exports["SL_IID_ENGINE"] = const_cast<void*>(static_cast<const void*>(&SL_IID_ENGINE));
  exports["SL_IID_PLAY"] = const_cast<void*>(static_cast<const void*>(&SL_IID_PLAY));
  exports["SL_IID_RECORD"] = const_cast<void*>(static_cast<const void*>(&SL_IID_RECORD));
  exports["SL_IID_VOLUME"] = const_cast<void*>(static_cast<const void*>(&SL_IID_VOLUME));
}

}
