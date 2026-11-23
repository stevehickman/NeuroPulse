# Question for the principal and Safety: OI-PRIV-24 (NP-PRIV-ANALYSIS-004), resume on the open pause characteristic, and the §23 exception

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#952](https://github.com/stevehickman/NeuroPulse/issues/952)
**From:** NeurOne Firmware Engineering (`OI-PRIV-24` (NP-PRIV-ANALYSIS-004) in `docs/status/pending-decisions.md`; firmware privacy review `docs/np_priv_analysis_004.md`, Finding H4)
**To:** the principal (the `CLAUDE.md` §23 exception, `OI-SIMFW-73`); Safety / Risk (the ISO 14971 assessment, `NP-RM-001`)
**Response needed:** an option for Q1 and Q2, a yes or no for Q3, a short rationale for each, anything that would reopen it, your name, role and the date.

## 1. The question

`CLAUDE.md` §23 says nothing uploads or downloads in the clear. Two GATT writes were left open on purpose so that a failed
pairing can never refuse a stop: `SESSION_STOP` (0x000f) and `SESSION_PAUSE` (0x001a) (`REQ-FWHUB-81` (c), `UC-151`,
`OI-SIMFW-73`). The reason written for the exception was that these writes **"can only reduce stimulation"**
(`docs/human-input/oi-simfw-71-secure-channel-design.md` §10).

That was false. `SESSION_PAUSE` carries three codes: pause (0x01), panic (0x03) and **resume (0x02)**. Resume restarts
stimulation. Until this change the hub took it from any central in radio range, paired or not, and from any paused state,
including a session the photoparoxysmal detector had halted.

Plain form: **who may restart stimulation, and does the §23 exception still stand once resume is taken out of it?**

## 2. What is known

### 2.1 The finding, re-verified on 2026-11-21 against `main` at `49d6c51f`

| Claim | Code | Held? |
|-------|------|-------|
| The pause row has no encryption flag | `firmware/hub_control/src/np_gatt_server.c`, the `NP_GATT_SESSION_PAUSE_ID` row: `NP_GATT_PROP_WRITE` only | Yes |
| 0x02 goes to resume | `np_session_stop.c`, `np_session_pause_on_write`: anything not 0x01 or 0x03 that passed the value check calls `s_resume` | Yes |
| Resume runs from any `PAUSED` state | `np_session_runner.c`, `np_runner_resume` checks only `PAUSED` and the ramp gate; the run loop calls `do_resume()` on the event with no further check | Yes |
| A detector halt is a `PAUSED` state | `do_panic` (posted by `np_runner_halt(NP_HOLD_DETECTOR_DISCHARGE / _BLIND)`) sets `NP_SESSION_PAUSED` and the hold cause; nothing reads the hold cause before resuming | Yes |
| Resume is reachable only over GATT | `np_runner_resume` has one caller, `np_session_pause_init` in `np_hub_control_main.c` | Yes (no USB or button path) |

The finding held as written. One qualification of its weight: **the detector is built but not armed** (`REQ-FWHUB-67`,
`-70`: `np_ppx_feed()` has no caller, evidence level NONE), and photic driving at 3–30 Hz is refused outright in its place
(`REQ-FWHUB-65`). So a detector hold cannot arise on today's firmware. Every other pause could be resumed by a stranger.

### 2.2 What has been done (no ruling needed: it only raises protection)

**`REQ-FWHUB-81` (e), built 2026-11-21.** `np_gatt_on_write` refuses a resume (0x02) on `SESSION_PAUSE` with ATT 0x0F
(insufficient encryption) when the link is not encrypted. The write reaches no handler. Pause (0x01), panic (0x03) and
stop stay open, exactly as before.

- **The refusal does not depend on the hold cause.** Refusing only after a detector halt would tell a stranger whether the
  detector had fired, a fact about the wearer's brain. `CLAUDE.md` §5.1 rule 2 forbids that. A resume is refused on an
  unencrypted link after a person's pause, a panic and a detector halt alike.
- **Tests:** `np_gatt_server_tests`, `test_a_resume_needs_an_encrypted_link_whatever_holds_the_session` (`@verifies
  REQ-FWHUB-81#e`; a mutant without the guard fails five checks); `simulator/server/fw-bridge.test.js`, end to end through
  the twin running the real `np_gatt_server.c`.
- **The wire contract did not change.** Same characteristic, same byte. A central on an encrypted link sees no difference.
  Every app central was traced:

| Central | How it resumes | Effect |
|---------|----------------|--------|
| Compose shared app (Android, desktop/Windows, browser) | `NeurOneGattManager.requestSessionResume()`, writes 0x02 to `SESSION_PAUSE_ID` | None on an encrypted link. Every session it can start was uploaded over an encrypted link (`writeProtocolChunk` is gated on the encrypted-only serial read, `REQ-SW03-31`), and on reconnect it reads the serial again. A refused resume reaches `onCharacteristicWriteFailed` and `writeRefusals`. |
| iOS SwiftUI app | Retired by #946 (2026-11-22); iOS now runs the Compose app above | Nothing left to change. |
| React web app, `app/windows` C# project | No hub link or session screen | Nothing to change. |
| Simulator twin | Runs the hub's own `np_gatt_server.c` (`platform_sim/np_sim_gatt.c`); its bonded link reports encrypted at `gatt_init` | None; the new end-to-end test drops the link to show the refusal. The display-role simulator server (`simulator/server/index.js`) has no resume. |

### 2.3 What encryption does not give

