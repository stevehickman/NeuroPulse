#!/usr/bin/env bash
# Reproduces NP-FW-CVNS-001 Rev 11 §14.6 (OI-CVNS-13): false trips and detection
# for the cardiac interlock as built and for each lever, through each R-peak
# pipeline (0 raw detections, 1 hub as built, 2 hub pulse gate without its
# 2000 ms upper bound).  Host gcc + perl only; nothing is written inside the
# repository.  The levers are edited COPIES of np_cardiac_interlock.c:
#   built    the unit as built
#   refr300  the MCU ignores an edge < 300 ms after the last one it accepted
#   median   the current HR is the median of the 8 intervals, not their mean
#   persist  the excursion must hold for 1 s (history 19, horizon +1 s)
#   win12    a 12-interval window (history 24: 1 + 5 + 12 x 60/40)
# Usage: run_oi_cvns_13_sim.sh [lever ...]   (default: all)
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
mcu="$here/../.."
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
unit="$mcu/src/np_cardiac_interlock.c"
for pat in '#define NP_RR_BUF_SIZE   8U' 'if (np_hal_rpeak_edge_pending()) {' \
           'if (delta_bpm > NP_CARDIAC_HR_DELTA_BPM && !s_cutoff_active) {' \
           'int16_t cur_bpm = current_hr_bpm();' '/* History: restart from one snapshot'; do
  grep -qF -- "$pat" "$unit" || { echo "unit changed ($pat): re-derive the levers" >&2; exit 1; }
done

variant() {  # $1 lever -> writes $out/u_$1.c
  local v="$out/u_$1.c"
  case "$1" in
    built) cp "$unit" "$v" ;;
    refr300) perl -0pe '
      s{(np_safe_status_t np_cardiac_interlock_init\(void\))}{static bool sim_accept_edge(void)
{
    if (s_first_beat_seen && (np_hal_tim2_get_capture() - s_last_capture) < 300000U) {
        np_hal_rpeak_edge_clear();
        return false;
    }
    return true;
}

$1};
      s{if \(np_hal_rpeak_edge_pending\(\)\) \{}{if (np_hal_rpeak_edge_pending() && sim_accept_edge()) \{};' "$unit" > "$v" ;;
    median) perl -0pe '
      s{(/\* History: restart from one snapshot)}{static int16_t sim_median_bpm(void)
{
    uint32_t a[NP_RR_BUF_SIZE];
    uint8_t  n = (s_rr_count < NP_RR_BUF_SIZE) ? s_rr_count : NP_RR_BUF_SIZE;
    if (n == 0U) { return 0; }
    for (uint8_t i = 0U; i < n; i++) { a[i] = s_rr_buf[i]; }
    for (uint8_t i = 1U; i < n; i++) { uint32_t x = a[i]; int j = i - 1;
        while (j >= 0 && a[j] > x) { a[j + 1] = a[j]; j--; } a[j + 1] = x; }
    uint32_t m = (n % 2U) ? a[n / 2U] : (a[n / 2U - 1U] + a[n / 2U]) / 2U;
    return rr_to_bpm(m);
}

$1};
      s{int16_t cur_bpm = current_hr_bpm\(\);}{int16_t cur_bpm = sim_median_bpm();};
      s{hist_seed\(current_hr_bpm\(\), now_ms\)}{hist_seed(sim_median_bpm(), now_ms)};' "$unit" > "$v" ;;
    persist) { echo '#include "np_safety_config.h"'
               echo '#undef NP_CARDIAC_HR_HIST_LEN'
               echo '#define NP_CARDIAC_HR_HIST_LEN 19U'
               perl -0pe '
      s{(np_safe_status_t np_cardiac_interlock_init\(void\))}{static bool     sim_over;
static uint32_t sim_over_ms;
static bool sim_persist(bool over, uint32_t now_ms)
{
    if (!over) { sim_over = false; return false; }
    if (!sim_over) { sim_over = true; sim_over_ms = now_ms; }
    return (now_ms - sim_over_ms) >= 1000U;
}

$1};
      s{if \(delta_bpm > NP_CARDIAC_HR_DELTA_BPM && !s_cutoff_active\) \{}{if (sim_persist(delta_bpm > NP_CARDIAC_HR_DELTA_BPM, now_ms) && !s_cutoff_active) \{};' "$unit"; } > "$v" ;;
    win12) { echo '#include "np_safety_config.h"'
             echo '#undef NP_CARDIAC_HR_HIST_LEN'
             echo '#define NP_CARDIAC_HR_HIST_LEN 24U'
             echo '#undef NP_CARDIAC_BASELINE_BEATS'
             echo '#define NP_CARDIAC_BASELINE_BEATS 12U'
             sed -e 's/#define NP_RR_BUF_SIZE   8U/#define NP_RR_BUF_SIZE  12U/' "$unit"; } > "$v" ;;
    *) echo "unknown lever $1" >&2; exit 2 ;;
  esac
  gcc -O2 -std=c11 -D_GNU_SOURCE -I"$mcu/include" -I"$mcu/../common/include" \
      "$here/oi_cvns_13_false_trip_sim.c" "$v" -lm -o "$out/sim_$1"
}

levers=("$@"); [ ${#levers[@]} -eq 0 ] && levers=(built refr300 median persist win12)
for l in "${levers[@]}"; do
  variant "$l"
  for p in 0 1 2; do
    echo "== $l, pipeline $p — steps (mode 0)";          "$out/sim_$l" 0 "$p"
    echo "== $l, pipeline $p — false trips, white (1)";  "$out/sim_$l" 1 "$p"
    echo "== $l, pipeline $p — false trips, RSA (2)";    "$out/sim_$l" 2 "$p"
    echo "== $l, pipeline $p — dropped beats (3)";       "$out/sim_$l" 3 "$p"
  done
done
