#!/usr/bin/env bash
#
# check-js-syntax.sh — parse every tracked JavaScript file under simulator/.
#
# simulator/ is the one code surface in this repository with no build, no test
# and no lint of any kind: 9 browser ES modules plus a Node CommonJS server.
# A typo in any of them ships. This is the cheapest gate that is not a lie —
# it proves each file parses, nothing more, and it says so.
#
# ── Why not `node --check <file>` ─────────────────────────────────────────────
# Because it does not work here, and it fails in the direction that matters.
# On Node 24, `node --check simulator/js/ui.js` exits 0 on a file with
# `function broken( {` appended to it. Measured, not assumed. A gate built on
# that form would report success having verified nothing — the exact failure
# this file exists to prevent.
#
# Reading the source on stdin with an explicit `--input-type` is the form that
# actually parses:
#
#     node --input-type=module   --check < file    # exits 1 on bad ESM
#     node --input-type=commonjs --check < file    # exits 1 on bad CJS
#
# ── Why the mode is declared per directory, not sniffed ───────────────────────
# The two halves of simulator/ are genuinely different module systems and each
# file must be parsed the way it is actually loaded:
#
#   simulator/js/**          <script type="module"> in simulator/index.html:141
#   simulator/server/**      require(); its package.json has no "type": "module"
#
# An unclassified file is a hard error rather than a default, so adding a third
# directory fails loudly here instead of being silently skipped.
#
# ── Usage ─────────────────────────────────────────────────────────────────────
#   check-js-syntax.sh              Check every tracked simulator/**/*.js.
#   check-js-syntax.sh --self-test  Prove both modes accept good input and
#                                   REJECT bad input, then exit. CI runs this
#                                   before trusting the checker, for the same
#                                   reason ci-changed-scope.sh does.

set -euo pipefail

np_die() { printf 'check-js-syntax: %s\n' "$*" >&2; exit 2; }

# Print the parse mode for a path, or die if it is not classified.
np_mode_for() {
  case "$1" in
    simulator/js/*)     printf 'module\n' ;;
    simulator/server/*) printf 'commonjs\n' ;;
    *) np_die "unclassified file: $1 (add its directory to np_mode_for)" ;;
  esac
}

# Parse stdin in the given mode. Returns node's exit status.
#
# CI-Kind: gate
# CI-Self-Test: scripts/check-js-syntax.sh --self-test
# CI-Scans: every tracked simulator/**/*.js, in the module system it is loaded in
# CI-Scan-Paths: simulator/**
np_parse() { node "--input-type=$1" --check; }

np_self_test() {
  local failures=0

  # A good and a bad sample for each mode. The bad ones must be rejected; a
  # checker that cannot fail is not a checker.
  local good_module='export const x = 1; import { y } from "./y.js"; console.log(y);'
  local good_cjs='const y = require("y"); module.exports = { y };'
  local bad='function broken( {'

  np_check_case() { # <mode> <source> <expect: pass|fail> <label>
    local mode=$1 src=$2 expect=$3 label=$4 rc=0
    printf '%s\n' "$src" | np_parse "$mode" >/dev/null 2>&1 || rc=$?
    if [ "$expect" = pass ] && [ "$rc" -ne 0 ]; then
      printf 'self-test FAIL: %s — expected parse, got exit %s\n' "$label" "$rc" >&2
      failures=$((failures + 1))
    elif [ "$expect" = fail ] && [ "$rc" -eq 0 ]; then
      printf 'self-test FAIL: %s — bad source parsed clean (gate is vacuous)\n' "$label" >&2
      failures=$((failures + 1))
    fi
  }

  np_check_case module   "$good_module" pass 'valid ESM accepted'
  np_check_case module   "$bad"         fail 'broken ESM rejected'
  np_check_case commonjs "$good_cjs"    pass 'valid CJS accepted'
  np_check_case commonjs "$bad"         fail 'broken CJS rejected'

  # The classifier must refuse a path it does not know, rather than guessing.
  if ( np_mode_for 'some/unknown/file.js' ) >/dev/null 2>&1; then
    printf 'self-test FAIL: unclassified path did not error\n' >&2
    failures=$((failures + 1))
  fi

  if [ "$failures" -ne 0 ]; then
    printf 'check-js-syntax: %s self-test assertion(s) failed\n' "$failures" >&2
    exit 1
  fi
  printf 'check-js-syntax: self-test OK (5 assertions)\n'
}

if [ "${1:-}" = --self-test ]; then
  np_self_test
  exit 0
fi

command -v node >/dev/null 2>&1 || np_die 'node not found on PATH'

files=$(git ls-files 'simulator/**/*.js')
[ -n "$files" ] || np_die 'no tracked simulator/**/*.js files — did the tree move?'

count=0
failed=0
while IFS= read -r f; do
  [ -n "$f" ] || continue
  mode=$(np_mode_for "$f")
  if np_parse "$mode" < "$f" >/dev/null 2>/tmp/np-js-syntax-err; then
    count=$((count + 1))
  else
    printf 'SYNTAX ERROR (%s): %s\n' "$mode" "$f" >&2
    sed -n '1,6p' /tmp/np-js-syntax-err >&2
    failed=$((failed + 1))
  fi
done <<EOF
$files
EOF

# `scanned: <int>` leads the line by contract — scripts/check-gate-coverage.ts
# probes for it, so a PASS always names the population it covered.
printf 'scanned: %s file(s) parsed, %s failed\n' "$count" "$failed"
[ "$failed" -eq 0 ]
