#!/usr/bin/env bash
#
# NeurOne SHDR fleet schema — live constraint test harness (OI-EMMC2-07)
# Document: NP-FW-EMMC-002 Rev 1 §G.4 / schema Rev 4
#
# Executes ci/shdr/shdr_fleet_schema.sql against a REAL PostgreSQL and then runs
# ci/shdr/shdr_schema_constraint_tests.sql, which proves each integrity
# constraint rejects the data it exists to reject.
#
# WHY THIS EXISTS ALONGSIDE test_shdr_schema.py. The Python gate is a static
# analyser: it reads the DDL as text and enforces the privacy invariants (no
# PII, no timestamps, no dose columns, correct socket/UID declarations). It
# cannot tell you whether the file is valid SQL, nor whether a CHECK actually
# fires — a constraint with a typo'd predicate parses fine and passes every
# name-and-type check while silently admitting the rows it was written to block.
# This harness closes that gap by making the database itself the judge.
#
# Two modes:
#   * If $DATABASE_URL or $PGHOST is set (CI with a postgres service), use it.
#   * Otherwise spin up a throwaway local cluster on a high port, run, tear down.
#     Nothing outside $TMPDIR is touched and no existing cluster is contacted.
#
# Usage:
#   ci/run_shdr_constraint_tests.sh            # throwaway local cluster
#   DATABASE_URL=postgres://... ci/run_shdr_constraint_tests.sh
#
# Exits 0 only when the schema loads AND every constraint test passes.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SCHEMA="$REPO_ROOT/ci/shdr/shdr_fleet_schema.sql"
TESTS="$REPO_ROOT/ci/shdr/shdr_schema_constraint_tests.sql"

# initdb/postgres abort with "postmaster became multithreaded during startup"
# under some macOS locales; C is safe and the schema uses no locale-sensitive
# collation.
#
# CI-Kind: gate
# CI-Self-Test: ci/shdr/shdr_schema_constraint_tests.sql
# CI-Scan-Probe: external — needs a live PostgreSQL server
# CI-Scans: every integrity constraint in the SHDR schema, against a live PostgreSQL
# CI-Scan-Paths: ci/shdr/shdr_fleet_schema.sql ci/shdr/shdr_schema_constraint_tests.sql
export LC_ALL=C LANG=C

for f in "$SCHEMA" "$TESTS"; do
    [[ -f "$f" ]] || { echo "ERROR: missing $f" >&2; exit 1; }
done

command -v psql >/dev/null 2>&1 || {
    echo "ERROR: psql not found. Install PostgreSQL client tools." >&2; exit 1; }

CLUSTER_DIR=""
STARTED_CLUSTER=0

cleanup() {
    if [[ $STARTED_CLUSTER -eq 1 && -n "$CLUSTER_DIR" ]]; then
        pg_ctl -D "$CLUSTER_DIR/pgdata" -m immediate stop >/dev/null 2>&1 || true
        rm -rf "$CLUSTER_DIR"
    fi
}
trap cleanup EXIT

if [[ -n "${DATABASE_URL:-}" ]]; then
    PSQL=(psql "$DATABASE_URL")
    echo "Using DATABASE_URL"
elif [[ -n "${PGHOST:-}" ]]; then
    PSQL=(psql)
    echo "Using PGHOST=$PGHOST"
