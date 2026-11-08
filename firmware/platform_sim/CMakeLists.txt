# NeurOne Firmware-in-the-Loop Simulator — sim platform layer (HOST ONLY)
# Document: NP-SIM-FW-001 §3.2; this is the PBM spike (§8 step 1)
#
# REQ-SIMFW-01: this directory is never built for the device.  firmware/platform/
# is the layer a device image links, and it is cross-compile only; this is the
# layer a host process links, and it is host only.  The guard below is the whole
# of that property until the CI check (REQ-SIMFW-02's census, §8 step 2) exists.
#
# REQ-SIMFW-03: every firmware source below is the device's own file, listed by
# its path under firmware/.  Nothing is copied, and nothing under firmware/
# outside this directory carries a simulator conditional.

cmake_minimum_required(VERSION 3.20)

if(CMAKE_CROSSCOMPILING)
    return()
endif()

if(NOT NP_BUILD_TESTS OR NOT NP_BUILD_SIM)
    return()
endif()

set(NP_SIM_INCLUDES
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/include   # declarations only, as the hub host tests use it
    ${CMAKE_CURRENT_SOURCE_DIR}/../shdr/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../hub_control/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../pbm/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../hrv_biofeedback/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../cervical_vns/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../crypto/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    # headers only: np_safety_spi.c and np_session_runner.h name FreeRTOS types.  The kernel is not
    # linked in the spike (single-threaded); the real scheduler arrives with the task layer (§8 step 3).
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/freertos/kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/freertos/portable/ThirdParty/GCC/Posix
)

# The production governor refuses every load and the simulator keeps it (OI-SIMFW-11).  It is
# linked under another name so the thin np_pbm_power_admit in np_sim_spike_link.c can sit in
# front of it and carry the host-test-only opening; the same rename np_socket_dispatch_tests uses.
set_source_files_properties(../hub_control/src/np_pbm_power_gov.c PROPERTIES
    COMPILE_DEFINITIONS "np_pbm_power_admit=np_pbm_power_admit_production")

# The device's own sources the PBM socket path runs.
set(NP_SIM_PBM_FIRMWARE
    ../hub_control/src/np_socket_dispatch.c
    ../hub_control/src/np_module_map.c
    ../hub_control/src/np_protocol.c
    ../hub_control/src/np_pbm_power_gov.c
    ../hub_control/src/np_safety_spi.c
    ../hub_control/src/np_socket_layouts.c
    ../hub_control/src/np_layout_bind.c
    ../hub_control/src/np_socket_layouts.generated.c
    ../hub_control/src/np_pbm_socket_telem.c
    ../hub_control/modules/np_mod_pbm.c
    # stimulation (BES/tACS and tDCS): the driver, the commanded-versus-delivered cross-check and the ramp model
    ../hub_control/modules/np_mod_stim.c
    ../hub_control/modules/np_mod_eeg.c
    # auricular VNS, and the HRV session library it drives (pure C: no kernel, no platform seam)
    ../hub_control/modules/np_mod_vns.c
    ../hub_control/modules/np_mod_audio.c
    ../hub_control/modules/np_mod_visual.c
    ../hub_control/src/np_ppx_policy.c   # np_mod_visual.c calls np_ppx_zone_permitted / np_ppx_seeing
    ../hub_control/src/np_ppx_detect.c
    ../hub_control/modules/np_mod_intranasal.c
    ../hub_control/modules/np_mod_t2_stubs.c
    # cervical VNS: the driver, the safety library it integrates and the re-enable manager
    ../hub_control/modules/np_mod_cvns.c
    ../cervical_vns/src/np_cvns_interlock.c
    ../cervical_vns/src/np_cvns_session.c
    ../cervical_vns/src/np_cvns_stim.c
    ../hub_control/src/np_cvns_reenable.c
    ../hub_control/src/np_cvns_fault_summary.c
    ../hrv_biofeedback/src/np_hrv_session.c
    ../hrv_biofeedback/src/np_hrv_ppg.c
    ../hrv_biofeedback/src/np_hrv_coherence.c
    ../hrv_biofeedback/src/np_hrv_pacer.c
    ../hrv_biofeedback/src/np_hrv_tavns_sync.c
    ../hrv_biofeedback/src/np_hrv_eeg_biofeedback.c
    ../hub_control/src/np_stim_xcheck.c
    ../hub_control/src/np_ramp_plan.c
    ../pbm/src/np_pbm_detect.c
    ../pbm/src/np_pbm_drive.c
    ../pbm/src/np_pbm_dose.c
)

