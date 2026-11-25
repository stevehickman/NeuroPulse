# Questions for EE and ME: OI-FWHUB-36, the device-only electrode impedance test

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#1057](https://github.com/stevehickman/NeuroPulse/issues/1057)
**From:** a Claude Code session working for the principal (`OI-FWHUB-36` in `NP-FW-HUB-001` §13)
**To:** Electrical Engineering (the impedance front end, the reference loads) and Mechanical Engineering (the dock or
harness, the accessory fixtures), with the safety MCU's owner (SW-01, IEC 62304 Class C) for Q3. No addressee is named.
**Response needed:** an option for each of Q1 to Q4; for Q5, the route by which the load values and pass window will be
derived (not the figures themselves, which wait on `OI-SWCI-34`); a short rationale (CLAUDE.md §18 wants a
derivation); anything that would reopen it; your name, role and the date.

**Nothing here is built and nothing here is decided.** No load value, pass window, tolerance or fixture is proposed,
because none has a derivation (CLAUDE.md §18, §19). Each question lists options and what each costs.

## 1. Why this test exists

The principal ruled on 2026-11-23 (`OI-PRIV-40` (NP-PRIV-ANALYSIS-004), option D of
`oi-priv-40-impedance-bit-in-fault-latch-status.md`) that the **in-session** electrode impedance check is **UHDR**.
That covers its result and everything derived from it: the safety MCU's latched IMPEDANCE bit with its slot and count,
the cervical VNS summary's impedance-pass flag, and `IMP_CROSSVAL`. The reason is that the check measures one circuit
that runs through the wearer (skin, gel, hair, placement, anything worn in the path, the leads, the front end), so
its result cannot tell a device fault from a fact about the person (`docs/reference/data-architecture-detail.md`
§5.1). The firmware changes landed on 2026-11-23, and no SHDR record now discloses it.

That leaves SHDR with **no impedance-derived evidence of device condition at all.** The ruling's answer is a separate
test that takes the same measurement **through a known load, with no person in the circuit**, so the result describes
only the device. All maintenance data is SHDR (`NP-FW-HUB-001` D-41), and §6.9 already holds a catalogue row for it:

| Test | Emits? | Runs where | Output | Class |
|---|---|---|---|---|
| `IMP`: electrode impedance per stimulation channel through a known load | drives a 1 kHz excitation current | signed maintenance session, **in a harness or dock the hub detects, never on a head** | pass / fail against the load's expected value | SHDR (D-41), **not built** |

The questions below are what has to be settled before that row can be built.

## 2. What is known

### 2.1 The in-session check, which the test would reuse

- **Where it runs:** on the safety MCU, `firmware/safety_mcu/src/np_impedance_check.c` (SW01-M06). It injects a 1 kHz
  test current for 50 ms (`NP_IMPEDANCE_TEST_HZ`, `NP_IMPEDANCE_TEST_MS`, `np_safety_config.h`) **before** enabling a
  channel, with stimulation output disabled (enforced by `np_impedance_check_gate()`, FMEA-M05-07).
- **Its four channels:** 0 = VNS/HRV (auricular clip), 1 = tDCS, 2 = BES/tACS, 3 = cervical VNS (a T2 accessory).
  Cervical VNS also has a per-electrode read (left, right), `np_hal_impedance_read_cvns_electrode_ohm()`.
- **Its window:** it grants a channel only between a per-channel floor and `NP_IMPEDANCE_MAX_OHM` (10 000 Ω). The floor
  is 500 Ω for VNS/HRV and cervical VNS, and 1 Ω for tDCS and BES/tACS. The 1 Ω floor rejects only 0 Ω, which is what
  a front end stuck at a rail reads (FMEA-M06-02). The principal adopted the floors as interim values (`OI-FMEA-14`).
- **What leaves the safety MCU:** the enable grant or refusal, and the IMPEDANCE bit with its slot in the heartbeat
  status byte, which is a device-internal control path. **No ohms value reaches the hub** from the gating check.
- **The front end is not designed.** `np_safety_config.h` names the excitation timer, sense pins and a 10 kΩ reference
  leg as **provisional** (`OI-SWCI-34`). The reference leg is uncalibrated, so the ohms the check reads do not rest on
  any bench measurement. Only the fail-safe direction holds: a fault reads high and refuses.

