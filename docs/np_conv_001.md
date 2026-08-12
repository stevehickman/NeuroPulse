# NeurOne Naming and Notation Conventions

**Project:** NeurOne
**Document:** NP-CONV-001
**Revision:** 6
**Date:** 2026-08-11
**Status:** ACTIVE
**Effective Date:** 2026-08-11
**Author:** NeurOne Systems Engineering
**Approved By:** — (new document)
**References:** NP-HW-HEXTILE-001 Rev 3 §7.4; NP-DRV-SHELL-002 Rev 2 §5.1.7; NP-HW-HUB-001 Rev 3; NP-FW-CVNS-001 Rev 2 §5.1; NP-FMEA-001 (OI-FMEA-01); `docs/FRONT_MATTER_TEMPLATES.md`; `docs/ABBREVIATIONS.md`
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
NP-DRV-SHELL-002 Rev 2, where the `_n` was an index and repeatedly read as a polarity flag.

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
| Filename | **the document serial, and nothing else** — lower-snake of the ID plus the extension (§4.0) | `docs/np_hw_hextile_001.md` |
| **New number vs new revision** | **New number** when the *architecture is replaced*; **new revision** when decisions are *inherited* | `NP-DRV-SHELL-002` took a new number from `-001` (architecture replaced); `NP-HW-FPC-001 Rev 5` layered on Rev 4 (inherited) |
| Revision | **positive integer**, in the `**Revision:**` field only — **never in the title, never in the filename** | `**Revision:** 3` |
| Status | `DRAFT` · `ACTIVE` · `BASELINED` · `SUPERSEDED` · `ARCHIVED` | |
| Superseded documents | live in `docs/superseded/`, indexed by `docs/superseded/README.md` | |
| Front matter | first 12 fields fixed and ordered | `docs/FRONT_MATTER_TEMPLATES.md` is normative |

### 4.0 The filename IS the document serial (BINDING, added at Rev 3)

> **A controlled document's filename is the lower-snake form of its document serial, plus the
> extension. Nothing more, nothing less, nothing else.**
>
> `NP-TOOL-HEXTILE-001` → `np_tool_hextile_001.md`. Not `np_tool_hextile.md`, not
> `np_hextile_tooling_001.md`, not `np_tool_hextile_001_rev2.md`, not
> `neurone_hextile_tooling.docx`.

**Why the filename carries the serial rather than a description.** The serial is the only part of a
document that never changes. Titles get rewritten, authors leave, status moves DRAFT → BASELINED →
SUPERSEDED, and the document itself gets superseded and moved. A reference that names any of those
things eventually dangles. This document set proves the point at scale: **~38 references to
`NP-RISK-001` still resolve today**, after that document was retired, re-baselined into three
successors, and moved to `docs/superseded/`. Had they cited "the Zone Module Risk Register", every
one would have broken on 2026-08-11.

Three further properties fall out of it, and they are the reason the rule is worth its cost:

| Property | What it buys |
|---|---|
| **Mechanically derivable** | Given `NP-FAI-HUB-001` in any citation, the file is `docs/np_fai_hub_001.md` without a lookup table, an index, or a search. |
| **Absence is detectable** | The set becomes enumerable, so "which artifacts have no tooling specification?" is answerable by listing `np_tool_*`. That is exactly how `NP-ART-001` §4 found nine artifacts with no owning document. You cannot detect a gap in a set with no naming scheme. |
| **A document can be named before it exists** | `NP-FAI-CVNS-001` was cited in three places and had never been written. Under this rule that is a *findable* absence, not an invisible one — it became `OI-ART-05`. |

#### 4.0.1 The rule is exclusive, not just prescriptive

> **Nothing that is not a serialed controlled document may have a filename matching
> `np_<domain>_<nnn>.<ext>`.**

A rule that says "documents look like this" is worth little if other things also look like this —
the pattern stops being evidence. So the converse binds too: if a file matches the serial shape, it
**is** a controlled document with that serial, and the claim is checkable by opening it and reading
the `**Document:**` field.

