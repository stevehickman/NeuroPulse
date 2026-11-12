# Principal question for review: OI-SIMFW-04, which clock the twin runs on

**Status:** ANSWERED 2026-11-11 (the principal). Build open; the sub-decisions the answer leaves open are drafted in `oi-simfw-04-clock-domains.md`. Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Software Engineering (`OI-SIMFW-04`, `NP-SIM-FW-001` §7, §30)
**To:** The principal
**Response needed:** an option for Q1 and an answer to Q2, with a short rationale, name, role and date.
**Why now:** the twin as built already runs on one kind of clock, and that was never recorded as a choice. `OI-SIMFW-50` (the hub's own five tasks under the FreeRTOS POSIX port) is where the choice starts to bind, and the principal approved that item first in the order of `NP-SIM-FW-001` §28. *(Written before the answer; see section 5.)*

## 1. What is known (measured or read on 2026-11-11)

- **The twin does not link the FreeRTOS kernel today.** `firmware/platform_sim/CMakeLists.txt` takes the kernel's headers only. The kernel calls the linked hub sources make (`vTaskDelay`, `xTaskGetTickCount`, the event group, the mutex) are a one-thread model in `firmware/platform_sim/src/np_sim_runner_seams.c`. No hub task runs.
- **The clock is real until something advances it, then virtual.** `np_sim_now_ms()` (`np_sim_bus.c`) returns the host's monotonic clock until the first `np_sim_advance_ms()`, and a counter after it. The module scenario suites call `np_sim_use_real_clock()`. Attaching the safety MCU makes the clock virtual (`np_sim_safety_attach()`).
- **With the safety MCU attached, the heartbeat and both watchdogs are on the same virtual clock.** The MCU's SysTick, its TIM2 and its independent watchdog all read `np_sim_now_ms()` (`np_sim_safety_hal.c`); the MCU's loop runs one iteration per virtual millisecond; the hub's heartbeat is played by the harness (`safety_beat`, `safety_run`: one heartbeat every 200 ms of that clock). A run is repeatable to the millisecond.
- **Nothing here is timing evidence.** `REQ-SIMFW-06` says so for either clock.
- **The POSIX port schedules on host signals and threads** (`NP-SIM-FW-001` §2). Under it a tick is a host timer, so a virtual clock means the twin, not the host, decides when the kernel's tick handler runs.

## 2. What is assumed

That `OI-SIMFW-50` will link the kernel and run the hub's tasks. If the answer to that item is "keep the harness as the caller", the as-built clock (option B below, without the kernel) simply stays, and this question reduces to recording it.

## 3. Questions

**Q1. Once the hub's tasks run in the twin, which clock do they run on?**

- **(A) Real time.** The POSIX port as vendored, ticking from the host timer. Costs: runs are not repeatable, a loaded CI runner can turn a 200 ms heartbeat into a watchdog cutoff that the firmware did not cause, and the safety MCU model (virtual today) must be moved to real time or the two clocks drift. Gains: no change to how the port schedules; task interleaving is whatever the host produces.
- **(B) Virtual time for everything.** The twin drives the kernel's tick from `np_sim_advance_ms()`, so the hub's tasks, the heartbeat and the safety MCU's watchdogs share one counter. Costs: a tick source of the twin's own in `firmware/platform_sim/` in place of the port's timer (the vendored kernel is SOUP and is not edited, so this is a shim around it and has to be shown not to change the port's scheduling decisions); interleavings a real host would produce by pre-emption at arbitrary instants are not explored. Gains: repeatable runs; a scenario can state "1.5 s without a heartbeat" exactly; what is built today for the safety MCU carries over unchanged.
- **(C) Both, selected per run.** Virtual for the scenario suites and CI, real for a person at the page. Costs: two modes to keep working, and a result seen in one may not reproduce in the other; the page must say which it is in. Gains: each use gets the clock that suits it.

**Q2. Must the hub's heartbeat and the safety MCU's watchdogs always be on the same clock?** Yes or no. Today they are. With "no", a real-time hub against a virtual-time MCU becomes possible, and a heartbeat-loss cutoff seen in the twin could be an artefact of the two clocks and not of the firmware.

## 4. What to send back

The option for Q1 and the answer to Q2, a sentence on why (`CLAUDE.md` §18 wants the reason recorded), anything that would reopen it, and name, role and date. The answer is recorded in `NP-SIM-FW-001` §7 (`OI-SIMFW-04`) and in `docs/status/completed-decisions.md`.

## 5. Answers

**The principal, 2026-11-11, in two parts, verbatim.**

1. "The time model (OI-SIMFW-04) should reflect the hardware as closely as possible."
2. After the lead proposed the model below: "Yes, record it that way for OI-SIMFW-04."

**The decision** is therefore the lead's proposal, which the principal agreed to. The reasoning behind the proposal is the lead's and not the principal's.

- **One virtual timeline:** a single deterministic scheduler advances virtual time.
- **Several clock domains derived from it, not one identical clock for every part.** From the hardware as the repository describes it:
  - the hub, on its own oscillator; it sends the 200 ms heartbeat;
  - the safety MCU's SYSCLK, which SysTick and TIM2 share, so their ratio is exact by construction (`firmware/safety_mcu/include/np_safety_config.h`, "Tick liveness");
  - the safety MCU's independent watchdog on the LSI oscillator, whose 29.5 to 34 kHz spread makes the nominal 1,000 ms about 941 to 1,085 ms (`np_safety_config.h`, "Hardware independent watchdog").
- **Each domain has its own rate offset, and the default is exact (offset zero).** With no scenario set, the behaviour equals the single virtual clock already built for the safety MCU.
- **Drift and the LSI spread are named scenarios** (for example SYSCLK slow or fast, the LSI at 29.5 kHz and at 34 kHz, the hub fast or slow against the safety MCU). The failure modes they let the simulator exercise are the one `NP-FMEA-001` FMEA-M02-02 says is not caught (SYSCLK itself slowing), the independent watchdog's timeout margin (`OI-SWCI-49`) and the SYSCLK drift item of GitHub #467.

**Against the options drafted in section 3.**

- **Q1:** nearest to **option B (virtual time for everything)**: one timeline, deterministic, no real-time mode. It goes beyond B in one respect. B put the hub's tasks, the heartbeat and the watchdogs on "one counter". The decision derives several domains from the one timeline, each with its own rate. Option C (a real-time mode beside the virtual one) is not part of it. How wall time relates to virtual time for a person at the page is left open (`oi-simfw-04-clock-domains.md`, Q4).
- **Q2 (must the heartbeat and the watchdogs share a clock): no.** They are on separate domains: the heartbeat on the hub's, the heartbeat watchdog on the safety MCU's SYSCLK, the independent watchdog on the LSI. With every offset at its default of zero they coincide, which is what is built today.

**Rationale.** The principal's stated reason is the first sentence: reflect the hardware as closely as possible. No other rationale and no condition that would reopen it was given.

**Recorded in:** `NP-SIM-FW-001` §5 (`REQ-SIMFW-22`) and §7 (`OI-SIMFW-04`: decided, build open), and `docs/status/completed-decisions.md` (2026-11-11). **Not built.** The twin still has one virtual counter that every part of the safety MCU model reads, and it runs no hub task.

**Left open by the answer** (drafted, not decided, in `oi-simfw-04-clock-domains.md`): how the hub's FreeRTOS tasks get virtual time; which offsets and spreads are the named scenarios and where their figures come from (none is invented: only the LSI's two ends are stated in a file); whether jitter is modelled; how wall time relates to virtual time for the page and for a bench run; what a timing result from the twin may be evidence for.