else
    command -v initdb >/dev/null 2>&1 || {
        echo "ERROR: initdb not found and no DATABASE_URL/PGHOST set." >&2; exit 1; }

    CLUSTER_DIR="$(mktemp -d "${TMPDIR:-/tmp}/np-shdr-pg.XXXXXX")"

    # Pick a free port rather than a fixed one: a leftover cluster from an
    # interrupted run would otherwise make this script fail with "address
    # already in use" — a failure of the harness that reads like a failure of
    # the schema.
    PORT="${NP_PG_PORT:-}"
    if [[ -z "$PORT" ]]; then
        for candidate in $(seq 55432 55470); do
            if ! (exec 3<>"/dev/tcp/127.0.0.1/$candidate") 2>/dev/null; then
                PORT="$candidate"
                break
            fi
        done
    fi
    [[ -n "$PORT" ]] || { echo "ERROR: no free port in 55432-55470" >&2; exit 1; }

    echo "Starting throwaway PostgreSQL cluster on 127.0.0.1:$PORT"
    initdb -D "$CLUSTER_DIR/pgdata" -U np --auth=trust >"$CLUSTER_DIR/initdb.log" 2>&1 || {
        tail -20 "$CLUSTER_DIR/initdb.log" >&2; exit 1; }

    # unix_socket_directories is emptied deliberately: the socket path derived
    # from a long scratch dir can exceed the 103-byte sun_path limit, which
    # kills startup. TCP on loopback avoids the whole class of problem.
    pg_ctl -D "$CLUSTER_DIR/pgdata" \
           -o "-p $PORT -c unix_socket_directories= -c listen_addresses=127.0.0.1" \
           -l "$CLUSTER_DIR/pg.log" start >/dev/null 2>&1 || {
        tail -20 "$CLUSTER_DIR/pg.log" >&2; exit 1; }
    STARTED_CLUSTER=1

    for _ in $(seq 1 30); do
        pg_isready -h 127.0.0.1 -p "$PORT" >/dev/null 2>&1 && break
        sleep 0.5
    done
    pg_isready -h 127.0.0.1 -p "$PORT" >/dev/null 2>&1 || {
        echo "ERROR: cluster did not become ready" >&2; tail -20 "$CLUSTER_DIR/pg.log" >&2; exit 1; }

    createdb -h 127.0.0.1 -p "$PORT" -U np shdr_test
    PSQL=(psql -h 127.0.0.1 -p "$PORT" -U np -d shdr_test)
fi

echo
echo "── Loading schema ──────────────────────────────────────────────────────"
"${PSQL[@]}" -v ON_ERROR_STOP=1 -q -f "$SCHEMA"
echo "Schema loaded without error."

echo
echo "── Object inventory ────────────────────────────────────────────────────"
"${PSQL[@]}" -tA -c "
  SELECT 'tables:      ' || count(*) FROM pg_tables  WHERE schemaname='public'
  UNION ALL SELECT 'views:       ' || count(*) FROM pg_views   WHERE schemaname='public'
  UNION ALL SELECT 'indexes:     ' || count(*) FROM pg_indexes WHERE schemaname='public'
  UNION ALL SELECT 'triggers:    ' || count(*) FROM pg_trigger WHERE NOT tgisinternal
  UNION ALL SELECT 'checks:      ' || count(*) FROM pg_constraint c
              JOIN pg_class t ON t.oid=c.conrelid
              JOIN pg_namespace n ON n.oid=t.relnamespace
             WHERE n.nspname='public' AND c.contype='c'
  UNION ALL SELECT 'uniques:     ' || count(*) FROM pg_constraint c
              JOIN pg_class t ON t.oid=c.conrelid
              JOIN pg_namespace n ON n.oid=t.relnamespace
             WHERE n.nspname='public' AND c.contype='u';"

# Guard against the retired model reappearing at the LIVE object level, not just
# in the file text: a column added by a later migration would be invisible to a
# grep of the DDL but is visible here.
echo
echo "── Retired fixed-zone model absent from live catalog ───────────────────"
ZONE_COLS=$("${PSQL[@]}" -tA -c "
  SELECT count(*) FROM information_schema.columns
   WHERE table_schema='public'
     AND (column_name = 'zone_id' OR column_name ~ '^zone[0-9]+_');")
ZONE_TABLES=$("${PSQL[@]}" -tA -c "
  SELECT count(*) FROM pg_tables
   WHERE schemaname='public' AND tablename IN ('pbm_zone_telemetry','thermal_profiles');")
echo "fixed-zone columns: $ZONE_COLS   retired tables: $ZONE_TABLES"
if [[ "$ZONE_COLS" != "0" || "$ZONE_TABLES" != "0" ]]; then
    echo "FAIL: the retired fixed five-zone model is present in the live schema." >&2
    exit 1
fi

echo
echo "── Constraint tests ────────────────────────────────────────────────────"
"${PSQL[@]}" -q -t -f "$TESTS"

echo
echo "SHDR Rev 4 live constraint tests: PASS"
