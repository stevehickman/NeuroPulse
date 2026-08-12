# Module Identity, History Portability, and Fleet Characterisation Data Programme

**Project:** NeurOne
**Document:** NP-MOD-ID-001
**Revision:** A
**Date:** 2026-08-11
**Status:** DRAFT
**Effective Date:** TBD — pending principal approval and the two BLOCKING open items in §9
**Author:** NeurOne Firmware + Data Architecture
**Approved By:** Pending
**References:** NP-FW-EMMC-002 Rev A (§A warranty token, §B factory reset, §C UKMD), NP-HEX-ZM-001 Rev B (§4 UID inventory, §4a SMART-1), NP-HW-HEXTILE-001 Rev B (§6.2 on-module driver, D-3/D-4/D-7), NP-THERM-CFD-001 (§4 zone power map), NP-HW-HUB-001 Rev C (OI-HUB-C06), CLAUDE.md §5.1 (UHDR/SHDR boundary), §5.2 (predictive maintenance), §6.0 (two consent subjects), `firmware/hub_control/include/np_module_map.h`
**Related Issues:** PR #268 (SHDR schema Rev D), OI-EMMC2-08
**Gate:** NP-COORD-001 — new gate MOD-ID-1 proposed (§10)
**IEC 62304 Class:** SW-02 Class B (hub control + on-module firmware); no Class C surface
**Supersedes:** None
**Parent Document:** NP-FW-EMMC-002 Rev A

---

## 1. Purpose

