# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0

if(NOT ELIXIR_LAUNCHER_ROOT)
  set(ELIXIR_LAUNCHER_ROOT "${CMAKE_SOURCE_DIR}")
endif()
set(ELIXIR_LAUNCHER_DIR "${ELIXIR_LAUNCHER_ROOT}/launcher")
set(_elixir_launcher_tests "${ELIXIR_LAUNCHER_ROOT}/tests/windows/launcher")

add_library(elixir_launcher_core STATIC
  ${ELIXIR_LAUNCHER_DIR}/core/accounts.cc
  ${ELIXIR_LAUNCHER_DIR}/core/apk_component.cc
  ${ELIXIR_LAUNCHER_DIR}/core/audio_devices.cc
  ${ELIXIR_LAUNCHER_DIR}/core/bundle.cc
  ${ELIXIR_LAUNCHER_DIR}/core/cache_cleaner.cc
  ${ELIXIR_LAUNCHER_DIR}/core/config.cc
  ${ELIXIR_LAUNCHER_DIR}/core/crash_logs.cc
  ${ELIXIR_LAUNCHER_DIR}/core/diagnostics.cc
  ${ELIXIR_LAUNCHER_DIR}/core/fflags_file.cc
  ${ELIXIR_LAUNCHER_DIR}/core/game_shortcuts.cc
  ${ELIXIR_LAUNCHER_DIR}/core/global_settings.cc
  ${ELIXIR_LAUNCHER_DIR}/core/installer.cc
  ${ELIXIR_LAUNCHER_DIR}/core/instance.cc
  ${ELIXIR_LAUNCHER_DIR}/core/instances.cc
  ${ELIXIR_LAUNCHER_DIR}/core/json.cc
  ${ELIXIR_LAUNCHER_DIR}/core/link_request.cc
  ${ELIXIR_LAUNCHER_DIR}/core/paths.cc
  ${ELIXIR_LAUNCHER_DIR}/core/mods.cc
  ${ELIXIR_LAUNCHER_DIR}/core/notices.cc
  ${ELIXIR_LAUNCHER_DIR}/core/process.cc
  ${ELIXIR_LAUNCHER_DIR}/core/releases.cc
  ${ELIXIR_LAUNCHER_DIR}/core/shortcuts.cc
  ${ELIXIR_LAUNCHER_DIR}/core/roblox_download.cc
  ${ELIXIR_LAUNCHER_DIR}/core/roblox_links.cc
  ${ELIXIR_LAUNCHER_DIR}/core/roblox_update.cc
  ${ELIXIR_LAUNCHER_DIR}/core/sha256.cc
  ${ELIXIR_LAUNCHER_DIR}/core/update.cc
  ${ELIXIR_LAUNCHER_DIR}/core/util.cc
)
elixir_target(elixir_launcher_core)
target_include_directories(elixir_launcher_core
  PUBLIC "${ELIXIR_LAUNCHER_DIR}"
  PRIVATE "${ELIXIR_LAUNCHER_ROOT}/src"
  PRIVATE "${ELIXIR_LAUNCHER_ROOT}/third_party/Vulkan-Headers/include")
target_link_libraries(elixir_launcher_core PUBLIC
  bcrypt winhttp dxgi ole32 oleaut32 shell32 shlwapi advapi32 propsys uuid xmllite windowscodecs version)
include(${ELIXIR_LAUNCHER_ROOT}/cmake/ElixirLaunchLink.cmake)
target_link_libraries(elixir_launcher_core PUBLIC elixir_roblox_launch_link)
if(BUILD_TESTING)
  target_compile_definitions(elixir_launcher_core PRIVATE ELIXIR_SOURCE_DIR="${ELIXIR_LAUNCHER_ROOT}")
endif()

set(ELIXIR_DISCORD_APPLICATION_ID "1575125962696171631" CACHE STRING
  "Discord application ID for Elixir's Rich Presence (empty: none)")
