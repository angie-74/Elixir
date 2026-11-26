# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0
include_guard(GLOBAL)

get_filename_component(_elixir_launch_link_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT TARGET nlohmann_json::nlohmann_json)
  include(FetchContent)
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

add_library(elixir_roblox_launch_link STATIC
  "${_elixir_launch_link_root}/src/runtime/roblox_experience_launch_request.cc"
  "${_elixir_launch_link_root}/src/runtime/roblox_launch_uri.cc"
  "${_elixir_launch_link_root}/src/runtime/url_split.cc")
if(COMMAND elixir_target)
  elixir_target(elixir_roblox_launch_link)
endif()
target_include_directories(elixir_roblox_launch_link PUBLIC "${_elixir_launch_link_root}/include")
target_link_libraries(elixir_roblox_launch_link PUBLIC nlohmann_json::nlohmann_json)
