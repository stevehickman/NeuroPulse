# Superseded Documents

Every document in this directory has been replaced. None of it is a design input. It is retained
because `21 CFR §820.30(j)` requires the design history to be reconstructable, and because several
of these documents are cited by name in decisions that are still live — deleting them would break
the reasoning trail, not just the link.

**Do not use anything here for new design, tooling, procurement or firmware work.** Each entry
below names its successor. Where a document was only *partly* retired, the surviving part is named
too, because "superseded" has been used loosely in this project and the distinction matters when
someone is looking for the last place a number was derived.

## Rules that apply inside this directory

- **Revision labels are not converted.** `NP-CONV-001` §4.1 moved the active set from letters to
  integers on 2026-08-11. Documents here keep the label they were written with, under §1.1 —
  *rename forward, never backward*. §4.1's mapping table (`A`=1 … `AA`=27) resolves any citation.
- **Filenames ARE normalised — these files are named for their serial, like every other controlled
  document.** `NP-CONV-001` §4.0 binds retired documents too, and §4.3 records why the earlier
  exemption was wrong: §1.1 protects records of *what was written*, not addresses. A retired
  document is the case where deriving the address from the serial matters **most**, because it is
  the one nobody remembers the descriptive name of. `NP-RISK-001`'s ~38 inbound citations resolve to
  `np_risk_001.docx` by rule, not by lookup.
- **Former filenames are recorded below** so an external citation of the old descriptive name still
  leads somewhere. Inside git, these are renames — `git log --follow` traverses them.
- **Nothing here is edited except to add a supersession banner.** The text is a record of what was
  believed when it was written.
- **Superseded revisions are retained, not deleted.** `NP-CONV-001` §4.0.4: a document stays if it
  is referenced anywhere or if a process/legal duty requires it. `NP-QMS-001` §Records sets DHF
  retention at life of device + 2 years (21 CFR §820.180), and everything here is a design record
  indexed by `NP-DHF-001`, so both limbs say retain. `NP-HW-FPC-001` accordingly holds two files —
  Rev 4 (`.docx`) and Rev 5 (`.md`) — which is legitimate: §4.0 binds the filename, and each
  basename is the serial and nothing else.

## Index

### The 2026-07-15 architecture replacement

`NP-HEX-ZM-001` replaced the five position-unique zone-module slots (Zone 1–5 / ZM-01…ZM-05,
66 × 78 mm modules, 20-pin Hirose FH34S FPC tails, ZONE_ID resistor-ladder detection) with one
universal 40 mm hex-tile SKU tiling ~80 sockets, addressed by UID auto-inventory. Everything in
this group died with that decision.

| File | Document | Rev as written | Replaced by | Note |
|---|---|---|---|---|
| `np_fai_zm_001.docx` | **NP-FAI-ZM-001** | A | `NP-FAI-001` (programme + applicability), `NP-FAI-HUB-001` (first delivered checklist) | Inspected the zone-module FPC assembly. Its architecture-independent method — PDMS adhesion, thermal-cycling qualification, IPX4-after-service, accessibility — is carried forward in `NP-FAI-001` §4, not lost. |
| `np_risk_001.docx` | **NP-RISK-001** | C | `NP-RISK-002` (re-baseline + disposition of all 26), `NP-RISK-003`, `NP-RISK-004` | **This was the ISO 14971 baseline risk file**, not merely a stale document. Every one of RISK-01…RISK-26 has an explicit disposition in `NP-RISK-002` §3 (5 retired, 20 carried, 1 closed-confirmed). |
| `np_drv_shell_001.docx` | **NP-DRV-SHELL-001** | B | `NP-DRV-SHELL-002` (architecture + requirements + DRC), `NP-REV-SHELL-001` (the review record instrument) | Already carried a 2026-07-28 supersession banner. Its IPC-2223D bend-radius basis survives, re-derived, as `NP-DRV-SHELL-002` REQ-BR2-01…05. |
| `np_tool_zm_001.docx` | **NP-TOOL-ZM-001** | A | `NP-TOOL-HEXTILE-001` | Specified position-unique moulds (F-01 zone key, F-02 numeral, F-03 braille) that the type-agnostic socket makes meaningless. F-04–F-07 carry forward as concepts. |
| `np_hw_fpc_001.docx` | **NP-HW-FPC-001** | D | `NP-HW-HEXTILE-001` | Base-module FPC spec, 5 slots. Retained: referenced by `NP-DHF-001` and under the §820.180 DHF retention duty. |
| `np_hw_fpc_001.md` | **NP-HW-FPC-001** (1064 variant) | E | `NP-HW-HEXTILE-001`, tile type T1-C | Dual-PD architecture and InGaAs PD choice survive and are cited from `NP-DRV-SHELL-002` §13. |
| `np_tool_zm_sm_001.md` | **NP-TOOL-ZM-SM-001** | A | `NP-TOOL-HEXTILE-001` | Smart-module mould variant of a mould family that no longer exists. Its F-SM-03 mechanical key was **confirmed unnecessary** — SMART-1 makes every socket I2C/TIA-capable, so there is no wrong socket to key against. |
| `np_fw_za_001.md` | **NP-FW-ZA-001** | A | `firmware/hub_control/np_module_map.*`; `firmware/zone_announce/np_zone_notify.h` | Detection mechanism (ZONE_ID resistor ladder on FPC pin 18) fully retired. The audio/debounce implementation in `firmware/zone_announce/` is still live code and still builds; only its trigger changed. |

