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
- Layer 2: 0.2mm mu-metal liner (15–25dB ELF magnetic) — PETG laminate encapsulation, silicone RTV sealant at all cutout edges
- Layer 3: **Palladium-coated polyester** inner liner (replaces silver — tarnish-immune for device lifetime, 40–60dB RF) — permanent shielding claim, verified by fleet SHDR attenuation monitoring
- Layer 4: Carbon-loaded EMI absorber foam (cavity resonance suppression)
- Layer 5: USB-C + accessory port filters (30–50dB)
- **Active:** 3-axis fluxgate magnetometers + Helmholtz coil pairs · Combined: 35–45dB ELF magnetic, 40–60dB RF
- Shell bonded to EEG DRL output (active EEG shield)
- Non-conductive CFRP window at TMS coil site (prevents eddy current field loss)
- Three firmware additions: TMS-gated cancellation · adaptive notch at BES/tACS stimulus frequency · synchronous Helmholtz subtraction from EEG

> **Every dB figure above is a design target. None has been measured** — `EMF-1` (two-layer attenuation
> ≥ single-shell baseline) has never run, and `EMF-3`, `RISK-20` and `OI-THCOOL-06` are open on the
> seams and penetrations that set the real floor. **What the stack is for, and which layers earn their
> cost, is `docs/np_bib_emf_001.md` (NP-BIB-EMF-001).** Three findings that bear directly on this list:
>
> - **Layer 2's rationale as written here is the one benefit the evidence does not support.** Ambient
>   ELF at WHO residential levels (0.07–0.11 µT) is 2,000–18,000× below where any neural effect is
>   reported, and EEG — unlike MEG — does not require external magnetic-field suppression. The layer
>   stays because `NP-HEX-ZM-001` §5.3.1 co-designs the Helmholtz coils with it as **one magnetic
>   circuit**, because `NP-ENV-OPRANGE-001` §2 makes it the degraded-mode fallback, and possibly
>   because of IEC 61000-4-8 immunity — **none of which is stated here.**
> - **Layer 4 is the only layer with no dB figure in any document**, including `NP-DT-001`
>   `DI-PERF-22`. It is explicitly not primary shielding, and no document names the source that excites
>   the resonance it suppresses — the radios live in the hub. It costs **18 % of the outward thermal
>   path** and blocks two fixes to a BLOCKING `OI-SINK-01`. **`OI-BIBEMF-08`: EMC states its
>   requirement in dB against a named source and band, or the layer goes.**
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
