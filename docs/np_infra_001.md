# NeuroPulse Infrastructure Setup Guide

**Project:** NeuroPulse  
**Document:** NP-INFRA-001  
**Revision:** A  
**Date:** 2026-06-14  
**Status:** ACTIVE  
**Effective Date:** 2026-06-14  
**Author:** Steve Hickman (CEO, interim Quality authority)  
**Approved By:** Steve Hickman, CEO  
**References:** —  
**Related Issues:** —  
**Gate:** Required before TestFlight beta and T1 launch  
**IEC 62304 Class:** —

---

## 1. Purpose and scope

This document is the operational reference for every external service the NeuroPulse iOS app contacts at runtime. It covers what to purchase, how to configure each service, what the API contract is, and what must be done before first public exposure (TestFlight beta, launch).

**Four services are in scope:**

| Service | Domain / Host | First required |
|---------|--------------|----------------|
| SHDR fleet database | `fleet.neuropulse.internal` | T1 launch |
| OTA firmware distribution | `firmware.neuropulse.ai` | T1 launch |
| Consumable shop | `neuropulse.com` | TestFlight beta |
| Analytics | `eu.i.posthog.com` (PostHog EU cloud) | TestFlight beta |

Everything here is cross-referenced to the Swift source file that embeds each URL so the infrastructure team can trace a service back to exactly where the app uses it.

---

## 2. Domain registrations required

Purchase these domains from your registrar (Namecheap, Cloudflare Registrar, etc.) before any external-facing work begins. Trademark clearance for `neuropulse` is a pending action — confirm clearance before registering (see CLAUDE.md §13.1).

| Domain | Why | Notes |
|--------|-----|-------|
| `neuropulse.com` | Customer-facing shop, consumable reorder links | Primary brand domain. Also register `.io`, `.co`, `.health` as defensive registrations |
| `neuropulse.ai` | OTA firmware distribution subdomain (`firmware.neuropulse.ai`) | Required for T1 launch. Also useful as AI-adjacent brand domain |

`fleet.neuropulse.internal` is an internal hostname resolved by private DNS — no registrar purchase required. Configure in your private DNS zone (e.g. Route53 private hosted zone, Cloudflare Zero Trust, or /etc/hosts in development).

---

## 3. SHDR fleet database — `fleet.neuropulse.internal`

**Source file:** [`app/ios/NeuroPulse/Data/SHDRUploader.swift`](../app/ios/NeuroPulse/Data/SHDRUploader.swift)

### 3.1 Purpose

The app uploads SHDR (System Health Data Record) binary blobs after each USB-C session. SHDR is device-condition telemetry only — LED degradation, NTC temperatures, impedance trends, firmware version history — never user biology (CLAUDE.md §5.1). See `NP-FW-EMMC-001 Rev A §7` for the full SHDR data schema.

### 3.2 Architecture

```
iOS App (SHDRUploader)
  │  POST /v1/shdr
  │  X-NP-Device-Token: <64-char hex opaque warranty token>
  │  Body: raw SHDR binary (from shdr_staging.bin, written by hub CDC interface)
  │  TLS: SPKI-pinned (SHDRFleetPinningDelegate)
  ▼
fleet.neuropulse.internal:443
  ├── Receives SHDR blob
  ├── Associates with opaque device token (NP-FW-EMMC-002 Rev A §A)
  └── Writes to fleet analytics database
```

The device token is a 256-bit TRNG opaque warranty token — it is never joined to user identity (no-join rule enforced by CI, see CLAUDE.md §13.4 OI-EMMC2-07). The fleet DB links to the hub device only.

### 3.3 API contract

**Endpoint:** `POST https://fleet.neuropulse.internal/v1/shdr`

| Field | Value |
|-------|-------|
| Method | POST |
| Content-Type | `application/octet-stream` |
| `X-NP-Device-Token` header | 64-char lowercase hex string (256-bit opaque TRNG token) |
| Body | Raw SHDR binary blob as received from hub CDC interface via `shdr_staging.bin` |
| Success response | `200 OK` (body ignored by client) |
| Error responses | `400` for malformed token format; `5xx` for server error (client retries on next USB-C connect) |

The server must not attempt to correlate `X-NP-Device-Token` with any user identity, email, or purchase record. The token is the sole linkage key between the SHDR blob and any warranty record. See NP-FW-EMMC-002 Rev A §A for the token architecture.

### 3.4 Infrastructure setup

1. **Private DNS:** add an A/CNAME record for `fleet.neuropulse.internal` in your private DNS zone pointing to the fleet service host. The `.internal` TLD is not publicly routable — this is intentional.

