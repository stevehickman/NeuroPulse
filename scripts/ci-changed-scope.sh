#!/usr/bin/env bash
#
# ci-changed-scope.sh — decide whether a change is in scope for a workflow.
#
# NP-SW-CI-001 §6.7 (phase 6).  This exists so a workflow can stop using a
# workflow-level `paths:` filter and still build only what the change could have
# affected.  The distinction matters for exactly one reason:
#
#   A workflow suppressed by `paths:` never reports a check at all, and a
#   REQUIRED check that never reports renders as "Expected — waiting for status"
#   and blocks the pull request forever.  A job suppressed by a job-level `if:`
#   reports `conclusion: skipped`, which satisfies a required check.
#
# So the relevance lists move out of `on: { push, pull_request }: paths:` and
# into a `changes` job that calls this script.  §5.0 is unchanged — nothing
# irrelevant is compiled or tested — only the reporting surface moves.
#
# Deliberately NOT a third-party action (dorny/paths-filter and friends).  This
# repository has just completed a Class C SOUP exercise (NP-SW-CI-001 §9,
# NP-SOUP-CMSIS-001); importing new supply-chain surface for something `git diff`
# does in a few lines is a bad trade, and unlike an action this file is reviewed,
# diffed and unit-tested in the same PR as the code it gates.
#
# ── Pattern shapes ────────────────────────────────────────────────────────────
# EXACTLY TWO shapes are supported, which is all the retired `paths:` lists ever
# used:
#
#   prefix/**        every file at or below prefix/
#   some/exact/path  that one path, byte for byte
#
# Anything else — `firmware/*.c`, `**/x`, a bare `**` — is a hard ERROR (exit 2)
# rather than a silent non-match.  That direction is deliberate: a pattern shape
# this script does not understand must be loud, because the failure mode of
# quietly matching nothing is a gate that reports green while building nothing.
# If a future list needs a third shape, teach it here and add a self-test.
#
# ── Fail-safe direction ───────────────────────────────────────────────────────
# Callers must treat ANY error, empty output, or unknown base revision as
# "relevant" and build.  `--relevant` only ever prints `false` on a definite,
# successful negative.  See the `if:` conditions in the calling workflows, which
# are written `!= 'false'` rather than `== 'true'` for this reason.
#
# ── Usage ─────────────────────────────────────────────────────────────────────
#   ci-changed-scope.sh --self-test
#       Run the built-in unit assertions.  Non-zero on any failure.  Every
#       `changes` job runs this before trusting the matcher, so a broken matcher
#       fails CI loudly instead of skipping every build silently.
#
#   ci-changed-scope.sh --match    <patterns-file> <files-file>
#       Print the subset of <files-file> matched by <patterns-file>.
#
#   ci-changed-scope.sh --relevant <patterns-file> <files-file>
#       Print `true` if any file matched, else `false`.
#
#   ci-changed-scope.sh --diff-base <event> <event-base> <head> <default-branch> <ref>
#       Print the revision to diff <head> against, or NOTHING when the scope of
#       the change is unknown (the caller then builds).  See np_diff_base.
#
# Pattern files may contain blank lines and `#` comments, so the relevance list
# can carry its reasoning inline next to the entries it explains.
#
# CI-Kind: gate
# CI-Self-Test: scripts/ci-changed-scope.sh --self-test
# CI-Scan-Probe: external — the matcher runs against relevance lists a CI job generates
# CI-Scans: workflow relevance lists against the changed-file set
# CI-Scan-Paths: .github/workflows/**

set -euo pipefail

np_die() { printf 'ci-changed-scope: %s\n' "$*" >&2; exit 2; }

# np_match_one <pattern> <path>
# 0 = match, 1 = no match, exit 2 = unsupported pattern shape.
np_match_one() {
  pattern=$1
  path=$2

  case $pattern in
    '') np_die "empty pattern" ;;
    */'**')
      # Keep the trailing slash: `firmware/bootloader/` must not match
      # `firmware/bootloader_notes.md`.  This is the boundary bug that a naive
      # `${pattern%\*\*}` plus prefix test gets right only by accident, so it is
      # asserted in --self-test.
      prefix=${pattern%'**'}
      case $path in
        "$prefix"*) return 0 ;;
        *)          return 1 ;;
      esac
      ;;
    *'*'*|*'?'*|*'['*|*']'*|*'!'*)
      np_die "unsupported pattern shape: '$pattern' (only 'prefix/**' and exact paths)" ;;
    *)
      # Exact match.  Note this is a string comparison, not a regex: a literal
      # `.` in `firmware/CMakeLists.txt` must not act as a wildcard.  Asserted.
      [ "$path" = "$pattern" ]
      ;;
  esac
}

