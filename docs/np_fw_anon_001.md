# Research Anonymisation Engine Firmware Specification

**Project:** NeuroPulse
**Document:** NP-FW-ANON-001
**Revision:** A
**Date:** 2026-06-03
**Status:** ACTIVE
**Effective Date:** 2026-06-03
**Author:** Steve Hickman (CEO, interim Quality authority)
**Approved By:** Steve Hickman, CEO
**References:** NP-PRIV-REM-001 STEP-11; NP-FW-EMMC-002 §A §D §E; NP-FW-EMMC-001 Rev A §6 §15; 45 CFR §164.514(b)(1) (HIPAA Expert Determination); NP-PRIV-001 Rev A HIGH-02
**Related Issues:** —
**Gate:** —
**IEC 62304 Class:** SW-02 Class B (main processor — anonymisation does not control stimulation)
**Supersedes:** —
**Parent Document:** NP-FW-EMMC-001  

---

## 1. Purpose

This specification governs the firmware research anonymisation engine that runs on the i.MX RT1062 main processor. The engine transforms UHDR data into study-specific anonymised extracts for researcher delivery, entirely on-device, before any data leaves the NeuroPulse hub. NeuroPulse cannot access raw UHDR at any point — the UHDR partition key (UKMD) is derived from the user's biometric/PIN and never held by NeuroPulse infrastructure.

The engine satisfies the anonymisation standard required by NP-PRIV-001 Rev A HIGH-02:
- **k-anonymity:** k ≥ 10 (minimum group size; groups smaller than k are suppressed)
- **l-diversity:** l ≥ 3 (minimum distinct sensitive-attribute values per k-group)
- **Differential privacy:** ε ≤ 1.0 (total privacy loss budget), δ ≤ 10⁻⁵ (failure probability)

The engine also satisfies the HIPAA Expert Determination method (45 CFR §164.514(b)(1)) when combined with a signed NP-ANON-CERT expert certification (see STEP-31/32).

---

## 2. Architecture Overview

```
Study descriptor (signed Ed25519)
         │
         ▼
┌─────────────────────────────────────────────────────────────┐
│  np_anon_session — main pipeline (FreeRTOS task, 16 KB stack) │
│                                                               │
│  1. Verify study descriptor signature                         │
│  2. Unlock UHDR partition (UKMD from biometric gate)          │
│  3. Read approved UHDR elements → Scratch (AES-256-CTR)       │
│  4. Apply date rounding                                       │
│  5. np_anon_kgroup: k-anonymity grouping + l-diversity check  │
│  6. np_anon_dp: Laplace noise on numeric features             │
│  7. np_anon_output: format extract, sign with study pub key   │
│  8. Write signed extract to output queue                      │
│  9. np_anon_scratch_complete: zero key, SANITIZE Scratch      │
└─────────────────────────────────────────────────────────────┘
         │
         ▼
  Signed anonymised extract → BLE/USB-C to app → researcher
```

All working data lives in the Scratch partition (AES-256-CTR encrypted, per NP-FW-EMMC-002 §D). The UKMD is held in SRAM only during the unlock window and zeroed immediately after the UHDR read completes. The Scratch session key (K_scratch) is also SRAM-only and is zeroed after the extract is written and Scratch is sanitised.

---

## 3. Study Descriptor

The study descriptor is a JSON-encoded document, cryptographically signed by NeuroPulse (Ed25519), that authorises a specific research extract. The device verifies the signature before any UHDR data is accessed.

### 3.1 Descriptor schema

```json
{
  "study_id":            "NP-STUDY-001",
  "version":             1,
  "issued_at":           "2027-03-01T00:00:00Z",
  "expires_at":          "2027-06-01T00:00:00Z",
  "k_min":               10,
  "l_min":               3,
  "epsilon_total":       1.0,
  "delta":               0.00001,
  "date_rounding_days":  7,
  "elements": [
    { "id": "eeg_alpha_ratio",    "type": "FLOAT", "sensitivity": 1.0  },
    { "id": "eeg_theta_ratio",    "type": "FLOAT", "sensitivity": 1.0  },
    { "id": "hrv_rmssd_ms",       "type": "FLOAT", "sensitivity": 200.0},
    { "id": "session_count",      "type": "INT",   "sensitivity": 1.0  },
    { "id": "protocol_category",  "type": "ENUM",  "sensitivity": null }
  ],
  "quasi_identifiers":  ["age_decile", "session_count_tier"],
  "sensitive_attributes": ["eeg_alpha_ratio", "hrv_rmssd_ms", "protocol_category"],
  "study_public_key":    "[Base64-encoded Ed25519 public key for output encryption]",
  "neuropulse_signature": "[Base64-encoded Ed25519 signature over all fields above]"
}
```

