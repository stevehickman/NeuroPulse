#!/usr/bin/env python3
"""
ci/test_shdr_batch_writer.py — the SHDR batch writer leaves no docking order in xmin or ctid.

Document: NP-INFRA-001 §3.4; OI-PRIV-76; REQ-SHDR-12 (docs/reference/data-architecture-detail.md).

Loads ci/shdr/shdr_fleet_schema.sql and infra/fleet/ingest/shdr_batch_writer.sql into a REAL PostgreSQL,
stages a month of dockings from many devices the way an ingest would (each upload its own transaction, in
arrival order), writes the month's batch, and asks the database itself what it recorded:

  * every row the batch wrote carries one xmin (one transaction);
  * the physical order of the rows (ctid) is uncorrelated with arrival order, both for single records and
    for "which device docked first";
  * a device that docked twice is merged by its own counters, whatever order its uploads arrived in;
  * staging is unlogged, closed to PUBLIC, and dropped by the transaction that writes the batch, and a
    batch that fails writes nothing and keeps staging;
  * an open month is never written, and every month column the batch writes is the batch's month;
  * every table in the SHDR schema is written by the writer or named as not yet handled.

THE ORDER THRESHOLD IS DERIVED, NOT CHOSEN. Under the null hypothesis (physical order is a random
permutation of arrival order) Spearman's rho over n items has mean 0 and standard deviation 1/sqrt(n-1).
A check fails when |rho| >= 5/sqrt(n-1): a false failure on a correct writer has probability about 6e-7
per check, and a writer that inserts in arrival order scores rho near 1 (the self-test shows it).

Two modes:
  * $DATABASE_URL set (CI, a postgres service): creates and drops its own databases on that server.
  * otherwise: starts a throwaway cluster on a free loopback port (initdb refuses root, so as root it runs
    the server as the `postgres` user) and removes it afterwards.

Usage:
  python3 ci/test_shdr_batch_writer.py              the gate
  python3 ci/test_shdr_batch_writer.py --self-test  shows the order checks catch a writer that does not batch

CI-Kind: gate
CI-Self-Test: python3 ci/test_shdr_batch_writer.py --self-test
CI-Self-Test-Reads-Tree: the falsification runs the real schema and the order checks against a naive writer on a live PostgreSQL, so the schema is the base the mutant writers run on
CI-Scan-Probe: external — needs a live PostgreSQL server
CI-Scans: the SHDR batch writer, on a live PostgreSQL: one xmin per batch, ctid order against arrival order, order-free merges, staging lifetime and month handling
CI-Scan-Paths: infra/fleet/ingest/shdr_batch_writer.sql ci/shdr/shdr_fleet_schema.sql
"""
from __future__ import annotations

import glob
import math
import os
import random
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import traceback
import urllib.parse

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCHEMA = os.path.join(REPO, "ci", "shdr", "shdr_fleet_schema.sql")
WRITER = os.path.join(REPO, "infra", "fleet", "ingest", "shdr_batch_writer.sql")

MONTH = "2026-10-01"       # a closed month: the writer refuses an open one
EARLIER = "2026-09-01"


# ── PostgreSQL plumbing ───────────────────────────────────────────────────────

def pg_bin(name: str) -> str:
    found = shutil.which(name)
    if found:
        return found
    for cand in sorted(glob.glob(f"/usr/lib/postgresql/*/bin/{name}"), reverse=True):
        return cand
    raise SystemExit(f"ERROR: {name} not found and no DATABASE_URL set")


