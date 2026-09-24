# CLAUDE.md — revision history

> Relocated from the CLAUDE.md header (Rev 40) to slim the always-loaded core. This is the
> authoritative narrative of what each CLAUDE.md revision changed and why. Referenced from
> CLAUDE.md → Document Map.
>
> **Since Rev 47 this is the ONLY copy.** The CLAUDE.md header carried condensed summaries of the
> most recent revisions alongside this file; they are gone, because a digest of a narrative is a
> second source of truth about why a decision was made, and its selection of which revisions to
> summarise decayed unowned (the Document Map still said "Rev 33–42" four revisions on). The header
> now points here and says nothing about any revision. **Do not reintroduce a summary there.**
>
> **APPEND-ONLY, newest first.** Each entry records what was decided *and believed* when it was
> written. Do not edit or delete an entry to reflect a later change — add a new entry that names
> the entry it corrects.
>
> Revisions before Rev 33 are in git history and in `docs/status/completed-decisions.md`; Rev 33
> is the reorganization that created the current core/subsidiary split.

## Current revision

**Rev 55 (2026-09-24) — §2.3 gains a paragraph naming the gate that enforces its trigger rule. No
locked decision changed; `OI-ACC-06` closed, `OI-ACC-09` raised.**

**What changed.** §2.3's trigger rule (Rev 48) had no gate. It now has one,
`scripts/check-consumable-triggers.ts`, and `commercial-model.md` §2.3 has a Trigger column for it to
read. The new paragraph says the rule is enforced, what the gate checks, and that a new consumable
prompt needs a row before it ships.

**Why in the core.** §5.1, §6.2 and §17 each name their gate in the core. Naming a gate tells the
reader that a change touching the invariant will fail CI, not just review. §2.3 was the only one of
those invariants with no gate to name.

**What it does not change.** The rule's text, the two admissible kinds, the scope limit, every
threshold value and every price. The hydrogel tip and VNS clip pad thresholds are relabelled
unvalidated placeholders in `commercial-model.md` and in code comments on both platforms, with
values unchanged. That applies §2.3's existing sentence and does not add a new one.

## Earlier revisions

**Rev 54 (2026-09-24) — §1's *"No such gate exists yet"* replaced by what now exists and what is not
in force; §4.2 gains the tier-identity interlock row; §4.2's safety-MCU module count updated. No
locked decision changed; `OI-UPG-01`'s firmware half implemented, `OI-UPG-08` raised.**

**What changed.** `NP-REG-UPG-001` Rev 3 §7.7 built the stimulation half of `REQ-UPG-01` and the
firmware half of `REQ-UPG-02`. A new Class C module, SW01-M10, verifies a UID-bound Ed25519 tier record
in the safety MCU's OTP and withholds every T2 enable line unless the record verifies as T2. The hub
refuses T2 protocols at load as F4. §1 rule 1 said the gate did not exist. It now says the gate exists,
that it is not in force, and why.

**Why "not in force" is the load-bearing phrase.** The image carries an all-zero placeholder
authority key, and that fails closed: **every unit, T2 included, is T1** until the key ceremony and
the OTP programming step exist (`OI-UPG-08`). A reader who took "the gate exists" alone would cite §1
as evidence that T2 units work, or that a T1 unit's refusal has been verified on silicon. Neither is
true. Rev 52's entry was careful not to say the gate existed. This entry is careful not to say more
than that it now does.

**Why a §4.2 row.** §4.2's table is where a reader looks for what the safety MCU withholds, and this
is now one of those things. The row names the fail-closed direction and the open item, for the same
reason as above. The module count moved from *"9 modules as of 2026-08"* to 10. The line already
defers to `wc -l`, which now reads ~2,900.

**What it does not change.** No decision. `REQ-UPG-01`'s software sentence is still unbuilt, and the
app's tier is still UI state (`OI-UPG-01`). A T2-tier protocol built only from T1 modalities
(`NP-PWRSRC-001` §6.2) still has no gate. `RISK-PWRSRC-10` stays open for it.

## Earlier revisions

**Rev 53 (2026-09-23) — §17's canonical-surface row states the one exception to equal key sets: a
locale may add its own CLDR plural categories. No locked decision changed; `OI-I18N-03` closed.**

