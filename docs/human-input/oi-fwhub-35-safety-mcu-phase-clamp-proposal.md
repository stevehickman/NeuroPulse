# OI-FWHUB-35 (d): the safety MCU's declared-phase clamp. Why it exists, and a proposal to refuse instead

**Status:** DRAFT, not sent. **From:** a Claude Code session working for the principal. **To:** the safety MCU's
owner (SW-01, IEC 62304 Class C). No addressee is named.

**This is a proposal. Nothing under `firmware/safety_mcu/` is changed by it, and no safety MCU test or document is
edited.** The principal asked (2026-11-11): "Have the agent prepare the refusal change as a proposal. Also - why
does this clamp exist? What is the justification for this max value?"

## 1. The decision wanted

Should the safety MCU keep clamping a declared phase longer than `NP_CHARGE_MAX_PHASE_US` (2,000,000 µs), or
withhold the channel instead (option B below), and is 2 s the right figure to hold at all?

## 2. What the clamp is

A charge-balanced channel (BES/tACS, VNS, cervical VNS, clinical tACS) is checked per phase: the commanded
current times the phase duration, against 40 µC/cm² times the declared electrode area. The hub declares the phase
duration in `np_safety_chan_wave_cmd_t.phase_us`, a 32-bit field
(`firmware/common/include/np_spi_wire_types.h` lines 468 to 475).

Two places clamp that declaration at `NP_CHARGE_MAX_PHASE_US`:
- the hub, `np_chan_decl_half_period_us()` (`firmware/hub_control/src/np_chan_decl.c`), before it sends;
- the safety MCU, `np_charge_monitor_set_channel_waveform()`
  (`firmware/safety_mcu/src/np_charge_monitor.c` lines 270 to 293), on receipt.

## 3. Why it exists, and where the 2 s comes from

**Found in the repository.** The constant's own comment answers both questions
(`firmware/common/include/np_spi_wire_types.h` lines 453 to 466):

- *What it is for:* "both ends clamp: the hub so the frame it transmits already says what the MCU will conclude,
  and the MCU because it must not depend on the hub having done so — a corrupt or hostile phase_us would otherwise
  overflow the uint64 per-phase arithmetic."
- *What it is not:* "this clamps a declaration DOWN, and a shorter declared phase yields a SMALLER computed
  charge. So the clamp is not itself a safety control — it is bounds-checking, and what makes an under-declared
  phase safe is the fail-closed declaration gate, not this."
- *Where 2 s comes from:* "2,000,000 µs = 0.25 Hz. Below the 0.5 Hz BES/tACS floor (CLAUDE.md §3), so it clamps
  nothing real."

`np_charge_monitor.c` repeats the overflow reason at lines 431 to 433: "current_ua <= 65535 and phase_us <=
NP_CHARGE_MAX_PHASE_US (2e6), so the widest intermediate is 65535 x 2e6 x 2000 = 2.6e14 — comfortably inside
uint64." Its test `test_phase_clamped` (`firmware/safety_mcu/tests/np_charge_monitor_tests.c` line 803) checks
that "an absurd phase_us clamps and still trips (no overflow wrap)".

**So the answer is: it is an arithmetic bound with a stated purpose, and 2 s is a round figure chosen to sit one
octave below the lowest BES/tACS frequency. It is not a health limit and no source says it is.**

**Two things the stated reason does not survive.**

1. **The overflow it guards against cannot happen with the present types.** `phase_us` is 32 bits and the
   commanded current is 16 bits, so the widest intermediate without any clamp is
   65,535 × 4,294,967,295 × 2,000 = 5.6 × 10^17, and a `uint64_t` holds 1.8 × 10^19. This is my arithmetic, not a
   statement in the repository; the comments quoted above assume the clamp is needed. If the field or the factor
   were widened later it could become needed, which is a reason to keep a bound, not this value.
2. **"It clamps nothing real" was only true by assumption.** Until 2026-11-11 the hub driver had no frequency
   limit, so a BES/tACS command below 0.25 Hz was driven with its true phase and declared with a clamped one, and
   the per-phase check passed a phase shorter than the one delivered. The declaration gate the comment relies on
   does not catch that: it withholds a channel with **no** declaration, and a clamped declaration is a
   declaration. The hub now refuses BES/tACS outside 0.5–40 Hz and clinical tACS below 0.5 Hz
   (`NP-FW-HUB-001` `REQ-FWHUB-78` (j)), and `np_chan_decl_tests` proves over every 16-bit frequency that no
   command the hub admits reaches the clamp. So today it is unreachable from the hub.