Two consequences, both applied on 2026-08-11:

- Files that *had* a serial but a descriptive filename were renamed to the serial —
  `neurone_shell_fpc_routing_review.docx` → `np_drv_shell_001.docx`,
  `neurone_supplier_selection_checklist.docx` → `np_proc_sup_001.docx`, and 23 others.
- A file that matched the serial shape but carried no serial had to be resolved rather than
  tolerated: `np_condition_links_001.md` looked like a serial while its front matter had no
  `**Document:**` field at all (it stated the revision inside `**Status:**`). Its real serial is
  `NP-COND-LINK-001`, so it became `np_cond_link_001.md` and gained the missing fields.

Files with no serial and no serial-shaped name are unaffected and need no serial:
`ABBREVIATIONS.md`, `FRONT_MATTER_TEMPLATES.md`, `np_hex_zm_isa.md`, `pbm_neuro_protocols.md`,
`docs/status/*`, `docs/reference/*`.

#### 4.0.1a Enforced by script, not by care

Per §8 — *a convention worth writing down is worth a script* — both halves are checked by
**`scripts/check-doc-filenames.ts`**:

```bash
bun scripts/check-doc-filenames.ts
```

It resolves each file's serial from the authority that is actually reliable for that file type: the
`**Document:**` field for `.md`, and the `NP-DHF-001` register row for `.docx`/`.pdf`. **That split
is not fastidiousness.** An earlier version of the check read the first `NP-…-NNN` token in a
`.docx` body and concluded the bibliography's serial was `NP-REG-PBM1064-001` — it had found a
*citation*, not the document's own ID, and it would have renamed nine files wrongly with complete
confidence.

The check was **falsified before it was trusted**, in both directions: renaming a conforming file
away from its serial trips A, and adding a serial-shaped file with no serial of record trips B.
A checker that has only ever been run against a passing tree has demonstrated nothing.

#### 4.0.2 Serial versus revision — they are different numbers

This is the distinction the rule depends on, and the two are easy to conflate because both are
digits attached to a document:

| | **Serial** (`-003`) | **Revision** (`Rev 3`) |
|---|---|---|
| Answers | *which document* | *which version of that document* |
| Two current at once? | **Yes** — `NP-RISK-003` and `NP-RISK-004` are peers, both Rev 1 | **No** — Rev 3 supersedes Rev 2 |
| Ordered? | No. `-004` is not "after" `-003`; they cover different artifacts | Strictly ordered |
| A new one means | a new document came into existence | an existing document changed |
| Written in | the document ID, and therefore the filename | the `**Revision:**` field **only** |

**The two collide numerically by accident, and that accident is what makes the rule necessary to
state.** `NP-PRIV-ANALYSIS-002` is at **Rev 2**; `NP-PRIV-ANALYSIS-003` is at **Rev 3**. Reading
either filename as carrying a revision is a reasonable inference and is wrong. Only §4.0 makes the
number in a filename unambiguous.

The operational test for which one to increment is unchanged, and is in the table above: **new
serial when the architecture is replaced; new revision when decisions are inherited.**

#### 4.0.3 Vendored files are the only exception

> **A vendored file keeps the name its vendor gave it.** Renaming it breaks correspondence with
> upstream — the datasheet, the release tarball, the SOUP record, the diff against a new version.

`firmware/vendor/freertos/`, `firmware/vendor/cmsis_core/` and `firmware/vendor/cmsis_device_g0/`
hold byte-exact upstream subsets whose per-file provenance and SHA-256 are recorded in their
`VERSION` files. Byte-exactness is verified by re-download and `cmp`; a rename would void that.
This is the same principle as §Scope's vendor-name rule: **where a vendor name conflicts with a
NeurOne convention, the vendor wins.**

#### 4.0.4 Retaining a superseded revision — the test is references and retention duty (BINDING, decided 2026-08-11)

