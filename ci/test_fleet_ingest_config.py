#!/usr/bin/env python3
"""The SHDR fleet ingest edge keeps no source address and no device token in any log (NP-INFRA-001 §3.3a; privacy review Finding 6).

`infra/fleet/Caddyfile` is the reference edge for `POST /v1/shdr`. This test reads it and fails if any of these no longer holds:

  F1  every `log` block (the named access log and the default/runtime logger) writes through a `filter` format, so no log line is
      unfiltered;
  F2  each of those filters deletes the client's address (request>remote_ip, request>client_ip, request>remote_port), a forwarded
      address (request>headers>X-Forwarded-For) and the device token (request>headers>X-Np-Device-Token);
  F3  the upstream is not handed the client's address (header_up -X-Forwarded-For and -X-Real-IP);
  F4  every log file is rolled by age (`roll_keep_for`), at most MAX_KEEP_HOURS;
  F5  the site address is a parse-time {$VAR} (a runtime {env.VAR} is refused by Caddy as a site address).

`--self-test` proves each check goes red on a copy broken in exactly that way. Run: python3 ci/test_fleet_ingest_config.py [--self-test]

CI-Kind: gate
CI-Self-Test: python3 ci/test_fleet_ingest_config.py --self-test
CI-Self-Test-Reads-Tree: each case copies the real reference Caddyfile and breaks the copy in one way, so the real file is the base the mutants are made from
CI-Scans: the fleet ingest reference Caddyfile, for the log filters, the upstream headers, the log retention and the site address (F1-F5)
CI-Scan-Paths: infra/fleet/Caddyfile
"""
# @verifies-file REQ-SHDR-06
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
CADDYFILE = ROOT / "infra" / "fleet" / "Caddyfile"

MAX_KEEP_HOURS = 168  # one week: a ceiling this test enforces, not a derived figure (OI-FLEET-01, UC-095)
MUST_DELETE = (
    "request>remote_ip", "request>remote_port", "request>client_ip",
    "request>headers>X-Forwarded-For", "request>headers>X-Np-Device-Token",
)


def blocks(text, opener):
    """The bodies of every `opener { ... }` block (braces balanced), outermost match first."""
    out = []
    for m in re.finditer(opener, text, re.M):
        i, depth = m.end(), 1
        while i < len(text) and depth:
            depth += {"{": 1, "}": -1}.get(text[i], 0)
            i += 1
        out.append(text[m.end():i - 1])
    return out


def check(text):
    problems = []
    logs = blocks(text, r"^\s*log(?:\s+default)?\s*\{")
    if len(logs) < 2:
        problems.append(f"F1 expected the access log and the default logger, found {len(logs)} log block(s)")
    for n, body in enumerate(logs, 1):
        if not re.search(r"format\s+filter\s*\{", body):
            problems.append(f"F1 log block {n} does not write through a `filter` format")
            continue
        fields = " ".join(blocks(body, r"fields\s*\{"))
        for f in MUST_DELETE:
            if not re.search(re.escape(f) + r"\s+delete\b", fields):
                problems.append(f"F2 log block {n} does not delete {f}")
        if not blocks(body, r"output\s+file\s+\S+\s*\{") or "roll_keep_for" not in body:
            problems.append(f"F4 log block {n} has no `roll_keep_for` retention")
        for h in re.findall(r"roll_keep_for\s+(\d+)h", body):
            if int(h) > MAX_KEEP_HOURS:
                problems.append(f"F4 log block {n} keeps {h}h, over the {MAX_KEEP_HOURS}h ceiling")
        if re.search(r"roll_keep_for\s+\d+(?!h)\w*", body) and not re.search(r"roll_keep_for\s+\d+h", body):
            problems.append(f"F4 log block {n} states its retention in a unit other than hours")
    for h in ("-X-Forwarded-For", "-X-Real-IP"):
        if not re.search(r"header_up\s+" + re.escape(h) + r"\b", text):
            problems.append(f"F3 the upstream is not stripped of {h[1:]}")
    if "{env." in text.split("\n\n")[-1] or re.search(r"^\{env\.", text, re.M):
        problems.append("F5 a site address uses the runtime {env.VAR} placeholder")
    if not re.search(r"^\{\$FLEET_HOST\}\s*\{", text, re.M):
        problems.append("F5 the site address is not the parse-time {$FLEET_HOST}")
    return problems


def self_test():
    good = CADDYFILE.read_text()
    mutants = {
        "F1": lambda t: t.replace("format filter {\n\t\t\twrap json", "format json {\n\t\t\twrap json", 1),
        "F2 address": lambda t: t.replace("request>remote_ip delete", "request>remote_ipX delete"),
        "F2 token": lambda t: t.replace("request>headers>X-Np-Device-Token delete", "request>headers>X-Other delete"),
        "F3": lambda t: t.replace("header_up -X-Real-IP", "header_up X-Real-IP {remote_host}"),
        "F4 none": lambda t: t.replace("roll_keep_for 72h", "roll_size 10MiB"),
        "F4 long": lambda t: t.replace("roll_keep_for 72h", "roll_keep_for 9999h"),
        "F5": lambda t: t.replace("{$FLEET_HOST} {", "{env.FLEET_HOST} {"),
    }
    failures = []
    if check(good):
        failures.append("the real Caddyfile does not pass: " + "; ".join(check(good)))
    for name, mutate in mutants.items():
        broken = mutate(good)
        if broken == good:
            failures.append(f"mutant {name}: the mutation changed nothing (the test and the file have drifted)")
        elif not check(broken):
            failures.append(f"mutant {name}: the broken file still passes")
    return failures


def main(argv):
    if "--self-test" in argv:
        failures = self_test()
        for f in failures:
            print("FAIL " + f, file=sys.stderr)
        print("SELF-TEST " + ("FAIL" if failures else "PASS — each check goes red on an input broken in its way"))
        return 1 if failures else 0
    problems = check(CADDYFILE.read_text())
    for p in problems:
        print("FAIL " + p, file=sys.stderr)
    print("scanned: 1 file (infra/fleet/Caddyfile)")
    print("fleet ingest: " + (f"{len(problems)} problem(s)" if problems else "F1-F5 hold"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
