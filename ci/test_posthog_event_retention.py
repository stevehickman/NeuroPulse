#!/usr/bin/env python3
"""Tests for infra/posthog/apply_event_retention.py against a fake ClickHouse (NP-ANALYTICS-001 §3.4, OI-ANALYTICS-01 (3)).

The fake keeps one table's DDL and answers the two queries the script makes. The cases that matter are the failures: a TTL that
differs must exit 1 and be named, a table that silently keeps its old TTL must fail the read-back, anything unreadable must count
as not met (exit 2), and a value above the telemetry policy's 30 days must say so.

Run: python3 ci/test_posthog_event_retention.py [--self-test]

CI-Kind: gate
CI-Self-Test: python3 ci/test_posthog_event_retention.py --self-test
CI-Scans: the event-retention script's behaviour against a fake ClickHouse (a differing, ignored, unreadable or out-of-range retention must not read as met)
CI-Scan-Paths: infra/posthog/apply_event_retention.py
CI-Self-Test-Reads-Tree: the mutants are made by patching the real apply_event_retention.py module that the suite imports from infra/posthog
"""
# @verifies-file REQ-SW03-34
import importlib.util
import io
import pathlib
import sys
import unittest
import urllib.error
import urllib.parse

ROOT = pathlib.Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("apply_event_retention", ROOT / "infra/posthog/apply_event_retention.py")
aer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(aer)

TABLE = "posthog.sharded_events"


class Response:
    def __init__(self, body="", status=200):
        self.body, self.status = body, status

    def read(self):
        return self.body.encode()

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class FakeClickHouse:
    """One events table. `ignore_alter` keeps the old TTL after an ALTER; `fail` raises or answers an error."""

    def __init__(self, ttl_days=None, ignore_alter=False, status=None, unreachable=False, missing=False):
        self.ttl_days, self.ignore_alter, self.status, self.unreachable, self.missing = ttl_days, ignore_alter, status, unreachable, missing
        self.seen = []

    def ddl(self):
        ttl = f"\nTTL toDate(timestamp) + toIntervalDay({self.ttl_days})" if self.ttl_days else ""
        return f"CREATE TABLE posthog.sharded_events (`uuid` UUID, `timestamp` DateTime64(6))\nENGINE = MergeTree\nORDER BY uuid{ttl}\nSETTINGS index_granularity = 8192"

    def __call__(self, request, timeout=None):
        if self.unreachable:
            raise urllib.error.URLError("down")
        query = urllib.parse.parse_qs(urllib.parse.urlparse(request.full_url).query)["query"][0]
        self.seen.append((query, request.headers))
        if self.status:
            return Response("", self.status)
        if query.startswith("ALTER TABLE"):
            if not self.ignore_alter:
                self.ttl_days = int(query.rsplit("toIntervalDay(", 1)[1].rstrip(")"))
            return Response()
        return Response("" if self.missing else self.ddl())


def store(fake):
    return aer.ClickHouse("http://localhost:8123", "posthog", "secret", opener=fake)


def run(fake, days=30, apply=False):
    err = io.StringIO()
    return aer.run(store(fake), [TABLE], days, apply, err=err), err.getvalue()


