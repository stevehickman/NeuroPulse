## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-EMMC-002 from → to}: §C.8.6 records that the per-person UHDR key store is built and host-tested, with REQ-EMMC2-02 to -05 (OI-PRIV-34, NP-PRIV-ANALYSIS-004).**
## NP-FW-EMMC-002
author: NeurOne Firmware Engineering
text: **New §C.8.6 and REQ-EMMC2-02 to -05: the first increment of §C.8, the per-person key store (np_uhdr_profile.c), is built and host-tested: one Config record per profile, a device-bound unlock wrap and a portable recovery wrap, a helmet-generated 128-bit recovery code handed out once, credential-free export with no unlock wrap, import into an empty slot as dormant, re-binding by recovery code, deletion, and the §C.7 attempt limit shared. Four new seams (SW-02 count 123 to 127). §C.8.5 records the profile cap (8, PROVISIONAL, UC-204) and the per-profile attempt-limit question. File sealing, the file export and the mount change are not built (OI-PRIV-34, NP-PRIV-ANALYSIS-004, #962).**
