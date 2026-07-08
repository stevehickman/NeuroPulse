# T2 Clinical Scripting API Specification

**Project:** NeurOne
**Document:** NP-API-001
**Revision:** A
**Date:** 2026-06-07
**Status:** DRAFT
**Effective Date:** 2026-06-07
**Author:** SmartyPants / PAI
**Approved By:** TBD (Privacy Lead sign-off required before external publication)
**References:** NP-INT-FHIR-001 Rev A, NP-PRIV-REM-001 Rev A (STEP-15), NP-LEGAL-BAA-001 Rev A, np_npps_ref_001.md, NP-SW-001 Rev A, NP-APP-TELEMETRY-001 Rev B
**Related Issues:** —
**Gate:** NP-COORD-001 G1 (NP-PRIV-REM-001 STEP-15)
**IEC 62304 Class:** —
**Supersedes:** —
**Parent Document:** —

---

## §1 Purpose and Scope

The NeurOne T2 Clinical Scripting API enables authorised researchers and clinicians to:
- Upload and compile custom NPPS session protocols to a patient's NeurOne T2 device
- Monitor live session telemetry via WebSocket
- Retrieve FHIR R4-formatted session outcome data
- Query SHDR device health metrics for fleet monitoring

**This is not a consumer API.** Every API key holder must:
1. Have an active Business Associate Agreement (BAA, NP-LEGAL-BAA-001) on file before key issuance
2. Receive Privacy Lead approval for their intended use case
3. Be subject to an independent security audit (NP-SEC-PENTEST-002, STEP-26) before any external clinical key is issued

This document is a G1 gate deliverable per NP-COORD-001 and NP-PRIV-REM-001 STEP-15. No T2 API code may be written before this specification is approved.

---

## §2 Authentication and Authorization

### 2.1 API Key Issuance

- **Prerequisite:** executed BAA on file; Privacy Lead approval; clinical tier assignment documented
- **Key format:** 256-bit cryptographically random, encoded as 64-character lowercase hex
- **HTTP header:** `Authorization: Bearer NP-T2-{64-hex-chars}`
- **Expiry:** 90 days from issuance; enforced at API gateway; expired keys return `401 INVALID_KEY`
- **Rotation:** issuer provides new key; 7-day grace period for old key; old key revoked after grace period
- **Revocation:** immediate; in-flight requests with revoked key return `401 INVALID_KEY`
- **Key storage requirement:** keys must never be stored in client source code; key vault required (e.g. HashiCorp Vault, AWS Secrets Manager, environment variable injection at runtime)

### 2.2 Clinical Access Tiers

| Tier | Fee | UHDR elements accessible via API | BAA required |
|------|-----|-----------------------------------|--------------|
| Monitor | $49/patient/month | Session timestamps, duration, protocol parameters only | Yes |
| Assess | $149/patient/month | + EEG band power ratios, neurofeedback scores, dose logs (J/cm², mC) | Yes |
| Full Clinical | $299/patient/month | + HRV RMSSD, coherence score, adaptation events, outcome logs | Yes |
| Research | $599/study/month | IRB-defined minimum; k≥10 anonymisation; no patient IDs | Yes + IRB |

### 2.3 UHDR Three-Condition Access Gate

**ALL THREE of the following conditions must be simultaneously true for any UHDR element to be returned:**

1. **Explicit consent grant** — the user has actively consented to clinical data access via the app (separate from onboarding consent; revocable at any time independently)
2. **API access toggle enabled** — the user has enabled the API access toggle in the NeurOne app (a distinct user-controlled setting, independent of consent)
3. **Element within use-case scope** — the requested data element falls within the requesting tier's documented minimum-necessary scope (§2.2)

If **any** condition is false: return `403 Forbidden` with `"np_error_code": "UHDR_ACCESS_DENIED"`. Never return partial UHDR data with one or two conditions met.

**Consent withdrawal:** On user consent withdrawal, all API requests for that `patient_token` return `403 UHDR_ACCESS_DENIED` immediately, for all data periods including historical sessions. This is enforced at the API gateway layer independently of device-level enforcement.

---

## §3 REST API Endpoints

**Base URL:** `https://api.neurone.life/t2/v1`

