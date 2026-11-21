# Question for the principal: OI-PRIV-31 (NP-PRIV-ANALYSIS-004), whether and how a signed protocol is made fresh

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#959](https://github.com/stevehickman/NeuroPulse/issues/959)
**From:** NeurOne Software Engineering (`OI-PRIV-31` (NP-PRIV-ANALYSIS-004) in `docs/status/pending-decisions.md`; firmware privacy review `docs/np_priv_analysis_004.md`, Finding M4)
**To:** the principal (design and wire format). Safety, for option D's risk acceptance.
**Response needed:** an option, a short rationale, anything that would reopen it, and your name, role and the date.

## 1. The question

A validly signed session protocol can be loaded again on the helmet it was signed for, as many times as
anyone who holds a copy likes. Should the hub refuse a protocol it has already accepted, and if so, by
which mechanism?

The other half of Finding M4 needed no decision and is done. The device-serial guard used to be skipped
when the helmet could not read its serial, and it accepted every serial-less protocol when the serial was
all zero. It now fails closed in both cases (`REQ-FWHUB-90`, `NP-FW-HUB-001` §4.2). That guard is
cross-device only. This question is about the same device.

## 2. What is known

| Fact | Where |
|---|---|
| The signed header has `session_uuid` (16 random bytes from the app), `compiled_at_unix` and `device_serial`. It has no nonce and no counter | `np_hub_types.h` `np_proto_header_t`; `NP-FW-HUB-001` §4.2 |
| `compiled_at_unix` is copied into the session descriptor and never compared with anything. The hub has no RTC backup, so it cannot judge a timestamp's age | `np_protocol.c` (the descriptor copy); CLAUDE.md §4.5 |
| No record of accepted UUIDs or sequence numbers exists, in RAM or in the Config store | `np_protocol.c`, `np_session_runner.c` |
| Uploading needs an encrypted, bonded BLE link (`0x0008`, `REQ-FWHUB-80`/`-81`) or physical USB-C access. USB-C CDC has no link-layer encryption yet (CLAUDE.md §23, `OI-SIMFW-62`) | `NP-FW-HUB-001` §2.5 |
| A replayed protocol is still checked against every hub limit, the tier, the hardware version, the layout and the photic zone, and the safety MCU checks its signature again. A replay can only run something that was signed and admissible | `np_runner_load`; `firmware/safety_mcu/src/np_session_sig.c` |
| `session_uuid` keys the UHDR session record. A replay writes a second UHDR record under the same UUID | `np_session_runner.c` (the UUID copy into the UHDR record), `np_session_log.c` |
| Mode 3 (autonomous) runs a pre-programmed protocol repeatedly without the phone. Repetition is a feature there. Today `NP_PROTO_FLAG_AUTONOMOUS` is set by iOS and read by no firmware | CLAUDE.md §4.6; `np_hub_types.h` |
| The app's `used` flag (`SWR-WO-02`/`-03`) is advisory, carries no device, count or time, and is not a security control | `NP-SW-001`; `NP-NPPS-REF-001` |
| No descriptor has shipped, so a header change only needs a version bump (`REQ-FWHUB-10`) | `NP-FW-HUB-001` §4.5 |

**What a replay gains an attacker:** a session the wearer did not ask for, started now (an unsolicited
stimulation, within signed limits), plus a duplicate UHDR session key. It does not give a session outside
the limits or a session on another helmet.

## 3. Options

**A. Hub challenge (recommended).** Before each upload the hub publishes a fresh random nonce (a GATT
read, and the same over CDC). The app puts it in the signed header and the hub accepts a descriptor only
if it carries the current nonce, which is used once.
- *Wire:* 16 bytes in the header and a version bump. `hubCompiler.ts`, the Android, iOS and Windows
  compilers, the goldens and `scripts/check-hub-wire-format.ts` change together. A new characteristic,
  encrypted-link only.
- *Safety MCU:* none, if its digest covers the serialized descriptor as it does now (to be confirmed).
- *Mode 3:* unaffected. Programming is one upload, and re-running a stored protocol is not an upload.
- *Cost:* one extra round trip per upload. A protocol cannot be signed ahead of time for later upload.
  No persistent hub state, so no flash wear and no §5 question.
- *Fit:* §23's payload encryption (`OI-SIMFW-62`) also needs a hub-contributed random value for key
  agreement. Designing the two together gives freshness for almost nothing.

**B. Signed monotonic counter.** The app keeps a counter per helmet, and the hub persists the last one it
accepted and refuses any counter that is not larger.
- *Wire:* 4–8 bytes in the header and a version bump, so the same four compilers change.
- *Cost:* app state per helmet that has to survive reinstalls and agree across several phones (a clinic
  helmet), so the app also needs a GATT read of the hub's last counter. That is a challenge with extra
  state. One Config write per accepted session.

**C. Persisted record of accepted UUIDs, or of the last `compiled_at`, with no header change.**
- A UUID ring of N entries refuses a repeat only while the UUID is still in the ring. Older captures are
  accepted again.
- "`compiled_at` greater than last accepted" locks out a phone whose clock runs behind.
- *§5:* both persist a fact about when the person had sessions. Under "when in doubt → UHDR" this belongs
  in the UHDR partition, not Config, so the check would depend on the UHDR mount.

**D. No freshness, risk recorded.** Keep the cross-device guard (now fail closed) and state the residual
risk: an unsolicited session from a captured protocol, by someone with a bonded link or USB-C access.
- *Cost:* none in code. It needs an ISO 14971 entry and Safety's acceptance, because the hazard is an
  unwanted stimulation. CDC stays the weak path until `OI-SIMFW-62` covers it.

## 4. Recommendation

**A, designed together with §23's payload encryption (`OI-SIMFW-62`), as one wire version.** It is the
only option that has no persistent state, no §5 question and no clock dependence, and it covers USB-C and
BLE alike. Until it is built, record D's residual risk in the risk file so the gap is accepted knowingly
rather than by default.

**Would reopen it:** a requirement to sign protocols offline for later upload (which rules out A and
favours B), or a Mode 3 design in which the hub accepts uploads with no phone present.

## 5. Response

| Field | Answer |
|---|---|
| Option | |
| Rationale | |
| Would reopen | |
| Name, role, date | |
