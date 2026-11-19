#!/usr/bin/env python3
"""The PostHog stack keeps its privacy configuration (NP-ANALYTICS-001, NP-APP-TELEMETRY-001 §4; privacy review Finding 4).

infra/posthog had no CI at all, and its compose file did not even parse (a `: ` inside an unquoted error message). This test
renders the stack with `docker compose config` and reads the Caddyfile and .env.example, and fails if any of these no longer holds:

  P1  the compose file parses, and refuses to start without a pinned PostHog version (POSTHOG_VERSION);
  P2  every PostHog image carries a tag, and the tag is not `latest`;
  P3  PostHog's own privacy switches are on: no GeoIP database, no object storage (so no session recording), no telemetry
      to posthog.com, no Sentry;
  P4  PostHog's port is published on the loopback address only, and no service but the TLS proxy publishes any port;
  P5  the data services (Postgres, Redis, ZooKeeper, ClickHouse, Kafka) sit on the internal network only, and that network is internal;
  P6  the Caddyfile does not hand the client's address to PostHog (it removes X-Forwarded-For and X-Real-IP) and sends HSTS
      for every host except localhost;
  P7  .env.example does not pin `latest`;
  P8  no Caddy log keeps the client's address (OI-PRIV-05 (5), the interim floor): the access log and the default logger both
      exist and both write through a `filter` format that deletes request>remote_ip, request>remote_port, request>client_ip and
      the X-Forwarded-For and X-Real-Ip request headers;
  P9  the Caddyfile is one Caddy accepts: `caddy adapt`, run in the image the compose file names. A filter that Caddy cannot
      parse protects nothing, and P6 and P8 read the text only.

`--self-test` proves each check goes red on a copy broken in exactly that way. Without Docker the compose checks are reported as
SKIPPED, and without a Docker daemon P9 is, and the run still fails if --require-docker is given (CI gives it), so a missing
Docker never reads as a pass.

Run: python3 ci/test_posthog_infra.py [--self-test] [--require-docker]

CI-Kind: gate
CI-Self-Test: python3 ci/test_posthog_infra.py --self-test --require-docker
CI-Scans: the rendered PostHog compose file, its Caddyfile and .env.example, for the privacy configuration P1-P9
CI-Scan-Paths: infra/posthog/docker-compose.yml infra/posthog/caddy/Caddyfile infra/posthog/.env.example
CI-Self-Test-Reads-Tree: each case copies the real compose file, Caddyfile and .env.example and breaks the copy in one way, so the real files are the base the mutants are made from
"""
import copy
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
INFRA = ROOT / "infra" / "posthog"
ENV = {"SECRET_KEY": "x", "POSTGRES_PASSWORD": "x", "CLICKHOUSE_PASSWORD": "x", "ERASE_CLICKHOUSE_PASSWORD": "x", "POSTHOG_VERSION": "release-test"}
POSTHOG_SWITCHES = {"DISABLE_MMDB": "true", "OBJECT_STORAGE_ENABLED": "false", "CAPTURE_INTERNAL_METRICS": "false", "SENTRY_DSN": ""}
DATA_SERVICES = ("db", "redis", "zookeeper", "clickhouse", "kafka")
PROXY = "caddy"


def compose(extra_env=None, drop=()):
    """`docker compose config`'s answer as (exit code, JSON or None, stderr). `drop` removes variables from the environment."""
    env = {k: v for k, v in os.environ.items() if k not in ENV and k not in drop}
    env.update({k: v for k, v in ENV.items() if k not in drop})
    env.update(extra_env or {})
    p = subprocess.run(["docker", "compose", "--profile", "https", "config", "--format", "json"], cwd=INFRA, env=env,
                       capture_output=True, text=True)
    return p.returncode, (json.loads(p.stdout) if p.returncode == 0 else None), p.stderr


