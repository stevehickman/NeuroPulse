# OI-NVRAM-24: which sensor feeds the working-environment record, and what counts as its time?

**Status:** DRAFT, ready to send. Recommends Q1 B, Q2 A, Q3 yes.
**GitHub issue:** [#1086](https://github.com/stevehickman/NeuroPulse/issues/1086) (the cost of temperature and humidity sensing, which Q1 B waits on). This brief does not do that study.
**Sender:** the session that worked on `OI-NVRAM-24` (a Claude agent), drafted 2026-11-26.
**Addressee:** the Principal, with the Privacy Lead (Q1's demonstration) and Firmware.
**Owning open item:** `OI-NVRAM-24` (`docs/status/pending-decisions.md`; `NP-FW-NVRAM-001` §13). Register rows UC-215, UC-216 and UC-217 (`docs/status/unjustified-choices.md`).
**Not a controlled document and not a design input** (`docs/human-input/README.md`): no serial, not in `NP-DHF-001`, and it writes no requirement (`CLAUDE.md` §18).

**How to read it.** **[E]** is established, with the file that shows it. **[P]** is proposed here. The record is
specified in `NP-FW-NVRAM-001` §8.5.1. This file asks only what that section cannot settle by itself.

---

## 1. What I need from you

| # | Question | Recommendation |
|---|----------|----------------|
| Q1 | Which sensor feeds the band totals? **A:** the hub NTC, which exists, as a record of the hub's own temperature. **B:** a dedicated ambient NTC on the hub, outside the heatsink path and away from the head, which does not exist yet (cost: #1086). **C:** both, as two records: the hub NTC now, the ambient NTC when it is fitted. | **B.** You asked for the working environment, and only B measures it. A measures how hot the device ran, which the per-channel on-time totals (`OI-NVRAM-23`) already describe |
| Q2 | What time do the bands count? **A:** every second the hub is powered. **B:** only seconds inside a session. | **A** |
| Q3 | Do you accept the band range in §4: 29 bands of 5 °F from −5 °F to 140 °F, one end band below and one above, and a separate count of seconds with no valid reading? | **Yes** |

What to send back: the option for each, a short reason (`CLAUDE.md` §18 wants one), anything that would reopen it, and
your name, role and date.

## 2. Why this is asked

You ruled on 2026-11-26 that the System Health Data Record (SHDR) records the helmet's working environment as time in
5 °F bands, at first (`docs/human-input/oi-nvram-22-dose-accum-wavelengths.md` §7, ruling 5). The record cannot be
built until a sensor is chosen, and the repository has no sensor that both measures the environment and may feed SHDR.

## 3. What is known

**The sensors that exist [E].** The safety MCU reads six NTC thermistors and nothing else
(`firmware/safety_mcu/include/np_safety_config.h`, `NP_NTC_CHANNEL_COUNT` 6). Each tile also carries its own NTC,
read by the hub through the cluster frame (`np_mod_pbm_hal_socket_ntc_read()`, `NP-HW-HUB-001` §9.2).

| Sensor | Where it is | What it reads | May it feed SHDR? |
|---|---|---|---|
| Tile NTC, one per tile | Against the scalp | Scalp-contact temperature while worn | **No.** Its per-socket reading in a session is ruled User Health Data Record (UHDR) content (F7, `docs/reference/data-architecture-detail.md` §5.1) |
| Cranial sense-domain NTCs, five | In the shell, over the cranial lattice | The shell region each one senses | **No**, for the same reason: they sense the lattice that sits on the head |
| Hub NTC, one (`NP_NTC5`, ADC1 channel 16) | On the hub, which is assumed to sit outside the shell at the occipital arch, beside the Boa dial (`NP-TOOL-HUB-001` §2; the mounting is not CAD-locked, `OI-HTOOL-01`) | The hub's temperature: room air, plus the heat the heatsink takes from the cranial lattice (`NP-THERM-CFD-R1-001` §5) | **Classed SHDR already** (`CLAUDE.md` §4.5; `docs/reference/hardware-detail.md` §4.5, "logged in SHDR"), but see below |
| Dedicated ambient NTC | **Does not exist.** `NP-ENV-001` §5 costs one at about $0.05 to $0.20; which source is "ambient" is `OI-ENV-05`, open | Room air, if it is placed off the thermal path | Yes, if it is placed away from the head |

**Why tile NTCs are out [E].** Body heat is biology. A total built from a scalp-coupled sensor would need
`CLAUDE.md` §5.1 rule 1's positive demonstration that it carries nothing about the wearer, and no such demonstration is
plausible: the reading is the person's skin temperature. Reading tile NTCs only while the helmet is off the head does
not help either. The record would then depend on whether the helmet is worn, and time worn is about the person. A
redaction conditioned on a sensitive predicate leaks it (`CLAUDE.md` §5.1 rule 2). The maintenance dock reads a tile
NTC with no head present, but only during a test the warranty owner runs, and that measures the dock, not the working
environment (`NP-FW-HUB-001` §6.9).

**Why the hub NTC is not a clean answer [E].**

1. **Only the safety MCU reads it.** The hub firmware, which owns Config and writes Map 3, has no reading of it. The
   safety MCU uses it for one thing: at 62 °C it cuts all stimulation (`np_thermal_interlock.c`,
   `NP_FAULT_SLOT_HUB_NTC`). No heartbeat reply or report window carries a temperature to the hub
   (`firmware/common/include/np_spi_wire_types.h`). Reading it for SHDR needs either a new report in the Class C
   firmware's reply window or a second read path on the hub. Either is a change to safety firmware or to the hub PCB
   for a record that is not a safety function.
2. **It reads the device's heat, not the room's.** The hub heatsink is where the cranial lattice's heat goes
   (`NP-TOOL-HUB-001` §2, point 3). During a session the hub NTC reads room air plus self-heating.
   `NP-THERM-COOL-001` §7.5.4 says the self-heat takes minutes to decay and the time constant is unmeasured. A band
   total from it says mostly how hard the device was driven, which the per-channel on-time totals already say.
3. **Its exposure to the head is not known.** The hub's mounting at the occipital arch is a working assumption
   (`OI-HTOOL-01`). Whether head heat reaches its NTC is a measurement nobody has made. The SHDR class it carries today
   was given for a supercapacitor ageing estimate. It was not given for a temperature series with a demonstration
   behind it. When in doubt, the reading is UHDR (`CLAUDE.md` §5.1 rule 1).
4. **Its conversion cannot read the cold half of the range.** `adc_to_celsius()` has a table from 0 to 110 °C. It
   returns whole degrees, and any reading colder than 1 °C comes back as 1 °C. Below about 34 °F every reading would
   land in one band. Whole degrees also put each 5 °F band edge up to 1 °C (1.8 °F) off its place. The table itself is
   approximate and waits on a bench check (`OI-SIMFW-24`).
5. **It may not survive.** The count of six NTC channels comes from the retired five-slot architecture and is to be
   derived again (`OI-FMEA-06`, `OI-HUB-C20`).

## 4. The band range, derived (Q3) [P]

This part does not depend on the sensor. It is written in full in `NP-FW-NVRAM-001` §8.5.1.

- **Range: the survival envelope.** `NP-ENV-001` §2.1 commits the helmet to survive −20 to +60 °C (−4 to 140 °F)
  without permanent degradation. A reading inside that envelope is in specification and a reading outside it is not.
  A wear record exists to tell those two apart. The operating limits sit inside the envelope (`NP-ENV-OPRANGE-001` §2,
  block above +35 °C), so the range covers them.
- **Edges on whole multiples of 5 °F.** 140 °F (+60 °C, the hot survival limit) and 95 °F (+35 °C, the ambient block)
  fall on band edges. −4 °F does not, because 144 °F is not a multiple of 5. So the lowest band starts at −5 °F, 0.56 °C
  past the envelope. That offset is smaller than the 1 °C step of the conversion as built, so no reading could tell the
  difference. An edge grid anchored at −4 °F would put neither 140 °F nor 95 °F on an edge.
- **Count.** (140 − (−5)) / 5 = **29 bands** inside the range, plus **2 end bands** that saturate (below −5 °F, and
  140 °F or above), so **31**. The record also keeps one count of seconds with no valid reading.
- **Why end bands saturate rather than drop the reading.** Time outside the envelope is the exposure the warranty
  owner and the fleet most need to see, and dropping it would hide it. Saturating ends also keep one invariant that a
  test can check: the bands and the no-reading count add up to all the time counted.
- **Why "no reading" is its own count.** A fault reading must not land in an end band. The ADC's fail-safe value reads
  as 110 °C (`np_hal_adc.c`, `NP_HAL_ADC_FAILSAFE_COUNT`), so a broken sensor would otherwise look like a hot hub.
- **Limits considered and not used.** 42 °C is a scalp limit, and the sensor is off the scalp. 62 °C is a junction
  cutoff, and it falls inside the top end band. Neither is a property of the environment.
- **Units.** The firmware works in Celsius. The reading is taken in tenths of a degree, as `NP-FW-POE-001`'s block
  already assumes (`OI-POE-06`), and converted to the band index in integer arithmetic.

## 5. Options and their costs

### Q1: the sensor

- **A, the hub NTC.** No new part. It needs a temperature report from the safety MCU or a second read path, so a Class C
  or PCB change. It needs the Privacy Lead's demonstration that a head does not reach it: a bench comparison, worn and
  not worn, with no stimulation, at room equilibrium, once the hub mounting is fixed. It needs the conversion table
  extended down to −21 °C at 0.1 °C. The result is the hub's temperature, not the room's, and the record should be
  named that way.
- **B, a dedicated ambient NTC.** It measures what you asked for. Its cost is #1086's to find. The same sensor would
  also close `OI-ENV-05` and fix `NP-FW-POE-001`'s dwell at 60 s (`NP-THERM-COOL-001` §7.5.4). It could be read by the
  hub on its own ADC, which keeps it out of the Class C firmware. The record waits for the part.
- **C, both.** A's costs now and B's later, plus two records to keep apart in the fleet schema.

### Q2: the time counted

- **A, powered time.** Wear on the electronics and the supercapacitor accrues whenever the hub is on, in a session or
  not. It needs no session state. Unpowered time cannot be counted by any choice: there is no clock and no backup
  battery (`CLAUDE.md` §4.5).
- **B, session time.** It counts only the time the helmet does therapy. It misses idle and charging time.

## 6. What is specified whichever way Q1 goes [P]

`NP-FW-NVRAM-001` §8.5.1 holds the rest:

- **One record per helmet**, because there is one sensor per helmet.
- **In Config, as its own replicated record of fixed length**, beside Map 3 and outside its row, which has no room
  (§3.3.1.6 there). The SHDR partition is not used because its writer only appends. Totals appended one after another
  would become a series of per-window values, which D-18 keeps out of the fleet.
- **The record is 132 bytes:** a 4-byte header and 32 counters of 32 bits. A later coarsening only lowers the band
  count, so the record never needs to grow.
- **A sample every second** into the band of the latest valid reading.
- **Written to Config every hour of powered time and at each session end.** A power cut loses at most that much
  (UC-217).
- **The fleet columns** are specified in the open item and not added to the schema. They reach the fleet only on
  D-39's terms.

## 7. What happens on each answer

- **Q1 B:** the record is built when the ambient NTC is on the hub PCB. `OI-NVRAM-24` stays open on #1086 and
  `OI-ENV-05`.
- **Q1 A:** `OI-NVRAM-24` gains three prerequisites. The Privacy Lead's bench demonstration comes first, then the safety
  MCU report (with the Safety MCU owner) and the extended table. The record is then built and named as the hub
  temperature.
- **Q1 C:** as A now, with a second record added under B later.
- **Q2 and Q3:** recorded in §8.5.1. UC-216 records Q2's answer either way, because neither choice is derived.

**Humidity** is not asked here. No sensor exists, and #1086 is costing one.
