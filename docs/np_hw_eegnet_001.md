# EEG Electrode Net — Architecture, Sizing, and Interference Control

**Project:** NeurOne
**Document:** NP-HW-EEGNET-001
**Revision:** 1
**Date:** 2026-08-18
**Status:** DRAFT
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (new document)
**References:** NP-HEX-ZM-001 §3.1/§3.2/§3.3/§4a/§5.3, NP-HELMET-GEOM-001 §0/§2/§3.1/§5, NP-DRV-SHELL-002 §3.5/§5.1/§9.1–§9.6/§10.1, NP-HW-HUB-001 §4.5/§7.4, NP-HW-HEXTILE-001 §1/§6.4/§7.2, NP-THERM-BEZEL-001, NP-RISK-002 (RISK-21), NP-RISK-004, NP-COST-001 §2/§6, NP-HFE-002 §5, NP-OPT-PSF-001, NP-ENV-OPRANGE-001 §4, NP-CONV-001 Rev 6, CLAUDE.md §3/§4.2/§4.3/§4.4/§5.1
**Related Issues:** —
**Gate:** NET-1 (strain-tracking fidelity), NET-2 (placement-verification qualification); interacts with REG-1, THERM-1, SEAL-1
**IEC 62304 Class:** N/A — the net is hardware. It is a risk control with no software class; the firmware that reads its impedance matrix and gates tES/visual stimulation carries the class (Class C where it owns an enable line, per CLAUDE.md §4.2).
**Supersedes:** None. Contests NP-HEX-ZM-001 §4a's T1-B tile type; see §0 and §7.
**Parent Document:** NP-HEX-ZM-001

---

> **⚠ Read §0 before anything else, and do not read this document as a recommendation to adopt.**
> It specifies a net because the request was to specify one, and the net does fix something no
> parameter change can fix. It also makes four things worse, one of them a safety-gate determinism
> problem and one of them a BOM regression on the exact term `NP-COST-001` already calls the largest
> uncosted risk. Both halves are in §7. **Nothing in this document should be quoted as a cost saving.**

---

## 0. The decision that has to come first — T1-B's electrode is dual-rated

The T1-B tile's Ag/AgCl contact is typed `NP_ELEM_DUAL_ELECTRODE`. It **records EEG and delivers
BES / tACS / tDCS from the same contact** (`NP-HEX-ZM-001` §4a; `NP-DRV-SHELL-002` §5.1.4 pin 13
`ELEC`, "dual-rated: EEG record **and** tES drive"). "Move the EEG electrodes to a net" is therefore
not one change. It is three mutually exclusive ones, and they have very different value:

| Option | What it means | Consequence |
|---|---|---|
| **(a)** Net carries EEG only; T1-B survives as a tES tile | Two electrode systems on one head | **Delivers almost none of the benefit.** The socket keeps pins 13/14/15, network N4 stays, `REG-1` stays (tES placement is a 10-20 problem too), the spring pod stays, `OI-HEXTILE-05` stays. Pure additive cost. |
| **(b)** Net carries EEG **and** tES ★ | One electrode system, off-lattice | **The only variant with leverage.** Everything in §7.1 follows from this and only this. Also the only variant that re-homes two safety interlocks onto a soft part (§7.2.4). |
| **(c)** Net carries EEG; tES deleted from T1 | Product change | CLAUDE.md §3 modalities 4 and 5 are locked. Out of scope for an engineering document. |

**This document specifies option (b).** Every claim below is conditional on it. If the principal
selects (a), most of §7.1 evaporates and the net becomes a cost line with a registration benefit
confined to recording.

---

## 1. The problem is not pod travel — and this matters, because travel is what everyone is trying to fix

The request framed the failure as insufficient pod travel. That is **right about the conclusion and
wrong about the mechanism**, and the correction is load-bearing rather than pedantic: it is the
reason no amount of tuning inside the tiled architecture can succeed.

### 1.1 There are two errors, not one

**Radial (the axis travel actually addresses).** The one adult SKU covers 520–620 mm circumference
(CLAUDE.md §4.4; `NP-HEX-ZM-001` §3.3 `HEAD_CIRCUMFERENCE_MM = 620`). Mean radius runs 82.8 → 98.7 mm,
a delta of **15.9 mm**. Spring-decoupled pods travel **±12 mm** (CLAUDE.md §4.4). If nominal is flush
to the 620 mm datum — which it must be, because the bowl is cut to the largest skull — the smallest
head demands ~15.9 mm of inward travel and receives 12.

> **Radial margin ≈ −4 mm, before any cephalic-index or local-curvature deviation is added.**
> Marginal, and in principle fixable by more travel.

**Tangential (the axis travel cannot address at all).** A 10-20 site sits at a fixed *fraction* of
the wearer's own nasion→inion arc — the 10-20 system is defined that way, and `NP-HEX-ZM-001` §3.1
says so explicitly, calling the arc *"the 10-20 system's own longitudinal ruler."* A socket sits at a
fixed *position* on the helmet's arc. The 62 cm arc is **331 mm** (§3.1); under geometric similarity
the 52 cm arc is 331 × 520/620 = **277.6 mm**. At arc fraction *f* the site and the socket diverge by
*f* × 53.4 mm:

| 10-20 line | *f* | Divergence, nasion-datum | Divergence, Cz-datum |
|---|---|---|---|
| Fp | 0.10 | 5.3 mm | 21.4 mm |
| F | 0.31 | 16.5 mm | 10.1 mm |
| **C (Cz)** | 0.52 | **27.8 mm** | 1.1 mm |
| P | 0.73 | 39.0 mm | 12.3 mm |
| **O (Oz)** | 0.94 | **50.2 mm** | 23.5 mm |

Re-datuming to Cz — the obvious mitigation — halves the worst case to ~23.5 mm and does not come
close to rescuing it.

### 1.2 Why travel cannot help

**Pod travel is a radial degree of freedom.** A tangential error is displacement along the scalp
surface. A radial actuator's contribution to a tangential error is exactly zero, at any travel
value. A pod with infinite travel still presses an electrode firmly onto the wrong piece of cortex.

