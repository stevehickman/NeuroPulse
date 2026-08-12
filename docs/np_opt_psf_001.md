# NP-OPT-PSF-001 — Transcranial PBM Optical Point-Spread Function and Spatial Resolution Floor

**Project:** NeurOne
**Document:** NP-OPT-PSF-001
**Revision:** 1
**Date:** 2026-07-20
**Status:** ACTIVE — modelling result, not a measurement
**Author:** NeurOne Systems
**References:** NP-HEX-ZM-001 Rev 1 §3 (module geometry); `docs/pbm_neuro_protocols.md` §3; `docs/np_bib_1064_001.md`; `protocols/predefined/00-zones.npps`; PR #210 (zone membership ruling)
**Model source:** `scripts/pbm-optical-psf.ts` (re-runnable)
**Gate:** —
**IEC 62304 Class:** — (analysis tool, not device software)

---

## 1. Why this document exists

PR #210 locked the inclusive zone-membership rule (a midline socket belongs to both hemisphere
zones of its lobe) and estimated, **from geometry alone**, that a midline module puts "roughly
half its emission contralateral", making ~14% of `Frontal Right`'s delivered energy land on left
prefrontal cortex. That estimate assumed light travels straight down. It does not.

Two consequences were then *inferred*, and recorded in PR #210 explicitly as inference rather
than measurement:

1. Partial-module masking (option (c), minor-address subsetting) would be largely ineffective for
   PBM because tissue diffusion blurs element-level boundaries.
2. A narrower zone (option (b)) would be *only partially* effective for the same reason, so
   expectations of what it buys should be tempered.

A grep of `docs/pbm_neuro_protocols.md` and `docs/np_hex_zm_001.md` for scatter, diffusion, spot
size, beam, or penetration data returned nothing usable. The repo had no optical spatial model at
all. This document supplies one, and the results **confirm inference (1) and refute inference (2)**.

## 2. Method

`scripts/pbm-optical-psf.ts` implements weighted-photon Monte Carlo transport in the MCML
formulation (Wang, Jacques & Zheng 1995) through a plane-parallel four-layer head, scoring
fluence in cylindrical (r, z) bins for a pencil beam at the origin. Because that geometry is
radially symmetric, the scored field **is** a point-spread function: the response to any extended
source is the source convolved with the PSF. A module's footprint is obtained by convolving the
PSF with a uniform regular-hexagon aperture of flat-to-flat width W.

Two exact reductions are used:

- **Line profile** through the module centre → FWHM and 10–90% edge transition width.
- **1-D marginal** (integrated over y) → the left/right energy split. For a radially symmetric
  kernel this depends only on the module's distance from the midline, so a socket's row position
  drops out entirely. That is why the zone results below need only each socket's x offset.

### 2.1 Tissue model (1064 nm)

| Layer | Thickness | μa (mm⁻¹) | μs′ (mm⁻¹) | g | n |
|---|---|---|---|---|---|
| Scalp | 6 mm | 0.015 | 0.65 | 0.9 | 1.4 |
| Skull | 7 mm | 0.012 | 0.75 | 0.9 | 1.4 |
| CSF | 2 mm | 0.0144 | 0.03 | 0.9 | 1.4 |
| Gray matter | 45 mm | 0.020 | 0.75 | 0.9 | 1.4 |

CSF μa is **floored at the pure-water value at 1064 nm** (0.14 cm⁻¹ = 0.0144 mm⁻¹). CSF is
>98% water and cannot absorb less than water does. A first draft of this table carried 0.003,
which is the water figure at ~800 nm — a wavelength error caught in review. Its numerical effect
was small (2 mm layer), but it is recorded here because it means the table must be checked *at
wavelength* rather than inherited from an 800 nm compilation.

Gray-matter surface therefore sits at **15 mm** below the scalp. Values are mid-range literature
figures for 1064 nm (skin and cranial bone: Bashkatov et al. 2005; brain: Yaroslavsky et al.
2002; CSF: Custo et al. 2006). 1064 nm sits in the low-absorption window — less haemoglobin
absorption than 810 nm, water absorption still small, and μs′ falls roughly as λ⁻ᵇ, so μs′ here is
below the more commonly quoted 800 nm values.

### 2.2 Convergence, self-test, and sensitivity

**Recorded run: 5×10⁵ photons per scenario** (the script's default is 2×10⁶; every figure below
is reproducible at 3×10⁵). Two independent seeds agree to 0.1 mm on every width and 0.1
percentage point on every energy fraction — but that agreement is close to vacuous on its own,
because these quantities are dominated by the deterministic aperture convolution rather than by
Monte Carlo variance. Seed agreement is reported for completeness, not as the accuracy argument.