2. **Server:** provision an HTTPS server (nginx, Caddy, or a cloud API gateway). Minimum spec for T1 at 10,000 devices: 2 vCPU, 4 GB RAM, 500 GB block storage.

3. **Database:** append-only store keyed by `(device_token, session_timestamp)`. ClickHouse works well for fleet telemetry at scale; PostgreSQL is sufficient for < 50,000 devices.

4. **Upload gate:** the app uploads only when `warrantyConsentGranted == true` (user accepted warranty consent at first run) and the hub reports USB-C power source. The fleet server should enforce that a token was seen before (i.e. registered during warranty onboarding) and reject unknown tokens to prevent blind data injection.

### 3.5 TLS and SPKI pinning — pre-launch actions

The fleet endpoint is SPKI-pinned in `SHDRFleetPinningDelegate`. Placeholder hashes ship in the app; they match nothing real, so all fleet uploads fail until replaced.

**Before launch:**

```bash
# Generate a TLS certificate for fleet.neuropulse.internal (e.g. via internal CA).
# Then derive the SPKI SHA-256 hash:
openssl x509 -in fleet-cert.pem -pubkey -noout \
  | openssl pkey -pubin -outform der \
  | openssl dgst -sha256 -binary \
  | base64
```

Replace `pinnedHashes` in `SHDRFleetPinningDelegate` with at least two hashes: the current active key and one backup (for zero-downtime key rotation). The backup hash should correspond to a pre-generated key pair held offline.

---

## 4. OTA firmware distribution — `firmware.neuropulse.ai`

**Source file:** [`app/ios/NeuroPulse/OTA/FirmwareUpdateService.swift`](../app/ios/NeuroPulse/OTA/FirmwareUpdateService.swift)  
**Related:** [`app/ios/NeuroPulse/Models/OTAModels.swift`](../app/ios/NeuroPulse/Models/OTAModels.swift)

### 4.1 Purpose

The app periodically fetches a manifest from `firmware.neuropulse.ai` to check whether a hub firmware update is available. If a newer version exists, the app downloads the signed `.npfw` binary and transfers it to the hub over BLE. The hub independently verifies the Ed25519 signature before flashing (dual-layer security: TLS pinning on the download + Ed25519 on the image).

### 4.2 Architecture

```
iOS App (FirmwareUpdateService)
  │
  ├── GET /manifest.json  ─────────────────────────────────→ CDN origin
  │   No device ID, no version, no auth. Identical for all clients.
  │   Response: FirmwareManifest JSON (see §4.4)
  │
  └── GET /firmware/main-<version>.npfw  ─────────────────→ CDN origin
      Static path derived from version string only.
      Response: Ed25519-signed binary firmware image
                (verified by hub before flashing — NP-FW-EMMC-001 Rev A §8)
```

**Privacy design:** all requests are anonymous. The CDN receives no user, device, or session identifier. The manifest URL is the same for every client. Download URLs are deterministically derived from the version string in the manifest; no presigned or personalised URLs are used.

### 4.3 Domain and CDN setup

1. **Register `neuropulse.ai`** with your domain registrar.

2. **Create a CDN distribution** (AWS CloudFront, Cloudflare R2, Fastly, or GCS with custom domain):
   - Object storage origin bucket: `neuropulse-firmware` (or equivalent)
   - Create a CNAME: `firmware.neuropulse.ai` → CDN distribution domain

3. **Enable HTTPS** on the CDN distribution. Obtain a TLS certificate for `firmware.neuropulse.ai` (e.g. ACM on AWS, or Cloudflare Universal SSL).

4. **Public read access:** all objects under `/manifest.json` and `/firmware/` must be publicly readable (no auth required — privacy design decision, see §4.2).

5. **Cache headers:**
   - `manifest.json`: `Cache-Control: no-cache` or short TTL (30–60s) so clients see the latest version promptly.
   - `firmware/*.npfw`: long TTL (e.g. `immutable, max-age=31536000`) — a given version never changes.

### 4.4 Manifest format

The manifest is a JSON object at `https://firmware.neuropulse.ai/manifest.json`. It is decoded by `FirmwareManifest: Decodable` in `OTAModels.swift`.

```json
{
  "version":              "1.2.3",
  "releaseDate":          "2026-06-14",
  "imageSizeBytes":       2097152,
  "sha256Hex":            "a3f1...e9b2",
  "ed25519KeyFingerprint":"c7d0...8812",
  "isSafetyMCUFirmware":  false,
  "releaseNotes":         "Fixes cardiac interlock lockout wrap bug (OI-SW01-M05-06)."
}
```

