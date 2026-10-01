#!/usr/bin/env bash
# Reproduces NP-FW-CVNS-001 Rev 10 §14.5.1 (OI-CVNS-12, closed): detection and
# false-trip figures for the cardiac interlock AS BUILT, the lagged comparison
# (8-interval mean vs every 1 s snapshot of it in the last 12 s). Host gcc only;
# nothing is written inside the repository.
#
# The Rev 9 figures for the unconditional 5 s refresh it replaced (window 8 and
# 5, refresh on and off) are reproduced by this script at commit 69ea5e7.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
mcu="$here/../.."
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
gcc -O2 -std=c11 -D_GNU_SOURCE -I"$mcu/include" -I"$mcu/../common/include" \
    "$here/oi_cvns_12_detect_sim.c" "$mcu/src/np_cardiac_interlock.c" -lm -o "$out/sim"
echo "== as built — steps (mode 0)";       "$out/sim" 0
echo "== as built — false trips (mode 1)"; "$out/sim" 1