# np_read_patterns <file> — echoes one cleaned pattern per line; errors if empty.
np_read_patterns() {
  file=$1
  [ -f "$file" ] || np_die "pattern file not found: $file"
  count=0
  while IFS= read -r line || [ -n "$line" ]; do
    line=${line%%#*}
    # strip surrounding whitespace (bash 3.2 compatible)
    while case $line in ' '*|"$(printf '\t')"*) true ;; *) false ;; esac; do line=${line# }; line=${line#	}; done
    while case $line in *' '|*"$(printf '\t')") true ;; *) false ;; esac; do line=${line% }; line=${line%	}; done
    [ -n "$line" ] || continue
    printf '%s\n' "$line"
    count=$((count + 1))
  done < "$file"
  # An empty relevance list must never mean "nothing is relevant" — that would
  # skip every build while every check went green.
  [ "$count" -gt 0 ] || np_die "pattern file has no patterns: $file"
}

np_match() {
  patterns_file=$1
  files_file=$2
  [ -f "$files_file" ] || np_die "file list not found: $files_file"

  patterns=$(np_read_patterns "$patterns_file")

  while IFS= read -r path || [ -n "$path" ]; do
    [ -n "$path" ] || continue
    printf '%s\n' "$patterns" | {
      while IFS= read -r pattern || [ -n "$pattern" ]; do
        [ -n "$pattern" ] || continue
        if np_match_one "$pattern" "$path"; then
          printf '%s\n' "$path"
          break
        fi
      done
    }
  done < "$files_file"
}

# np_diff_base <event> <event-base> <head> <default-branch> <ref>
# Print the merge base to diff <head> against; print nothing when it is unknown.
#
# Every `changes` job used to carry this inline, and every copy compared a push
# against `github.event.before`.  That is right for an ordinary commit and wrong
# for a "merge main into branch" commit: the diff from the branch's previous tip
# to the merge is everything main gained in the meantime, so a docs-only branch
# ran the web build because main had touched scripts/sync-locales.ts (run
# 35930198671, PR #408).  The pull_request event for the same head scoped it
# correctly, because it diffs against the base.  A push to a feature branch now
# asks the same question the pull request does — what does this branch change
# relative to the default branch — so the two events can no longer disagree.
#
#   pull_request                 <event-base> (the PR's base sha)
#   push to the default branch   <event-base> (`before`): main has no base but
#                                itself, and diffing it against origin/main would
#                                find nothing and skip every job
#   push to any other branch     origin/<default-branch>; <event-base> only if
#                                that ref is missing.  Also covers a branch's
#                                FIRST push, whose `before` is all zeros
#   anything else                unknown — workflow_dispatch is a human asking
#                                for a build, and gets one
#
# The consequence for a push is deliberate: the diff is the WHOLE branch, not the
# commits in this push, so a later docs-only commit on a branch that touched
# app/web earlier still builds the web app.  That is the pull request's answer
# too, and it errs toward building.
np_diff_base() {
  event=$1 event_base=$2 head=$3 default_branch=$4 ref=$5
  zero=0000000000000000000000000000000000000000
  git cat-file -e "${head}^{commit}" 2>/dev/null || return 0

  candidates=""
  case $event in
    pull_request) candidates=$event_base ;;
    push)
      if [ -n "$default_branch" ] && [ "$ref" = "refs/heads/$default_branch" ]; then
        candidates=$event_base
      else
        candidates="origin/$default_branch $event_base"
      fi
      ;;
  esac

  for candidate in $candidates; do
    case $candidate in ''|"$zero"|origin/) continue ;; esac
    git cat-file -e "${candidate}^{commit}" 2>/dev/null || continue
    if merge_base=$(git merge-base "$candidate" "$head" 2>/dev/null); then
      printf '%s\n' "$merge_base"
      return 0
    fi
  done
  return 0
}

# ── Self-test ─────────────────────────────────────────────────────────────────
# These assert the matcher's semantics, not that it "ran".  The prefix-boundary
# and the literal-dot cases are the two that a plausible-looking implementation
# gets wrong, and both would fail OPEN (matching too much) or CLOSED (matching
# nothing) without ever erroring.

np_st_fail=0
np_st_expect() { # <expected 0|1> <pattern> <path> <label>
  set +e
  np_match_one "$2" "$3"
  got=$?
  set -e
  if [ "$got" -ne "$1" ]; then
    printf 'FAIL %s: pattern=%s path=%s expected=%s got=%s\n' "$4" "$2" "$3" "$1" "$got" >&2
    np_st_fail=$((np_st_fail + 1))
  fi
}
np_st_expect_die() { # <pattern> <path> <label>
  set +e
  ( np_match_one "$1" "$2" ) 2>/dev/null
  got=$?
  set -e
  if [ "$got" -ne 2 ]; then
    printf 'FAIL %s: pattern=%s should be rejected (exit 2), got=%s\n' "$3" "$1" "$got" >&2
    np_st_fail=$((np_st_fail + 1))
  fi
}

