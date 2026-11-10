# OI-SIMFW-27 — a proposed interruption policy: outage length, flapping, ramps, and what is logged and delivered

**Status:** PARTLY ANSWERED 2026-11-10 by the principal: **Q1 A, Q2 A, Q3 C, Q4 A, Q5 A** (recorded in §4 and in `docs/status/completed-decisions.md`); **Q6 to Q8 are open**. Not yet sent to the safety MCU's owner; the owner may still object to any answer. **From:** NeurOne Software Engineering (drafted with the principal's direction, 2026-11-09).
**To:** the safety MCU's owner (not yet named), with Risk and Clinical/HFE where marked. **Owning item:** `OI-SIMFW-27`
(`docs/np_sim_fw_001.md` §23); touches `OI-SIMFW-41` only where noted.

**This is a proposal, not a decision and not a design input.** Nothing in the repo may act as if it were accepted
(`docs/human-input/README.md` rule 4). The requirement IDs below (`PR-1` …) are placeholders; real IDs are assigned
when it is accepted and written to its owning file. Every number is a named parameter, because no number here has a
derivation (CLAUDE.md §18, §19): each parameter says what must be derived and who may set it.

## 1. What this is for

Today a heartbeat loss longer than the watchdog timeout cuts every line, and the first live beat after it grants them again.
Nothing latches, nothing is reported in the reply, and a ramp is not told about it (`OI-SIMFW-27`). The same shape recurs:
any uncommanded interruption (a withheld grant, a contact that chatters, a link that drops, a reset) either restarts
quietly or latches, depending on which module happened to be written first. The principal asked for one policy:

1. An outage shorter than a threshold may restart; longer latches. The threshold is configurable.
2. Repeated short interruptions ("flapping") latch by one common rule, the cardiac interlock's approach, for every cause.
3. A ramp interrupted in any way is handled by one rule.
4. Every interruption is logged. Only those that change what the person experiences are delivered to them.

## 2. Definitions (these terms are used in no other sense below)

| Term | Meaning | Where it is today |
|---|---|---|
| **Heartbeat period** | 200 ms: how often the hub sends a heartbeat (the "kick") | `NP_SAFETY_HEARTBEAT_MS`, `firmware/common/include/np_shared_constants.h` |
| **Watchdog timeout** | 1500 ms: the longest the MCU waits after the last *accepted* beat before it cuts. Industry usage calls this the watchdog period or timeout: the time allowed between services before the watchdog acts. **This repo's code and the header say "timeout"; "period" there means the heartbeat period.** This draft uses *timeout* only, and `REQ-SIMFW-16` and one simulator comment say "watchdog period" for the 1.5 s and are being corrected to match | `NP_SAFETY_WATCHDOG_MS`; `np_spi_watchdog.c` |
| **Independent watchdog (IWDG)** | A separate hardware timer, nominal 1000 ms, that resets the MCU if its own loop hangs. Not the heartbeat watchdog | `NP_SAFETY_IWDG_TIMEOUT_MS` |
| **Accepted beat** | A well-formed heartbeat that passed the sequence gate (the first two beats after a reset or a restarted run are not accepted, so up to 400 ms) | `np_spi_watchdog_seq_accept` |
| **Interruption** | Delivery on a line stops, or its level falls, for a reason the person did not command: a heartbeat outage, a withheld or withdrawn grant (contact, thermal, charge, cardiac), a link loss, a reset or brown-out. A pause, a stop and a protocol step are commanded and are *not* interruptions | new |
| **Outage length** | From the last accepted beat to the next accepted beat, **measured by the same check that detects the loss** (`np_spi_watchdog_check`), so detection and measurement cannot disagree. It includes the 1500 ms before the loss is even recognised and up to 400 ms after the link returns | new |
| **How a watchdog duration is measured** | Two usual ways. **(a) A hardware down-counter** on a clock independent of what it supervises: timeout = (reload + 1) × prescaler ÷ clock frequency, reloaded by each service (a kick). **(b) A software timestamp comparison**: the time of the last *accepted* service is stored, and every loop `elapsed = now − last` is computed with **unsigned subtraction** (safe across the millisecond counter's wrap, about 49 days) and compared with the timeout, **every loop and not only when a frame arrives**, so a silent link still expires. The heartbeat watchdog is (b); the IWDG is (a). Either way the measuring clock must not depend on what it supervises: the MCU cross-checks SysTick against the independent TIM2 timer and latches `NP_FAULT_SLOT_TICK` on a disagreement | `np_spi_watchdog.c` (`np_spi_watchdog_check`, `np_spi_watchdog_tick_liveness`); `NP_SAFETY_TICK_*` |
| **A duration is a range, not a point** | Each measured duration carries a **tolerance**: the oscillator's spread (the IWDG's 32 kHz oscillator is 29.5 to 34 kHz, so a nominal 1000 ms is about 941 to 1085 ms) and, for a loop-checked timeout, up to **one loop iteration** of detection delay, because the cut happens between the timeout and the timeout plus the iteration. This MCU's worst iteration includes a 40 ms flash page erase and an Ed25519 verify, and has not been measured on silicon (`OI-SWCI-49`). **`T_resume`, `T_lockout` and `W` below are stated with their tolerance, and a bound that must hold is checked at the worst end of that range** | `NP_SAFETY_IWDG_TIMEOUT_MS` comment |
| **Timeout against kick period** | A timeout should exceed the kick period plus jitter plus the worst loop time. Here 1500 ms against 200 ms is 7.5 times, so about seven consecutive beats can be missed before the cut | `NP_SAFETY_HEARTBEAT_MS`, `NP_SAFETY_WATCHDOG_MS` |
| **Flapping** | Interruptions of one line recurring faster than the person could treat them as separate events (§4) | new |