| Field | Type | Description |
|-------|------|-------------|
| `version` | String | Semantic version `major.minor.patch`. Must parse as three non-negative integers. The hub's GATT `firmware_version` characteristic encodes this as a uint32 LE: bits [23:16]=major, [15:8]=minor, [7:0]=patch. |
| `releaseDate` | String | ISO 8601 date, `YYYY-MM-DD`. Parsed by `ISO8601DateFormatter` with `.withFullDate`. |
| `imageSizeBytes` | Int | Exact byte count of the `.npfw` binary. The app validates download size matches before transferring to hub. |
| `sha256Hex` | String | Lowercase hex SHA-256 of the `.npfw` binary. Used by the hub for post-transfer integrity check. |
| `ed25519KeyFingerprint` | String | Hex fingerprint of the Ed25519 public key used to sign this image. Shown to the user in the OTA confirmation screen before they approve the update. |
| `isSafetyMCUFirmware` | Bool | `true` if this image targets the STM32G071 Safety MCU. Safety MCU updates trigger a separate explicit user confirmation flow (per NP-FW-EMMC-001 Rev A §8). |
| `releaseNotes` | String | Plain-text release notes shown to user. Keep concise — the OTA UI card has limited vertical space. |

### 4.5 Firmware image path convention

Images are hosted at:
```
https://firmware.neuropulse.ai/firmware/main-<version>.npfw
```

Example: `https://firmware.neuropulse.ai/firmware/main-1.2.3.npfw`

The file is the Ed25519-signed binary produced by the secure build pipeline (manufacturing root key — see NP-FW-EMMC-001 Rev A §8.1 and `firmware/bootloader/src/np_signature.c`). The manufacturing root Ed25519 private key must be stored in an HSM and never exposed in CI.

**Signing workflow:**
```bash
# Build the unsigned firmware binary
cmake --build build/ --target np_main_processor_fw

# Sign with manufacturing root private key (HSM-backed in production)
openssl pkeyutl -sign -inkey mfg_root_private.pem -rawin \
  -in firmware.bin -out firmware.sig

# Append 64-byte signature to binary (format expected by np_signature.c)
cat firmware.bin firmware.sig > main-1.2.3.npfw

# Upload to CDN
aws s3 cp main-1.2.3.npfw s3://neuropulse-firmware/firmware/main-1.2.3.npfw \
  --cache-control "immutable, max-age=31536000"
```

### 4.6 TLS and SPKI pinning — pre-launch actions

The firmware endpoint is SPKI-pinned in `FirmwarePinningDelegate`. Placeholder zero-byte hashes ship in the app and match no real certificate.

**Before launch:**

```bash
openssl s_client -connect firmware.neuropulse.ai:443 2>/dev/null \
  | openssl x509 -noout -pubkey \
  | openssl pkey -pubin -outform DER \
  | openssl dgst -sha256 -binary \
  | xxd -p -c 256
```

Replace `pinnedSPKIHashes` in `FirmwareUpdateService` with the resulting hex strings (wrapped in `Data(hexString:)`, 64 hex chars each). Pin at least two hashes (active + backup key).

---

## 5. Consumable shop — `neuropulse.com`

**Source file:** [`app/ios/NeuroPulse/Consumable/ConsumableTracker.swift`](../app/ios/NeuroPulse/Consumable/ConsumableTracker.swift)

### 5.1 Purpose

When the SHDR consumable count crosses a threshold, the app fires a notification with a one-tap "Order" deep link. The link opens `neuropulse.com` in Safari. This is the primary consumable MRR driver (CLAUDE.md §2.3: intranasal sleeves drive $19/month subscription revenue).

### 5.2 URL routing

The app constructs URLs using `ConsumableKind.rawValue`, which is the integer case index of the `ConsumableKind: Int` enum. The mapping is fixed by enum declaration order — **do not reorder or insert cases before existing ones** without updating the web shop routing simultaneously.

| URL path | `rawValue` | Product | Price |
|----------|-----------|---------|-------|
| `/consumables/0` | 0 | Intranasal Sleeves | $19 / 30-pack or $19/mo subscription |
| `/consumables/1` | 1 | Electrode Hydrogel Tips | $12–16 / 8-pack or $9.99/mo subscription |
| `/consumables/2` | 2 | VNS Clip Pads | $8 / 2-pack |
| `/consumables/3` | 3 | Audio Cup Foam | $24 / set |

Each path must return `200 OK` with the correct product page before TestFlight beta. A `404` silently breaks the in-app reminder's call to action. The `orderURL` computed property returns `nil` if `URL(string:)` fails, so the "Order" button is simply hidden — a silent failure that would be missed without pre-launch testing.

