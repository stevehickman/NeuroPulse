# Risk, clinical and device-run request: OI-ZONE-01, the safe / caution / danger zones

**Status:** DRAFT, ready to send; not yet sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Software (`OI-ZONE-01` in `docs/status/pending-decisions.md`; `docs/reference/safety-zones.md`; register rows `UC-073` to `UC-077`)
**To:** Risk and Clinical reviewers for Part A; whoever holds the devices for Part B (names to be assigned; the item's owner column reads "Risk + Clinical + App")
**Response needed:** Part A, a ruling on each of Q1 to Q3 with rationale; Part B, pass or fail for each box, with the device, OS version and any log line for a failure; names, roles and a date.

## 1. What the zone model is

A protocol's electrical dose is reduced to a few axes. Each has a **caution** and sometimes a **danger** boundary.
A block's zone is its worst axis, and a protocol's zone is its worst block.

| Zone | What happens |
|---|---|
| Safe | Runs without a word |
| Caution | The validator warns with an acknowledgement id naming the dose; the compiler refuses the protocol unless that id is acknowledged |
| Danger | The validator reports an error and the compiler refuses the protocol, whatever is acknowledged |

It is **app-side and advisory to the safety MCU**, which still enforces its own ceilings. Of the shipped library
of 68 single protocols (2026-10-07), 66 are safe, 2 are in caution (`ADHD Focus`, the 0.75 Hz sleep tACS) and none
is in danger.

## Part A. Questions for Risk and Clinical

### 2. What is known about the boundaries

| Axis | Applies to | Caution | Danger | Basis |
|---|---|---|---|---|
| Charge density per phase | BES/tACS, clinical tACS, VNS, cervical VNS | 10 µC/cm² | 40 µC/cm² | Danger is the existing hardware ceiling (Shannon 1992, McCreery 1990). Caution is a quarter of it, below the 12 µC/cm² at which McCreery saw damage (Cogan 2016): a position relative to two figures, not a derivation (`UC-073`) |
| Shannon *k* | rectangular phases of 1 ms or less | 1.0 | none | The literature boundary is 1.5 to 1.85 on cortical-surface electrodes; 1.0 is a placeholder (`UC-074`) |
| RMS current density | the same | 1.0 mA/cm² | 3.0 mA/cm² | No derivation (`UC-075`) |
| Mean current density | the same | 100 µA/cm² | 300 µA/cm² | No derivation (`UC-076`) |
| Session charge density | tDCS | 100 mC/cm² | 150 mC/cm² | Danger is the existing hardware ceiling (`OI-CHARGE-06`); caution is a placeholder (`UC-077`) |

Every figure except the two hardware ceilings is a **placeholder** held in one file so it can move without a code
change. The boundaries divide by the fixed pad areas, which are provisional and unmeasured (`UC-006`), so a smaller
real pad raises every density in proportion. Frequency and duty enter only through the two current-density axes; there is no
axis on frequency alone.

**Why this matters now.** With the 2 mA auricular ceiling removed and a 40 mA stop-gap in its place, the
current-density danger boundaries are what stop a high-frequency, high-current auricular protocol from
compiling: they bind long before 40 mA (about 6 mA at 500 µs and 25 Hz). They are placeholders doing real work.

### 3. Questions

**Q1. Are the placeholder boundaries acceptable as app-side advisories until they are derived?** For each axis
in the table, say: keep as is (advisory, placeholder), replace with a derived or cited figure (and give it), or
drop the axis. Under `CLAUDE.md` §19 a placeholder stays in the register until justified, so "keep" means it
stays listed as unjustified.

**Q2. Should any danger boundary be enforced on the safety MCU, not only in the app?** At present only the
charge-density ceilings are hardware. The two current-density boundaries are app-only, so a build of the
compiler that skips them would still reach the MCU. Options: leave them app-side; move one or both to the MCU
(a Class C change, with its own requirement and test); or decide per modality. This overlaps `OI-VNSCLIP-09`
(whether the clip needs a current ceiling) and `OI-SW03-01` (whether the app's own refusals are a risk
control).

**Q3. Is the acknowledgement design acceptable from a risk point of view?** The principal decided two things:
either the user or a clinician may acknowledge, on the same screen (2026-10-07); and **no separate
acknowledgement record is kept**, because that the protocol ran indicates it was acknowledged, since the
compiler refuses a caution-zone protocol without the ids (2026-10-08). Nobody records who acknowledged, because
an acknowledgement is a decision about the person (`CLAUDE.md` §5). The principal said to revisit only if a
regulator or clinic asks for an audit trail, which would also need a §5 ruling on where it lives. Does Risk
accept that, or does it want an audit trail now?

## Part B. Device run of the acknowledgement screen

### 4. What is built, and what has never been run

The screen lists a protocol's cautions with **one switch per caution**, because an id names one dose. Confirm is
disabled until every switch is on; cancel, and swiping or tapping the sheet away, acknowledge nothing. The ids go
to the compiler for that one run and are dropped.

Where it exists, as far as the sender knows: a shared Compose dialog (`CautionAcknowledgementDialog.kt`), an iOS
SwiftUI view (`CautionAcknowledgementView.swift`) and a web dialog (`CautionAckDialog.tsx`, **not mounted** because
the web app has no launch path for it). Windows has no UI; its compiler takes the ids. **Which hosts mount the
shared dialog today was not checked.** The screens have unit tests but **have never been run on a device**.

### 5. Boxes to tick (each host that mounts the screen)

Use the shipped `ADHD Focus` (tDCS, 102.9 mC/cm² a session) and the 0.75 Hz sleep tACS as the two caution
protocols. A pass is the behaviour written; anything else is a fail.

- [ ] Select a **safe** protocol → no caution screen; it uploads.
- [ ] Select a **caution** protocol → the screen appears and **lists each caution on its own line with its own switch**.
- [ ] **Confirm is disabled** until every switch is on.
- [ ] With one switch off, nothing uploads.
- [ ] **Cancel**, or dismissing the sheet by swipe or tap outside, acknowledges nothing and nothing uploads.
- [ ] With every switch on and Confirm pressed, the protocol uploads (or compiles).
- [ ] Select the same protocol again → the screen appears again (the acknowledgement was not kept).
- [ ] Edit the protocol so its dose changes (for example a longer tDCS duration) → the old acknowledgement no
      longer applies and the screen shows the new dose.
- [ ] A **danger** protocol (for example a tDCS dose over 150 mC/cm²) is **refused even if every switch is on**.
- [ ] Nothing about the acknowledgement appears in logs or anywhere the app stores data (check the app's own
      log and its stored files for the ids).
- [ ] Record the device, OS version, the app build, and whether the user or a clinician mode was used.

## 6. What we need back

1. Part A: a ruling on Q1 to Q3, with the rationale in two or three sentences (`CLAUDE.md` §18), and for Q1 the
   figure and its source where a boundary is replaced.
2. Part B: pass or fail per box and host, with the details above.
3. Conditions that would reopen a ruling (for example a measured pad area or a derived boundary).
4. Names, roles and dates.

## 7. What happens next

Rulings are recorded in `docs/status/pending-decisions.md` (`OI-ZONE-01`) and `docs/status/completed-decisions.md`,
and the boundaries move in `common/npps/constants.json` (with the register rows). A boundary that moves to the
safety MCU becomes its own requirement first. Nothing in the repository acts as if an answer has been given.