# np_st_diff <label> <expected changed files, space-separated | UNKNOWN> <np_diff_base args...>
np_st_diff() {
  label=$1 want=$2
  shift 2
  base=$(np_diff_base "$@")
  if [ -z "$base" ]; then
    got=UNKNOWN
  else
    got=$(git diff --name-only "$base" "$3" | tr '\n' ' ')
    got=${got% }
  fi
  if [ "$got" != "$want" ]; then
    printf 'FAIL diff-base-%s: expected [%s], got [%s]\n' "$label" "$want" "$got" >&2
    np_st_fail=$((np_st_fail + 1))
  fi
}

# A real history, because the defect was in which revision gets diffed, and no
# string fixture can show that.  main advances a web-relevant script while the
# branch edits only a doc, then the branch merges main in.
np_self_test_diff_base() {
  repo=$1
  (
    export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_NOSYSTEM=1
    export GIT_AUTHOR_NAME=t GIT_AUTHOR_EMAIL=t@t GIT_COMMITTER_NAME=t GIT_COMMITTER_EMAIL=t@t
    mkdir -p "$repo" && cd "$repo"
    git init -q -b main .
    mkdir docs scripts
    echo 0 > docs/a.md; echo 0 > scripts/s.ts
    git add -A && git commit -qm root
    git checkout -qb feature
    echo 1 > docs/a.md && git commit -qam 'branch: doc only'
    git checkout -q main
    echo 1 > scripts/s.ts && git commit -qam 'main: script'
    git update-ref refs/remotes/origin/main main
    git checkout -q feature
    git merge -q --no-edit main
  ) >/dev/null 2>&1 || { printf 'FAIL diff-base: could not build fixture repo\n' >&2; np_st_fail=$((np_st_fail + 1)); return; }

  root=$(git -C "$repo" rev-parse main~1)
  before=$(git -C "$repo" rev-parse feature^1)
  merge=$(git -C "$repo" rev-parse feature)
  main=$(git -C "$repo" rev-parse main)
  zero=0000000000000000000000000000000000000000

  pushd "$repo" >/dev/null
  # The regression: the merge push must see the branch's doc, not main's script.
  np_st_diff merge-push       'docs/a.md'    push "$before" "$merge" main refs/heads/feature
  np_st_diff first-push       'docs/a.md'    push "$zero"   "$merge" main refs/heads/feature
  np_st_diff pull-request     'docs/a.md'    pull_request "$main" "$merge" main refs/pull/1/merge
  # main itself is diffed against `before`, never against origin/main (= itself).
  np_st_diff default-branch   'scripts/s.ts' push "$root" "$main" main refs/heads/main
  np_st_diff default-zero     'UNKNOWN'      push "$zero" "$main" main refs/heads/main
  np_st_diff dispatch         'UNKNOWN'      workflow_dispatch '' "$merge" main refs/heads/feature
  np_st_diff unknown-head     'UNKNOWN'      push "$before" "$zero" main refs/heads/feature
  # Without origin/<default>, a branch push falls back to `before` — which is
  # the old behaviour, and shows the defect: main's script, not the branch's doc.
  git update-ref -d refs/remotes/origin/main
  np_st_diff no-origin        'scripts/s.ts' push "$before" "$merge" main refs/heads/feature
  popd >/dev/null
}

