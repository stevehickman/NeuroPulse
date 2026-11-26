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
  P11 ClickHouse's own server log is bounded (OI-PRIV-46 (2)). clickhouse/np-privacy.xml has one `<logger>` whose `level` and
      `errorlog_level` are `warning` or quieter (the stock image logs at `trace`, which records query text, an erase's
      distinct_id included), whose `size` and `count` are within the ceilings below (the stock image keeps ten files of
      1000M), that sends nothing to syslog and raises no logger above `warning` in `<levels>`; and, on the rendered compose
      file, every service whose image is a ClickHouse server mounts that file into a `config.d/` directory, so the override
      is applied. The file check needs no Docker; the mount check is written for any compose layout, the rebuilt one
      (OI-ANALYTICS-03) included.

`--self-test` proves each check goes red on a copy broken in exactly that way. Without Docker the compose checks are reported as
SKIPPED, and without a Docker daemon P9 is, and the run still fails if --require-docker is given (CI gives it), so a missing
Docker never reads as a pass.

Run: python3 ci/test_posthog_infra.py [--self-test] [--require-docker]

CI-Kind: gate
CI-Self-Test: python3 ci/test_posthog_infra.py --self-test --require-docker
CI-Scans: the rendered PostHog compose file, its Caddyfile, .env.example and ClickHouse's np-privacy.xml, for the privacy configuration P1-P11
CI-Scan-Paths: infra/posthog/docker-compose.yml infra/posthog/caddy/Caddyfile infra/posthog/.env.example infra/posthog/clickhouse/np-privacy.xml
CI-Self-Test-Reads-Tree: each case copies the real compose file, Caddyfile, .env.example and np-privacy.xml and breaks the copy in one way, so the real files are the base the mutants are made from
"""
import copy
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parent.parent
INFRA = ROOT / "infra" / "posthog"
ENV = {"SECRET_KEY": "x", "POSTGRES_PASSWORD": "x", "CLICKHOUSE_PASSWORD": "x", "ERASE_CLICKHOUSE_PASSWORD": "x", "POSTHOG_VERSION": "release-test"}
POSTHOG_SWITCHES = {"DISABLE_MMDB": "true", "OBJECT_STORAGE_ENABLED": "false", "CAPTURE_INTERNAL_METRICS": "false", "SENTRY_DSN": ""}
DATA_SERVICES = ("db", "redis", "zookeeper", "clickhouse", "kafka")
PROXY = "caddy"
# P10 ceilings, per container: the figures the compose file uses, with no derivation (UC-211). Lowering one is free;
# raising one is a register change.
LOG_MAX_SIZE_BYTES = 10 * 1024 * 1024
LOG_MAX_FILES = 3
LOG_DRIVERS_BOUNDED = ("local", "json-file")
# P11 ceilings for ClickHouse's server log, per log file (clickhouse-server.log and .err.log): the figures np-privacy.xml
# uses, with no derivation (UC-220). Lowering one is free; raising one is a register change.
CH_LOG_MAX_SIZE_BYTES = 10 * 1024 * 1024
CH_LOG_MAX_COUNT = 3
# Poco's level names from `warning` down: anything here keeps no information, debug or trace line, and so no query text
# from a query that succeeds.
CH_LOG_QUIET_LEVELS = ("none", "fatal", "critical", "error", "warning")
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


def ch_log_size_bytes(value):
    """Poco FileChannel's size rotation (`10M`, `512K`, plain bytes) as bytes, or None for anything else (a time rotation
    such as `daily` bounds no size)."""
    m = re.fullmatch(r"\s*(\d+)\s*([KM]?)\s*", str(value or ""), re.I)
    if not m:
        return None
    return int(m.group(1)) * {"": 1, "k": 1024, "m": 1024 ** 2}[m.group(2).lower()]


def clickhouse_logger_problems(text):
    """P11 on np-privacy.xml: the server's file log at `warning` or quieter, bounded in size and file count."""
    try:
        root = ET.fromstring(text)
    except ET.ParseError as e:
        return [f"P11 np-privacy.xml does not parse: {e}"]
    loggers = root.findall("logger")
    if len(loggers) != 1:
        return [f"P11 np-privacy.xml has {len(loggers)} <logger> blocks, required exactly one: without it the stock image "
                "logs at trace, ten files of 1000M per log"]
    lg = loggers[0]
    out = []
    for key in ("level", "errorlog_level"):
        v = (lg.findtext(key) or "").strip().lower()
        if v not in CH_LOG_QUIET_LEVELS:
            out.append(f"P11 <logger><{key}> is {v or 'unset'!r}, required one of {', '.join(CH_LOG_QUIET_LEVELS)}"
                       + (" (unset, errorlog_level defaults to notice, which raises the server's level)" if key == "errorlog_level" and not v else ""))
    size = ch_log_size_bytes(lg.findtext("size"))
    if size is None or size <= 0:
        out.append(f"P11 <logger><size> is {lg.findtext('size')!r}, not a size bound")
    elif size > CH_LOG_MAX_SIZE_BYTES:
        out.append(f"P11 <logger><size> {lg.findtext('size').strip()} is over the {CH_LOG_MAX_SIZE_BYTES // 1024 ** 2}M ceiling (UC-220)")
    count = (lg.findtext("count") or "").strip()
    if not count.isdigit() or int(count) < 1:
        out.append(f"P11 <logger><count> is {lg.findtext('count')!r}, not a file count")
    elif int(count) > CH_LOG_MAX_COUNT:
        out.append(f"P11 <logger><count> {count} is over the ceiling of {CH_LOG_MAX_COUNT} (UC-220)")
    if (lg.findtext("use_syslog") or "").strip().lower() in ("1", "true", "yes"):
        out.append("P11 <logger><use_syslog> is on: syslog takes the log where no bound here reaches")
    levels = lg.find("levels")
    if levels is not None:
        for el in levels.iter():
            if el is levels or len(el):
                continue
            if el.tag == "name":
                continue
            v = (el.text or "").strip().lower()
            if v not in CH_LOG_QUIET_LEVELS:
                out.append(f"P11 <logger><levels> sets {el.tag} to {v!r}, louder than warning")
    return out


