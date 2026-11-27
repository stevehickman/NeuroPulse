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
  P6  no reverse_proxy in the Caddyfile hands its upstream (PostHog, the erasure relay) a header that can carry the client's
      address (CLIENT_ADDRESS_HEADERS, each as `header_up -NAME`, none set to a value; OI-PRIV-51), and the Caddyfile sends HSTS
      for every host except localhost;
  P7  .env.example does not pin `latest`;
  P8  no Caddy log keeps the client's address (OI-PRIV-05 (5), the interim floor): the access log and the default logger both
      exist and both write through a `filter` format that deletes request>remote_ip, request>remote_port, request>client_ip and
      the whole request-header map, request>headers (OI-PRIV-51: deleting named headers leaves any header nobody listed);
  P9  the Caddyfile is one Caddy accepts: `caddy adapt`, run in the image the compose file names. A filter that Caddy cannot
      parse protects nothing, and P6 and P8 read the text only.
  P10 every service's container log is bounded (OI-PRIV-57, OI-PRIV-46 (1)): its `logging:` driver is `local` or
      `json-file` with `max-size` and `max-file` no larger than the ceilings below, or `none`. Docker's default keeps a
      container's output with no limit, and a driver that ships logs off the host (syslog, fluentd, a cloud service)
      moves them where no bound here reaches. A size bound is not an age bound: Docker's drivers have no age option.
  P11 the erasure relay's ClickHouse user (clickhouse/np-erase-user.xml) is admitted from the relay's container only
      (OI-PRIV-50): its <networks> holds <host> entries and nothing else, each the compose name of the service that runs
      erase_relay.py; that service shares a network with the ClickHouse service that mounts the file into users.d; and no
      other service claims the name as a container_name or network alias. An <ip> range (the old ::/0) or a <host_regexp>
      would admit any container on the internal network that has the password. The relay and ClickHouse are found by what
      they run and mount, not by fixed service names, so the check carries into the rebuilt stack (OI-ANALYTICS-03).
  P12 .env.example names no plain-http URL to a host other than this machine (OI-PRIV-50): PostHog's port is published on
      127.0.0.1 only (P4), so `http://<LAN address>:8000` cannot work, and suggesting it invites rebinding PostHog to the
      LAN over plain HTTP. Another device goes through the https profile.

`--self-test` proves each check goes red on a copy broken in exactly that way. Without Docker the compose checks are reported as
SKIPPED (P11 among them: it reads the rendered file), and without a Docker daemon P9 is, and the run still fails if --require-docker is given (CI gives it), so a missing
Docker never reads as a pass.

Run: python3 ci/test_posthog_infra.py [--self-test] [--require-docker]

CI-Kind: gate
CI-Self-Test: python3 ci/test_posthog_infra.py --self-test --require-docker
CI-Scans: the rendered PostHog compose file, its Caddyfile, .env.example and the erasure relay's ClickHouse user file, for the privacy configuration P1-P12
CI-Scan-Paths: infra/posthog/docker-compose.yml infra/posthog/caddy/Caddyfile infra/posthog/.env.example infra/posthog/clickhouse/np-erase-user.xml
CI-Self-Test-Reads-Tree: each case copies the real compose file, Caddyfile, .env.example and ClickHouse user file and breaks the copy in one way, so the real files are the base the mutants are made from
"""
import copy
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import urllib.parse
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parent.parent
INFRA = ROOT / "infra" / "posthog"
ENV = {"SECRET_KEY": "x", "POSTGRES_PASSWORD": "x", "CLICKHOUSE_PASSWORD": "x", "ERASE_CLICKHOUSE_PASSWORD": "x", "POSTHOG_VERSION": "release-test"}
POSTHOG_SWITCHES = {"DISABLE_MMDB": "true", "OBJECT_STORAGE_ENABLED": "false", "CAPTURE_INTERNAL_METRICS": "false", "SENTRY_DSN": ""}
DATA_SERVICES = ("db", "redis", "zookeeper", "clickhouse", "kafka")
PROXY = "caddy"
# P11: the erasure relay's ClickHouse user, the file that defines it, and what identifies the relay's service and ClickHouse's.
ERASE_USER = "np_erase"
ERASE_USER_FILE = INFRA / "clickhouse" / "np-erase-user.xml"
ERASE_RELAY_SCRIPT = "erase_relay.py"
USERS_D = "/etc/clickhouse-server/users.d/"
# P12: the hosts a plain-http URL may name: this machine.
LOOPBACK_HOSTS = ("localhost", "127.0.0.1", "::1")
# P10 ceilings, per container: the figures the compose file uses, with no derivation (UC-211). Lowering one is free;
# raising one is a register change.
LOG_MAX_SIZE_BYTES = 10 * 1024 * 1024
LOG_MAX_FILES = 3
LOG_DRIVERS_BOUNDED = ("local", "json-file")
# The request headers a proxy can carry the client's address in. Caddy forwards every header it does not set itself, so each is
# removed by name before the upstream; the logs delete the whole header map instead (OI-PRIV-51; the same list as
# ci/test_fleet_ingest_config.py).
CLIENT_ADDRESS_HEADERS = (
    "X-Forwarded-For", "X-Real-IP", "Forwarded", "True-Client-IP", "CF-Connecting-IP", "X-Client-IP",
    "X-Cluster-Client-IP", "Fastly-Client-IP", "Via",
)


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


def log_size_bytes(value):
    """Docker's size notation (`10m`, `512k`, `1g`, plain bytes) as bytes, or None when it is not one."""
    import re as _re
    m = _re.fullmatch(r"\s*(\d+)\s*([kmg]?)b?\s*", str(value), _re.I)
    if not m:
        return None
    return int(m.group(1)) * {"": 1, "k": 1024, "m": 1024 ** 2, "g": 1024 ** 3}[m.group(2).lower()]