### 3.2 Element types and permitted IDs

| Element ID | Type | UHDR source field | Max sensitivity (Δ) |
|---|---|---|---|
| `eeg_alpha_ratio` | FLOAT [0,1] | Alpha band power / total power | 1.0 |
| `eeg_theta_ratio` | FLOAT [0,1] | Theta band power / total power | 1.0 |
| `eeg_delta_ratio` | FLOAT [0,1] | Delta band power / total power | 1.0 |
| `eeg_gamma_ratio` | FLOAT [0,1] | Gamma band power / total power | 1.0 |
| `hrv_rmssd_ms` | FLOAT [0,300] | HRV RMSSD per session | 200.0 |
| `hrv_coherence` | FLOAT [0,10] | HRV coherence score | 10.0 |
| `session_count` | INT | Total sessions to date | 1.0 |
| `session_count_tier` | ENUM | See §5.2 | — (quasi-identifier) |
| `protocol_category` | ENUM | Protocol type coarsened | — (sensitive attribute) |
| `pbm_dose_j_cm2` | FLOAT [0,60] | Per-zone PBM dose | 60.0 |
| `age_decile` | ENUM | User-entered age bucket | — (quasi-identifier) |

**Hard limit:** A study descriptor may include at most 12 elements. The firmware rejects descriptors exceeding this limit.

**Prohibited elements:** The following UHDR fields must never appear in any study descriptor. The firmware rejects any descriptor containing them:
- Raw EEG waveforms (any channel)
- Session timestamps (resolved finer than 1-week intervals)
- PPG optical signal
- Closed-loop adaptation event details
- Impedance raw values
- Eye-open/closed state

---

## 4. Date Rounding

Applied as the first transformation, before all other processing.

```c
/* Rounds timestamp to the nearest multiple of rounding_interval_s */
time_t np_anon_round_date(time_t ts, uint32_t rounding_interval_s) {
    return (ts / rounding_interval_s) * rounding_interval_s;
}
```

The `date_rounding_days` field in the descriptor is converted to seconds at parse time. Minimum enforced by firmware: 7 days (604800 seconds). Descriptors specifying finer rounding are rejected.

Rounded dates appear in the output extract only. The original UHDR timestamps are never modified and never leave the device.

---

## 5. k-Anonymity and l-Diversity (np_anon_kgroup)

### 5.1 Quasi-identifier grouping

Records are grouped by the combination of quasi-identifier values. The quasi-identifiers are specified in the descriptor's `quasi_identifiers` array and must be drawn from the permitted element list (§3.2).

```c
/* np_anon_kgroup.h */

/* Maximum records in a study cohort (Scratch partition budget) */
#define NP_ANON_MAX_RECORDS     2000

typedef struct {
    uint8_t  qi_hash[16];       /* MD5 of concatenated QI values — grouping key */
    uint16_t record_idx;        /* index into the working record array */
} np_anon_qi_entry_t;

/* Sort by qi_hash, then scan for groups < k_min and mark suppressed */
np_status_t np_anon_kgroup_apply(
    np_anon_record_t *records,
    uint16_t          n_records,
    uint8_t           k_min,
    uint8_t           l_min,
    const char **     sensitive_attr_ids,
    uint8_t           n_sensitive_attrs
);
```

### 5.2 Coarsened quasi-identifier values

Before grouping, continuous QI values are coarsened to reduce the number of unique groups:

| Element | Coarsening rule |
|---|---|
| `age_decile` | User-entered age bracket: 18–29, 30–39, 40–49, 50–59, 60–69, 70+ |
| `session_count_tier` | 1–10 sessions, 11–50 sessions, 51–200 sessions, 201+ sessions |

Age decile is collected from the user during onboarding as a voluntary bracket (not DOB). It is stored in UHDR as a categorical value and never as a precise date of birth.

### 5.3 l-Diversity check

For each k-group, the engine verifies that each sensitive attribute has at least `l_min` distinct values:

```c
/* Returns NP_OK if group passes l-diversity; NP_ERR_LDIVERSITY if not */
np_status_t np_anon_ldiversity_check(
    const np_anon_record_t *group_records,
    uint16_t                group_size,
    uint8_t                 l_min,
    const char *            sensitive_attr_id
);
```

Groups failing l-diversity are suppressed (all records excluded from the extract). Suppression is logged to SHDR: `anon_groups_suppressed: uint16` (count only — no group content). If more than 50% of records are suppressed, the entire extract is aborted and the task returns `NP_ERR_ANON_SUPPRESSION_EXCESSIVE`.