CLICKHOUSE_IMAGE = re.compile(r"(^|/)clickhouse(-server|-posthog)?(:|@|$)")


def clickhouse_mount_problems(c):
    """P11 on the rendered compose file: every ClickHouse server mounts np-privacy.xml into a config.d directory."""
    out = []
    servers = [(n, s) for n, s in sorted(c["services"].items()) if CLICKHOUSE_IMAGE.search(s.get("image") or "")]
    if not servers:
        return ["P11 no service runs a ClickHouse server image, so nothing applies np-privacy.xml"]
    for name, s in servers:
        mounted = any(pathlib.PurePosixPath(str(v.get("source", ""))).name == "np-privacy.xml"
                      and re.search(r"/config\.d/[^/]+\.xml$", str(v.get("target", "")))
                      for v in s.get("volumes") or [] if isinstance(v, dict))
        if not mounted:
            out.append(f"P11 {name}: does not mount clickhouse/np-privacy.xml into a config.d/ directory, so the server "
                       "keeps the stock logger (trace, ten files of 1000M)")
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


def env_problems(text):
    m = re.search(r"^POSTHOG_VERSION=(.*)$", text, re.M)
    if not m:
        return ["P7 .env.example has no POSTHOG_VERSION line"]
    if m.group(1).strip().strip("'\"").lower() == "latest":
        return ["P7 .env.example pins POSTHOG_VERSION to latest"]
    return []


def read():
    return (INFRA / "caddy" / "Caddyfile").read_text(), (INFRA / ".env.example").read_text()


def read_ch():
    return (INFRA / "clickhouse" / "np-privacy.xml").read_text()