**The accuracy argument is an exact self-test.** A symmetric aperture centred on the midline must
split its energy exactly 50/50, because the PSF is radially symmetric and the hexagon is even in
x. The script asserts this on every run and fails the process if it deviates by more than
0.05 pp. It currently returns **50.000%**. An earlier revision returned 49.7%, which review
traced to two numerical defects in the reductions — a biased cache key that translated the whole
profile by +0.125 mm, and a left-Riemann bin split that gave the midline-straddling bin wholesale
to one side. Both are fixed; the assertion is what would catch a recurrence.

**Sensitivity.** Four axes are swept automatically: seed, μs′ ±30%, and CSF thickness 1 mm / 4 mm.
The CSF sweep matters most and was added after review pointed out that the original ±30% μs′
sweep was probing the *insensitive* axis:

| Scenario | Resolution floor (§3) | `Frontal Right` contralateral | narrowed |
|---|---|---|---|
| nominal | 26.2 mm | 16.3% | 3.6% |
| μs′ −30% | 28.1 mm | 16.5% | 3.9% |
| μs′ +30% | 24.9 mm | 16.1% | 3.4% |
| **CSF 1 mm** | **24.0 mm** | 15.9% | 3.2% |
| **CSF 4 mm** | **30.5 mm** | 16.9% | 4.5% |

Sub-arachnoid CSF is a low-scattering light pipe (μs′ = 0.03) and varies 1–5 mm across subjects
and sulcal position, so it moves the answer about twice as much as a ±30% change in scattering
does. The lateral result *is* robust to μs′ — but for a reason worth stating correctly: not
because μs′ is well known (it is not; literature compilations differ by ~2× at 1064 nm), but
because the aperture is much wider than the transport mean free path, so aperture geometry
dominates. Doubling μs′ globally moves the module edge width by under 1 mm.

### 2.3 Limits — read before citing

- **Plane-parallel, not curved.** The vault has R_s ≈ 65–130 mm (NP-HEX-ZM-001 §3). Over the
  ±40 mm that dominates these answers the slab is a local approximation; curvature makes true
  lateral spread slightly *wider* in arc-length terms, so the contralateral fractions here are
  mild under-estimates.
- **Literature optical properties, not measured on NeurOne hardware.** Absolute depth-dose from
  this model should be treated with caution. The *lateral* results are far more robust, because
  lateral spread is governed by μs′ and the transport mean free path rather than by absolute
  attenuation.
- **Socket pitch is taken as equal to module flat-to-flat width**, per the row structure in
  `scripts/sync-socket-map.ts`. Socket x/y remain PROVISIONAL until replaced from shell CAD.
- **This is a model, not a phantom measurement.** It is the first quantitative bound the repo has,
  and it should be checked against a tissue phantom before any external claim rests on it.

### 2.4 Module width note

PR #210 framed the question around a "30–34 mm hex". NP-HEX-ZM-001 §3 gives the **design point as
40 mm** flat-to-flat, with 34–46 mm workable and 38–42 mm ideal. Both 34 mm and 40 mm are modelled
below; the conclusions are the same for both.

## 3. Result 1 — the spatial resolution floor

### 3.1 The right statistic

The obvious thing to measure is the 10–90% edge width of one module's own profile. **That is the
wrong number for a "floor",** and getting this wrong is easy enough that it is worth spelling out.

A module's edge width is contaminated by its own aperture two ways: a 40 mm hexagon is too narrow
for its plateau to saturate (the profile is already down to ~88% at 10 mm off-centre), which
depresses the normalising peak, and the *opposite* edge's tail lifts the 10% level. Both compress
the measured width and make the boundary look sharper than the tissue permits. Measured that way a
40 mm module gives 22.8 mm — but a 120 mm aperture, whose plateau does saturate, gives 26.2 mm.
The number moves with the aperture, so it is not a property of the tissue.

The aperture-independent quantity is the **edge-spread function** (ESF) — the response to a
half-plane source, equal to the cumulative of the line-spread function and computed exactly with
no convolution. That is a genuine floor for any non-negative source distribution.

### 3.2 The numbers

| Quantity (at cortex, 15 mm) | Value |
|---|---|
| **Resolution floor — ESF 10–90%** | **26.2 mm** (24.0–30.5 across the sensitivity sweep) |
| Resolution floor — ESF 25–75% | 12.9 mm (11.9–14.7) |
| One 40 mm module's own 10–90% edge | 22.8 mm *(aperture-limited — not a floor)* |
| One 34 mm module's own 10–90% edge | 21.6 mm *(aperture-limited — not a floor)* |