This is why the tolerance framing in `REG-1` — *"registers to 10-20 … within tolerance,"* with the
±12 mm pod travel cited in its support (`NP-HEX-ZM-001` §7) — cannot be satisfied by tightening
anything. The lattice is one rigid geometry; 10-20 is a one-parameter family of geometries indexed
by head size. No single member of a rigid family matches a scaling family at more than one point.

### 1.3 The document set already records the symptoms

Two entries read as tuning problems and are actually the premise showing through:

- **Fp1 and Fp2 cannot each have a socket.** The Fp row is 72 mm of arc wide and holds one 40 mm
  tile, so socket 1 straddles the midline (`NP-HEX-ZM-001` §3.2). More generally, the row pitch is
  **34.6 mm** and 10-20 lines are ~**33 mm** apart, so *"the lattice registers to alternate lines at
  best."* **No pitch inside the 34–46 mm workable window resolves adjacent 10-20 lines.** This is a
  defect the architecture generates and has no parameter to correct.
- **Oz is ~18 mm anterior.** Socket 74 is the nearest midline occipital address, at 86.2 % of arc
  against a true O line at ~94 % (§3.2 caveat). The photoparoxysmal halt depends on that site.

**A conformable net repairs both by construction, because its electrode positions are fractions of
its own geodesics rather than positions on a bowl.** That is the single unique benefit of this
change, and §7 argues it is enough to justify the costs. It is not, on its own, obviously enough —
that is a principal call.

---

## 2. Net architecture

### 2.1 Topology — equal-tension geodesic strut network (lead)

The net is a tessellated network of **equal-length, equal-tension elastic struts** with electrode
pucks at vertices, not a fabric cap with electrodes sewn on.

**Mechanism, stated as a mechanism rather than a claim.** If every strut carries the same tension
and every strut has the same unstretched length, every strut experiences the same strain. Uniform
strain is a similarity transform: all geodesic distances scale by a common factor λ, and any point
defined as a *fraction* of a geodesic path stays at that fraction. Placement is then **scale-
invariant by construction** rather than correct-within-tolerance at one size.

Real nets deviate from this. The deviation is quantified as η in §3 and is the whole sizing question.

**Fallback: low-stretch fabric in discrete sizes** (the `actiCAP` pattern, ~2 cm circumference per
size). Cheaper to make, worse per-size coverage — a 520–620 mm range at 2 cm/size is 5–6 SKUs, with
5–6 sets of inventory, tooling, and consumable-fit permutations. Named so the trade is on the record.

### 2.2 Registration to the head — three landmark anchors

The net registers to **anatomy**, not to the helmet:

| Anchor | Landmark | Why it is usable by an untrained wearer |
|---|---|---|
| A1 | Nasion | Palpable depression; the standard 10-20 origin |
| A2 | Inion | Palpable occipital protuberance |
| A3/A4 | Left / right preauricular points | Palpable notch anterior to the tragus |

The wearer seats four features they can feel. Everything else follows from strut geometry. This is
the same registration procedure a clinical EEG technician performs, reduced to four tactile targets.

**Landmark identification error is a real budget line, not zero** — allocated 4 mm in §3.4.

### 2.3 Contact force lives in the puck, not in net tension

Contact force is specified unchanged at **80–120 g** (CLAUDE.md §4.4), but it is produced by a
**compliant element inside each puck** (Shore 30A silicone dome, ~3–5 mm working travel), not by
global net tension.

**Why local rather than global.** Global tension couples contact force to size — a large head in a
small net gets more force everywhere. Local springs decouple them, so net tension can be chosen for
comfort and placement fidelity alone, which are the two things it must serve.

> **Rejected alternative, recorded because it was the first idea and it is wrong.** Let the net
> position the electrode and let the helmet's existing calibrated spring pod press through the net
> onto the puck's back face. This reuses a designed, specified spring. It fails because the pod
> plunger must then *find* the puck, and the puck's tangential position is precisely the quantity
> that varies by up to 27.8 mm. The proposal reintroduces the registration problem the net exists to
> delete. Contact force moves into the puck.

### 2.4 Radial stack budget

| Element | Nominal | Note |
|---|---|---|
| Net strut, crossing a tile aperture | ≤0.5 mm | Occlusion and thermal budget, §5 |
| Puck body, proud of the strut plane | 5.0 mm | Uncompressed |
| Puck working travel | 3.0–5.0 mm | Delivers 80–120 g |
| Puck hard stop | at 5.0 mm compression | Prevents the helmet bottoming a puck on the largest head |
| Optical-module bezel (unchanged) | 1.0 mm | `NP-THERM-BEZEL-001` |

**Required CAD check (`NET-CAD-01`):** ≥7.0 mm clearance between the L0 module face plane and the
scalp at every candidate electrode site, for the 620 mm head. `NP-HELMET-GEOM-001` §5 states modules
"normally make no structural contact with anything inboard," so clearance likely exists — but it has
never been checked *at electrode sites for the largest head*, which is the only case that binds.

### 2.5 Materials

| Element | Lead | Reason | Fallback / risk |
|---|---|---|---|
| Struts crossing a tile aperture | **PTFE or FEP monofilament**, undyed | **No C–H bonds**, so no C–H overtone absorption anywhere in 660–1170 nm; hydrophobic; non-magnetic; ISO 10993 precedent as surgical mesh; low friction on scalp | Undyed PET — cheaper, but carries a C–H second-overtone band near 1150–1200 nm, i.e. **directly on the 1170 nm T2 laser line**. Must be measured, not assumed (§5.2). |
| Elastic take-up | LSR silicone elements, sited off the optical path where routing allows | Elastic recovery; fluoropolymers creep | Silicone has C–H bands; keeping it off apertures is the mitigation |
| Puck body | PTFE dome over PEEK core | PTFE is the best diffuse NIR reflector available (~98 %), which the puck needs anyway for §4 | — |
| Electrode | **Sintered Ag/AgCl, unchanged**, with the existing snap-off bayonet hydrogel tip | The CLAUDE.md §2.3 consumable model (electrode tips, 60–72 % GM) must survive untouched, and does | — |
| Recording conductor | Carbon-loaded resistive polymer filament | Eddy-current suppression (§5.4), lead-current limiting (§6) | — |
| Stimulation conductor (T2 only) | Fine nickel-free Cu, discrete film resistor at the puck | Must carry mA | — |
| Guard | Carbon-loaded polymer sheath, DRL-driven | Guards by potential, not by conductivity (§6.5) | — |

