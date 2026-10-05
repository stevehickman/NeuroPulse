# Clinical and engineering request: OI-CVNS-13 (what nuisance-cutoff rate is acceptable?)

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Safety / Quality (`NP-FW-CVNS-001` §14.6, `OI-CVNS-13`; `NP-RISK-002` hazards 25-c, 25-e)
**To:** (1) A clinical reviewer, for the acceptable rate. (2) The embedded safety team, for the measurement. The item's owner column reads "Embedded safety team + Clinical". No names are assigned.
**Response needed:** the rate, the measurement plan and its owner, and the choice in section 5, each with a name and date.
**Related:** `oi-risk2-09-cvns-detection-adequacy.md` and `oi-risk2-06-clinical-question.md` go to the same clinical reviewer.

## 1. The two things asked

1. **Clinical:** how often can a cervical VNS (cVNS) session be cut by a **false** cardiac alarm before
   the cutoff stops being acceptable? Each trip costs a 30 s lockout, an app confirmation and a repeat
   impedance check.
2. **Engineering:** what **are** the missed and split R-peak detection rates at `RPEAK_IN`, on real A13
   PPG, from 50 to 100 BPM, at rest and with motion? Nobody has measured them.

The item cannot close until the first sets a target and the second says whether the device meets it.

## 2. Why it matters

The cardiac interlock was redesigned on 2026-10-01 so that it detects every change `REQ-CVNS-09` names.
That made it trip more often on things that are not cardiac events. A false trip is the safe direction
(stimulation stops), but the consequences spread:

- **Hazard 25-e (nuisance cutoff)** is rated S1 × P5 = ALARP, and the P5 rests on a rate nobody has
  measured (`NP-RISK-002` §4.3.3).
- **Hazard 25-c (profile switching)** is rated P2 on the assumption that a deliberate switch past two
  warnings is rare. A frequent nuisance cutoff gives people a **motive**. A high measured rate re-opens
  that rating (`NP-RISK-002` §4.3.5).
- The item **blocks T2 clinical release** and FAI-CV03.

## 3. What is known (simulation only)

Source: `NP-FW-CVNS-001` §14.6.1. Synthetic white-noise R-R jitter, fixed seed, 300 sessions per cell,
**120 s sessions**, through the hub's real rule and the real Class C code. White noise is
**pessimistic** for real heart-rate variability. These are not measurements.

| HR | R-R SD | 1 % missed beats | 3 % missed | 1 % split | 3 % split |
|----|--------|------------------|------------|-----------|-----------|
| 50 | 20 ms | 0.3 % | 6.3 % | 5.0 % | 27.3 % |
| 60 | 20 ms | 1.0 % | 7.7 % | 9.0 % | 35.0 % |
| 70 | 20 ms | 4.0 % | 21.7 % | 3.0 % | 35.3 % |
| 100 | 20 ms | 21.7 % | 74.0 % | 5.0 % | 21.7 % |

Figures are the share of sessions that trip. The 50 and 60 BPM "missed" columns reflect a hub fix made
on 2026-10-01 (before it, 61 % and 36 % at 1 % missed). With **no** artefacts, no session trips except at
100 BPM with 50 ms SD (10.7 %). The paced-breathing case (0.1 Hz) trips 27–100 % at 100 BPM and none at 50 or 70 BPM.

**What cannot be removed in the safety MCU.** It cannot tell a missed detection from a beat the heart did
not make. A median filter would remove the false trips but then **never detects dropped-beat bradycardia**
(rejected). A 12-interval window cuts trips most but needs the 18 s detection bound raised to about
24 s (held in reserve). So the remainder is a **detection-quality** problem upstream, in A13's PPG and the hub's detector.

**A second source is coming.** The ±5 BPM hub cross-check (`OI-CVNS-14`, built 2026-10-04) adds trips of its
own on a moving, noisy heart rate, and that rate is also unmeasured (`OI-CVNS-15` (b)).

## 4. What each person is asked

**Clinical reviewer**
- (a) A target: the largest acceptable fraction of cVNS sessions that end in a false cardiac cutoff,
  with the session length it applies to. Give one number or a range, and the reason.
- (b) Does the acceptable rate **depend on the user** (for example, higher for a clinic-supervised session
  than a home one)? The device does not know which it is.
- (c) Is a lockout per trip (30 s, app confirmation, repeat impedance) itself acceptable, or is the cost the
  reason the rate must be low? The requirement for it is `REQ-CVNS-09`, not this item.
- (d) Is it acceptable to **tolerate** a higher rate if the alternative is a longer detection bound
  (the 12-interval option)? This ties to `OI-RISK2-09` (a).

**Embedded safety team**
- (e) Who records PPG from A13, across 50–100 BPM, at rest and in motion, and how many subjects and sessions?
  The note in §14.6.2 says "recorded or bench PPG", with FAI-CV03 or earlier.
- (f) Run the measured R-R trains through `run_oi_cvns_13_sim.sh` in place of the synthetic ones, and
  extend it to include the `OI-CVNS-14` cross-check. Report the missed and split rates, and the false-trip rate.
- (g) Owner and date for both.
- **A caution on FAI-CV03.** `NP-FW-CVNS-001` names it as the venue, but `NP-REG-CVNS-001` describes it as a
  three-subject tolerability study needing IRB approval and a T2 prototype (status PENDING). I did not check
  whether it records R-peak detection quality. A bench or recorded-PPG measurement ("or earlier") may be the
  faster route, so say which you intend.

## 5. What happens with the answers

`NP-FW-CVNS-001` §14.6.2 lists three outcomes. **This request does not choose among them.**
1. The measured rate is within the clinical target, and the item closes.
2. It is not. A SW-02 requirement on missed and split rates is derived from the target. (Under CLAUDE.md §18 it is
   required **only once** the clinical rate exists, which is why no requirement figure is set today.)
3. It is not, and the 12-interval window with a ~24 s detection bound goes to the principal. That raises
   CLAUDE.md §4.2's detection bound and would need `REQ-CVNS-09` changed.

## 6. What this does not decide

- It does not change any threshold, window or latency.
- It does not set the clinical rate itself. The reviewer does, and the CEO records it.
- It does not close `OI-CVNS-15`, though the measurement serves both.
- It does not replace FAI-CV02 or FAI-CV03 on silicon.

## References
`docs/np_fw_cvns_001.md` §14.6, §14.6.1, §14.6.2, §14.7, `OI-CVNS-13`, `OI-CVNS-15` ·
`docs/np_risk_002.md` §4.3.3 (25-c, 25-e), §4.3.5 · `docs/np_hw_cvns_001.md` `REQ-CVNS-09` ·
`firmware/safety_mcu/tests/analysis/run_oi_cvns_13_sim.sh`