All requests require:
- `Authorization: Bearer NP-T2-{key}` header
- `Content-Type: application/json` (for POST/PUT)
- TLS 1.3 (no TLS 1.2 fallback permitted)

Rate limits apply globally (see §7).

### 3.1 Protocol Compilation — `POST /protocols`

Compiles NPPS protocol source text and returns a signed binary session descriptor.

**Request body:**
```json
{
  "npps_source": "<NPPS protocol text>",
  "target_tier": "T2",
  "modalities": ["tms", "eeg", "hd_tdcs"],
  "label": "depression_protocol_v3"
}
```

**Response 200:**
```json
{
  "protocol_id": "proto_7f3a2b1c",
  "signed_descriptor_b64": "<base64-encoded signed .npps binary>",
  "compiled_at": "2026-06-07T09:00:00Z",
  "modalities_included": ["tms", "eeg", "hd_tdcs"],
  "duration_seconds": 1800,
  "requires_tier": "Full Clinical",
  "signature_key_id": "NP-SIGN-2026-001",
  "dose_limits_checked": true
}
```

**Errors:** `400 NPPS_COMPILE_ERROR` (includes line/column); `403 MODALITY_NOT_IN_TIER`; `422 DOSE_LIMIT_EXCEEDED`

### 3.2 Session Scheduling — `POST /devices/{device_id}/sessions`

Schedules a signed protocol for delivery to a patient's device.

**Request body:**
```json
{
  "patient_token": "<opaque-32-char-token>",
  "signed_descriptor_b64": "<from §3.1>",
  "scheduled_at": "2026-06-08T09:00:00Z",
  "notify_webhook_url": "https://clinic.example.com/webhooks/np"
}
```

**Response 201:**
```json
{
  "session_id": "sess_a1b2c3d4",
  "status": "scheduled",
  "device_id": "dev_xyz",
  "scheduled_at": "2026-06-08T09:00:00Z"
}
```

**Errors:** `403 UHDR_ACCESS_DENIED`; `404 DEVICE_NOT_FOUND`; `409 SESSION_CONFLICT`

### 3.3 Device Status (SHDR only) — `GET /devices/{device_id}/status`

Returns SHDR device health metrics. **No UHDR. No patient identity visible to caller.**

**Response 200:**
```json
{
  "device_id": "dev_xyz",
  "shdr": {
    "firmware_version": "1.4.2",
    "session_count": 52,
    "led_pd_ratio_zone1": 0.93,
    "emf_attenuation_db": 41.8,
    "supercap_cycles": 318,
    "hub_temp_c": 36.5,
    "pre_eol_info": "01",
    "last_ota": "2026-05-28",
    "contact_resistance_kohm": 1.2
  }
}
```

### 3.4 FHIR Observation Query — `GET /patients/{patient_token}/fhir/Observation`

**Requires:** Assess tier or above; UHDR three-condition gate.

**Query parameters:**
- `date`: ISO week string (e.g. `2026-W23`) or ISO date (`2026-06-07`)
- `code`: LOINC code or NP local code (e.g. `80404-7` for RMSSD, `NP-EEG-ALPHA`)
- `_count`: max 100 per request

**Response 200:** FHIR Bundle (resourceType: "Bundle") of NP-Observation resources per NP-INT-FHIR-001.

Timestamps in returned observations are rounded to **day precision** — never finer. Observation effective[x] is an effectivePeriod spanning a calendar day (UTC).

**Errors:** `403 UHDR_ACCESS_DENIED`; `400 INVALID_FHIR_DATE`

### 3.5 FHIR Diagnostic Report — `GET /patients/{patient_token}/fhir/DiagnosticReport`

**Requires:** Assess tier or above; UHDR gate.

Returns weekly session summary reports (NP-DiagnosticReport resources, per NP-INT-FHIR-001). One report per ISO week. Reports aggregate multiple NP-Observation resources.

### 3.6 FHIR Procedure (write) — `POST /patients/{patient_token}/fhir/Procedure`

**Requires:** Full Clinical tier; UHDR gate.

Creates a session audit record for the patient's clinical record.

