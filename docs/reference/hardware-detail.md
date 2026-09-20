# Hardware detail — shielding stack, fit system, power table, status indicators

> Relocated from CLAUDE.md §4.3, §4.4, §4.5 and §4.7 (Rev 43) to slim the always-loaded core.
> Content is verbatim; section numbers are unchanged, so an inbound `CLAUDE.md §4.5` citation lands
> on the same material. **CLAUDE.md §4 keeps §4.1 (processor stack) and §4.2 (safety architecture)
> in full** — §4.2 is one of the three live constraints and is cited more than any other hardware
> section — plus §4.6 (the four operating modes) and the invariant of each section below.
>
> **Read this file when:** specifying or reviewing the shielding stack, sizing or fitting the
> headset, budgeting power or selecting a PD source, or working on the status-LED behaviour.
>
> Related: thermal work is `docs/np_therm_*`; the power budget derivation is
> `docs/np_pwr_budget_001.md` and the source analysis `docs/np_pwrsrc_001.md`; charger *policy*
> (which charger ships with which configuration) is `docs/reference/commercial-model.md` §2.2.

### 4.3 EMF shielding (5-layer passive + active)
- Layer 1: CFRP outer (30–50dB RF)
- Layer 2: 0.2mm mu-metal liner (15–25dB ELF magnetic) — PETG laminate encapsulation, silicone RTV sealant at all cutout edges.
  **What actually holds this layer in place is NOT the ELF-attenuation figure above** (`NP-BIB-EMF-001` §4 — ambient ELF at WHO residential levels is 2,000–18,000× below any reported effect, and EEG, unlike MEG, does not require external magnetic suppression). **Four dependencies do, and they are load-bearing in different ways:**
  | # | Dependency | Why it binds | Owner |
  |---|---|---|---|
  | **D1** | **The Helmholtz coils are co-designed with the mu-metal as ONE magnetic circuit** — *"the high-permeability mu-metal (outer bowl) shunts and reshapes any nearby coil's flux… co-locating passive shield + active trim on the outer bowl keeps them a single characterized subsystem"* | **Decisive.** The calibrated quantity in the active loop is the **coil-drive → field transfer function, defined WITH the mu-metal present.** Removing L2 does not subtract 15–25 dB from a total — it **invalidates the actuator characterisation on all three axes**, and with it `REQ-EMI-05`'s feed-forward subtraction and `REQ-EMI-11`'s per-configuration recalibration. The active half would need **redesign, not re-measurement** | `NP-HEX-ZM-001` §5.3.1 reason 2 |
  | **D2** | **L2 is the degraded-mode fallback** | `NP-ENV-OPRANGE-001` §2 makes the active-cancellation envelope **SOFT** *because* *"fluxgate temp drift degrades cancellation; passive 5-layer shield always present."* Remove L2 and **a soft bound becomes a hard one**, re-opening `NP-ENV-001`'s operating range | `NP-ENV-OPRANGE-001` §2 |
  | **D3** | **L2 carries the shell's magnetic-continuity architecture** | Magnetic shields leak at butt-joints, which is why all shielding is consolidated onto **one unbroken outer bowl**. `NP-HELMET-GEOM-001` §89 routes L2 around the TMS window; `NP-THERM-COOL-001` §6.2 and `OI-THCOOL-06` (**CLOSED 2026-08-30**; reopening recommended by `OI-EMCCAV-10`) exist to protect its reluctance at the posterior boss | `NP-HEX-ZM-001` §5.2, §5.3(d) |
  | **D4** | **IEC 61000-4-8 may REQUIRE it** — unresolved | IEC 60601-1-2 incorporates power-frequency magnetic immunity (typically 30 A/m ≈ 37.7 µT, far above the 0.1 µT ambient of §4). That is **device immunity, not user protection**, and with fluxgates and a µV front end inside the envelope it is plausibly load-bearing. **`regulatory-strategy.md` §8 omits IEC 60601-1-2 entirely** | **`OI-BIBEMF-04` — gates any L2 removal** |
  **And the claim arithmetic moves:** L2 supplies the 15–25 dB half of the combined 35–45 dB ELF figure. Removing it leaves the ELF claim resting on an active loop `OI-PWRTH-05` already records as **off for most of a TMS train**, and CLAUDE.md §1 and §4.3 would both need re-derivation.
  > **The defect this table closes (`NP-BIB-EMF-001` §7.2, 2026-09-15; written in 2026-09-20).** For four revisions this line justified Layer 2 by the **one benefit the evidence does not support** and omitted all four dependencies that actually hold it — *"which is a documentation defect, and it is why the layer looked removable."* **D1–D4 are the reasons. The dB figure is not one of them.** Layer 2 is **not** in the same position as Layer 4 (`NP-EMC-CAV-001`): L4's candidate dependencies turned out to be an unbuilt mould, an unpublished sentence and an inferred function, while L2's are four real ones, of which D1 alone is decisive.
