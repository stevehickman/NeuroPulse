# Batch Document Authoring Plan
**Date:** 2026-06-06  
**Units:** 3 independent parallel document authors

---

## Context

Three regulatory/engineering documents are blocking further development work:

1. **NP-FMEA-001** — The SW-01 Safety MCU unit-level FMEA is required by IEC 62304 §7.1 for Class C software. It must identify every software failure mode for SW01-M01..M08 with potential harm and response time. It is listed as a G1 gate item in NP-SW-001 §11 and NP-COORD-001.

2. **NP-API-001** — The T2 scripting API specification must exist before any T2 API code is written (NP-PRIV-REM-001 STEP-15, NP-COORD-001 G1 gate). It gates the entire T2 clinical scripting surface and must bake in the privacy requirements discovered during NP-PRIV-REM-001.

3. **NP-DT-001** — The Design Input/Output Traceability Matrix is required by 21 CFR §820.30 and ISO 13485:2016 §7.3 and is a G2 exit criterion (NP-DP-001 §6.4). Every design output must trace to a design input; every input must trace to verification evidence.

All three documents are independent of each other and can be authored in parallel.

---

## Research Findings

### Unit 1 — SW-01 Safety MCU FMEA

**SW01-M01..M08 module inventory (from NP-SW-001 Rev A):**

| ID | Module | File | Primary function |
|----|--------|------|-----------------|
| SW01-M01 | Stimulation enable GPIO management | np_gpio_enable.c/.h | Owns all stim GPIO; hardware interlock state machine |
| SW01-M02 | SPI heartbeat watchdog | np_spi_watchdog.c/.h | 200ms heartbeat from SW-02; 1.5s timeout → all-cutoff <50ms |
| SW01-M03 | Charge density monitor | np_charge_density.c/.h | Integrates charge per electrode; aborts at 95% of 40µC/cm² |
| SW01-M04 | Thermal interlock | np_thermal.c/.h | NTC ADC per zone; throttles at 62°C junction |
| SW01-M05 | Cervical VNS cardiac interlock | np_cvns_interlock.c/.h | R-peak GPIO; rolling HR window; >15 BPM → cutoff <5.1ms |
| SW01-M06 | Impedance check | np_impedance.c/.h | 1kHz AC before session enable; blocks if out of range |
| SW01-M07 | Session protocol signature verification | np_session_sig.c/.h | Ed25519 verify before any GPIO enable |
| SW01-M08 | Fault latch and fault log | np_fault.c/.h | Latch fault; log to SHDR via SPI; require app clear to re-enable |

**Response time specs:** SPI watchdog cutoff ≤50ms; cervical VNS cardiac cutoff <5.1ms (spec ≤100ms); photoparoxysmal ≤200ms (SW-02 owned); charge density cutoff within one PWM period <25µs; impedance block is synchronous (before any pulse).

**ISO 14971 severity scale:** S1 (negligible) → S5 (catastrophic/death); probability P1 (remote) → P5 (frequent).

### Unit 2 — T2 Scripting API Spec (NP-API-001)

**Key inputs:**
- 15 NPPS modality blocks (npps-reference.md): eeg, bes, tdcs, pbm_transcranial, pbm_intranasal, vns_hrv, audio, visual, tms, tacs, hd_tdcs, cervical_vns, pbm_1064nm, hrv_biofeedback, vibrotactile
- WebSocket message types from simulator/server/index.js: SESSION_START, ZONE_CONFIG, ACCESSORY_CONFIG, TELEMETRY, FAULT, SESSION_COMPLETE
- FHIR R4 resources (NP-INT-FHIR-001): NP-Patient, NP-Observation, NP-DiagnosticReport, NP-Procedure
- Privacy requirements (NP-PRIV-REM-001 STEP-15): SHDR-only default; UHDR requires explicit consent + API toggle + use-case scope; rate limits 1,000/hr / 10,000/day; 256-bit keys; 90-day expiry

**Scope:** REST API + WebSocket for T2 clinical orchestration. Not a consumer API — requires BAA. Independent security audit before any clinical key is issued (STEP-26).

### Unit 3 — Design Input/Output Traceability Matrix (NP-DT-001)

