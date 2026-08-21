# Competitive Position

> Relocated from CLAUDE.md Rev 32 §9 to slim the always-loaded core. Authoritative content for competitive positioning and claims. Referenced from CLAUDE.md → Document Map.

| Feature | NeurOne Home | NeurOne Pro | Vielight Neuro Pro 2 (~$5K) | Neuronic Neuradiant 1070 ($3K–5K) | Sens.ai (~$1.5–2K + sub) | Neurode (~$999 + $29/mo) |
|---------|----------------|----------------|------------------------------|------------------------|--------------------------|--------------------------|
| PBM wavelengths | 660+810nm base / 660+810+1064nm with smart module upgrade (3λ) | 660+810+1064nm+1170nm (4λ) | 810nm (1λ) | 1070nm (1λ) | ~810nm (1λ) | None |
| Total LED count | **⚠ needs recompute** — was 600 (300/wavelength) fixed across 5 zone modules; that hardware model is retired (NP-HEX-ZM-001), total now scales with T1-A tiles populated per build, pending REG-1 tile-count lock | 600 + 1170nm LDs | ~12 transcranial | **256** (1 wavelength) | ~7 midline | None (no PBM) |
| Peak irradiance | 400 mW/cm² pulsed* / 200 CW; **1064 ch. 28 mW/cm² (OI-HEXTILE-21)** | 400 + 1,000 mW/cm² | 400 mW/cm² | **20–40 mW/cm² declared; ~6 mW/cm² measured** (PBM Foundation / Optronic Labs 2024, −79%) | Not specified | N/A |
| Emitter areal density | **8.5 /cm²** (90 sites per 13.86 cm² tile, 3.80 mm pitch) | same | — | **~0.47 /cm²** (256 over ~500–650 cm², ~15 mm pitch) | — | N/A |
| Addressable granularity | **80 sockets / 18 clusters** | same | 3 heads | **4 quadrants** | — | N/A |
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
- 🛑 **"300 LEDs per wavelength — matching Neuronic's total LED count..." — DO NOT UNBLOCK. Retire the claim (2026-08-21).** It stays arithmetically blocked on the A-1 assumption (~1,143/wavelength against Neuronic's 256), but the analysis below establishes that **an emitter count is not a capability claim at all**, so unblocking it would publish a true number that supports a false inference. `NP-OPT-PSF-001` §3.3: the cortical resolution floor is **26.2 mm, ~65 % of the 40 mm tile pitch** — *"there is little spatial selectivity left to buy below module granularity."* **The dense lattice buys irradiance, not precision.** Replace with the irradiance and dose-verification claims below, which are defensible on the same evidence.

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

---

## Neuronic Neuradiant 1070 — what the difference actually is (2026-08-21)

> **Why this section exists.** "How does Neuronic populate the whole helmet when we cannot?" is asked
> often, and the intuitive answer — *they use fewer emitters* — is true but explains less than half of
> it. Getting this right matters in both directions: it retires one claim we were preparing to make
> (LED count) and supplies two better ones. Derivation of the power side: `NP-PWR-BUDGET-001` §3.4–§3.7.

**Sourcing caveat, stated first.** Neuronic figures below are from vendor listings and the
PBM Foundation / Optronic Labs (2024) third-party testing summary, republished by Vielight — an
interested competitor, though not the measuring party. **No Neuronic power-supply rating was
obtainable**; the ~54 W electrical figure is *derived* from our own 1064 nm emitter model applied to
256 emitters, not sourced. Treat every Neuronic number as third-party, and note the declared and
measured figures differ by −79 %, which is itself the most important fact on this page.

### The decomposition

Irradiance = emitter areal density × per-emitter optical flux. Both terms differ, roughly equally:

| Term | Neuronic | NeurOne T1-A | Ratio |
|---|---|---|---|
| Areal density (per channel) | ~0.47 /cm² | 4.24 /cm² | **~9×** |
| Per-emitter flux | ~13 mW (back-derived from measured) | 95 mW (design target, `NP-HW-HEXTILE-001` §4.3) | **~7×** |
| **Scalp irradiance** | **~6 mW/cm² measured** | **403 mW/cm² per channel** | **~65×** |

So: **~28× fewer emitters, each run ~7× softer.** Neither lever alone explains it. Note the split
depends on which Neuronic figure is true — at their *declared* 20–40 mW/cm², per-emitter output would
be 43–86 mW, essentially our design point, and the entire difference would be density.

### Why they can afford it: 1070 nm is too inefficient to create a thermal problem

At η_wp ≈ 4.8 % (`NP-HW-HEXTILE-001` §4.3, our own figure for 1064 nm), 256 emitters draw ~54 W electrical and emit ~3.3 W
optical. ~51 W of heat over ~550 cm² is **~0.09 W/cm² — below our own T1-*standard* worst-zone flux**
(0.10–0.15 W/cm², `NP-THERM-CFD-001` §4). Their whole helmet, all four quadrants, is thermally
quieter than one NeurOne zone at standard settings. **Our 660+808 combination extracts 6–10× more
optical output per watt, and that efficiency is exactly what walks us into the 42 °C wall.**

### The correction that matters: local irradiance ≠ total optical output

| | Total optical output |
|---|---|
| NeurOne, whole PBM budget (~40 W × ~34 % η_wp) | **~13–14 W** |
| Neuronic, measured | 3.3 W |
| Neuronic, **declared** | **11–22 W** |

**Total optical output is capped by the USB-C PD envelope, not by emitter count** — adding sockets
redistributes it, never enlarges it (`NP-PWR-BUDGET-001` §3.7). At declared spec Neuronic's total
output is comparable to our *entire* PBM budget. For a **whole-head dose target** our advantage over
their declared spec is close to nil (~21 min vs 8–17 min to 20 J/cm², since they irradiate 550 cm²
at once and we can light six tiles). For a **focal high-irradiance protocol** it is decisive.

**One operating point versus a range.** Neuronic spreads a fixed budget thin across the whole vault.
NeurOne can concentrate it (200–400 mW/cm² focal) *or* spread it (~30 W whole-vault, `NP-PWR-BUDGET-001`
§3.6) — **but not both at once.** The benefit is optionality and access to the top of the band, not
raw quantity.

### Against the evidence base — the yardstick that decides it

`docs/pbm_neuro_protocols.md` MASTER SUMMARY: positive trials cluster at **0.02–0.3 W/cm², 10–120
J/cm², 6–30 min**, and dosimetry lesson 1 is *"under-dosing, not mechanism failure, explains most
nulls."*

| | Irradiance vs band | Dose in a 20-min session |
|---|---|---|
| Neuronic, **measured** | **below the 0.02 W/cm² floor** | **7.2 J/cm²** — under the ≥10 J/cm² threshold |
| Neuronic, **declared** | bottom of band | 24–48 J/cm² — in band |
| NeurOne 660/808 | **spans to the top of the band** | 120–240 J/cm² |
| NeurOne 1064 | bottom of band | 33 J/cm² — but see `OI-HEXTILE-21` |

Two further asymmetries the table does not show:

1. **Neuronic has no 810 nm channel at all.** 808–830 nm carries the most trial support in the whole
   database — Grade A/B across AD, MCI, depression, TBI, stroke. **Most Grade A/B protocols are not
   runnable on 1070 nm hardware at any irradiance.** This is a capability gap, not a spec gap, and it
   is the strongest single point in our favour.
2. **Session length is the price of under-irradiance.** 60 J/cm² at 6 mW/cm² is **167 minutes**; at
   200 mW/cm² it is 5. That is a product-viability constraint, not a nicety.

### Three things this does NOT support — read before briefing

- **Not spatial precision.** See the retired LED-count claim above; ~26 mm cortical floor.
- **Not whole-head simultaneity.** Grade A Alzheimer's specifies *whole-head*, and cross-cutting
  principle 4 is *"target the network, not one spot."* Neuronic's sparse topology natively matches
  that geometry; ours must sequence six tiles or drop to their irradiance. **On the single strongest
  indication in the database their topology is the better fit — it is simply too dim.**
- **Not our own 1064 nm channel.** `NP-HW-HEXTILE-001` §4.3.2 gives 28 mW/cm² against the Grade A cognitive protocol's
  0.25 W/cm² — **9× short**, on the same η_wp ≈ 4.8 % wall that bounds Neuronic. `OI-HEXTILE-21`.
  **Do not make comparative 1064 nm claims until that item resolves.**

### The two claims this analysis does support

1. **Dose verification.** The entire Neuronic story is a **−79 % declared-to-measured gap**. Dual-PD
   metering (R-8) is the direct structural answer to it. *"The only device that measures the dose it
   delivered rather than declaring it"* is defensible today and is strengthened, not weakened, by a
   competitor's third-party measurement.
2. **Wavelength coverage.** 660 + 808 nm against 1070 nm only — access to the most evidence-backed
   wavelength in the database.

> **⚠ Consequence for `OI-HEXTILE-06`.** Options 2 and 3 (silicon PD; per-cluster PD) trade away the
> dose metering that claim 1 rests on. **Dropping it to save ~$9/tile makes us a better-specified
> Neuronic rather than a differently-verified one** — an argument the BOM model in `NP-COST-001` §6
> cannot see, now recorded at `NP-HW-HEXTILE-001` §6.4 and against the open item itself.

**Sources (retrieved 2026-08-21; vendor pages could not be fetched directly from the build
environment — figures are from search-result summaries and should be re-verified before publication):**
bio-medical.com Neuradiant listing · rehabmart.com Neuradiant listing · lighttherapyinsiders.com
Neuradiant review · pbmfoundation.org PBM_Testing_CaseStudy_1.pdf (Optronic Labs, 2024) ·
vielight.com transcranial-irradiance lab post · neuronic.com product page · Neuradiant 1070 user manual.

---

- "Real-time J/cm² dose metering — the only device that shows you the exact dose your brain received"
- "Only consumer brain device with palladium-fabric EMF shielding verified by continuous fleet monitoring"
- "Autonomous closed-loop operation from any power bank — no phone required"
