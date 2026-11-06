## DHF
author: NeurOne Firmware Engineering
text: **NP-FW-HUB-001 Rev {NP-FW-HUB-001 from → to}, NP-SW-001 Rev {NP-SW-001 from → to}: the principal's rulings on the photoparoxysmal detector (option C), the panic control and the sensitivity setting are recorded as requirements; nothing is built.** `REQ-FWHUB-67` to `-71`, `SWR-SS-09`, `OI-FWHUB-31` and `OI-FWHUB-32`. No limit changed.
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **`REQ-FWHUB-67` to `-71` added (not built) and `OI-FWHUB-31`, `OI-FWHUB-32` raised.** The detection criterion (first generalised discharge; occipital-only spikes a clinician switch), fail-closed when the detector cannot see, validation tiers and the manual override, the sensitivity setting, and the audit of the pause control against a panic button (pause refused during ramp-up; tDCS ramps down on pause).
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3.6: `SWR-SS-09` added (not built): a modality-independent panic control, fixed in the lower left corner of every screen, that pauses at once and asks for a second step in a centred modal dialog.** No existing requirement text changed.