def logging_problems(c):
    """P10 on the rendered compose file."""
    out = []
    for name, s in sorted(c["services"].items()):
        lg = s.get("logging")
        if not lg or not lg.get("driver"):
            out.append(f"P10 {name}: no `logging:` driver, so Docker's default keeps its output with no limit")
            continue
        driver = lg["driver"]
        if driver == "none":
            continue
        if driver not in LOG_DRIVERS_BOUNDED:
            out.append(f"P10 {name}: logging driver '{driver}' is not one this check can bound (local, json-file or none)")
            continue
        opts = lg.get("options") or {}
        size = log_size_bytes(opts.get("max-size", ""))
        if size is None or size <= 0:
            out.append(f"P10 {name}: no usable max-size ({opts.get('max-size')!r})")
        elif size > LOG_MAX_SIZE_BYTES:
            out.append(f"P10 {name}: max-size {opts['max-size']} is over the {LOG_MAX_SIZE_BYTES // 1024 ** 2}m ceiling (UC-211)")
        files = str(opts.get("max-file", "")).strip()
        if not files.isdigit() or int(files) < 1:
            out.append(f"P10 {name}: no usable max-file ({opts.get('max-file')!r})")
        elif int(files) > LOG_MAX_FILES:
            out.append(f"P10 {name}: max-file {files} is over the ceiling of {LOG_MAX_FILES} (UC-211)")
    return out


def caddy_problems(text):
    out = []
    code = "\n".join(line.split("#", 1)[0] for line in text.splitlines())  # comments do not count
    proxies = blocks(code, r"reverse_proxy\s+\S+\s*\{")
    if not proxies:
        out.append("P6 Caddyfile has no reverse_proxy block to check")
    for n, body in enumerate(proxies, 1):
        for h in CLIENT_ADDRESS_HEADERS:
            if not re.search(rf"^\s*header_up\s+-{re.escape(h)}\s*$", body, re.M | re.I):
                out.append(f"P6 reverse_proxy {n} does not remove {h} (header_up -{h})")
            if re.search(rf"^\s*header_up\s+{re.escape(h)}\s+\S", body, re.M | re.I):
                out.append(f"P6 reverse_proxy {n} sets {h} to a value, which hands the upstream the client's address")
    if not re.search(r"^\s*@(\w+)\s+not\s+host\s+localhost\b", code, re.M):
        out.append("P6 Caddyfile has no matcher for 'every host except localhost'")
    elif not re.search(r"^\s*header\s+@\w+\s+Strict-Transport-Security\s+\"max-age=\d+", code, re.M):
        out.append("P6 Caddyfile does not send Strict-Transport-Security for non-local hosts")
    return out


MUST_DELETE = ("request>remote_ip", "request>remote_port", "request>client_ip", "request>headers")


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