- Layer 3: **Palladium-coated polyester** inner liner (replaces silver — tarnish-immune for device lifetime, 40–60dB RF) — permanent shielding claim, verified by fleet SHDR attenuation monitoring
- Layer 4: Carbon-loaded EMI absorber foam — **requirement stated 2026-09-20: `REQ-CAV-02`, loaded Q ≤ 20 over 420 MHz – 3 GHz, i.e. ≥ 26.2 dB of peak-field reduction at resonance** (`NP-EMC-CAV-001` §5). **This layer supplies 0.26 dB of it**; the wearer's head supplies 49.8 dB. **`REQ-CAV-04`: delete the station, with the 3 mm re-loft of the outer bowl BINDING** — vacating it without re-lofting fills it with stagnant air (54 % worse per mm) and the outward path goes 0.410 → 0.450; with the re-loft, 0.335. **Recommended, not executed** — `NP-EMC-CAV-001` §8.2, `OI-EMCCAV-08`
- Layer 5: USB-C + accessory port filters (30–50dB)
- **Active:** 3-axis fluxgate magnetometers + Helmholtz coil pairs · Combined: 35–45dB ELF magnetic, 40–60dB RF
- Shell bonded to EEG DRL output (active EEG shield)
- Non-conductive CFRP window at TMS coil site (prevents eddy current field loss)
- Three firmware additions: TMS-gated cancellation · adaptive notch at BES/tACS stimulus frequency · synchronous Helmholtz subtraction from EEG

> **Every dB figure above is a design target. None has been measured** — `EMF-1` (two-layer attenuation
> ≥ single-shell baseline) has never run, and `EMF-3` and `RISK-20` are open on the
> seams and penetrations that set the real floor. (**`OI-THCOOL-06` was CLOSED 2026-08-30** by its
> owning document — `NP-BIB-EMF-001` carried it as open in error; recommended for reopening against
> the posterior-boss collar by `NP-EMC-CAV-001` `OI-EMCCAV-10`.) **What the stack is for, and which layers earn their
> cost, is `docs/np_bib_emf_001.md` (NP-BIB-EMF-001).** Three findings that bear directly on this list:
>
> - **Layer 2's dB figure is not why Layer 2 is here.** Ambient ELF at WHO residential levels
>   (0.07–0.11 µT) is 2,000–18,000× below where any neural effect is reported, and EEG — unlike MEG —
>   does not require external magnetic-field suppression. **The four dependencies that actually hold
>   the layer are now written into its entry above as D1–D4** (2026-09-20), closing the documentation
>   defect §7.2 raised. **D1 is decisive**: the Helmholtz coils and the mu-metal are one magnetic
>   circuit, so removing L2 invalidates the actuator characterisation rather than subtracting dB.
> - **Layer 4 now has a dB figure, and it does not meet it.** `OI-BIBEMF-08` is **discharged** by
>   `NP-EMC-CAV-001` (2026-09-20), which names the source `NP-BIB-EMF-001` §7.3 could not find — not
>   radios, but the **18 STM32G071 cluster controllers `NP-DRV-SHELL-002` §3.2 puts inside the
>   envelope** — derives the band (**420 MHz – 3 GHz**, whose lower edge moves 84 MHz with head
>   circumference), and states the requirement as **`REQ-CAV-02`: Q_L ≤ 20, i.e. ≥ 26.2 dB**. Layer 4
>   supplies **0.26 dB** of that: a 3 mm non-magnetic absorber against a conductor is λ/217 at the
>   lowest mode, and in the thin limit its surface impedance is purely reactive *independent of the
>   loading*. **The wearer's head supplies 49.8 dB**, in every state where the sources are energised.
>   So the layer costs 18 % of the outward thermal path and two fixes to a BLOCKING `OI-SINK-01` for
>   1 % of its own stated job. **`REQ-CAV-04`: delete the station, and the 3 mm re-loft of the outer
>   bowl is BINDING** — vacating 3 mm fills it with stagnant air at **0.115 m²K/W against the foam's
>   0.075, 54 % worse per mm**, so without the re-loft the outward path goes 0.410 → **0.450**; with
>   it, **0.335**, the best figure available. **No tooling is cut and nothing is published**, so this
>   is the cheapest the change will ever be. One question remains and it is mechanical, not EMC
>   (`OI-EMCCAV-08` / `MECH-2`): if the over-center cluster clamps and spring plungers cannot take up
>   the tolerance stack once 3 mm of incidental compliance leaves the gap, the station returns as a
>   **thin ceramic-filled pad sized by that stack** (~0.355). `NP-EMC-CAV-001` §8.2 (Rev 3).
> - **The stack is aperture-limited, not layer-limited.** Adding layers above the seam floor buys
>   nothing, which is why `EMF-1` is the measurement that decides every layer's value.

