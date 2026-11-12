## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}, {NP-NPPS-REF-001 from → to}, {NP-HW-HEXTILE-001 from → to}, {NP-REG-UPG-001 from → to}: `OI-UINT16-01` built (the principal): the stimulation charge, VNS contact impedance, cervical impedance report and TMS pulse count are 32-bit, the safety MCU extended heartbeat grows from 38 to 42 bytes, and the TMS descriptor block from 10 to 12 bytes.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§7.1, §4.6: the extended heartbeat is 42 bytes (a 4-byte zero tail; the impedance report is 12 bytes and the cardiac, tier and withhold reports sit 4 bytes later), and the TMS block is 12 bytes; `charge_uC`, `impedance_ohm` and `pulse_count` are `uint32`, with no clamp (`OI-UINT16-01`, register row `UC-176`).**
## NP-NPPS-REF-001
author: NeurOne Software Engineering
text: **§9 TMS `pulse_count`: the descriptor field is 32 bits, so a pulse count up to 4294967295 compiles and a larger one is refused (`OI-UINT16-01`).**
## NP-HW-HEXTILE-001
author: NeurOne Hardware Engineering
text: **§Frame capacity: the active heartbeat is 42 bytes; the 4-byte tail added by `OI-UINT16-01` is a zero reply-window tail and not channel data, so the finding that the channel array has no spare bytes stands.**
## NP-REG-UPG-001
author: NeurOne Systems Engineering
text: **§7.8: the tier report is at window bytes 24 to 30 of a 42-byte reply since `OI-UINT16-01` (was 20 to 26 of 38).**
