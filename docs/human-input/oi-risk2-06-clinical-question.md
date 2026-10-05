# Clinical question for review: OI-RISK2-06 (hazard 25-d)

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Safety / Quality (`NP-RISK-002`, `docs/np_risk_002.md` §4.3.3, §4.3.4, §6)
**To:** Clinical reviewer (name to be assigned; the item's owner column reads only "Clinical + Safety")
**Response needed:** a yes/no ruling with rationale, the reviewer's name and role, and the date.

## 1. The question

After the cervical VNS (cVNS) cardiac interlock cuts stimulation for a person, should that same
person also be withheld **auricular** VNS until the cardiac lockout is cleared?

Plain form: does a cardiac event during cervical stimulation make auricular stimulation unsafe for
that person too, or is it safe to leave available?

## 2. Why we are asking

The cardiac cutoff was originally an all-channel stop. In Rev 6 (2026-09-23) the principal narrowed it
to withhold **cervical VNS only**, so that a cardiac event does not remove every therapy, such as
photobiomodulation or neurofeedback, from the person. That choice rested on "block only what it was
meant to block". Whether auricular VNS belongs in what it was meant to block was **assumed, not
answered**. This file cannot answer it: it is a question about vagal physiology and cardiac risk.

## 3. What the device does today

| | Cervical VNS (T2 accessory) | Auricular VNS (T1 and T2) |
|---|---|---|
| Site | Neck, over the cervical vagus | Ear (auricular branch) |
| Limits | ≤ 4 mA class, T2 only | 1–25 Hz, ≤ 2 mA, biphasic charge-balanced, 40 µC/cm² per phase |
| Cardiac interlock | Yes | **No.** Contact-confirmation interlock only |
| Withheld after a cardiac cutoff | Yes | **No** |

**The cardiac trigger** (`REQ-CVNS-09`): a heart-rate change of more than 15 BPM within 5 s, ending at
40 BPM or more. Detection takes up to 18 s from onset, and the enable line goes low within 100 ms of
detection. Then a 30 s lockout, app confirmation and a repeat impedance check are required before
cervical VNS re-enables.

**Scope of the block:** held per user profile, in safety-MCU flash, so it survives power loss. Every
other modality stays available, including auricular VNS.

## 4. Facts the reviewer should weigh

- The trigger is **not a confirmed cardiac diagnosis**. It is a heart-rate step from a PPG-derived
  signal and may be a **nuisance cutoff** (hazard 25-e). In simulation, 0–100 % of 120 s sessions trip
  under white-noise jitter. The real rate is unmeasured, and the rating is provisional on it.
  Adding auricular VNS to the block would extend the effect of every nuisance cutoff to a second
  therapy.
- Auricular stimulation targets the auricular branch of the vagus at lower amplitude, with a
  different anatomical pathway from cervical stimulation. We have not established whether it can
  produce a clinically relevant cardiac effect in this population.
- A cardiac event could have been caused by cervical VNS, or could be unrelated to it. The cutoff
  does not distinguish the two.
- Severity is rated S5 (the rating system's top level) because the hazard is a cardiac event in a
  person who may then be stimulated again. Probability is provisionally P1.
- A device-wide block was already rejected: it withholds a working therapy from people with no event.

## 5. Options and what each costs

**A. Leave as is (no).** Auricular VNS stays available after a cardiac cutoff. No code change.
Hazard 25-d is re-rated on the reviewer's rationale and closed.

**B. Withhold auricular VNS too (yes).** One-line firmware change to `NP_CARDIAC_BLOCK_MASK`,
with matching doc and test updates. 25-d is closed as a control. Cost: a T1 therapy is lost for
affected users, including after nuisance cutoffs, and some may switch profiles to get around it
(25-c).

**C. Conditional (for example, withhold until app confirmation only, or only within the 30 s
lockout).** This would be new design work and would need its own requirement and verification.
Only choose this if there is a clinical reason for a time or confirmation bound.

## 6. What we need back

1. Ruling: A, B or C.
2. The clinical rationale in two or three sentences. It is recorded as the derivation for the
   decision (CLAUDE.md §18).
3. Any condition that would reopen the ruling, such as evidence on the auricular-cardiac effect or a
   measured nuisance rate.
4. Reviewer name, role and date.

## 7. What happens next

On a ruling, Safety updates `NP-RISK-002` (25-d, §4.3.4, the §6 item), logs the decision in
`docs/status/completed-decisions.md`, and for B or C changes `np_safety_config.h`,
`NP-FW-CVNS-001`, `NP-SW-FAULTMSG-001` and the tests. The ratings are approved by the Quality Lead
(`NP-RM-001` §8.2), not by this review alone.

## References
`docs/np_risk_002.md` §4.3.3 (25-c, 25-d, 25-e), §4.3.4, §6 `OI-RISK2-06` · `docs/np_fw_cvns_001.md`
Rev 6, §5.4.1 · `docs/np_hw_cvns_001.md` `REQ-CVNS-09` ·
`firmware/safety_mcu/include/np_safety_config.h:202`
