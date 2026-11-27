#!/usr/bin/env python3
"""Who may read the analytics store, and what the read log records (NP-ANALYTICS-001 §7.7 and §7.8, OI-PRIV-47).

Two things are checked, neither against a live stack:

  U  The ClickHouse users the stack defines (infra/posthog/clickhouse/*-user.xml). Each script's user holds exactly the grants
     listed in GRANTS below and no other; none is PostHog's application user; each password comes from the container's
     environment, and the compose file refuses to start ClickHouse without it (an unset from_env password is an EMPTY password,
     so the user would log in with none); and every user file is mounted into the ClickHouse container.
  L  infra/posthog/read_log.py: the entry format, the unconditional path redaction, the query-log mapping that keeps neither the
     query's text nor the client's address, the alert rules A1-A3, A5-A8, A10 and A11, and the routing the principal decided.

`--self-test` proves the suite goes red on a user file given one grant too many, a password made optional, a redaction that keeps
an ID, a query-log mapping that keeps the text, and a routing table with one rule moved.

Run: python3 ci/test_posthog_read_access.py [--self-test]

CI-Kind: gate
CI-Self-Test: python3 ci/test_posthog_read_access.py --self-test
CI-Scans: the ClickHouse user files and the compose file's ClickHouse service, for each script's grants and required passwords; the read-log module's format, redaction and alert routing
CI-Scan-Paths: infra/posthog/clickhouse infra/posthog/docker-compose.yml infra/posthog/read_log.py
CI-Self-Test-Reads-Tree: the user-file mutants are copies of the real files broken in one way, and the read-log mutants patch the real module the suite imports
"""
import importlib.util
import io
import pathlib
import re
import sys
import unittest
import xml.etree.ElementTree as ET
from datetime import datetime, timedelta, timezone

ROOT = pathlib.Path(__file__).resolve().parent.parent
INFRA = ROOT / "infra" / "posthog"
spec = importlib.util.spec_from_file_location("read_log", INFRA / "read_log.py")
rl = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rl)

APPLICATION_USER = "posthog"
# Every ClickHouse user a user file may define, and exactly the grants it may hold (NP-ANALYTICS-001 §7.6, §7.7).
GRANTS = {
    "np_erase": {"GRANT ALTER DELETE ON posthog.sharded_events", "GRANT SELECT(distinct_id) ON posthog.sharded_events"},
    "np_retention": {"GRANT SELECT(database, name, create_table_query) ON system.tables",
                     "GRANT ALTER MODIFY TTL ON posthog.sharded_events"},
}
# The routing the principal decided on 2026-11-24 (§7.7).
IMMEDIATE = {"A1", "A2", "A4", "A8", "A9", "A11"}
WEEK = {"A3", "A5", "A6", "A7", "A10"}
NOW = datetime(2026, 11, 26, 9, 0, 0, tzinfo=timezone.utc)
AN_ID = "0190c3a2-7d4e-7b1a-9c55-2f6e8a1b3c4d"


def read_files():
    return ({p.name: p.read_text() for p in sorted((INFRA / "clickhouse").glob("*-user.xml"))},
            (INFRA / "docker-compose.yml").read_text())


def clickhouse_service(compose: str) -> str:
    """The text of the compose file's `clickhouse:` service block."""
    found = re.search(r"^  clickhouse:\n(.*?)(?=^  \S|\Z)", compose, re.M | re.S)
    return found.group(1) if found else ""


def user_problems(user_files: dict, compose: str) -> list:
    out, service = [], clickhouse_service(compose)
    if not user_files:
        return ["no ClickHouse user file found"]
    for name, text in user_files.items():
        try:
            users = ET.fromstring(text).find("users")
        except ET.ParseError as e:
            out.append(f"{name}: not XML ({e})")
            continue
        for user in list(users if users is not None else []):
            who = user.tag
            if who == APPLICATION_USER:
                out.append(f"{name}: defines PostHog's application user")
                continue
            if who not in GRANTS:
                out.append(f"{name}: user {who} is not one of the users §7.7 lists")
                continue
            have = {q.text.strip() for q in user.findall("grants/query") if q.text}
            if have != GRANTS[who]:
                out.append(f"{name}: {who} grants {sorted(have)}, allowed exactly {sorted(GRANTS[who])}")
            password = user.find("password")
            env = password.get("from_env") if password is not None else None
            if not env or (password.text or "").strip() or user.find("password_sha256_hex") is not None:
                out.append(f"{name}: {who}'s password must come from the environment (from_env) and nowhere else")
            elif not re.search(rf"^\s+{env}:\s*\$\{{{env}:\?[^}}]+\}}\s*$", service, re.M):
                out.append(f"{name}: the clickhouse service must refuse to start without {env} (${{{env}:?...}}), "
                           f"or {who} exists with an empty password")
        if not re.search(rf"\./clickhouse/{re.escape(name)}:/etc/clickhouse-server/users\.d/{re.escape(name)}:ro", service):
            out.append(f"{name}: not mounted read-only into the clickhouse service's users.d")
    return out


