#!/usr/bin/env python3
"""The analytics erasure relay (NP-ANALYTICS-001 §7.6, OI-ANALYTICS-01, OI-PRIV-48, REQ-SW03-22).

When a person withdraws research analytics the app sends their old pseudonymous analytics ID here, as

    POST /np/erase   {"distinct_id": "<the old ID>"}

and this service makes sure the analytics store holds no events under that ID. The app keeps the ID queued until it is told the
events are gone, so the answers are the contract (the app's side is AnalyticsErasureQueue, REQ-SW03-19):

    200  the store holds no events under the ID: they were deleted, or there were none. The app drops the ID.
    202  the store still holds events under the ID and the relay has queued it for its next batched delete (OI-PRIV-48). The
         app keeps the ID and asks again later; the relay deletes it whether or not the app asks again.
    400  the request is malformed (not one JSON object with one well-formed distinct_id); the app keeps the ID
    404  any other path; 405 any other method; 413 the body is too large. The app keeps the ID on these and on every other
         4xx too: only a 200 makes it drop one (REQ-SW03-19 (b)), so a relay deployed at the wrong path loses nothing
    429  too many requests just now; the app keeps the ID and asks again at its next launch
    502  the store could not be read, or the ID could not be queued; the app keeps the ID and asks again later

There is no account and no Authorization header. The ID is a random value only the phone and the analytics store know, so
presenting it is the proof the request is the owner's, and the only thing a caller can do with an ID is have it erased.

ORDER OF RELEASE. An app released before OI-PRIV-48's app half drops an ID on any 2xx, so a 202 sent to it loses an ID the store
still holds. Do not deploy this relay where such apps are still in use (NP-ANALYTICS-001 §7.6).

HOW IT WORKS.
- A request is validated before it takes a rate-limit token, so refused requests cannot starve real withdrawals.
- An ID already in the queue is answered 202. Otherwise the store is asked whether it holds events under the ID: none is 200,
  some is queued (durably, before the answer) and 202.
- A worker deletes the queued IDs in batches, one `ALTER TABLE ... DELETE WHERE distinct_id IN (...)` per table, and waits for
  the mutation to finish (`mutations_sync=2`). Only then does an ID leave the queue. A batch that fails stays queued for the next.
- The queue holds the IDs still to be deleted and nothing else: no list of erased IDs is kept, because that list is what erasure
  exists to remove. That is why a later ask is answered from the store itself and not from memory.

What it keeps: the queue file (pending IDs only, mode 0600) and a few counters in memory. No ID and no source address is written
to any log (`log_message` is silent). Caddy removes the client address before it forwards (np_analytics_001 §3.4), so the rate
limit is one bucket for the whole service, not per caller.

The ClickHouse user may delete from the events tables and read their distinct_id column, and nothing else
(clickhouse/np-erase-user.xml). Reading the column is what lets a later ask be answered without a list of erased IDs; it also lets
whoever holds this user's password list the pseudonymous IDs the store holds (OI-PRIV-48).

NOT YET EXERCISED AGAINST A REAL POSTHOG STACK (OI-ANALYTICS-01 (2)): the table name (`posthog.sharded_events`), the grant and the
mutation must be checked against the version in POSTHOG_VERSION before this is relied on. `--self-check` does it against a live
stack: it writes a test event under a throwaway ID, erases it through this code and reads it back.

Configuration is from the environment: CLICKHOUSE_URL (http://clickhouse:8123), ERASE_CLICKHOUSE_USER, ERASE_CLICKHOUSE_PASSWORD,
ERASE_TABLES (default posthog.sharded_events, comma separated), ERASE_LISTEN (default 0.0.0.0:8080), ERASE_RATE_PER_SECOND
(default 5), ERASE_BURST (default 20), ERASE_QUEUE_PATH (default /var/lib/np-erase/queue), ERASE_BATCH_SECONDS (default 60),
ERASE_BATCH_MAX (default 500), ERASE_MUTATION_TIMEOUT_SECONDS (default 600). The last three have no derivation (UC-208). The
password is never printed.
"""
import http.server
import json
import os
import re
import sys
import tempfile
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
DEFAULT_QUEUE_PATH = "/var/lib/np-erase/queue"
# No derivation (UC-208): a batch a minute keeps a withdrawal's deletion far inside the 30 days NP-APP-TELEMETRY-001 §5 allows
# while running few mutations; 500 IDs keeps one mutation's IN list and URL small; 600 s is a guess at the longest one mutation
# over a 30-day events table takes. A batch that times out stays queued and is tried again.
DEFAULT_BATCH_SECONDS = 60
DEFAULT_BATCH_MAX = 500
DEFAULT_MUTATION_TIMEOUT_SECONDS = 600


