#!/usr/bin/env python3
"""The analytics erasure relay (NP-ANALYTICS-001 §7.6, OI-ANALYTICS-01, REQ-SW03-22).

When a person withdraws research analytics the app sends their old pseudonymous analytics ID here, once, as

    POST /np/erase   {"distinct_id": "<the old ID>"}

and this service deletes the events the analytics store holds under that ID. The app keeps the ID queued until the answer is
final, so the answers are the contract (the app's side is AnalyticsErasureQueue, REQ-SW03-19):

    200  the events were deleted, or there were none (the call is idempotent); the app drops the ID
    400  the request is malformed (not one JSON object with one well-formed distinct_id); the app drops the ID for good
    404  any other path; 405 any other method
    413  the body is too large
    429  too many requests just now; the app keeps the ID and asks again at its next launch
    502  the store did not take the deletion; the app keeps the ID and asks again later

There is no account and no Authorization header. The ID is a random value only the phone and the analytics store know, so
presenting it is the proof the request is the owner's, and the only thing a caller can do with an ID is erase it.

What it keeps: nothing. No ID and no source address is written to any log or to disk (`log_message` is silent), and the only
state is a few counters and a token bucket in memory. A log of IDs would be the list this feature exists to erase. Caddy removes
the client address before it forwards (np_analytics_001 §3.4), so the rate limit is one bucket for the whole service, not per
caller.

HOW IT DELETES. The app sets `personProfiles = .never`, so PostHog holds no person record for the ID, and the person-deletion
call of np_analytics_001 §7.2 needs one. The events are rows of ClickHouse's events table with a `distinct_id` column, so this
deletes them there, with a ClickHouse user that may do nothing else (see .env.example). NOT YET EXERCISED AGAINST A REAL POSTHOG
STACK (OI-ANALYTICS-01 (2)): the table name (`posthog.sharded_events`) and the mutation are those of a single-node PostHog
self-host and must be checked against the version in POSTHOG_VERSION before this is relied on. `--self-check` does it against
a live stack: it writes a test event under a throwaway ID, erases it through this code and reads it back.

Configuration is from the environment: CLICKHOUSE_URL (http://clickhouse:8123), ERASE_CLICKHOUSE_USER, ERASE_CLICKHOUSE_PASSWORD,
ERASE_TABLES (default posthog.sharded_events, comma separated), ERASE_LISTEN (default 0.0.0.0:8080), ERASE_RATE_PER_SECOND
(default 5), ERASE_BURST (default 20). The password is never printed.
"""
import http.server
import json
import os
import re
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request

MAX_BODY_BYTES = 1024
TIMEOUT_SECONDS = 30
# What PostHog's SDKs generate as an anonymous ID is a UUID; this accepts that and any ID of the same alphabet and a sane length.
DISTINCT_ID = re.compile(r"^[0-9A-Za-z-]{16,64}$")
TABLE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*\.[A-Za-z_][A-Za-z0-9_]*$")
DEFAULT_TABLES = ("posthog.sharded_events",)


class EraseFailed(Exception):
    """The store did not take the deletion (answered 502, so the app keeps the ID)."""


class TokenBucket:
    """One bucket for the whole service: `rate` tokens a second up to `burst`. `take()` is False when empty."""

    def __init__(self, rate, burst, clock=time.monotonic):
        self.rate, self.burst, self.clock = float(rate), float(burst), clock
        self.tokens, self.last = float(burst), clock()
        self.lock = threading.Lock()

    def take(self):
        with self.lock:
            now = self.clock()
            self.tokens = min(self.burst, self.tokens + (now - self.last) * self.rate)
            self.last = now
            if self.tokens >= 1:
                self.tokens -= 1
                return True
            return False


class ClickHouseEraser:
    """Deletes the events under an ID from the configured ClickHouse tables. The ID is a bound parameter, never spliced into
    the query, and the credentials travel in headers, never in the URL."""

    def __init__(self, url, user, password, tables=DEFAULT_TABLES, opener=urllib.request.urlopen):
        for table in tables:
            if not TABLE.match(table):
                raise ValueError(f"not a database.table name: {table!r}")
        self.url, self.user, self.password, self.tables, self.opener = url, user, password, tuple(tables), opener

    def erase(self, distinct_id):
        for table in self.tables:
            query = f"ALTER TABLE {table} DELETE WHERE distinct_id = {{id:String}}"
            url = self.url.rstrip("/") + "/?" + urllib.parse.urlencode({"query": query, "param_id": distinct_id})
            request = urllib.request.Request(
                url, method="POST", headers={"X-ClickHouse-User": self.user, "X-ClickHouse-Key": self.password}
            )
            try:
                with self.opener(request, timeout=TIMEOUT_SECONDS) as response:
                    if not 200 <= response.status < 300:
                        raise EraseFailed(f"the store answered {response.status}")
            except urllib.error.HTTPError as error:
                raise EraseFailed(f"the store answered {error.code}") from None
            except (urllib.error.URLError, OSError) as error:
                raise EraseFailed(f"the store could not be reached ({type(error).__name__})") from None


