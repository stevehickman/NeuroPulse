# Manual test index

**Status:** DRAFT. Not a controlled document: no `**Document:**` serial, not in `NP-DHF-001`. It indexes
procedures that live elsewhere and decides nothing. The owning file's procedure wins over anything here.
**Compiled:** 2026-10-29 from a search of `docs/`, `app/*/ISA.md` and `simulator/`. It is **not proven
complete**; see "Coverage and gaps".

A *manual* test is one a person must run and judge because no CI job can. Automated tests are tagged
`@verifies` and tracked by `bun scripts/req-trace.ts` (CLAUDE.md §21); nothing below is, so this index is
the only thing that keeps these from being forgotten.

## 1. Index

| ID | Test | Needs | Runnable now? | Owner doc | Blocks |
|----|------|-------|---------------|-----------|--------|
| **MT-01** | Age gate is the first screen on a clean install | iOS simulator | Yes | `app/ios/ISA.md` ISC-83 | none recorded |
| **MT-02** | Crash reporter sends no health data | Device or simulator, debug build, vendor dashboard | Only once a crash reporter is integrated (not checked) | `docs/np_app_telemetry_001.md` §6; `OI-TEL-01`; `NP-PRIV-AUDIT-001` AUDIT-08 | First TestFlight / Play Store internal release |
| **MT-03** | Plaintext-HTTP exception removed from the iOS build | Release build config | Yes, at release time | `docs/np_analytics_001.md` ("Remove this exception before any TestFlight or App Store build") | TestFlight / App Store build |
| **MT-04** | Safety-MCU update leaves NV pages 62–63 intact | Safety MCU, **the real update tool** | **No.** No update path exists | `docs/human-input/oi-risk2-07-update-path-test.md` (draft); `NP-RISK-002` §4.3.3 hazard 25-b; `OI-RISK2-07` | The first safety-MCU update path |
| **MT-05** | Cardiac interlock detection and cutoff latency (`FAI-CV02`) | T2 hub, cVNS accessory, R-peak generator, oscilloscope | **No.** Hardware | `docs/np_fw_cvns_001.md` §9; `OI-FMEA-02` | Hardware bench |
| **MT-06** | Watchdog cutoff latency to all enable nets | Safety-MCU hardware, oscilloscope | **No.** Hardware | `docs/np_fmea_001.md` `OI-FMEA-01` | Hardware bench |

MT-05 to MT-24 are hardware or bench tests. None can run today: the project is pre-tooling and no hardware
exists (`CLAUDE.md` header; `NP-SW-CI-001`). Their procedures, criteria and figures stay in the owner docs
and are **not** repeated here. Where a row says "gated", the owner doc names the open item that must close first.