> **Whether a superseded document or revision stays on disk is not a matter of preference. Two
> questions decide it, and either one alone is sufficient to retain:**
>
> 1. **Is it referenced anywhere?**
> 2. **Is it required to be retained for process or legal reasons?**
>
> If neither, retention is optional and nobody need care. If either, it stays.

**For NeurOne, question 2 is already answered, and the answer is yes.** `NP-QMS-001` §Records sets
the Design History File retention period at **life of device + 2 years (21 CFR §820.180)**, and
`NP-DHF-001` is the §820.30(j) index of design records. A superseded design document is a design
record — indeed superseded revisions are much of what a DHF exists to hold, since the file must
demonstrate the design *was developed in accordance with the plan*, which is a claim about history.

So the operating position for this project is: **superseded design documents are retained.** They
live in `docs/superseded/`, indexed, with their successors named.

##### What "retained in version control" does and does not satisfy

Git holds every committed state, and for an *uncommitted* working file it holds nothing — which is
why the precondition below exists at all. But a git blob is not a substitute for a retained record
when a controlled index links to that record: `NP-DHF-001` cites files by path, and a path that
resolves only via `git show <sha>:<path>` is not an index entry, it is an instruction to reconstruct
one.

> **Deletion is therefore reserved for the case where both questions above answer *no*.** In this
> document set that case has not yet arisen.

##### The precondition, if deletion is ever justified

An intermediate revision may be deleted **only if it was committed first**, established positively
rather than assumed — git can only return what it was given:

```bash
git log --follow --oneline -- <path>     # it has history
git show <commit>:<path> > /tmp/check    # the blob comes back
cmp /tmp/check <path>                    # byte-identical
```

##### Correction of record — this section reversed its own Rev 4 position

Rev 4 of this document stated the opposite rule: *"a serial resolves to exactly one file …
intermediate revisions are not retained as independent documents"*, and acted on it by **deleting**
`np_hw_fpc_001.docx` (`NP-HW-FPC-001` Rev 4). That was wrong on both limbs of the test above — the
file is referenced from `NP-DHF-001` and `docs/status/document-register.md`, and it falls under the
§820.180 retention duty. **It has been restored, along with its DHF index link.**

Two claims made in support of that deletion were also wrong, and are corrected here rather than
quietly dropped:

| Rev 4 claim | Actual |
|---|---|
| "`check-doc-filenames.ts` fails on the two-file case" | **It passed.** Verified by checking out the decision commit `19be91d` with both files present and running it: `A PASS · B PASS`. The failure observed later was self-inflicted — it appeared only *after* the DHF link was removed, which is what stripped the file of a serial of record. |
| "§4.0 cannot hold — the serial addresses two things" | **§4.0 holds.** It binds the *filename*, and `np_hw_fpc_001.docx` and `np_hw_fpc_001.md` both satisfy it: each basename is the serial and nothing else. "One serial, one file" was a stronger principle, invented here and never stated in §4.0. |

The generalisation that survives is narrower and is what §4's own test already said: **do not issue
what is really a new revision as a new document.** That is a rule about authoring, applied when a
document is about to be issued — see §4's *new number vs new revision* row. It is not a licence to
delete history.

##### Going forward, not retroactively

`NP-DB-001`…`NP-DB-005` are the clearest example of the authoring mistake: five serials whose
revision numbers track them one-for-one, four titled simply *Design Brief* — one document issued
five times under new **serials** when each had inherited the previous one's decisions and should
have taken a new **revision**. They are referenced from the DHF and covered by the same retention
duty, so they stay exactly as written. **OI-CONV-06 is closed on that basis.**


### 4.1 Revisions are integers (BINDING, changed at Rev 2)

> **A document revision is a positive integer. First issue is Rev 1.**

Rev 1 of this document specified a single uppercase letter, which is what the set had been using.
That failed on its own terms and was replaced rather than patched:

