# Hex-Tile Module — Risk Register and Problem Analysis

**Project:** NeurOne
**Document:** NP-RISK-003
**Revision:** 1
**Date:** 2026-08-11
**Status:** ACTIVE
**Effective Date:** 2026-08-11
**Author:** NeurOne Quality (interim: Steve Hickman, CEO)
**Approved By:** Steve Hickman, CEO
**References:** NP-RISK-002 Rev 1 §3 (disposition source); NP-RM-001 Rev 1 §4 (scales); NP-HEX-ZM-001 Rev 2 §3, §4a, §6, §7; NP-HW-HEXTILE-001 Rev 3 §3–§7, D-3, OI-HEXTILE-01…17; NP-TOOL-HEXTILE-001 Rev 1; NP-FAI-001 Rev 1 §4–§5; NP-ART-001 Rev 1 §2.1; NP-OPT-PSF-001 Rev 1; NP-THERM-BEZEL-001 Rev 1 §4.5; ISO 14971:2019
**Related Issues:** —
**Gate:** NP-COORD-001 G2 (pre-tooling)
**IEC 62304 Class:** N/A
**Supersedes:** Partially supersedes `NP-RISK-001` Rev 3 — the tile-facing subset, per `NP-RISK-002` §3
**Parent Document:** NP-RISK-002 Rev 1

---

**Artifacts covered:** **A1** hex-tile module shell (one universal 40 mm mould, all types) ·
**A2** hex-tile FPC and element population (T1-A base PBM, T1-B EEG/electrode, T1-C 1064 nm smart,
T2-D 1170 nm laser).

**Not covered:** the socket the tile plugs into (A3) and everything behind it — `NP-RISK-004`. The
boundary is the tile's back-face pad array: the tile owns its pads, the L1 laminate owns the socket.

---

## 1. Problem analysis

### 1.1 The governing change: one part, thirty times

The retired zone module was **five large parts, each unique**. The hex tile is **one small part,
repeated ~30 times on a ~80-socket lattice**. Every risk below is a consequence of that trade, and
the trade is not symmetric — it buys enormously on inventory, tooling and mis-insertion, and it
pays on everything that scales with *count of interfaces*.

| Property | Retired zone module | Hex tile | Direction |
|---|---|---|---|
| Distinct moulds | 5 (+1 smart variant) | **1** | strongly better |
| Mis-insertion hazard | keying in 5 layers (RISK-15) | **structurally impossible** — type-agnostic socket | eliminated |
| Perimeter seals per headset | 5 | **~30** | **worse** |
| Emitting-face area per part | 66 × 78 mm | 40 mm hex | worse for baffle/PD packing |
| Dynamic-flex paths | 5 × 3 bend segments | **0** | eliminated |
| Parts to inspect at FAI | 5 unique | 1 design × 30 | better |

Two of those rows are the whole risk story for this artifact. **Seal count went up 6×** and
**emitting-face area went down ~3×** while the same PD1, PD2, baffle, NTC and gasket groove must all
still fit on it.

### 1.2 The compounding problem: a small face with a fixed furniture list

The retired module's optical furniture — a baffled PD1 with a 1.0 mm aperture and 2.0–3.0 mm height,
a PD2 aperture at the array centre, an NTC, and a continuous 2.5 mm gasket bead — was sized against
a 5,148 mm² face. A 40 mm hex has an area of ~1,039 mm² before the bezel takes its share, and the
bezel decision itself is unsettled (**OI-HEXTILE-01**: 2.5 mm in `NP-HW-HEXTILE-001` §3, 1.0 mm in
`NP-THERM-BEZEL-001` §4.5, with 1.0 mm directed on 2026-08-11 and not yet propagated).

At 2.5 mm bezel the active field is **14.5 % smaller** than at 1.0 mm. That single unpropagated
decision moves every irradiance figure, the emitter count, the baffle's share of the face, and
therefore the accept criteria of an FAI checklist that cannot yet be written. It is the highest-
leverage open item on this artifact and it is not a hard question — it is an unpropagated answer.

### 1.3 Why the seal-count increase is not a linear scaling

IPX4 for the assembled headset is a **series** property: one leaking seal fails the claim. Going
from 5 seals to ~30 does not multiply the risk by 6 — it multiplies the number of independent
opportunities for a single non-conforming gasket, insertion, or rim finish to fail the whole device.
Meanwhile the *service* exposure multiplies too: `NP-FAI-001` §5's qualification grows from 50 swaps
to ~300.

`NP-HEX-ZM-001` §6 states the requirement — *"a per-tile seam-length budget is required"* — and it
has not been set. That is `RISK-HEX-01`.

### 1.4 What the architecture removed, and why it matters more than what it added

