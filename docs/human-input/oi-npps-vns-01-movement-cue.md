# Firmware, safety and clinical question for review: OI-NPPS-VNS-01 (a), where does the movement cue come from?

**Status:** DRAFT, ready to send; not yet sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Software (`OI-NPPS-VNS-01` (a) in `docs/status/pending-decisions.md`; `NP-NPPS-REF-001` §4.6)
**To:** Firmware (hub), Safety and a clinical adviser (names to be assigned; the item's owner column reads "FW Hub + App + Clinical")
**Response needed:** a ruling on each of Q1 to Q4, with rationale, the reviewers' names and roles, and the date.
**Related:** `oi-npps-vns-01-right-and-bilateral-side.md` ((f), a separate question on the same item). The threshold-relative intensity fields ((b), (b2)) were retired on 2026-10-30 and are not part of this.

## 1. The question

The paired-VNS stroke studies deliver a short burst of stimulation **on a movement cue**, not a continuous
train. The `.npps` grammar can say so (`trigger: movement_cue` with `burst: 0.5s`), but the validator and the
compiler refuse it, because the hub has no input that reads a cue. Where should the cue come from, and what
must the system do when a cue is missed, doubled or lost?

Plain form: what tells the headset "the person just started to move", and what is safe when that signal is
wrong?

## 2. What is known

**What the studies do**
- Chang 2021 (36 chronic-stroke patients, left cymba): 500 ms bursts at 30 Hz, 0.3 ms pulses, triggered by a
  movement cue, 256 stimulations in each 1 h session, 9 sessions. Where its cue came from is not recorded in
  the open item; the reviewer should check the paper.
- Capone 2017 used 30 s trains every 5 minutes, with no movement trigger.
- So a cue-triggered protocol has a **fixed burst, an unknown number of bursts and an unknown spacing**,
  decided by the person's movement and the task, not by the file.

**What the device does today**
- The grammar has `trigger` (`continuous`, the default, or `movement_cue`) and `burst`. A `movement_cue` block
  is refused by the validator and the compiler (`REQ-NPPS-07`), so the protocol is refused, not run
  continuously.
- The hub has no trigger input. Its only commanded interface is the companion app, over USB-C or Bluetooth.
  The hub and headset have no stop control, which the principal has accepted for now (`OI-UI-KMP-09`).
- The safety MCU owns every stimulation enable line (`CLAUDE.md` §4.2) and enforces the 40 µC/cm² per-phase
  charge limit. It has no burst-count, spacing or duty term. The app-side zone model does bring pulse rate
  and duty in, through current density, but it is advisory.
- Connected mode streams in real time (under 1 ms is the stated target); autonomous mode (Mode 3) runs from a
  power bank with no phone and no app (`CLAUDE.md` §4.6).
- A change to an accessory port lands on both tiers or neither (`REQ-UPG-03`).

**What is not in any document read:** a latency requirement from movement to burst, a limit on bursts per
session, a minimum spacing between bursts, or what a stuck or lost cue should do.

## 3. Options for the source

| | Where the cue comes from | Works in Mode 3 | What it costs |
|---|---|---|---|
| **A** | The app, over Bluetooth or USB: a button, or an app-side integration with the task | No (needs the app) | The cheapest. Latency is the app link's, which is variable. The cue stops if the link drops |
| **A2** | A wearable motion sensor, such as an Apple Watch worn on the affected arm, detecting movement onset and sending the cue to the app (or to the hub) | No (needs the app, or a hub that accepts a second Bluetooth device) | See the note below. It adds a detection algorithm, a second link and a platform dependency |
| **B** | A task controller or robot, over the hub's USB-C data connection | Only if the hub can accept a second device | Whether the hub's USB-C port can act as anything but a device to the app is not established in any document read |
| **C** | A dedicated wired input on the hub or the accessory port | Yes, if the robot is wired to it | New hardware and a new specification. A wired signal from outside equipment raises patient-isolation questions. It would sit on both tiers (`REQ-UPG-03`) |
| **D** | None: `movement_cue` stays refused | n/a | Paired-stimulation protocols cannot be authored. No work |

**Note on A2 (a wearable such as an Apple Watch).** The sender has not verified these points against Apple's
documentation or tested them; they are for the Firmware and App reviewers to confirm or correct.
- A watch has motion sensors and could, in principle, signal that an arm has started to move. Nothing in this
  repository uses one.
- The cue would pass over two links (watch to phone, then phone to hub, or watch to hub), so its delay is the
  sum of both and is not guaranteed by the platform. The watch app would also have to keep running and
  sending while the arm moves.
- The hard part is the **detection**, not the link. A stroke patient's attempted movement may be small or
  slow, and tremor, spasticity or a caregiver moving the limb could look like movement onset. A false cue
  delivers a burst nobody intended; a missed one delivers none (Q3).
- It would serve only one phone ecosystem unless equivalents are built for the others, which cuts against
  the platform's one-shared-UI rule (`CLAUDE.md` §20). It would also put a consumer device in a path that ends in
  stimulation, which bears on the app's software safety class (`OI-SW03-01`).
- Movement data from the person is UHDR (`CLAUDE.md` §5) and must stay on the device.

## 4. Questions

**Q1. Which source, if any?** A, A2, B, C or D, or a staged answer (for example D now, A later).

**Q2. What does the signed descriptor bound?** The cue chooses the instant of a burst; it should not choose
the dose. The sender's reading is that the platform's rule (a protocol is refused, never reshaped) points to
declaring an envelope in the signed protocol, so the worst case is known and checked before signing:
- the burst length (already `burst`)
- a minimum spacing between bursts
- a maximum number of bursts in a session

  With those, the worst-case duty and mean current can be computed and held to the zone model and the
  charge limits at signing. Do the reviewers want that envelope, or something else? Should the safety MCU
  enforce any of it, which would be a Class C change?

**Q3. What must it do when the cue is wrong?**
- **A missed cue:** no burst. Is "no stimulation" the accepted fail-safe?
- **A doubled cue, or a cue during a burst:** ignored, or queued, or counted? A stuck-high cue: one burst, then
  nothing until it falls?
- **The cue source is lost mid-session** (link drop, cable out): stop, hold, or carry on with no bursts?
- **A cue during a pause, a ramp or an interlock hold:** always ignored?

**Q4. What latency does the therapy need, and how reliable must the detection be?** The clinical adviser should say how long after the movement onset
a burst may begin without losing the pairing. That figure decides whether source A or A2 is viable at all, since the
app link's delay is variable. For A2, the clinician should also say how many false and missed cues are tolerable.

## 5. What we need back

1. A ruling on Q1 to Q4.
2. The rationale in two or three sentences, recorded as the derivation (`CLAUDE.md` §18). For Q2, the numbers
   if an envelope is wanted, and their basis.
3. For Q4, the latency requirement and where it comes from.
4. Conditions that would reopen the ruling.
5. Reviewer names, roles and dates.

## 6. What happens next

The ruling is recorded in `docs/status/pending-decisions.md` (`OI-NPPS-VNS-01` (a)) and
`docs/status/completed-decisions.md`. A hub input (C) or a safety-MCU limit becomes its own specification and
requirement before anything is built. Nothing in the repository acts as if an answer has been given.