---

## 6. Differential Privacy — Laplace Mechanism (np_anon_dp)

### 6.1 Privacy budget allocation

The total ε budget (from descriptor `epsilon_total`, maximum 1.0) is divided equally among all numeric elements in the descriptor. If the descriptor contains N numeric elements:

```c
float epsilon_per_feature = epsilon_total / n_numeric_elements;
```

**Example:** 5 numeric elements, ε_total = 1.0 → ε_per_feature = 0.2

The budget is allocated at parse time. Adding more elements to a descriptor automatically reduces the per-feature budget — there is no way to exceed the total budget.

### 6.2 Laplace noise generation

For each numeric element with sensitivity Δ and per-feature budget ε_f, a Laplace-distributed noise value is added:

```
noise = (Δ / ε_f) × sgn(u) × ln(1 − 2|u|)
```

where `u` is a uniform random variable in (−0.5, 0.5) exclusive, generated from the i.MX RT1062 TRNG peripheral (SNVS TRNG block).

```c
/* np_anon_dp.h */

/* Generates Laplace(0, scale) noise using TRNG.
   scale = sensitivity / epsilon_per_feature
   Returns noise value; caller adds to the feature value and clamps to [min, max]. */
float np_anon_dp_laplace_noise(float scale);

/* Apply DP noise to a record's numeric element, clamping to valid range */
np_status_t np_anon_dp_apply(
    float *value,           /* in/out: feature value, modified in place */
    float  sensitivity,     /* L1 sensitivity Δ of this feature */
    float  epsilon_feature, /* per-feature privacy budget */
    float  value_min,       /* minimum valid value for clamping */
    float  value_max        /* maximum valid value for clamping */
);
```

Post-noise values are clamped to the valid range of the feature (e.g., ratios to [0, 1], RMSSD to [0, 300]). Clamping after noise addition does not meaningfully reduce privacy guarantees at the sensitivity values specified.

### 6.3 Categorical/ENUM elements

ENUM elements (protocol_category, session_count_tier, age_decile) are not subject to Laplace noise. They contribute to the quasi-identifier grouping (k-anonymity) and l-diversity check, which together provide their privacy protection. No additional randomisation is applied to enum values.

### 6.4 Nonce and TRNG requirements

Each call to `np_anon_dp_laplace_noise` draws fresh bytes from the TRNG. The TRNG is seeded from the SNVS entropy source at system boot. If the TRNG health test fails, `np_anon_dp_laplace_noise` returns `NP_ERR_TRNG_HEALTH` and the anonymisation task is aborted.

---

## 7. Output Format

### 7.1 Extract record schema

```c
typedef struct {
    /* All numeric values have already had DP noise applied */
    float    eeg_alpha_ratio;       /* [0,1] or NAN if not in descriptor */
    float    eeg_theta_ratio;       /* [0,1] or NAN if not in descriptor */
    float    eeg_delta_ratio;       /* [0,1] or NAN if not in descriptor */
    float    eeg_gamma_ratio;       /* [0,1] or NAN if not in descriptor */
    float    hrv_rmssd_ms;          /* [0,300] or NAN if not in descriptor */
    float    hrv_coherence;         /* [0,10] or NAN if not in descriptor */
    uint16_t session_count;         /* integer; DP noise applied (rounded to int) */
    uint8_t  session_count_tier;    /* enum 0-3: 1-10/11-50/51-200/201+ */
    uint8_t  protocol_category;     /* enum 0-7: see §3.2 */
    float    pbm_dose_j_cm2;        /* [0,60] or NAN */
    uint8_t  age_decile;            /* enum 0-5: 18-29/.../70+ */
    uint32_t week_ordinal;          /* session week, date-rounded (not a timestamp) */
    uint8_t  suppressed;            /* 1 if this record was suppressed; included as padding */
} np_anon_extract_record_t;
```

Suppressed records are included in the transmission as zeroed dummy records to prevent record count itself from revealing suppression patterns (traffic analysis protection). The `suppressed` flag is set to 1 for suppressed records and the researcher's analysis pipeline must filter them.

### 7.2 Extract envelope

The extract is wrapped in a signed envelope:

```json
{
  "study_id": "NP-STUDY-001",
  "device_warranty_token": "[opaque 256-bit token — not linked to user identity]",
  "extract_timestamp_week": "2027-W11",
  "n_records_total": 150,
  "n_records_suppressed": 8,
  "k_applied": 10,
  "l_applied": 3,
  "epsilon_applied": 1.0,
  "records_b64": "[Base64-encoded AES-256-GCM ciphertext of records array]",
  "records_nonce": "[Base64-encoded 12-byte GCM nonce]",
  "records_tag": "[Base64-encoded 16-byte GCM tag]",
  "device_signature": "[Ed25519 signature over all above fields, device key]"
}
```

