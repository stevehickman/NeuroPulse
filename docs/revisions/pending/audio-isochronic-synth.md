date: 2026-11-25
## DHF
author: NeurOne Firmware Engineering
text: **{NP-HW-AUDIO-001 from → to}: a hardware-independent isochronic synthesis (`np_audio_synth`, `REQ-AUDIO-29`) and the open item for the pulse shape and the codec (`OI-AUDIOHW-18`).**
## NP-HW-AUDIO-001
author: NeurOne Firmware Engineering
text: **§5.3 requirements table: new `REQ-AUDIO-29` (the isochronic synthesis renders a carrier gated at the pulse rate from stated figures only). §9: new `OI-AUDIOHW-18` (the pulse shape and the codec are undecided, so the synthesis is not yet wired into the driver).**
## LOG
text: **{date} — `OI-AUDIOHW-18` opened: the real codec path for pulsing the carrier is split into what can be built now and what the principal decides.** The principal asked for the real codec path and chose the hardware-independent core. Built: `firmware/hub_control/src/np_audio_synth.c` renders the carrier gated at the pulse rate, with a raised-cosine edge, and refuses a configuration it cannot render (`REQ-AUDIO-29`, host test `np_audio_synth_tests`). It has no default sample rate, duty cycle or edge ramp: none has a source (`OI-AUDIOHW-14` chooses no codec, and the only duty cycles in the documents are a mouse study's 4 % to 80 %, whose methods could not be read). Not built, and why: `np_mod_audio.c` does not call it, and the codec and SAI binding stays the trapping stub, until the principal states the duty cycle and the edge ramp with their basis and `OI-AUDIOHW-14` picks hardware. The raised-cosine curve is a choice with no derivation (`UC-212`).
