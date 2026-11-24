# NeurOne PostHog Analytics — Self-Hosted Configuration Reference

**Project:** NeurOne  
**Document:** NP-ANALYTICS-001  
**Revision:** 9
**Date:** 2026-11-21  
**Status:** ACTIVE  
**Effective Date:** 2026-06-13  
**Author:** Steve Hickman (CEO, interim Quality authority)  
**Approved By:** Steve Hickman, CEO  
**References:** —  
**Related Issues:** —  
**Gate:** —  
**IEC 62304 Class:** —

---

> **Deployment option context:** NeurOne supports two PostHog deployment paths.
> The iOS app (`PostHogAnalyticsBackend.swift`) currently points to the **PostHog EU cloud**
> (`eu.i.posthog.com`) as the fastest path to TestFlight beta. This document covers the
> **self-hosted** alternative — preferred for T1 launch and required for T2 clinical cloud
> (eliminates the third-party analytics DPA). For EU cloud setup, see
> `docs/np_infra_001.md §6`. For the decision matrix, see `docs/np_infra_001.md §6.1`.

---

## 1. Purpose and scope

This document is the configuration reference for NeurOne's self-hosted PostHog analytics instance. It covers:

- Privacy constraints and how each is enforced
- First-time setup and required UI-level project settings
- The server-side property denylist and how to modify it
- HTTPS configuration for device testing and production
- Upgrading, backup, and data management
- Prohibited properties reference
- Relationship to other NP compliance documents

PostHog replaces the need for a third-party analytics vendor, eliminating the DPA/BAA requirement with an analytics provider, simplifying `PrivacyInfo.xcprivacy`, and giving NeurOne full control over data residency.

---

## 2. Architecture

```
iOS App
  │  PostHog iOS SDK (explicit events only, no autocapture)
  │  SDK posts to: http://localhost:8000/capture  (or HTTPS in production)
  ▼
┌─────────────────────────────────────────────────────┐
│  PostHog web server  (Django/Gunicorn, port 8000)   │
│  PostHog plugin server  ← NP property denylist runs │
│  PostHog Celery worker  ← GDPR erasure tasks here   │
│  Kafka  ─→  ClickHouse  (analytics event store)     │
│  PostgreSQL  (metadata: projects, flags, cohorts)    │
│  Redis  (cache, task queue)                          │
│  ZooKeeper  (ClickHouse coordination)                │
└─────────────────────────────────────────────────────┘
```

**What is and isn't in this stack:**

| Component | Present | Reason |
|-----------|---------|--------|
| MinIO (session recording storage) | No | Session recording is disabled; OBJECT_STORAGE_ENABLED=false |
| GeoIP database (MaxMind) | No | DISABLE_MMDB=true |
| PostHog telemetry to posthog.com | No | CAPTURE_INTERNAL_METRICS=false |
| Caddy HTTPS proxy | Optional | Activated with `--profile https`; required for physical devices |
| ClickHouse replication | No | Single-node; ZooKeeper still required for PostHog schema compatibility |

---

## 3. First-time setup

### 3.1 Generate secrets

```bash
cd infra/posthog
cp .env.example .env

# Generate SECRET_KEY
python3 -c "import secrets; print(secrets.token_hex(32))"
# → paste into .env as SECRET_KEY=

# Generate POSTGRES_PASSWORD and CLICKHOUSE_PASSWORD (use different values)
python3 -c "import secrets; print(secrets.token_hex(16))"
```

### 3.2 Start the stack

```bash
cd infra/posthog
docker compose up -d

# Watch for PostHog to finish migrating the database and start serving
docker compose logs -f posthog
# Ready when you see: "Application startup complete" or similar
# First start takes 2-5 minutes (database migrations)
```

### 3.3 Create the organization and project

1. Open `http://localhost:8000`
2. Complete the PostHog signup flow (creates the first admin user)
3. Create an organization named **"NeurOne"**
4. Create a project named **"NeurOne App"**
5. Note the **Project API Key** shown at the end — this is what the iOS SDK uses

### 3.4 Required project-level privacy settings (UI)

Three of these settings are scripted (Rev 3, privacy review Finding 4): `infra/posthog/apply_project_settings.py --check` reads the project's `anonymize_ips`, `session_recording_opt_in` and `autocapture_opt_out` and exits non-zero if one differs, and `--apply` sets them and reads them back (`POSTHOG_PERSONAL_API_KEY` from the environment). **The script has not been run against a real PostHog instance** (`OI-ANALYTICS-02`); run `--check` against yours before relying on it. Heatmaps, web analytics, event and person retention, and the Property Filter transformation are not covered and stay manual. They are not configurable via environment variables. Complete them immediately after project creation.