| ID | Test | Needs | Owner doc | State per owner doc |
|----|------|-------|-----------|---------------------|
| **MT-07** | Cervical electrode placement (`FAI-CV01`) | Neck phantom, cVNS cable, hub | `NP-FW-CVNS-001` §9 | PENDING, hardware |
| **MT-08** | Cervical VNS tolerability in 3 healthy adults (`FAI-CV03`) | Volunteers, clinician, ECG, **IRB approval** | `NP-FW-CVNS-001` §9 | PENDING. Blocks T2 clinical release, not the G3-08 software gate |
| **MT-09** | HD-tDCS plumbing, focality, SNR constants (`FAI-HD01`, `HD03`, `HD04`) | T2 prototype, phantom | `NP-FW-HD-001` §12–13 | Software passes; bench BLOCKING for G3-07 |
| **MT-10** | 1064 nm module: 3-channel irradiance, dose metering, fouling, aging (`FAI-SM-04`, `SM-06`, `SM-07`, `SM-08`) | Calibrated 1064 nm optical bench | `NP-FW-PBM1064-001` (FAI table) | Hardware bench |
| **MT-11** | 1170 nm dose metering and TEC interlock (`FAI-T2-05`, `T2-06`) | Calibrated 1170 nm bench, T2 prototype | `NP-SES-1064-001` (FAI table) | Hardware bench, PENDING |
| **MT-12** | ADS1299 EMI rejection ratio, 420 MHz–3 GHz | Injection board, parts in hand, RF set-up | `NP-EMC-EMIRR-001` | DRAFT, never run, no data. Bounds `REQ-CAV-01`, `OI-EMCCAV-01` |
| **MT-13** | Shielding and cavity sweeps (`EMF-1`, `EMF-1a`–`1f`, `EMF-1-SINK-1`, `-2`) | `EMF-1` fixture, VNA, head phantoms 52/57/62 cm, fluxgate/Helmholtz bench | `NP-HEX-ZM-001` §5.6.1; `NP-EMC-CAV-001` §7 | Fixture does not exist; **`EMF-1` has never run**; the plan is not a procedure |
| **MT-14** | PDMS–polyimide thermal-cycling qualification (`FAI-TC01`–`TC06`) | Bonded coupons, thermal chamber, peel tester, spectrophotometer | `NP-FAI-001` §4 | `FAI-TC02` is BLOCKING and QUAL |
| **MT-15** | Ingress: IPX4 as-built, post-service, gasket ageing (`FAI-IPX-01`–`IPX-05`) | IEC 60529 spray rig, 40 °C/75 % RH chamber (90 days for `IPX-04`) | `NP-FAI-001` §5 | `FAI-IPX-02` BLOCKING; `IPX-05` gated on `NP-TOOL-HEXTILE-001` `OI-THEX-03` |
| **MT-16** | Hub enclosure first article (`FAI-HUB-01`–`26`) | Physical hub parts, gauges, force gauge, IEC 60601-1 test finger | `NP-FAI-HUB-001` | Several rows gated (`OI-HTOOL-02`, `-03`, `-08`; `OI-NASAL-09`); `HUB-23` and `HUB-24` BLOCKING |
| **MT-17** | PDMS–PI bond line visual and peel (`FAI-M01`–`M03`) | 10× magnifier, peel tester | `NP-FAI-001` §4 | Not checked |
| **MT-18** | Impedance front-end calibration: do the ohms `np_hal_impedance_read_ohm()` returns match a known load? (`OI-SWCI-34`) | Safety-MCU board with the real analog front end, reference resistors | `NP-SW-CI-001` `OI-SWCI-34`; `np_safety_config.h` (`NP_IMP_SENSE_R_OHM`) | Front end undesigned; values are an uncalibrated placeholder. The fail-safe direction holds regardless |
| **MT-19** | FlexRAM partition took effect on silicon (`OI-SWCI-39`, gating `OI-SWCI-42`'s `.bss` move and `§4.13.3`'s weight-matrix placement) | i.MX RT1062 board running the bootloader | `NP-SW-CI-001` §4.10, §4.10.6, §4.12.3, §4.13.3 | Three register writes never run on silicon. `ucHeap` does not depend on them |
| **MT-20** | 3.3 V logic budget per populated tile in each state (`HT-DRC-12`, `REQ-FWTILE-08`) | Populated hextile module, 25–62 °C chamber, current measurement | `NP-HW-HEXTILE-001` (`HT-DRC-12`); `NP-FW-HEXTILE-001` §10 | Open; depends on standby firmware (`OI-HEXTILE-07`) |
| **MT-21** | U3 holds `I_cap` and U4 trips while U1 commands 100 % duty and full `CUR` (`OI-HEXTILE-24(g)`) | Hextile with U3/U4 fitted, the selected emitter | `NP-HW-HEXTILE-001` `OI-HEXTILE-24` | Gated: no emitter selected (`OI-HEXTILE-02`), U3/U4 not chosen. Until it closes, the thermal cut is the only non-firmware bound |
| **MT-22** | Shell (`NP-DRV-SHELL-002` §11) bench design-review items: `SH2-DRC-08`, `10`, `10a`, `10b`, `12`, `14`, `16`, `17`, `24`, `26`, `27`, `28` | Per row: socket force rig, current probes, oscilloscope, magnetometer, PD readback | `NP-DRV-SHELL-002` §11; the record is `NP-REV-SHELL-001` | None signed. `SH2-DRC-17` is BLOCKED on `OI-SHELL2-07` (budget unset) |
| **MT-23** | Safety-MCU IWDG on silicon: worst-case main-loop iteration, and a deliberately stalled loop resets with every enable disabled (`OI-SWCI-49`) | Safety-MCU board, scope or logic analyser | `NP-SW-CI-001` `OI-SWCI-49`; feeds `NP-FMEA-001` `OI-FMEA-01` | Not run. The 1,000 ms timeout is provisional; no WCET has been measured. Run with MT-06 |
| **MT-24** | On-skin 1 kHz impedance distribution for the tES electrode set, to set a minimum-impedance floor (`OI-FMEA-14`) | Calibrated impedance front end (MT-18), human volunteers | `NP-FMEA-001` `OI-FMEA-14` | Blocked on MT-18. Current floors (500 Ω VNS-HRV / cervical VNS, 1 Ω tDCS and BES/tACS) are interim and change only on evidence |

