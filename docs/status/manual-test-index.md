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

MT-05 and MT-06 are firmware timing checks, not app tests. They are listed because they have no automated
form and a person has to run them. Their full procedures are in the owner docs and are **not** repeated here.

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

## 4. Coverage and gaps

- **Not found:** any manual test in `app/android/ISA.md` (it has no `manual` rows), and any manual
  procedure for the Windows or web apps.
- **Not read:** the other FAI bench procedures (`NP-FAI-001`, `NP-EMC-EMIRR-001`, `EMF-1` in
  `NP-HEX-ZM-001`, `NP-FW-BENCH-001`, `FAI-HD01/03/04`, `FAI-SM-*`). They are hardware tests. Adding them
  here is a decision for the owner: this index is software-first.
- **Not read:** `simulator/HOWTO.md` is a user manual and has no test script that I found.
- **Not assigned:** nobody owns MT-01 or MT-03. MT-02's source names the engineering lead. MT-04's owners
  are unassigned.
- **Not decided:** where results are filed, and whether this index becomes a controlled document (that
  needs a serial and a row in `NP-DHF-001`).

## 5. Keeping this current

A new manual test adds a row to section 1 and cites a common-step list rather than restating it. A test
that becomes automated is tagged `@verifies` (CLAUDE.md §21) and its row is struck here, not deleted.
