#!/usr/bin/env python3
"""Pin the analytics stack to one PostHog commit (NP-ANALYTICS-001 §7.3; OI-ANALYTICS-03).

PostHog publishes no versioned self-hosted releases: its hobby deployment follows `master`. This script is the one way the stack
in infra/posthog moves to a PostHog version, and it moves the whole stack at once.

  python3 pin_posthog.py --commit <40-hex SHA>      # re-pin (needs the network: GitHub and the image registries)
  python3 pin_posthog.py --check                     # verify the pin offline (CI runs this; needs only the Docker CLI)
  python3 pin_posthog.py --fetch-tree                # check out the PostHog files the stack mounts (./posthog), at the pin

--commit does five things, and writes nothing unless all succeed:

  1. Downloads PostHog's docker-compose.hobby.yml, docker-compose.base.yml and .env.services AT THAT COMMIT and writes them here
     byte for byte (never edited: NeurOne's changes live in docker-compose.neurone.yml, which is applied on top).
  2. Classifies every service's image, as the commit's hobby file writes it:
       versioned     the commit names a tag (ClickHouse `clickhouse-posthog:26.9.5.2-posthog-…`, Redpanda, Postgres, …). That
                     tag is used, never one chosen here; it is resolved to the digest it names today.
       posthog-built PostHog builds it and the file floats it on `master` or `latest` (the Django app, posthog-node and the Rust
                     services). PostHog rebuilds an image only when its own code changes, so no commit has a full set of
                     builds of its own (the principal's option A, docs/human-input/oi-analytics-03-pin-mechanism.md). The
                     floating tag is resolved to a digest, and the build's `org.opencontainers.image.revision` must be the
                     commit itself or one of its ancestors; otherwise the pin is refused (choose a newer commit).
       unversioned   PostHog leaves the version open (`caddy`). Not pinned here: docker-compose.neurone.yml must name one.
  3. Checks ancestry with git against PostHog's history (a treeless, shallow fetch: commits only).
  4. Writes docker-compose.pin.yml (each pinned service's `image: repo@sha256:…`) and posthog-pin.json (the record).
  5. Prints what changed against the previous pin, so the upgrade record in NP-MAN-OPS-001 §4 can name it.

After a re-pin the upgrade procedure of NP-ANALYTICS-001 §7.3 is owed in full (retention, settings and erasure checks against the
running stack); this script cannot run them.

--check needs no network. It fails if a vendored file differs from the hash recorded at pinning (a hand edit), if a service in
the hobby file has no pin or a pin no longer matches the image the file writes (a re-vendor without a re-pin), if
docker-compose.pin.yml differs from the record, or if a PostHog-built image has no checked revision.
"""
import argparse
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

HERE = pathlib.Path(__file__).resolve().parent
UPSTREAM_REPO = "https://github.com/PostHog/posthog"
RAW = "https://raw.githubusercontent.com/PostHog/posthog/{commit}/{path}"
VENDORED = ("docker-compose.hobby.yml", "docker-compose.base.yml", ".env.services")
# The scripts bin/deploy-hobby writes into ./compose (the web and Temporal worker entrypoints), extracted from that script at the
# commit. Nothing else of deploy-hobby is used: it reports the install to PostHog and downloads a GeoIP database.
DEPLOY_SCRIPT = "bin/deploy-hobby"
COMPOSE_SCRIPTS = ("compose/start", "compose/wait", "compose/temporal-django-worker")
PIN_JSON = HERE / "posthog-pin.json"
PIN_YML = HERE / "docker-compose.pin.yml"
FLOATING = ("master", "latest", "")
# What bin/deploy-hobby substitutes for the variables in the hobby file's image strings (at the commit this was written
# against). POSTHOG_APP_TAG defaults to `latest` there; a 40-hex value only exists for commits that rebuilt the app.
IMAGE_VARS = {"REGISTRY_URL": "posthog/posthog", "POSTHOG_APP_TAG": "latest", "DOCKER_REGISTRY_PREFIX": "",
              "NODE_IMAGES_REGISTRY_URL": None, "POSTHOG_NODE_TAG": None, "CLICKHOUSE_SERVER_IMAGE": None}
