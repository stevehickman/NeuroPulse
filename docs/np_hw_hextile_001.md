# Hex-Tile Module — Electrical / FPC Specification (T1-A, T1-C)

**Project:** NeurOne
**Document:** NP-HW-HEXTILE-001
**Revision:** 26
**Date:** 2026-09-29
**Status:** DESIGN STUDY — not a tooling baseline. Every numeric value below is a proposed engineering commitment, not a measured or locked figure. See §10 (Decisions) and §11 (Open Items).
**Effective Date:** —
**Author:** NeurOne Hardware Engineering
**Approved By:** — (pending design review)
**References:** NP-HEX-ZM-001 (2026-07-15) §3 geometry, §4 addressing, §4a module-type taxonomy + SMART-1; NP-HW-FPC-001 Rev 5 (SUPERSEDED — reused for driver topology §6.2, InGaAs PD selection §5.1, PDMS bonding §7, TIA-saturation methodology §5.3); NP-HW-HUB-001 Rev 3 (§3.1 cluster-controller tier, §7.4 interface contract, §7.5 OI-HUB-C17 synthesis, §8.1/§8.3); NP-DRV-SHELL-002 Rev 2 (§3.3a OI-HUB-C17c resolution, §5.1 socket contact budget, REQ-SKT-01, REQ-EMI-03/07); NP-FW-PBM1064-001 Rev 2; NP-OPT-PSF-001 Rev 1; NP-THERM-CFD-R1-001 Rev 1; NP-THERM-BEZEL-001 Rev 1; CLAUDE.md §3 (modality stack), §4.2 (safety architecture), §4.5 (power); NP-PWR-BUDGET-001 Rev 2 (§3.4 efficacy floor, §3.5 full-population bound, §3.6 whole-vault mode, §3.7 irradiance vs total output); NP-SES-PWR-001 Rev 1 (the protocol-library audit of §9); `docs/pbm_neuro_protocols.md` (MASTER SUMMARY, dosimetry lesson 1)
**Related Issues:** —
**Gate:** GATE-2 (PBM coupling bench) — LED array must meet dose spec at the temporal worst case before this layout is tooled
**IEC 62304 Class:** — (hardware; the on-module driver firmware is Class B, see §6.5)
**Supersedes:** — (new document; fills the gap declared in NP-HW-FPC-001 Rev 5 supersession note: *"no document yet specifies the T1-A/T1-C hex-tile FPC pinout or electrical layout"*)
**Parent Document:** NP-HEX-ZM-001

---

> **Rev 26 (2026-09-29): `OI-HEXTILE-32` CLOSED. The laser skin reference is adopted in full as
> R-4's average term (principal; CLAUDE.md Rev 60, D-10).** The per-channel 200 mW/cm² is gone: the
> weighted sum Σ Ēᵢ / (200 × C_A(λᵢ)) ≤ 1 is the whole average term. One channel alone may average
> 200 at 660 nm (unchanged), ≈ 329 at 808 nm and ≈ 364 at 830 nm. Above ≈ 850 nm the 400 peak binds
> first, so 1064 nm is held to 400, not the reference's 1000. The decision was taken with all four of
> its listed inputs open, and D-10 records what that leaves.

> **Rev 25, corrected before merge (2026-09-29):** as first drafted, R-4 applied 200 mW/cm² to each
> channel on its own. `NP-BIB-PBMIRR-001` §3.1 shows that the skin reference adds across wavelengths
> on the same tissue. 660 nm and 808 nm at 200 each would score 1.61 against it; the old 25 % rule
> held that pair to 0.8. R-4 now carries the weighted sum as well. Adopting the reference in full is
> `OI-HEXTILE-32`.

> **Rev 25 (2026-09-28): R-4 loses its 25 % duty cap. A protocol's duty and mode are delivered as
> written, and the hardware is redesigned where it cannot deliver them (principal direction;
> CLAUDE.md Rev 59).** R-4 is now 400 mW/cm² peak and 200 mW/cm² time-averaged, at any duty. Only the
> coupling to 25 % goes. A pulse train at 99 % duty is CW, so the CW ceiling means anything only as
> a bound on the average.
>
> **What this breaks in this document.** D-9's U4 (`REQ-TDRV-02`) holds each channel to ≤ 50 %
> conduction over any 250 ms. That forbids CW, and every duty above 50 %, at any current, including
> Cassano 36 mW/cm² CW, which is 18 % of the CW ceiling. So U4 must be redesigned: it must bound the
> **average** and let a channel conduct continuously below 200 mW/cm². That is **`OI-HEXTILE-30`**.
> U3 (`REQ-TDRV-01`, the 400 peak) is unaffected. The rest of the chain is **`OI-HEXTILE-31`**: the
> language (1–25 %), the compiler's silent clamp, the hub constant and the library, which was
> authored around the cap. **Rev 24's resolution of `OI-HEXTILE-25`(c) is withdrawn** (see that
> row).

> **Rev 24 (2026-09-28): `OI-HEXTILE-07` CLOSED. The on-module firmware is specified in
> `NP-FW-HEXTILE-001` Rev 1 (SW-04, Class B).** The register map keeps every
> `NP-FW-PBM1064-001` §5.1 address. It adds a version and a command register, the identity block at
> `0x40` and the odometer at `0xE0`. It defines **no PD readback registers**, because D-4 was not
> adopted. Three of this document's items get answers:
>
> - **`OI-HEXTILE-25`(c):** the tile clamps duty to 25 % in every mode, CW included. **⚠ WITHDRAWN
>   Rev 25:** that made CW undeliverable. R-4 no longer caps duty.
> - **`OI-HEXTILE-25`(d):** `CUR` above `I_cap` saturates in U3, and firmware holds no copy of `I_cap`.
> - **`OI-PBMCH-01`, firmware half:** `CUR` = 0 holds the gate off.
>
> **Writing it found four things this document must still resolve**, raised there as
> `OI-FWTILE-01`…`-04`:
>
> - **U1 has no temperature input.** §6.2 says RT1 is *"read by U1 ADC"*, but §7.2 routes it to the
>   controller, so the local throttle R-9 needs is unbuildable.
> - **U3 has a fixed reference and no setpoint path**, so `CUR` has no circuit to act on.
> - **§6.2's SOIC-8/SOT-23-8 package** carries 6 I/O against 8–15 needed.
> - **Nothing holds a gate off while U1 is unpowered**, and `VLED` mates before `VCC_3V3` (§7.3).
>
> The §6.2 U1 row, §6.5, §11, §12 and §13 carry pointers. No part, value or figure changes.

> **Rev 23 (2026-09-28): `OI-HEXTILE-07` gains a binding input, the tile identity block.**
> `NP-HEX-ZM-001` Rev 9 §4.1 closes `OI-HEXMAP-02` on the hub side. The hub reads a tile's UID and
> element types from a CRC-32-sealed block at register `0x40`, over the socket-indexed I2C seam. The
> on-module firmware (§6.5, `OI-HEXTILE-07`) must implement that block byte for byte and keep it
> immutable for the life of the tile. The layout is owned there, not restated here. Nothing else in
> this document changes, and nothing is measured.

> **Rev 22 (2026-09-28): the three dimmer 660 nm candidates are read off their own datasheets. Rev 21's lead-grade claim that a mid-power part "covers the whole library" does not survive them. Luminus MP-2835-1100-DR is the best of the three at delivering protocol output. No part is selected.** GitHub #333.
>
> The GH DASPA2.24 (v1.5), Luminus MP-2835 Colors (PDS-003017 Rev 6) and Lumileds DS237 datasheets
> were supplied. Their relative-flux curves were digitised (§4.3 Rev 22 note). **GH DASPA2.24 is
> rated `I_F` min 30 mA** (*"Do not use below 30 mA"*). That is a floor of ~125 mW/cm² at 45 sites,
> which excludes five authored protocols. It is typically **98.3 mW at 100 mA**, not Rev 21's
> ~110–115. **Neither 2835 part states a minimum current.** Their curves are plotted from 10 mA
> (Luminus) and 25 mA (Lumileds), which gives floors of ~42 and ~99 mW/cm². **No candidate reaches
> the library's three lowest targets (22, 30 and 36 mW/cm²) on datasheet evidence.** The Luminus
> curve is linear through the origin down to 10 mA, so it reaches them at ~5–8.4 mA by
> extrapolation. **That is the one gap left, and a bench measurement closes it, not a rating**
> (`OI-HEXTILE-29`).

> **Rev 21 (2026-09-28): `OI-HEXTILE-29` is re-derived against what the protocols ask for. Fewer sites does not rescue the 660 nm primary, and a dimmer mid-power part covers the whole library. No part is selected.** GitHub #333.
>
> The compiler maps `intensity` linearly onto the 8-bit `CUR_A` register, 0–180 mA at 0.71 mA per
> step, and writes the same value to both channels (`hubCompiler.ts` `intensityReg()`). So each
> protocol's irradiance has to come from **analog current**. Duty is already spent on the pulse
> pattern, and R-4 is a peak ceiling. The library's CH_A targets run from **22 mW/cm²**
> (`clinical-06`, 5.5 %) to **~322 mW/cm²** (80 %). At 45 sites that is **5.2–75.9 mW per
> emitter**, and 94.3 mW at R-4.
>
> **GH CSSRM5.24 delivers none of it.** Its 100 mA rated minimum is a floor of `N` × 15 mW/cm²:
> **663 at 45 sites and 353 at 24**, which is above every authored target. So `OI-HEXTILE-29`'s
> way out (a), fewer sites, fails on the protocols even where it passes on R-4. **A mid-power
> 660 nm part at ~1.1 mW/mA covers the library at 45 sites between ~5 and ~90 mA**, if its rated
> range reaches down to ~5 mA. Two such parts are found, **at lead grade only**: ams-OSRAM
> OSCONIQ P 2226 **GH DASPA2.24** and Luminus **MP-2835-1100-DR**. Neither datasheet was reachable
> (`docs/status/pending-decisions.md` §13.2e(k)).
>
> **⚠ CORRECTED Rev 22:** the datasheets show that GH DASPA2.24's rated range stops at 30 mA and
> that neither 2835 part is characterised below 10 mA. So "covers the whole library" holds for
> no candidate on datasheet evidence (Rev 22 banner).