def compose_problems(c):
    out = []
    services = c["services"]
    for name, s in services.items():
        img = s.get("image", "")
        if img.startswith("posthog/posthog"):
            tag = img.partition(":")[2]
            if not tag or tag == "latest":
                out.append(f"P2 {name}: image '{img}' is not pinned to a release tag")
    ph = services.get("posthog", {}).get("environment", {})
    for k, want in POSTHOG_SWITCHES.items():
        if ph.get(k) != want:
            out.append(f"P3 posthog: {k} is {ph.get(k)!r}, required {want!r}")
    for name, s in services.items():
        for port in s.get("ports") or []:
            if name == "posthog":
                if port.get("host_ip") != "127.0.0.1":
                    out.append(f"P4 posthog: port {port.get('published')} is published on {port.get('host_ip') or 'every interface'}, not 127.0.0.1")
            elif name != PROXY:
                out.append(f"P4 {name}: publishes port {port.get('published')}; only the TLS proxy may")
    if not c.get("networks", {}).get("analytics-internal", {}).get("internal"):
        out.append("P5 network analytics-internal is not internal")
    for name in DATA_SERVICES:
        nets = services.get(name, {}).get("networks")
        nets = list(nets) if nets else []
        if nets != ["analytics-internal"]:
            out.append(f"P5 {name}: networks {nets}, required only analytics-internal")
    return out


def caddy_problems(text):
    out = []
    code = "\n".join(line.split("#", 1)[0] for line in text.splitlines())  # comments do not count
    for h in ("X-Forwarded-For", "X-Real-IP"):
        if not re.search(rf"^\s*header_up\s+-{h}\s*$", code, re.M):
            out.append(f"P6 Caddyfile does not remove {h} (header_up -{h})")
        if re.search(rf"^\s*header_up\s+{h}\s+\S", code, re.M):
            out.append(f"P6 Caddyfile sets {h} to a value, which hands PostHog the client's address")
    if not re.search(r"^\s*@(\w+)\s+not\s+host\s+localhost\b", code, re.M):
        out.append("P6 Caddyfile has no matcher for 'every host except localhost'")
    elif not re.search(r"^\s*header\s+@\w+\s+Strict-Transport-Security\s+\"max-age=\d+", code, re.M):
        out.append("P6 Caddyfile does not send Strict-Transport-Security for non-local hosts")
    return out


MUST_DELETE = (
    "request>remote_ip", "request>remote_port", "request>client_ip",
    "request>headers>X-Forwarded-For", "request>headers>X-Real-Ip",
)


def blocks(text, opener):
    """The bodies of every `opener { ... }` block (braces balanced)."""
    out = []
    for m in re.finditer(opener, text, re.M):
        i, depth = m.end(), 1
        while i < len(text) and depth:
            depth += {"{": 1, "}": -1}.get(text[i], 0)
            i += 1
        out.append(text[m.end():i - 1])
    return out


def caddy_log_problems(text):
    out = []
    code = "\n".join(line.split("#", 1)[0] for line in text.splitlines())  # comments do not count
    default, access = blocks(code, r"^\s*log\s+default\s*\{"), blocks(code, r"^\s*log\s*\{")
    if not default:
        out.append("P8 Caddyfile has no `log default` block: error entries carry the client's address and go unfiltered")
    if not access:
        out.append("P8 Caddyfile has no access `log` block to filter")
    for n, body in enumerate(default + access, 1):
        if not re.search(r"format\s+filter\s*\{", body):
            out.append(f"P8 log block {n} does not write through a `filter` format")
            continue
        fields = " ".join(blocks(body, r"fields\s*\{"))
        for f in MUST_DELETE:
            if not re.search(re.escape(f) + r"\s+delete\b", fields):
                out.append(f"P8 log block {n} does not delete {f}")
    return out


def caddy_image():
    m = re.search(r"^\s*image:\s*(caddy:\S+)\s*$", (INFRA / "docker-compose.yml").read_text(), re.M)
    return m.group(1) if m else None


def docker_daemon():
    """True if a Docker daemon answers. `docker compose config` needs only the CLI; running the Caddy image needs the daemon."""
    return shutil.which("docker") is not None and subprocess.run(["docker", "info"], capture_output=True).returncode == 0


