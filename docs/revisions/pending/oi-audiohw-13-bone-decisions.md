## DHF
author: NeurOne Firmware Engineering
text: **{NP-HW-AUDIO-001 from → to}: `OI-AUDIOHW-13` parts 2 and 3 decided by the principal and recorded, nothing built: the bone level field is a `uint32` `bone_level_half_db` with `bone_conduct_en` removed, and a combined hearing-level ceiling applies when both audio paths play (new `REQ-AUDIO-21`(e)); the dB re 1 µN figure stays open for want of a source. {NP-FW-HUB-001 from → to}, {NP-NPPS-REF-001 from → to}, {NP-HW-HEXTILE-001 from → to}, {NP-REG-UPG-001 from → to}: `OI-UINT16-01` built (the principal): the stimulation charge, VNS contact impedance, cervical impedance report and TMS pulse count are 32-bit, the safety MCU extended heartbeat grows from 38 to 42 bytes, and the TMS descriptor block from 10 to 12 bytes. {NP-FW-HRV-001 from → to}, {NP-FW-EMMC-002 from → to}: every event counter widened to 32 bits with no saturation (`OI-UINT16-01`), the DFU download length and the OTA chunk index bounded. {NP-APP-ROADMAP-001 from → to}: the CONSUMABLE_STATUS characteristic is 16 bytes (4 x uint32).**
## NP-HW-AUDIO-001
author: NeurOne Systems Engineering
text: **§5, §5.2, §9: `REQ-AUDIO-21` gains part (e), the combined hearing-level ceiling, and `OI-AUDIOHW-13` records the decided wire field (`uint32`, flag removed, no protocol-version bump, authored key) and the combined rule; part 1 (the figure) and part 4 (hardware) remain open; the register row is `UC-175`.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§7.1, §4.6: the extended heartbeat is 42 bytes (a 4-byte zero tail; the impedance report is 12 bytes and the cardiac, tier and withhold reports sit 4 bytes later), and the TMS block is 12 bytes; `charge_uC`, `impedance_ohm` and `pulse_count` are `uint32`, with no clamp (`OI-UINT16-01`, register row `UC-176`); the SHDR `0x88` PBM counts record is 17 bytes and the HRV session record 35 bytes (counts widened, none saturates).**
## NP-NPPS-REF-001
author: NeurOne Software Engineering
text: **§9 TMS `pulse_count`: the descriptor field is 32 bits, so a pulse count up to 4294967295 compiles and a larger one is refused (`OI-UINT16-01`).**
## NP-HW-HEXTILE-001
author: NeurOne Hardware Engineering
text: **§Frame capacity: the active heartbeat is 42 bytes; the 4-byte tail added by `OI-UINT16-01` is a zero reply-window tail and not channel data, so the finding that the channel array has no spare bytes stands.**
## NP-REG-UPG-001
author: NeurOne Systems Engineering
text: **§7.8: the tier report is at window bytes 24 to 30 of a 42-byte reply since `OI-UINT16-01` (was 20 to 26 of 38).**
## NP-FW-HRV-001
author: NeurOne Firmware Engineering
text: **§8.1.1: `rr_sample_count` and `tavns_stim_count` are 32-bit, and `rr_sample_count` counts every accepted R-R interval and not the ring's occupancy; the UHDR session record is 35 bytes (`OI-UINT16-01`).**
## NP-FW-EMMC-002
author: NeurOne Firmware Engineering
text: **§G, §H: the accelerometer characterisation bins and `impact_event_count` are 32-bit and never saturate, so the sum always holds; the fleet columns are `BIGINT` (`OI-UINT16-01`).**
## NP-APP-ROADMAP-001
author: NeurOne Software Engineering
text: **§ GATT characteristics: CONSUMABLE_STATUS is 16 bytes, 4 x uint32 LE, widened from 4 x uint16 so no count is held at 65535 (`OI-UINT16-01`).**
