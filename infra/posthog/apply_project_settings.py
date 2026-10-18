#!/usr/bin/env python3
"""Apply, or check, the PostHog project's privacy settings (NP-ANALYTICS-001 §3.4).

These settings used to be manual UI steps, so a fresh deployment kept client IP addresses until someone clicked. This script
states them once and makes them checkable:

    POSTHOG_PERSONAL_API_KEY=... python3 apply_project_settings.py --host https://analytics.example.org --project-id 1 --check
    POSTHOG_PERSONAL_API_KEY=... python3 apply_project_settings.py --host https://analytics.example.org --project-id 1 --apply

--check changes nothing and exits 0 only if every setting already has its required value. --apply sets them, then READS THEM
BACK and exits 0 only if the read-back matches: a setting the server silently ignores (an unsupported field name, a version that
renamed it) is a failure, not a success.

Exit codes: 0 all settings as required; 1 a setting differs (named on stderr); 2 the settings could not be read or applied
(no key, a refused request, an unreachable host, a field missing from the answer). A setting that cannot be read counts as not met.

The API key is read from the environment only, never from an argument (arguments show in process listings), and is never printed.

NOT YET EXERCISED AGAINST A REAL POSTHOG INSTANCE. The field names are those PostHog's projects API documents
(posthog.com/docs/api/projects: anonymize_ips, autocapture_opt_out, session_recording_opt_in, all booleans); the route
`/api/projects/<id>/` is the one NP-ANALYTICS-001 §7.2 already uses. Run --check against the real instance before relying on it
(OI-ANALYTICS-02). What it does not cover is listed there: heatmaps and web analytics (no field was confirmed), the Property Filter
transformation, and event retention (see np_analytics_001 §3.4: self-hosted PostHog may not enforce one).
"""
import argparse
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request

# Setting -> the value it must have. The first three are the project's privacy settings (np_analytics_001 §3.4).
REQUIRED = {
    "anonymize_ips": True,             # "Discard client IP data": no address is stored with an event
    "session_recording_opt_in": False,  # no session recording (OBJECT_STORAGE_ENABLED=false already makes it non-functional)
    "autocapture_opt_out": True,        # no automatic capture of clicks and screens
}

TIMEOUT_SECONDS = 30
LOCAL_HOSTS = ("localhost", "127.0.0.1", "::1")


class SettingsError(Exception):
    """The settings could not be read or applied (exit 2)."""


def project_url(host: str, project_id: str) -> str:
    parsed = urllib.parse.urlparse(host)
    if parsed.scheme not in ("http", "https") or not parsed.hostname:
        raise SettingsError("--host must be a URL such as https://analytics.example.org")
    if parsed.scheme == "http" and parsed.hostname not in LOCAL_HOSTS:
        raise SettingsError("--host must be https:// unless it is localhost: the API key would cross the network in the clear")
    if not project_id.isdigit():
        raise SettingsError("--project-id must be the project's number")
    return f"{parsed.scheme}://{parsed.netloc}/api/projects/{project_id}/"


def call(method: str, url: str, key: str, body: dict | None = None) -> dict:
    data = json.dumps(body).encode() if body is not None else None
    request = urllib.request.Request(url, data=data, method=method)
    request.add_header("Authorization", f"Bearer {key}")
    request.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(request, timeout=TIMEOUT_SECONDS) as response:
            answer = json.loads(response.read().decode())
    except urllib.error.HTTPError as e:
        raise SettingsError(f"{method} {url} was refused with HTTP {e.code}") from None
    except (urllib.error.URLError, TimeoutError, OSError) as e:
        raise SettingsError(f"{method} {url} failed: {getattr(e, 'reason', e)}") from None
    except ValueError:
        raise SettingsError(f"{method} {url} did not answer with JSON") from None
    if not isinstance(answer, dict):
        raise SettingsError(f"{method} {url} did not answer with an object")
    return answer


def differences(project: dict) -> list[str]:
    """What is not as required, one line each. A field the server did not return is unknown, so it counts as not met."""
    out = []
    for field, want in REQUIRED.items():
        if field not in project:
            out.append(f"{field}: not in the server's answer (required {want})")
        elif project[field] is not want:
            out.append(f"{field}: is {project[field]!r}, required {want}")
    return out


def run(host: str, project_id: str, apply: bool, key: str | None) -> int:
    if not key:
        raise SettingsError("POSTHOG_PERSONAL_API_KEY is not set")
    url = project_url(host, project_id)
    if apply:
        call("PATCH", url, key, dict(REQUIRED))
    diffs = differences(call("GET", url, key))
    for line in diffs:
        print(("after --apply, " if apply else "") + line, file=sys.stderr)
    if diffs:
        return 1
    print("project settings are as required: " + ", ".join(f"{k}={v}" for k, v in REQUIRED.items()))
    return 0


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--host", required=True, help="the PostHog base URL, e.g. https://analytics.example.org")
    ap.add_argument("--project-id", required=True, help="the project's number")
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true", help="change nothing; exit 1 if a setting differs")
    mode.add_argument("--apply", action="store_true", help="set the settings, then read them back")
    args = ap.parse_args(argv)
    try:
        return run(args.host, args.project_id, args.apply, os.environ.get("POSTHOG_PERSONAL_API_KEY"))
    except SettingsError as e:
        print(f"error: {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