## 2. Common step lists

Steps that recur. A test cites a list by ID and names only what differs.

**CS-A. Prepare a debug build on a clean state**
1. Build the app in the debug configuration from the commit under test; record the commit hash.
2. Erase the app from the simulator or device so no earlier state, keychain item or consent flag survives.
3. Install the build.

**CS-B. Launch and observe**
1. Launch the app from the home screen.
2. Do not interact until the first screen has fully rendered.
3. Record what is on screen (screenshot).

**CS-C. Inspect the vendor dashboard for a report**
1. Open the vendor's report view for the build under test.
2. Wait for the report to arrive; note the time. If none arrives, record that, and do not infer a pass.
3. Open the report in full, including any attachments or "breadcrumbs" tab.

**CS-D. Record the result**
1. Record: test ID, commit hash, build configuration, device or simulator name and OS version, date,
   tester, pass or fail.
2. Attach the screenshots the test asks for.
3. File the record where the owner doc says. If it names none, say so in the record; do not choose one.
4. A failure raises an open item. It is never waved through, and the test is not re-run until the cause is
   understood.

**CS-E. Hardware bench set-up** *(MT-04 to MT-06)*
1. Confirm the unit's firmware build, hardware revision and serial; record them.
2. Confirm instrument calibration dates are current; record instrument IDs.
3. Power from a USB-C PD source that matches the tier's charger policy (CLAUDE.md §2.2, §4.5).
4. No person is connected to any stimulation output. Use a dummy load.

**CS-F. Bench record for a pass criterion**
1. Copy the pass criterion from the owner doc **as it stands at the revision you are testing against**.
   Record that revision. Do not copy a figure from this index: several owner docs restate criteria across
   revisions (for example `OI-FMEA-02` and `FAI-CV02`).
2. Record each trial, not only the worst one, and record the instrument reading beside the criterion.
3. A measurement outside the criterion is a fail even if close.
4. Where the owner doc says "never quote as a measurement" until the test has run (`EMF-1`, `EMIRR`), the
   record is the first measurement. Say so in it.

## 3. Procedures

### MT-01. Age gate is first (`ISC-83`)
Pass: the age gate is the first screen on a clean install.
1. CS-A, on the iOS simulator.
2. CS-B.
3. Pass only if the first screen is the age gate. Any other screen first (onboarding, consent, home) is a fail.
4. CS-D.

Related automated check: `ISC-85` greps for `ageConfirmed = true` (0 matches expected). It does not
replace this test; it shows nothing sets the flag in code.

### MT-02. Crash reporter carries no health data (`OI-TEL-01`)
Owner: engineering lead. Source: `NP-APP-TELEMETRY-001` §6.
1. CS-A, on a device, debug build.
2. Start a debug session, so a session screen is showing.
3. Trigger a crash on purpose.
4. CS-C.
5. **Must contain:** stack trace, OS version, app version, device class.
6. **Must not contain:** any text from the session screen, any network request body, any variable value,
   any screenshot, any user identifier.
7. Take a screenshot of the vendor's crash report UI. The source says to record it in
   `NP-APP-ROADMAP-001` Rev 2.
8. CS-D.

The report is third-party data, but the app under test is a debug build with test data only. **Never run
this with a real person's UHDR on the device** (CLAUDE.md §5). Repeat on iOS and Android; the source does
not say whether one run covers both, so run both until the owner says otherwise.

### MT-03. Plaintext-HTTP exception removed
1. Open the iOS release build's `Info.plist` / App Transport Security settings.
2. Pass only if there is no `NSExceptionAllowsInsecureHTTPLoads` entry, and no LAN-IP exception domain.
3. CS-D.

