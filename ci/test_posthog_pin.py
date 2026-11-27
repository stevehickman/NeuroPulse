#!/usr/bin/env python3
"""The analytics stack stays pinned to one PostHog commit, and refuses a ClickHouse older than its controls need (OI-ANALYTICS-03).

Two things, both REQ-SW03-41:

  (a) infra/posthog/pin_posthog.py --check, on the real files and on copies broken one way each: a vendored PostHog file
      edited by hand, a service with no pin, a pin made for another image string, an image tag the commit did not set, a
      PostHog-built image with no checked revision, a generated docker-compose.pin.yml edited by hand. Each must be named.
  (b) infra/posthog/clickhouse_version_guard.py against a fake ClickHouse HTTP interface: 24.10 and later pass; older,
      unreadable, unreachable and refused answers fail, never pass.

Run: python3 ci/test_posthog_pin.py [--self-test]

CI-Kind: gate
CI-Self-Test: python3 ci/test_posthog_pin.py --self-test
CI-Scans: the PostHog pin (vendored files, posthog-pin.json, docker-compose.pin.yml) and the ClickHouse version guard's behaviour against a fake server
CI-Scan-Paths: infra/posthog/pin_posthog.py infra/posthog/posthog-pin.json infra/posthog/docker-compose.pin.yml infra/posthog/clickhouse_version_guard.py
CI-Self-Test-Reads-Tree: each pin case copies the real pinned files into a temporary directory and breaks the copy in one way; the guard mutants patch the real module the suite imports
"""
# @verifies-file REQ-SW03-41
import contextlib
import http.server
import io
import json
import pathlib
import shutil
import sys
import tempfile
import threading
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
INFRA = ROOT / "infra" / "posthog"
sys.path.insert(0, str(INFRA))
import clickhouse_version_guard as guard  # noqa: E402
import pin_posthog  # noqa: E402

PINNED = pin_posthog.VENDORED + pin_posthog.COMPOSE_SCRIPTS + ("posthog-pin.json", "docker-compose.pin.yml")


def pinned_copy():
    d = pathlib.Path(tempfile.mkdtemp())
    for p in PINNED:
        (d / p).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(INFRA / p, d / p)
    return d


def problems_after(change, images_change=None):
    """pin_posthog's problems on a copy of the pinned files after `change(dir, record)`; the record is written back."""
    d = pinned_copy()
    try:
        record = json.loads((d / "posthog-pin.json").read_text())
        change(d, record)
        (d / "posthog-pin.json").write_text(json.dumps(record, indent=2) + "\n")
        images = pin_posthog.raw_images(d)
        if images_change:
            images_change(images)
        return pin_posthog.check_problems(d, images=images)
    finally:
        shutil.rmtree(d)


class Pin(unittest.TestCase):
    # @verifies REQ-SW03-41#a
    def test_the_real_pin_holds(self):
        self.assertEqual(pin_posthog.check_problems(), [])

    # @verifies REQ-SW03-41#a
    def test_a_vendored_posthog_file_edited_by_hand_is_named(self):
        def edit(d, _):
            with open(d / "docker-compose.hobby.yml", "a") as f:
                f.write("\n# edited\n")
        self.assertTrue(any(p.startswith("PIN2 docker-compose.hobby.yml") for p in problems_after(edit)))

    # @verifies REQ-SW03-41#a
    def test_a_service_with_no_pin_is_named(self):
        self.assertTrue(any(p.startswith("PIN3 service web has no pin")
                            for p in problems_after(lambda d, r: r["services"].pop("web"))))

    # @verifies REQ-SW03-41#a
    def test_a_pin_made_for_another_image_string_is_named(self):
        got = problems_after(lambda d, r: None, images_change=lambda i: i.update(capture="ghcr.io/posthog/posthog/capture:sha-1234567"))
        self.assertTrue(any(p.startswith("PIN3 capture") for p in got), got)

    # @verifies REQ-SW03-41#a
    def test_a_clickhouse_tag_the_commit_did_not_set_is_named(self):
        got = problems_after(lambda d, r: r["services"]["clickhouse"].update(tag="24.3"))
        self.assertTrue(any(p.startswith("PIN5 clickhouse") for p in got), got)

    # @verifies REQ-SW03-41#a
    def test_a_posthog_built_image_with_no_checked_revision_is_named(self):
        for change in (lambda d, r: r["services"]["capture"].pop("revision"),
                       lambda d, r: r["services"]["capture"].update(revision_is_ancestor=False)):
            self.assertTrue(any(p.startswith("PIN6 capture") for p in problems_after(change)))

    # @verifies REQ-SW03-41#a
    def test_a_digest_missing_or_the_generated_file_edited_is_named(self):
        self.assertTrue(any(p.startswith("PIN4 web") for p in problems_after(lambda d, r: r["services"]["web"].update(digest="latest"))))
        def edit(d, _):
            t = (d / "docker-compose.pin.yml").read_text()
            (d / "docker-compose.pin.yml").write_text(t.replace("@sha256:", "@sha256:0", 1))
        self.assertTrue(any(p.startswith("PIN7") for p in problems_after(edit)))

    # @verifies REQ-SW03-41#a
    def test_only_a_full_commit_sha_is_accepted_to_pin(self):
        with self.assertRaises(SystemExit):
            pin_posthog.do_pin("master")

    # @verifies REQ-SW03-41#a
    def test_floating_and_versioned_images_are_told_apart(self):
        self.assertEqual(pin_posthog.classify("ghcr.io/posthog/posthog/capture:master")["kind"], "posthog-built")
        self.assertEqual(pin_posthog.classify("$REGISTRY_URL:$POSTHOG_APP_TAG")["kind"], "posthog-built")
        self.assertEqual(pin_posthog.classify("${NODE_IMAGES_REGISTRY_URL:-ghcr.io/posthog/posthog-node}:${POSTHOG_NODE_TAG:-latest}")["kind"], "posthog-built")
        ch = pin_posthog.classify("${CLICKHOUSE_SERVER_IMAGE:-ghcr.io/posthog/clickhouse-posthog:26.9.5.2-posthog-c73aad4ad67d}")
        self.assertEqual((ch["kind"], ch["tag"]), ("versioned", "26.9.5.2-posthog-c73aad4ad67d"))
        self.assertEqual(pin_posthog.classify("caddy")["kind"], "unversioned")
        self.assertEqual(pin_posthog.classify("redis:7.2-alpine")["repository"], "library/redis")