**Hydrophobicity does double duty** — it is the NIR-transparency choice *and* the wet-shunt
suppression choice (§5.5). That coincidence is why the fluoropolymer lead is strong.

### 2.6 Service and life

User-removable and washable without tools. **Creep/compression set is budgeted at 3 mm** of
equivalent placement drift over service life (§3.4) and is a stated qualification item, not an
assumption — fluoropolymers in particular creep, which is exactly why the *geometry-defining* struts
and the *elastic take-up* are different materials (§2.5).

---

## 3. Sizing — and why "how stretchable" is the wrong question

### 3.1 The model

For a net of nominal circumference C₀ worn on a head of circumference C_h, with λ = C_h/C₀:

> **e(f) = η · f · L · |λ − 1|**

| Term | Meaning |
|---|---|
| **e** | placement error at the site, mm |
| **η** | **strain-tracking fidelity** — the *fraction of geodesic displacement that fails to track*. Dimensionless, 0 = perfect similarity transform |
| *f* | the site's arc fraction (0.94 at Oz — the worst case, so *f* = 0.94 governs) |
| *L* | nasion→inion arc at nominal, 331 mm at 620 mm (`NP-HEX-ZM-001` §3.1) |
| λ − 1 | the strain the net is being asked to absorb |

### 3.2 The counterintuitive consequence

η and |λ − 1| **multiply**. So:

> **A very stretchy net with poor fidelity needs MORE sizes than a modestly stretchy net with good
> fidelity.** Stretchiness buys accommodation range; fidelity is what determines whether the
> accommodation lands the electrodes anywhere useful. They are different properties, and only the
> second is a placement property.

η is a **topology** property far more than a material property — which is why §2.1 specifies a strut
network first and a material second. A plain elastic cap concentrates strain near its loaded rim and
leaves the unloaded crown nearly unstrained; that is the physical content of a large η, and it is
why elastic caps drift at the vertex.

### 3.3 Size count is an output of a measurement

Nets are worn **in tension only** (λ ≥ 1, nominal at the smallest head in the band) — a slack net
slips, and a slipped net has unbounded error. Allocating 5 mm of the error budget to strain (§3.4),
the per-size stretch a band may absorb is Δ = 5/(0.94 · 331 · η), and

> **N = ⌈ ln(620/520) / ln(1 + Δ) ⌉ = ⌈ 0.1759 / ln(1 + Δ) ⌉**

Inverted into a decision rule on the measured value:

| Measured η | Per-size stretch | **Sizes required** |
|---|---|---|
| ≤ 0.084 | 19.2 % | **1** |
| ≤ 0.175 | 9.2 % | **2** |
| ≤ 0.266 | 6.0 % | **3** |
| ≤ 0.357 | 4.5 % | **4** |
| ≤ 0.449 | 3.6 % | **5** |

**Design baseline: 3 sizes**, which is satisfied by any η ≤ 0.266 — comfortably above a competent
geodesic strut network and below a plain elastic cap. **2 sizes** if `NET-1` returns η ≤ 0.175.

Two things bound a band, and they are different: the **lower** bound is slack/slip, the **upper**
bound is whichever of placement fidelity and comfort pressure binds first. At 19 % stretch the
single-size case is almost certainly pressure-limited before it is placement-limited, which is why
1 size is in the table but is not the baseline.

> **Tooling consequence, and it is the important one.** Sizes are specified as a **uniform scale
> factor on one topology** — one mould family, N cavity scalings, one strut count, one puck part, one
> tail. The size count can therefore change after `NET-1` returns without re-architecting anything.
> Nothing downstream may encode N.

### 3.4 Error budget

| Contributor | Allocation | Basis |
|---|---|---|
| Landmark identification by wearer | 4 mm | Untrained palpation of nasion/inion/preauricular |
| Strain-tracking (η) | 5 mm | §3.3, drives the size count |
| Cephalic-index / shape mismatch | 5 mm | §3.5 |
| Creep / compression set over life | 3 mm | §2.6 |
| **RSS total** | **8.7 mm** | **Within the ±10 mm tolerance** |

**Where ±10 mm comes from, rather than being asserted:** 10-20 lines are ~33 mm apart
(`NP-HEX-ZM-001` §3.2), so ±16.5 mm is the point at which a site becomes *ambiguous with its
neighbour*. ±10 mm holds ~40 % margin against that ambiguity limit and is consistent with the 5–10 mm
error of routine manual 10-20 placement. It is a design tolerance, not a clinical claim.

### 3.5 Cephalic index is a shape axis, and size cannot touch it

Circumference does not determine the nasion→inion arc. Two 580 mm heads at CI 0.72 and CI 0.85 have
materially different sagittal-to-coronal arc ratios. `NP-HEX-ZM-001` §3.3 assumes `CEPHALIC_INDEX =
0.78`; the adult human range is roughly 0.70–0.85.

Scaling a net uniformly changes size, not shape, so **no number of sizes addresses CI.**

> **This answers the request's either/or — adjustable net *or* several sizes — as *both, on
> orthogonal axes*, and the reason is anthropometric rather than a compromise.** The net is **sized**
> on the coronal/overall scale, where fidelity limits reach, and **adjustable** on an independent
> **sagittal take-up** (nasion→Cz→inion chain), where scale cannot help because the variation is a
> shape change. One graduated, detented take-up; the app states the setting during guided fit.

### 3.6 Gate NET-1 — the experiment that sets N

**Question.** What is η for the candidate net, over 520–620 mm, at 10-20 sites?

**Hypotheses (plural, and each falsifiable):**
- **H1** — Equal-tension geodesic strut topology achieves η ≤ 0.175 → 2 sizes.
- **H2** — η is in 0.175–0.266 → 3 sizes (the baseline).
- **H3** — η is site-dependent rather than a scalar, and the sagittal chain tracks worse than the
  coronal, because sagittal struts are loaded through fewer junctions. *If H3 holds, the scalar model
  in §3.1 is wrong and the size count must be derived per-axis.* **H3 is the hypothesis most likely
  to be true and the one the protocol is designed to expose.**

