# OI-SIMFW-21 (point 1): when must the intranasal driver re-read the sleeve?

**Status:** DRAFT, not sent. **From:** a Claude Code session working for the principal (2026-11-11). **To:** the
principal, with Firmware (hub drivers) and Risk. **Owning item:** `OI-SIMFW-21` point 1 (`docs/np_sim_fw_001.md` §18).
Related: `REQ-NASAL-10`, `REQ-FWHUB-38`, GitHub #826.

**This is a question, not a decision.** No firmware was changed. Point 2 of the item was fixed on 2026-11-09
(`REQ-NASAL-15`) and is not reopened.

## 1. The question

`np_mod_intranasal.c` reads the sleeve authentication once, in its init, and keeps the answer. A probe whose sleeve or
contact has since gone is still driven by the next command. **When must the driver check again?** Pick one:

| Option | What it means | Cost |
|---|---|---|
| **A. At every drive command** | The driver re-reads authentication and the photodiode contact before each command that starts or changes emission, and refuses (leaving nothing emitting, `REQ-NASAL-15`) if either fails | Two HAL reads per command. A sleeve removed while a command is already emitting is not seen until the next command |
| **B. At every telemetry tick while emitting** | As A, and the driver re-reads both on its periodic tick while a side is on, and stops both sides when either fails | A periodic read during emission. The period is a new figure and needs a reason (CLAUDE.md §18). A contact that flickers stops the session, so a debounce rule is needed, which is another figure |
| **C. On change** | The hardware raises an interrupt or a latched flag when the sleeve or contact changes, and the driver stops on it | Needs a hardware signal the accessory specification (`NP-HW-NASAL-001`, DRAFT) does not define |
| **D. The cached answer is right** | Admission is checked between sessions only (`REQ-NASAL-10` says "session admission"; the registry's idle rescan re-runs `detect()` every 500 ms between sessions, `REQ-FWHUB-38`). The row records why nothing more is needed | None in code. Needs a stated reason why a sleeve removed mid-session is not a hazard, for example that emission into air or an unsleeved probe is bounded by another control |

**Recommendation: A now, and B only if Risk names a hazard that A leaves open.** A adds no figure, matches the words of
`REQ-NASAL-10` (both checks for admission) at the moment emission is admitted, and fails closed. B and C each need a
number or a signal that nothing yet requires. D needs a hazard argument I could not find.

## 2. What is known

- `REQ-NASAL-10`: "Session admission requires both an authentication pass and a photodiode contact confirmation", traced
  to `np_mod_intranasal_detect()`.
- `detect()` reads both; `init()` reads authentication only and caches it; a drive command tests the cached flag.
- `np_hub_control_main.c` says a module pulled mid-session is "its driver's own interlock's business", and no interlock
  is specified for the sleeve.
- The sleeve is also the authenticated consumable (CLAUDE.md §2.3). This question is about emission, not about the
  replacement prompt, and no prompt trigger is proposed here.

Not known: what hazard an unsleeved or unseated probe presents while emitting (hygiene, eye exposure from a probe out of
the nostril, dose), and so whether a mid-command removal must stop emission and how fast.

## 3. What I need back

The option; for B, the period and debounce and where each is traced; for D, the reason; and your name, role and date.
When answered, the rule becomes a row in `NP-HW-NASAL-001` beside `REQ-NASAL-10`, the driver follows it, and
`np_mod_intranasal_tests` pins it. Until then the driver is unchanged.