`RISK-11`'s retirement deserves stating plainly, because it is easy to lose in a list of
carried-forward hazards. The retired design's dominant failure mode was **fatigue cracking at a
stiffener edge, catastrophic and immediate, over 1,000 module swap cycles**, on a 20-pin FPC that
every swap actuated through three bend segments. `NP-DRV-SHELL-002` §8.2 makes the module
interconnect's dynamic-flex set **empty**. A whole class of latent, delayed, service-induced failure
is gone — not mitigated, gone.

---

## 2. Risk register

Scales per `NP-RM-001` §4. Status: **MITIGATED** (controls in place, residual acceptable) ·
**OPEN** (no adequate control yet) · **ALARP** (control selected, verification outstanding).

| ID | Sev | Hazard | Cause | Consequence | Control | Owner | Status |
|---|---|---|---|---|---|---|---|
| **RISK-03** | HIGH | 400 mW/cm² peak irradiance has no regulatory opinion | Never commissioned; open since 2026-05-06 | Emitter drive current cannot be finalised; a lower approved ceiling is a business risk, not a hardware one | Hardware sized for the worst case (180 mA / 25 % duty); any lower ceiling makes every spec easier. **No hardware action available** | CEO / Regulatory Counsel | **OPEN — external** |
| **RISK-04** | HIGH | PDMS–PI bond delaminates under thermal cycling | 15:1 CTE mismatch (300 vs 18 ppm/°C); plain O₂ plasma gives only 7–12 N/m | Dose metering measures an air gap, not tissue; fouling protection lost; tile replacement | 75 nm SiO₂ interlayer → 174–860 N/m; per-batch screen FAI-M02/M03; **200-cycle qualification FAI-TC02 [BLOCKING]** | HW EE / Mfg Eng | **ALARP — FAI-TC02 never run** |
| **RISK-06** | HIGH | PD1 sees direct LED light → dose metering invalid | No baffle, or baffle too short; edge coupling | Reported J/cm² wrong; window fouling undetected. **This is the primary product differentiator — a wrong measurement makes the claim fraudulent** | Baffle wall (`NP-TOOL-HEXTILE-001` F-TH-04) + RTV perimeter seal + synchronous lock-in detection; first-article opaque-tape test < 1 % of LED-on signal | HW EE / Optical | **MITIGATED — harder on a 40 mm face** |
| **RISK-08** | HIGH | Emitter V_f spread degrades current matching | V_f varies ±50–100 mV within a bin | Overdriven strings run hot; underdriven deliver sub-therapeutic dose; metering unreliable | V_f binning per lot with certificate. **Regressed: `OI-HEXTILE-02` has un-selected the emitters, so there is no part to bin** | HW EE / Procurement | **OPEN — control lost to OI-HEXTILE-02** |
| **RISK-02** | HIGH | Tile power conductors undersized | Stack-up, trace width and copper weight unspecified for a 24 V / 1.04 A tile | Localised heating, copper fatigue, open circuit | 24 V rail (D-6) quarters I²R vs the retired 12 V assumption. **`OI-HEXTILE-12` leaves the stack-up unspecified** | HW EE | **OPEN at the new numbers** |
| **RISK-05** | HIGH | LED string current hogging without per-string regulation | V_f mismatch across parallel strings | Thermal runaway on the low-V_f string; sub-dose on the others | Moved on-module by **D-3**; register map and local NTC throttle are `OI-HEXTILE-07` | HW EE | **CARRIED — control not yet specified** |
| **RISK-14** | HIGH | One PD cannot separate LED ageing from window fouling | Both present as reduced measured output | Wrong dose reported, in the direction of over-delivery | Dual PD (PD1 forward, PD2 backscatter); ratio separates the two in firmware. **PD2 aperture position must be re-derived for the 40 mm face** — the retired X=33.0/Y=39.0 mm was a 66 × 78 mm array centre | HW EE | **MITIGATED — geometry re-derivation open** |
| **RISK-16** | **HIGH** *(was LOW)* | IPX4 fails after user field replacement | ~30 co-moulded gaskets in series; no seam-length budget | Water ingress to a tile PCB or the socket array | Per-tile gasket + groove/primer retention; `NP-FAI-001` FAI-IPX-01…05, **FAI-IPX-02 [BLOCKING]**. **Severity raised here: exposure grew 6× and the budget is unset** | ME | **OPEN — see RISK-HEX-01** |
| **RISK-19** | HIGH | Gasket delaminates from the tile body | Adhesion failure under repeated insertion | Loss of seal → RISK-16 | 0.5 mm undercut retention groove + silicone primer; 50-cycle extraction test | ME | **MITIGATED** |
| **RISK-23** | HIGH | A mandatory moulded feature is omitted before first cut | Cumulative feature complexity on one small part | Tooling modification: $15–40 k and 6–8 weeks per feature | Consolidated mandatory-feature checklist — `NP-TOOL-HEXTILE-001` §2 and its mould design review | ME | **MITIGATED** |
| **RISK-HEX-01** | **HIGH** | No per-tile seam-length budget exists for the ~30-seal IPX4 claim | Requirement stated in `NP-HEX-ZM-001` §6, never set | The IPX4 claim rests on an unbudgeted series of seals | **None yet.** `NP-TOOL-HEXTILE-001` OI-THEX-03 owns it | ME + Quality | **OPEN — new** |
| **RISK-HEX-02** | MEDIUM | Optical furniture does not fit the 40 mm face at the final bezel | Baffle, PD1, PD2 aperture, NTC and gasket groove sized against a 5,148 mm² face; the hex face is ~1,039 mm² before bezel | Either emitter count falls below dose requirement, or the baffle is shortened and RISK-06 returns | Blocked on **OI-HEXTILE-01** (bezel 1.0 vs 2.5 mm) and **OI-HEXTILE-04** (intra-tile uniformity model) | ME + Optical | **OPEN — new** |
| **RISK-HEX-03** | MEDIUM | T1-B has no emitter count | Electrode pod diameter, and hence depopulated ring count, deferred (`OI-HEXTILE-05`) | PBM coverage at electrode sites is unquantified — the stated reason T1-B keeps PBM at all | `OI-HEXTILE-05`; T1-B layout deferred to a later `NP-HW-HEXTILE-001` revision | HW EE | **OPEN — new** |

