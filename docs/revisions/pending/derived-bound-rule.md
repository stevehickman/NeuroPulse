## DHF
author: NeurOne Firmware Engineering
text: **CLAUDE.md Rev 82 → 83, {NP-FW-NVRAM-001 from → to}, {NP-SOUP-LFS-001 from → to}: §18 gains the rule that a derived bound is computed from its foundational value and a fixed foundational value states why; Map 3 has one 64-byte layout (D-34), the 32-byte version 1 removed.**
## NP-FW-NVRAM-001
author: NeurOne Firmware Engineering
text: **§3.3.1.5, §7.2.1, D-25, D-33, D-34 (`OI-UINT16-01`, the principal, 2026-11-19): Map 3 has one layout, a 64-byte row at version 1 with 32-bit counts stored whole. The 32-byte row, the duplicate tail and the 0xFFFF ceiling are removed because no hardware has shipped and no row was ever written; D-33 is superseded. 768 rows, 9 per socket, margin 9 days home and 0.8 days clinic.**
## NP-SOUP-LFS-001
author: NeurOne Firmware Engineering
text: **§13 (inline records): Map 3's records are 64 bytes, not 32, after `NP-FW-NVRAM-001` D-34; they still sit under the 256 B `inline_max`.**