class EraseFailed(Exception):
    """The store could not be read or did not take the deletion (the request answers 502, so the app keeps the ID)."""


def _id_array(ids):
    """A ClickHouse Array(String) literal of validated IDs, for a bound parameter. Refuses anything that is not an ID."""
    for i in ids:
        if not isinstance(i, str) or not DISTINCT_ID.match(i):
            raise ValueError("not a distinct_id")
    return "[" + ",".join(f"'{i}'" for i in ids) + "]"


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
    """Reads which IDs the configured ClickHouse tables hold events under, and deletes a batch of IDs from them. The IDs are a
    bound parameter, never spliced into the query, and the credentials travel in headers, never in the URL."""

    def __init__(self, url, user, password, tables=DEFAULT_TABLES, opener=urllib.request.urlopen,
                 mutation_timeout=DEFAULT_MUTATION_TIMEOUT_SECONDS):
        for table in tables:
            if not TABLE.match(table):
                raise ValueError(f"not a database.table name: {table!r}")
        self.url, self.user, self.password, self.tables, self.opener = url, user, password, tuple(tables), opener
        self.mutation_timeout = mutation_timeout

    def _post(self, params, timeout):
        url = self.url.rstrip("/") + "/?" + urllib.parse.urlencode(params)
        request = urllib.request.Request(url, method="POST", headers={"X-ClickHouse-User": self.user, "X-ClickHouse-Key": self.password})
        try:
            with self.opener(request, timeout=timeout) as response:
                if not 200 <= response.status < 300:
                    raise EraseFailed(f"the store answered {response.status}")
                return response.read().decode()
        except urllib.error.HTTPError as error:
            raise EraseFailed(f"the store answered {error.code}") from None
        except (urllib.error.URLError, OSError, UnicodeDecodeError) as error:
            raise EraseFailed(f"the store could not be reached ({type(error).__name__})") from None

    def present(self, ids):
        """The subset of `ids` that any table still holds events under."""
        ids = list(ids)
        if not ids:
            return set()
        found = set()
        for table in self.tables:
            query = f"SELECT DISTINCT distinct_id FROM {table} WHERE distinct_id IN {{ids:Array(String)}} FORMAT TabSeparated"
            text = self._post({"query": query, "param_ids": _id_array(ids)}, TIMEOUT_SECONDS)
            found.update(line for line in text.splitlines() if line in ids)
        return found

    def erase(self, ids):
        """Delete every event under `ids` from every table, and return only once the mutations have finished."""
        ids = list(ids)
        if not ids:
            return
        for table in self.tables:
            query = f"ALTER TABLE {table} DELETE WHERE distinct_id IN {{ids:Array(String)}}"
            self._post({"query": query, "param_ids": _id_array(ids), "mutations_sync": "2"}, self.mutation_timeout)