**Method.** Eleven rigid head-forms, 520–620 mm in 10 mm steps, at CI 0.72 / 0.78 / 0.85 (33 forms).
Ground-truth 10-20 sites marked on each form by measured arc fraction. Fit each net size, photograph
under calibrated stereo or structured light, and measure puck-centre to marked-site distance.
η is recovered per site by regressing measured error on *f* · L · |λ − 1|.

**Sample size.** With between-specimen SD of η ≈ 0.05, resolving η to ±0.04 — the precision needed
to separate the 2-size and 3-size bands — requires **~25 fit trials per net size**. At SD ≈ 0.03 it
falls to ~9. Protocol specifies **n = 25 per size**, reducible on a measured SD from the first 10.

**Decision rule.** Enter the measured 95 % upper confidence bound on η into the §3.3 table. Use the
**upper bound**, not the point estimate — a size count chosen on a point estimate is wrong half the
time by construction.

**Falsification condition.** If no size count ≤ 5 satisfies ±10 mm, the equal-tension geodesic
approach has failed and the fallback in §2.1 (low-stretch fabric, ~2 cm bands) is taken instead.

**NET-1 gates:** the size count, the tooling cavity count, packaging, the fit SKU matrix, and any
placement-accuracy claim in marketing or regulatory material.

---

## 4. Placement verification — gate NET-2

A net that is correct by construction is still worn by a person who may have put it on crooked.
Registration must be **measured per session**, not assumed per manufacture.

### 4.1 The scheme uses hardware that already exists

Every PBM tile carries **PD1** (behind the window) and **PD2** (scalp-facing, measuring backscattered
tissue power) — the RISK-14 Option B dual-photodiode dose-metering pair, CLAUDE.md §3 modality 1.

Each electrode puck carries a **PTFE annulus** around the Ag/AgCl contact, ~98 % diffuse NIR
reflectance. Scalp under hair returns far less. The contrast is large.

**Contract.**

| | |
|---|---|
| **Input** | Sequential low-power illumination of each tile; PD2 return read on that tile and its ring-1 neighbours |
| **Intermediate** | Per-tile albedo excess → puck-occupancy map at tile resolution (~±17 mm) |
| **Solve** | Least-squares rigid-body pose of the net against its *known* internal geometry. Because the net's inter-puck geometry is known to ±5 mm, the pose fit is over-determined and resolves better than any single tile reading |
| **Output** | Either "seated" with a residual, or a specific correction: *"shift 12 mm posterior"* |
| **Failure** | **Fail closed.** Pose unresolved → no session start; the wearer is asked to reseat. Never a silent best-guess pose |

### 4.2 Three honest limits, stated rather than buried

1. **It measures net-to-*helmet* pose, not net-to-*head* pose.** Head-to-helmet is set separately by
   the 5-position forehead bridge and the Boa dial. The scheme closes one half of the loop; §2.2's
   landmark anchors close the other. Only together do they constitute registration, and neither alone
   should be described as verifying placement.
2. **The ranging flash is a dose.** It is NIR energy delivered to the scalp and **must be metered into
   the J/cm² per-zone record**, which is UHDR under CLAUDE.md §5.1. A verification feature that
   silently accrues unlogged dose would defeat the dose-metering differentiator.
3. **NIR albedo depends on skin tone and hair.** Contrast against a 98 % PTFE annulus is large, but
   "large" is an assumption until measured. **Validation across Fitzpatrick I–VI and across hair
   density and colour is required, not suggested.** The failure mode is not merely reduced accuracy —
   it is a feature that *works better for some users than others*, which is a validity problem before
   it is an accuracy problem, and shares a root with the known skin-tone dependence of PPG and PBM.

### 4.3 Data classification

Follows the existing boundary rule (CLAUDE.md §5.1) by direct analogy with *"raw VNS impedance →
UHDR; contact resistance trend → SHDR"*:

| Element | Class | Reason |
|---|---|---|
| Per-tile PD2 albedo readings | **UHDR** | Scalp optical properties are user biology, and hair/skin phototype is inferable from them |
| Resolved net pose, per session | **UHDR** | Derived from the above; head geometry is user biology |
| Ranging-flash dose (J/cm²) | **UHDR** | Already the class of all PBM dose |
| Coarse boolean `net_pose_resolved` | **SHDR** | Device-condition signal with no user biology, and it must be a bare boolean — a *count* of failures weakly signals an atypical head, exactly the pattern already caught for the anonymisation-pipeline `failed_step` |

### 4.4 What NET-2 does and does not close

**NET-2 qualifies the verification scheme** — discrimination across phototypes, pose accuracy against
a ground-truth fixture, fail-closed behaviour, and dose accounting.

> **It does not close `REG-1`, and no claim to that effect is made here.** `REG-1` is about where the
> *socket lattice* sits relative to anatomy. §7.1.1 argues that question shrinks; it does not vanish,
> and it is not answered by this scheme.

---

## 5. Modality interference

Every T1 modality and every T2 addition sharing the cranial vault gets a verdict **and a falsifier** —
a measurement that could show the verdict wrong. **No cell says "none" without one.**

| # | Modality | Interaction | Verdict | Falsifier |
|---|---|---|---|---|
| 1 | PBM transcranial | Occlusion, NIR absorption, standoff, PD fouling discrimination | **Material — controlled by §5.1–5.3** | `NET-DRC-01/02/03` |
| 2 | PBM intranasal | Not in the net envelope | **None** | Physical fit check with net donned |
| 3 | EEG neurofeedback | This *is* the net | **Improved registration, possibly degraded artifact** | `SH2-DRC-16` (§7.2.1) |
| 4 | BES / tACS | Net carries the drive (option b) | **Material — wet shunt, §5.5** | `NET-DRC-06/07` |
| 5 | tDCS | As above, plus 40 µC/cm² | **Material; charge limit unaffected** | `NET-DRC-07` |
| 6 | VNS + HRV | Auricular clip, below the lattice ear cut-out | **None** | Clip fit + A1/A2 continuity with net donned |
| 7 | Neural audio | Ear cups are a rim-mounted subassembly; net terminates above the ear cut-out | **None, by exclusion** | Acoustic seal + 40 dB mesh RF check with net donned |
| 8 | Visual stimulation | Goggle/lens, outside the vault | **None**, but §7.2.4 re-homes its Oz safety gate | `NET-DRC-09` |
| T2 | 21-ch qEEG | Separate net part, same architecture | **Out of scope here** | — |
| T2 | 1170 nm deep PBM | Laser line sits on PET's C–H overtone | **Material — drives the fluoropolymer choice** | `NET-DRC-02` |
| T2 | TMS | 0.1–0.5 T through the net | **Material — §5.6** | `NET-DRC-10/11` |

