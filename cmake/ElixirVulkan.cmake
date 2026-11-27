# Copyright 2026 Elixir Project Authors
# Licensed under the Apache License, Version 2.0

set(ELIXIR_VULKAN_SRC "${CMAKE_SOURCE_DIR}/src/graphics/vulkan")

add_library(elixir_vulkan STATIC
  "${ELIXIR_VULKAN_SRC}/generated/vk_sysv_thunks.cc"
  "${ELIXIR_VULKAN_SRC}/guest_vulkan.cc"
  "${ELIXIR_VULKAN_SRC}/device_order.cc"
  "${ELIXIR_VULKAN_SRC}/extension_policy.cc"
  "${ELIXIR_VULKAN_SRC}/present_policy.cc"
  "${ELIXIR_VULKAN_SRC}/etc2_decoder.cc"
  "${ELIXIR_VULKAN_SRC}/etc2_emulation.cc"
  "${ELIXIR_VULKAN_SRC}/texture_override.cc"
)
elixir_target(elixir_vulkan)
set_source_files_properties("${ELIXIR_VULKAN_SRC}/etc2_decoder.cc" PROPERTIES
  COMPILE_OPTIONS "$<$<CONFIG:Release,RelWithDebInfo>:/Ob2>")
target_include_directories(elixir_vulkan SYSTEM PUBLIC
  "${CMAKE_SOURCE_DIR}/third_party/Vulkan-Headers/include")
target_link_libraries(elixir_vulkan PUBLIC elixir_platform_windows windowscodecs ole32)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_custom_target(elixir_vulkan_regenerate_thunks
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/gen_vulkan_thunks.py"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Regenerating src/graphics/vulkan/generated from vk.xml"
    VERBATIM)
endif()

if(BUILD_TESTING)
  set(ELIXIR_VULKAN_TESTS "${CMAKE_SOURCE_DIR}/tests/windows/vulkan")

  elixir_gtest(elixir_vulkan_unit_test
    "${ELIXIR_VULKAN_TESTS}/vulkan_thunk_table_test.cc"
    "${ELIXIR_VULKAN_TESTS}/vulkan_extension_policy_test.cc")
  target_link_libraries(elixir_vulkan_unit_test PRIVATE elixir_vulkan)

  elixir_gtest(elixir_vulkan_integration_test
    "${ELIXIR_VULKAN_TESTS}/vulkan_guest_integration_test.cc")
  target_link_libraries(elixir_vulkan_integration_test PRIVATE
    elixir_vulkan elixir_linker elixir_bionic_runtime user32)
  target_compile_definitions(elixir_vulkan_integration_test PRIVATE
    ELIXIR_GUEST_TEST_DIR="${ELIXIR_GUEST_TEST_DIR}")

  elixir_gtest(elixir_vulkan_etc2_unit_test
    "${ELIXIR_VULKAN_TESTS}/vulkan_etc2_decoder_test.cc"
    "${ELIXIR_VULKAN_TESTS}/vulkan_etc2_emulation_test.cc")
  target_link_libraries(elixir_vulkan_etc2_unit_test PRIVATE elixir_vulkan)

  elixir_gtest(elixir_vulkan_etc2_integration_test
    "${ELIXIR_VULKAN_TESTS}/vulkan_etc2_integration_test.cc")
  target_link_libraries(elixir_vulkan_etc2_integration_test PRIVATE
    elixir_vulkan elixir_linker elixir_bionic_runtime)
  target_compile_definitions(elixir_vulkan_etc2_integration_test PRIVATE
    ELIXIR_GUEST_TEST_DIR="${ELIXIR_GUEST_TEST_DIR}")

  if(Python3_Interpreter_FOUND)
    add_test(NAME elixir_vulkan_thunks_up_to_date
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/gen_vulkan_thunks.py" --check
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  endif()

  get_filename_component(_elixir_llvm_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
  find_program(ELIXIR_LLVM_OBJDUMP NAMES llvm-objdump HINTS "${_elixir_llvm_bin}" NO_DEFAULT_PATH)
  if(Python3_Interpreter_FOUND AND ELIXIR_LLVM_OBJDUMP)
    add_test(NAME elixir_vulkan_thunk_codegen
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/check_vulkan_thunk_codegen.py"
              "${ELIXIR_LLVM_OBJDUMP}" "$<TARGET_FILE:elixir_vulkan>" "$<CONFIG>")
    set_tests_properties(elixir_vulkan_thunk_codegen PROPERTIES SKIP_RETURN_CODE 77)
  endif()
endif()