**Encrypted is not authenticated.** The hub is told only that the link is encrypted. A link paired by Just Works is
encrypted, and anyone in range can pair that way (`OI-SIMFW-72`, undecided). So a stranger who pairs can still resume.
What (e) closes is the case of a central that never paired. What remains needs an answer to `OI-SIMFW-72`.

### 2.4 What `REQ-FWHUB-67` says about the detector hold

The halt "is a **pause that needs a second step** (stop, or resume from where it paused), never an automatic resume, and an
automated halt errs toward stopping". It does not say who may take the second step. The Compose app (every platform, iOS included since #946) puts the
Stop / Resume question to the person when `SESSION_HOLD` reads 3 or 4.

## 3. Questions

### Q1 (the principal). Does the §23 exception stand for stop, pause and panic, now that resume is out of it?

**Built 2026-11-23, whatever the ruling (`OI-PRIV-72`, #1011; `REQ-FWHUB-81` (f)):** the open writes leaked the wearer's session through their refusal codes. A stop or a panic answered 0x80 with no session running, and a pause during ramp-up answered 0x82, which happens only with tDCS or cervical VNS in the protocol. On a link that is not encrypted, a well-formed stop, pause or panic now answers OK whatever came of it; a malformed one is refused, the same in every state; an encrypted link gets the exact answer. Options A and B below now carry no state leak; under C the point is moot.

| Option | What it means | Cost |
|--------|---------------|------|
| **A. Keep stop, pause and panic open; restate the reason** | The exception's reason becomes "each of these can only reduce or end stimulation", which is now true. | None to build. A stranger in range can still stop, pause or panic a session (denial of therapy, not harm). |
| B. Encrypt pause (0x01) as well; keep stop and panic open | The review's suggested split. | A wire change for every central (pause on its own characteristic or behind 0x0F). It buys nothing against a stranger, who can still stop the session. A person whose pairing failed loses pause and must stop. |
| C. Encrypt all three, with a rule that a failed pairing cannot refuse a stop | The exception ends. | Needs a design for a stop that never waits on a key, which §23's reason for the exception says does not exist today. |

### Q2 (Safety and the principal; an ISO 14971 trade-off). Who may resume a session the detector halted?

| Option | What it means | Cost |
|--------|---------------|------|
| A. Any encrypted link (as built) | The detector hold is resumed like any other pause. | Anyone who can pair by Just Works can restart photic stimulation after a detected discharge. |
| **B. Interim: a detector hold is never resumed over BLE; the only second step is stop** | `np_runner_resume` refuses when the hold cause is `NP_HOLD_DETECTOR_DISCHARGE` or `_BLIND`. The refusal is seen only by an encrypted central, which can already read `SESSION_HOLD`, so it leaks nothing new. | It narrows `REQ-FWHUB-67`'s "stop, or resume" to "stop", which is a change to a ruling of the principal's (`OI-FWHUB-29`). A person halted by a false detection (the detector errs toward stopping) must start a new session. The apps' Resume button for holds 3 and 4 must go. |
| C. Only an authenticated link, when one exists | Resume of a detector hold requires an authenticated pairing or an app-layer credential. | Cannot be built until `OI-SIMFW-72` is decided. Until then it is A. |

### Q3 (Safety / Risk). Should `NP-RM-001` carry a hazard for stimulation restarted by an unauthorised central?

No row names it today. The controls would be `REQ-FWHUB-81` (e) and the safety MCU's own grant at resume (`SWR-SS-05`); the
residual is a central that pairs by Just Works (`OI-SIMFW-72`). Yes or no, and the severity Risk assigns.

## 4. Recommendation

- **Q1: A** (with `OI-PRIV-72`, already built, closing the refusal-code leak). The exception exists so that a failed pairing never refuses a stop. That reason is sound and is now true of
  every code left open. Encrypting pause alone (B) costs every central a wire change and gives no protection, because the
  stranger can stop the session.
- **Q2: B now, C when `OI-SIMFW-72` gives an authenticated link.** It is the reading of `REQ-FWHUB-67` that errs toward
  stopping, and while pairing is unauthenticated "any encrypted link" means anyone in range who pairs. The cost falls on a
  false detection, which the detector's design already accepts. Today B changes nothing a person sees: the detector is not
  armed.
- **Q3: yes.** The hazard existed and no record of it exists outside this item and the review.

## 5. What each ruling changes

Nothing below is done until the ruling is recorded.

| Ruling | Changes |
|--------|---------|
| Q1 A | `OI-SIMFW-73` and `UC-151` close with the restated reason; `REQ-FWHUB-81` (c) cites the ruling; `completed-decisions.md` entry. |
| Q1 B | A pause characteristic or code behind encryption; Compose, iOS and the twin change; `REQ-FWHUB-81` (c) narrows to stop and panic. |
| Q1 C | A design for a stop that never waits on a key, then a §23 change. |
| Q2 B | `np_runner_resume` (or `np_session_pause_on_write`) refuses a detector hold; a new part of `REQ-FWHUB-67` or `-72` with a host test; the apps drop Resume for holds 3 and 4 and say why; the locale strings change. |
| Q2 C | Waits on `OI-SIMFW-72`. |
| Q3 yes | A hazard row in `NP-RM-001`, with a revision fragment. |

## 6. What I need back

1. The option for Q1 and Q2, and a yes or no for Q3.
2. A short rationale for each (`CLAUDE.md` §18).
3. Any condition that would reopen it, for example an authenticated pairing method being chosen under `OI-SIMFW-72`.
4. Your name, role and the date.