**Request body (FHIR Procedure):**
```json
{
  "resourceType": "Procedure",
  "status": "completed",
  "code": { "coding": [{ "system": "http://snomed.info/sct", "code": "229070002" }] },
  "subject": { "reference": "Patient/{patient_token}" },
  "performedPeriod": {
    "start": "2026-06-07T09:00:00Z",
    "end": "2026-06-07T09:30:00Z"
  },
  "outcome": { "coding": [{ "code": "NP-SESSION-SATISFACTORY" }] }
}
```

Permitted status values: `completed` only. Time precision: minute (not second).

### 3.7 Bulk Export — `GET /patients/{patient_token}/export`

**Requires:** Full Clinical or Research tier; **bulk-export credential** (separate from standard API key); UHDR gate; for Research tier, IRB protocol ID required in header `X-NP-IRB-ID`.

**Bulk-export credentials:** issued per request via `POST /credentials/bulk-export`. Requires standard API key + Privacy Lead-authorised justification in request body. Credential is 256-bit random, single-use, valid 24 hours.

**Response:** `202 Accepted` with `{ "export_job_id": "export_abc123" }`. Poll `GET /export-jobs/{id}` for status. Download link valid 1 hour when ready.

### 3.8 Audit Log — `GET /audit`

Returns the caller's own API audit log entries. Query parameters: `from`, `to` (ISO 8601 datetimes), `_count` (max 1,000).

**Response 200:**
```json
{
  "entries": [
    {
      "log_id": "audit_abc123",
      "key_id": "NP-T2-7f3a2b1c...",
      "patient_token": "pt_opaque32",
      "endpoint": "GET /patients/{token}/fhir/Observation",
      "timestamp_utc": "2026-06-07T14:32:00.000Z",
      "result": "success",
      "np_error_code": null,
      "http_status": 200,
      "records_returned": 4
    }
  ]
}
```

---

## §4 WebSocket Session Control API

### 4.1 Connection

**Endpoint:** `wss://hub.neurone.life/t2/session` (cloud proxy) or `ws://{device-ip}:9000` (direct LAN)

**Authentication:** Present API key in HTTP upgrade header: `Authorization: Bearer NP-T2-{key}`

### 4.2 Client Roles

- `controller` — sends session commands; receives telemetry and fault events
- `display` — receives session state broadcasts (for clinical monitoring UIs)

### 4.3 Message Types

| Type | Direction | Payload summary |
|------|-----------|-----------------|
| `CLIENT_HELLO` | Client → Server | `{ "role": "controller", "version": "1.0" }` |
| `CONNECTED` | Server → Client | `{ "version": "1.4.2", "capabilities": [...] }` |
| `SESSION_START` | Controller → Hub | `{ "signed_descriptor_b64": "...", "patient_token": "..." }` — Ed25519 verified by hub before any GPIO enable |
| `SESSION_PAUSE` | Controller → Hub | `{}` |
| `SESSION_STOP` | Controller → Hub | `{}` |
| `ZONE_CONFIG` | Controller → Hub | `{ "zone_id": 1, "config": {...} }` |
| `ACCESSORY_CONFIG` | Controller → Hub | `{ "name": "intranasal_probe", "visible": true }` |
| `TELEMETRY` | Hub → Controller | See §4.4 — ≥10 Hz during active session |
| `FAULT` | Hub → Controller | `{ "fault_code": "CHARGE_DENSITY_LIMIT", "module": "SW01-M03", "session_id": "..." }` |
| `SESSION_COMPLETE` | Hub → Controller | `{ "session_id": "...", "uhdr_written": true, "duration_s": 1800 }` |
| `ERROR` | Server → Client | `{ "np_error_code": "...", "message": "...", "request_id": "..." }` |

### 4.4 Telemetry Payload

```json
{
  "ts_ms": 1749286400000,
  "session_id": "sess_a1b2c3d4",
  "phase": "steady",
  "elapsed_s": 342,
  "eeg_bands": { "alpha": 0.42, "theta": 0.31, "gamma": 0.18, "delta": 0.09 },
  "hrv_rmssd_ms": 48.2,
  "coherence_x100": 712,
  "pbm_dose_j_cm2": [1.2, 1.1, 0.9, 1.3, 1.0],
  "bes_charge_uc_cm2": 12.4,
  "hub_temp_c": 36.8,
  "impedance_ok": true,
  "adaptation_events_since_last": 0
}
```