| Failure | Evidence |
|---|---|
| The alphabet ran out | `NP-DHF-001` reached **Rev AA** — the 27th issue. `AA` sorts before `B` in every tool that sorts text, so the register's own index was mis-ordering its newest entry. |
| Letters do not carry magnitude | Nothing in "Rev H" says *eighth*. Readers were counting on their fingers to tell whether `NP-SW-CI-001` Rev H was ahead of `NP-FMEA-001` Rev D. |
| A parallel numeric scheme already existed | `NP-DHF-001` §5.1 listed the design briefs as revisions **1, 2, 3, 4** while every other row used letters. Two schemes, one table. |
| A third, dotted scheme also existed | `NP-COORD-001` reached `Rev A.10`, which no other document used and no rule described. |

**The mapping applied on 2026-08-11 is positional and total** — `A`=1 … `Z`=26, `AA`=27 — so any
revision cited in git history, in a commit message, or in a `.docx` still on file resolves without
ambiguity:

| A | B | C | D | E | F | G | H | I | J | K | L | M |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 |

| N | O | P | Q | R | S | T | U | V | W | X | Y | Z | AA |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 14 | 15 | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 | 24 | 25 | 26 | 27 |

No letter was skipped in this document set — `I`, `O` and `Q` were all in use (`NP-DHF-001` passed
through each) — so the mapping is positional with no gaps, and it is injective within every document.

> **This section is the one place in the active set where letter revisions still appear, and that is
> deliberate.** `Rev H`, `Rev AA` and `Rev A.10` are quoted above as *evidence for the change*; a
> mapping table that could not name what it maps from would be unusable. §5 already records the same
> trap for section marking — *"a rule document that quotes its own anti-pattern fails its own rule"*
> — and it applies here too. An audit finding letter revisions in `np_conv_001.md` §4.1 has
> found the explanation, not a miss. The other two known survivors are
> `docs/superseded/**` (§1.1, by design) and `np_priv_001.pdf` (binary, OI-CONV-04).

### 4.2 What the integer rule does NOT cover

This rule binds **document** revisions. Three neighbouring things keep letters, and the boundary is
stated here because a blanket conversion would have silently rewritten all three:

| Not a document revision | Example | Why it keeps letters |
|---|---|---|
| **PCB revision** | `Hub PCB Rev C` | A board revision is a hardware artifact identity that appears on silkscreen and in fabrication packages. It is set by the EE release, not by this register. `NP-HW-HUB-001 Rev 3` and `Hub PCB Rev C` currently move together; that coupling is now visible instead of implied. |
| **Database schema revision** | SHDR `schema Rev D` | An on-disk format identity (`ci/shdr/shdr_fleet_schema.sql`). Renaming it would invalidate the migration record. |
| **Vendor / standards revision** | `IPC-2223D` | Owned by the issuing body — §Scope's vendor-wins rule applies. |

### 4.3 No revision information in filenames

> **A filename names the document, not the issue of it.** Version control holds the revision history.

A revision in a filename means a rename on every revision, and a rename on every revision breaks
every inbound link. It is also the one piece of metadata version control already holds perfectly.

The design brief was `neurone_design_brief_r5.docx` until 2026-08-11, and the `_r5` was **already
stale in two register entries** — which is the failure mode in miniature. Under §4.0 it is now
`np_db_005.docx`, where the `005` is `NP-DB-005`'s serial, not its revision. Its revision is 5 as
well, coincidentally, which is precisely the collision §4.0.2 exists to disambiguate.

**Superseded documents are not exempt from §4.0.** Rev 2 of this document exempted them, reasoning
from §1.1's *rename forward, never backward*. That was wrong and is corrected here: §1.1 protects
historical **records of what was written** — revision-history rows, decision logs, the text inside a
retired document. A filename is not a record; it is an address, and the whole point of §4.0 is that
the address is derivable from the serial. A retired document is the case where derivability matters
*most*, because it is the one nobody remembers the descriptive name of. `NP-RISK-001`'s ~38 inbound
citations now resolve to `docs/superseded/np_risk_001.docx` by rule rather than by lookup.