class Retention(unittest.TestCase):
    def test_a_table_at_the_required_days_is_met(self):
        code, err = run(FakeClickHouse(30))
        self.assertEqual((code, err), (0, ""))

    def test_a_different_ttl_is_named_and_exits_1(self):
        code, err = run(FakeClickHouse(90))
        self.assertEqual(code, 1)
        self.assertIn(f"{TABLE}: TTL is 90 days, required 30 days", err)

    def test_no_ttl_at_all_is_not_met(self):
        code, err = run(FakeClickHouse(None))
        self.assertEqual(code, 1)
        self.assertIn("TTL is none", err)

    def test_check_changes_nothing(self):
        fake = FakeClickHouse(90)
        run(fake)
        self.assertFalse([q for q, _ in fake.seen if q.startswith("ALTER")])
        self.assertEqual(fake.ttl_days, 90)

    def test_apply_sets_the_ttl_and_reads_it_back(self):
        fake = FakeClickHouse(90)
        code, _ = run(fake, apply=True)
        self.assertEqual((code, fake.ttl_days), (0, 30))
        self.assertTrue(fake.seen[-1][0].startswith("SELECT"), "the last thing it does is read the TTL back")

    def test_a_table_that_keeps_its_old_ttl_after_apply_fails(self):
        code, err = run(FakeClickHouse(90, ignore_alter=True), apply=True)
        self.assertEqual(code, 1)
        self.assertIn("after --apply", err)

    def test_the_password_travels_in_a_header_not_the_url(self):
        fake = FakeClickHouse(30)
        run(fake)
        self.assertTrue(all(h.get("X-clickhouse-key") == "secret" for _, h in fake.seen))

    def test_unreadable_is_never_met(self):
        for fake in (FakeClickHouse(status=403), FakeClickHouse(unreachable=True), FakeClickHouse(missing=True)):
            with self.assertRaises(aer.RetentionError):
                run(fake)

    def test_above_the_policy_says_which_policy_and_by_how_many_days(self):
        code, err = run(FakeClickHouse(90), days=90)
        self.assertEqual(code, 0)
        self.assertIn("NP-APP-TELEMETRY-001 §5", err)
        self.assertIn("for 60 days", err)

    def test_at_or_below_the_policy_says_nothing(self):
        for days in (7, 30):
            self.assertEqual(run(FakeClickHouse(days), days=days)[1], "")

    def test_the_default_is_the_policys_30(self):
        self.assertEqual((aer.DEFAULT_DAYS, aer.POLICY_DAYS), (30, 30))

    def test_the_days_are_a_whole_number_of_1_to_365(self):
        self.assertEqual(aer.parse_days(" 45 "), 45)
        for bad in ("0", "-1", "366", "thirty", "", "1.5"):
            with self.assertRaises(aer.RetentionError, msg=bad):
                aer.parse_days(bad)

    def test_a_table_name_is_not_spliced_into_the_query_unchecked(self):
        with self.assertRaises(aer.RetentionError):
            aer.run(store(FakeClickHouse(30)), ["posthog.events; DROP TABLE x"], 30, False, err=io.StringIO())

    def test_a_password_is_required_and_a_clear_text_remote_url_is_refused(self):
        self.assertEqual(aer.main(["--url", "http://localhost:8123", "--check"]), 2)
        with self.assertRaises(aer.RetentionError):
            aer.ClickHouse("http://analytics.example.org:8123", "u", "p")


def run_suite() -> int:
    result = unittest.TextTestRunner(stream=io.StringIO()).run(unittest.defaultTestLoader.loadTestsFromTestCase(Retention))
    return 0 if result.wasSuccessful() else 1


def self_test() -> int:
    """The suite must go red when the script is broken in the ways that matter, and stay green on the real one.

    Mutant 1: the comparison says every table is fine. Mutant 2: --apply trusts its ALTER and skips the read-back.
    Mutant 3: the policy warning is dropped (someone raises the value above 30 and is told nothing).
    """
    problems = []
    if run_suite() != 0:
        problems.append("the suite is not green on the real script")
    real_diff, real_run, real_notice = aer.differences, aer.run, aer.POLICY_NOTICE
    try:
        aer.differences = lambda *a: []
        if run_suite() == 0:
            problems.append("mutant 1 (comparison always passes) was not caught")
    finally:
        aer.differences = real_diff

    def trusting(store, tables, days, apply, err=sys.stderr):
        if apply:
            for t in tables:
                store.query(f"ALTER TABLE {t} MODIFY TTL {aer.ttl_expression(days)}")
            return 0
        return real_run(store, tables, days, apply, err)

    try:
        aer.run = trusting
        if run_suite() == 0:
            problems.append("mutant 2 (--apply without a read-back) was not caught")
    finally:
        aer.run = real_run
    try:
        aer.POLICY_NOTICE = ""
        if run_suite() == 0:
            problems.append("mutant 3 (no policy warning above 30) was not caught")
    finally:
        aer.POLICY_NOTICE = real_notice
    for p in problems:
        print("FAIL self-test: " + p, file=sys.stderr)
    print("SELF-TEST " + ("FAIL" if problems else "PASS — the suite goes red on each broken script"))
    return 1 if problems else 0


if __name__ == "__main__":
    if "--self-test" in sys.argv[1:]:
        sys.exit(self_test())
    print(f"scanned: {unittest.defaultTestLoader.loadTestsFromTestCase(Retention).countTestCases()} retention cases")
    unittest.main(verbosity=2)