### 5.1 PBM occlusion — `NET-DRC-01`

Strut routing cannot be guaranteed to avoid tile apertures, because strut geometry scales with the
head and the tile lattice does not — the same divergence as §1.1. Occlusion must therefore be handled
**by material, not by geometry**.

> **`REQ-NET-01`** — Areal occlusion of any tile aperture by net material ≤ **8 %**, at every size and
> at both ends of the sagittal take-up range.

### 5.2 NIR absorbance — `NET-DRC-02`

> **`REQ-NET-02`** — Single-pass transmittance of any net element that can lie in a tile aperture
> ≥ **97 %** at 660, 810, 1064 and **1170 nm**, measured on production material at production
> thickness.

1170 nm is called out because it is where the fallback material is weakest. This requirement is what
makes the PET fallback a *measurement* rather than a guess.

### 5.3 Dose metering — `NET-DRC-03`

PD1/PD2 ratio separates PDMS fouling from LED aging (CLAUDE.md §3 modality 1). **A strut in the
optical path perturbs that ratio and can read as fouling.** Two consequences:

> **`REQ-NET-03`** — The fouling/aging discriminator must be re-characterised with the net installed.
> **`OI-EEGNET-04`** — Whether dose-metering calibration coefficients become net-size-dependent is
> **open**. If they do, the coefficients are no longer purely module property, which contradicts
> `NP-HW-HUB-001` §9.5. Not resolved here.

### 5.4 Fluxgates and Helmholtz cancellation

The active cancellation depends on 3-axis fluxgate magnetometers mounted on **L1**
(`NP-HEX-ZM-001` §5.3c), and `REQ-EMI-10` forbids conductive additions to L1 without re-qualification.
The net is not on L1, but it sits millimetres inboard of it, inside the sensing volume.

> **The net is held to a stricter standard than any fixed part, and here is the reason.** A fixed
> ferromagnetic or conductive mass is *calibrated out* — `REQ-EMI-11` already re-triggers calibration
> on `np_module_map` rebuild. **The net is the only magnetically-relevant mass in the assembly whose
> position changes between sessions, which stretches, and which does not appear in `np_module_map` at
> all.** It cannot be calibrated out by any existing mechanism.

> **`REQ-NET-04`** — Zero ferromagnetic content. Total remanent moment ≤ **1 nAm²**. Nickel underplate
> is **prohibited**, including inside the connector; specify gold over palladium or gold over a
> Pd-barrier copper.
> **`REQ-NET-05`** — Every net conductor ≥ **1 kΩ** end-to-end, so eddy currents are negligible at ELF.
> **`REQ-NET-06`** — No continuous metallic shield film anywhere in the net.
> **`REQ-NET-07`** — The net adds no conductive element to L1.

### 5.5 Wet shunt — a burn hazard, not a signal-quality nuisance

Sweat, hydrogel and cleaning residue make textile conductive. Under tES a surface shunt between
anode and cathode concentrates current at the shunt's entry point — a documented tDCS burn mechanism.
It is the reason hydrophobic yarn is not a nicety.

> **`REQ-NET-08`** — Creepage between any two stimulating pucks ≥ **25 mm** along every net surface path.
> **`REQ-NET-09`** — Inter-electrode leakage ≥ **10 MΩ** dry and ≥ **100 kΩ** after a specified
> sweat-soak conditioning.
> **`REQ-NET-10`** — Before any tES enable, the **safety MCU** measures the inter-electrode impedance
> matrix and inhibits if any off-diagonal element falls below threshold. Assigned to the safety MCU
> because CLAUDE.md §4.2 gives it ownership of every stimulation enable GPIO; this is the same pattern
> as the existing VNS contact-confirmation interlock.

The 40 µC/cm² hardware charge-density limit is unaffected — it is enforced in the safety MCU
independent of the electrode carrier.

### 5.6 TMS (T2) — `NET-DRC-10/11`

At 0.5 T in ~100 µs, dB/dt ≈ **5 × 10³ T/s** (assumption stated; the real waveform is a damped
sinusoid and the peak should be taken from the coil driver spec). Induced EMF = A · dB/dt, so a 2 cm²
enclosed loop sees ~1 V.

> **`REQ-NET-11`** — Electrode temperature rise under the worst-case rTMS train ≤ **2 °C**.
> **`REQ-NET-12`** — No ferrite and no magnetically saturable component anywhere in the net.
> **`REQ-NET-13`** — Enclosed loop area ≤ **2 cm²** between any recording lead and the reference lead
> over the whole run (§6.2).

---

## 6. Wiring — the antenna question, answered on the right physics

### 6.1 "Antenna" is the wrong model here, and saying so changes the fix

The request asks that the wiring not act as an antenna, alone or through modality interaction. The
concern is exactly right; the mechanism is not radiative.

The net lives **inside** the Faraday envelope (`NP-DRV-SHELL-002` §9.6), and the BT/Wi-Fi radios are
in the control hub, **not** the headset (CLAUDE.md §4.1). The emitters that remain are internal and
slow: I2C at ≤400 kHz, LED PWM at ≤40 Hz with fast edges, ELF Helmholtz drive. At 400 kHz, λ ≈ 750 m.
A 300 mm lead is **λ/2500**.

> **A structure that small cannot resonate and cannot radiate meaningfully. It is not an antenna at
> any frequency present inside the shell.** What it can do is far worse and far more likely:
> **near-field mutual inductance** (enclosed loop area × dB/dt) and **mutual capacitance** (displacement
> current onto a high-impedance node). Those two are the design targets. Ferrites, RF absorbers and
> shield cans — the reflexive antenna fixes — buy nothing here and are prohibited anyway by
> `REQ-EMI-10` and `REQ-NET-04`/`-06`.

