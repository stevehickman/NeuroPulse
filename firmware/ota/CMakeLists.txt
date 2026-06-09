# NeuroPulse OTA — host-testable state machine module
# Document: NP-FW-EMMC-001 Rev A §8.3
#
# This module contains the OTA state record structure, CRC32 validation, and
# boot-attempt counter logic.  It has no hardware dependencies and is compiled
# for the host when NP_BUILD_TESTS=ON (in addition to the target).
#
# The np_ota_state library is a pure-C static library consumed by:
#   • firmware/ota/tests/np_ota_tests  (host unit tests)
#   • firmware/bootloader/             (cross-compiled, uses its own np_types.h
#                                       but shares the identical algorithm)

cmake_minimum_required(VERSION 3.20)
project(np_ota C)

add_library(np_ota_state STATIC src/np_ota_state.c)
target_include_directories(np_ota_state PUBLIC include)
target_compile_options(np_ota_state PRIVATE -Wall -Wextra -Wpedantic)

if(NP_BUILD_TESTS)
    add_executable(np_ota_tests tests/np_ota_tests.c)
    target_link_libraries(np_ota_tests PRIVATE np_ota_state)
    target_compile_options(np_ota_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
