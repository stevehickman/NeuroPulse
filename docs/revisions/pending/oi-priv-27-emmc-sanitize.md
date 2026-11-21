date: 2026-11-21
## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-EMMC-002 from → to}, {NP-SW-001 from → to}: `OI-PRIV-27` (`NP-PRIV-ANALYSIS-004` H7, GitHub #955). The factory reset now purges: eMMC SANITIZE is CMD6 SWITCH to EXT_CSD[165], not CMD38; a new step R-7a runs it after the UHDR, SHDR and Config erases, and the bootloader's interrupted-reset path runs it too; `REQ-SW02-13` added; the authenticated reset command is briefed for the principal.**
## NP-FW-EMMC-002
author: NeurOne Firmware Engineering
text: **§B.3: R-5 named "SANITIZE (CMD38, SANITIZE_START)", which is a plain erase; corrected to CMD6 SWITCH writing 1 to EXT_CSD[165] (support in EXT_CSD[231] bit 6), moved to a new step R-7a after the erases so it also purges SHDR and Config, with no fallback to a plain erase; the 240 s bound is `UC-202`; R-1/R-2 have no implementation (`OI-PRIV-27`, `NP-PRIV-ANALYSIS-004`).**
## NP-SW-001
author: NeurOne Firmware Engineering
text: **§5.2.2: `REQ-SW02-13` added: a factory reset purges the data partitions with eMMC SANITIZE started through EXT_CSD, after the last erase and before the new salt and token, and a SANITIZE that fails keeps the reset in progress (`OI-PRIV-27`, `NP-PRIV-ANALYSIS-004` H7).**
