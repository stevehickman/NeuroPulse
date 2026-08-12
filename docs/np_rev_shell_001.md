# Shell Interconnect Design Review Record

**Project:** NeurOne
**Document:** NP-REV-SHELL-001
**Revision:** 1
**Date:** 2026-08-11
**Status:** DRAFT — open review; no item signed
**Effective Date:** — (opens when `NP-DRV-SHELL-002` reaches BASELINED)
**Author:** NeurOne Mechanical + Hardware Engineering
**Approved By:** —
**References:** **NP-DRV-SHELL-002 Rev 2 (the criteria of record — §5.1 REQ-SKT-01, §8.3 REQ-BR2-01…05, §9 REQ-EMI-03…11, §11 SH2-DRC-01…28)**; NP-DRV-SHELL-001 Rev 2 (superseded predecessor — the review instrument this replaces); NP-HW-HEXTILE-001 Rev 3 §7.1–7.2; NP-HW-HUB-001 Rev 3 §7; NP-RISK-004 Rev 1 §3; NP-ART-001 Rev 1; NP-QMS-DC-001 Rev 1 (record types); NP-COORD-001 G2; IPC-2223D; IEC 60068-2-21
**Related Issues:** —
**Gate:** **NP-COORD-001 G2 — shell tooling first cut. This record must be complete and signed before steel is cut.**
**IEC 62304 Class:** N/A
**Supersedes:** The design-review-record function of `NP-DRV-SHELL-001` Rev 2 §5 and §8 (23-item DRC checklist and eight-signatory sign-off block)
**Parent Document:** NP-DRV-SHELL-002 Rev 2

---

## 0. The question this document was created to answer

> *"Is there a set of routing requirements and a routing review document to replace
> `neurone_shell_fpc_routing_review.docx`? If not, create as needed."*

**Requirements: yes, they exist.** They are not missing and were not lost when
`NP-DRV-SHELL-001` was retired. They live in `NP-DRV-SHELL-002` Rev 2, restated rather than
inherited — §8.3 says so explicitly: *"The IPC-2223D basis in the retired §3 is
architecture-independent and carries over. Its consumers change, so the requirements are restated
rather than inherited."*

| Requirement family | Where | Count |
|---|---|---|
| Bend radius and flex | `NP-DRV-SHELL-002` §8.3 — **REQ-BR2-01 … REQ-BR2-05** | 5 |
| Socket contact array | §5.1.6 — **REQ-SKT-01** (two staggered rows, binding) | 1 |
| EMI, EEG integrity, shielding | §9 — **REQ-EMI-03, -04, -05, -06, -07, -08, -09, -10, -11** | 9 |
| Physical routing and aperture | §4.1–§4.3, incl. the segregated-return requirement at the posterior boss | 3 |
| Socket contact budget | §5.1.4 — 19 contacts, closed | 1 |

**Review: partially.** `NP-DRV-SHELL-002` §11 carries a 33-item checklist (SH2-DRC-01…28 plus
02a/02b/05a/10a/10b) with method, pass criterion and owning discipline for each. What did **not**
survive the retirement is the *record* instrument: the retired document's §8 held eight named
signatories and was the artifact that gated shell tooling first cut. A checklist without a record is
a list of intentions.

**This document is that record, and only that.** It holds no criteria of its own — duplicating them
would create a second source of truth that could drift from `NP-DRV-SHELL-002` silently, which is
exactly the failure mode §8 of that document exists to prevent. It holds reviewer, date, verdict and
evidence per item, and the sign-off block.

---

## 1. Status of the review

**This review cannot be closed yet.** `NP-DRV-SHELL-002` is `DRAFT` and its own §11 states the
condition: DRAFT → BASELINED additionally requires **REG-1** and **ACT-1** to close. Both are open.

Beyond that, three items in the checklist depend on decisions that have not been made, and are
marked `BLOCKED` rather than `FAIL` below — a distinction that matters, because a FAIL is a design
defect and a BLOCKED is a missing input:

