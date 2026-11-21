## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-CVNS-001 from → to}: the cervical VNS SHDR session summary is reshaped to three flags (impedance pass, cutoff, fault reason); REQ-CVNS-15 added (OI-PRIV-23 (NP-PRIV-ANALYSIS-004), GitHub #951).**
## NP-FW-CVNS-001
author: NeurOne Firmware Engineering
text: **§4.6: np_cvns_shdr_summary_t is three one-byte flags (impedance check passed, cutoff occurred, fault reason) and no longer lists electrode configuration, frequency, current, duration or per-electrode kΩ, which are UHDR (§8.2; data-architecture-detail.md §5.1). REQ-CVNS-15 added, verified by host tests and a _Static_assert. §14 OI-CVNS-04 re-worded to name the three-flag struct (OI-PRIV-23 (NP-PRIV-ANALYSIS-004), GitHub #951, H3). The write is still a stub; nothing had reached SHDR. IEC 62304 SW-02 Class B: np_cvns_types.h and np_cvns_session.c change; no Class C code, constant, limit or interlock behaviour changes.**