POSTHOG_BUILT_REPOS = re.compile(r"^(docker\.io/)?posthog/posthog$|^ghcr\.io/posthog/posthog(/[a-z0-9-]+)?$|^ghcr\.io/posthog/posthog-node$")
ACCEPT = ", ".join((
    "application/vnd.oci.image.index.v1+json", "application/vnd.docker.distribution.manifest.list.v2+json",
    "application/vnd.oci.image.manifest.v1+json", "application/vnd.docker.distribution.manifest.v2+json"))
SHA40 = re.compile(r"^[0-9a-f]{40}$")
DIGEST = re.compile(r"^sha256:[0-9a-f]{64}$")


# ── image strings ────────────────────────────────────────────────────────────

def substitute(raw):
    """The image the hobby file names, with its variables as bin/deploy-hobby sets them and ${VAR:-default} defaults applied."""
    def one(m):
        name, default = m.group(1) or m.group(3), m.group(2)
        val = IMAGE_VARS.get(name)
        if val is not None:
            return val
        if default is None:
            raise ValueError(f"image '{raw}' uses ${name} with no default and no known value")
        return default
    return re.sub(r"\$\{(\w+):-([^}]*)\}|\$(\w+)", one, raw)


def split_ref(ref):
    """(registry host, repository, tag) for an image reference; Docker Hub is `docker.io` and single names get `library/`."""
    name, _, tag = ref.rpartition(":") if ":" in ref.split("/")[-1] else (ref, "", "")
    parts = name.split("/")
    if len(parts) > 1 and ("." in parts[0] or ":" in parts[0] or parts[0] == "localhost"):
        host, repo = parts[0], "/".join(parts[1:])
    else:
        host, repo = "docker.io", name
    if host == "docker.io" and "/" not in repo:
        repo = "library/" + repo
    return host, repo, tag


def display_repo(host, repo):
    if host == "docker.io":
        return repo[len("library/"):] if repo.startswith("library/") else repo
    return f"{host}/{repo}"


def classify(raw):
    ref = substitute(raw)
    host, repo, tag = split_ref(ref)
    built = bool(POSTHOG_BUILT_REPOS.match(display_repo(host, repo))) or bool(POSTHOG_BUILT_REPOS.match(f"{host}/{repo}"))
    if tag in FLOATING:
        kind = "posthog-built" if built else "unversioned"
    else:
        kind = "versioned"
    return {"compose_image": raw, "ref": ref, "registry": host, "repository": repo, "tag": tag, "kind": kind}


# ── registries ───────────────────────────────────────────────────────────────

def _get(url, headers=None, method="GET"):
    # Up to four tries on a 429, waiting Retry-After or 30 s, at most 120 s: tool limits with no derivation (UC-224).
    req = urllib.request.Request(url, headers=headers or {}, method=method)
    for attempt in range(4):
        try:
            return urllib.request.urlopen(req, timeout=60)
        except urllib.error.HTTPError as e:
            if e.code != 429 or attempt == 3:
                if e.code == 429:
                    raise SystemExit(f"{url.split('/')[2]} is rate-limiting this address (429); pin again later")
                raise
            time.sleep(min(int(e.headers.get("Retry-After") or 30), 120))
        except urllib.error.URLError as e:
            raise SystemExit(f"cannot reach {url.split('/')[2]}: {e.reason}")


def registry_token(host, repo):
    api = "registry-1.docker.io" if host == "docker.io" else host
    try:
        _get(f"https://{api}/v2/")
        return api, None
    except urllib.error.HTTPError as e:
        challenge = e.headers.get("WWW-Authenticate", "")
    params = dict(re.findall(r'(\w+)="([^"]*)"', challenge))
    if "realm" not in params:
        raise RuntimeError(f"{api}: no token realm in {challenge!r}")
    url = f"{params['realm']}?scope=repository:{repo}:pull" + (f"&service={params['service']}" if "service" in params else "")
    with _get(url) as r:
        body = json.load(r)
    return api, body.get("token") or body.get("access_token")