**The network address (Rev 5; `NP-APP-TELEMETRY-001` §4.1, `OI-PRIV-05` (5)).** The principal decided on 2026-11-19 that analytics events are to reach this host through an Oblivious HTTP relay, so that it never receives the sender's address; that is not built, and nothing in this stack provides it yet. Until it is, the floor is the proxy's: `caddy/Caddyfile` removes every header a proxy can carry the address in (`X-Forwarded-For`, `X-Real-IP`, `Forwarded`, `True-Client-IP`, `CF-Connecting-IP`, `X-Client-IP`, `X-Cluster-Client-IP`, `Fastly-Client-IP`, `Via`) before PostHog and before the erasure relay, and neither of its logs keeps the client's address, its port or any request header (`OI-PRIV-51`; before 2026-11-21 only `X-Forwarded-For` and `X-Real-IP` were removed, and the logs deleted only those two headers). `ci/test_posthog_infra.py` checks the header removal (P6), the log filter on both logs (P8) and that Caddy accepts the file (P9). The setting below is a second layer behind that floor.

**Navigate to: Project Settings → Privacy**

| Setting | Value | Why |
|---------|-------|-----|
| IP address collection | **Mask full IP address** | No IP data stored even though DISABLE_MMDB already prevents geolocation |
| Session recording | **Disabled** | Belt-and-suspenders — OBJECT_STORAGE_ENABLED=false already makes recording non-functional |
| Person display name | Leave as default | n/a |

**Navigate to: Project Settings → Data Management → Data Retention**

| Setting | Value | Why |
|---------|-------|-----|
| Event retention | **30 days** (`EVENT_RETENTION_DAYS`, §7.1) | `NP-APP-TELEMETRY-001` §5: a withdrawn person's analytics ID is deleted from the vendor within 30 days. The principal set 30 on 2026-11-20 (`OI-ANALYTICS-01` (3)). Enforced by `infra/posthog/apply_event_retention.py`, not by the PostHog UI. |
| Person retention | **30 days** | Anonymous person records only; matches event retention. **Manual UI step, no script and no check:** procedure in the note below. |

> **Retention is enforced by a ClickHouse TTL, not by the UI setting (`REQ-SW03-34`).** `apply_event_retention.py --apply` sets `TTL toDate(timestamp) + toIntervalDay(N)` on the events table and reads it back; `--check` verifies it, and must be run after every PostHog upgrade (a recreated table loses its TTL). It has not been run against a real stack (`OI-ANALYTICS-01` (2)). The person-retention row stays a manual UI step.

> **Manual step: person retention (no script covers it).** `apply_event_retention.py` sets the TTL on the *events* table only, and `apply_project_settings.py` has no field for person retention (no API field was confirmed), so this one is done by hand in PostHog and nothing checks it.
> 1. Open **Project Settings → Data Management → Data Retention**.
> 2. Set **Person retention** to the same number of days as `EVENT_RETENTION_DAYS` (30 by default). Do not set it longer: a person record outliving its events keeps the anonymous ID after the events are gone, which is the ID `NP-APP-TELEMETRY-001` §5 promises to delete within 30 days.
> 3. Save, and write the date and the value in the deployment's change log. That record is the only evidence the step was done.
> 4. **Repeat it, and re-check it, whenever `EVENT_RETENTION_DAYS` changes, after every PostHog upgrade, and when the project is recreated.** A recreated project starts with PostHog's default, not this value.
>
> The app sets `personProfiles = .never` (§7.6), so PostHog may hold no person record for an ID at all, and then this setting has nothing to delete. It is kept as a second line behind the events TTL and is not what meets the 30 days. As with event retention, a self-hosted PostHog may not enforce the setting (below), so a saved value is a record of intent, not proof of deletion.

> **Retention is unverified in PostHog itself (Rev 3).** The Data Retention settings above are what this document has always asked for, but I could not confirm from PostHog's documentation that a **self-hosted** instance enforces them at all: the retention feature may exist only on PostHog Cloud. Until it is shown to delete old events on this stack, treat the 90 days (and the 30 of `NP-APP-TELEMETRY-001` §5) as a wish, not a control (`OI-ANALYTICS-02`).

> **Note:** "Person" records in PostHog are anonymous identifiers (the `$device_id` the iOS SDK generates). They are not linked to a NeurOne account or any UHDR data. 30-day retention satisfies data minimisation.

> **Retention against the withdrawal rule (Rev 2).** `NP-APP-TELEMETRY-001` §5 says a withdrawn person's analytics ID is deleted from the vendor within **30 days**. With 90-day retention the events of a person who withdrew could stay for up to 90 days. **The principal decided on 2026-11-20 to set retention to 30 days** (`OI-ANALYTICS-01` (3)), which meets the 30 days whether or not the erasure relay of §7.6 works; the cost is trend data older than 30 days.

**Navigate to: Project Settings → Autocapture & Heatmaps**

| Setting | Value |
|---------|-------|
| Enable Autocapture | **Off** |
| Enable Heatmaps | **Off** |
| Enable Web Analytics | **Off** (this is for websites, not relevant) |

### 3.5 Copy the Project API Key

From Project Settings → Project API Key. This value goes into the iOS SDK configuration as `POSTHOG_API_KEY` in the app's configuration. It is **not a secret** — it is safe to commit to the iOS source code or include in the app binary. It only allows *writing* events to your PostHog instance; it cannot read data.

---

## 4. Server-side property denylist

