#!/usr/bin/env python3
"""Apply, or check, how long the analytics store keeps events (NP-ANALYTICS-001 §3.4 and §7.1, OI-ANALYTICS-01 (3), REQ-SW03-34).

    EVENT_RETENTION_DAYS=30 RETENTION_CLICKHOUSE_PASSWORD=... python3 apply_event_retention.py --url http://localhost:8123 --check
    ... python3 apply_event_retention.py --url http://localhost:8123 --apply

WHY 30. `NP-APP-TELEMETRY-001` §5 says a person who withdraws research consent has their pseudonymous analytics ID deleted
from the vendor within 30 DAYS. The erasure relay (erase_relay.py, np_analytics_001 §7.6) does that when it works, but until it is
shown to work against a real stack (OI-ANALYTICS-01 (2)) retention is what makes the promise true: an event older than the
retention is gone, whoever withdrew. THE PRINCIPAL SET IT TO 30 ON 2026-11-20. A value above 30 breaks that promise for as many
days as it exceeds 30, and loses nothing the policy lets NeurOne keep; a value below 30 only loses long-term trend data. So the
value is configurable (EVENT_RETENTION_DAYS, or --days), and this script says so on stderr whenever it is above 30. Never above
365: that needs a formal DPA review (np_analytics_001 §7.1).

HOW. PostHog's retention setting is a PostHog Cloud feature that a self-hosted instance may not enforce (np_analytics_001 §3.4),
so this does not rely on it: it sets a ClickHouse TTL on the events table, `TTL toDate(timestamp) + toIntervalDay(N)`, which
ClickHouse enforces itself when it merges parts. --check changes nothing and exits 0 only if every table's TTL is exactly N days.
--apply runs `ALTER TABLE ... MODIFY TTL`, then READS THE TTL BACK and exits 0 only if it matches (a table that silently kept an
old TTL is a failure). A TTL is removed from rows at the next merge, not at the instant it expires.

WHO IT RUNS AS (OI-PRIV-47, np_analytics_001 §7.7, REQ-SW03-34 (e)). Its own ClickHouse user, `np_retention`
(clickhouse/np-retention-user.xml), which may read system.tables and change the events table's TTL and nothing else. The user is
RETENTION_CLICKHOUSE_USER (default np_retention) and the password RETENTION_CLICKHOUSE_PASSWORD. It refuses to run as `posthog`,
PostHog's application user: every script and operator has a user of its own, so that the read log can say who read what, and the
application's credential is used by the application alone.

Exit codes: 0 every table is as required; 1 a table differs (named on stderr); 2 the store could not be read or changed (no
password, PostHog's own user named, a refused request, an unreachable host, an answer with no TTL to read). Anything that cannot be read counts as not met.

NOT YET EXERCISED AGAINST A REAL POSTHOG STACK (OI-ANALYTICS-01 (2)): the table name (posthog.sharded_events) is the single-node
self-host's and must be checked against POSTHOG_VERSION; a PostHog upgrade that recreates the table drops the TTL, so run --check
after every upgrade. The password is read from the environment only and is never printed.
"""
import argparse
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request

# The telemetry policy's deletion horizon for a withdrawn person (NP-APP-TELEMETRY-001 §5). Above this the policy is not met.
POLICY_DAYS = 30
DEFAULT_DAYS = 30
MAX_DAYS = 365
DEFAULT_TABLES = ("posthog.sharded_events",)
# The script's own ClickHouse user (clickhouse/np-retention-user.xml), and PostHog's application user, which it must never use
# (np_analytics_001 §7.7: PostHog's application and every script and operator are separate ClickHouse users).
DEFAULT_USER = "np_retention"
APPLICATION_USER = "posthog"
TIMEOUT_SECONDS = 30
TABLE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*\.[A-Za-z_][A-Za-z0-9_]*$")
TTL_IN_DDL = re.compile(r"\bTTL\s+toDate\(timestamp\)\s*\+\s*toIntervalDay\((\d+)\)")
LOCAL_HOSTS = ("localhost", "127.0.0.1", "::1")

POLICY_NOTICE = (
    f"warning: retention of {{days}} days is above the {POLICY_DAYS} days in NP-APP-TELEMETRY-001 §5 (a withdrawn person's analytics "
    f"ID is deleted from the vendor within {POLICY_DAYS} days): for {{over}} days the policy is not met while the erasure relay "
    "is unverified (OI-ANALYTICS-01). Setting it above 30 is the principal's decision."
)


class RetentionError(Exception):
    """The store could not be read or changed (exit 2)."""


def parse_days(raw) -> int:
    try:
        days = int(str(raw).strip())
    except ValueError:
        raise RetentionError(f"EVENT_RETENTION_DAYS must be a whole number of days, not {raw!r}") from None
    if not 1 <= days <= MAX_DAYS:
        raise RetentionError(f"EVENT_RETENTION_DAYS must be 1 to {MAX_DAYS} (more needs a formal DPA review, np_analytics_001 §7.1), not {days}")
    return days


