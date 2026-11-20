## DHF
author: NeurOne Firmware Engineering
text: **CLAUDE.md Rev 84 → 85, {NP-FW-HUB-001 from → to}, {NP-NPPS-REF-001 from → to}, {NP-HW-HEXTILE-001 from → to}, {NP-REG-UPG-001 from → to}, {NP-HW-EEGNET-001 from → to}: the safety heartbeat is 66 bytes with 32-bit commanded currents and the VNS current is 32 bits; §18 adds the economic reason and ranks the reasons for a bound; the `UC-082` frequency ranges are defaults, not limits; the TMS, clinical tACS and audio-beat frequencies are 32 bits (`OI-UINT16-01`), so the three parameter blocks grow by two bytes and no frequency is cut off at 65.535 Hz.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§4.6, §7.1: `np_mod_tms_params_t.freq_mhz`, `np_mod_clin_tacs_params_t.freq_mhz` and `np_mod_audio_params_t.beat_mhz` are 32 bits (the principal, 2026-11-20), the blocks are 14, 10 and 10 bytes and `NP_HUB_PROTO_VERSION` is not bumped (`REQ-FWHUB-10`). The auricular VNS current is 32 bits (VNS block 14 bytes) and so is every `current_ua[]` of the safety heartbeat, which is 66 bytes (§7.1). The unsourced authoring ranges are defaults (`UC-082`).**
## NP-NPPS-REF-001
author: NeurOne Firmware Engineering
text: **§4.1, §4.2, §4.6, §4.7, §4.10, §4.11, §4.12, §14 `REQ-NPPS-23`, `REQ-NPPS-33`: the PBM, audio-beat, TMS and clinical tACS frequency ranges `UC-082` listed are defaults and no longer enforced (the principal, 2026-11-20); the breathing range is 4.5–7.0 breaths/min with its source; the descriptor widths and the audio tone bound are what still refuse a value.**
## NP-HW-HEXTILE-001
author: NeurOne Firmware Engineering
text: **§8.4.2 frame capacity: the safety heartbeat is 66 bytes, with a 32-bit `current_ua[]` and no zero tail (`OI-UINT16-01`, the principal, 2026-11-20); still no reserved byte.**
## NP-REG-UPG-001
author: NeurOne Firmware Engineering
text: **§7: the tier report is at window bytes 24 to 30 of a 66-byte reply window (was a 42-byte reply); the offsets are unchanged.**
## NP-HW-EEGNET-001
author: NeurOne Firmware Engineering
text: **Safety-MCU frame growth row: the extended heartbeat is 66 bytes (the 32-bit current array, `OI-UINT16-01`).**