This is the same finding `NP-DRV-SHELL-002` §9.6 reached from the other direction: *"A Faraday cage
does not protect the EEG electrodes and fluxgates that share the enclosure with the source."*

### 6.2 Topology — tree, co-routed, single tail

> **`REQ-NET-14`** — Conductor topology is a **tree** terminating at one posterior connector. No closed
> conductive ring at any scale. (Inherits `REQ-EMI-09`.)
> **`REQ-NET-15`** — All leads are co-routed in one flat bundle along a common geodesic to the
> posterior termination, so the loop between any lead and the reference is a thin ribbon rather than a
> bowl-scale circuit. This is what makes `REQ-NET-13`'s 2 cm² achievable.
> **`REQ-NET-16`** — The tail terminates at the **posterior aggregation node**, where the ADS1299 bank
> already sits (`NP-DRV-SHELL-002` §3.5). The µV path therefore never enters L1, never crosses the
> parting-plane boss, and passes through **no pogo contact and no analog mux**.

### 6.3 Series resistance at the puck

> **`REQ-NET-17`** — Series resistance is integrated **in the puck**, not at the connector. A resistor
> at the far end limits nothing: the induced EMF appears along the lead, and only a resistance in
> series with the lead's own loop bounds the resulting current.

**Noise cost, computed rather than waved away.** Johnson noise over a 100 Hz band:

| Series R | Noise | Against ADS1299 ≈140 nV rms |
|---|---|---|
| 1 kΩ | 41 nV | +4 % total |
| 5 kΩ | 93 nV | +20 % |
| 10 kΩ | 131 nV | +37 % |

Not free at 5–10 kΩ. **What makes it affordable is that the semi-dry electrode's own source impedance
is already 5–50 kΩ**, so the added resistor shifts an already-dominated budget by a fraction rather
than a factor. Stated as a real cost because pretending it is zero would be wrong.

The series R also forms a low-pass with lead capacitance — an incidental benefit against fast PWM
edges, and one that must be bounded so it does not encroach on the EEG passband.

### 6.4 The tES conflict, resolved differently for T1 and T2

A 10 kΩ series resistor and a 2 mA stimulation current cannot share a conductor: 20 V of drop and
40 mW in the puck.

**T1 — one conductor per site, because T1 has no TMS.** Series R is needed only for fault-current
limiting and edge filtering, both satisfied at **1 kΩ**. At 2 mA that is 2 V of compliance — trivial —
and it doubles as a genuine tDCS safety feature, limiting fault current if a driver fails. Noise cost
+4 %. Count: **8 sites + reference + DRL = 10 conductors + guard.**

**T2 — two conductors per dual-rated site, because TMS is present.** Recording needs ≥5 kΩ in the lead
during a pulse; stimulation needs <50 Ω. Count: **21 × 2 + reference + DRL = 44 conductors + guard.**

> **Rejected: a parallel inductor to pass DC stimulation while blocking the TMS-induced transient.**
> Two independent failures. (i) At TMS spectral content (~3–10 kHz) a practical inductance gives an
> impedance comparable to the resistor, so it only partly helps. (ii) Any inductance large enough
> requires a ferrite core — ferromagnetic, prohibited near the fluxgates by `REQ-NET-04`, and it would
> saturate in the TMS field regardless.

> **`OI-EEGNET-07`** — Whether concurrent TMS **and** full 21-channel tES capability is actually a
> required protocol is **open**, and it is the single requirement that doubles the T2 tail. Worth
> asking before it is built.

### 6.5 Guarding — resistive, not braided

> **`REQ-NET-18`** — The guard is a **carbon-loaded polymer sheath driven from the DRL**, not a metallic
> braid.

A guard works by holding the surrounding conductor at the signal's potential so no displacement
current flows into it. **That is a statement about potential, not about conductivity** — a resistive
guard guards. Its limit is its RC: at ~10 kΩ/m and ~100 pF/m, τ ≈ 1 µs, effective to roughly 150 kHz.
That covers the EEG band with four orders of margin. Residual coupling from fast PWM edges above that
is handed to the existing deterministic-artifact machinery (`REQ-EMI-03` sense-quiet windows,
`REQ-EMI-04` dithering prohibition), which is precisely what those requirements exist for.

### 6.6 Reference

> **`REQ-NET-19`** — A net-borne reference at FCz plus a net-borne DRL is the baseline. **A1/A2 linked
> ears are taken from the VNS clip's contact pads over the existing 6-pin cable** (CLAUDE.md §3
> modality 6) whenever the clip is worn — this provision already exists and is not reinvented here.

---

## 7. What this change actually does — both halves

### 7.1 Genuine leverage

**7.1.1 `REG-1`'s content largely dissolves, and one unfixable defect is fixed.** With no electrode on
any socket, the lattice only has to cover scalp *optically*, against `NP-OPT-PSF-001`'s resolution
floor rather than a 10-20 tolerance. The Fp1/Fp2 shared-socket defect and the Oz-18 mm-anterior caveat
(§1.3) both dissolve. **This is the only item on either list that no parameter change can deliver.**

`REG-1` does not vanish: PBM zone naming still needs it. Its scope collapses; its tight-tolerance half
leaves. This may unblock the shell-tooling first cut via `OI-REVSH-01` — *may*, because
`NP-DRV-SHELL-002` also needs `ACT-1`.

**7.1.2 The socket-pinout union rule stops paying for a type that no longer exists.**
`NP-HW-HEXTILE-001` §1 makes the socket pinout *"the union of every type's needs,"* which is why pins
13 `ELEC` / 14 `ELEC_SHLD` / 15 `AGND` exist at **all ~80 sockets** for a T1-B that might land
anywhere. Delete the type and network **N4 leaves the socket interface entirely**, taking the
per-cluster electrode mux, the L1 scalp-facing DRL guard plane, and the cluster-tail feed with it.
`OI-SHELL2-10` (µV path through a pogo contact plus a mux) is deleted rather than mitigated.