### Design brief lineage

Superseded by `np_db_005.docx` (NP-DB-005), which is the current brief.

| File | Document | Rev as written |
|---|---|---|
| `np_db_001.docx` | NP-DB-001 | 1 |
| `np_db_002.docx` | NP-DB-002 | 2 |
| `np_db_003.docx` | NP-DB-003 | 3 |
| `np_db_004.docx` | NP-DB-004 | 4 |

### Decomposed into per-topic specifications

| File | Document | Rev as written | Replaced by |
|---|---|---|---|
| `np_fw_req_001.docx` | NP-FW-REQ-001 | A | `NP-FW-PBM1064-001`, `NP-FW-HRV-001`, `NP-FW-CVNS-001`, `NP-FW-HD-001`, `NP-FW-EMMC-001` |
| `np_mod_ext_001.docx` | NP-MOD-EXT-001 | A | the six per-modality specifications listed in `docs/status/document-register.md` |

## Former filenames

Every file here was renamed to its serial on 2026-08-11 (`NP-CONV-001` §4.0). If you arrived with
an old name from an external citation, this is where it went.

| Former filename | Now | Serial |
|---|---|---|
| `neurone_fai_zone_module.docx` | `np_fai_zm_001.docx` | NP-FAI-ZM-001 |
| `neurone_fpc_zone_module_risks_revA.docx` | `np_risk_001.docx` | NP-RISK-001 |
| `neurone_shell_fpc_routing_review.docx` | `np_drv_shell_001.docx` | NP-DRV-SHELL-001 |
| `neurone_tool_zone_module_001.docx` | `np_tool_zm_001.docx` | NP-TOOL-ZM-001 |
| `neurone_fpc_zone_module_spec_revA.docx` | `np_hw_fpc_001.docx` | NP-HW-FPC-001 (Rev 4) |
| `neurone_fw_requirements_001_superseded.docx` | `np_fw_req_001.docx` | NP-FW-REQ-001 |
| `neurone_additional_modalities_superseded.docx` | `np_mod_ext_001.docx` | NP-MOD-EXT-001 |
| `neurone_design_brief_superseded.docx` | `np_db_001.docx` | NP-DB-001 |
| `neurone_design_brief_r2_superseded.docx` | `np_db_002.docx` | NP-DB-002 |
| `neurone_brief_r3_superseded.docx` | `np_db_003.docx` | NP-DB-003 |
| `neurone_brief_r4_superseded.docx` | `np_db_004.docx` | NP-DB-004 |

> **`NP-HW-FPC-001` holds two files here, and that is correct.** `np_hw_fpc_001.docx` is Rev 4;
> `np_hw_fpc_001.md` is Rev 5. Both are retained under `NP-CONV-001` §4.0.4 — each is referenced
> from `NP-DHF-001` and covered by the §820.180 DHF retention duty. §4.0 binds the *filename*, and
> both satisfy it: the basename is the serial and nothing else.
>
> Rev 4 of the conventions briefly held the opposite ("one serial, one file") and deleted the
> `.docx`; §4.0.4 records the reversal and the two claims that were wrong. **OI-CONV-05 closed.**

## What is deliberately *not* here

Three active documents are architecture-coupled to the retired zone module but have **not** been
moved, because moving them would leave a live artifact with no governing document at all. Each is
tracked instead in `NP-ART-001` §5:

- **`np_tool_shell_001.docx` (NP-TOOL-SHELL-001)** — its F-01 is "zone slot plug anchor posts
  (×5, colour-coded)", and both its parent documents are now in this directory. It is the only
  tooling specification the headset shell has.
- **`np_coord_001.docx` (NP-COORD-001)** — titled *Zone Module FPC
  Engineering Coordination Checklist*; its G1/G2/G3 gate structure is scoped to an assembly that no
  longer exists, but those gates are cited as the release gates for documents that *are* current.
- **`np_proc_fpc_001.docx` (NP-PROC-FPC-001)** — specifies the Hirose FH34S
  20-pin connector and the RA-copper/Vf-binning requirements for a tailed FPC. Hex tiles have no
  tail; the LED requirements survive.