| Depth | W = 34 mm | W = 40 mm |
|---|---|---|
| **FWHM** at scalp surface | 33.8 mm | 39.8 mm |
| **FWHM** at cortex (15 mm) | 34.7 mm | 40.0 mm |
| **FWHM** at cortex + 5 mm | 35.4 mm | 40.4 mm |
| **10–90% edge width** at scalp surface | 7.5 mm | 7.7 mm |
| **10–90% edge width** at cortex (15 mm) | 21.6 mm | 22.8 mm |
| **10–90% edge width** at cortex + 5 mm | 23.2 mm | 24.5 mm |

> **The tissue cannot produce a cortical boundary sharper than ~26 mm (10–90%), regardless of
> aperture shape, masking, or addressing scheme.** The blur is imposed after the light leaves the
> device, so no amount of device-side cleverness sharpens it.

Two honest caveats on that headline. First, the kernel is heavy-tailed, so a single-number "floor"
is a poor descriptor: the same kernel gives 26.2 mm at 10–90% but 12.9 mm at 25–75%. Quote the
criterion with the number. Second, the floor is the *most* CSF-sensitive quantity in this document
(24.0–30.5 mm), so treat ~26 mm as a central estimate with a real ±20% spread across subjects.

**Do not read the FWHM row as a finding about tissue.** FWHM ≈ aperture width is a mathematical
identity, not a physical result: for any normalised symmetric kernel convolved with a top-hat, the
profile at the aperture edge is 50% of peak by construction, so FWHM tracks W for any kernel at
all. It is reported only to show that the plateau is intact; the tissue is doing no work in it.
The physically meaningful change from scalp to cortex is in the **edge**, not the width.

### 3.3 Why this matters for the module pitch

The floor (~26 mm) is roughly **65% of the module pitch** (40 mm). The addressing granularity
NeurOne chose for mechanical and BOM reasons therefore already sits near the optical resolution
limit at cortical depth. That is the whole answer to the partial-module question in §4.1: there is
little spatial selectivity left to buy below module granularity.

## 4. Result 2 — lateralization

Fraction of a module's **cortical** energy landing contralateral (x < 0), by module centre offset
from the midline, in units of module pitch:

| Offset | W = 34 mm | W = 40 mm |
|---|---|---|
| 0 (midline socket) | 50.0% | 50.0% |
| 0.5 pitch | 11.0% | 9.2% |
| 1.0 pitch | 1.0% | 0.6% |
| 1.5 pitch | 0.1% | <0.05% |
| 2.0 pitch | <0.05% | <0.05% |

The 0.5-pitch row is where the retained-socket spill lives, and it is the one number that
vindicates PR #210's instinct that diffusion matters: a module centred half a pitch off the
midline has its geometric edge *exactly on* the midline, so pure geometry would say 0%
contralateral. Diffusion makes it 9.2%. But by one full pitch that has already collapsed to 0.6%.
The midline row is exactly 50.0% by symmetry — that is the model's self-test (§2.2), not a
measurement.

Applied to the `Frontal Right` socket list, weighting every module equally:

| Zone | Sockets | Contralateral fraction at cortex |
|---|---|---|
| `Frontal Right` (inclusive, as shipped) | 11: `1, 3, 5, 6, 9, 10, 13, 14, 15, 18, 19` | **16.3%** (W=40) / 16.9% (W=34) |
| `Frontal Right (excl. midline)` | 8: `3, 6, 9, 10, 14, 15, 18, 19` | **3.6%** (W=40) / 4.4% (W=34) |
| **Reduction delivered by narrowing** | — | **12.7 pp absolute — 78% relative** (W=34: 12.5 pp, 74%) |

Across the full sensitivity sweep the inclusive figure stays in 15.9–16.9% and the narrowed figure
in 3.2–4.5%, so the relative reduction is 74–80% on every scenario run.

### 4.1 What this does to PR #210's two inferences

**PR #210's midline geometry was right.** A midline module really does put half its cortical
energy contralateral — exactly 50%, as symmetry requires. The zone-level estimate of ~14% was
slightly low: the true figure is 16.3%, because the retained right-hand sockets also spill
medially. PR #210 reasoned "3 of 11 modules × ~50%" = 13.6%; the model adds 3 × 9.2% from the
three half-pitch sockets and 3 × 0.6% from the full-pitch ones, giving 16.3%. Right order of
magnitude, and the gap is precisely the diffusion term the geometric argument omitted.

