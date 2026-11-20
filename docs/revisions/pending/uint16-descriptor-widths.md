## DHF
author: NeurOne Firmware Engineering
text: **CLAUDE.md Rev 83 → 84, {NP-FW-HUB-001 from → to}, {NP-NPPS-REF-001 from → to}: §18 adds the economic reason and ranks the reasons for a bound; the `UC-082` frequency ranges are defaults, not limits; the TMS, clinical tACS and audio-beat frequencies are 32 bits (`OI-UINT16-01`), so the three parameter blocks grow by two bytes and no frequency is cut off at 65.535 Hz.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§4.6: `np_mod_tms_params_t.freq_mhz`, `np_mod_clin_tacs_params_t.freq_mhz` and `np_mod_audio_params_t.beat_mhz` are 32 bits (the principal, 2026-11-20); the blocks are 14, 10 and 10 bytes and `NP_HUB_PROTO_VERSION` is not bumped (`REQ-FWHUB-10`). The validator's authorable ranges are unchanged.**
## NP-NPPS-REF-001
author: NeurOne Firmware Engineering
text: **§4.1, §4.2, §4.6, §4.7, §4.10, §4.11, §4.12, §14 `REQ-NPPS-23`, `REQ-NPPS-33`: the PBM, audio-beat, TMS and clinical tACS frequency ranges `UC-082` listed are defaults and no longer enforced (the principal, 2026-11-20); the breathing range is 4.5–7.0 breaths/min with its source; the descriptor widths and the audio tone bound are what still refuse a value.**