> **This spends an option deliberately.** `NP-HW-HEXTILE-001` §7.2 warns that removing those pins
> *"would silently re-impose the type-restricted placement model that SMART-1 was decided to
> eliminate."* Removing them because no type needs them is legitimate — but `SMART-1`'s rationale
> covers a *future* electrode-bearing smart tile at any socket, including for the IRB custom studies
> in CLAUDE.md §6.3. That option is being spent, not overlooked.

**7.1.3 A cluster of open items closes by deletion.** The electrode-pod body diameter is unspecified
anywhere in the document set and is the input deciding T1-B's LED ring depopulation. Deleting T1-B
closes `OI-HEXTILE-05`, `RISK-HEX-03`, the ELECTRODE-POD part, the bezel's type-dependent `s = 0`
split (`NP-THERM-BEZEL-001`, `BEZEL-1b`), and the per-type operating-envelope split
(`NP-ENV-OPRANGE-001` §4 — T1-B's +5 °C gel low bound disappears and every tile becomes −10 → +43 °C).
`OI-COST-02` (Core 3-vs-4 tiles) dissolves.

**7.1.4 Taxonomy collapses 3 types → 2, with a real accessibility gain.** `NP-HFE-002` §5's *"9
positions that matter out of ~80"* becomes **zero positions that matter**; L1(d) — the only HFE
feature gated on `REG-1` — is deleted. For a blind user, positional counting leaves module placement
entirely.

### 7.2 What gets worse — read this before adopting

**7.2.1 `SH2-DRC-16` (<5 µVpp artifact) may REGRESS, not improve.** The tempting claim is that moving
electrodes off L1 restores the retired ≥15 mm PBM-to-EEG separation. **It does not, and the honest
reading is the opposite.** The tiles' *emitting face* is the scalp-facing face, so a net between scalp
and tile sits **closer** to the LED drive loop than L1's guarded face does. Two of the four
compensating mechanisms in `NP-DRV-SHELL-002` §9.1 weaken: mechanism 2 is lost outright (the DRL guard
plane is an L1 structure), and mechanism 1 lengthens (the analog path now runs from a floating net
across the bowl boundary to the PAN). §9.2's arithmetic is untouched — the therapeutic band still
cannot be filtered on the tile — so the **source is unchanged**. `SH2-DRC-16` is retained verbatim and
becomes harder again.

> Routing the net outside the outer bowl to recover geometric separation is **rejected**: it reopens
> exactly the shield-aperture problem `NP-HEX-ZM-001` §5.2 exists to avoid.

**7.2.2 BOM moves the wrong way, on the worst possible term.** `NP-COST-001` §5 gives Home Standard's
emitter count as an explicit formula: **21 × 90 + 9 × 44 = 2,286** (T1-A = 90, T1-B ≈ 44 per
`NP-HW-HEXTILE-001` §4.2 — T1-B is depopulated for pod clearance). Delete T1-B and every tile is
T1-A: **30 × 90 = 2,700. +414 emitters, +18.1 %.** This is exact arithmetic on the source formula,
not an estimate. That inflates term **U** — the emitter-count delta `NP-COST-001` §2 already
flags as *"very likely large and positive"* and the reason `OI-HUB-C08` cannot close — against a
configuration already at **−41 % to −51 % gross margin** with a ~$1,196 break-even. The net, its
contacts, connector, tooling and FAI are all net-new on top.

> **Nothing here is a cost saving.** The N4 mux deletion (~$11–22/headset, `NP-DRV-SHELL-002` §10.1)
> is real but is smaller than the emitter regression. **No figure in this document may be entered into
> `NP-COST-001`; that document owns the re-derivation and must do it as a whole.**

**7.2.3 Two tooling savings reverse.** `cad/CAD_PARTS_LIST.md` records `EEG-ROUTE-CHANNEL` as
**RETIRED**, its `REQ-ST-01..07` features *"deleted from shell tooling (a net simplification of the
shell tool — the complexity moves into L1 lamination)."* A net needs routing, strain relief and a
connector, so that channel or its equivalent returns — moving complexity back into the more expensive
tool. And **`RISK-21` reverts**: `NP-RISK-002` disposes of it (EEG electrode cable routing
unspecified, HIGH) on the ground that *"There are no EEG cables to route."* That becomes false.

**7.2.4 Two safety gates re-home onto a soft, user-placed part. This is the highest-consequence item
in this document.**

| Gate | Today | On a net |
|---|---|---|
| Photoparoxysmal halt | `check_placement({Oz, EEG\|DUAL})` against keyed socket 74 — **discrete, digital, deterministic**; <200 ms halt | **Continuous, analog**: net seated *and* Oz contact impedance acceptable |
| tES montage presence | Electrodes present at all montage sockets (HD-tDCS 4×1 = 5) | As above, across the montage |

`np_module_map_check_placement()` and its `type_mask` model do not generalise to a net. **A new
presence primitive is required**, and it must be at least as deterministic as the one it replaces for
a Class C <200 ms halt path. That is a firmware safety work item, not a documentation one.

**7.2.5 The identity model has no slot for a net.** *"Socket = position, module = type, all discovered
by UID auto-inventory"* has no representation for a cranial element that is **neither a socket nor a
module**. Code deletion is trivial — `NP_ELEM_EEG_ELECTRODE` (5) and `NP_ELEM_TES_ELECTRODE` (6)
already exist and are unused, so the enum anticipated the split — but adding the net is an **addition
to the architecture**, not a deletion from it.

**7.2.6 What does NOT improve, despite looking as though it should.** The 18-cluster tier does not
shrink to 12 and the $114.12 controller tier does not drop. `SYM-1`'s recorded justification is the
per-cluster safety cut domain, the I2C segment, inclusive-midline zone membership, EMDR L/R
alternation, hemispheric **PBM** targeting and `NP-OPT-PSF-001`'s lateralisation model. Only
"bilateral montages" is electrode-related, and it follows tES. **`SYM-1` and `CONTIG-1` survive
intact.**

### 7.3 Locked decisions touched — was → is → cause

Per `NP-CONV-001` §7. Nothing below is reversed by this document; each is *raised*.

