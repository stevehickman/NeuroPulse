# Quality Lead request: OI-RISK2-05 (confirm the closure and approve the re-derived residuals)

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Quality (`NP-RISK-002` §6 `OI-RISK2-05`, closed 2026-09-25)
**To:** Quality Lead (interim: Steve Hickman, CEO), with Firmware.
**Response needed:** the confirmations in section 4, and a name and date for each.
**Read this first:** `OI-RISK2-05` is **closed**. Nothing is blocked and nothing is wrong that I found. This request exists because the closure changed safety-MCU behaviour and re-derived residual ratings, and I could find a recorded Quality Lead approval for only some of them.

## 1. What closed

The item found that `NP-FMEA-001` §3.5 (SW01-M05) scored controls the safety-MCU code did not contain,
and that §2's watchdog row relied on a hardware IWDG the firmware did not configure. On 2026-09-25 the
principal chose to **build the missing controls** instead of scoring them away (`NP-FMEA-001` Rev 12,
GitHub #437):

- a hardware **IWDG** (`np_hal_iwdg.c`), started just before the main loop at 1,000 ms nominal;
- a Class C **pre-arm hold** and a **3 s R-peak staleness cutoff** (`np_cardiac_interlock.c`, controls
  C2, C2a, C2b).

Its follow-on, `OI-FMEA-12`, then built a heartbeat sequence gate and two more Class C controls (`NP-FMEA-001`
Rev 13), and re-derived four more rows.

## 2. What I could and could not confirm

| Check | Result |
|-------|--------|
| The item is marked closed in `NP-RISK-002` §6 with its reasons | Yes |
| Hazard 25-b's owed test moved to `OI-RISK2-07`, not dropped | Yes |
| Rows still without a control routed to an open item | Yes, `OI-FMEA-12`, which has since closed in parts (a), (b), (d) |
| Quality Lead approval recorded for **FMEA-M05-09** | **Yes**, `NP-FMEA-001` Rev 21 |
| Quality Lead approval recorded for the other re-derived residuals | **I did not find one.** See below |
| The new controls run on silicon | **No.** Host-tested and mutation-checked only. IWDG on silicon is `OI-SWCI-49` |

**The residuals I could not find an approval for.** After Rev 12 and Rev 13, `NP-FMEA-001`'s module
summary shows these as S5 × P1 = ALARP:

- **SW01-M02** (SPI heartbeat watchdog): FMEA-M02-02, -03, -05, now scored on the IWDG and the sequence gate.
- **SW01-M05** (cervical cardiac interlock): FMEA-M05-01, -03, -06 as listed, with M05-07 re-derived to S4 × P1.

These were authored by "Systems Engineering + principal" (`NP-FMEA-001` Rev 12 and 13). A principal's decision to *build* a control is not the same act as the Quality Lead's approval of a *rating* (`NP-RM-001` §8.2). I may
have missed a record elsewhere, so please check before treating this as a gap.

## 3. Why it matters

Each of those rows moved from S5 × P2 (a score the earlier text had assigned) to S5 × P1, on the strength of a
control that exists only on the host. The ALARP justification for FMEA-M02-05 says an IWDG is "the feasible further
reduction whose absence leaves that ALARP justification incomplete". It is built now, but the scenario it
backstops (a hung main loop) has not been shown on hardware. RISK-25 moving to P1 waits on this
(`NP-RISK-002` §4.3.1).

## 4. Confirmations needed

1. **Approval of the re-derived residuals.** Approve, challenge or defer each: M02-02, -03, -05 and M05-01,
   -03, -06, -07. If an approval already exists, give its location so it can be cited.
2. **Is "host-tested, not on silicon" an acceptable basis for P1 at this stage?** `NP-RISK-002` already keeps
   P1 as a *target* and holds the system rating at P2 (§4.3.1). Confirm the FMEA rows may use P1 at
   unit level on that same reasoning, as FMEA-M05-09 does.
3. **The IWDG timeout.** `OI-SWCI-49` records a 1,000 ms nominal value and a 941–1,085 ms spread across the
   LSI's tolerance. Confirm the value is acceptable, or send it back to Firmware.
4. **Owners.** Name who closes `OI-SWCI-49` and who owns the IWDG silicon test.

## 5. What this does not decide

- It does not reopen `OI-RISK2-05`. A "challenge" opens a new item, and the closure stands.
- It does not change any rating. Approval is a separate act from the one that proposed the rating.
- It does not run anything on silicon. That waits on hardware and on FAI-CV02.

## References
`docs/np_risk_002.md` §4.3.1, §6 `OI-RISK2-05`, `OI-RISK2-07` · `docs/np_fmea_001.md` Rev 12, 13, 21; module
summary; FMEA-M02-02/-03/-05, M05-09 · `docs/np_sw_ci_001.md` `OI-SWCI-49` · `docs/np_rm_001.md` §8.2
