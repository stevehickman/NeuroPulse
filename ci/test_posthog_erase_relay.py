#!/usr/bin/env python3
"""Tests for infra/posthog/erase_relay.py against a fake ClickHouse (NP-ANALYTICS-001 §7.6, OI-ANALYTICS-01).

The relay is the server half of the app's erasure contract (REQ-SW03-19): 2xx means the ID is gone and the app drops it,
5xx or no answer means it is kept and retried. The cases that matter are the ones where that contract could be broken: a
store failure that answers 200, an ID spliced into the query, a body that is not exactly one well-formed ID, an unbounded
caller, and an ID or address that reaches a log.

Run: python3 ci/test_posthog_erase_relay.py [--self-test]

CI-Kind: gate
CI-Self-Test: python3 ci/test_posthog_erase_relay.py --self-test
CI-Scans: the erasure relay's behaviour against a fake ClickHouse (a failed deletion must not answer 200, the ID must be a bound parameter, malformed or excessive requests must be refused, nothing identifying may be logged)
CI-Scan-Paths: infra/posthog/erase_relay.py
CI-Self-Test-Reads-Tree: the mutants are made by patching the real erase_relay.py module that the suite imports from infra/posthog
"""
# @verifies-file REQ-SW03-22
import contextlib
import http.client
import http.server
import importlib.util
import io
import json
import pathlib
import sys
import threading
import unittest
import urllib.parse

ROOT = pathlib.Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("erase_relay", ROOT / "infra/posthog/erase_relay.py")
er = importlib.util.module_from_spec(spec)
spec.loader.exec_module(er)

GOOD_ID = "0b1c2d3e-4f50-6172-8394-a5b6c7d8e9f0"
USER, PASSWORD = "eraser", "s3cret-pass"


class FakeClickHouse:
    """Records each request; answers `status` (or drops the connection when status is None)."""

    def __init__(self, status=200):
        self.status = status
        self.seen = []
        outer = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *a):
                pass

            def do_POST(self):
                outer.seen.append((self.path, {k.lower(): v for k, v in self.headers.items()}))
                if outer.status is None:
                    self.connection.close()
                    return
                self.send_response(outer.status)
                self.send_header("Content-Length", "0")
                self.end_headers()

        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.url = f"http://127.0.0.1:{self.server.server_address[1]}"
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def close(self):
        self.server.shutdown()
        self.server.server_close()


class Relay:
    def __init__(self, store, rate=1000, burst=1000, clock=None):
        self.eraser = er.ClickHouseEraser(store.url, USER, PASSWORD)
        bucket = er.TokenBucket(rate, burst, clock) if clock else er.TokenBucket(rate, burst)
        self.server = er.make_server(("127.0.0.1", 0), self.eraser, bucket)
        self.port = self.server.server_address[1]
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def call(self, method, path, body=None, headers=None):
        conn = http.client.HTTPConnection("127.0.0.1", self.port, timeout=10)
        try:
            conn.request(method, path, body=body, headers=headers or {})
            return conn.getresponse().status
        finally:
            conn.close()

    def erase(self, payload):
        raw = payload if isinstance(payload, bytes) else json.dumps(payload).encode()
        return self.call("POST", "/np/erase", raw, {"Content-Type": "application/json"})

    def close(self):
        self.server.shutdown()
        self.server.server_close()


class FakeClock:
    def __init__(self):
        self.now = 0.0

    def __call__(self):
        return self.now


