# NeurOne Naming and Notation Conventions

**Project:** NeurOne
**Document:** NP-CONV-001
**Revision:** A
**Date:** 2026-08-11
**Status:** ACTIVE
**Effective Date:** 2026-08-11
**Author:** NeurOne Systems Engineering
**Approved By:** — (new document)
**References:** NP-HW-HEXTILE-001 Rev C §7.4; NP-DRV-SHELL-002 Rev B §5.1.7; NP-HW-HUB-001 Rev C; NP-FW-CVNS-001 Rev B §5.1; NP-FMEA-001 (OI-FMEA-01); `docs/FRONT_MATTER_TEMPLATES.md`; `docs/ABBREVIATIONS.md`
**Related Issues:** —
**Gate:** — (applies continuously; binding at schematic capture and at firmware pin-table authoring)
**IEC 62304 Class:** N/A (documentation convention; §3 has Class C consumers and is flagged accordingly)
**Supersedes:** None — first consolidated statement. Individual rules were previously stated inline in NP-HW-HEXTILE-001 §7.4 and NP-DRV-SHELL-002 §5.1.7, which now point here.
**Parent Document:** None

---

> **Why this document exists.** Three naming collisions reached the document set in a single week —
> `GUARD` against the DRL guard *plane*, `/ALERT` against `ALERT#`, and `SEAT_N` against a `_N`
> suffix that already meant *cardinality* in the firmware. **None was caught by reading; all three
> were caught by diffing tables mechanically.** A name collision is the rare defect that *reads as
> agreement*, so the countermeasure has to be a written rule plus a mechanical check, not care.
>
> **Scope.** These conventions bind **NeurOne-defined** identifiers: interface signal names, document
> IDs, requirement and open-item IDs, and section references. They do **not** bind vendor names —
> `SPI1_NSS`, `I2C_ISR_ALERT`, `SDA`/`SCL` and other peripheral or standards-body names are used
> exactly as their source defines them, because renaming them would break the correspondence to the
> datasheet. Where a vendor name violates a rule here, the vendor wins and the exception is noted.

---

## 1. Hardware signal names

### 1.1 Active-low signals — `#` suffix (BINDING)

> **Every active-low NeurOne signal name terminates with `#`.**

| Signal | Where | Polarity |
|---|---|---|
| `ALERT#` | socket pin 12, cluster tail | open-drain, wire-OR; asserted LOW on module fault |
| `SEAT#` | socket pin 19 | tied to `PGND` through 1 kΩ; LOW = fully seated |
| `ATTN#` | tier-0 cluster bus | open-drain; any controller pulls LOW to request service |

Active-**high** signals take no suffix. The absence of `#` is therefore meaningful, which is the
point: a reader can determine polarity from the name alone, and a name that carries the wrong
suffix is a visible defect rather than a silent one.

`PBM_CRANIAL_EN#` (the Class C cranial enable, `NP-HW-HUB-001` §3.1) also carries the suffix:
`np_safety_config.h:7-8` puts it among the ten active-LOW enable GPIOs.

> **Historical records keep the name they were written with.** Revision-history rows, the DHF index
> (`np_dhf_001.md`), and `docs/status/completed-decisions.md` are append-only records of what was
> true when written. Renaming inside them would destroy the evidence of how a name changed, which is
> the thing those files exist to preserve. Rename forward, never backward.

### 1.2 Suffixes that must NOT be used for polarity, and why each is already taken

This is the load-bearing table. Every entry below was proposed at some point and rejected on
evidence, not taste.

| Suffix | **Already means** | Evidence | Verdict |
|---|---|---|---|
| `_N` | **cardinality** — "number of" | `PBM_TILE_N`, `EEG_TILE_N` in `firmware/hub_control/tests/np_module_map_tests.c` are `sizeof(x)/sizeof(x[0])` | **Forbidden for polarity.** `SEAT_N` read as "number of seats" |
| `_L` | **Left** (anatomical / bilateral) | `CVNS_ENABLE_L` / `CVNS_ENABLE_R` in `NP-FW-CVNS-001` §5.1 are the left and right electrode drivers | **Forbidden for polarity** |
| `_R` | **Right** | as above | **Forbidden for polarity** |
| `_B` | **channel B** | `CH_B`, `LED_B`, `CUR_B`, `DUTY_B`, `NP_BANK_B`, `NP_PBM1064_REG_*_B` | **Forbidden for polarity** ("bar" is a common convention elsewhere; it is not available here) |
| `_LOW` | **a low threshold or a low setting** | `NP_ADAPT_TRIGGER_EEG_ALPHA_LOW`, `NP_TIA_GAIN_LOW` | **Forbidden for polarity** |
| leading `/` | **hierarchical path separator** in EDA net naming (KiCad, Altium) | a net named `/ALERT` reads as a root-scope path and can be silently re-scoped or duplicated on netlist import | **Forbidden** |