The PostHog **Property Filter** app (a built-in PostHog app) runs in the plugin server and drops configured properties *before* events are written to ClickHouse. This is the server-side enforcement layer for NP-APP-TELEMETRY-001's prohibited property list.

### 4.1 Installing the Property Filter app

1. Navigate to: **Data Pipeline → Transformations → + New transformation**
2. Search for **"Property Filter"**
3. Click **Enable**
4. Configure as below

### 4.2 Denied properties (baseline — matches NP-APP-TELEMETRY-001)

Enter the following properties in the **Properties to filter out** field, one per line:

```
imp
impedance
impedance_flags
pass_flags
session_sequence
eeg
hrv
rmssd
ppg
coherence_raw
heart_rate
bpm
adaptation_event
adapt_trigger
session_timestamp
completedAt
epoch
rr_interval
```

**Also deny all PostHog default person properties that capture device fingerprints:**

```
$initial_referring_domain
$initial_current_url
$referrer
$referring_domain
$current_url
```

### 4.3 Adding a property to the denylist

When NP-APP-TELEMETRY-001 is updated to prohibit a new property:

1. Go to Data Pipeline → Transformations → Property Filter → Edit
2. Add the property name to the list
3. Click Save
4. The change takes effect immediately for new events; it does not retroactively purge existing ClickHouse data

If a property was already written to ClickHouse and must be purged, use the person/event deletion API (see §7.2).

### 4.4 Verifying the denylist

After installing the Property Filter, send a test event from the iOS simulator containing a prohibited property (e.g. `impedance: 100`). Then query PostHog:

1. Navigate to Activity → Live Events
2. Find the test event
3. Confirm the prohibited property is absent from the event properties panel

---

## 5. iOS SDK configuration

The iOS SDK must be configured to match these server-side controls. The iOS SDK integration is tracked in `app/ios/ISA.md` (ISC group: Analytics). The required SDK configuration is:

```swift
// PostHog SDK setup — call during app init, AFTER consent is obtained
// Never call before the consent flow completes (AUDIT-02 requirement)
let config = PostHogConfig(
    apiKey: "YOUR_PROJECT_API_KEY",   // from Project Settings
    host: "http://localhost:8000"      // or your production URL
)

// Disable all automatic data collection
config.captureApplicationLifecycleEvents = false
config.captureScreenViews = false
config.sessionReplay = false

// Send events in batches rather than immediately
config.flushAt = 20
config.flushIntervalSeconds = 30

PostHogSDK.shared.setup(config)
// Do NOT call .setup() here. Call it only after consent is confirmed:
// See AnalyticsGate in the iOS app — PostHogSDK.shared.setup() is called
// inside AnalyticsGate.enable(), which is wired to ConsentStore.
```

**Consent integration (AUDIT-02):**

The SDK must not be initialized until after the user completes the consent flow. `AnalyticsGate.enable()` calls `PostHogSDK.shared.setup(config)`. `AnalyticsGate.reset()` calls `PostHogSDK.shared.optOut()` and discards the client. This is already wired in the iOS app (see `AnalyticsGate.reset()` — NP-PRIV-ANALYSIS-002).

**Permitted events:**

Only the event names `NP-APP-TELEMETRY-001` §3.3 lists may be sent, and the list is enforced in the shared core (`events` in `common/npps/analytics-properties.json`, `REQ-SW03-07`; Rev 5, `OI-PRIV-05`). That section is the authority; this table repeats it:

| Event name | When fired | Permitted properties |
|------------|-----------|---------------------|
| `app_opened` | App foreground | Any of `NP-APP-TELEMETRY-001` §3 |
| `onboarding_step_completed` | Each onboarding screen | `onboarding_step` and any other of §3 |
| `screen_viewed` | One of the four screens of §3.1 | `screen_name` and any other of §3 |
| `feature_used` | Feature tap (non-health) | Any of §3. No property names the feature |
| `error_reported` | One of the app failures of §3.4 | `error_code` and any other of §3 |
| `app_crashed` | On next launch after crash | Any of §3 (no stack trace — crash traces are local-only) |

**`consent_granted` and `consent_revoked` are removed** (they were listed here through Rev 4): no event is named for a consent, a session or a clinician. **`step_name` and `feature_name` were never permitted properties**: `NP-APP-TELEMETRY-001` §3 names neither, so the core drops an event that carries one. The onboarding step is `onboarding_step`.

**Prohibited in all events:** any property in the NP-APP-TELEMETRY-001 prohibited list. The iOS SDK's `PostHogSDK.shared.capture()` must never be called with these properties. The server-side Property Filter (§4) is a backstop, not the primary control.

---

## 6. HTTPS setup

### 6.1 iOS Simulator (HTTP is sufficient)

The iOS Simulator runs on the Mac and shares its network stack. `http://localhost:8000` works without HTTPS because:
- The iOS Simulator is exempt from App Transport Security
- No physical network hops; data never leaves the Mac

### 6.2 Physical device testing on local network

Option A — HTTP with NSExceptionDomains (development builds only):

