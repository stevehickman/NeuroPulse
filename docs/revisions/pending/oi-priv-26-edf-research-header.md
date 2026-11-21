date: 2026-11-21
## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-EMMC-002 from → to}, {NP-SW-001 from → to}, {NP-PRIV-REM-001 from → to}: OI-PRIV-26 (NP-PRIV-ANALYSIS-004) (GitHub #954, NP-PRIV-ANALYSIS-004 H6): the EDF+ research gate checks a separate research header (code X or per-study, dates blank or rounded to the study's window, no time of day, fixed recording ID, whole header scanned) instead of the local header, which passed exact times and a code that links extracts; `REQ-SW02-12` added.**
## NP-FW-EMMC-002
author: NeurOne Firmware Engineering
text: **§E.2 marked as the local header only and its check widened to the whole patient-ID field; §E.4 gate now checks the research header under the study's policy; §E.5 added (research EDF+ header, per-study code, date rounding, what is not yet covered) (OI-PRIV-26 (NP-PRIV-ANALYSIS-004), GitHub #954).**
## NP-SW-001
author: NeurOne Software Engineering
text: **§5.2.2: `REQ-SW02-12` states that a research extract carries only the §E.5 research header, and what the gate refuses (OI-PRIV-26 (NP-PRIV-ANALYSIS-004)).**
## NP-PRIV-REM-001
author: NeurOne Quality
text: **STEP-05 marked reopened in part: the research gate passed exact dates, the recording ID and a linking code; the repair is NP-FW-EMMC-002 §E.5 and the remainder is in OI-PRIV-26 (NP-PRIV-ANALYSIS-004) (GitHub #954).**
