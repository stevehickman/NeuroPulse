# Quality and safety request: OI-SW03-01, the companion app's software safety class

**Status:** DRAFT, ready to send; not yet sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Software (`OI-SW03-01` in `docs/status/pending-decisions.md`; `NP-SW-001` §3.1)
**To:** Quality and Safety (names to be assigned; the item's owner column reads "Quality + Safety")
**Response needed:** a ruling on each of Q1 to Q4, with rationale, the reviewers' names and roles, and the date.
**Kind of decision:** an IEC 62304 §4.3 classification and an ISO 14971 argument. It is not a design choice the sender can make.

## 1. The question

`NP-SW-001` §3.1 classes **SW-03, the "iOS/Android application", as Class B**, because "SW-01 and SW-02 provide
independent stimulation safety". Is Class B still right for the companion app, and for the shared core it now
runs on?

Plain form: when the app's own checks are wrong or missing, is there always something else, outside the app,
that stops the harm?

## 2. What has changed since §3.1 was written

1. **The app is not two platform apps any more.** It is one Compose Multiplatform UI on Android, iOS, macOS,
   Windows and the web (`CLAUDE.md` §20), over a Rust core (`common/npps-core`, §22) that parses, validates,
   applies the zone model to, and compiles protocols, and refuses them before they are signed. That core is in
   no software item of `NP-SW-001` §2, whose table names only Swift and Kotlin.
2. **The app is the wearer's only commanded way to pause or stop a session** (`SWR-SS-05`, `SWR-SS-06`). The
   accepted fallback is pulling the cable (`SWR-SS-08`), whose safety is itself open (`RISK-PWRSRC-01`,
   `OI-PWRSRC-11`).
3. **Some limits are enforced only before signing, in the app.** `CLAUDE.md` §3 says a protocol outside a PBM
   ceiling "is rejected before it is signed", and the pre-signing check is not built (`OI-HEXTILE-30`,
   `OI-HEXTILE-31`).

## 3. What each app or core refusal depends on

**This table is the sender's preliminary reading of the documents and a few source files. It is not verified, and establishing it is the point of
the item.** For each function the app or core performs, the question is whether something else independently
enforces the same limit.

| Function | What the app or core does | What may enforce it independently | Sender's reading |
|---|---|---|---|
| Charge ceilings | Refuses a protocol over 40 µC/cm² per phase or 150 mC/cm² per session | The safety MCU's charge monitor, against the area and waveform class the hub declares from the signed descriptor. Fixed-pad areas are firmware constants, provisional (`UC-006`); the tDCS area comes from the descriptor | Probably independent, but the divisor is declared by the app for tDCS and HD-tDCS, and unmeasured for the fixed pads |
| Session signing | Signs the protocol | The hub and the safety MCU verify the Ed25519 signature and refuse an unsigned or altered protocol | Independent for integrity. It does not check that the signed content is safe |
| Hardware version and tier | Stamps the version; marks T2 | The hub refuses a mismatched version; the safety MCU withholds T2 lines (not in force until `OI-UPG-08`) | Independent for version. Tier is not in force |
| PBM irradiance and duty | Refuses a protocol over the weighted-sum, 400 peak and duty limits | A hardware drive-stage cap on the tile (`NP-HW-HEXTILE-001` D-9) and the safety MCU's temperature limit | Partly: a current cap and a heat limit, not the irradiance dose. The pre-signing check is not built |
| Zone model | Refuses danger and asks for caution acknowledgements | Only the charge ceilings above | App-only for the current-density boundaries (`OI-ZONE-01`) |
| Cardiac interlock (T2) | None | The safety MCU, R-peak input | Independent |
| Pause and stop | Sends the command | The safety MCU's watchdog on a lost heartbeat; the cable pull | Independent only for a lost link or power; the commanded stop is the app's |
| Cue and burst timing (if built) | Would choose the instant of a burst | Nothing, as proposed (`OI-NPPS-VNS-01` (a)) | New exposure if built |

## 4. IEC 62304 context, for the reviewers to confirm

The sender's understanding, which Quality should check against the purchased standard: software is Class A if no
injury is possible, Class B if non-serious injury is possible, and Class C if death or serious injury is
possible, and the class may be lowered where risk control measures outside the software bring the risk down.
Class C takes the unit-level requirements, detailed design and verification of `NP-SW-001` §3.3 and the Class C
SOUP rules (§9.4). Segregation under §5.3.5 lets items of different class sit in one system if their independence
is shown.

## 5. Options

- **A. Keep SW-03 at Class B**, and record the independence argument per function. Needs the table in section 3
  confirmed, and the gaps closed or accepted (the pre-signing PBM check; the divisor for tDCS).
- **B. Segregate the app** into items under §5.3.5, with the compile / validate / sign path and the stop path in
  their own, higher-class item and the rest of the app at Class B. This forces the shared core into the software
  item list.
- **C. Class the whole app Class C.** The simplest argument and the largest verification burden.

## 6. Questions

**Q1. Which option, and which class for each function?** In particular, the compile, validate and sign path, and
the pause and stop path.

**Q2. Does the shared Rust core become a software item?** If so, what is it called and what class is it, since
every platform reaches it (JNI, C ABI, WebAssembly)?

**Q3. What counts as independent?** For each function in section 3, what hardware or firmware control Quality and
Safety accept as an independent risk control, and how that independence is to be verified. Where the answer is
"nothing yet", is that a gap to close before release or an accepted residual risk?

**Q4. Does the commanded stop need its own treatment?** The cable pull is the accepted fallback for now; is the
app's commanded stop a risk control, and so part of the classified item?

## 7. What we need back

1. A ruling on Q1 to Q4, with the rationale per function in two or three sentences (`CLAUDE.md` §18).
2. The corrections to the table in section 3, and the gaps listed as closed or accepted.
3. Conditions that would reopen it (for example the pre-signing PBM check being built, or a hub stop control).
4. Reviewer names, roles and dates.

## 8. What happens next

The classification and its rationale go into `NP-SW-001` §3.1 and the decision into
`docs/status/completed-decisions.md`. It decides whether a link hang in the Bluetooth library
(`NP-SOUP-BTLE-001` §5.3) is a hazard or an inconvenience (`OI-UI-KMP-09`), the SOUP class of the app's
libraries, and the verification the core's refusals need. Nothing in the repository acts as if an answer has been given.