### 5.3 Domain setup

1. **Register `neuropulse.com`** (and defensive registrations: `.co`, `.io`, `.health`, `.care`).
2. **Configure DNS:** point `neuropulse.com` to your e-commerce platform or web server.
3. **HTTPS:** required — App Transport Security enforces HTTPS for all outgoing links.
4. **E-commerce platform:** Shopify, WooCommerce, or custom. The URL path structure `/consumables/<int>` must be configured as a route in whatever platform is used.
5. **Subscription setup:** intranasal sleeves and electrode tips have monthly subscription options (CLAUDE.md §2.3). Configure recurring billing before T1 launch (Shopify Subscriptions, ReCharge, or equivalent).

### 5.4 Pre-launch checklist

- [ ] `https://neuropulse.com/consumables/0` returns 200, shows Intranasal Sleeves product page with $19 price and subscription option
- [ ] `https://neuropulse.com/consumables/1` returns 200, shows Electrode Hydrogel Tips product page
- [ ] `https://neuropulse.com/consumables/2` returns 200, shows VNS Clip Pads product page
- [ ] `https://neuropulse.com/consumables/3` returns 200, shows Audio Cup Foam product page
- [ ] Each page renders correctly when opened via Safari on iPhone (not just desktop browser)
- [ ] Subscription billing configured for `/consumables/0` ($19/mo) and `/consumables/1` ($9.99/mo)

### 5.5 Future-proofing the URL scheme

The current integer-path scheme (`/consumables/0`) is implementation-coupled to the Swift enum order. Before adding new consumable types, consider migrating to a slug-based scheme (e.g. `/consumables/intranasal-sleeves`). This would require:
- A URL rewrite layer on the web server mapping old integer paths to slugs
- A `ConsumableKind` computed property `shopSlug: String` replacing the raw integer interpolation
- A code + infrastructure migration deployed atomically (old integer links must continue to work for any installed version of the app)

---

## 6. Analytics — PostHog EU cloud

**Source file:** [`app/ios/NeuroPulse/Analytics/PostHogAnalyticsBackend.swift`](../app/ios/NeuroPulse/Analytics/PostHogAnalyticsBackend.swift)  
**Related:** [`docs/np_app_telemetry_001.md`](np_app_telemetry_001.md), [`docs/np_analytics_001.md`](np_analytics_001.md)

### 6.1 Two deployment options

The codebase currently uses **PostHog EU cloud** (`eu.i.posthog.com`) as the analytics ingestion endpoint. A separate self-hosted PostHog deployment specification exists in `docs/np_analytics_001.md` (NP-ANALYTICS-001 Rev A).

| Option | Host | DPA required | Data residency | When to use |
|--------|------|-------------|----------------|-------------|
| **PostHog EU cloud** (current code) | `eu.i.posthog.com` | Yes — PostHog's DPA | EU (by default) | Fastest path to TestFlight beta |
| **Self-hosted PostHog** | Your server | No third-party DPA | Your EU server | Preferred for T2 clinical cloud; eliminates third-party DPA (STEP-05 in NP-PRIV-REM-001) |

**For TestFlight beta:** use PostHog EU cloud (fastest setup). **For T1 launch:** decision pending — if self-hosted is not deployed before launch, execute PostHog's DPA before any EU/EEA user activates the app.

### 6.2 PostHog EU cloud setup