class Server:
    """A PostgreSQL server: the one $DATABASE_URL names, or a throwaway cluster."""

    def __init__(self) -> None:
        self.cluster: str | None = None
        url = os.environ.get("DATABASE_URL")
        if url:
            self.base = urllib.parse.urlsplit(url)
            return
        port = next(p for p in range(55480, 55530) if self._free(p))
        self.cluster = tempfile.mkdtemp(prefix="np-shdr-batch-")
        as_root = os.geteuid() == 0
        if as_root:
            shutil.chown(self.cluster, "postgres")
        data = os.path.join(self.cluster, "pgdata")
        run_as = ["runuser", "-u", "postgres", "--"] if as_root else []
        env = dict(os.environ, LC_ALL="C", LANG="C")
        subprocess.run(run_as + [pg_bin("initdb"), "-D", data, "-U", "np", "--auth=trust"],
                       check=True, capture_output=True, env=env)
        subprocess.run(run_as + [pg_bin("pg_ctl"), "-D", data, "-l", os.path.join(self.cluster, "pg.log"),
                                 "-o", f"-p {port} -c unix_socket_directories= -c listen_addresses=127.0.0.1",
                                 "start"], check=True, capture_output=True, env=env)
        self.run_as, self.data = run_as, data
        self.base = urllib.parse.urlsplit(f"postgres://np@127.0.0.1:{port}/postgres")
        for _ in range(60):
            if subprocess.run([pg_bin("pg_isready"), "-h", "127.0.0.1", "-p", str(port)],
                              capture_output=True).returncode == 0:
                break
            time.sleep(0.5)
        else:
            raise SystemExit("ERROR: throwaway cluster did not become ready")

    @staticmethod
    def _free(port: int) -> bool:
        with socket.socket() as s:
            return s.connect_ex(("127.0.0.1", port)) != 0

    def url(self, db: str) -> str:
        return urllib.parse.urlunsplit(self.base._replace(path="/" + db))

    def admin(self, sql: str) -> None:
        maint = self.base.path.lstrip("/") or "postgres"
        psql(self.url(maint), sql)

    def stop(self) -> None:
        if self.cluster:
            subprocess.run(self.run_as + [pg_bin("pg_ctl"), "-D", self.data, "-m", "immediate", "stop"],
                           capture_output=True)
            shutil.rmtree(self.cluster, ignore_errors=True)


def psql(url: str, sql: str, *, check: bool = True) -> subprocess.CompletedProcess:
    r = subprocess.run([pg_bin("psql"), url, "-X", "-q", "-tA", "-F", "|", "-v", "ON_ERROR_STOP=1"],
                       input=sql, capture_output=True, text=True)
    if check and r.returncode != 0:
        raise RuntimeError(f"psql failed:\n{r.stderr.strip()}\n--- sql ---\n{sql[:2000]}")
    return r


def rows(url: str, sql: str) -> list[list[str]]:
    out = psql(url, sql).stdout.strip()
    return [line.split("|") for line in out.splitlines() if line]


class Db:
    """A fresh database holding the schema and the writer, cloned from a template."""

    counter = 0

    def __init__(self, server: Server, template: str) -> None:
        Db.counter += 1
        self.server = server
        self.name = f"np_batch_{os.getpid()}_{Db.counter}"
        server.admin(f'CREATE DATABASE "{self.name}" TEMPLATE "{template}"')
        self.url = server.url(self.name)

    def sql(self, sql: str) -> list[list[str]]:
        return rows(self.url, sql)

    def drop(self) -> None:
        self.server.admin(f'DROP DATABASE IF EXISTS "{self.name}"')


# ── A month of dockings ───────────────────────────────────────────────────────

class Docking:
    def __init__(self, token: str, session_index: int, modules: list[str]) -> None:
        self.token, self.session_index, self.modules = token, session_index, modules


def fleet(n_devices: int, seed: int) -> list[Docking]:
    """Dockings in arrival order: each device docks 1-4 times, its own uploads in session order."""
    rng = random.Random(seed)
    queues = []
    for i in range(n_devices):
        token = f"{i + 1:064x}"
        modules = [f"{(i + 1) * 16 + k:016x}" for k in (1, 2)]
        start = rng.randrange(0, 200)
        queues.append([Docking(token, start + s, modules) for s in range(rng.randint(1, 4))])
    arrivals: list[Docking] = []
    while any(queues):
        q = rng.choice([q for q in queues if q])
        arrivals.append(q.pop(0))
    return arrivals


def tx(token_hex: str) -> str:
    return f"decode('{token_hex}','hex')"