**Privacy note:** `eeg_bands`, `hrv_rmssd_ms`, and `coherence_x100` are UHDR-class fields. They are only present in telemetry if the UHDR three-condition gate (§2.3) is satisfied for the requesting caller at session-start time.

### 4.5 Connection Lifecycle

- **Heartbeat:** 30-second server → client ping; client must pong within 10 s or connection is closed
- **Reconnect:** exponential backoff (1 s, 2 s, 4 s, 8 s, max 30 s); session continues on hub in Mode 3 (autonomous) regardless of WebSocket connectivity
- **Session persistence:** hub runs session to completion even if all WebSocket connections drop; UHDR is written on-device

---

## §5 FHIR Data Model

See NP-INT-FHIR-001 Rev A for full resource profiles. Summary:

| Resource | Usage | Key constraint |
|----------|-------|----------------|
| NP-Patient | Patient identity | Opaque clinic-assigned MRN only; no name, DOB, telecom, address |
| NP-Observation | Clinical measurements | EEG band ratios, HRV RMSSD, coherence score, dose; day-precision timestamps |
| NP-DiagnosticReport | Weekly session summary | ISO week granularity; status: final only |
| NP-Procedure | Session audit (Full Clinical) | completed status; minute-precision time; SNOMED 229070002 |

**Prohibited FHIR resource types:** RelatedPerson, Coverage, Claim, Condition, MedicationRequest, ImagingStudy, AllergyIntolerance.

**Data minimisation:** the FHIR API enforces HIPAA minimum necessary — only the elements required for the stated clinical tier use case are returned.

---

## §6 Privacy and Data Governance

### 6.1 SHDR-only default

All API responses are SHDR-only by default. No UHDR element is returned unless all three conditions in §2.3 are simultaneously satisfied. Partial satisfaction does not unlock partial data.

### 6.2 Consent withdrawal

On consent withdrawal, `403 UHDR_ACCESS_DENIED` is returned for all subsequent requests for that `patient_token`, for all data time periods. The app enforces withdrawal on-device; the API enforces it at the gateway layer independently and redundantly.

### 6.3 Audit log schema

Every API call (including denied requests) is logged:

```json
{
  "log_id": "audit_{uuid}",
  "key_id": "NP-T2-{first-8-chars-of-key}...",
  "patient_token": "{opaque}",
  "endpoint": "{method} {path-template}",
  "timestamp_utc": "{ISO-8601-ms}",
  "result": "success | denied | error",
  "np_error_code": "{code or null}",
  "http_status": 200,
  "records_returned": 4,
  "bulk_export_job_id": "{id or null}"
}
```

Audit logs are retained for 7 years (HIPAA minimum). Callers can retrieve their own audit log (§3.8); NeurOne support can retrieve all logs under BAA.

### 6.4 Research tier anonymisation

Research tier responses use on-device anonymisation (NP-FW-ANON-001 Rev A): k-anonymity k≥10, l-diversity l≥3, differential privacy ε≤1.0, δ≤10⁻⁵. NeurOne never processes raw UHDR — anonymisation is performed entirely on-device; only the anonymised extract is transmitted. NP-ANON-CERT-[study_id] (signed Expert Determination certification) required before each study descriptor is deployed (NP-PRIV-REM-001 STEP-32).

---

## §7 Rate Limits and Quotas

| Limit | Value | Scope | HTTP on violation |
|-------|-------|-------|-------------------|
| Standard requests | 1,000 / hour | Per API key | 429 |
| Standard requests | 10,000 / day | Per API key | 429 |
| FHIR observation queries | 500 / hour | Per API key | 429 |
| Bulk export jobs | 5 / day | Per API key | 429 |
| WebSocket connections | 3 concurrent | Per API key | 429 on upgrade |
| Protocol compilations | 100 / hour | Per API key | 429 |

All 429 responses include a `Retry-After` header (seconds). Bulk-export credential requests are not rate-limited but require Privacy Lead justification per request.

---

## §8 Error Codes

All error responses: `{ "np_error_code": "...", "message": "...", "request_id": "..." }`