class Users(unittest.TestCase):
    # @verifies REQ-SW03-34#e REQ-SW03-22#e
    def test_each_script_user_holds_exactly_its_grants_and_a_required_password(self):
        files, compose = read_files()
        self.assertEqual(user_problems(files, compose), [])
        self.assertEqual({u.tag for t in files.values() for u in ET.fromstring(t).find("users")}, set(GRANTS))

    # @verifies REQ-SW03-34#e
    def test_one_grant_too_many_is_caught(self):
        files, compose = read_files()
        name = "np-retention-user.xml"
        files[name] = files[name].replace("</grants>", "<query>GRANT SELECT ON posthog.sharded_events</query></grants>")
        self.assertTrue(any("np_retention grants" in p for p in user_problems(files, compose)))

    # @verifies REQ-SW03-34#e REQ-SW03-22#e
    def test_an_optional_password_is_caught(self):
        files, compose = read_files()
        for env in ("RETENTION_CLICKHOUSE_PASSWORD", "ERASE_CLICKHOUSE_PASSWORD"):
            broken = re.sub(rf"\$\{{{env}:\?[^}}]+\}}", f"${{{env}:-}}", compose, count=1)
            self.assertTrue(any(env in p for p in user_problems(files, broken)), env)

    # @verifies REQ-SW03-34#e
    def test_the_application_user_in_a_user_file_is_caught(self):
        files, compose = read_files()
        files["np-retention-user.xml"] = files["np-retention-user.xml"].replace("np_retention>", "posthog>")
        self.assertTrue(any("application user" in p for p in user_problems(files, compose)))


def entry(**over):
    base = rl.from_query_log(7, {"user": "np_retention", "event_time": NOW, "tables": ["system.tables"], "result_rows": 1,
                                 "normalized_query_hash": 1234, "exception_code": 0, "initial_address": "10.0.0.9",
                                 "query": "SELECT create_table_query FROM system.tables"}, ["10.0.0.2"])
    base.update(over)
    return base


def ch_row(query, user="alice", code=0, address="10.0.0.9"):
    return {"user": user, "event_time": NOW, "tables": ["posthog.sharded_events"], "result_rows": 5,
            "normalized_query_hash": 99, "exception_code": code, "initial_address": address, "query": query}


