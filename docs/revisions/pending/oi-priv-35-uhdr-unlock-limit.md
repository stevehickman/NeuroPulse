## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-EMMC-002 from → to}: the UHDR unlock gets an attempt limit (§C.7, `REQ-EMMC2-01`), and the 32-byte XTS key and the 64 MiB Argon2id memory are marked open for the cryptography reviewer (`OI-PRIV-35`, NP-PRIV-ANALYSIS-004 Findings M9 and M10, GitHub #963).**
## NP-FW-EMMC-002
author: NeurOne Firmware Engineering
text: **New §C.7 and `REQ-EMMC2-01`: every unlock attempt that reaches the KDF is charged to a failed-attempt count persisted in Config before the credential is tested, a success clears it, and after 5 failures an exponential back-off in uptime (30 s doubling to 24 h, PROVISIONAL, UC-203) refuses attempts without running the KDF; a reboot restarts the wait and a storage fault refuses the attempt. §C.4 gains step 0. §C.2 gains a note that a 32-byte UKMD is not an XTS-AES-256 key and that 64 MiB of Argon2id is 64 times the device's on-chip RAM with no SDRAM fitted; neither is changed here, both go to the `OI-EMMC2-02` reviewer (`docs/human-input/oi-priv-35-uhdr-xts-key-and-argon2id.md`) (`OI-PRIV-35`, NP-PRIV-ANALYSIS-004).**
