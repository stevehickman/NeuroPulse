## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HEXTILE-001 from → to}, {NP-FW-PBM1064-001 from → to}, {NP-SES-1064-001 from → to}, {NP-FW-HUB-001 from → to}, {NP-NPPS-REF-001 from → to}: the PBM tile takes a pulse frequency in millihertz instead of one of six codes (`OI-PBMTILE-01`, the principal, 2026-11-21); the hub reads the register back and fails the command when the tile refuses it; the transcranial descriptor block and the session descriptor preset carry a 32-bit frequency.**
## NP-FW-HEXTILE-001
author: NeurOne Firmware Engineering
text: **§5.2, §5.2a, `REQ-FWTILE-15`: `PWM_FREQ_A/B/C` (`0x05`–`0x07`) are retired and reserved; `FREQ_A/B/C` at `0x12`, `0x16`, `0x1A` hold a 32-bit millihertz frequency (0 = CW, reset 40000). A frequency the tile cannot realise is refused and the previous value kept; the range and resolution are open (`OI-PBMTILE-03`).**
## NP-FW-PBM1064-001
author: NeurOne Firmware Engineering
text: **§5.1, §5.2, §5.3, §7.3: the register map follows the tile (`FREQ_A/B/C`); the six-code table is retired; startup and `np_pbm_drive_set_freq()` write the frequency and read it back (`NP_PBM_ERR_FREQ_REFUSED`); the EEG-adaptive frequency is the dominant frequency in millihertz.**
## NP-SES-1064-001
author: NeurOne Firmware Engineering
text: **§1, §3: the v5 preset carries `freq_mhz` (32 bits; preset 9 bytes, group 25 bytes, wire length 97 for one group); §3 sets the EEG-adaptive frequency to the dominant frequency itself, with no band and no code table; the 3-tick hysteresis is unsourced (`UC-198`).**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§4.6: `np_mod_pbm_base_params_t` is 7 bytes and `np_mod_pbm_smart_params_t` 9 (`freq_mhz` replaces the one-byte `freq_code`); `NP_HUB_PROTO_VERSION` is not bumped (`REQ-FWHUB-10`). The PBM HAL seams take the 32-bit frequency.**
## NP-NPPS-REF-001
author: NeurOne Firmware Engineering
text: **§4.1 frequency row, `REQ-NPPS-33`: a transcranial PBM frequency is 32-bit millihertz (0.75 Hz is 750 mHz); the whole-hertz refusal stays for the intranasal probe and the 1170 nm block; the editor step is 0.5.**
