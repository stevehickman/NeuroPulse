# Question for the principal: OI-PRIV-40 (NP-PRIV-ANALYSIS-004), may the fault-latch report carry the IMPEDANCE bit?

**Status:** ANSWERED 2026-11-23 by the principal: option D, extended to every SHDR record derived from the in-session check (§8). Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#968](https://github.com/stevehickman/NeuroPulse/issues/968)
**From:** NeurOne Firmware + Tooling (`OI-PRIV-40` (NP-PRIV-ANALYSIS-004) in `docs/status/pending-decisions.md`; firmware privacy review, Finding L2)
**To:** the principal
**Response needed:** an option for Q1, and for Q2 if Q1 is B; a short rationale; anything that would reopen it; your name, role and the date.

## 1. The question

The safety MCU's fault-latch report, `np_fault_latch_build_report()` (`firmware/safety_mcu/src/np_fault_latch.c:183-193`), is the one fixed-shape record any future hub-facing read of the latch must be built from. It reports three fields: `status`, `slot` and `count`. **`status` is the whole latched status byte**, so it carries `NP_SAFETY_STATUS_IMPEDANCE` (bit 3: "impedance check failed") whenever that bit was latched.

`scripts/check-redaction-shape.ts` classes IMPEDANCE as **sensitive** (a tissue property; `CLAUDE.md` §5.1 routes raw VNS and EEG impedance to UHDR). The same byte also carries `NP_SAFETY_STATUS_CARDIAC`, whose disclosure was ruled under the locked "safety interlock log → SHDR" rule. IMPEDANCE has no such ruling.

Plain form: **is "the electrode contact check failed on channel N" a fact about the device or about the person?**

## 2. What is known

### 2.1 Nothing writes the report to SHDR today

- The report's only callers are the simulator (`firmware/platform_sim/src/np_sim_safety_hal.c:370`, `:483`), which uses it to model the latch surviving a warm reset, and the host test `firmware/safety_mcu/tests/np_fault_latch_privacy_tests.c`.
- The comment above the builder says `count` "never leaves SRAM today" and that a future dedicated fault-latch SPI read command MUST be built from the report. No such command exists.
- The 8-byte heartbeat reply frame (`np_safety_tx_frame_t`) carries `status` and `fault_slot` to the hub. That is a device-internal control path: the hub's cervical VNS driver must act on IMPEDANCE (`np_mod_cvns.c:503`). It is not an SHDR write, and nothing proposed here changes it.

So the finding as worded ("the whole fault-latch `status` byte, IMPEDANCE bit included, is reported") is correct about the report and about the gate's blindness, and **overstated about consequence: no IMPEDANCE bit reaches SHDR through this report today.** It would, unchanged, the day the read command is built.

### 2.2 Masking the bit alone would create a new oracle

The instruction for this item suggested masking IMPEDANCE in `status`. Read against the code, a mask alone does not work:

- `np_impedance_check_poll()` (`np_impedance_check.c:145-150`) sets **only** `NP_SAFETY_STATUS_IMPEDANCE` (no `FAULT`, no `CUTOFF`), and sets `fault_slot` to the failing channel's slot. A recheck that passes clears the bit.
- `np_fault_latch_commit()` counts every change of `(status, slot)`.
- With IMPEDANCE masked, an impedance failure reports `status == OK` with `slot != 0xFF` and `count` incremented. No other fault produces "OK with a slot". **That pattern identifies the impedance failure, and the channel, with certainty**: the #272 shape (a redaction whose pattern reveals its predicate), which `CLAUDE.md` §5.1 rule 2 forbids.
- `count` also rises once per pass/fail toggle, so a run of impedance rechecks shows as a run of transitions whatever `status` says.

### 2.3 What the per-field rulings already say

`docs/reference/data-architecture-detail.md` §5.1: raw VNS and EEG impedance → UHDR; the contact-resistance trend → SHDR; cervical VNS gel-pad contact pass/fail per electrode (`CVNS_PAD_STATUS`) → **UHDR**, "derived from raw tissue impedance", never written to SHDR. A latched "impedance check failed" bit is a one-bit pass/fail of the same measurement.

### 2.4 Other routes to the same fact (not this item)

- The hub writes `NP_CVNS_SHDR_EV_IMP_FAILED` and `IMP_TIMEOUT` to SHDR from `log_cvns_event()`. That is `OI-PRIV-28` (NP-PRIV-ANALYSIS-004), #956, and the gate lists it as a known violation.
- The ruled `IMP_CROSSVAL` divergence flag (hub and MCU disagree) is SHDR and stays.

## 3. Questions

**Q1. How is IMPEDANCE classed for the fault-latch report?**

- **A. Sensitive (UHDR-leaning). The report never discloses it, by bit, slot or count.** Recommended. When the read command is designed, the report's shape changes so that an impedance-only transition is indistinguishable from no transition: for example `status` masked to device-class bits, and `slot` and `count` taken only from transitions that set a device-class bit. That is still one fixed shape, unconditional in the sense §5.1 rule 2 means (the projection is the same for every fault), but it is a redesign of a Class C reporting path and is done with the read command, not before.
  - *Cost:* the fleet loses a latched count of impedance failures from this path. The contact-resistance trend (already SHDR) is the maintenance signal for electrode wear. The simulator's warm-reset model, which restores the latch from the report, must read the latch by another route.