def upload_rows(d: Docking) -> list[tuple[str, str, str]]:
    """The rows one upload produces, as (target, column list, values)."""
    s, t = d.session_index, tx(d.token)
    out = [
        ("devices", "warranty_token, device_model, hw_revision, manufacture_date, factory_id, session_count,"
                    " usb_c_insertion_count, supercap_cycle_count",
         f"{t}, 'NP-T1-HOME-STD', 'B', '2026-01-01', 'F1', {s + 1}, {s + 3}, {2 * s}"),
        ("hub_thermal_telemetry", "warranty_token, session_index, hub_ntc_celsius", f"{t}, {s}, 31.5"),
        ("power_telemetry", "warranty_token, session_index, pd_negotiated_watts, supercap_capacitance_f",
         f"{t}, {s}, 65, 22.0"),
        ("storage_health", "warranty_token, pre_eol_info, wear_indicator_slc, littlefs_block_utilisation,"
                           " session_index", f"{t}, 1, 3, 0.2, {s}"),
    ]
    for m in d.modules:
        out.append(("module_life", "warranty_token, module_uid, module_part_type, module_manufacture_date,"
                                   " module_session_count, module_mate_cycles_observed, distinct_socket_count,"
                                   " pd_ratio_trend_per_session",
                    f"{t}, {tx(m)}, 'T1-A-PBM-BASE', '2026-01-01', {s + 1}, {s // 10}, 1, {-0.001 * (s + 1)}"))
    return out


def stage_sql(d: Docking, month: str) -> str:
    """What an ingest stages for one upload, as one transaction."""
    suffix = f"__{month[:4]}_{month[5:7]}"
    body = "\n".join(f"INSERT INTO np_stage.{tgt}{suffix} ({cols}) VALUES ({vals});"
                      for tgt, cols, vals in upload_rows(d))
    return f"BEGIN;\n{body}\nCOMMIT;"


def stage_month(db: Db, arrivals: list[Docking], month: str) -> None:
    db.sql(f"SELECT np_stage_open('{month}');")
    psql(db.url, "\n".join(stage_sql(d, month) for d in arrivals))


def flush(db: Db, month: str) -> list[list[str]]:
    return db.sql(f"BEGIN; SELECT * FROM np_batch_flush('{month}'); COMMIT;")


# ── The order checks (shared by the gate and the self-test) ───────────────────

WRITTEN = ("devices", "module_life", "hub_thermal_telemetry", "power_telemetry", "storage_health")


def distinct_xmins(db: Db, month: str) -> int:
    union = " UNION ALL ".join(
        f"SELECT xmin::text AS x FROM {t} WHERE "
        + ("last_seen_month" if t in ("devices", "module_life") else "ingest_month")
        + f" = '{month}'" for t in WRITTEN)
    return int(db.sql(f"SELECT count(DISTINCT x) FROM ({union}) u;")[0][0])


def ctid_key(ctid: str) -> tuple[int, int]:
    block, offset = ctid.strip("()").split(",")
    return int(block), int(offset)


def ranks(values: list) -> list[float]:
    order = sorted(range(len(values)), key=lambda i: values[i])
    r = [0.0] * len(values)
    for pos, i in enumerate(order):
        r[i] = float(pos)
    return r


def spearman(a: list, b: list) -> float:
    ra, rb = ranks(a), ranks(b)
    n = len(ra)
    ma, mb = sum(ra) / n, sum(rb) / n
    cov = sum((x - ma) * (y - mb) for x, y in zip(ra, rb))
    va = math.sqrt(sum((x - ma) ** 2 for x in ra))
    vb = math.sqrt(sum((y - mb) ** 2 for y in rb))
    return cov / (va * vb)


def limit(n: int) -> float:
    return 5.0 / math.sqrt(n - 1)


def record_order_rho(db: Db, arrivals: list[Docking]) -> tuple[float, int]:
    """Spearman's rho between arrival order and ctid order, one row per docking."""
    arrival = {(d.token, d.session_index): i for i, d in enumerate(arrivals)}
    got = db.sql("SELECT encode(warranty_token,'hex'), session_index, ctid::text FROM hub_thermal_telemetry;")
    a = [arrival[(t, int(s))] for t, s, _ in got]
    c = [ctid_key(x) for _, _, x in got]
    return spearman(a, c), len(got)


def first_docked_rho(db: Db, arrivals: list[Docking]) -> tuple[float, int]:
    """Spearman's rho between the order devices first docked in and the order of their first row on disk."""
    first_arrival: dict[str, int] = {}
    for i, d in enumerate(arrivals):
        first_arrival.setdefault(d.token, i)
    got = db.sql("SELECT encode(warranty_token,'hex'), ctid::text FROM hub_thermal_telemetry;")
    first_ctid: dict[str, tuple[int, int]] = {}
    for t, c in got:
        k = ctid_key(c)
        if t not in first_ctid or k < first_ctid[t]:
            first_ctid[t] = k
    toks = sorted(first_ctid)
    return spearman([first_arrival[t] for t in toks], [first_ctid[t] for t in toks]), len(toks)


