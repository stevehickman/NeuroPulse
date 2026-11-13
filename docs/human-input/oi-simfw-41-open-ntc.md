# OI-SIMFW-41: must an open NTC be detected, and how? Two controlled documents disagree

**Status:** DRAFT, not sent. **From:** a Claude Code session working for the principal (2026-11-11). **To:** the safety
**GitHub issue:** [#826](https://github.com/stevehickman/NeuroPulse/issues/826)
MCU's owner (not yet named), with Risk. **Owning item:** `OI-SIMFW-41` (`docs/np_sim_fw_001.md` §23). Related:
`OI-SIMFW-24` (the table and the divider), `OI-FMEA-06`, GitHub #826, #467.

**This is a proposal, not a decision.** No firmware was changed for it.

## 1. The question

A thermal sense channel that reads full scale (4095 counts) is read as a cold sensor, so nothing is cut. **Two controlled
documents state opposite behaviours for that reading**, and the code follows the second:

| Source | What it says |
|---|---|
| `NP-FMEA-001` FMEA-M04-02 (S4 × P3 = 12, UNACCEPTABLE initial; residual S2 × P1, ACCEPTABLE) | "Open-circuit NTC returns ADC value above valid range (> maximum expected at ambient); out-of-range value treated as hardware fault, zone immediately disabled (conservative: no reading = worst case); ADC channel monitored for continuity at session start" |
| `NP-SW-001` §5.1.1 `REQ-SW01-05` | "An ADC failsafe value always cuts, and full scale does not." Pinned by `np_hal_platform_tests.c` (`test_adc_failsafe_value_actually_cuts`: "full-scale ADC does NOT cut") |
| `np_thermal_interlock.c` as built | No range check: `adc_to_celsius()` maps any count at or above the table's second entry to 1 °C. No continuity check exists, at session start or anywhere else |

So FMEA-M04-02's residual rating rests on a control the firmware does not contain, and a requirement and a passing Class C
test pin its absence. Which is right? Pick one:

| Option | What it means | Cost |
|---|---|---|
| **A. Detect in firmware, cut as worst case** | A count above a threshold `C_open` is treated as the hottest reading: the cranial lattice (for the hub channel, every line) is cut exactly as an over-temperature is, and it re-arms when the reading is back in range and at or below the re-arm temperature. FMEA-M04-02 becomes true | A Class C change with its own tests. `REQ-SW01-05`'s last clause and the `np_hal_platform_tests` assertion change (the table-orientation control must be kept another way, for example with a mid-scale cold count). The simulator's unset reading (4095) and the page's "cold" count must become an ambient count. `C_open` is a new constant (see §2). The hub cannot tell "open" from "hot" |
| **B. As A, with its own fault slot** | As A, and the reply's fault slot says "sensor open" instead of a domain index, so the app can say the sensor failed and not "too hot" | As A, plus a new `NP_FAULT_SLOT_*` value, the hub's fault mapping and an app message key in eleven locales (CLAUDE.md §17). Whether an open sensor latches until power-on (as `FAULT` does) or re-arms (as `THERMAL` does) must be chosen |
| **C. Detect in hardware** | The sense circuit is changed so an open sensor cannot read as cold (for example the NTC in the high leg, so open reads 0 counts, which already cuts) | A schematic change before layout. The table is regenerated in the other orientation (`OI-SIMFW-24` needs it regenerated or re-sourced anyway). `NP_HAL_ADC_FAILSAFE_COUNT` and its reasoning reverse, and a short then reads cold instead |
| **D. Do not detect** | FMEA-M04-02 is re-derived on the controls that exist (it names the on-module PTC or thermal fuse as backup), and the claimed control is withdrawn | A risk-file change only. Risk must accept the residual at S4 without the Class C control |

**Recommendation: B, with `C_open` set from the bench measurement `OI-SIMFW-24` asks for.** The FMEA already claims the
control and its residual depends on it. D withdraws a control from an S4 hazard. A alone tells the person "too hot" when
a sensor has failed. C is worth asking EE about while the schematic is open, but it moves the blind spot to a short.

## 2. What is known, and what is not

- **From the code (read 2026-11-11, not run on hardware):** the table descends (cold is a high count), so the ADC reads the
  voltage across the NTC and the NTC is the low leg. On the divider the header states (10 kohm NTC, B = 3950 K, 10 kohm
  series, 3.3 V) an open NTC reads full scale, and a shorted one reads 0, which cuts.
- **Run in the simulator against the real firmware (`OI-SIMFW-41`):** 4095 on a cranial channel, with a BES line granted,
  left status OK, no slot, and the line enabled.
- **Why no threshold was picked here.** The valid range is a number nowhere. The table's coldest entry is 3820 counts,
  labelled 0 °C. By my calculation on the stated divider, 0 °C is about 3156 counts and −40 °C about 3996, so the table
  and the divider already disagree (`OI-SIMFW-24`), and on the stated circuit the gap between the coldest credible
  reading and the rail is under 100 counts. A threshold in that gap needs the real divider, the ADC's noise and the
  leakage on the node. The table's own edge (3820) would cut a device at a temperature that is unknown until the bench
  says what the divider is. Either would be a constant with no derivation (CLAUDE.md §18, §19).
- **Not known:** whether the built circuit reads full scale on an open sensor; the lowest temperature the device must
  run at; whether all six channels are populated (`NP_NTC_CHANNEL_COUNT` is the retired five-zone count, `OI-FMEA-06`;
  an unpopulated channel would read open and, under A or B, cut for good).

## 3. What I need back

The option. For A or B: who derives `C_open` and from what, and whether an open sensor latches or re-arms. Whether
`REQ-SW01-05`'s "full scale does not" may change. A short reason, and your name, role and date. Until an answer is
recorded, the firmware, `REQ-SW01-05`, FMEA-M04-02 and the simulator stay as they are, and the simulator's scenario keeps
recording the behaviour as built.