Source: `docs/np_analytics_001.md`. This is a check on a file, so a grep could automate it; that is a
suggestion, not a requirement (CLAUDE.md §18).

### MT-04. Safety-MCU update preserves the cardiac cutoff (`OI-RISK2-07`)
**Not runnable.** The owner request is an unsent draft and itself says the sufficient test is a proposal
until Quality adopts it. Treat the steps as a sketch.
1. CS-E.
2. Record a cutoff for a test user in NV pages 62–63.
3. Run the update through the **real** update path, not a simulation of it.
4. Reboot. Confirm the user is still withheld from `NP_SAFETY_EN_CVNS` until the full re-enable sequence
   completes, and that unattributable and other users' entries survive.
5. Repeat with the update interrupted (power loss mid-write) and, if one exists, a rollback.
6. Confirm the failure direction: if the pages cannot be kept, the update **refuses**.
7. CS-D.

### MT-05. Cardiac interlock response time (`FAI-CV02`)
**Not runnable (hardware).** Follow `NP-FW-CVNS-001` §9 as amended (Rev 10). Do not copy its figures or its
pass criterion into a record from this index. Use CS-E and CS-D. Detection time and cutoff latency are
recorded separately, and a cutoff time is never quoted without its detection time (CLAUDE.md §4.2).

### MT-06. Watchdog cutoff latency (`OI-FMEA-01`)
**Not runnable (hardware).** Follow `NP-FMEA-001` `OI-FMEA-01`. Use CS-E and CS-D. The item names two
nodes to be read, and the pass criterion is the item's.

### MT-07 to MT-24. Hardware and bench tests
For each: CS-E, then the owner doc's procedure, then CS-F, then CS-D. Notes the owner docs make that change
how you run them:

- **MT-07 / `FAI-CV01`:** silicone neck phantom, 0.25 S/m. The four pass criteria (`CV01-A` to `D`) are in the owner doc.
- **MT-08 / `FAI-CV03`:** a clinical study. It needs IRB approval, an attending clinician and emergency
  equipment, and screening exclusions. This is not a bench test and no one runs it without those. Do not
  start it from this index.
- **MT-09 / `FAI-HD01`, `HD03`, `HD04`:** the cap has no register row yet, so these have no nameable
  artifact checklist (`NP-HW-TACSDRV-001`, `OI-FAI-06`, `OI-ART-08`, `OI-TCAP-06`).
- **MT-10 / MT-11:** need a calibrated optical bench at the stated wavelength. 1064 nm and 1170 nm are T2
  or smart-module lines: do not run them on a unit presented as T1 (`REQ-UPG-01`).
- **MT-12 / MT-13:** `NP-EMC-EMIRR-001` and `EMF-1` have never run, so no dB figure from either may be
  published as measured (CLAUDE.md §4.3). Order for `EMF-1`: `1a` first, `1d` last, per the owner doc.
- **MT-18:** do not calibrate against a headset on a person. Use a known resistive load and record the
  front-end design revision. Whatever it shows, the refuse-the-enable direction on every error path is
  the safety property and stays as built.
- **MT-19:** run it before any change that moves `.bss` into DTCM, not after. If the partition did not
  take effect, the failure looks like arbitrary corruption, not an immediate fault (`§4.10.6`).
- **MT-20 / MT-21:** the 80-tile ~1 W figure holds only if standby is the default. `HT-DRC-12` closes on
  a bench measurement, not on the owner doc's table. MT-21 cannot be started until `OI-HEXTILE-02` and
  `OI-HEXTILE-24` close. A 660/808 nm emitter has not been chosen.
- **MT-23:** run with MT-06, because `OI-FMEA-01` asks for the same reset-to-disabled latency. The 1,000 ms
  IWDG timeout and the 1.5 s heartbeat timeout are different mechanisms; record which one each trace shows.
- **MT-24:** involves human volunteers, so it needs the same ethics route as MT-08 before anyone is recruited
  (the source names Clinical as an owner). Calibrate the front end first (MT-18); a distribution through an
  uncalibrated front end derives nothing. Record the distribution only. Do not set a floor from this index.
