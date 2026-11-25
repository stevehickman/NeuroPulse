date: 2026-11-25
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}, {NP-PRIV-ANALYSIS-007 from → to}: `OI-PRIV-63` closed. A phone marked as the helmet owner's keeps the warranty owner's records through a user's clear; a user's own phone, the default, forgets them (`REQ-SHDR-10` (d)).**
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3 `SWR-MG-09`: the old phone's clear keeps the warranty owner's records and the mark on a phone marked as the owner's (`REQ-SHDR-10` (d); the principal, 2026-11-25, `OI-PRIV-63`); an erase keeps nothing.**
## NP-PRIV-ANALYSIS-007
author: NeurOne Software Engineering
text: **§3.3 L17 marked fixed (`OI-PRIV-63` closed, `REQ-SHDR-10` (d)).**
## LOG
text: **{date} — `OI-PRIV-63` CLOSED: a user's move off a phone keeps the warranty owner's records on a clinic phone and wipes them on the user's own (the principal; `REQ-SHDR-10` (d), `SWR-MG-09`).** The owner page marks a phone as the owner's (off by default, so an unmarked phone is the user's own). On a marked phone the core's clear reads the owner's records and the mark before it destroys the data key and writes them back under the new key; an erase forgets them either way. Items (1) and (2) were built in #1043.
