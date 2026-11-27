#!/usr/bin/env python3
"""The analytics read log: its entry format, and the alert rules A1 to A11 (NP-ANALYTICS-001 §7.7 and §7.8, OI-PRIV-47, REQ-SW03-40).

The principal decided on 2026-11-24 that every read of the analytics data is recorded, that the record holds no analytics ID and
no query value, and that eleven "should never happen" triggers raise an alert on every occurrence. This module is the part of that
which does not depend on the deployment: what one log entry is, how a PostHog page path or a ClickHouse query-log row becomes one,
and which entries and events raise which alert, routed how. It does NOT forward anything, run a proxy, hold a bucket or send an
alert: the sign-in proxy, the forwarder, the locked bucket and the alert channel are still to be built and wait on deployment
choices (the identity provider, the cloud provider and the channel; docs/human-input/oi-priv-47-read-log-deployment.md).

ONE ENTRY is one JSON object on one line (JSON Lines), with exactly these keys:

    v       1, the format version
    seq     the forwarder's own counter, +1 per entry, so a gap shows an entry lost between forwarder and bucket
    at      when the read happened, UTC, whole seconds: "2026-11-26T09:15:02Z"
    source  "proxy" (a PostHog page through the sign-in proxy), "clickhouse" (a query in ClickHouse's query log) or
            "heartbeat" (the forwarder saying it is alive, A8)
    who     the identity: the identity provider's account name for "proxy", the ClickHouse user for "clickhouse", the
            forwarder's name for "heartbeat"
    object  "proxy": the page's path with every segment that is not a known route word replaced by ":id", and no query
            string; "clickhouse": the tables read, sorted, comma separated; "heartbeat": ""
    kind    "read", "single_installation" (one installation's events on their own, A6), "export" (A7), "access_change"
            (an account, user or grant created, changed or removed, A5) or "heartbeat"
    rows    rows returned (ClickHouse's result_rows), or null where the source cannot know it (a proxied page)
    query   ClickHouse's normalized_query_hash as 16 hex digits, or null. ClickHouse computes it with every literal removed,
            so it identifies the query's shape and carries no value
    outcome "ok", "refused" (the server refused the grant, A3) or "error"
    origin  "clickhouse" only: "application" when the query came from PostHog's own container, "other" when it did not (A2);
            null for the other sources. The address it was decided from is not kept.

WHAT IS NEVER IN AN ENTRY: an analytics ID (distinct_id, person ID, event UUID), a query's text or any value in it, a client
address, a user agent, a request header. The path redaction is UNCONDITIONAL (CLAUDE.md §5.1 rule 2): it does not try to recognise
an ID, which would leak whatever it failed to recognise; it keeps only segments on a fixed list of route words and replaces every
other segment with ":id", so a page whose path carries an ID is recorded as its page type. A route word PostHog adds in a later
version is recorded as ":id" until it joins ROUTE_WORDS: over-redaction, never a leak. `validate()` refuses an entry that breaks
any of this, so a forwarder built on this module cannot write one.

NOT YET EXERCISED AGAINST A REAL STACK: ROUTE_WORDS is PostHog's route vocabulary as far as known, to be checked against the PostHog
commit pinned in posthog-pin.json when the proxy is built; the query-log columns are ClickHouse 23.12's (OI-ANALYTICS-03 (4)).
"""
import json
import re
import sys
from datetime import datetime, timezone

FORMAT_VERSION = 1
SOURCES = ("proxy", "clickhouse", "heartbeat")
KINDS = ("read", "single_installation", "export", "access_change", "heartbeat")
OUTCOMES = ("ok", "refused", "error")
ORIGINS = ("application", "other")
KEYS = ("v", "seq", "at", "source", "who", "object", "kind", "rows", "query", "outcome", "origin")
REDACTED = ":id"

# The words a PostHog page or API path is made of. Any other segment is replaced by ":id". Lower case; compared lower case.
ROUTE_WORDS = frozenset("""
    api project projects environments organization organizations person persons events event insights insight dashboard
    dashboards cohorts cohort data-management definitions properties actions activity explore sql query web home settings
    feature_flags experiments surveys notebooks annotations exports exported export batch_exports csv toolbar static login logout
    signup me users groups replay session_recordings trends funnels retention paths stickiness lifecycle new edit
""".split())
# Page types, read from the redacted template only, so the type survives the ID's removal (§7.7, A6 and A7).
SINGLE_INSTALLATION_WORDS = frozenset({"person", "persons", "event", "events", "session_recordings", "replay"})
EXPORT_WORDS = frozenset({"export", "exports", "exported", "batch_exports", "csv"})

