#!/usr/bin/env bash
# NeurOne Firmware-in-the-Loop Simulator — line coverage of the device's own sources under the simulator's scenarios.
# NP-SIM-FW-001 REQ-SIMFW-13.  HOST ONLY.
#
#   firmware/platform_sim/tools/sim-coverage.sh [source ...]
#
# Builds the simulator with --coverage in a scratch directory, runs every scenario test, and prints the percentage of
# executable lines each named firmware source reached.  Sources are paths under firmware/, for example
# hub_control/modules/np_mod_stim.c.  With none named it reports every module source the simulator links.
# Needs gcc, cmake and gcov.  This is a measurement of what the scenarios reach, not a verification of the code
# (REQ-SIMFW-06).
set -euo pipefail

root="$(cd "$(dirname "$0")/../../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cmake -S "$root/firmware" -B "$work" -DNP_BUILD_TESTS=ON -DNP_BUILD_SIM=ON -DCMAKE_C_FLAGS="--coverage" >/dev/null
cmake --build "$work" --target np_sim_run_tests -j"$(nproc)" >/dev/null

objdir="$work/platform_sim/CMakeFiles/np_sim_pbm.dir"
if [ "$#" -eq 0 ]; then
  set -- hub_control/modules/np_mod_pbm.c hub_control/modules/np_mod_stim.c hub_control/src/np_stim_xcheck.c \
         hub_control/src/np_socket_dispatch.c hub_control/src/np_module_map.c \
         safety_mcu/src/np_fault_latch.c safety_mcu/src/np_nv_state.c safety_mcu/src/np_session_sig.c \
         safety_mcu/src/np_cardiac_interlock.c safety_mcu/src/np_thermal_interlock.c safety_mcu/src/np_charge_monitor.c \
         safety_mcu/src/np_safety_main.c safety_mcu/src/np_tier_identity.c safety_mcu/src/np_impedance_check.c \
         safety_mcu/src/np_spi_watchdog.c safety_mcu/src/np_gpio_mgr.c
fi

printf '%-52s %s\n' "source" "lines reached"
for src in "$@"; do
  gcda="$(find "$objdir" -name "$(basename "$src").gcda" -path "*$(dirname "$src" | sed 's|^[^/]*/||')*" | head -n1)"
  if [ -z "$gcda" ]; then
    printf '%-52s %s\n' "$src" "not linked or never run"
    continue
  fi
  pct="$(cd "$(dirname "$gcda")" && gcov -n "$gcda" 2>/dev/null | grep -A1 "File '.*/$(basename "$src")'" | sed -n 's/Lines executed:\(.*\)/\1/p' | head -n1)"
  printf '%-52s %s\n' "$src" "${pct:-unknown}"
done