What *is* retained inside `docs/superseded/` is the **revision label** each document was written
with (§1.1 proper) and a record of its former filename, in `docs/superseded/README.md`, so an
external citation of the old name still leads somewhere.

## 5. Section references

> **`§N` with no space** — `§7.2`, `§8.2.1`, `§5.1.7`. The section sign goes immediately before
> the number, with no separating space.
>
> **Enforced by `scripts/check-section-refs.ts`**, which runs in CI (*"Section refs resolve and
> use the canonical form"*) and also verifies that every reference resolves to a section that
> actually exists. Run it locally before pushing: `bun scripts/check-section-refs.ts`.
>
> *Note: the incorrect form is described here rather than quoted, because the checker scans for
> the literal pattern and a rule document that quotes its own anti-pattern fails its own rule.
> Rev 1 did exactly that and was caught by CI — which is the intended behaviour of both.*

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
and why** — do not overwrite silently. The pattern of record is `NP-HW-HEXTILE-001` Rev 2's
cluster-count correction banner: a table of *was → is → cause*, then the surviving rationale.

A superseded rationale that was **outweighed rather than refuted** is marked as such and retained.
`NP-HW-HEXTILE-001` D-4 is the reference example.

## 8. Verification

> **Cross-document interface agreement is verified by mechanical diff, never by review.**

`SH2-DRC-05b` and `HT-DRC-23` both require the socket pin tables to be compared **name for name**
programmatically. The repository already applies this principle to section references —
`scripts/check-section-refs.ts`, gated in CI — and that is the model: a convention worth writing
down is worth a script. This is a rule because all three collisions in the header note above survived
multiple human readings — a name mismatch reads as agreement.

A probe that compares two tables must be **falsified before it is trusted**: perturb one entry and
confirm the check fails.

---

## 9. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-CONV-01** | **`SAFE_EN[n]` polarity disagrees with the safety-MCU firmware, and §1.1 made it visible.** `NP-DRV-SHELL-002` §6 specifies **LOW = rail removed = disabled** (so `SAFE_EN[n]` is active-**HIGH**, and SH2-DRC-13's "defaults LOW at reset" is the *safe* state). The safety MCU specifies the opposite for its enable lines: *"Active-LOW open-drain: **LOW = stimulation enabled**; HIGH = disabled. Power loss or reset [→ disabled]"* (`np_safety_config.h:7-8`, `np_safety_main.c:14`). **Both are internally coherent and fail-safe on their own terms; together they are inverted.** The hazard is the undocumented mismatch: anyone implementing `SAFE_EN[n]` to the safety MCU's house convention would make LOW *enable* the cluster, turning SH2-DRC-13's "default LOW at reset" from the safe state into **stimulation enabled at power-on reset**. Naming has been applied **as the documents currently state polarity** — `SAFE_EN[n]` carries no `#` because SHELL-002 declares it active-high — so **resolving this item may require a rename to `SAFE_EN#[n]`**. Not resolved here: this is a safety-architecture question, not a naming one. **The inversion is now visible in `NP-HW-HUB-001` §3.1's own block diagram**, where an active-low `PBM_CRANIAL_EN#` from the safety MCU feeds a tier whose per-cluster gate is declared active-high — which is precisely the effect §1.4 intends | Safety + EE Lead | **Cluster-carrier schematic; Hub PCB Rev C.** Assess with **OI-FMEA-01** and **OI-HUB-C07** |
| **OI-CONV-03** | **`NP-COORD-001`'s dotted minor revisions are not resolved, only mapped.** That document reached `Rev A.10` under a two-part scheme (`A`, `A.1` … `A.10`) that no other NeurOne document uses and §4.1 does not describe. The 2026-08-11 conversion mapped the **major only** — `Rev A.10` → `Rev 1.10` — because flattening the eleven issues to `Rev 1`…`Rev 11` would assert an ordering the revision history does not actually establish (it is not recorded whether `Rev A` and `Rev A.1` were distinct issues or the same issue relabelled). Resolving it means reading the eleven change records and re-issuing the document under a flat integer. **Note also that the document and the register disagree**: `docs/status/document-register.md` and `NP-DHF-001` both cite `Rev A.9` while the `.docx` itself reads `Rev A.10` | Quality | Documentation consistency; not tooling-blocking |
| **OI-CONV-04** | **The eleven active `.docx` documents still carry letter revisions internally.** §4.1's conversion was applied to the Markdown corpus — which is the set of record — and to every Markdown citation of a `.docx` document's revision, so the register, the DHF and all cross-references are numeric. The Word files' own header text was not edited: in every file sampled the revision string is split across Word runs (`Rev</w:t>…<w:t> A`), so a text substitution would have silently missed occurrences, and a partial conversion is worse than none. These files are on the path to Markdown conversion anyway (`np_*.md` is the newer generation); the revision label should convert with the format, in one step, not before it. Until then §4.1's mapping table resolves the discrepancy. Affected (by serial, per §4.0): `NP-BIB-001`, `NP-CLIN-001`, `NP-DB-005`, `NP-COORD-001`, `NP-PROC-FPC-001`, `NP-FW-EMMC-001`, `NP-SBIR-001`, `NP-PROC-SUP-001`, `NP-TOOL-LENS-001`, `NP-TOOL-SHELL-001`, plus every `.docx` under `docs/superseded/`, `neurone_tool_shell_001` | Quality | Documentation consistency; not tooling-blocking |
| ~~OI-CONV-05~~ | **✅ CLOSED 2026-08-11 — `NP-HW-FPC-001` legitimately holds two files, and that is fine.** Rev 4 read the two-file case as a rule violation and deleted the Rev 4 `.docx`; Rev 6 reversed that and restored it. §4.0 binds the *filename*, and `np_hw_fpc_001.docx` and `np_hw_fpc_001.md` each satisfy it — basename is the serial and nothing else. "One serial, one file" was a stronger principle invented at Rev 4 and never stated in §4.0. The file is referenced from `NP-DHF-001` and falls under the §820.180 DHF retention duty, so both limbs of §4.0.4's test say retain | — (closed) | — |
| ~~OI-CONV-06~~ | **✅ CLOSED 2026-08-11 by decision — leave them as written.** `NP-DB-001`…`NP-DB-005` are five serials whose revision numbers track them one-for-one, four titled simply *Design Brief*: one document issued five times under new **serials** instead of new **revisions**, inverting §4's own test. **But each serial resolves to exactly one file, so §4.0 holds and `check-doc-filenames.ts` passes** — unlike `NP-HW-FPC-001`, this is not a collision. Four of the five are superseded and therefore already out of active use; renumbering would collapse five `21 CFR §820.30(j)` design-record entries and rewrite their DHF rows, a records change with regulatory surface taken against documents nobody will design from again. **§4.0.4 binds the issuing of new revisions, not the archive** (see *Scope* under §4.0.4) | — (closed) | — |
| **OI-CONV-02** | Sweep the remaining document set against §1 and §5. This revision covers the socket/cluster interface documents (`NP-HW-HEXTILE-001`, `NP-DRV-SHELL-002`, `NP-HW-HUB-001`); the modality, firmware and app specs have not been audited for active-low signals carrying no `#` | Systems | Documentation consistency; not tooling-blocking |

---

## 10. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| **6** | **2026-08-11** | NeurOne Systems Engineering | **§4.0.4 replaced — retention is decided by two objective questions, not by preference or tidiness.** Principal direction: a superseded document stays if it is **referenced anywhere**, or if there is a **process or legal requirement to retain it**; absent both, nobody need care. **For NeurOne question 2 is already answered yes** — `NP-QMS-001` §Records sets DHF retention at *life of device + 2 years (21 CFR §820.180)*, and a superseded design document is a design record, since a DHF exists to demonstrate the design *was developed in accordance with the plan*, which is a claim about history. **This section reverses its own Rev 4 position and the action taken under it**: `np_hw_fpc_001.docx` (`NP-HW-FPC-001` Rev 4) was deleted at Rev 4 and is **restored**, with its `NP-DHF-001` index link. Two supporting claims are corrected rather than dropped — (i) Rev 4 asserted `check-doc-filenames.ts` failed on the two-file case; **it passed**, verified by checking out the decision commit `19be91d` with both files present, and the later failure was self-inflicted, appearing only after the DHF link was removed; (ii) Rev 4 asserted §4.0 could not hold, but §4.0 binds the filename and both files satisfy it — *one serial, one file* was a stronger principle invented at Rev 4 and never stated. What survives is narrower and was already in §4's *new number vs new revision* row: **do not issue what is really a new revision as a new document.** That is an authoring rule, not a licence to delete history. Records the distinction between a git blob and a retained record: a path resolvable only via `git show <sha>:<path>` is not an index entry, it is an instruction to reconstruct one. **OI-CONV-06 stays closed** — `NP-DB-001`…`NP-DB-005` are referenced and covered by the same retention duty, so they stay as written. |
| **5** | **2026-08-11** | NeurOne Systems Engineering | **§4.0.4 scoped — it binds going forward, and is not a retroactive sweep of the archive.** Principal direction. Rev 4 stated the rule without bounding its reach, which left it reading as a mandate to renumber history. The bound is **collision, not tidiness**: a superseded intermediate revision that collides with nothing is left as written, because retirement has already removed it from active use and deletion would add nothing. The one case that *was* actioned, `NP-HW-FPC-001`, is a different shape and the section now tabulates the difference — its serial resolved to **two** files, so §4.0 could not hold and `check-doc-filenames.ts` failed; the design briefs resolve one-to-one and the checker passes. **Closes OI-CONV-06 by decision: `NP-DB-001`…`NP-DB-005` stay as written.** They remain the clearest example of the mistake §4's test exists to prevent — five consecutive issues that took a new serial when they had inherited the previous one's decisions and should have taken a new revision — and the section now says plainly that this is what the rule is *for*: the next document about to be issued, not the archive behind it. |
| **4** | **2026-08-11** | NeurOne Systems Engineering | **§4.0.4 added — an intermediate revision is not an independent document; version control is the revision store.** Principal decision, and it *follows* from §4.0 rather than sitting beside it: if a serial is an address it cannot address two things, and since §4.3 bars a revision from appearing in a filename, two revisions on disk have no legal way to be distinguished. **The precondition is the whole safety of the rule and is stated as such: an intermediate revision may be deleted only if it was committed first**, established positively (`git log --follow`, `git show`, `cmp`) rather than assumed — git can only return what it was given. Closes **OI-CONV-05** by deleting `np_hw_fpc_001.docx` (Rev 4) after confirming 15 commits of history, first introduction on `9939182` under a third filename (`neuropulse_fpc_zone_module_spec_revA.docx`, predating the project rename), and a byte-identical 64,767-byte blob at HEAD. Corrects Rev 3's description of that file as Rev 3 — the DHF records it as Rev 4 (written as Rev D). Raises **OI-CONV-06**: `NP-DB-001`…`NP-DB-005` are the same pathology at larger scale — five serials whose revisions track them one-for-one, four titled identically — i.e. one document issued five times under new *serials* instead of new *revisions*, inverting §4's own test. Deliberately not acted on: it collapses five DHF design-record rows and carries regulatory surface. |
| **3** | **2026-08-11** | NeurOne Systems Engineering | **The filename IS the document serial (§4.0) — new binding rule, and it is *exclusive*.** Rev 2 said only "lower-snake of the ID" in a table cell, with no statement that nothing else may match that shape. That was too weak to be checkable: a rule that says documents look like `np_x_nnn.md` is worth little if other files also do, because the pattern stops being evidence. §4.0 states the rule, §4.0.1 states the converse, and both were enforced on the whole set: **25 files renamed to their serial** (`neurone_shell_fpc_routing_review.docx` → `np_drv_shell_001.docx`, `neurone_supplier_selection_checklist.docx` → `np_proc_sup_001.docx`, and 23 more), and the one file that matched the serial *shape* while carrying no serial (`np_condition_links_001.md`, whose front matter had no `**Document:**` field and stated its revision inside `**Status:**`) was resolved to `np_cond_link_001.md` and given the missing fields. **§4.0.2 states the serial-versus-revision distinction explicitly**, because the two collide numerically by accident — `NP-PRIV-ANALYSIS-002` is at Rev 2 — and reading a filename's digits as a revision is a reasonable inference that §4.0 now forecloses. **§4.0.3 makes vendored files the sole exception**: upstream names are kept because byte-exactness against a recorded SHA-256 is verified by re-download and `cmp`, which a rename voids. **§4.3 corrected**: Rev 2 exempted `docs/superseded/` from filename normalisation on a misreading of §1.1. §1.1 protects records of *what was written*, not addresses; a retired document is where derivability matters most, since nobody remembers its descriptive name. `NP-RISK-001`'s ~38 citations now resolve by rule. Raises **OI-CONV-05** — `NP-HW-FPC-001` resolves to two files, which the rule surfaced rather than created, and which is deliberately not fixed with a filename. |
| **2** | **2026-08-11** | NeurOne Systems Engineering | **Document revisions become integers (§4.1), replacing Rev 1's single-uppercase-letter rule.** Rev 1's rule is stated, then replaced with the four pieces of evidence that took it: `NP-DHF-001` had reached **Rev AA** (27th issue) which text-sorts before `B`; letters carry no magnitude; `NP-DHF-001` §5.1 was *already* numbering the design briefs 1–4 in a table whose other rows used letters; and `NP-COORD-001` had a third, dotted scheme (`Rev A.10`) that no rule described. Mapping is **positional and total** (`A`=1 … `Z`=26, `AA`=27) and published as a table, so revisions cited in git history stay resolvable; no letter was skipped in this set, so it is injective. **§4.2 states the boundary the conversion did not cross** — PCB revisions, SHDR database schema revisions and vendor/standards revisions keep letters, each with its reason; the exclusions were established by enumerating every token preceding `Rev <letter>` in the tree, not by assumption. **§4.3 forbids revision information in filenames** (`np_db_005.docx` → `np_db_005.docx`) and **§4 adds `docs/superseded/`** as the home for retired documents. Superseded documents are **not** converted — §1.1's *rename forward, never backward* applies to revision labels as it does to signal names. Raises **OI-CONV-03** (NP-COORD-001's dotted scheme mapped, not resolved; register and document also disagree on A.9 vs A.10) and **OI-CONV-04** (eleven active `.docx` retain letters internally — their revision text is split across Word runs, so partial conversion was the only mechanical option and was refused). |
| 1 | 2026-08-11 | NeurOne Systems Engineering | Initial release. Consolidates rules previously stated inline in `NP-HW-HEXTILE-001` §7.4 and `NP-DRV-SHELL-002` §5.1.7. **Binding rule: all active-low NeurOne signals terminate with `#`** (§1.1), applied to `ALERT#`, `SEAT#` and newly to `ATTN#`. §1.2 records the five suffixes and one prefix that are unavailable for polarity, each with the evidence that took it. §1.3 introduces `SIGNAL[n]` index notation, replacing `SAFE_EN_n`. **§2 corrects the firmware mapping from `_L` to `_ACTIVE_LOW`** — `_L` already means *Left* in `NP-FW-CVNS-001` §5.1, which the original mapping missed. §3 states the Class C boundary: no firmware identifier is renamed, and OI-FMEA-01 keeps ownership of the unmarked safety-MCU enable lines. §4–§8 record document-ID, section-marking, identifier-family, revision-practice and verification conventions already in use but never written down. **Raises OI-CONV-01** — applying §1.1 exposed an inverted `SAFE_EN[n]` polarity between `NP-DRV-SHELL-002` §6 and the safety-MCU firmware, which is a safety-architecture question and is not resolved here. |