# ClickHouse 23.12 error codes that mean "not allowed", as opposed to a broken query (A3).
REFUSED_CODES = frozenset({497, 516, 192})  # ACCESS_DENIED, AUTHENTICATION_FAILED, UNKNOWN_USER
ACCESS_CHANGE = re.compile(r"^\s*(?:(?:CREATE|ALTER|DROP)\s+(?:USER|ROLE|QUOTA|ROW\s+POLICY|POLICY|SETTINGS\s+PROFILE|PROFILE)\b|GRANT\b|REVOKE\b)", re.I)
# A filter on one installation's ID: `distinct_id = …`, `distinct_id IN (…)`, `person_id = …` (A6 in ClickHouse).
SINGLE_INSTALLATION_FILTER = re.compile(r"\b(?:distinct_id|person_id)\s*(?:=|==|\bIN\b|\bLIKE\b)", re.I)
EXPORT_QUERY = re.compile(r"\bINTO\s+OUTFILE\b|\bFORMAT\s+(?:CSV|TSV|TabSeparated|Parquet|Arrow|JSONEachRow|Native)\w*\b|\b(?:s3|url|file|hdfs)\s*\(", re.I)
HASH = re.compile(r"^[0-9a-f]{16}$")
AT = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
WHO = re.compile(r"^[A-Za-z0-9_.@+-]{1,128}$")
TABLE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*\.[A-Za-z_.][A-Za-z0-9_.]*$")

# The ClickHouse user the erasure relay deletes with: the one user that may filter on distinct_id (§7.6, A6).
ERASE_USER = "np_erase"

# A8. The forwarder writes a heartbeat entry every HEARTBEAT_SECONDS whether or not anything was read, and A8 fires when the
# bucket's newest entry is older than HEARTBEAT_SECONDS * HEARTBEAT_MISSED. NOT DERIVED: UC-221 (OI-PRIV-47). A8 is routed
# immediately, so the interval bounds how long a stopped forwarder silences every other alert; nothing sets the figure.
HEARTBEAT_SECONDS = 300
HEARTBEAT_MISSED = 3
# A10. The drift checks run once a day. NOT DERIVED: UC-222 (OI-PRIV-47). The bound is derived (A10 is routed within the week, so
# the checks must run at least weekly, or a drift could go a whole routing period unseen); the daily figure is not.
A10_CHECK_INTERVAL_SECONDS = 24 * 3600
A10_MAX_INTERVAL_SECONDS = 7 * 24 * 3600

# Routing (the principal, 2026-11-24, NP-ANALYTICS-001 §7.7). "immediate": to the Privacy Lead at once; "week": within the week.
ALERTS = {
    "A1": ("immediate", "a read by an identity not on the approved list"),
    "A2": ("immediate", "PostHog's own ClickHouse credential used from anywhere but the PostHog container"),
    "A3": ("week", "a refused query on an analytics table"),
    "A4": ("immediate", "a PostHog page reached without passing through the sign-in proxy"),
    "A5": ("week", "an account or permission created, changed or removed"),
    "A6": ("week", "one installation's events viewed on their own"),
    "A7": ("week", "an export or bulk download"),
    "A8": ("immediate", "the read log stopped arriving"),
    "A9": ("immediate", "the locked bucket's lock, retention or deletion rule changed, or a deletion attempted"),
    "A10": ("week", "a privacy setting drifted"),
    "A11": ("immediate", "an erasure failed"),
}
CEO_ALWAYS = frozenset({"A9"})


class EntryError(ValueError):
    """An entry that must not be written."""


def at_utc(when: datetime) -> str:
    if when.tzinfo is None:
        raise EntryError("a time without a zone is ambiguous")
    return when.astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def redact_path(path: str) -> str:
    """The path with its query string and fragment dropped and every segment not in ROUTE_WORDS replaced by ":id"."""
    bare = re.split(r"[?#]", path or "", maxsplit=1)[0]
    segments = [s for s in bare.split("/") if s]
    return "/" + "/".join(s.lower() if s.lower() in ROUTE_WORDS else REDACTED for s in segments)


def page_kind(template: str) -> str:
    """The page type of a redacted template: an export, one installation's events (a single-installation word followed by an
    ID), or an ordinary read."""
    segments = [s for s in template.split("/") if s]
    if any(s in EXPORT_WORDS for s in segments):
        return "export"
    for word, nxt in zip(segments, segments[1:]):
        if word in SINGLE_INSTALLATION_WORDS and nxt == REDACTED:
            return "single_installation"
    return "read"


