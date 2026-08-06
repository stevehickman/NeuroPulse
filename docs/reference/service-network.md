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

**Residual mandatory service per T1 user over 5 years:** 2–5 optician visits (Rx clip, already part of their workflow) + 0–1 fluxgate calibration + 0–2 damage-driven events.

## 8.3 Interface protection covers

Three cover types, all tethered to headset:

| Cover type | Material | Retention | Count in box | Replacement |
|-----------|----------|-----------|-------------|-------------|
| Zone slot plugs (5 per headset) | Shore 30A medical silicone, 5 colours (position-coded) | Friction/compression in slot, IP54 | 5 installed + 5 spare | 5-pack $9.99 |
| Accessory port covers (3 per headset) | Shore 40A TPE + encapsulated steel disc + Shore 20A silicone fins | N42 magnetic attraction via steel disc, ~400g pull | 3 installed + 2 spare | 3-pack $7.99 |
| Lens rim guards (2 per headset) | Shore 85A UV-stable TPU, clear | Mechanical snap-fit over rim profile | 2 installed + 1 spare pair | Pair $6.99 |

**Anchor posts:** molded into headset shell at zero incremental tooling cost if specified before first cut. All tethered — cannot be permanently lost without deliberate cutting.
