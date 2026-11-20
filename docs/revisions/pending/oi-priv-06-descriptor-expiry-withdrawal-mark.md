date: 2026-11-19
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}, {NP-SW-PORTAL-API-001 from → to}: `OI-PRIV-06` closed. A withdrawal is marked with a day and a count, not an instant; a study descriptor's signed form gains an expiry day (CLAUDE.md Rev 82).**
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3, `SWR-MG-04`: a withdrawal in a bundle is a mark, the day and that decision's withdrawal count (`yyyy-MM-dd#NNNNNN`, `REQ-CONSENT-24`), where it was a UTC time. The merge rule is unchanged: the later mark wins, as text.**
## NP-SW-PORTAL-API-001
author: NeurOne Software Engineering
text: **§4a: the canonical form a study descriptor is signed over is `NP-STUDY-DESCRIPTOR-V2`, which adds the expiry day as its last field (`REQ-CONSENT-09` i). No V1 descriptor was ever issued.**