**Inference (2) is REFUTED.** A narrower zone is *not* "only partially effective". Narrowing
removes 100% of each omitted module's contralateral contribution, and the medial spill from the
retained sockets is small (3.6%) because contralateral fraction collapses fast with offset — 50%
at the midline, 0.6% at one full pitch away. Option (b) buys **78% of the available reduction**.
The expectation should be raised, not tempered.

**Inference (1) is CONFIRMED, with a mechanism.** Modelling an aperture that lights only the
ipsilateral half of a midline module — exactly what option (c) minor-address subsetting would
enable — gives:

| Treatment of a midline module | Contralateral fraction at cortex |
|---|---|
| Lit whole | 50.0% |
| **Half-masked (option (c))** | **22.1%** (W=40) / 24.7% (W=34) |
| Omitted from the zone (option (b)) | 0% |

Half-masking removes **56%** of that module's contralateral leak. That is not nothing — the
optics does not make masking *useless*, and it would be overclaiming to say a 26 mm blur means a
20 mm mask feature does nothing. What the optics establishes is the ceiling: even a perfectly
implemented half-module mask leaves 22% spilling contralateral, because the mask feature (20 mm)
is smaller than the boundary the tissue can render (~26 mm 10–90%), so most of the masked half's
territory gets re-illuminated by scatter from the lit half.

The decisive comparison is therefore not masking-versus-nothing but **masking versus the mechanism
that already exists**:

| | Contralateral leak | Cost |
|---|---|---|
| Option (c) — half-mask the midline module | 22.1% | new firmware capability, new addressing surface, new validation burden, fresh owner ruling |
| Option (b) — omit it from the zone | **0%** | one zone definition, zero firmware |

> **Partial-module inclusion is strictly dominated as a lateralization tool** — worse outcome at
> higher cost. It should not be revisited for this purpose. If it is ever proposed again it must
> be justified by something other than spatial selectivity at cortical depth, because on that
> axis the free option wins outright.

The honest framing of the optics' contribution: it does not by itself prove option (c) worthless,
it proves option (c) *cannot reach zero* — and since option (b) reaches zero for free, that is
enough to close the question.

## 4.2 Incidental finding — coupling loss

The model reports **48.8% diffuse reflectance**, plus 2.8% specular reflection at the air/scalp
interface. **Roughly half the emitted optical power never enters the tissue at all.** That is
expected for head tissue at 1064 nm with an n = 1.4 index mismatch and is not evidence of a bug —
but it is only self-consistent with the chosen μs′/μa, so it validates nothing by itself.

It is flagged here because it is a 2× factor: any dose calculation elsewhere in the repo that
treats emitted power as delivered power is wrong by that factor. This model does not audit those
calculations. Worth a separate check against NP-SES-1064-001's dose accounting.

## 5. Reproducing

```bash
bun scripts/pbm-optical-psf.ts                    # 2e6 photons, ~2 min
bun scripts/pbm-optical-psf.ts --photons 1e6 --json psf.json
```

The script prints all four scenarios (nominal, second seed, μs′ −30%, μs′ +30%) and writes the
full result set as JSON with `--json`.

## 6. Follow-ups

| Item | Why | Owner |
|---|---|---|
| Tissue-phantom validation of the ~26 mm ESF figure | This is a model; no external or clinical claim should rest on it unmeasured | Optics |
| **Re-derive the whole optical table at 1064 nm from primary sources** | The CSF μa error (§2.1) shows the table was assembled partly from 800 nm compilations. Gray-matter μs′ at 1064 nm is the weakest remaining entry. | Optics |
| **Sweep skull and scalp thickness as well as CSF** | CSF thickness already moves the floor ±12%; the other layer thicknesses are untested and vary as much between subjects | Optics |
| Curved-geometry MC once shell CAD lands | Plane-parallel under-estimates lateral spread on a curved vault | Optics |
| Re-run at 660 nm and 810 nm | Resolution floor differs by wavelength; the three-tier penetration stack claim spans all three | Optics |
| Measure NeurOne module μs′/μa through the PDMS window | Literature values are not hardware values | Optics + ME |
| **Audit dose accounting against the ~51% coupling loss (§4.2)** | Any calculation treating emitted power as delivered power is 2× out | Optics + FW |

## 7. Cross-references

- `docs/np_hex_zm_001.md` §3 — module geometry, W design point, curvature
- `docs/pbm_neuro_protocols.md` §3 — the 1064 nm cognitive protocol this was raised against
- `docs/np_bib_1064_001.md` — 1064 nm evidence, emitter count, duty-cycle irradiance
- `docs/status/completed-decisions.md` — the clinical-03 zone decision this informed
- `protocols/predefined/00-zones.npps` — zone definitions
- `scripts/sync-socket-map.ts` — socket row structure and the lateralized-protocol audit
