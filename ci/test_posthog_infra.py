#!/usr/bin/env python3
"""The PostHog stack keeps its privacy configuration (NP-ANALYTICS-001, NP-APP-TELEMETRY-001 §4; privacy review Finding 4).

infra/posthog had no CI at all, and its compose file did not even parse (a `: ` inside an unquoted error message). This test
renders the stack with `docker compose config` and reads the Caddyfile and .env.example, and fails if any of these no longer holds:

The stack is three compose files (OI-ANALYTICS-03; NP-ANALYTICS-001 §7.3): PostHog's hobby file at the pinned commit, the pin's
digests (docker-compose.pin.yml) and NeurOne's layer (docker-compose.neurone.yml), rendered together through the COMPOSE_FILE line
of .env.example, which is what an operator's `docker compose` reads.

  P1  the stack renders, and refuses to without each required variable (REQUIRED_VARIABLES, names only: PostHog's own file reads two of them as
      plain $VARs, so NeurOne's layer requires them);
  P2  every service PostHog defines runs an image by sha256 digest, the pin holds (pin_posthog.py --check: the vendored PostHog
      files unedited, every service pinned, every PostHog-built image's revision the commit or an ancestor), and the two images
      NeurOne names (Caddy, the relay's Python) carry a tag that is not `latest`;
  P3  PostHog's privacy switches are set on every service that runs PostHog's app or Node image: no GeoIP (DISABLE_MMDB), no
      usage telemetry to posthog.com (OPT_OUT_CAPTURE), one organisation (MULTI_ORG_ENABLED);
  P4  no service but the proxy publishes a port, and the proxy publishes 80 and 443 and its plain-HTTP port on 127.0.0.1 only;
  P5  the internal network is internal, and every service but the proxy sits on it and on nothing else; the proxy sits on it
      and the external network; no other network exists;
  P6  no reverse_proxy in the Caddyfile hands its upstream (PostHog, the erasure relay) a header that can carry the client's
      address (CLIENT_ADDRESS_HEADERS, each as `header_up -NAME`, none set to a value; OI-PRIV-51), and the Caddyfile sends HSTS
      for every host except localhost;
  P7  .env.example asks for no release tag (no POSTHOG_VERSION: PostHog publishes none, and the version is the pinned commit) and
      its COMPOSE_FILE names the three files in their order;
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
  P13 ClickHouse's own server log is bounded (OI-PRIV-46 (2)). clickhouse/np-privacy.xml has one `<logger>` whose `level` and
      `errorlog_level` are `warning` or quieter (the stock image logs at `trace`, which records query text, an erase's
      distinct_id included), whose `size` and `count` are within the ceilings below (the stock image keeps ten files of
      1000M), that sends nothing to syslog and raises no logger above `warning` in `<levels>`; and, on the rendered compose
      file, every service whose image is a ClickHouse server mounts that file into a `config.d/` directory, so the override
      is applied. The file check needs no Docker; the mount check is written for any compose layout, the rebuilt one
      (OI-ANALYTICS-03) included.

  P14 Caddy serves only what NeurOne uses (OI-ANALYTICS-03): its reverse proxies go to the erasure relay, PostHog's capture
      service and its web server and nowhere else; every public endpoint PostHog's own proxy exposes beyond those
      (UNSERVED_PATHS) is in an `@unserved` matcher answered `respond 404`; both sites import the one routes snippet. With a
      Docker daemon the adapted configuration is read too, and each path in ROUTE_CASES must reach the handler named, in the
      order Caddy tries its routes (a matcher written but shadowed by an earlier route protects nothing).

`--self-test` proves each check goes red on a copy broken in exactly that way. Without Docker the compose checks are reported as
SKIPPED (P11 among them: it reads the rendered file), and without a Docker daemon P9 is, and the run still fails if --require-docker is given (CI gives it), so a missing
Docker never reads as a pass.

Run: python3 ci/test_posthog_infra.py [--self-test] [--require-docker]

CI-Kind: gate
CI-Self-Test: python3 ci/test_posthog_infra.py --self-test --require-docker
CI-Scans: the rendered PostHog stack (three compose files and the pin), its Caddyfile, .env.example, the erasure relay's ClickHouse user file and ClickHouse's np-privacy.xml, for the privacy configuration P1-P14
CI-Scan-Paths: infra/posthog/docker-compose.hobby.yml infra/posthog/docker-compose.pin.yml infra/posthog/docker-compose.neurone.yml infra/posthog/posthog-pin.json infra/posthog/caddy/Caddyfile infra/posthog/.env.example infra/posthog/clickhouse/np-erase-user.xml infra/posthog/clickhouse/np-privacy.xml
CI-Self-Test-Reads-Tree: each case copies the real compose file, Caddyfile, .env.example and ClickHouse config files and breaks the copy in one way, so the real files are the base the mutants are made from
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
sys.path.insert(0, str(INFRA))
import pin_posthog  # noqa: E402  (P2 runs the pin's own offline check)

COMPOSE_FILES = ("docker-compose.hobby.yml", "docker-compose.pin.yml", "docker-compose.neurone.yml")
REQUIRED_VARIABLES = ("POSTHOG_SECRET", "ENCRYPTION_SALT_KEYS", "OBJECT_STORAGE_ACCESS_KEY_ID", "OBJECT_STORAGE_SECRET_ACCESS_KEY",
                    "ERASE_CLICKHOUSE_PASSWORD", "RETENTION_CLICKHOUSE_PASSWORD")
ENV = {**{k: "x" for k in REQUIRED_VARIABLES}, "DOMAIN": "localhost", "REGISTRY_URL": "posthog/posthog", "POSTHOG_APP_TAG": "pinned",
       "TLS_BLOCK": "", "COMPOSE_FILE": ":".join(COMPOSE_FILES)}
POSTHOG_SWITCHES = {"DISABLE_MMDB": "true", "OPT_OUT_CAPTURE": "true", "MULTI_ORG_ENABLED": "false"}
# P3: the images that run PostHog's Django app or its Node services (by repository, digest or tag after it).
POSTHOG_APP_IMAGE = re.compile(r"^(docker\.io/)?posthog/posthog[@:]|^ghcr\.io/posthog/posthog-node[@:]")
PROXY = "proxy"
NEURONE_IMAGES = {PROXY: "caddy", "erase-relay": "python"}
INTERNAL, EXTERNAL = "analytics-internal", "analytics-external"
# P14: the upstreams Caddy may proxy to, and PostHog's public endpoints it must refuse (the paths PostHog's own proxy routes to
# its replay, AI, logs, object-storage, live-events, webhook, survey, remote-config and flag services, at the pinned commit).
ALLOWED_UPSTREAMS = ("erase-relay:8080", "capture:3000", "web:8000")
UNSERVED_PATHS = ("/s", "/s/*", "/i/v0/ai", "/i/v0/ai/*", "/i/v1/ai/events", "/i/v1/logs", "/i/v1/logs/*", "/i/v1/traces",
                  "/i/v1/traces/*", "/i/v1/metrics", "/i/v1/metrics/*", "/posthog", "/posthog/*", "/livestream", "/livestream/*",
                  "/public/webhooks", "/public/webhooks/*", "/public/m/*", "/surveys", "/api/surveys", "/array/*", "/flags",
                  "/flags/*", "/api/feature_flag/local_evaluation", "/api/feature_flag/local_evaluation/*")
# P14 on Caddy's adapted configuration: a request path and the handler it must reach first (an upstream, or 404).
ROUTE_CASES = (("/capture/", "capture:3000"), ("/batch/", "capture:3000"), ("/e/", "capture:3000"), ("/i/v0/e/", "capture:3000"),
               ("/np/erase", "erase-relay:8080"), ("/s/", 404), ("/i/v0/ai", 404), ("/i/v0/ai/x", 404), ("/i/v1/logs", 404),
               ("/posthog/x", 404), ("/flags/", 404), ("/livestream/x", 404), ("/array/x/config", 404), ("/", "web:8000"),
               ("/api/projects/", "web:8000"))
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
# P13 ceilings for ClickHouse's server log, per log file (clickhouse-server.log and .err.log): the figures np-privacy.xml
# uses, with no derivation (UC-223). Lowering one is free; raising one is a register change.
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
    p = subprocess.run(["docker", "compose", "config", "--format", "json"], cwd=INFRA, env=env,
                       capture_output=True, text=True)
    return p.returncode, (json.loads(p.stdout) if p.returncode == 0 else None), p.stderr


def compose_problems(c, pin_problems=None):
    out = []
    services = c["services"]
    for name, s in sorted(services.items()):
        img = s.get("image", "")
        if name in NEURONE_IMAGES:
            repo, _, tag = img.partition(":")
            if repo.rpartition("/")[2] != NEURONE_IMAGES[name] or not tag or tag == "latest":
                out.append(f"P2 {name}: image '{img}' is not a tagged {NEURONE_IMAGES[name]} image (never `latest`)")
        elif not re.search(r"@sha256:[0-9a-f]{64}$", img):
            out.append(f"P2 {name}: image '{img}' is not pinned by digest (pin_posthog.py writes every PostHog image)")
    for prob in (pin_posthog.check_problems() if pin_problems is None else pin_problems):
        out.append(f"P2 the pin does not hold: {prob}")
    app = [n for n, s in sorted(services.items()) if POSTHOG_APP_IMAGE.search(s.get("image", ""))]
    if not app:
        out.append("P3 no service runs PostHog's app or Node image, so no privacy switch can be checked")
    for name in app:
        env = services[name].get("environment") or {}
        for k, want in POSTHOG_SWITCHES.items():
            if env.get(k) != want:
                out.append(f"P3 {name}: {k} is {env.get(k)!r}, required {want!r}")
    for name, s in sorted(services.items()):
        for port in s.get("ports") or []:
            if name != PROXY:
                out.append(f"P4 {name}: publishes port {port.get('published')}; only the proxy may")
            elif str(port.get("target")) not in ("80", "443") and port.get("host_ip") != "127.0.0.1":
                out.append(f"P4 {name}: port {port.get('published')} (plain HTTP) is published on {port.get('host_ip') or 'every interface'}, not 127.0.0.1")
    nets = c.get("networks", {})
    if not nets.get(INTERNAL, {}).get("internal"):
        out.append(f"P5 network {INTERNAL} is not internal")
    for extra in sorted(set(nets) - {INTERNAL, EXTERNAL}):
        out.append(f"P5 network {extra} exists; only {INTERNAL} and {EXTERNAL} may")
    for name, s in sorted(services.items()):
        on = sorted(s.get("networks") or {})
        want = sorted([INTERNAL, EXTERNAL]) if name == PROXY else [INTERNAL]
        if on != want:
            out.append(f"P5 {name}: networks {on}, required {want}")
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
    """P13 on np-privacy.xml: the server's file log at `warning` or quieter, bounded in size and file count."""
    try:
        root = ET.fromstring(text)
    except ET.ParseError as e:
        return [f"P13 np-privacy.xml does not parse: {e}"]
    loggers = root.findall("logger")
    if len(loggers) != 1:
        return [f"P13 np-privacy.xml has {len(loggers)} <logger> blocks, required exactly one: without it the stock image "
                "logs at trace, ten files of 1000M per log"]
    lg = loggers[0]
    out = []
    for key in ("level", "errorlog_level"):
        v = (lg.findtext(key) or "").strip().lower()
        if v not in CH_LOG_QUIET_LEVELS:
            out.append(f"P13 <logger><{key}> is {v or 'unset'!r}, required one of {', '.join(CH_LOG_QUIET_LEVELS)}"
                       + (" (unset, errorlog_level defaults to notice, which raises the server's level)" if key == "errorlog_level" and not v else ""))
    size = ch_log_size_bytes(lg.findtext("size"))
    if size is None or size <= 0:
        out.append(f"P13 <logger><size> is {lg.findtext('size')!r}, not a size bound")
    elif size > CH_LOG_MAX_SIZE_BYTES:
        out.append(f"P13 <logger><size> {lg.findtext('size').strip()} is over the {CH_LOG_MAX_SIZE_BYTES // 1024 ** 2}M ceiling (UC-223)")
    count = (lg.findtext("count") or "").strip()
    if not count.isdigit() or int(count) < 1:
        out.append(f"P13 <logger><count> is {lg.findtext('count')!r}, not a file count")
    elif int(count) > CH_LOG_MAX_COUNT:
        out.append(f"P13 <logger><count> {count} is over the ceiling of {CH_LOG_MAX_COUNT} (UC-223)")
    if (lg.findtext("use_syslog") or "").strip().lower() in ("1", "true", "yes"):
        out.append("P13 <logger><use_syslog> is on: syslog takes the log where no bound here reaches")
    levels = lg.find("levels")
    if levels is not None:
        for el in levels.iter():
            if el is levels or len(el):
                continue
            if el.tag == "name":
                continue
            v = (el.text or "").strip().lower()
            if v not in CH_LOG_QUIET_LEVELS:
                out.append(f"P13 <logger><levels> sets {el.tag} to {v!r}, louder than warning")
    return out