def devices_order_rho(db: Db, arrivals: list[Docking]) -> tuple[float, int]:
    """The same question asked of the devices table, whose rows are one per device."""
    first_arrival: dict[str, int] = {}
    for i, d in enumerate(arrivals):
        first_arrival.setdefault(d.token, i)
    got = db.sql("SELECT encode(warranty_token,'hex'), ctid::text FROM devices;")
    return spearman([first_arrival[t] for t, _ in got], [ctid_key(c) for _, c in got]), len(got)


def order_findings(db: Db, arrivals: list[Docking], month: str) -> list[str]:
    """Every way the stored rows still say who docked first. Empty means none found."""
    found = []
    x = distinct_xmins(db, month)
    if x != 1:
        found.append(f"the month's rows carry {x} distinct xmin values, not 1")
    for name, fn in (("record", record_order_rho), ("first-docked device", first_docked_rho),
                     ("devices table", devices_order_rho)):
        rho, n = fn(db, arrivals)
        if abs(rho) >= limit(n):
            found.append(f"{name} ctid order follows arrival order: rho={rho:.3f} over {n}, limit {limit(n):.3f}")
    return found


# ── The gate's tests ──────────────────────────────────────────────────────────

ARRIVALS = fleet(n_devices=400, seed=76)


# @verifies REQ-SHDR-12#a,b
def test_batch_leaves_no_docking_order(srv: Server, tmpl: str) -> None:
    """One xmin for the whole batch; ctid order says nothing about which device docked first."""
    db = Db(srv, tmpl)
    try:
        assert len(ARRIVALS) > 800, len(ARRIVALS)
        stage_month(db, ARRIVALS, MONTH)
        flush(db, MONTH)
        problems = order_findings(db, ARRIVALS, MONTH)
        assert not problems, "\n".join(problems)
    finally:
        db.drop()


# @verifies REQ-SHDR-12#a
def test_each_month_is_one_transaction(srv: Server, tmpl: str) -> None:
    """Two months written as two batches: one xmin per month, two in all, and none from staging."""
    db = Db(srv, tmpl)
    try:
        first, second = fleet(150, seed=1), fleet(150, seed=2)
        stage_month(db, first, EARLIER)
        stage_month(db, second, MONTH)
        flush(db, EARLIER)
        flush(db, MONTH)
        assert distinct_xmins(db, MONTH) == 1
        union = " UNION ALL ".join(f"SELECT xmin::text AS x FROM {t}" for t in WRITTEN)
        total = int(db.sql(f"SELECT count(DISTINCT x) FROM ({union}) u;")[0][0])
        assert total == 2, f"{total} distinct xmin values over two batches"
        # A row written in September and not touched in October keeps September's batch xmin.
        sept_only = db.sql(f"SELECT count(*) FROM hub_thermal_telemetry WHERE ingest_month = '{EARLIER}';")
        assert int(sept_only[0][0]) == len(first)
    finally:
        db.drop()


# @verifies REQ-SHDR-12#c
def test_open_month_is_refused(srv: Server, tmpl: str) -> None:
    db = Db(srv, tmpl)
    try:
        current = db.sql("SELECT DATE_TRUNC('month', NOW())::date;")[0][0]
        db.sql(f"SELECT np_stage_open('{current}');")
        r = psql(db.url, f"SELECT * FROM np_batch_flush('{current}');", check=False)
        assert r.returncode != 0 and "has not closed" in r.stderr, r.stderr
        r = psql(db.url, "SELECT np_stage_open('2026-10-15');", check=False)
        assert r.returncode != 0 and "first of a month" in r.stderr, r.stderr
    finally:
        db.drop()


