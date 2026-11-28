# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0

include(FetchContent)

if(NOT TARGET elixir_utf8proc)
  FetchContent_Declare(
    elixir_utf8proc_src
    URL https://github.com/JuliaStrings/utf8proc/archive/refs/tags/v2.9.0.tar.gz
    URL_HASH SHA256=18c1626e9fc5a2e192311e36b3010bfc698078f692888940f1fa150547abb0c1
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR elixir-not-a-cmake-project
  )
  FetchContent_MakeAvailable(elixir_utf8proc_src)
  add_library(elixir_utf8proc STATIC "${elixir_utf8proc_src_SOURCE_DIR}/utf8proc.c")
  set_target_properties(elixir_utf8proc PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_compile_definitions(elixir_utf8proc PUBLIC UTF8PROC_STATIC)
  target_compile_options(elixir_utf8proc PRIVATE /W0 /utf-8
    $<$<CONFIG:Release,RelWithDebInfo>:/O2>)
  target_include_directories(elixir_utf8proc PUBLIC "${elixir_utf8proc_src_SOURCE_DIR}")
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
  FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  set(JSON_BuildTests OFF CACHE INTERNAL "")
  set(JSON_Install OFF CACHE INTERNAL "")
  FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(elixir_window STATIC
  src/window/window_pointer_capture_owner.cc
  src/window/window_text_input_owner.cc
  src/window/window_resize_readiness_gate.cc
  src/window/window_surface_lifecycle.cc
  src/window/window_fullscreen_request_gate.cc
  src/window/roblox_fullscreen_menu_request_gate.cc
  src/window/window_state_store.cc
  src/window/win32_keyboard.cc
  src/window/win32_gamepad.cc
  src/window/win32_text_input.cc
  src/window/win32_text_clipboard.cc
  src/window/win32_game_window.cc
  src/window/win32_window_api.cc
)
elixir_target(elixir_window)
target_include_directories(elixir_window PUBLIC
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/src/window/sdl3_compat")
target_link_libraries(elixir_window
  PUBLIC elixir_fflags nlohmann_json::nlohmann_json
  PRIVATE user32 gdi32 imm32 dwmapi)

add_library(elixir_input_runtime STATIC
  src/runtime/roblox_input_router.cc
  src/runtime/roblox_gamepad_input.cc
  src/runtime/roblox_text_editor.cc
  src/runtime/roblox_text_display_state.cc
  src/runtime/roblox_text_font_resolver.cc
  src/runtime/roblox_text_surface_overlay_windows.cc
  src/window/win32_text_overlay_window.cc
)
elixir_target(elixir_input_runtime)
target_link_libraries(elixir_input_runtime
  PUBLIC elixir_window elixir_utf8proc
  PRIVATE d2d1 dwrite user32 gdi32)

if(TARGET elixir_jnivm)
  add_library(elixir_roblox_input STATIC
    src/runtime/owned_pthread.cc
    src/runtime/game_session_coordinator.cc
    src/runtime/roblox_app_lifecycle.cc
    src/runtime/roblox_capability_resolver.cc
    src/runtime/roblox_game_session_native_adapter.cc
    src/runtime/roblox_native_text_box_info_reader.cc
    src/runtime/roblox_input_native_adapter.cc
    src/runtime/roblox_window_input_runtime.cc
    src/runtime/roblox_text_input_jni_bridge.cc
  )
  elixir_target(elixir_roblox_input)
  target_link_libraries(elixir_roblox_input PUBLIC elixir_input_runtime elixir_jnivm elixir_platform_windows)
endif()

if(TARGET elixir_roblox_input)
  add_library(elixir_roblox_experience STATIC
    src/runtime/roblox_experience_launch_bridge.cc
    src/runtime/roblox_fresh_game_launch_controller.cc
    src/window/window_game_surface_bridge.cc
    src/runtime/roblox_message_bus_request_bridge.cc
    src/runtime/roblox_account_protocol.cc
    src/runtime/roblox_permissions_bridge.cc
    src/runtime/roblox_call_protocol_bridge.cc
    src/runtime/roblox_web_view_bridge.cc
    src/runtime/roblox_browser_service_bridge.cc
    src/runtime/jnivm_platform_web_callbacks.cc
    src/runtime/webview_roblox_cookie.cc
    src/runtime/webview_helper_launcher.cc
    src/runtime/external_launch_broker.cc
    src/runtime/auth_runtime_composition.cc
    src/runtime/roblox_experience_composition.cc
  )
  elixir_target(elixir_roblox_experience)
  include(${CMAKE_SOURCE_DIR}/cmake/ElixirLaunchLink.cmake)
  target_link_libraries(elixir_roblox_experience PUBLIC
    elixir_roblox_input elixir_runtime_mocktail elixir_utf8proc
    elixir_roblox_launch_link nlohmann_json::nlohmann_json bcrypt)
endif()

if(TARGET elixir_runtime_core)
  target_link_libraries(elixir_runtime_core PUBLIC elixir_input_runtime)
  if(TARGET elixir_roblox_input)
    target_sources(elixir_runtime_core PRIVATE
      src/elixir_runtime/game_session.cc
      src/elixir_runtime/stall_profiler.cc)
    target_link_libraries(elixir_runtime_core PUBLIC elixir_roblox_input)
    target_compile_definitions(elixir_runtime_core PRIVATE ELIXIR_RUNTIME_HAS_WINDOW=1)
    if(TARGET elixir_runtime_test)
      target_sources(elixir_runtime_test PRIVATE tests/windows/runtime/frame_rate_cap_test.cc)
    endif()
  endif()
  if(TARGET elixir_roblox_experience)
    include(${CMAKE_SOURCE_DIR}/cmake/ElixirDiscordRpc.cmake)
    target_sources(elixir_runtime_core PRIVATE
      src/runtime/roblox_platform_web_symbols.cc
      src/elixir_runtime/experience_session.cc
      src/elixir_runtime/activity_journal.cc
      src/elixir_runtime/dynamic_resolution.cc
      src/elixir_runtime/frame_times.cc
      src/elixir_runtime/discord_presence.cc)
    target_link_libraries(elixir_runtime_core PUBLIC elixir_roblox_experience elixir_discord_rpc)
    target_compile_definitions(elixir_runtime_core PRIVATE ELIXIR_RUNTIME_HAS_EXPERIENCE=1)
  endif()
endif()

if(BUILD_TESTING)
  elixir_gtest(elixir_window_mocktail_test
    tests/input_pump_pacer_test.cc
    tests/roblox_fullscreen_menu_request_gate_test.cc
    tests/window_fullscreen_request_gate_test.cc
    tests/window_pointer_capture_owner_test.cc
    tests/window_resize_readiness_gate_test.cc
    tests/window_surface_lifecycle_test.cc
    tests/window_text_input_owner_test.cc
    tests/vulkan_surface_recovery_gate_test.cc
    tests/vulkan_present_observer_test.cc
    tests/vulkan_present_progress_gate_test.cc
    tests/roblox_gamepad_input_test.cc
    tests/roblox_text_editor_test.cc
    tests/roblox_text_display_state_test.cc)
  target_link_libraries(elixir_window_mocktail_test PRIVATE elixir_input_runtime)

  elixir_gtest(elixir_window_input_test
    tests/windows/window/keyboard_mapping_test.cc
    tests/windows/window/gamepad_mapping_test.cc
    tests/windows/window/game_window_test.cc
    tests/windows/window/window_state_store_windows_test.cc
    tests/windows/window/window_lifetime_test.cc
    tests/windows/window/text_overlay_window_test.cc)
  target_link_libraries(elixir_window_input_test PRIVATE elixir_input_runtime)

  if(TARGET elixir_roblox_experience)
    elixir_gtest(elixir_roblox_experience_mocktail_test
      tests/game_session_coordinator_test.cc
      tests/roblox_game_session_native_adapter_test.cc
      tests/roblox_experience_launch_bridge_test.cc
      tests/roblox_fresh_game_launch_controller_test.cc
      tests/window_game_surface_bridge_test.cc)
    target_link_libraries(elixir_roblox_experience_mocktail_test PRIVATE elixir_roblox_experience)

    elixir_gtest(elixir_roblox_platform_test
      tests/roblox_experience_composition_test.cc
      tests/roblox_permissions_bridge_test.cc
      tests/roblox_web_view_bridge_test.cc
      tests/roblox_browser_service_bridge_test.cc
      tests/webview_roblox_cookie_test.cc
      tests/roblox_launch_uri_test.cc
      tests/windows/runtime/platform_protocols_test.cc)
    target_link_libraries(elixir_roblox_platform_test PRIVATE elixir_roblox_experience)

    if(TARGET elixir_runtime_test)
      target_sources(elixir_runtime_test PRIVATE
        tests/windows/runtime/link_joins_test.cc
        tests/windows/runtime/activity_journal_test.cc
        tests/windows/runtime/dynamic_resolution_test.cc
        tests/windows/runtime/frame_times_test.cc
        tests/windows/runtime/discord_presence_test.cc
        tests/windows/runtime/stall_profiler_test.cc)
    endif()
  endif()
endif()