### 4.4 Fit system
- Boa-style occipital dial · 10cm range · 0.5mm/click · 50,000-cycle rated · enclosed PTFE-lined cable channel (prevents hair entanglement) · Boa replacement cable + tool in box · regrease kit available ($4.99 accessory)
- 5-position forehead bridge (5mm steps)
- Spring-decoupled electrode pods (80–120g, ±12mm, Shore 30A silicone)
- Temporal stability wings (snap-on, stored in hub dock)
- 1 adult SKU covers 52–62cm heads

### 4.5 Power

> **⚠ The table below is ELECTRICAL draw only, and two things it does not carry now have numbers.**
> Nothing here is edited — §4.5 is a locked section and the edit is routed as `OI-PWRTH-06` — but a
> figure quoted from it without these is misleading:
>
> - **There is no thermal-bound column, and the thermal bound binds first for PBM.** Against
>   `NP-THERM-SINK-001` `SPEC-SINK-01`, the sealed cavity admits **31.4 W of emitter power across the
>   whole lattice, 8.9 W at an authored N = 6 montage and 17.9 W at +35 °C ambient**
>   (`NP-PWR-THERM-001` §3). **The T1-peak row is an electrical peak the assembly cannot spend** —
>   38.4 W of device draw fully distributed, 15.9 W at N = 6 — so a 45 W supply already covers it and
>   the 65 W rung buys nothing thermally at T1.
> - **There is no TMS row, and the T2 figures were derived for the 1170 nm laser zone only.**
>   `SPEC-TMS-04` puts **T2 + TMS at ~176 W on a 48 V/5 A (240 W EPR) contract**, 2.4× the T2-peak
>   figure five other documents cite as an input — **conditional on `OI-PWR-02`**, which is BLOCKING
>   and whose other answer is ~2,886 W (`NP-PWR-THERM-001` §7, §9). TMS and 1170 nm are `max()`, not
>   `sum()`: no authored session runs both.
>
> Proposed replacement table with both columns: `docs/np_pwr_therm_001.md` §9. Derivations:
> `docs/np_pwrsrc_001.md` §13, `docs/np_pwr_budget_001.md` §6.

| Mode | Draw | Min USB-C PD | Power bank runtime (10,000mAh) |
|------|------|-------------|-------------------------------|
| Standby | 1W | 5V/0.5A | ~330 hours |
| EEG only | 2.5W | 5V/1A | ~130 hours |
| Standard T1 ★ | ~17–20W | 15V/2A (45W) | ~95–110 min |
| T1 peak | ~45–50W | 20V/3A (65W) | ~38–42 min |
| T2 standard | ~44–46W | 20V/3A (65W) | ~41–43 min |
| T2 peak | ~70–74W | 20V/5A (100W EPR) | ~24–27 min |

- 22F supercapacitor in control hub (absorbs LED duty-cycle transients, allows 50% aging over 5 years)
- Hub NTC thermistor for supercapacitor aging estimation (logged in SHDR)

### 4.7 Status indicators
- Left temple: green power LED (breathes at idle)
- Right temple: amber in-use LED (pulse rate mirrors session frequency — caregiver can confirm correct protocol across room)
- Fault: power LED red blink
- Stealth mode: app-controlled suppress (safety faults always fire)
