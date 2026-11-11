date: 2026-11-11
## DHF
author: NeurOne Software Engineering
text: **{NP-NPPS-REF-001 from → to}, {NP-FW-HUB-001 from → to}, {NP-FW-HEXTILE-001 from → to}, {NP-HW-HEXTILE-001 from → to}, {NP-SES-PWR-001 from → to}: the shared NPPS core refuses a value it used to clamp, wrap or cut (the principal, 2026-11-11: no silent clamping anywhere; the hub was done first). New REQ-NPPS-33. PBM duty above 25 % is refused (interim, OI-HEXTILE-31), so a continuous-wave block no longer compiles and nine shipped protocols are refused (OI-NPPS-CLAMP-01); a short tDCS ramp, and a TMS, audio, visual, PBM-frequency, time or serial value its descriptor field cannot carry, are refused. No limit value, range, grammar or wire format changes.**
## NP-NPPS-REF-001
author: NeurOne Software Engineering
text: **§14: new REQ-NPPS-33, the compiler refuses a value its descriptor field cannot carry as authored and never clamps, wraps, cuts or raises one, and refuses first everything the hub's load-time check refuses (REQ-FWHUB-78). §4.1, §4.2, §4.5, §4.7, §4.8, §4.10, §4.11: the rows for duty, frequency, ramp, beat, carrier, EMDR cadence and pulse count say what the descriptor carries and that the compiler refuses outside it. A CW block parses and is refused for its 100 % duty until OI-HEXTILE-31. New OI-NPPS-CLAMP-01 (in the open-items log) lists what is left: nine CW protocols, rounding to a step, an audio beat above 65.535 Hz, unknown strings encoded as a default. No grammar, range or limit value changes.**
## NP-FW-HUB-001
author: NeurOne Software Engineering
text: **§13 OI-FWHUB-35 (f): the shared core's clamps are refusals (NP-NPPS-REF-001 REQ-NPPS-33), and a core test reads the hub's limit constants from its headers. §8: the shared core maps reason 40 (NP_HUB_ERR_OUTSIDE_LIMIT) to the cause outside_limit, and reasons 41 and 42 (the two cervical block refusals, REQ-FWHUB-78 (k)) to cervical_block_too_short and cervical_block_too_long, which the hub's own change left owed. The item stays open on its other parts. No hub requirement or code changes.**
## NP-FW-HEXTILE-001
author: NeurOne Software Engineering
text: **§5.5: the compiler refuses a duty above 25 % where it clamped, and so refuses a CW block, which it used to sign with the duty register at 0x32. The paragraph on what can reach a tile is corrected to say so. No tile requirement changes.**
## NP-HW-HEXTILE-001
author: NeurOne Software Engineering
text: **§13 OI-HEXTILE-31: item (ii), the compiler's duty clamp, is a refusal at the same 25 % (NP-NPPS-REF-001 REQ-NPPS-33 (a)). No place reshapes a duty any more; the validator, the compiler and the hub still hold the 25 % the item removes. The item stays open.**
## NP-SES-PWR-001
author: NeurOne Software Engineering
text: **§2.3: a dated note that the compiler refuses a duty above 25 % and no longer caps it, so the CW preset the paragraph discusses does not compile until OI-HEXTILE-31. No table or figure changes.**