**Not found.** No design document derives 2 s. `docs/np_dt_001.md` §3.2.1 and `NP-SW-001` §5.1 (SW01-M03) do not
mention the constant. The repository's history is squashed, so the commit that added it cannot be read
(`git log -S` shows only later commits that touch the name). It had no row in
`docs/status/unjustified-choices.md` until this change (`UC-146`).

**Literature.** Not searched. The instruction was to search only if the repository did not answer, and it does:
the value is a bounds check, not a dose limit. For the record, nothing in the repository connects a maximum phase
duration to a health source; the health control for a charge-balanced channel is the 40 µC/cm² per-phase ceiling,
which already grows with phase duration and needs no separate cap on it.

## 4. Options

| Option | What it does | Per-phase check | Overflow | Failure scenario |
|---|---|---|---|---|
| **A. Keep the clamp as a guard** | Nothing changes. The hub's refusal makes it unreachable from a correct hub | Correct for every command the hub admits. For a hub fault or a hostile frame that declares more than 2 s, the check runs against 2 s, which **under-states** the charge | Bounded as today | A hub defect that drives a slower waveform than it declares is passed at the 2 s figure. Silent |
| **B. Refuse: withhold the channel** | A declared phase above the bound is not stored. The channel is marked as having an invalid declaration and is kept out of `granted_mask` for the session, as an undeclared channel is | Never runs on a value the MCU altered. A channel with a phase the MCU will not honour does not stimulate | Bounded: the out-of-range value is never used in arithmetic | A hub defect becomes a channel that does not start, visible as an enable never granted. Fail-closed |
| **C. Remove the clamp** | The declared phase is used as sent, up to the field's 32 bits | Correct for any declared phase: a longer phase gives a larger computed charge, which trips sooner. The strict direction | Still inside `uint64_t` by the arithmetic in §3, but with no bound a later change to the field or factor has nothing to catch it | None for charge. The risk is to a future edit, not to a person |

**Recommendation: B.** It is the same rule the hub now follows (refuse, never reshape), it fails closed, and it
keeps a bound for the arithmetic without ever computing on an altered value. C is defensible on the arithmetic
alone and is simpler, but it removes the only range check on a field that arrives over SPI. A is what exists and
is safe only as long as the hub is right, which is the thing a Class C monitor is there not to assume.

## 5. The refusal option in concrete terms (not applied)

**Function:** `np_charge_monitor_set_channel_waveform()` in `firmware/safety_mcu/src/np_charge_monitor.c`.

**Behaviour.** Replace the clamp with a refusal that the existing declaration gate enforces:

```c
static bool s_phase_invalid[NP_SAFETY_MAX_CHANNELS];   /* cleared where s_wave_class and s_phase_us are cleared */

    /* in np_charge_monitor_set_channel_waveform(), in place of the clamp: */
    if (phase_us > NP_CHARGE_MAX_PHASE_US) {
        s_phase_invalid[channel] = true;   /* never stored, never used in arithmetic */
        return;
    }
    if (phase_us > s_phase_us[channel]) {
        s_phase_us[channel] = phase_us;
    }

    /* in np_charge_monitor_decl_gate(), beside the undeclared-class test: */
    if (s_wave_class[ch] == 0U || s_phase_invalid[ch]) {
        state->granted_mask &= (uint16_t)~bit;
    }
```

The order inside `set_channel_waveform` matters: today the waveform class is ORed in before the phase is looked
at. In the proposal an out-of-range phase must not leave a class stored with a phase of 0, because
`charge_per_phase_nc()` returns 0 for a phase of 0 and the channel would then pass. Either the invalid flag is
tested by the gate as above (which covers it), or the class is not stored either.

