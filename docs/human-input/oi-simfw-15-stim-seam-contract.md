# OI-SIMFW-15: what the stimulation seam's charge total and current sign mean

**Status:** DRAFT, not sent. **From:** a Claude Code session working for the principal (2026-11-11). **To:** Firmware
**GitHub issue:** [#826](https://github.com/stevehickman/NeuroPulse/issues/826)
(hub drivers), with EE (the stimulation DAC board, which does not exist yet). No addressee is named. **Owning item:**
`OI-SIMFW-15` (`docs/np_sim_fw_001.md` §13). Related: `OI-FMEA-09`, GitHub #826.

**This is a proposal, not a decision.** One comment was added to `np_sw02_platform_hal.h` (§2, item 1), because the
meaning it states was already written in `np_hub_types.h`. Nothing else changed.

## 1. The questions

`np_sw02_platform_hal.h` declares `np_mod_stim_hal_read_charge()` and `np_mod_stim_hal_read_current()` with no statement
of what they return, so a DAC driver or a simulator answers by guesswork. Two things need an owner's answer.

**Q1. What does `np_mod_stim_hal_read_charge(pair)` total?** `np_mod_stim.c` says "uint16_t µC (session total)".

| Option | What it means | Cost |
|---|---|---|
| **A. DC only; a charge-balanced waveform reads 0** | The integral of the delivered current's magnitude since the DC command began. BES/tACS reads 0, because a session integral of a charge-balanced waveform is not a dose (CLAUDE.md §3: never compare a per-phase limit with a session integral). This is what the simulator does today | A comment. The type problem below remains |
| **B. The integral of the magnitude for every waveform** | One rule, and the AC figure is a number someone will compare with the 150 mC/cm² DC ceiling | Invites exactly the comparison §3 forbids |
| **C. Remove the seam** | The value is logged to UHDR (`charge_uC`) and read by nothing else. The safety MCU's ceiling uses the commanded current and never this read (`np_charge_monitor.c`) | A UHDR record layout change (`np_session_log.c`), which the EDF and app readers follow |

**Whatever is chosen, the type cannot hold a session.** 16 bits of µC saturates at 65.5 mC: a 2 mA tDCS command reaches
that in about 33 s, and the §3 ceiling for a 25 cm² pad is 3.75 C. So today's "session total" stops being one half a
minute into a session at the maximum current. Widening it changes the seam and the `charge_uC` field of the UHDR
stimulation record, so it is a decision and not a detail.

**Q2. What sign does `np_mod_stim_hal_read_current(pair)` carry for DC?**

| Option | What it means | Cost |
|---|---|---|
| **A. A magnitude** | Always zero or positive. The cross-check (`np_stim_xcheck.c`) already takes the magnitude, and says "DC polarity may read back negative" | A comment. A reversed output is invisible to the hub |
| **B. Signed, positive in the commanded direction** | A current flowing against the commanded polarity reads negative, so a reversed output can be told from a correct one | A comment now. Acting on the sign is a new check in `np_stim_xcheck.c` (`OI-FMEA-09`), which needs its own requirement and a hazard to trace to (CLAUDE.md §18) |
| **C. Signed, positive when pair-A is the anode** | The physical direction, whatever was commanded | The caller must combine it with the command to judge it |

**Recommendation: Q1 A with the width question raised to whoever owns the UHDR stimulation record, and Q2 B.** A keeps
the dose language of §3 intact. B costs nothing until a check uses it and is the only option under which a polarity
reversal, which swaps anode and cathode under the person's electrodes, can ever be detected. Whether to detect it is
Risk's question and is not asked here.

## 2. What is known

1. **The polarity argument is already defined**, in `np_hub_types.h`: `polarity` 0 is the anode at pair-A, 1 is the
   cathode at pair-A. The seam passes it unchanged (`np_mod_stim.c`). The seam header now says so, citing that
   definition; this is a comment and changes no behaviour, so it has no test.
2. For BES/tACS the current read is already defined as the peak magnitude over the sample window (`np_mod_stim.c`,
   `OI-STIM-06`), because the cross-check compares it with the commanded amplitude.
3. `np_platform_stub.c` traps on both reads: no DAC driver exists, and the delivered-current read is a stub
   (`OI-FMEA-09`).
4. The simulator reports 0 charge for AC and a magnitude for current, and says so (`np_sim_stim.c`).

Not known: what the DAC board will be able to measure, since it is not designed; whether anything will ever need the
hub's charge total, given that the Class C ceiling does not use it.

## 3. What I need back

An option for Q1 and for Q2, who decides the width of `charge_uC`, a short reason, and your name, role and date. When
answered, `np_sw02_platform_hal.h` states both, the simulator's model follows, and `OI-SIMFW-15` closes.
