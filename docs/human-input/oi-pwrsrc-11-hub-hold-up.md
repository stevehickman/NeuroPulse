# Safety and electrical question for review: OI-PWRSRC-11, what the outputs do when the last power source goes

**Status:** DRAFT, ready to send; not yet sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Systems Engineering (`OI-PWRSRC-11` and `RISK-PWRSRC-01` in `docs/np_pwrsrc_001.md`; `OI-IFU-02` in `docs/np_ifu_001.md`)
**To:** Safety lead and EE lead (names to be assigned; the item's owner column reads "Safety + EE")
**Response needed:** a ruling on each of the four questions in section 4, a measurement plan or its result, names, roles and a date.
**Why now:** the principal accepted unplugging the USB-C cable as the way to stop a session when the app link is unavailable (`OI-UI-KMP-09`, 2026-10-28, `SWR-SS-08`). The user instruction that says so, `IFU-01`, is held until this item is answered.

## 1. The question

When the last power source is removed from a running session, what does each stimulation output do on the
way down, and is that an acceptable end of stimulation?

Plain form: the headset has no battery, so an unplug is a power failure at the speed of the hub's bulk
capacitance. We do not know how long the rails hold, what the outputs do while they collapse, or whether
that is safe enough to tell a wearer "unplugging the cable is a safe way to stop a session."

## 2. What is known

**The facts in the repository**
- There is no battery or coin cell (`NP-FW-EMMC-002` §H). The only stored energy is the hub's **22 F
  supercapacitor**, which `CLAUDE.md` specifies as absorbing LED duty-cycle transients. It is **not specified
  as a hold-up reserve**. No hold-up requirement exists, and no controlled document gives its rail voltage
  or usable depth of discharge (`NP-PWRSRC-001` §10.1).
- `RISK-PWRSRC-01` is rated **High**, OPEN: "stimulation continues into a collapsing rail after the last
  source is removed", with the effect "uncontrolled termination of an active tDCS/tACS/VNS output,
  bypassing the hardware-enforced 30 s ramp." No acceptance criterion is defined for it; the file says it
  needs a hold-up bench measurement that does not exist.
- The proposed mitigation (decision D-6 in `NP-PWRSRC-001`, routed to the principal and Safety) is a latched
  `PWR_ADEQUATE` input on the safety MCU plus a **passive discharge path independent of the rails**.
  `OI-PWRSRC-11` is said to have to dimension the hold-up first.

**Two facts that bear on how serious "bypassing the ramp" is**
- **An abrupt end is already the designed fail-safe for faults.** The safety MCU cuts every stimulation enable in
  under 50 ms once its 1.5 s heartbeat watchdog expires (`CLAUDE.md` §4.2), and shutdown disables the enables *first*, "the
  graceful ramp is a comfort and dose consideration layered on top of a cut that has already been requested"
  (`NP-FW-HUB-001` §5.5). So an uncontrolled stop is not new. What is new here is that the cut is not
  commanded: the rails fall while the output stages are still driving.
- **The 30 s tDCS ramp has no recorded source.** The register records "no source found" for it (`UC-057`).
  Whether the ramp protects against a hazard or only against sensation is not stated anywhere read. That
  decides how much an abrupt end matters for tDCS, and it is a question for the reviewer, not something
  this file assumes.

**What is not known**
- Rail voltage, usable depth of discharge, and the time the rails stay above the level the output stages
  need, for each modality's load.
- What each output stage does on the way down: current falling smoothly, a step, a transient above the
  commanded level, or a residual DC level on a channel meant to be charge-balanced.
- Whether a source *loss* (cable pull, power bank dropping out, a PD renegotiation) differs from a source
  *sag* in the output behaviour. `NP-PWRSRC-001` treats them as one path (§10.3, D-7).

## 3. What the outputs are

| Output | Waveform class | What an abrupt end could leave behind (to be confirmed by Safety) |
|---|---|---|
| tDCS, HD-tDCS | DC, 150 mC/cm² per session | A step to zero from up to 2 mA, with the 30 s ramp bypassed |
| BES / tACS, clinical tACS | Charge-balanced, 40 µC/cm² per phase | An interrupted phase, so a residual of at most one phase's charge if the stage is not capacitively coupled |
| Auricular VNS | Charge-balanced, 40 µC/cm² per phase, up to 40 mA (stop-gap) | The same, at a larger current |
| Cervical VNS (T2) | Charge-balanced, cardiac interlock | The same, plus the lockout that must survive a power cycle (`OI-FAULTMSG-01`) |
| PBM, visual, audio | Optical / acoustic | Stops when the emitters lose supply; no stored charge in the body |

The right-hand column is the sender's reading and is labelled as such. It is not a finding.

## 4. Questions

**Q1 (Safety, ISO 14971). Is a power-loss end of stimulation, taken as it happens with no commanded ramp,
an acceptable safe state for each output class in section 3?**
Options and what each costs:
- **A. Yes, if the measurement shows the outputs reach zero without a transient and with no residual above a
  stated limit.** No hardware change. The IFU states the result. Needs Q2 and Q3.
- **B. Yes only with a defined power-loss ramp exception.** The hub holds enough energy to ramp down, so the
  22 F supercapacitor (or a new reserve) is specified as a hold-up reserve with a requirement, a rail
  voltage and a depth of discharge. Cost: a new hold-up requirement, probably a larger or additional store,
  and the 30 s ramp would still be longer than any plausible hold-up unless the ramp is shortened (see Q4).
- **C. Yes only with a passive discharge path independent of the rails.** The proposal in `RISK-PWRSRC-01`.
  Cost: new hardware on every output, and a specification of what the path must achieve.

**Q2 (EE). What is the measurement plan, and who runs it?** The sender proposes, for review:
- Rail voltage and the supercapacitor voltage against time after each source is removed, at the worst
  rated load per modality and at the T1 and T2 peak draws.
- Output current against time on each channel (tDCS, tACS, auricular VNS, cervical VNS) into a representative
  electrode load, and the DC level left on the charge-balanced channels after the stop.
- The same for a source *sag* (bank current limit, thermal foldback), not only a clean removal.
- The cable-pull case in Mode 3 from a power bank, where the cable to pull is the bank's.

**Q3 (Safety). What is the acceptance criterion?** `CLAUDE.md` §18 wants a derivation for any figure. The
sender has no basis to supply one. Candidate bases, for the reviewer to choose from or replace:
- *No worse than the existing fault cutoff:* the output is at zero within the time the watchdog cutoff
  already allows (under 50 ms after it fires, `CLAUDE.md` §4.2), with no excursion above the commanded level.
- *Inside the charge ceilings:* any residual charge or DC stays inside the per-phase and per-session limits
  of `CLAUDE.md` §3.
- *A basis the reviewer names.*

**Q4 (Safety, Clinical). Why does the 30 s tDCS ramp exist, and does it bind on a power loss?** The register
records no source for it. If it protects against sensation only, an abrupt end may be acceptable under A.
If it protects against a hazard, B or C is needed. This answer may also be that the ramp is itself an
equivalence-type figure (as the 25 Hz auricular ceiling turned out to be), in which case it should be
re-examined under `CLAUDE.md` §18 on its own item, not decided here.

## 5. What we need back

1. A ruling on Q1 (A, B, C or a mix by output class), with the rationale in two or three sentences,
   recorded as the derivation.
2. Q2: who runs the bench measurement, and when. Q3: the acceptance criterion and its basis.
3. Q4: the reason for the ramp, or that none is known.
4. What `IFU-01` may say in the meantime. Today it is held, and its draft says unplugging "is a safe way to
   stop a session." The reviewer can allow wording that claims only that stimulation stops, or hold it.
5. Conditions that would reopen the ruling (for example a measurement that shows a transient).
6. Reviewer names, roles and dates.

## 6. Out of scope here

The other open points in `IFU-01` are separate: whether power-up restarts, resumes or waits in Mode 3 after
the cable goes back in; which cable is reachable in use; and whether a physical stop or a timeout is
needed (`OI-UI-KMP-09`, which the principal can reopen). `OI-FAULTMSG-01` covers whether the cervical
lockout survives a power cycle.

## 7. What happens next

The ruling is recorded in `docs/np_pwrsrc_001.md` (`RISK-PWRSRC-01`, `OI-PWRSRC-11`), in
`docs/status/completed-decisions.md` and in the `IFU-01` release gate. Nothing in the repository acts as if
an answer has been given.
