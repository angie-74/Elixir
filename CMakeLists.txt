# Copyright 2026 Mocktail Project Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Modified by Elixir.

cmake_minimum_required(VERSION 3.20)
project(Elixir
  VERSION 0.1.15
  DESCRIPTION "Native Android Gaming for Windows"
  LANGUAGES CXX
)

include(GNUInstallDirs)
find_package(Threads REQUIRED)

set(MOCKTAIL_BUILD_GIT_COMMIT "unknown")
set(MOCKTAIL_BUILD_GIT_DIRTY "unknown")
find_package(Git QUIET)
if(Git_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE MOCKTAIL_GIT_REV_PARSE_RESULT
    OUTPUT_VARIABLE MOCKTAIL_GIT_REV_PARSE_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(MOCKTAIL_GIT_REV_PARSE_RESULT EQUAL 0 AND
     MOCKTAIL_GIT_REV_PARSE_OUTPUT MATCHES "^[0-9a-fA-F]+$")
    string(TOLOWER "${MOCKTAIL_GIT_REV_PARSE_OUTPUT}"
      MOCKTAIL_BUILD_GIT_COMMIT)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=normal
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      RESULT_VARIABLE MOCKTAIL_GIT_STATUS_RESULT
      OUTPUT_VARIABLE MOCKTAIL_GIT_STATUS_OUTPUT
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    if(MOCKTAIL_GIT_STATUS_RESULT EQUAL 0)
      if(MOCKTAIL_GIT_STATUS_OUTPUT STREQUAL "")
        set(MOCKTAIL_BUILD_GIT_DIRTY "false")
      else()
        set(MOCKTAIL_BUILD_GIT_DIRTY "true")
      endif()
    endif()
  endif()
endif()

set(MOCKTAIL_DEFAULT_COMPATIBILITY_MANIFEST
  "${CMAKE_SOURCE_DIR}/config/roblox_compatibility.json"
  CACHE FILEPATH
  "Fallback compatibility manifest used outside an installed runtime"
)
set(MOCKTAIL_DEFAULT_SIGNING_TRUST_MANIFEST
  "${CMAKE_SOURCE_DIR}/config/roblox_signing_certificates.json"
  CACHE FILEPATH
  "Fallback signing trust manifest used outside an installed runtime"
)
set(MOCKTAIL_DEFAULT_DOWNLOAD_SOURCES_MANIFEST
  "${CMAKE_SOURCE_DIR}/config/roblox_download_sources.json"
  CACHE FILEPATH "Pinned Roblox APK download sources"
)
set(MOCKTAIL_DISCORD_APPLICATION_ID "1537088975720812655" CACHE STRING
  "Discord application ID baked into Mocktail Rich Presence releases"
)
string(LENGTH "${MOCKTAIL_DISCORD_APPLICATION_ID}"
  MOCKTAIL_DISCORD_APPLICATION_ID_LENGTH)
if(NOT MOCKTAIL_DISCORD_APPLICATION_ID STREQUAL "" AND
   (NOT MOCKTAIL_DISCORD_APPLICATION_ID MATCHES "^[0-9]+$" OR
    MOCKTAIL_DISCORD_APPLICATION_ID_LENGTH LESS 17 OR
    MOCKTAIL_DISCORD_APPLICATION_ID_LENGTH GREATER 20))
  message(FATAL_ERROR
    "MOCKTAIL_DISCORD_APPLICATION_ID must be empty or a Discord snowflake")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
  include(${CMAKE_SOURCE_DIR}/cmake/ElixirWindows.cmake)
  return()
endif()

message(FATAL_ERROR
  "Elixir builds on Windows. For Linux, use Mocktail, which Elixir is built "
  "on: https://github.com/komaruworld/mocktail")
