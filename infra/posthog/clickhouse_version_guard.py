#!/usr/bin/env python3
"""Refuse to go on when the analytics store's ClickHouse is older than NeurOne's controls need (NP-ANALYTICS-001 §7.3).

A guard ON TOP OF the pin (pin_posthog.py), never in place of it: the pin decides which ClickHouse runs, because PostHog's queries
and migrations target the build its compose file names. This catches the one thing a pin cannot: a deployment whose ClickHouse is
not the pinned one (a CLICKHOUSE_SERVER_IMAGE override, an edited compose file, a store run elsewhere).

MINIMUM is 24.10. ClickHouse added `SQL SECURITY DEFINER` to views in 24.2 and extended it to parameterized views in 24.10
(ClickHouse changelog, PR #69984). The erasure relay's narrow design (OI-ANALYTICS-03 (5), OI-PRIV-48) is a parameterized
DEFINER view that answers only "which of these IDs still have events"; on an older server that view cannot be created without
handing its reader the table behind it. PostHog's own build at the pin is 26.9.

It asks `SELECT version()` over ClickHouse's HTTP interface, which needs no grant, and exits 0 only on a version it can read that
is at least MINIMUM. An unreachable server, a refused login or an unreadable answer is a failure, never a pass. The erase-relay
service runs it before the relay starts (docker-compose.neurone.yml), with the relay's own user, because that user is admitted
from the relay's container only (OI-PRIV-50).

  python3 clickhouse_version_guard.py            # reads CLICKHOUSE_URL, ERASE_CLICKHOUSE_USER, ERASE_CLICKHOUSE_PASSWORD
"""
import base64
import os
import re
import sys
import urllib.error
import urllib.request

MINIMUM = (24, 10)


def parse(text):
    """(major, minor, …) from ClickHouse's version string ('26.9.5.2', '24.10.1.2812'), or None when it is not one."""
    m = re.fullmatch(r"\s*(\d+)\.(\d+)(?:\.(\d+))?(?:\.(\d+))?\s*", text or "")
    return tuple(int(g) for g in m.groups() if g is not None) if m else None


def problem(version_text, minimum=MINIMUM):
    """None when the version is at least the minimum; otherwise what is wrong."""
    v = parse(version_text)
    if v is None:
        return f"ClickHouse answered {version_text!r}, which is not a version"
    if v[:2] < minimum:
        return (f"ClickHouse {version_text.strip()} is older than {minimum[0]}.{minimum[1]}, the minimum NeurOne's controls need "
                "(parameterized views with SQL SECURITY DEFINER); run the ClickHouse the pin names")
    return None


def server_version(url, user, password, timeout=30):
    req = urllib.request.Request(url.rstrip("/") + "/", data=b"SELECT version()", method="POST")
    if user:
        token = base64.b64encode(f"{user}:{password}".encode()).decode()
        req.add_header("Authorization", "Basic " + token)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.read().decode()


def main(env=None):
    env = os.environ if env is None else env
    url = env.get("CLICKHOUSE_URL", "")
    if not url:
        print("version guard: CLICKHOUSE_URL is not set", file=sys.stderr)
        return 1
    try:
        text = server_version(url, env.get("ERASE_CLICKHOUSE_USER", ""), env.get("ERASE_CLICKHOUSE_PASSWORD", ""))
    except (urllib.error.URLError, OSError, ValueError) as e:
        print(f"version guard: cannot read ClickHouse's version from {url}: {e}", file=sys.stderr)
        return 1
    p = problem(text)
    if p:
        print("version guard: " + p, file=sys.stderr)
        return 1
    print(f"version guard: ClickHouse {text.strip()} (minimum {MINIMUM[0]}.{MINIMUM[1]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
