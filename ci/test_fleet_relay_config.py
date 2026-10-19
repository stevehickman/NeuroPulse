#!/usr/bin/env python3
"""The Oblivious HTTP relay's reference configuration keeps no log and passes no address on (NP-INFRA-001 §3.3b; REQ-SHDR-07 (g)).

`infra/fleet/relay/Caddyfile` is the reference relay for the fleet upload. The relay is the half of Oblivious HTTP that sees the
phone's address, so the only things that can undo the protection in its own configuration are a log and a forwarded header. This
reads the file and fails if any of these no longer holds:

  R1  no `log` block other than the default logger, and the default logger's output is `discard` (no access log, no runtime log);
  R2  the upstream is handed no X-Forwarded-For, X-Real-IP, Forwarded, Cookie or Authorization (`header_up -NAME`);
  R3  exactly one route is served, `POST /relay` with `Content-Type message/ohttp-req`, and everything else is `respond 404`;
  R4  the request body is capped (`request_body` with `max_size`);
  R5  the site address and the upstream are parse-time {$VAR}s.

It cannot check that the relay is run by someone other than the gateway's operator, which is the other half of the protection.
`--self-test` proves each check goes red on a copy broken in exactly that way. Run: python3 ci/test_fleet_relay_config.py [--self-test]

CI-Kind: gate
CI-Self-Test: python3 ci/test_fleet_relay_config.py --self-test
CI-Self-Test-Reads-Tree: each case copies the real reference Caddyfile and breaks the copy in one way, so the real file is the base the mutants are made from
CI-Scans: the fleet relay reference Caddyfile, for logs, forwarded headers, the one route, the body cap and the site address (R1-R5)
CI-Scan-Paths: infra/fleet/relay/Caddyfile
"""
# @verifies-file REQ-SHDR-07#g
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
CADDYFILE = ROOT / "infra" / "fleet" / "relay" / "Caddyfile"
MUST_STRIP = ("X-Forwarded-For", "X-Real-IP", "Forwarded", "Cookie", "Authorization")


def code_of(text):
    return "\n".join(line.split("#", 1)[0] for line in text.splitlines())


def problems(text):
    code = code_of(text)
    out = []
    logs = re.findall(r"^\s*log\b([^\n{]*)\{", code, re.M)
    if [l.strip() for l in logs] != ["default"]:
        out.append(f"R1 the only log block allowed is `log default`, found {[l.strip() for l in logs]}")
    m = re.search(r"^\s*log\s+default\s*\{(.*?)^\s*\}", code, re.M | re.S)
    if not m or not re.search(r"^\s*output\s+discard\s*$", m.group(1), re.M):
        out.append("R1 the default logger's output is not `discard`")
    for h in MUST_STRIP:
        if not re.search(rf"^\s*header_up\s+-{re.escape(h)}\s*$", code, re.M):
            out.append(f"R2 the upstream is not stripped of {h} (header_up -{h})")
        if re.search(rf"^\s*header_up\s+{re.escape(h)}\s+\S", code, re.M):
            out.append(f"R2 the Caddyfile sets {h} to a value")
    r = re.search(r"@relay\s*\{(.*?)\n\s*\}", code, re.S)
    body = r.group(1) if r else ""
    if not (re.search(r"^\s*method\s+POST\s*$", body, re.M) and re.search(r"^\s*path\s+/relay\s*$", body, re.M)
            and re.search(r"^\s*header\s+Content-Type\s+message/ohttp-req\s*$", body, re.M)):
        out.append("R3 the relay route is not exactly POST /relay with Content-Type message/ohttp-req")
    if not re.search(r"^\s*handle\s*\{\s*respond\s+404\s*\}", code, re.M | re.S):
        out.append("R3 there is no catch-all `respond 404`")
    if len(re.findall(r"^\s*handle\b", code, re.M)) != 2:
        out.append("R3 more or fewer than the relay route and the 404 are served")
    if not re.search(r"request_body\s*\{[^}]*max_size\s+\S+", code, re.S):
        out.append("R4 the request body is not capped")
    if not re.search(r"^\{\$RELAY_HOST\}\s*\{", code, re.M):
        out.append("R5 the site address is not the parse-time {$RELAY_HOST}")
    if not re.search(r"reverse_proxy\s+\{\$GATEWAY_UPSTREAM\}", code):
        out.append("R5 the upstream is not the parse-time {$GATEWAY_UPSTREAM}")
    return out


def self_test(text):
    failures = []

    def expect(label, broken, tag):
        got = problems(broken)
        if not any(p.startswith(tag) for p in got):
            failures.append(f"self-test: '{label}' did not raise {tag} (got {got})")

    if problems(text):
        failures.append(f"self-test: the real Caddyfile should be clean but raised {problems(text)}")
    expect("an access log", text + '\n{$RELAY_HOST} {\n\tlog {\n\t\toutput stdout\n\t}\n}\n', "R1")
    expect("default log to a file", text.replace("output discard", "output file /var/log/caddy/r.log"), "R1")
    for h in MUST_STRIP:
        expect(f"{h} not stripped", re.sub(rf"^\s*header_up -{h}\n", "", text, flags=re.M), "R2")
    expect("forwards the address", text.replace("header_up -X-Forwarded-For", "header_up X-Forwarded-For {remote_host}"), "R2")
    expect("any method", text.replace("method POST", "method GET"), "R3")
    expect("any path", text.replace("path /relay", "path /*"), "R3")
    expect("any content type", re.sub(r"\n\s*header Content-Type message/ohttp-req", "", text), "R3")
    expect("no 404", text.replace("respond 404", "respond 200"), "R3")
    expect("no body cap", re.sub(r"request_body \{\s*max_size 1MB\s*\}", "", text), "R4")
    expect("runtime site address", text.replace("{$RELAY_HOST} {", "{env.RELAY_HOST} {"), "R5")
    expect("literal upstream", text.replace("{$GATEWAY_UPSTREAM}", "https://gw.example"), "R5")
    for f in failures:
        print("FAIL " + f, file=sys.stderr)
    print("SELF-TEST " + ("FAIL" if failures else "PASS — each check goes red on an input broken in its way"))
    return 1 if failures else 0


if __name__ == "__main__":
    text = CADDYFILE.read_text()
    if "--self-test" in sys.argv[1:]:
        sys.exit(self_test(text))
    found = problems(text)
    for p in found:
        print("FAIL " + p, file=sys.stderr)
    print("scanned: 1 file (infra/fleet/relay/Caddyfile)")
    print("fleet relay: " + (f"{len(found)} problem(s)" if found else "R1-R5 hold"))
    sys.exit(1 if found else 0)
