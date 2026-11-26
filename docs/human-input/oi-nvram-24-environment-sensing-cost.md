# OI-NVRAM-24: should the helmet get a working-environment sensor, and which one?

**Status:** DRAFT, ready to send.
**GitHub issue:** [#1086](https://github.com/stevehickman/NeuroPulse/issues/1086)
**Sender:** a Claude Code session working issue #1086 for the principal, drafted 2026-11-26.
**Addressee:** the Principal, with EE (hub schematic), Firmware and the Privacy Lead.
**Owning open item:** `OI-NVRAM-24` (`docs/status/pending-decisions.md`; `NP-FW-NVRAM-001` §8.5, §13). Related: `OI-ENV-05` (`NP-ENV-001` §6), `OI-POE-06`, `OI-OPR-06`, `OI-SIMFW-24`, `OI-HTOOL-01`; register rows UC-215, UC-216, UC-217, UC-029.
**Not a controlled document and not a design input** (`docs/human-input/README.md`): no serial, not in `NP-DHF-001`, and it writes no requirement (`CLAUDE.md` §18). It changes no firmware, no SHDR schema and nothing in `OI-NVRAM-24`'s record design, which another session owns.

**How to read it.** **[E]** is established, with the file that shows it. **[P]** is proposed here. **[U]** is an unverified figure: no distributor listing or datasheet could be fetched from this session (`digikey.com` and `sensirion.com` did not resolve), so every price and part figure below is unverified.

---

## 1. What I need from you

The ruling of 2026-11-26 (`docs/human-input/oi-nvram-22-dose-accum-wavelengths.md` §7) keeps the helmet's working temperature as time in 5 °F bands, and wants humidity if it can be sensed. This brief costs the ways of sensing both. Please answer two questions.

- **Q1. Which sensing goes in?** One of options A, B or C in §4. D is costed only to show why it is ruled out.
- **Q2. Does the SHDR sensor also serve the ambient safety gate (`OI-ENV-05`)?** Yes puts it on the safety MCU (Class C). No keeps it on the main processor (Class B), and `OI-ENV-05` is decided on its own.

Please send back the option for each question, a line of rationale, what would make you reopen it, and your name, role and date.

## 2. What the existing sensors can and cannot tell us [E]

| Sensor | Where it is, and what reads it | Range and resolution | Accuracy | Ambient working temperature? |
|---|---|---|---|---|
| **Tile NTC** (one per tile, `RT1`, 10 kΩ, B25/85 3435) | On the tile, on the scalp side (Path B1; `NP-HW-HEXTILE-001` §6.2, R-9). Read through socket pin 15 by the cluster controller's NTC mux (`NP-HW-HUB-001` §8.1; D-4 not adopted). The main processor gets a per-socket reading and session peak (`np_pbm_socket_telem.h`, F7) | Not stated for the tile path | Not stated. The NTC's tolerance and the cluster ADC's error are not in the record | **No.** In a session it reads emitter heating plus scalp temperature. It is UHDR (F7: scalp-contact temperature that marks a lit socket). Off the head, the idle pass keeps a pass or fail verdict and throws the reading away (`docs/reference/data-architecture-detail.md` §5.1, the maintenance rows). A configuration with no tiles has none |
| **Cranial thermal sense domains** (five) | On the shell, wired to the safety MCU's ADC1 (`np_safety_config.h`, `NP_NTC0`–`NP_NTC4`) | Table covers 0 to 110 °C in whole degrees (`np_thermal_interlock.c`). Nothing below 0 °C can be shown, although the survival envelope reaches −20 °C (`NP-ENV-001` §2.1) | **Not known, and probably wrong at room temperature.** The table disagrees with the circuit its own comment describes (`OI-SIMFW-24`). By my arithmetic, not checked by a second person: on the stated 10 kΩ / 10 kΩ divider, 25 °C gives 2048 counts, and `adc_to_celsius()` turns 2048 into 32 °C. That is about 7 °C (13 °F) high, more than two 5 °F bands | **No.** They sense the shell where the tiles heat it. They are Class C and report only a thermal flag over SPI (`NP_SAFETY_STATUS_THERMAL`), not a temperature. The count may change (`OI-FMEA-06`) |
| **Hub NTC** (one) | Inside the hub enclosure, used for supercapacitor ageing (`docs/reference/hardware-detail.md` §4.5). Wired to the safety MCU's ADC1 (`NP_NTC5`, channel 16), so it is Class C. The hub is outside the shielded shell, at the occipital arch, with a fan-cooled heatsink (`NP-TOOL-HUB-001` §2, a working assumption until `OI-HTOOL-01`) | Same table and limits as above | Same doubt (`OI-SIMFW-24`). `docs/reference/durability-maintenance.md` §7.3 would cross-calibrate the head NTCs against it and flag a ±1.5 °C offset. That figure has no source (UC-216) | **Partly.** At power-up, before self-heating, it reads close to ambient. In use it reads the hub's own heat. `NP-THERM-COOL-001` §7.5.4 treats it as a proxy only, with a settling time of at least 5τ_hub, and τ_hub has not been measured. That makes it a fair measure of the hub's own wear (supercapacitor ageing depends on temperature), but a poor measure of the room |
| **Ambient sensor** | **None.** `NP-ENV-001` §5 leaves it open: reuse NTCs at session start, or add a dedicated ambient NTC (`OI-ENV-05`). The safety MCU's config has no ambient channel (`OI-OPR-06`) | — | — | — |
| **Humidity sensor** | **None.** `NP-ENV-001` §5 recommends against one. The humidity effects it cares about are either sensed already (hydrogel dry-out shows as electrode impedance) or transient (condensation is handled by a warm-up hold) | — | — | — |

**Two limits that apply to every option [E].**

- **Only powered time is seen.** The hub keeps no backup clock (`CLAUDE.md` §2.3, §4.5) and Map 3 has no clock (D-18). So time in a band counts only time with the hub powered. Storage and transport conditions, which also cause wear, are not captured by any option here.
- **Getting a reading into SHDR from a Class C sensor changes Class C code.** SHDR is written on the main processor (Class B). Any sensor on the safety MCU's ADC needs a new field in the SPI frame, and that goes through the Class C change control.

**Answer to "is a dedicated ambient sensor needed?" [P].** For a working-environment record, yes. The tile NTCs are UHDR and read heat. The cranial domains read heat and report no value. The hub NTC is a fair record of the hub's own heat, but not of the room, and its calibration is in doubt until `OI-SIMFW-24` is benched.

## 3. Where a sensor can go: shielding, enclosure and classification [E]

- **Not inside the headset shell.** A humidity sensor needs air from outside. Inside the shell that means a hole in the shielding stack, and the stack is limited by its holes and seams, not by its number of layers (`NP-BIB-EMF-001` §7.6; `RISK-20`, `EMF-3`). A digital sensor and its bus inside the envelope would also be a new in-envelope source, and `NP-EMC-CAV-001` says those must be dealt with at the source, because the wearer's head is the only damping left. Inside the shell a sensor would also see head heat and sweat. Under `CLAUDE.md` §5.1 rule 1 that makes its readings UHDR, so they could not go to SHDR.
- **The hub is the natural place.** It is outside the shell, it already has an air path (the fan, the louvre slot and the 30 µm foam filter: `NP-TOOL-HUB-001` F-04, UC-031; `docs/reference/durability-maintenance.md` §7.2), and it already carries the antennas and the hub NTC. A sensor at the air inlet, before the heatsink and away from the supercapacitor, reads the room much better than the hub NTC does.
- **The hub is still near the body.** It sits at the occipital arch, so warm, moist air from the neck and hair could reach an inlet that faces the head. Until a measurement shows the readings carry no user biology, `CLAUDE.md` §5.1 rule 1 makes them UHDR, and they stay on the device. **[P] The demonstration:** at a fixed, controlled room temperature and humidity, compare the readings with the helmet worn and not worn, over the session lengths in use. The Privacy Lead sets the pass criterion. If being worn moves the readings, the band totals reveal when and how long the helmet was worn, and the sensor needs a different inlet or a different place.
- **Humidity and heat interact [E, physics].** Relative humidity depends on the temperature where it is measured. Air warmed by the hub reads drier than the room. Placing the sensor upstream of the heat helps. Recording dew point, or absolute humidity worked out from the two readings, would not depend on that heating. Which quantity to record, and its bands, is `OI-NVRAM-24`'s decision. It is not made here.

## 4. Options and their costs

**How to read the cost column (`docs/np_cost_001.md`).** Every cost in the document set is a **floor** (`CLAUDE.md` §2.1; UC-029). A part here is a per-unit **BOM** delta, and it lands on every configuration of both tiers, because they share one hub. It is not COGS: each configuration turns BOM into COGS with its own multiplier (`NP-COST-001` §2, A-3; `OI-COST-05`), and none is applied here. No GM% or retail figure follows from this brief, and none may be set before `OI-HEXTILE-06` (`OI-COST-10`). The only unit prices in the record are internal and uncited (UC-029): `NP-ENV-001` §5 gives "~$0.05–0.20" for a dedicated ambient NTC and "+$1–3" for a humidity sensor, and `docs/reference/durability-maintenance.md` §7.2 gives "+$0.15" for the hub NTC. **They are quoted as the record states them and are unverified [U].** A distributor listing at production quantity, or a supplier quote, for the part EE selects would verify them.

| | **A. Use the hub NTC** | **B. Add a dedicated ambient NTC in the hub** | **C. Add a digital temperature and humidity sensor in the hub** | **D. Same as C, in the headset shell** |
|---|---|---|---|---|
| **What it measures** | Hub internal temperature. Close to ambient only at power-up | Ambient temperature at the inlet | Ambient temperature and relative humidity at the inlet | Temperature and humidity inside the shell, with the head in it |
| **Humidity** | No | No | Yes | Yes, but it is the wearer's |
| **BOM delta** | None (the part exists) | One thermistor and one resistor. Record figure "~$0.05–0.20" [U] | One I²C sensor, decoupling, and possibly a protective membrane. Record figure "+$1–3" [U] | As C, plus sealing a hole through the shield |
| **Board area** | None | One small passive pair, plus thermal isolation from the hub's heat sources | One small package, plus thermal isolation and an air path to the inlet. The isolation and air path cost more area than the part does (a layout question for EE) | Not on the hub PCB. Needs a hole in the shell stack |
| **Power** | None new | Negligible: a divider read now and then | Small. The part sleeps between reads, and the average depends on how often `OI-NVRAM-24` reads it [U, no datasheet fetched] | As C |
| **Firmware** | A Class C change: a temperature field in the safety MCU's SPI frame, its hub-side reader, and fixing the table (`OI-SIMFW-24`) first | Class B: an ADC channel on the main processor, a conversion table built from the chosen part's datasheet, and the band totals (`OI-NVRAM-24`) | Class B: an I²C driver on the main processor (CRC check, error handling, heater policy if the part has one), and the band totals. A vendor driver would be SOUP and need a licence check (`CLAUDE.md` §22) | As C, plus EMC rework |
| **Safety MCU and Class B / C boundary** | Touches the safety MCU (Class C) | None, if the part is on the main processor. On the safety MCU instead (Q2 yes), it is a seventh ADC channel and a pin (`OI-HUB-C20`'s I/O count, `OI-FMEA-06`) plus A's SPI frame change | None, if on the main processor. The safety MCU firmware has no I²C driver today, so serving the safety gate from this part would need more Class C work than B does | As C |
| **Shielding and enclosure** | None | None. The hub is outside the shell | None to the shell. The hub needs an inlet that faces away from the head (`OI-HTOOL-01`) | **Ruled out:** a hole in an aperture-limited stack, and an in-envelope source (§3) |
| **Classification (`CLAUDE.md` §5.1)** | Needs the worn and not-worn demonstration (§3). Its session heat follows use | Needs the demonstration | Needs the demonstration, for humidity as well | **UHDR:** it sees head heat and sweat, so it can never be SHDR |
| **Fits the 5 °F bands?** | No, not until `OI-SIMFW-24` settles the table (it may read more than two bands high), and it reads hub heat | Yes, if the part's tolerance over the band range is finer than a band (EE to show from the datasheet) | Yes, as B | — |

**What does not change in any option [E].** The readings reach SHDR only as running totals, only from opted-in helmets within the characterisation window, until the `NP-MOD-ID-001` §7.4 review gate rules (`NP-FW-NVRAM-001` D-39). There is no per-session row, no ordinal and no clock (D-18). The record's band range, edges, read rate and where it lives stay `OI-NVRAM-24`'s, and the room in Map 3 stays `OI-NVRAM-23`'s.

## 5. Recommendation [P]

**Q1: option C.** It is the only option that gives humidity, which the principal wants, and the only one that gives a room temperature fit for 5 °F bands without touching the safety MCU. The sensor goes on the main processor (Class B), at the hub inlet, upstream of the heatsink. It goes in with three conditions:

1. **EE selects the part and verifies its price** against a distributor listing or quote, and shows from the datasheet that its temperature tolerance is finer than a 5 °F band across the band range. If the verified price is not acceptable, **fall back to B** for temperature and record humidity as not wanted (`NP-ENV-001` §5 stands).
2. **The §3 worn and not-worn demonstration is on file before any reading leaves the device.** Until then the readings are UHDR and stay on the device (`CLAUDE.md` §5.1 rule 1). Firmware can start counting before the demonstration. The counts are uploaded only once it passes, so the first dataset is not lost.
3. **The hub NTC keeps its job** (hub and supercapacitor heat), and stays a candidate for a hub-wear band record. It is not used as the room.

**Q2: no.** Keep the SHDR sensor on the Class B side, and let `OI-ENV-05` decide the safety gate's ambient input on its own. A second thermistor for the safety MCU, if `OI-ENV-05` wants one, is option B's cost again. That is cheaper and simpler than routing a digital part into Class C code, or making an SHDR record depend on the safety MCU.

## 6. What happens on each answer

- **C (with Q2 no):** EE adds the part to the hub schematic and BOM (`NP-HW-HUB-001`) and the inlet to the enclosure (`NP-TOOL-HUB-001`, `OI-HTOOL-01`). `OI-NVRAM-24` takes the sensor, the quantity (temperature, plus relative humidity, dew point or absolute humidity) and the bands. The Privacy Lead runs the §3 demonstration. `NP-ENV-001` §5's "no live RH sensor" is revised in its owning document, for the SHDR record only. The safety gate is unaffected.
- **B:** as C without the humidity part. Humidity is recorded in `OI-NVRAM-24` as not wanted after #1086.
- **A:** `OI-SIMFW-24` must be benched first. The safety MCU's owner takes the SPI frame change through Class C change control. The record measures hub heat, not the room, and should be labelled as such.
- **Q2 yes:** `OI-ENV-05` is closed on that part. The safety MCU gains the input (`OI-HUB-C20`, `OI-FMEA-06`), and `NP-FW-POE-001` §6 and `t_dwell` follow (`OI-POE-06`).

**What would reopen it:** a verified price far from the record's figures. A failed worn and not-worn demonstration at the hub. A hub placement other than the occipital arch outside the shell (`OI-HTOOL-01`). The first dataset's analysis, which may show that bands or humidity are not needed (UC-215).