# The HRV library's own -Wextra finding (an unused parameter in np_hrv_eeg_biofeedback.c) is not this layer's to fix
# and does not belong in this layer's build output.
set_source_files_properties(../hrv_biofeedback/src/np_hrv_eeg_biofeedback.c PROPERTIES COMPILE_OPTIONS "-Wno-unused-parameter")

# The safety MCU, in the loop (NP-SIM-FW-001 §22, OI-SIMFW-09): its own sources, listed by their paths under firmware/, with
# np_safety_main.c's main() renamed by a compile definition on that one file so it can run on a thread.  No firmware file is
# edited (REQ-SIMFW-03).  The platform/ drivers (register level) are not here: src/np_sim_safety_hal.c defines the np_hal_* seams.
set(NP_SIM_SAFETY_FIRMWARE
    ../safety_mcu/src/np_safety_main.c
    ../safety_mcu/src/np_gpio_mgr.c
    ../safety_mcu/src/np_spi_watchdog.c
    ../safety_mcu/src/np_charge_monitor.c
    ../safety_mcu/src/np_thermal_interlock.c
    ../safety_mcu/src/np_cardiac_interlock.c
    ../safety_mcu/src/np_impedance_check.c
    ../safety_mcu/src/np_session_sig.c
    ../safety_mcu/src/np_fault_latch.c
    ../safety_mcu/src/np_nv_state.c
    ../safety_mcu/src/np_tier_identity.c
)
set_source_files_properties(../safety_mcu/src/np_safety_main.c PROPERTIES COMPILE_DEFINITIONS "main=np_sim_safety_main")
# np_gpio_mgr_tests' own device: GPIOA and GPIOB as two opaque host addresses, force-included; the tier authority key is the TEST
# key np_tier_identity_tests signs with, force-included the same way (REQ-SIMFW-08: test keys only).
set(NP_SIM_SAFETY_PORTS ${CMAKE_CURRENT_SOURCE_DIR}/../safety_mcu/tests/np_gpio_mgr_test_ports.h)
set(NP_SIM_SAFETY_TIERKEY ${CMAKE_CURRENT_SOURCE_DIR}/../safety_mcu/tests/np_tier_test_key.h)
foreach(f ${NP_SIM_SAFETY_FIRMWARE})
    set_source_files_properties(${f} PROPERTIES
        INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/../safety_mcu/include;${CMAKE_CURRENT_SOURCE_DIR}/../crypto/include;${CMAKE_CURRENT_SOURCE_DIR}/../common/include"
        COMPILE_OPTIONS "-std=gnu11;-include;${NP_SIM_SAFETY_PORTS};-Wno-unused-parameter")
endforeach()
set_property(SOURCE ../safety_mcu/src/np_tier_identity.c APPEND PROPERTY COMPILE_OPTIONS "-include;${NP_SIM_SAFETY_TIERKEY}")
set_source_files_properties(src/np_sim_safety_hal.c PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/../safety_mcu/include;${CMAKE_CURRENT_SOURCE_DIR}/../crypto/include;${CMAKE_CURRENT_SOURCE_DIR}/../crypto/vendor/monocypher;${CMAKE_CURRENT_SOURCE_DIR}/../common/include;${CMAKE_CURRENT_SOURCE_DIR}/src"
    COMPILE_OPTIONS "-include;${NP_SIM_SAFETY_PORTS}")

add_library(np_sim_pbm STATIC
    src/np_sim_bus.c
    src/np_sim_safety_hal.c
    src/np_sim_safety_link.c
    src/np_sim_safety_harness.c
    ${NP_SIM_SAFETY_FIRMWARE}
    src/np_sim_pbm.c
    src/np_sim_stim.c
    src/np_sim_stim_harness.c
    src/np_sim_eeg.c
    src/np_sim_eeg_harness.c
    src/np_sim_vns.c
    src/np_sim_vns_harness.c
    src/np_sim_audio.c
    src/np_sim_audio_harness.c
    src/np_sim_visual.c
    src/np_sim_visual_harness.c
    src/np_sim_intranasal.c
    src/np_sim_intranasal_harness.c
    src/np_sim_stubs_harness.c
    src/np_sim_cvns.c
    src/np_sim_cvns_harness.c
    src/np_sim_unrouted.c
    src/np_sim_pbm_harness.c
    src/np_sim_spike_link.c
    ${NP_SIM_PBM_FIRMWARE}
)
target_include_directories(np_sim_pbm PUBLIC ${NP_SIM_INCLUDES})
target_compile_definitions(np_sim_pbm PUBLIC NP_FREERTOS_POSIX_HOST _GNU_SOURCE)
target_compile_options(np_sim_pbm PRIVATE -std=gnu11 -Wall -Wextra -O0 -g -DNPTEST_HOST)
target_link_libraries(np_sim_pbm PUBLIC np_crypto m pthread)