Add to `Info.plist`:
```xml
<key>NSAppTransportSecurity</key>
<dict>
    <key>NSExceptionDomains</key>
    <dict>
        <key>192.168.x.x</key>  <!-- your Mac's LAN IP -->
        <dict>
            <key>NSExceptionAllowsInsecureHTTPLoads</key>
            <true/>
        </dict>
    </dict>
</dict>
```

Remove this exception before any TestFlight or App Store build.

Option B — HTTPS with Caddy (recommended):

```bash
# Start with HTTPS profile
POSTHOG_HOST=localhost docker compose --profile https up -d

# Install Caddy's CA into the Mac Keychain
docker exec $(docker compose ps -q caddy) caddy trust

# Then in Keychain Access: find "Caddy Local Authority" → Get Info → Trust → Always Trust

# For physical devices: export the CA cert and install via AirDrop or email
# Settings → General → VPN & Device Management → Install Certificate
# Then: Settings → General → About → Certificate Trust Settings → Enable full trust
```

### 6.3 Production / EU data residency

For T2 clinical cloud (NP-PRIV-REM-001 STEP-19 — EU data residency):

1. Deploy the stack on an EU-region cloud instance (e.g. AWS eu-west-1, GCP europe-west1)
2. Point DNS: `analytics.yourdomain.example` → instance public IP
3. Ensure ports 80 and 443 are open inbound
4. Update `.env`:
   ```
   SITE_URL=https://analytics.yourdomain.example
   IS_BEHIND_PROXY=true
   POSTHOG_HOST=analytics.yourdomain.example
   ```
5. Start with HTTPS:
   ```bash
   docker compose --profile https up -d
   ```
6. Caddy obtains and auto-renews the Let's Encrypt certificate

**EU-specific:** All Docker volumes store data on the instance's local disk. Confirm with your cloud provider that local disks in the chosen region are indeed EU-resident (they are for the major providers in their EU regions, but verify for your specific instance type and any backup policies).

---

## 7. Data management

### 7.1 Adjusting event retention

Event retention is `EVENT_RETENTION_DAYS` in `.env` (default **30**), applied as a ClickHouse TTL by `infra/posthog/apply_event_retention.py` (`REQ-SW03-34`). It is not the PostHog UI slider, which a self-hosted instance may not enforce (§3.4).

**Why 30, and what changing it means.** `NP-APP-TELEMETRY-001` §5 promises that a person who withdraws research consent has their pseudonymous analytics ID deleted from the vendor within **30 days**. Retention is what keeps that true while the erasure relay is unverified (`OI-ANALYTICS-01`). **Raising the value above 30 breaks that promise for each day over 30**, and the script says so on stderr; that is the principal's decision, not a tuning choice. Lowering it only loses trend data.

To change:
- Set `EVENT_RETENTION_DAYS` in `.env`, then run `python3 apply_event_retention.py --url <clickhouse> --apply` and then `--check`
- A TTL removes rows when ClickHouse next merges the parts, not at the instant they expire
- Re-run `--check` after every PostHog upgrade

**Considerations for changing retention:**
- 30 days (the default): meets the telemetry policy's 30-day deletion promise, but loses trend data older than that
- Longer (e.g. 180 days): more trend visibility, but **breaks that promise** and carries higher storage and privacy liability
- **Never exceed 365 days** without a formal DPA review — this exceeds what NP-APP-TELEMETRY-001 currently justifies

### 7.2 Responding to a right-to-erasure request

PostHog supports person deletion via the API or UI. Since our PostHog persons are anonymous device IDs (not linked to named users), erasure requests would come via the NeurOne app's privacy controls rather than directly to PostHog.

If an erasure is required:

**Via UI:**
1. Navigate to Persons & Groups
2. Search by the anonymous `distinct_id` the iOS SDK used for that user
3. Click the person → Delete person + their events

**Via API:**
```bash
curl -X DELETE \
  "http://localhost:8000/api/projects/<project_id>/persons/<person_id>/?delete_events=true" \
  -H "Authorization: Bearer <personal_api_key>"
```

The `delete_events=true` flag ensures ClickHouse event records are also queued for deletion (processed asynchronously by the Celery worker).

### 7.3 Upgrading PostHog

1. Review the PostHog release notes: https://github.com/PostHog/posthog/releases
2. Update `POSTHOG_VERSION` in `.env` to the new release tag
3. Pull and restart:
   ```bash
   docker compose pull
   docker compose up -d
   ```
4. PostHog runs database migrations automatically on startup
5. Monitor logs: `docker compose logs -f posthog`

**Pinning versions:** always pin to a specific release tag (e.g. `release-1.43.0`) rather than `latest` once any data exists. Unpinned latest can introduce breaking schema migrations without warning.

### 7.4 Backups

PostHog's analytics data lives in:
- Docker volume `neurone-analytics_postgres_data` — PostgreSQL (metadata)
- Docker volume `neurone-analytics_clickhouse_data` — ClickHouse (events)

Back up both volumes regularly. For production:

```bash
# PostgreSQL backup (runs inside the db container)
docker compose exec db pg_dump -U posthog posthog | gzip > posthog-pg-$(date +%Y%m%d).sql.gz

# ClickHouse backup (use clickhouse-backup or native backup command)
docker compose exec clickhouse clickhouse-client \
  --user posthog --password "$CLICKHOUSE_PASSWORD" \
  --query "BACKUP DATABASE posthog TO File('/var/lib/clickhouse/backup/$(date +%Y%m%d)')"
```

**Privacy note:** backups of PostHog data contain only analytics event data (engagement_tier, feature_used, etc.) — no UHDR data. They are still internal NeurOne operational data and should be stored with appropriate access controls, not on the same storage as UHDR backups.

### 7.5 Disk usage

Expected steady-state disk usage at 90-day retention (the former default; 30 days needs about a third of the event storage) with ~10,000 MAU:
- ClickHouse data: 5–20 GB (events compress well in ClickHouse columnar storage)
- PostgreSQL data: <1 GB
- Kafka logs: <5 GB (24-hour retention, consumed quickly)

Monitor with: `docker compose exec clickhouse clickhouse-client -q "SELECT formatReadableSize(sum(bytes_on_disk)) FROM system.parts WHERE active"`

---

### 7.6 The erasure relay (built, not verified against a live stack, `OI-ANALYTICS-01`)

When a person withdraws research analytics the app sends their old pseudonymous analytics ID to a NeurOne service, the **erasure relay**, which holds the PostHog private API key (the key must never be in the app) and deletes what PostHog holds under that ID. The app side is built: the ID is queued on the phone and retried until the relay answers (`AnalyticsErasureQueue`, `REQ-SW03-19`). The relay is `infra/posthog/erase_relay.py` (`REQ-SW03-22`), run by the `erase-relay` service of the `https` compose profile behind Caddy's `/np/erase` route. **It has been tested only against a fake ClickHouse; until the `--self-check` below passes on a real stack, treat no ID as erased.**

What the app sends and expects:

| | |
|---|---|
| Request | `POST https://<analytics host>/np/erase`, `Content-Type: application/json`, body `{"distinct_id": "<the old ID>"}` and nothing else. No account, device name, token or consent record, and no `Authorization` header: the ID is a random value only the phone and PostHog know, so presenting it is the proof the request is the owner's. |
| `200`–`299` | Accepted. Erasing an ID PostHog does not hold must also answer `2xx`: the call is idempotent. The app drops the ID from its queue. |
| `4xx` other than `408` and `429` | Refused. **The app keeps the ID** and asks again at its next launch (it goes on to the next queued ID in the same drain). The queue holds the only copy, and a `401`, `403` or `404` is what a misdeployed relay or a proxy in front of it answers, so no refusal drops an ID (`OI-PRIV-08` (2), `REQ-SW03-19` (b)). The app queues only IDs of the shape the relay accepts, so a `400` from the relay itself is not expected. |
| `408`, `429`, `5xx`, no answer | Try later. The app keeps the ID and asks again at its next launch; the rest of the queue waits. |

What the relay does:

- **Deletes in ClickHouse, not through PostHog's API.** The app sets `personProfiles = .never`, so PostHog may hold no person record for the ID, and the person-deletion call of §7.2 needs one. The relay runs `ALTER TABLE posthog.sharded_events DELETE WHERE distinct_id = {id:String}` with the ID as a bound parameter. It connects as `np_erase`, a ClickHouse user that may `ALTER DELETE` on that table and do nothing else (`clickhouse/np-erase-user.xml`), so it holds no PostHog key at all.
- **Accepts exactly one shape.** A body that is one JSON object with the single key `distinct_id` and a 16–64 character value of letters, digits and hyphens; anything else is `400`. Over 1 KiB is `413`. A store that refuses or does not answer is `502`, so the app keeps the ID. One bucket for the service (5 a second, burst 20; `ERASE_RATE_PER_SECOND`, `ERASE_BURST`) answers `429`; the ID is a 122-bit random value, so guessing one is not the threat, flooding the store with mutations is.
- **Keeps nothing.** Neither the source address (Caddy strips it, the relay logs none) nor the ID is written anywhere; the only record is four counters (erased, refused, limited, failed) printed at shutdown.
- **Finishes within the policy's 30 days** only if the mutation runs; a ClickHouse mutation is asynchronous and the relay answers `200` when the store accepts it, not when the rows are gone.