class EraseRelay(unittest.TestCase):
    def setUp(self):
        self.store = FakeClickHouse()
        self.relay = None
        self.addCleanup(self.store.close)

    def start(self, **kw):
        self.relay = Relay(self.store, **kw)
        self.addCleanup(self.relay.close)
        return self.relay

    def test_a_good_id_is_deleted_and_answers_200(self):
        relay = self.start()
        self.assertEqual(relay.erase({"distinct_id": GOOD_ID}), 200)
        self.assertEqual(len(self.store.seen), 1)

    def test_the_id_is_a_bound_parameter_and_never_in_the_query_text(self):
        self.start().erase({"distinct_id": GOOD_ID})
        path, _ = self.store.seen[0]
        params = urllib.parse.parse_qs(urllib.parse.urlparse(path).query)
        self.assertEqual(params["param_id"], [GOOD_ID])
        self.assertNotIn(GOOD_ID, params["query"][0])
        self.assertIn("{id:String}", params["query"][0])
        self.assertTrue(params["query"][0].startswith("ALTER TABLE posthog.sharded_events DELETE WHERE distinct_id"))

    def test_credentials_travel_in_headers_and_not_in_the_url(self):
        self.start().erase({"distinct_id": GOOD_ID})
        path, headers = self.store.seen[0]
        self.assertNotIn(PASSWORD, path)
        self.assertEqual(headers.get("x-clickhouse-key"), PASSWORD)
        self.assertEqual(headers.get("x-clickhouse-user"), USER)

    def test_erasing_twice_answers_200_both_times(self):
        relay = self.start()
        self.assertEqual(relay.erase({"distinct_id": GOOD_ID}), 200)
        self.assertEqual(relay.erase({"distinct_id": GOOD_ID}), 200)

    def test_a_store_that_refuses_answers_502_so_the_app_keeps_the_id(self):
        self.store.status = 500
        self.assertEqual(self.start().erase({"distinct_id": GOOD_ID}), 502)

    def test_a_store_that_does_not_answer_answers_502(self):
        self.store.status = None
        self.assertEqual(self.start().erase({"distinct_id": GOOD_ID}), 502)

    def test_a_store_that_is_down_answers_502(self):
        relay = self.start()
        self.store.close()
        self.assertEqual(relay.erase({"distinct_id": GOOD_ID}), 502)
        self.store = FakeClickHouse()  # so the cleanup has something to close

    def test_malformed_bodies_are_400_and_reach_no_store(self):
        relay = self.start()
        bad = [
            b"",
            b"not json",
            b"[]",
            b'"' + GOOD_ID.encode() + b'"',
            json.dumps({}).encode(),
            json.dumps({"distinct_id": 7}).encode(),
            json.dumps({"distinct_id": ""}).encode(),
            json.dumps({"distinct_id": "short"}).encode(),
            json.dumps({"distinct_id": "a" * 65}).encode(),
            json.dumps({"distinct_id": GOOD_ID + "' OR 1=1"}).encode(),
            json.dumps({"distinct_id": GOOD_ID, "extra": 1}).encode(),
            json.dumps({"distinct_id": GOOD_ID, "email": "a@b.c"}).encode(),
            b"\xff\xfe",
        ]
        for body in bad:
            with self.subTest(body=body):
                self.assertEqual(relay.erase(body), 400)
        self.assertEqual(self.store.seen, [])

    def test_a_body_over_the_cap_is_413(self):
        relay = self.start()
        self.assertEqual(relay.erase(b"x" * (er.MAX_BODY_BYTES + 1)), 413)
        self.assertEqual(self.store.seen, [])

    def test_a_request_with_no_length_is_400(self):
        relay = self.start()
        conn = http.client.HTTPConnection("127.0.0.1", relay.port, timeout=10)
        conn.putrequest("POST", "/np/erase")
        conn.endheaders()
        self.assertEqual(conn.getresponse().status, 400)
        conn.close()

    def test_calls_beyond_the_burst_are_429_and_refill_with_time(self):
        clock = FakeClock()
        relay = self.start(rate=1, burst=3, clock=clock)
        self.assertEqual([relay.erase({"distinct_id": GOOD_ID}) for _ in range(5)], [200, 200, 200, 429, 429])
        self.assertEqual(len(self.store.seen), 3)
        clock.now += 2
        self.assertEqual(relay.erase({"distinct_id": GOOD_ID}), 200)

    def test_other_paths_and_methods_are_refused(self):
        relay = self.start()
        self.assertEqual(relay.call("POST", "/other", b"{}", {}), 404)
        self.assertEqual(relay.call("GET", "/np/erase"), 405)
        self.assertEqual(relay.call("PUT", "/np/erase", b"{}"), 405)
        self.assertEqual(relay.call("DELETE", "/np/erase"), 405)
        self.assertEqual(relay.call("GET", "/nothing"), 404)
        self.assertEqual(relay.call("GET", "/healthz"), 200)
        self.assertEqual(self.store.seen, [])

    def test_neither_the_id_nor_the_address_is_written_to_stdout_or_stderr(self):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            relay = self.start()
            relay.erase({"distinct_id": GOOD_ID})
            relay.erase(b"junk")
            self.store.status = 500
            relay.erase({"distinct_id": GOOD_ID})
        text = out.getvalue() + err.getvalue()
        self.assertNotIn(GOOD_ID, text)
        self.assertNotIn("127.0.0.1", text)

    def test_the_counters_hold_numbers_and_no_ids(self):
        relay = self.start()
        relay.erase({"distinct_id": GOOD_ID})
        relay.erase(b"junk")
        self.store.status = 500
        relay.erase({"distinct_id": GOOD_ID})
        c = relay.server.counters
        self.assertEqual((c.erased, c.refused, c.failed), (1, 1, 1))
        self.assertFalse(any(isinstance(v, str) for v in vars(c).values()))

    def test_a_table_that_is_not_database_dot_table_is_refused_at_start(self):
        for bad in ("events", "a.b; DROP TABLE x", "a.b.c", ""):
            with self.subTest(table=bad), self.assertRaises(ValueError):
                er.ClickHouseEraser("http://x", "u", "p", tables=(bad,))

    def test_configuration_without_credentials_does_not_start(self):
        with self.assertRaises(SystemExit):
            er.from_environment({"CLICKHOUSE_URL": "http://x"})
        address, eraser, _ = er.from_environment(
            {"CLICKHOUSE_URL": "http://x", "ERASE_CLICKHOUSE_USER": "u", "ERASE_CLICKHOUSE_PASSWORD": "p"}
        )
        self.assertEqual(address, ("0.0.0.0", 8080))
        self.assertEqual(eraser.tables, ("posthog.sharded_events",))


