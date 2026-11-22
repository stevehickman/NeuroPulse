date: 2026-11-22
## DHF
author: NeurOne Software Engineering
text: **{NP-COND-LINK-001 from → to}, {NP-SW-001 from → to}: `OI-PRIV-66` (1) to (4). The condition-link host check refuses non-canonical numeric hosts, CGNAT, multicast and private IPv4 inside IPv6 literals; an unreadable session history is kept, not overwritten (`REQ-SW03-35`); a direct route that does not say it is paired is withheld (`SWR-MG-10`).**
## NP-COND-LINK-001
author: NeurOne Software Engineering
text: **§3 rule 8: a numeric host must be a canonical dotted quad; CGNAT (100.64/10), multicast and reserved IPv4 are refused; an IPv6 literal must parse, carry no zone identifier, and any IPv4 address it embeds passes the rule; one trailing dot is ignored (`OI-PRIV-66` (1), NP-PRIV-ANALYSIS-007 L14).**
## NP-SW-001
author: NeurOne Software Engineering
text: **§5.3.1: new `REQ-SW03-35`, an unreadable session history is quarantined before anything can write over it, and `REQ-SW03-06` starts clean without destroying it; §6.3 `SWR-MG-10`: a direct route whose host does not say it is paired is withheld (`OI-PRIV-66` (2), (4)).**
