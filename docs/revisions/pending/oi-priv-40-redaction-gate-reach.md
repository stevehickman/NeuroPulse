## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}, {NP-SW-CI-001 from → to}: `check-redaction-shape.ts` now finds every SHDR writer in device firmware, fails an undeclared one, and flags whole-status copies; known violations are tied to their open items (OI-PRIV-40 (NP-PRIV-ANALYSIS-004), #968).**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§6.3: states what `check-redaction-shape.ts` enforces since OI-PRIV-40 (NP-PRIV-ANALYSIS-004): every SHDR sink call sits in a declared reporting path, no declared path names a sensitive bit or copies a whole status word, and its reach is partial (names, not data flow).**
## NP-SW-CI-001
author: NeurOne Tooling
text: **§5.0.2 CI-Scan-Paths table: `check-redaction-shape.ts` also reads `docs/status/pending-decisions.md`, to fail a known violation whose open item is closed (OI-PRIV-40 (NP-PRIV-ANALYSIS-004)).**
