date: 2026-11-09
## DHF
author: NeurOne Firmware Engineering
text: **{NP-SIM-FW-001 from → to}, {NP-FW-HUB-001 from → to}, {NP-HW-AUDIO-001 from → to}: OI-SIMFW-19 closed. The audio driver retunes only a binaural session on an EEG band update and refuses a volume above 100.** `np_mod_audio_set_eeg_band` re-applied binaural whatever the running mode (an isochronic or noise session became binaural; an off command started a tone), and `volume_pct` reached the planar drivers and the bone transducer unchecked (found by the firmware-in-the-loop simulator). A band update now acts only on mode 0, as NP-FW-HUB-001 §8.5 says; a volume above 100, the specified range, is refused with the invalid-argument status before any state changes. Refused rather than clamped; this is not an acoustic ceiling (OI-AUDIOHW-01 stays open). Class B only; no Class C file touched. New host test `np_mod_audio_tests` verifies REQ-AUDIO-17 and -18 (37 checks fail without the fix, all pass with it); the simulator scenarios assert the fixed behaviour. Also in platform_sim only: the simulator did not build on main (np_safety_spi_send_hr_report gained an argument and np_cardiac_sens_level was unlinked after OI-CVNS sensitivity work); the harness passes 0 and a sim double returns 0.
## NP-SIM-FW-001
author: NeurOne Firmware Engineering
text: **§7, §16: OI-SIMFW-19 closed.** The two audio scenarios assert the fix. The simulator build repaired (safety harness argument; cardiac sensitivity double).
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§8.5: only a binaural session is retuned by an EEG band update; a volume above 100 is refused (OI-SIMFW-19).**
## NP-HW-AUDIO-001
author: NeurOne Firmware Engineering
text: **REQ-AUDIO-17 and REQ-AUDIO-18 added (OI-SIMFW-19).** Volume range 0-100 enforced by refusal; band update gated on binaural mode. No figure set.