### 1.3 Index and instance notation

> **An index takes bracket notation: `SIGNAL[n]`. A trailing `_n` must never denote an index**, because
> lowercase `_n` is visually indistinguishable from a polarity marker in most fonts and in every
> plain-text diff.

`SAFE_EN[n]` is the per-cluster safety enable for cluster *n*. It was written `SAFE_EN_n` through
NP-DRV-SHELL-002 Rev B, where the `_n` was an index and repeatedly read as a polarity flag.

### 1.4 The polarity of a signal is part of its specification, not its implementation

Because §1.1 makes the name carry the polarity, **a name change is a specification change** and a
polarity disagreement becomes visible at the name. That is the intended effect — see
**OI-CONV-01**, which this convention exposed rather than created.

---

## 2. Firmware identifiers

`#` is not a legal C identifier character, so a signal name cannot travel unchanged from a schematic
into firmware. The mapping is fixed here rather than left to whoever writes the driver:

> **`<SIGNAL>#` maps to `<SIGNAL>_ACTIVE_LOW` in firmware identifiers.**
>
> `ALERT#` → `..._ALERT_ACTIVE_LOW` · `SEAT#` → `..._SEAT_ACTIVE_LOW` · `ATTN#` → `..._ATTN_ACTIVE_LOW`

**Why the long form, and not something terser.** Every short candidate is taken (§1.2) — that is
the first reason and it is sufficient. The second is that verbosity is a *feature* here: this is a
safety-relevant marker on a device where **NP-FMEA-001 OI-FMEA-01** already records unstated
polarity as a hazard, in those words — *"unstated — and on an active-LOW enable line that is a
safety question."* A name that cannot be misread is the correct trade against a name that is short.
These identifiers are also rare; there are a handful, not hundreds.

**Correction of record.** NP-HW-HEXTILE-001 §7.4 and NP-DRV-SHELL-002 §5.1.7 initially specified
`_L` for this mapping. **That was wrong** — `_L` already means *Left* in `NP-FW-CVNS-001` §5.1. Both
documents now point here.

---

## 3. What this document does NOT change (Class C boundary)

**No firmware identifier is renamed by this document, and none should be renamed to satisfy it
without a change order.**

The safety MCU's ten stimulation enable lines are all active-LOW and none carries polarity in its
name — `NP_EN_PBM_CRANIAL_PIN`, `NP_EN_BES_PIN`, `NP_EN_TDCS_PIN` and the rest encode it only in
header comments (`np_safety_config.h:7`, `np_gpio_mgr.c:5`). That is IEC 62304 **Class C** code.
Renaming it is a Class C change with verification consequences, it is already owned by
**OI-FMEA-01**, and it is out of scope here. §2 exists so that *new* names converge, and so that
OI-FMEA-01 has a convention to adopt if it is ever closed.

---

## 4. Document identifiers and files

| Item | Convention | Example |
|---|---|---|
| Document ID | `NP-<DOMAIN>-<NNN>` | `NP-HW-HEXTILE-001`, `NP-DRV-SHELL-002` |
| Filename | lower-snake of the ID, `.md` | `docs/np_hw_hextile_001.md` |
| **New number vs new revision** | **New number** when the *architecture is replaced*; **new revision** when decisions are *inherited* | `NP-DRV-SHELL-002` took a new number from `-001` (architecture replaced); `NP-HW-FPC-001 Rev E` layered on Rev D (inherited) |
| Revision | single uppercase letter, in the `**Revision:**` field only — **never in the title** | `**Revision:** C` |
| Status | `DRAFT` · `ACTIVE` · `BASELINED` · `SUPERSEDED` · `ARCHIVED` | |
| Front matter | first 12 fields fixed and ordered | `docs/FRONT_MATTER_TEMPLATES.md` is normative |

## 5. Section references

> **`§N` with no space** — `§7.2`, `§8.2.1`, `§5.1.7`. Never `§ 7.2`.

Cross-document references name the document first: `` `NP-HW-HUB-001` §7.4 ``.

## 6. Identifier families

| Family | Form | Meaning | Numbering |
|---|---|---|---|
| Open item | `OI-<DOC>-<NN>` | unresolved question owned by that document | append-only; **never renumber**; closed items are struck through and retained, never deleted |
| Design review check | `<PREFIX>-DRC-<NN>` | verification item (`SH2-DRC-`, `HT-DRC-`, `HUB-DRC-`) | append-only; suffix letters for insertions (`SH2-DRC-02a`) |
| Requirement | `REQ-<AREA>-<NN>` | binding requirement | `REQ-EMI-06`, `REQ-BR2-01`, `REQ-SKT-01` |
| Decision | `D-<n>` | numbered decision inside one document | `D-3`, `D-7` |
| Principal decision | `<NAME>-<n>` | standing cross-document decision | `CLUSTER-1`, `SYM-1`, `CONTIG-1`, `SMART-1` |
| Gate | `<NAME>-<n>` | programme gate | `REG-1`, `MECH-2`, `THERM-1a` |

