# OI-FWHUB-28: does the app's stop or pause need a latency bound, and from what?

**Status:** DRAFT, not sent. **From:** a Claude Code session working for the principal. **To:** Risk, with Clinical/HFE and
**GitHub issue:** [#848](https://github.com/stevehickman/NeuroPulse/issues/848)
the safety MCU's owner. No addressee is named.

## 1. The question

The app's **Stop** (`SWR-SS-06`) and **Pause** (`SWR-SS-05`) withdraw every stimulation enable the hub requests. **No document states how
quickly.** A figure would be a requirement, and `CLAUDE.md` §18 does not allow one without a hazard it traces to. Is a bound
needed, and if so from which hazard? Pick one:

| Option | What it means | Cost |
|---|---|---|
| **A. No bound** | The timing in §2 is recorded as information. The accepted way to stop in a hurry stays the cable pull (`IFU-01`, the principal, 2026-10-28), whose timing is `OI-PWRSRC-11`'s bench measurement. The IFU says nothing numeric about the app. | None. |
| **B. A bound from a named hazard** | Risk names the hazard (for example "stimulation continuing after the person has pressed Stop") and the acceptable time, in the risk file. A requirement row then states it and a test pins it. | A risk-file entry and a requirement. If the figure is below about 200 ms, option C is forced. |
| **C. An out-of-cycle heartbeat on stop or pause** | The hub sends a heartbeat the moment it withdraws the enables, instead of waiting for the next one. | Changes the Class B heartbeat task (`NP_HUB_TASK_PRIO_HEARTBEAT` is "must never miss the 200 ms window") and touches the Class C sequence-counter logic (`OI-FMEA-12 (a)`: a repeated or out-of-sequence frame is not acted on). Needs a Class C review. Only worth it if B demands it. |

**Recommendation: A**, unless Risk names a hazard. The cable pull is the safety stop the IFU gives the person, and the app
stop is its convenience path; nothing found makes the app stop a hazard control.

## 2. What is known (read from the code at `main`, 2026-11-03)

After the app's stop is posted to the hub, the contributions to the time until the enable lines drop are:

| Link | Bound | Source |
|---|---|---|
| App → hub transport (a BLE write) | Not the hub's; depends on the link's connection interval | outside `firmware/hub_control` |
| The runner notices | **Within one scheduler tick, 1 ms.** The run loop waits on the event group, so a stop is acted on when posted. Before #755 this was up to 1 s. | `configTICK_RATE_HZ` 1000 (`FreeRTOSConfig.h`); `np_runner_run_tests` ("acted on at the moment it is posted") |
| The runner withdraws the request | Immediately: `np_safety_spi_disable_all()` clears the requested mask in RAM. | `np_safety_spi.c`; `np_safety_spi_tests` |
| The withdrawal reaches the safety MCU | **Up to the next heartbeat: at most `NP_SAFETY_HEARTBEAT_MS`, 200 ms.** The heartbeat task reads the mask each cycle. | `np_shared_constants.h`; `np_hub_control_main.c` |
| The safety MCU drops the line | On receiving a live frame it sets its requested mask and writes the GPIO later in the same loop iteration. **The iteration period is not measured here.** | `np_safety_main.c` |

So the hub's own contribution is **about 200 ms plus the MCU's iteration**, dominated by the heartbeat period, a locked figure
(`CLAUDE.md` §4.2). A pause is the same path. The modules' own ramp-downs (for example the cervical VNS's 5 s) do not delay the cut:
the enables are withdrawn first (`REQ-FWHUB-63`).

Not known: the transport time; the MCU's iteration period; any hazard that a bound would serve; how long the output takes to reach zero
after the enable drops (`OI-PWRSRC-11`).

## 3. What I need back

The option (A, B or C); if B, the hazard, the figure and where it is traced (a standard, a measurement or a derivation, per
§18); a short reason; and your name, role and date. If A, I record the §2 timing in `NP-FW-HUB-001` as information and close
`OI-FWHUB-28`. Nothing in this file is a decision: until an answer is recorded no document states a bound.