def erase_user_problems(c, xml_text):
    """P11 on the rendered compose file and the ClickHouse user file."""
    out = []
    try:
        root = ET.fromstring(xml_text)
    except ET.ParseError as e:
        return [f"P11 {ERASE_USER_FILE.name} does not parse: {e}"]
    user = root.find(f"users/{ERASE_USER}")
    if user is None:
        return [f"P11 {ERASE_USER_FILE.name} defines no {ERASE_USER} user"]
    nets = user.find("networks")
    if nets is None or len(nets) == 0:
        # ClickHouse's default for a user with no <networks> is to admit it from anywhere.
        return [f"P11 {ERASE_USER}: no <networks>, so ClickHouse admits it from any address"]
    services = c["services"]
    relays = [n for n, s in services.items() if any(ERASE_RELAY_SCRIPT in str(a) for a in (s.get("command") or []))]
    if len(relays) != 1:
        return [f"P11 the compose file has {len(relays)} services that run {ERASE_RELAY_SCRIPT} ({relays}); required exactly one"]
    relay = relays[0]
    hosts = []
    for e in nets:
        if e.tag != "host":
            out.append(f"P11 {ERASE_USER}: <{e.tag}>{(e.text or '').strip()}</{e.tag}> admits by address or pattern; only <host>{relay}</host> may")
            continue
        hosts.append((e.text or "").strip())
        if hosts[-1] != relay:
            out.append(f"P11 {ERASE_USER}: <host>{hosts[-1]}</host> is not the erasure relay's service name ({relay})")
    if relay not in hosts:
        return out
    houses = [n for n, s in services.items()
              if any(str(v.get("source", "")).endswith(ERASE_USER_FILE.name) and str(v.get("target", "")).startswith(USERS_D)
                     for v in (s.get("volumes") or []) if isinstance(v, dict))]
    if len(houses) != 1:
        out.append(f"P11 {len(houses)} services mount {ERASE_USER_FILE.name} into {USERS_D} ({houses}); required exactly one, ClickHouse")
    else:
        shared = set(services[houses[0]].get("networks") or {}) & set(services[relay].get("networks") or {})
        if not shared:
            out.append(f"P11 {relay} shares no network with {houses[0]}, so ClickHouse cannot resolve its name and the relay cannot connect")
    for n, s in services.items():
        if n == relay:
            continue
        if s.get("container_name") == relay:
            out.append(f"P11 {n}: container_name {relay} lets it answer to the relay's name and connect as {ERASE_USER}")
        for net, cfg in (s.get("networks") or {}).items():
            if relay in ((cfg or {}).get("aliases") or []):
                out.append(f"P11 {n}: network alias {relay} on {net} lets it answer to the relay's name and connect as {ERASE_USER}")
    return out


def env_url_problems(text):
    """P12 on .env.example: every plain-http URL, comments included, names this machine."""
    out = []
    for m in re.finditer(r"http://[^\s'\"<>]+", text):
        host = urllib.parse.urlsplit(m.group(0)).hostname or ""
        if host not in LOOPBACK_HOSTS:
            out.append(f"P12 .env.example suggests {m.group(0)}: PostHog is published on 127.0.0.1 only, so another device goes through the https profile")
    return out


def env_problems(text):
    m = re.search(r"^POSTHOG_VERSION=(.*)$", text, re.M)
    if not m:
        return ["P7 .env.example has no POSTHOG_VERSION line"]
    if m.group(1).strip().strip("'\"").lower() == "latest":
        return ["P7 .env.example pins POSTHOG_VERSION to latest"]
    return []


def read():
    return (INFRA / "caddy" / "Caddyfile").read_text(), (INFRA / ".env.example").read_text()


def read_erase_user():
    return ERASE_USER_FILE.read_text()


