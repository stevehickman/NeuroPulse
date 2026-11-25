#!/usr/bin/env python3
"""Tests for infra/posthog/erase_relay.py against a fake ClickHouse (NP-ANALYTICS-001 §7.6, OI-ANALYTICS-01, OI-PRIV-48).

The relay is the server half of the app's erasure contract (REQ-SW03-19): 200 means the store holds no events under the ID and
the app drops it; 202 means the relay has queued it for a batched delete and the app keeps it; 5xx or no answer means it is kept
and retried. The cases that matter are the ones where that contract could be broken: a 200 for an ID the store still holds, a
202 for an ID that was not durably queued, a batch that failed but left the queue, an ID spliced into a query, a refused request
that spends the rate limit, an unbounded caller, and an ID or address that reaches a log.

Run: python3 ci/test_posthog_erase_relay.py [--self-test]

CI-Kind: gate
CI-Self-Test: python3 ci/test_posthog_erase_relay.py --self-test
CI-Scans: the erasure relay's behaviour against a fake ClickHouse (200 only when the store holds nothing under the ID, 202 only after the ID is queued on disk, a failed batch stays queued, IDs are bound parameters, malformed requests are refused before the rate limit, excessive requests are refused, nothing identifying is logged)
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
import os
import pathlib
import re
import stat
import sys
import tempfile
import threading
import unittest
import urllib.parse

ROOT = pathlib.Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("erase_relay", ROOT / "infra/posthog/erase_relay.py")
er = importlib.util.module_from_spec(spec)
spec.loader.exec_module(er)

GOOD_ID = "0b1c2d3e-4f50-6172-8394-a5b6c7d8e9f0"
OTHER_ID = "ffffffff-0000-1111-2222-333333333333"
THIRD_ID = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"
USER, PASSWORD = "eraser", "s3cret-pass"
LAST_BUCKET = []  # the bucket of the relay most recently started, for the self-test's token-order mutant


class FakeClickHouse:
    """A store that holds events under the IDs in `held`. It answers the relay's SELECT with the held IDs among those asked
    about, and its ALTER ... DELETE by forgetting them. `select_status` / `delete_status` force an answer (None drops the
    connection). Every request is recorded as (kind, params, headers)."""

    def __init__(self, held=()):
        self.held = set(held)
        self.select_status = 200
        self.delete_status = 200
        self.seen = []
        outer = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *a):
                pass

            def do_POST(self):
                params = {k: v[0] for k, v in urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query).items()}
                query = params.get("query", "")
                kind = "delete" if query.startswith("ALTER TABLE") else "select" if query.startswith("SELECT") else "other"
                outer.seen.append((kind, params, {k.lower(): v for k, v in self.headers.items()}))
                status = outer.delete_status if kind == "delete" else outer.select_status
                if status is None:
                    self.connection.close()
                    return
                body = b""
                if status == 200:
                    ids = re.findall(r"'([0-9A-Za-z-]+)'", params.get("param_ids", ""))
                    if kind == "select":
                        body = "".join(i + "\n" for i in ids if i in outer.held).encode()
                    elif kind == "delete":
                        outer.held -= set(ids)
                self.send_response(status)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.url = f"http://127.0.0.1:{self.server.server_address[1]}"
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def kinds(self):
        return [k for k, _, _ in self.seen]

    def close(self):
        self.server.shutdown()
        self.server.server_close()


class Relay:
    def __init__(self, store, queue_path, rate=1000, burst=1000, clock=None, tables=er.DEFAULT_TABLES):
        self.eraser = er.ClickHouseEraser(store.url, USER, PASSWORD, tables=tables)
        self.bucket = er.TokenBucket(rate, burst, clock) if clock else er.TokenBucket(rate, burst)
        LAST_BUCKET[:] = [self.bucket]
        self.queue = er.EraseQueue(queue_path)
        self.server = er.make_server(("127.0.0.1", 0), self.eraser, self.bucket, self.queue)
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

    def batch(self, limit=er.DEFAULT_BATCH_MAX):
        return er.run_batch(self.eraser, self.queue, self.server.counters, limit)

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
        self.addCleanup(lambda: self.store.close())
        self.dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.dir.cleanup)
        self.queue_path = os.path.join(self.dir.name, "queue")

    def start(self, **kw):
        relay = Relay(self.store, self.queue_path, **kw)
        self.addCleanup(relay.close)
        return relay

    def queued_on_disk(self):
        if not os.path.exists(self.queue_path):
            return []
        with open(self.queue_path) as f:
            return f.read().split()

    # ── the answers ───────────────────────────────────────────────────────────────────────────────────────────────────

    def test_an_id_the_store_does_not_hold_answers_200_and_is_not_queued(self):
        relay = self.start()
        self.assertEqual(relay.erase({"distinct_id": GOOD_ID}), 200)
        self.assertEqual(self.store.kinds(), ["select"])
        self.assertEqual(self.queued_on_disk(), [])

    def test_an_id_the_store_holds_is_queued_on_disk_and_answers_202_without_deleting_yet(self):
        self.store.held = {GOOD_ID}
        relay = self.start()
        self.assertEqual(relay.erase({"distinct_id": GOOD_ID}), 202)
        self.assertEqual(self.queued_on_disk(), [GOOD_ID])
        self.assertNotIn("delete", self.store.kinds())
        self.assertEqual(self.store.held, {GOOD_ID})

    def test_a_queued_id_answers_202_again_without_reading_the_store(self):
        self.store.held = {GOOD_ID}
        relay = self.start()
        relay.erase({"distinct_id": GOOD_ID})
        before = len(self.store.seen)
        self.assertEqual(relay.erase({"distinct_id": GOOD_ID}), 202)
        self.assertEqual(len(self.store.seen), before)
        self.assertEqual(self.queued_on_disk(), [GOOD_ID])

    def test_after_the_batch_the_id_leaves_the_queue_and_the_next_ask_answers_200(self):
        self.store.held = {GOOD_ID}
        relay = self.start()
        self.assertEqual(relay.erase({"distinct_id": GOOD_ID}), 202)
        self.assertEqual(relay.batch(), 1)
        self.assertEqual(self.store.held, set())
        self.assertEqual(self.queued_on_disk(), [])
        self.assertEqual(relay.erase({"distinct_id": GOOD_ID}), 200)

    def test_events_that_arrive_after_a_delete_are_queued_again(self):
        self.store.held = {GOOD_ID}
        relay = self.start()
        relay.erase({"distinct_id": GOOD_ID})
        relay.batch()
        self.store.held.add(GOOD_ID)  # an event still in transit when the batch ran
        self.assertEqual(relay.erase({"distinct_id": GOOD_ID}), 202)
        self.assertEqual(self.queued_on_disk(), [GOOD_ID])

    # ── the batch ─────────────────────────────────────────────────────────────────────────────────────────────────────

    def test_a_batch_is_one_waiting_delete_per_table_with_the_ids_bound(self):
        self.store.held = {GOOD_ID, OTHER_ID}
        relay = self.start(tables=("posthog.sharded_events", "posthog.other_events"))
        relay.erase({"distinct_id": GOOD_ID})
        relay.erase({"distinct_id": OTHER_ID})
        self.store.seen.clear()
        relay.batch()
        deletes = [p for k, p, _ in self.store.seen if k == "delete"]
        self.assertEqual(len(deletes), 2)
        for params, table in zip(deletes, ("posthog.sharded_events", "posthog.other_events")):
            self.assertEqual(params["query"], f"ALTER TABLE {table} DELETE WHERE distinct_id IN {{ids:Array(String)}}")
            self.assertEqual(params["param_ids"], f"['{GOOD_ID}','{OTHER_ID}']")
            self.assertEqual(params["mutations_sync"], "2", "the batch waits for the mutation before the IDs leave the queue")
            self.assertNotIn(GOOD_ID, params["query"])

    def test_the_store_check_binds_the_id_and_never_puts_it_in_the_query_text(self):
        self.start().erase({"distinct_id": GOOD_ID})
        _, params, _ = self.store.seen[0]
        self.assertEqual(params["param_ids"], f"['{GOOD_ID}']")
        self.assertNotIn(GOOD_ID, params["query"])
        self.assertTrue(params["query"].startswith("SELECT DISTINCT distinct_id FROM posthog.sharded_events WHERE distinct_id IN {ids:Array(String)}"))

    def test_a_failed_batch_keeps_the_ids_queued_for_the_next(self):
        self.store.held = {GOOD_ID}
        relay = self.start()
        relay.erase({"distinct_id": GOOD_ID})
        for status in (500, None):
            with self.subTest(status=status):
                self.store.delete_status = status
                self.assertEqual(relay.batch(), 0)
                self.assertEqual(self.queued_on_disk(), [GOOD_ID])
                self.assertEqual(relay.erase({"distinct_id": GOOD_ID}), 202)
        self.store.delete_status = 200
        self.assertEqual(relay.batch(), 1)
        self.assertEqual(self.queued_on_disk(), [])

    def test_a_batch_takes_at_most_its_limit_oldest_first(self):
        self.store.held = {GOOD_ID, OTHER_ID, THIRD_ID}
        relay = self.start()
        for i in (GOOD_ID, OTHER_ID, THIRD_ID):
            relay.erase({"distinct_id": i})
        self.assertEqual(relay.batch(limit=2), 2)
        self.assertEqual(self.queued_on_disk(), [THIRD_ID])

    def test_an_empty_queue_makes_no_request(self):
        relay = self.start()
        self.assertEqual(relay.batch(), 0)
        self.assertEqual(self.store.seen, [])

    # ── the queue ─────────────────────────────────────────────────────────────────────────────────────────────────────

    def test_the_queue_survives_a_restart_and_is_readable_by_its_owner_only(self):
        q = er.EraseQueue(self.queue_path)
        q.add(GOOD_ID)
        q.add(OTHER_ID)
        q.add(GOOD_ID)
        self.assertEqual(stat.S_IMODE(os.stat(self.queue_path).st_mode), 0o600)
        again = er.EraseQueue(self.queue_path)
        self.assertEqual(again.batch(10), [GOOD_ID, OTHER_ID])
        again.remove([GOOD_ID])
        self.assertEqual(er.EraseQueue(self.queue_path).batch(10), [OTHER_ID])

    def test_the_queue_ignores_lines_that_are_not_ids(self):
        with open(self.queue_path, "w") as f:
            f.write(f"{GOOD_ID}\nnot an id\n' OR 1=1\n\n{OTHER_ID}\n")
        self.assertEqual(er.EraseQueue(self.queue_path).batch(10), [GOOD_ID, OTHER_ID])

    def test_an_id_that_cannot_be_queued_answers_502_and_not_202(self):
        self.store.held = {GOOD_ID}
        relay = self.start()

        def refuse(_id):
            raise OSError("disk full")

        relay.queue.add = refuse
        self.assertEqual(relay.erase({"distinct_id": GOOD_ID}), 502)

    # ── failures of the store ─────────────────────────────────────────────────────────────────────────────────────────

    def test_a_store_that_refuses_the_check_answers_502_so_the_app_keeps_the_id(self):
        self.store.select_status = 500
        self.assertEqual(self.start().erase({"distinct_id": GOOD_ID}), 502)
        self.assertEqual(self.queued_on_disk(), [])

    def test_a_store_that_does_not_answer_answers_502(self):
        self.store.select_status = None
        self.assertEqual(self.start().erase({"distinct_id": GOOD_ID}), 502)

    def test_a_store_that_is_down_answers_502(self):
        relay = self.start()
        self.store.close()
        self.assertEqual(relay.erase({"distinct_id": GOOD_ID}), 502)
        self.store = FakeClickHouse()  # so the cleanup has something to close

    def test_credentials_travel_in_headers_and_not_in_the_url(self):
        self.store.held = {GOOD_ID}
        relay = self.start()
        relay.erase({"distinct_id": GOOD_ID})
        relay.batch()
        for _, params, headers in self.store.seen:
            self.assertNotIn(PASSWORD, json.dumps(params))
            self.assertEqual(headers.get("x-clickhouse-key"), PASSWORD)
            self.assertEqual(headers.get("x-clickhouse-user"), USER)

    # ── refusals and the rate limit ───────────────────────────────────────────────────────────────────────────────────

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

    def test_refused_requests_do_not_spend_the_rate_limit(self):
        relay = self.start(rate=0.001, burst=1, clock=FakeClock())
        for _ in range(10):
            self.assertEqual(relay.erase(b"junk"), 400)
        self.assertEqual(relay.erase(b"x" * (er.MAX_BODY_BYTES + 1)), 413)
        self.assertEqual(relay.erase({"distinct_id": GOOD_ID}), 200, "the one token is still there for a real request")
        self.assertEqual(relay.erase({"distinct_id": GOOD_ID}), 429)

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

    # ── what it keeps ─────────────────────────────────────────────────────────────────────────────────────────────────

    def test_neither_the_id_nor_the_address_is_written_to_stdout_or_stderr(self):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            self.store.held = {GOOD_ID}
            relay = self.start()
            relay.erase({"distinct_id": GOOD_ID})
            relay.erase(b"junk")
            relay.batch()
            self.store.select_status = 500
            relay.erase({"distinct_id": GOOD_ID})
        text = out.getvalue() + err.getvalue()
        self.assertNotIn(GOOD_ID, text)
        self.assertNotIn("127.0.0.1", text)

    def test_the_counters_hold_numbers_and_no_ids(self):
        self.store.held = {GOOD_ID}
        relay = self.start()
        relay.erase({"distinct_id": GOOD_ID})
        relay.erase({"distinct_id": OTHER_ID})
        relay.erase(b"junk")
        relay.batch()
        self.store.select_status = 500
        relay.erase({"distinct_id": GOOD_ID})
        c = relay.server.counters
        self.assertEqual((c.queued, c.gone, c.erased, c.refused, c.failed), (1, 1, 1, 1, 1))
        self.assertFalse(any(isinstance(v, str) for v in vars(c).values()))

    def test_the_id_list_refuses_anything_that_is_not_an_id(self):
        for bad in ("short", GOOD_ID + "'", "a b c d e f g h i j", 7):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                er._id_array([GOOD_ID, bad])

    # ── configuration ─────────────────────────────────────────────────────────────────────────────────────────────────

    def test_a_table_that_is_not_database_dot_table_is_refused_at_start(self):
        for bad in ("events", "a.b; DROP TABLE x", "a.b.c", ""):
            with self.subTest(table=bad), self.assertRaises(ValueError):
                er.ClickHouseEraser("http://x", "u", "p", tables=(bad,))

    def test_configuration_without_credentials_does_not_start(self):
        with self.assertRaises(SystemExit):
            er.from_environment({"CLICKHOUSE_URL": "http://x"})
        env = {"CLICKHOUSE_URL": "http://x", "ERASE_CLICKHOUSE_USER": "u", "ERASE_CLICKHOUSE_PASSWORD": "p"}
        address, eraser, _, settings = er.from_environment(env)
        self.assertEqual(address, ("0.0.0.0", 8080))
        self.assertEqual(eraser.tables, ("posthog.sharded_events",))
        self.assertEqual(settings, {"queue_path": er.DEFAULT_QUEUE_PATH, "batch_seconds": 60, "batch_max": 500})
        self.assertEqual(eraser.mutation_timeout, 600)
        for name in ("ERASE_BATCH_SECONDS", "ERASE_BATCH_MAX", "ERASE_MUTATION_TIMEOUT_SECONDS"):
            with self.subTest(name=name), self.assertRaises(SystemExit):
                er.from_environment({**env, name: "0"})


def run_suite() -> int:
    """The number of failed or errored tests when the suite runs against the module as it currently is."""
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(EraseRelay)
    result = unittest.TextTestRunner(stream=io.StringIO(), verbosity=0).run(suite)
    return len(result.failures) + len(result.errors)


def self_test() -> int:
    """The suite must go red when the relay is broken in the ways that matter, and stay green on the real one.

    Mutant 1: any body is accepted as an ID (validation off).
    Mutant 2: the rate limit never says no.
    Mutant 3: a refused request spends a token (the limit is taken before validation).
    Mutant 4: the store check is skipped, so an ID the store holds is answered 200 (the app would drop it).
    Mutant 5: a 202 is sent without the ID reaching the disk (a restart would lose it).
    Mutant 6: a failed batch still takes its IDs out of the queue.
    Mutant 7: the IDs are spliced into the delete's query text instead of bound.
    """
    problems = []
    if run_suite() != 0:
        problems.append("the suite is not green on the real relay")

    real = {
        "parse": er.parse_request,
        "take": er.TokenBucket.take,
        "present": er.ClickHouseEraser.present,
        "add": er.EraseQueue.add,
        "erase": er.ClickHouseEraser.erase,
    }

    def mutant(name, apply, restore):
        try:
            apply()
            if run_suite() == 0:
                problems.append(f"{name} was not caught")
        finally:
            restore()

    mutant("mutant 1 (validation off)",
           lambda: setattr(er, "parse_request", lambda raw: GOOD_ID),
           lambda: setattr(er, "parse_request", real["parse"]))
    mutant("mutant 2 (no rate limit)",
           lambda: setattr(er.TokenBucket, "take", lambda self: True),
           lambda: setattr(er.TokenBucket, "take", real["take"]))

    def taking_first(raw):
        if LAST_BUCKET:
            LAST_BUCKET[0].take()
        return real["parse"](raw)

    mutant("mutant 3 (token taken before validation)",
           lambda: setattr(er, "parse_request", taking_first),
           lambda: setattr(er, "parse_request", real["parse"]))
    mutant("mutant 4 (store check skipped)",
           lambda: setattr(er.ClickHouseEraser, "present", lambda self, ids: set()),
           lambda: setattr(er.ClickHouseEraser, "present", real["present"]))

    def memory_only(self, distinct_id):
        with self.lock:
            if distinct_id not in self._ids:
                self._ids = self._ids + [distinct_id]

    mutant("mutant 5 (202 without the disk)",
           lambda: setattr(er.EraseQueue, "add", memory_only),
           lambda: setattr(er.EraseQueue, "add", real["add"]))

    def swallowing(self, ids):
        try:
            real["erase"](self, ids)
        except er.EraseFailed:
            pass

    mutant("mutant 6 (failed batch leaves the queue)",
           lambda: setattr(er.ClickHouseEraser, "erase", swallowing),
           lambda: setattr(er.ClickHouseEraser, "erase", real["erase"]))

    def splicing(self, ids):
        for table in self.tables:
            listed = ",".join(f"'{i}'" for i in ids)
            self._post({"query": f"ALTER TABLE {table} DELETE WHERE distinct_id IN ({listed})", "mutations_sync": "2"},
                       self.mutation_timeout)

    mutant("mutant 7 (IDs spliced into the delete)",
           lambda: setattr(er.ClickHouseEraser, "erase", splicing),
           lambda: setattr(er.ClickHouseEraser, "erase", real["erase"]))

    for p in problems:
        print("FAIL self-test: " + p, file=sys.stderr)
    print("SELF-TEST " + ("FAIL" if problems else "PASS — the suite goes red on each broken relay"))
    return 1 if problems else 0


if __name__ == "__main__":
    if "--self-test" in sys.argv[1:]:
        sys.exit(self_test())
    print(f"scanned: {len(unittest.defaultTestLoader.getTestCaseNames(EraseRelay))} relay behaviours")
    unittest.main(verbosity=2)