### 2.2 The hub has a second impedance read, for the clip only

`np_mod_vns.c` detects the auricular clip by an impedance read on the hub (`np_mod_vns_hal_impedance_check()`,
`OI-VNS-01`), against its own contact range (`VNS_CONTACT_MIN_OHM`, 500 Ω). That read also passes through the wearer,
and its raw value is written only to UHDR.

*Found while preparing this brief, out of its scope:* the comments on `contact_confirmed` (`np_hub_types.h:599`,
`np_mod_vns.c:308`) call it "SHDR-safe", and `np_session_log.c:386` says it "is written in SHDR fault record on
disconnect". No code writes it to SHDR, so nothing leaks, but under the 2026-11-23 ruling a contact confirmation that
passes through the wearer is UHDR, and the comments now say otherwise.

### 2.3 What the test inherits from §6.9 and D-41

- **An emitting or driving test runs only as a signed maintenance session** through the normal session path, so the
  safety MCU, charge ceilings and signature check apply unchanged (§6.9). The maintenance session kind is not built
  (`OI-FWHUB-20`).
- **"Docked" is a hardware signal the hub reads itself**, never the app's or the user's say-so. It fails closed to
  "not docked", and it must be one a person cannot produce while wearing the helmet (D-41 condition 2;
  `OI-FWHUB-25`). The dock does not exist.
- **Who runs it:** the warranty owner, to diagnose their own device (§6.9, the principal, 2026-09-27).
- **How results are kept:** one latest-state row per unit tested, with no per-run log (§6.9's rule for `0x87`).
  *Assumed* to carry over to `IMP` as one latest-state verdict per channel. That is a firmware matter, not a question
  here.

## 3. The questions

### Q1. Who runs the test, and in what?

§6.9 says the warranty owner runs maintenance tests. The catalogue row says "a harness or dock", and they are not the
same thing.

| Option | What it means | Cost |
|---|---|---|
| **A. Owner, in the maintenance dock** | The dock of `OI-FWHUB-25` gains a reference load per headset stimulation channel, and a fixture for each accessory | Every dock carries the loads and fixtures, which adds cost per unit sold. Keeps §6.9's "owners diagnose their own device" for impedance. The dock already has to exclude a head and be detected from hardware |
| **B. Service network only, in a harness** | A service fixture (`docs/reference/service-network.md`) and not an owner test | One fixture per service point, not per unit. The owner cannot run `IMP`, so §6.9's catalogue row says "service only" and the app's maintenance section does not offer it. Impedance device data reaches SHDR only after a service visit |
| **C. Split** | Headset channels (tDCS, BES/tACS) in the owner's dock, accessories (clip, cervical pads) on a service fixture | Two presence signals to design and verify. Fits if accessory fixtures are too bulky or costly to put in every dock |

### Q2. What does the load connect to?

| Option | What is tested | Cost |
|---|---|---|
| **A. At the electrode surface** | The helmet's tES electrodes rest on, and the clip and cervical pads clamp to, contacts that close through the reference load | The whole chain short of skin: front end, leads, connector, electrode. Needs a fixture per electrode geometry, with contact force and contact resistance controlled well enough that a fixture fault does not read as a device fault |
| **B. At the port** | A load plug replaces the accessory, or the load is wired across the headset channel's connector | The hub path and front end only. The accessory's leads and electrodes, which are the parts most likely to wear, are not tested. Cheap, and the plug is simple |

For the headset's tES channels the two options may coincide, depending on where those electrodes sit. That is ME's
call.

### Q3. Where is pass or fail decided? (safety MCU owner)

This decides whether the test needs a Class C change.

| Option | How it works | What it catches | Cost |
|---|---|---|---|
| **A. The in-session gate, unchanged** | The maintenance session asks for the channel at zero amplitude. Granted = pass, refused with IMPEDANCE = fail | Open, short, front end at a rail. **Not** drift inside the gate's window, which is wide (floor to 10 kΩ) | **No Class C change, and no new path out of the safety MCU.** Buildable once the front end and the dock exist. With a single load inside the window, a front end stuck reading "good" passes. A second load the gate **must refuse** (an open, or one above the maximum) tests the other direction |
| **B. The safety MCU returns ohms to the hub** | A new reply carries the measured ohms in a maintenance session, and the hub compares them with a window around the load's value | Drift, if the window is tighter than the gate's | **Class C change** (a new reply). The safety MCU cannot tell a maintenance session in a dock from a session on a head, because only the hub reads "docked" (D-41 condition 2). Ohms from a check on a head are UHDR, so the hub becomes the only thing standing between them and SHDR |
| **C. The safety MCU reads the presence signal itself** | The safety MCU sees "docked" directly and compares with the window itself | Drift | **The largest Class C change** (a new input and a new comparison). It is the only option where the part that measures also verifies that no person is in the circuit |

Recommendation, for the reviewer to accept or reject: **A, with two loads per channel (one the gate grants, one it
refuses).** B and C are worth their Class C cost only if a failure inside the gate's window has to be caught. §18 asks
what would fail if it were not: name the hazard or warranty failure that drift detection prevents, and B or C follows
from it.

### Q4. Must "docked" hold for the whole measurement?

D-41 makes the dock exclude a head physically, so a person cannot be in the circuit while it is closed. The open
question is a dock opened mid-test.

| Option | Cost |
|---|---|
| **A. The hub reads the signal before the session starts and after the result, and a verdict with "not docked" at either point is discarded (`NOT_RUN`)** | Firmware only. A dock opened and closed again between the two reads is not caught. That matters only if a head can reach the electrodes in that window, which the 50 ms measurement makes unlikely but ME should confirm |
| **B. "Docked" falling ends the maintenance session** (the hub stops it, and the safety MCU withholds the channel) | Needs the hub to watch the signal continuously during the session, which `OI-FWHUB-25` already requires ("readable by the hub continuously"). Still not a Class C input unless Q3 is C |

### Q5. The loads and the pass window: how will they be derived?

**No figure is asked for here,** because none can be derived before the front end is designed (`OI-SWCI-34`). What
this brief needs back is the route:

- **Load value per channel:** inside the gate's window for the granted load (Q3 A), and outside it for the refused one.
  Is it a nominal electrode-plus-skin impedance from a cited source, or simply a value the front end resolves well?
- **Pass window** (only if Q3 is B or C): from the front end's accuracy budget (reference leg tolerance, ADC, excitation
  amplitude) plus the load's tolerance, so the window is computed and not chosen (CLAUDE.md §18, "a derived bound is
  computed from its foundational value").