if(NOT ELIXIR_DISCORD_APPLICATION_ID STREQUAL "" AND
   NOT ELIXIR_DISCORD_APPLICATION_ID MATCHES "^[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]([0-9]?[0-9]?[0-9]?)$")
  message(FATAL_ERROR "ELIXIR_DISCORD_APPLICATION_ID must be empty or a Discord application ID (17 to 20 digits)")
endif()
target_compile_definitions(elixir_launcher_core PRIVATE
  ELIXIR_DISCORD_APPLICATION_ID="${ELIXIR_DISCORD_APPLICATION_ID}")

if(TARGET elixir_apk_download)
  target_link_libraries(elixir_launcher_core PUBLIC elixir_apk_download)
  target_compile_definitions(elixir_launcher_core PRIVATE ELIXIR_WITH_ROBLOX_DOWNLOAD=1)
endif()

if(TARGET elixir_apk_tool)
  target_compile_definitions(elixir_launcher_core PRIVATE
    ELIXIR_APK_TOOL_NAME="$<TARGET_FILE_NAME:elixir_apk_tool>"
    ELIXIR_APK_TOOL_BUILD_PATH="$<TARGET_FILE:elixir_apk_tool>")
  add_dependencies(elixir_launcher_core elixir_apk_tool)
  message(STATUS "Elixir launcher: APK component elixir_apk_tool found")
else()
  message(STATUS "Elixir launcher: APK component (elixir_apk_tool) not available; "
                 "Install APK will report it as not built")
endif()

