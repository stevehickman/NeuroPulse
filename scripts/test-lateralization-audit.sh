#!/usr/bin/env bash
# test-lateralization-audit.sh — behavioural tests for the single-hemisphere-zone audit
# in sync-socket-map.ts.
#
# The audit encodes a design invariant (PR #210: zone membership is inclusive, so a
# single-hemisphere zone leaks cortical energy to the other hemisphere). A gate that
# silently stops firing is worse than no gate, so every accept AND every reject path is
# exercised here against fixture protocols in a temp directory — the real protocol tree
# is never written to.
#
# Run:  bash scripts/test-lateralization-audit.sh

set -uo pipefail
cd "$(dirname "$0")/.."

FIX="$(mktemp -d)"
trap 'rm -rf "$FIX"' EXIT

pass=0
fail=0

# t <name> <expected exit> <protocol source>
t() {
  printf '%s' "$3" > "$FIX/probe.npps"
  bun scripts/sync-socket-map.ts --check --audit-dir "$FIX" >/dev/null 2>&1
  local got=$?
  if [ "$got" = "$2" ]; then
    echo "  ok    $1"
    pass=$((pass + 1))
  else
    echo "  FAIL  $1 (exit $got, expected $2)"
    fail=$((fail + 1))
  fi
}

# bare <name> <expected exit> <args...> — no fixture file written
bare() {
  local name="$1" want="$2"; shift 2
  "$@" >/dev/null 2>&1
  local got=$?
  if [ "$got" = "$want" ]; then
    echo "  ok    $name"
    pass=$((pass + 1))
  else
    echo "  FAIL  $name (exit $got, expected $want)"
    fail=$((fail + 1))
  fi
}

echo "baseline — the real protocol tree must pass"
bare "real repo passes" 0 bun scripts/sync-socket-map.ts --check

echo
echo "detection — what must FAIL"
t "new single-hemisphere protocol"  1 'protocol "P" { pbm_transcranial { zones: ["Parietal Left"] } }'
t "unknown zone name"               1 'protocol "P" { pbm_transcranial { zones: ["Frontal Rihgt"] } }'
t "unrecognised bare selector"      1 'protocol "P" { pbm_transcranial { zones: frontal_right_typo } }'
t "quoted scalar (fails closed)"    1 'protocol "P" { pbm_transcranial { zones: "Frontal Right" } }'
t "unpaired lobe among paired ones" 1 'protocol "P" { pbm_transcranial { zones: ["Frontal Left", "Frontal Right", "Temporal Left"] } }'

echo
echo "tolerance — what must PASS"
t "bilateral pair"                  0 'protocol "P" { pbm_transcranial { zones: ["Parietal Left", "Parietal Right"] } }'
t "narrowed (excl. midline) zone"   0 'protocol "P" { pbm_transcranial { zones: ["Frontal Right (excl. midline)"] } }'
t "clinician_selected bare token"   0 'protocol "P" { pbm_transcranial { zones: clinician_selected } }'
t "whole-region aggregate zone"     0 'protocol "P" { pbm_transcranial { zones: ["Vault (excl. Occipital)"] } }'
t "commented-out zone line" 0 'protocol "P" { pbm_transcranial {
        # was: zones: ["Parietal Right"]
        zones: ["Frontal Left", "Frontal Right"] } }'
t "hemispheres in separate blocks" 0 'protocol "P" {
    pbm_transcranial { zones: ["Frontal Left"] }
    pbm_transcranial { zones: ["Frontal Right"] } }'
t "non-PBM block is out of scope"   0 'protocol "P" { tdcs { zones: ["Parietal Left"] } }'

echo
echo "scope safety — the gate must never pass vacuously"
rm -f "$FIX/probe.npps"
bare "empty fixture dir is exit 2"     2 bun scripts/sync-socket-map.ts --check --audit-dir "$FIX"
bare "missing dir is exit 2"           2 bun scripts/sync-socket-map.ts --check --audit-dir /nonexistent-np
bare "--audit-dir needs --check"       2 bun scripts/sync-socket-map.ts --audit-dir "$FIX"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