class FakeClickHouse(http.server.BaseHTTPRequestHandler):
    answer, status, seen = "26.9.5.2\n", 200, []

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length") or 0)).decode()
        FakeClickHouse.seen.append((body, self.headers.get("Authorization")))
        self.send_response(FakeClickHouse.status)
        self.end_headers()
        self.wfile.write(FakeClickHouse.answer.encode())

    def log_message(self, *a):
        pass


def guard_with(answer, status=200, url=None):
    FakeClickHouse.answer, FakeClickHouse.status, FakeClickHouse.seen = answer, status, []
    srv = http.server.HTTPServer(("127.0.0.1", 0), FakeClickHouse)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    try:
        env = {"CLICKHOUSE_URL": url or f"http://127.0.0.1:{srv.server_port}", "ERASE_CLICKHOUSE_USER": "np_erase",
               "ERASE_CLICKHOUSE_PASSWORD": "pw"}
        saved = sys.stderr, sys.stdout
        sys.stderr = sys.stdout = io.StringIO()
        try:
            return guard.main(env)
        finally:
            sys.stderr, sys.stdout = saved
    finally:
        srv.shutdown()


class Guard(unittest.TestCase):
    # @verifies REQ-SW03-41#b
    def test_the_minimum_is_24_10(self):
        self.assertEqual(guard.MINIMUM, (24, 10))

    # @verifies REQ-SW03-41#b
    def test_24_10_and_later_pass(self):
        for v in ("24.10.1.2812", "24.10", "26.9.5.2", "25.1.1.1"):
            self.assertEqual(guard_with(v + "\n"), 0, v)
        self.assertEqual(FakeClickHouse.seen[-1][0], "SELECT version()")

    # @verifies REQ-SW03-41#b
    def test_older_versions_fail(self):
        for v in ("24.9.3.1", "24.2.1.1", "23.12.1.1368", "9.99"):
            self.assertEqual(guard_with(v), 1, v)

    # @verifies REQ-SW03-41#b
    def test_unreadable_unreachable_or_refused_fail(self):
        self.assertEqual(guard_with("not a version"), 1)
        self.assertEqual(guard_with("", status=516), 1)
        self.assertEqual(guard_with("26.9.5.2", url="http://127.0.0.1:1"), 1)
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(guard.main({}), 1)


def run_suite():
    loader = unittest.defaultTestLoader
    suite = unittest.TestSuite([loader.loadTestsFromTestCase(Pin), loader.loadTestsFromTestCase(Guard)])
    result = unittest.TextTestRunner(stream=io.StringIO()).run(suite)
    for _, tb in result.failures + result.errors:
        print(tb.strip().splitlines()[-1], file=sys.stderr)
    return 0 if result.wasSuccessful() else 1


def self_test():
    """The suite must go red when the guard or the pin check is broken in the ways that matter, and stay green on the real code."""
    problems = []
    if run_suite() != 0:
        problems.append("the suite is not green on the real code")
    saved = guard.MINIMUM, guard.problem, pin_posthog.check_problems
    mutants = (
        ("the guard's minimum lowered to 24.2", lambda: setattr(guard, "MINIMUM", (24, 2))),
        ("the guard passes anything", lambda: setattr(guard, "problem", lambda *a, **k: None)),
        ("the pin check finds nothing", lambda: setattr(pin_posthog, "check_problems", lambda *a, **k: [])),
    )
    for label, break_it in mutants:
        try:
            break_it()
            if run_suite() == 0:
                problems.append(f"mutant '{label}' survived")
        finally:
            guard.MINIMUM, guard.problem, pin_posthog.check_problems = saved
    for p in problems:
        print("FAIL " + p, file=sys.stderr)
    print("SELF-TEST " + ("FAIL" if problems else "PASS — each mutant turns the suite red"))
    return 1 if problems else 0


def main(argv):
    if "--self-test" in argv:
        return self_test()
    code = run_suite()
    print("posthog pin and version guard: " + ("FAIL" if code else "hold"))
    return code


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
