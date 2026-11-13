# OI-SIMFW-24: bench measurement of the NTC sense circuit, to settle the thermal table

**Status:** DRAFT, not sent. **From:** a Claude Code session working for the principal (2026-11-11). **To:** the bench and
**GitHub issue:** [#826](https://github.com/stevehickman/NeuroPulse/issues/826)
first-article owner, with EE (hub schematic) and the safety MCU's owner. No addressee is named. **Owning item:**
`OI-SIMFW-24` (`docs/np_sim_fw_001.md` §22). Related: `OI-SIMFW-41`, `OI-FMEA-06`, GitHub #826, #467, #440.

**No firmware was changed.** Which of the table and the header is right is a bench fact, and nothing here assumes it.

## 1. What disagrees

`np_thermal_interlock.c` says its table is for a 10 kohm NTC (B = 3950 K, R25 = 10 kohm) over a 10 kohm divider at 3.3 V,
12 bits. Computed from that statement, against what the table holds:

| Temperature | Stated divider (counts) | Table (counts) |
|---|---|---|
| 25 °C | 2048 | 2527 |
| 62 °C (the cutoff, `NP_NTC_CUTOFF_DEG_C`) | 770 | 650 |

The code cuts when the count falls below the table's 61 °C entry (670). **By my calculation, not checked by a second
person,** 670 counts on the stated divider is about 67 °C, so on the stated circuit the cutoff would fire about 5 °C
late. The row says "about 4 C"; the direction is the same and the bench figure replaces both. The host test's own
constants (`ADC_HOT` 660, "62 °C") follow the table, so the tests agree with the table and cannot say which is right.
The simulator takes raw counts and asserts neither.

## 2. What the bench must measure

Per sense channel (six today: five cranial, one hub; `OI-FMEA-06` may change the count), on the built board or the
first article:

1. **The circuit as built:** which leg the NTC is in, the series resistor's value and tolerance, the reference voltage at
   the ADC, any filter capacitor or protection resistor on the node, and the NTC's part number with its R25, its B or
   resistance table, and its tolerance, from the datasheet.
2. **Counts against temperature:** the raw count `np_hal_adc_read_channel()` returns with the sensor held at a reference
   thermometer's 25, 40, 55 (the re-arm), 62 (the cutoff) and 70 °C, and at the lowest temperature the device must
   operate at. State the reference's uncertainty and the spread over repeated readings.
3. **The two fault readings** (for `OI-SIMFW-41`): the count with the sensor disconnected and with it shorted, and the
   spread of each.
4. **Sampling error:** the count through the production source impedance against the count through a low-impedance
   source at the same voltage (`np_hal_adc.c` notes that under-sampling reads low, which reads hot).

Not asked here: that 62 °C at the junction corresponds to 42 °C at the applied part. That is the thermal evidence of
`DI-SAFE-08` and `DI-SAFE-13` (#440), and the table cannot settle it.

## 3. The two acceptable resolutions (from the row; item 1 decides which)

| Finding | Resolution |
|---|---|
| **The built divider is the stated one** | The table is regenerated from it, with the generator and its inputs committed so the table has a derivation. The cutoff then fires earlier than today, and the host tests' counts are re-derived |
| **The built divider is another one, and the table matches it** | The header, `np_safety_hal.h` and `np_hal_adc.c` state the divider the table was made for, and the table's source is recorded |

If the measurement matches neither, the table is regenerated from the measured circuit. In every case the measured counts
at 55 and 62 °C become the table's acceptance check, and the tolerance stack (NTC, resistor, reference, ADC) is stated
in °C at the cutoff, because the margin to the 42 °C limit depends on it.

## 4. What I need back

The item 1 circuit description, the item 2 to 4 readings with the instrument used, which resolution applies, and your
name, role and date. Until then `OI-SIMFW-24` stays open and neither the table nor the header changes.
