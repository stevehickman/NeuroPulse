# OI-SIMFW-33: which faults a new session may clear, and what ends the signature "hard lock"

**Status:** DRAFT, not sent. **From:** a Claude Code session working for the principal (2026-11-11). **To:** the safety
**GitHub issue:** [#826](https://github.com/stevehickman/NeuroPulse/issues/826)
MCU's owner (not yet named), with Risk. **Owning item:** `OI-SIMFW-33` (`docs/np_sim_fw_001.md` §22). Related:
`OI-SIMFW-32`, GitHub #826, #467.

**This is a proposal, not a decision.** No firmware was changed for it.

## 1. What happens today

- A failed signature (slot 0xFD) sets `FAULT`. The next session start clears it (`np_session_sig_reenable()`), while
  fewer than `NP_SIG_FAIL_MAX` (3) consecutive failures have been counted.
- After three, the session start no longer clears it. The code calls this a hard lock: "only a power-cycle can clear
  `s_sig_fail_count`", "hard lock — require device power-cycle".
- **But a signature is still verified during the lock, and a good one sets the count to 0.** `FAULT` stays for that
  session, and the session start after it clears the lock. So the lock ends after one good signature and one more
  session start, with no power cycle. The comment and the code disagree.
- Every other slot is cleared by power-on only. Three corrupt signature frames (slot 0xFC) are not cleared by a later
  good signature or a new session.

No controlled document states the lock. `np_safety_config.h` says "consecutive Ed25519 verify failures → hard lock";
`NP-FMEA-001` §3.7 does not mention a failure count. So there is no requirement to implement, and the choice is open.

## 2. The questions

**Q1. What ends the lock after three consecutive bad signatures?**

| Option | What it means | Cost |
|---|---|---|
| **A. Power-on only, as the comment says** | Once the count reaches the maximum it is not reset by a good signature. A signature frame received during the lock is not verified | A small Class C change with a host test. A hub that sent three bad descriptors needs the headset unplugged, including in a clinic |
| **B. A good signature, as the code does** | The comments are corrected: three consecutive failures hold `FAULT` until a good signature has been seen and a new session starts | None in behaviour. The lock is then a rate limit and not a lock |

What the lock is for decides this. It cannot be a defence against forging a signature by trial: three tries or three
million make no difference to Ed25519. It can be a defence against a hub that is malfunctioning and keeps trying, and
for that purpose a good signature is evidence the hub has recovered.

**Q2. Should a corrupt-frame fault (0xFC) be recoverable at a session start, as a signature failure is?**

| Option | What it means | Cost |
|---|---|---|
| **A. No, power-on only (as built)** | Three corrupt frames end stimulation until power is removed | A noisy SPI link during session setup takes the unit out of use until unplugged |
| **B. Yes, by the same count rule as 0xFD** | A new session clears it, up to the same maximum | A small Class C change. A link that corrupts frames is then retried up to three sessions |

**Recommendation: Q1 B, Q2 A, and ask Risk whether the lock traces to a hazard.** B changes no behaviour and makes the
code's own description true. I found no hazard that A's stricter lock controls, and CLAUDE.md §18 asks for one before a
rule is tightened into a requirement. For Q2, a corrupt frame is a hardware-integrity signal and power-on is the
existing fail-closed answer. If Risk names a hazard for the lock, Q1 A follows and I would not argue against it: it is
the fail-closed reading.

**Two design inputs also bear on this.** `NP-DT-001` `DI-SAFE-02` ("fault latch requires explicit clear") and
`DI-SAFE-09` ("any safety cutoff latches the fault state; explicit app confirmation + repeat impedance check required
to re-enable") state a latch with an explicit clear for every safety cutoff. No explicit clear exists in the firmware
for any fault except the cardiac cutoff. How those two inputs should read is asked once, as Q9 of
`oi-simfw-27-interruption-policy.md` §8, and the answer here must agree with it.

## 3. What is known

Run in the simulator against the real firmware (`np_sim_safety_tests.c`): every statement in §1. Read from the code:
`s_sig_fail_count` is in zero-initialised RAM, so any reset, including an independent-watchdog reset, zeroes it while
the fault latch keeps `FAULT` (`OI-SIMFW-32`). Not known: how often a real hub produces a bad or corrupt signature
frame.

## 4. What I need back

An option for Q1 and Q2, the hazard the lock traces to if there is one, a short reason, and your name, role and date.
Until then the firmware and its comments stay as they are.