def validate(entry: dict) -> dict:
    """Raise EntryError unless `entry` is exactly the format above and holds nothing the format forbids."""
    if not isinstance(entry, dict) or tuple(sorted(entry)) != tuple(sorted(KEYS)):
        raise EntryError(f"an entry has exactly the keys {', '.join(KEYS)}")
    e = entry
    if e["v"] != FORMAT_VERSION or type(e["seq"]) is not int or e["seq"] < 0:
        raise EntryError("bad v or seq")
    if not isinstance(e["at"], str) or not AT.match(e["at"]):
        raise EntryError("at is UTC to the second, e.g. 2026-11-26T09:15:02Z")
    if e["source"] not in SOURCES or e["kind"] not in KINDS or e["outcome"] not in OUTCOMES:
        raise EntryError("unknown source, kind or outcome")
    if not isinstance(e["who"], str) or not WHO.match(e["who"]):
        raise EntryError("who is an account or user name")
    if e["rows"] is not None and (type(e["rows"]) is not int or e["rows"] < 0):
        raise EntryError("rows is a count or null")
    if e["query"] is not None and (not isinstance(e["query"], str) or not HASH.match(e["query"])):
        raise EntryError("query is a 16-hex-digit normalized hash or null")
    obj = e["object"]
    if not isinstance(obj, str):
        raise EntryError("object is a string")
    if e["source"] == "proxy":
        if redact_path(obj) != obj:
            raise EntryError("a proxied page is recorded only as its redacted template")
        if e["query"] is not None or e["origin"] is not None:
            raise EntryError("a proxied page has no query hash and no origin")
    elif e["source"] == "clickhouse":
        if obj and not all(TABLE.match(t) for t in obj.split(",")):
            raise EntryError("a ClickHouse entry's object is database.table names")
        if e["origin"] not in ORIGINS:
            raise EntryError("a ClickHouse entry says whether it came from the application")
    else:
        if (obj, e["kind"], e["rows"], e["query"], e["origin"]) != ("", "heartbeat", None, None, None):
            raise EntryError("a heartbeat carries nothing but who, when and seq")
    if (e["kind"] == "heartbeat") != (e["source"] == "heartbeat"):
        raise EntryError("only a heartbeat is of kind heartbeat")
    return entry


def from_proxy(seq: int, when: datetime, who: str, path: str, outcome: str = "ok") -> dict:
    """An entry for one PostHog page the sign-in proxy let `who` see."""
    template = redact_path(path)
    return validate({"v": FORMAT_VERSION, "seq": seq, "at": at_utc(when), "source": "proxy", "who": who, "object": template,
                     "kind": page_kind(template), "rows": None, "query": None, "outcome": outcome, "origin": None})


def from_query_log(seq: int, row: dict, application_addresses) -> dict:
    """An entry for one row of ClickHouse's system.query_log (type QueryFinish, ExceptionBeforeStart or ExceptionWhileProcessing).

    `row` holds: user, event_time (an aware datetime), tables (list), result_rows, normalized_query_hash (int), exception_code,
    initial_address, query. The query's text is read here to classify it (A5, A6, A7) and is then dropped with the address; the
    entry keeps neither."""
    text = row.get("query") or ""
    code = int(row.get("exception_code") or 0)
    if ACCESS_CHANGE.search(text):
        kind = "access_change"
    elif EXPORT_QUERY.search(text):
        kind = "export"
    elif SINGLE_INSTALLATION_FILTER.search(text):
        kind = "single_installation"
    else:
        kind = "read"
    return validate({
        "v": FORMAT_VERSION, "seq": seq, "at": at_utc(row["event_time"]), "source": "clickhouse", "who": row["user"],
        "object": ",".join(sorted(set(row.get("tables") or ()))), "kind": kind,
        "rows": int(row.get("result_rows") or 0), "query": f"{int(row['normalized_query_hash']) & 0xFFFFFFFFFFFFFFFF:016x}",
        "outcome": "ok" if code == 0 else ("refused" if code in REFUSED_CODES else "error"),
        "origin": "application" if row.get("initial_address") in set(application_addresses) else "other",
    })


def heartbeat(seq: int, when: datetime, forwarder: str) -> dict:
    return validate({"v": FORMAT_VERSION, "seq": seq, "at": at_utc(when), "source": "heartbeat", "who": forwarder, "object": "",
                     "kind": "heartbeat", "rows": None, "query": None, "outcome": "ok", "origin": None})