**Fault reporting.** I read the two existing gates (`np_charge_monitor_geom_gate`, lines 317 to 327;
`np_charge_monitor_decl_gate`, lines 346 to 358). Both withhold the channel from `granted_mask` and **neither
latches a fault or sets a status bit**: the hub sees an enable it requested and was not granted. The proposal does
the same, so it needs no new wire status. If the owner wants the reason reported, that is a new bit in the
heartbeat status and a wire change on both processors, and belongs with `OI-SIMFW-61` (the Class C half of "the
firmware reports which check failed").

**The hub side, if B is taken.** `np_chan_decl_half_period_us()` should stop clamping too. It is unreachable for
any command the hub admits, so removing it changes no behaviour; that edit is Class B and can follow the owner's
decision in the same change.

**Tests** (`firmware/safety_mcu/tests/np_charge_monitor_tests.c`): `test_phase_clamped` becomes a test that a
phase one above the bound withholds the channel and that the stored phase is unchanged; a phase exactly at the
bound is stored and checked; a second, valid declaration in the same session does not clear the invalid mark; a
session reset does. `np_safety_spi_proto_tests.c` line 656 (the bound exceeds 16 bits) is unaffected.

**Requirement.** `NP-SW-001` §5.1 SW01-M03 gains a part: a declared phase outside the range the monitor accepts
withholds the channel; the monitor never substitutes a value. What fails without it: the per-phase check passes
a phase shorter than the one declared. Trace: CLAUDE.md §4.2 (fail-closed declaration), §3 (a ceiling refuses).

## 6. What a change to SW-01 triggers

From the files I read: the safety MCU host tests and cross-compile leg (`.github/workflows/safety-mcu-ci.yml`,
which runs on any change under `firmware/safety_mcu/`), a revision of `NP-SW-001` for SW01-M03, and SW-01 review,
since "no change here may add a bit to a Class C wire format without SW-01 review" (`NP-FW-HUB-001` header) and
this one changes Class C behaviour. `NP_CHARGE_MAX_PHASE_US` itself is in the shared header
`firmware/common/include/np_spi_wire_types.h`, so a change to its value touches both processors.

**Not checked by me:** whether the safety MCU's flash image carries a CRC or a version record that a code change
must update, what the Class C release procedure requires beyond the CI legs, and whether the FMEA
(`NP-FMEA-001`) has a row that cites the clamp. The owner will know; I did not read those.

## 7. Should 2 s be a hardcode at all?

The principal's rule of 2026-11-11: "Max session should only be hardcoded for a health / safety reason - and it
should be documented where that reason came from", and otherwise a maximum belongs in the protocol definition.

Applied here:
- 2 s is not a health or safety figure and nothing documents it as one (§3). By the rule it should not stand as
  a hardcoded *limit on what may be authored*.
- It is not acting as one today. What limits the authored frequency is the 0.5 Hz floor, which is a
  `CLAUDE.md` §3 value held in `common/npps/constants.json` and now enforced by the hub; that floor has no
  source either (`UC-143`), and it is the number a protocol author actually meets.
- What the safety MCU needs from this constant is a range for a field it receives, so that its arithmetic and
  its test of that arithmetic have a bound. That is an implementation bound, like the 16-bit width of a current,
  and it does not have to be 2 s. Under option B it could be the field's full width (no figure at all, option C
  in effect), or any value comfortably above the longest phase a protocol can author. If the BES/tACS floor is
  ever lowered below 0.25 Hz, a 2 s bound would start refusing real protocols under B, where under A it silently
  under-states them. Under either, the two numbers are coupled and nothing says so except the comment.
- The phase the hub declares is already protocol-authored: it is computed from the signed frequency or pulse
  width. Making the *bound* hub-declared would let the hub set the range of its own declaration, which defeats
  the independence the MCU's check exists for. So: the declared phase stays authored, and the bound, if one is
  kept, stays in the MCU as an arithmetic guard and is named as that, not as a maximum phase.

**The overflow bound under each choice.** Keeping 2 s: 2.6 × 10^14, as the comment says. Using the field's full
width: 5.6 × 10^17, still inside 1.8 × 10^19 (§3, my arithmetic; the owner should check it, and the 2000/3141
sine factor in particular).

## 8. What to send back

The option (A, B or C); whether 2 s stays, changes or goes; whether the refusal should be reported in the
heartbeat status; conditions that would reopen it; and the reviewer's name, role and date. The answer is recorded
in `NP-SW-001` (SW01-M03), `NP-FW-HUB-001` `OI-FWHUB-35` (d) and `docs/status/completed-decisions.md` before any
safety MCU code changes.
