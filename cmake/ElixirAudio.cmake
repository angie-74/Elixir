# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0

include_guard(GLOBAL)

set(ELIXIR_AUDIO_SRC "${CMAKE_SOURCE_DIR}/src/audio")

add_library(elixir_audio STATIC
  "${ELIXIR_AUDIO_SRC}/audio_sink.cc"
  "${ELIXIR_AUDIO_SRC}/pcm_converter.cc"
  "${ELIXIR_AUDIO_SRC}/pcm_render_queue.cc"
  "${ELIXIR_AUDIO_SRC}/wasapi_audio_windows.cc"
  "${ELIXIR_AUDIO_SRC}/opensl_simple_buffer_queue.cc"
  "${ELIXIR_AUDIO_SRC}/opensl_playback_runtime.cc"
  "${ELIXIR_AUDIO_SRC}/opensl_recorder.cc"
  "${ELIXIR_AUDIO_SRC}/opensl_exports_windows.cc"
  "${ELIXIR_AUDIO_SRC}/fmod_java_audio_runtime.cc"
  "${ELIXIR_AUDIO_SRC}/audio_device_menu_windows.cc"
  "${ELIXIR_AUDIO_SRC}/roblox_output_device_bridge.cc"
)
elixir_target(elixir_audio)
target_include_directories(elixir_audio PUBLIC
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(elixir_audio PUBLIC elixir_platform_windows ole32 avrt)

if(TARGET elixir_jnivm)
  add_library(elixir_audio_jni STATIC
    "${ELIXIR_AUDIO_SRC}/fmod_jni_audio_bridge.cc"
    "${ELIXIR_AUDIO_SRC}/webrtc_jni_audio_bridge.cc"
  )
  elixir_target(elixir_audio_jni)
  target_link_libraries(elixir_audio_jni PUBLIC elixir_audio elixir_jnivm)
endif()

if(BUILD_TESTING)
  set(ELIXIR_AUDIO_TESTS "${CMAKE_SOURCE_DIR}/tests/windows/audio")

  elixir_gtest(elixir_audio_unit_test
    "${ELIXIR_AUDIO_TESTS}/pcm_converter_test.cc"
    "${ELIXIR_AUDIO_TESTS}/pcm_render_queue_test.cc"
    "${ELIXIR_AUDIO_TESTS}/opensl_queue_adapter_test.cc")
  target_link_libraries(elixir_audio_unit_test PRIVATE elixir_audio elixir_bionic_runtime)

  elixir_gtest(elixir_fmod_java_audio_runtime_test
    "${CMAKE_SOURCE_DIR}/tests/fmod_java_audio_runtime_test.cc")
  target_link_libraries(elixir_fmod_java_audio_runtime_test PRIVATE elixir_audio)

  elixir_gtest(elixir_audio_opensl_test
    "${ELIXIR_AUDIO_TESTS}/opensl_export_test.cc")
  target_link_libraries(elixir_audio_opensl_test PRIVATE elixir_audio elixir_bionic_runtime)

  elixir_gtest(elixir_audio_wasapi_test
    "${ELIXIR_AUDIO_TESTS}/wasapi_device_test.cc")
  target_link_libraries(elixir_audio_wasapi_test PRIVATE elixir_audio elixir_bionic_runtime)

  elixir_gtest(elixir_audio_lifecycle_test
    "${ELIXIR_AUDIO_TESTS}/audio_lifecycle_test.cc")
  target_link_libraries(elixir_audio_lifecycle_test PRIVATE elixir_audio elixir_bionic_runtime psapi)
  target_link_options(elixir_audio_lifecycle_test PRIVATE /STACK:16777216)

  elixir_gtest(elixir_audio_guest_test
    "${ELIXIR_AUDIO_TESTS}/opensl_guest_test.cc")
  target_link_libraries(elixir_audio_guest_test PRIVATE elixir_audio elixir_linker elixir_bionic_runtime)
  target_compile_definitions(elixir_audio_guest_test PRIVATE
    ELIXIR_GUEST_TEST_DIR="${ELIXIR_GUEST_TEST_DIR}")

  if(TARGET elixir_audio_jni)
    elixir_gtest(elixir_audio_jni_test
      "${ELIXIR_AUDIO_TESTS}/jni_audio_bridge_test.cc")
    target_link_libraries(elixir_audio_jni_test PRIVATE elixir_audio_jni elixir_bionic_runtime)
  endif()
endif()

if(TARGET elixir_runtime_core)
  target_link_libraries(elixir_runtime_core PUBLIC elixir_audio)
  target_compile_definitions(elixir_runtime_core PRIVATE ELIXIR_RUNTIME_HAS_OPENSL=1)
  if(TARGET elixir_audio_jni)
    target_link_libraries(elixir_runtime_core PUBLIC elixir_audio_jni)
    target_compile_definitions(elixir_runtime_core PRIVATE ELIXIR_RUNTIME_HAS_AUDIO_JNI=1)
  endif()
endif()
