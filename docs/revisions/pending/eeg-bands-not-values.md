## DHF
author: NeurOne Firmware Engineering
text: **{NP-SES-1064-001 from → to}, {NP-FW-PBM1064-001 from → to}, {NP-FW-HUB-001 from → to}, {NP-HW-AUDIO-001 from → to}, {NP-SIM-FW-001 from → to}, {NP-SW-001 from → to}: EEG bands are labels for ranges and never preset values (the principal, 2026-11-20): the PBM adaptive frequency is the tile code nearest the dominant frequency, and the audio beat follows the dominant frequency itself.**
## NP-SES-1064-001
author: NeurOne Firmware Engineering
text: **§3: the EEG-adaptive table of five bands is replaced by the tile's supported frequencies and a nearest-in-hertz rule (a tie takes the lower); the hysteresis counts ticks the nearest code differed. No EEG band picks a value.**
## NP-FW-PBM1064-001
author: NeurOne Firmware Engineering
text: **§5.2, §7.3: the codes are the frequencies the tile accepts, not EEG bands; the preset and EEG-adaptive mappings take the nearest code by distance in hertz; §5.2 points at `OI-PBMTILE-01` (the hub's direct path writes the authored whole-hertz byte as a code) and `OI-PBMTILE-02` (whether the tile should take a frequency).**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§8.5: `np_mod_audio_set_eeg_band()` and its band-to-beat table are replaced by `np_mod_audio_set_eeg_dominant_freq()`, which sets the beat to the frequency itself and refuses 0 or a carrier plus beat above the tone bound.**
## NP-HW-AUDIO-001
author: NeurOne Firmware Engineering
text: **§8.2 table and `REQ-AUDIO-18`: the adaptive beat is the dominant EEG frequency, not a band's preset; the requirement names `np_mod_audio_set_eeg_dominant_freq()`.**
## NP-SIM-FW-001
author: NeurOne Firmware Engineering
text: **The audio harness and bridge command is `audio_eeg_freq` (millihertz), replacing `audio_eeg_band`.**
## NP-SW-001
author: NeurOne Software Engineering
text: **§5.2.2: `REQ-SW02-11` states that an EEG-adaptive PBM frequency is the tile code nearest the dominant frequency and that no EEG band picks it.**
