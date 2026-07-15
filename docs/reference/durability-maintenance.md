# Durability + Maintenance (all locked)

> Relocated from CLAUDE.md §7 (Rev 32) to slim the always-loaded core. Authoritative content for durability/maintenance design changes. Referenced from CLAUDE.md → Document Map.

## 7.1 Critical design changes (must be in tooling specifications before first cut)

| Change | BOM delta | Why critical |
|--------|-----------|-------------|
| ITO → AgNW conductive lens coating | +$8–12/lens | ITO: 0.5% strain-to-failure, cracks on flex or point impact. AgNW: 5–10% flex, compatible with hard coat, maintains 85–90% transmission |
| Hard clamshell case (replaces soft pouch) | +$8–14 | Lens scratching certain within first month without case. Includes probe dock. Doubles as shipping container. |
| Intranasal probe hub dock (molded) | Hub retool | Y-probe dropped probe-first fractures junction. Cannot be retrofitted. |
| Reference photodiode per zone (behind PDMS window) | +$2 total | Detects LED aging AND PDMS window fouling simultaneously. Eliminates 3-year service calibration visit. Protects J/cm² dose metering claim. |
| **Second scalp-side photodiode PD2 per zone (RISK-14 Option B)** | +$0.75–1.50/headset total | On scalp-facing PDMS surface, pin 19. PD1/PD2 ratio separates fouling (PD1↓ PD2 stable) from LED aging (both↓). T1 and T2 share identical zone module mold — firmware flag only. |
| Zone module connectors: 1,000-cycle rated (Hirose FH34S-20S-0.5SH or JAE FF03 — 0.5 mm pitch, back-flip lever ZIF, ≥1,000 insertion cycles) | +$2.00 | Molex SlimStack is a board-to-board connector — not an FPC family. Standard Molex ZIF FPC connectors rated only 20–30 cycles. Hirose FH34S confirmed as correct family. Confirm ≥1,000-cycle rating from full datasheet before BOM lock. 0.35 mm pitch insufficient — current per pin too high. Must specify before PCB layout. See NP-HW-FPC-001. |
| Lever-actuated ZIF for zone modules | Included above | Back-flip lever ZIF (Hirose FH34S mechanism) — zero insertion force, tool-free extraction. Enables user self-service zone module swaps. |
| **Zone module sliding eject lever (RISK-22 Option A)** | +$0.40/module | 10–12mm lever arm, 3:1 mechanical advantage, ≤1N extraction force. Recessed flush when closed; snap-fit detent prevents accidental ejection. 316SS hinge pin. Required for users with Parkinson's/post-stroke hand weakness. |
| **Self-sealing co-molded silicone gasket per zone module (RISK-16 Option A)** | +$0.30–0.60/module | Shore 40–50A medical silicone, D-section 2.5×2.0mm, 20% compression when seated. No user RTV required. IPX4 compliant after 10 field swap cycles (FAI-IPX-02 BLOCKING test). Gasket retention groove + silicone primer prevent delamination. |
| **Five-layer zone module keying (RISK-15)** | Included in tooling | Layer 1: asymmetric mechanical key (unique per zone, prevents physical mis-insertion). Layer 2: ZONE_ID resistor (ZM-01=10kΩ through ZM-05=220kΩ, 1%, pin 18; firmware debounce 3×ADC at 100ms). Layer 3: ISO 17049 braille + raised numeral. Layer 4: N tactile dots on shell at each slot. Layer 5: bone conduction audio ("Frontal Left connected"). Covers color-blind AND blind users. |
| **EEG cable routing channel in shell (RISK-21)** | $0 (tooling) | Dedicated 8×5mm molded channel on outer CFRP surface (opposite side from FPC bundle). ≥15mm separation from zone module FPCs required (DRC-18). Must be in shell tooling spec before first cut. |
| **PDMS SiO₂ interlayer bonding process** | Process cost | 75 nm RF magnetron sputtered SiO₂ on PI surface before O₂ plasma activation. Achieves 174–860 N/m peel force. 200-cycle IEC 60068-2-14 qualification required before production. See NP-FAI-ZM-001 §3e. |
| Interface protection covers (all tethered) | +$8–9 total | Anchor posts molded into shell at zero cost if specified before first cut. |
| Sliding rail lens mount | +$1.20 | Eliminates alignment jig. User self-install. |
| Dual-bank OTA firmware + USB-C DFU recovery | $0 (software) | Must be in bootloader from first firmware line. Cannot be added later. |
| Separate UHDR/SHDR eMMC partitions | $0 (firmware) | Must be in firmware specification before any storage architecture is written. |

## 7.2 Other locked design changes