| Item | Blocked on | Why it cannot be reviewed |
|---|---|---|
| SH2-DRC-13 | `OI-CONV-01` / `RISK-SHELL-03` | The item reads *"`SAFE_EN[n]` defaults LOW at Safety-MCU power-on reset"*. Whether LOW is the safe state is **the disagreement itself** (`NP-RISK-004` §1.3). Reviewing this item against either convention would record a pass for a state that is unsafe under the other. |
| SH2-DRC-17 | `OI-SHELL2-07` | Pass criterion is *"within cancellation-loop budget"*. The budget has not been set. |
| SH2-DRC-20 | `OI-HUB-C07` | Boss contact-group segregation depends on whether the cranial enable is per-cluster or broadcast; the broadcast option changes {N2/N5} from a star to a trunk, which changes the group structure and not merely its size. |

---

## 2. Review record

Verdict: **PASS** · **FAIL** (design defect — disposition required) · **BLOCKED** (missing input,
named) · **—** (not yet reviewed).

Criteria and methods are **not restated here**. Read each item in `NP-DRV-SHELL-002` §11.

| Item | Discipline | Reviewer | Date | Verdict | Evidence reference |
|---|---|---|---|---|---|
| SH2-DRC-01 | FW/ME | | | — | |
| SH2-DRC-02 | ME/EE | | | — | |
| SH2-DRC-02a | ME/FW | | | — | |
| SH2-DRC-02b | EE/FW | | | — | |
| SH2-DRC-03 | ME | | | — | |
| SH2-DRC-04 | ME | | | — | |
| SH2-DRC-05 | ME | | | — | |
| SH2-DRC-05a | ME/EE | | | — | |
| SH2-DRC-05b | EE | | | — | |
| SH2-DRC-06 | ME | | | — | |
| SH2-DRC-07 | ME | | | — | |
| SH2-DRC-08 | ME/EE | | | — | |
| SH2-DRC-09 | EE | | | — | |
| SH2-DRC-10 | ME/HFE | | | — | |
| SH2-DRC-10a | EE/ME | | | — | |
| SH2-DRC-10b | EE/FW | | | — | |
| SH2-DRC-11 | ME | | | — | |
| SH2-DRC-12 | EE/Safety | | | — | |
| **SH2-DRC-13** | FW | | | **BLOCKED** | `OI-CONV-01` — polarity disagreement unresolved |
| SH2-DRC-14 | EE/Safety | | | — | |
| SH2-DRC-15 | EE | | | — | |
| SH2-DRC-16 | EE | | | — | |
| **SH2-DRC-17** | EE | | | **BLOCKED** | `OI-SHELL2-07` — fluxgate self-field budget unset |
| SH2-DRC-18 | EE | | | — | |
| SH2-DRC-19 | EE | | | — | |
| **SH2-DRC-20** | EE/ME | | | **BLOCKED** | `OI-HUB-C07` — enable topology undecided |
| SH2-DRC-21 | FW | | | — | |
| SH2-DRC-22 | FW | | | — | |
| SH2-DRC-23 | FW | | | — | |
| SH2-DRC-24 | FW | | | — | |
| SH2-DRC-25 | EE/ME | | | — | |
| SH2-DRC-26 | EE/FW | | | — | |
| SH2-DRC-27 | EE | | | — | |
| SH2-DRC-28 | FW | | | — | |

**33 items.** The retired predecessor carried 23; `NP-DRV-SHELL-002` Rev 1 carried 28.

---

## 3. Mechanical-diff items

Three items must not be reviewed by reading. `NP-CONV-001` §8 is binding here: *"Cross-document
interface agreement is verified by mechanical diff, never by review"* — all three name collisions
that motivated that rule survived multiple human readings, because **a name mismatch reads as
agreement**.

| Item | What must be diffed | Against |
|---|---|---|
| **SH2-DRC-05b** | Socket pin table, pin for pin **and name for name** | `NP-HW-HEXTILE-001` §7.2 |
| **SH2-DRC-01** | Cluster partition — every socket covered exactly once | Generated from `scripts/sync-socket-map.ts` |
| **SH2-DRC-02a** | 18-cluster partition against the rendered map | `docs/diagrams/np_hextile_cluster_map.svg` |

