# NeurOne Operations Manual

**Project:** NeurOne  
**Document:** NP-MAN-OPS-001  
**Revision:** 2
**Date:** 2026-11-27  
**Status:** DRAFT  
**Effective Date:** —  
**Author:** NeurOne Software Engineering  
**Approved By:** — (new document)  
**References:** NP-ANALYTICS-001, NP-APP-TELEMETRY-001, NP-INFRA-001  
**Related Issues:** —  
**Gate:** —  
**IEC 62304 Class:** —

---

## 1. Purpose and scope

This is the manual for the people who **run** the services NeurOne operates, as opposed to the people who design them. It holds the steps to configure a service, to check that it is configured, and to keep it that way. It is procedure, not specification: a figure here is a value to set or a result to expect, and the reason for it is in the document it cites.

**One chapter so far:**

| Chapter | Service | Specification it implements |
|---------|---------|-----------------------------|
| §3 to §7 | Self-hosted PostHog analytics (usage telemetry only) | `NP-ANALYTICS-001`; policy `NP-APP-TELEMETRY-001` |

The other services (the SHDR fleet database, OTA firmware distribution, the consumable shop) are in `NP-INFRA-001` and join this manual when their operating steps are written down. This manual adds to `NP-ANALYTICS-001` and does not replace it: that document says what the stack is and why; this one says what to do, in order, and what to write down.

**What PostHog holds here, and what it does not.** Product-usage events, each tagged with a random anonymous ID. It holds no UHDR (User Health Data Record) and no SHDR (System Health Data Record), and the ID is linked to no account (`NP-APP-TELEMETRY-001`). **No host emits an analytics event today** (nothing calls `track`, `OI-ANALYTICS-01`), so a stack that has never received an event has nothing to retain or erase; the checks below use probe events the checks themselves write.

## 2. Before you start

| You need | Why |
|----------|-----|
| A host with Docker and the `docker compose` plugin | The stack is `infra/posthog/docker-compose.yml` |
| A checkout of this repository at the revision you are deploying | The scripts named below are in `infra/posthog/` and `ci/` |
| `infra/posthog/.env`, made from `.env.example` | Every secret and setting; **never committed** (it is git-ignored) |
| A PostHog personal API key, in the shell as `POSTHOG_PERSONAL_API_KEY` | Only `apply_project_settings.py` uses it. Create it under your PostHog user settings. Never pass it as an argument: arguments show in process listings |
| Python 3.10 or later on the machine that runs the scripts | `apply_project_settings.py` and `apply_event_retention.py` are standard-library only |
| A written change log for the deployment | Several steps leave no trace in the stack. The log is the only evidence they were done (§7) |

**Run the scripts from the checkout, against the running stack.** ClickHouse is on the internal compose network only and is not published, so a script on another machine cannot reach it. Either run it in a container on that network (as §4 step 5 does for the relay) or open a shell with `docker compose exec` and use the container's own tools. Do not publish ClickHouse's port to make a script work: `ci/test_posthog_infra.py` (P4, P5) fails a compose file that does.

## 3. Configure PostHog

Do these once for a new stack, and again for a stack that has been recreated, in this order.

1. **Secrets and version.** `cd infra/posthog && cp .env.example .env`. Fill `SECRET_KEY`, `POSTGRES_PASSWORD`, `CLICKHOUSE_PASSWORD` and `ERASE_CLICKHOUSE_PASSWORD` (four different values, generated as `.env.example` shows). Set `POSTHOG_VERSION` to one release tag. The stack refuses to start while it is empty, and `latest` is not a pin.
2. **Retention.** Leave `EVENT_RETENTION_DAYS` at **30** unless the principal has decided otherwise. The reason is in `.env.example` and `NP-ANALYTICS-001` §7.1: `NP-APP-TELEMETRY-001` §5 promises that a withdrawn person's analytics ID is deleted from the vendor within 30 days, and a value above 30 breaks that promise for each day over.
3. **Start.** `docker compose up -d`, then `docker compose logs -f posthog` until it serves. The first start runs database migrations and takes several minutes. For the HTTPS profile (the erasure relay and the TLS proxy run in it), `docker compose --profile https up -d`.
4. **Organization and project.** `NP-ANALYTICS-001` §3.3: an organization named "NeurOne", a project named "NeurOne App". Note the project number (the `<id>` in `/api/projects/<id>/`) and copy the Project API Key into the build's `POSTHOG_PROJECT_TOKEN`.
5. **Project privacy settings, by script.** From the checkout:
   ```bash
   python3 infra/posthog/apply_project_settings.py --host https://<analytics host> --project-id <id> --apply
   python3 infra/posthog/apply_project_settings.py --host https://<analytics host> --project-id <id> --check
   ```
   Exit 0 on `--check` means the project keeps no client address (`anonymize_ips`), records no sessions and captures nothing automatically. Exit 1 names the setting that differs; exit 2 means it could not be read. **Treat 2 as not met.**
