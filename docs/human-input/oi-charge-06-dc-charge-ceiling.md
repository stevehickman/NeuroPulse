# Regulatory question for review: OI-CHARGE-06, the 150 mC/cm² DC charge ceiling

**Status:** DRAFT, ready to send; not yet sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Systems Engineering (`OI-CHARGE-06` in `docs/status/pending-decisions.md`; `NP-DT-001` §3.2.1, `DI-SAFE-01`)
**To:** Regulatory reviewer, with a clinical reviewer for the T2 question (names to be assigned; the item's owner column reads "Regulatory")
**Response needed:** a ruling on each of Q1 to Q4, with rationale, the reviewer's name and role, and the date.
**Out of scope:** the 40 µC/cm² per-phase limit for charge-balanced channels (BES/tACS, VNS, cVNS, clinical tACS). It is unchanged and carries its Shannon (1992) and McCreery (1990) citation.

## 1. The question

The DC channels (tDCS and HD-tDCS) are limited to **150 mC/cm² per session per electrode**, enforced by the
safety MCU against the electrode area in the signed descriptor (`CLAUDE.md` §3, `DI-SAFE-01`). The figure
was derived from two anchors outside the repository, and it has had no regulatory review. Do you accept it
as the ceiling, and for which tiers?

Plain form: is 150 mC/cm² per session an acceptable hard limit for a consumer wellness tDCS device (T1), and
does it also make sense for the T2 small-electrode HD-tDCS?

## 2. What is known

**How the figure was derived** (`NP-DT-001` §3.2.1, 2026-09-15). The previous figure had no source; the chain
was circular (a design input whose source was the document asserting it).
- **Envelope.** `docs/tdcs_database_full.csv` holds 237 studies; 204 state current, area and duration
  together. Their per-session charge densities run from 8.6 to 600 mC/cm², median 68.6. 150 admits 92.6% of
  them. The most exposed large-pad RCT in the set is Brunoni ELECT-TDCS (2013 and 2017) at 2 mA for 30 min on
  25 cm² pads, **144 mC/cm²**. The ceiling excludes the small-electrode tail (4 cm² montages at 300 to
  600 mC/cm²).
- **Margin.** Liebetanz et al. (2009) measured a rat epicranial DC lesion threshold of 5,240 mC/cm², the only
  measured DC damage threshold in the evidence base. 150 is **35×** below it.
- **Clinical consequence.** At the earlier reading (40 mC/cm²), 13 of the 14 shipped predefined tDCS protocols
  exceeded the ceiling. At 150, all of them are admissible, and the ceiling binds at about 31 minutes of 2 mA
  on a standard pad.

**What it has not had**
- No regulatory review. `NP-DT-001` marks the figure **PROVISIONAL** until this closes (register row
  `UC-005`, in the justified table by external citation, with the check against the sources tracked separately).
- `IEC 60601-2-10` is cited (`DI-REG-02`) for current limits and waveform parameters. Its verification,
  **VE-11, is Open** (accredited-lab testing, planned for Month 10 to 14). It is a nerve and muscle
  stimulator output standard and sets no cortical charge-density ceiling.
- The 144 mC/cm² Brunoni protocol sits just under the ceiling. The margin to the one measured damage
  threshold is a rat figure on epicranial DC, not a human skin or cortical one.

**An arithmetic result the sender noticed while preparing this.** HD-tDCS (T2) declares a 3.5 mm Ag/AgCl
electrode of **0.0962 cm²** (`NP_HD_ELECTRODE_AREA_CM2`). 150 mC/cm² on that area is **14.4 mC**, which
is 7.2 s at 2 mA or 14.4 s at 1 mA. A conventional HD-tDCS session of 20 minutes at 1 mA is 1.2 C, or about
12,500 mC/cm² on that electrode. If the ceiling applies to HD-tDCS as `CLAUDE.md` §3 states, a normal HD-tDCS
session cannot run on the declared electrode. The sender has not checked how the Class C monitor treats the
CLIN_STIM channel in this case; the point is that the figure and the electrode are not consistent. This is
related to, and not the same as, `OI-TCAP-01` (the 2 mA, 6 A/m² and 40 µC/cm² limits cannot all hold on that
electrode), which is an EE and Clinical decision about the electrode size and is not decided here.

## 3. Questions

**Q1. Is 150 mC/cm² per session an acceptable T1 wellness ceiling for tDCS?**
- **A. Yes, as derived.** Record the sign-off; `NP-DT-001` drops PROVISIONAL (the VE-11 check stays open).
- **B. Yes, with a different number.** State it and the basis. This is a one-line constant on the Class C
  MCU (`NP_CHARGE_DC_LIMIT_MC_CM2`); the two app-side pre-flights read their own named constant.
- **C. Not until VE-11 or another test is done.** The figure stays provisional and nothing is decided.

**Q2. Does the same ceiling apply to T2, in particular to HD-tDCS?**
- **A. Same ceiling.** Then the HD-tDCS electrode, current or duration must change (`OI-TCAP-01`), because a
  3.5 mm electrode cannot run a conventional session under it.
- **B. A different ceiling for T2** (a number, with its basis, for small electrodes).
- **C. A different quantity for small electrodes**, such as a current-density criterion, and the 150 mC/cm²
  stays for large pads only.

**Q3. Should a current-density criterion (mA/cm²) sit alongside the charge-density ceiling?** The
Nitsche and Bikson safety framework states the limits that way, and the two together bound the published
envelope. Charge density alone limits the total on an area and says nothing about instantaneous density. The
app-side zone model has current-density axes for charge-balanced channels but none for DC
(`docs/reference/safety-zones.md`).
- **A. No.** Charge density is enough for T1.
- **B. Yes, as an advisory** in the zone model only.
- **C. Yes, as a hardware limit** on the safety MCU (a Class C change, with its own requirement and test).

**Q4. How does this relate to IEC 60601-2-10 and VE-11?** Does the regulatory pathway require VE-11 to pass
before a ceiling can be signed off, or can the sign-off precede it and the lab test confirm it later?

## 4. What we need back

1. A ruling on Q1 to Q4, and for Q1 and Q2 which tier each applies to.
2. The rationale in two or three sentences, recorded as the derivation (`CLAUDE.md` §18).
3. If the ceiling changes, the number and its basis.
4. Conditions that would reopen it (for example the VE-11 result).
5. Reviewer names, roles and dates.

## 5. What happens next

The ruling is recorded in `docs/np_dt_001.md` §3.2.1 and `DI-SAFE-01`, in `docs/status/pending-decisions.md`
(`OI-CHARGE-06`), and in `docs/status/completed-decisions.md`. Any change to the constant is a Class C
change with its own requirement and test. Nothing in the repository acts as if an answer has been given.
