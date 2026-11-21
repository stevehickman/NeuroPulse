## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}: `OI-PRIV-21` (NP-PRIV-ANALYSIS-004 H1, GitHub #949) closed by `REQ-FWHUB-89`. The SHDR session-end record is the session count and firmware version only; duration, abort reason and the modality mask stay in UHDR. The per-tick SHDR EEG calibration (`0x85`) and zone-slot PBM health (`0x81`) records are retired. Rulings in `data-architecture-detail.md` §5.1; host tests in `np_log_backend_tests`.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§5.5, §6.1, §10.1: the SHDR session-end record (`0x80`) is the device session count and the firmware version, nothing else; tags `0x81` and `0x85` retired; new `REQ-FWHUB-89` (`OI-PRIV-21`, NP-PRIV-ANALYSIS-004).**