- **MT-22:** one row for twelve shell items. Log each `SH2-DRC-nn` separately in `NP-REV-SHELL-001`, which
  is the instrument that records them. The items' methods and pass criteria are in `NP-DRV-SHELL-002` §11.
  `SH2-DRC-14` is the same <50 ms cutoff class as MT-06 but at the cluster rails, so it is not a duplicate.
- **MT-14 to MT-17:** mechanical and materials first-article checks. `FAI-HUB-23` and `FAI-HUB-24` are
  rotating-fan and finger-probe safety checks. Run with the fan guard and a stop within reach.

## 4. Coverage and gaps

- **Not found:** any manual test in `app/android/ISA.md` (it has no `manual` rows), and any manual
  procedure for the Windows or web apps.
- **Hardware rows are an index only.** MT-05 to MT-24 were taken from the owner docs' test tables and
  headings, not from reading each procedure end to end. I read `FAI-CV01`, `CV02`, `CV03`, the `FAI-SM`,
  `FAI-T2` and `FAI-HD` tables, the `EMF-1` plan and the head of `NP-EMC-EMIRR-001`. I did not read the
  `NP-FAI-001` and `NP-FAI-HUB-001` rows in full.
- **Not included, deliberately:** `FAI-SM-01`/`02`/`03`/`05`/`09`/`10`/`11`, `FAI-T2-01`–`04` (marked
  Software PASS in their docs), and `FAI-ANON-01`–`09` (firmware tests whose execution mode I did not
  confirm). If any of these turns out to be run by hand, it belongs here.
- **Added at MT-18 to MT-22 (and MT-23, MT-24):** `OI-SWCI-34`, `OI-SWCI-39` (with `-42`), `HT-DRC-12`, `OI-HEXTILE-24(g)` and
  the shell bench items. `OI-SWCI-42` is closed, but its `.bss` move still waits on MT-19. I read the
  `NP-SW-CI-001` §4.10 and OI rows, the `HT-DRC-12` rows in both hextile docs, the `OI-HEXTILE-24` row and the
  `NP-DRV-SHELL-002` §11 table. I did not read `NP-REV-SHELL-001` past its status rows.
- **`OI-FMEA` triage (every row in `NP-FMEA-001`'s open-item table; `07` and `09` have no row there):** only `01` and `02` are bench tests (MT-06, MT-05),
  and `14` is a measurement (MT-24). The rest are not tests a person runs, so they have no row:

  | Item | Why no row |
  |------|-----------|
  | `03` | Closed (built, CRC-checked in code and host tests) |
  | `04` | Closed not applicable (no current-sense ADC exists) |
  | `05` | Spec to author in `NP-SW-001` |
  | `06` | Re-analysis against the hex-lattice architecture |
  | `08` | A decision on a ramp-down margin in the charge monitor |
  | `10` | An FMEA section to author for SW01-M09 |
  | `11` | Closed (built, `np_gpio_mgr_tests`) |
  | `12` | (a), (b), (d) closed by building; (c) is an ALARP justification pending `OI-CVNS-11` |
  | `13` | (i) a SYSCLK-vs-LSI control to build or re-score; (ii) closed. If (i) is built, its check on silicon belongs in a row |
- **Still not included:** `SH2-DRC` rows whose method is CAD, schematic, netlist or code review (not tests a
  person runs on a bench), and bench items I could only infer from a one-line mention. Each should get a row once
  its owner confirms it is a distinct test.
- **Same measurement, two places:** `FAI-CV02` is `OI-FMEA-02`'s test, and `FAI-HD01/03/04` and `FAI-SM-04`
  also appear in `NP-QMS-DC-001`'s design-verification list. This index lists each test once.
- **Not read:** `simulator/HOWTO.md` is a user manual and has no test script that I found.
- **Not assigned:** nobody owns MT-01 or MT-03. MT-02's source names the engineering lead. MT-04's owners
  are unassigned.
- **Not decided:** where results are filed, and whether this index becomes a controlled document (that
  needs a serial and a row in `NP-DHF-001`).

## 5. Keeping this current

A new manual test adds a row to section 1 and cites a common-step list rather than restating it. A test
that becomes automated is tagged `@verifies` (CLAUDE.md §21) and its row is struck here, not deleted.
