date: 2026-11-22
## DHF
author: NeurOne Software Engineering
text: **{NP-PRIV-ANALYSIS-007 from → to}: `OI-PRIV-63` (1) and (2) fixed. A staged SHDR record is one file per helmet under the owner store's salted hash, read with one reading of the helmet and deleted on revoke (`REQ-SHDR-08` (e), (f)); a different hub token over a kept one ends that helmet's custody and forgets its owner's decisions (`REQ-SHDR-09` (d)). L17 checked and left open for a ruling.**
## NP-PRIV-ANALYSIS-007
author: NeurOne Software Engineering
text: **§3.2 M1 and M4 marked fixed with what remains; §3.3 L17 marked checked and not fixed (the clear destroys the key that seals the records first, and `SWR-MG-09`), both under `OI-PRIV-63`.**