**Why append-only numbering matters.** Renumbering an open item silently breaks every cross-document
reference to it, and those references are the only mechanism keeping this document set consistent.

## 7. Revising a locked position

When a document replaces one of its own earlier positions, **state what it was, what replaced it,
and why** — do not overwrite silently. The pattern of record is `NP-HW-HEXTILE-001` Rev B's
cluster-count correction banner: a table of *was → is → cause*, then the surviving rationale.

A superseded rationale that was **outweighed rather than refuted** is marked as such and retained.
`NP-HW-HEXTILE-001` D-4 is the reference example.

## 8. Verification

> **Cross-document interface agreement is verified by mechanical diff, never by review.**

`SH2-DRC-05b` and `HT-DRC-23` both require the socket pin tables to be compared **name for name**
programmatically. This is a rule because all three collisions in the header note above survived
multiple human readings — a name mismatch reads as agreement.

A probe that compares two tables must be **falsified before it is trusted**: perturb one entry and
confirm the check fails.

---

## 9. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-CONV-01** | **`SAFE_EN[n]` polarity disagrees with the safety-MCU firmware, and §1.1 made it visible.** `NP-DRV-SHELL-002` §6 specifies **LOW = rail removed = disabled** (so `SAFE_EN[n]` is active-**HIGH**, and SH2-DRC-13's "defaults LOW at reset" is the *safe* state). The safety MCU specifies the opposite for its enable lines: *"Active-LOW open-drain: **LOW = stimulation enabled**; HIGH = disabled. Power loss or reset [→ disabled]"* (`np_safety_config.h:7-8`, `np_safety_main.c:14`). **Both are internally coherent and fail-safe on their own terms; together they are inverted.** The hazard is the undocumented mismatch: anyone implementing `SAFE_EN[n]` to the safety MCU's house convention would make LOW *enable* the cluster, turning SH2-DRC-13's "default LOW at reset" from the safe state into **stimulation enabled at power-on reset**. Naming has been applied **as the documents currently state polarity** — `SAFE_EN[n]` carries no `#` because SHELL-002 declares it active-high — so **resolving this item may require a rename to `SAFE_EN#[n]`**. Not resolved here: this is a safety-architecture question, not a naming one. **The inversion is now visible in `NP-HW-HUB-001` §3.1's own block diagram**, where an active-low `PBM_CRANIAL_EN#` from the safety MCU feeds a tier whose per-cluster gate is declared active-high — which is precisely the effect §1.4 intends | Safety + EE Lead | **Cluster-carrier schematic; Hub PCB Rev C.** Assess with **OI-FMEA-01** and **OI-HUB-C07** |
| **OI-CONV-02** | Sweep the remaining document set against §1 and §5. This revision covers the socket/cluster interface documents (`NP-HW-HEXTILE-001`, `NP-DRV-SHELL-002`, `NP-HW-HUB-001`); the modality, firmware and app specs have not been audited for active-low signals carrying no `#` | Systems | Documentation consistency; not tooling-blocking |

---

## 10. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| A | 2026-08-11 | NeurOne Systems Engineering | Initial release. Consolidates rules previously stated inline in `NP-HW-HEXTILE-001` §7.4 and `NP-DRV-SHELL-002` §5.1.7. **Binding rule: all active-low NeurOne signals terminate with `#`** (§1.1), applied to `ALERT#`, `SEAT#` and newly to `ATTN#`. §1.2 records the five suffixes and one prefix that are unavailable for polarity, each with the evidence that took it. §1.3 introduces `SIGNAL[n]` index notation, replacing `SAFE_EN_n`. **§2 corrects the firmware mapping from `_L` to `_ACTIVE_LOW`** — `_L` already means *Left* in `NP-FW-CVNS-001` §5.1, which the original mapping missed. §3 states the Class C boundary: no firmware identifier is renamed, and OI-FMEA-01 keeps ownership of the unmarked safety-MCU enable lines. §4–§8 record document-ID, section-marking, identifier-family, revision-practice and verification conventions already in use but never written down. **Raises OI-CONV-01** — applying §1.1 exposed an inverted `SAFE_EN[n]` polarity between `NP-DRV-SHELL-002` §6 and the safety-MCU firmware, which is a safety-architecture question and is not resolved here. |