# The spike process: stdin text commands in, JSON lines out.
add_executable(np_sim_pbm_spike spike/np_sim_pbm_spike.c)
target_link_libraries(np_sim_pbm_spike PRIVATE np_sim_pbm)
target_compile_options(np_sim_pbm_spike PRIVATE -std=gnu11 -Wall -Wextra -O0 -g)

# The tests drive the same library directly.
add_executable(np_sim_pbm_tests tests/np_sim_pbm_tests.c)
target_link_libraries(np_sim_pbm_tests PRIVATE np_sim_pbm)
target_compile_options(np_sim_pbm_tests PRIVATE -std=gnu11 -Wall -Wextra -O0 -g)
# Not add_test(): CI's host-test partition, counts and coverage floors (NP-SW-CI-001 §6.10, §6.14)
# do not know this target, and it is opt-in (NP_BUILD_SIM).  Run it as:  ctest -R np_sim  via the
# custom target below.  NP-SIM-FW-001 OI-SIMFW-13 owns putting it in the partition.
add_executable(np_sim_stim_tests tests/np_sim_stim_tests.c)
target_link_libraries(np_sim_stim_tests PRIVATE np_sim_pbm)
target_compile_options(np_sim_stim_tests PRIVATE -std=gnu11 -Wall -Wextra -O0 -g)
add_executable(np_sim_safety_tests tests/np_sim_safety_tests.c)
target_link_libraries(np_sim_safety_tests PRIVATE np_sim_pbm)
target_compile_options(np_sim_safety_tests PRIVATE -std=gnu11 -Wall -Wextra -O0 -g)
add_executable(np_sim_eeg_tests tests/np_sim_eeg_tests.c)
target_link_libraries(np_sim_eeg_tests PRIVATE np_sim_pbm)
target_compile_options(np_sim_eeg_tests PRIVATE -std=gnu11 -Wall -Wextra -O0 -g)
add_executable(np_sim_vns_tests tests/np_sim_vns_tests.c)
target_link_libraries(np_sim_vns_tests PRIVATE np_sim_pbm)
target_compile_options(np_sim_vns_tests PRIVATE -std=gnu11 -Wall -Wextra -O0 -g)
add_executable(np_sim_audio_tests tests/np_sim_audio_tests.c)
target_link_libraries(np_sim_audio_tests PRIVATE np_sim_pbm)
target_compile_options(np_sim_audio_tests PRIVATE -std=gnu11 -Wall -Wextra -O0 -g)
add_executable(np_sim_visual_tests tests/np_sim_visual_tests.c)
target_link_libraries(np_sim_visual_tests PRIVATE np_sim_pbm)
target_compile_options(np_sim_visual_tests PRIVATE -std=gnu11 -Wall -Wextra -O0 -g)
add_executable(np_sim_intranasal_tests tests/np_sim_intranasal_tests.c)
target_link_libraries(np_sim_intranasal_tests PRIVATE np_sim_pbm)
target_compile_options(np_sim_intranasal_tests PRIVATE -std=gnu11 -Wall -Wextra -O0 -g)
add_executable(np_sim_stubs_tests tests/np_sim_stubs_tests.c)
target_link_libraries(np_sim_stubs_tests PRIVATE np_sim_pbm)
target_compile_options(np_sim_stubs_tests PRIVATE -std=gnu11 -Wall -Wextra -O0 -g)
add_executable(np_sim_cvns_tests tests/np_sim_cvns_tests.c)
target_link_libraries(np_sim_cvns_tests PRIVATE np_sim_pbm)
target_compile_options(np_sim_cvns_tests PRIVATE -std=gnu11 -Wall -Wextra -O0 -g)
add_custom_target(np_sim_run_tests
    COMMAND np_sim_pbm_tests
    COMMAND np_sim_stim_tests
    COMMAND np_sim_eeg_tests
    COMMAND np_sim_vns_tests
    COMMAND np_sim_audio_tests
    COMMAND np_sim_visual_tests
    COMMAND np_sim_intranasal_tests
    COMMAND np_sim_stubs_tests
    COMMAND np_sim_cvns_tests
    COMMAND np_sim_safety_tests
    DEPENDS np_sim_pbm_tests np_sim_stim_tests np_sim_eeg_tests np_sim_vns_tests np_sim_audio_tests np_sim_visual_tests np_sim_intranasal_tests np_sim_stubs_tests np_sim_cvns_tests np_sim_safety_tests
    COMMENT "Running the simulator scenario tests")