**Decided, not built: a `202` for "queued"** (`OI-PRIV-48`, the principal 2026-11-24; GitHub #971). Today every accepted ID runs its own `ALTER … DELETE` at once, and any request, malformed or not, spends the one rate-limit bucket. The relay is to change in three ways:

- **Validate before the rate limit.** A malformed request is refused before it takes a token, so refused requests cannot starve real withdrawals.
- **Queue and batch.** An accepted ID is queued durably and answered **`202`**. The queue is deleted in batches (`distinct_id IN (…)`, or a lightweight `DELETE` where the pinned ClickHouse version has one), on a schedule still to be chosen. `200` keeps its meaning, "deleted, or there were none", and is given only once the ID's deletion has been carried out.
- **The queue holds pending IDs only.** An ID leaves the queue when its batch completes, and the relay keeps no list of erased IDs: that list is what erasure exists to remove. How the app's next ask for a queued ID is answered `200` (a re-ask, or a status call) is for the build.

**Order of release:** the app treats `202` as "keep the ID and go on to the next" and drops an ID only on `200`, and that app change ships **before** any relay answers `202`. Today's app drops on any `2xx` (`REQ-SW03-19` (b)), so a `202` sent to it would drop an ID the store still holds. `REQ-SW03-19` (b) and `REQ-SW03-22` (a) change with the build.

**Before relying on it** (`OI-ANALYTICS-01` (2)): the full procedure, with the retention and proxy checks around it and the record to keep, is `NP-MAN-OPS-001` §4. The relay's part is: on the real stack, with `ERASE_CHECK_CLICKHOUSE_USER` and `_PASSWORD` set to a user that may insert and select, run `docker compose --profile https run --rm erase-relay python /app/erase_relay.py --self-check`. It writes a probe event under a throwaway ID, erases it through the relay's own code and reads it back; exit 0 means the table name, the grant and the mutation are right for `POSTHOG_VERSION`. A PostHog upgrade can rename the table, so repeat it after §7.3.

**Known residues.** ClickHouse's `system.mutations` keeps the text of a finished mutation, so the erased ID can remain in that system table; it holds no event data, but it is the list the feature exists to shorten. Whether and how long it is kept is not verified. Replicas, backups (§7.4) and Kafka logs (24 hours) are not touched by the relay.

### 7.7 Who may read the analytics data, and the read log (`OI-PRIV-47`, decided by the principal 2026-11-24; GitHub #970)

The analytics store holds each installation's event timeline under its pseudonymous ID. NeurOne holds no link from that ID to a person, but a timeline per installation is still an internal read that needs an owner and a record. This section is the policy. **Only the erasure relay's own ClickHouse user (`np_erase`, §7.6) is built; everything else here is decided and not built.**

**Who holds an account.**

- A PostHog account is held only by the **Analytics Lead** role.
- The **CEO** approves each account before it is created.

**Separate users.**

- PostHog's application reaches ClickHouse as its own user, and nothing else uses that credential.
- Each script and each operator has its own ClickHouse user, with only the rights its task needs: `np_erase` may only delete (§7.6), and the retention script's user may only read `system.tables` and change the TTL (§7.1). An operator who needs to read data does so as a named user of their own, so the read log shows who.

**Every read is recorded.**

- **Reads through PostHog** are recorded by a sign-in proxy in front of PostHog's pages. It records the signed-in person and each page they view. ClickHouse cannot do this, because every PostHog query reaches it as PostHog's one application user. The capture endpoints the apps post to, and `/np/erase`, stay outside the proxy: they read nothing.
- **Reads directly in ClickHouse** are recorded per user, which the separate users make attributable.
- **What an entry holds:** who, when, which page or table, the number of rows returned, and a hash of the query. **No analytics ID and no query values.** A page whose path carries an ID (a person or event view) is recorded with the ID removed. Otherwise the log would be a second copy of the IDs that erasure (§7.6) cannot reach.

**Where the log is kept.**

- Off the analytics host, in write-once storage with a deletion lock (S3 Object Lock, GCS bucket lock or Azure immutable blob), owned by the **Privacy Lead**. The Analytics Lead and the CEO can neither edit nor delete it.
- In an EU region wherever EU users are in scope (§6.3).
- **Kept for 12 months, rolling, from each entry's creation, then deleted.** The lock is set to 12 months and a deletion rule follows it, so write-once does not become kept-forever. Source: CNIL recommendation 2021-122 on logging measures (14 October 2021), which recommends a rolling 6 months to 1 year and asks for a documented justification beyond it; the UK NCSC's *10 Steps: logging and monitoring* advises at least 6 months, because incidents can take months to detect. Checking these two citations against their text is tracked by `OI-PRIV-47`.

**Review.**

- The **Privacy Lead** reviews the log **at least quarterly**, and whenever an alert fires. The quarterly minimum is the principal's choice, with no external source (`UC-206`). What raises an alert is listed below.
- **The Privacy Lead's review records are a separate record from the log.** They say what was reviewed, when, and what was found, and they hold no analytics data. They are kept for **no more than 3 years**, the usual maximum for internal-control records in the same CNIL recommendation. **They are kept in a restricted folder in the company's document system** (decided by the principal 2026-11-24): only the Privacy Lead can write there and the CEO can read, version history stays on so an edit to a record shows, and a retention label deletes each record 3 years after it is created. They are kept out of this repository, whose readers are too many and whose history cannot delete at 3 years.

**Alerts** (decided by the principal 2026-11-24). Each trigger below is something that should never happen, so every occurrence raises an alert and none needs a threshold:

| # | Trigger | What it catches |
|---|---|---|
| A1 | A read by an identity not on the approved list | A PostHog account the CEO did not approve, or a ClickHouse user not in the list of users |
| A2 | PostHog's own ClickHouse credential used from anywhere but the PostHog container | Direct use of the application's credential, which the separate-users rule forbids |
| A3 | A refused query on an analytics table | A script or a person attempting more than their grant |
| A4 | A PostHog page reached without passing through the sign-in proxy | A read that would never reach the log |
| A5 | An account or permission created, changed or removed | A PostHog account, ClickHouse user or grant, checked against a CEO approval |
| A6 | One installation's events viewed on their own | Looking at a single pseudonymous timeline instead of aggregates: a single-installation page in PostHog (the page type survives the ID's removal), or a ClickHouse query filtering on `distinct_id` by any user but `np_erase` |
| A7 | An export or bulk download | Data leaving the store in bulk |
| A8 | The read log stops arriving | A stopped forwarder, which would silence every other alert; detected by a heartbeat entry that goes missing |
| A9 | The locked bucket's lock, retention or deletion rule changed, or a deletion attempted | Tampering with the audit trail |
| A10 | A privacy setting drifts | IP anonymisation off, session recording or autocapture on, or the event TTL changed; found by running `apply_project_settings.py --check` and `apply_event_retention.py --check` on a schedule |
| A11 | An erasure fails | The relay answered `502`, so a withdrawal's deletion is at risk (§7.6) |