## 3. Proposed requirements

### PR-1 — an interruption is time-qualified

On any interruption of a line, the safety MCU holds the line off and measures the interruption's length with its own clock.
*(The clock is trusted because a frozen or slow SysTick already latches `NP_FAULT_SLOT_TICK`; a clock fault is a latched
fault first, so an outage can never be mistaken for a short one by a bad clock.)*

- Length **below `T_resume`**: the line may be granted again, subject to PR-3.
- Length **at or above `T_resume`**: the line latches, and is released only by the re-enable path of PR-2.

*What fails without it:* a long loss of the hub resumes stimulation with no acknowledgement and no record, and a short glitch
latches a session for no reason. *Traces to:* the principal (2026-11-09); the cardiac interlock's treatment of an event it cannot time (`np_cardiac_interlock.c`).

### PR-2 — a latch releases the way the cardiac interlock's does

Every latch this policy creates (a long outage, flapping) is released only by: **the lockout `T_lockout` elapsing, then the app's
confirmation, then a repeat of the contact check for the electrical lines, with the hub agreeing afresh.** The lockout restarts
at power-on, because there is no RTC and no battery, so the time since the cutoff is unknown and is treated as just happened.
The latch is persisted across a power cycle in the same non-volatile cutoff record the cardiac cutoff uses.
Existing latches (`FAULT`, `THERMAL`, `CHARGE`, `CARDIAC`, signature) are not changed by this policy.

*Fails without it:* two latches that release differently teach the person, and the next change, two rules.
*Traces to:* `REQ-CVNS-09` (30 s lockout, app confirmation, repeat impedance); `np_cardiac_interlock.c`.

### PR-3 — an interruption is counted, and a count in a window latches

One interruption log per line is the single source. **Every uncommanded interruption of the line is an entry, whatever its
cause** (outage, withdrawn grant, contact chatter, rejected-frame storms that dropped the line, reset, brown-out). When `N`
entries fall inside a window `W`, the line latches by PR-2. Counting all causes together is deliberate: the person feels one
stimulus stopping and starting, not a list of causes.

**"The cardiac approach", as the principal confirmed it (2026-11-09):** the cardiac interlock has no count-in-window; its first event latches.
So what is shared is the **latch and release path** (PR-2) and the **fail-closed handling of an unknown time**, not a counter.
This fixes how PR-2 and PR-3 are read; it does not answer Q4.
If the owner wants the cardiac interlock itself to count, that is a separate decision about a `REQ-CVNS-09` behaviour that was presented to FDA.

**Across a reset (option C of Q3, chosen by the principal 2026-11-09):** the window's start is not known across a reset (no RTC), and a window that restarted empty
on every boot would never trip on a reset loop (a brown-out loop, a loose connector that cuts power, a watchdog-reset loop), because each boot would start at a count of 1.
So in-run interruptions are counted in RAM, and **each unexpected reset is also counted once, at boot, from the reset cause, in a small persisted counter** (no flash write during a flap storm, and none while stimulating).
At boot the persisted count is added to the RAM count **as if no time had passed** (the fail-closed rule of the cardiac interlock). It is **cleared once the line has run for `W` without an interruption**, using uptime since boot, which is known, so a device left unplugged does not latch on a stale count.
The reset cause must be readable on the part; if it is not, this option falls back to counting every boot (a stricter reading). *(Not checked on the hardware.)*

