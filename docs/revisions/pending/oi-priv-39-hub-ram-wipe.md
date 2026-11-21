date: 2026-11-21
## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}: no protocol, session record or biosignal stays in hub RAM once it is no longer needed (`REQ-FWHUB-84`, new §6.10; `OI-PRIV-39` (NP-PRIV-ANALYSIS-004), GitHub #967).**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **New §6.10 and `REQ-FWHUB-84` (a) to (f): the transport buffer, the runner's parsed protocol and records, the logger's UHDR buffer, adaptation ring and backend staging block, the detector's channel state, and the HRV and sLORETA scratch are zeroed by a non-elidable clear where each stops being needed; the pass at `np_uhdr_key_background()` is not built because nothing calls it (`OI-PRIV-39` (NP-PRIV-ANALYSIS-004)).**
