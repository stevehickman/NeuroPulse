# Competitive Position

> Relocated from CLAUDE.md Rev 32 §9 to slim the always-loaded core. Authoritative content for competitive positioning and claims. Referenced from CLAUDE.md → Document Map.

| Feature | NeurOne Home | NeurOne Pro | Vielight Neuro Pro 2 (~$5K) | Neuronic 1070 ($3K–5K) | Sens.ai (~$1.5–2K + sub) | Neurode (~$999 + $29/mo) |
|---------|----------------|----------------|------------------------------|------------------------|--------------------------|--------------------------|
| PBM wavelengths | 660+810nm base / 660+810+1064nm with smart module upgrade (3λ) | 660+810+1064nm+1170nm (4λ) | 810nm (1λ) | 1070nm (1λ) | ~810nm (1λ) | None |
| Total LED count | **⚠ needs recompute** — was 600 (300/wavelength) fixed across 5 zone modules; that hardware model is retired (NP-HEX-ZM-001), total now scales with T1-A tiles populated per build, pending REG-1 tile-count lock | 600 + 1170nm LDs | ~12 transcranial | 256–300 (1 wavelength) | ~7 midline | None (no PBM) |
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

**Neurode gap note (2026-07-13):** Two Neurode capabilities have no NeurOne equivalent today — **fNIRS hemodynamic brain monitoring** and **tRNS**. Both are low-cost to close: fNIRS reuses NeurOne's existing NIR optics (see `docs/np_feas_fnirs_001.md`); tRNS is a firmware preset on the T2 arbitrary-waveform driver. Neurode is otherwise a single-modality ADHD/focus device with no PBM, EEG, EMF shielding, or phone-free operation.

**Key competitive claims:**
- ⚠ **"50× more transcranial LEDs than Vielight at 17% of the price" — STILL BLOCKED, but for a different reason now (2026-08-16, `NP-COST-001`).** The **price** half is sound and unaffected: retail is locked, and Home Standard's $849 against a ~$5K Vielight Neuro Pro 2 is 17%. The **LED-count** half is now *computable but assumption-dependent*: at `NP-COST-001` §2 A-1's 30-tile Home Standard (21 × T1-A + 9 × T1-B) and `NP-HW-HEXTILE-001` §4.2's per-tile emitter counts, the total is **~2,286 emitters — ~190× Vielight's ~12**, not 50×. **Do not publish that figure.** A-1 is an assumption, not a decision (**OI-COST-01**), and the socket count itself is PROVISIONAL pending REG-1/ACT-1. The claim unblocks when tile population is fixed, not before.
- ⚠ **"300 LEDs per wavelength — matching Neuronic's total LED count..." — STILL BLOCKED, same basis.** On the same A-1 assumption it would be **~1,143 per wavelength** against Neuronic's 256–300. Same caveat: assumption-dependent, PROVISIONAL, do not publish.

> **🛑 EVERY PRICE COMPARISON ON THIS PAGE IS NOW PROVISIONAL — retail pricing was UNLOCKED on
> 2026-08-16** (principal direction; `NP-COST-001` Rev 2 §8, CLAUDE.md Rev 39). **Do not publish,
> quote or brief any price comparison from this page until a new price is set.**
>
> **This note replaced an earlier one, and the reason matters.** The earlier version said these
> comparisons were *"factually correct and safe to use"* **because retail was locked** — the lock
> *was* the entire justification, and it is gone. That is exactly the kind of dependency that
> survives quietly after its premise is withdrawn, so it is stated here rather than deleted.
>
> **Why it moves so far.** `NP-COST-001` found all four T1 configurations gross-margin negative
> (Home Standard +36% → **−41% to −51%**). At the implied ladder, Home Standard needs
> **$1,869–1,997** against today's $849, and cannot be sold below **~$1,196** at any margin at all.
> Two comparisons on this page break at those numbers:
>
> | Claim | At $849 (in force) | At $1,869–1,997 (implied) |
> |---|---|---|
> | vs Vielight Neuro Pro 2 (~$5K) | 17% of the price | **~40% of the price** |
> | vs Sens.ai (~$1.5–2K) | undercuts by ~2× | **direct price peer — lands inside their band** |
>
> **Sens.ai is the one to watch.** It moves from a differently-priced adjacent product to a head-on
> competitor, and it already has EEG, PBM and closed-loop. The competitive frame on this page was
> built when that was not true.
>
> **Nothing here is rewritten**, because the comparisons cannot be recomputed until a price exists —
> and `OI-HEXTILE-06` must be decided before one is set (`OI-COST-10`), since silicon PD plus a
> 20-tile build moves the implied Home Standard price to **~$1,383**. Tracked as **OI-COST-09**.
- "Real-time J/cm² dose metering — the only device that shows you the exact dose your brain received"
- "Only consumer brain device with palladium-fabric EMF shielding verified by continuous fleet monitoring"
- "Autonomous closed-loop operation from any power bank — no phone required"
