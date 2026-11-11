## DHF
author: NeurOne Firmware Engineering
text: **{NP-SW-001 from → to}, {NP-REG-UPG-001 from → to}: the helmet hardware version in the tier identity (issue #569 package 1), what was left.** The hub's parse of the tier report is host-tested through the real `np_safety_spi.c`; the hub forgets the version when its link is re-initialised; the removal of the default version 1 is a build check; the safety MCU tests cover every value of the tier byte. The factory step (`REQ-UPG-04`) is not built. No wire format changed.
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3.2 status cells of `SWR-HV-06` and `SWR-HV-07`.** `SWR-HV-06`: the hub-parse host test is built (`np_safety_spi_tests`), and re-initialising the link now resets the hub's version to the default, as it already reset the tier. `SWR-HV-07`: the removal item is checked by the build, since the generated socket table is a compile error with a second hardware version while `NP_HW_VERSION_DEFAULT` is defined. Parts declared for both rows in `docs/status/requirement-parts.json` and every test tag names its part. No requirement text changed. Still open: the factory step, and the removal itself.
## NP-REG-UPG-001
author: NeurOne Systems Engineering
text: **§7.8 and the `OI-UPG-09` row brought up to what is built.** §7.8 said none of the hardware version was built and that the tier report's room had not been checked; the firmware half is built, the report is seven bytes at window bytes 20 to 26 with its checksum over bytes 0 to 4, and the hub-side refusal exists. Item 5 records the build check on the default's removal. `REQ-UPG-04` (the factory step) is unchanged: not built. No decision changed.