**Design input categories (NP-QMS-DC-001):**
- Performance requirements (stim params, EEG specs, PBM irradiance, connectivity, power)
- Safety requirements (hardware limits, thermal, watchdog timing, MPE, regulatory naming)
- Usability requirements (fit range, contact force, module extraction force, LED readability)
- Regulatory/standards requirements (IEC 60601-1, IEC 60601-2-10, IEC 62471, IEC 62133, FCC Part 15, IEC 62304, 21 CFR 820)
- Interface requirements (USB-C PD, BT 5.3 LE Audio GATT, FHIR R4)

**Design output types:** HW specs (NP-HW-*), FW specs (NP-FW-*), source code (firmware/), tooling specs (NP-TOOL-*), FAI checklists (NP-FAI-*), risk register (NP-RISK-001), labelling (TBD)

**Gate:** G2 exit criterion (NP-DP-001 §6.4). Month 6 target.

---

## Work Units

| # | Title | Output file | Description |
|---|-------|-------------|-------------|
| 1 | SW-01 Safety MCU FMEA | `docs/neuropulse_sw_fmea_001.md` | Author NP-FMEA-001 Rev A: unit-level FMEA for SW01-M01..M08 per IEC 62304 §7.1 Class C requirements |
| 2 | T2 Scripting API Spec | `docs/neuropulse_api_001.md` | Author NP-API-001 Rev A: T2 clinical scripting API specification covering REST + WebSocket, NPPS upload, FHIR data access, privacy requirements |
| 3 | Design I/O Traceability Matrix | `docs/neuropulse_dt_001.md` | Author NP-DT-001 Rev A: design input/output traceability matrix per 21 CFR §820.30 and ISO 13485:2016 §7.3 linking all requirements to verification evidence |

---

## E2E Verification

These are documentation files (markdown), not runnable code. Verification for each worker:

1. The output `.md` file exists and is valid GitHub-flavored markdown
2. The document number, revision (Rev A), and date (2026-06-06) appear in the frontmatter/header
3. All cross-references use correct NP-* document numbers (verify against DHF index)
4. Required sections are present (see worker prompts below)
5. Run: `grep -c "SW01-M0[1-8]" docs/neuropulse_sw_fmea_001.md` → should return ≥8 (for Unit 1)
6. Run: `grep -c "NP-API-001" docs/neuropulse_api_001.md` → should find the document self-reference (Unit 2)
7. Run: `wc -l docs/neuropulse_dt_001.md` → should be >200 lines (Unit 3)

---

## Codebase Conventions

- All documents use GitHub-flavored markdown
- Document number format: `NP-[TYPE]-[ABBREV]-[3-digit-seq]-[NNN]`; revision as `Rev A`, `Rev B`, etc.
- Date format: ISO 8601 `YYYY-MM-DD`
- Tables use `|` GFM pipe format
- No HTML except `<details>`, `<aside>`, `<callout>`
- Document file names follow pattern: `neuropulse_[abbreviated_doc_id]_001.md`
- All documents must reference CLAUDE.md as the authoritative design record
- Status indicator in header: `Status: DRAFT — pending review`
- All new documents must be added to `docs/` directory
- After authoring, update CLAUDE.md §14 (DOCUMENTS GENERATED table) and §13.4 (mark pending decision closed if applicable)

---

## Worker Instructions Template

*(Copied verbatim into each agent prompt)*

```
After you finish implementing the change:
1. **Code review** — Invoke the `Skill` tool with `skill: "code-review"` to find correctness bugs (it reports findings; it does not edit code). Fix any findings it surfaces before continuing.
2. **Run unit tests** — Run the project's test suite (check for package.json scripts, Makefile targets, or common commands like `npm test`, `bun test`, `pytest`, `go test`). If tests fail, fix them.
3. **Test end-to-end** — Follow the e2e test recipe from the coordinator's prompt (below). If the recipe says to skip e2e for this unit, skip it.
4. **Commit and push** — Commit all changes with a clear message, push the branch, and create a PR with `gh pr create`. Use a descriptive title. If `gh` is not available or the push fails, note it in your final message.
5. **Report** — End with a single line: `PR: <url>` so the coordinator can track it. If no PR was created, end with `PR: none — <reason>`.
```
