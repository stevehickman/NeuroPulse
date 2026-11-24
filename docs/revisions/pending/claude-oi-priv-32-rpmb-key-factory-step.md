date: 2026-11-23
## DHF
author: NeurOne Firmware Engineering
text: **{NP-REG-UPG-001 from → to}: OI-PRIV-32 (NP-PRIV-ANALYSIS-004), GitHub #960: new §7.10 designs the factory step that programs each helmet's eMMC RPMB key, and raises REQ-RPMBK-01 to -06; the key source, field re-provisioning and C or Rust for the bootloader's KDF and MAC are briefed for the principal (docs/human-input/oi-priv-32-rpmb-key-source.md). Built: the bootloader key seam with a device source that always fails (every image stays refused), and a host model of the programming sequence in common/npps-core. No decision changed.**
## NP-REG-UPG-001
author: NeurOne Firmware Engineering
text: **§7.10 (new): the factory step that programs the eMMC RPMB key for the rollback floor (OI-PRIV-32 (NP-PRIV-ANALYSIS-004), GitHub #960), run in the §7.9 pass before the locks; REQ-RPMBK-01 to -06. Built: np_rpmb_key.h with a failing device source, and the host model rpmb_provision.rs. Not decided: the key source, field re-provisioning, C or Rust (brief). §9 cross-references OI-PRIV-32 and OI-SEC-02. No decision changed.**