def resolve(entry):
    """Adds `digest` and, where the registry records one, `revision` (org.opencontainers.image.revision)."""
    api, token = registry_token(entry["registry"], entry["repository"])
    auth = {"Authorization": f"Bearer {token}"} if token else {}
    url = f"https://{api}/v2/{entry['repository']}/manifests/{entry['tag'] or 'latest'}"
    # HEAD gives the digest and does not count against Docker Hub's anonymous pull limit; the body is read only where a
    # revision is needed.
    with _get(url, {**auth, "Accept": ACCEPT}, method="HEAD") as r:
        digest = r.headers["Docker-Content-Digest"]
    if entry["kind"] != "posthog-built":
        entry["digest"] = digest
        return entry
    with _get(url, {**auth, "Accept": ACCEPT}) as r:
        if r.headers["Docker-Content-Digest"] != digest:
            raise SystemExit(f"{entry['ref']} moved while it was being pinned; pin again")
        manifest = json.load(r)
    revision = (manifest.get("annotations") or {}).get("org.opencontainers.image.revision")
    if not revision:
        child = manifest
        if "manifests" in manifest:
            plat = [m for m in manifest["manifests"] if (m.get("platform") or {}).get("architecture") == "amd64"] or manifest["manifests"]
            with _get(f"https://{api}/v2/{entry['repository']}/manifests/{plat[0]['digest']}", {**auth, "Accept": ACCEPT}) as r:
                child = json.load(r)
        revision = (child.get("annotations") or {}).get("org.opencontainers.image.revision")
        if not revision and "config" in child:
            with _get(f"https://{api}/v2/{entry['repository']}/blobs/{child['config']['digest']}", auth) as r:
                labels = (json.load(r).get("config") or {}).get("Labels") or {}
            revision = labels.get("org.opencontainers.image.revision")
    entry["digest"] = digest
    if revision:
        entry["revision"] = revision
    return entry


# ── git ──────────────────────────────────────────────────────────────────────

def history(commit, revisions, workdir):
    """Fetch commits only (no trees, no blobs) back far enough to hold every revision; returns {revision: is_ancestor}."""
    git = ["git", "-C", workdir]
    subprocess.run(["git", "init", "-q", workdir], check=True)
    out = {}
    for depth in (500, 5000, 50000):  # how deep to look for a revision: no derivation (UC-224)
        subprocess.run(git + ["fetch", "-q", "--filter=tree:0", f"--depth={depth}", UPSTREAM_REPO, commit], check=True)
        missing = [r for r in revisions if subprocess.run(git + ["cat-file", "-e", r + "^{commit}"], capture_output=True).returncode]
        if not missing:
            break
    for r in revisions:
        out[r] = subprocess.run(git + ["merge-base", "--is-ancestor", r, commit], capture_output=True).returncode == 0
    date = subprocess.run(git + ["show", "-s", "--format=%cI", commit], capture_output=True, text=True, check=True).stdout.strip()
    return out, date


# ── rendering ────────────────────────────────────────────────────────────────

def render(directory):
    p = subprocess.run(["docker", "compose", "-f", "docker-compose.hobby.yml", "config", "--no-interpolate", "--format", "json"],
                       cwd=directory, capture_output=True, text=True)
    if p.returncode:
        raise RuntimeError(f"docker compose cannot render the hobby file: {p.stderr.strip()}")
    return json.loads(p.stdout)


def tree_paths(directory=HERE):
    """The paths inside PostHog's repository that the hobby file bind-mounts (./posthog/…), relative to the repository."""
    root = (pathlib.Path(directory) / "posthog").resolve()
    out = set()
    for s in render(directory)["services"].values():
        for v in s.get("volumes") or []:
            src = pathlib.Path(v.get("source", "")) if v.get("type") == "bind" else None
            if src and root in src.parents:
                out.add(str(src.relative_to(root)))
    return sorted(out)


def do_fetch_tree():
    """Check out, at the pinned commit, only the files of PostHog's repository the hobby file mounts, into ./posthog."""
    record = json.loads(PIN_JSON.read_text())
    commit, dest = record["commit"], HERE / "posthog"
    git = ["git", "-C", str(dest)]
    if not (dest / ".git").exists():
        subprocess.run(["git", "init", "-q", str(dest)], check=True)
    subprocess.run(git + ["sparse-checkout", "set", "--no-cone"] + ["/" + t for t in tree_paths()], check=True)
    subprocess.run(git + ["fetch", "-q", "--depth=1", "--filter=blob:none", UPSTREAM_REPO, commit], check=True)
    subprocess.run(git + ["checkout", "-q", "--detach", commit], check=True)
    head = subprocess.run(git + ["rev-parse", "HEAD"], capture_output=True, text=True, check=True).stdout.strip()
    if head != commit:
        raise SystemExit(f"./posthog is at {head}, not the pinned {commit}")
    print(f"./posthog holds PostHog {commit}: " + ", ".join(tree_paths()))


