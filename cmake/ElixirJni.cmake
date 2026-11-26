# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0

add_library(elixir_jnivm STATIC
  src/jnivm/jnivm.cc
  src/jnivm/jni_references.cc
  src/jnivm/jni_fields.cc
  src/jnivm/jni_strings.cc
)
elixir_target(elixir_jnivm)
target_include_directories(elixir_jnivm PUBLIC
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/third_party/jni/include")
target_link_libraries(elixir_jnivm PUBLIC elixir_abi elixir_bionic_runtime)

if(BUILD_TESTING)
  set(_jni_test_support "${CMAKE_SOURCE_DIR}/tests/windows/jni/jni_test_support.cc")

  foreach(_mocktail_jni_test IN ITEMS
      jnivm_test
      jnivm_fields_test
      jnivm_message_bus_test
      jnivm_experience_lifecycle_test
      jnivm_native_text_box_info_test
      jnivm_webrtc_audio_manager_test)
    elixir_gtest(elixir_${_mocktail_jni_test} tests/${_mocktail_jni_test}.cc
      ${_jni_test_support})
    target_link_libraries(elixir_${_mocktail_jni_test} PRIVATE elixir_jnivm)
    target_include_directories(elixir_${_mocktail_jni_test} PRIVATE
      "${CMAKE_SOURCE_DIR}/tests/windows/jni")
  endforeach()

  elixir_gtest(elixir_jni_sysv_test tests/windows/jni/jni_sysv_test.cc
    ${_jni_test_support})
  target_link_libraries(elixir_jni_sysv_test PRIVATE elixir_jnivm)
  target_include_directories(elixir_jni_sysv_test PRIVATE
    "${CMAKE_SOURCE_DIR}/src/jnivm"
    "${CMAKE_SOURCE_DIR}/tests/windows/jni")

  elixir_gtest(elixir_jni_guest_test tests/windows/jni/jni_guest_test.cc)
  target_link_libraries(elixir_jni_guest_test PRIVATE elixir_jnivm elixir_linker)
  target_include_directories(elixir_jni_guest_test PRIVATE
    "${CMAKE_SOURCE_DIR}/tests/windows/jni")
  target_compile_definitions(elixir_jni_guest_test PRIVATE
    ELIXIR_GUEST_TEST_DIR="${ELIXIR_GUEST_TEST_DIR}")
endif()