- **B. Device-class. The bit may be reported as it is.** The argument: the check is a device interlock; a failure is as often a dry or worn electrode as anything about the wearer.
  - *Cost:* it moves data toward SHDR against `CVNS_PAD_STATUS`'s existing UHDR ruling (§2.3), and needs §5.1 rule 1's positive demonstration that the bit says nothing about the person. On a shared device it is also a per-session contact history.
- **C. Drop `status` and `slot` from the report; keep only a count of device-class transitions.** The simplest shape that cannot leak.
  - *Cost:* also drops the deliberately published CARDIAC kind from this path. The hub's SHDR `fault_log` already carries the cardiac cutoff, so nothing is lost there.

- **D. Split the measurement by where it is taken** (added 2026-11-23, from the principal's question). The in-session check is taken through the wearer: skin, gel, hair, placement, anything worn in the path, the leads and the front end are one circuit, and a driver error also reads as "too high". It cannot tell a device fault from the person, so its result is **UHDR**, and the report never discloses it (as A). Device condition comes from a **separate device-only test**: the same measurement through a known load in a test harness or the maintenance dock, with no person in the circuit. That result is device-class by construction, as every maintenance result is under `NP-FW-HUB-001` D-41.
  - *Cost:* the device-only test does not exist. It needs a known load (a harness or the dock, `OI-FWHUB-25`), hub detection that a person cannot fake, and a signed maintenance session, because it drives an excitation current. Until it exists the fleet has no impedance-derived device signal.

**Q2. Under B, what does the classification of the bit in the gate become?** The gate's `PREDICATES` entry moves to `device`, with the ruling cited; the known violation for `np_fault_latch_build_report` then still fires on `CARDIAC` and needs CARDIAC's own ruling cited before it can be removed.

## 4. Present state while this is open

- No code change to the fault latch. Masking the bit alone (§2.2) would weaken protection by creating an oracle, so it is not done.
- `scripts/check-redaction-shape.ts` now flags the whole-status copy in `np_fault_latch_build_report()` and holds it as a known violation owned by `OI-PRIV-40` (NP-PRIV-ANALYSIS-004). Closing the item without changing the code fails the gate.

## 5. Recommendation

**Q1: A.** It follows "when in doubt → UHDR", matches `CVNS_PAD_STATUS`, costs nothing that is built (no SHDR consumer exists), and keeps the fault-latch report honest before the read command is designed. The design work under A is recorded against `OI-PRIV-40` (NP-PRIV-ANALYSIS-004) and done with the read command.

## 6. What each ruling changes

| Ruling | Changes |
|--------|---------|
| Q1 A | A per-field row in `data-architecture-detail.md` §5.1 (fault-latch IMPEDANCE bit → UHDR; never disclosed by the report). The report's projection is redesigned with the fault-latch read command, with host tests in `np_fault_latch_privacy_tests.c` proving an impedance-only transition reports identically to none. The gate's known violation is then removed. |
| Q1 B | A per-field row (→ SHDR) with the positive demonstration; the gate's `PREDICATES` entry changes; CARDIAC's ruling is cited for the remaining bit. |
| Q1 C | The report struct loses two fields; the simulator's warm-reset model changes; the privacy tests change. |

## 7. What I need back

1. The option for Q1 (and Q2 if B).
2. A short rationale (`CLAUDE.md` §18).
3. Any condition that would reopen it.
4. Your name, role and the date.

## 8. The answer (the principal, 2026-11-23)

**Q1: D.** In the principal's words: "Treat the existing impedance test as UHDR, if we keep it at all, and add a todo to extend the test harness to test impedance for the device only." Asked whether the ruling also covers the two SHDR records already derived from the in-session check, the principal answered **both move to UHDR**:
- the cervical VNS session summary's impedance-pass flag (`np_cvns_shdr_summary_t.impedance_check_pass`; was SHDR under `OI-PRIV-23` and `NP-FW-CVNS-001` §8.2), and
- the hub-versus-MCU divergence flag `NP_CVNS_SHDR_EV_IMP_CROSSVAL` (was SHDR under `OI-CVNS-HUB-11`).

**"If we keep it at all."** The check stays. It is the contact-confirmation interlock of `CLAUDE.md` §4.2 (VNS: "Safety MCU reads impedance; holds if contacts not confirmed") and the tES enable gate (`NP-FMEA-001` FMEA-M05-07). Removing it would remove a safety control, which is an ISO 14971 decision (`CLAUDE.md` §18). The ruling changes where its **result** may go, not whether stimulation waits for it.

**What it changed:**
- `data-architecture-detail.md` §5.1: a row for the in-session check, and the two earlier rows amended.
- `NP-FW-HUB-001` §6.3 and §6.9: the IMP_CROSSVAL sentence, and a catalogue row for the device-only test, with **`OI-FWHUB-36`** for building it.
- `NP-FW-CVNS-001` §8.2: notes on both rows that classed the pass/fail flag SHDR.
- `scripts/check-redaction-shape.ts`: the three writers of those flags are known violations under `OI-PRIV-40` (NP-PRIV-ANALYSIS-004) until the firmware changes.
- **Not yet done (firmware, no further ruling needed):** remove the flag from the cervical VNS summary (`REQ-CVNS-15` and its size assertion change), stop writing `IMP_CROSSVAL` to SHDR (the fail-closed block it accompanies stays), and reshape the fault-latch report so an impedance-only transition is indistinguishable from none, by bit, slot or count (§2.2).