# @verifies REQ-SHDR-12#c
def test_month_columns_are_the_batch_month(srv: Server, tmpl: str) -> None:
    """A staged row's own month (here its default, the month the test runs in) is replaced by the batch's."""
    db = Db(srv, tmpl)
    try:
        stage_month(db, fleet(20, seed=3), MONTH)
        flush(db, MONTH)
        for t, col in (("devices", "last_seen_month"), ("module_life", "last_seen_month"),
                       ("hub_thermal_telemetry", "ingest_month"), ("power_telemetry", "ingest_month"),
                       ("storage_health", "ingest_month")):
            got = db.sql(f"SELECT DISTINCT {col}::text FROM {t};")
            assert got == [[MONTH]], f"{t}.{col}: {got}"
    finally:
        db.drop()


def _two_dockings(db: Db, order: tuple[int, int]) -> list[list[str]]:
    a = Docking(f"{0xA1:064x}", 10, [f"{0xB1:016x}"])
    b = Docking(f"{0xA1:064x}", 20, [f"{0xB1:016x}"])
    seq = [a, b] if order == (0, 1) else [b, a]
    stage_month(db, seq, MONTH)
    flush(db, MONTH)
    return db.sql(
        "SELECT d.session_count, d.usb_c_insertion_count, m.module_session_count,"
        " m.module_mate_cycles_observed, m.pd_ratio_trend_per_session,"
        " (SELECT count(*) FROM hub_thermal_telemetry) FROM devices d JOIN module_life m USING (warranty_token);")


# @verifies REQ-SHDR-12#d
def test_device_docking_twice_merges_by_its_counters(srv: Server, tmpl: str) -> None:
    """The newer upload wins by the device's own counters whichever arrived first, and both records stay."""
    results = []
    for order in ((0, 1), (1, 0)):
        db = Db(srv, tmpl)
        try:
            results.append(_two_dockings(db, order))
        finally:
            db.drop()
    assert results[0] == results[1], f"arrival order changed the merge: {results}"
    sessions, usb, msc, mate, trend, hub_rows = results[0][0]
    assert (int(sessions), int(usb), int(msc), int(mate), int(hub_rows)) == (21, 23, 21, 2, 2), results[0]
    assert abs(float(trend) - (-0.021)) < 1e-6, trend


# @verifies REQ-SHDR-12#d
def test_merge_against_a_stored_row_never_regresses(srv: Server, tmpl: str) -> None:
    """An older upload written in a later batch does not move a grow-only counter back or replace newer trends."""
    db = Db(srv, tmpl)
    try:
        stage_month(db, [Docking(f"{0xA2:064x}", 30, [f"{0xB2:016x}"])], EARLIER)
        flush(db, EARLIER)
        stage_month(db, [Docking(f"{0xA2:064x}", 5, [f"{0xB2:016x}"])], MONTH)   # late, older upload
        flush(db, MONTH)
        got = db.sql("SELECT d.session_count, m.module_session_count, m.pd_ratio_trend_per_session,"
                     " d.last_seen_month::text FROM devices d JOIN module_life m USING (warranty_token);")
        sessions, msc, trend, seen = got[0]
        assert (int(sessions), int(msc), seen) == (31, 31, MONTH), got
        assert abs(float(trend) - (-0.031)) < 1e-6, trend
    finally:
        db.drop()


# @verifies REQ-SHDR-12#d
def test_redelivered_record_is_written_once(srv: Server, tmpl: str) -> None:
    db = Db(srv, tmpl)
    try:
        d = Docking(f"{0xA3:064x}", 7, [f"{0xB3:016x}"])
        stage_month(db, [d, d], MONTH)
        flush(db, MONTH)
        got = db.sql("SELECT (SELECT count(*) FROM hub_thermal_telemetry), (SELECT count(*) FROM module_life),"
                     " (SELECT count(*) FROM devices);")
        assert got == [["1", "1", "1"]], got
    finally:
        db.drop()


# @verifies REQ-SHDR-12#d
def test_record_for_an_unknown_device_is_not_written(srv: Server, tmpl: str) -> None:
    """A record whose token has no devices row (stored or staged) is counted and not written."""
    db = Db(srv, tmpl)
    try:
        stage_month(db, fleet(5, seed=4), MONTH)
        psql(db.url, f"INSERT INTO np_stage.hub_thermal_telemetry__2026_10 (warranty_token, session_index,"
                     f" hub_ntc_celsius) VALUES ({tx('ff' * 32)}, 1, 30.0);")
        out = {r[0]: r for r in flush(db, MONTH)}
        assert out["hub_thermal_telemetry"][3] == "1", out["hub_thermal_telemetry"]
        got = db.sql(f"SELECT count(*) FROM hub_thermal_telemetry WHERE warranty_token = {tx('ff' * 32)};")
        assert got == [["0"]], got
    finally:
        db.drop()