CLICKHOUSE_IMAGE = re.compile(r"(^|/)clickhouse(-server|-posthog)?(:|@|$)")


def clickhouse_mount_problems(c):
    """P13 on the rendered compose file: every ClickHouse server mounts np-privacy.xml into a config.d directory."""
    out = []
    servers = [(n, s) for n, s in sorted(c["services"].items()) if CLICKHOUSE_IMAGE.search(s.get("image") or "")]
    if not servers:
        return ["P13 no service runs a ClickHouse server image, so nothing applies np-privacy.xml"]
    for name, s in servers:
        mounted = any(pathlib.PurePosixPath(str(v.get("source", ""))).name == "np-privacy.xml"
                      and re.search(r"/config\.d/[^/]+\.xml$", str(v.get("target", "")))
                      for v in s.get("volumes") or [] if isinstance(v, dict))
        if not mounted:
            out.append(f"P13 {name}: does not mount clickhouse/np-privacy.xml into a config.d/ directory, so the server "
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
    m = re.search(r"^\s*image:\s*(caddy:\S+)\s*$", (INFRA / "docker-compose.neurone.yml").read_text(), re.M)
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


def caddy_adapt(text):
    """(adapted JSON or None, error) from `caddy adapt` in the compose file's own Caddy image. Needs Docker."""
    image = caddy_image()
    if image is None:
        return None, "the compose file names no caddy image"
    p = subprocess.run(["docker", "run", "--rm", "-i", "-e", "POSTHOG_HOST=localhost", image,
                        "caddy", "adapt", "--adapter", "caddyfile", "--config", "/dev/stdin"],
                       input=text, capture_output=True, text=True)
    if p.returncode != 0:
        return None, (p.stderr.strip().splitlines() or [str(p.returncode)])[-1]
    return json.loads(p.stdout), None


def caddy_route_problems(text):
    """P14 on the Caddyfile text."""
    out = []
    code = "\n".join(line.split("#", 1)[0] for line in text.splitlines())
    for up in re.findall(r"reverse_proxy\s+(\S+)", code):
        if up not in ALLOWED_UPSTREAMS:
            out.append(f"P14 Caddy proxies to {up}; only {', '.join(ALLOWED_UPSTREAMS)} are served")
    m = re.search(r"^\s*@unserved\s*\{", code, re.M)
    if not m:
        return out + ["P14 Caddyfile has no @unserved matcher"]
    listed = set(" ".join(re.findall(r"^\s*path\s+(.*)$", blocks(code, r"^\s*@unserved\s*\{")[0], re.M)).split())
    for path in UNSERVED_PATHS:
        if path not in listed:
            out.append(f"P14 @unserved does not list {path}, so it falls through to a service")
    handled = blocks(code, r"^\s*handle\s+@unserved\s*\{")
    if not handled or not re.fullmatch(r"\s*respond\s+404\s*", handled[0]):
        out.append("P14 `handle @unserved` does not answer `respond 404` and nothing else")
    uns, cap = code.find("handle @unserved"), code.find("handle @capture")
    if cap != -1 and uns > cap:
        out.append("P14 `handle @unserved` comes after `handle @capture`, whose /i/v0/* then takes /i/v0/ai")
    sites = re.findall(r"^(\S[^\n{]*)\{\s*\n\s*import\s+posthog_routes\s*\n\s*\}", code, re.M)
    if len(sites) != 2:
        out.append(f"P14 {len(sites)} site blocks import posthog_routes and nothing else; required two (the TLS host and :8000)")
    return out


def _path_matches(pattern, path):
    return path.startswith(pattern[:-1]) if pattern.endswith("*") else path == pattern


def _first_handler(routes, path):
    """The handler a request for `path` reaches first in Caddy's adapted routes: an upstream 'host:port', a status, or None."""
    for r in routes:
        matchers = r.get("match") or [{}]
        # A request to this site: a `host` matcher is the site's own and matches; a path matcher is evaluated; any other
        # matcher (`not`, a header) is taken not to match, so it cannot make a path look served.
        if not any(all((k == "host") or (k == "path" and any(_path_matches(p, path) for p in v)) for k, v in m.items())
                   for m in matchers):
            continue
        for h in r.get("handle") or []:
            if h.get("handler") == "reverse_proxy":
                return h["upstreams"][0]["dial"]
            if h.get("handler") == "static_response":
                return int(h.get("status_code", 200))
            if h.get("handler") == "subroute":
                got = _first_handler(h.get("routes") or [], path)
                if got is not None:
                    return got
    return None


def caddy_adapted_route_problems(adapted):
    """P14 on Caddy's adapted JSON: each ROUTE_CASES path reaches its handler first, on every site."""
    out = []
    servers = (((adapted or {}).get("apps") or {}).get("http") or {}).get("servers") or {}
    if not servers:
        return ["P14 Caddy's adapted configuration has no HTTP server"]
    for sname, srv in sorted(servers.items()):
        for path, want in ROUTE_CASES:
            got = _first_handler(srv.get("routes") or [], path)
            if got != want:
                out.append(f"P14 server {sname}: {path} reaches {got!r}, required {want!r}")
    return out


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
    out = []
    if re.search(r"^\s*POSTHOG_VERSION\s*=", text, re.M):
        out.append("P7 .env.example asks for POSTHOG_VERSION: PostHog publishes no release tags, the version is the pinned commit")
    if re.search(r"^\s*\w*(TAG|IMAGE)\w*\s*=\s*['\"]?(latest|master)['\"]?\s*$", text, re.M | re.I):
        out.append("P7 .env.example points an image at latest or master")
    m = re.search(r"^COMPOSE_FILE=(.*)$", text, re.M)
    if not m or m.group(1).strip() != ":".join(COMPOSE_FILES):
        out.append(f"P7 .env.example's COMPOSE_FILE is {m.group(1).strip() if m else 'missing'!r}, required {':'.join(COMPOSE_FILES)!r}")
    return out


def read():
    return (INFRA / "caddy" / "Caddyfile").read_text(), (INFRA / ".env.example").read_text()


def read_erase_user():
    return ERASE_USER_FILE.read_text()


def read_ch():
    return (INFRA / "clickhouse" / "np-privacy.xml").read_text()


def check(require_docker):
    problems = []
    caddy, env = read()
    problems += caddy_problems(caddy) + caddy_log_problems(caddy) + caddy_route_problems(caddy) + env_problems(env) + env_url_problems(env) + clickhouse_logger_problems(read_ch())
    if shutil.which("docker") is None:
        msg = "compose checks P1-P5, P10, P11, the P13 mount check and the Caddy checks P9 and adapted P14 SKIPPED: docker is not installed"
        if require_docker:
            problems.append(msg + " (and --require-docker was given)")
        else:
            print(msg)
    else:
        code, cfg, err = compose()
        if code != 0:
            problems.append(f"P1 the compose file does not render: {err.strip().splitlines()[-1] if err.strip() else code}")
        else:
            problems += compose_problems(cfg) + logging_problems(cfg) + erase_user_problems(cfg, read_erase_user()) + clickhouse_mount_problems(cfg)
        if docker_daemon():
            problems += caddy_adapt_problems(caddy)
            adapted, _ = caddy_adapt(caddy)
            if adapted is not None:
                problems += caddy_adapted_route_problems(adapted)
        elif require_docker:
            problems.append("Caddy check P9 SKIPPED: no Docker daemon answers (and --require-docker was given)")
        else:
            print("Caddy check P9 SKIPPED: no Docker daemon answers")
        # P1: it must refuse to start without each required variable (only the names are printed; every value here is "x").
        for name in REQUIRED_VARIABLES:
            if compose(drop=(name,))[0] == 0:
                problems.append(f"P1 the compose file renders without {name}: it should refuse to start")
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
    expect("PostHog gets Forwarded", caddy_problems(re.sub(r"(reverse_proxy web:8000 \{.*?)\n\s*header_up -Forwarded\n", r"\1\n", caddy, count=1, flags=re.S)), "P6")
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
    expect(".env.example asks for a release tag", env_problems(env + "POSTHOG_VERSION=release-1.43.0\n"), "P7")
    expect(".env.example floats the app image", env_problems(re.sub(r"^POSTHOG_APP_TAG=.*$", "POSTHOG_APP_TAG=latest", env, flags=re.M)), "P7")
    expect(".env.example drops the pin file", env_problems(env.replace(":docker-compose.pin.yml", "")), "P7")
    expect(".env.example has no COMPOSE_FILE", env_problems(re.sub(r"^COMPOSE_FILE=.*$", "", env, flags=re.M)), "P7")
    expect("Caddy proxies to replay capture", caddy_route_problems(caddy.replace("reverse_proxy capture:3000", "reverse_proxy replay-capture:3000")), "P14")
    expect("Caddy proxies to object storage", caddy_route_problems(caddy.replace("  handle @unserved {", "  handle /posthog/* {\n    reverse_proxy objectstorage:19000\n  }\n  handle @unserved {")), "P14")
    expect("/s is served", caddy_route_problems(caddy.replace("    path /s /s/ /s/*\n", "")), "P14")
    expect("/flags is served", caddy_route_problems(re.sub(r"\n    path /flags [^\n]*", "", caddy)), "P14")
    expect("@unserved answers 200", caddy_route_problems(caddy.replace("respond 404", "respond 200")), "P14")
    a, b = caddy.index("  # PostHog endpoints NeurOne does not serve"), caddy.index("  # PostHog's capture endpoints go to")
    c_ = caddy.index("  # All other traffic (the PostHog UI")
    expect("@unserved after @capture", caddy_route_problems(caddy[:a] + caddy[b:c_] + caddy[a:b] + caddy[c_:]), "P14")
    expect("the :8000 site does not import the routes", caddy_route_problems(caddy.replace("http://:8000 {\n  import posthog_routes", "http://:8000 {\n  respond 200")), "P14")
    expect(".env.example suggests a LAN address over http", env_url_problems(env + "#   Physical device (local net): http://192.168.x.x:8000\n"), "P12")
    expect(".env.example sets SITE_URL to a LAN address over http", env_url_problems(env + "SITE_URL=http://10.0.0.5:8000\n"), "P12")
    ch = read_ch()
    expect("no <logger> block", clickhouse_logger_problems(re.sub(r"<logger>.*?</logger>", "", ch, flags=re.S)), "P13")
    expect("the logger at trace", clickhouse_logger_problems(ch.replace("<level>warning</level>", "<level>trace</level>")), "P13")
    expect("the logger at information", clickhouse_logger_problems(ch.replace("<level>warning</level>", "<level>information</level>")), "P13")
    expect("no errorlog_level (defaults to notice)", clickhouse_logger_problems(ch.replace("<errorlog_level>warning</errorlog_level>", "")), "P13")
    expect("the stock size", clickhouse_logger_problems(ch.replace("<size>10M</size>", "<size>1000M</size>")), "P13")
    expect("no size", clickhouse_logger_problems(ch.replace("<size>10M</size>", "")), "P13")
    expect("a time rotation", clickhouse_logger_problems(ch.replace("<size>10M</size>", "<size>daily</size>")), "P13")
    expect("the stock count", clickhouse_logger_problems(ch.replace("<count>3</count>", "<count>10</count>")), "P13")
    expect("no count", clickhouse_logger_problems(ch.replace("<count>3</count>", "")), "P13")
    expect("syslog on", clickhouse_logger_problems(ch.replace("<count>3</count>", "<count>3</count><use_syslog>1</use_syslog>")), "P13")
    expect("a logger raised to trace", clickhouse_logger_problems(ch.replace("<count>3</count>", "<count>3</count><levels><logger><name>executeQuery</name><level>trace</level></logger></levels>")), "P13")
    expect("a logger only in a comment", clickhouse_logger_problems(re.sub(r"(<logger>.*?</logger>)", r"<!-- \1 -->", ch, flags=re.S)), "P13")
    expect("a second logger", clickhouse_logger_problems(ch.replace("</clickhouse>", "<logger><level>trace</level></logger></clickhouse>")), "P13")
    if clickhouse_logger_problems(ch.replace("<count>3</count>", "<count>3</count><levels><logger><name>executeQuery</name><level>none</level></logger></levels>")):
        failures.append("self-test: P13 refused a <levels> entry that silences a logger")
    for label, got in (("the real Caddyfile", caddy_problems(caddy) + caddy_log_problems(caddy) + caddy_route_problems(caddy)), ("the real .env.example", env_problems(env) + env_url_problems(env)),
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
            adapted, err = caddy_adapt(caddy)
            if adapted is None or caddy_adapted_route_problems(adapted):
                failures.append(f"self-test: the real Caddyfile's routes should be clean but raised {err or caddy_adapted_route_problems(adapted)}")
            else:
                swapped, err = caddy_adapt(caddy[:a] + caddy[b:c_] + caddy[a:b] + caddy[c_:])
                expect("adapted: @unserved after @capture", caddy_adapted_route_problems(swapped) if swapped else [f"P14 {err}"], "P14")
                expect("adapted: /s/ not refused", caddy_adapted_route_problems(caddy_adapt(caddy.replace("    path /s /s/ /s/*\n", ""))[0]), "P14")
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
            mutate("the app image by tag", "P2", lambda c: c["services"]["web"].update(image="posthog/posthog:latest"))
            mutate("ClickHouse by tag", "P2", lambda c: c["services"]["clickhouse"].update(image="ghcr.io/posthog/clickhouse-posthog:26.9.5.2-posthog-c73aad4ad67d"))
            mutate("Caddy on latest", "P2", lambda c: c["services"]["proxy"].update(image="caddy:latest"))
            expect("the pin does not hold", compose_problems(cfg, pin_problems=["PIN2 docker-compose.hobby.yml differs"]), "P2")
            mutate("GeoIP on in plugins", "P3", lambda c: c["services"]["plugins"]["environment"].update(DISABLE_MMDB="false"))
            mutate("telemetry on in the worker", "P3", lambda c: c["services"]["worker"]["environment"].pop("OPT_OUT_CAPTURE"))
            mutate("multi-org on in web", "P3", lambda c: c["services"]["web"]["environment"].update(MULTI_ORG_ENABLED="true"))
            mutate("plain HTTP on every interface", "P4", lambda c: [p.pop("host_ip", None) for p in c["services"]["proxy"]["ports"] if str(p.get("target")) == "8000"])
            mutate("SeaweedFS published", "P4", lambda c: c["services"]["seaweedfs"].update(ports=[{"host_ip": "127.0.0.1", "published": "8333", "target": 8333}]))
            mutate("ClickHouse published", "P4", lambda c: c["services"]["clickhouse"].update(ports=[{"published": "8123", "target": 8123}]))
            mutate("network not internal", "P5", lambda c: c["networks"]["analytics-internal"].update(internal=False))
            mutate("capture on the external network", "P5", lambda c: c["services"]["capture"]["networks"].update({"analytics-external": None}))
            mutate("PostHog's default network back", "P5", lambda c: c["networks"].update(default={"name": "x"}))
            mutate("the proxy off the external network", "P5", lambda c: c["services"]["proxy"]["networks"].pop("analytics-external"))

            def mutate_log(label, fn):
                c = copy.deepcopy(cfg)
                fn(c)
                expect(label, logging_problems(c), "P10")
            mutate_log("the proxy logs with Docker's default", lambda c: c["services"]["proxy"].pop("logging"))
            mutate_log("a PostHog service keeps PostHog's 50m", lambda c: c["services"]["capture"]["logging"]["options"].update({"max-size": "50m"}))
            mutate_log("the erasure relay ships logs to syslog", lambda c: c["services"]["erase-relay"].update(logging={"driver": "syslog"}))
            mutate_log("PostHog keeps a larger file", lambda c: c["services"]["web"]["logging"]["options"].update({"max-size": "500m"}))
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
                                  ("np_erase from another service", xml.replace(host, "<host>web</host>")),
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
            mutate_erase("PostHog aliased as the relay", lambda c: c["services"]["web"].update(networks={"analytics-internal": {"aliases": ["erase-relay"]}}))
            mutate_erase("Kafka's container named as the relay", lambda c: c["services"]["kafka"].update(container_name="erase-relay"))
            mutate_erase("a second service runs the relay", lambda c: c["services"].update({"erase-relay-2": copy.deepcopy(c["services"]["erase-relay"])}))
            mutate_erase("ClickHouse does not mount the user file", lambda c: c["services"]["clickhouse"].update(volumes=[v for v in c["services"]["clickhouse"]["volumes"] if "np-erase-user" not in str(v.get("source"))]))
            def mutate_ch(label, fn):
                c = copy.deepcopy(cfg)
                fn(c)
                expect(label, clickhouse_mount_problems(c), "P13")
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
    # Eight files: the three compose files, posthog-pin.json, the Caddyfile, .env.example and the two ClickHouse files.
    print("scanned: 8 files (docker-compose.hobby.yml, docker-compose.pin.yml, docker-compose.neurone.yml, posthog-pin.json, "
          "Caddyfile, .env.example, clickhouse/np-erase-user.xml, clickhouse/np-privacy.xml)")
    print("posthog infra: " + (f"{len(problems)} problem(s)" if problems else "P1-P14 hold"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
