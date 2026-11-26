# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0
if(TARGET elixir_discord_rpc)
  return()
endif()

include(FetchContent)
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

add_library(elixir_discord_rpc STATIC
  src/discord_rpc/discord_rpc_client.cc
  src/discord_rpc/entities.cc
  src/discord_rpc/helper.cc
  src/discord_rpc/io.cc
  src/discord_rpc/json.cc
  src/discord_rpc/logging.cc
  src/discord_rpc/message.cc
  src/discord_rpc/registry.cc
  src/discord_rpc/rpc.cc
)
elixir_target(elixir_discord_rpc)
target_link_libraries(elixir_discord_rpc PUBLIC nlohmann_json::nlohmann_json advapi32)

if(BUILD_TESTING)
  elixir_gtest(elixir_discord_rpc_test
    tests/windows/discord_rpc/recorded_test.cc
    tests/windows/discord_rpc/scenario_test.cc
    tests/windows/discord_rpc/unit_test.cc)
  target_link_libraries(elixir_discord_rpc_test PRIVATE elixir_discord_rpc)
  target_compile_definitions(elixir_discord_rpc_test PRIVATE
    "ELIXIR_DISCORD_RPC_FIXTURE=\"${CMAKE_SOURCE_DIR}/tests/windows/discord_rpc/discord-rpc-csharp-1.6.1.70.json\"")
endif()
