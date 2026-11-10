date: 2026-11-10
## DHF
author: NeurOne Firmware Engineering
text: **{NP-FW-HUB-001 from → to}, {NP-SIM-FW-001 from → to}: the goggles', probe's and clip's own safety checks each return their own hub status (`OI-SIMFW-47`, the principal's ruling that the firmware reports which check failed), with the Class C part proposed as `OI-SIMFW-61`; and the inventory of programmable parts and what the twin runs (`OI-SIMFW-39`), with one open item per part that does not run.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **§17 requirements: new `REQ-FWHUB-77` (Class B; `np_hub_types.h` −32 to −39, with −30 for the photic-zone refusal at the drive): the goggles', intranasal probe's and auricular VNS clip's own safety checks no longer share `NP_HUB_ERR_SAFETY_REJECTED`; each returns its own status, and none is written to the SHDR fault log with its code.**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **§7, §25: `OI-SIMFW-47` decided and closed for Class B and the app (the reason table now names each check; the view names the tier gate and a line the hub never requested); new `OI-SIMFW-61`, a proposal for the safety MCU's owner to report why the declaration and geometry gates withheld a line. §7, new §28: `OI-SIMFW-39` inventory done (every programmable part, its firmware, whether the twin runs it, measured with `sim-coverage.sh` over the linked sources); `OI-SIMFW-50` to `OI-SIMFW-60` raised for the parts that do not run; the cluster controllers and the hex-tile MCU have no firmware code, and the active EMF stage has no source.**
