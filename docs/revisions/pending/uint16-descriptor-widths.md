## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}: the TMS, clinical tACS and audio-beat frequencies are 32 bits (`OI-UINT16-01`), so the three parameter blocks grow by two bytes and no frequency is cut off at 65.535 Hz.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§4.6: `np_mod_tms_params_t.freq_mhz`, `np_mod_clin_tacs_params_t.freq_mhz` and `np_mod_audio_params_t.beat_mhz` are 32 bits (the principal, 2026-11-20); the blocks are 14, 10 and 10 bytes and `NP_HUB_PROTO_VERSION` is not bumped (`REQ-FWHUB-10`). The validator's authorable ranges are unchanged.**