The `records_b64` field is encrypted with the study public key from the descriptor (NaCl box or AES-256-GCM with ECDH-derived key). Only the researcher with the corresponding private key can decrypt the records. NeuroPulse cannot read the extract content even in transit.

---

## 8. Privacy Budget Governance

### 8.1 Per-device budget tracking

The engine tracks cumulative privacy budget spent per study across all sessions:

```c
/* Config partition — per-study budget record */
typedef struct {
    char     study_id[32];
    float    epsilon_spent;   /* cumulative ε spent across all extracts for this study */
    uint32_t extract_count;   /* number of extracts generated */
} np_anon_budget_record_t;
```

If `epsilon_spent` would exceed 1.0 for a given study after a new extract, the engine refuses to generate the extract and returns `NP_ERR_BUDGET_EXHAUSTED`. The user is notified; they can re-consent to a fresh study (which starts a new budget record) or withdraw consent entirely.

### 8.2 Consent withdrawal

When the user withdraws research consent, the engine:
1. Immediately stops processing any pending study descriptors
2. Zeroes all study budget records in the Config partition for that study
3. Issues `NP_ANON_CONSENT_WITHDRAWN` SHDR event (no study content — flag only)

Per the research architecture, this makes future data flows impossible. Already-transmitted extracts cannot be recalled (disclosed at L3 consent and per-project invitation).

---

## 9. FAI Requirements

| FAI ID | Test | Requirement | Method |
|---|---|---|---|
| FAI-ANON-01 | k-anonymity enforcement | All output groups have ≥ k_min records | Generate 500 synthetic UHDR records; run engine with k_min=10; verify no group <10 in output |
| FAI-ANON-02 | l-diversity enforcement | All groups have ≥ l_min distinct values per sensitive attribute | Construct test dataset with known diversity distribution; verify suppression of non-diverse groups |
| FAI-ANON-03 | Differential privacy budget | ε_spent ≤ ε_total after extraction | Run extraction with 10 numeric elements at ε=1.0; verify budget record shows ε_spent ≤ 1.0 |
| FAI-ANON-04 | DP noise calibration | Empirical sensitivity matches theoretical | Run 10,000 trials; measure empirical L1 sensitivity; verify within 5% of Δ/ε_f |
| FAI-ANON-05 | Raw waveform prohibition | No raw EEG waveform bytes in output | Descriptor requesting raw EEG field is rejected; firmware returns NP_ERR_ELEMENT_PROHIBITED |
| FAI-ANON-06 | Date rounding enforcement | No timestamp finer than 1-week resolution in output | All week_ordinal values align to ISO week boundaries; no day/hour/minute granularity |
| FAI-ANON-07 | Output encryption | Researcher private key required to read records | Attempt decrypt without private key; verify failure |
| FAI-ANON-08 | Consent withdrawal | No extract generated after withdrawal | Withdraw consent; attempt extraction; verify NP_ANON_CONSENT_WITHDRAWN and no extract |
| FAI-ANON-09 | Adversarial re-identification | Attempt to re-identify known synthetic individual | Apply known-background-knowledge attack; verify failure with ≥95% confidence |

FAI-ANON-09 uses synthetic test data with known ground truth. The adversarial attack combines the extract with a synthetic "public dataset" containing 1,000 records with overlapping quasi-identifiers. The test passes if the target individual cannot be identified with probability > 0.05 (i.e., at most 1 in 20 re-identification attempts succeeds). This test requires the external DP reviewer (STEP-11/31).

---

## 10. Open Items

| OI-ID | Description | Blocking for |
|---|---|---|
| OI-ANON-01 | External DP reviewer sign-off on ε=1.0 and Δ values (per element) | FAI-ANON-04, FAI-ANON-09; STEP-11 completion |
| OI-ANON-02 | TRNG health test integration with SNVS entropy source on i.MX RT1062 | FAI-ANON-03; first firmware build |
| OI-ANON-03 | study_public_key scheme selection — NaCl box vs AES-256-GCM+ECDH | Output format finalisation |
| OI-ANON-04 | age_decile collection in app onboarding — voluntary bracket, no DOB | App spec (NP-APP-ROADMAP-001 Rev B) |
| OI-ANON-05 | Expert Determination certification template (NP-ANON-CERT) finalised | STEP-32; IRB protocol approval |