*Fails without it:* a loose USB-C connector dropping out for 1 s ten times a minute never reaches `T_resume`, yet stimulation cycles on and off.

### PR-4 — one rule for a ramp interrupted by any cause

Today two modules ramp (tDCS, at the protocol's slope and at least 30 s; cervical VNS, 10 s up and 5 s down) and they are interrupted
in several ways: a pause (ramp down at the slope, then a resume that reverses it from where it got), a stop (tDCS ramps to zero over 30 s), an abort or panic (cuts at once),
a withheld or withdrawn grant, a heartbeat outage, a reset. The rule, for all of them:

> **The level a resume starts from is the level the delivery actually has at that instant. The ramp rises from there at the
> protocol's slope, never faster, never as a step. An uncommanded interruption is resumed through the same gate a person's resume uses (`np_ramp_plan`).**

So after a hard cut (outage, withdrawn grant, brown-out) the level is zero and a full ramp runs; after a pause that is reversed part-way, the level is the mid-ramp level, as today.
No path may resume at a level the person was last at while the hardware has been at zero.

- **The session clock stands still during an uncommanded interruption**, as it does for a pause (principal ruling 2026-10-13), so the dose delivered is the dose authored.
- **The hub's ramp model is told when the MCU withholds a grant.** *Not shown either way, to be checked:* from reading, the tDCS driver's ramp model (`np_mod_stim.c`) takes no input from the MCU's grant, and the cervical VNS driver holds at zero for a missing grant only while it is in RAMP_UP (`np_cvns_stim.c`). If so, after an MCU cut the driver may still command the pre-cut level and the first grant would step the current. A scenario that cuts a line mid-ramp and mid-steady-state, then re-grants, pins this.

*Fails without it:* a step from zero to the commanded current, which the 30 s tDCS ramp exists to prevent.
*Traces to:* the tDCS 30 s ramp (CLAUDE.md §3); `SWR-SS-05`; the pause ruling of 2026-10-13.

### PR-5 — every interruption is logged; only those that change behaviour are delivered

- **Logged (all):** every interruption, in the MCU's cutoff log, with the line, the cause class, the outage length, the ramp phase and level at the start, and the outcome (resumed, latched). Flapping entries are logged one by one.
- **Delivered to the user (only these):** (a) a latch, because stimulation stays off and they must act; (b) an interruption that dropped a line and restarted its ramp, as a non-blocking notice, because the person felt it. **Not delivered:** a loss shorter than the watchdog timeout (the line never dropped), and rejected frames the sequence gate absorbed.
- A latch is both logged and delivered. The reply to the hub carries a sticky "interruption since last read" flag and the last outage length, so a short interruption is visible to the hub even when it is not shown to the person. This closes the second half of `OI-SIMFW-27` (no reply reported the cut).
- **Data class:** an interruption is a *device condition* (SHDR) by the §5.1 test, and the cause field must be carried in the **same uniform shape for every cause**, so a cardiac cause is not revealed by a different record layout (§5.1 rule 2, `scripts/check-redaction-shape.ts`). The cardiac cause is carried as `np_fault_latch.c` carries it today. When in doubt, UHDR.

*Fails without it:* a log that omits flapping cannot show why a session latched; a notification for every absorbed glitch teaches the person to ignore the one that matters.

### PR-6 — the parameters and who may set them

| Parameter | Meaning | Constraint | Who may set it | Derivation needed |
|---|---|---|---|---|
| `T_resume` | longest outage that may restart | **at least the watchdog timeout (1500 ms) at its worst-case tolerance**, and at most a factory maximum `T_resume_max` | factory (OTP or signed record), then only **shorter**, never longer | the longest interruption a session tolerates; for tDCS and cervical VNS, from the clinical side |
| `T_lockout` | lockout before a latch can be released | at least the cardiac 30 s | as `T_resume` | follows the cardiac derivation, or its own |
| `N`, `W` | flapping count and window | `N` at least 2 | as `T_resume` | the rate at which a person stops treating interruptions as separate |

**Never settable by the app** (CLAUDE.md §4.2: no app-side path may bypass the MCU). The only direction any setting may move after manufacture is the safe one, the pattern of `REQ-CVNS-14` (the owner may make the interlock more sensitive, never less).
Each chosen number is a row in `docs/status/unjustified-choices.md` until derived (CLAUDE.md §19).

## 4. Questions for the reviewer (answer by picking an option)

| # | Question | Options and what each costs | Answer |
|---|---|---|---|
| Q1 | Should an outage be time-qualified at all (PR-1), or should any outage over the watchdog timeout latch? | **A** time-qualified (this draft). **B** latch on any outage: simplest, but a connector glitch ends the session. **C** keep today's no-latch and add only logging and reporting: no new latch, resumes with no acknowledgement | **A**, principal, 2026-11-10: time-qualified |
| Q2 | Is the flapping window counted across all causes (PR-3)? | **A** all causes together (draft). **B** per cause class: easier to tune, but a mix of causes never trips | **A**, principal, 2026-11-10: all causes together |
| Q3 | Does the count survive a reset? | **A** restarts empty, the reset itself is an entry: simplest, but a reset loop never trips. **B** the whole count persisted, as if no time passed: strictest, costs a flash write per interruption (up to a 40 ms erase in the loop) and can latch on a stale count. **C** in-run counts in RAM, each unexpected reset counted once at boot in a small persisted counter, cleared after a quiet run of `W` (**chosen by the principal 2026-11-09, recorded here; the owner may still object**) | **C**, principal, 2026-11-10 (also 2026-11-09) |
| Q4 | Does the cardiac interlock itself get a count (PR-3's note)? | **A** no, share only the latch and release path (draft). **B** yes: a change to a behaviour presented to FDA | **A**, principal, 2026-11-10: the cardiac interlock gets no count; `REQ-CVNS-09` is unchanged |
| Q5 | Is a ramp resumed from the actual level for every cause, with a full ramp after a hard cut (PR-4)? | **A** yes (draft). **B** resume mid-ramp after a short cut too: gentler, but it steps current if the hardware was at zero | **A**, principal, 2026-11-10: resume from the actual level, a full ramp after a hard cut |
| Q6 | Where is the ramp's authority? | **A** the hub owns the ramp and is told of every grant change (draft; smallest change). **B** the MCU also enforces a maximum slope after any re-grant, independent of the hub: strongest, new MCU code and a new hazard control | open |
| Q7 | Is an interruption that dropped a line and restarted a ramp delivered as a notice (PR-5)? | **A** yes, non-blocking (draft). **B** logged only until it latches: quieter, but the person feels a restart with no explanation | open |
| Q8 | Where does the persistent latch live? | **A** the cardiac cutoff's non-volatile record (draft). **B** a new record: separate, but a second compaction path (`OI-SIMFW-35` is the worked case) | open |

## 5. What is known, what is assumed

**Known (from the code, not run):** the heartbeat period, watchdog timeout and IWDG timeout above; that the watchdog tick clears `WATCHDOG` and `CUTOFF` and re-grants on the first live beat; that the cardiac interlock latches on its first event, has a 30 s lockout, restarts it at power-on, and releases only after app confirmation and a repeat impedance check; the ramp modules named in PR-4.
**Known (run in the simulator against the real firmware, 2026-11-09):** a heartbeat loss drops the line and the first beat restores it, with no flag in the reply.
**Assumed:** that the tDCS and cervical VNS drivers can step current after an MCU re-grant (PR-4's check), and that no other module ramps. Neither is established.

## 6. What to send back

For each of Q1 to Q8: the option chosen, a short rationale (CLAUDE.md §18 wants a derivation, not a restatement), the derivation or the open item that will supply each parameter in PR-6, any condition that would reopen the ruling, and your name, role and date.

## 7. What follows once it is answered

The owning files take the requirements (the safety MCU's firmware document and `NP-FMEA-001`; `NP-RISK-*` carries the hazard rows); `docs/status/completed-decisions.md` logs the ruling; the firmware changes in `np_spi_watchdog.c`, `np_safety_main.c` (the reply's spare bytes), `np_safety_config.h`, the two ramp drivers and `np_session_runner.c`; a scenario in `np_sim_safety_tests.c` pins each ruling. **The simulator then follows the firmware** (`REQ-SIMFW-16` (b)), and its "Heartbeat lost" button gains a duration choice so both sides of `T_resume` can be run. Until then nothing in the simulator or the firmware changes.