**Routing** (decided by the principal 2026-11-24). A1, A2, A4, A8, A9 and A11 alert the Privacy Lead **immediately**: each means a control is failing or being bypassed. A3, A5, A6, A7 and A10 alert the Privacy Lead **within the week**: they are often legitimate (an approved new account, a planned export), and each needs a recorded reason. **The CEO is copied on A9, and on any alert caused by the Privacy Lead's own account**, so the person who reviews the log is not the one person it cannot catch. Every alert and its outcome goes into the Privacy Lead's review records.

**Deferred until a baseline exists.** Three triggers need a threshold: (B1) rows returned by one person in a day well above their measured normal; (B2) access at an unusual time or from a new device or location, where the identity provider's own signal is preferred; (B3) a burst of `429` answers from the erasure relay (`OI-PRIV-48`). Their values are set from the measured log at the first quarterly review after the log is running, not chosen beforehand, so each rests on a measurement (`CLAUDE.md` §19).

**To build** (`OI-PRIV-47`): the sign-in proxy and the identity provider behind it, recording each page's type so A6 and A7 can be seen; the log forwarder, its heartbeat (A8) and the locked bucket; a ClickHouse user per script and operator; alerts A1 to A11, including the schedule A10 runs on and counters the erasure relay exposes while it runs (today it prints them only at shutdown) for A11; and a check, against the pinned `POSTHOG_VERSION`, of whether PostHog records page views itself (as far as is known, self-hosted PostHog records changes, not views).

## 8. Permitted and prohibited property reference

This section is the PostHog-specific view of NP-APP-TELEMETRY-001. The authoritative source is `docs/np_app_telemetry_001.md`.

### 8.1 Permitted properties (complete list)

```
engagement_tier          "new" | "active" | "established" — never raw count
step_name                onboarding step identifier (string enum)
feature_name             non-health feature name (string enum)
```

**This list is out of date and is not the authority** (Rev 5): the permitted properties are the nine of `NP-APP-TELEMETRY-001` §3, which name neither `step_name` nor `feature_name`, and the core drops an event that carries either.

`engagement_tier` is the only property permitted to describe app usage intensity. It is defined as:
- `"new"` — 1–5 app launches
- `"active"` — 6–50 app launches
- `"established"` — 51+ app launches

It counts app launches, not stimulation sessions. It resets on uninstall.

### 8.2 Prohibited properties (complete list for PostHog denylist)

The following must never reach ClickHouse. The server-side Property Filter (§4) drops them as a backstop; the iOS SDK must not send them in the first place.

**Biometric / UHDR-adjacent:**
- `imp`, `impedance`, `impedance_flags`, `pass_flags` — electrode impedance
- `eeg`, `eeg_*` — any EEG-related property
- `hrv`, `rmssd`, `heart_rate`, `bpm`, `ppg` — cardiac data
- `coherence_raw`, `coherence_score` — HRV coherence
- `rr_interval`, `rr_*` — R-R interval data
- `adapt_trigger`, `adaptation_event`, `adapted_*` — adaptation log data
- `session_timestamp`, `completedAt`, `epoch` — exact timestamps (UHDR-class)
- `session_sequence`, `session_count` — raw session count integer (use `engagement_tier`)

**Device / location:**
- `$geoip_*` — any GeoIP property (DISABLE_MMDB makes these empty but deny as belt-and-suspenders)
- `$ip` — raw IP address
- `$initial_current_url`, `$current_url`, `$referrer`, `$referring_domain`, `$initial_referring_domain`

### 8.3 How to modify this list

To add a prohibited property:
1. Update `docs/np_app_telemetry_001.md` (source of truth, requires doc revision)
2. Add the property to the PostHog Property Filter app (§4.3)
3. Update the iOS SDK to ensure the property is never sent (PR with test)
4. Update §8.2 of this document to match

