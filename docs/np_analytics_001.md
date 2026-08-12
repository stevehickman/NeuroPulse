# NeurOne PostHog Analytics — Self-Hosted Configuration Reference

**Project:** NeurOne  
**Document:** NP-ANALYTICS-001  
**Revision:** 1
**Date:** 2026-06-13  
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

These settings are not configurable via environment variables. Complete them immediately after project creation.

**Navigate to: Project Settings → Privacy**

| Setting | Value | Why |
|---------|-------|-----|
| IP address collection | **Mask full IP address** | No IP data stored even though DISABLE_MMDB already prevents geolocation |
| Session recording | **Disabled** | Belt-and-suspenders — OBJECT_STORAGE_ENABLED=false already makes recording non-functional |
| Person display name | Leave as default | n/a |

**Navigate to: Project Settings → Data Management → Data Retention**

| Setting | Value | Why |
|---------|-------|-----|
| Event retention | **90 days** | Balances product insight needs with data minimisation. See §7.1 to adjust. |
| Person retention | **90 days** | Anonymous person records only; matches event retention |

> **Note:** "Person" records in PostHog are anonymous identifiers (the `$device_id` the iOS SDK generates). They are not linked to a NeurOne account or any UHDR data. However, 90-day retention still satisfies data minimisation.

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

Only these event types may be sent. No other events should be captured:

| Event name | When fired | Permitted properties |
|------------|-----------|---------------------|
| `app_opened` | App foreground | `engagement_tier` |
| `onboarding_step_completed` | Each onboarding screen | `step_name`, `engagement_tier` |
| `consent_granted` | Consent flow completed | `engagement_tier` |
| `consent_revoked` | User revokes consent | `engagement_tier` |
| `feature_used` | Feature tap (non-health) | `feature_name`, `engagement_tier` |
| `app_crashed` | On next launch after crash | `engagement_tier` (no stack trace — crash traces are local-only) |

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

Event retention is set per-project in PostHog UI at Project Settings → Data Management → Data Retention. The default configured in §3.4 is 90 days.

To change:
- Go to Project Settings → Data Management → Data Retention
- Adjust the slider
- Changes apply to new data writes immediately; existing data is purged on the next ClickHouse TTL run (typically within 24 hours)

**Considerations for changing retention:**
- Shorter retention (30 days): stronger data minimisation, but loses long-term trend data
- Longer retention (180 days): more trend visibility for product decisions, but higher storage and higher privacy liability
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

Expected steady-state disk usage at 90-day retention with ~10,000 MAU:
- ClickHouse data: 5–20 GB (events compress well in ClickHouse columnar storage)
- PostgreSQL data: <1 GB
- Kafka logs: <5 GB (24-hour retention, consumed quickly)

Monitor with: `docker compose exec clickhouse clickhouse-client -q "SELECT formatReadableSize(sum(bytes_on_disk)) FROM system.parts WHERE active"`

---

## 8. Permitted and prohibited property reference

This section is the PostHog-specific view of NP-APP-TELEMETRY-001. The authoritative source is `docs/np_app_telemetry_001.md`.

### 8.1 Permitted properties (complete list)

```
engagement_tier          "new" | "active" | "established" — never raw count
step_name                onboarding step identifier (string enum)
feature_name             non-health feature name (string enum)
```

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
