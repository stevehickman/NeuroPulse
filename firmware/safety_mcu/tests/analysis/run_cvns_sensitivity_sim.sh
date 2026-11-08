#!/usr/bin/env bash
# Reproduces NP-FW-CVNS-001 §14.8 (REQ-CVNS-14): what each cardiac-cutoff sensitivity level buys (earlier detection) and
# costs (false trips) through the hub as built (pipeline 1).  Host gcc only; nothing is written inside the repository.  The unit is
# an edited COPY of np_cardiac_interlock.c with the level's threshold written in place of the table lookup, so the sweep matches
# the unit's own comparison; the simulation is oi_cvns_13_false_trip_sim.c, unchanged.
# Usage: run_cvns_sensitivity_sim.sh [mode ...]   (default: 0 1 2; mode 1 and 2 take minutes)
#   level 0 -> NP_CARDIAC_HR_DELTA_BPM, 1 -> NP_CARDIAC_HR_DELTA_L1_BPM, 2 -> NP_CARDIAC_HR_DELTA_L2_BPM
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
mcu="$here/../.."
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
unit="$mcu/src/np_cardiac_interlock.c"
pat='delta_bpm > HR_DELTA_BY_LEVEL[s_sens_cur]'
grep -qF -- "$pat" "$unit" || { echo "unit changed ($pat): re-derive the sweep" >&2; exit 1; }
modes=("$@"); [ ${#modes[@]} -eq 0 ] && modes=(0 1 2)
names=(NP_CARDIAC_HR_DELTA_BPM NP_CARDIAC_HR_DELTA_L1_BPM NP_CARDIAC_HR_DELTA_L2_BPM)
for lvl in 0 1 2; do
  sed -e "s/$(printf '%s' "$pat" | sed 's/[][\.*^$/]/\\&/g')/delta_bpm > ${names[$lvl]}/" "$unit" > "$out/u_$lvl.c"
  gcc -O2 -std=c11 -D_GNU_SOURCE -I"$mcu/include" -I"$mcu/../common/include" \
      "$here/oi_cvns_13_false_trip_sim.c" "$out/u_$lvl.c" -lm -o "$out/sim_$lvl"
done
for m in "${modes[@]}"; do
  for lvl in 0 1 2; do
    echo "== level $lvl (threshold ${names[$lvl]}), mode $m, pipeline 1"
    "$out/sim_$lvl" "$m" 1
  done
done