def ttl_expression(days: int) -> str:
    return f"toDate(timestamp) + toIntervalDay({days})"


class ClickHouse:
    """Runs a query over ClickHouse's HTTP interface. Credentials travel in headers, never in the URL."""

    def __init__(self, url, user, password, opener=urllib.request.urlopen):
        parsed = urllib.parse.urlparse(url)
        if parsed.scheme not in ("http", "https") or not parsed.hostname:
            raise RetentionError("--url must be a URL such as http://localhost:8123")
        if parsed.scheme == "http" and parsed.hostname not in LOCAL_HOSTS and not parsed.hostname.endswith(".internal") and "." in parsed.hostname:
            raise RetentionError("--url must be https:// unless it is a local or compose-internal host: the password would cross the network in the clear")
        self.url, self.user, self.password, self.opener = url.rstrip("/"), user, password, opener

    def query(self, sql: str, **params) -> str:
        args = {"query": sql, **{f"param_{k}": v for k, v in params.items()}}
        request = urllib.request.Request(
            self.url + "/?" + urllib.parse.urlencode(args), method="POST",
            headers={"X-ClickHouse-User": self.user, "X-ClickHouse-Key": self.password},
        )
        try:
            with self.opener(request, timeout=TIMEOUT_SECONDS) as response:
                if not 200 <= response.status < 300:
                    raise RetentionError(f"the store answered {response.status}")
                return response.read().decode()
        except urllib.error.HTTPError as e:
            raise RetentionError(f"the store answered {e.code}") from None
        except (urllib.error.URLError, OSError) as e:
            raise RetentionError(f"the store could not be reached ({type(e).__name__})") from None


def current_days(store: ClickHouse, table: str):
    """The days of the table's TTL, or None when it has none. Raises when the table is not there to be read."""
    database, name = table.split(".")
    ddl = store.query("SELECT create_table_query FROM system.tables WHERE database = {db:String} AND name = {t:String}", db=database, t=name)
    if not ddl.strip():
        raise RetentionError(f"{table} was not found in the store")
    found = TTL_IN_DDL.search(ddl.replace("\\n", "\n").replace("\\'", "'"))
    return int(found.group(1)) if found else None


def differences(store: ClickHouse, tables, days: int) -> list:
    out = []
    for table in tables:
        have = current_days(store, table)
        if have != days:
            out.append(f"{table}: TTL is {'none' if have is None else f'{have} days'}, required {days} days")
    return out


def run(store: ClickHouse, tables, days: int, apply: bool, err=sys.stderr) -> int:
    for table in tables:
        if not TABLE.match(table):
            raise RetentionError(f"not a database.table name: {table!r}")
    if days > POLICY_DAYS:
        print(POLICY_NOTICE.format(days=days, over=days - POLICY_DAYS), file=err)
    if apply:
        for table in tables:
            store.query(f"ALTER TABLE {table} MODIFY TTL {ttl_expression(days)}")
    diffs = differences(store, tables, days)
    for line in diffs:
        print(("after --apply, " if apply else "") + line, file=err)
    if diffs:
        return 1
    print(f"event retention is {days} days on: " + ", ".join(tables))
    return 0


def main(argv) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--url", required=True, help="ClickHouse's HTTP address, e.g. http://localhost:8123")
    ap.add_argument("--days", help=f"days to keep events (default: EVENT_RETENTION_DAYS, else {DEFAULT_DAYS})")
    ap.add_argument("--tables", default=",".join(DEFAULT_TABLES), help="comma separated database.table names")
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true", help="change nothing; exit 1 if a table differs")
    mode.add_argument("--apply", action="store_true", help="set the TTL, then read it back")
    args = ap.parse_args(argv)
    try:
        days = parse_days(args.days if args.days is not None else os.environ.get("EVENT_RETENTION_DAYS", DEFAULT_DAYS))
        user = os.environ.get("RETENTION_CLICKHOUSE_USER", DEFAULT_USER).strip()
        if user == APPLICATION_USER:
            raise RetentionError(f"refusing to run as {APPLICATION_USER!r}, PostHog's own ClickHouse user: this script has its own "
                                 f"({DEFAULT_USER}, np_analytics_001 §7.7)")
        password = os.environ.get("RETENTION_CLICKHOUSE_PASSWORD")
        if not password:
            raise RetentionError("RETENTION_CLICKHOUSE_PASSWORD is not set")
        store = ClickHouse(args.url, user, password)
        return run(store, [t.strip() for t in args.tables.split(",") if t.strip()], days, args.apply)
    except RetentionError as e:
        print(f"error: {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
