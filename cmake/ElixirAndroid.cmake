# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0

if(NOT TARGET elixir_bionic)
  message(FATAL_ERROR "ElixirAndroid.cmake must be included after elixir_bionic is defined")
endif()

set(ELIXIR_ANDROID_SRC "${CMAKE_SOURCE_DIR}/src/android")

add_library(elixir_android STATIC
  "${ELIXIR_ANDROID_SRC}/register.cc"
  "${ELIXIR_ANDROID_SRC}/native_window.cc"
  "${ELIXIR_ANDROID_SRC}/looper.cc"
  "${ELIXIR_ANDROID_SRC}/input.cc"
  "${ELIXIR_ANDROID_SRC}/assets.cc"
  "${ELIXIR_ANDROID_SRC}/configuration.cc"
  "${ELIXIR_ANDROID_SRC}/choreographer.cc"
  "${ELIXIR_ANDROID_SRC}/sensors.cc"
  "${ELIXIR_ANDROID_SRC}/system.cc"
  "${ELIXIR_ANDROID_SRC}/egl.cc"
  "${ELIXIR_ANDROID_SRC}/gles.cc"
  "${ELIXIR_ANDROID_SRC}/mediandk.cc"
  "${ELIXIR_ANDROID_SRC}/media_decoder.cc"
  "${CMAKE_SOURCE_DIR}/src/libc_shim/vulkan_etc1_sky_transcoder.cc"
)
elixir_target(elixir_android)
target_include_directories(elixir_android PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(elixir_android PUBLIC
  elixir_bionic elixir_linker elixir_platform_windows zlibstatic
  user32 gdi32 dxgi dwmapi advapi32 mfplat mfuuid wmcodecdspuuid ole32 delayimp)
target_link_options(elixir_android INTERFACE "/DELAYLOAD:mfplat.dll")
if(TARGET elixir_vulkan)
  target_link_libraries(elixir_android PUBLIC elixir_vulkan)
  target_compile_definitions(elixir_android PUBLIC ELIXIR_ANDROID_WITH_VULKAN=1)
endif()

if(BUILD_TESTING)
  add_executable(elixir_android_test "${CMAKE_SOURCE_DIR}/tests/windows/android/android_test.cc")
  elixir_target(elixir_android_test)
  target_link_libraries(elixir_android_test PRIVATE elixir_android GTest::gtest)
  target_compile_definitions(elixir_android_test PRIVATE
    ELIXIR_GUEST_TEST_DIR="${ELIXIR_GUEST_TEST_DIR}")
  add_test(NAME elixir_android COMMAND elixir_android_test)
  set_tests_properties(elixir_android PROPERTIES TIMEOUT 300)
  if(TARGET elixir_runtime_test AND TARGET elixir_jnivm)
    target_sources(elixir_runtime_test PRIVATE "${CMAKE_SOURCE_DIR}/tests/windows/runtime/video_codec_discovery_test.cc")
  endif()
endif()

if(TARGET elixir_runtime_core)
  target_link_libraries(elixir_runtime_core PUBLIC elixir_android)
  target_compile_definitions(elixir_runtime_core PRIVATE ELIXIR_RUNTIME_HAS_ANDROID=1)
endif()
