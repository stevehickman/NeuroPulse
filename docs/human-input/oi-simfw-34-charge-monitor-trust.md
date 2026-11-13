# OI-SIMFW-34: is the hub trusted for the electrode area and the phase the charge monitor uses?

**Status:** DRAFT, not sent. **From:** a Claude Code session working for the principal (2026-11-11). **To:** the safety
**GitHub issue:** [#826](https://github.com/stevehickman/NeuroPulse/issues/826)
MCU's owner (not yet named) and the principal, with Risk. **Owning item:** `OI-SIMFW-34` (`docs/np_sim_fw_001.md` §22).
Related: GitHub #386 (electrode areas are unmeasured constants, `OI-CHARGE-07`), #826, #467; `OI-SIMFW-61`.

**This is a proposal, not a decision.** No firmware was changed for it. The charge ceilings themselves (CLAUDE.md §3)
are not in question and nothing here relaxes one.

## 1. What happens today

The Class C charge monitor derives both ceilings from an area and a phase the hub declares
(`np_safety_chan_limit_cmd_t`, `np_safety_chan_wave_cmd_t`). Each frame carries a checksum, not a signature, and is
applied whenever it arrives. Run in the simulator against the real firmware:

| Finding | What was seen |
|---|---|
| **The area can be raised mid-session** | 0.1 cm² raised to 25 cm² moved the DC budget from 7.5 s to 3.75 C, and the channel was not cut at 7.5 s. The last frame wins on the MCU |
| **Any area is accepted** | 65 535 mcm² (65.5 cm²) is taken as sent |
| **A pulsed channel with a phase of 0 µs is never cut** | Its charge per phase computes as 0 (`charge_per_phase_nc()` returns 0 for a zero phase). 65 mA was not cut in the run |
| **A declaration only grows** | Classes OR together and the longest phase wins. This is the strict direction and is not a finding |

CLAUDE.md §3 says both ceilings are "enforced against the electrode area in the signed descriptor". `NP-FMEA-001`
FMEA-M03-05 scores its residual on "Area travels in the **signed** descriptor, so altering it invalidates the signature
SW01-M07 verifies" and "the **smallest** declared area wins". Both are true of the hub (`np_session_runner.c`): the hub
reads the area from the descriptor whose hash the MCU verified and takes the smallest across commands. **Neither is true
at the MCU**, which receives the area on its own frame and cannot check it against the hash. So the Class C ceiling is
exactly as trustworthy as the Class B hub that declares its inputs.

## 2. The question

**Is the hub trusted for these two inputs?** If it is, say so in the FMEA. If it is not, pick how far to go:

| Option | What it means | Cost |
|---|---|---|
| **A. Trusted, and stated** | FMEA-M03-05 and the §3 wording are corrected to say the area and phase reach the MCU from the hub on unsigned frames, and the residual is re-scored on the hub's integrity | Documents only. A Class C control then rests on Class B code, which is the dependency the two-processor split exists to avoid |
| **B. Monotonic and bounded at the MCU** | No signature. Within a session an area may only shrink (the smallest wins at the MCU too) and a phase may only grow (already true). A pulsed or sine class declared with a phase of 0 is treated as not declared, so the declaration gate withholds the channel. Declarations are refused once the channel is granted | A small Class C change with host tests. Closes the three findings against a faulty hub. Does not stop a hub that declares a wrong area from the start. An upper bound on the area would be a new figure and is left out unless #386 supplies one |
| **C. Bound to the signed hash** | The areas and phases are carried in a block the session signature covers, and the MCU checks that block against the hash it verified before it applies them | A change to the descriptor format, the signing service, the app's compiler, the hub and the MCU. The strongest answer, and the only one that makes §3's sentence true at the MCU |
| **D. B now, C when the descriptor format next changes** | B's checks are built now; C is recorded as an open item against the next format revision | B's cost now, C's later |

**Recommendation: D.** B's three rules each fail closed, add no figure, and match rules the hub already follows (the
smallest area wins; an undeclared waveform is not granted). C is right in the end but touches five components and should
ride a format change that is happening anyway. A leaves a commanded-dose ceiling that one wrong frame can lift by a
factor of 250.

**Interaction with #386.** The product electrode areas are provisional constants (`OI-CHARGE-07`). None of the options
makes a wrong area right: they only decide who may change the area after the session starts. An upper bound on the area
at the MCU cannot be chosen until the electrodes are measured.

## 3. What is known, and what is not

Known from the run: the table in §1. Known from the code: the hub floors the area and takes the smallest across
commands; the MCU applies each frame as it arrives; `phase_us` 0 yields 0 nC per phase. Also recorded in the row and not
a finding: a charge or thermal trip withholds every channel at the next grant, not only the tripped one.

Not known: whether any shipped protocol relies on re-declaring an area mid-session (I found none); whether a pulsed
modality could legitimately declare a phase of 0 (I found none: a pulse with no width delivers nothing).

## 4. What I need back

The option, and for B or D whether "refused once granted" is wanted as well as "only shrinks". A short reason, and your
name, role and date. Until then the firmware is unchanged and the simulator's scenarios keep recording the behaviour as
built.