A comparison script satisfying these must be **falsified before it is trusted** (`NP-CONV-001` §8):
perturb one entry in one table and confirm the check fails. A diff that passes against two identical
inputs has demonstrated nothing. Recorded as **OI-REVSH-02**.

---

## 4. Non-conformance and disposition

| Item | Observed | Disposition | Eng approval | Quality approval |
|---|---|---|---|---|
| | | | | |

No waiver without Engineering **and** Quality sign-off.

---

## 5. Sign-off

Shell tooling first cut is a **one-way decision** — the retired predecessor said so and it remains
true: *"Failure to specify the routing path before tooling is cut cannot be remediated without a
full shell retool."* Signatories below attest that every item they own carries a PASS with evidence,
or a dispositioned non-conformance.

| Discipline | Items owned | Name | Date | Signature |
|---|---|---|---|---|
| Mechanical Engineering | DRC-03, -04, -05, -05a, -06, -07, -08, -10, -10a, -11, -20, -25 | | | |
| Hardware / EE | DRC-02, -02b, -05a, -05b, -08, -09, -10a, -10b, -12, -14, -15, -16, -17, -18, -19, -20, -25, -26, -27 | | | |
| Firmware | DRC-01, -02a, -10b, -13, -21, -22, -23, -24, -26, -28 | | | |
| Safety | DRC-12, -13, -14 | | | |
| Human Factors | DRC-10 | | | |
| Thermal | (via `OI-SHELL2-11`, not a DRC item — see `NP-RISK-004` OI-RISK4-02) | | | |
| Quality | all; §4 disposition | | | |

**Review result:** ☐ CLOSED — tooling release approved ☐ CLOSED WITH DISPOSITION ☐ OPEN

---

## 6. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-REVSH-01** | This review cannot open until `NP-DRV-SHELL-002` reaches BASELINED, which requires **REG-1** (socket lattice registers to 10-20 against shell CAD) and **ACT-1** (active-surface boundary set). Reviewing a DRAFT would produce signed evidence against numbers the document itself calls provisional. | ME + Systems | **Shell tooling first cut** |
| **OI-REVSH-02** | Write the comparison script for the §3 mechanical-diff items and **falsify it** before first use — perturb one pin name and confirm the check fails. The repository already applies this pattern in `scripts/check-section-refs.ts`, gated in CI; that is the model. | EE + FW | SH2-DRC-05b |
| **OI-REVSH-03** | Confirm the §5 signatory list against final item ownership. It is derived from `NP-DRV-SHELL-002` §11's owner column, not from a programme-wide list — the retired document's fixed eight-signatory block referenced roles that no longer map to the work. | Quality | Sign-off validity |
| **OI-REVSH-04** | Decide whether the three BLOCKED items gate the whole review or only the affected subsystems. SH2-DRC-13 plausibly gates everything — an inverted cranial stimulation enable is not a subsystem question. | Safety + Quality | Review closure rule |

---

## 7. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-08-11 | NeurOne Mechanical + Hardware Engineering | Initial release. Restores the design-review **record** instrument lost when `NP-DRV-SHELL-001` Rev 2 was retired: that document's §5 checklist was replaced by `NP-DRV-SHELL-002` §11, but its §8 eight-signatory sign-off block — the artifact that actually gated shell tooling first cut — had no successor. §0 answers the question that prompted this document: the routing **requirements** exist and were restated rather than lost (REQ-BR2-01…05, REQ-SKT-01, REQ-EMI-03…11), and the review **checklist** exists (33 items); only the record was missing. **This document deliberately holds no criteria** — it records reviewer, date, verdict and evidence per item and points at `NP-DRV-SHELL-002` §11 for what is being judged, so the two cannot drift apart. Three items open as **BLOCKED** rather than unreviewed, each naming its missing input: SH2-DRC-13 (`OI-CONV-01` — reviewing it would record a pass for a state that is unsafe under the competing convention), SH2-DRC-17 (`OI-SHELL2-07`), SH2-DRC-20 (`OI-HUB-C07`). §3 marks the three items that must be verified by mechanical diff and requires the diff be falsified before it is trusted. Raises OI-REVSH-01…04. |