def caddy_adapt_problems(text):
    """P9: `caddy adapt` in the compose file's own Caddy image. Needs Docker."""
    image = caddy_image()
    if image is None:
        return ["P9 the compose file names no caddy image to check the Caddyfile with"]
    p = subprocess.run(["docker", "run", "--rm", "-i", "-e", "POSTHOG_HOST=localhost", image,
                        "caddy", "adapt", "--adapter", "caddyfile", "--config", "/dev/stdin"],
                       input=text, capture_output=True, text=True)
    if p.returncode != 0:
        last = (p.stderr.strip().splitlines() or [str(p.returncode)])[-1]
        return [f"P9 Caddy does not accept the Caddyfile: {last}"]
    return []


def env_problems(text):
    m = re.search(r"^POSTHOG_VERSION=(.*)$", text, re.M)
    if not m:
        return ["P7 .env.example has no POSTHOG_VERSION line"]
    if m.group(1).strip().strip("'\"").lower() == "latest":
        return ["P7 .env.example pins POSTHOG_VERSION to latest"]
    return []


def read():
    return (INFRA / "caddy" / "Caddyfile").read_text(), (INFRA / ".env.example").read_text()


def check(require_docker):
    problems = []
    caddy, env = read()
    problems += caddy_problems(caddy) + caddy_log_problems(caddy) + env_problems(env)
    if shutil.which("docker") is None:
        msg = "compose checks P1-P5 and the Caddy check P9 SKIPPED: docker is not installed"
        if require_docker:
            problems.append(msg + " (and --require-docker was given)")
        else:
            print(msg)
    else:
        code, cfg, err = compose()
        if code != 0:
            problems.append(f"P1 the compose file does not render: {err.strip().splitlines()[-1] if err.strip() else code}")
        else:
            problems += compose_problems(cfg)
        if docker_daemon():
            problems += caddy_adapt_problems(caddy)
        elif require_docker:
            problems.append("Caddy check P9 SKIPPED: no Docker daemon answers (and --require-docker was given)")
        else:
            print("Caddy check P9 SKIPPED: no Docker daemon answers")
        # P1: it must refuse to start without a pinned version, and without each secret.
        for var in ("POSTHOG_VERSION", "SECRET_KEY", "POSTGRES_PASSWORD", "CLICKHOUSE_PASSWORD"):
            if compose(drop=(var,))[0] == 0:
                problems.append(f"P1 the compose file renders without {var}: it should refuse to start")
    return problems