- **Cervical VNS per-electrode:** whether the test checks left and right separately. It is a T2 accessory, so the
  question arises only on T2 units.

Until those exist, any value written into `np_safety_config.h` or the dock specification needs a row in
`docs/status/unjustified-choices.md` (CLAUDE.md §19).

## 4. What follows from the answers (firmware, no further ruling)

Whatever the options, these follow and are recorded here so that the reviewer can see the whole cost:

- A new SHDR record type for `IMP` verdicts (one latest-state row per channel, no run log), with its writer declared as
  a reporting path in `scripts/check-redaction-shape.ts`. The gate must show the writer is reachable **only** from the
  maintenance path with "docked" confirmed, and never from the in-session check.
- The `IMP` test in the maintenance request, with `REFUSED` (as `LED` is today) until the dock signal and the
  maintenance session kind exist.
- A hazard row in `NP-RISK-004` for the dock detected as present when it is not. `OI-FWHUB-25` already calls for it,
  and here the consequence is a privacy one (an on-head measurement written to SHDR) as well as a safety one.
- A requirement row in `NP-FW-HUB-001`, to §18's standard: what fails without the test is that SHDR has no
  impedance-derived device condition (the principal's 2026-11-23 ruling), traced to `OI-PRIV-40` and D-41.

## 5. Order of work

`OI-SWCI-34` (front end) → this brief's Q3 and Q5 → `OI-FWHUB-25` (dock and its signal, Q1, Q2, Q4) → `OI-FWHUB-20`
(maintenance session kind) → firmware for §4. Q1 and Q2 can be answered now, because they do not depend on the front
end.