1. **Create a PostHog EU account** at [https://eu.posthog.com](https://eu.posthog.com) (not `us.posthog.com` — EU data residency is required under GDPR Art. 44 for EU/EEA users).

2. **Create a project:** name it "NeuroPulse App". Note the Project API Key (format: `phc_<chars>`).

3. **Configure the build variable:** the token is read at runtime from `Info.plist` key `PostHogProjectToken`, which is sourced from the `POSTHOG_PROJECT_TOKEN` build variable. **Never hardcode the token in source** — the `no_hardcoded_secret` SwiftLint rule (ISC-9) will reject it.

   - **Developer builds:** create a local `.xcconfig` file (added to `.gitignore`):
     ```
     POSTHOG_PROJECT_TOKEN = phc_your_actual_token_here
     ```
     Reference it in your Xcode scheme's pre-action or in the Build Settings xcconfig path.

   - **CI (GitHub Actions):** add `POSTHOG_PROJECT_TOKEN` as a repository secret, then pass it to `xcodebuild`:
     ```yaml
     - name: Build
       run: xcodebuild ... POSTHOG_PROJECT_TOKEN=${{ secrets.POSTHOG_PROJECT_TOKEN }}
     ```

4. **PostHog project settings (UI):** after account creation, navigate to Project Settings and confirm:

   | Setting | Required value |
   |---------|---------------|
   | IP address collection | Mask full IP address |
   | Session recording | Disabled |
   | Autocapture | Off |
   | Heatmaps | Off |
   | Event retention | 90 days |
   | Person retention | 90 days |

5. **Server-side property filter:** install the PostHog "Property Filter" transformation (Data Pipeline → Transformations). Add all prohibited properties from `NP-APP-TELEMETRY-001 Rev B §3` to the denylist. Full property list: see `docs/np_analytics_001.md §8.2`.

6. **Execute PostHog DPA:** download and sign PostHog's Data Processing Agreement from your account settings before any EU/EEA user data flows through the account. File with NP-LEGAL.

### 6.3 Token flow (no source changes needed)

```
PostHog EU dashboard
  → Project API Token: phc_xxxxx
       ↓
Local .xcconfig / CI secret: POSTHOG_PROJECT_TOKEN=phc_xxxxx
       ↓
Xcode build substitutes $(POSTHOG_PROJECT_TOKEN) → Info.plist PostHogProjectToken
       ↓
PostHogAnalyticsBackend.configure() reads: Bundle.main.object(forInfoDictionaryKey: "PostHogProjectToken")
       ↓
PostHogConfig(projectToken: token, host: "https://eu.i.posthog.com")
```

### 6.4 Migrating to self-hosted

When the self-hosted stack (NP-ANALYTICS-001 Rev A, `docs/np_analytics_001.md`) is deployed in an EU region:

1. Obtain the self-hosted PostHog project API token (from the self-hosted UI)
2. Update the `POSTHOG_PROJECT_TOKEN` build variable to point to the new token
3. Update `PostHogAnalyticsBackend` to replace `"https://eu.i.posthog.com"` with the self-hosted URL (e.g. `"https://analytics.neuropulse.internal"`)
4. No other source changes required — the SDK initialisation and event routing are already correct

---

## 7. Master pre-launch infrastructure checklist

### TestFlight beta (required before any external tester)

- [ ] **Analytics:** PostHog EU account created; `POSTHOG_PROJECT_TOKEN` set in CI secrets; property filter installed; DPA executed
- [ ] **Consumables shop:** all four `/consumables/<0–3>` paths return 200 with correct product pages on `neuropulse.com`; tested on physical iPhone in Safari

### T1 launch (required before App Store submission)

- [ ] **Fleet:** `fleet.neuropulse.internal` server provisioned; `POST /v1/shdr` endpoint operational; SPKI hashes derived and replaced in `SHDRFleetPinningDelegate.pinnedHashes`; no-join CI test passing (OI-EMMC2-07)
- [ ] **OTA:** `firmware.neuropulse.ai` CDN live; first signed `manifest.json` uploaded; first `.npfw` binary uploaded; SPKI hashes derived and replaced in `FirmwareUpdateService.pinnedSPKIHashes`; OTA tested end-to-end on physical device
- [ ] **Consumables shop:** subscription billing configured for intranasal sleeves ($19/mo) and electrode tips ($9.99/mo)
- [ ] **PostHog:** PostHog DPA executed (if still on EU cloud) or self-hosted stack deployed in EU region
- [ ] **Trademark clearance:** `neuropulse` cleared before domains are publicly associated with marketing material (CLAUDE.md §13.1)

---

## 8. Related documents

| Document | Relationship |
|----------|-------------|
| `docs/neuropulse_fw_emmc_001.docx` (NP-FW-EMMC-001 Rev A) | SHDR partition schema; dual-bank OTA bootloader protocol |
| `docs/np_fw_emmc_002.md` (NP-FW-EMMC-002 Rev A §A) | Warranty token architecture; no-join rule for fleet DB |
| `docs/np_app_telemetry_001.md` (NP-APP-TELEMETRY-001 Rev B) | Permitted/prohibited analytics event properties; `engagement_tier` enum |
| `docs/np_analytics_001.md` (NP-ANALYTICS-001 Rev A) | Full self-hosted PostHog deployment specification |
| `docs/np_priv_rem_001.md` (NP-PRIV-REM-001) | STEP-05 (analytics DPA), STEP-19 (EU data residency for T2 cloud) |
| `docs/np_app_roadmap_001.md` (NP-APP-ROADMAP-001 Rev B) | iOS app development phases; OTA Watch app phases |
| `app/ios/ISA.md` | ISC-3/8/9: SwiftLint enforcement; ISC-157: single PostHog integration point |
