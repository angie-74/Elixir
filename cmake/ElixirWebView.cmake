# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0

if(NOT TARGET elixir_roblox_experience)
  return()
endif()

set(ELIXIR_WEBVIEW2_SDK_DIR "" CACHE PATH
  "Folder with an unpacked Microsoft.Web.WebView2 package (empty: download the pinned package)")
set(_elixir_webview2_version "1.0.4370.26")
set(_elixir_webview2_sha512
  "eb2faf9a00433ffd5879266242df494528f7c397df7622ffbaefd2c08437852832bea727446ba784ebf01861b958eac3a75e741ba30ff098d11bc02d5b9159e7")

set(_elixir_webview2_sdk "${ELIXIR_WEBVIEW2_SDK_DIR}")
if(_elixir_webview2_sdk STREQUAL "")
  set(_elixir_webview2_root "${CMAKE_BINARY_DIR}/_deps/webview2")
  set(_elixir_webview2_sdk "${_elixir_webview2_root}/sdk")
  if(NOT EXISTS "${_elixir_webview2_sdk}/build/native/include/WebView2.h" OR
     NOT EXISTS "${_elixir_webview2_sdk}/build/native/x64/WebView2LoaderStatic.lib")
    set(_elixir_webview2_package
      "${_elixir_webview2_root}/microsoft.web.webview2.${_elixir_webview2_version}.zip")
    message(STATUS "Elixir: downloading Microsoft.Web.WebView2 ${_elixir_webview2_version} from nuget.org")
    file(DOWNLOAD
      "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/${_elixir_webview2_version}/microsoft.web.webview2.${_elixir_webview2_version}.nupkg"
      "${_elixir_webview2_package}"
      EXPECTED_HASH SHA512=${_elixir_webview2_sha512}
      TLS_VERIFY ON
      STATUS _elixir_webview2_status)
    list(GET _elixir_webview2_status 0 _elixir_webview2_code)
    if(NOT _elixir_webview2_code EQUAL 0)
      message(FATAL_ERROR
        "The WebView2 SDK could not be downloaded (${_elixir_webview2_status}). "
        "Unpack Microsoft.Web.WebView2 ${_elixir_webview2_version} yourself and pass "
        "-DELIXIR_WEBVIEW2_SDK_DIR=<folder>.")
    endif()
    file(ARCHIVE_EXTRACT INPUT "${_elixir_webview2_package}" DESTINATION "${_elixir_webview2_sdk}"
      PATTERNS "build/native/include/WebView2.h" "build/native/include/WebView2EnvironmentOptions.h"
               "build/native/x64/WebView2LoaderStatic.lib" "LICENSE.txt" "NOTICE.txt")
  endif()
endif()
if(NOT EXISTS "${_elixir_webview2_sdk}/build/native/include/WebView2.h" OR
   NOT EXISTS "${_elixir_webview2_sdk}/build/native/x64/WebView2LoaderStatic.lib")
  message(FATAL_ERROR "No WebView2 SDK in ${_elixir_webview2_sdk}")
endif()

add_library(elixir_webview2_sdk INTERFACE)
target_include_directories(elixir_webview2_sdk SYSTEM INTERFACE "${_elixir_webview2_sdk}/build/native/include")
target_link_libraries(elixir_webview2_sdk INTERFACE
  "${_elixir_webview2_sdk}/build/native/x64/WebView2LoaderStatic.lib"
  version advapi32 ole32 shell32 shlwapi user32)

add_library(elixir_webview_host STATIC
  src/webview/webview_helper_policy.cc
  src/webview/webview_host_windows.cc
)
elixir_target(elixir_webview_host)
target_include_directories(elixir_webview_host PUBLIC "${CMAKE_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(elixir_webview_host
  PUBLIC elixir_roblox_experience nlohmann_json::nlohmann_json
  PRIVATE elixir_webview2_sdk dwmapi)

if(TARGET elixir_launcher)
  target_link_libraries(elixir_launcher PRIVATE elixir_webview_host)
  target_compile_definitions(elixir_launcher PRIVATE ELIXIR_HAS_WEBVIEW=1)
endif()

if(BUILD_TESTING)
  elixir_gtest(elixir_webview_test
    tests/windows/runtime/webview_policy_test.cc
    tests/windows/runtime/webview_host_test.cc)
  target_link_libraries(elixir_webview_test PRIVATE elixir_webview_host)
  if(TARGET elixir_launcher)
    add_dependencies(elixir_webview_test elixir_launcher)
    target_compile_definitions(elixir_webview_test PRIVATE
      ELIXIR_WEBVIEW_TEST_PROGRAM="$<TARGET_FILE:elixir_launcher>")
  endif()
endif()
