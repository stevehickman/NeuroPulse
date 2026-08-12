# Optional Accessories + Companion Software (provisional specs)

> Relocated from CLAUDE.md Rev 32 §3b to slim the always-loaded core. This is the authoritative content for provisional accessories. Referenced from CLAUDE.md → Document Map.

## 40Hz Vibrotactile — Mastoid LRA Pad (provisional)

Purpose-built accessory delivering precisely characterized 40Hz somatosensory stimulation, aligned to Tsai lab (MIT) GENUS multi-sensory protocol. **Provisional — release contingent on HOPE Phase 3 results (mid-2026). Hardware provision in first revision at zero tooling cost.**

- **Actuator:** Linear resonant actuator (LRA), 8–10mm diameter, e.g. Jinlong JMC0834 or equivalent. LRA preferred over ERM: precise frequency control, low distortion, flat resonance profile at 40Hz drive.
- **Driver IC:** Texas Instruments DRV2605L (I2C, open-loop mode). Open-loop drive at 40Hz ± 0.5Hz with amplitude control via gain register. No resonant frequency tuning required (drive frequency set in firmware, not actuator resonance).
- **Placement:** Posterior temporal / mastoid process. Rationale: (1) direct bone coupling to skull — better transmission to somatosensory cortex than wrist; (2) adjacent to temporal lobe somatosensory representation; (3) compatible with existing temporal stability wing anchor — no new shell tooling required if anchor boss provisioned at first cut.
- **Output spec:** 0.6–1.2G peak acceleration at skin surface (DRV2605L gain register configurable); 40Hz ± 0.5Hz; duty cycle 100% continuous (20-minute session target).
- **Form factor:** 30mm diameter silicone overmoulded pad; clip attachment to temporal wing; 3-pin pogo or JST connector to hub accessory port. Shore 30A silicone contact face for comfort.
- **Power draw:** ~80–120mW continuous. Supplied from hub accessory port (existing 500mA capability).
- **BOM estimate:** LRA $1.50–2.50 + DRV2605L $1.20–1.80 + PCB/passives $0.50 + silicone housing $0.80–1.20 = **$4–7 total per pad**. Pair (bilateral option): $8–14.
- **Retail accessory price (projected):** $49–79 per pad / $79–119 bilateral.
- **Firmware:** 40Hz square-wave drive pattern; session start/stop synchronized with audio/visual 40Hz channels via hub; amplitude ramp 2s up/down (comfort).
- **Marketing note:** Apple Watch sync app is available as a free companion — but the mastoid pad delivers results the Watch cannot. Key differences: mastoid placement couples directly to skull (vs wrist-to-brain soft tissue attenuation); 40Hz ± 0.5Hz locked precision (vs uncharacterised Taptic Engine output); hub-powered (vs Watch battery drain from 20 min continuous haptics). Message: "Use the Watch app as an extra layer — for the full experience, the pad is what delivers it." See marketing-notes.md for full draft copy.
- **Status:** PROVISIONAL — await HOPE Phase 3 (Cognito Therapeutics, n=670, mid-2026). If positive: release mastoid pad within 6 months. Hardware anchor boss in temporal wing tooling at first cut (zero incremental tooling cost).

## Apple Watch Sync App (provisional)

Companion watchOS app extending NeurOne session experience to Apple Watch. Three sync channels. Does **not** replace purpose-built NeurOne hardware for any therapeutic function — supplements it.

- **Communication:** BT 5.3 LE (hub already has BT radio, antennas in hub); WatchConnectivity framework via paired iPhone app; session sync protocol over BLE GATT custom service.
- **Channel 1 — Haptic sync:** watchOS Core Haptics delivers 40Hz pattern in synchronization with NeurOne hub session clock. Adds wrist somatosensory channel on top of mastoid LRA pad. Not a standalone therapeutic — supplement only. Caveat in app: "Works best with NeurOne mastoid vibrotactile accessory."
- **Channel 2 — Audio sync:** Watch app plays binaural beats / isochronic tones / breathing pacer audio through AirPods or earphones paired to Watch, synchronized to hub session. Useful when user wants bone conduction reserved for breathing cue while earphones handle binaural beats, or for sessions away from the hub speaker range.
- **Channel 3 — Visual sync:** Watch display shows 40Hz visual flicker (reduced brightness, GENUS-compatible) or EMDR left/right indicator arrow synchronized to goggle session. Also: session status, coherence score live feed, HRV biofeedback breathing ring (complementary to app display for wrist-glance UX).
- **Additional Watch functions:** Session timer + haptic end-of-session alert; protocol selector (basic, without phone); quick impedance check result notification; consumable low reminders.
- **Regulatory note:** All Watch-delivered functions are declared as session monitoring / user interface aids, not therapeutic delivery. Therapeutic claims attach to NeurOne hardware only.
- **BOM delta:** $0 hardware. Software development cost only.
- **Status:** PROVISIONAL. Added to Year 1 iOS app development roadmap (NP-APP-ROADMAP-001 Rev 1 — Issue #32). Develops in 4 phases post-core-app-launch: Phase 1 session status + HRV breathing ring (Month 1–2), Phase 2 audio sync (Month 3–4), Phase 3 haptic sync 40Hz (Month 4–5), Phase 4 40Hz visual flicker (Month 6+, blocked on OI-WA-02 screen brightness characterization ≥100 nits at 40Hz). See `docs/np_app_roadmap_001.md`.