class Counters:
    def __init__(self):
        self.erased = self.refused = self.limited = self.failed = 0


def parse_request(raw):
    """The distinct_id in a body that is exactly one JSON object with exactly that one well-formed string, else None."""
    try:
        body = json.loads(raw)
    except (ValueError, UnicodeDecodeError):
        return None
    if not isinstance(body, dict) or set(body) != {"distinct_id"}:
        return None
    value = body["distinct_id"]
    return value if isinstance(value, str) and DISTINCT_ID.match(value) else None


def make_server(address, eraser, bucket, counters=None):
    counters = counters or Counters()

    class Handler(http.server.BaseHTTPRequestHandler):
        server_version = "np-erase"
        sys_version = ""

        def log_message(self, *args):
            """Silent by design: neither the ID nor the source address is written anywhere."""

        def _answer(self, status):
            self.send_response(status)
            self.send_header("Content-Length", "0")
            self.end_headers()

        def do_POST(self):
            if self.path != "/np/erase":
                return self._answer(404)
            try:
                length = int(self.headers.get("Content-Length", "-1"))
            except ValueError:
                length = -1
            if length < 0:
                counters.refused += 1
                return self._answer(400)
            if length > MAX_BODY_BYTES:
                counters.refused += 1
                return self._answer(413)
            if not bucket.take():
                counters.limited += 1
                return self._answer(429)
            distinct_id = parse_request(self.rfile.read(length))
            if distinct_id is None:
                counters.refused += 1
                return self._answer(400)
            try:
                eraser.erase(distinct_id)
            except EraseFailed:
                counters.failed += 1
                return self._answer(502)
            counters.erased += 1
            self._answer(200)

        def do_GET(self):
            if self.path == "/healthz":
                return self._answer(200)
            self._answer(404 if self.path != "/np/erase" else 405)

        do_PUT = do_DELETE = do_PATCH = lambda self: self._answer(405 if self.path == "/np/erase" else 404)

    server = http.server.ThreadingHTTPServer(address, Handler)
    server.counters = counters
    return server


def from_environment(env=os.environ):
    for name in ("CLICKHOUSE_URL", "ERASE_CLICKHOUSE_USER", "ERASE_CLICKHOUSE_PASSWORD"):
        if not env.get(name):
            raise SystemExit(f"{name} is not set")
    tables = tuple(t.strip() for t in env.get("ERASE_TABLES", ",".join(DEFAULT_TABLES)).split(",") if t.strip())
    eraser = ClickHouseEraser(env["CLICKHOUSE_URL"], env["ERASE_CLICKHOUSE_USER"], env["ERASE_CLICKHOUSE_PASSWORD"], tables)
    host, _, port = env.get("ERASE_LISTEN", "0.0.0.0:8080").rpartition(":")
    bucket = TokenBucket(float(env.get("ERASE_RATE_PER_SECOND", "5")), float(env.get("ERASE_BURST", "20")))
    return (host, int(port)), eraser, bucket


def self_check(env=os.environ):
    """Against a live stack: write one event under a throwaway ID, erase it through this code, read it back. Exit 0 only if it
    is gone. Needs ERASE_CHECK_CLICKHOUSE_USER / _PASSWORD with the right to INSERT and SELECT (the relay's own user cannot)."""
    import uuid

    _, eraser, _ = from_environment(env)
    probe = str(uuid.uuid4())
    base = eraser.url.rstrip("/")
    headers = {
        "X-ClickHouse-User": env.get("ERASE_CHECK_CLICKHOUSE_USER", ""),
        "X-ClickHouse-Key": env.get("ERASE_CHECK_CLICKHOUSE_PASSWORD", ""),
    }

    def run(query, **params):
        url = base + "/?" + urllib.parse.urlencode({"query": query, **params})
        with urllib.request.urlopen(urllib.request.Request(url, method="POST", headers=headers), timeout=TIMEOUT_SECONDS) as r:
            return r.read().decode().strip()

    table = eraser.tables[0]
    run(f"INSERT INTO {table} (uuid, event, distinct_id, team_id, timestamp) VALUES (generateUUIDv4(), 'np_erase_probe', {{id:String}}, 0, now())", param_id=probe)
    before = int(run(f"SELECT count() FROM {table} WHERE distinct_id = {{id:String}}", param_id=probe))
    eraser.erase(probe)
    for _ in range(30):  # a mutation finishes a moment later
        after = int(run(f"SELECT count() FROM {table} WHERE distinct_id = {{id:String}}", param_id=probe))
        if after == 0:
            break
        time.sleep(1)
    print(f"self-check: {before} probe event(s) written, {after} left after the erase")
    return 0 if before >= 1 and after == 0 else 1


def main(argv):
    if "--self-check" in argv:
        return self_check()
    address, eraser, bucket = from_environment()
    server = make_server(address, eraser, bucket)
    print(f"erasure relay listening on {address[0]}:{address[1]}", file=sys.stderr, flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        c = server.counters
        print(f"erased={c.erased} refused={c.refused} limited={c.limited} failed={c.failed}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