def raw_images(directory):
    """{service: image as written} for the hobby file, `extends` resolved, variables left as written."""
    return {name: s.get("image", "") for name, s in render(directory)["services"].items()}


def pin_yml_text(record):
    lines = [
        "# GENERATED by pin_posthog.py from posthog-pin.json. Do not edit: re-pin instead (NP-ANALYTICS-001 §7.3).",
        f"# PostHog commit {record['commit']} ({record['commit_date']}).",
        "# Each image is the digest the commit's own image string resolved to when the pin was made; a PostHog-built image's",
        "# revision is the commit or one of its ancestors (checked). Services PostHog leaves unversioned are named in",
        "# docker-compose.neurone.yml instead.",
        "services:",
    ]
    for name, e in sorted(record["services"].items()):
        if e["kind"] == "unversioned":
            continue
        lines.append(f"    {name}:")
        lines.append(f"        image: {display_repo(e['registry'], e['repository'])}@{e['digest']}")
    return "\n".join(lines) + "\n"


def compose_scripts(deploy_text):
    """{compose/<name>: text} from deploy-hobby's `cat > compose/<name> <<EOF … EOF` blocks."""
    out = {}
    for m in re.finditer(r"^cat > (compose/[\w-]+) <<EOF\n(.*?)^EOF$", deploy_text, re.M | re.S):
        out[m.group(1)] = m.group(2)
    if set(out) != set(COMPOSE_SCRIPTS):
        raise SystemExit(f"refused: {DEPLOY_SCRIPT} writes {sorted(out)} into ./compose, this script expects {list(COMPOSE_SCRIPTS)}; "
                         "review what changed before pinning")
    return out