def run_suite() -> int:
    """The number of failed or errored tests when the suite runs against the module as it currently is."""
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(EraseRelay)
    result = unittest.TextTestRunner(stream=io.StringIO(), verbosity=0).run(suite)
    return len(result.failures) + len(result.errors)


def self_test() -> int:
    """The suite must go red when the relay is broken in the ways that matter, and stay green on the real one.

    Mutant 1: any body is accepted as an ID (validation off).
    Mutant 2: the rate limit never says no.
    Mutant 3: a failed deletion is swallowed (the app would drop an ID the vendor still holds).
    Mutant 4: the ID is spliced into the query text instead of bound.
    """
    problems = []
    if run_suite() != 0:
        problems.append("the suite is not green on the real relay")

    real_parse, real_take, real_erase = er.parse_request, er.TokenBucket.take, er.ClickHouseEraser.erase

    try:
        er.parse_request = lambda raw: GOOD_ID
        if run_suite() == 0:
            problems.append("mutant 1 (validation off) was not caught")
    finally:
        er.parse_request = real_parse

    try:
        er.TokenBucket.take = lambda self: True
        if run_suite() == 0:
            problems.append("mutant 2 (no rate limit) was not caught")
    finally:
        er.TokenBucket.take = real_take

    def swallowing(self, distinct_id):
        try:
            real_erase(self, distinct_id)
        except er.EraseFailed:
            pass

    try:
        er.ClickHouseEraser.erase = swallowing
        if run_suite() == 0:
            problems.append("mutant 3 (failure swallowed) was not caught")
    finally:
        er.ClickHouseEraser.erase = real_erase

    def splicing(self, distinct_id):
        for table in self.tables:
            query = f"ALTER TABLE {table} DELETE WHERE distinct_id = '{distinct_id}'"
            url = self.url.rstrip("/") + "/?" + urllib.parse.urlencode({"query": query})
            request = er.urllib.request.Request(
                url, method="POST", headers={"X-ClickHouse-User": self.user, "X-ClickHouse-Key": self.password}
            )
            try:
                with self.opener(request, timeout=er.TIMEOUT_SECONDS):
                    pass
            except Exception:
                raise er.EraseFailed("x") from None

    try:
        er.ClickHouseEraser.erase = splicing
        if run_suite() == 0:
            problems.append("mutant 4 (ID spliced into the query) was not caught")
    finally:
        er.ClickHouseEraser.erase = real_erase

    for p in problems:
        print("FAIL self-test: " + p, file=sys.stderr)
    print("SELF-TEST " + ("FAIL" if problems else "PASS — the suite goes red on each broken relay"))
    return 1 if problems else 0


if __name__ == "__main__":
    if "--self-test" in sys.argv[1:]:
        sys.exit(self_test())
    print(f"scanned: {len(unittest.defaultTestLoader.getTestCaseNames(EraseRelay))} relay behaviours")
    unittest.main(verbosity=2)