# @verifies REQ-SHDR-12#e
def test_staging_is_unlogged_closed_and_dropped(srv: Server, tmpl: str) -> None:
    db = Db(srv, tmpl)
    try:
        stage_month(db, fleet(10, seed=5), MONTH)
        got = db.sql("SELECT count(*), bool_and(c.relpersistence = 'u') FROM pg_class c"
                     " JOIN pg_namespace n ON n.oid = c.relnamespace"
                     " WHERE n.nspname = 'np_stage' AND c.relkind = 'r' AND c.relname LIKE '%__2026_10';")
        assert got == [["18", "t"]], got
        db.sql("DO $$ BEGIN IF NOT EXISTS (SELECT 1 FROM pg_roles WHERE rolname = 'np_reader')"
               " THEN CREATE ROLE np_reader; END IF; END $$;")
        got = db.sql("SELECT has_schema_privilege('np_reader', 'np_stage', 'USAGE'),"
                     " has_table_privilege('np_reader', 'np_stage.devices__2026_10', 'SELECT');")
        assert got == [["f", "f"]], f"staging readable by a role the ingest did not grant: {got}"
        flush(db, MONTH)
        left = db.sql("SELECT count(*) FROM pg_class c JOIN pg_namespace n ON n.oid = c.relnamespace"
                      " WHERE n.nspname = 'np_stage' AND c.relname LIKE '%__2026_10';")
        assert left == [["0"]], f"staging for the written month survived the batch: {left}"
        r = psql(db.url, f"SELECT * FROM np_batch_flush('{MONTH}');", check=False)
        assert r.returncode != 0 and "no staging" in r.stderr, r.stderr
    finally:
        db.drop()


# @verifies REQ-SHDR-12#e
def test_failed_batch_writes_nothing_and_keeps_staging(srv: Server, tmpl: str) -> None:
    """A failure late in the batch (the last table written) leaves every SHDR table untouched and staging whole."""
    db = Db(srv, tmpl)
    try:
        arrivals = fleet(30, seed=6)
        stage_month(db, arrivals, MONTH)
        db.sql("CREATE FUNCTION np_test_fail() RETURNS trigger LANGUAGE plpgsql AS"
               " $$ BEGIN RAISE EXCEPTION 'injected failure'; END $$;"
               " CREATE TRIGGER np_test_fail BEFORE INSERT ON storage_health"
               " FOR EACH ROW EXECUTE FUNCTION np_test_fail();")
        r = psql(db.url, f"BEGIN; SELECT * FROM np_batch_flush('{MONTH}'); COMMIT;", check=False)
        assert r.returncode != 0 and "injected failure" in r.stderr, r.stderr
        written = db.sql("SELECT (SELECT count(*) FROM devices) + (SELECT count(*) FROM module_life)"
                         " + (SELECT count(*) FROM hub_thermal_telemetry);")
        assert written == [["0"]], f"a failed batch wrote rows: {written}"
        staged = db.sql("SELECT count(*) FROM np_stage.hub_thermal_telemetry__2026_10;")
        assert staged == [[str(len(arrivals))]], staged
        db.sql("DROP TRIGGER np_test_fail ON storage_health;")
        flush(db, MONTH)
        assert distinct_xmins(db, MONTH) == 1
    finally:
        db.drop()


# @verifies REQ-SHDR-12#f
def test_every_shdr_table_is_handled_or_named(srv: Server, tmpl: str) -> None:
    db = Db(srv, tmpl)
    try:
        tables = {r[0] for r in db.sql("SELECT tablename FROM pg_tables WHERE schemaname = 'public';")}
        handled = {r[0] for r in db.sql("SELECT target FROM np_batch_tables();")}
        named = {r[0] for r in db.sql("SELECT target FROM np_batch_unsupported_tables();")}
        assert not (handled & named), handled & named
        assert tables == handled | named, (
            f"not handled and not named: {sorted(tables - handled - named)};"
            f" named but not in the schema: {sorted((handled | named) - tables)}")
    finally:
        db.drop()