class EraseQueue:
    """The IDs still to be deleted, kept in one file so a restart loses none. Each change is written to a new file, flushed to
    disk and moved over the old one, so a crash leaves the old list or the new one, never a torn one. The file holds pending IDs
    and nothing else, and is readable by the relay's user only."""

    def __init__(self, path):
        self.path = path
        self.lock = threading.Lock()
        self._ids = []
        if os.path.exists(path):
            with open(path, encoding="ascii") as f:
                self._ids = [line.strip() for line in f if DISTINCT_ID.match(line.strip())]

    def __contains__(self, distinct_id):
        with self.lock:
            return distinct_id in self._ids

    def __len__(self):
        with self.lock:
            return len(self._ids)

    def add(self, distinct_id):
        """Queue an ID, on disk, before returning. Raises OSError when it cannot be written; the ID is then not queued."""
        if not DISTINCT_ID.match(distinct_id):
            raise ValueError("not a distinct_id")
        with self.lock:
            if distinct_id in self._ids:
                return
            self._write(self._ids + [distinct_id])

    def batch(self, limit):
        """The oldest `limit` queued IDs, left in the queue."""
        with self.lock:
            return list(self._ids[:limit])

    def remove(self, ids):
        with self.lock:
            gone = set(ids)
            self._write([i for i in self._ids if i not in gone])

    def _write(self, ids):
        directory = os.path.dirname(os.path.abspath(self.path))
        os.makedirs(directory, mode=0o700, exist_ok=True)
        fd, tmp = tempfile.mkstemp(dir=directory, prefix=".queue-")
        try:
            with os.fdopen(fd, "w", encoding="ascii") as f:
                os.fchmod(f.fileno(), 0o600)
                f.write("".join(i + "\n" for i in ids))
                f.flush()
                os.fsync(f.fileno())
            os.replace(tmp, self.path)
        except BaseException:
            with_suppressed_unlink(tmp)
            raise
        dir_fd = os.open(directory, os.O_RDONLY)
        try:
            os.fsync(dir_fd)
        finally:
            os.close(dir_fd)
        self._ids = ids


def with_suppressed_unlink(path):
    try:
        os.unlink(path)
    except OSError:
        pass


class Counters:
    """Numbers only; never an ID or an address."""

    def __init__(self):
        self.gone = self.queued = self.erased = self.refused = self.limited = self.failed = 0


def run_batch(eraser, queue, counters, limit=DEFAULT_BATCH_MAX):
    """Delete the oldest queued IDs and take them out of the queue once the store has. A batch that fails stays queued."""
    ids = queue.batch(limit)
    if not ids:
        return 0
    try:
        eraser.erase(ids)
    except EraseFailed:
        counters.failed += 1
        return 0
    queue.remove(ids)
    counters.erased += len(ids)
    return len(ids)


class BatchWorker(threading.Thread):
    def __init__(self, eraser, queue, counters, every, limit):
        super().__init__(daemon=True)
        self.eraser, self.queue, self.counters, self.every, self.limit = eraser, queue, counters, every, limit
        self.stopped = threading.Event()

    def run(self):
        while not self.stopped.wait(self.every):
            try:
                run_batch(self.eraser, self.queue, self.counters, self.limit)
            except OSError:  # the queue could not be rewritten: the IDs stay queued, and the next batch deletes nothing and removes them
                self.counters.failed += 1


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


def make_server(address, eraser, bucket, queue, counters=None):
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
            distinct_id = parse_request(self.rfile.read(length))
            if distinct_id is None:
                counters.refused += 1
                return self._answer(400)
            # Only a well-formed request takes a token (OI-PRIV-48), so refused ones cannot starve real withdrawals.
            if not bucket.take():
                counters.limited += 1
                return self._answer(429)
            if distinct_id in queue:
                counters.queued += 1
                return self._answer(202)
            try:
                held = distinct_id in eraser.present([distinct_id])
            except EraseFailed:
                counters.failed += 1
                return self._answer(502)
            if not held:
                counters.gone += 1
                return self._answer(200)
            try:
                queue.add(distinct_id)
            except OSError:
                counters.failed += 1
                return self._answer(502)
            counters.queued += 1
            self._answer(202)

        def do_GET(self):
            if self.path == "/healthz":
                return self._answer(200)
            self._answer(404 if self.path != "/np/erase" else 405)

        do_PUT = do_DELETE = do_PATCH = lambda self: self._answer(405 if self.path == "/np/erase" else 404)

    server = http.server.ThreadingHTTPServer(address, Handler)
    server.counters = counters
    server.queue = queue
    return server