np_self_test() {
  # prefix/** — the positive cases
  np_st_expect 0 'firmware/bootloader/**' 'firmware/bootloader/src/np_main.c'  'prefix-direct'
  np_st_expect 0 'firmware/bootloader/**' 'firmware/bootloader/a/b/c.h'        'prefix-nested'
  np_st_expect 0 'firmware/bootloader/**' 'firmware/bootloader/x y.c'          'prefix-space'

  # prefix/** — the boundary.  If this regresses, the Class B list starts
  # matching sibling paths and the scoping principle (§5.0) is silently gone.
  np_st_expect 1 'firmware/bootloader/**' 'firmware/bootloader_notes.md'       'prefix-boundary'
  np_st_expect 1 'firmware/bootloader/**' 'firmware/bootloaderx/y.c'           'prefix-boundary-dir'
  np_st_expect 1 'firmware/bootloader/**' 'firmware/bootloader'                'prefix-bare-dir'
  np_st_expect 1 'firmware/bootloader/**' 'docs/np_sw_ci_001.md'               'prefix-unrelated'

  # exact — must be a string compare, not a regex or a glob
  np_st_expect 0 'firmware/CMakeLists.txt' 'firmware/CMakeLists.txt'           'exact-hit'
  np_st_expect 1 'firmware/CMakeLists.txt' 'firmware/CMakeLists.txt.bak'       'exact-suffix'
  np_st_expect 1 'firmware/CMakeLists.txt' 'x/firmware/CMakeLists.txt'         'exact-prefixed'
  np_st_expect 1 'firmware/CMakeLists.txt' 'firmware/CMakeListsXtxt'           'exact-literal-dot'
  np_st_expect 1 'firmware/CMakeLists.txt' 'firmware/cmakelists.txt'           'exact-case'

  # unsupported shapes must be loud
  np_st_expect_die 'firmware/*.c'  'firmware/a.c'    'reject-star'
  np_st_expect_die '**/x'          'a/x'             'reject-leading-globstar'
  np_st_expect_die 'firmware/a?.c' 'firmware/ab.c'   'reject-question'

  # empty pattern file must be an error, never "nothing is relevant"
  tmp=$(mktemp -d)
  trap 'rm -rf "$tmp"' EXIT
  : > "$tmp/empty.paths"
  printf '# only a comment\n\n' > "$tmp/comments.paths"
  printf 'firmware/crypto/**\nfirmware/CMakeLists.txt\n' > "$tmp/real.paths"
  printf 'docs/a.md\n' > "$tmp/docsonly.files"
  printf 'docs/a.md\nfirmware/crypto/np_sha.c\n' > "$tmp/mixed.files"

  for f in empty comments; do
    set +e
    ( np_read_patterns "$tmp/$f.paths" ) >/dev/null 2>&1
    got=$?
    set -e
    [ "$got" -eq 2 ] || {
      printf 'FAIL empty-list-%s: expected exit 2, got %s\n' "$f" "$got" >&2
      np_st_fail=$((np_st_fail + 1))
    }
  done

  # end-to-end: relevance in both directions
  got=$(np_match "$tmp/real.paths" "$tmp/docsonly.files" | wc -l | tr -d ' ')
  [ "$got" = "0" ] || { printf 'FAIL e2e-negative: expected 0 matches, got %s\n' "$got" >&2; np_st_fail=$((np_st_fail + 1)); }
  got=$(np_match "$tmp/real.paths" "$tmp/mixed.files" | wc -l | tr -d ' ')
  [ "$got" = "1" ] || { printf 'FAIL e2e-positive: expected 1 match, got %s\n' "$got" >&2; np_st_fail=$((np_st_fail + 1)); }

  np_self_test_diff_base "$tmp/repo"

  if [ "$np_st_fail" -ne 0 ]; then
    printf 'ci-changed-scope: %s self-test assertion(s) FAILED\n' "$np_st_fail" >&2
    exit 1
  fi
  printf 'ci-changed-scope: self-test OK\n'
}

# np_check_tree <patterns-file>
# Assert every pattern still points at something that exists in the tracked tree.
#
# The `!= 'false'` fail-safe in the calling workflows protects against the matcher
# ERRORING.  It does not protect against a pattern that is merely WRONG:
# `firmware/bootlaoder/**` is a perfectly well-formed pattern that matches nothing,
# so a typo silently takes the whole module out of scope, every firmware job skips,
# and every check goes green.  A pattern pointing at nothing is an error here, for
# the same reason an empty pattern list is.
np_check_tree() {
  patterns=$(np_read_patterns "$1")
  bad=0
  printf '%s\n' "$patterns" | {
    while IFS= read -r pattern || [ -n "$pattern" ]; do
      [ -n "$pattern" ] || continue
      case $pattern in
        */'**') probe=${pattern%'**'} ;;
        *)      probe=$pattern ;;
      esac
      if [ -z "$(git ls-files -- "$probe" | head -n 1)" ]; then
        printf '::error::relevance pattern matches nothing in the tracked tree: %s\n' "$pattern" >&2
        bad=$((bad + 1))
      else
        printf 'ok: %s\n' "$pattern"
      fi
    done
    [ "$bad" -eq 0 ]
  }
}

case ${1:-} in
  --self-test) np_self_test ;;
  --check-tree)
    [ $# -eq 2 ] || np_die "usage: --check-tree <patterns-file>"
    np_check_tree "$2"
    ;;
  --match)
    [ $# -eq 3 ] || np_die "usage: --match <patterns-file> <files-file>"
    np_match "$2" "$3"
    ;;
  --relevant)
    [ $# -eq 3 ] || np_die "usage: --relevant <patterns-file> <files-file>"
    if [ -n "$(np_match "$2" "$3")" ]; then printf 'true\n'; else printf 'false\n'; fi
    ;;
  --diff-base)
    [ $# -eq 6 ] || np_die "usage: --diff-base <event> <event-base> <head> <default-branch> <ref>"
    np_diff_base "$2" "$3" "$4" "$5" "$6"
    ;;
  *)
    np_die "usage: $0 --self-test | --match <patterns> <files> | --relevant <patterns> <files> | --diff-base <event> <event-base> <head> <default-branch> <ref>"
    ;;
esac
