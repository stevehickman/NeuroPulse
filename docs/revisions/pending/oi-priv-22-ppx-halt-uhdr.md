## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}: the photoparoxysmal halt is no longer written to SHDR (`OI-PRIV-22` of NP-PRIV-ANALYSIS-004, Finding H2, GitHub #950). A set halt wrote an SHDR fault record on every telemetry tick, a record that existed only when a neurological predicate was true (`CLAUDE.md` §5.1 rule 2). The halt is now a byte of every UHDR visual record, set or clear (`REQ-FWHUB-88`). Whether the general "safety interlock log → SHDR" line is narrowed is the principal's (`docs/human-input/oi-priv-22-detector-interlocks-in-shdr.md`).**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§6.1: the `0x16` visual record gains a `ppx_halt` byte, written in every record, and the conditional SHDR `0x82` write on a photoparoxysmal halt is removed. §10.1: new `REQ-FWHUB-88` (the halt is UHDR and in no SHDR record), verified by two `np_log_backend_tests` tests that fail on the earlier code (`OI-PRIV-22` of NP-PRIV-ANALYSIS-004).**