| Decision | Was | Is (proposed) | Cause |
|---|---|---|---|
| `NP-HEX-ZM-001` §4a T1-B tile type | Dual-rated electrode tile placeable at any socket | Deleted; electrodes off-lattice | §1 — tangential registration is unachievable on a rigid lattice |
| Socket pinout (`NP-HW-HEXTILE-001` §7) | 16/19-position union including 13/14/15 | Two positions freed | §7.1.2 |
| `REG-1` scope | Lattice registers to 10-20 within tolerance | PBM zone naming only | §7.1.1 |
| `RISK-21` disposition | *"There are no EEG cables to route"* | **Reverts** | §7.2.3 |
| `EEG-ROUTE-CHANNEL` | RETIRED from shell tooling | Returns in some form | §7.2.3 |
| Single inner transparent shield (`NP-HELMET-GEOM-001` §0) | **ABANDONED**, on exactly one stated ground: *"electrodes must galvanically contact skin, which a continuous dielectric barrier physically blocks"* | **Reopened — NOT decided here** | That ground is removed when no electrode is in the lattice. Consequences for sealing (`SEAL-1`, 80 gaskets), cleaning and tooling are large and belong to a principal decision, not to this document |

### 7.4 Blast radius, measured

**30 files, 102 references.** Code surface is 6 files: `np_module_map.h`, `np_module_map_tests.c`,
`np_zone_notify.h`, `np_zone_announce.c`, `ZoneModuleInfo.swift`, `shdr_fleet_schema.sql`; plus 2
editscripts and 2 CAD files. The remainder are controlled documents, each taking a revision event
under `NP-CONV-001`. Eight ISCs in `np_hex_zm_isa.md` (49, 51, 52, 54, 55, 56, 62, 68) are directly
falsified.

### 7.5 Two observations for the principal

**This is an oscillation, not a one-way improvement.** EEG was previously *outside* the tiles, on a
separate harness on the far side of the shell wall, and was moved *in*; the artifact problem came with
it and `NP-DRV-SHELL-002` §9.1 is the record of paying for that. This moves it back out, and the
constraint that reasserts is mechanical/tooling complexity. Each swing trades artifact integrity
against tooling simplicity without either being decisively dominant — **and that pattern is the signal
that neither is the binding constraint.** The binding constraint is the one thing that does not
oscillate: **a 40 mm hex lattice cannot resolve adjacent 10-20 lines.** That is what should decide
this, and it is the argument in favour.

**Sequencing.** `OI-HEXTILE-05` already blocks T1-B's layout, so leaving T1-B *undecided* costs nothing
today. `REG-1` is the urgent gate, and its scope depends entirely on this outcome. **Decide T1-B before
spending more effort on `REG-1`** — otherwise that effort may be spent establishing a registration the
architecture no longer needs.

---

## 8. Open items

| ID | Item | Owner | Gate |
|---|---|---|---|
| **OI-EEGNET-01** | **§0 fork not decided.** (a) EEG-only net / (b) EEG+tES net / (c) tES deleted from T1. Everything in this document assumes (b) | **Principal** | Blocks all |
| OI-EEGNET-02 | η unmeasured; size count is a baseline, not a result | ME | **NET-1** |
| OI-EEGNET-03 | Sagittal take-up detent count and range unspecified pending `NET-1` H3 | ME | NET-1 |
| OI-EEGNET-04 | Whether PD1/PD2 dose calibration becomes net-size-dependent, contradicting `NP-HW-HUB-001` §9.5's module-property rule | EE | NET-2 |
| OI-EEGNET-05 | PD2 albedo discrimination unvalidated across Fitzpatrick I–VI and hair density/colour | Systems + Clinical | **NET-2** |
| OI-EEGNET-06 | New presence primitive for the photoparoxysmal and tES gates — Class C, <200 ms | FW Safety | Blocks T2 visual + all tES |
| OI-EEGNET-07 | Is concurrent TMS + full 21-ch tES actually required? It doubles the T2 tail | Clinical | T2 |
| OI-EEGNET-08 | `NP-THERM-BEZEL-001` re-run with net material in the bezel gap and reduced evaporative scalp cooling | Thermal | **THERM-1** |
| OI-EEGNET-09 | `NP-COST-001` whole-document re-derivation. **Direction is likely adverse** (§7.2.2) | Finance + Systems | Precedes any pricing per `OI-COST-10` |
| OI-EEGNET-10 | Single inner transparent shield reopened by §7.3 — sealing, cleaning, tooling consequences unexplored | **Principal** | SEAL-1 |
| OI-EEGNET-11 | Net has no representation in the module-identity model (§7.2.5) | FW + App | — |
| OI-EEGNET-12 | Residual risk that a T1-B consumer exists which no grep pattern in §7.4 caught | Systems | Before tooling |
| OI-EEGNET-13 | Net risk register (`NP-RISK-005`?) and FAI do not exist. Per `NP-ART-001`, this would be a tenth artifact with no owning risk document | QA | Before tooling |

## 9. Cross-references

`NP-HEX-ZM-001` §3.1 (arc, tile count), §3.2 (10-20 rows, Fp/Oz defects), §4a (T1-B), §5.2 (shield
seam), §5.3 (fluxgate siting) · `NP-HELMET-GEOM-001` §0 (inner-shield abandonment), §2 (radial stack),
§5 (bezel, no inboard contact) · `NP-DRV-SHELL-002` §3.5 (N4, PAN), §5.1 (socket pins), §9.1–§9.6
(EMI, `REQ-EMI-01..11`), §10.1 (BOM) · `NP-HW-HEXTILE-001` §1 (pinout union), §7.2 (SMART-1 option) ·
`NP-HW-HUB-001` §9.5 (calibration is module property) · `NP-THERM-BEZEL-001` (bezel, THERM-1) ·
`NP-RISK-002` (RISK-21) · `NP-COST-001` §2 (term U), §6 (`OI-HEXTILE-06`) · `NP-OPT-PSF-001` ·
`NP-HFE-002` §5 · `NP-ENV-OPRANGE-001` §4 · `NP-CONV-001` Rev 6 · CLAUDE.md §3, §4.2, §4.3, §4.4, §5.1

---

*Rev 1 is a DRAFT and must not be baselined. Its central number — the size count — is an output of
`NET-1`, which cannot be run because no hardware exists. Its central decision — the §0 fork — is a
principal call that has not been made. The document is written to be decidable, not to be adopted.*
