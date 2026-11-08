## DHF
author: NeurOne Firmware Engineering
text: **NP-FW-HUB-001 Rev {NP-FW-HUB-001 from → to}: the photoparoxysmal detector's channel roles are data (`OI-FWHUB-32` (1a)); the T1 montage has no occipital channel (`OI-FWHUB-34`).** A montage and a role table that Clinical edits; the shipped T1 montage resolves to "cannot see". Nothing calls it yet and no limit changed.
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **`OI-FWHUB-34` raised; `OI-FWHUB-32` (1a) updated.** `np_ppx_roles.c` resolves a montage against a role table into the detector's masks; the table is the engineering proposal (`UC-125`) with F3, F4 and Fz left to Clinical. The T1 montage (Fp1/2, F3/4, C3/4, P3/4) has no occipital channel, so the detector can never see on it. Four mutants caught.