class ReadLog(unittest.TestCase):
    # @verifies REQ-SW03-40#a
    def test_an_entry_has_exactly_the_format_keys(self):
        good = entry()
        self.assertEqual(set(good), set(rl.KEYS))
        for bad in ({**good, "query_text": "SELECT 1"}, {k: v for k, v in good.items() if k != "seq"},
                    {**good, "at": "2026-11-26 09:00"}, {**good, "query": "SELECT 1"}, {**good, "rows": -1}):
            with self.assertRaises(rl.EntryError):
                rl.validate(bad)

    # @verifies REQ-SW03-40#b
    def test_a_page_path_keeps_no_id_and_no_query_string(self):
        for path in (f"/project/2/person/{AN_ID}", f"/api/projects/2/persons/{AN_ID}/?distinct_id={AN_ID}",
                     f"/project/2/events/{AN_ID}#x", f"/project/2/person/{AN_ID[:16]}", "/project/2/person/alice"):
            e = rl.from_proxy(1, NOW, "analytics.lead", path)
            self.assertNotIn(AN_ID[:16], rl.line(e))
            self.assertNotIn("alice", e["object"])
            self.assertNotIn("?", e["object"])
        self.assertEqual(rl.redact_path(f"/project/2/person/{AN_ID}?x=1"), "/project/:id/person/:id")

    # @verifies REQ-SW03-40#b
    def test_the_page_type_survives_the_ids_removal(self):
        self.assertEqual(rl.from_proxy(1, NOW, "a", f"/project/2/person/{AN_ID}")["kind"], "single_installation")
        self.assertEqual(rl.from_proxy(1, NOW, "a", "/project/2/insights/abc")["kind"], "read")
        self.assertEqual(rl.from_proxy(1, NOW, "a", "/api/projects/2/exports/5/content")["kind"], "export")

    # @verifies REQ-SW03-40#b
    def test_an_unredacted_page_cannot_be_written(self):
        with self.assertRaises(rl.EntryError):
            rl.validate({**rl.from_proxy(1, NOW, "a", "/project/2"), "object": f"/project/2/person/{AN_ID}"})

    # @verifies REQ-SW03-40#c
    def test_a_query_log_row_keeps_neither_text_nor_address(self):
        e = rl.from_query_log(3, ch_row(f"SELECT * FROM events WHERE distinct_id = '{AN_ID}'"), ["10.0.0.2"])
        text = rl.line(e)
        for gone in (AN_ID, "SELECT", "10.0.0.9", "distinct_id"):
            self.assertNotIn(gone, text)
        self.assertEqual((e["query"], e["object"], e["rows"], e["origin"]), ("0000000000000063", "posthog.sharded_events", 5, "other"))

    # @verifies REQ-SW03-40#c
    def test_a_query_is_classified_before_its_text_is_dropped(self):
        kinds = {q: rl.from_query_log(1, ch_row(q), [])["kind"] for q in (
            "SELECT count() FROM events", "SELECT * FROM events WHERE distinct_id IN ('a')", "GRANT SELECT ON *.* TO bob",
            "CREATE USER bob", "SELECT * FROM events INTO OUTFILE 'x.csv'", "SELECT * FROM events FORMAT CSVWithNames")}
        self.assertEqual(list(kinds.values()), ["read", "single_installation", "access_change", "access_change", "export", "export"])
        self.assertEqual(rl.from_query_log(1, ch_row("SELECT 1", code=497), [])["outcome"], "refused")
        self.assertEqual(rl.from_query_log(1, ch_row("SELECT 1", code=62), [])["outcome"], "error")

    # @verifies REQ-SW03-40#d
    def test_entry_alerts(self):
        approved = {"posthog", "np_erase", "np_retention", "alice", "analytics.lead"}

        def rules(e, pl=()):
            return [a["rule"] for a in rl.entry_alerts(e, approved, APPLICATION_USER, pl)]
        self.assertEqual(rules(entry()), [])
        self.assertEqual(rules(rl.from_query_log(1, ch_row("SELECT 1", user="mallory"), [])), ["A1"])
        self.assertEqual(rules(rl.from_query_log(1, ch_row("SELECT 1", user="posthog", address="10.0.0.9"), ["10.0.0.2"])), ["A2"])
        self.assertEqual(rules(rl.from_query_log(1, ch_row("SELECT 1", user="posthog", address="10.0.0.2"), ["10.0.0.2"])), [])
        self.assertEqual(rules(rl.from_query_log(1, ch_row("SELECT 1", code=497), [])), ["A3"])
        self.assertEqual(rules(rl.from_query_log(1, ch_row("CREATE USER x"), [])), ["A5"])
        self.assertEqual(rules(rl.from_query_log(1, ch_row("SELECT 1 WHERE distinct_id = 'a'"), [])), ["A6"])
        self.assertEqual(rules(rl.from_query_log(1, ch_row("SELECT 1 WHERE distinct_id IN ('a')", user="np_erase"), [])), [])
        self.assertEqual(rules(rl.from_proxy(1, NOW, "analytics.lead", f"/project/2/person/{AN_ID}")), ["A6"])
        self.assertEqual(rules(rl.from_proxy(1, NOW, "analytics.lead", "/api/projects/2/exports/1")), ["A7"])
        self.assertEqual(rules(rl.heartbeat(1, NOW, "fwd")), [])

    # @verifies REQ-SW03-40#d
    def test_a_silent_or_gapped_log_raises_a8(self):
        beats = [rl.heartbeat(i, NOW - timedelta(seconds=rl.HEARTBEAT_SECONDS * (3 - i)), "fwd") for i in range(4)]
        self.assertEqual(rl.gap_alerts(beats, NOW), [])
        late = NOW + timedelta(seconds=rl.HEARTBEAT_SECONDS * rl.HEARTBEAT_MISSED + 1)
        self.assertEqual([a["rule"] for a in rl.gap_alerts(beats, late)], ["A8"])
        self.assertEqual([a["rule"] for a in rl.gap_alerts(beats[:1] + beats[2:], NOW)], ["A8"])
        self.assertEqual([a["rule"] for a in rl.gap_alerts([], NOW)], ["A8"])

    # @verifies REQ-SW03-40#d
    def test_drift_and_erasure_alerts(self):
        ok = {"apply_project_settings.py": 0, "apply_event_retention.py": 0}
        self.assertEqual(rl.drift_alerts(ok, NOW - timedelta(days=1), NOW), [])
        self.assertEqual([a["rule"] for a in rl.drift_alerts({**ok, "apply_event_retention.py": 2}, NOW, NOW)], ["A10"])
        self.assertEqual([a["rule"] for a in rl.drift_alerts(ok, NOW - timedelta(days=8), NOW)], ["A10"])
        self.assertLessEqual(rl.A10_CHECK_INTERVAL_SECONDS, rl.A10_MAX_INTERVAL_SECONDS)
        self.assertEqual(rl.erasure_alerts(3, 3), [])
        self.assertEqual([a["rule"] for a in rl.erasure_alerts(3, 4)], ["A11"])
        self.assertEqual([a["rule"] for a in rl.erasure_alerts(3, 0)], ["A11"])

    # @verifies REQ-SW03-40#e
    def test_routing_is_the_principals(self):
        self.assertEqual({r for r, (how, _) in rl.ALERTS.items() if how == "immediate"}, IMMEDIATE)
        self.assertEqual({r for r, (how, _) in rl.ALERTS.items() if how == "week"}, WEEK)
        self.assertEqual(rl.alert("A9", "x")["to"], ["privacy_lead", "ceo"])
        self.assertEqual(rl.alert("A3", "x", who="bob", privacy_lead={"pl"})["to"], ["privacy_lead"])
        caused = rl.entry_alerts(rl.from_query_log(1, ch_row("SELECT 1", user="pl", code=497), []), {"pl"}, APPLICATION_USER, {"pl"})
        self.assertEqual([(a["rule"], a["to"]) for a in caused], [("A3", ["privacy_lead", "ceo"])])


