# OI-FWHUB-35: how a refused upload can say which limit, and what "no limits in the language" would mean

**Status:** DRAFT, not sent. **From:** a Claude Code session working for the principal. **To:** the principal, with
**GitHub issue:** [#845](https://github.com/stevehickman/NeuroPulse/issues/845)
Software (the shared core and the apps) and FW Hub. No addressee is named.

**Nothing here is built and nothing here is decided.** Two things the principal has said on PR #840 need a design
before anyone builds them. Part 1 is a proposal for the first. Part 2 sets out what the second would mean. No limit
is changed by this file.

## Part 1. Saying which limit a refused protocol was outside

The principal, 2026-11-11: "helmet should say which limit(s) a refused protocol exceeded".

### 1.1 What the frame carries today

The helmet answers an upload on the `UPLOAD_CHECK` characteristic (`firmware/hub_control/include/np_upload_check.h`;
`UPLOAD_CHECK_LEN` in `common/npps/constants.json`). It is 34 bytes and **has no room left**:

| Bytes | Field |
|---|---|
| 0 | status: none, accepted, refused, pending |
| 1 | reason: 0, or the refusal's `np_hub_status_t` as a positive number |
| 2 to 33 | SHA-256 of the bytes the helmet received |

Today a value outside a limit is refused with reason 40 (`NP_HUB_ERR_OUTSIDE_LIMIT`), one reason for every limit,
and the two cervical structural checks have reasons 41 and 42. `np_cmd_limits_admit()` stops at the first command
it refuses, so the helmet knows one cause, not all of them.

The shared core turns the reason into a cause name (`helmet_cause`, `common/npps-core/src/library.rs`), and each app
turns the cause into one message with no parameter (`HelmetRefusalMessages.kt`, `ProtocolUploadFlow.swift`).

### 1.2 The limits this PR's refusals cover

Each would need an id. Parts are those of `NP-FW-HUB-001` `REQ-FWHUB-78`.

| Id (proposed) | Limit or check | Part |
|---|---|---|
| 1 | BES/tACS amplitude | (a) |
| 2 | tDCS current | (b) |
| 3 | tDCS ramp | (b) |
| 4 | auricular VNS amplitude | (c) |
| 5 | intranasal duty | (d) |
| 6 | PBM duty | (e) |
| 7 | EEG gain code | (h) |
| 8 | BES/tACS frequency | (j) |
| 9 | clinical tACS frequency | (j) |
| 10 | audio level | (g) |
| 11 | cervical VNS frequency | (g) |
| 12 | cervical VNS amplitude | (g) |
| 13 | cervical VNS pulse width | (g) |
| 14 | cervical VNS side | (g) |
| 15 | goggles reserved zone-mask bit | (l) |
| 16 | cervical block too short (structural) | (k) |
| 17 | cervical block too long (structural) | (k) |

Parts (f) and (i) are not limits (what a refusal leaves behind; drivers not yet built).

### 1.3 Options

| Option | What changes on the wire | Says all of them? | Cost |
|---|---|---|---|
| **A. A status per limit** | Nothing in the frame: reasons 43 onward, one per row above, as 41 and 42 already are | No: one reason byte, so the first limit found | Hub: one enum value each and the admit function returns it. Core: one mapping each. Apps: one cause and one locale key each (17 keys × 11 locales). No frame change, no length change |
| **B. A bitmask in a longer frame** | The frame grows by 4 bytes (a 32-bit mask of limit ids) to 38 | Yes | A wire change: `UPLOAD_CHECK_LEN`, the generated constants, the hub, the core's frame parser, every app's parser and its tests, and old apps reading a longer frame. The admit function checks every command instead of stopping |
| **C. A list of ids in a longer frame** | The frame grows by a count and up to N id bytes | Yes, up to N | As B, plus a variable length to bound and test |
| **D. A second characteristic** | A new READ characteristic carries the detail (mask or list, and optionally which command); `UPLOAD_CHECK` is unchanged | Yes | A new GATT id in `constants.json` and every app; two reads to correlate, so the detail must carry the same hash or be read under the same notification |

For the message, either one locale key per id (the person reads exactly what to change), or one message that takes
the limit's name as a parameter. The refusal messages take no parameter today, so the second needs that added to
both message tables, and the limit names themselves become locale keys anyway.

### 1.4 What each side must change (for A, and the extra for B to D)

- **Hub:** ids in `np_cmd_limits.h` beside the predicates; `np_cmd_limits_check()` returns which limit, not only
  yes or no. For B to D: collect over all commands; build the longer frame or the second characteristic.
- **Core (`library.rs`):** `helmet_cause` maps each reason to a cause name; its tests list them. For B to D: parse
  the detail and return a list of causes. The core is also where reasons 40, 41 and 42 are still unmapped.
- **Kotlin, Swift, Windows:** one cause each in the three places a cause is named, and the two message tables. For
  B to D: the screen shows a list.
- **Locales:** the keys, in all eleven files.
- **Tests:** the hub's load tests per id; the core's cause table; the apps' "every cause has its own message" tests.
- **Requirement:** a new part of `REQ-FWHUB-78`, and `SWR-UP-02` in `NP-SW-001` for the apps.

### 1.5 Recommendation

**A first, D if "limit(s)" must mean all of them at once.** A needs no wire change and gives the person the exact
limit, which is most of the value; its weakness is that a protocol outside two limits is refused twice, once for
each. B and C change a frame every app parses, for a case (several limits at once) that only a descriptor not made
by the shared core can reach, since the core refuses these values before signing. If all of them at once is
required, D keeps the existing frame intact. I have not built any of it.

**Question for the principal:** is naming the first limit enough (A), or must one refusal name every limit (D)?
And should the detail also say which block of the protocol, which none of the options above carries?

## Part 2. The principal's rule on limits and the language, 2026-11-11

Verbatim: "The protocol language should not have max or min values embedded in the language - those can be
documented in the user manual (along with their source references). That way, if new research comes out that
changes a value the language itself doesn't change - just the doc".

Beside it, the same day: "A max length for a specific protocol should be specified in the protocol definition - not
hardcoded. Max session should only be hardcoded for a health / safety reason - and it should be documented where
that reason came from."

What follows is what I think that means for the repository. It is findings and options. It decides nothing.

### 2.1 Where limits live today

| Place | What it holds | Who reads it |
|---|---|---|
| `NP-NPPS-REF-001` §4 | The authorable range of each field, as part of the language reference | Authors; the validator is written to it |
| `common/npps/constants.json`, group `NPHardwareLimits` | The numbers (85 constants in the file's first group and its neighbours; for example `BES_TACS_MAX_MILLIAMPS`, `TDCS_MAX_MILLIAMPS`, `VNS_MAX_MILLIAMPS`, `BES_TACS_MIN_HZ`) | Generated into the Rust core and each app |
| `common/npps-core/src/validate.rs`, `compiler.rs` | The checks: a value outside a range is an error before signing (`REQ-NPPS-23`); the compiler refuses or, in places, still clamps | Every app, through the core |
| Hub headers (`np_cmd_limits.h`, `np_shared_constants.h`, `np_pbm_config.h`, `np_cvns_config.h`) | A second copy of some numbers, hand-written, tied to the JSON by `check-hub-wire-format.ts` check E for five of them | The hub drivers and the load-time check |
| Safety MCU (`np_safety_config.h`) | The two charge ceilings (40 µC/cm² per phase, 150 mC/cm² per session), thermal and impedance limits, the cardiac thresholds | The Class C monitor |
| A `limits` block in a `.npps` file | A clinician's or owner's tighter limits for their protocols | The validator |

So the language reference states ranges, and the validator enforces them from a constants file that is compiled
into the core.

### 2.2 What would have to move for the language to hold no minimum or maximum

- **`NP-NPPS-REF-001` §4** would describe each field's type and unit and stop stating a range. The ranges and
  their sources would move to the user manual (`NP-IFU-001` or a successor), as the principal says.
- **The validator** is the question. Three readings of the rule, in order of how much changes:
  1. *The language reference stops stating numbers, and nothing else changes.* The numbers stay in
     `constants.json`, the validator still refuses outside them, and a change of value is a change to that file and
     the manual, not to the language document. Smallest change; the language "itself" does not change when a value
     does. But the core still has the numbers compiled in.
  2. *The numbers become data the core loads, not constants it is built with.* A limits table, shipped and
     versioned like the wavelength rules, that the validator reads. A change of value is a new table, with no new
     build of the core or the apps.
  3. *The validator enforces no ranges at all*, only structure and units, and the limits exist only in the manual
     and in whatever the helmet enforces. This removes the pre-signing refusal `REQ-NPPS-23` requires and that
     `CLAUDE.md` §3 relies on ("a protocol outside a ceiling is rejected before it is signed").
- **The hub's copies** would follow whichever reading is taken. Under 2 they would have to come from the same
  table, or the hub and the validator can disagree.

### 2.3 A limits table versioned like the wavelength rules

`docs/human-input/oi-ui-kmp-10-wavelength-rules-versioning.md` proposes, for the wavelength rules: an identity that
is the hash of the resolved set; a release version `major.minor` assigned by NeurOne; an append-only register of
(version, hash) with a build check; and a stamp carried in the compile result, in the signed descriptor outside the
protocol's content hash, and in the UHDR session record, and kept out of SHDR.

The same shape would fit a limits table:
- the table is a file NeurOne ships, with a version and a register of releases;
- the core computes its hash (`CLAUDE.md` §22) and validates a protocol against it;
- the signed descriptor carries the stamp of the table it was validated against, so a session record says which
  limits were in force;
- the helmet either trusts the stamp, or holds the table itself and checks. Only the second is an independent
  check, and it means the helmet needs the table delivered and verified like firmware.

That proposal is itself unanswered (its Q1 to Q8), so this would be built on a scheme not yet decided.

### 2.4 What changing one value would then require

Under reading 1 or 2: the source for the new value recorded in the manual; a new row in the register of releases;
the validator's tests for that range; the hub's copy and its gate, if the hub holds one; the register of
unjustified choices updated if the value now has a source (`CLAUDE.md` §19); and review by whoever owns the limit
(Clinical, Risk). Under reading 2 no app build is needed; under reading 1 one is.

### 2.5 The tension with the hard limits

`CLAUDE.md` §3 calls a set of figures hard limits that "constrain any protocol or firmware work", and §4.2 puts the
charge ceilings in the safety MCU where "the app cannot override" them. Those are enforced below the language and
do not depend on it: the safety MCU cuts a channel over 40 µC/cm² per phase whatever the language allowed. So:

- A limit that is a **hazard control** (the charge ceilings, the 42 °C limit, IEC 62471) stays in firmware under
  the principal's own rule ("hardcoded for a health / safety reason"), with its source documented. `CLAUDE.md` §18
  already says removing one is an ISO 14971 decision.
- A limit that is **not** one (a frequency band, a current figure with no source: `UC-141` to `UC-143`) is what the
  rule moves out of the language. Today the hub enforces several of those too, since this PR. If they leave the
  language they need a home that the hub's refusal and the validator's both read, or the two drift.
- **Which is which is not written anywhere.** The register of unjustified choices lists figures with no source; it
  does not say which of them guard a hazard. Deciding that, figure by figure, is the work this rule starts.

### 2.6 Questions the principal would have to answer

1. Which reading of §2.2: the reference stops stating numbers (1), the numbers become loaded data (2), or the
   validator stops checking ranges (3)?
2. If (2): is the limits table versioned and stamped as the wavelength rules proposal describes, and does the
   helmet hold it or trust the stamp?
3. For each limit, is it a hazard control that stays hardcoded with a source, or a value that moves? Who decides
   each: Clinical, Risk, the principal?
4. Does the pre-signing refusal stay (`REQ-NPPS-23`, `CLAUDE.md` §3)? Reading 3 removes it.
5. What becomes of the hub's refusals added in this PR at values with no source (BES/tACS 1 mA and 0.5–40 Hz, tDCS
   2 mA and its 30 s ramp, the 25 % duty): do they stay as the hub's, move to the table, or go?
6. Which document is "the user manual" for this: `NP-IFU-001`, or a new reference of limits and their sources?

## What to send back

For Part 1: A or D (or B, C), and whether the block must be named. For Part 2: answers to §2.6, or a narrower
question to work on first. The reviewer's name, role and date. Nothing is built until the answer is recorded in
`NP-FW-HUB-001` `OI-FWHUB-35` and `docs/status/completed-decisions.md`.