if(EXISTS "${ELIXIR_LAUNCHER_DIR}/ui/app.cc")
  add_library(elixir_launcher_ui STATIC
    ${ELIXIR_LAUNCHER_DIR}/ui/accessibility.cc
    ${ELIXIR_LAUNCHER_DIR}/ui/app.cc
    ${ELIXIR_LAUNCHER_DIR}/ui/app_extras.cc
    ${ELIXIR_LAUNCHER_DIR}/ui/app_links.cc
    ${ELIXIR_LAUNCHER_DIR}/ui/app_roblox.cc
    ${ELIXIR_LAUNCHER_DIR}/ui/installer_pages.cc
    ${ELIXIR_LAUNCHER_DIR}/ui/launch_menu.cc
  ${ELIXIR_LAUNCHER_DIR}/ui/launch_progress.cc
    ${ELIXIR_LAUNCHER_DIR}/ui/pages.cc
    ${ELIXIR_LAUNCHER_DIR}/ui/pages_extras.cc
    ${ELIXIR_LAUNCHER_DIR}/ui/pages_roblox.cc
    ${ELIXIR_LAUNCHER_DIR}/ui/lucide.cc
    ${ELIXIR_LAUNCHER_DIR}/ui/renderer.cc
    ${ELIXIR_LAUNCHER_DIR}/ui/ui.cc
  )
  elixir_target(elixir_launcher_ui)
  target_link_libraries(elixir_launcher_ui PUBLIC
    elixir_launcher_core elixir_fflags d2d1 dwrite windowscodecs dwmapi uxtheme oleacc comctl32 comdlg32 gdi32 user32)
  if(BUILD_TESTING)
    target_compile_definitions(elixir_launcher_ui PRIVATE
      ELIXIR_SOURCE_DIR="${ELIXIR_LAUNCHER_ROOT}")
  endif()

  set(_elixir_download_package_svg "${ELIXIR_LAUNCHER_DIR}/assets/roblox-download-package.svg")
  set(_elixir_download_package_png "${CMAKE_BINARY_DIR}/launcher/roblox-download-package.png")
  add_custom_command(
    OUTPUT "${_elixir_download_package_png}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/launcher"
    COMMAND elixir_svg_rasterize png "${_elixir_download_package_svg}" "${_elixir_download_package_png}" 128
    DEPENDS elixir_svg_rasterize "${_elixir_download_package_svg}"
    COMMENT "Rendering the Roblox download package icon"
    VERBATIM)
  add_custom_target(elixir_launcher_artwork DEPENDS "${_elixir_download_package_png}")

  enable_language(RC)
  file(TO_NATIVE_PATH "${ELIXIR_ICON_ICO}" _elixir_rc_icon)
  file(TO_NATIVE_PATH "${ELIXIR_BRANDING_DIR}/elixir-logo-886x320.png" _elixir_rc_logo)
  file(TO_NATIVE_PATH "${ELIXIR_BRANDING_DIR}/elixir-icon-256.png" _elixir_rc_icon_png)
  file(TO_NATIVE_PATH "${_elixir_download_package_png}" _elixir_rc_download_package)
  file(TO_NATIVE_PATH "${ELIXIR_LAUNCHER_ROOT}/THIRD_PARTY_NOTICES.txt" _elixir_rc_notices)
  file(TO_NATIVE_PATH "${ELIXIR_LAUNCHER_ROOT}/LICENSE" _elixir_rc_license)
  set(ELIXIR_RC_BUNDLE "")
  set(_elixir_rc_bundle_files "")
  set(_elixir_bundle_entries
    "130|${ELIXIR_LAUNCHER_ROOT}/config/roblox_signing_certificates.json"
    "131|${ELIXIR_LAUNCHER_ROOT}/config/roblox_compatibility.json"
    "132|${ELIXIR_LAUNCHER_ROOT}/config/roblox_download_sources.json")
  if(ELIXIR_GUEST_RUNTIME_DIR AND EXISTS "${ELIXIR_GUEST_RUNTIME_DIR}/libelixir-extension.so"
     AND EXISTS "${ELIXIR_GUEST_RUNTIME_DIR}/libz.so")
    list(APPEND _elixir_bundle_entries
      "120|${ELIXIR_GUEST_RUNTIME_DIR}/libelixir-extension.so"
      "121|${ELIXIR_GUEST_RUNTIME_DIR}/libz.so")
  else()
    message(WARNING "Elixir.exe is built without the guest runtime libraries and cannot start Roblox: "
                    "build them with tools/build_guest_runtime.ps1 and set ELIXIR_GUEST_RUNTIME_DIR")
  endif()
  foreach(_entry IN LISTS _elixir_bundle_entries)
    string(REPLACE "|" ";" _parts "${_entry}")
    list(GET _parts 0 _id)
    list(GET _parts 1 _file)
    file(TO_NATIVE_PATH "${_file}" _native)
    string(REPLACE "\\" "\\\\" _native "${_native}")
    string(APPEND ELIXIR_RC_BUNDLE "${_id} RCDATA \"${_native}\"\n")
    list(APPEND _elixir_rc_bundle_files "${_file}")
  endforeach()
  string(REPLACE "\\" "\\\\" ELIXIR_RC_ICON "${_elixir_rc_icon}")
  string(REPLACE "\\" "\\\\" ELIXIR_RC_LOGO "${_elixir_rc_logo}")
  string(REPLACE "\\" "\\\\" ELIXIR_RC_ICON_PNG "${_elixir_rc_icon_png}")
  string(REPLACE "\\" "\\\\" ELIXIR_RC_DOWNLOAD_PACKAGE "${_elixir_rc_download_package}")
  string(REPLACE "\\" "\\\\" ELIXIR_RC_NOTICES "${_elixir_rc_notices}")
  string(REPLACE "\\" "\\\\" ELIXIR_RC_LICENSE "${_elixir_rc_license}")
  set(ELIXIR_RC_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
  set(ELIXIR_RC_VERSION_MINOR ${PROJECT_VERSION_MINOR})
  set(ELIXIR_RC_VERSION_PATCH ${PROJECT_VERSION_PATCH})
  set(ELIXIR_LAUNCHER_RC "${CMAKE_BINARY_DIR}/launcher/Elixir.rc")
  configure_file("${ELIXIR_LAUNCHER_DIR}/Elixir.rc.in" "${ELIXIR_LAUNCHER_RC}" @ONLY)
  set_source_files_properties("${ELIXIR_LAUNCHER_RC}" PROPERTIES
    OBJECT_DEPENDS "${ELIXIR_ICON_ICO};${ELIXIR_BRANDING_DIR}/elixir-logo-886x320.png;${ELIXIR_BRANDING_DIR}/elixir-icon-256.png;${_elixir_download_package_png};${ELIXIR_LAUNCHER_ROOT}/THIRD_PARTY_NOTICES.txt;${ELIXIR_LAUNCHER_ROOT}/LICENSE;${_elixir_rc_bundle_files}")

  add_executable(elixir_launcher WIN32
    ${ELIXIR_LAUNCHER_DIR}/main.cc
    "${ELIXIR_LAUNCHER_RC}"
    ${ELIXIR_LAUNCHER_DIR}/Elixir.manifest)
  elixir_target(elixir_launcher)
  set_target_properties(elixir_launcher PROPERTIES OUTPUT_NAME Elixir)
  target_include_directories(elixir_launcher PRIVATE "${ELIXIR_LAUNCHER_ROOT}/src")
  target_link_libraries(elixir_launcher PRIVATE elixir_launcher_ui elixir_runtime_host elixir_apk_tool_lib shell32)
  target_link_options(elixir_launcher PRIVATE /STACK:16777216)
  add_dependencies(elixir_launcher elixir_branding elixir_launcher_artwork)
endif()

if(BUILD_TESTING)
  elixir_gtest(elixir_launcher_core_test
    ${_elixir_launcher_tests}/accounts_test.cc
    ${_elixir_launcher_tests}/core_test.cc
    ${_elixir_launcher_tests}/update_test.cc
    ${_elixir_launcher_tests}/diagnostics_test.cc
    ${_elixir_launcher_tests}/roblox_features_test.cc
    ${_elixir_launcher_tests}/roblox_update_test.cc
    ${_elixir_launcher_tests}/roblox_links_test.cc
    ${_elixir_launcher_tests}/instance_test.cc
    ${_elixir_launcher_tests}/link_request_test.cc
    ${_elixir_launcher_tests}/instances_test.cc
    ${_elixir_launcher_tests}/installer_test.cc
    ${_elixir_launcher_tests}/releases_test.cc
    ${_elixir_launcher_tests}/mods_shortcuts_test.cc
    ${_elixir_launcher_tests}/notices_test.cc)
  target_link_libraries(elixir_launcher_core_test PRIVATE elixir_launcher_core)
  target_compile_definitions(elixir_launcher_core_test PRIVATE ELIXIR_SOURCE_DIR="${ELIXIR_LAUNCHER_ROOT}")
  if(TARGET elixir_apk_download)
    target_compile_definitions(elixir_launcher_core_test PRIVATE ELIXIR_WITH_ROBLOX_DOWNLOAD=1)
  endif()
  if(TARGET elixir_apk_fixtures)
    add_dependencies(elixir_launcher_core_test elixir_apk_fixtures)
    target_compile_definitions(elixir_launcher_core_test PRIVATE
      ELIXIR_APK_FIXTURE_DIR="${ELIXIR_APK_FIXTURE_DIR}")
  endif()

  if(TARGET elixir_launcher_ui)
    elixir_gtest(elixir_launcher_ui_test
      ${_elixir_launcher_tests}/ui_smoke_test.cc
      ${_elixir_launcher_tests}/launch_menu_test.cc
      ${_elixir_launcher_tests}/motion_test.cc
      ${_elixir_launcher_tests}/lucide_test.cc
      "${ELIXIR_LAUNCHER_RC}"
      ${ELIXIR_LAUNCHER_DIR}/Elixir.manifest)
    target_link_libraries(elixir_launcher_ui_test PRIVATE elixir_launcher_ui)
    add_dependencies(elixir_launcher_ui_test elixir_branding elixir_launcher_artwork elixir_launcher)
    target_compile_definitions(elixir_launcher_ui_test PRIVATE
      ELIXIR_LAUNCHER_SCREENSHOT_DIR="${CMAKE_BINARY_DIR}/launcher-screenshots")
  endif()
endif()
