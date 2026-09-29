#!/usr/bin/env bash
# Reproduces NP-FW-CVNS-001 Rev 9 §14.5 (OI-CVNS-12). Host gcc only; nothing is
# written inside the repository. Variants: window N in {8 (as built), 5}, and
# the 5 s baseline refresh on (as built) or disabled.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
mcu="$here/../.."
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
unit="$mcu/src/np_cardiac_interlock.c"
for pat in '^#define NP_RR_BUF_SIZE   8U' '(now_ms - s_baseline_established_ms) >= NP_CARDIAC_OBS_MS'; do
  grep -qF -- "${pat#^}" "$unit" || { echo "unit changed ($pat): re-derive the variants" >&2; exit 1; }
done
for n in 8 5; do
  for refresh in on off; do
    v="$out/ci_${n}_${refresh}.c"
    { echo "#define SIM_BEATS ${n}U"
      [ "$refresh" = off ] && echo "#define SIM_OBS 100000000U" || echo "#define SIM_OBS NP_CARDIAC_OBS_MS"
      sed -e "s/#define NP_RR_BUF_SIZE   8U/#define NP_RR_BUF_SIZE ${n}U/" \
          -e 's/NP_CARDIAC_BASELINE_BEATS/SIM_BEATS/g' \
          -e 's/(now_ms - s_baseline_established_ms) >= NP_CARDIAC_OBS_MS/(now_ms - s_baseline_established_ms) >= SIM_OBS/' "$unit"
    } > "$v"
    gcc -O2 -std=c11 -D_GNU_SOURCE -I"$mcu/include" -I"$mcu/../common/include" \
        "$here/oi_cvns_12_detect_sim.c" "$v" -lm -o "$out/sim_${n}_${refresh}"
    echo "== N=$n refresh=$refresh — steps (mode 0)"; "$out/sim_${n}_${refresh}" 0
    if [ "$refresh" = on ]; then
      echo "== N=$n refresh=on — false trips (mode 1)"; "$out/sim_${n}_${refresh}" 1
    fi
  done
done