def self_test(require_docker):
    """Each check must go red on an input broken in exactly its way, and stay green on the real one."""
    caddy, env = read()
    failures = []

    def expect(label, got, tag):
        if not any(p.startswith(tag) for p in got):
            failures.append(f"self-test: '{label}' did not raise {tag} (got {got})")

    expect("Caddy sets X-Forwarded-For", caddy_problems(caddy.replace("header_up -X-Forwarded-For", "header_up X-Forwarded-For {remote_host}")), "P6")
    expect("Caddy leaves X-Real-IP alone", caddy_problems(caddy.replace("header_up -X-Real-IP\n", "")), "P6")
    expect("Caddy forwards X-Real-IP", caddy_problems(caddy + "\n    header_up X-Real-IP {remote_host}\n"), "P6")
    expect("a removal only in a comment", caddy_problems(caddy.replace("    header_up -X-Forwarded-For\n", "    # header_up -X-Forwarded-For\n")), "P6")
    expect("no HSTS", caddy_problems(re.sub(r"^\s*header @public Strict-Transport-Security.*$", "", caddy, flags=re.M)), "P6")
    expect("no localhost matcher", caddy_problems(caddy.replace("@public not host localhost 127.0.0.1", "@public host example.org")), "P6")
    expect("no default logger", caddy_log_problems(re.sub(r"log default \{", "log other {", caddy)), "P8")
    expect("an access log with no filter", caddy_log_problems(re.sub(r"(  log \{\n    output stdout\n    level WARN\n)    format filter \{.*?\n    \}\n", r"\1", caddy, flags=re.S)), "P8")
    for f in MUST_DELETE:
        expect(f"a log keeps {f}", caddy_log_problems(caddy.replace(f + " delete", f + "X delete", 1)), "P8")
        expect(f"the second log keeps {f}", caddy_log_problems((f + "X delete").join(caddy.rsplit(f + " delete", 1))), "P8")
    expect("a filter only in a comment", caddy_log_problems(caddy.replace("request>remote_ip delete", "# request>remote_ip delete")), "P8")
    expect(".env.example pins latest", env_problems(re.sub(r"^POSTHOG_VERSION=.*$", "POSTHOG_VERSION=latest", env, flags=re.M)), "P7")
    expect(".env.example lacks the line", env_problems(re.sub(r"^POSTHOG_VERSION=.*$", "", env, flags=re.M)), "P7")
    for label, got in (("the real Caddyfile", caddy_problems(caddy) + caddy_log_problems(caddy)), ("the real .env.example", env_problems(env))):
        if got:
            failures.append(f"self-test: {label} should be clean but raised {got}")

    if shutil.which("docker") is None:
        if require_docker:
            failures.append("self-test: docker is not installed (and --require-docker was given)")
        else:
            print("self-test of the compose checks SKIPPED: docker is not installed")
    else:
        if docker_daemon():
            if caddy_adapt_problems(caddy):
                failures.append(f"self-test: the real Caddyfile should be accepted but raised {caddy_adapt_problems(caddy)}")
            expect("a Caddyfile with an unclosed block", caddy_adapt_problems(caddy.rstrip().rstrip("}")), "P9")
            expect("a log filter Caddy does not know", caddy_adapt_problems(caddy.replace("format filter {", "format no_such_encoder {")), "P9")
        elif require_docker:
            failures.append("self-test: no Docker daemon answers (and --require-docker was given), so P9 was not exercised")
        else:
            print("self-test of the Caddy check P9 SKIPPED: no Docker daemon answers")
        code, cfg, err = compose()
        if code != 0:
            failures.append(f"self-test: the real compose file does not render: {err.strip()}")
        else:
            if compose_problems(cfg):
                failures.append(f"self-test: the real compose file should be clean but raised {compose_problems(cfg)}")
            def mutate(label, tag, fn):
                c = copy.deepcopy(cfg)
                fn(c)
                expect(label, compose_problems(c), tag)
            mutate("latest image", "P2", lambda c: c["services"]["posthog"].update(image="posthog/posthog:latest"))
            mutate("untagged image", "P2", lambda c: c["services"]["posthog-worker"].update(image="posthog/posthog"))
            mutate("GeoIP on", "P3", lambda c: c["services"]["posthog"]["environment"].update(DISABLE_MMDB="false"))
            mutate("object storage on", "P3", lambda c: c["services"]["posthog"]["environment"].update(OBJECT_STORAGE_ENABLED="true"))
            mutate("telemetry on", "P3", lambda c: c["services"]["posthog"]["environment"].update(CAPTURE_INTERNAL_METRICS="true"))
            mutate("a Sentry DSN", "P3", lambda c: c["services"]["posthog"]["environment"].update(SENTRY_DSN="https://x@sentry.io/1"))
            mutate("PostHog on every interface", "P4", lambda c: c["services"]["posthog"]["ports"][0].pop("host_ip"))
            mutate("ClickHouse published", "P4", lambda c: c["services"]["clickhouse"].update(ports=[{"published": "8123", "target": 8123}]))
            mutate("network not internal", "P5", lambda c: c["networks"]["analytics-internal"].update(internal=False))
            mutate("Kafka on the external network", "P5", lambda c: c["services"]["kafka"].update(networks={"analytics-internal": None, "analytics-external": None}))
    return failures


def main(argv):
    require_docker = "--require-docker" in argv
    if "--self-test" in argv:
        failures = self_test(require_docker)
        for f in failures:
            print("FAIL " + f, file=sys.stderr)
        print("SELF-TEST " + ("FAIL" if failures else "PASS — each check goes red on an input broken in its way"))
        return 1 if failures else 0
    problems = check(require_docker)
    for p in problems:
        print("FAIL " + p, file=sys.stderr)
    print("scanned: 3 files (docker-compose.yml, Caddyfile, .env.example)")
    print("posthog infra: " + (f"{len(problems)} problem(s)" if problems else "P1-P9 hold"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
