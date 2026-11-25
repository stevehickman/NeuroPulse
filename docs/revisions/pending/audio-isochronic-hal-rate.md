date: 2026-11-25
## DHF
author: NeurOne Firmware Engineering
text: **{NP-HW-AUDIO-001 from → to}: the hub passes the isochronic pulse rate to the audio HAL, which it had dropped (`REQ-AUDIO-28`, `OI-AUDIOHW-17`).**
## NP-HW-AUDIO-001
author: NeurOne Firmware Engineering
text: **§5.3 requirements table: new `REQ-AUDIO-28` (an isochronic command reaches the output stage with its carrier and its pulse rate). §9: new `OI-AUDIOHW-17`, fixed in the same change.**
## LOG
text: **{date} — `OI-AUDIOHW-17` opened and fixed: the hub dropped the isochronic pulse rate (the principal asked for the change).** Found while closing `OI-NPPS-AUDIO-01`: `np_mod_audio.c` called `np_mod_audio_hal_set_isochronic(p->carrier_hz, p->level_half_db)`, so `beat_mhz`, which holds the rate, never reached the HAL, and the one-frequency signature never said which frequency it meant (undefined since the simulator commit of 2026-11-09). The call is now `np_mod_audio_hal_set_isochronic(carrier_hz, rate_mhz, level_half_db)` (`REQ-AUDIO-28`), changed in the HAL header, the trapping stub, the simulator model (which now records the rate in `audio_beat_mhz`) and both test doubles, with a hub host test that the carrier and the rate arrive and a simulator assertion on both. Still owed: the real codec path, which must pulse the carrier at the rate (`OI-AUDIOHW-14`).