| HTTP | np_error_code | Meaning |
|------|---------------|---------|
| 400 | NPPS_COMPILE_ERROR | NPPS source validation failure; includes line/column |
| 400 | INVALID_FHIR_DATE | Date parameter not ISO week or ISO date |
| 401 | INVALID_KEY | API key absent, malformed, or expired |
| 403 | UHDR_ACCESS_DENIED | Three-condition gate not satisfied |
| 403 | MODALITY_NOT_IN_TIER | Requested modality exceeds caller's clinical tier |
| 403 | BAA_NOT_ON_FILE | No active BAA on file for this key |
| 403 | BULK_EXPORT_CREDENTIAL_REQUIRED | Standard key cannot perform bulk export |
| 404 | DEVICE_NOT_FOUND | device_id not registered or not associated with account |
| 404 | PATIENT_NOT_FOUND | patient_token not registered |
| 409 | SESSION_CONFLICT | Device already has a pending or active session |
| 422 | DOSE_LIMIT_EXCEEDED | Protocol exceeds NeurOne safety dose limits |
| 429 | RATE_LIMIT_EXCEEDED | Rate limit hit; see Retry-After header |
| 500 | INTERNAL_ERROR | Server error; include request_id when contacting support |
| 503 | HUB_UNAVAILABLE | Device hub not reachable (device offline or unreachable) |

---

## §9 Security Requirements

1. **TLS 1.3 mandatory** — all REST and WebSocket connections; no TLS 1.2 fallback; permitted cipher suites: TLS_AES_256_GCM_SHA384, TLS_CHACHA20_POLY1305_SHA256
2. **Certificate pinning** — hub WebSocket clients pin the NeurOne intermediate CA certificate (SPKI SHA-256 hash); implementation pattern per NP-PRIV-ANALYSIS-002 LOW-11
3. **Key vault required** — API keys must never be stored in source code, config files, or logs; key vault injection at runtime required for all automated callers
4. **Independent security audit** — NP-SEC-PENTEST-002 scope and execution required before any external clinical API key is issued; audit must cover: authentication bypass, UHDR gate bypass, rate-limit bypass, key expiry bypass, SPI injection via WebSocket
5. **CI boundary enforcement** — the following must have automated tests in the API server's CI pipeline:
   - `UHDR_ACCESS_DENIED` returned when any of the three gate conditions is false
   - Expired key returns `401` (not `200`)
   - 1,001st request in one hour returns `429`
   - Bulk export with standard key returns `403 BULK_EXPORT_CREDENTIAL_REQUIRED`
   - Consent withdrawal propagates to `403` within one request cycle
6. **SBOM** — all API server dependencies included in NP-SBOM-001 (Year 2, before 510(k) submission)

---

## §10 Open Items

| ID | Item | Owner | Target |
|----|------|-------|--------|
| OI-API-01 | Privacy Lead sign-off on §6 (data governance) required before external publication of this document | Privacy Lead | Before G1 close |
| OI-API-02 | Legal review of bulk-export credential flow (§3.7) — confirm Privacy Lead justification process satisfies HIPAA minimum-necessary | Legal + Privacy Lead | Month 6 |
| OI-API-03 | Define NP-SEC-PENTEST-002 audit scope document; enumerate all endpoints, auth flows, UHDR gate bypass scenarios | Security + Privacy Lead | Month 12 |
| OI-API-04 | Research tier: define IRB protocol ID validation API flow with institutional IRB offices | Clinical | Month 9 |
| OI-API-05 | Specify webhook push model for session-complete notifications (§3.2 `notify_webhook_url`) — HMAC-SHA256 signature on webhook payload | Engineering | Month 8 |
| OI-API-06 | FHIR Subscription resource support for real-time observation streaming — Full Clinical tier; FHIR R4 Subscription backport | Engineering | Month 12 |
| OI-API-07 | LSL streaming endpoint specification — used by T2 research integration; reference NP-INT-FHIR-001 §5 | Engineering | Month 10 |

---

## §11 Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| A | 2026-06-07 | SmartyPants / PAI | Initial issue — G1 gate deliverable; NP-PRIV-REM-001 STEP-15 |
