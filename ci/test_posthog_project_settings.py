#!/usr/bin/env python3
"""Tests for infra/posthog/apply_project_settings.py against a fake PostHog (NP-ANALYTICS-001 §3.4, OI-ANALYTICS-02).

The fake keeps a project's settings and answers GET and PATCH like PostHog's projects API is documented to. The cases that
matter are the failures: a setting that differs must exit 1 and be named, a setting the server silently ignores must fail the
read-back, and anything that cannot be read must count as not met (exit 2), never as met.

Run: python3 ci/test_posthog_project_settings.py [--self-test]

CI-Kind: gate
CI-Self-Test: python3 ci/test_posthog_project_settings.py --self-test
CI-Scans: the project-settings script's behaviour against a fake PostHog (a differing, ignored, unreadable or unauthorised setting must not read as met)
CI-Scan-Paths: infra/posthog/apply_project_settings.py
CI-Self-Test-Reads-Tree: the mutants are made by patching the real apply_project_settings.py module that the suite imports from infra/posthog
"""
# @verifies-file REQ-SW03-21
import contextlib
import http.server
import importlib.util
import io
import json
import os
import pathlib
import sys
import threading
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("apply_project_settings", ROOT / "infra/posthog/apply_project_settings.py")
aps = importlib.util.module_from_spec(spec)
spec.loader.exec_module(aps)

GOOD = {"anonymize_ips": True, "session_recording_opt_in": False, "autocapture_opt_out": True}
KEY = "phx_test_key"


class FakePostHog:
    """A tiny PostHog: one project, a bearer key, and switches for the ways a server can misbehave."""

    def __init__(self, project, ignore_patch=False, status=None, body=None):
        self.project = dict(project)
        self.ignore_patch = ignore_patch
        self.status = status
        self.body = body
        self.seen = []
        outer = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *a):
                pass

            def _answer(self):
                outer.seen.append((self.command, self.path, self.headers.get("Authorization")))
                if outer.status:
                    self.send_response(outer.status)
                    self.end_headers()
                    return
                payload = outer.body if outer.body is not None else json.dumps(outer.project)
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(payload.encode())

            def do_GET(self):
                if self.headers.get("Authorization") != f"Bearer {KEY}":
                    outer.seen.append((self.command, self.path, "bad"))
                    self.send_response(401)
                    self.end_headers()
                    return
                self._answer()

            def do_PATCH(self):
                n = int(self.headers.get("Content-Length", 0))
                body = json.loads(self.rfile.read(n) or b"{}")
                if not outer.ignore_patch:
                    outer.project.update(body)
                outer.patched = body
                self._answer()

        self.server = http.server.HTTPServer(("127.0.0.1", 0), Handler)
        self.url = f"http://127.0.0.1:{self.server.server_port}"
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *a):
        self.server.shutdown()
        self.server.server_close()


def run_cli(host, args, key=KEY):
    """Run main() with the key in the environment (as a person would) and return (exit code, stdout, stderr)."""
    out, err = io.StringIO(), io.StringIO()
    env = {k: v for k, v in os.environ.items() if k != "POSTHOG_PERSONAL_API_KEY"}
    if key is not None:
        env["POSTHOG_PERSONAL_API_KEY"] = key
    old = os.environ.copy()
    os.environ.clear()
    os.environ.update(env)
    try:
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = aps.main(["--host", host, "--project-id", "7", *args])
    finally:
        os.environ.clear()
        os.environ.update(old)
    return code, out.getvalue(), err.getvalue()