6. **Event retention, by script.** Details and exit codes: the docstring of `apply_event_retention.py`.
   ```bash
   python3 infra/posthog/apply_event_retention.py --url http://<clickhouse> --apply
   python3 infra/posthog/apply_event_retention.py --url http://<clickhouse> --check
   ```
   (It reads `RETENTION_CLICKHOUSE_PASSWORD` and `EVENT_RETENTION_DAYS` from the environment; export both from `.env`. It runs as its own ClickHouse user, `np_retention`, and refuses to run as `posthog`: `NP-ANALYTICS-001` §7.7. Do not give it PostHog's `CLICKHOUSE_PASSWORD`.)
7. **Person retention, by hand** (`NP-ANALYTICS-001` §3.4, the note under the retention table). Nothing checks it, so record it in the change log.
8. **Heatmaps, web analytics and the Property Filter app** (`NP-ANALYTICS-001` §3.4 and §4). Not covered by any script; do them by hand and verify the denylist as `NP-ANALYTICS-001` §4.4 says.

## 4. The real-stack check

**Why this exists.** Every script above was tested only against a fake. The erasure relay, the retention TTL and the project-settings script each assume a PostHog table name, a field name or a grant that has never been confirmed on a real stack for the pinned `POSTHOG_VERSION` (`OI-ANALYTICS-01` (2), `OI-ANALYTICS-02`). **Until each step below has passed on a real stack, no ID can be called erased and retention is a wish, not a control** (`NP-ANALYTICS-001` §3.4). This section is the process for finding out.

**Rules for running it.**
- **Use a stack with no real person's data**, or accept that a failure of step 4 or 5 could leave a probe row behind. Every step writes only rows under a throwaway random ID, never under an ID a phone is using.
- **Do the steps in order.** Each needs the one before it. A step that fails stops the run: fix the cause, then start again from that step.
- **Write down the result of every step** in the record of §7, including a failure. A step with no entry did not happen.
- **Never edit a script or a check to make a step pass.** A failure means the stack or the assumption is wrong. If an assumption is wrong (a different table name, a field PostHog renamed), that is a change to the script and its tests, and a new revision of `NP-ANALYTICS-001`.
- All commands run from `infra/posthog/`.

### Step 1 · The stack is the one you think it is

```bash
docker compose ps                                  # every service running or healthy
curl -fsS http://localhost:8000/_health            # {"status": "ok"}   (adjust the port to POSTHOG_PORT)
grep '^POSTHOG_VERSION=' .env                      # a release tag, not empty, not "latest"
docker compose exec clickhouse clickhouse-client --user posthog --password "$CLICKHOUSE_PASSWORD" \
  --query "SELECT version()"
```

**Pass:** all services up, health `ok`, a pinned tag. **Record:** the tag and ClickHouse's version. Every later result is true for that pair only.

### Step 2 · The events table is the table the scripts name

The relay deletes from, and the retention script puts a TTL on, `posthog.sharded_events`. Confirm it exists and is the table events land in.

```bash
docker compose exec clickhouse clickhouse-client --user posthog --password "$CLICKHOUSE_PASSWORD" --query \
  "SELECT database, name, engine FROM system.tables WHERE name LIKE '%events%' AND database = 'posthog'"
docker compose exec clickhouse clickhouse-client --user posthog --password "$CLICKHOUSE_PASSWORD" --query \
  "SHOW CREATE TABLE posthog.sharded_events"
```

**Pass:** `posthog.sharded_events` is listed, and its columns include `uuid`, `event`, `distinct_id`, `team_id` and `timestamp` (the probe in step 4 inserts exactly those). **If it is not there:** find the table the project's events are in (send one event from a test client and `SELECT count() FROM posthog.<candidate>`), and stop. The table name is then wrong in `erase_relay.py`, `apply_event_retention.py` and `ERASE_TABLES` and must be changed in all three, with their tests, before going on. **Also record:** whether PostHog's own distributed `posthog.events` table fronts it. A mutation or TTL on the distributed table alone does nothing to the data.

### Step 3 · Project settings take

```bash
POSTHOG_PERSONAL_API_KEY=... python3 apply_project_settings.py --host https://<host> --project-id <id> --check
```

**Pass:** exit 0, and `--apply` followed by `--check` is also 0 (the read-back is what proves the server did not silently ignore a field). **If it exits 2 with "not in the server's answer":** the field name differs in this PostHog version; do not work around it, record the version and raise it against `OI-ANALYTICS-02`.

### Step 4 · The retention TTL takes, and actually deletes

```bash
python3 apply_event_retention.py --url http://<clickhouse> --apply     # exit 0
python3 apply_event_retention.py --url http://<clickhouse> --check     # exit 0
```

Then show that ClickHouse really removes an old row. Use a throwaway ID, a timestamp older than the retention, and a forced merge:

```bash
PROBE=$(python3 -c "import uuid; print(uuid.uuid4())")
CH="docker compose exec -T clickhouse clickhouse-client --user posthog --password $CLICKHOUSE_PASSWORD"
$CH --param_id "$PROBE" --query "INSERT INTO posthog.sharded_events (uuid, event, distinct_id, team_id, timestamp) VALUES (generateUUIDv4(), 'np_retention_probe', {id:String}, 0, now() - INTERVAL 40 DAY)"
$CH --param_id "$PROBE" --query "SELECT count() FROM posthog.sharded_events WHERE distinct_id = {id:String}"   # 1 or 0, see below
$CH --query "OPTIMIZE TABLE posthog.sharded_events FINAL"
$CH --param_id "$PROBE" --query "SELECT count() FROM posthog.sharded_events WHERE distinct_id = {id:String}"   # must be 0
```

(The 40 days must be more than `EVENT_RETENTION_DAYS`; use a larger number if you changed it.)

**Pass:** both script runs exit 0 and the last count is 0. A count of 0 *before* the merge is also fine: ClickHouse may have dropped the row at insert. **If the count stays 1 after the forced merge:** the TTL is present but not deleting (for example `ttl_only_drop_parts` is set, or the table is replicated and the merge ran on another replica). Record it and stop: retention is not a control on this stack. **Record:** the TTL text from `SHOW CREATE TABLE` after the `--apply`.

### Step 5 · The erasure relay deletes

Set up a user that may insert and select, for this check only. The simplest is the `posthog` administrator already in `.env`:

```bash
docker compose --profile https run --rm --use-aliases \
  -e ERASE_CHECK_CLICKHOUSE_USER=posthog -e ERASE_CHECK_CLICKHOUSE_PASSWORD="$CLICKHOUSE_PASSWORD" \
  erase-relay python /app/erase_relay.py --self-check
```

The check writes a probe event under a throwaway ID, erases it through the relay's own code, and reads it back.

**Pass:** it prints `self-check: 1 probe event(s) written, 0 left after the erase` and exits 0. This confirms the table name, the `np_erase` user's grant and the mutation for this version. **If it fails with a permission error:** `clickhouse/np-erase-user.xml` is not loaded or does not grant `ALTER DELETE` on the table; check that ClickHouse mounted it (`docker compose exec clickhouse ls /etc/clickhouse-server/users.d/`). **If it fails with an authentication error for `np_erase`:** ClickHouse admits that user only from an address the name `erase-relay` resolves to (`OI-PRIV-50`); check that the command carries `--use-aliases`, which gives the check's container that name. **If the count after is not 0:** a mutation can lag by seconds on a loaded store; the check waits 30 seconds, so a persisting row is a real failure.

### Step 6 · The relay answers as the app expects, through the proxy

Step 5 tested the code. This tests the route a phone uses. Insert a probe stamped **now** (not 40 days ago: the retention TTL of step 4 would remove an old row before the relay was tried), then call the relay through Caddy:

```bash
PROBE=$(python3 -c "import uuid; print(uuid.uuid4())")
$CH --param_id "$PROBE" --query "INSERT INTO posthog.sharded_events (uuid, event, distinct_id, team_id, timestamp) VALUES (generateUUIDv4(), 'np_erase_probe', {id:String}, 0, now())"
```

(`CH` is defined in step 4.)

```bash
curl -sS -o /dev/null -w '%{http_code}\n' -X POST https://<analytics host>/np/erase \
  -H 'Content-Type: application/json' -d "{\"distinct_id\":\"$PROBE\"}"          # 200
curl -sS -o /dev/null -w '%{http_code}\n' -X POST https://<analytics host>/np/erase \
  -H 'Content-Type: application/json' -d '{"distinct_id":"not-valid"}'           # 400
curl -sS -o /dev/null -w '%{http_code}\n' https://<analytics host>/np/erase      # 405 (a GET)
```

Then `SELECT count()` for the probe as in step 4: **0**.

**Pass:** 200, 400 and 405 as shown, and the probe is gone. These are the answers `AnalyticsErasureQueue` depends on (`REQ-SW03-19` (b)): a proxy that answers `404` for the route, or `401` in front of it, is a misdeployment and loses nothing, but the relay then does nothing, so the step fails. **Also check, once:** that neither Caddy's access log nor the relay's output contains the probe ID or a client address (`docker compose logs caddy erase-relay | grep "$PROBE"` must print nothing).

### Step 7 · What the mutation leaves behind

`NP-ANALYTICS-001` §7.6 records an unverified residue: ClickHouse keeps the text of a finished mutation, which can include the erased ID.

```bash
$CH --query "SELECT command, is_done, create_time FROM system.mutations WHERE table = 'sharded_events' ORDER BY create_time DESC LIMIT 5"
```

**Record:** whether the probe ID from step 6 appears in `command`, and how long a finished mutation stays listed. `system.mutations` is a view over the table's live mutation files, so look at when the oldest finished row's `create_time` is and whether it grows without bound; run the query again after an upgrade or restart and note whether finished rows survive. This step has no pass or fail: its result is the answer to "does the list this feature exists to shorten keep the IDs, and for how long", and it goes in `NP-ANALYTICS-001` §7.6 in place of "not verified".

### Step 8 · It all still holds after an upgrade

A PostHog upgrade can recreate the events table, which drops its TTL, or rename it, which breaks the relay. After any change to `POSTHOG_VERSION` (and the first time, by rehearsing one on a copy), repeat steps 1 to 6. `apply_event_retention.py --check` is the quick test for the TTL.

## 5. Maintain PostHog

None of these has a calendar interval: each is tied to an event or to something you can measure. A step that would need a fixed interval to mean anything has none until someone derives one (`CLAUDE.md` §19).

| When | Do |
|------|----|
| **Every time the stack is started or restarted** | `docker compose ps`, then `curl /_health`. Both must be clean. |
| **Before and after changing `POSTHOG_VERSION`** | Back up (below). Change the tag, `docker compose pull && docker compose up -d`, watch `docker compose logs -f posthog` until it serves, then run steps 1 to 6 of §4. Do not leave an upgraded stack unchecked: until step 4 and step 5 pass, the 30-day promise is not kept. |
| **When `EVENT_RETENTION_DAYS` changes** | `apply_event_retention.py --apply`, then `--check`; repeat the person-retention step by hand (§3 step 7); write both in the change log. A value above 30 prints the policy warning: that decision is the principal's. |
| **When the project or the stack is recreated** | All of §3, then all of §4. A recreated project starts with PostHog's defaults. |
| **When a disk alarm or `df` shows the volumes filling** | `NP-ANALYTICS-001` §7.5 has the expected usage. At 30-day retention the event storage is about a third of the 90-day figure. Check that step 4's TTL is still present before adding disk. |
| **When a right-to-erasure request reaches you** | A person's own withdrawal needs nothing from you: the app sends the ID to the relay. For a request made another way, `NP-ANALYTICS-001` §7.2. |
| **When a key is suspected exposed** | Rotate `SECRET_KEY`, `POSTGRES_PASSWORD`, `CLICKHOUSE_PASSWORD` or `ERASE_CLICKHOUSE_PASSWORD` in `.env`, then `docker compose up -d`. A rotated ClickHouse password also needs `CLICKHOUSE_PASSWORD` re-exported for the scripts. The relay's `np_erase` password is the only one the internet-facing relay holds. |

**Backups** (`NP-ANALYTICS-001` §7.4): both volumes, PostgreSQL by `pg_dump` and ClickHouse by `BACKUP DATABASE`. **A backup keeps what retention and the relay delete.** A restored backup brings back events older than the TTL and IDs a person had erased, so: restore only into a stack that will then run step 4's `--apply`, and keep backups no longer than the retention. Store them apart from any UHDR backup, with their own access control.

**What does not need doing:** the relay holds no state to maintain (its counters are in memory), Kafka's 24-hour log retention is set in the compose file, and ClickHouse's system-log TTLs are in `clickhouse/np-privacy.xml`.

## 6. When something fails

| Symptom | Likely cause | What to do |
|---------|--------------|------------|
| `docker compose up` stops with "Set POSTHOG_VERSION in .env" | The version is empty | Set it to a release tag. It never defaults. |
| `apply_project_settings.py` exits 2, "refused with HTTP 401/403" | The personal API key is wrong or lacks the project | Create a key in PostHog under the user that owns the project |
| `apply_project_settings.py` exits 2, "not in the server's answer" | This PostHog version names the field differently | Record the version, raise it against `OI-ANALYTICS-02`. Do not edit the script to skip the field. |
| `apply_event_retention.py` exits 2, "was not found in the store" | The events table is not `posthog.sharded_events` | §4 step 2 |
| `apply_event_retention.py --check` exits 1 after it passed | An upgrade recreated the table | `--apply`, `--check`, then §4 steps 4 to 6 |
| The relay answers `502` | The store refused the mutation (wrong grant, ClickHouse down) | The app keeps the ID and asks again at its next launch, so nothing is lost. Fix the grant (§4 step 5) and watch the relay's counters at shutdown. |
| The relay answers `429` | A flood, or a burst of withdrawals | The app retries at its next launch. Raise `ERASE_RATE_PER_SECOND` only if real traffic needs it. |
| The relay answers `404` or `401` | Caddy is not routing `/np/erase`, or something in front is refusing it | The app keeps every ID on these (`REQ-SW03-19` (b)), so nothing is lost, but nothing is erased either. Fix the route, then §4 step 6. |
| A person says they withdrew and want proof | There is none to give by design: the relay keeps no log of IDs | The ID is on their phone only until the relay accepts it, and after the erase it is on nobody's. Say that (`NP-APP-TELEMETRY-001` §5). |

## 7. The record

Copy this table into the deployment's change log for each run of §4. **A blank line is a step not done.**

| Step | Date | Run by | Result (pass, or what failed) | Evidence (the output that shows it) |
|------|------|--------|-------------------------------|-------------------------------------|
| 1 stack, with `POSTHOG_VERSION` and ClickHouse version | | | | |
| 2 events table is `posthog.sharded_events` | | | | |
| 3 project settings take | | | | |
| 4 retention TTL takes and deletes | | | | |
| 5 relay `--self-check` | | | | |
| 6 relay through the proxy, and the logs hold no ID | | | | |
| 7 what the mutation leaves behind | | | | |
| 8 repeated after an upgrade | | | | |
| Person retention set by hand (§3 step 7) | | | | |

**When steps 1 to 6 have passed on a real stack,** the person who ran them closes the loop in the repository, in one change: `OI-ANALYTICS-01` (2) and `OI-ANALYTICS-02` in `docs/status/pending-decisions.md` (say which version it was true for), the "not verified" and "NOT YET EXERCISED" notes in `NP-ANALYTICS-001` §3.4 and §7.6, and in the docstrings of `erase_relay.py`, `apply_event_retention.py` and `apply_project_settings.py`, and the step-7 answer in `NP-ANALYTICS-001` §7.6. Only then may an ID be called erased and retention called a control. **A step that fails is not closed by this manual:** it becomes an open item against the document that owns the assumption.

## 8. Related documents

| Document | What it is for here |
|----------|---------------------|
| `NP-ANALYTICS-001` | The stack's specification: architecture, privacy settings, denylist, relay contract |
| `NP-APP-TELEMETRY-001` | The telemetry policy: what may be collected, and the 30-day deletion promise |
| `NP-INFRA-001` | The other services NeurOne operates, and the choice between PostHog cloud and self-hosted |
| `NP-SW-001` | `REQ-SW03-19`, `-21`, `-22` and `-34`: the app's queue, the settings script, the relay and the retention script |
| `docs/status/pending-decisions.md` | `OI-ANALYTICS-01`, `OI-ANALYTICS-02` |

---

## Document history

| Rev | Date | Author | Change |
|-----|------|--------|--------|
| 1 | 2026-11-20 | NeurOne Software Engineering | New document. PostHog configuration, the real-stack check for `OI-ANALYTICS-01` (2), maintenance and failure handling. Not yet exercised: every command in §4 is unrun. |
| 2 | 2026-11-27 | NeurOne Software Engineering | **§4 step 5: the relay self-check runs with --use-aliases, because ClickHouse admits np_erase only from an address the name erase-relay resolves to (OI-PRIV-50); what an authentication error means.** |