---

## 3. Verification map

| Risk | Verified by | Where |
|---|---|---|
| RISK-04 | FAI-M01…M03, FAI-TC01…TC06 | `NP-FAI-001` §4 |
| RISK-16, RISK-HEX-01 | FAI-IPX-01…IPX-05 | `NP-FAI-001` §5 |
| RISK-06 | First-article optical isolation test; `NP-TOOL-HEXTILE-001` THEX-MDR-06 | `NP-TOOL-HEXTILE-001` §4 |
| RISK-19, RISK-23, RISK-HEX-02 | Mould design review, 12 items | `NP-TOOL-HEXTILE-001` §4 |
| RISK-02, RISK-05, RISK-08, RISK-HEX-03 | Blocked — no FAI checklist can be written | `NP-ART-001` §3.2 (`NP-FAI-HEXFPC-001`) |
| RISK-14 | Blocked — PD2 aperture position undefined for the hex face | `NP-ART-001` §3.2 |
| RISK-03 | No verification available; external | — |

---

## 4. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-RISK3-01** | **Propagate the 1.0 mm bezel decision into `NP-HW-HEXTILE-001` §3 (OI-HEXTILE-01).** This is not an engineering question — the value was directed on 2026-08-11 on the strength of the only calculated figure in the set (`NP-THERM-BEZEL-001` §4.5). Until it lands, the active field area, every irradiance figure, the emitter count and RISK-HEX-02 all sit on a 14.5 % ambiguity. | HW EE | **RISK-HEX-02; every tile FAI criterion** |
| **OI-RISK3-02** | Re-derive the PD2 aperture position for the 40 mm hex face. The retired coordinates are the geometric centre of a 66 × 78 mm array and are meaningless here; `NP-TOOL-ZM-001` F-04 marked them POSITION LOCKED, which is exactly the kind of inherited number that survives an architecture change unnoticed. | Optical + ME | RISK-14; mould geometry |
| **OI-RISK3-03** | Set the per-tile seam-length budget (RISK-HEX-01). | ME + Quality | IPX4 claim |
| **OI-RISK3-04** | Re-score RISK-16 formally against the `NP-RM-001` §4 scales. It is carried at HIGH here on the reasoning in §1.3; `NP-RISK-002` OI-RISK2-01 holds the systematic re-scoring, and this one should not wait for it. | Quality | ISO 14971 currency |

---

## 5. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-08-11 | NeurOne Quality | Initial release. Holds the tile-facing subset of the retired `NP-RISK-001` risk file per `NP-RISK-002` §3: RISK-02, -03, -04, -05, -06, -08, -14, -16, -19, -23 carried with their original IDs and meanings, plus three new hazards under prefixed IDs (RISK-HEX-01…03). **Problem analysis (§1) states the governing trade — one part repeated ~30 times instead of five unique parts — and identifies the two rows where it costs**: perimeter seals rise 5 → ~30 for a series IPX4 claim with no seam-length budget, and the emitting face shrinks ~3× while carrying the same optical furniture. **RISK-16 is carried at raised severity** (LOW → HIGH) on that basis. Records that the unpropagated bezel decision (OI-HEXTILE-01, 1.0 vs 2.5 mm, a 14.5 % swing in active area) is the highest-leverage open item on this artifact and is an unpropagated answer rather than an open question. Raises OI-RISK3-01…04. |
