# Competitive Position

> Relocated from CLAUDE.md §9 (Rev 32) to slim the always-loaded core. Authoritative content for competitive positioning and claims. Referenced from CLAUDE.md → Document Map.

| Feature | NeurOne Home | NeurOne Pro | Vielight Neuro Pro 2 (~$5K) | Neuronic 1070 ($3K–5K) | Sens.ai (~$1.5–2K + sub) | Neurode (~$999 + $29/mo) |
|---------|----------------|----------------|------------------------------|------------------------|--------------------------|--------------------------|
| PBM wavelengths | 660+810nm base / 660+810+1064nm with smart module upgrade (3λ) | 660+810+1064nm+1170nm (4λ) | 810nm (1λ) | 1070nm (1λ) | ~810nm (1λ) | None |
| Total LED count | 600 (300/wavelength) | 600 + 1170nm LDs | ~12 transcranial | 256–300 (1 wavelength) | ~7 midline | None (no PBM) |
| Peak irradiance | 400 mW/cm² pulsed* | 400 + 1,000 mW/cm² | 400 mW/cm² | Not specified | Not specified | N/A |
| Real-time dose (J/cm²) | Yes — per zone | Yes | No | No | No | N/A |
| Brain monitoring | EEG (electrical) | EEG (electrical) | None | None | EEG (electrical) | **fNIRS (hemodynamic, "152"-ch)** — NeurOne has no fNIRS |
| EEG channels | 8 semi-dry 24-bit | 21 wet gel 24-bit | None | None | 3 dry midline | None (fNIRS instead) |
| Closed-loop | Yes — autonomous EEG-adaptive | Yes — all modalities | No | No | EEG→PBM only | Yes — fNIRS-informed intensity |
| BES/tACS | Yes | Yes clinical | No | No | No | No |
| tRNS (random noise) | No | Possible via 16-ch arbitrary-waveform driver (not a named preset) | No | No | No | **Yes — primary modality (0.55–1.55mA, PFC)** |
| tDCS | Yes | Yes + HD-tDCS | No | No | No | No |
| TMS | No | Yes focal | No | No | No | No |
| VNS + HRV | Auricular electrical + PPG | + cervical option | Optical VNS (separate) | No | HRV only | No |
| Audio entrainment | Binaural + bone conduction | + clinical EMDR | No | No | No | No |
| Visual stimulation | 108 LEDs/lens + EMDR + retinal PBM + Mode F | + EEG-adaptive, seizure detection | No | No | No | No |
| EMF shielding | 5-layer palladium + active | Same | None | None | None | None |
| Autonomous mobile | Yes — power bank | Yes | BT only | BT only | BT only | No — app required |
| No mandatory subscription | Yes | Yes | Yes | Paywall on PLUS | Required $99–199/yr | No — $29/mo required |
| Published clinical trials | None (new product)* | None (new product)* | 35+ RCTs | Limited | Ongoing | 1 claimed (DB placebo-controlled RCT, n=120 ADHD; status unclear) |

*See regulatory-strategy.md and pending-decisions.md for pending actions on irradiance claim and evidence gap.

**Neurode gap note (2026-07-13):** Two Neurode capabilities have no NeurOne equivalent today — **fNIRS hemodynamic brain monitoring** and **tRNS**. Both are low-cost to close: fNIRS reuses NeurOne's existing NIR optics (see `docs/np_fnirs_feasibility_001.md`); tRNS is a firmware preset on the T2 arbitrary-waveform driver. Neurode is otherwise a single-modality ADHD/focus device with no PBM, EEG, EMF shielding, or phone-free operation.

**Key competitive claims:**
- "50× more transcranial LEDs than Vielight at 17% of the price"
- "300 LEDs per wavelength — matching Neuronic's total LED count at each of the two CCO absorption peaks they don't cover"
- "Real-time J/cm² dose metering — the only device that shows you the exact dose your brain received"
- "Only consumer brain device with palladium-fabric EMF shielding verified by continuous fleet monitoring"
- "Autonomous closed-loop operation from any power bank — no phone required"