def sha256(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()


# ── commands ─────────────────────────────────────────────────────────────────

def do_pin(commit):
    if not SHA40.match(commit):
        sys.exit("--commit takes a full 40-hex commit SHA, never a branch or tag: a name moves, a SHA does not")
    with tempfile.TemporaryDirectory() as tmp:
        stage = pathlib.Path(tmp) / "stage"
        stage.mkdir()
        for path in VENDORED:
            with _get(RAW.format(commit=commit, path=path)) as r:
                (stage / path).write_bytes(r.read())
        with _get(RAW.format(commit=commit, path=DEPLOY_SCRIPT)) as r:
            scripts = compose_scripts(r.read().decode())
        (stage / "compose").mkdir()
        for path, text in scripts.items():
            (stage / path).write_text(text)
        images = raw_images(stage)
        services = {name: classify(img) for name, img in images.items()}
        cache = {}
        for name, e in sorted(services.items()):
            if e["kind"] == "unversioned":
                continue
            key = (e["registry"], e["repository"], e["tag"])
            if key not in cache:
                cache[key] = resolve(dict(e))
            e.update({k: cache[key][k] for k in ("digest", "revision") if k in cache[key]})
        built = sorted({e["revision"] for e in services.values() if e["kind"] == "posthog-built" and "revision" in e})
        nameless = [n for n, e in services.items() if e["kind"] == "posthog-built" and "revision" not in e]
        if nameless:
            sys.exit(f"refused: no revision recorded for PostHog-built image(s) {sorted(nameless)}, so ancestry cannot be shown")
        ancestry, date = history(commit, built, str(pathlib.Path(tmp) / "git"))
        late = sorted(n for n, e in services.items() if e["kind"] == "posthog-built" and not ancestry[e["revision"]])
        if late:
            sys.exit(f"refused: the current build of {late} is not from {commit} or an ancestor of it. "
                     "Choose a newer commit (normally the current master) and pin again.")
        for e in services.values():
            if e["kind"] == "posthog-built":
                e["revision_is_ancestor"] = True
        record = {
            "commit": commit, "commit_date": date, "upstream": UPSTREAM_REPO,
            "vendored": {p: sha256(stage / p) for p in VENDORED + COMPOSE_SCRIPTS},
            "image_vars": {k: v for k, v in IMAGE_VARS.items() if v is not None},
            "services": dict(sorted(services.items())),
        }
        old = json.loads(PIN_JSON.read_text()) if PIN_JSON.exists() else None
        (HERE / "compose").mkdir(exist_ok=True)
        for p in VENDORED + COMPOSE_SCRIPTS:
            (HERE / p).write_bytes((stage / p).read_bytes())
            if p in COMPOSE_SCRIPTS:
                os.chmod(HERE / p, 0o755)
    PIN_JSON.write_text(json.dumps(record, indent=2) + "\n")
    PIN_YML.write_text(pin_yml_text(record))
    if old:
        print(f"moved {old['commit']} -> {commit}")
        for name in sorted(set(old["services"]) | set(record["services"])):
            a, b = old["services"].get(name, {}), record["services"].get(name, {})
            if a.get("digest") != b.get("digest") or a.get("compose_image") != b.get("compose_image"):
                print(f"  {name}: {a.get('compose_image', '-')} {a.get('digest', '-')[:19]} -> {b.get('compose_image', '-')} {b.get('digest', '-')[:19]}")
    print(f"pinned PostHog {commit}: {sum(e['kind'] != 'unversioned' for e in record['services'].values())} images; "
          "now run the upgrade procedure of NP-ANALYTICS-001 §7.3")


def check_problems(directory=HERE, images=None):
    directory = pathlib.Path(directory)
    out = []
    try:
        record = json.loads((directory / "posthog-pin.json").read_text())
    except (OSError, ValueError) as e:
        return [f"PIN1 posthog-pin.json is missing or not JSON: {e}"]
    if not SHA40.match(record.get("commit", "")):
        out.append(f"PIN1 the pin names no 40-hex commit ({record.get('commit')!r})")
    for p in VENDORED + COMPOSE_SCRIPTS:
        want = record.get("vendored", {}).get(p)
        try:
            got = sha256(directory / p)
        except OSError:
            out.append(f"PIN2 {p} is missing")
            continue
        if got != want:
            out.append(f"PIN2 {p} differs from the file vendored at the pin (edited by hand, or re-vendored without a re-pin)")
    services = record.get("services", {})
    if images is None:
        try:
            images = raw_images(directory)
        except RuntimeError as e:
            return out + [f"PIN3 {e}"]
    for name, img in sorted(images.items()):
        e = services.get(name)
        if e is None:
            out.append(f"PIN3 service {name} has no pin")
            continue
        if e.get("compose_image") != img:
            out.append(f"PIN3 {name}: the hobby file writes '{img}', the pin was made for '{e.get('compose_image')}'")
        if e.get("kind") != "unversioned" and not DIGEST.match(e.get("digest", "")):
            out.append(f"PIN4 {name}: no sha256 digest")
        if e.get("kind") == "versioned":
            want = classify(img)
            if (e.get("repository"), e.get("tag")) != (want["repository"], want["tag"]):
                out.append(f"PIN5 {name}: pinned {e.get('repository')}:{e.get('tag')}, the commit names {want['repository']}:{want['tag']}")
        if e.get("kind") == "posthog-built":
            if not SHA40.match(e.get("revision", "")) or e.get("revision_is_ancestor") is not True:
                out.append(f"PIN6 {name}: PostHog-built image with no revision shown to be the commit or an ancestor")
        if e.get("kind") != classify(img)["kind"]:
            out.append(f"PIN5 {name}: recorded as {e.get('kind')}, the hobby file's image is {classify(img)['kind']}")
    for name in sorted(set(services) - set(images)):
        out.append(f"PIN3 the pin names {name}, which the hobby file does not have")
    try:
        if (directory / "docker-compose.pin.yml").read_text() != pin_yml_text(record):
            out.append("PIN7 docker-compose.pin.yml is not what posthog-pin.json generates (edited by hand?)")
    except (OSError, KeyError) as e:
        out.append(f"PIN7 docker-compose.pin.yml cannot be checked: {e}")
    return out


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--commit", help="the PostHog commit (40-hex SHA) to move the whole stack to")
    g.add_argument("--check", action="store_true", help="verify the pin offline")
    g.add_argument("--fetch-tree", action="store_true", help="check out the PostHog files the stack mounts, at the pinned commit")
    a = ap.parse_args(argv)
    if a.commit:
        do_pin(a.commit)
        return 0
    if a.fetch_tree:
        do_fetch_tree()
        return 0
    problems = check_problems()
    for p in problems:
        print("FAIL " + p, file=sys.stderr)
    print("posthog pin: " + (f"{len(problems)} problem(s)" if problems else "holds"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
