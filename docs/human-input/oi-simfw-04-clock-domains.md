# Principal question for review: OI-SIMFW-04 (a), what the clock-domain time model still needs decided

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#825](https://github.com/stevehickman/NeuroPulse/issues/825)
**From:** NeurOne Software Engineering (`OI-SIMFW-04`, `NP-SIM-FW-001` §7, `REQ-SIMFW-22`)
**To:** The principal (with the safety MCU's owner for Q2, Quality for Q5, and whoever owns bench and first-article test, not yet named, for Q4 and Q5)
**Response needed:** an option for each of Q1 to Q5, with a short rationale, name, role and date.
**Why now:** the principal decided on 2026-11-11 that the twin runs on one virtual timeline with several clock domains derived from it (`oi-simfw-04-time-model.md`, ANSWERED). It is not built. Five things are left open that the build cannot start without. Nothing in the repository acts as if these were answered.

## 1. What is known (each from a file read on 2026-11-11)

- **The decision:** one virtual timeline; three domains (the hub; the safety MCU's SYSCLK, which SysTick and TIM2 share; the safety MCU's independent watchdog on the LSI oscillator); a rate offset per domain, exact by default; drift and the LSI spread as named scenarios (`REQ-SIMFW-22`).
- **What is built:** one virtual millisecond counter. The safety MCU's SysTick, TIM2 and independent watchdog all read it (`firmware/platform_sim/src/np_sim_safety_hal.c`), and the heartbeat is played by the harness every 200 ms of it. The twin links no FreeRTOS kernel and runs no hub task (`firmware/platform_sim/CMakeLists.txt`).
- **SysTick and TIM2 share SYSCLK**, so their ratio is exact by construction, and a SYSCLK that slows is not caught by the tick check (`firmware/safety_mcu/include/np_safety_config.h`, "Tick liveness"; `NP-FMEA-001` FMEA-M02-02; GitHub #467, `OI-FMEA-13` (i)).
- **The safety MCU's SYSCLK is 64 MHz from the internal HSI16 through the PLL** (`firmware/safety_mcu/platform/np_hal_clock.c`).
- **The independent watchdog is on the LSI:** nominal 1,000 ms at 32 kHz, about 941 to 1,085 ms across a 29.5 to 34 kHz spread; the timeout itself is PROVISIONAL (`np_safety_config.h`, "Hardware independent watchdog"; `OI-SWCI-49`).
- **The FreeRTOS POSIX port takes its tick from a host timer and schedules with host threads and signals** (`NP-SIM-FW-001` §2). The vendored kernel is SOUP and is not edited.
- **After a real loss of the page's link, the page advances the virtual clock by the wall time lost**, capped at 60 s and ignored under 1.5 s (`NP-SIM-FW-001` §23, `UC-135`).
- **`REQ-SIMFW-06`:** no simulator output is verification evidence, and the simulator says its timing is indicative. **`REQ-SIMFW-10`:** a scenario run on a rig could be evidence, with the runner and models validated.

## 2. What is not known

- **The accuracy of the HSI16 that SYSCLK comes from.** No file read states a tolerance. The STM32G071 datasheet's HSI16 accuracy table (over temperature and supply, and after trimming) would give it, or a measurement on the board.
- **The hub's clock source and its tolerance.** No file read names the i.MX RT1062's oscillator or crystal for this board or a tolerance for it. The hub board's schematic and the part's datasheet would give it, or a measurement.
- **Where the 29.5 to 34 kHz comes from.** The header comment states it and names no source. The STM32G071 datasheet's LSI table is the likely one; it was not opened.
- **Whether the POSIX port can be driven from a tick it does not generate without changing its scheduling decisions.** Not tried.

## 3. Questions

**Q1. How do the hub's FreeRTOS tasks get virtual time?** (This binds only if `OI-SIMFW-50` puts the hub's own tasks in the twin.) **Status 2026-11-13:** `OI-SIMFW-45` and `OI-SIMFW-50` were started on the principal's approval of their order, and option A was built as a cooperative scheduler on host threads (`NP-SIM-FW-001` §31), with the reasons B and C were not taken written there. It stands until the principal answers; `OI-SIMFW-75` asks whether it does.

- **(A) Keep the one-thread kernel model the twin has today, and extend it.** The hub's task bodies are called by the twin in an order it chooses, on the hub's domain. Costs: it is a model of the kernel and not the kernel, so pre-emption, priorities and blocking are the twin's and not FreeRTOS's; what `OI-SIMFW-50` exists to show (the five tasks interacting) is shown only as far as the model is right. Gains: fully deterministic; nothing in the port is touched.
- **(B) Link the vendored kernel with the POSIX port, and replace only the port's tick source** with one the virtual timeline calls, from `firmware/platform_sim/`. Costs: the port expects its timer signal at host-time intervals; whether it schedules the same way when ticks arrive as fast as the twin issues them is unknown and has to be shown; host threads still run between ticks, so two runs can interleave differently inside one tick unless that is also controlled. Gains: the kernel's own scheduler decides.
- **(C) Write a small port of the kernel for the twin** (one host thread, tasks as coroutines, tick from the timeline). Costs: a new kernel port to write and to trust, which §2 of the specification called "a new kernel to qualify" when it rejected the WebAssembly route; larger than A or B. Gains: the kernel's own scheduler and full determinism.

**Q2. Which offsets and spreads are the named scenarios, and where do their figures come from?**

- **The independent watchdog (LSI):** the two ends, 29.5 kHz and 34 kHz, are in the header comment. **Confirm they may be used as scenario figures before their source is checked**, or require the datasheet to be read first.
- **SYSCLK slow and fast, and the hub fast and slow against the safety MCU:** no figure exists in any file read (section 2). Options: **(A)** the scenarios exist but take their offset as an input the person gives each time, with no default and no named value, until a datasheet figure or a measurement is recorded; **(B)** wait, and build these scenarios only when the figures are sourced; **(C)** the principal supplies figures now, each then entered in `docs/status/unjustified-choices.md` until sourced (`CLAUDE.md` §19). This draft proposes no number.
- Should a scenario exist for a **step** change of rate during a run (a clock that drifts part-way through a session), or only a constant offset for the whole run?

**Q3. Is jitter modelled at all?** A virtual clock has none.

- **(A) No jitter.** Each domain is exactly its rate. Costs: failures that need an edge to land early or late by a varying amount are not seen. Gains: deterministic; nothing to source.
- **(B) Jitter as a named scenario, from a seeded generator**, so a run repeats exactly. Costs: a distribution and its size have to come from somewhere, and none is in any file read; an invented one is an unjustified choice. Gains: a class of timing faults becomes visible.
- **(C) Not now; revisit when a measured figure exists.**

**Q4. How is wall time related to virtual time, for the page and for a bench run?**

- **(A) The page paces virtual time at one virtual second per wall second, and a scenario suite runs as fast as it can.** Costs: under load the page falls behind and must either skip or slow, and which it does has to be said. Gains: a person sees the helmet in something like real time.
- **(B) As A, with a speed control on the page** (pause, slower, faster). Costs: more to build and to label, since a person may forget the run is not at real speed. Gains: a 30 s ramp can be watched in 3 s.
- **(C) Virtual time only ever moves when something asks it to** (a command, a scenario step), as today. Costs: the page does not animate on its own. Gains: nothing to pace; fully repeatable.

Also: **the existing rule that a link loss is played back as lost heartbeat time** (`UC-135`) charges the wall time to the timeline. Under several domains, to which is it charged: the hub's domain only (the hub was silent), or the whole timeline? And **on a bench run with real hardware in the loop** (`OI-SIMFW-10`, unanswered), real parts keep real time, so virtual domains and real clocks meet: is that case in scope now, or left until a rig exists?

**Q5. What may a timing result from the twin be evidence for?**

- **(A) Nothing.** `REQ-SIMFW-06` as written; the domains make the twin more useful for finding faults, not for proving timing. Costs: a result such as "the watchdog still cuts with the LSI at 29.5 kHz" has to be shown again on silicon to count. Gains: no validation burden on the time model.
- **(B) Evidence of logic under stated clock ratios, not of timing.** A run could support a claim such as "with the watchdog's clock at this ratio to the hub's, the firmware's logic cuts", while saying nothing about the real durations. Costs: `REQ-SIMFW-06` would need rewording and the time model would need validating under the quality system; the line between logic and timing has to be drawn so it cannot be stretched. Gains: FMEA-M02-02's residual and `OI-SWCI-49`'s margin get an analysis tool whose output can be cited.
- **(C) As A for the simulator; decide for a bench run when a rig exists** (`REQ-SIMFW-10`).

## 4. What to send back

An option for each question, a sentence on why for each (`CLAUDE.md` §18 wants the reason recorded), anything that would reopen it, and name, role and date. The answers are recorded in `NP-SIM-FW-001` §7 (`OI-SIMFW-04`) and in `docs/status/completed-decisions.md`, and the requirement rows they imply are written then.