---

## 9. Health monitoring

PostHog exposes a health check endpoint:

```bash
curl http://localhost:8000/_health
# → {"status": "ok"}
```

Container health:
```bash
docker compose ps          # all containers should show "healthy" or "running"
docker compose logs --tail=50 posthog
docker compose logs --tail=50 posthog-plugin-server
```

ClickHouse query activity:
```bash
docker compose exec clickhouse clickhouse-client \
  --user posthog --password "$CLICKHOUSE_PASSWORD" \
  --query "SELECT count() FROM system.query_log WHERE event_date = today()"
```

---

## 10. Relationship to other NP documents

| Document | Relationship |
|----------|-------------|
| `docs/np_app_telemetry_001.md` (NP-APP-TELEMETRY-001 Rev 2) | **Source of truth** for permitted/prohibited properties and SDK init gate |
| `docs/np_priv_audit_001.md` (NP-PRIV-AUDIT-001 Rev 1) | AUDIT-01 (analytics vendor not selected) and AUDIT-02 (SDK init gate) — self-hosting closes AUDIT-01 |
| `docs/np_priv_analysis_002.md` (NP-PRIV-ANALYSIS-002) | AnalyticsGate.reset() wired to consent withdrawal — applies equally to PostHog SDK |
| `docs/np_priv_rem_001.md` (NP-PRIV-REM-001) | STEP-05 (analytics vendor DPA) — self-hosting eliminates the third-party DPA requirement |
| `docs/np_priv_notice_001.md` (NP-PRIV-NOTICE-001) | §3 (app analytics disclosure) — references self-hosted analytics; no third-party name needed |
| `infra/posthog/docker-compose.yml` | Deployment configuration |
| `infra/posthog/.env.example` | Environment variable reference |
| `infra/posthog/clickhouse/np-privacy.xml` | ClickHouse system log TTLs |
| `infra/posthog/caddy/Caddyfile` | HTTPS termination configuration |

---

## 11. Compliance checklist — close AUDIT-01

The following must be completed to close AUDIT-01 (analytics vendor not selected, blocking external beta):

- [ ] Deploy PostHog locally using this configuration
- [ ] Complete first-time UI setup (§3.3 and §3.4)
- [ ] Install and configure the Property Filter app (§4.1 and §4.2)
- [ ] Verify denylist enforcement (§4.4) using a test event from the simulator
- [ ] Integrate PostHog iOS SDK with `AnalyticsGate` — SDK init only post-consent
- [ ] Verify `AnalyticsGate.reset()` calls `PostHogSDK.shared.optOut()` on consent withdrawal
- [ ] Pin `POSTHOG_VERSION` to a specific release in `.env`
- [ ] Update `PrivacyInfo.xcprivacy` — PostHog SDK will have required entries; self-hosting means no third-party domain disclosure needed for data *transmission* (only the SDK binary's API usage declarations)
- [ ] Update `docs/np_app_telemetry_001.md` to record PostHog as the selected analytics platform

---

*Self-hosting PostHog eliminates the third-party analytics DPA (NP-PRIV-REM-001 STEP-05), simplifies PrivacyInfo.xcprivacy, and satisfies EU data residency requirements by deploying in an EU cloud region.*

## Revision history

Revisions before the first row below were not recorded in a table in this document.

| Rev | Date | Author | Description |
|---|---|---|---|
| 5 | 2026-11-21 | NeurOne Software Engineering | **§7.6: the app keeps a queued ID on every 4xx and asks again at its next launch; only a 2xx drops it, so a relay deployed at the wrong path or behind a refusing proxy loses nothing (`OI-PRIV-08` (2), `REQ-SW03-19` (b)).** |
| 6 | 2026-11-16 | NeurOne Software Engineering | **Permitted events table follows `NP-APP-TELEMETRY-001` §3.3: `consent_granted` and `consent_revoked` removed, `screen_viewed` and `error_reported` added; §8.1 marked out of date (`step_name` and `feature_name` were never permitted) (`OI-PRIV-05`).** |
| 7 | 2026-11-19 | NeurOne Software Engineering | **§3.4: the proxy's access log and default logger delete the client's address, port and forwarding headers, checked by `ci/test_posthog_infra.py` P8 and P9; the relay of `NP-APP-TELEMETRY-001` §4.1 is decided and not built.** |
| 8 | 2026-11-20 | NeurOne Software Engineering | **§3.4 and §7.1: the principal set event retention to 30 days on 2026-11-20 (`NP-APP-TELEMETRY-001` §5 promises deletion of a withdrawn person's ID within 30 days). It is `EVENT_RETENTION_DAYS`, applied as a ClickHouse TTL by `apply_event_retention.py` and checked with `--check`; the policy is cited beside the value, and a value above 30 is flagged as breaking it. Not run against a real stack.** |
| 9 | 2026-11-21 | NeurOne Software Engineering | **The network-address paragraph: the analytics proxy removes nine client-address headers before PostHog and the erasure relay, and its logs keep no request header (`OI-PRIV-51`).** |