def check(require_docker):
    problems = []
    caddy, env = read()
    problems += caddy_problems(caddy) + caddy_log_problems(caddy) + env_problems(env) + clickhouse_logger_problems(read_ch())
    if shutil.which("docker") is None:
        msg = "compose checks P1-P5, P10, the P11 mount check and the Caddy check P9 SKIPPED: docker is not installed"
        if require_docker:
            problems.append(msg + " (and --require-docker was given)")
        else:
            print(msg)
    else:
        code, cfg, err = compose()
        if code != 0:
            problems.append(f"P1 the compose file does not render: {err.strip().splitlines()[-1] if err.strip() else code}")
        else:
            problems += compose_problems(cfg) + logging_problems(cfg) + clickhouse_mount_problems(cfg)
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
    ch = read_ch()
    expect("no <logger> block", clickhouse_logger_problems(re.sub(r"<logger>.*?</logger>", "", ch, flags=re.S)), "P11")
    expect("the logger at trace", clickhouse_logger_problems(ch.replace("<level>warning</level>", "<level>trace</level>")), "P11")
    expect("the logger at information", clickhouse_logger_problems(ch.replace("<level>warning</level>", "<level>information</level>")), "P11")
    expect("no errorlog_level (defaults to notice)", clickhouse_logger_problems(ch.replace("<errorlog_level>warning</errorlog_level>", "")), "P11")
    expect("the stock size", clickhouse_logger_problems(ch.replace("<size>10M</size>", "<size>1000M</size>")), "P11")
    expect("no size", clickhouse_logger_problems(ch.replace("<size>10M</size>", "")), "P11")
    expect("a time rotation", clickhouse_logger_problems(ch.replace("<size>10M</size>", "<size>daily</size>")), "P11")
    expect("the stock count", clickhouse_logger_problems(ch.replace("<count>3</count>", "<count>10</count>")), "P11")
    expect("no count", clickhouse_logger_problems(ch.replace("<count>3</count>", "")), "P11")
    expect("syslog on", clickhouse_logger_problems(ch.replace("<count>3</count>", "<count>3</count><use_syslog>1</use_syslog>")), "P11")
    expect("a logger raised to trace", clickhouse_logger_problems(ch.replace("<count>3</count>", "<count>3</count><levels><logger><name>executeQuery</name><level>trace</level></logger></levels>")), "P11")
    expect("a logger only in a comment", clickhouse_logger_problems(re.sub(r"(<logger>.*?</logger>)", r"<!-- \1 -->", ch, flags=re.S)), "P11")
    expect("a second logger", clickhouse_logger_problems(ch.replace("</clickhouse>", "<logger><level>trace</level></logger></clickhouse>")), "P11")
    if clickhouse_logger_problems(ch.replace("<count>3</count>", "<count>3</count><levels><logger><name>executeQuery</name><level>none</level></logger></levels>")):
        failures.append("self-test: P11 refused a <levels> entry that silences a logger")
    for label, got in (("the real Caddyfile", caddy_problems(caddy) + caddy_log_problems(caddy)), ("the real .env.example", env_problems(env)),
                       ("the real np-privacy.xml", clickhouse_logger_problems(ch))):
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
            real = compose_problems(cfg) + logging_problems(cfg) + clickhouse_mount_problems(cfg)
            if real:
                failures.append(f"self-test: the real compose file should be clean but raised {real}")
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

            def mutate_ch(label, fn):
                c = copy.deepcopy(cfg)
                fn(c)
                expect(label, clickhouse_mount_problems(c), "P11")
            ch_name = next(n for n, s in cfg["services"].items() if CLICKHOUSE_IMAGE.search(s.get("image") or ""))
            mutate_ch("ClickHouse without np-privacy.xml", lambda c: c["services"][ch_name].update(
                volumes=[v for v in c["services"][ch_name]["volumes"] if not str(v.get("source", "")).endswith("np-privacy.xml")]))
            mutate_ch("np-privacy.xml mounted outside config.d", lambda c: [v.update(target="/etc/np-privacy.xml")
                for v in c["services"][ch_name]["volumes"] if str(v.get("source", "")).endswith("np-privacy.xml")])
            mutate_ch("a second ClickHouse server without it", lambda c: c["services"].update(
                clickhouse2={"image": "ghcr.io/posthog/clickhouse-posthog:26.9.5.2", "volumes": []}))
            mutate_ch("no ClickHouse server at all", lambda c: c["services"].pop(ch_name))
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
    print("scanned: 4 files (docker-compose.yml, Caddyfile, .env.example, np-privacy.xml)")
    print("posthog infra: " + (f"{len(problems)} problem(s)" if problems else "P1-P11 hold"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
