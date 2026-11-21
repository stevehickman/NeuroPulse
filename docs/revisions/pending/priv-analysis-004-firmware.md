date: 2026-11-21
## DHF
author: NeurOne Quality
text: **NP-PRIV-ANALYSIS-004 Rev 1 added (privacy review of `firmware/`, 26 findings, 21 new ones filed as OI-PRIV-21 to OI-PRIV-41, GitHub #949–#969); {NP-FW-CVNS-001 from → to}, {NP-FW-HRV-001 from → to}: do-not-close notes on OI-CVNS-04 and OI-HRV-04.**
## NP-FW-CVNS-001
author: NeurOne Quality
text: **§14 OI-CVNS-04: note added that it must not be closed by wiring `np_cvns_shdr_summary_t` as it stands, because the struct carries raw per-electrode impedance and stimulation parameters, which §5.1 classes as UHDR (OI-PRIV-23, GitHub #951, NP-PRIV-ANALYSIS-004 H3). No requirement or design changed.**
## NP-FW-HRV-001
author: NeurOne Quality
text: **§11 OI-HRV-04: note added that it must not be closed as worded, because the coherence trend and the resonance breathing rate derive from HRV, which §5.1 classes as UHDR (OI-PRIV-30, GitHub #958, NP-PRIV-ANALYSIS-004 M3). No requirement or design changed.**