def _positive(env, name, default, cast=float):
    value = cast(env.get(name, default))
    if value <= 0:
        raise SystemExit(f"{name} must be above zero")
    return value


def from_environment(env=os.environ):
    """(address, eraser, bucket, queue settings) from the environment. Refuses to start without the store's credentials."""
    for name in ("CLICKHOUSE_URL", "ERASE_CLICKHOUSE_USER", "ERASE_CLICKHOUSE_PASSWORD"):
        if not env.get(name):
            raise SystemExit(f"{name} is not set")
    tables = tuple(t.strip() for t in env.get("ERASE_TABLES", ",".join(DEFAULT_TABLES)).split(",") if t.strip())
    timeout = _positive(env, "ERASE_MUTATION_TIMEOUT_SECONDS", DEFAULT_MUTATION_TIMEOUT_SECONDS)
    eraser = ClickHouseEraser(env["CLICKHOUSE_URL"], env["ERASE_CLICKHOUSE_USER"], env["ERASE_CLICKHOUSE_PASSWORD"], tables,
                              mutation_timeout=timeout)
    host, _, port = env.get("ERASE_LISTEN", "0.0.0.0:8080").rpartition(":")
    bucket = TokenBucket(float(env.get("ERASE_RATE_PER_SECOND", "5")), float(env.get("ERASE_BURST", "20")))
    settings = {
        "queue_path": env.get("ERASE_QUEUE_PATH", DEFAULT_QUEUE_PATH),
        "batch_seconds": _positive(env, "ERASE_BATCH_SECONDS", DEFAULT_BATCH_SECONDS),
        "batch_max": _positive(env, "ERASE_BATCH_MAX", DEFAULT_BATCH_MAX, int),
    }
    return (host, int(port)), eraser, bucket, settings


def self_check(env=os.environ):
    """Against a live stack: write one event under a throwaway ID, erase it through this code, read it back. Exit 0 only if the
    relay's own user saw it before and does not see it after. Needs ERASE_CHECK_CLICKHOUSE_USER / _PASSWORD with the right to
    INSERT (the relay's own user cannot)."""
    import uuid

    _, eraser, _, _ = from_environment(env)
    probe = str(uuid.uuid4())
    base = eraser.url.rstrip("/")
    headers = {
        "X-ClickHouse-User": env.get("ERASE_CHECK_CLICKHOUSE_USER", ""),
        "X-ClickHouse-Key": env.get("ERASE_CHECK_CLICKHOUSE_PASSWORD", ""),
    }
    table = eraser.tables[0]
    url = base + "/?" + urllib.parse.urlencode({
        "query": f"INSERT INTO {table} (uuid, event, distinct_id, team_id, timestamp) VALUES (generateUUIDv4(), 'np_erase_probe', {{id:String}}, 0, now())",
        "param_id": probe,
    })
    with urllib.request.urlopen(urllib.request.Request(url, method="POST", headers=headers), timeout=TIMEOUT_SECONDS):
        pass
    before = probe in eraser.present([probe])
    with tempfile.TemporaryDirectory() as d:
        queue = EraseQueue(os.path.join(d, "queue"))
        queue.add(probe)
        run_batch(eraser, queue, Counters())
        left = len(queue)
    after = probe in eraser.present([probe])
    print(f"self-check: seen before the erase {before}, left in the queue {left}, seen after {after}")
    return 0 if before and not after and left == 0 else 1


def main(argv):
    if "--self-check" in argv:
        return self_check()
    address, eraser, bucket, settings = from_environment()
    queue = EraseQueue(settings["queue_path"])
    server = make_server(address, eraser, bucket, queue)
    worker = BatchWorker(eraser, queue, server.counters, settings["batch_seconds"], settings["batch_max"])
    worker.start()
    print(f"erasure relay listening on {address[0]}:{address[1]}", file=sys.stderr, flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        worker.stopped.set()
        c = server.counters
        print(f"gone={c.gone} queued={c.queued} erased={c.erased} refused={c.refused} limited={c.limited} failed={c.failed} "
              f"pending={len(queue)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
