date: 2026-11-03
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}: the Mode 3 "stored" rule is corrected in the shared app (it reports a protocol stored only when the helmet holds it), and more Swift test cases are ported to Kotlin.** See the NP-SW-001 history.
## NP-SW-001
author: NeurOne Software Engineering
text: **§5.3.1: correction to Rev 143 — in Mode 3 the shared app reports a protocol stored only when the helmet holds it (`SWR-UP-02`, `SWR-HV-08`).** Rev 143 copied the Rev 132 rule ("a tier or hardware-version refusal is shown, any other outcome reports stored"), which Revs 133 to 135 had replaced in the iOS app: `ProtocolUploadFlow.program` reports stored only for an accepted answer to this upload, or no answer, and says the reason after every refusal and after an answer for other bytes. The Swift test port found it (`testModeThreeIsNotStoredAfterAnyRefusal`). `ProgrammingOutcome` now holds the rule, the Session tab uses it, and the per-cause messages are one function (`helmetRefusalMessages`) with a test that each cause has its own text. Also ported: the zone-resolution cases of the iOS socket-mask tests (`REQ-NPPS-17`), the `zones:` parsing refusals (`REQ-NPPS-08`), the consumable tracker, plural and tier-price, erasure, session history, descriptor compiler, parser and bundled-library cases.