**What changed.** §17's table said all eleven locale files "carry the same key set". Russian needs
`_FEW` and `_MANY`, and Arabic needs `_ZERO`, `_TWO`, `_FEW` and `_MANY`, on plural families where
English has only `_ONE` and `_OTHER`. Under the old sentence those languages could not be translated
correctly (#191). The row now says every locale carries en.json's keys and may add only its own CLDR
categories to a family en.json defines. That is exactly what `check-locale-strings.ts` enforces.

**Why in the core and not only in `localization.md`.** The sentence is the one a person reads before
adding a key. Left as it was, it states a rule the gate no longer applies, and someone reading it
would take a legitimate `ru.json` `_FEW` for an orphan and delete it. The mechanics (sources, ledger,
web selection) are in `localization.md` §17.2 and §17.7, not here.

**What it does not change.** Every locale still carries every en.json key; no generated file is
committed; the single-source rule is untouched.

## Earlier revisions

**Rev 52 (2026-09-23) — §1 gains the T1 → T2 upgrade model as an invariant, and the Document Map
gains `NP-REG-UPG-001`. One locked decision taken (`OI-TACSDRV-06`); no figure moved.**

**What was decided.** The principal took `NP-REG-UPG-001`'s recommendation: **a T1 owner reaches T2
by buying a new unit, and no T1 unit is converted by anyone.** Three reasons were given. Purchasers
reuse any modules they bought. The regulatory issues of conversion and user installation are avoided.
And a rolling upgrade cannot get round the T1 → T2 price differential.

**Why it belongs in the core rather than only in its document.** Two of its consequences bind work
that never opens `NP-REG-UPG-001`. **(1) Module carry-over** makes every T1 module interface a two-tier
interface. Anyone revising a socket pinout, an accessory port or the inventory protocol needs to know
that, and `OI-MMSOCK-12`'s socket change was already in flight. **(2) The rolling-upgrade reason** makes
a T1 refusal of T2 modalities a requirement. Anyone writing gating, entitlement or feature-flag code
needs to know that the gate must sit where the enables are owned, and that the app's
`NP_PROTO_FLAG_T2_TIER` is not it. **§1's "modular field-upgradeability" line would otherwise read as
permission to promote T1 as upgradeable to Pro**, which is intended-use evidence. So the new text
scopes it to within a tier.

**What it is careful not to say.** It does not say the gate exists. It does not. `OI-UPG-01` carries
the implementation, and the §1 text says so, so that nobody cites §1 as evidence that a T1 unit
refuses a T2-D tile today. It states no price or cost. The T2 offering for a buyer who already owns
modules is Product's (`OI-UPG-04`), and no price may be set before `OI-HEXTILE-06`. It does not settle
the conditions under which carried modules become 510(k) components. That is counsel's Q2, and the
design meanwhile builds them as tier-common part numbers under the QMS.

## Earlier revisions

**Rev 51 (2026-09-23) — the Layer 4 absorber is deleted: `REQ-CAV-04` taken by the principal (GitHub
#391). §1 and §4.3 now read *4-layer*. This is the change the Rev 50 correction below told a later
reader not to make without a decision; the decision now exists.**

**What changed in CLAUDE.md.** §1's founding principle reads *"4-layer EMF shielding"*. §4.3's heading
reads *(4-layer passive + active)* and its body names the passive layers as **1, 2, 3 and 5**. The
§4.3 note that argued for the deletion (*"Recommended, not executed"*) is replaced by one recording
it, the binding 3 mm outer-bowl re-loft (outward thermal path 0.410 → **0.335** m²K/W; radial stack
30–35 → **27–32 mm**), and three rules that bind from here. Two Document Map rows are reworded so they
no longer describe Layer 4 as a live candidate. **The combined 35–45 dB ELF / 40–60 dB RF figure is
unchanged** — Layer 4 contributed to neither — and it is still a design target (`EMF-1` has never
run).

**The three rules the note carries, and why each is there.**
1. **Never renumber.** `NP-EMC-CAV-001` §8.2 warned that *L5 → L4* would silently repoint every
   historical citation of "Layer 4" at the port filters. The number is retired and held.
2. **"Five-layer keying" is a different thing.** `NP-DT-001` `DI-USE-05` / `DO-HW-01`, `NP-RISK-003`,
   `NP-RM-001` §221 and `durability-maintenance.md` §18 describe the retired RISK-15 zone-module keying
   scheme. They were deliberately left alone (`NP-EMC-CAV-001` §8.4's homonym trap).
3. **The station comes back one way only.** If `MECH-2`'s spring plungers cannot take up the (now
   ±0.80) clamp tolerance stack, `OI-EMCCAV-08` puts back a thin, **electrically insulating,
   non-magnetic** ceramic-filled pad (`REQ-CAV-03`) sized by that stack, at ~0.355 m²K/W. Never an
   absorber: `NP-EMC-CAV-001` §6 found there is no RF function to restore.

**Carried through the document set** per `NP-EMC-CAV-001` §8.4: `hardware-detail.md` §4.3,
`NP-EMC-CAV-001` Rev 14, `NP-BIB-EMF-001` Rev 6, `NP-HELMET-GEOM-001` §0/§2/§3.3/§4/§6.6/§7 and its
ISA, `NP-DT-001` Rev 4 (`DI-PERF-22`), `NP-ART-001` Rev 7 (A6, `OI-ART-01`), `NP-THERM-COOL-001` Rev 13
(`OI-THCOOL-04` closed, `OI-THCOOL-15` narrowed, `OI-THCOOL-21` raised), `NP-THERM-SINK-001`
(`OI-SINK-07` moot), `NP-ENV-OPRANGE-001` §2 (D2's evidence sentence — substance unchanged),
`NP-HEX-ZM-001` §5.1/§5.7 and its ISA, `NP-THERM-CFD-001`, `NP-HW-FITOVER-001`, `NP-HW-AUDIO-001`,
`NP-PWR-THERM-001`, `NP-RM-001`, `NP-TOOL-HUB-001`, `NP-CONV-001`, `competitive-position.md`,
`regulatory-strategy.md` and `cad/CAD_PARTS_LIST.md`.

**What was deliberately NOT changed.** The thermal network scripts and the tables computed from them
still carry the 0.410 baseline with the foam in it. That is conservative for every scalp-side
conclusion and understates every capability figure; re-running it is analysis, not an edit, and is
`OI-THCOOL-21`. `NP-TOOL-SHELL-001` (a generated `.docx`) is not edited: the re-loft rides the
re-scope `OI-ART-01` already owes (GitHub #331). Historical entries in this file and in
`completed-decisions.md` that say *5-layer* are left as written.

## Earlier revisions

**CORRECTION to the Rev 50 entry below (2026-09-22) — Rev 50 carries TWO changes, not one, and the
second arrived without a revision bump. CLAUDE.md remains at Rev 50; this corrects the record, not
the file.**

Per this file's own rule — *do not edit an entry to reflect a later change; add a new entry that
names the entry it corrects* — the Rev 50 entry below is left as written. It describes §18 as though
§18 were the whole of Rev 50. It is not.

**What else Rev 50 carries.** Commit `707fe2d` (*"EMC cavity resonance analysis: establish Layer 4
requirement in dB"*, #365, merged to `main` 2026-09-22) added fifteen lines to CLAUDE.md — a Document
Map row for `docs/np_emc_cav_001.md`, and a note under §4.3 — **and left the header at Rev 49.** The
§18 work was rebased onto it, so both now sit in one file reading Rev 50, and the second change had no
entry in this file at all.

**What those fifteen lines say**, recorded here because otherwise nothing in this file does:

- A Document Map row pointing at `NP-EMC-CAV-001` for cavity resonance — the source that excites it,
  the band, and Layer 4's requirement in dB.
- A §4.3 note: **Layer 4 now has a requirement and fails it.** `REQ-CAV-02` sets **loaded Q ≤ 20 over
  420 MHz – 3 GHz (≥ 26.2 dB)** against a named source — the **18 cluster controllers inside the
  envelope**, not a radio — and **the layer supplies 0.26 dB while the wearer's head supplies
  49.8 dB**. `REQ-CAV-04` recommends deleting the station, with the 3 mm re-loft of the outer bowl
  **BINDING** if it is deleted, because vacating 3 mm otherwise fills it with stagnant air that is
  54 % worse per mm than the foam.

**The one thing a later reader must not "fix".** That note recommends a 4-layer stack, but **§1 and
§4.3 still read *5-layer*, and that is deliberate** — the note says *"Recommended, not executed"*, and
`OI-EMCCAV-08` (`MECH-2`) is the open question that decides it. The apparent contradiction between
§1's *"5-layer EMF shielding"* and a §4.3 note arguing for four is the state of the decision, not a
documentation defect. Do not reconcile it by editing §1.

**The process point, which is why this entry exists rather than a silent renumber.** A CLAUDE.md
content change that skips the revision bump gets **no narrative entry**, and then a later revision
absorbs it invisibly — which is the decay this file was split out to stop. Rev 47's entry records the
same failure mode in the other direction (a header digest whose selection *"decayed unowned"*).
Nothing here renumbers or rewrites `707fe2d`'s work; the correction **records** the gap rather than
repairing it, because the change is `main`'s and its author may want to number it themselves.

**Rev 50 (2026-09-21) — §18 added: a requirement must be required. One new section, mirroring
`NP-CONV-001` §7.1; no existing invariant changed and no figure moved.**

**Why it is in the always-loaded core rather than only in the conventions document.** §18 governs an
act performed constantly and almost invisibly — writing a number into a specification. A rule that
only binds when someone happens to open `NP-CONV-001` does not bind that act. The core carries the
rule and the scope limit; the test, the worked example and the audit item stay in §7.1.

**What produced it.** `NP-PROC-FPC-001` §2.3 required *"Maximum junction temperature (Tj_max)
≥ 125 °C"*, justified as *"operating headroom above 62 °C throttle threshold"*. Traced while working
GitHub #333: **`125` appeared nowhere else in the document set as a temperature and had no firmware
referent.** The chain that *is* derived — 42 °C scalp face (IEC 60601-1) → 62 °C junction throttle →
65 °C cutoff → ~70 °C PTC → 85 °C die cutoff — is entirely firmware and hardware interlocks, none of
them a supplier specification. The row had already rejected a candidate emitter at `T_j` 115 °C, a
part with 53 °C of headroom over the throttle. It was retired at that document's Rev 7.

**The generalisation, which is the owner's and is stated as a rule:** *nothing should be a
requirement if it isn't actually required; never add unnecessary constraints.* Two questions must
have answers before a number enters a controlled document — **what fails if this is not met**, and
**where is that traceable** — and **a Notes cell that restates the requirement is not a derivation**.

**Why this is not a tidiness rule.** Most procurement and interface documents in this set open with
*"all specifications are MANDATORY unless marked ADVISORY; a purchase order that omits a mandatory
specification is non-conforming."* Under that preamble an unrequired figure does not sit inertly —
it **rejects usable parts and manufactures false failures**, with the full authority of the document.

**The scope limit is part of the rule, not a caveat on it.** A rule that licenses removing
requirements is dangerous in a device programme, so §18 states what it does not reach: externally
imposed limits (IEC 60601-1's 42 °C, IEC 62471 MPE, §3's charge ceilings), hazard controls, ISO 14971
dispositions, and requirements whose derivation exists but is merely uncited. **"I could not find the
derivation" is a reason to look, then to raise an open item, never to retire.**

**Raised with it: `OI-CONV-08`**, whose first pass found three clusters (`NP-PROC-FPC-1064-001` §3.3
and §4.1, `NP-TOOL-HUB-001`) and one document worth copying (`NP-HW-CVNS-001`, which carries an
explicit *derived?* column per requirement). The sweep is recorded as incomplete, with its three
method gaps named.

## Earlier revisions

**Rev 49 (2026-09-20) — one Document Map row: the six accessory and applicator hardware
specifications, because until today four artifacts' governing specification was §3 itself.** No
invariant changed, no figure moved, and no section gained or lost content. This entry is longer than
the edit because the edit is a pointer to work that happened elsewhere.

**What happened outside CLAUDE.md.** GitHub #332 / `NP-ART-001` `OI-ART-04` asked for owning
specifications for the manufactured artifacts that had none. Six were written — `NP-HW-AUDIO-001`
(audio cup), `NP-HW-NASAL-001` (intranasal Y-probe and hygiene sleeve), `NP-HW-VNSCLIP-001`
(auricular VNS/HRV clip), `NP-HW-CVNS-001` (cervical VNS accessory), `NP-HW-TMS-001` (TMS coil) and
`NP-HW-TACSDRV-001` (the 21-channel clinical tACS driver stage, newly registered as artifact A16).
`NP-ART-001` Rev 3 records the whole disposition.

**Why that needs a row here rather than nothing.** For A11 and A12 the *governing specification of
record* was **`CLAUDE.md` §3 modality ⑦ and modality ②** — roster lines in the always-loaded core
standing in for hardware documents, which is exactly the confusion the Document Map exists to
prevent. The new row says where a modality's hardware is now specified and, as importantly, that
**§3's roster is not that**. One row, in the subject-matter table, beside the FAI programme and the
artifact register it belongs with.

**What the row deliberately says about maturity.** All six are **DRAFT and requirements-grade**:
they carry what the existing record already binds, with citations, and enumerate what is missing.
**No dimension, material, emitter, exposure limit, force, constant or price is created by any of
them**, and `NP-FAI-001` §2's F1/F2 still fail on all six, so no artifact FAI became writable — the
count stays at 1 of 16. A reader who follows the row must not mistake an owning document for a
released one, so the row says so itself.

**Three findings from that work that touch locked sections, and are recorded rather than acted on
here:**

- **§3's hard-limits table has no intranasal row.** It has *PBM scalp* and *PBM deep (T2)*; the
  intranasal probe — a shipping T1 modality applied to **mucosa** — inherits its 25 % duty ceiling
  from the cranial tiles by way of a firmware comment, and the 42 °C limit §4.2 realises with a
  per-zone NTC has no sensor in the probe to realise it with. `OI-NASAL-02`, **BLOCKING**. Adding
  the row is a clinical and regulatory decision and is routed to the existing `RISK-03` engagement,
  not taken here.
- **§4.2's TMS row is titled *Coil protection* and names no patient-facing control.** Every other
  stimulation row in that table names one. There is a Class C enable, and nothing anywhere states
  what must cut it off — no dose, train, inter-train or contraindication limit. `OI-TMS-03`.
- **The audio cup mesh frame's 40 dB RF figure is a shielding claim outside §4.3's stack**, on a
  **user-replaceable** part, with a fouling trigger that is an unimplemented HAL stub.
  `OI-AUDIOHW-03`; §4.3 is locked and adding to it is a CLAUDE.md decision, so nothing was added.

**Nothing in §1–§6 or §16–§17 is edited by this revision.** `check-section-refs` and
`check-doc-filenames` clean.

## Earlier revisions

**Rev 48 (2026-09-15) — §2.3: what "measurement-triggered" actually admits, because the term had no
test and a row could pass it by sounding right.** No decision changed, and no exception was granted.
The invariant's own sentence survives verbatim; what it gained is a way to fail.

**This is Rev 48, not 47.** It was written as Rev 47 against a tree whose history table ended at 46;
#360 merged its own Rev 47 first. On the precedent this file and `docs/np_dhf_001.md` history row 48
already carry (PRs #272/#273, then #351), **the earlier claimant keeps the number** and this entry
takes the next — so any citation of #360's work still resolves. **#360's Rev 47 is the entry directly
below, and it surfaced this contradiction deliberately without resolving it:** carrying §2.3's
invariant into `docs/reference/commercial-model.md` put the rule and the foam row in one reader's
view for the first time, which is what made it resolvable here. The warning block it left above that
table is replaced by the disposition, not deleted.

**The item that forced it had a false premise.** `OI-ACC-02` recorded that the audio cup foam's
prompt is `Calendar reminder` on a `6–12 months` interval in `docs/reference/commercial-model.md`
§2.3, against §2.3's locked *"all consumable prompts are measurement-triggered (§5.2), never
calendar-triggered"*, and offered two outcomes: build the foam a seal or compression proxy, or write
a reasoned exception. `NP-ACC-PRIORITY-001` declined both, rightly, on the ground that inventing a
proxy is design work. **Neither was needed.** The shipped app already triggers the foam on a session
count from the hub's SHDR-class `CONSUMABLE_STATUS` characteristic — a measurement the device takes.
The calendar lived only in the row's prose and could never have lived anywhere else: with no battery,
coin cell or `VBAT` rail (§4.5), the SNVS RTC has no backup domain and wall time dies on every
disconnect. **A calendar prompt here is unimplementable, not merely forbidden**, and §2.3 now says so
— the same reasoning that denominated `NP-FW-EMMC-002` §H's characterisation window in records.

**Why a definition rather than a fix to one row.** *"Measurement-triggered"* had no test, so the
table was adjudicated on whether a Notes cell *sounded* like a measurement. That is how the foam row
survived from the table's first revision until a ranking exercise happened to read it — and it is how
the **mesh frame** row still passes while shipping nothing at all, its *"App driver impedance flags
fouling"* resting on an unimplemented HAL stub whose return value is discarded and on a consumable
kind that does not exist (`OI-ACC-05`). §2.3 now states two admissible kinds, and requires the row to
say which: a **condition measurement** of the part, or an **exposure count** of the quantity driving
its degradation **with that mechanism named**, plus what such a count cannot see — a part that failed
early, was damaged, or degraded off the device.

**The clause that does the real work is about provenance, not kind.** The trigger's kind and the
threshold's derivation are two claims, and satisfying the first does not satisfy the second. Without
that sentence this revision would have *laundered* the foam: `150` was back-derived from `6–12
months` through an unvalidated *assuming daily use* assumption and does not match its own comment
(`~180`), so a kind-only rule would have moved that defect from visible-with-an-open-item to
certified-compliant-and-invisible. A threshold no measurement supports is an unvalidated placeholder
and is labelled one, per `NP-FW-EMMC-002` §G.2 — whose accelerometer thresholds are the same defect.

**The scoping sentence is not boilerplate.** `docs/reference/data-architecture-detail.md` §5.2
carries a broader claim — *"All reminders measurement-triggered, not calendar-triggered"* — and
reading this definition onto it would delete two controls with no measurement to substitute: the
**3–5 year Tier B fluxgate visit** (scale-factor drift is not self-detectable, and it is the
maintenance action behind §4.3's attenuation claim) and the **$1,800/yr T2 calibration visit**
(`NP-PWRSRC-001` D-16, the ISO 14971 re-acknowledgement point). §2.3's clause is therefore scoped to
consumable replacement prompts and names both exclusions.

**Where the detail is.** The disposition, the two claims it falsifies, and what the exposure count
cannot see are in `docs/reference/commercial-model.md` §2.3; the successors `OI-ACC-04…06` are in
`docs/status/pending-decisions.md`. Record: `docs/status/completed-decisions.md`, 2026-09-15.

**Rev 47 (2026-09-15) — the header's revision digest and the whole of §2's figures leave the core; no
design decision changed.** The always-loaded core went from 38,093 to 36,119 bytes (−5.2%). Fifth
application of the Rev 33 core/subsidiary split, under Rev 43's
criterion — *a figure that cannot be quoted from the core without first opening another file does
not belong in the core* — plus a second one this revision adds: **a narrative that exists in full
somewhere else does not belong in the core either, because a digest of it is a second source of
truth about why a decision was made.** That is Rev 41's argument about generated locale files,
applied to prose about the past rather than prose about the present.

**(i) The header's revision digest → this file.** The header carried condensed summaries of Rev
41–46 above the live-constraint list, while this file carried the authoritative narrative of all of
them. Every claim in the digest was already here, in more detail and with its reasoning intact — so
the digest's only distinct content was its *selection*: six revisions chosen, one to five lines each,
with the rest of Rev 33–40 represented by a pointer. **A selection is an editorial judgement that
nothing recorded, and it decayed the way an unowned judgement does**: the Document Map's row for this
file still read *"Rev 33–42"* four revisions after Rev 46 was written. The header now states where
the history is, that none of it is summarised, and that it must be read before assuming why an
invariant reads the way it does. **The `**Revision:** N (current)` stamp stays** — it is the file's
own version, not a narrative, and `check-section-refs.ts` aside it is the only machine-legible thing
in the header.

**(ii) §2's figures and tables → `docs/reference/commercial-model.md` §2.1–§2.3.** Rev 43 moved §2.1's
BOM / COGS / GM% columns and kept name, tier, retail-in-force and modalities on the argument that
those are "what identifies a configuration." **Half of that argument does not survive inspection.**
The Retail column is a figure set governed by §2.1's own rule that no figure may be quoted without
first reading `docs/np_cost_001.md` — the exact condition Rev 43 used to evict the cost columns, and
it applied identically to the column beside them. The Modalities column restates §3, which is itself
a pointer to `docs/reference/modality-stack.md`. And `docs/reference/commercial-model.md` §2.1
already repeated both columns verbatim *"so this table stands on its own"* — so the core's copy was
not the only copy, and two copies of a price table is the failure Rev 41 is named after. **What
survived is the tier mapping**, which is not a figure and is not derivable from §1: §1 defines T1 as
FDA-exempt wellness and T2 as a 510(k) target but never says which configuration is which, and a
regulatory question about Home Premium needs that line. It is now one prose line of six names
grouped by tier, and the Tier column was added to the reference table so nothing was destroyed by
the move.

**§2 is a pointer section that is not empty, and the four things it keeps are the point.** §2.2's
*keyed to peak draw, not price* and *any PD-compliant charger must work — the app informs, never
blocks*; §2.3's *only authenticated consumable* and *every consumable prompt is measurement-triggered,
never calendar-triggered*. None is a number, and each constrains code written by someone who would
never open a commercial file — an app developer writing a charger warning or a consumable reminder.
§2.1's pricing guardrail and §2.1a's *break-even binds before margin* stay for the same reason in the
other direction: they are what makes a cost answer safe to give, and constraint 1 of the header
already depends on the first of them.

**Each rule was also written into the receiving file, which is the part a relocation usually gets
wrong.** A rule the core keeps *and* the reference file omits is a rule that disappears the moment
the core is slimmed again — and one of these was already in that state: **§2.3's
measurement-triggered invariant existed nowhere but CLAUDE.md.** `docs/reference/commercial-model.md`
§2.3 had the consumable table with no statement of the rule governing it, and
`docs/reference/data-architecture-detail.md` §5.2 states the analogous rule for *reminders*, not
consumables. Moving §2.3 without carrying the sentence would have deleted an invariant that
`docs/np_acc_priority_001.md` cites by section number.

**And carrying it surfaced a contradiction that was previously split across two files.** The
consumable table's audio-cup-foam row names *"Calendar reminder"* as its trigger, which the invariant
forbids. `docs/np_acc_priority_001.md` raised this and deliberately declined to resolve it —
inventing a seal or compression proxy is the design work the invariant protects — but with the rule
in CLAUDE.md and the row in the reference file, nothing put the two in a reader's view at once.
They are now adjacent, with the contradiction flagged and still unresolved: **it is recorded here as
surfaced, not fixed**, because choosing between a new measurement trigger and a reasoned exception is
a principal decision (Product + FW).

**(iii) Every "retired" framing leaves the core, on the ground that nothing here was ever published
or used externally, so there is no outside reader who needs to be told what a thing used to be.**
Three occurrences, each reframed rather than simply deleted, because in each case the routing or the
rule was doing work the retirement narrative was only decorating. The **risk-file Document Map row**
read *"index + disposition of the retired RISK-01…26 register"*; a reader arriving with an ISO 14971
question needs the ID range to grep for, not the register's status, so the row now reads *"the ISO
14971 index, and the disposition of every RISK-01…26 ID."* The **`docs/superseded/README.md` row**
read *"Retired documents — index with successors named · tracing why something changed"*; the row is
the core's only pointer to those files, so deleting it would have made them unreachable from here,
and *"tracing why something changed"* was never the live hazard anyway. It now names the hazard
itself: *"Earlier document versions — index naming the current document for each · you have a figure
or a file and need to confirm it is the current one."* **§16 was the one that mattered, and it was
inverted rather than trimmed.** It read *"Retired term: Health Data Record (HDR) — ambiguous,
replaced throughout all documents"*, which describes a past event and leaves a reader to infer the
rule. It is now the rule: **never name a health data record "HDR" or "Health Data Record"**, because
the term does not say whose record it is, which is the one thing §5 turns on. The section title lost
the word "CHANGES" for the same reason — it states a convention, not a history of one. **Two things
were added that the old wording did not carry.** The prohibition is now scoped: **`HDR` meaning a
binary *header*** — `blob(n) = HDR(8) + …` in the LittleFS and NVRAM documents — is a different word
and is explicitly fine, which the old note's *"replaced throughout all documents"* flatly
contradicted for seven live uses. And **the rule turns out to have a live violation**, which is why
inverting it was worth doing rather than deleting it: `docs/reference/data-architecture-detail.md`
§5.2 specifies predictive-maintenance Phase 2 as a *"fleet-trained LSTM on **HDR** sensor
trajectories"* — the retired term, in the record sense, in a locked section's detail file — and
`docs/np_fw_emmc_002.md` §G quotes that phrase back verbatim. **Not fixed here, and deliberately
so:** the phrase is quoted as specification wording in two places, so the correction (`SHDR`, per
§5.2's own *"SHDR-based"* heading) has to change the source and both quotations together, which is
an edit to a locked section's detail file and not a CLAUDE.md revision. Recorded as found.

**Nothing broke.** Only `scripts/check-section-refs.ts` reads CLAUDE.md, and only its top-level
numbered headings; §2 and every one of §2.1 / §2.1a / §2.2 / §2.3 is retained as a stub, so the
inbound citations still resolve. No design decision changed, no figure was rewritten, and no locked
section's substance was altered.

## Earlier revisions

**Rev 46 (2026-09-15) — §3 and §4.2: one charge ceiling becomes two, because it was always two, and
the interlock that enforces them starts enforcing.** A locked decision changed, and the figure it
replaces had no source anywhere in the document set.

**What the single number could not say.** §3 and §4.2 locked *"40 µC/cm² hardware limit"* across the
whole electrical tier. That figure is real and correct — as a **per-phase** limit for **pulsed**
stimulation (Shannon/McCreery). It was being applied to a **session-cumulative integral**, which is a
category error in two directions at once. For tDCS, DC through a 35 cm² pad, it gives a 1.4 mC budget
that 2 mA exhausts in **0.7 s** — less than the 30 s ramp this same firmware enforces as a *minimum*.
For BES/tACS, VNS, cervical VNS and clinical tACS, all charge-balanced biphasic, net delivered charge
is ~zero by construction, so integrating |I| over a session measures nothing physical at all and
would have tripped every one of them in 0.4–1.0 s. §3 now carries **150 mC/cm² per session** for the
DC channels and **40 µC/cm² per phase** for the charge-balanced ones, and §4.2's interlock row names
the waveform split rather than one number.

**Why this was not a units fix.** Tracing the ceiling for `OI-CHARGE-05` found the citation chain was
**circular**: `NP-DT-001` DI-SAFE-01 — a design *input* — cited CLAUDE.md and NP-SW-001; NP-SW-001
asserted it with no source; CLAUDE.md locked it with no source. No paper, no standard, no predicate
device. Under 21 CFR 820.30(c) a design input whose source is the document asserting it is not yet a
design input, so this was a **first-time derivation, not a reversal** — nobody's recorded judgement
was being overturned, because none had been recorded. `NP-DT-001` §3.2.1 now derives both ceilings
from anchors outside this repository, and marks the 150 **PROVISIONAL** pending Regulatory sign-off
(`OI-CHARGE-06`) rather than presenting a derivation as an approval.

**The clinical consequence was weighed here rather than inherited.** At the old ceiling the routine
**2 mA × 20 min** protocol was unavailable, and 13 of the 14 shipped predefined tDCS protocols
exceeded it. That is not a defect in the protocols — 68.6 mC/cm² is the most common protocol in the
literature and the median of this repository's own 237-study database. The alternative fix, inflating
the declared pad areas until the ceiling was satisfiable, was rejected: pad areas are physical facts
about the hardware, and `OI-CHARGE-04` exists precisely to stop software assuming them.

**Why it belongs in the core rather than in the detail file.** The same shape as Rev 41 and Rev 45:
an invariant stated in one place that the rest of the tree could not tell was wrong. Three
independent documents had already pasted the pulsed waveform onto tDCS — `ABBREVIATIONS.md` defined
tDCS as *"DC, charge-balanced biphasic pulses"*, which is self-contradictory — and none of them was
close enough to the enforcement to notice. A ceiling whose period is unstated is a ceiling every
reader has to guess at, so §3 now states the period as well as the unit in both rows.

**And the control was inert while all of this was being reasoned about.** `np_hub_control_main.c`
passed the safety MCU no commanded current at all, so nothing accumulated: `OI-CHARGE-01` was logged
CLOSED with only its safety-MCU half wired, and DI-SAFE-01, FMEA SW01-M03 and NP-FW-BENCH-001's
*"never bypassable"* were describing a control enforcing nothing. It enforces now. **Read that as the
reason both figures are stated with their period in the core**: the wrong ceiling did no damage only
because nothing was checking it, which is not a safety argument.

## Earlier revisions

**Rev 45 (2026-09-13) — §6.0: "never asked" and "said stop" are different states, and the boolean
that was standing in for both could not hold the difference.** No decision changed. §6.0 has said
since it was locked that withdrawing blanket consent "stops **ALL** research data flows"; the
sentence is unchanged, and this is what makes it true of the code.

**What the flag could not say.** `ResearchConsentState.blanketConsentGranted == false` describes a
user who never turned L3 on and a user who turned it off, and §6.0 treats those differently — only
the second stopped everything. Withdrawal deliberately leaves the nine L2 categories ticked, because
clearing a stored preference is its own decision and not one withdrawal should smuggle in. The
consequence, once Rev 44's ingestion gate existed to expose it, was that a user who withdrew from
all research kept being admitted studies through the categories they had never gone back to un-tick
— research data flows continuing after the user stopped them. `blanketConsentWithdrawnAt` records
the true→false transition, and `blanketConsentWithdrawn` outranks L2 at the gate.

**Why it belongs in the core.** This is the third instance of one shape in four revisions: Rev 42
(a timeless tier could only answer §6.1's retroactive question yes), Rev 44 (one invitation type had
to pick one meaning of silence for everyone), and now this. In each, a locked sentence was true of
the specification and unrepresentable in the model, and in each the model quietly answered in the
permissive direction. The guard against a later change re-collapsing them is naming the distinction
where the invariants live, not only where the field is declared.

**Three properties of the marker are load-bearing**, and all three are the kind a refactor can
remove without appearing to change anything: the **store** sets it and no screen can (the consent UI
commits a whole state and cannot see a transition, so a stale commit could otherwise erase it); it is
guarded on the **transition, not the value**, like §6.2.5's analytics teardown, so withdrawing
something never granted does not bar an L2 participant; and **re-granting clears the condition, not
the record**.

**Where the detail is.** The gate's step 3a, the dashboards' three postures, and the migration note
are in `docs/reference/consent-engine.md` §6.3. Record: `docs/status/completed-decisions.md`,
2026-09-13.


**Rev 44 (2026-09-10) — §6.2 and §6.3: the device is the last gate on a study descriptor, and L3's
engagement notification is not an L2 consent request.** No decision changed. Both sentences the core
gained were already implied by locked text; neither was implementable, and one of them the code had
quietly decided the other way.

**Why the L3 clause needed saying in the core.** §6.2 has said since it was locked that a blanket-
consent user "still receives per-study *engagement* notifications, **not consent requests**". Read as
copy, that is a wording rule. Read as behaviour, it is a statement about **what silence means**: an
unanswered consent request means *not participating*, an unread engagement notification means
*participating*, because L3 already answered. `StudyInvitation` had one shape for both, so it had to
pick one of those defaults for everyone — and it had picked the L2 one. An L3 user would have been
re-asked a question they had told the product to stop asking, and counted out of a study they were
in. That is the same failure mode as Rev 42's: a locked sentence that no model on either platform
could express the other half of. `StudyInvitation.Posture` now carries which question is being put,
an engagement notification records participation at ingestion, and its opt-out is a withdrawal
rather than a decline. A later change that collapses the two back into one type re-takes the
decision, which is why the clause is in the core rather than only in the owning file.

**Why the ingestion clause needed saying in the core.** §6.3's workflow has NeurOne review the
study, build the eligible list and send the invitation — all server-side — and the device's job
looked like receiving it. `ConsentStore.addInvitation()` did exactly that: it took a *finished*
invitation, approved element list and "what they CANNOT see" prose included, from whatever called
it, which on both platforms was only ever a unit test (`OI-CONSENT-03`). Three things follow from
making the **signed descriptor** of §5.3 step 1 the thing that crosses onto the device instead.
§5.3's k≥10 and ≥1-week floors become enforceable, and the device is the only place they can be
enforced, because §5.3 puts the anonymisation on the device. What a study cannot see becomes the
**complement** of what it asked for, computed on-device, rather than a sentence written by the party
asking for access. And verification is **closed by default** — the only `StudyDescriptorVerifier`
shipped refuses every descriptor — so the transport that does not exist yet (`OI-CONSENT-07`) cannot
be built without also supplying the signature check. All three are structural facts a later change
can undo by re-admitting a ready-made invitation.

**Where the detail is.** The gate's six checks and why their order is load-bearing, the two
postures, and what deliberately was not built are in `docs/reference/consent-engine.md` §6.3.
Record: `docs/status/completed-decisions.md`, 2026-09-10.

**Rev 43 (2026-09-13) — core/subsidiary split extended a third time; no design decision changed.**
The always-loaded core went from 38,680 to 34,153 bytes (−11.7%). What moved, and the one criterion it moved
under: **a figure that cannot be quoted from the core without first opening another file does not
belong in the core.** It can only be misremembered there — which is the same argument Rev 41 made
about a generated file sitting in the working tree, applied to prose.

**§17 → `docs/reference/localization.md` (the largest single relocation, ~3.9 KB).** §17 was 20% of
the core and had no subsidiary file. The core keeps the rule and the surface table — text lives in
`locales/*.json`, code carries a key, the three per-platform files are git-ignored build outputs
that are never edited or committed, module-level tables hold keys, firmware is exempt — because
those are what a line of app code needs at the moment it is written. The generator mechanics, the
iOS two-hook requirement and the CI run that proved it, the `bun`-on-`PATH` consequence, the
placeholder/plural rules, the modality `_NAME`/`_DESC` derivation, the not-translated list and the
`PENDING_PATHS` reach are §17.1–§17.6 of the new file. **The trigger is unchanged and is the point:**
the core still fires on *any* non-firmware code generation, and now says to read the detail file
before adding a key — which is when the mechanics are needed, and when `locales/*.json` is open
anyway.

**§2.1's BOM / COGS / GM% columns → `docs/reference/commercial-model.md` §2.1.** The core keeps
each configuration's name, tier, retail price in force and modalities included — what identifies a
configuration, and what 231 uses of those names across the tree resolve against — plus the invariant
that every T1 row is gross-margin negative and every figure is a floor excluding term **U**. It no
longer carries the figures. Rev 40 had kept the table on the reasoning that "a cost or margin
question usually starts there"; that holds for the identity columns and fails for the money ones,
because §2.1's own rule is that **no figure may be quoted, cited or acted on without first reading
`docs/np_cost_001.md`**. A number that cannot be used from where it is read is not an invariant, and
the cost model is the most-revised content in the document set — Rev 38 replaced every figure and
`OI-HEXTILE-06` will move them again. §2.1a's illustrative figures went with them for the same
reason; the core keeps the shape of the finding (break-even binds before margin; both Pro rows are
profitable today; `OI-COST-08` and `OI-COST-09`). The ★ box-contents list moved as packaging detail.

**§4.3, §4.4, §4.5, §4.7 → `docs/reference/hardware-detail.md`.** Per-layer shielding dB, fit-system
ranges and materials, the mode/draw/PD/runtime table and the status-LED behaviour are lookup data of
exactly the kind the core's own preamble warns is not there. The core keeps each section's
invariant: combined 35–45dB ELF / 40–60dB RF and palladium-not-silver; one adult SKU for 52–62cm;
the two peak draws §2.2's charger policy is keyed to; and that stealth mode never suppresses a
safety fault. **§4.1 and §4.2 did not move** — §4.2 is one of the three live constraints and is
cited more than any other hardware section. §4.6 did not move either; four one-line modes are not
detail. **This was the weakest of the four relocations on size** (~0.8 KB): §4's bullets were
already compressed, so a stub carrying the invariant costs nearly what the bullets did. It was taken
for consistency with the §2.1 rule, not for the bytes.

**§5.1's two contents enumerations → `docs/reference/data-architecture-detail.md` §5.1.** The core
keeps both defining tests, the when-in-doubt rule, the conditional-redaction rule, each record's
owner/access/storage lines, and a representative handful of each contents list. The full
enumerations are in the file that was already authoritative per field. Deliberately conservative:
§5.1 is the second-most-cited section (109 inbound citations) and the lists are how a new field gets
classified quickly, so the exemplars stay.

**Considered and rejected: §6.2's layer table.** `consent-engine.md` already carries the complete
per-layer table, so the core's abridged one looks like a duplicate. Replacing it with prose saved 73
bytes and cost scannability. It stays.

**What the reduction actually cost, recorded because the estimate was wrong.** The moved text is
~7.4 KB; the file shrank by 4.6 KB. Two things eat the difference, and both are structural rather
than avoidable: **a stub that carries a section's invariant costs 30–60% of the bullets it replaces**
(§4 was the extreme case — 2,285 bytes of already-compressed bullets became 1,484 bytes of stub, an
0.8 KB return on a whole new file), and **the Document Map grows by a row per subsidiary file**, so
it went 5,114 → 5,610 bytes and is now the second-largest block in the core. The lesson for a Rev 44:
relocation pays where the source is *prose* (§17 returned 3.9 KB of its 7.6), and barely pays where
the source is already a dense list. Sections whose content is mostly invariant — §3, §5.1, §6.0,
§6.2 — were left alone for that reason, not overlooked.

**Nothing was broken by this.** Only `scripts/check-section-refs.ts` reads CLAUDE.md, and only its
top-level headings; every §N and every subsection number is retained as a stub, so the 663 inbound
citations still resolve — the gate reports 781 citations against 8 valid sections, all resolving.
Content was relocated verbatim except where a stub restates an invariant in fewer words. Record:
`docs/status/completed-decisions.md`, 2026-09-13.

**Rev 42 (2026-09-09) — §6.1: a clinician grant's reach is scoped in time, because the tier alone
could only ever answer §6.1's retroactive question one way.** No decision changed. §6.1 has said
since it was locked that *retroactive and prospective access are presented as separate consent
decisions even when made simultaneously*, and the sentence in the core is unchanged. What changed is
that the sentence is now implementable, and one clause was added saying so.

**Why it needed saying in the core rather than only in the owning file.** `ClinicianConsentGrant`
derived `approvedElements` from `tier`, and a tier is timeless. Raising it — which was the entire
body of the unreferenced `expandClinicianAccess()` that `OI-CONSENT-02` was raised about — hands the
clinician each newly added element over every session ever recorded. The workflow was not merely
unbuilt: the one piece of it that existed took §6.1's retroactive decision silently, and took it
yes, and **no model on either platform could express the other answer**. A grant now carries
`ClinicianAccessScope`s — element set, effective-from, and separately whether prior data is
included — so *"widen it going forward, leave my earlier sessions alone"* is representable. That is
the kind of structural fact a later change can quietly undo by re-deriving access from the tier, and
the core is where such a thing is guarded.

**Where the detail is.** The workflow as implemented — the differential document and the three
changes it refuses (nothing added; something removed; either side being the Research tier, whose
element set is IRB-defined per study descriptor so its emptiness is never a set), the two-step
presentation of the decisions, and the two halves deliberately not built (`OI-CONSENT-05`,
`OI-CONSENT-06`) — is in `docs/reference/commercial-model.md` §6.1. Record:
`docs/status/completed-decisions.md`, 2026-09-09.

**Rev 41 (2026-09-08) — §17: the generated locale files leave the repository; `locales/*.json` becomes
the only committed copy of any user-facing string.** Rev 40's §17 already named canonical as the
place a string is edited, and `sync-locales.ts --check` failed CI when a generated file drifted from
it. That was a staleness check, not a single-source rule: the String Catalog, the eleven web copies
and the eleven `values*/strings.xml` were all still tracked, so the same 1,420 strings were committed
four times over, and every one of those files was sitting in the working tree, editable, looking
exactly like source. **The failure mode this closes is not hypothetical — it is the one §17 was
written after.** 26 keys once existed only in the committed `.xcstrings`, 18 of them referenced by
iOS source, and the NP-HFE-002 rewording of two setup strings was applied to the catalogue alone;
regenerating would have reverted live copy to instructing users to listen for a bone-conduction tone
the firmware no longer emits. A `--check` job reports that after it has happened. It cannot stop the
edit being made in the wrong file.

**What changed.** The three generated trees are git-ignored and regenerated at build time by each
app's own build: a `canonicalLocales` Vite plugin (web, alongside the `canonicalProtocols` plugin
that made the same argument for `protocols/`), the `syncLocales` Gradle task (Android), and the
NeurOne target's first build phase (iOS). Android's output moved out of the source tree entirely,
into `<buildDir>/generated/res/locales` registered as a res `srcDir`; web's moved from
`app/web/src/locales/` — where it sat beside the hand-written `supportedLocales.ts` — to
`app/web/src/generated/locales/`, so one ignored directory replaces a glob inside a source folder.
The iOS catalogue keeps its path, because the `.xcodeproj` carries a file reference Xcode resolves
at project load. **Generator output is byte-identical to what was committed**; only the location and
the tracking changed.

**Consequences worth knowing.** (i) `sync-locales.ts --check` is retired and replaced by
`--verify-untracked`, which asserts no generated artifact is tracked — including the two retired
output locations, where a re-committed file would silently win over the generated tree rather than
announce itself. Staleness was only ever a symptom of the outputs being committed; with them gone
the invariant to guard is the absence itself. (ii) **The Android and iOS builds now require `bun` on
`PATH`**, which was previously true only of the web build; `android-ci`, `ios-ci` and both compiled
CodeQL legs install it. (iii) **A canonical locale edit no longer touches `app/ios/**` or
`app/android/**`**, so those workflows' path filters gained `locales/**` and `scripts/sync-locales.ts`
— without that, a canonical change that breaks the String Catalog or the Android resource merge
would not have run the workflow that catches it. (iv) The web locale tests now read canonical rather
than the generated copy: every assertion they make is a property of the source of truth, and against
the generated tree they would have restated what the generator guarantees by construction while
passing just as happily on a stale copy. No key, no string value and no rule from Rev 40's §17
changed.

> **Rev 40 (2026-09-01) — core/subsidiary split extended; no design decision changed.** The
> always-loaded core was reduced from ~69 KB to the invariants that bear on most conversations. The
> revision history (this file), the commercial model (§2.1a ladder, §2.2 charger tables, §2.3
> consumables, §6.1 clinician tiers), the full modality specifications (§3 T1/T2 detail), the §5.1
> boundary-resolution list, §5.2/§5.3 detail, and the §6.2 rationale plus §6.3 portal moved to
> `docs/reference/`. **Every top-level section (§1–§6, §16) and every subsection number is retained
> in CLAUDE.md as a stub carrying its invariant and naming the file that now holds the detail**, so
> the 663 inbound `CLAUDE.md §N` citations in firmware, app code and the DHF still resolve — the
> regression `scripts/check-section-refs.ts` exists to prevent. Content was relocated verbatim, not
> edited.
>
> The three `docs/status/` logs gained a "How to read this file" block with grep recipes, and
> `completed-decisions.md` was **reordered chronologically** by each entry's own date (2026-05-09 →
> 2026-08-31), with the 30 entries carrying no date of their own collected in a labelled block ahead
> of the timeline in their previous relative order. That reordering is a one-time exception to the
> file's append-only rule, taken by principal direction: no entry's text changed and none was added or
> removed — the entry set was asserted identical before and after, and the previous order is in git
> history.
>
> **Rev 39 (2026-08-16) — RETAIL PRICING UNLOCKED by principal direction. No price is set by this revision.** Taken against Rev 38's finding that all four T1 configurations are gross-margin negative. Unlocking the constraint turns the arithmetic around — GM% becomes the input, retail the output — and yields a **ladder, not a number**, published as new **§2.1a**: break-even Home Standard **$1,196–1,278**, target margin **$1,869–1,997** (a 2.20–2.35× increase on $849); Core $955–1,121; Home Lite $1,445–1,587; Home Premium $2,475–2,637. **The six configurations keep the prices currently in force** — choosing new ones is a separate commercial decision, and §2.1a records four things to weigh first. **Break-even binds before margin does**: Home Standard cannot be sold below ~$1,196 at any margin, already 1.4× its current price. **Three consequences the lock was concealing.** (i) The T1 and T2 ladders **collide** — Home Premium at $2,475–2,637 against a Pro Entry at $4,999 makes §1's two-tier structure one tier with a regulatory footnote (**OI-COST-08**). (ii) **Pro is where the *target*, not the cost, is the thing to question** — both Pro rows are profitable today (+$2,499, +$10,163/unit) and only holding 73%/81% demands $9K and $20K; this is **not** a mandate to raise T2 pricing. (iii) **Every competitive price claim is live again** — `docs/reference/competitive-position.md`'s comparisons were safe *because retail was locked*, and that justification is gone; at $1,997 Home Standard is ~40% of a ~$5K Vielight, not 17%, and becomes a direct Sens.ai price peer (**OI-COST-09**). **Binding sequence: `OI-HEXTILE-06` must be decided BEFORE any price is set** (**OI-COST-10**) — silicon PD plus a 20-tile build moves Home Standard's target-margin retail $1,997 → ~$1,383, so pricing first prices against a cost that decision invalidates. All §2.1a figures inherit §2.1's floor status: term **U** is still excluded, so this is the *least* retail would have to move. §2.2 charger policy remains unaffected — it is keyed to peak draw, not price. See `docs/np_cost_001.md` Rev 2 §8.
>
> **Rev 38 (2026-08-16) — §2.1 BOM / COGS / GM% re-derived against the hex-tile architecture. Retail unchanged and still locked. All four T1 configurations are now gross-margin negative.** The old columns (Core $168–169 / 42% … Pro Full $1,506 / 81%) were built on the retired five-zone-module design; they are superseded by `NP-COST-001`, and the stopgap caveat in `NP-DB-005` §4 is replaced with real figures. **Home Standard goes $405 → $897–959 BOM, +36% GM → −41% to −51%.** Pro is unaffected (+50% / +73%) because its retail is 3–6× its cost. The dominant term is not the cluster tier the brief was scoped around — it is **`NP-HW-HEXTILE-001` §6.4's $11.53/tile driver + metering, of which ~$10 is two InGaAs photodiodes**; at 30 tiles that is $346 against a $405 BOM, and **none of `OI-HEXTILE-06`'s three options, alone or combined, restores a positive T1 margin** (NP-COST-001 §6). Two corrections of record: (i) the brief's three deltas are **not additive** — `NP-DRV-SHELL-002` **Rev 2** §10.1's $175–225 already contains the $114.12 controller tier *and* the $32–64 socket arrays, and supersedes Rev 1's $125–216; (ii) `NP-HW-HUB-001` §8's blanket "every figure is VOID" banner is **stale**, because `OI-HUB-C17c` resolved against D-4 and the TIA/mux/ADC lines survive (OI-COST-06). **`OI-HUB-C08` is NOT closed and cannot be**: `OI-HEXTILE-02` has selected no 660/808 nm emitter, so *the dominant BOM line has no unit price on either side of the subtraction* — every figure is therefore a **floor** excluding term **U**, and OI-HUB-C08 is additionally under-scoped because it never covered the emitter-count delta (600 → up to 2,814 emitters). **Three inputs this model needs do not exist anywhere in the document set** and are recorded as assumptions, not decisions: per-configuration tile population (OI-COST-01), whether every configuration carries the full 18-cluster L1 (OI-COST-04), and — because no single BOM→COGS rule is recoverable from the six published pairs — a per-configuration COGS multiplier (OI-COST-05). §2.2 charger policy **confirmed unaffected** (peak draw unchanged; concurrency held at ~5–6 tiles by `NP-HW-HEXTILE-001` §9). Socket count ~80 remains **PROVISIONAL** pending REG-1/ACT-1 and every derived figure inherits that.
>
> **Rev 37 (2026-08-16) — a priori research consent goes from four onboarding screens to two; no consent axis removed, no withdrawal path weakened.** `§6.2` now separates **layers from screens**: L1–L4 remain the four consent layers — the units of the data model, the withdrawal surfaces and every citation elsewhere — presented across two screens. **S1 "What you get back" (L4 + L1) comes first**, because L1 and L4 were always the same question (L1 asks *may we contact you*, L4 asks *what about*), and because leading with reciprocity is consistent with §6.2's own reasoning. **S2 "What you share" (L2 + L3) carries two controls, not one.** Selecting all nine categories is **not** blanket consent: L2 is *scope*, L3 is *posture*, and "everything, but ask me" is a real position that survives only while both axes do (§6.2.2). **Select-all deliberately does NOT auto-enable the blanket toggle** (§6.2.3) — ticking nine boxes expresses breadth of interest, not a wish to stop being consulted; the usability objection is answered with copy, not state. §6.2.4 states the three copy rules that keep an L4-first screen from becoming an inducement, reusing `NP-MOD-ID-001` §7.5's honest-exchange framing. **One latent defect fixed as a precondition of the merge:** `withdrawBlanketResearchConsent()` was correct and tested on both platforms, but **nothing on iOS called it from the UI** — turning blanket consent off in Research Preferences committed through `updateResearchConsent()` and skipped the analytics teardown, while Android routed it correctly. Merging L2+L3 onto one commit-at-the-end screen would have made that bypass the ordinary path. The teardown is now enforced at the store ingestion point on a **true→false transition** of blanket consent (§6.2.5), guarding the transition rather than the value so a category-only edit still cannot trigger it. Withdrawal granularity and effectiveness are unchanged.

> **Rev 36 (2026-08-12) — §5.1's accelerometer boundary gains one bounded, self-closing exception; §5.2 gains the cohort that feeds it.** No locked decision was reversed: §G.3's prohibition stands unqualified for every device that has not opted in, which is the fleet by default. The problem it solves is that `drop_detected` and `maintenance_alert` are computed from two numbers (`15.0f` g; 3 drop-bearing gaps) guessed before any hardware existed, while §G.3 prohibits every field that could validate them — **the spec forecloses the evidence needed to make itself correct**, so NP-PRIV-001 HIGH-01's own Option C was a destination with no road to it. NP-FW-EMMC-002 Rev 2 §H opens a time-boxed, opt-in, **warranty-owner**-consented window collecting a coarsened per-gap impact histogram, purpose-bound to predictive-maintenance training, rows deleted at close. **The window is denominated in records, not calendar time** — this device has no battery or coin cell, so its RTC has no backup domain and wall time is re-supplied by the phone; a calendar expiry would be both losable and settable backwards. Consent rests on there being no identifiable subject (NP-MOD-ID-001 §7.5.1) and is *stronger* than that precedent: a duty map at least requires the device to be **worn**, and a drop does not. **Two limits recorded rather than papered over:** §G.3's ban on a cumulative drop count and on per-drop timing is **already defeated by aggregation** over `shdr_accel_records`' per-gap rows, independently of §H (OI-EMMC2-11, principal decision required); and §G.2's "rolling 7-**day** window" was never implementable for the same clock reason, so it is now counted in session gaps, which changes what `maintenance_alert` means (OI-EMMC2-10). See `docs/status/completed-decisions.md` (2026-08-12).

> **Rev 35 (2026-08-12) — one locked §5.1 boundary resolution reversed: the fault-latch `tick_ms` conditional suppression was itself a leak.** The 2026-07-14 gate reported fault-latch `count` unconditionally but zeroed `tick_ms` only for `NP_SAFETY_STATUS_CARDIAC`, so the observable pair `count > 0 && tick_ms == 0` identified a cardiac fault **with certainty and with no correlation work** — a one-bit oracle *strictly worse* than no redaction, because a bare relative SysTick value is meaningless without a session record SHDR does not hold, whereas the redaction pattern is self-interpreting. **New general rule, now binding across the design: a redaction applied conditionally on a sensitive predicate leaks that predicate** — it must be unconditional, or the predicate must not be inferable from the pattern of redaction (the "no such user" vs "wrong password" failure shape). Two findings made during the fix: the suppression **never protected the cardiac predicate at all** (the hub already publishes it as `fault_log.fault_type = 'CVNS_HR_CUTOFF'` by the locked "safety interlock log → SHDR" rule), and **fault event timing was already classified UHDR unconditionally** (`fault_log` has no timing column; `np_log_shdr_fault()` discards `session_ms` for every caller). Resolution: `tick_ms` is not SHDR-reportable at all; `status`/`slot`/`count` go through one fixed-shape marshaller `np_fault_latch_build_report()`, replacing the two independent accessors whose independence *was* the oracle. IEC 62304 Class C reporting-path change only — no safety function altered. NP-FW-EMMC-001 → Rev 2. See `docs/status/completed-decisions.md` (2026-08-12).

> **Rev 34 (2026-08-11) — zone-module-era documents retired; document conventions changed. No design decision changed.** Four documents that specified, inspected, risk-assessed or tooled the retired 5-zone-slot module were moved to `docs/superseded/` and replaced per artifact: `NP-FAI-ZM-001` → `NP-FAI-001` + `NP-ART-001` + `NP-FAI-HUB-001`; **`NP-RISK-001` — which was the ISO 14971 baseline risk file, not merely a stale spec** → `NP-RISK-002` (disposition of all 26 RISK IDs — 5 retired, 20 carried, 1 closed-confirmed) + `NP-RISK-003` + `NP-RISK-004`; `NP-DRV-SHELL-001` → `NP-DRV-SHELL-002` (already) + `NP-REV-SHELL-001` (the review *record*, which had no successor); `NP-TOOL-ZM-001` → `NP-TOOL-HEXTILE-001`. **Every retired and superseded document now lives in `docs/superseded/`**, indexed with its successor. **Three conventions changed (`NP-CONV-001` Rev 3 §4):** the **filename of a controlled document is its serial and nothing else** (§4.0), and the rule is *exclusive* — nothing that is not a serialed document may look like one — enforced by `bun scripts/check-doc-filenames.ts`; 25 files were renamed to their serial (`neurone_shell_fpc_routing_review.docx` → `np_drv_shell_001.docx`, and 24 more). Vendored files are the sole exception (§4.0.3). Also: document revisions are now **positive integers**, not letters (the alphabet had run out at `Rev AA`), with a published positional mapping so historical citations still resolve — PCB, database-schema and vendor revisions deliberately keep letters; and **no active filename carries revision information** (`neurone_design_brief_r5.docx` → `np_db_005.docx`). **Read `docs/np_art_001.md` before asking whether an artifact has a spec** — it records that nine of fifteen manufactured artifacts have no owning document, four of them shipping T1 modalities.

> **Rev 33 (2026-07-15) — structural reorganization.** This core file now holds only the always-relevant invariants (product, config, modalities, hardware, data architecture, consent). The large archival/reference sections were moved to subsidiary files under `docs/` and are listed in the Document Map below — read them on demand only when a conversation needs them. This keeps the context loaded every session small. **No design decisions changed; content was relocated, not edited.** When a locked decision changes, update the relevant subsidiary file (and note it in `docs/status/completed-decisions.md`).