def run_suite() -> int:
    loader = unittest.defaultTestLoader
    suite = unittest.TestSuite([loader.loadTestsFromTestCase(Users), loader.loadTestsFromTestCase(ReadLog)])
    return 0 if unittest.TextTestRunner(stream=io.StringIO()).run(suite).wasSuccessful() else 1


def self_test() -> int:
    """The checks must go red on a module broken in each way that matters, and stay green on the real one."""
    problems = []
    if run_suite() != 0:
        problems.append("the suite is not green on the real tree")
    files, compose = read_files()
    broken = dict(files)
    broken["np-erase-user.xml"] = broken["np-erase-user.xml"].replace("</grants>", "<query>GRANT SELECT ON posthog.*</query></grants>")
    if not user_problems(broken, compose):
        problems.append("a user file with an extra grant was not caught")
    if not user_problems(files, compose.replace("RETENTION_CLICKHOUSE_PASSWORD:?", "RETENTION_CLICKHOUSE_PASSWORD:-")):
        problems.append("an optional password was not caught")
    if not user_problems(files, compose.replace("np-retention-user.xml:/etc", "np-retention-user.xml:/tmp")):
        problems.append("an unmounted user file was not caught")

    def mutant(label, attr, value):
        real = getattr(rl, attr)
        setattr(rl, attr, value)
        try:
            if run_suite() == 0:
                problems.append(f"mutant ({label}) was not caught")
        finally:
            setattr(rl, attr, real)

    mutant("redaction keeps every segment", "redact_path", lambda p: re.split(r"[?#]", p or "", maxsplit=1)[0] or "/")
    real_from = rl.from_query_log
    mutant("the query text is kept as the object", "from_query_log",
           lambda seq, row, apps: {**real_from(seq, row, apps), "object": row["query"]})
    mutant("A11 moved to the week", "ALERTS", {**rl.ALERTS, "A11": ("week", rl.ALERTS["A11"][1])})
    mutant("no CEO copy on A9", "CEO_ALWAYS", frozenset())
    mutant("the erase user's filter raises A6", "ERASE_USER", "\0none")
    for p in problems:
        print("FAIL self-test: " + p, file=sys.stderr)
    print("SELF-TEST " + ("FAIL" if problems else "PASS — the suite goes red on each broken case"))
    return 1 if problems else 0


if __name__ == "__main__":
    if "--self-test" in sys.argv[1:]:
        sys.exit(self_test())
    loader = unittest.defaultTestLoader
    count = loader.loadTestsFromTestCase(Users).countTestCases() + loader.loadTestsFromTestCase(ReadLog).countTestCases()
    print(f"scanned: {count} read-access cases")
    sys.exit(0 if unittest.main(verbosity=2, exit=False).result.wasSuccessful() else 1)