class ProjectSettings(unittest.TestCase):
    def test_check_passes_when_every_setting_is_as_required_and_changes_nothing(self):
        with FakePostHog(GOOD) as fake:
            code, out, err = run_cli(fake.url, ["--check"])
        self.assertEqual(code, 0, err)
        self.assertEqual([m for m, *_ in fake.seen], ["GET"], "--check must not write")
        self.assertIn("anonymize_ips=True", out)

    def test_check_names_each_setting_that_differs_and_exits_1(self):
        with FakePostHog({"anonymize_ips": False, "session_recording_opt_in": True, "autocapture_opt_out": True}) as fake:
            code, _, err = run_cli(fake.url, ["--check"])
        self.assertEqual(code, 1)
        self.assertIn("anonymize_ips", err)
        self.assertIn("session_recording_opt_in", err)
        self.assertNotIn("autocapture_opt_out", err, "a setting that is right is not reported")

    def test_apply_sets_the_settings_and_confirms_them_by_reading_back(self):
        with FakePostHog({"anonymize_ips": False, "session_recording_opt_in": True, "autocapture_opt_out": False}) as fake:
            code, out, err = run_cli(fake.url, ["--apply"])
            self.assertEqual(code, 0, err)
            self.assertEqual(fake.project, {**GOOD})
            self.assertEqual(fake.patched, GOOD)
            self.assertEqual([m for m, *_ in fake.seen], ["PATCH", "GET"], "write, then read back")

    def test_apply_fails_when_the_server_silently_ignores_a_setting(self):
        # An unsupported or renamed field is accepted with a 200 and not stored. Trusting the PATCH would report success.
        with FakePostHog({"anonymize_ips": False, "session_recording_opt_in": False, "autocapture_opt_out": True}, ignore_patch=True) as fake:
            code, _, err = run_cli(fake.url, ["--apply"])
        self.assertEqual(code, 1)
        self.assertIn("after --apply", err)
        self.assertIn("anonymize_ips", err)

    def test_a_field_the_server_does_not_return_counts_as_not_met(self):
        with FakePostHog({"anonymize_ips": True, "session_recording_opt_in": False}) as fake:
            code, _, err = run_cli(fake.url, ["--check"])
        self.assertEqual(code, 1)
        self.assertIn("autocapture_opt_out: not in the server's answer", err)

    def test_a_value_that_is_truthy_but_not_the_boolean_is_not_met(self):
        with FakePostHog({"anonymize_ips": "true", "session_recording_opt_in": 0, "autocapture_opt_out": 1}) as fake:
            code, _, err = run_cli(fake.url, ["--check"])
        self.assertEqual(code, 1)
        self.assertEqual(err.count("required"), 3)

    def test_a_refused_request_is_exit_2_not_a_pass(self):
        with FakePostHog(GOOD, status=403) as fake:
            code, _, err = run_cli(fake.url, ["--check"])
        self.assertEqual(code, 2)
        self.assertIn("HTTP 403", err)

    def test_a_wrong_key_is_exit_2(self):
        with FakePostHog(GOOD) as fake:
            code, _, err = run_cli(fake.url, ["--check"], key="wrong")
        self.assertEqual(code, 2)
        self.assertIn("HTTP 401", err)

    def test_an_answer_that_is_not_json_or_not_an_object_is_exit_2(self):
        for body in ("<html>login</html>", "[1, 2]", ""):
            with FakePostHog(GOOD, body=body) as fake:
                code, _, _ = run_cli(fake.url, ["--check"])
            self.assertEqual(code, 2, repr(body))

    def test_an_unreachable_host_is_exit_2(self):
        code, _, err = run_cli("http://127.0.0.1:9", ["--check"])
        self.assertEqual(code, 2)
        self.assertIn("failed", err)

    def test_no_key_is_exit_2_and_nothing_is_sent(self):
        with FakePostHog(GOOD) as fake:
            code, _, err = run_cli(fake.url, ["--check"], key=None)
        self.assertEqual(code, 2)
        self.assertIn("POSTHOG_PERSONAL_API_KEY", err)
        self.assertEqual(fake.seen, [])

    def test_the_key_is_sent_as_a_bearer_header_and_never_printed(self):
        with FakePostHog(GOOD) as fake:
            code, out, err = run_cli(fake.url, ["--apply"])
        self.assertTrue(all(auth == f"Bearer {KEY}" for _, _, auth in fake.seen))
        self.assertNotIn(KEY, out + err)

    def test_a_plain_http_host_is_refused_unless_it_is_local(self):
        code, _, err = run_cli("http://analytics.example.org", ["--check"])
        self.assertEqual(code, 2)
        self.assertIn("https", err)

    def test_the_host_and_project_id_must_be_well_formed(self):
        for host in ("analytics.example.org", "ftp://analytics.example.org", "https://"):
            self.assertEqual(run_cli(host, ["--check"])[0], 2, host)
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            os.environ["POSTHOG_PERSONAL_API_KEY"] = KEY
            try:
                code = aps.main(["--host", "https://analytics.example.org", "--project-id", "7/../8", "--check"])
            finally:
                del os.environ["POSTHOG_PERSONAL_API_KEY"]
        self.assertEqual(code, 2)

    def test_the_required_settings_are_the_three_the_doc_names(self):
        self.assertEqual(aps.REQUIRED, GOOD)


def run_suite() -> int:
    """The number of failed or errored tests when the suite runs against the module as it currently is."""
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ProjectSettings)
    result = unittest.TextTestRunner(stream=io.StringIO(), verbosity=0).run(suite)
    return len(result.failures) + len(result.errors)


def self_test() -> int:
    """The suite must go red when the script is broken in the ways that matter, and stay green on the real one.

    Mutant 1: the comparison always says every setting is fine (the script would report success for any project).
    Mutant 2: --apply trusts its PATCH and skips the read-back (a setting the server silently ignores would read as applied).
    """
    problems = []
    if run_suite() != 0:
        problems.append("the suite is not green on the real script")

    real_differences, real_run = aps.differences, aps.run
    try:
        aps.differences = lambda project: []
        if run_suite() == 0:
            problems.append("mutant 1 (comparison always passes) was not caught")
    finally:
        aps.differences = real_differences

    def trusting_run(host, project_id, apply, key):
        if not key:
            raise aps.SettingsError("POSTHOG_PERSONAL_API_KEY is not set")
        url = aps.project_url(host, project_id)
        if apply:
            aps.call("PATCH", url, key, dict(aps.REQUIRED))
            return 0
        return real_run(host, project_id, apply, key)

    try:
        aps.run = trusting_run
        if run_suite() == 0:
            problems.append("mutant 2 (--apply without a read-back) was not caught")
    finally:
        aps.run = real_run
    for p in problems:
        print("FAIL self-test: " + p, file=sys.stderr)
    print("SELF-TEST " + ("FAIL" if problems else "PASS — the suite goes red on each broken script"))
    return 1 if problems else 0


if __name__ == "__main__":
    if "--self-test" in sys.argv[1:]:
        sys.exit(self_test())
    print(f"scanned: {len(aps.REQUIRED)} project settings")
    unittest.main(verbosity=2)