| Change | BOM delta | Rationale |
|--------|-----------|-----------|
| Palladium-coated EMF shielding fabric | +$6/headset | Silver tarnishes 12–18 months. Palladium tarnish-immune for device lifetime. Fleet SHDR verifies stable attenuation — marketable, measurable claim. |
| N52 → N42 magnets in lens rim | −$0.80 | N52 brittle under corner drop. N42 more impact-tolerant, ≥1mm polymer wall required on all faces. |
| Braided aramid USB-C cable + dual silicone strain relief | +$3–4/cable | Commodity cables fail at strain relief within 6–18 months. 50,000+ flex cycle rating. Spare in box. |
| MagSafe hard gold contacts (>0.5µm cobalt-alloyed) | +$1.20 | Oxidised contacts → power throttling. Contact resistance monitored in SHDR. |
| 22F supercapacitor (from 10F) | +$1.80 | Allows 50% degradation over 5 years while maintaining transient absorption. |
| Industrial eMMC + LittleFS | +$2.40 | 30,000+ P/E cycles. Write endurance monitored in SHDR. |
| Bone conduction driver silicone isolator | +$1.80 | Piezoelectric element brittle — Shore 20–30A silicone mount absorbs impact. |
| Silicone over-mold at Y-probe junction | +$1.20 | Flex without fracture. Minimum 20mm bend radius marked on probe shaft. |
| Silicone potting for micro-LED array | +$2.40/lens | 1,800+ thermal cycles over device lifetime without delamination. |
| Hard coat on EC lens (3–5µm silicone) | +$4.00/lens | Standard in automotive EC mirrors. Prevents scratch damage to active EC layer. |
| Aluminium bayonet for audio cups | +$1.60 | Plastic detent flattens at 500–1,000 cycles. Metal-to-metal rated for device lifetime. |
| Moisture-barrier electrode tip hydration caps (WVTR <0.5) | +$0.80 | Extends factory-sealed electrode storage life to 24+ months. |
| Audio cup user-replaceable snap-in mesh frame | +$0.60 | Snap-in frame; user pops out, rinses, reinserts or replaces ($9.99/pair). |
| Mesh cleaning brush in box | Minimal | Standard maintenance tool for mesh surfaces. |
| Hub air filter (30-micron foam, snap-out) | +$0.45 | Captures carpet fibres in clinical environments. Hub NTC temperature trend flags cleaning need. |
| Automated nightly UHDR backup (incremental) | $0 | When on USB-C power. Failure becomes hardware swap not data loss. |
| ADS1299 self-calibration at session start | $0 | Internal reference routed to all channels. Eliminates EEG amplitude drift. Coefficients in SHDR. |
| Plasma-activated PDMS for optical windows | Minimal | Hydrophilic surface repels sebum. Anti-fouling without additional parts. |
| EC rim contact hard gold plating | Included in contact spec | EC driver monitors transition time as contact resistance proxy. Cleaning prompt when >3s transition. |
| Hub NTC thermistor | +$0.15 | Cross-calibrates headset NTCs, monitors supercapacitor aging, enables hub temperature-based SHDR alerts. |
| Enclosed PTFE-lined Boa cable channel | $0 (tooling) | Prevents hair entanglement — must be in occipital arch tooling. |
| Tool-free hub fan (quarter-turn captive fastener) | +$0.80 | No screwdriver required for fan replacement. |
| Humidity indicator card in box | +$0.30 | Visual confirmation package integrity during shipping and storage. |
| IPX4 rating target for headset | Testing cost | Splash-proof from all directions. Enables "workout-safe" marketing claim. |

## 7.3 Calibration self-maintenance

| Sensor | Self-calibration method | Residual service requirement |
|--------|------------------------|------------------------------|
| PBM photodiode | Dual-PD: PD1 (behind PDMS, forward emission) + PD2 (scalp-facing, backscatter). PD1/PD2 ratio separates fouling from LED aging. PD1↓ PD2 stable → fouling prompt. Both↓ proportional → LED aging correction. | None — dual-PD eliminates ambiguity that single PD could not resolve |
| EEG amplifier (ADS1299) | Internal reference routed to all channels at session start — gain/offset correction applied | None — fully self-calibrating |
| NTC thermistors | Hub NTC cross-calibration: compare headset NTCs vs hub reference at ambient equilibrium (>10 min since last session) | None — flag at ±1.5°C offset |
| Fluxgate magnetometers | Zero-field nulling at session start + geomagnetic field magnitude comparison via phone GPS | **3–5 year Tier B service visit** (scale factor drift requires Helmholtz test coil) |
| EC lens contacts | EC driver monitors transition time — flag when >3 seconds (vs 2s spec) | Cleaning prompt; no service visit |
| Audio cup mesh | Driver impedance monitoring — detects fouling pattern | User-replaceable snap-in frame — no service visit |