SHDR schema Rev D (PR #268) re-keyed PBM and thermal fleet telemetry on
`(socket_number, module_uid)` so that a module's degradation history follows the
part rather than the position. That change opened **OI-EMMC2-08**, recorded as
BLOCKING: a persistent per-module hardware UID stored fleet-wide defeats the
factory-reset de-linking that `devices.device_transferred` exists to provide.

This document specifies the resolution, and in doing so replaces the binary the
open item was framed as. OI-EMMC2-08 offered two options — keep the raw UID and
accept a standing cross-device correlator, or store a per-device HMAC and lose
cross-device part history entirely. Both are wrong, because **cross-device part
history is not a continuous requirement.** It is required only at enumerable
custodial events, each of which has a legitimate authorising act attached. The
resolution is therefore to make linkage an *event* rather than a *state*.

This document also specifies the **fleet characterisation data programme** — a
time-boxed relaxation of the Rev D `DOSE-01` invariant, adopted so that the
question "does socket position drive module aging?" can be answered empirically
rather than assumed.

## 2. Scope

In scope: module identifier derivation and lifecycle; on-module non-volatile
history storage; history portability across devices and owners; the extended
SHDR field set collected during characterisation, its expiry, and its review
gate.

Out of scope: the SHDR schema DDL itself (a Rev E change, specified here but
implemented separately); the rotation optimiser algorithm; fleet-key transport
UX in the app; the statistical model implementation.

---

## 3. Decision summary

| ID | Decision | §  |
|---|---|---|
| **MODID-1** | SHDR stores `module_ref = HMAC-SHA256(fleet_key, module_uid)`, never the raw UID | §4 |
| **MODID-2** | `fleet_key` is scoped to a **warranty owner's enrolled device group**, not to a single device | §4.2 |
| **MODID-3** | `fleet_key` lives in the Config partition and is destroyed by factory reset, which rotates every ref on that device | §4.3 |
| **MODID-4** | Every module carries its own **net-history odometer** in the U1 MCU's EEPROM | §5 |
| **MODID-5** | History portability across owners is achieved by the odometer travelling **in the part** — no server hand-off, no linkage record, no transfer protocol | §6 |
| **MODID-6** | Carried-in baselines are **coarsened** before reaching SHDR, to break the counter-value join | §6.3 |
| **CHAR-1** | `DOSE-01` is relaxed for a time-boxed characterisation window to admit per-socket optical duty | §7 |
| **CHAR-2** | Expiry is **fail-closed in firmware**, not a server-side flag | §7.3 |
| **CHAR-3** | The extended set is **opt-in**, cohort-scoped; participants receive predictive maintenance first | §7.5 |
| **CHAR-4** | Non-participation must never degrade a **safety-critical** function — participation buys earlier and better maintenance, never baseline safety | §7.5.2 |

---

## 4. Fleet-scoped module reference (MODID-1 … MODID-3)

### 4.1 Derivation

```
module_ref = HMAC-SHA256(fleet_key, module_uid) truncated to 16 bytes
```

`module_uid` is the 8-byte (`NP_HEXMAP_UID_LEN`) UID each tile self-reports over
I2C — the identifier `np_module_map` already depends on for auto-inventory
(NP-HEX-ZM-001 §4, R-12) and for UID-derived dynamic I2C addressing
(NP-HW-HEXTILE-001 D-7). No new identity concept is introduced on the hardware
side; only what leaves the device changes.

Truncation to 16 bytes is a storage economy, not a security parameter: the
security property required is *unlinkability across fleets*, which rests on the
secrecy of `fleet_key`, not on digest length. 128 bits leaves collision
probability negligible at any plausible fleet-database size.

**The raw `module_uid` never leaves the device.** It exists on the module, on the
I2C bus inside the Faraday envelope, and in hub RAM. It is not written to the
SHDR partition and is not uploaded.

### 4.2 Why the scope is the owner's device group, not the device

A per-*device* key would make the same module produce a different ref on each of
one owner's devices, defeating the legitimate case where a multi-device owner
moves modules among their own helmets and expects accumulated history to keep
accumulating. That case involves no privacy boundary crossing at all: the
devices share a warranty owner, who is the consent subject for SHDR
(CLAUDE.md §6.0).

Scoping the key to the enrolled group therefore aligns the cryptographic
boundary with the **consent** boundary, which is the correct place for it:

| Boundary | Refs comparable? | Correct? |
|---|---|---|
| Within one owner's enrolled devices | Yes | Yes — one consent subject, history should accumulate |
| Across two owners (clinic ↔ home, resale) | **No** | Yes — this is the linkage OI-EMMC2-08 objects to |
| Before vs after factory reset | **No** | Yes — restores `device_transferred` de-linking |

### 4.3 Provisioning, enrolment, and rotation

- **Generation.** The first device of a fleet generates a 256-bit `fleet_key`
  from the i.MX RT1062 TRNG with NIST SP 800-90B health tests, on the same path
  already specified for the warranty token (NP-FW-EMMC-002 §A.2).
- **Storage.** Config partition, under the AES-256-XTS SHDR key — alongside the
  warranty token, and with the same properties.
- **Enrolment of an additional device.** App-mediated, both devices
  authenticated and present, owner-authorised. The key is copied to the joining
  device. This is the *only* mechanism by which a `fleet_key` propagates.
- **Rotation.** Factory reset step **R-7 zeroes the Config partition**
  (NP-FW-EMMC-002 §B.2), which destroys `fleet_key` along with the warranty
  token. R-9's new token is joined by a newly generated `fleet_key`. Every
  `module_ref` the device subsequently reports is therefore unrelated to any it
  reported before — **the ~80-way re-identification join that OI-EMMC2-08
  describes cannot be constructed.** No new reset step is required; the existing
  sequence already has the right shape.
- **Leaving a fleet.** A factory reset removes one device from its group. Its
  modules' refs change; the fleet's other devices are unaffected. This matches
  the existing `device_transferred` archive semantics exactly.

### 4.4 Residual: fleet size is itself a weak signal

A fleet whose devices share a key produces refs that co-occur across those
devices' rows, which reveals *that* those devices belong to one owner and how
many modules they share. This is a disclosure about an **organisation**, not a
person, and the warranty owner is the consenting party for it. It is materially
weaker than the cross-owner linkage it replaces. Recorded, not mitigated.

---

## 5. On-module odometer (MODID-4)

### 5.1 The enabling fact

NP-HW-HEXTILE-001 **D-3** fits the on-module driver to **every tile type**, not
only T1-C: `U1` is a tinyAVR 2-series MCU (ATtiny426/427-class) on every tile in
the BOM (§6.2). Tile identity is "UID self-report over I2C" (R-2), so every
module already contains an addressable microcontroller with non-volatile
storage.

The module can therefore keep its own odometer. This is not a new component, a
new pin, or a BOM change — it is unused capability in a part already specified.

### 5.2 Storage budget

ATtiny424/426/427 provides **128 bytes of EEPROM**, rated **100,000
write/erase cycles** with **40-year retention at 55 °C**
([datasheet DS40002311A](https://ww1.microchip.com/downloads/en/DeviceDoc/ATtiny424-426-427-824-826-827-DataSheet-DS40002311A.pdf)).

128 bytes is the binding constraint, and it is the reason the odometer holds
**net totals only, never history**. That constraint and the requirement agree:
what must travel with a part is its odometer reading, not its logbook.

Layout: **four 32-byte slots**, written round-robin.

| Field | Bytes | Notes |
|---|---|---|
| `magic` | 2 | record identification |
| `version` | 1 | format version |
| `seq` | 1 | monotonic; newest valid slot wins |
| `session_count` | 4 | uint32 |
| `mate_cycles_observed` | 2 | uint16; ≥500 rating, ample headroom |
| `emitter_on_seconds` | 4 | uint32 — see §7, characterisation-window field |
| `thermal_seconds_over_threshold` | 4 | uint32 |
| `peak_ntc_celsius_ever` | 1 | uint8, offset-encoded |
| `throttle_events` | 2 | uint16 |
| `manufacture_day` | 2 | uint16, days since 2026-01-01 |
| `pd_ratio_last` | 2 | uint16 fixed-point |
| `flags` | 1 | derate / retirement / mate-limit |
| `distinct_socket_count` | 1 | uint8 |
| `reserved` | 1 | must be zero |
| `crc32` | 4 | over UID ‖ payload — binds the record to its module |
| **Total** | **32** | ×4 slots = 128 B exactly |

The CRC covers the module's own UID as well as the payload, so a record
physically transplanted between modules fails validation rather than silently
transferring one part's history to another.

### 5.3 Write-endurance budget

Cadence is one update per session end.

| Case | Sessions/yr | Writes/slot/yr | Years to 100,000 |
|---|---|---|---|
| Home, 1/day | 365 | 91 | **1,096** |
| Home heavy, 3/day | 1,095 | 274 | **365** |
| Clinic, 12/day | 4,380 | 1,095 | **91** |

Even without slot rotation the clinic case reaches 22.8 years. **The four-slot
rotation is therefore not an endurance measure — endurance is a non-issue — it
is a crash-safety measure.** A write interrupted by power loss corrupts at most
one slot, and `newest valid CRC wins` recovers the previous good record. Stating
this plainly matters, because a future reviewer who reads the rotation as an
endurance workaround may "optimise" it away and remove the power-loss
protection with it.

### 5.4 Retention derating — OPEN

The 40-year retention figure is specified at 55 °C. EEPROM retention derates
with temperature, and a module's cell sits behind an emitter plane whose
junction throttles at 62 °C (CLAUDE.md §4.2). The module's actual
time-at-temperature distribution has not been checked against Microchip's
retention derating curve. **OI-MODID-03** (§9).

---

## 6. History portability (MODID-5, MODID-6)

### 6.1 The four cases

| Case | Mechanism | Linkage record created? |
|---|---|---|
| 1. Owner buys a new device, moves modules | New device reads the odometer on first insertion, seeds its local baseline | **None** |
| 2. Owner transfers modules to a different party | Identical to case 1 — the odometer is in the part | **None** |
| 3. Multi-device owner, one warranty owner | Shared `fleet_key` ⇒ same ref on every device; accumulation is automatic | **None** |
| 4. Data sufficiency for predictive maintenance | §7 characterisation programme | — |

**There is no transfer protocol, because none is needed.** Cases 1 and 2 were
originally framed as needing a "port" operation. They do not: the part carries
its own reading, so the receiving device learns the net history by reading the
module it is physically holding. No server mediation, no authorisation
handshake, and — critically — **no moment at which a value joinable to the
previous owner's records exists anywhere.**

What the receiving party learns is total sessions, total mate cycles, and a
degradation summary: device-condition data, disclosed to the new custodian of
the device, revealing nothing about the previous user's biology. This is the
used-part disclosure an odometer provides, and it is a feature.

### 6.2 SHDR representation

`module_life` gains carried-in baseline columns. The fleet-wide figure for a
module becomes `carried_in + Σ(per-device partials)`, which remains
order-free — the property §5 of the Rev D schema header depends on.

### 6.3 The counter-value join, and its mitigation (MODID-6)

**A residual the odometer introduces, which must not be missed.** If fleet A's
last reported partial for a module is 4,317 sessions and a new ref in fleet B
appears with `carried_in_session_count = 4,317`, those two rows can be joined on
the value itself. The cryptographic separation of refs is bypassed by the
arithmetic.

Mitigation: **carried-in baselines are coarsened before they reach SHDR.**
Proposed initial buckets — round down to the nearest 250 sessions and nearest 25
mate cycles. The device retains the exact value locally for its own reminder
logic; only the uploaded baseline is bucketed.

The cost is bounded: on a part with an expected life of several thousand
sessions, a 250-session bucket is a few percent of baseline error, applied once
at transfer and never compounding. The benefit is that the anonymity set for any
given bucket value grows with fleet size instead of being a near-unique
fingerprint.

Exact bucket widths are left open pending real fleet-size figures — a bucket
that is large relative to the fleet is wasted precision, and one that is small
is no mitigation. **OI-MODID-02** (§9).

---

## 7. Fleet characterisation data programme (CHAR-1, CHAR-2)

### 7.1 Why the relaxation was adopted

Rev D added `DOSE-01`, prohibiting any per-socket dose, energy, irradiance,
on-time or duty column in SHDR, on the reasoning that at 80-socket resolution
such a figure reconstructs a user's treatment geography, and treatment geography
implies indication.

That invariant also blocks the question it is now necessary to answer. Module
degradation cannot be attributed to socket position without an **exposure
denominator**: without one, "this socket is thermally punishing" and "this
socket simply gets driven more" are indistinguishable, and a rotation optimiser
trained on the difference is fitting noise.

The prior favours position mattering. Four mechanisms in the thermal work make
socket position load-bearing, and in every case the CFD **designs to worst case
rather than resolving per position** — correct for a safety limit, and precisely
why it cannot answer an aging question:

1. **Zone power map.** NP-THERM-CFD-001 §4 distributes heat per the zone power
   map and designs the worst-case (highest-flux) zone: 0.10–0.15 W/cm² at T1
   standard, 0.25–0.35 W/cm² at the T2 1170 zone.
2. **Perfusion.** Pennes blood perfusion is the heat sink and varies by scalp
   region; the CFD sweeps it as a `{low, mid, high}` band, not a per-socket map.
3. **Bezel standoff.** Optical modules stand 0.6–1.0 mm off the scalp; electrode
   modules protrude *past* the bezel, `s = 0` by design (NP-THERM-BEZEL-001) —
   a different thermal boundary condition, and module-kind-dependent.
4. **Fan convection.** Swept as a scalar `h = 25–100 W/m²·K`, not a
   per-position field; airflow over a curved bowl is not uniform.

### 7.2 Extended field set

Admitted to SHDR **for opted-in devices only** (§7.5), for the characterisation
window only, per socket per session:

- `emitter_on_seconds` — optical duty, the exposure denominator
- `thermal_seconds_over_threshold`
- `throttle_event_count`
- `peak_ntc_celsius` (already present in Rev D)

`DOSE-01` is **narrowed, not removed**: delivered *dose* in J/cm² and raw
irradiance remain prohibited, being direct readouts of the therapeutic quantity
rather than of drive time. The CI check is amended to allow the enumerated
fields above and continue rejecting everything else.

Because collection is opt-in (§7.5), the relaxation is **cohort-scoped**:
`DOSE-01` continues to hold in its Rev D form for every device that has not
opted in, which will be the large majority of the fleet. The narrowed invariant
describes what the schema *may* carry, not what any given device *does* carry.

### 7.3 Time-box and fail-closed expiry (CHAR-2)

**Window:** ends at the earlier of (a) N devices × M sessions sufficient for the
§7.4 model, or (b) 24 months from first fleet upload.

**Expiry is enforced in firmware by a build-time constant**, after which the
extended fields are not populated. It is deliberately *not* a server-side
collection flag: a flag that must be actively flipped off is a flag that stays
on. The device stops producing the data; the fleet DB does not merely stop
asking for it.

**Nothing is retroactive.** Data already collected cannot be un-collected. This
asymmetry is the reason the window is bounded at adoption rather than reviewed
"when convenient".

### 7.4 The review gate and its decision rule

At window close, fit degradation rate as a function of exposure, socket, and
module kind — socket as a random effect, exposure as a covariate:

```
degradation_rate ~ exposure + module_kind + (1 | socket)
```

| Outcome | Action |
|---|---|
| Socket variance component is a material fraction of total after controlling exposure | **Keep** socket, keep a duty denominator, size the retained precision to the effect |
| Socket variance component negligible | **Collapse** `socket_number` to don't-care or drop it, and drop the duty fields — this is the outcome the programme exists to license |
| Effect present but explained entirely by module kind | Keep `socket_part_type_wear`, drop socket-level duty |

The second outcome is the goal, not a disappointment: it would license reverting
to the strictest data set with evidence rather than assumption.

### 7.5 Consent: opt-in, with reciprocity (CHAR-3)

The extended set is collected **only from devices whose operator has
affirmatively opted in**, and participants receive predictive maintenance first
— earlier access to the Phase 2/3 personalised models their data trains
(CLAUDE.md §5.2).

This is the right structure, and not merely a softer one. The extended set is a
materially new category of SHDR content: an 80-socket duty map is
treatment-geography data, and the programme's own precedent classes far weaker
signals as health-adjacent — CLAUDE.md §5.1 holds that *a per-device count of
anonymisation validate-failures* is health-adjacent under WA MHMD / GDPR Art. 9
because it weakly signals something about the wearer. A duty map is a much
stronger signal than a failure count. Folding that into warranty registration
would have been bundled consent for a purpose the registrant never contemplated.
A separate, affirmative, granular, revocable opt-in with a stated benefit is what
the same reasoning asks for.

The reciprocity is genuine rather than decorative, and it should be described as
the honest exchange it is: the fleet model cannot personalise for a device whose
exposure it cannot see, so participants get better predictions *because* of what
they contributed, not as an inducement bolted on afterwards.

#### 7.5.1 Who may consent depends on how many people use the device

The opt-in binds to the **warranty owner**, consistent with the existing SHDR
consent subject (CLAUDE.md §6.0), in every configuration except one — a device
with a single wearer who did not register it. The reason the general case holds
is structural rather than incidental, and worth stating precisely, because the
exception falls out of the same reasoning.

**The determining variable is how many people use the device, not who bought
it.** A home device is not reliably single-user — households contain several
people and any subset may use the headset — and a single-user device is not
reliably worn by its registrant, since one may be bought and registered by a
family member for a child, partner or parent. Home-versus-clinic is a prior on
the question, not the question. The full taxonomy and its consequence for the
enrolment flow are in §A.2; the summary is:

- **Several users** (shared household device, or a multi-patient clinic device) —
  the duty stream is a mixture with no attribution path, so registrant-scoped
  consent is sound. This is the case the rest of this section analyses.
- **One user who is the registrant** — attributable, and the registrant is the
  subject, so their consent is the wearer's consent. Sound.
- **One user who is *not* the registrant** — attributable, and the registrant is
  **not** the subject. Registrant-scoped consent is **not** sufficient here, and
  the enrolment flow must route the decision to the wearer (§A.3.2).

**Multi-user devices — why registrant consent suffices.** §6.0 rightly holds
that a clinic has not consented on any patient's behalf, but that principle
bites on data *about a patient*, and for a mixed-use device the extended set is
not that. SHDR carries **no per-session subject identifier and no clock**,
verified against the live catalog of the Rev D schema:

| Probe | Result |
|---|---|
| Columns matching `user\|patient\|subject\|person\|tag\|session_id\|episode\|visit\|operator\|wearer` | **none** |
| Columns of type `timestamp`/`time`/`interval` | **none** |
| `date`-typed columns | `ingest_month`, `last_seen_month` (month-truncated retention anchors) and `manufacture_date`, `module_manufacture_date` (factory facts) |

A duty row is therefore `(warranty_token, session_index, socket_number,
module_ref, duty)` — a device, an **ordinal**, a position, and a number. On any
shared device — a clinic's, or a household's — the duty stream is a **mixture
over everyone who used it**, with nothing marking where one person's sessions end
and another's begin. Attributing a socket pattern to an individual would require
aligning session ordinals against an appointment book or a household routine, and
the timestamp that alignment needs does not exist: `ingest_month` is
month-granular by construction (Rev B timestamp minimisation), and
`session_index` is a counter, not a clock.

So the registrant of a shared device is not disclosing data about any wearer.
They are disclosing **device utilisation**, which for a clinic sits downstream of
clinical decisions the clinic already owns as its own record. That is a
disclosure the registrant is entitled to authorise.

Note what this argument does **not** cover: it depends on the mixture, so it
lapses entirely on a sole-wearer device. There the duty map resolves to one
person, and consent must come from that person — which is why the enrolment flow
asks before disclosing (§A.3) rather than assuming a configuration.

#### 7.5.1.1 INVARIANT — the two absences the consent model depends on

Owner-scoped consent is sufficient **because of** those two absences, which
makes them load-bearing rather than hygiene. Either one returning reopens the
question and would force a user-bound opt-in:

- **No sub-month clock in SHDR.** Enforced by CI check `TIME-01`.
- **No per-session subject identifier in SHDR.** Partially enforced by `PII-01`
  (`\buser_id\b`, `\bpatient_id\b`, `\bcustomer_id\b`).

Anyone removing or narrowing those checks must understand they are not tightening
data hygiene — they are removing the foundation of this consent model.

**A live gap in the second one.** CLAUDE.md §3 (T2 additions) already defines an
*"anonymized session tag: random session identifier for clinical multi-patient
environments — clinic holds patient-to-tag mapping"*. That is precisely the
column that would restore per-patient attribution on a shared device, and a
column named `session_tag` or `anon_session_tag` would **not** be caught by
`PII-01`'s current patterns. It is correctly absent from the schema today
(verified), but nothing stops it being added. **OI-MODID-07** proposes extending
the pattern list to close that. Note the tag is designed so *NeurOne* cannot
cross-reference it — but the *clinic* holds the mapping, and the clinic is the
party whose consent is standing in for the patient's here, so the tag's presence
would matter even though NeurOne could not resolve it alone.

#### 7.5.2 Non-coercion invariant (CHAR-4)

Participation may buy **earlier and better** maintenance prediction. It must
never buy **baseline safety**. Every safety-critical reminder and every
interlock in CLAUDE.md §4.2 and §5.2 applies identically to non-participants;
what they forgo is Phase 2/3 personalisation and early access, not protection.
A choice whose refusal degrades safety is not a free choice, and would make the
opt-in coercive in exactly the way the structure is meant to avoid.

#### 7.5.3 Withdrawal

Opting out stops extended-field collection from the next session. It does not
retroactively remove data already in the fleet model, and the enrolment text
must say so plainly — the same irreversibility notice the research-consent flow
already gives (CLAUDE.md §6.2 L3).

### 7.6 Selection bias — the methodological cost of opt-in

Opt-in buys consent legitimacy and costs statistical generalisability, and the
review gate must account for it rather than discover it afterwards.

The characterisation cohort is **self-selected**. Opted-in devices plausibly
differ systematically from the fleet: more engaged owners, heavier users, a
different clinic-to-home mix, possibly a different protocol mix. Any of those
correlates with socket duty, which is the exact covariate the §7.4 model
controls for.

Consequences for §7.4:

- The socket-position conclusion is estimated **on the cohort**, and its
  generalisation to the whole fleet is an assumption, not a result.
- Cohort composition must be compared against the fleet on the variables SHDR
  *does* hold for everyone — device model, session count, config tier,
  clinic-vs-individual registration — so the direction and rough size of any
  skew is known.
- A **negative** result (socket does not matter) is the more robust one under
  selection bias: if position fails to explain variance even in a cohort skewed
  toward heavy use, that conclusion travels to lighter users comfortably. A
  positive result sized on heavy users should not be extrapolated to the fleet
  without stating the assumption.

**OI-MODID-01 (BLOCKING)** — §9.

---

## 8. Impact

### 8.1 SHDR schema (a Rev E change, not made here)

| Change | Tables |
|---|---|
| `module_uid BYTEA(8)` → `module_ref BYTEA(16)` | `module_inventory`, `module_placement_events`, `module_life`, `pbm_module_telemetry`, `module_thermal_telemetry`, `module_rotation_advice` |
| Add `carried_in_*` baseline columns (coarsened) | `module_life` |
| Add extended exposure fields | `pbm_module_telemetry`, `module_thermal_telemetry` |
| Degenerate-value CHECKs re-derived for the ref domain | all of the above |

`socket_number` is **unchanged and retained** — that is the point of §7.

### 8.2 CI gates

- `MODUID-01` → **`MODREF-01`**: ref is `BYTEA`, length-checked to 16, degenerate
  values rejected; and a new assertion that **no `module_uid` column exists
  anywhere**, so the raw UID cannot reappear by a later edit.
- `DOSE-01`: amended to the §7.2 allow-list; continues to reject J/cm² and
  irradiance.
- New `CHAR-01`: the extended fields carry the characterisation-window marker,
  so they cannot outlive the programme unnoticed.
- Live constraint suite (`ci/shdr/shdr_schema_constraint_tests.sql`) extended to
  cover the new constraints, with reject- and accept-cases as established.

### 8.3 Firmware

| Area | Work |
|---|---|
| On-module (U1) | Odometer record, 4-slot rotation, CRC over UID ‖ payload, I2C read/write commands |
| `hub_control` | Odometer read at inventory, baseline seeding, coarsening before SHDR write |
| Config partition | `fleet_key` record; enrolment import/export |
| Factory reset | No new step — R-7 already destroys the key (§4.3). Add a **test** asserting it does |

### 8.4 What this closes

**OI-EMMC2-08 is resolved** by §4 + §6, and can move from BLOCKING to closed once
§9's open items land. The mate-cycle concern recorded alongside it in that open
item is unaffected and remains a live judgment.

---

## 9. Open items

| OI | Description | Blocking for |
|---|---|---|
| **OI-MODID-01** | Opt-in enrolment and withdrawal text. **DRAFTED — Appendix A.** Individual and institutional variants, withdrawal, and programme-close copy. Remaining to close: legal review, and OI-MODID-08 first (consent text cannot contradict the standing privacy notice) | **BLOCKING — before any characterisation-window collection** |
| **OI-MODID-08** | **NP-PRIV-NOTICE-001 Rev C §2 conflict.** The notice states SHDR *"contains no user biology and cannot identify you — it describes your device's condition only."* True of the standard set; **false for an opted-in single-user device**, where per-socket drive time is a record of which areas of that user's head are treated. Scope the §2 claim to non-participating devices and cross-reference the enrolment disclosure | **BLOCKING — must land BEFORE enrolment copy ships** |
| **OI-MODID-07** | Extend `PII-01` patterns to reject a per-session subject tag (`session_tag`, `anon_session_tag`, and the T2 anonymized-session-tag concept generally), closing the enforcement gap under the §7.5.1.1 invariant | Schema Rev E |
| **OI-MODID-06** | Cohort-vs-fleet skew analysis plan (§7.6) — which comparison variables, and the statement of assumption required before generalising a positive socket result beyond the cohort | Review gate (§7.4) |
| **OI-MODID-02** | Coarsening bucket widths for carried-in baselines (§6.3), sized against real fleet figures | Schema Rev E |
| **OI-MODID-03** | EEPROM retention derating at the module's actual time-at-temperature distribution vs the 55 °C / 40-year figure (§5.4) | Module firmware release |
| **OI-MODID-04** | `fleet_key` enrolment UX and its authentication requirements; behaviour when an owner sells one device out of a group | App + firmware |
| **OI-MODID-05** | N and M for the §7.3 window, and the effect-size threshold for the §7.4 decision rule | Programme start |

## 10. Verification

Proposed gate **MOD-ID-1** (NP-COORD-001):

| Check | Method |
|---|---|
| Raw `module_uid` never appears in an SHDR upload | Firmware test + `MODREF-01` CI assertion |
| Refs incomparable across fleets | Unit test: two keys, one UID, distinct refs |
| Factory reset rotates every ref | Integration test over the R-1…R-12 sequence |
| Odometer survives power loss mid-write | Injected power-fail at each slot offset; newest-valid-CRC recovery asserted |
| Odometer rejects a transplanted record | CRC-over-UID negative test |
| Carried-in baselines are coarsened on upload | Assert uploaded value ≡ 0 mod bucket width |
| Extended fields stop at window expiry | Build-time expiry test with a clock past the boundary |
| Extended fields absent without opt-in | Upload from a non-opted-in device asserted to carry none of §7.2 |
| **No per-session subject identifier in SHDR** | Catalog assertion over `information_schema.columns` — the §7.5.1.1 invariant, checked against the live schema, not the DDL text |
| **No sub-month clock in SHDR** | `TIME-01` plus the month-truncation CHECKs already in Rev D |
| Non-participation does not degrade safety | Assert every §4.2 interlock and safety-critical reminder fires identically with opt-in off |

---

## Appendix A — Enrolment and withdrawal copy (addresses OI-MODID-01)

Draft product copy for the §7.5 opt-in. Voice and structure follow
NP-PRIV-NOTICE-001. **One gated flow, not per-audience variants** — the
disclosure branches on how many people use the device, which is not knowable
from whether it was bought by a person or an institution (§A.2).

### A.1 Prerequisite — a conflict with the live privacy notice

**NP-PRIV-NOTICE-001 Rev C §2 currently states, of SHDR:**

> *"Your SHDR contains data about the device's condition, not about you"* … *"SHDR
> contains no user biology and cannot identify you — it describes your device's
> condition only."*

For the standard SHDR set that is accurate. **For an opted-in single-user
device it would no longer be**, because per-socket drive time on a device with
one user is a record of which areas of that user's head are treated and how
often. The notice would be making a promise the programme breaks.

This must be fixed **before**, not alongside, enrolment: consent text that
contradicts the standing privacy notice is not informed consent. See
**OI-MODID-08** (§9). The minimum change is to scope the §2 claim to
non-participating devices and cross-reference the enrolment disclosure.

### A.2 The axis is how many people use the device — not who bought it

An earlier draft of this appendix split the copy by audience, individual owner
versus clinic, on the assumption that a home device has one user. **That
assumption is false.** Households contain more than one person and any subset of
them may use the device; a home device can be single-user or shared, and the
purchaser is not necessarily the wearer.

Home-versus-clinic is therefore the wrong axis. It is a *prior* on the real
question, not the question. What determines whether the extended set is
attributable to a person is:

| Configuration | Duty map describes | Attributable? | Registrant = wearer? | Consent sound? |
|---|---|---|---|---|
| **One user, who registered the device** | that person's treatment pattern | **Yes** | Yes | **Yes** |
| **One user, who did *not* register it** — a device bought and registered by a family member for a child, partner, or parent | that person's treatment pattern | **Yes** | **No** | **No — the wearer must choose** |
| **Several users** — shared household device, or a multi-patient clinic device | mixed utilisation, unmarked | No (§7.5.1) | n/a | **Yes** |

The middle row is a genuine gap, and it is *structurally identical* to the clinic
single-patient case already flagged — a sole wearer who is not the consenting
party. The earlier draft caught it for clinics and missed it for homes, purely
because of the false single-user assumption.

**NeurOne cannot determine which row applies.** SHDR carries no user identifier
by design (§7.5.1), so nothing in the uploaded data distinguishes one person's
sessions from a household's. Only the registrant knows. The enrolment flow must
therefore *ask*, and branch the disclosure on the answer — which is what §A.3
does, replacing the two audience variants with one gated flow.

**The answer is an app-side gate only.** The household's user count must **not**
be written to SHDR. It is a fact about who lives in a house, it is not needed
after the gating decision, and a `household_user_count` column would be a new
disclosure introduced by the very mechanism meant to protect against one.

### A.3 Enrolment — screen 1, common to all registrants

> **Help us learn what actually wears your modules out**
>
> We want to find out whether *where* a module sits in the headset affects how
> fast it wears out. We genuinely don't know yet. Answering it needs detail we
> don't normally collect, so we're asking rather than assuming.
>
> **What we'd collect, per module, per session**
>
> - how many seconds each module's emitters were driven
> - how long each module spent above its warm threshold
> - how often the heat limiter engaged
> - the peak temperature each module reached
>
> **What it never includes**
>
> No brainwave or heart-rate data. No treatment dose. No names, email, or
> address. **No clock** — we record which session number something happened in,
> never the date or time of day. This cannot show *when* the device is used or
> build a picture of anyone's routine.
>
> **First, one question.** What this data can reveal depends entirely on how many
> people use this headset, so we need to ask before explaining the rest.
>
> **Who uses this headset?**
>
> ☐ **Only me**
> ☐ **Only one person, and it isn't me** — I bought or registered it for someone else
> ☐ **More than one person**

The three answers branch to §A.3.1, §A.3.2 and §A.3.3. The question is asked
because NeurOne cannot determine the answer from the data (§A.2), and it is
**not stored in SHDR**.

Institutional registrants see the same question phrased for their context —
"Only one patient / More than one patient" — and reach the same three branches.
There is no separate clinic flow, because a single-patient clinic device and a
single-user home device raise the identical issue.

#### A.3.1 "Only me" — the registrant is the sole wearer

> **What this shows about you — read this part**
>
> Because you are the only person using this headset, this data shows **which
> areas of your head you treat, and how much**. Someone reading it could form an
> impression of what you are using the device for. That is a real disclosure, and
> it is why we are asking rather than switching it on.
>
> **What you get.** Your device joins the group we can build personalised
> maintenance predictions for first. We can only predict wear for a device whose
> workload we can see — so this follows from taking part rather than being a
> reward for it. If too few people take part the personalised models may not
> arrive, and we will tell you either way.
>
> **What does not change.** Every safety feature works identically whether you
> take part or not, and every safety-critical alert reaches you the same way.
> Taking part buys earlier and better *maintenance* predictions. It never buys
> safety.
>
> **This ends by itself.** Collection stops automatically at the end of the study
> window — you don't have to remember to turn it off.
>
> ☐ **Yes, include my device**  ☐ **No thanks**
>
> "No thanks" changes nothing about how your device works.

#### A.3.2 "Only one person, and it isn't me" — the wearer must choose

> **This choice isn't yours to make — and that's deliberate**
>
> Because one person is the only user of this headset, the data would show
> **which areas of their head are treated, and how much**. Someone reading it
> could form an impression of what they are using it for.
>
> That is personal to them, not to whoever bought the device. So we need **their**
> decision, not yours.
>
> **What to do:** hand them the device, or send them this invitation, and they can
> decide from their own account. Nothing is collected until they do.
>
> **If they'd rather not decide, or cannot,** simply leave this device out of the
> study. It will work exactly as it does now.
>
> ☐ **Send them the invitation**  ☐ **Leave this device out**

This branch is the case the earlier audience-split draft missed for homes while
catching it for clinics (§A.2). It covers a device bought by a family member for
a child, partner or parent, and a clinic device assigned to one patient — the
same structure, the same answer.

#### A.3.3 "More than one person" — mixed use

> **Why this doesn't single anyone out**
>
> Because several people use this headset, what we receive is a single stream
> mixed across all of them, and nothing marks where one person's sessions end and
> the next begins:
>
> - There is **no user or session identifier** in this data.
> - There is **no clock** — sessions are numbered 1, 2, 3 … with no date or time
>   attached, so records cannot be matched against anyone's schedule or
>   appointment book.
>
> What you are sharing is how the *device* was used, not what any individual
> received. As the registrant, that is yours to authorise.
>
> **Please tell the others.** They can't be identified in this data, but they
> should know it is being collected. We'd rather you tell them than have them
> find out from a settings screen.
>
> **If that changes.** If the headset later becomes one person's alone, come back
> to this screen — the answer above stops being true, and we'll ask again.
>
> **What you get:** priority access to module rotation guidance and
> remaining-life estimates as they become available.
>
> **What does not change:** all safety interlocks and safety-critical alerts are
> identical whether or not a device takes part.
>
> ☐ **Yes, include this device**  ☐ **No thanks**

Institutional registrants additionally get **per-device control** — enrol a whole
fleet or select individual devices — and a reminder that any device assigned to a
single patient belongs in §A.3.2, not here.

### A.4 Household composition is not static

The gating answer is a point-in-time statement about a household or a clinic's
device assignment, and both change. A device answered "more than one person" can
become one person's alone; a device bought for someone else can be handed on.

Two consequences:

1. The §A.3.3 copy invites the registrant back if the situation changes, and the
   setting must remain reachable and editable — not a one-time onboarding
   question that can never be revisited.
2. Re-confirmation should be prompted at the natural boundary rather than on a
   timer. **Factory reset is the right trigger:** it already marks a change of
   custody (`device_transferred`, NP-FW-EMMC-002 §A.6), it already rotates
   `fleet_key` (§4.3), and enrolment should not survive it. A device coming out
   of reset starts un-enrolled and asks again.

### A.5 Withdrawal

> **You've left the maintenance study**
>
> Your device stops sending the extra detail from your next session onward.
>
> **What we cannot undo:** detail already contributed remains in the models it
> has already trained. We cannot remove one device's contribution from a model
> that has already learned from it. This is the same limitation that applies to
> research data, and it is why we mention it here rather than at the point you
> try to leave.
>
> **What returns to normal:** your device goes back to standard maintenance
> predictions — the same ones every non-participating device receives.
>
> **What never changed:** every safety feature and safety-critical alert. Those
> were never part of this.

### A.6 Programme close notice

> **The maintenance study has ended — thank you**
>
> Your device has stopped sending the extra detail. Nothing is required from you.
>
> **What we learned:** [result summary, including a null result if that is the
> finding.]

Publishing the outcome to participants — including a null result — mirrors the
research-consent L4 commitment (CLAUDE.md §6.2) and is the point at which a
"collect more now, prune later" programme demonstrates it meant the second half.

---

*NP-MOD-ID-001 Rev A is a DRAFT for principal review. It specifies no code and
changes no schema; implementation follows approval, OI-MODID-08 (privacy-notice
correction), and legal review of Appendix A.*