def check(require_docker):
    problems = []
    caddy, env = read()
    problems += caddy_problems(caddy) + caddy_log_problems(caddy) + env_problems(env) + env_url_problems(env)
    if shutil.which("docker") is None:
        msg = "compose checks P1-P5, P10, P11 and the Caddy check P9 SKIPPED: docker is not installed"
        if require_docker:
            problems.append(msg + " (and --require-docker was given)")
        else:
            print(msg)
    else:
        code, cfg, err = compose()
        if code != 0:
            problems.append(f"P1 the compose file does not render: {err.strip().splitlines()[-1] if err.strip() else code}")
        else:
            problems += compose_problems(cfg) + logging_problems(cfg) + erase_user_problems(cfg, read_erase_user())
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
    expect("Caddy forwards X-Real-IP", caddy_problems(caddy.replace("header_up -X-Real-IP\n", "header_up -X-Real-IP\n      header_up X-Real-IP {remote_host}\n", 1)), "P6")
    expect("PostHog gets Forwarded", caddy_problems(re.sub(r"(reverse_proxy posthog:8000 \{.*?)\n\s*header_up -Forwarded\n", r"\1\n", caddy, count=1, flags=re.S)), "P6")
    expect("the erasure relay gets Via", caddy_problems(re.sub(r"(reverse_proxy erase-relay:8080 \{.*?)\n\s*header_up -Via\n", r"\1\n", caddy, count=1, flags=re.S)), "P6")
    expect("a log keeps headers by name only", caddy_log_problems(caddy.replace("request>headers delete", "request>headers>X-Forwarded-For delete", 1)), "P8")
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
    expect(".env.example suggests a LAN address over http", env_url_problems(env + "#   Physical device (local net): http://192.168.x.x:8000\n"), "P12")
    expect(".env.example sets SITE_URL to a LAN address over http", env_url_problems(re.sub(r"^SITE_URL=.*$", "SITE_URL=http://10.0.0.5:8000", env, flags=re.M)), "P12")
    for label, got in (("the real Caddyfile", caddy_problems(caddy) + caddy_log_problems(caddy)), ("the real .env.example", env_problems(env) + env_url_problems(env))):
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
            # (A missing final brace is not a usable mutant: Caddy 2.8 adapts a file whose last block is left open.)
            expect("a directive Caddy does not know", caddy_adapt_problems(caddy.replace("encode gzip", "no_such_directive gzip")), "P9")
            expect("a log filter Caddy does not know", caddy_adapt_problems(caddy.replace("format filter {", "format no_such_encoder {")), "P9")
        elif require_docker:
            failures.append("self-test: no Docker daemon answers (and --require-docker was given), so P9 was not exercised")
        else:
            print("self-test of the Caddy check P9 SKIPPED: no Docker daemon answers")
        code, cfg, err = compose()
        if code != 0:
            failures.append(f"self-test: the real compose file does not render: {err.strip()}")
        else:
            if compose_problems(cfg) + logging_problems(cfg):
                failures.append(f"self-test: the real compose file should be clean but raised {compose_problems(cfg) + logging_problems(cfg)}")
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

            def mutate_log(label, fn):
                c = copy.deepcopy(cfg)
                fn(c)
                expect(label, logging_problems(c), "P10")
            mutate_log("caddy logs with Docker's default", lambda c: c["services"]["caddy"].pop("logging"))
            mutate_log("the erasure relay ships logs to syslog", lambda c: c["services"]["erase-relay"].update(logging={"driver": "syslog"}))
            mutate_log("PostHog keeps a larger file", lambda c: c["services"]["posthog"]["logging"]["options"].update({"max-size": "500m"}))
            mutate_log("ClickHouse keeps more files", lambda c: c["services"]["clickhouse"]["logging"]["options"].update({"max-file": "50"}))
            mutate_log("Kafka has no max-file", lambda c: c["services"]["kafka"]["logging"]["options"].pop("max-file"))

            xml = read_erase_user()
            if erase_user_problems(cfg, xml):
                failures.append(f"self-test: the real erasure user and compose file should be clean but raised {erase_user_problems(cfg, xml)}")
            host = "<host>erase-relay</host>"
            for label, broken in (("np_erase from ::/0", xml.replace(host, "<ip>::/0</ip>")),
                                  ("np_erase from the internal subnet", xml.replace(host, "<ip>172.16.0.0/12</ip>")),
                                  ("np_erase from the relay and a range", xml.replace(host, host + "<ip>::/0</ip>")),
                                  ("np_erase by host_regexp", xml.replace(host, "<host_regexp>.*</host_regexp>")),
                                  ("np_erase from another service", xml.replace(host, "<host>posthog</host>")),
                                  ("np_erase with no networks", re.sub(r"<networks>.*?</networks>", "", xml, flags=re.S)),
                                  ("np_erase renamed away", xml.replace("<np_erase>", "<np_other>").replace("</np_erase>", "</np_other>"))):
                expect(label, erase_user_problems(cfg, broken), "P11")

            def mutate_erase(label, fn):
                c = copy.deepcopy(cfg)
                fn(c)
                expect(label, erase_user_problems(c, xml), "P11")
            def rename_relay(c):
                c["services"]["np-relay"] = c["services"].pop("erase-relay")
            mutate_erase("the relay renamed without the user file", rename_relay)
            mutate_erase("the relay off ClickHouse's network", lambda c: c["services"]["erase-relay"].update(networks={"analytics-external": None}))
            mutate_erase("PostHog aliased as the relay", lambda c: c["services"]["posthog"].update(networks={"analytics-internal": {"aliases": ["erase-relay"]}, "analytics-external": None}))
            mutate_erase("Kafka's container named as the relay", lambda c: c["services"]["kafka"].update(container_name="erase-relay"))
            mutate_erase("a second service runs the relay", lambda c: c["services"].update({"erase-relay-2": copy.deepcopy(c["services"]["erase-relay"])}))
            mutate_erase("ClickHouse does not mount the user file", lambda c: c["services"]["clickhouse"].update(volumes=[v for v in c["services"]["clickhouse"]["volumes"] if "np-erase-user" not in str(v.get("source"))]))
            if logging_problems({"services": {"x": {"logging": {"driver": "none"}}}}):
                failures.append("self-test: P10 refused the `none` driver, which keeps nothing")
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
    print("scanned: 4 files (docker-compose.yml, Caddyfile, .env.example, clickhouse/np-erase-user.xml)")
    print("posthog infra: " + (f"{len(problems)} problem(s)" if problems else "P1-P12 hold"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