> **Rev 20 (2026-09-28): `OI-LED-01` is read for the three parts it names, off their own datasheets. Every string length in this document was a design target, and all three move. `OI-HEXTILE-29` raised. No emitter is selected and nothing is decided.** GitHub #333.
>
> The GH CSSRM5.24, SFH 4718A and L1IZ-0850 datasheets were supplied and their `IF = f(VF)` traces
> were read at 120–180 mA (§8.1.1 table, `NP-PROC-FPC-001` Rev 9 §2.6.3). **CH_A's 660 nm primary
> runs at 1.78 V, not 2.10 V, so its string is N = 13, not 11.** SFH 4718A is 1.41 V at N = 16, not
> 1.60 V at 14. The L1IZ-0850 trace **fails its own self-check**. It reads 0.26 V above its datasheet's
> typical, so its N is 8 or 9 and cannot be settled from this document. **At every datasheet N, §8.1.1's
> ±2-site allocation slack fails for most candidates** (table under §8.1.1).
>
> **`OI-HEXTILE-29`: the 660 nm primary cannot reach R-4's ceiling inside its own rated current
> range at 45 sites.** It delivers ~233 mW at 150 mA against §4.3's 95 mW target. Holding 400 mW/cm²
> at 45 sites needs ~60 mA by extrapolation (the curve starts at 100 mA), and the datasheet rates `I_F` **min 100 mA** (*"Do not use below
> 100 mA"*). At 100 mA a 45-site CH_A is still ~663 mW/cm². So `OI-HEXTILE-24`(a)'s `I_cap` has no
> legal value for this part at this site count.
>
> **The GH CSSRM5.24 package (3.0 mm ±0.1) is the first shortlisted square part that fits §4.1's
> lattice** (`OI-HEXTILE-22`), with 0.19 mm to spare at maximum material.

> **Rev 19 (2026-09-28): two geometry statements under `OI-HEXTILE-22` are corrected. No ✗ changes, no emitter is selected, and nothing is decided.** GitHub #333.
>
> §4.1 said a 45° package rotation clears the 3.45 mm Luminus part by ~0.03 mm. **It does not.** At 45°
> the largest square that fits is `p/√2` = 2.69 mm, the worst orientation. No uniform rotation beats
> `p√3/2` = 3.29 mm, and a per-sublattice search found nothing better, so **rotation is closed as a
> way out**. `OI-HEXTILE-22`'s second way out said a 2835 package's 3.5 mm axis "already exceeds
> 3.29 mm". That bound is for a **square**. A rectangle needs a **3.80 × 3.29 mm envelope, long axis on
> a lattice row**, and a 2835 fits it. The item's three ways out are unchanged. The second one is
> less restrictive than it was written. The site placement margin those clearances must exceed is
> still unstated.

> **Rev 18 (2026-09-27): §7.3's pad-length stagger has an order and no dimension. `OI-HEXTILE-28` raised. No figure is set and nothing is decided.**
>
> `NP-FW-NVRAM-001` `OI-NVRAM-04` (GitHub #444) asked for the stagger between contact group 3 and
> `SEAT#` so that a pre-break window would have a duration. `NP-FW-NVRAM-001` Rev 6 §4.3.1 closes it,
> because nothing there needs that window. But §7.3's own two promises need the dimension: every return
> mates before any supply, and `SEAT#` mates last under worst-case tilt. Those promises carry
> `RISK-SHELL-01`'s control, and no document tracked the dimension. Under CLAUDE.md §18 a derivation
> that exists but carries no number is raised as an open item, so it is raised here, in the document
> that owns §7.3. It is a mating-order margin, **not a timing window**, and no extraction velocity
> is assumed.

> **Rev 17 (2026-09-27): §6.2 and §8.1 disagree on whether drive current is regulated per channel or per string. `OI-HEXTILE-27` raised. Nothing is decided and no figure changes.**
>
> §6.2 fits one FET, one sense resistor and one U3 per channel, and `REQ-TDRV-01` and `OI-HEXTILE-24`(b)
> are written per channel. §8.1 and §8.1.1 say shorter strings would double *"the parallel strings and
> sense resistors"*, which follows only if every string has its own. The question surfaced in
> `NP-FEAS-FNIRS-001` Rev 2 §4 Risk B, which proposes switching strings individually. That is safe
> only under per-string regulation. The choice belongs to `OI-HEXTILE-24`'s selection of U3.

> **Rev 16 (2026-09-26): `OI-HEXTILE-01` was two dimensions read as one. The directed 1.0 mm is the bezel's *height*, and §3 needs its *width*. No figure in §3 or §4 changes. The item is re-scoped, not closed.**
>
> **What it was.** §3 and §11 recorded a *"bezel width conflict"*: 2.5 mm here, 1.0 mm in
> `NP-THERM-BEZEL-001`. `NP-RISK-003` `OI-RISK3-01` and `NP-TOOL-HEXTILE-001` `OI-THEX-02` then asked
> for the directed 1.0 mm (principal direction, 2026-08-11) to be *propagated into §3*. GitHub #330
> carried that request.
>
> **Why it is not a conflict.** `NP-THERM-BEZEL-001` §1 defines its quantity as the bezel **height**
> `h_b`, which *"sets the module-face-to-scalp air gap `s`"*. §4.2 uses it only as `R_gap = s/k_air`, a
> thickness normal to the scalp. `NP-HELMET-GEOM-001` §2 stacks it radially, as a land **+1.0 mm proud
> of the face**, and `NP-THERM-CFD-001` sweeps it as the air-gap thickness. §3 uses its figure as a
> **lateral width** in the plane of the tile (`W_a = W − 2·bezel`), and so does its source, the
> `NP-HEX-ZM-001` §3.1 column *"Active coverage (2.5 mm bezel)"*. A height does not set an active-field
> area. Writing 1.0 mm into §3 would have moved A_a, every §4.3 irradiance figure and the §4.1 pitch
> ceiling on a quantity that governs none of them.
>
> **What replaced it.** **Height `h_b` = 1.0 mm is settled** by the direction, and it binds the tile
> (`NP-TOOL-HEXTILE-001` F-TH-09a). **Lateral width is open.** It has no derivation anywhere in the set:
> 2.5 mm is a column-header input. §3 keeps it as the working assumption, because it is the figure
> §4's numbers were computed against, and changing it is a design decision that nothing here makes.
> Under `CLAUDE.md` §18, an unsupported figure found in a derivation chain is raised as an open item,
> not retired, so `OI-HEXTILE-01` is re-scoped to the width.
>
> **One arithmetic slip is corrected on the way** (flagged in `cad/CAD_PARTS_LIST.md` L-2(b)). §3
> said a 1.0 mm band would give *"12.15 cm² (+14.5 %)"*. The correct figure is **12.51 cm²
> (+17.9 %)** (W_a = 38.0 mm); 12.15 cm² back-solves to a 1.27 mm band. It is a what-if and moves
> nothing.

> **Rev 15 (2026-09-25): bookkeeping only. `OI-HEXTILE-13` is marked closed in §11, where it had stayed open after 2026-08-16.** `NP-HW-HUB-001` OI-HUB-C07 closed it on that date. This document's §8.4.1 status line, finding 3, `NP-RISK-004` §2.1 and `NP-DRV-SHELL-002` §6 all record the closure, and the §11 row was the one place that did not. Found during the GitHub #437 triage. No decision, figure or requirement changes.

> **Rev 14 (2026-09-25): the photodiode fitted to every tile may not see two of the three wavelengths it meters. `OI-HEXTILE-26` raised. No part is changed and nothing is decided.**
>
> Found by `NP-CONV-001` `OI-CONV-08` (b), GitHub #394, while checking `NP-PROC-FPC-1064-001` §4.1's
> *"Spectral range 900–1700 nm — covers 808nm CH_B"*, which is false on its face. The selected part's
> published spectral response range is **0.9–1.7 µm** (Hamamatsu's G12180-010A product listing and
> distributor listings, read 2026-09-25. The datasheet PDF could not be retrieved from this
> environment, so confirm against it). §5.1 below says *"InGaAs is broadband (600–1700 nm)"*, and on
> that basis fits the part to **T1-A**, whose only channels are 660 nm and 808 nm. **If 0.9 µm is the
> lower edge, PD1 and PD2 on a T1-A tile respond to neither of its channels, and on T1-C they see only
> CH_C.** The dual-PD dose metering of CLAUDE.md §3 ① would then be metering nothing on the flagship
> tile. The same listings give the package as **metal TO-18, φ1.0 mm**, not the *"SMD ceramic,
> 5.0 × 5.0 mm"* that `NP-PROC-FPC-1064-001` §4.2 records. That bears on the 1.6 mm annular-ring pad
> §5.1 inherits.
>
> Stated at the strength of the evidence held: a manufacturer product listing and distributor
> listings, not a datasheet read. That is enough to stop relying on "broadband" and not enough to
> re-select. Nothing here changes a part, a pad or a K coefficient. The decision, and whether a
> silicon PD returns to T1-A (the trade §5.1 declined), is `OI-HEXTILE-26`'s.

> **Rev 13 (2026-09-24) — the drive stage is regulated, and duty-limited in hardware: `NP-SOUP-LFS-001` §13.12's options A and B, decided together by Safety + Hardware Engineering. `OI-HEXTILE-23` decided; `OI-HEXTILE-24` and `-25` raised. No emitter, no part and no value is selected.**
>
> **D-9** (§10) records the decision and §6.2 gains two rows, **U3** and **U4**. The derivation is in
> `NP-SOUP-LFS-001` §13.14, and these are the requirements it places here:
>
> - **`REQ-TDRV-01`.** CH_A and CH_B conduct at most `I_cap`, set by a reference that no firmware,
>   register or stored value can raise. `I_cap` is sized so that at the regulation's upper tolerance
>   limit, and at the selected emitter's highest-flux bin and coldest junction, irradiance while on
>   is **≤ 400 mW/cm²** (R-4).
> - **`REQ-TDRV-02`.** A circuit on the gate path, not in U1, holds each channel's conducting
>   fraction to **≤ 50 %** over any window **`T_w` ≥ 250 ms**. Because flux is monotonic in current,
>   `REQ-TDRV-01` then caps the average at **200 mW/cm²**, R-4's CW ceiling, with no emitter data.
>   The 250 ms keeps the 2 Hz / 25 % preset's 125 ms pulses legal.
>
> CH_C is exempt from both: at full drive it gives 28 mW/cm² (§4.3.2), below the CW ceiling.
>
> **§4.3.1 and §8.1.1 now describe the intended circuit, and §6.2 is what changes.** That was the
> choice `OI-HEXTILE-23` asked for. Four consequences, all raised as **`OI-HEXTILE-25`** and none
> decided here:
>
> 1. §4.3.1's 150 mA design point gives 403 mW/cm², which is **0.75 % over** the cap. The cap falls
>    near 148.9 mA before tolerance.
> 2. Under D-9, CW at 200 mW/cm² is **~50 % PWM at `I_cap`**, not DC at ~75 mA, so §4.3.1's WPE
>    advantage is forgone.
> 3. The hub clamps **every** mode, CW included, to 25 % duty (`NP_PBM_DUTY_MAX_REG`), so CW 200 is
>    unreachable by the firmware today.
> 4. `CUR` spans 0–180 mA (`NP-FW-PBM1064-001` §5.1), and codes above `I_cap` now saturate.
>
> Parts, values and the R1–R3 derivation are **`OI-HEXTILE-24`**. **Decided, not built:** until
> that closes, the safety MCU's thermal cut is still the only non-firmware bound on a tile.

---

> **Rev 12 (2026-09-24) — the drive stage is specified two incompatible ways. Open item only; no value changes.**
>
> `NP-SOUP-LFS-001` Rev 8 §13.12, answering `OI-LFS-04` / `OI-NVRAM-10`, found that §6.2 and §8.1.1
> describe different circuits. §6.2 (inherited from `NP-HW-FPC-001` §6.2, whose text says *"the FETs
> switch LED current via series sense resistors"*) is a **switched** stage: tile-MCU PWM on a low-side
> N-FET, a sense resistor that the tile firmware reads for over-current. §8.1.1 is a **regulated**
> stage: *"constant-current control"* with a dropout. §4.3.1's claim that full drive *"cannot be
> commanded past its own optical limit even before firmware intervenes"* needs the regulated one, and
> §6.2's parts list has no regulating element. Its sense-resistor row also cites *"value set by string
> current, §6.3"* — §6.3 is the rigidizer fit. Raised as **`OI-HEXTILE-23`** (§11). It is the
> hardware half of `OI-LFS-04`: option A there (a fixed-reference regulating stage) resolves both.

---

> **Rev 11 (2026-09-21) — one of the three requirement failures recorded at Rev 10 was against a requirement that does not exist any more. Status only.**
>
> `NP-PROC-FPC-001` §2.3's *"Maximum junction temperature (Tj_max) ≥ 125 °C"* is **RETIRED** at that document's Rev 7. It was never derived — `125` appears nowhere else in the document set as a temperature and has no firmware referent — and it was removed rather than relaxed, on the principle that a figure nothing requires should not be a requirement, because an unnecessary constraint in a MANDATORY procurement document rejects usable parts. It had already done so once, to this document's own `OI-HEXTILE-02` candidate at `T_j` 115 °C.
>
> **No safety control is lost and nothing in this document's thermal reasoning changes.** Junction temperature is bounded by the 42 °C scalp / 62 °C throttle / 65 °C cutoff / ~70 °C PTC / 85 °C die-cutoff chain (`CLAUDE.md` §4.2, FMEA-M04) — firmware and hardware, not a supplier specification. §9.3's thermal argument, §8.1.1's residual budget and `OI-HEXTILE-18`/`-19` are untouched.
>
> **`OI-HEXTILE-02` now carries two open requirement failures for the Luminus family, not three** — the missing L70 figure and the wavelength bin. GitHub #333.
>
> ---
>
> **Rev 10 (2026-09-21) — a real emitter is put through §8.1.1 for the first time, and two of this document's own numbers do not survive it. `OI-HEXTILE-22` raised. No decision, no part selection, no interface change.**
>
> `NP-PROC-FPC-001` Rev 5 assessed the Luminus **SST-06 / SST-10-IRD-810** family against §2.2/§2.3 from the manufacturer's datasheets (PDS-003021 Rev 02, PDS-003022 Rev 01). Every previous candidate reached this document as a design target or an estimate; this is the first one carrying datasheet values at the operating point, and putting it through §8.1.1 returns three results, two of which are about **this document** rather than about the part.
>
> **1. The dead band is cleared, and §8.1.1's arithmetic holds.** Reading Δ`V_f` off each datasheet's `ΔV_f` vs `I_F` trace against its 350 mA binning reference gives `V_f` = **2.82 V** (SST-10) and **2.85 V** (SST-06) at 150 mA — **below** §8.1.1's 3.00–3.20 V dead band, not inside it. At **N = 8** that is 22.58 V, a 1.42 V residual and **5.9 %** linear overhead: inside §8.1's budget, and landing between the 2.80 V and 3.30 V rows §8.1.1 already tabulates. The read is self-validated — the trace returns Δ`V_f` = −0.005 V at 350 mA, where it is zero by construction.
>
> **2. §8.1.1's caveat 1 is right about the binning dependency and wrong about the margin. `NP-PROC-FPC-001` §2.1's ±0.10 V bin does not make fixed-N comfortable; it makes it critically sized.** Luminus bins in **0.2 V increments**, which meets §2.1 exactly as a standard shipping condition. But 0.2 V across N = 8 is **1.6 V of spread in `N · V_f`**, and §8.1's entire residual window is `(24 − V_dropout) − 22.4 = 1.6 − V_dropout`. **A fixed N = 8 holding across a full ±0.10 V bin therefore requires a driver dropout of zero.** Worked against the three bins this part actually ships in, only the lowest clears the 24 V functional ceiling across its whole width, and it does so while falling under the 22.4 V thermal floor at its low edge (20.98 V, 12.6 % overhead). This is **`OI-HEXTILE-18` and `OI-HEXTILE-19` arriving as a number instead of a warning**, and it is not specific to Luminus — any supplier's ±0.10 V bin does this at N = 8. §8.1.1 gains the worked table.
>
> **3. `OI-HEXTILE-22` — §4.1's 3.80 mm pitch was derived from areal density and edge clearance, and never checked against an emitter package.** A 3.80 mm triangular lattice puts the 60° neighbour at **(1.90, 3.29) mm**, so an axis-aligned square package must be **≤ 3.29 mm** or it overlaps its neighbour. The Luminus package is 3.45 mm square (3.65 mm at maximum material) and needs ≥ 4.21 mm. **So do two of the three parts already shortlisted in `NP-PROC-FPC-001` §2.6.2** — SFH 4703AS (3.85 mm) needs ≥ 4.45 mm and SFH 4718A (3.75 mm) needs ≥ 4.33 mm, both **above §4.1's own 4.04 mm ceiling at 91 sites**. Only L1IZ-0850 (1.9 × 1.37 mm) fits, and only with its long axis on the 60° axis. **The one shortlisted NIR part that fits the specified lattice is the 850 nm one, which is the part that fails the wavelength window** — a fact for `OI-LED-W1`'s owners, and not a decision taken here.
>
> **Nothing in §4.2, §5, §6, §7, §9 or §10 changes, and no emitter is selected.** §4.1 and §8.1.1 gain notes, §11 gains `OI-HEXTILE-22`, `OI-HEXTILE-02` gains the verified data. GitHub #333; trace in `docs/status/pending-decisions.md` §13.2e(i).

---

> **Rev 9 (2026-09-21) — `NP-PROC-FPC-001` Rev 4 discharges §8.1.1's caveat 1. Status only; no derivation, count, decision or interface in this document changes.**
>
> §8.1.1 caveat 1 recorded that fixed-N string construction depends on `NP-PROC-FPC-001` §2.1's ±0.10 V within-order Vf bin, and that the owning document *"currently justifies itself only on RISK-08 current-hogging grounds"*. **It no longer does** — `NP-PROC-FPC-001` Rev 4 §2.1 states the string-construction dependency and draws the consequence: on the current-hogging axis the bin degrades gracefully, on the string-construction axis it does not, so relaxing it removes the premise that a fixed N exists rather than widening an imbalance. A §2.5 "use-as-is" disposition at ±0.15 V is now assessed against string length too. The caveat is updated to say what the owning document says.
>
> **Two corrections in this document's own citations, found in the same pass.** (i) `OI-HEXTILE-02` cited the binning precedent as *"NP-PROC-FPC-001 §4.2"*; the binning specification is **§2.1** — §4 of that document is the Hirose connector. (ii) Every citation of that document in the active set called it **Rev 1**; it was at **Rev 3** (header stale at "Rev B" against its own Rev C history) and is now at **Rev 4**. Both fixed here.
>
> **What did not change, and is the whole point of saying so: no emitter is selected.** `OI-HEXTILE-02` is open; §4.3's V_f and radiant-flux figures are still design targets, not datasheet values; §8.1's worked strings — 11 × 2.10 V and 14 × 1.60 V — still rest on those targets, and `OI-LED-01`'s curve-read at 120–180 mA is not discharged for any candidate. `OI-HEXTILE-02` gains one input: a candidate family that is in-window on centroid, in the right package class, and **unverified** — see the item itself. GitHub #333; trace in `docs/status/pending-decisions.md` §13.2e(h).

---

> **Rev 8 (2026-08-21) — §9's concurrency ceiling measured against the authored protocol library. §9.2's "~6 tiles" is corrected to a range; `OI-HEXTILE-09` is confirmed as an actual gap, not a hypothetical one. No decision, derivation, count or interface changes.**
>
> `NP-SES-PWR-001` ran §9's arithmetic over `protocols/predefined/` — 20 protocols carrying a `pbm_transcranial` block — using this document's own §4.2/§4.3/§8.1 per-tile model. Three results land on §9 and on `OI-HEXTILE-09`.
>
> **1. `OI-HEXTILE-09` is real, and the margin is not small.** §9.3 raised it as a gap in the delivered wire format. **2 of 20 protocols fit the 40 W emitter budget as authored; 17 exceed it by 1.25× to 40×; 1 is operator-scoped and indeterminate. Every one compiles clean** — `app/web/src/lib/hubCompiler.ts` has no power or budget check of any kind.
>
> **2. §9.2's "~6 tiles" is an artefact of one operating point and should not be quoted as a rule.** It assumes **6.25 W/tile** (400 mW/cm², 25 % duty, both channels, 100 % intensity) and **no authored protocol runs there.** Real per-tile draw spans **1.3–20.0 W**, so the concurrency limit spans **2–32 tiles**. §9.2's table is arithmetically correct and is unchanged; what is added is that its rows are *operating points*, not a ceiling. **A tile-count governor is unsafe at one end and needlessly restrictive at the other** — it would permit 6 CW tiles (120 W, 2.7× R-10) while forbidding 32 low-intensity ones that fit. This is `NP-PWR-BUDGET-001` D-4 confirmed against data rather than argument.
>
> **3. The binding constraint is CW, not the lattice.** Pulsed protocols draw 1.3–5.0 W/tile and fail on *socket count* — they are authored against 37–80 sockets when 8–32 would fit. **CW protocols fail on per-tile draw**: one tile at 80 % CW is 20 W, half the entire emitter budget.
>
> **Three findings that belong elsewhere and are recorded there, not here.** (i) The dominant cause of the over-budget condition is that **protocols target lobe-scale zones where their own evidence specifies electrode-scale sites** — a clinical-validity defect that presents as a power number (`OI-SESPWR-01`). (ii) `frequency: 0Hz` combined with `duty_cycle:` is undefined and worth a **4× swing** in the budget of a fifth of the library (`OI-SESPWR-03`) — **this blocks `OI-HEXTILE-09`**, since a governor cannot be written against an undefined input. (iii) A cascade primitive would need a per-protocol admissibility flag defaulting to *no*, because time-multiplexing destroys the network-wide rhythmic drive every 40 Hz entrainment protocol claims, **including the Grade A Alzheimer's protocol** (`OI-SESPWR-04`).
>
> **Nothing in §4, §5, §6, §7, §8 or §10 is altered.** §9.2 gains a note; `OI-HEXTILE-09` gains the measured result and one new blocker.

---

> **Rev 7 (2026-08-21) — four decision inputs recorded against existing open items, and two new open items raised. No decision, count, derivation or interface in this document changes.**
>
> `NP-PWR-BUDGET-001` Rev 2 ran §9's arithmetic in directions this document had not, and grounded it against the efficacy band in `docs/pbm_neuro_protocols.md`. Four results bear on items this document owns. They are recorded here because `NP-CONV-001` §6 makes an open item the property of its document — the analysis lives in the power-budget study, and only the routing lives here.
>
> | Item | Input recorded |
> |---|---|
> | **`OI-HEXTILE-06`** | ***Populated* is not *driven*, and the cost decision has an argument on the other side.** Standby cost of a populated socket is **2 mA / 25 mW**, not 25 W (§8.3); a fully populated helmet under the §9 governor draws what a 20-tile build draws. Separately, §6.4 options 2 and 3 trade away the dual-PD dose metering — see the new paragraph at §6.4 for why that is a heavier cost than $9/tile |
> | **`OI-HEXTILE-09`** | **The governor must be denominated in watts against the negotiated PD contract, not in a tile count.** A "~6 tiles" rule is wrong in *both* directions — it forbids the affordable whole-vault mode of `NP-PWR-BUDGET-001` §3.6 and permits 6 tiles at full dual-channel drive (~150 W). `NP-DRV-SHELL-002` SH2-DRC-02b's pass condition should state the unit |
> | **`OI-HEXTILE-03`** | **Reframed, not closed.** It reads as a session-length verification; the underlying problem is *irradiance reachability* and it does not resolve by re-timing a protocol. Split out as `OI-HEXTILE-21` |
> | **§9.3** | A **fourth consequence** added. §9.3's *"placement options, not capability"* is **too strong**: whole-vault simultaneous illumination at ~30 W is a capability six tiles cannot produce at any drive level, and the Grade A Alzheimer's protocol asks for that geometry |
>
> **Two new open items, both raised rather than resolved.**
>
> **`OI-HEXTILE-20` — §8.1's 25.0 W/tile peak may not be a legal operating point.** §4.3.1 puts each T1-A channel at 403 mW/cm² at full 150 mA drive, so both channels simultaneously is **806 mW/cm² against R-5's 600 mW/cm² aggregate ceiling**. Either R-5 binds only the three-channel case its source addresses, in which case say so, or the true per-tile peak is **~18.6 W** and every figure derived from 25.0 W — the 1.04 A rail current, the 0.35 A/pin contact derating that set `VLED` at 3 contacts (D-5/D-6), §9's concurrency table — is conservative by ~25 %. **The contact count is the reason this is not editorial**: it is tooling-blocking under D-5.
>
> **`OI-HEXTILE-21` — the 1064 nm channel cannot reach its own flagship protocol's irradiance.** §4.3.2 gives CH_C **28 mW/cm²** at 30 sites; `pbm_neuro_protocols.md` grades cognitive enhancement **A** at **1064 nm CW, 0.25 W/cm², 60 J/cm², 8 min**. That is **9× short**, and a 90-site 1064-only tile reaches only ~85 mW/cm² — still 3× short. **This is an η_wp ≈ 4.8 % emitter wall, not a power or layout shortfall**: R-6 already caps drive current, so neither more watts nor more sites closes it. It bears directly on `OI-HEXTILE-02`'s successor question for CH_C and on whether the 1064 nm claim should be stated against the AD protocol (1060–1080 nm, 0.1–0.3 W/cm², which a tile *can* approach) rather than the cognitive one.
>
> **Nothing in §4 (emitter lattice), §5, §7 (socket interface), §8 or §10 (Decisions) is altered by this revision.** §6.4's option list and §9's tables are unchanged; both gain a paragraph.

---

> **Rev 6 (2026-08-18) — `OI-HEXTILE-14` CLOSED. Status only; no derivation, count or decision in this document changed.**
>
> Rev 5 arbitrated §8.4.1 and stated that *"**OI-HEXTILE-14 is NOT closed here** — the connector/conductor count remains its decision."* This is that decision.
>
> §8.2.2's recommendation — **options 1 + 3: provision 20 connector positions, adopt D-7's 32-segment tree** — is now applied across every peer. `NP-DRV-SHELL-002` **Rev 2** took it on 2026-08-11; `NP-HW-HUB-001` **Rev 6** (2026-08-18) brought the last one into line, re-sizing §7.4, §6.3, §5.2, §8.2/§8.5 and HUB-DRC-C02 off 12 (and off `ceil(n/8)` = 10) to **18**. Changed here: §8.2.2's status banner (PROPOSED → **ADOPTED**), `OI-HEXTILE-14` (**closed**), `OI-HEXTILE-10`'s count precondition (**satisfied**), **HT-DRC-20** (Open → **✓**), and the two §8.2.1 sentences that described peers as still carrying 12/10.
>
> **Option 4's condition is satisfied, but option 4 is still not taken.** Rev 5 accepted §8.4.1, which is exactly the condition §8.2.2 attached to removing `SAFE_EN[n]` from the tail (12 → 11 conductors, multi-drop trunk, connector count insensitive to cluster count). The reduction is therefore **unblocked** — and deliberately deferred: `NP-HW-HUB-001` **HUB-REQ-C05** requires the Class B per-cluster gate to be commanded from a tier *above* the cluster controller it gates, and until that command path is fixed it is not established that no per-cluster conductor is needed. Provisioning stays at the more forgiving 12 conductors / 20 positions. **Routed as a residual, not closed silently.**
>
> **The derivation is untouched.** 18 is still what §8.2.1 proves, and the agreement recorded at HT-DRC-20 is on the *derivation* — cluster count comes from the clamp partition, never from a division of the socket total — so a REG-1 re-cut re-derives rather than re-negotiates. 20 positions bound the re-cut that can be absorbed without a PCB re-spin.

> **Rev 5 (2026-08-16) — §8.4.1 ACCEPTED. OI-HEXTILE-13 closes. Documentation only; no firmware change, no hardware interface change.**
>
> Safety review (**OI-HUB-C07**) has arbitrated §8.4.1 and **accepted it**. The falsifier §8.4.1
> stated was searched for and **could not be produced**, and `NP-HW-HUB-001` Rev 4 §7.2.1 records why
> its absence is *structural*: no hazard in the tree has an extent of one cluster, because hazard
> extents are physical — a tile heats, a modality accumulates charge, a rail collapses — while the
> cluster is a clamp-plate and FPC boundary drawn for one-handed serviceability.
>
> | | Rev 4 | **Rev 5** |
> |---|---|---|
> | §8.4.1 status | PROPOSED, safety review arbitrates | **DECIDED — accepted** |
> | D-8 policy-bit count | "either 18 or 1" | **1** |
> | D-8 switch count | 18 | **18 — unchanged**, now explicitly IEC 62304 **Class B** |
> | §8.4 finding 1 (package unspecified) | blocking | **unblocked** — demand ~40 → ~23 I/O |
> | §8.4.2 re-layout | time-boxed, open | **not taken** — the word is unchanged, time-box moot |
> | §8.2.2 option 4 | conditional on §8.4.1 | **condition satisfied** — but the count is OI-HEXTILE-14's call |
>
> **Nothing in §4 (emitter lattice), §5, §6, §7 (socket interface) or §9 changes.** `NP-HW-HUB-001`
> Rev 4 and `NP-DRV-SHELL-002` Rev 3 are revised in the same change set. **OI-HEXTILE-14 is NOT
> closed here** — the connector/conductor count remains its decision.
>
> *Rebase note: this landed after `NP-HW-HEXTILE-001` Rev 4 (§8.1.1 forward-voltage
> budget) reached `main` independently, so this change is **Rev 5**. The two are disjoint — Rev 4
> touches §8.1/§8.1.1 and raises OI-HEXTILE-18/19; Rev 5 touches §8.4/§8.4.1/§8.2.2, D-8 and
> HT-DRC-13/19. Neither alters the other's sections.*

> **Rev 4 (2026-08-16) — §8.1's forward-voltage budget stated as a rule (new §8.1.1). No decision changed; nothing in §4, §7 or §9 is affected.**
>
> Rev 3 gave the budget only as two worked instances — `11 × 2.10 V` and `14 × 1.60 V` — and those
> instances were being cited elsewhere **as a forward-voltage constraint on the architecture**, which
> they are not: §4.3's V_f figures are design targets (OI-HEXTILE-02), and the string bound is a
> property of the **rail**, not of the emitters. `docs/status/pending-decisions.md` §13.2e traces where
> that misreading landed — a "~1.6–2.2 V the existing 660 nm string/driver architecture assumes" claim
> that was the sole stated electrical ground for excluding the only in-window NIR candidate under
> **OI-LED-W1**. §8.1.1 states the rule generally so a candidate emitter can be tested against it, and
> separates its two bounds, which are **different kinds of constraint**: `N · V_f ≤ 24 V − V_dropout`
> is a hard functional ceiling; `N · V_f ≥ 22.4 V` is a thermal budget allocation.
>
> Two gaps the generalisation exposed, both raised rather than closed: the **≤7 % allocation is
> asserted, not derived** (**OI-HEXTILE-18**, which also notes no minimum driver dropout is specified
> anywhere), and **no tolerance is stated on the 24 V rail** (**OI-HEXTILE-19**), so every string
> figure here is a nominal-point calculation. Also records that fixed-N string construction depends on
> `NP-PROC-FPC-001` §2.1's ±0.10 V Vf bin — load-bearing beyond the RISK-08 current-hogging grounds
> §2.1 gave for it at the time (**stated in §2.1 itself as of that document's Rev 4, 2026-09-21**). **No emitter was selected; OI-HEXTILE-02 and OI-LED-W1 remain open.**
>
> *Revision label note: the banner below is written "Rev C", the letter scheme in use when it was
> issued. Per `NP-CONV-001` §4.1 that maps positionally to **Rev 3**; §1.1 keeps historical records as
> written, so it is not renamed.*

> **Rev C (2026-08-11) — socket interface re-cut to 19 positions (§7.1–7.2, §7.3, §8.1). Hardware interface change; no firmware change requested here.**
>
> Closes `NP-DRV-SHELL-002` Rev 2 **OI-SHELL2-09(i)**, which flagged this document's 16-position interface as **blocking socket tooling** — a tooled interface, not a paper inconsistency.
>
> | Quantity | Rev 2 | **Rev C** | Cause |
> |---|---|---|---|
> | Socket positions | 16 | **19** | Two independent causes, below |
> | `VLED` / `PGND` | 4 + 4 | **3 + 3** | Principal decision 2026-08-11; ≥2× degraded-case rule (§8.1) |
> | `PD1_K`, `PD2_K`, `NTC` | absent (D-4) | **restored** | **OI-HUB-C17c resolved against D-4** — N3 survives |
> | `SYNC`, `DGND` | absent | **added** | REQ-EMI-03 phase reference (also OI-HUB-C05); REQ-EMI-07 return separation |
> | Pad layout | single row, 32 mm | **two staggered rows, ~18 mm** | Forced — 19 at 2.00 mm pitch does not fit a 40 mm hex in one row |
> | Current per `VLED` contact | 0.26 A | **0.35 A** nominal / **0.52 A** degraded | §8.1 |
> | Peak contact force per module | 4.8–8.0 N | **5.7–9.5 N** (34.2–57.0 N per 6-tile plate) | §7.1 |
>
> **The two causes are independent and should not be conflated.** Three of the four added positions come from a *decision reversal elsewhere* — `NP-DRV-SHELL-002` §3.3a keeps the TIA + ADC on the cluster controller, so network N3 still crosses the socket. Two more (`SYNC`, `DGND`) come from *requirements this document had not accounted for*: REQ-EMI-03 needs a deterministic phase reference that I2C broadcast cannot provide, and REQ-EMI-07 needs `PGND` to be LED return only. Against those five, `VLED`/`PGND` dropping 4+4 → 3+3 returns two positions.
>
> **What is NOT changed, deliberately.** **D-4's reasoning is not withdrawn** — §5.3's TIA-saturation analysis and its case for on-module conversion remain correct, and were outweighed rather than refuted (the conservative ADC-drift argument, FAI-SM-06). **§5.3 and §6 still read as though D-4 holds**, because rewriting them is a larger change than this revision's scope and would touch the driver topology, the BOM, and OI-HEXTILE-06. Raised as **OI-HEXTILE-15**. Nothing in §4 (emitter lattice), §9 (concurrency ceiling) or §8.2 (I2C fan-out) is affected.
>
> **`NP-HW-HEXTILE-001` §7.2 and `NP-DRV-SHELL-002` §5.1.4 now agree pin for pin.** They specify one physical interface; **HT-DRC-23** exists so that stays true.

---

> **Rev B (2026-08-04) — cluster-count correction. Documentation only; no firmware change.**
>
> Rev 1 §8.2 stated that NP-HEX-ZM-001 §5.4a "partitions ~80 tiles into 4–10 clusters". **4–10 was that document's figure for the retired 30-socket lattice** and was carried over to ~80 sockets without rescaling. Several component counts were sized off it. This revision derives the count from the lattice and the two standing principal decisions (**CLUSTER-1**, 7-hex flower; **SYM-1**, mirror-symmetric partition, 2026-08-04) and propagates it.
>
> | Quantity | Rev 1 | Rev 2 | Where |
> |---|---|---|---|
> | Clusters at n = 80 | 4–10 | **18** (provably minimal) | §8.2.1 |
> | D-8 VLED high-side switches | 4–10 | **18** | §8.4 |
> | I2C pull-up resistors | 20 (at 10 segments) | **36** (18 segments) | §8.2 |
> | I2C segments used of 32 available | "a small fraction" | **18 / 32 (56 %)** | §8.2 |
> | Cluster-controller boards / tier BOM | 10 / $63.40 | **18 / $114.12** | §8.2.1, NP-HEX-ZM-001 §4a |
>
> **Neither CLUSTER-1 nor D-7's muxing architecture changes.** One tier of PCA9548A muxing over LPI2C1–4 still suffices (32 ≥ 18). Two new open items are raised rather than papered over: **OI-HEXTILE-13** (whether per-cluster safety *policy* is wanted at 18 clusters — the same question NP-HW-HUB-001 §7.4 routes to OI-HUB-C07; the safety-MCU GPIO budget does *not* demonstrably close at 18, and the package is unspecified. **A proposed resolution is recorded at §8.4.1** — split the enable by IEC 62304 class — for safety review to accept or falsify, plus **§8.4.2**, which establishes what the Class C enable word can and cannot absorb — the active 38-byte heartbeat has **zero spare bytes**, and enable-bit positions double as charge-monitor channel indices, so only the §7.2 collapse-with-holes is genuinely free) and **OI-HEXTILE-14** (peer documents still sized off 10 or 12, and 18 exceeds the 16 cluster-tail connectors NP-DRV-SHELL-002 §7.1 provisions — **options and a recommendation at §8.2.2**).

---

> **⚠ READ FIRST — what this document is and is not.**
>
> NP-HEX-ZM-001 specifies the hex-tile **mechanical, socket, and addressing** model. This document specifies the **electrical and FPC** design inside one 40 mm tile, for the two PBM-only types (**T1-A** base, **T1-C** 1064 smart). It is the hex-tile replacement for the electrical content of the retired NP-HW-FPC-001.
>
> **T1-B (EEG/electrode tile) is deliberately out of scope for Rev 1** — its layout is a masking derivation from the lattice defined here (§4.5), but the pod clearance diameter that drives it is not yet fixed. **T2-D (1170 nm laser tile) is out of scope entirely** (laser drive ≠ LED drive).
>
> **Nothing here is looked up.** The retired document's LED counts, pitch, connector, and pinout were all derived from a 66 × 78 mm module and a 5-slot hub, and none of them survives the change of form factor or the SMART-1 decision. Every number below is derived in-line from a stated requirement and a stated assumption, and every assumption that is not yet backed by a datasheet or a bench result is named as such in §11.

---

## 1. Scope

Specifies the electrical design of the universal 40 mm hex tile as populated for **T1-A** (base PBM) and **T1-C** (1064 nm smart PBM):

1. Emitter lattice geometry, count, and wavelength allocation within the 40 mm hex footprint (§4)
2. Photodiode placement and the dose-metering signal chain (§5)
3. The on-module driver, and the decision to fit one to **every** tile type rather than only T1-C (§6)
4. The socket interface — contact technology, pin count, and pinout (§7)
5. Per-socket power and I2C delivery at ~80 sockets under SMART-1 (§8)
6. The concurrency ceiling that the existing power envelope imposes on whole-vault tiling (§9)

**Invariant inherited from NP-HEX-ZM-001 §4a and not re-opened here:** all tile types share one mould, one outline, one socket interface, and one orientation-only mechanical key. A "type" is an FPC population difference and nothing else. Consequently **the socket pinout is the union of every type's needs**, including types not specified in this revision — any pin that T1-B or a future type requires must be present at every socket. This is what makes §7 a **19**-position interface rather than a 10-position one. *(Rev B said 16; see the §7.1 Rev 3 banner. The union rule is unchanged — what changed is which networks cross the socket, after OI-HUB-C17c restored N3.)*

---

## 2. Requirements this design is derived from

| # | Requirement | Source |
|---|---|---|
| R-1 | Tile is a regular hexagon, 40 mm flat-to-flat, module cap radius R_m = 87 mm | NP-HEX-ZM-001 §3 |
| R-2 | Any tile type inserts into any socket; identity by UID self-report, not mechanical keying | NP-HEX-ZM-001 §4a |
| R-3 | Every socket is I2C- and TIA-capable (SMART-1) | NP-HEX-ZM-001 §4a, §7 |
| R-4 | PBM ceiling: **time-averaged Σ Ēᵢ / (200 × C_A(λᵢ)) ≤ 1 over the channels on the tile; 400 mW/cm² peak**, at the duty and mode the protocol specifies. One channel alone: 200 at 660 nm, ≈ 329 at 808 nm, ≈ 364 at 830 nm, and 400 at 1064 nm, where the peak binds before the reference's 1000. A protocol outside it is refused, never reshaped. The average terms are the laser skin reference (`NP-BIB-PBMIRR-001` §3.1, unverified until `OI-BIBPBM-01`). The 400 is a design figure with no counterpart in the standards, and above ≈ 850 nm it is the binding average bound (D-10). *(Rev 26, CLAUDE.md Rev 60: the per-channel 200 is removed, `OI-HEXTILE-32`. Rev 25, CLAUDE.md Rev 59. Was: "400 mW/cm² peak pulsed at ≤25 % duty; 200 mW/cm² CW".)* | CLAUDE.md §3 modality 1; `NP-BIB-PBMIRR-001` §3.1 |
| R-5 | Three-channel aggregate ceiling 600 mW/cm² | NP-FW-PBM1064-001 Rev 2 (OI-PBM-05) |
| R-6 | Emitter drive 120–180 mA for L70 80,000–100,000 h | CLAUDE.md §3 modality 1 |
| R-7 | Session dose 60 J/cm² (660/808 nm), 36 J/cm² (1064 nm) | NP-FW-PBM1064-001 Rev 2 |
| R-8 | Dual-PD dose metering: PD1 forward (behind PDMS), PD2 scalp-facing backscatter; ratio separates fouling from ageing | CLAUDE.md §3 (RISK-14 Option B) |
| R-9 | Per-tile NTC, hardware current throttle at 62 °C junction; scalp face ≤42 °C | CLAUDE.md §4.2; NP-THERM-CFD-R1-001 (Path B1) |
| R-10 | Headset power envelope: T1 standard ~17–20 W, T1 peak ~45–50 W | CLAUDE.md §4.5 |
| R-11 | Safety MCU physically owns stimulation enable; app crash cannot cause unsafe output | CLAUDE.md §4.2 |
| R-12 | Module UID drives auto-inventory; re-inventory only on UID change | NP-HEX-ZM-001 §4 |

---

## 3. Available area — the budget everything else spends

| Quantity | Value | Derivation |
|---|---|---|
| Tile flat-to-flat, W | 40.00 mm | R-1 |
| Tile circumradius, a = W/√3 | 23.09 mm | |
| Tile area, (√3/2)·W² | 13.86 cm² | NP-HEX-ZM-001 §3.1 |
| Perimeter bezel **lateral width** (assumed) | 2.50 mm | NP-HEX-ZM-001 §3.1 coverage column, an assumed input with no derivation — **OI-HEXTILE-01**. *Not* the 1.0 mm of NP-THERM-BEZEL-001, which is the bezel **height** (face-to-scalp standoff) and does not enter this table (Rev 16) |
| **Active field** flat-to-flat, W_a = W − 2·bezel | **35.00 mm** | |
| Active field circumradius, a_a | 20.21 mm | |
| **Active field area, A_a** | **10.61 cm²** | (√3/2)·35² = 1061 mm² |
| Active fraction | 76.6 % | matches the 77 % in NP-HEX-ZM-001 §3.1 ✓ |

The bezel has two dimensions, and only one of them is set. Its **height** `h_b` = **1.0 mm** (principal direction 2026-08-11; `NP-THERM-BEZEL-001` §4.5) is the face-to-scalp standoff. It governs the thermal decoupling and pod travel, and it does not appear in this table. Its **lateral width**, the band this table subtracts, has no derived value in the document set. The 2.5 mm used here is the `NP-HEX-ZM-001` §3.1 column input, carried as the working assumption (**OI-HEXTILE-01**).

The width's direction of change is not free. A narrower band enlarges A_a: at 1.0 mm, W_a = 38.0 mm and A_a = **12.51 cm² (+17.9 %)**. That lowers every §4.3 irradiance figure at a given drive current, which **loosens the thermal and aggregate-ceiling budgets but moves §4.3.1's design point away from the 400 mW/cm² ceiling**. It also raises the §4.1 pitch ceiling. The band cannot be narrowed freely either, because the perimeter band also has to hold the co-moulded gasket and its retention groove (`NP-TOOL-HEXTILE-001` F-TH-06/F-TH-07, THEX-MDR-08; the retired F-05 gasket alone was 2.5 mm wide). *(Rev 16: this paragraph previously treated the 1.0 mm height as a competing width and gave its A_a as 12.15 cm² (+14.5 %), which was also an arithmetic slip.)*

---

## 4. Emitter lattice

### 4.1 The lattice — one geometry for every tile type

**Decision (D-1): a 5-ring centered-hexagonal lattice of 91 sites, at 3.80 mm pitch, identical on every tile type.**

A centered hexagonal array of n rings holds 3n² + 3n + 1 sites. For n = 5 that is **91**, and the array's own boundary is a hexagon — so it registers to the tile outline with no wasted corners, which a square grid cannot do inside a hexagon.

| n (rings) | Sites | Max pitch inside a_a = 20.21 mm | Areal density at that pitch |
|---|---|---|---|
| 4 | 61 | 5.05 mm | 5.7 /cm² |
| **5 ★** | **91** | **4.04 mm** | **8.6 /cm²** |
| 6 | 127 | 3.37 mm | 12.0 /cm² |

Pitch is set to **3.80 mm**, not the 4.04 mm ceiling, leaving 1.2 mm of clearance between the outermost emitter sites and the active-field boundary for placement tolerance and the PDMS window edge bead. Array circumradius = 5 × 3.80 = **19.00 mm**.

> **⚠ The 1.2 mm of clearance is at the array *boundary*. Nothing in this section checks the pitch
> against the *footprint of the emitter that sits on each site* — and at 3.80 mm most of the
> shortlist does not fit (`OI-HEXTILE-22`, raised Rev 10).** A triangular lattice of pitch `p` puts
> the 60° neighbour at `(p/2, p√3/2)` = **(1.90, 3.29) mm**, so two axis-aligned square packages
> clear each other only if the package is **≤ 3.29 mm**. Against the parts actually shortlisted in
> `NP-PROC-FPC-001` §2.6.2 and §2.6.3:
>
> | Candidate | Package | Minimum pitch | At 3.80 mm |
> |---|---|---|---|
> | Lumileds L1IZ-0850 (850 nm) | 1.9 × 1.37 mm | 2.19 mm | ✓ fits, long axis on the 60° axis |
> | Luminus SST-06 / SST-10-IRD-810 | 3.45 mm sq (3.65 max) | 4.21 mm | ✗ overlaps |
> | ams-OSRAM SFH 4718A (860 nm) | 3.75 mm sq | 4.33 mm | ✗ overlaps |
> | ams-OSRAM SFH 4703AS (810 nm) | 3.85 mm sq | 4.45 mm | ✗ overlaps |
> | *Rev 20:* ams-OSRAM GH CSSRM5.24 (660 nm, CH_A) | 3.0 mm sq ±0.1 (3.1 max) | 3.58 mm | ✓ fits, 0.19 mm clearance at max material |
>
> Three of the four need more pitch than **4.04 mm**, which is this section's own ceiling at n = 5,
> so for them the conflict is not resolvable by spending the 1.2 mm boundary clearance — it costs a
> ring (n = 4, 61 sites, 5.05 mm available) or a different package. **The pitch and the emitter
> package have to be chosen together, and §4.3's irradiance figures depend on the outcome of both.**
> ~~A 45° package rotation clears the 3.45 mm part by ~0.03 mm, which is not a manufacturable margin.~~
>
> **⚠ CORRECTED Rev 19 (2026-09-28, GitHub #333): the struck sentence is wrong, and so is the "≤ 3.29 mm"
> reading of a rectangular package. Neither correction changes a ✗ in the table.**
>
> - **Rotation is no escape, and 45° is the worst orientation.** Two identical squares of side `s`, at
>   centre separation `p` along a direction at angle `α` to the package edges, clear iff
>   `p · max(|cos α|, |sin α|) ≥ s`. Rotating every package by `θ` puts the three neighbour directions
>   at `−θ`, `60° − θ` and `120° − θ`, which modulo 90° are three points 30° apart. One of them is
>   always within 15° of the package diagonal, so **`s ≤ p · cos 30° = p√3/2 = 3.29 mm` for every
>   `θ`**, and that bound is reached at `θ = 0` (mod 30°). At `θ = 45°` the 0° neighbour sits on the
>   diagonal and **`s ≤ p/√2 = 2.69 mm`**. The 3.45 mm part overlaps by 0.76 mm there. It does not
>   clear by 0.03 mm.
> - **Per-sublattice orientation does no better, on a numeric search.** §4.2's 3-colouring allows a
>   separate angle per colour. A separating-axis check over both relative angles on a 3° grid found
>   no arrangement above 3.29 mm. That is a search, not a proof. The **density bound** holds for any
>   orientations: `s² ≤ (√3/2)·p²`, so `s ≤ 3.54 mm`. It is exact only as the array grows, so it
>   corroborates rather than proves. SST-06/10 at maximum material (3.65 mm), SFH 4718A (3.75 mm)
>   and SFH 4703AS (3.85 mm) all exceed it, so **no orientation scheme of any kind** seats them at
>   3.80 mm.
> - **A rectangle fits an envelope, not a single axis.** An `L × W` package with `L` along a lattice
>   row clears its row neighbour iff `L ≤ p`, and clears its 60°/120° neighbours at
>   `(p/2, p√3/2)` iff `W ≤ p√3/2` (for `L > p/2`). So the site envelope is **3.80 × 3.29 mm,
>   long axis on a row**, and a square package is the special case `≤ 3.29 mm`. A **2835**
>   package (3.5 × 2.8 mm) therefore **fits** row-aligned, with 0.30 mm and 0.49 mm clearance. It
>   overlaps only when turned across the row. The L1IZ-0850 row's 2.19 mm is the long-axis-across
>   figure. Row-aligned, its minimum pitch is `max(1.90, 1.37 · 2/√3)` = **1.90 mm**. Either way the
>   ✓ stands.
>
> No document states a placement tolerance or courtyard for these sites, so none of these
> clearances is yet a pass. That margin is part of `OI-HEXTILE-22` and is not set here (CLAUDE.md
> §18).

This independently reproduces the estimate NP-HEX-ZM-001 §3.1 carried without deriving — *"the densest tile (tri-wavelength PBM ~90 elements at ~3.5 mm pitch)"*. That the row-construction guess and this packing derivation agree at ~90 is the same two-independent-ways corroboration the parent document applied to the socket count.

**Site 0 (the array centre) is reserved and carries no emitter on any type.** On T1-A and T1-C it is the PD1 aperture (§5.1); on T1-B it is the electrode pod axis (§4.5). Reserving it once, for all types, is what lets a single lattice serve every population. **90 emitter sites remain.**

### 4.2 Wavelength allocation

A triangular lattice is exactly 3-colourable (each site's six nearest neighbours are three of one other colour and three of the third), which makes the 3-wavelength case the natural one and the 2-wavelength case a merge of two colours.

| Type | CH_A 660–670 nm | CH_B 808–830 nm | CH_C 1064 nm | Total emitters |
|---|---|---|---|---|
| **T1-A** | 45 | 45 | — | **90** |
| **T1-C** | 30 | 30 | 30 | **90** |
| *T1-B (out of scope, §4.5)* | *~22* | *~22* | *—* | *~44* |

T1-A interleaves CH_A and CH_B by alternating lattice rows. A triangular lattice contains odd cycles and therefore admits no perfect 2-colouring; row alternation leaves same-wavelength adjacency along one of the three lattice axes. This is accepted — at 3.8 mm pitch behind a diffusing PDMS window with several mm of standoff, per-wavelength granularity below the optical mixing length is not observable at the scalp, and NP-OPT-PSF-001 establishes that spatial structure well below the ~26 mm resolution floor buys nothing at depth.

### 4.3 Irradiance — does 90 emitters actually reach spec?

**Assumed emitter performance (OI-HEXTILE-02 — no base-tile emitter part is selected; these are design targets the eventual part must meet, not datasheet values):**

| Channel | V_f at 150 mA | Radiant flux at 150 mA | Implied WPE |
|---|---|---|---|
| CH_A 660–670 nm | 2.10 V | 95 mW | 30 % |
| CH_B 808–830 nm | 1.60 V | 95 mW | 40 % |
| CH_C 1064 nm | 1.40 V | 10 mW | 4.8 % |

> **Rev 20 — the datasheet values, read at 150 mA (`OI-LED-01`; `NP-PROC-FPC-001` Rev 9 §2.6.3).**
> These do not replace the targets above, because no part is selected. They show how far each
> candidate sits from the targets:
>
> | Candidate | Channel | V_f at 150 mA | Radiant flux at 150 mA | WPE | 45 sites at 150 mA |
> |---|---|---|---|---|---|
> | ams-OSRAM GH CSSRM5.24 | CH_A | **1.78 V** | **~233 mW** typ (≥ 218 mW, group V7) | ~87 % | ~990 mW/cm², 2.5× R-4 |
> | ams-OSRAM SFH 4718A (850 nm centroid) | CH_B | **1.41 V** | ~110 mW typ | ~52 % | ~467 mW/cm², 1.2× R-4 |
> | Lumileds L1IZ-0850 | CH_B | **2.62–2.87 V** (trace fails self-check) | ~162 mW typ (≥ 139 mW) | ~38–41 % | ~688 mW/cm², 1.7× R-4 |
> | Luminus SST-10-IRD-810 (Rev 10) | CH_B | 2.82 V | ~220 mW | ~52 % | ~930 mW/cm², 2.3× R-4 |
>
> **Every candidate exceeds its 95 mW target, so none reaches R-4 "by construction" at 150 mA.**
> Each needs a lower drive current or fewer sites (`OI-HEXTILE-20`, `OI-HEXTILE-25`). For the
> 660 nm primary, the lower current is not available: `OI-HEXTILE-29`.

> **Rev 22 — the three dimmer 660 nm candidates, off their datasheets (`OI-HEXTILE-29`).** The
> relative-flux curves were digitised: the GH DASPA2.24 and DS237 traces from their vector paths,
> and the Luminus trace from its raster. Irradiance is at 45 sites on 10.61 cm², typical flux.
>
> | Candidate | Typ. flux at test current | Lowest datasheet point | 45-site floor there | Current for 121 → 322 → 400 mW/cm² | `I_F` max |
> |---|---|---|---|---|---|
> | ams-OSRAM GH DASPA2.24 (QMRK) | 98.3 mW at 100 mA (89–130) | **30 mA, rated minimum** | ~125 mW/cm² | ✗ → 77 → 96 mA | 250 mA |
> | Luminus MP-2835-1100-DR | 63 mW at 60 mA (≥ 50) | 10 mA, curve start; **no rated minimum** | ~42 mW/cm² | 28 → 72 → 88 mA | 200 mA |
> | Lumileds L1SP-DRD0002800000 | 0.62 µmol/s ≈ 112 mW at 120 mA | 25 mA, curve start; **no rated minimum** | ~99 mW/cm² | 30 → 80 → 100 mA | 250 mA |
>
> The DS237 flux is converted from PPF at 660 nm (0.181 J/µmol). The Luminus curve reads 0.16 at
> 10 mA and 0.32 at 20 mA of its 60 mA value, which is linear through the origin. Its `V_f` trace
> reads 1.94 V at 60 mA against a table typical of 2.0 V, inside the ±0.1 V tolerance. **All three
> fit §4.1:** GH DASPA2.24 is 2.2 × 2.6 mm ±0.05, so it fits at any rotation. The two 2835 parts are
> 3.5 × 2.8 mm (Luminus ±0.15) and fit row-aligned only.

1064 nm is an order of magnitude worse because it is far off the efficient direct-bandgap window; the retired document's part (EPITEX L1064-02AU, NP-PROC-FPC-1064-001) remains the reference and its low flux is the reason the retired design needed 150 emitters to do anything. That constraint does not go away in a smaller tile — it is why §4.3.2 concludes what it does.

**4.3.1 T1-A, per channel at full 150 mA drive:**

E = N · Φ / A_a = 45 × 95 mW / 10.61 cm² = **403 mW/cm²**

This lands, by construction, on the **400 mW/cm² pulsed peak ceiling** (R-4). That is the intended design point: **full drive at the top of the L70 current window equals the firmware-enforced peak ceiling**, so the array cannot be commanded past its own optical limit even before firmware intervenes, and CW operation at the 200 mW/cm² ceiling runs at roughly half current (≈75 mA), comfortably inside the L70 window and at better WPE.

> **Rev 13 — both claims in that paragraph change under D-9, and neither is decided here (`OI-HEXTILE-25`).**
> **(a)** The *"cannot be commanded past its own optical limit"* claim now has a circuit behind it
> (§6.2 U3). But 403 mW/cm² is 0.75 % **over** the ceiling U3 enforces, so `I_cap` sits below 150 mA:
> ~148.9 mA at the design-target flux, less the regulation tolerance and the flux bin.
> **(b)** U4 forbids a gate held on for more than 50 % of any window, at any current. CW at
> 200 mW/cm² therefore becomes ~50 % PWM at `I_cap`, and DC at ~75 mA is no longer available. The
> WPE advantage is forgone, and the difference is heat, a §9.3 term.

**4.3.2 T1-C, all three channels:**

| Channel | N | E at 150 mA | Session time to reach dose (R-7) |
|---|---|---|---|
| CH_A 660–670 | 30 | 269 mW/cm² | 60 J/cm² ÷ 0.269 W/cm² = 3.7 min |
| CH_B 808–830 | 30 | 269 mW/cm² | 3.7 min |
| CH_C 1064 | 30 | **28 mW/cm²** | 36 J/cm² ÷ 0.028 W/cm² = **21 min** |
| **Aggregate** | 90 | **566 mW/cm²** | vs 600 mW/cm² ceiling (R-5) ✓ |

Two results worth stating plainly:

- **The aggregate ceiling is satisfied with 5.7 % margin at full drive on all three channels simultaneously** — not by firmware throttling, but by the emitter count itself. That is the desirable ordering: the hardware cannot exceed the limit that firmware also enforces.
- **1064 nm session length is set by emitter efficiency, not by protocol choice.** 21 minutes at full drive is a plausible session, but it is the floor — there is no headroom to shorten it, and any 1064 nm protocol shorter than ~21 min cannot reach the 36 J/cm² dose in a 40 mm tile. This is a real narrowing versus the retired 66 × 78 mm module and it should be checked against the clinical protocols in `protocols/predefined/clinical-03-pbm-cognitive-1064.npps` before this layout is tooled (**OI-HEXTILE-03**).

### 4.4 Uniformity

The retired 6 mm-pitch design claimed ±15–25 % irradiance variation. At 3.80 mm pitch the source spacing is 37 % smaller and the emitter count per unit area is 2.7× higher, so variation should improve materially — but **this document does not assert a number.** Uniformity depends on emitter beam angle, PDMS diffuser scattering coefficient, and window standoff, none of which is fixed yet. The claim is deferred to the illumination model (**OI-HEXTILE-04**); the GATE-2 coupling bench in NP-HEX-ZM-001 §7 is the measurement that settles it.

What can be said without a model: the **inter-tile** seam, not the intra-tile pitch, is now the dominant uniformity term. A 2.5 mm bezel band on each of two adjacent tiles puts 5 mm of unpopulated width between the outermost emitters of neighbouring tiles — larger than the 3.8 mm intra-tile pitch. Whole-vault uniformity is therefore a bezel-**width** problem, which is a reason for OI-HEXTILE-01 to derive the narrowest band that still holds the gasket and groove. *(Rev 16: this sentence previously pointed at "the 1.0 mm figure". That figure is the bezel height, which does not change the seam.)*

### 4.5 T1-B — why it is a masking derivation, not a separate layout

T1-B is out of scope for Rev 1, but the lattice was chosen so that T1-B is a depopulation of it rather than a new design. Depopulating whole rings around the reserved centre opens a circular clearance for the spring electrode pod:

| Rings depopulated | Sites removed | Emitters remaining | Clear diameter |
|---|---|---|---|
| 0–1 | 7 | 84 | 7.6 mm |
| 0–2 | 19 | 72 | 15.2 mm |
| 0–3 | 37 | 54 | 22.8 mm |

NP-HEX-ZM-001 §4a states T1-B has "~half the LED count", which corresponds to depopulating rings 0–3. The actual choice is set by the pod body diameter, which is not specified anywhere in the current document set (**OI-HEXTILE-05**). Whatever it resolves to, T1-B needs no new FPC outline, no new lattice, and no new socket interface — only a different placement file.

---

## 5. Photodiodes

### 5.1 PD1 — forward emission, at site 0

**Decision (D-2): PD1 occupies the reserved centre site, on the emitting face, behind the PDMS window.**

Reusing the array centre gives PD1 a position that is (a) identical across types, (b) already excluded from the emitter placement, and (c) at the point of maximum optical symmetry, so the PD1 reading is insensitive to which lattice axis an individual emitter batch is weak on. The centre reads above the field spatial average because of edge roll-off; this is a fixed multiplicative offset absorbed by the **existing** per-wavelength K coefficients written at factory calibration (NP-FW-PBM1064-001 Rev 2 §6.6, integrating-sphere procedure) — no new calibration mechanism is introduced.

- **Component:** InGaAs, Hamamatsu G12180-010A or qualified equivalent — inherited unchanged from NP-HW-FPC-001 Rev 5 §5.1, which its supersession note lists as still-reusable.
- **Fitted on both T1-A and T1-C.** ⚠ *Rev 14: the premise of this bullet is contradicted by the part's published 0.9–1.7 µm range (see the Rev 14 banner; `OI-HEXTILE-26`). Retained as written.* InGaAs is broadband (600–1700 nm), so one part covers both the 2-channel and 3-channel populations and there is one PD SKU across the tile family. Per-wavelength dose separation is by firmware time-multiplexing and K coefficients, exactly as NP-FW-PBM1064-001 Rev 1 §6.2 already specifies.
- **Pad geometry:** 1.6 mm annular ring, hard gold ≥0.5 µm cobalt-alloyed — inherited unchanged.

Fitting InGaAs to T1-A (which has no 1064 nm channel and could use cheaper silicon) is a deliberate cost-for-uniformity trade: one PD part number across all tiles, no per-type TIA gain question, and no possibility of a silicon-PD tile being calibrated with InGaAs coefficients. See D-4 for why the gain question disappears entirely.

### 5.2 PD2 — scalp-facing backscatter

Co-located with PD1 in XY, on the opposite (scalp-facing) copper layer. The PD1/PD2 ratio is the fouling-versus-ageing discriminator (R-8), and that logic is only valid if both photodiodes sample the same optical path — co-location is load-bearing, not incidental. This is inherited directly from NP-HW-FPC-001 Rev 5 §5.2 and is the one geometric relationship that carries over from the retired design unchanged.

Same part, same pad geometry as PD1.

### 5.3 The TIA question — and why it stops being a hub problem

NP-HW-FPC-001 Rev 5 §5.3 established that InGaAs responsivity at 1064 nm (~0.90 A/W) is ~2× silicon at 808 nm (~0.47 A/W), saturating a 47 kΩ hub-side TIA, and resolved it with a per-slot DG2788A gain switch. SMART-1 turns that per-slot fix into a per-socket one, which NP-HEX-ZM-001 §4a and NP-HW-HUB-001's supersession note both flag as unscoped Hub PCB NRE at ~80 sockets.

**Decision (D-4): the TIA and its ADC move on-module. No PD analog signal crosses the socket interface.**

The saturation analysis in NP-HW-FPC-001 Rev 5 §5.3 remains correct physics; what changes is where it is solved. On-module:

- The gain is fixed at design time to the PD actually fitted, because the module knows what it is. There is no runtime gain-switch, no `GAIN_SEL` line, no DG2788A, and no ZONE_ID-to-gain sequencing hazard (the retired NP-HW-HUB-001 §5.1 ordering requirement disappears rather than scaling to 80).
- No high-impedance analog signal is routed across a spring contact and a metre of FPC through a shell that also carries LED switching currents and sits millimetres from µV EEG leads. This was already marginal at 5 slots; at 80 it is the dominant noise-injection path in the system.
- **The ~80× DG2788A + cascaded-analog-mux Hub PCB NRE that SMART-1 opened is not redesigned — it is deleted.**

Cost of the decision: the on-module MCU's ADC now sets dose-metering resolution. The ATtiny402 named in the retired design has a 10-bit ADC, which is thin for a dose claim that is a stated competitive differentiator. **A tinyAVR 2-series part (ATtiny426/427-class, 12-bit ADC with PGA) is specified instead** — same architecture, same UPDI programming, same package family, ~$0.10–0.15 more. See §6.2.

---

## 6. On-module driver

### 6.1 Every tile carries a driver — not just T1-C

**Decision (D-3): the on-module driver, which NP-HEX-ZM-001 §4a describes as a T1-C distinguishing feature, is fitted to every tile type.**

This is the largest single departure from the retired architecture and it is forced, not chosen. The retired 20-pin connector spent **16 of 20 pins on LED anode/cathode pairs** carrying drive current from hub-side drivers. Scaling that to the hex lattice:

- 80 sockets × 3 channels = **240 hub-side constant-current driver channels**, versus 15 in the retired design.
- 80 sockets × up to 16 current-carrying conductors = **~1,280 power conductors** to route from the hub, through the posterior blind-mate boss (NP-HEX-ZM-001 §5.3c), across the two-bowl parting plane, and out to the tile field.
- Every one of those conductors is a dI/dt source running alongside the EEG harness inside the Faraday envelope, in a product whose primary technical claim is measured EMF shielding.

None of that is buildable. Moving the driver on-module reduces the socket's power interface to a single DC rail pair and reduces the hub's job from *driving* 240 channels to *supplying* one bus and *commanding* over I2C. Combined with D-4, the socket interface becomes fully type-independent, which R-2 requires anyway.

**The cost is real and is not hidden:** see §6.4.

### 6.2 Driver topology

Inherited in concept from NP-HW-FPC-001 Rev 5 §6.2 — its supersession note lists the ATtiny + N-FET architecture as still-reusable — and re-validated here for the smaller tile.

| Ref | Function | Part class | Notes |
|---|---|---|---|
| U1 | I2C slave MCU, 3× PWM, ADC | tinyAVR 2-series, ATtiny426/427-class, SOIC-8/SOT-23-8 **⚠ Rev 24: inconsistent. The 8-pin tinyAVRs are the 0/1-series; the firmware needs 8–15 I/O, i.e. a 20-pin-class part (`NP-FW-HEXTILE-001` §12, `OI-FWTILE-03`)** | 12-bit ADC + PGA for PD metering (§5.3); TWI address-match wake from standby (§8.3). *Rev 24: firmware in `NP-FW-HEXTILE-001`. No PD metering on U1 (D-4 not adopted); RT1 is not reachable by U1 on the §7.2 pinout (`OI-FWTILE-01`)* |
| Q1–Q3 | Low-side N-MOSFET per channel | IRLML6344-class, SOT-23 | V_GS(th) 0.4–1.0 V, fully enhanced at 3.3 V gate drive |
| R1–R3 | Current sense per channel | 0.5 Ω / 1 Ω, 0402, 1 % | ~~value set by string current, §6.3~~ *(Rev 13: that citation was wrong — §6.3 is the rigidizer fit.)* Under D-9, R = `V_ref` / `I_cap`, derived from U3's reference (`REQ-TDRV-01`) — **`OI-HEXTILE-24`** |
| U3 *(Rev 13, D-9)* | Regulating stage, CH_A/CH_B — **fixed** reference + error amplifier driving Q1/Q2, or a constant-current regulator IC | **not selected** | `REQ-TDRV-01`: conducting current ≤ `I_cap`, set by a reference no firmware, register or stored value can raise. U1's PWM gates it on and off; it cannot move the ceiling. **Its dropout is §8.1.1's `V_dropout`** |
| U4 *(Rev 13, D-9)* | Gate-duty limiter, CH_A/CH_B — between U1's PWM output and the gate | **not selected** | `REQ-TDRV-02`: conduction ≤ 50 % over any window `T_w` ≥ 250 ms, at its own upper tolerance limit. It acts on the gate path, so it holds when U1 is wedged or wrong. The form (monostable with a minimum off-time, or an RC integrator and comparator) is EE's |
| D1 | PD1 (InGaAs) | G12180-010A | §5.1 |
| D2 | PD2 (InGaAs) | G12180-010A | §5.2 |
| U2 | Transimpedance amplifier, dual | single-supply, rail-to-rail, SOT-23-8 | fixed gain per fitted PD (D-4) |
| RT1 | NTC thermistor | 10 kΩ, B25/85 3435 | on-module; read by U1 ADC, §6.5 |

**CH_C is not in `REQ-TDRV-01` or `-02`** (Rev 13): at full drive it reaches 28 mW/cm² (§4.3.2), below the CW ceiling even held on continuously, so neither bound is required by R-4 on that channel. Fitting U3/U4 to CH_C anyway, so that the three channels share one layout, is EE's choice (`OI-HEXTILE-24`).

**T1-A fits the identical assembly with Q3, R3, and the CH_C string omitted.** One rigidizer artwork, one pick-and-place program with a depopulation variant — the same "population differs, geometry does not" principle the mould already follows.

The FET thermal result from NP-HW-FPC-001 Rev 5 §6.2 carries over unchanged and with margin: at 180 mA and R_DS(on) = 27 mΩ, P = I²R = **0.87 mW** per FET. Dissipation is not a constraint on the driver; it is a constraint on the emitters (§9.3).

### 6.3 Physical fit — the 22 × 14 mm rigidizer in a 40 mm hex

The retired design placed the driver on a 22 × 14 × 0.8 mm FR4 rigidizer in a 24 × 16 × 3.5 mm cavity (NP-TOOL-ZM-SM-001 Rev 1 §4). The concern raised when this task was scoped — that this will not fit a much smaller tile — resolves favourably, for a geometric reason:

- A 22 × 14 mm rectangle has a half-diagonal of 13.0 mm. The 40 mm hex has an **inradius of 20.0 mm**. The rigidizer fits inside the tile outline with 7 mm of margin on every side. In-plane area was never the binding constraint; the retired module was simply large enough that nobody had to check.
- The binding constraint is **z**, and it is satisfied by the concave-shell geometry NP-HEX-ZM-001 §3.4 establishes: tiles tessellate at their **innermost** (emitting) faces and splay outward into the shell, "where gaps are harmless." The driver mounts on the **reverse (shell-facing) face of the tile FPC**, in exactly that roomy volume. It does not compete with the emitter field for area, because they are on opposite faces.

**Consequence for tooling:** because every type now carries a driver (D-3), the rigidizer cavity is a **standard feature of the universal hex-tile mould**, not a variant. This removes the last reason for a smart-module mould variant, complementing NP-HEX-ZM-001 §4a's removal of `OI-SM-SHELL-01` — the retired NP-TOOL-ZM-SM-001 has no successor, and none is needed.

### 6.4 BOM — stated, because it is a programme-level number

NP-HEX-ZM-001 §4a records the per-socket smart-capability cost as *"Known cost, not yet quantified."* Quantifying it is a deliverable of this document, and the answer is large enough to require a decision rather than an acknowledgement.

| Item | Unit | Per tile |
|---|---|---|
| U1 tinyAVR 2-series | $0.45–0.55 | $0.50 |
| Q1–Q3 N-FET (×3; ×2 on T1-A) | $0.12 | $0.36 |
| U2 dual TIA | $0.25–0.40 | $0.32 |
| D1/D2 InGaAs PD (×2) | $4.00–6.00 | **$10.00** |
| R/C passives, NTC, sense resistors | — | $0.20 |
| Rigidizer PCB (FR4, 2-layer) | $0.15 | $0.15 |
| **Driver + metering sub-total per tile** | | **~$11.53** |

**At 80 populated sockets this is ~$920 per headset** — against a Home Standard BOM of $405 (CLAUDE.md §2.1). The dominant term is not the driver at all; it is **two InGaAs photodiodes per tile, at ~$10 of the ~$11.50.**

This is a genuine programme-level finding and it is not resolvable inside a hardware layout document. Three directions, in rough order of attractiveness:

1. **Do not populate all sockets.** §9 shows the power envelope permits only ~5–6 tiles to run concurrently regardless; the lattice's value is *placement freedom*, not simultaneous activation. A build populating 20–30 tiles retains full protocol flexibility at a quarter of the cost. This is a configuration decision the product tiers already support.
2. **Silicon PD on T1-A.** Reverting §5.1's one-PD-SKU choice saves ~$9/tile on the majority type, at the cost of reintroducing a per-type calibration distinction (which D-4 makes safe, since gain is now set on-module per fitted part).
3. **One PD pair per cluster, not per tile.** Breaks the per-tile J/cm² metering claim, which NP-HEX-ZM-001 §4a explicitly protects ("each tile meters itself"). Listed for completeness; not recommended.

Routed to **OI-HEXTILE-06** for a cost/scope decision by the principal. **No option is selected here** — this document's job is to make the number visible, not to trade away a stated product claim.

> **Added Rev 7 — what options 2 and 3 actually cost, which the BOM line does not show.** Both
> reduce or remove the dual-PD metering of R-8. That metering is not only a feature; it is the
> structural answer to the failure mode this product category is known for. Independent testing of a
> marketed 1070 nm helmet found a **−79 % gap between declared and measured scalp power density**
> (`docs/reference/competitive-position.md`), and `pbm_neuro_protocols.md`'s dosimetry lesson 1
> attributes most negative trials to under-dosing rather than mechanism failure. **A device that
> cannot measure its own delivered dose cannot distinguish itself from one that under-delivers by
> 5×, and cannot detect it in the field either.** Weigh that against ~$9/tile before selecting
> option 2 or 3. Option 1 (partial population) is the only one of the three that leaves the metering
> claim intact on every tile that exists — and `NP-PWR-BUDGET-001` §3.6 gives it an argument in the
> other direction, since a whole-vault mode wants *more* sockets populated, not fewer. **The two
> pressures are genuinely opposed and the decision is still the principal's.**

### 6.5 Firmware boundary

The on-module MCU firmware is a new software item. It implements the register map already defined in NP-FW-PBM1064-001 Rev 1 §5.1 (registers 0x00–0x0D) extended with PD1/PD2 ADC readback registers made necessary by D-4, plus the on-module NTC and the local 62 °C throttle (R-9).

**The local throttle is a hardware-adjacent Class C-adjacent function and must be independent of hub commands** — a tile must throttle itself on over-temperature even if the I2C bus is silent. The safety MCU's independent backstop is the per-cluster VLED gate (§8.4), not per-socket, because an STM32G071 does not have 80 spare GPIOs. This two-level arrangement — fine-grained on-module, coarse hardware cut at the cluster — is proposed as the resolution to NP-HEX-ZM-001 §7 **OI-HUB-SOCKET-01**, which currently states that socket-addressed commands are dropped because `NP_SAFETY_EN_PBM_ZONE_0..4` is per-zone-slot.

Firmware specification is **not** in this document; it is **OI-HEXTILE-07** (successor to the retired NP-FW-ZM-TINY402-001 / OI-PBM-08).

**Rev 24: it is `NP-FW-HEXTILE-001` (`docs/np_fw_hextile_001.md`), which closes `OI-HEXTILE-07`.** Two premises of the paragraphs above do not survive D-4's non-adoption, and that document says so: there are no PD1/PD2 ADC readback registers, and the local throttle needs a temperature input U1 does not have (`OI-FWTILE-01`).

---

## 7. Socket interface

### 7.1 Contact technology

The retired design used a Hirose FH34S 20-pin 0.5 mm ZIF with a back-flip lever. **That is not usable here.** A ZIF lever must be manually actuated per connector; NP-HEX-ZM-001 §5.4a's whole premise is that a user with Parkinson's H&Y II–III swaps tiles by throwing one cluster clamp and lifting the tile out on its ejector spring. A lever per tile contradicts the accessibility requirement the cluster clamp exists to satisfy.

**Decision (D-5, Rev 3): 19-position spring-contact (pogo) interface at 2.00 mm pitch, in two staggered rows. Spring pins on the socket, flat pads on the module.**

> **⚠ Rev 2 said 16 positions in a single row. Both figures are superseded, and the two changes have
> different causes — neither is a correction of an arithmetic slip.**
>
> | | Rev 2 | **Rev C** | Cause |
> |---|---|---|---|
> | Positions | 16 | **19** | §7.2 — three signals returned that **D-4** had removed, plus three the interconnect needs |
> | `VLED` / `PGND` | 4 + 4 | **3 + 3** | `NP-DRV-SHELL-002` Rev 2 §5.1.5, **principal decision 2026-08-11** |
> | Row layout | single row, 32 mm | **two staggered rows, ~18 mm** | forced by 19 at 2.00 mm pitch — see below |
> | Current per contact | 0.26 A | **0.35 A** nominal, **0.52 A** on loss of one | §8.1 |
>
> **What changed and why.** `NP-DRV-SHELL-002` Rev 2 §3.3a resolved **OI-HUB-C17c against D-4**
> (principal direction, 2026-08-11): the switched-gain TIA, PD mux, NTC mux and ADC stay on the
> cluster controller rather than moving on-module. **Network N3 therefore survives**, and with it
> `PD1_K`, `PD2_K` and `NTC` at the socket. That single decision accounts for three of the four added
> positions; `SYNC` and `DGND` are the fourth and fifth, and `SEAT#` — this document's own
> contribution — is retained. **Rev B's own §7.2 count was correct given D-4; D-4 no longer holds.**
>
> **`VLED`/`PGND` 4+4 → 3+3 is a separate decision with its own rule**, and the rule matters more
> than the number: *`VLED` is sized so the loss of any one contact still leaves ≥2× derating against
> the contact rating.* Rev 2's 4+4 gives ~3× degraded and was never wrong — it was more than the rule
> requires, at the cost of two extra contacts on an interface with a live RISK-22 one-handed-force
> constraint. See `NP-DRV-SHELL-002` §5.1.5 for the 2-vs-3-vs-4 comparison in full.

| Property | Value | Rationale |
|---|---|---|
| Positions | **19** | §7.2 |
| Pitch | 2.00 mm | unchanged |
| **Row layout** | **two staggered rows, nominally 9 + 10** | **Forced, not preferred.** A 19-position single row spans 36 mm pad-centre to pad-centre (38 mm including pads), which fits a 40 mm flat-to-flat hex only along the 46.19 mm vertex-to-vertex diagonal and tapers into the corners exactly where the ±0.4 mm lateral tolerance below is hardest to hold. Two rows span ~18 mm, inside the 20.0 mm **inradius**. This is `NP-DRV-SHELL-002` **REQ-SKT-01** |
| Springs on | socket (inner bowl) | keeps the moving, wearing, fatiguing element in the part that is never removed; the swappable tile is passive gold pad |
| Plating | hard gold ≥0.8 µm over nickel, both sides | fretting/oxidation resistance, per the §5.3b spring-finger precedent |
| Current per contact | ≥1.0 A continuous | pogo pins in this size class support 1–3 A; §8.1 needs **0.35 A/pin nominal, 0.52 A on loss of one contact** |
| Contact resistance | ≤50 mΩ | matched to the NP-HEX-ZM-001 §5.3b ground-bond target; binding for the electrode line (§7.2 note) **and now for `PD1_K`/`PD2_K`**, which carry 14–72 µA photocurrent (§7.2 note) |
| Mating cycles | ≥500 | service-event frequency, not daily; far below the Boa dial's 50,000 |
| Blind-mate tolerance | ±0.4 mm lateral, ±0.5 mm Z | spring travel absorbs cluster-clamp plate variation across a curved cluster. **The two-row layout is what keeps this holdable at 19 positions** |
| Contact force | 0.3–0.5 N per contact → **5.7–9.5 N per module** | **34.2–57.0 N per 6-tile clamp plate.** Note this is *below* the 18-contact/7-tile figure `NP-DRV-SHELL-002` Rev 1 carried, because no cluster reaches 7 tiles under the 18-cluster partition (§8.2.1) |

Pogo contacts also suit the **cluster clamp mechanics** directly: NP-HEX-ZM-001 §5.4a describes a clamp plate with a spring-loaded plunger per module, precisely because a rigid plate over a curved cluster cannot seat evenly. Spring contacts tolerate the residual Z variation that survives the plungers; a ZIF or board-edge connector would not.

Orientation is fixed by the tile's existing asymmetric mechanical key (R-2). The pad pattern is additionally asymmetric about the tile's long axis so a mis-keyed insertion cannot make contact — a fail-open, not a fail-wrong, geometry. **The two-row layout makes this easier, not harder:** a row-length difference (9 vs 10) is itself an asymmetry, where a single symmetric row needed a deliberate keying feature to carry it.

### 7.2 Pinout

**Nineteen positions.** The count is derived, not chosen: it is the union of every tile type's needs (§1), at the conductor width the power budget requires (§8.1), across the networks that actually cross the socket after **OI-HUB-C17c** (§7.1 banner). It is identical to `NP-DRV-SHELL-002` Rev 2 §5.1.4 — the two documents specify one physical interface and now agree pin for pin.

| Pin | Signal | Net | Direction | Rev 2 | Notes |
|---|---|---|---|---|---|
| 1 | VLED | N1 | socket → module | pins 1–4 | 24 V LED supply, §8.1 |
| 2 | VLED | N1 | socket → module | | paralleled ×3 for current sharing and single-contact redundancy |
| 3 | VLED | N1 | socket → module | | |
| 4 | PGND | N1 | — | pins 5–8 | **LED return only** — see the `DGND` note below; paralleled ×3 to match VLED |
| 5 | PGND | N1 | — | | |
| 6 | PGND | N1 | — | | |
| 7 | VCC_3V3 | N1 | socket → module | pin 9 | logic supply, ≤2 mA standby / ≤25 mA active (§8.3) |
| 8 | **DGND** | N1 | — | **NEW** | logic return, **separate from PGND** — see note |
| 9 | SDA | N2 | bidirectional | pin 10 | I2C data, 400 kHz fast mode |
| 10 | SCL | N2 | socket → module | pin 11 | I2C clock |
| 11 | **SYNC** | N2 | socket → module | **NEW** | broadcast sample/pulse-phase reference — see note |
| 12 | ALERT# | N2 | module → socket | pin 12 | open-drain, wired-OR per bus segment; thermal/fault/OCP assertion so the hub need not poll ~80 modules. **Renamed from Rev 2's `/ALERT`** — §7.4 |
| 13 | **PD1_K** | N3 | module → socket | **RESTORED** | PD1 forward-emission photocurrent, 14–72 µA |
| 14 | **PD2_K** | N3 | module → socket | **RESTORED** | PD2 scalp-facing backscatter photocurrent |
| 15 | **NTC** | N3 | module → socket | **RESTORED** | per-tile thermistor, 42 °C / 62 °C interlock chain (R-9) |
| 16 | AGND | N3 | — | pin 15 | analog/sense return, star-referenced **at the cluster controller**, **not** tied to PGND on the module |
| 17 | **ELEC** | N4 | bidirectional | pin 13 | dual-rated Ag/AgCl electrode — EEG µV signal **and** BES/tACS/tDCS current. Unused on T1-A/T1-C; **present at every socket** because R-2 permits a T1-B in any socket |
| 18 | **ELEC_SHLD** | N4 | socket → module | pin 14 | driven shield / DRL for pin 17, referenced to the EEG DRL output (CLAUDE.md §4.3). **`ELEC_SHLD` adopted over `NP-DRV-SHELL-002`'s `GUARD`** — §7.4 |
| 19 | SEAT# | — | module → socket | pin 16 | tied to PGND on the module through 1 kΩ; detects *partial* seating (§7.3) |

**Notes on the contentious pins:**

- **Pins 13–15 (`PD1_K`, `PD2_K`, `NTC`) are back, and this is the consequence of OI-HUB-C17c, not a reversal of anything this document argued.** **D-4** removed them by moving the TIA and ADC on-module, and §5.3's reasoning for that is unchanged physics that this revision does not withdraw. What changed is the *decision*: `NP-DRV-SHELL-002` Rev 2 §3.3a keeps the analog front end on the cluster controller, on the conservative ground that **a carrier-mounted ADC never faces the 25 → 62 °C drift question that a tile-mounted one must answer against the ±15 % dose claim (FAI-SM-06)**. The cost is exactly what §5.3 warned of and it is now carried openly: **three spring contacts sit in the photocurrent path**, so contact-resistance drift and fretting become dose-metering error terms rather than being designed out. The ≤50 mΩ / ≥500-cycle spec in §7.1 is therefore binding on pins 13–15 as well as on pin 17. **§5.3 and §6 are NOT rewritten by this revision** — see **OI-HEXTILE-15**.
- **Pin 8 (`DGND`) is not redundant with `PGND`.** The obvious argument for merging them — ground bounce is small, ~52 mV at ≤50 mΩ and 1.04 A against a 0.99 V I2C threshold — is true and is not the point. `NP-DRV-SHELL-002` **REQ-EMI-07** forbids the LED return from using *"any structure other than its paired `PGND` conductor"*, because that is what makes the §9.3 broadside supply/return pair cancel. If module logic current also flows in `PGND`, the current in the return is no longer the current in the supply and the cancellation is exact only for the LED term. **One pin keeps a requirement enforceable.**
- **Pin 11 (`SYNC`) closes a gap this document had left unowned.** Rev 2 had no phase-reference contact, but `NP-DRV-SHELL-002` **REQ-EMI-03** requires bus traffic and PBM pulse phase to be deterministic and phase-locked to the EEG/fluxgate sample frame, and **REQ-EMI-04** prohibits the usual EMC escape (dithered PWM) precisely so the artifact stays a subtractable known line. An I2C broadcast carries arbitration and segment-switch jitter and cannot serve that. This is also the pin **OI-HUB-C05** was asking for from the other end — *"T1-C PWM phase sync routing from the on-module ATtiny402 … is not yet defined"*. It now has a home.
- **Pins 17–18 and 16 are the price of R-2.** T1-A and T1-C do not use the electrode pair. They exist at every socket because "any type in any socket" means a T1-B may be inserted anywhere, and an electrode signal cannot be carried over I2C — it is a µV analog recording path to the ADS1299 *and* a stimulation current path from the tES driver. Removing them would silently re-impose the type-restricted placement model that SMART-1 was decided to eliminate.
- **Pin 12 (`ALERT#`) earns its position by arithmetic.** Polling 80 modules over segmented I2C for thermal status at the 10 ms cadence R-9 implies is not achievable; a wired-OR interrupt turns an 80-module poll into an exception path.
- **Pin 19 (SEAT#) is not redundant with the I2C presence poll — and the case for it is now stronger.** NP-HEX-ZM-001 §5.4a correctly notes an unseated tile fails its inventory poll. But a *partially* seated tile can answer I2C on two contacts while the PD, NTC, or electrode contacts are marginal. **With N3 restored that failure mode is live rather than hypothetical:** a tile answering I2C while `PD1_K` sits at elevated contact resistance returns an *under-read* photocurrent, which firmware interprets as low optical output and compensates for by driving harder — a silent dose-integrity failure against a stated competitive claim. SEAT# is positioned at the mechanical extreme of the pad pattern so it is the **last** contact to mate; if it reads low, every other contact is seated.
- **No ZONE_ID pin.** The retired resistor ladder (NP-HW-FPC-001 Rev 5 §3.2) is gone, replaced by UID self-report over I2C (R-12). Its ADC channel, its 1 %-tolerance resistor requirement, its threshold-margin analysis, and its debounce requirement (RISK-18, NP-SW-001 §5.2.1) do not apply to this interface. **NP-SW-001 §5.2.1 and the RISK-18 debounce requirement will need re-scoping** where they bind on module detection — flagged as **OI-HEXTILE-08**; they remain in force for any surviving non-tile accessory detection.
- **No UID EEPROM at any socket.** **D-3** fits a driver MCU to every tile type, so every tile self-reports its UID over I2C. `NP-DRV-SHELL-002` Rev 1's separate 24AA02UID line for T1-A/T1-B is deleted (~$8.50/headset).

### 7.3 Contact sequencing

Pad lengths are staggered so mating order is deterministic. **Rev C adds the four new pins to the sequence rather than leaving them at group 3 by default** — `DGND` moves up because it is a return, and the N3 sense lines stay last-but-one because nothing is harmed by their being late:

1. **PGND, `DGND`** (longest) — **every return established before any supply.** Rev 2 had only `PGND` here because logic shared it; with a separate logic return (pin 8) both must lead
2. **VLED, AGND, ELEC_SHLD**
3. **VCC_3V3, SDA, SCL, `SYNC`, `ALERT#`, ELEC, `PD1_K`, `PD2_K`, `NTC`**
4. **SEAT#** (shortest) — asserts only when the stack is fully home

Break order is the reverse. This prevents the module logic powering up against a floating return, and guarantees SEAT# cannot read seated during a partial insertion.

**Two-row consequence.** With the array in two staggered rows (§7.1), the length stagger must be applied **within each row and consistently across both**, and `SEAT#` must sit at the extreme of whichever row seats last under the worst-case tilt the ±0.5 mm Z tolerance permits. A stagger that is correct row-by-row but inconsistent between rows can let one row make full contact while the other is still partial — which is precisely the state SEAT# exists to detect. Verified by `NP-DRV-SHELL-002` **SH2-DRC-05a** and **SH2-DRC-10b**.

**Rev 18: the stagger has an order and no dimension (`OI-HEXTILE-28`).** Nothing above gives the
length difference between groups, so the sequencing promised here cannot yet be checked against the
geometry that must deliver it. The dimension is a **mating-order margin**. It is not a timing window.
`NP-FW-NVRAM-001` §4.3.1 shows that no firmware in the set needs the break-first interval's duration,
and no extraction velocity is assumed here.

---

### 7.4 Signal naming — one name per conductor (decided 2026-08-11)

**Two conductors carried two names each between `NP-DRV-SHELL-002` and `NP-HW-HEXTILE-001`. Both
are settled here; neither choice has technical content, and both were chosen on a stated hazard
rather than on taste.**

| Conductor | Names in use | **Adopted** | Why |
|---|---|---|---|
| Socket pin 14 / 18 — DRL-driven shield for `ELEC` | `GUARD` (SHELL-002) · `ELEC_SHLD` (HEXTILE, HUB-001 §113) | **`ELEC_SHLD`** | **`GUARD` collides with a different conductor these same documents already name.** §3.5, §4.1 and REQ-EMI-02 all specify a *DRL-driven **guard plane*** on L1's scalp-facing face, under which the N4 shared electrode lanes run. The socket pin is driven **from** that plane but is not it — they are different nets with different extents. Naming the pin `GUARD` makes "the guard" ambiguous in the one document that specifies both. `ELEC_SHLD` names what it shields and pairs typographically with `ELEC` |
| Socket pin 12 / 8 — open-drain module fault | `/ALERT` (HEXTILE) · `ALERT#` (SHELL-002, HUB-001 §7.4/§7.5.1) | **`ALERT#`** | **A leading `/` is the hierarchical-path separator in EDA net naming** (KiCad, Altium): a net literally named `/ALERT` reads as a root-scope hierarchical path and can be silently re-scoped or duplicated on netlist import. That is a mechanism, not a preference. `ALERT#` was also already the majority usage across the document set |

**Why this was worth a decision at all.** A conductor with two names is harmless in prose and is not
harmless in a netlist, a test fixture, or a firmware pin table, where the failure mode is a silent
duplicate net or an unconnected pin — a defect that *reads* as agreement. Both collisions were found
by diffing the two pin tables mechanically rather than by reading them, which is why
**SH2-DRC-05b** / **HT-DRC-23** specify that check as a diff rather than a review.

**A third and fourth name were settled on the same criterion (OI-HEXTILE-17, closed 2026-08-11).**
Both were initially recorded as cosmetic residuals. Checking the codebase showed the first is not
cosmetic at all:

| Conductor | Names in use | **Adopted** | Why |
|---|---|---|---|
| Socket pin 19 — partial-seating detect | `SEAT_N` | **`SEAT#`** | **`_N` already means *cardinality* in this codebase, not active-low.** `firmware/hub_control/tests/np_module_map_tests.c` defines `PBM_TILE_N` and `EEG_TILE_N` as `sizeof(x)/sizeof(x[0])` — so `SEAT_N` reads as *"number of seats"* to anyone who has read the module map. That is a live ambiguity in the same class as the `GUARD` / guard-plane collision above, not a style difference |
| Socket pin 13 / 17 — dual-rated electrode | `ELEC` (HEXTILE §7.2, SHELL-002 §5.1.4) · `ELEC_SIG` (HUB-001 §113/§124) | **`ELEC`** | Two reasons. **Ownership:** the socket interface is defined by the two pin tables; `NP-HW-HUB-001` §7.4 explicitly *adopts* SHELL-002's contract, so it is the consumer — two normative pin tables do not change to match one banner's prose. **Accuracy:** this pin is dual-rated to carry **tES stimulation current** (≤2 mA T1 / ≤4 mA T2), not only a recording signal. `_SIG` biases the reader toward the recording role, which is the half that is *not* safety-relevant. `ELEC` / `ELEC_SHLD` is asymmetric and costs nothing |

> **Rule of record — now programme-wide, in `NP-CONV-001` §1.1: every active-low NeurOne signal name
> terminates with `#`.** Not `_N` (cardinality), not `_L`/`_R` (Left/Right — `NP-FW-CVNS-001` §5.1),
> not `_B` (channel B), not `_LOW` (threshold), and not a leading `/` (EDA path separator).
> `NP-CONV-001` §1.2 records each with the evidence that took it.
>
> Active-**high** signals take no suffix, so the absence of `#` is meaningful. Applying this beyond
> the socket found one further active-low signal: `NP-HW-HUB-001`'s tier-0 service-request line,
> now **`ATTN#`**. Indices use bracket notation — **`SAFE_EN[n]`**, not `SAFE_EN_n`, because a
> lowercase `_n` is indistinguishable from a polarity marker in a plain-text diff
> (`NP-CONV-001` §1.3).
>
> **⚠ The rule exposed a live safety-architecture conflict, which is raised and not resolved:**
> `SAFE_EN[n]` is written here as active-**high** (§6: LOW removes the rail), while the safety MCU
> specifies the opposite for its enable lines (*"LOW = stimulation enabled"*,
> `np_safety_config.h:7-8`). Both are internally fail-safe; together they are inverted, and
> SH2-DRC-13's "defaults LOW at reset" would flip from *safe* to *stimulation enabled* under the
> firmware's convention. **`NP-CONV-001` OI-CONV-01.**

**`#` is not a legal identifier character, so the doc→firmware mapping is stated rather than left to
whoever writes the driver. The full rule now lives in `NP-CONV-001` §2:**

> **`<SIGNAL>#` maps to `<SIGNAL>_ACTIVE_LOW` in firmware identifiers.**

> **⚠ Correction.** An earlier draft of this section specified **`_L`**. That was wrong: **`_L`
> already means *Left*** — `NP-FW-CVNS-001` §5.1 defines `CVNS_ENABLE_L` / `CVNS_ENABLE_R` as the
> left and right electrode drivers. Every other short candidate is taken as well: `_N` is
> cardinality, `_B` is channel B (`CH_B`, `LED_B`, `NP_BANK_B`), `_LOW` is a threshold
> (`NP_TIA_GAIN_LOW`). `NP-CONV-001` §1.2 records all of them with evidence.

**No firmware change is requested here, and none should be made to satisfy this.** The safety MCU's
ten stimulation enable lines are all active-LOW and carry polarity only in header comments
(`np_safety_config.h:7`, `np_gpio_mgr.c:5`); that is IEC 62304 **Class C** code already owned by
`NP-FMEA-001` **OI-FMEA-01**. §2 of `NP-CONV-001` exists so *new* names converge and so OI-FMEA-01
has a convention to adopt. See `NP-CONV-001` §3.

---

## 8. Per-socket power and I2C under SMART-1

### 8.1 VLED rail

**Decision (D-6): VLED = 24 V DC.**

Derived from conductor current, not from convenience. With the driver on-module (D-3), the socket carries DC bus power rather than per-string drive, so the only free variable is rail voltage, and it trades directly against contact current.

| Rail | T1-A peak current per tile (both channels, 150 mA) | Per VLED pin (**×3**, Rev 3) | *Per VLED pin (×4, Rev 2)* |
|---|---|---|---|
| 5 V | 5.0 A | 1.67 A — exceeds the contact rating outright | *1.25 A — exceeds comfortable pogo derating* |
| 12 V | 2.08 A | 0.69 A | *0.52 A* |
| **24 V ★** | **1.04 A** | **0.35 A** | *0.26 A* |

T1-A peak electrical: CH_A 45 × 0.315 W = 14.2 W, CH_B 45 × 0.24 W = 10.8 W → **25.0 W instantaneous**. At 24 V that is 1.04 A, or **0.35 A per contact across three paralleled VLED pins — ~3× derating against the ≥1.0 A contact rating (§7.1)**.

> **The nominal figure is not what set the count.** Three contacts were chosen on the **degraded**
> case, under the rule stated in `NP-DRV-SHELL-002` §5.1.5: *`VLED` is sized so the loss of any one
> contact still leaves ≥2× derating.* At 3 contacts a single-contact loss puts 0.52 A on each
> survivor (~2×); at 2 it puts **1.04 A — exactly the rating, zero margin**, into the fretting →
> resistance → local I²R heating → more fretting runaway that HT-DRC-08 and `NP-DRV-SHELL-002`
> SH2-DRC-09 exist to bound. Rev 2's 4 contacts gave ~3× degraded, which exceeds the rule at the
> cost of two contacts on an interface carrying a live RISK-22 one-handed-force constraint.
> **If the rail, the tile peak power or the contact rating changes, re-derive the count from the
> rule — 3 is a result, not a constant.**

The 5 V row is now excluded twice over: it exceeded comfortable derating at 4 contacts and exceeds the **rating itself** at 3.

24 V also suits series-string construction: at 11 series 660 nm emitters (11 × 2.10 = 23.1 V) or 14 series 808 nm (14 × 1.60 = 22.4 V), the residual dropped across the FET and sense resistor is ≤1.6 V, so linear-loss overhead stays under 7 %. A 12 V rail would halve the string length and double the number of parallel strings and sense resistors on a tile that has no room for them.

> **Rev 20:** both worked strings rest on design targets, and neither survives the datasheets. At
> 150 mA the 660 nm primary gives **13 × 1.78 V = 23.15 V**, and the NIR candidates give
> 16 × 1.41 V (SFH 4718A), 8 × 2.82 V (Luminus) or 8–9 × ~2.6–2.9 V (L1IZ-0850). See the §8.1.1
> table. The rail choice itself is unaffected.

#### 8.1.1 The forward-voltage budget stated as a rule (Rev 4)

The two worked strings above are instances, not the constraint. Stated generally, so that a candidate
emitter can be tested against it without re-deriving it — and because the instances were being quoted
elsewhere *as* a Vf constraint on the architecture, which they are not (see
`docs/status/pending-decisions.md` §13.2e):

> **N · V_f ≥ 22.4 V** — the residual is a *thermal budget allocation*. Violating it produces heat in
> the linear stage, not a failure; the dissipation lands on the tile and is therefore a term in §9.3.
>
> **N · V_f ≤ 24 V − V_dropout(min)** — a *hard functional ceiling*. Below the stage's dropout the
> constant-current control falls out of regulation and the string is uncontrolled.

**The two bounds are different kinds of constraint and must not be quoted as one range.** Only the
upper is functional.

| V_f | Best N | N · V_f | Residual | Overhead | Meets the rule |
|---|---|---|---|---|---|
| 1.60 V (CH_B design target) | 14 | 22.40 V | 1.60 V | 6.7 % | ✓ at the ceiling |
| 2.10 V (CH_A design target) | 11 | 23.10 V | 0.90 V | 3.8 % | ✓ |
| 2.80 V | 8 | 22.40 V | 1.60 V | 6.7 % | ✓ |
| 3.00 < V_f < 3.20 V | — | — | — | — | ✗ **no integer N** (dead band; widens with `V_dropout`) |
| 3.30 V | 7 | 23.10 V | 0.90 V | 3.8 % | ✓ |
| **2.82 V** — Luminus SST-10-IRD-810 at 150 mA, **datasheet** | **8** | **22.58 V** | **1.42 V** | **5.9 %** | **✓** |
| **2.85 V** — Luminus SST-06-IRD-810 at 150 mA, **datasheet** | **8** | **22.82 V** | **1.18 V** | **4.9 %** | **✓** |
| **1.78 V** — ams-OSRAM GH CSSRM5.24 (660 nm) at 150 mA, **datasheet** (Rev 20) | **13** | **23.15 V** | **0.85 V** | **3.5 %** | **✓** (N = 12 is 21.37 V, under the floor) |
| **1.41 V** — ams-OSRAM SFH 4718A at 150 mA, **datasheet** (Rev 20) | **16** | **22.56 V** | **1.44 V** | **6.0 %** | **✓** (N = 17 is 23.97 V and needs `V_dropout` ≤ 0.03 V) |
| **2.62–2.87 V** — Lumileds L1IZ-0850 at 150 mA, **datasheet, bracket** (Rev 20) | **9 or 8** | 23.56 V / 22.98 V | 0.44 V / 1.02 V | 1.8 % / 4.2 % | **✓ at either end, but N differs**. Undetermined |

**The last two rows are the only ones in this table that are not design targets or bracket
estimates** (Rev 10). They are read off each datasheet's `ΔV_f` vs `I_F` trace against its 350 mA
binning reference, and the read is self-validated — the trace returns `ΔV_f` = −0.005 V at 350 mA,
where it is zero by construction. `NP-PROC-FPC-001` §2.6.3 carries the full assessment, including
three requirement failures that have nothing to do with this section.

> **Rev 20 — the three parts `OI-LED-01` names, read off their own `IF = f(VF)` traces.** Each
> vector trace was extracted from the supplied PDF and interpolated. Each is self-checked where its
> datasheet tabulates `V_f`:
>
> - **GH CSSRM5.24** (v1.1, 2023-01-18): **1.77 / 1.78 / 1.80 V** at 120 / 150 / 180 mA. The trace
>   gives 1.990 V at 700 mA against 1.99 V typical ✓.
> - **SFH 4718A** (v1.5, 2026-01-09; single pulse, 100 µs): **1.39 / 1.41 / 1.43 V**. The trace gives
>   1.744 V at 1 A against 1.75 V typical ✓.
> - **L1IZ-0850** (DS190 dated 2018-01-09): the trace reads 2.83 / 2.87 / 2.91 V, but it gives
>   **3.455 V at 1 A against Table 2's 3.2 V typical ✗**, so the plotted device is 0.26 V above
>   typical. The trace's own shape (−0.582 V from 1 A to 150 mA), anchored to the typical, gives
>   **2.62 V**. The bracket is **2.62–2.87 V**, and N = 9 at the low end or 8 at the high end.
>   `NP-PROC-FPC-001` Rev C cites a DS190 dated **2018-06-19**. That revision may reconcile the two,
>   and it has not been read.
>
> **Binning against caveat 1 below.** GH CSSRM5.24 ships in **0.10 V** `V_f` groups (E1–F2, at
> 700 mA), so it meets §2.1 as a standard condition. Its group edges do not suit a single N. At
> 150 mA the four groups need **N = 14, 13, 12 and 12**. Only F2 meets both bounds across its width
> at N = 12, and it does so only with `V_dropout` ≤ 0.11 V. So **a fixed N needs a single ordered
> group**. SFH 4718A has **no `V_f` groups at all**, and its typical-to-maximum spread is 0.30 V at
> 1 A, or 4.8 V across N = 16. L1IZ-0850 bins in **0.5 V** steps, which is 4.0 V across N = 8.
> **Neither NIR part meets §2.1's ±0.10 V bin without a special binning agreement** (`OI-LED-06`).
>
> **Allocation slack.** The consequence paragraph below allows ±2 sites. Against 45 sites (T1-A)
> and 30 (T1-C), at the datasheet N:
>
> | Part | N | T1-A: 45 sites | T1-C: 30 sites |
> |---|---|---|---|
> | GH CSSRM5.24 | 13 | 39 (6 empty) ✗ | 26 (4 empty) ✗ |
> | SFH 4718A | 16 | 32 (13 empty) ✗ | 16 (14 empty) ✗ |
> | L1IZ-0850 | 9 / 8 | 45 exact ✓ / 40 ✗ | 27 ✗ / 24 ✗ |
> | Luminus SST-06/10 | 8 | 40 (5 empty) ✗ | 24 (6 empty) ✗ |
> | MP-2835-1100-DR (Rev 22 candidate) | 11 typ (12 / 11 / 10 for `V_f` bins C3 / D3 / E3) | 44 (1 empty) ✓ at N = 11 | 22 (8 empty) ✗ |
>
> **The ±2 figure does not survive the datasheets.** It was sized for N = 11 and N = 14. Whatever
> selection closes it will also change §4.3's irradiance, which is already over R-4 for every
> candidate (§4.3 Rev 20 note, `OI-HEXTILE-29`).

**Consequence for emitter selection (OI-HEXTILE-02).** A ~3 V AlGaAs NIR emitter is **not**
categorically incompatible with this rail — at 7–8 series it lands on residuals this section already
accepts. What it costs is *per-tile*: 7–8 emitters per string against CH_B's specified 14 roughly
doubles the parallel strings and sense resistors, which is the same objection raised against a 12 V
rail one paragraph above, on the same tile that has no room for them. That is a rigidizer-layout and
current-matching question, **not a rail or Hub PCB question.** There is also a dead band at
**3.00 < V_f < 3.20 V** where no integer N satisfies both bounds — N = 7 falls under the 22.4 V floor,
N = 8 exceeds the rail. It is ≥0.20 V wide and **widens as `V_dropout` grows** (OI-HEXTILE-18), since
the N = 8 ceiling is `(24 − V_dropout)/8`, not 3.00 V.

**Two caveats that bound every number in this section:**

1. **Fixed-N construction depends on Vf binning, and that dependency has not been stated before.**
   A commodity emitter's datasheet typ→max Vf spread is routinely ≥0.7 V per die; across 7–14 in
   series that is several times the entire 1.6 V residual budget, and no fixed N survives it. What
   makes fixed-N viable is `NP-PROC-FPC-001` §2.1's mandatory **±0.10 V within-order Vf bin**. **It is
   load-bearing for string construction, not only for RISK-08 current matching** — and as of that
   document's **Rev 4 (2026-09-21)** §2.1 says so itself, where before it justified the bin on
   current-hogging grounds alone.

   > **⚠ Rev 10 — the bin makes fixed-N *possible*, not *comfortable*, and the margin is zero.**
   > A ±0.10 V bin is **0.2 V wide**, and 0.2 V across **N = 8** is **1.6 V of spread in `N · V_f`**.
   > §8.1's entire residual window is `(24 − V_dropout) − 22.4` = **`1.6 − V_dropout`**. So:
   >
   > > **A fixed N = 8 holding across a full ±0.10 V bin requires `V_dropout` = 0.**
   >
   > Worked against the three 0.2 V bins the Luminus part actually ships in (shifted −0.177 V from
   > their 350 mA binning point to 150 mA):
   >
   > | Shipped bin at 350 mA | At 150 mA | `N · V_f` at N = 8 | ≤ 24 V ceiling | ≥ 22.4 V floor |
   > |---|---|---|---|---|
   > | 2.8–3.0 V | 2.62–2.82 V | 20.98–22.58 V | ✓ | ✗ under at the low edge (12.6 % overhead) |
   > | 3.0–3.2 V | 2.82–3.02 V | 22.58–24.18 V | ✗ over the rail at the top edge | ✓ |
   > | 3.2–3.4 V | 3.02–3.22 V | 24.18–25.78 V | ✗ | ✓ |
   >
   > **Only the lowest bin clears the hard functional ceiling across its whole width, and it does so
   > by spending more than the 7 % thermal allocation at its low edge.** This is not a Luminus
   > property — *any* supplier's ±0.10 V bin does this at N = 8. It is **`OI-HEXTILE-18` (no minimum
   > dropout is specified) and `OI-HEXTILE-19` (no rail tolerance is stated) arriving as a number**,
   > and it says the three are one question, not three: the bin width, the dropout and the rail
   > tolerance all spend the same 1.6 V. Resolving it means tightening the bin below ±0.10 V,
   > re-deriving the 7 % allocation from §9.3, or adding a per-string trim — an EE Lead decision that
   > `OI-HEXTILE-02` should not be closed ahead of. The distinction Rev 4 adds is the one that matters here: on the
   current-hogging axis the bin degrades gracefully, on this axis it does not. Relaxing it does not
   widen a current imbalance; it removes the premise that a fixed N exists. Any §2.5 "use-as-is"
   disposition of an out-of-bin lot at ±0.15 V is therefore a string-length decision as well as a
   current-matching one.
2. **The 7 % figure is asserted here, not derived.** It is not traceable to a tile power or
   temperature limit anywhere in this document or in `NP-HW-HUB-001` / `NP-DRV-SHELL-002`. Treat it as
   a chosen allocation open to re-derivation from §9.3, not as a bound with a physical basis —
   **OI-HEXTILE-18**.

**Consequence:** emitter count per channel must be an integer multiple of string length, which the 45/45 and 30/30/30 allocations of §4.2 do not exactly satisfy for every candidate V_f. The allocation carries **±2 sites of slack**, to be closed when emitters are selected (OI-HEXTILE-02). Worked example at the assumed V_f: CH_A 44 = 4 strings × 11; CH_B 42 = 3 × 14. The lattice geometry is unaffected — unallocated sites are simply left unpopulated.

### 8.2 I2C fan-out — the architecture SMART-1 requires

NP-HEX-ZM-001 §4a states the retired one-PCA9546A-plus-one-GPIO-mux design *"does not scale to ~30–80 sockets without a materially different I2C fan-out architecture (cascaded/multi-stage muxing)."* This is that architecture.

The retired design needed a mux per slot for one reason: every smart module shared the factory-burned address 0x30, so two modules on one bus collide. **The collision is what should be removed, not worked around** — muxing 80 sockets to preserve a fixed address is solving the wrong problem.

**Decision (D-7): dynamic address assignment from the module UID, over per-cluster bus segments.**

- **Segmentation follows the mechanical clusters.** NP-HEX-ZM-001 §5.4a partitions the lattice into clusters under **CLUSTER-1** (7-hex "flower", partial flower where the lattice edge cannot host a full one) and **SYM-1** (the partition is mirror-symmetric about the sagittal midline). Reusing that partition electrically means one bus segment = one cluster = one VLED switching domain (§8.4) — one physical grouping serving mechanics, power, safety, and addressing rather than four incompatible partitions.
- **The cluster count is 18 at the v1 80-socket lattice — derived, not carried over.** See §8.2.1. Earlier revisions of this section stated "4–10 clusters", which was NP-HEX-ZM-001 §5.4a's figure for the **retired 30-socket** lattice and does not survive rescaling; 4 clusters at 80 sockets would mean 20 tiles per cluster, which exceeds this section's own 10–19-modules-per-segment capacitance limit and is impossible under CLUSTER-1's 7-tile ceiling.
- **The i.MX RT1062 provides LPI2C1–LPI2C4.** One PCA9548A-class 8-channel switch per bus gives up to 32 segments; **18 clusters uses 56 % of that**, leaving 14 segments of headroom for a REG-1 lattice re-cut. Note this is *one tier* of muxing, not the cascade the parent document anticipated — because addresses no longer collide, muxing is only needed for bus capacitance, not arbitration. **The muxing architecture is unaffected by the count correction** (32 ≥ 18 with margin), but the two-level `8 branches × ≤2 clusters = 16` topology of NP-DRV-SHELL-002 §3.4 **is** — 18 > 16 (OI-HEXTILE-14; options at §8.2.2).
- **Address assignment uses the UID that already exists.** NP-HEX-ZM-001 §4 specifies power-on UID polling with re-inventory only on UID change. An SMBus-ARP-style assignment (address resolution from a unique device identifier) maps onto that directly: the hub enumerates a segment, assigns each module an address from its UID, and the resulting map feeds `np_module_map` unchanged. **No new identity concept is introduced** — the UID the addressing layer already depends on becomes the addressing key.
- **Capacitance:** at 400 kHz fast mode the 400 pF bus limit permits roughly 10–19 modules per segment with disciplined routing. Realised cluster sizes under CLUSTER-1 + SYM-1 are **3–6 tiles** (§8.2.1), comfortably inside that, with ≥1.7× margin even against a full 7-tile flower. Segment routing length is the real constraint and is a Hub PCB Rev C layout item. Note the earlier "3–7-tile cluster sizes" phrasing bracketed the **3-hex triad**, which CLUSTER-1 excludes; the surviving range is the partial flower (3–6) up to the full flower (7).
- **Pull-ups:** one 4.7 kΩ pair per segment (not per socket). At **18 segments, 36 resistors** — versus the 160 that a per-socket scheme would need.

### 8.2.1 Where 18 comes from

The count is fixed by the lattice, not chosen. Inputs: the v1 socket lattice `ROW_WIDTHS = [3,6,7,8,9,8,9,8,7,6,5,4]` (80 sockets, 12 coronal rows, `scripts/sync-socket-map.ts`), **CLUSTER-1** (flower or partial flower only), and **SYM-1** (mirror-symmetric partition).

1. **The six midline clusters are forced.** A cluster containing a midline socket must equal its own mirror image, so its centre must be self-mirror — i.e. *on* the midline. Only odd-width rows carry a midline socket (r0, r2, r4, r6, r8, r10 → sockets **{2, 13, 29, 46, 62, 74}**, NP-HEX-ZM-001 §3.2), and a flower spans only rows *r*−1…*r*+1, so each midline cluster holds **exactly one** midline socket. Hence exactly **6** midline-centred, self-symmetric clusters.
2. **They absorb exactly 30 sockets** — 6 centres + 12 in-row petals (x = ±1) + 10 contested petals at x = ±0.5 on r1…r9 + 2 on r11.
3. **The residual is 50 sockets: two mirror-image lateral bands of 25.** Exhaustive branch-and-bound over all flower/partial-flower covers of one band gives a minimum of **6** clusters per band (the naïve ceil(25/7)=4 is unreachable — the residual bands are only 2–3 sockets wide, so most flowers cannot fill).

**Total: 6 + 2 × 6 = 18 clusters, and this is provably minimal**, not a greedy result. Cluster sizes are 3–6 tiles. Diagram: `docs/diagrams/np_hextile_cluster_map.svg`.

**Contiguity is a binding shape rule, not an aesthetic one (CONTIG-1).** Minimising cluster *count* does not by itself constrain cluster *shape*: a partition can satisfy CLUSTER-1 and SYM-1 while still placing a petal whose only in-cluster contact is the centre — a **pendant petal**, with a foreign socket on both flanks. The first 18-cluster partition generated for this revision contained four (sockets 27, 31, 77, 80). That shape is mechanically inadmissible, because the cluster's structural member is the **clamp plate**, not the tile group (tiles are independent modules in independent sockets), and the plate cannot bridge the gap — the gap socket belongs to a different cluster whose plate actuates independently. The plate must therefore reach a pendant petal on a **cantilever arm**:

| Arm geometry | Value | Source |
|---|---|---|
| Length, centre plunger → pendant plunger | 40.0 mm | tile pitch, §4.1 |
| Maximum neck width crossing one tile boundary | **23.09 mm** (one hex edge, W/√3), less clearance for the two flanking plates | §3 |
| Dome the arm must follow over that span | 2.33 mm sagitta at R_m = 87 mm | R-1 |

**Rule: a cluster's petals must form a contiguous arc around its centre**, measured over ring positions that exist in the lattice. Dropping *outer* petals — CLUSTER-1's own wording — yields this automatically; only a count-minimising search violates it. A single missing petal in an otherwise complete ring (a horseshoe plate, e.g. C2/C4) is admissible: the petals still chain, so there is no cantilever. What is excluded is an **isolated** petal.

The four pendant petals were resolved at **zero cost in cluster count** by reassigning each to an adjacent cluster that already touches it — 73→C16, 75→C18, 27→C8, 31→C9 (principal direction, 2026-08-04). The partition remains 18 clusters, mirror-symmetric, max size 6, with **zero pendant petals and zero broken arcs**.

**The symmetry constraint costs clusters.** Without SYM-1 the minimum is **12** (ceil(80/7), the figure NP-HW-HUB-001 §4.4 and NP-DRV-SHELL-002 §7.1 both carried until 2026-08-16). SYM-1 raises it to 18 — a 50 % increase — because the midline forces a column of six clusters that are mostly partial flowers, and the residual lateral bands are too narrow to pack efficiently. *Every peer document once sized hardware off 12 or off `ceil(n/8)` = 10; all three counts were in play and only 18 satisfies the standing decisions.* **All peers now read 18 — OI-HEXTILE-14 closed 2026-08-16.**

### 8.2.2 Interconnect capacity at 18 clusters — options for OI-HEXTILE-14

**Status: ADOPTED 2026-08-16 — options 1 + 3, exactly as recommended below.** `NP-DRV-SHELL-002` **Rev 2** (2026-08-11) took them first (§7.1: 18 populated / 20 provisioned; §3.4: D-7's 32-segment tree), and `NP-HW-HUB-001` **Rev 5** (2026-08-16) brought the last peer into agreement. **OI-HEXTILE-14 is closed** and **HT-DRC-20 passes.** Option 4 (broadcast `SAFE_EN`, 11-conductor tail, multi-drop trunk) is **not** adopted — it remains conditional on **OI-HUB-C07** / `OI-HEXTILE-13`, and both peers are written to be correct either way. Options 2, 5, 6 and 7 were assessed and rejected; the reasoning below is retained as the record of why. Original text follows unchanged.

**Two independent "16"s bind, and they belong to different documents.** They must not be conflated:

| # | Constraint | Source | Binds at |
|---|---|---|---|
| **C1** | Cluster-tail **connector positions** on the Hub PCB, 12 pins each | NP-DRV-SHELL-002 §7.1 | 16 (12 populated) |
| **C2** | I2C **tree capacity**, `8 branches × ≤2 clusters` | NP-DRV-SHELL-002 §3.4 | 16 |

**C2 does not exist under this document's own D-7.** D-7 is 4 × LPI2C, each with one 8-channel PCA9548A = **32 segments**, of which 18 uses 56 %. The two documents describe different trees, and that disagreement is already open as **OI-HUB-C17** — where NP-HW-HUB-001 §7's own comparison recommends D-4/D-7 prevailing. So C2 may resolve itself; **C1 will not**, and is the one that must be fixed before Rev 3 layout.

| # | Option | Effect | Assessment |
|---|---|---|---|
| **1** | **Provision 18 → 20 connector positions** | +24…+96 conductors through the §5.3c posterior boss (216–240 vs 192) | **Recommended.** The cheap axis: SHELL-002 §7.3 notes adding a *conductor* costs 16 hub pins, so widening the tail is expensive while adding tails is not. Still far below the ~880 that Rev 2's star implied. 20 rather than 18 absorbs a REG-1 re-cut without a second re-spin |
| **2** | Raise branch fan-out `8 × 2` → `8 × 3` = 24 | Fixes C2 with **no new silicon** — the tier-1 bus already addresses 32 controllers on a 5-bit strap | Only needed if SHELL-002's cluster-MCU tree survives OI-HUB-C17. Capacitance is not the obstacle either way: with a cluster MCU the branch sees 3 controllers; under D-7 the segment sees ≤6 modules, both inside the 10–19 limit (§8.2) |
| **3** | **Adopt D-7's topology wholesale for Rev 3** | C2 disappears; 18 of 32 segments, 14 spare | **Recommended.** Costs nothing new — already the standing requirement in OI-HEXTILE-10. Needs OI-HUB-C17 decided |
| **4** | **Multi-drop trunk instead of per-cluster star** | Connector count becomes largely **insensitive** to cluster count | **Recommended if §8.4.1 is accepted** — see the interaction below |
| 5 | Decouple electrical from mechanical clusters (populate fewer sockets, OI-HEXTILE-06) | Electrical clusters < 18 while mechanical stays 18; the capacity-8 board SKU already tolerates it | Legitimate but trades a stated principle — D-7's "one physical grouping serving mechanics, power, safety and addressing" — for connector count. Not the lead option |
| 6 | Re-cut the lattice to ≤16 clusters | — | **Not available without breaking a principal decision.** The 6 midline clusters are forced by SYM-1 given six odd-width rows, and 18 is provably minimal at n = 80 (§8.2.1); reaching 16 needs ~10 fewer sockets, which REG-1 registration is unlikely to permit |
| 7 | Pair clusters onto shared tails | 18 clusters over ≤16 tails | Breaks "board + clamp + sockets as a single FRU" (NP-HW-HUB-001 §4.4) and forces an asymmetric pairing under a symmetric partition. Rejected |

**Interaction with §8.4.1 — the reason option 4 is strategic rather than cosmetic.** N1 (power) is already a broadside tree and N2 (control) is already a two-level tree; **`SAFE_EN[n]` (N5) is the only star component of the tail** (NP-DRV-SHELL-002 §4 network table), and it is therefore the structural reason each cluster must terminate at the hub individually. If §8.4.1's single Class C `NP_SAFETY_EN_PBM_CRANIAL` is accepted, that line becomes a **broadcast**: the tail drops 12 → 11 conductors and clusters can tap a trunk instead of each running a dedicated star leg. The per-cluster high-side gate already sits on the cluster carrier (SHELL-002 §5.1 BOM), so local gating is unaffected. **This is what makes the interconnect robust to a future REG-1 re-cut** rather than merely sufficient at 18.

> **Update 2026-08-16 — option 4's condition is now SATISFIED, and the count is still not decided here.** §8.4.1 was accepted by safety review (**OI-HUB-C07** closed), so the single Class C `NP_SAFETY_EN_PBM_CRANIAL` *is* the resolution and `SAFE_EN[n]` becomes a broadcast line. That unlocks option 4 — 12 → 11 conductors, multi-drop trunk, connector count insensitive to cluster count. **It does not adopt it.** The connector and conductor count is **OI-HEXTILE-14's** decision, and `NP-DRV-SHELL-002` §7.1a's standing instruction — build against **20 positions and 12-conductor tails**, the expensive-and-safe direction — remains in force until OI-HEXTILE-14 takes it. Do not pre-empt it by provisioning 11.

**Recommendation: options 1 + 3, with 4 if §8.4.1 survives safety review.** Adopt D-7's 32-segment tree, provision **20** connector positions, and — if the single cranial enable is accepted — remove `SAFE_EN[n]` from the tail and let a trunk absorb future count changes. **The failure mode to avoid is cutting Rev 3 against 16 and discovering the shortfall in layout**, which is precisely what OI-HEXTILE-14's "coordinate before either is released" exists to prevent.

### 8.3 3.3 V logic budget

The retired per-module figure was ≤50 mA. **At 80 modules that is 4.0 A / 13.2 W of pure logic overhead — roughly a third of the entire T1 peak envelope (R-10), spent before a single photon is emitted.** The retired number was sized for 5 modules and does not survive multiplication.

**Requirement (binding on OI-HEXTILE-07):**

| State | Per module | ×80 |
|---|---|---|
| Standby (MCU in power-down, TWI address-match wake armed) | ≤2 mA | 160 mA / 0.53 W |
| Active (PWM running, ADC sampling) | ≤25 mA | ~6 concurrent (§9) → 150 mA / 0.50 W |
| **Total** | | **~310 mA / ~1.0 W** |

This is achievable — tinyAVR 2-series parts wake from standby on TWI address match — but it is a **hard firmware requirement**, not an incidental property. A module that idles at the retired 50 mA makes the whole-vault lattice infeasible on the existing power envelope. Stated here because it is a hardware-driven constraint that only the module firmware can satisfy.

### 8.4 Safety gating

R-11 requires the safety MCU to physically own the enable path. The STM32G071 cannot present 80 GPIOs, so per-socket gating is not available at the safety layer.

**Decision (D-8): the safety MCU gates VLED per cluster** — **18 high-side switches** on the cluster segments defined in §8.2, replacing the retired `NP_SAFETY_EN_PBM_ZONE_0..4` five-zone scheme. Cutting VLED removes emitter drive regardless of on-module state, so a wedged module MCU cannot sustain output. Per-socket granularity is provided by the on-module driver (fine, fast, not safety-rated); the cluster gate is the coarse, hardware, safety-rated backstop.

This is proposed as the hardware half of the resolution to **OI-HUB-SOCKET-01**.

**The GPIO argument this decision rests on does not close at 18, and is now an open item.** D-8's premise is that "the STM32G071 cannot present 80 GPIOs", which is true and unaffected. But the inference that the cluster count is therefore comfortable was written against 4–10. At 18 it needs re-checking, and re-checking surfaces three problems:

| # | Finding | Evidence |
|---|---|---|
| 1 | **The safety MCU package is not specified anywhere in the document tree.** STM32G071 spans UFQFPN28 (~22 I/O) to LQFP64 (~52 I/O). The only package-qualified STM32G071 in the tree is NP-HW-HUB-001 §8.3's **UFQFPN32 cluster MCU** — a different part. | `docs/np_hw_hub_001.md:1173`; no package in `firmware/safety_mcu/` |
| 2 | **Demand at 18 enables is ~38–40 I/O.** SPI1 slave 4 (NSS is load-bearing — frames are delineated by NSS transfer length) + R-peak capture 1 + nine non-cranial modality enables + 6 NTC ADC channels + fault-indicator LED (FMEA-M08-04 requires it on a *different port* from the enables) + SWD 2 ≈ 22, **plus 18** = ~40. That excludes every ≤32-pin package and leaves LQFP48 with almost no margin. | `firmware/safety_mcu/include/np_safety_config.h`; `docs/np_fmea_001.md` FMEA-M08-04 |
| 3 | **✅ ANSWERED 2026-08-16 — per-cluster *policy* is NOT wanted (§8.4.1; `NP-HW-HUB-001` §7.2.1). Original finding text follows.** **The open question is per-cluster *policy*, not a conflict between the peer documents.** An earlier draft of this section called NP-HW-HUB-001 §7.2 and NP-DRV-SHELL-002 §6 *incompatible*; **that was wrong and is withdrawn.** NP-HW-HUB-001 **§7.4 already reconciles them**: *"These are compatible and were reached from different ends: 12–16 physical enable **lines** fanned out from **one** policy **bit**."* Physical per-cluster gates and a single policy bit are the same design. What is genuinely undecided is whether **per-cluster policy** — independently commanded cluster bits — is wanted, which §7.4 routes to **OI-HUB-C07**. | `docs/np_hw_hub_001.md` §7.2, **§7.4:928**; `docs/np_drv_shell_002.md:190,378,432,482` |
| 4 | **Per-cluster policy does not fit the current Class C enable word, but the word can now be widened cheaply.** The enable mask is `uint16_t` (`NP_SAFETY_EN_ALL_MASK = 0x3FFF`, 14 bits used, 2 spare). 18 cluster bits + 9 surviving modality bits = **27 > 16**; §7.4 found the same at 16 clusters (25 bits). Collapsing the five zone bits to one cranial bit yields 10 used / **6 spare** — still short, so recycling bits alone is insufficient. **However (principal, 2026-08-04): no SHDR fault records have been generated yet**, so §7.2's "bits 1–4 reserved, not reused" rule does not bind and widening to `uint32_t` is a pre-production change with no migration and no ambiguous historical logs. **The wire format is therefore a cost, not a ceiling.** | `firmware/safety_mcu/include/np_safety_protocol.h:46–60, 90–118`; `docs/np_hw_hub_001.md` §7.4 |

~~**The open question is finding 3: it is not decided whether the safety layer can cut one cluster or only the whole cranial lattice.**~~ **✅ DECIDED 2026-08-16 — the safety layer cuts the whole cranial lattice; per-cluster *policy* is not adopted (§8.4.1, `NP-HW-HUB-001` §7.2.1). OI-HEXTILE-13 and OI-HUB-C07 both close.** D-8's **switch** count stays **18**; its **policy-bit** count resolves to **1**, and the 18 switches are IEC 62304 **Class B**. Findings 1, 2 and 4 were all *costs* of per-cluster policy rather than blockers, and none of them decided the question — the argument that did is in §8.4.1. Their disposition:

- **Finding 1 (package unspecified) — UNBLOCKED, not closed.** The package is still unspecified, but it is no longer *blocked*: demand falls from ~40 to **~23 I/O**, which stops excluding the mid-range options. Selection is now `NP-HW-HUB-001` **OI-HUB-C20**, gating G1 layout.
- **Finding 2 (~38–40 I/O) — superseded.** That figure counted 18 cluster enables. At one it is ~23.
- **Finding 4 (word can be widened cheaply) — correct, and deliberately not exercised.** The word is **unchanged**; §8.4.2's re-layout is not taken and its time-box is therefore moot. Widening remains available should the reopening conditions in §8.4.1 ever be met.

> **Note (firmware, no change requested) — the PA4 collision sits inside a macro set that is already slated for deletion.**
>
> `np_safety_config.h:24` declares **PA4** as SPI1 NSS (load-bearing — `np_safety_main.c` distinguishes frame types by NSS-delineated transfer length), while `:45` assigns `NP_EN_PBM_ZONE4_PIN = (1U << 4)` on GPIOA. Same pin, two owners. (`NP_EN_INTRANASAL_PIN` is also `1U << 4` but on GPIOB — not a collision.) The header calls bank assignments *"provisional pending PCB layout (G1 gate)"*, so this is a provisional-allocation artifact, not a live defect.
>
> **It should not be tracked as a standalone pin conflict.** `NP_SAFETY_EN_PBM_ZONE_0..4` / `NP_EN_PBM_ZONE0..4` encode the **retired 5-module-slot** meaning of "zone". Under the current architecture a zone is *"a named SET OF MODULES, defined as a list of socket addresses"*, authored in `protocols/predefined/00-zones.npps`, with **no fixed count and user-extensible** (CLAUDE.md §3). Crucially, **zones overlap** — that file's §"inclusive membership" rule puts every midline socket in BOTH the Left and the Right zone of its lobe, and requires firmware to dedup. **An overlapping, user-definable set can never be a hardware enable domain**: "cut zone *X*" is undefined when a socket belongs to two zones. Clusters *partition* the lattice; zones do not. This is why D-8 gates per **cluster**, and it makes the zone-enable macros structurally dead rather than merely miscounted.
>
> NP-HW-HUB-001 §7.2 already mandates their removal (five zone bits → one `NP_SAFETY_EN_PBM_CRANIAL`), which deletes PA4's second owner as a side effect. **The correct home is therefore that cleanup, not this open item** — cross-referenced from **NP-FMEA-001 FMEA-M08-04**, which already covers "stimulation enable GPIO shares a pin with another function" (S5 → ALARP, control = *"GPIO assignment verified against schematic in hardware design review"* — a control that has not yet executed, which is why the collision is still present).
>
> **Two siblings carry the same retired concept and should be cleaned up together:** `NP_NTC_CHANNEL_COUNT 6 /* 5 zones + 1 hub */` in `np_safety_config.h`, and NP-FMEA-001 FMEA-M04's *"reads the NTC thermistor ADC channel for each PBM zone (5 zones)"*.
>
> **No firmware change is made by this revision** — the deletion belongs to NP-HW-HUB-001 §7.2 and is gated on OI-HUB-C07 / OI-HUB-C17.

### 8.4.1 Resolution to OI-HEXTILE-13 — keep per-cluster policy out of Class C

> **Status: ✅ DECIDED AND ACCEPTED, 2026-08-16.** Safety review (**OI-HUB-C07**) arbitrated this
> proposal and adopted it unchanged. **OI-HEXTILE-13 CLOSES.** The falsifier below was searched for
> across `NP-RISK-003`, `NP-RISK-004` and `NP-FMEA-001` and **could not be produced** — see "How the
> falsifier resolved" at the end of this section, and `NP-HW-HUB-001` Rev 4 §7.2.1 for the full
> record. Consequences: **D-8's policy-bit count resolves to 1** (its switch count stays 18, now
> explicitly Class B); the Class C enable word is **unchanged**, so §8.4.2's re-layout is **not
> taken** and its time-box is moot; safety-MCU package selection is **unblocked** (`NP-HW-HUB-001`
> OI-HUB-C20); and §8.2.2 option 4's condition is satisfied, though **the connector and conductor
> count remains OI-HEXTILE-14's decision, not this one's.** One requirement was added that this
> section did not anticipate — **HUB-REQ-C05**, `NP-HW-HUB-001` §7.2.2: the Class B per-cluster gate
> must be commanded from a tier *above* the cluster controller it gates, otherwise the one scenario
> per-cluster gating exists for (a wedged controller) is the scenario in which it cannot be commanded.

**This confirms NP-HW-HUB-001 §7.4 rather than proposing something new.** §7.4 already states the synthesis — *"12–16 physical enable lines fanned out from one policy bit"* — and already identifies the residual question as whether per-cluster *policy* is wanted. What this section adds is the arithmetic at **18** clusters and the argument that decides it.

**The trade is not safety-vs-safety.** Both peer documents concede that coarser cutting is never less safe: NP-HW-HUB-001 §7.2 ("over-cutting is a usability cost, never a hazard") and NP-DRV-SHELL-002 §6 ("an **availability** regression, not a safety one, and it is the conservative direction"). Per-cluster policy therefore buys **availability only**.

**Two arguments previously offered here are withdrawn.** They were costs, not blockers, and resting the case on them was wrong:

| Withdrawn argument | Why it does not decide the question |
|---|---|
| *"The peer documents specify incompatible architectures."* | They do not — NP-HW-HUB-001 §7.4 reconciles them explicitly. Per-cluster physical gates and a single policy bit are the same design, and the gates exist in **both** columns |
| *"27 enable bits do not fit the 16-bit Class C wire format."* | True today, but **no SHDR fault records have been generated yet** (principal, 2026-08-04), so §7.2's "bits 1–4 reserved, not reused" rule does not bind and the word can be widened to `uint32_t` as a pre-production change — no migration, no ambiguous historical logs. A cost, not a ceiling. See §8.4.2 |

**The argument that survives, and it is sufficient on its own:**

> **Per-cluster policy puts a *topological* map behind a Class C certification boundary, to buy an availability benefit that Class B can deliver instead.**

Eighteen independently-commanded cluster bits require the safety MCU to hold a socket→cluster mapping. That mapping is not identity — it changes whenever **MECH-2** revisits the clamp shape or **REG-1** re-cuts the lattice. NP-HW-HUB-001 §4.5.1 rejects encoding cluster identity in the logical address for exactly this reason. Behind a Class C boundary the consequence is worse than an awkward table: **a re-clustering becomes a Class C recertification** rather than a regenerated map. It also creates a failure mode that cannot otherwise exist — cutting the *wrong* cluster from a stale map, leaving the faulted one energised.

**Proposal — extend D-8's own two-level logic by one tier, and make only the top tier safety-rated:**

| Tier | Granularity | IEC 62304 class | Mechanism |
|---|---|---|---|
| Fine | per socket (~80) | B | on-module driver register (already D-8) |
| Coarse | **per cluster (18)** | **B** | per-cluster gate enables, for *availability* management |
| Hard | **whole cranial lattice** | **C** | single `NP_SAFETY_EN_PBM_CRANIAL`, in series with everything above |

This keeps NP-DRV-SHELL-002's per-cluster hardware gates and its availability benefit, keeps §7.2's single Class C bit and its 1-GPIO cost, and keeps the topological map outside the certified boundary. **R-11 is preserved** — the Class C bit is in series, so the safety MCU still physically owns the enable path and no Class B fault can re-energise a cut lattice. It also drops safety-MCU demand from ~40 to ~23 I/O, which closes on a mid-range package with margin.

**What would falsify this proposal.** It fails if safety review identifies a hazard where continuing to stimulate on the *other* clusters is safe, continuing on the faulted cluster is not, **and** a whole-lattice cut is itself unacceptable. Since a whole-lattice cut is always available and always safe, that requires the cut to be harmful in its own right — which nothing in the tree claims for PBM. (The cervical-VNS cardiac interlock has its own dedicated <100 ms enable path and is unaffected either way.) **Safety review should either produce such a hazard or close the item.**

**How the falsifier resolved (2026-08-16) — it could not be produced, and the reason is structural.**

The three registers were searched: `NP-RISK-003` (hex-tile module — **zero** cluster-domain entries), `NP-RISK-004` (14 entries; the closest, RISK-18, is an explicit *availability* failure), and `NP-FMEA-001` §3.1/§3.4/§3.8. Nothing qualifies. The useful finding is not the count but the extent column — **no hazard in the enumerated list has an extent of one cluster**:

| Hazard | Source | Actual extent | Existing response | Class |
|---|---|---|---|---|
| Scalp >42 °C / junction >62 °C | RISK-26, FMEA-M04 | **one tile** | on-module NTC throttle, duty → 0 | B |
| Dose mis-read on a part-seated tile | RISK-SHELL-01 | **one tile** | PD1/PD2 ratio, `SEAT#` | B |
| Charge density >40 µC/cm² | FMEA-M03 | **one modality** | its own `NP_SAFETY_EN_*` bit | **C** |
| Stale enable mask after fault entry | FMEA-M01-01 | **everything** | fault entry clears *all* bits | **C** |
| Heartbeat / watchdog loss | CLAUDE.md §4.2 | **everything** | <50 ms all-stimulation cutoff | **C** |
| Cardiac (cervical VNS) | CLAUDE.md §4.2 | **one modality** | dedicated <100 ms path | **C** |

Hazard extents are set by physics — a tile heats, a modality accumulates charge, a rail collapses. The cluster is a **clamp-plate and FPC-fan-out boundary**, chosen in `NP-HEX-ZM-001` §5.4a for one-handed serviceability under RISK-22 and constrained by CLUSTER-1/SYM-1/CONTIG-1. **A control at a granularity matching no enumerated hazard's extent is not performing a safety function** — which is why this section's own argument (a topological map behind a certification boundary) and the falsifier's failure are the same fact seen from two sides.

> **⚠ Residual assumption, named not buried.** This is a **closed enumeration over the hazard list as it stands** (`NP-RISK-003` Rev 1, `NP-RISK-004` Rev 2, `NP-FMEA-001` Rev 4) — **not** a proof that no such hazard can ever be identified. An empirical null over a list is not an analytic necessity, and neither this section nor `NP-HW-HUB-001` §7.2.1 claims one. The physical-extent argument supplies a *reason to expect the null to persist*, which is why reopening condition (a) demands a cluster-extent **physical process** rather than a scenario. **Hazard-list completeness is the assumption this closure rests on**, and `OI-FMEA-06`'s re-analysis of SW01-M04 and the enable-mask rows against the hex lattice is the live work that could disturb it.

> **⚠ The "Class B" label in the proposal table below is shorthand and is corrected at `NP-HW-HUB-001` §7.2.1.** IEC 62304 classifies **software items**, not gate transistors. The 18 per-cluster gates are **hardware risk control measures** and carry no 62304 class; what is Class B is the **hub software that commands them**. The safety-MCU software owning `NP_SAFETY_EN_PBM_CRANIAL` remains Class C. Read the table's middle row as "hardware gate, commanded by Class B software" — and note that **no item is downgraded from C to B**: the gates previously had no stated class at all, and the cluster-controller tier was already Class B under `NP-HW-HUB-001` §7.3.

**The case analysis that leaves the middle tier with no case of its own.** Per-cluster policy would be a middle tier between the Class B per-socket response and the Class C whole-lattice cut. A middle tier earns its place only if some state of the world makes it the right answer:

1. **Fine tier working** → it cuts 1 tile of the ~6 that can be concurrently active at all (§9.2). *Strictly better* availability than cutting a cluster. Not wanted here.
2. **Fine tier untrusted** (module MCU wedged, I2C silent) → the per-cluster **gate** is right — and that gate is retained, at Class B, hub-commanded.
3. **Hub untrusted** → no Class B granularity is reliable, and the correct response is the whole-lattice Class C cut.

There is no fourth case. **Per-cluster policy is needed only when Class B is untrusted, and precisely then the correct response is the cut it would replace.**

**What is not claimed.** The availability benefit of per-cluster granularity is **real** — a montage spreading ~6 active tiles across several clusters keeps its remaining tiles where a whole-lattice cut would end the session. This decision does not dispute that; **it assigns it to Class B, which delivers it in full.** The peers' shared concession that coarser is never less safe is *not* restated as a claim that finer is safer.

**What would reopen it.** (a) A physical process is identified whose extent is one cluster; (b) cluster membership stops being topological — fixed by something un-retoolable *and* anatomically registered, removing the `NP-HW-HUB-001` §4.5.1 objection; or (c) the Class B tier is shown unable to deliver per-cluster availability, making the middle tier's case a Class C one. None holds today.

### 8.4.2 The Class C enable word — what can and cannot be re-laid out

**Standing principal instruction (2026-08-04): no SHDR fault records have been generated, and none are to be assumed until the principal states otherwise.** NP-HW-HUB-001 §7.2 requires enable-word bits 1–4 to be *"reserved, not reused"* on the grounds that *"enable-bit positions appear in SHDR fault records; silently recycling a position would make historical logs misread."* With no records in existence, **that stated rationale does not currently bind.**

> **⚠ But the reservation rule survives the rationale it was given, and §7.2 does not say so.** Enable-bit positions have a **second consumer inside the firmware**: they are identical to charge-monitor channel indices. `NP_SAFETY_MAX_CHANNELS` (14) is specified as *"must match the `s_charge_nc[]` array size in `np_charge_monitor.c` **and the number of `NP_SAFETY_EN_* ` bits**"*; `NP_SAFETY_CH_CLIN_STIM` is defined as *"charge-monitor channel INDEX for CLIN_STIM (**= bit position of the enable bit**)"*; and the test suite shifts the index straight into the mask (`granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)`). **Enable-bit position ≡ `current_ua[]` slot ≡ charge accumulator index — a three-way identity, and it is Class C** (the 40 µC/cm² charge-density limit rests on it). A reader combining §7.2 with the no-SHDR-records finding would reasonably conclude that recycling bits 1–4 is now safe. **It is not.** NP-HW-HUB-001 should record this second rationale — raised as part of **OI-HEXTILE-14**.

**Frame capacity: there is none.** The active heartbeat `np_safety_rx_ext_frame_t` (`firmware/common/include/np_spi_wire_types.h`) allocates all 38 bytes:

| Offset | Field | Bytes |
|---|---|---|
| 0 | `magic[2]` | 2 |
| 2 | `session_status` | 1 |
| 3–4 | `enable_lo` / `enable_hi` | 2 |
| 5 | `channel_count` | 1 |
| 6 | `checksum` (over bytes 0–5) | 2 |
| 8 | `current_ua[NP_SAFETY_MAX_CHANNELS]` | 28 |
| 36 | `ext_checksum` (over bytes 8–35) | 2 |
| | **total** | **38 — no reserved field** |

*(The single reserved byte in `np_safety_rx_frame_t` is not available: that is the legacy 8-byte base retained only for the test suite and to document layout origin. The active heartbeat is the extended frame.)*

**Three changes, three different costs:**

| Change | Bit budget | Cost | Verdict |
|---|---|---|---|
| **Collapse `ZONE_0..4` → one `CRANIAL` bit, leaving bits 1–4 as holes** | 10 of 16 used | **Free.** Nothing above bit 4 moves; `current_ua[]` semantics, `NP_SAFETY_MAX_CHANNELS` and `s_charge_nc[]` all untouched | **Take it.** Worth doing regardless of how OI-HUB-C07 resolves — and it does not depend on the no-SHDR-records finding at all |
| **Compact the word — actually recycle bits 1–4** | 10 of 16, contiguous | **Not free.** Every modality bit shifts down 4, so `NP_SAFETY_CH_CLIN_STIM` 13 → 9 and every `current_ua[i]` slot changes meaning. Touches `s_charge_nc[]`, the hub-side packing in `np_hub_config.h`, and the charge-monitor tests | Only if a positive reason appears. Cosmetic tidiness is not one |
| **Widen for per-cluster policy (18 cluster + 9 modality = 27 bits)** | 27 — does not fit 16 | **Substantial.** `uint16_t` → `uint32_t`; frame must **grow** (zero spare bytes), moving `NP_SAFETY_RX_EXT_FRAME_LEN`, both checksum spans, the compile-time size assertion and eight `offsetof` assertions. If the bit ≡ channel identity is preserved, 27 bits implies `current_ua[27]` = 54 bytes, taking the 200 ms heartbeat from 38 to ~64 bytes | A real cost, on top of §8.4.1's Class C map objection |

**One escape, if per-cluster policy is ever adopted:** PBM clusters plausibly need no charge monitoring at all — charge density is a tES concept (BES/tDCS/CVNS), and `NP_SAFETY_CH_CLIN_STIM` is the only channel index the tree names explicitly. Breaking the bit ≡ channel identity would decouple enable width from `current_ua[]` width and remove most of the frame-growth cost. **That must be a deliberate Class C decision, not a side effect of a re-layout.**

**Net:** the enable word is **a cost, not a ceiling** — but the cost is larger than the GPIO count and larger than an earlier draft of this section stated. The only genuinely free move is the §7.2 collapse with bits 1–4 left as holes.

---

## 9. The concurrency ceiling — the finding that constrains protocol design

This section exists because the arithmetic falls out of §8.1 and materially changes what the hex lattice can be expected to do. It is stated here rather than left implicit.

### 9.1 Available optical power

| Quantity | Value | Source |
|---|---|---|
| T1 peak headset draw | 45–50 W | R-10 |
| Non-PBM overhead (processors, EEG, radios, module logic, fan) | ~6–8 W | CLAUDE.md §4.5 standby + §8.3 |
| **Available to PBM emitters** | **~38–42 W** | |
| T1-A tile at full drive, both channels | 25.0 W | §8.1 |
| T1-A tile at CW 200 mW/cm², both channels | 12.5 W | half current |

### 9.2 Concurrent-tile ceiling

| Mode | Per tile | Concurrent tiles within ~40 W |
|---|---|---|
| Dual-channel, 400 mW/cm² peak, **25 % duty** (R-4) | 6.25 W avg | **~6** |
| Dual-channel, 200 mW/cm² **CW** | 12.5 W | **~3** |
| Single-channel, 200 mW/cm² CW | ~6.3 W | ~6 |
| T1-C, three channels, 25 % duty | ~5.9 W avg | ~6 |

**Roughly six tiles can be active at once. Not eighty.**

> **Note added Rev 8 — read the rows above as operating points, not as a ceiling.** The "~6"
> belongs to the first row's **6.25 W/tile**, and `NP-SES-PWR-001` §2.1 finds that **no protocol in
> the authored library runs at that point.** Measured across `protocols/predefined/`, per-tile draw
> spans **1.3 W** (Autism, 20 % intensity at 25 % duty → **32** concurrent) to **20.0 W** (Vascular
> Baseline, 80 % CW → **2** concurrent). The arithmetic in the table is unchanged and correct; what
> is wrong is quoting a single tile count as the rule. **The governor must be watts** — see
> `OI-HEXTILE-09` and `NP-PWR-BUDGET-001` D-4.

### 9.3 What this means

The hex redesign does **not** increase deliverable dose, and was never going to — dose is bounded by the USB-C PD envelope and by the 42 °C scalp limit (R-9), neither of which the lattice changes. What it buys is **placement freedom**: any six-tile subset of ~80 positions, protocol-defined, rather than five fixed slots. That is a large gain for the research mission SMART-1 was decided to serve (NP-HEX-ZM-001 §4a: arbitrary montage/protocol design), and it is a different gain from the one "whole-vault active tiling" suggests.

Three consequences follow — a fourth was added at Rev 7:

1. **A global concurrent-power governor is required in firmware.** Today nothing prevents a protocol from naming 40 sockets in a `NP_PROTO_TARGET_SOCKET_MASK` bitmap (NP-HEX-ZM-001 §4b) and commanding them all on. That protocol would brown out the rail or trip PD negotiation. The compiler and the session runner both need a power-budget check against the negotiated USB-C PD contract. **OI-HEXTILE-09** — this is a genuine safety-adjacent gap in the delivered v2 wire format, not a future nicety.

   > **Update 2026-09-23 — the session-runner hook now exists, and ships closed.** Until
   > `NP-FW-HUB-001` Rev 2 this consequence was latent: no socket-addressed command could reach an
   > emitter at all (`OI-FWHUB-01`). Rev 2 adds the socket dispatch registry, and with it the runner-side
   > check this item requires, as one function — `np_pbm_power_admit()` — consulted before any socket
   > is driven. **It refuses every load**, because the governor cannot be written until this item is
   > designed and `OI-SESPWR-03` defines its input; the firmware owner of replacing it is
   > `OI-FWHUB-09`. **This item is not closed or changed by that** — the compiler half and the
   > governor's design are still here.
2. **NP-HEX-ZM-001 §6's aggregate thermal concern is bounded by the same arithmetic.** That document warns that "whole-vault active tiling raises aggregate scalp thermal load." It cannot, in the sense feared: the power envelope permits ~6 tiles ≈ 64 cm² of active area, which is comparable to the retired 5-slot design's footprint. The thermal risk is *local* (one tile at 42 °C) and is already owned by the per-tile NTC and Path B1 (NP-THERM-CFD-R1-001), not aggregate.
3. **§6.4's cost problem has a natural answer.** If only ~6 tiles can ever be live, populating all 80 with $11.50 of driver and metering hardware buys placement options, not capability. This is the strongest argument for option 1 in §6.4 — a partially-populated lattice — and the two open items should be decided together.

4. **(Rev 7) The ceiling is a power ceiling, not a tile ceiling — and consequence 3 overstates its
   own case.** `NP-PWR-BUDGET-001` §3.6 runs this section's arithmetic backwards: because irradiance
   scales with drive, **all 80 tiles can be lit simultaneously at ~1.5 % drive for ~30 W**, inside
   R-10's envelope. So *"placement options, not capability"* is too strong — whole-vault simultaneous
   illumination is a capability six tiles cannot produce at any drive level, and
   `pbm_neuro_protocols.md` grades Alzheimer's **A** with the site given as *"whole-head +
   intranasal"*. **The honest bound comes with it:** at that irradiance a 20-minute session delivers
   **7.2 J/cm²**, below the ≥10 J/cm² threshold that same document identifies, so the mode is
   *coverage*, not a therapeutic dose, and must never be presented as one. What the architecture
   cannot do is whole-vault coverage **and** in-band irradiance at once — it must choose, and the v2
   wire format can currently express only one of the two choices. **This does not reverse consequence
   3**, which remains a real cost argument; it removes the claim that full population buys nothing.
   Both belong to `OI-HEXTILE-06`, and `OI-PWR-07` asks whether the mode is a product feature at all.

**One consequence of consequence 1 that is easy to miss.** A governor written as a *tile count* is
wrong in both directions: it forbids the 80-tile 30 W case above, and permits six tiles at full
dual-channel drive — 6 × 25.0 W = **150 W**, three times R-10's peak. **The budget check must be in
watts against the negotiated PD contract**, with per-tile drive as an input, not a tile-count
threshold. Recorded against `OI-HEXTILE-09` (Rev 7); `NP-DRV-SHELL-002` SH2-DRC-02b's pass condition
*"global governor present"* should state the unit.

---

## 10. Decisions

Recorded so they can be challenged individually. None is locked; all are proposals for design review.

| ID | Decision | Rationale | Reversible? |
|---|---|---|---|
| **D-1** | 91-site 5-ring centered-hexagonal lattice at 3.80 mm pitch, site 0 reserved, identical on every tile type | Hexagonal array registers to a hexagonal tile with no corner waste; 3-colourable for the 3-wavelength case; independently reproduces NP-HEX-ZM-001 §3.1's ~90-element estimate; makes T1-B a masking derivation (§4.5) | Yes — FPC artwork only, pre-tooling |
| **D-2** | PD1 at the reserved centre site; PD2 co-located in XY on the scalp-facing layer | Type-independent position; maximum optical symmetry; centre-vs-average offset absorbed by existing factory K coefficients; co-location required for the PD1/PD2 fouling-vs-ageing ratio to be valid | Yes |
| **D-3** | On-module driver on **every** tile type, not only T1-C | 240 hub-side driver channels and ~1,280 power conductors are not buildable, and would put 80 dI/dt sources inside the Faraday envelope beside the EEG harness | **No** — sets the socket pinout and the Hub PCB Rev C architecture |
| ~~**D-4**~~ | ~~TIA + ADC on-module; no PD analog signal crosses the socket~~ **⚠ NOT ADOPTED — OI-HUB-C17c resolved against D-4, principal direction 2026-08-11.** The AFE stays on the cluster controller; **N3 survives** and `PD1_K`/`PD2_K`/`NTC` are back at the socket (§7.2, pins 13–15) | The rationale is **not withdrawn** — deleting the ~80× DG2788A NRE, design-time-fixed gain, and removing the longest high-impedance path were all real, and §5.3's physics is unchanged. It was **outweighed**, on the conservative ground that a controller-mounted ADC never faces the 25 → 62 °C drift question a tile-mounted one must answer against the ±15 % dose claim (FAI-SM-06). See `NP-DRV-SHELL-002` §3.3a for the full trade | **Superseded, not reversible-by-this-document** — §5.3 and §6 still read as if D-4 holds; **OI-HEXTILE-15** |
| **D-5** *(Rev C)* | **19-position** pogo interface, 2.00 mm pitch, **two staggered rows**, springs on socket. `VLED`/`PGND` = **3 + 3** | A per-tile ZIF lever contradicts the NP-HEX-ZM-001 §5.4a accessibility premise; spring contacts absorb the cluster-clamp Z variation the plungers do not. **Count is the union of the networks that actually cross the socket after D-4 was not adopted** (§7.2); `VLED` count follows the ≥2× degraded-case rule (§8.1); two rows are forced by 19 at 2.00 mm pitch inside a 40 mm hex (§7.1) | Partly — **pin count is load-bearing and now tooling-blocking**; contact style less so. *(Rev B: 16 positions, single row, 4 + 4)* |
| **D-6** | VLED = 24 V | Holds peak contact current to **0.35 A/pin (~3× derating nominal, ~2× on loss of one contact)** at Rev 3's 3 `VLED` pins — *Rev B stated 0.26 A/pin and 4× at 4 pins* — and keeps linear drive overhead ≤7 % at practical string lengths. **Adopted programme-wide as OI-HUB-C17b**, which closed `NP-DRV-SHELL-002` OI-SHELL2-01 against its 12 V estimate | Yes, with pin-count consequences |
| **D-7** | Per-cluster I2C segments with UID-derived dynamic addressing, **18 segments** at the v1 lattice (§8.2.1) | Removes the address collision instead of muxing around it; reuses the UID `np_module_map` already depends on; collapses cascaded muxing to one tier. 18 of 32 available segments — the one-tier conclusion survives the count correction | Yes |
| **D-8** *(Rev 4)* | Safety MCU gates VLED across the cranial lattice with **one Class C policy bit**; **18 high-side switches** retained per cluster as **IEC 62304 Class B** availability gates in series with it | STM32G071 has no 80-GPIO option; coarse hardware cut + fine on-module control is defence in depth, not a compromise. **Switch count 18; policy-bit count 1 — RESOLVED 2026-08-16, §8.4.1 / `NP-HW-HUB-001` §7.2.1, OI-HEXTILE-13 closed.** Per-cluster *policy* rejected because it puts a topological socket→cluster map behind a Class C boundary and matches no hazard's extent | Partly — the **class split** is now load-bearing; **HUB-REQ-C05** binds who commands the Class B gate |
| **D-9** *(Rev 13)* | **Every CH_A/CH_B drive stage is regulated to a fixed hardware reference (U3, `REQ-TDRV-01`) and gate-duty-limited in hardware (U4, `REQ-TDRV-02`)** — `NP-SOUP-LFS-001` §13.12 options A and B together, decided by Safety + Hardware Engineering 2026-09-24 | Before D-9, per-tile drive magnitude was bounded only by firmware plus the 62 °C thermal cut, which is a thermal limit standing in for an optical one (`OI-NVRAM-10`). A alone bounds the peak but not the duty, and B alone bounds the duty of an unregulated peak. Together they bound output at ≤ 400 mW/cm² peak and ≤ 200 mW/cm² average per channel, and no firmware, register or stored value can move either. Derivation: `NP-SOUP-LFS-001` §13.14 | Yes — but reversing it reopens `OI-NVRAM-10`. **Rev 25: U4's form conflicts with R-4 as restated.** A conduction-fraction limit forbids CW and any duty above 50 %, so U4 is to be redesigned to bound the average (`OI-HEXTILE-30`). U3 stands |
| **D-10** *(Rev 26)* | **R-4's average term is the laser skin reference in full: Σ Ēᵢ / (200 × C_A(λᵢ)) ≤ 1 over the tile's channels, with no separate per-channel 200** (principal, 2026-09-29, closing `OI-HEXTILE-32`). Scalp only. The intranasal probe is outside R-4 and still has no exposure ceiling (`OI-NASAL-02`) | The reference is the conventional bound for an LED array (`NP-BIB-PBMIRR-001` §3.1). The Rev 25 per-channel 200 was its 660 nm value applied to every wavelength, so it had no derivation of its own at 808–1064 nm. It lets Schiffer 2009 (810 nm, 250 CW) and Wang 2023 (820 nm) run on the average term as their trials did; both remain refused today by the duty cap and U4 (`OI-HEXTILE-30`, `-31`). **Decided with every listed input open, so each is recorded as a residual:** (1) C_A is working knowledge until `OI-BIBPBM-01`, and the pre-signing check's constants (`OI-HEXTILE-31`) may not be released on unverified values; (2) counsel on RISK-03 has not been commissioned (`NP-REG-PBM1064-001` §6A, Q15–Q17); (3) the 1064 nm heating figure (+3.76 °C at 1000 mW/cm²) is unreachable while the 400 peak holds, which makes the 400 a hazard control at 1064 nm, and it may not be raised or retired while `OI-BIBPBM-03` is open (CLAUDE.md §18); (4) the skin-type risk (`OI-BIBPBM-04`) is red-weighted, and 660 nm is unchanged (C_A = 1), but NIR averages rise up to ≈ 1.8×. **Margin:** zero to the reference at every wavelength. The 42 °C scalp limit and the 62 °C junction throttle (R-9) are unchanged and independent. R-5 (600 mW/cm² aggregate peak) is unchanged | **Yes** — a constant in the pre-signing check. Reversal on any residual (a lower verified C_A, counsel, a heating finding) restores a tighter figure with no hardware change |

**Rejected, with reasons:**

- **Scaling the retired 20-pin pinout.** Sixteen of its twenty pins carry LED drive current from hub-side drivers; that model does not survive ×16 socket growth (§6.1).
- **Retaining the ZONE_ID resistor ladder for type detection.** Superseded by UID auto-inventory (R-12), and a ladder cannot encode ~80 positions with usable ADC margin regardless.
- **A per-socket DG2788A gain switch.** The problem it solves disappears under D-4; replicating it 80× would be paying full NRE to preserve an artefact of hub-side metering.
- **One PD pair per cluster** (§6.4 option 3) — would break the per-tile J/cm² metering claim NP-HEX-ZM-001 §4a explicitly protects.

**Values deliberately NOT asserted:** intra-tile irradiance uniformity percentage (§4.4), final emitter part numbers and their V_f/flux (§4.3), exact per-channel emitter counts to string-length divisibility (§8.1), T1-B pod clearance and emitter count (§4.5), bezel width (§3). Each is an open item below, not an omission.

---

## 11. Open Items

| ID | Description | Blocking |
|---|---|---|
| **OI-HEXTILE-01** | **Derive the bezel lateral width.** *Re-scoped Rev 16: this row was a "bezel width conflict" between the 2.5 mm here and NP-THERM-BEZEL-001's 1.0 mm. That 1.0 mm is the bezel **height** (face-to-scalp standoff), is directed (2026-08-11), and is not a width, so there was no conflict to propagate.* The width is the band `W_a = W − 2·bezel` subtracts in §3. The only figure in the set is the 2.5 mm column input of NP-HEX-ZM-001 §3.1, which has no derivation, and this document uses it as the working assumption. A derivation must fit the co-moulded gasket and retention groove (`NP-TOOL-HEXTILE-001` F-TH-06/-07, THEX-MDR-08) and the PDMS window edge bead inside the band. It must also say what the result does to A_a and to every §4.3 irradiance figure (1.0 mm would give 12.51 cm², +17.9 %), to §4.1's pitch ceiling and `OI-HEXTILE-22`, and to the §4.4 inter-tile seam | FPC artwork; **all §4 irradiance figures**; `NP-TOOL-HEXTILE-001` F-TH-09b, F-TH-06 |
| **OI-HEXTILE-02** | Select 660–670 nm and 808–830 nm emitters for the base tile. §4.3's V_f and radiant-flux figures are design targets, not datasheet values. V_f binning ≤±0.1 V per `NP-PROC-FPC-001` **§2.1** (*corrected Rev 9 — this row cited §4.2, which is the Hirose connector; §2.1 is the binning specification, and as of that document's Rev 4 it states the string-construction dependency in §8.1.1 caveat 1 rather than leaving it implied*). Selection closes the string-length divisibility slack in §8.1. **Status at Rev 9: still open, and the blocker is upstream of this document.** Part selection cannot precede `OI-LED-W1`, the NIR wavelength decision, whose owners are **SAB** (science), **Regulatory Counsel** (RISK-03 scope and the published *"810nm"* claim) and **EE Lead** (cost) — open since 2026-07-28 with a decision brief written (`docs/status/pending-decisions.md` §13.2d). Two corrected inputs the owners now have that they did not: elevated V_f is **not** disqualifying on the 24 V rail (§8.1.1, §13.2e(b)), and the only shortlisted in-window part, SFH 4703AS, is **discontinued** while matching the published 810 nm claim on its own centroid specification. One input that is now **verified** (Rev 10): the **Luminus SST-06-IRD-810 / SST-10-IRD-810** dual-junction family, found by re-running the Option C search without the V_f filter and assessed in `NP-PROC-FPC-001` Rev 5 §2.6.3 against the manufacturer's datasheets. **In-window on centroid (λ_c 810 nm typ) on an ACTIVE part**, clearing §2.3 on package, θ_jc, DC current and pulse handling, meeting §2.1's ±0.10 V bin as a standard shipping condition, and giving `V_f` = **2.82 V at 150 mA** — below §8.1.1's dead band, N = 8 at 5.9 % overhead. **It does not clear everything, and this item must not be closed as though it did:** **neither datasheet publishes an L70 figure at all** against §2.3's ≥ 80,000 h, and the shipped wavelength bin spans λ_p 800–830 nm whose lower 8 nm is **below** the 808 nm window. *(Rev 11: a third failure was recorded here — `T_j` max 115 °C against §2.3's ≥ 125 °C. That requirement was never derived and is **RETIRED** at `NP-PROC-FPC-001` Rev 7, so it is not a failure and needs no waiver.)* **Its radiant flux at 150 mA is ~220 mW against §4.3's 95 mW design target**, which moves §4.3.1's operating point rather than validating it — 45 emitters × 220 mW is ~2.3× R-4's ceiling, so the same irradiance arrives at ~65 mA or at fewer sites (interacts with `OI-HEXTILE-20`). **And it does not fit §4.1's lattice** — `OI-HEXTILE-22`. GitHub #333. **Rev 20: `OI-LED-01` is read for the three parts it names** (§8.1.1 Rev 20 note). The results are GH CSSRM5.24 at **1.78 V** (N = 13, not 11), SFH 4718A at **1.41 V** (N = 16, not 14), and L1IZ-0850 at **2.62–2.87 V** (trace fails its self-check, N = 9 or 8). **The 660 nm primary now carries three open problems of its own.** It cannot reach R-4 at 45 sites inside its rated current range (`OI-HEXTILE-29`). Its only centroid group spans **646–666 nm**, typical 657 nm, against CLAUDE.md §3's **660–670 nm**, and the datasheet offers no peak bin to specify on a PO (`OI-LED-03`). And the datasheet publishes **no L70 figure** (`OI-LED-05`). It does fit the lattice | FPC artwork; emitter procurement; §4.3 validity; `NP-FAI-HEXFPC-001`; `OI-HUB-C08` term **U** |
| **OI-HEXTILE-03** | Verify the 21-minute 1064 nm minimum session (§4.3.2) against `protocols/predefined/clinical-03-pbm-cognitive-1064.npps` and the NP-BIB-1064-001 evidence base. A 40 mm tile cannot deliver 36 J/cm² faster. **⚠ Reframed at Rev 7 — this item is stated as a session-length check, and session length is the symptom.** The 21 minutes is a consequence of CH_C's 28 mW/cm², and no protocol re-timing fixes an irradiance that is 9× below what the protocol specifies. **The irradiance-reachability half is split out as `OI-HEXTILE-21`; this item retains only the protocol-authoring verification**, which is still worth doing and is now downstream of OI-HEXTILE-21's answer | 1064 nm protocol authoring; interacts with REG-1. **Sequence after `OI-HEXTILE-21`** |
| **OI-HEXTILE-04** | Illumination model for intra-tile uniformity at 3.80 mm pitch — needs emitter beam angle (OI-HEXTILE-02), PDMS diffuser scattering, and window standoff (SCAN-1) | Uniformity claim; GATE-2 bench design |
| **OI-HEXTILE-05** | T1-B spring electrode pod body diameter → number of depopulated rings (§4.5) and T1-B emitter count | T1-B layout (deferred to Rev 2) |
| **OI-HEXTILE-06** | **Cost/scope decision: ~$11.50/tile driver+metering, ~$920 at 80 sockets (§6.4).** Options: partial socket population / silicon PD on T1-A / per-cluster PD. Decide jointly with OI-HEXTILE-09 and §9.3. **Three inputs added at Rev 7, two of which push the opposite way to the cost model.** (i) ***Populated* is not *driven*** — a populated socket costs 2 mA / 25 mW in standby (§8.3) and its BOM, not 25 W; a fully populated helmet under the §9 governor draws what a 20-tile build draws, so the choice is purely BOM-vs-capability and the two are routinely conflated. (ii) **Options 2 and 3 trade away dose metering**, which is the structural answer to the −79 % declared-vs-measured failure documented in this product category — see the Rev 7 paragraph at §6.4. (iii) **`NP-PWR-BUDGET-001` §3.6 wants *more* sockets populated**, since whole-vault illumination at ~30 W is a capability six tiles cannot produce (§9.3 consequence 4). **The cost model cannot see (ii) or (iii)**; `NP-COST-001` §6 evaluates only the BOM axis | **Programme-level BOM; principal decision.** Note `OI-COST-10` makes this a precondition on setting retail price |
| ~~**OI-HEXTILE-07**~~ | **✅ CLOSED 2026-09-28 (Rev 24): specified in `NP-FW-HEXTILE-001` Rev 1.** Register map (§5), the identity block implemented byte for byte (§7), the odometer and the MODID-4a mate latch (§8), the §8.3 power states (§10), and the local throttle (§6). The local throttle is specified but not buildable until U1 has a temperature input (`OI-FWTILE-01`). No PD readback registers, because D-4 was not adopted. The IEC 62304 item is registered as **SW-04, Class B** (NP-SW-001 Rev 11). What remains is carried as `OI-FWTILE-01`…`-09`. *Original text:* On-module driver firmware spec — register map extending NP-FW-PBM1064-001 §5.1 with PD ADC readback and local NTC throttle; binding ≤2 mA standby / ≤25 mA active budget (§8.3). **Rev 23: must also implement the identity block at `0x40` (format, UID, element count, element types, CRC-32) exactly as `NP-HEX-ZM-001` Rev 9 §4.1 defines it. The hub's `OI-HEXMAP-02` reader is built against it.** Successor to the retired NP-FW-ZM-TINY402-001 (OI-PBM-08) | — (closed) |
| **OI-HEXTILE-08** | Re-scope NP-SW-001 §5.2.1 / RISK-18 ZONE_ID debounce: no ZONE_ID pin exists on this interface (§7.2). Requirement remains in force for surviving non-tile accessory detection; its module-detection scope needs restating under UID inventory | NP-SW-001 revision; traceability |
| **OI-HEXTILE-09** | **Global concurrent-power governor** in the protocol compiler and session runner: a `NP_PROTO_TARGET_SOCKET_MASK` naming more than ~6 tiles exceeds the PD contract (§9.3). Gap in the delivered v2 wire format. **⚠ Scoped at Rev 7 — the governor must be denominated in watts against the negotiated PD contract, never in a tile count.** A tile-count rule fails in both directions: it forbids the 80-tile / ~30 W whole-vault case (`NP-PWR-BUDGET-001` §3.6, inside the envelope) and permits **six tiles at full dual-channel drive = 150 W**, three times R-10's peak. Per-tile drive is an *input* to the check, not a constant — which is the whole point, since irradiance is the therapeutic parameter. **Unbounded today:** nothing in the delivered v2 wire format stops a protocol naming all 80 sockets at full drive, which §3.5 of the power-budget study bounds at ~2.0 kW. Also fix `NP-DRV-SHELL-002` SH2-DRC-02b, whose pass condition reads *"global governor present"* without a unit  **✅ MEASURED at Rev 8 — this is an actual gap, not a hypothetical one.** `NP-SES-PWR-001` audited the authored library: **2 of 20 protocols fit the 40 W emitter budget; 17 exceed it by 1.25× to 40×; 1 is operator-scoped.** All 20 compile clean — `hubCompiler.ts` has no power or budget check. The audit is `scripts/check-pbm-power.ts`, which re-derives the figures on demand. **New blocker: `OI-SESPWR-03` must resolve first** — `frequency: 0Hz` with `duty_cycle:` is undefined and swings the budget of a fifth of the library by 4×, and a governor cannot be written against an undefined input. **New sequencing note:** most of the over-budget condition is caused by lobe-scale zone targeting (`OI-SESPWR-01`), so a governor built against the current library would reject 17 protocols when the correct response to 15 of those is to fix the protocol, not the governor | Session execution safety; **decide with OI-HEXTILE-06**. `NP-DRV-SHELL-002` SH2-DRC-02b needs the unit stated. **Blocked on `OI-SESPWR-03`; sequence after `OI-SESPWR-01`** |
| **OI-HEXTILE-10** | Hub PCB **Rev C** must adopt this interface: 4× LPI2C + one-tier PCA9548A segmentation, **18** per-cluster 24 V high-side switches with safety-MCU enable (count per §8.2.1 — **was stated as 4–10; that figure was the retired 30-socket lattice's**), 3.3 V budget per §8.3. **Deletes** Rev 2's `GAIN_SEL[0..4]`, its five DG2788A switches, and its ZONE_ID-to-gain sequencing (§5 of that document). **✅ The count half is satisfied as of 2026-08-18** — `NP-DRV-SHELL-002` Rev 2 §7.1 and `NP-HW-HUB-001` Rev 6 §7.4 both now provision **20 positions with 18 populated**, so Rev C can be released against 18 (OI-HEXTILE-14 closed). The rest of this item — the segmentation, the 18 high-side switches, the §8.3 budget and the Rev 2 deletions — is unchanged and still open | Hub PCB Rev C; **coordinate before either is released** |
| ~~**OI-HEXTILE-13**~~ | **✅ CLOSED 2026-08-16 with `NP-HW-HUB-001` OI-HUB-C07 (Rev 4 §7.2.1). Per-cluster *policy* was decided against, and §8.4.1 was accepted as written (see §8.4.1's status line and finding 3).** Only this row was never marked closed. It was marked in Rev 15 (2026-09-25, GitHub #437) as bookkeeping, and nothing was decided then. The original text follows. **Is per-cluster safety *policy* wanted at 18 clusters? (§8.4)** The same question NP-HW-HUB-001 §7.4 routes to **OI-HUB-C07**. **Not a conflict between peer documents** — an earlier draft called §7.2 and NP-DRV-SHELL-002 §6 incompatible and that is **withdrawn**; §7.4 reconciles them as *"12–16 physical enable lines fanned out from one policy bit"*, and per-cluster physical gates exist in both. The undecided part is whether independently-commanded **cluster bits** are wanted. Costs of saying yes, none of them decisive: (a) the STM32G071 **package is unspecified** anywhere in the tree and demand at 18 enables is ~40 I/O, excluding every ≤32-pin option; (b) 18 cluster + 9 modality = **27 bits against a 16-bit Class C enable word** — a cost rather than a ceiling, since no SHDR fault records exist so the word can be widened pre-production (§8.4.2); (c) `np_safety_config.h` double-assigns **PA4** to SPI1 NSS and `NP_EN_PBM_ZONE4_PIN` — **re-homed to the §7.2 dead-macro cleanup** (the zone-enable macros encode the retired 5-slot meaning of "zone"; zones are now overlapping authored socket sets in `00-zones.npps` and can never be enable domains), cross-referenced from NP-FMEA-001 FMEA-M08-04. **→ PROPOSED RESOLUTION at §8.4.1:** split the enable by IEC 62304 class — per-cluster gates retained but owned by **Class B** for availability, with a **single Class C** `NP_SAFETY_EN_PBM_CRANIAL` in series as the hard interlock. **The one sufficient argument:** per-cluster policy puts a *topological* socket→cluster map behind a Class C boundary, so a MECH-2 or REG-1 change becomes a recertification rather than a regenerated table, and a stale map can cut the wrong cluster. Preserves R-11 (Class C bit in series), keeps NP-DRV-SHELL-002's availability benefit, drops demand to ~23 I/O. **Falsifier stated:** a hazard where cutting only the faulted cluster is required *and* a whole-lattice cut is unacceptable | **Safety review (OI-HUB-C07) arbitrates; blocks D-8 closure.** Review should either produce the falsifying hazard or close the item. Package selection follows. **Sequence before first SHDR fault record** — §8.4.2 |
| ~~**OI-HEXTILE-14**~~ | **✅ CLOSED 2026-08-16 by `NP-HW-HUB-001` Rev 5**, which re-sized the last peer off 12 (and off `ceil(n/8)` = 10) to **18**: §7.4 (18 populated / **20** provisioned connector positions, 216/240 pins, 4 × PCA9548A on LPI2C1–4), §6.3 (**18** DG2788A, not 10), §5.2 (mux rate restated **per cluster**, and a 16-controller tier-1 ceiling retired — 18 exceeds it, a bind neither this document nor SHELL-002 had noticed), §8.2/§8.5 (18 boards / $114.12; **216** parting-plane conductors, so the headline reduction is ~4.1× not the 8.8× that document claimed) and HUB-DRC-C02 (the `ceil(n/8)` formula inverted into the rule that catches it). **HT-DRC-20 passes.** `NP-DRV-SHELL-002` reached **Rev 3** with editorial fixes only. **Option 4 was NOT adopted** — the broadcast `SAFE_EN` simplification stays conditional on **OI-HUB-C07** / `OI-HEXTILE-13`, and every peer is written to be correct either way. **Residual, tracked elsewhere, not here:** `NP-HW-HUB-001` §5.2's tier-1 *topology* rewrite and its §8 recost remain **OI-HUB-C15**; `NP-DB-005`'s COUNT CONFLICT notice is corrected in `scripts/create_np_db_005.py`. Original analysis retained below. — **Stale cluster counts in peer documents.** SYM-1 makes the count 18; peers were sized off 12 or 10: NP-DRV-SHELL-002 §7.1 provisions **12 cluster-tail connectors, 16 positions** (18 does not fit, and its §3.4 `8 branches × ≤2 clusters = 16` tree cannot reach 18 without a third branch tier or 3-deep branches); NP-HW-HUB-001 §6.3 sizes DG2788A at "**1 per cluster (10 at n = 80)**"; NP-HEX-ZM-001 §5.4a's MECH-2 table prices the flower at **12 boards / $76.08**, actual is **18 / $114.12**. Each needs an editorial pass on its own revision — **not corrected by this revision**, which owns only NP-HW-HEXTILE-001. **Additionally: NP-HW-HUB-001 §7.2 justifies its "bits 1–4 reserved, not reused" rule *solely* by SHDR fault records, but a second, unstated rationale also holds — enable-bit position is identical to the charge-monitor channel index (`NP_SAFETY_MAX_CHANNELS`, `NP_SAFETY_CH_CLIN_STIM`, `current_ua[]`), which is Class C.** A reader combining §7.2 with the standing no-SHDR-records instruction would wrongly conclude that recycling bits 1–4 is safe. §7.2 must record the second rationale (§8.4.2). **→ PROPOSED RESOLUTION at §8.2.2:** two independent limits bind — **C1** the 16 provisioned connector positions, and **C2** the `8 branches × ≤2` I2C tree. **C2 does not exist under this document's D-7** (4 × LPI2C × PCA9548A = 32 segments, 18 used), so it resolves with OI-HUB-C17; **C1 does not self-resolve** and must be fixed before Rev 3 layout. Recommended: **adopt D-7's tree + provision 20 connector positions** (the cheap axis — adding tails costs far less than widening them, per SHELL-002 §7.3), and **if §8.4.1 is accepted, remove `SAFE_EN[n]` from the tail** — it is the only star component of the 12-conductor tail, so a single broadcast cranial enable permits a multi-drop trunk and makes connector count insensitive to a future REG-1 re-cut. Options 5–7 (decouple electrical/mechanical clusters, re-cut the lattice, pair clusters onto shared tails) assessed and not recommended | NP-DRV-SHELL-002, NP-HW-HUB-001 Rev 3, NP-HEX-ZM-001 revisions; **coordinate with OI-HEXTILE-10 and OI-HUB-C17 before either is released** |
| **OI-HEXTILE-15** | **§5.3 and §6 still read as though D-4 holds, and D-4 was not adopted.** `NP-DRV-SHELL-002` Rev 2 §3.3a resolved **OI-HUB-C17c against D-4** (principal, 2026-08-11): the switched-gain TIA, PD mux, NTC mux and ADC stay on the cluster controller. §7 is re-cut accordingly (Rev C), but **§5.3 ("The TIA question — and why it stops being a hub problem"), §6.2's U2 dual-TIA line, and §6.4's BOM still describe on-module conversion.** This revision deliberately did **not** rewrite them — the change reaches the driver topology, the per-tile BOM and OI-HEXTILE-06's PD-population options, and is larger than a socket-interface re-cut. **What is affected:** U2 (dual TIA, $0.32/tile ≈ $26 at 80 tiles) may be deleted from the module and its function returns to the controller; the 12-bit-ADC-with-PGA requirement on U1 (§5.3, §6.2) relaxes, since dose metering no longer depends on the on-module ADC — which may reopen the ATtiny402-vs-tinyAVR-2-series choice; §6.4's ~$11.53/tile figure moves. **What is NOT affected:** the InGaAs PD selection and co-location (D-2, §5.1–5.2), which are optical decisions independent of where the transimpedance stage sits, and therefore OI-HEXTILE-06's ~$10/tile PD-population question — still the dominant term, still orthogonal | **Module BOM; OI-HEXTILE-06; §6.2 part selection.** Not tooling-blocking — the socket interface (§7) is already correct |
| ~~**OI-HEXTILE-16**~~ | **✅ CLOSED 2026-08-11 — one name per conductor adopted (§7.4).** Pin 18 is **`ELEC_SHLD`** (over `GUARD`, which collides with the *DRL-driven guard plane* on L1 that `NP-DRV-SHELL-002` §3.5/§4.1/REQ-EMI-02 specify — the pin is driven from that plane but is not it). Pin 12 is **`ALERT#`** (over `/ALERT`: a leading `/` is the hierarchical-path separator in EDA net naming, so `/ALERT` reads as a root-scope path and can be silently re-scoped on netlist import). Propagated through this document, `NP-DRV-SHELL-002` and `NP-HW-HUB-001`. Residual naming items split out to **OI-HEXTILE-17** | — (closed) |
| ~~**OI-HEXTILE-17**~~ | **✅ CLOSED 2026-08-11 — both residuals settled on the same criterion that decided OI-HEXTILE-16 (§7.4).** **(a) `SEAT_N` → `SEAT#`.** The item was raised as "a third active-low convention", i.e. cosmetic. It is not: **`_N` already means *cardinality* in this codebase** — `firmware/hub_control/tests/np_module_map_tests.c` defines `PBM_TILE_N` / `EEG_TILE_N` as `sizeof(x)/sizeof(x[0])` — so `SEAT_N` reads as *"number of seats"*. That is a live ambiguity, not a style difference, and it settles the convention **toward `#`** rather than away from it: `ALERT#` is confirmed, not reversed, with SMBus precedent (`SMBALERT#`; the in-tree vendor header exposes `I2C_ISR_ALERT` for the same line). **Rule of record: active-low interface signals take `#` — never `_N`, never a leading `/`.** **(b) `ELEC_SIG` → `ELEC`.** Ownership: the interface is defined by this §7.2 and `NP-DRV-SHELL-002` §5.1.4, and `NP-HW-HUB-001` §7.4 explicitly *adopts* that contract, so two normative pin tables do not change to match one banner's prose. Accuracy: the pin is dual-rated to carry **tES stimulation current** (≤2 mA T1 / ≤4 mA T2), and `_SIG` biases the reader toward the recording role — the half that is *not* safety-relevant. **Doc→firmware mapping stated, since `#` is not a legal identifier character: `<SIGNAL>#` → `<SIGNAL>_L`, never `_N`.** Related but NOT closed by this: `NP-FMEA-001` **OI-FMEA-01** records that the firmware's ten active-LOW enable lines carry polarity only in comments; this decision adds no further unmarked names and gives that item a convention to converge on. **No firmware change requested** | — (closed) |
| **OI-HEXTILE-11** | Pogo contact qualification: ≤50 mΩ over ≥500 cycles **in the EEG signal path** (pin 13). Contact noise in a µV recording chain is not covered by the resistance spec alone | T1-B EEG performance; FAI |
| **OI-HEXTILE-12** | FPC stack-up, trace width/spacing, and copper weight for a 24 V / 1.04 A tile. PDMS bonding (SiO₂ 75 nm interlayer + O₂ plasma) and the 200-cycle IEC 60068-2-14 qualification inherit unchanged from NP-HW-FPC-001 Rev 5 §7 and remain BLOCKING | FPC artwork release |
| **OI-HEXTILE-20** | **§8.1's 25.0 W/tile peak may not be a legal operating point, and the contact count depends on it.** §4.3.1 puts each T1-A channel at **403 mW/cm²** at full 150 mA drive — *"by construction"* on R-4's 400 mW/cm² ceiling — so **both channels simultaneously is 806 mW/cm² against R-5's 600 mW/cm² aggregate ceiling.** Two readings, and the document does not say which holds: (a) **R-5 binds only the three-channel case** its source (`NP-FW-PBM1064-001` Rev 2, OI-PBM-05) addresses, in which case say so explicitly, because §4.3.2 presents 566 mW/cm² *"vs 600 mW/cm² ceiling ✓"* as a general check and a reader will apply it generally; or (b) **R-5 binds T1-A too**, in which case the true per-tile peak is **~18.6 W**, not 25.0 W. **Reading (b) is not conservative bookkeeping** — it propagates into the rail current (1.04 A → 0.78 A), the per-pin contact current that set `VLED` at 3 contacts under the ≥2× degraded-case rule (D-5, D-6, §8.1), §9's entire concurrency table, and `NP-PWR-BUDGET-001` §3.5. D-5's own text says the pin count is *"load-bearing and now tooling-blocking"* | **Socket contact count (D-5) — tooling-blocking.** Resolve with `NP-FW-PBM1064-001` as R-5's owner; propagates to §9, `NP-DRV-SHELL-002` §5.1, `NP-PWR-BUDGET-001` §3.5 |
| **OI-HEXTILE-21** | **The 1064 nm channel cannot reach the irradiance of its own flagship protocol, at any tile population.** §4.3.2 gives CH_C **28 mW/cm²** at 30 sites. `docs/pbm_neuro_protocols.md` grades cognitive enhancement **A** at **1064 nm CW, 0.25 W/cm², 60 J/cm²/site, 8 min** — **9× above** what the tile delivers; a hypothetical 90-site 1064-only tile reaches only ~85 mW/cm², still 3× short. **This is an emitter-efficiency wall, not a budget or layout shortfall:** η_wp ≈ 4.8 % at 1064 nm (§4.3), and R-6 already caps drive at 120–180 mA for L70, so neither more watts nor more sites closes it. Three responses, none free: select a materially better 1064 nm emitter (extends `OI-HEXTILE-02` to CH_C, and `NP-PROC-FPC-1064-001`'s EPITEX reference part is the current bound); accept CW-only operation and long sessions, stating the protocol NeurOne actually targets; or **claim 1064 nm against the Alzheimer's protocol (1060–1080 nm, 0.1–0.3 W/cm²) rather than the cognitive one** — a band a 90-site tile can approach. **Split from `OI-HEXTILE-03`, which framed this as session length.** Note the same wall bounds every 1070 nm competitor (`docs/reference/competitive-position.md`) | **1064 nm claims and protocol authoring; `OI-HEXTILE-02` scope for CH_C.** Not tooling-blocking — the lattice is unaffected either way |
| ~~**OI-HEXTILE-23**~~ | **✅ DECIDED 2026-09-24 (Rev 13, D-9) — regulated.** Safety + Hardware Engineering chose `NP-SOUP-LFS-001` §13.12 A + B. §4.3.1 and §8.1.1 describe the intended circuit, §6.2 gains U3 (the regulating element) and U4, and R1–R3's wrong *"§6.3"* citation is struck. The work left is implementation (`OI-HEXTILE-24`) and the consequences for the record (`OI-HEXTILE-25`). *Original text:* **The drive stage is specified as switched (§6.2) and as constant-current (§8.1.1, §4.3.1), and only the second supports the hardware optical-ceiling claim** (raised Rev 12, `NP-SOUP-LFS-001` §13.12). §6.2 lists no regulating element — tile-MCU PWM, a low-side FET and a sense resistor read by firmware — so while the FET conducts the current is set by 24 V, the string's `V_f` (which falls as the string heats) and the resistor: fixed by hardware, but unregulated. §4.3.1's *"cannot be commanded past its own optical limit even before firmware intervenes"* and §8.1.1's dropout rule both assume a regulator holding the sense voltage to a fixed reference. Decide the topology; if regulated, add the element to §6.2 and derive R1–R3 from the reference (the row's *"§6.3"* citation is wrong); if switched, retire §4.3.1's hardware-ceiling sentence and §8.1.1's dropout bound. The same decision answers `OI-LFS-04` / `OI-NVRAM-10` (whether any non-firmware bound on per-tile drive magnitude exists) | **Owner:** EE + Safety. **Blocking:** `OI-LFS-04`; §4.3.1; §8.1.1; tooling of the rigidizer |
| **OI-HEXTILE-24** | **Implement D-9: select U3 and U4 and fix their values** (raised Rev 13). (a) `I_cap`: sized from the selected emitter (`OI-HEXTILE-02`) so that the regulation's upper tolerance limit, at the highest-flux bin and coldest junction, gives ≤ 400 mW/cm² (`REQ-TDRV-01`). It will be below 150 mA (`OI-HEXTILE-25`(a)). (b) R1–R3 = `V_ref` / `I_cap`. (c) U3's dropout becomes §8.1.1's `V_dropout`, and it enters the ±0.10 V-bin arithmetic of Rev 10, where N = 8 already needs a dropout of zero, so this may force string length (`OI-HEXTILE-18`/`-19`). (d) U4's form and tolerance: ≤ 50 % at its upper limit, `T_w` ≥ 250 ms. A window above ~40 s re-derives its thermal argument (`NP-SOUP-LFS-001` §13.14). (e) Area on the 22 × 14 mm rigidizer, and the BOM line for §6.4 — uncosted, and every cost figure is a floor (`CLAUDE.md` §2.1). (f) Whether CH_C carries U3/U4 for layout uniformity. (g) Verification: a bench test that U3 holds `I_cap` and U4 trips while U1 commands 100 % duty and full `CUR`. **Until this closes, the thermal cut is the only non-firmware bound** | **Owner:** EE + Safety. **Blocking:** `RISK-FWHUB-10`, `RISK-NVRAM-04`; rigidizer tooling; §6.4 |
| **OI-HEXTILE-25** | **What D-9 does to the rest of the record** (raised Rev 13, found while deriving `REQ-TDRV-01`/`-02`). (a) §4.3.1's full-drive point, 403 mW/cm² at 150 mA, is 0.75 % over the cap D-9 enforces, so every figure computed at "150 mA full drive" (§4.3, §8.1, §9, `OI-HEXTILE-20`) is at a point that is no longer reachable. (b) CW at 200 mW/cm² becomes ~50 % PWM at `I_cap`, not DC at ~75 mA. §4.3.1's WPE advantage is forgone, and the extra heat is a §9.3 term. (c) The hub clamps every mode, CW included, to 25 % duty (`NP_PBM_DUTY_MAX_REG`, `NP-FW-PBM1064-001` §5.3), so R-4's 200 mW/cm² CW ceiling is unreachable through the firmware today. Decide which limit governs CW (`OI-HEXTILE-07`, the tile firmware specification). U4 at 50 % sits behind the firmware's 25 % and does not conflict with it. (d) `CUR` encodes 0–180 mA (`NP-FW-PBM1064-001` §5.1), about 480 mW/cm² at the design-target flux. Under D-9, codes above `I_cap` saturate, and the register map should say so. **Rev 24: (c) and (d) answered by `NP-FW-HEXTILE-001` §5.4–§5.5.** (c) ~~The 25 % ceiling governs CW, on the tile as on the hub, because nothing requires reaching 200 mW/cm² CW.~~ **Withdrawn Rev 25:** the 25 % ceiling made CW undeliverable as CW. R-4 no longer carries a duty cap, and CW is the gate held on (`NP-FW-HEXTILE-001` Rev 2 §5.5). What (c) asked is now `OI-HEXTILE-30`/`-31`. (d) The register map now states the saturation, and firmware holds no copy of `I_cap`. (a) and (b) stay open | **Owner:** EE + FW. **Blocking:** §4.3.1, §9.3; `NP-FW-PBM1064-001` §5; `OI-HEXTILE-07` |
| **OI-HEXTILE-27** | **Is drive current regulated per channel or per string? §6.2 and §8.1 disagree** (raised Rev 17, 2026-09-27, from `NP-FEAS-FNIRS-001` Rev 2 §4 Risk B). §6.2 lists one FET (Q1–Q3), one sense resistor (R1–R3) and one U3 **per channel**. `REQ-TDRV-01` is written per channel, and `OI-HEXTILE-24`(b) sizes R1–R3 per channel. But T1-A carries **4 CH_A strings and 3 CH_B strings** (§8.1.1 worked example). §8.1 and §8.1.1 both say shorter strings double *"the parallel strings and sense resistors"*, which follows only if each string has its own sense resistor. The two readings are different circuits. **(i) Per channel:** the strings are paralleled under one regulator, so their current split rests on V_f matching alone (the ±0.10 V bin, RISK-08). Q1/Q2 and U3 then carry the whole channel current, ~600 mA at 4 × 150 mA, not the 180 mA at which §6.2 computes FET dissipation. **(ii) Per string:** one U3 and one sense resistor per string (7 on T1-A), with `I_cap` and `REQ-TDRV-01` restated per string, at a rigidizer-area and §6.4 BOM cost. **Why it reaches beyond the circuit:** under (i), any future per-string on/off control forces the whole channel `I_cap` through the strings left on. With 1 of 4 CH_A strings on, that is ~4× per-emitter current, above the emitter rating and R-4's local 400 mW/cm² peak. So per-string switching is safe only under (ii). `NP-FEAS-FNIRS-001` proposes per-string switching as an fNIRS source; this item neither adopts nor rules it out. **Nothing is decided here** | **Owner:** EE + Safety. **Blocking:** `OI-HEXTILE-24` (U3 selection); §6.2; §6.4; any per-string drive control |
| **OI-HEXTILE-28** | **Dimension §7.3's pad-length stagger** (raised Rev 18, 2026-09-27, from `NP-FW-NVRAM-001` `OI-NVRAM-04`, GitHub #444 / #437). §7.3 fixes the mating **order** of the four groups and gives no length difference between any two of them. The dimension is required by §7.3's own mechanism, not by a timing consumer. (a) Each gap between successive groups must exceed the worst-case height differential across the array: the ±0.5 mm Z tolerance, tilt across both staggered rows (the two-row consequence above), and the curved-pair variation `NP-DRV-SHELL-002` §5.1.6a computes. Otherwise a return can trail a supply, or `SEAT#` can read home while group 3 is still partial, which is `RISK-SHELL-01`'s silent under-read. (b) The sum of the gaps, plus the wipe each group needs, must fit inside the spring pin's **usable working deflection**. That deflection is unspecified (`NP-DRV-SHELL-002` `OI-SHELL2-12`), so this item cannot close before that one reports from the `SH2-DRC-08` bench. (c) State it as a pad-artwork requirement that `SH2-DRC-05a` checks and `SH2-DRC-10b` verifies on the bench. **Do not derive it from a timing requirement.** `NP-FW-NVRAM-001` §4.3.1 shows that no firmware needs the break-first interval's duration, and any future consumer of that interval must state its own extraction-velocity basis. **Do not set a number from typical pogo-pin practice ahead of (b).** Couples to `OI-SHELL2-13`, since a single-point `SEAT#` bounds what (a) can guarantee | Pad artwork release; `SH2-DRC-05a`, `SH2-DRC-10b` |
| **OI-HEXTILE-29** | **The 660 nm primary cannot be operated at ≤ R-4's 400 mW/cm² at 45 sites inside its own rated current range** (raised Rev 20, 2026-09-28, GitHub #333). GH CSSRM5.24's datasheet (v1.1) rates `I_F` **min 100 mA**, twice: Maximum Ratings, and *"Do not use below 100 mA"* on the permissible-current chart. Its relative flux is 0.146 at 100 mA and 0.219 at 150 mA, against 1068 mW typical at 700 mA, so ~156 / ~233 mW per emitter. A 45-site T1-A CH_A (10.61 cm²) is therefore **~663 mW/cm² at the datasheet minimum** and ~990 mW/cm² at 150 mA. R-4 needs ≤ 94.3 mW per emitter, which is ~60 mA by extrapolation, below where the datasheet's curve starts. **So `OI-HEXTILE-24`(a)'s `I_cap` has no legal value for this part at this site count**, and U3 would regulate it below its rated range. T1-C's 30 sites are also over, at ~442 mW/cm² at 100 mA. Two caveats bound the numbers. The flux curve is drawn **broken below ~175 mA**, which footnote 6 says means *"higher differences between single devices"*. And the figures are typical: the brightest group (A2, ≤ 1165 mW at 700 mA) is the one `I_cap` must be sized against. **Ways out, not chosen here:** (a) **fewer CH_A sites.** At 100 mA the ceiling allows **~24 sites** against A2 and ~28 against V7, roughly half of 45, and `OI-HEXTILE-20` already asks for fewer sites; (b) **a lower-flux 660 nm part.** §2.6.1's alternates are unverified, so this is a search; (c) **ask ams-OSRAM** whether the 100 mA floor applies to the peak current of a pulsed drive or to the average, and what fails below it. That changes only what "rated" means, and it is a supplier question, not a reading. **Do not resolve this by PWM:** R-4 is a *peak* ceiling, and duty does not lower the peak. **Do not close `OI-HEXTILE-02` on this part until one of (a)–(c) is taken.** **Rev 21: (a) is WITHDRAWN as a way out, and (b) is now the recommended path.** The protocols need peak irradiance, not only a ceiling. The library's CH_A targets are 22–322 mW/cm², reached through analog `CUR_A` only, because the compiler writes `intensity` to the current register. The rated minimum sets a floor of about `N` × 15 mW/cm², which is **353 mW/cm² at 24 sites**, above every authored target, and still 88 mW/cm² at 6 sites. So fewer sites trades an R-4 violation for a library the channel cannot reach. **(b) resolves it** if the part sits near **~1.1 mW/mA with a rated range reaching ~5 mA**. The whole library is then 5–90 mA at 45 sites, and nothing moves in §4.2. Two lead-grade candidates are **GH DASPA2.24** (OSCONIQ P 2226, 2.2 × 2.6 mm, which fits §4.1 in any orientation; ~110–115 mW at 100 mA, 2.15 V, 250 mA max; one distributor lists it obsolete) and **MP-2835-1100-DR** (2.8 × 3.5 mm, fits row-aligned; 63 mW at 60 mA, 2.0 V). **Neither is datasheet-verified.** What verification must show: rated minimum current ≤ ~5 mA; flux linear enough at 5–10 mA to trust a 7-code setting; the `V_f` groups; the wavelength bin against 660–670 nm; L70; lifecycle status. §13.2e(k) carries the comparison **Rev 22: the three datasheets are read, and the lead-grade figures above are partly wrong.** *GH DASPA2.24* is rated **`I_F` min 30 mA** (*"Do not use below 30 mA"*), where it gives ~29.5 mW typical, a 45-site floor of **~125 mW/cm²**. It is 98.3 mW typical at 100 mA (89–130 mW in one ordering code), not ~110–115. It cannot deliver `clinical-06` (22), `clinical-05` Maiello (30), `clinical-04` Cassano (36), `clinical-07` (81) or `clinical-01` Wozniak (121), so it is **dropped as the preferred lead**. *MP-2835-1100-DR* states **no minimum current**. Its flux curve is plotted from 10 mA (a floor of ~42 mW/cm²) and is linear through the origin, which reaches 22 / 30 / 36 mW/cm² at ~5.2 / 7.0 / 8.4 mA **by extrapolation only**. It is 88 mA at 400 mW/cm², within a 200 mA maximum. It has an orderable 660–665 nm dominant-wavelength bin (RA), 8 mW power bins, and a datasheet revised July 2026. *L1SP-DRD0002800000* (DS237, 2018) states no minimum either, but is plotted only from 25 mA (floor ~99 mW/cm²). Its one peak bin is 650–670 nm. **None of the three datasheets gives an L70 figure. MP-2835-1100-DR is the best of the three at delivering the protocols, and it still does not close this item.** Closure needs two things: **a bench characterisation of MP-2835-1100-DR at 3–10 mA** (flux against current, part-to-part spread, drift with temperature), and **Luminus confirming in writing** that no minimum operating current applies. Until then, the library's three lowest-irradiance protocols have no datasheet-verified 660 nm part at 45 sites. The §8.1.1 string rule must also be re-run for the chosen part (Rev 22 row in the allocation-slack table). §13.2e(l) carries the comparison | `OI-HEXTILE-24`(a) `I_cap`; §4.2 CH_A allocation; §4.3.1; `OI-HEXTILE-02` |
| **OI-HEXTILE-26** | **Does the fitted photodiode respond at 660 nm and 808 nm?** (raised Rev 14, 2026-09-25, from `NP-CONV-001` `OI-CONV-08` (b), GitHub #394). §5.1 fits Hamamatsu G12180-010A to every tile on the premise *"InGaAs is broadband (600–1700 nm)"*. The part's published range is **0.9–1.7 µm**, which excludes CH_A (660 nm) and CH_B (808 nm), and T1-A carries nothing else. **Confirm from the datasheet's spectral-response curve** the responsivity at 660, 808–830 and 1064 nm. If it is negligible at 660/808, then: (i) PD1/PD2 cannot meter CH_A or CH_B on any tile, so the dose claim and the RISK-14 fouling/ageing discriminator (R-8) have no sensor on the flagship tile; (ii) §5.1's one-SKU trade reverses, and T1-A needs a silicon (or extended-range) PD, with its own K coefficients; (iii) T1-C needs both a Si and an InGaAs PD, or one extended-range part. The listings also give the package as **metal TO-18, φ1.0 mm**, not SMD, so the inherited 1.6 mm annular-ring pad needs re-checking. **Dose-metering path: not relaxed, not retired, nothing selected here.** Also owns correcting `NP-PROC-FPC-1064-001` §4.1–§4.2 (`OI-PBM-HW-09`) and `NP-FW-PBM1064-001` §6.1's *"InGaAs is broadband"* | **Owner:** EE Lead + Optical. **Blocking:** **PD selection for every tile; GATE-2 bench; `NP-FAI-HEXFPC-001`** |
| **OI-HEXTILE-22** | **§4.1's 3.80 mm lattice pitch was never checked against the footprint of the emitter that sits on each site, and most of the shortlist does not fit it** (raised Rev 10, GitHub #333). A triangular lattice of pitch `p` puts the 60° neighbour at `(p/2, p√3/2)`; at p = 3.80 mm that is **(1.90, 3.29) mm**, so two axis-aligned square packages clear each other only at **≤ 3.29 mm**. Luminus SST-06/SST-10-IRD-810 is 3.45 mm square (**3.65 mm at maximum material**, +0.20/−0.00) and needs ≥ 4.21 mm; **ams-OSRAM SFH 4718A (3.75 mm) needs ≥ 4.33 mm and SFH 4703AS (3.85 mm) needs ≥ 4.45 mm** — both above §4.1's own **4.04 mm** ceiling at n = 5, so for them it is not resolvable by spending the 1.2 mm boundary clearance. **Only Lumileds L1IZ-0850 (1.9 × 1.37 mm) fits**, and only with its long axis on the 60° axis — which is the 850 nm part, the one that fails the wavelength window. ~~A 45° package rotation clears the 3.45 mm part by ~0.03 mm and is not a manufacturable margin.~~ **Corrected Rev 19 (§4.1 note): no rotation helps.** Uniform rotation is bounded by `p√3/2` = 3.29 mm at every angle, and 45° is the worst case at `p/√2` = 2.69 mm. A per-sublattice search found nothing above 3.29 mm, and the three parts at or above 3.65 mm exceed even the orientation-free density bound of 3.54 mm. **Three ways out and this item does not pick one:** drop to n = 4 (61 sites, 5.05 mm available, and every §4.3 irradiance figure re-derives on the lower count); hold 91 sites and require a ≤ 3.29 mm package, which is a procurement constraint `NP-PROC-FPC-001` §2.3 does not currently state (it says only *“SMD 2835 or equivalent”* — ~~and 2835 is 2.8 × 3.5 mm, whose 3.5 mm axis already exceeds 3.29 mm~~ **corrected Rev 19: ≤ 3.29 mm binds a *square*. The general constraint is a 3.80 × 3.29 mm envelope with the long axis on a lattice row, and a 2835 fits it with 0.30 / 0.49 mm clearance.** The site placement tolerance or courtyard that those clearances must exceed is stated nowhere and belongs to this item); or re-derive the pitch jointly with the part under `OI-HEXTILE-02`. **Sequence before `OI-HEXTILE-02` and `OI-HEXTILE-04`** — beam angle and uniformity are both downstream of whichever pitch survives | §4.1 pitch; **all §4.3 irradiance figures**; FPC artwork; `OI-HEXTILE-02` part selection; `NP-PROC-FPC-001` §2.3 package requirement |
| **OI-HEXTILE-30** | **Redesign U4 (`REQ-TDRV-02`) to bound the time-averaged irradiance, not the conduction fraction** (raised Rev 25, R-4 as restated). U4 holds conduction to ≤ 50 % over any `T_w` ≥ 250 ms, so no channel can conduct continuously, at any current. CW protocols (Cassano 36, Maiello 30, Naeser 22 mW/cm²) and every duty above 50 % are undeliverable in hardware. The bound U4 exists for is still required: without it, firmware can hold a channel at `I_cap` continuously, at up to 400 mW/cm² average (`NP-SOUP-LFS-001` §13.14). The redesign must keep that bound at **200 mW/cm² average** and allow **any duty, CW included, at or below it**. **Rev 25 correction:** a per-channel hardware bound does not enforce R-4's weighted sum across the tile's channels, because two channels each at their own bound score 1.61 at 660 + 808 nm. So either U4 bounds the sum, or the sum rests on the pre-signing check and firmware alone. Say which, and record it as a hazard-control decision. **Rev 26 (D-10):** the per-channel average U4 must hold is now 200 × C_A of that channel's wavelength (200 at CH_A 660 nm, ≈ 329 at CH_B 808 nm; CH_C 1064 nm is held by U3's 400), not 200 for every channel. The "200" figures below are CH_A's; each channel's `I_cw` is sized to its own figure. Candidates for EE and Safety, none chosen: (a) a two-level limit keyed to a second fixed current `I_cw`, sized so the highest-flux bin gives ≤ 200 mW/cm² at `I_cw`. A window in which the current never exceeds `I_cw` is unlimited, which bounds it at 200 by flux. A window in which it does exceed `I_cw` holds **all** conduction to ≤ 50 %, which bounds it at 0.5 × 400. Limiting only the time spent above `I_cw` is not enough: half a window at 400 and half at `I_cw` averages 300; (b) an average-current integrator. §13.14 rejected (b) because it needs the emitter's flux per mA at low current, and (a) needs the same datum at `I_cw`. That emitter dependence is now the cost of delivering CW and must be paid, sized once `OI-HEXTILE-02` selects parts. Removing U4 without a replacement is an ISO 14971 decision, not a redesign (CLAUDE.md §18). **Verification:** U4's bench test (`OI-HEXTILE-24`(g)) gains two cases: CW held at the CW-ceiling current is not cut, and 100 % commanded duty at `I_cap` is cut to ≤ 200 average | **Owner:** EE + Safety. **Blocking:** every CW protocol and every duty above 50 %; `OI-HEXTILE-24`(d); rigidizer tooling |
| **OI-HEXTILE-31** | **Remove the 25 % duty cap from every place that enforces it, and replace each clamp with a refusal** (raised Rev 25, R-4 as restated). Five places silently turn a protocol into a different stimulus: (i) `NP-NPPS-REF-001` limits `duty_cycle` to 1–25 %; (ii) `app/web/src/lib/hubCompiler.ts` `dutyReg()` clamps with `Math.min(…, 0x32)`, so 50 % is compiled to 25 % **with no error**, the worst of the five, and the other runtimes must be checked for the same; (iii) the hub, `NP_PBM_DUTY_MAX_REG` in `np_pbm_drive_set_duty()` (`NP-FW-PBM1064-001` §5.5); (iv) the tile, now corrected in `NP-FW-HEXTILE-001` Rev 2; (v) U4 (`OI-HEXTILE-30`). **Sequence:** the replacement comes first. It is a pre-signing check that refuses a protocol whose peak exceeds 400 mW/cm², or whose time-averaged irradiance breaks R-4's average term at its declared duty: the weighted sum over the tile's channels (Rev 26, D-10: no separate per-channel 200). **Its C_A constants may not be released until `OI-BIBPBM-01` verifies them.** `app/web/src/lib/protocolValidator.ts` already refuses a duty above `NPHardwareLimits.pbmDutyCycleMaxPercent` (25), and `pbmCWMaxMWcm2: 200` is declared in `hardwareLimits.ts` (and `NPHardwareLimits.swift`, where iOS also uses it as the CW irradiance for its dose estimate). Since D-10 that single figure is the 660 nm value only, and it is replaced by the per-wavelength term, not edited. So the check is a change to an existing validator, mirrored in each runtime. It is not a new component. The clamps go only after it. The hub clamp is today's only firmware bound, so deleting it first would remove a hazard control with nothing in its place. **Also here:** `frequency: 0` with a `duty_cycle` must become a parser error (`OI-SESPWR-03`), since CW now has a meaning. And **the library must be re-derived from its sources.** Every pulsed PBM protocol carries 25 %, and Schiffer 2009 and Wang 2023 were converted from CW to 10 Hz / 25 % because of the ceilings. Each protocol's mode and duty returns to its source's. Schiffer (250 mW/cm²) and Wang (310 mW/cm²) as run are CW above the 200 average ceiling, so they stay refused unless the principal raises that ceiling. The hardware cannot fix that. **Rev 26:** the principal has (D-10). Both are inside the average term: the lowest limit either could meet is ≈ 329 at the 808 nm channel. So Schiffer is now refused only by the duty cap and U4, and so would Wang be if its mode is confirmed as CW | **Owner:** FW + App + Protocol authoring; Safety for the sequencing. **Blocking:** delivering any protocol at a duty other than 25 %, and any CW protocol |
| ~~**OI-HEXTILE-32**~~ | **✅ CLOSED 2026-09-29 (Rev 26): adopted in full (principal), D-10.** R-4's average term is the weighted sum alone, with no per-channel 200. The four inputs below were open when it was decided, and D-10 carries each one as a residual. The 400 peak now binds at 1064 nm and may not be retired while `OI-BIBPBM-03` is open. The original text follows. **Decision for the principal: adopt the laser skin reference in full as R-4's average term?** (raised Rev 25). R-4 now holds each channel's time average to 200 mW/cm², which is the reference's value at 660 nm, and adds the weighted sum Σ Ēᵢ / (200 × C_A(λᵢ)) ≤ 1 (`NP-BIB-PBMIRR-001` §3.1). Adopting the reference in full would let a single channel average up to 200 × C_A: ≈ 329 mW/cm² at 808 nm, ≈ 364 at 830 and 1000 at 1064. That is a relaxation for single-wavelength NIR protocols, and it is what lets Schiffer 2009 (810 nm, 250 CW) and Wang 2023 (820 nm) run as their trials did. **Inputs before deciding:** the C_A values checked against the purchased standards (`OI-BIBPBM-01`); counsel on RISK-03 (`NP-REG-PBM1064-001` §6A); the 1064 nm scalp-heating model (+3.76 °C at 1000 mW/cm²) against the 43 °C / 42 °C limit (`OI-BIBPBM-03`); and the red-weighted skin-type risk (`OI-BIBPBM-04`). **Also record:** Rev 25 already lets a single-channel pulsed protocol average 200 where the old rule allowed 100. At 660 nm that is exactly the reference, with zero margin | **Owner:** principal, with Safety + Regulatory. — (closed; the C_A constants still gate `OI-HEXTILE-31`) |

---

## 12. Design Review Checklist

| Item | Description | Status |
|---|---|---|
| HT-DRC-01 | 91-site lattice fits active field with ≥1.0 mm boundary clearance | ✓ (1.21 mm at 3.80 mm pitch, §4.1) |
| HT-DRC-02 | T1-A per-channel irradiance reaches the 400 mW/cm² pulsed ceiling at ≤180 mA | ✓ by construction (403 mW/cm² at 150 mA) — **conditional on OI-HEXTILE-02** |
| HT-DRC-03 | T1-C three-channel aggregate ≤600 mW/cm² | ✓ (566 mW/cm², 5.7 % margin, §4.3.2) |
| HT-DRC-04 | Emitter drive current inside the 120–180 mA L70 window in all modes | ✓ (150 mA peak; ~75 mA CW) |
| HT-DRC-05 | 1064 nm session reaches 36 J/cm² in an acceptable session length | Open — 21 min minimum, OI-HEXTILE-03 |
| HT-DRC-06 | Intra-tile irradiance uniformity quantified | Open — OI-HEXTILE-04 |
| HT-DRC-07 | Rigidizer fits the tile outline | ✓ (13.0 mm half-diagonal vs 20.0 mm inradius, §6.3) |
| HT-DRC-08 | Peak contact current ≤50 % of pogo rating, **nominal and on loss of any one `VLED` contact** | ✓ (**0.35 A nominal / 0.52 A degraded** vs ≥1.0 A — ~3× and ~2×, §8.1). Degraded case verified on real contacts by `NP-DRV-SHELL-002` **SH2-DRC-10a** |
| HT-DRC-09 | Socket pinout covers every tile type including T1-B and future types | ✓ by union construction (§1, §7.2) at **19 positions** — re-verify on any new type, **and on any change to which networks cross the socket** (the Rev 2 → Rev 3 count change came from exactly that, not from a new tile type) |
| HT-DRC-22 | **19-contact pad array is two staggered rows** and fits inside the 20.0 mm tile inradius, with mis-key asymmetry and `SEAT#` at the extreme of the last-seating row | Open — CAD; `NP-DRV-SHELL-002` REQ-SKT-01 / SH2-DRC-05a |
| HT-DRC-23 | Socket pinout matches `NP-DRV-SHELL-002` §5.1.4 pin for pin **and signal name for signal name** | ✓ at Rev 3 (§7.2, §7.4) — **run as a mechanical diff, not a review**: both name collisions closed by OI-HEXTILE-16 read as agreement to a human reader. Re-verify on either document's next revision |
| HT-DRC-10 | Contact mating sequence prevents powered-floating-return and false-seated states | ✓ (§7.3) |
| HT-DRC-11 | I2C address collision structurally impossible across ~80 modules | ✓ (UID-derived assignment, D-7) — needs firmware confirmation, OI-HEXTILE-07. **Rev 24: open on `OI-HUB-C15`.** One tile per segment (Mode S, `NP-FW-HEXTILE-001` §9) makes it impossible by topology. Shared segments (Mode D) rest on §9's items 1–5, which wait on `OI-FWTILE-07` |
| HT-DRC-12 | 3.3 V logic budget ≤1 W across 80 modules | Open — depends on standby firmware, OI-HEXTILE-07. **Rev 24: firmware obligations specified (`NP-FW-HEXTILE-001` §10, `REQ-FWTILE-08`). Closes on a bench measurement of a populated module in each state** |
| HT-DRC-13 | Safety MCU retains physical ownership of emitter enable (R-11) | ✓ **enable architecture RESOLVED 2026-08-16** — one Class C broadcast bit in series with 18 Class B per-cluster gates (§8.4.1); R-11 preserved because the Class C bit is in series and no Class B fault can re-energise a cut lattice. **MCU package still unselected but no longer blocked** — `NP-HW-HUB-001` OI-HUB-C20. Also verify **HUB-REQ-C05** (Class B gate commanded from above the cluster controller). Needs Hub PCB Rev 4, OI-HEXTILE-10 |
| HT-DRC-17 | Cluster count derived from the lattice under CLUSTER-1 + SYM-1, not carried from another lattice generation | ✓ (18, exhaustively verified, §8.2.1) — re-derive on any REG-1 lattice re-cut |
| HT-DRC-21 | No cluster contains a pendant petal; every cluster's petals form a contiguous arc (CONTIG-1) | ✓ (0 pendants, 0 broken arcs, §8.2.1) — **re-verify on any re-clustering**, this is not implied by cluster count |
| HT-DRC-18 | I2C segment count within one-tier mux capacity | ✓ (18 of 32, 56 %, §8.2) |
| HT-DRC-19 | Safety-MCU free-GPIO count covers the cranial enable plus all existing modality enables | **Open on package selection only — OI-HEXTILE-13 CLOSED 2026-08-16.** Demand is **~23 I/O** under the adopted §8.4.1 split (was ~40 at 18 enables). Verify against the package chosen under `NP-HW-HUB-001` **OI-HUB-C20**, after **OI-FMEA-06** re-derives `NP_NTC_CHANNEL_COUNT` |
| HT-DRC-20 | Peer documents (NP-DRV-SHELL-002, NP-HW-HUB-001, NP-HEX-ZM-001) agree on the cluster count | **✓ 2026-08-18 — all three now read 18** (`NP-DRV-SHELL-002` Rev 2 §7.1/§3.4; `NP-HW-HUB-001` Rev 6 §7.4/§6.3/§5.2/§8.2; `NP-HEX-ZM-001` §5.4a SYM-1 block). Was 12 / 10 / 12. **OI-HEXTILE-14 closed.** Re-verify on any REG-1 re-cut — the agreement is on the *derivation* (clamp partition, not `ceil(n/8)`), and 20 connector positions bound the re-cut it can absorb |
| HT-DRC-14 | Concurrent-tile power ceiling enforced before a protocol can be signed | Open — OI-HEXTILE-09 **(safety-adjacent)** |
| HT-DRC-15 | Per-tile J/cm² dose metering claim preserved | ✓ as specified (PD1/PD2 per tile) — **at risk from OI-HEXTILE-06 option 3** |
| HT-DRC-16 | PDMS bond + 200-cycle thermal cycling qualification carried forward | Inherited — remains BLOCKING, OI-HEXTILE-12 |

---

## 13. Cross-references

- **Parent:** NP-HEX-ZM-001 (`docs/np_hex_zm_001.md`) — §3 geometry, §4/§4b addressing and wire format, §4a taxonomy + SMART-1, §5.4a cluster clamps, §7 gates
- **Predecessor (SUPERSEDED, reused in part):** NP-HW-FPC-001 Rev 5 (`docs/superseded/np_hw_fpc_001.md`) — §5.1 InGaAs PD selection, §5.3 TIA-saturation methodology, §6.2 driver topology, §7 PDMS bonding all carried forward; §2/§3 connector and pinout, §3.2 ZONE_ID ladder, §4 LED counts all retired
- **Must co-revise:** NP-HW-HUB-001 (`docs/np_hw_hub_001.md`) — Rev 3 per OI-HEXTILE-10; §6.3 DG2788A count and §7.2 enable architecture per OI-HEXTILE-13/14
- **Must co-revise:** NP-DRV-SHELL-002 (`docs/np_drv_shell_002.md`) — §3.4 branch tree, §6 `SAFE_EN[n]`, §7.1 cluster-tail connector count (12/16 provisioned vs 18) per OI-HEXTILE-13/14
- **Cluster partition diagram:** `docs/diagrams/np_hextile_cluster_map.svg` — the 18-cluster midline-symmetric partition of the 80-socket lattice, socket ids and cluster boundaries (§8.2.1)
- **Lattice source of truth:** `scripts/sync-socket-map.ts` (`ROW_WIDTHS`) → `hardware/np_socket_map.json`, `app/web/src/lib/socketMap.generated.ts`
- **Firmware:** **NP-FW-HEXTILE-001 (`docs/np_fw_hextile_001.md`) — the on-module U1 firmware, SW-04 (Rev 24)**; NP-FW-PBM1064-001 Rev 2 (register map, §6.6 factory calibration); `firmware/hub_control/np_module_map.{h,c}` (UID inventory, `check_placement`)
- **Optics:** NP-OPT-PSF-001 (`docs/np_opt_psf_001.md`) — ~26 mm resolution floor at cortical depth; the basis for §4.2's acceptance of sub-millimetre wavelength interleave irregularity
- **Power:** NP-PWR-BUDGET-001 Rev 2 (`docs/np_pwr_budget_001.md`) — §3.4 the efficacy floor this document's R-4/R-5 ceilings sit above, §3.5 the full-population bound (~2.0 kW) and the *populated ≠ driven* distinction, §3.6 the ~30 W whole-vault mode behind §9.3 consequence 4, §3.7 why an emitter count is not a capability claim. **The analysis lives there; only the routing lives here**
- **Evidence:** `docs/pbm_neuro_protocols.md` — MASTER SUMMARY (0.02–0.3 W/cm², 10–120 J/cm², 6–30 min) and dosimetry lesson 1; the basis for OI-HEXTILE-21
- **Protocol library audit:** NP-SES-PWR-001 (`docs/np_ses_pwr_001.md`) — §9's ceiling measured against every authored protocol; §2.1 corrects the "~6 tiles" figure to a 2–32 range, §3 the zone-granularity defect, §4 what cascading can and cannot rescue. Script: `scripts/check-pbm-power.ts`
- **Competitive:** `docs/reference/competitive-position.md` — the comparative form of the irradiance and dose-metering arguments at §6.4 and §9.3
- **Thermal:** NP-THERM-CFD-R1-001 (Path B1, scalp-facing NTC), NP-THERM-BEZEL-001 (bezel **height** 1.0 mm; the lateral width is OI-HEXTILE-01)
- **Tooling:** the universal hex-tile mould gains a standard rigidizer cavity (§6.3); NP-TOOL-ZM-SM-001 (SUPERSEDED) needs no successor