GATE_TESTS = [
    test_batch_leaves_no_docking_order,
    test_each_month_is_one_transaction,
    test_open_month_is_refused,
    test_month_columns_are_the_batch_month,
    test_device_docking_twice_merges_by_its_counters,
    test_merge_against_a_stored_row_never_regresses,
    test_redelivered_record_is_written_once,
    test_record_for_an_unknown_device_is_not_written,
    test_staging_is_unlogged_closed_and_dropped,
    test_failed_batch_writes_nothing_and_keeps_staging,
    test_every_shdr_table_is_handled_or_named,
]


# ── The self-test: the order checks catch writers that do not batch ──────────

def naive_write(db: Db, arrivals: list[Docking], month: str, one_transaction: bool) -> None:
    """Writes each upload straight into the SHDR tables in arrival order, as an unbatched ingest would.

    Month columns are set to the batch month exactly as the real writer sets them, so the only difference
    the checks can see is the one under test: xmin and ctid.
    """
    upsert = {
        "devices": " ON CONFLICT (warranty_token) DO UPDATE SET"
                   " session_count = GREATEST(devices.session_count, EXCLUDED.session_count)",
        "module_life": " ON CONFLICT (warranty_token, module_uid) DO UPDATE SET module_session_count ="
                       " GREATEST(module_life.module_session_count, EXCLUDED.module_session_count)",
    }
    blocks = []
    for d in arrivals:
        stmts = []
        for tgt, cols, vals in upload_rows(d):
            mcol = "last_seen_month" if tgt in upsert else "ingest_month"
            stmts.append(f"INSERT INTO public.{tgt} ({cols}, {mcol}) VALUES ({vals}, '{month}')"
                         f"{upsert.get(tgt, '')};")
        body = "\n".join(stmts)
        blocks.append(body if one_transaction else f"BEGIN;\n{body}\nCOMMIT;")
    sql = "\n".join(blocks)
    psql(db.url, f"BEGIN;\n{sql}\nCOMMIT;" if one_transaction else sql)


def self_test(srv: Server, tmpl: str) -> int:
    failures = 0
    for label, one_tx, expect in (
        ("one transaction per upload, arrival order", False, ("xmin", "ctid")),
        ("one transaction, arrival order (no shuffle)", True, ("ctid",)),
    ):
        db = Db(srv, tmpl)
        try:
            naive_write(db, ARRIVALS, MONTH, one_tx)
            found = order_findings(db, ARRIVALS, MONTH)
            text = " | ".join(found)
            missing = [e for e in expect if e not in text]
            if missing:
                failures += 1
                print(f"FAIL  self-test [{label}]: the checks did not report {missing}; found: {found or 'nothing'}")
            else:
                print(f"PASS  self-test [{label}]: caught — {text}")
            if "xmin" not in expect and any("xmin" in f for f in found):
                failures += 1
                print(f"FAIL  self-test [{label}]: a one-transaction writer was reported for xmin: {found}")
        finally:
            db.drop()
    return failures


# ── Driver ───────────────────────────────────────────────────────────────────

def main() -> int:
    mode_self_test = "--self-test" in sys.argv[1:]
    srv = Server()
    tmpl = f"np_batch_tmpl_{os.getpid()}"
    try:
        srv.admin(f'CREATE DATABASE "{tmpl}"')
        psql(srv.url(tmpl), open(SCHEMA).read())
        psql(srv.url(tmpl), open(WRITER).read())
        if mode_self_test:
            failures = self_test(srv, tmpl)
            print(f"\nSHDR batch writer self-test: {'PASS' if failures == 0 else f'{failures} FAIL'}")
            return 0 if failures == 0 else 1
        failures = 0
        for t in GATE_TESTS:
            try:
                t(srv, tmpl)
                print(f"PASS  {t.__name__}")
            except Exception:
                failures += 1
                print(f"FAIL  {t.__name__}\n{traceback.format_exc()}")
        print(f"\nSHDR batch writer: {len(GATE_TESTS) - failures}/{len(GATE_TESTS)} PASS")
        return 0 if failures == 0 else 1
    finally:
        try:
            srv.admin(f'DROP DATABASE IF EXISTS "{tmpl}"')
        finally:
            srv.stop()


if __name__ == "__main__":
    sys.exit(main())