def line(entry: dict) -> str:
    """The entry as the one line the forwarder writes."""
    return json.dumps(validate(entry), separators=(",", ":"), sort_keys=True)


# ── Alerts ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

def alert(rule: str, cause: str, who=None, privacy_lead=None) -> dict:
    """An alert, routed: the Privacy Lead always; the CEO too on A9 and whenever the Privacy Lead's own account caused it."""
    routing, what = ALERTS[rule]
    ceo = rule in CEO_ALWAYS or (who is not None and privacy_lead is not None and who in set(privacy_lead))
    return {"rule": rule, "what": what, "routing": routing, "to": ["privacy_lead"] + (["ceo"] if ceo else []), "cause": cause}


def entry_alerts(entry: dict, approved, application_user: str, privacy_lead=()) -> list:
    """The alerts one log entry raises: A1, A2, A3, A5, A6 and A7. `approved` is every identity on the approved list (CEO-approved
    PostHog accounts and the ClickHouse users of §7.7); `privacy_lead` the Privacy Lead's own identities."""
    e = validate(entry)
    if e["source"] == "heartbeat":
        return []
    who, out = e["who"], []
    cause = f"{e['source']} {who} {e['object'] or '-'} at {e['at']} (seq {e['seq']})"
    if who not in set(approved):
        out.append(alert("A1", cause, who, privacy_lead))
    if e["source"] == "clickhouse" and who == application_user and e["origin"] != "application":
        out.append(alert("A2", cause, who, privacy_lead))
    if e["outcome"] == "refused":
        out.append(alert("A3", cause, who, privacy_lead))
    if e["kind"] == "access_change":
        out.append(alert("A5", cause, who, privacy_lead))
    if e["kind"] == "single_installation" and not (e["source"] == "clickhouse" and who == ERASE_USER):
        out.append(alert("A6", cause, who, privacy_lead))
    if e["kind"] == "export":
        out.append(alert("A7", cause, who, privacy_lead))
    return out


def gap_alerts(entries, now: datetime) -> list:
    """A8: the newest entry is older than the heartbeat allows, or the sequence skips (an entry lost on the way). There is one
    forwarder, so one sequence."""
    if now.tzinfo is None:
        raise EntryError("a time without a zone is ambiguous")
    entries = [validate(e) for e in entries]
    if not entries:
        return [alert("A8", "no entry at all has arrived")]
    out = []
    newest = max(datetime.strptime(e["at"], "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc) for e in entries)
    silent = (now - newest).total_seconds()
    if silent > HEARTBEAT_SECONDS * HEARTBEAT_MISSED:
        out.append(alert("A8", f"no entry for {int(silent)} s (allowed {HEARTBEAT_SECONDS * HEARTBEAT_MISSED} s)"))
    seqs = sorted(e["seq"] for e in entries)
    missing = [(a, b) for a, b in zip(seqs, seqs[1:]) if b != a + 1]
    if missing:
        out.append(alert("A8", "the sequence skips after " + ", ".join(str(a) for a, _ in missing[:5])))
    return out


def drift_alerts(check_exit_codes: dict, last_run: datetime, now: datetime) -> list:
    """A10: a drift check (apply_project_settings.py --check, apply_event_retention.py --check) that did not exit 0, or checks
    that have not run within the routing period. Exit 2 (could not read) counts as drift: unreadable is never met."""
    out = [alert("A10", f"{name} exited {code}") for name, code in sorted(check_exit_codes.items()) if code != 0]
    if (now - last_run).total_seconds() > A10_MAX_INTERVAL_SECONDS:
        out.append(alert("A10", "the drift checks have not run within the week"))
    return out


def erasure_alerts(failed_before: int, failed_now: int) -> list:
    """A11: the erasure relay's `failed` counter rose (a 502: a withdrawal's deletion is at risk). A counter that went down means
    the relay restarted, which loses the counts in between, so that too is raised rather than read as no failure."""
    if failed_now > failed_before:
        return [alert("A11", f"{failed_now - failed_before} erasure(s) failed")]
    if failed_now < failed_before:
        return [alert("A11", "the relay's counters reset: failures since the last reading are unknown")]
    return []


if __name__ == "__main__":
    # Validate a JSON Lines file of entries: exit 0 if every line is a valid entry, 1 naming the first that is not.
    for number, text in enumerate(open(sys.argv[1], encoding="utf-8") if len(sys.argv) > 1 else sys.stdin, 1):
        try:
            validate(json.loads(text))
        except (EntryError, ValueError) as exc:
            print(f"line {number}: {exc}", file=sys.stderr)
            sys.exit(1)
