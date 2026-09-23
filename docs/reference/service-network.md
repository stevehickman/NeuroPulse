# Service Network (all locked)

> Relocated from CLAUDE.md Rev 32 §8 to slim the always-loaded core. Authoritative content for service network design. Referenced from CLAUDE.md → Document Map.

## 8.1 Partner tiers

| Tier | Examples | Service tasks | Certification | Equipment | Revenue/yr at scale | Launch timing |
|------|----------|---------------|---------------|-----------|---------------------|---------------|
| A — Optical centers | LensCrafters, Pearle Vision, independent opticians | S3 Rx clip manufacture + fitting (primary) · Lens replacement (standard + EC) · Calibration (secondary) | 4-hr initial · 1-hr annual online | $400–600 calibration reference (loaned) + $80–120 jig (optional with sliding rail) | $8K–35K/yr | Year 1 — already engaged via S3 program |
| B — Electronics repair | uBreakiFix/Asurion · iFixit partners | Zone module FPC swap · DFU recovery · eMMC data recovery · Impact inspection · Fluxgate calibration | 6-hr initial · 2-hr annual practical | ESD workstation (existing) + eMMC adapter ($150–200) + DFU software (downloaded) | $4K–18K/yr | Year 2 — major metros first |
| C — Retail triage | Best Buy Geek Squad · Apple Authorized Service | Warranty intake + triage · Routing to Tier A/B · Consumable sales | 2-hr initial · 30-min annual online | None — partner app access only | $1K–7K/yr | Year 1–2 broadly — legitimacy signal |
| Depot — NeurOne mail-in | Backstop | All tasks · T2 same-day loaner · Precision fluxgate calibration | Full internal training | All in-house | Highest margin per task — backstop not primary | Day 1 |

## 8.2 Design changes that reduce service dependency

- Reference photodiode → eliminates 3-year PBM calibration service visit
- Sliding rail lens mount → user self-install, eliminates Tier A lens installation visit
- Lever ZIF connectors → user zone module swap, eliminates Tier B visit for upgrades
- Tool-free hub fan → user self-service
- Automated nightly UHDR backup → eliminates most data recovery emergencies

> **Note 2026-09-23 (`OI-TACSDRV-06`).** "Upgrades" in the lever-ZIF line above are **configuration
> upgrades within a tier**: tile additions such as T1-C, done by the user through bowl separation
> (`NP-HEX-ZM-001` §5.1). **No tier in §8.1 converts a T1 unit into a T2 unit.** A conversion would be
> medical-device manufacturing, not service. It touches factory-only assemblies (the PAN on the
> laminated L1 carrier, the posterior boss and, for TMS, the outer bowl), and a partner that did it
> would take on the manufacturer's obligations. `NP-REG-UPG-001` **recommends, but does not decide,**
> that T2 is only ever built as T2. If the principal chose conversion instead, it would be a **Depot**
> task and would need `EMF-1`-class attenuation measurement there (`NP-REG-UPG-001` §5.2).
>
> **DECIDED 2026-09-23 (`NP-REG-UPG-001` Rev 2 §7.0): T2 is only ever built as T2.** **No tier, the
> Depot included, changes a unit's tier**, and no service tool may write a unit's tier identity
> (`REQ-UPG-02`). The Depot does not need `EMF-1`-class capability for conversion. **Pro Entry → Pro
> Full stays inside T2.** Every T2 outer bowl carries the TMS window (pending `OI-UPG-05`), so the step
> adds the applicator, its supply and software. If TMS turns out to need a T2 hub variant, the hub
> swap is a Depot task inside one cleared device family. **Modules carry over** to a T1 owner's new
> T2 (`REQ-UPG-03`). Moving them is the same user tile swap as above, not a service event.

**Residual mandatory service per T1 user over 5 years:** 2–5 optician visits (Rx clip, already part of their workflow) + 0–1 fluxgate calibration + 0–2 damage-driven events.

## 8.3 Interface protection covers

Three cover types, all tethered to headset:

| Cover type | Material | Retention | Count in box | Replacement |
|-----------|----------|-----------|-------------|-------------|
| Zone slot plugs (5 per headset) | Shore 30A medical silicone, 5 colours (position-coded) | Friction/compression in slot, IP54 | 5 installed + 5 spare | 5-pack $9.99 |
| Accessory port covers (3 per headset) | Shore 40A TPE + encapsulated steel disc + Shore 20A silicone fins | N42 magnetic attraction via steel disc, ~400g pull | 3 installed + 2 spare | 3-pack $7.99 |
| Lens rim guards (2 per headset) | Shore 85A UV-stable TPU, clear | Mechanical snap-fit over rim profile | 2 installed + 1 spare pair | Pair $6.99 |

**Anchor posts:** molded into headset shell at zero incremental tooling cost if specified before first cut. All tethered — cannot be permanently lost without deliberate cutting.

> **Note 2026-09-23 (`OI-ART-07` closed).** The accessory **port** covers are on the control hub, not the headset shell, and their anchor posts are owned by `NP-TOOL-HUB-001` F-02 (≤ 20 mm tether — a hazard control against the hub fan, `RISK-HUB-01`). That specification does not yet list the hub accessory ports or a magnetic retention method; both are `OI-HTOOL-08`. The zone slot plugs above are retired with the zone-module architecture (`NP-TOOL-SHELL-001` F-01).
