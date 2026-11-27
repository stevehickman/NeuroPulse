# OI-PRIV-81: NP-API-001's session messages name fields that §5.1 classes UHDR

**Status:** DRAFT, put to the user in the session of 2026-11-27 · **From:** the session working `OI-PRIV-42` · **To:** the principal
**GitHub issue:** [#1105](https://github.com/stevehickman/NeuroPulse/issues/1105)

## What is already decided

`CLAUDE.md` §5.1: NeurOne never accesses UHDR (User Health Data Record), and a field that may tell us something about
the person is UHDR. `docs/reference/data-architecture-detail.md` §5.1 lists what UHDR holds. That list includes
session timestamps and duration, protocol parameters used, closed-loop adaptation events, PBM dose per zone,
neurofeedback scores and the HRV time series. The in-session electrode impedance check and everything derived from it
was ruled UHDR on 2026-11-23 (`OI-PRIV-40`).

On 2026-11-27 the user chose to make the simulator's device API strict. It now sends no UHDR value
(`NP-SIM-FW-001` `REQ-SIMFW-30`, `OI-PRIV-42`):

- `SESSION_COMPLETE` is `{session_id, uhdr_written}`.
- `TELEMETRY` is `{session_id, phase, hub_temp_c}`.

The simulator has no NP-API-001 §2.3 gate, so a gated field has nothing to pass.

## The conflict

NP-API-001 Rev 1 (`docs/np_api_001.md`) is the T2 WebSocket session-control API. It lists fields that §5.1 classes
UHDR:

| Message | Field | §5.1 classes it as | NP-API-001 says |
|---------|-------|--------------------|-----------------|
| `SESSION_COMPLETE` (§4.3) | `duration_s` | session duration | ungated |
| `TELEMETRY` (§4.4) | `ts_ms`, `elapsed_s` | session timestamps and duration | ungated |
| `TELEMETRY` (§4.4) | `impedance_ok` | the in-session impedance check (`OI-PRIV-40`) | ungated |
| `TELEMETRY` (§4.4) | `bes_charge_uc_cm2` | protocol parameters used (the commanded dose) | ungated |
| `TELEMETRY` (§4.4) | `adaptation_events_since_last` | closed-loop adaptation events | ungated |
| `TELEMETRY` (§4.4) | `pbm_dose_j_cm2` | PBM dose per zone | ungated (its privacy note leaves it out) |
| `TELEMETRY` (§4.4) | `eeg_bands`, `hrv_rmssd_ms`, `coherence_x100` | EEG, HRV | behind the §2.3 three-condition gate |

`hub_temp_c` is the hub's temperature. It is off the scalp, but no demonstration on file shows that it carries no
user biology (`OI-NVRAM-24`). The simulator sends it as `null`.

Today the simulator and NP-API-001 disagree. A T2 app written against the simulator never receives the fields in the
table, and an app written against NP-API-001 expects them.

## Q1. How should NP-API-001 §4.3 and §4.4 be brought into line with §5.1?

- **A. Strict, as the simulator now is.** No UHDR value crosses the session API, gated or not. `SESSION_COMPLETE`
  loses `duration_s`. `TELEMETRY` keeps `session_id`, `phase` and `hub_temp_c` (the last once `OI-NVRAM-24` has a
  demonstration). A clinician's live view reads UHDR through the REST API's §2.3 gate, not over this socket.
  *Cost:* T2 has no live physiological stream over the socket, and NP-API-001 is revised.
- **B. Gate every UHDR field.** Every field in the table, `duration_s` and `ts_ms` included, moves behind §2.3. The
  simulator would then need a stand-in for the gate, off by default.
  *Cost:* the socket carries UHDR to a third party whenever the gate passes. The privacy note and the gate's per-session
  evaluation must be specified, because §2.3 is written for REST requests.
- **C. Rule some fields device data.** For example, decide that `elapsed_s` or `impedance_ok` is not UHDR on this path.
  *Cost:* each such field needs §5.1 rule 1's positive demonstration of no user biology content, which `OI-PRIV-40`
  already ruled against for the impedance check.

## What to send back

The option, or another; a short rationale; and your name, role and date. Until then the simulator stays strict,
because protection only goes up (`CLAUDE.md` §5.1).
