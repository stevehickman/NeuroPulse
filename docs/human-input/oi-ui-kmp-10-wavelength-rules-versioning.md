# Principal decision for review: OI-UI-KMP-10 (H3), a versioning scheme for the wavelength rules

**Status:** DRAFT, ready to send; not yet sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#564](https://github.com/stevehickman/NeuroPulse/issues/564)
**From:** NeurOne Software Engineering (`OI-UI-KMP-10` open question (3) in `docs/status/pending-decisions.md`; GitHub #564)
**To:** the principal
**Response needed:** a ruling on each of Q1 to Q8 (§8), with a short rationale, any condition that would reopen it, and the date.
**Nothing here is decided.** §4 to §7 are a proposal. No code, firmware or controlled document acts on it: the session
record still carries no rules version (`SWR-HV-02`, `np_session_fitting.h`), and nothing computes one.

## 1. The question

The wavelength rules (`NP-NPPS-REF-001` §7a) say which emitter channel delivers a wavelength a protocol requests.
What is "the version of the wavelength rules", who assigns it, and how does the helmet come to write it at the start
of a session?

It has to be settled before the shipped rules are first changed. No change is planned, so nothing is blocked today.

## 2. What is already decided, and what exists

| Decided (source) | What it fixes |
|---|---|
| `OI-UI-KMP-10` H3 (2026-10-08) | The rules are defined by NeurOne and versioned in the same manner as protocols. They are not part of a protocol or its hash |
| `OI-UI-KMP-10` H1, `SWR-HV-01` | The protocol hash holds the wavelength as the protocol requests it. It excludes the rules and their version |
| `OI-UI-KMP-10` H2, `SWR-HV-02` | The helmet writes the rules version in use into the session record at the start of each session. The record is UHDR |
| `NP-NPPS-REF-001` §7a | A user may edit the windows. A user rule for a channel replaces the default for that channel only. The mapping is nearest nominal, a tie going to the earlier of 660, 808, 1064 |
| `OI-UI-KMP-10` P5, `SWR-PL-05` | The protocol model: a release version `major.minor` assigned by NeurOne, and an identity that is a content hash. What makes a change major is not written |

**Built.**
- The rules reach the compiler as the `wavelength_rules` option (`common/npps-core/src/compiler.rs`), read from the
  request field `wavelengthRules` (`api.rs`). When it is absent the compiler uses `WavelengthRules::default_rules()`.
- The compiled content hash leaves the rules out, and a test shows that loading a rules table does not change it
  (`common/npps-core/tests/identity.rs`).
- The hub writes the fitting record at session start (`NP_LOG_TAG_UHDR_FITTING`): hardware version and one byte per
  socket.

**Not built, anywhere.**
- No version of the rules exists in any form. The `wavelength_rules` block has no version field, the two stores that
  hold a user's edits (`np_wavelength_rules_user` on the web, `np.wavelength-rules.user` in the Kotlin core) hold no
  version, the compile request and its result carry none, the signed descriptor carries none, and the fitting record
  says so in its header and leaves the field out.
- **The helmet never sees the rules.** The app compiles a requested wavelength to a channel, and the helmet receives
  the channel. So the helmet can only record a rules version that the upload states.
- The shipped defaults exist as six hand-kept copies (the `.npps` file, Rust, TypeScript, Kotlin, Swift, C#). Drift
  tests compare the `.npps` file with the TypeScript and Kotlin copies. No test was found that compares it with the
  Rust copy, which is the one the compiler falls back to.

## 3. Why "version" needs more than one thing

Two facts pull apart:

1. **NeurOne releases the shipped rules.** A person reading history wants a short name for them, as for a protocol
   (`1.0`, `1.1`).
2. **A user may edit the windows.** An edited set is not a NeurOne release and has no number NeurOne assigned. A
   counter cannot name it, and two users' edits must not share a name.

The protocol model already answers this with two things: a content hash that says *which content*, and a release
version that NeurOne assigns to what it ships. The proposal applies the same pair to the rules.

## 4. The proposed scheme

### 4.1 What a version identifies

**The identity of a rule set is the hash of the resolved set**: the shipped defaults with the user's per-channel
overrides merged in, which is the set the compiler actually uses. It covers, for each of the three channels, the
nominal, the minimum and the maximum, or the fact that the channel has no rule.

It leaves out the name, the level and the description. They do not change which channel delivers a wavelength, as a
protocol's name does not change its hash.

**The mapping algorithm is part of the identity** through the domain prefix (§5). If the nearest-nominal rule or the
tie-break ever changes, the prefix changes, so every hash changes.

**Nobody assigns the hash.** The shared core computes it (`CLAUDE.md` §22: Rust, once, in `common/npps-core`).

### 4.2 The release version

**NeurOne assigns a release version `major.minor` to each set it ships**, by the protocol rule (`NP-NPPS-REF-001`
§3, "Release version"): the set is at `1.0` until a release is done; an edit after a release moves it at least a
minor step, once per release; a version never goes down and is never reused.

- The version is written in the shipped file as a `version` field of the `wavelength_rules` block.
- **A register of releases** lists every released pair (version, hash), one row each, append-only. It lives with the
  core, so every platform has the same list. A check fails the build when the shipped file's hash is not the last
  row, or when a released row changes.
- The shipped defaults in force today would be release `1.0`, hash `fdedc5e8…14e3` (vector V1, §5.3).

### 4.3 Major and minor

Proposed rule, which the core can compute, as a floor (NeurOne may still call a release major):

- **Minor:** every wavelength the old set delivered is delivered by the new set on the same channel. The new set may
  accept more.
- **Major:** some wavelength the old set delivered is now refused, or is delivered on a different channel. A change of
  the mapping algorithm is major.
- **No change of version:** an edit to the name or the description only. The hash does not move.

Why this line: a minor change cannot alter any session that ran before it. A major change means a protocol that ran
yesterday is refused today, or drives a different emitter.

### 4.4 Shipped and edited sets are told apart by the hash

- A resolved set whose hash is in the register **is** that release, whatever its `level` says. A user who edits a
  window and then puts it back is on the release again.
- Any other set is a **user set**. It has no release version of its own. It is named by its hash, and it also states
  the release it was merged over (its base).
- The `level` field stays what it is: where the block came from. It is not evidence of what the set contains.

### 4.5 When the version changes

| Event | Hash | Release version |
|---|---|---|
| NeurOne changes a shipped window, nominal or channel | changes | moves at the next release (§4.2) |
| A user edits a window | changes | none (user set); the base is unchanged |
| A user restores the defaults | back to the release's hash | the release |
| A new release arrives under a user's overrides | changes when an untouched channel changed | the base moves to the new release |
| Name or description edited | unchanged | unchanged |

### 4.6 A protocol compiled under an older version

A protocol is not bound to a rule set: its hash excludes the rules (`SWR-HV-01`). What is bound is the **compiled
descriptor**. The rules were applied when it was compiled, so the descriptor states the rule set it was compiled
under, and that statement travels with it.

- **Each upload compiles under the rules in force at that moment.** There is no stored compilation to migrate.
- **A descriptor already on the helmet** (Mode 3) keeps running under the set it was compiled with, and each session
  records that set. This is true: the channels in the descriptor were chosen by that set.
- **The app compares.** When the rules in force differ from the set a stored descriptor states, the app says so. When
  the difference changes this protocol (one of its wavelengths now maps to another channel, or is refused), the app
  marks the stored descriptor out of date and offers a new upload. It never re-uploads silently.
- **Used protocols are untouched.** A protocol's identity and its immutability once used do not depend on the rules,
  so a rules release cannot change a used protocol or its history.

### 4.7 How the version is carried

| Place | Carries | Status |
|---|---|---|
| Compile request (`wavelengthRules`) | the resolved rules, as today | built |
| Compile result | the rules stamp of §4.8, computed by the core from the rules it used | proposed |
| Signed descriptor | the same stamp, under the signature, and outside the protocol's content hash | proposed |
| Session record (the UHDR fitting frame) | the stamp, copied by the hub from the descriptor it is running | proposed |
| SHDR, fleet upload, hub diagnostic log | **nothing** | see below |

**The stamp stays out of SHDR.** A user's edited windows are close to unique to that user, so their hash would be a
value that follows a person across devices. `SWR-HV-02` already places the rules version in UHDR, and `CLAUDE.md`
§5.1 says when in doubt, UHDR.

### 4.8 The stamp

Fixed length, so the record's shape says nothing about the rules (`CLAUDE.md` §5.1 rule 2):

| Bytes | Field | Values |
|---|---|---|
| 1 | origin | `0` not stated · `1` NeurOne release · `2` user set |
| 1 | release major | for origin 1 the release; for origin 2 the base; `0` for origin 0 |
| 1 | release minor | as above |
| 36 | the windows | three channels in the order 660, 808, 1064; each is nominal, minimum, maximum as unsigned 32-bit little-endian picometres; a channel with no rule is twelve zero bytes |

**The windows themselves are carried, and the hash is not.** Any reader computes the hash from the 36 bytes (§5).
The reason is the issue's fifth point: history must stay readable. A hash of a user set can be resolved only by an
app that still holds that set. Carrying the windows lets any reader say how 810 nm was mapped in a session from two
years ago, with no lookup and no dependence on a store that may be gone.

### 4.9 Stored rules today, and migration

- **Nothing stored needs rewriting.** Neither store holds a version, and under this scheme neither will: the identity
  is computed from the content each time it is needed.
- **One new constraint.** The canonical form (§5) holds whole picometres, so a rule value must be a multiple of
  0.001 nm. Every shipped and fixture value is. A stored user value that is not would be shown to the user as needing
  correction and would not be rounded for them, because a rounded window is not the window they set.
- The six copies of the defaults are a separate defect. The stamp makes it visible: a platform compiling under a
  drifted copy would report a user set.

### 4.10 Failure cases

| Case | Proposed behaviour |
|---|---|
| The descriptor states no stamp (an app built before this) | The hub records origin `0` and zero bytes. It does not invent a version. Whether it should refuse instead is Q6 |
| A stamp claims a release, and the windows do not hash to that release | The reader trusts the windows. It shows them as a user set and notes the mismatch. The hub does not check, because it holds no register |
| A reader meets a release it does not know (a record from a newer app) | It shows the release version as stated and the windows, and says the release is not known to this app. It does not guess |
| The rules are edited after a descriptor was signed | The descriptor stays valid and keeps its stamp. The app's comparison of §4.6 applies |
| Two devices or two app installs with different rules | The same protocol hash, and a different stamp in each session record. History shows both. Nothing synchronises rules between installs (Q7) |
| A rule value not on the picometre grid | The set has no canonical form. The compile is refused with the value named |
| A rule set the validator refuses | As today: a parse error, never a silent default. It is never stamped |

## 5. The canonical form, with vectors to check by hand

### 5.1 Form

The hashed bytes are the domain line, then one JSON object, UTF-8, with no whitespace and no trailing newline:

```
NeurOne.WavelengthRules.1\n{"channels":[C660,C808,C1064]}
```

- `\n` is one line-feed byte.
- The array always has three members, in the order `led_660`, `led_808`, `led_1064`, whatever order the rules were
  written in.
- Each member has four keys in sorted order: `element`, `max_pm`, `min_pm`, `nominal_pm`.
- A value is the wavelength in whole picometres, as a decimal integer with no sign, no leading zero, no point and no
  exponent. 808 nm is `808000`; 632.8 nm is `632800`.
- A channel with no rule keeps its `element` and has `null` for the other three.
- The digest is SHA-256, written as 64 lowercase hex digits.

This follows the protocol content hash (`identity.rs`): a domain prefix, sorted keys, no whitespace, SHA-256. It
differs in one way. It holds integers only, so the result does not depend on how a library prints a floating-point
number.

### 5.2 To check a vector

```
printf 'NeurOne.WavelengthRules.1\n<the JSON line>' | sha256sum
```

### 5.3 Vectors

**V1, the shipped defaults** (266 bytes):

```
{"channels":[{"element":"led_660","max_pm":680000,"min_pm":650000,"nominal_pm":660000},{"element":"led_808","max_pm":840000,"min_pm":798000,"nominal_pm":808000},{"element":"led_1064","max_pm":1074000,"min_pm":1054000,"nominal_pm":1064000}]}
```

SHA-256: `fdedc5e8a0fb39d53eb73bed210a6ae7a2d29db742e4ea5acde439e4291d14e3`
Windows, 36 bytes: `20120a00 10eb0900 40600a00 40540c00 302d0c00 40d10c00 403c1000 30151000 50631000`

**V2, the fixture `npps/fixtures/wavelength_rules.npps` ("Lab A", 808 nm maximum raised to 860) merged over V1** (266 bytes):

```
{"channels":[{"element":"led_660","max_pm":680000,"min_pm":650000,"nominal_pm":660000},{"element":"led_808","max_pm":860000,"min_pm":798000,"nominal_pm":808000},{"element":"led_1064","max_pm":1074000,"min_pm":1054000,"nominal_pm":1064000}]}
```

SHA-256: `114d2dc02374dfa0ba875539e60254993dd0a917871e7cf08c5efca7a8666e1d`

**V3, V1 with no rule for the 1064 nm channel** (257 bytes):

```
{"channels":[{"element":"led_660","max_pm":680000,"min_pm":650000,"nominal_pm":660000},{"element":"led_808","max_pm":840000,"min_pm":798000,"nominal_pm":808000},{"element":"led_1064","max_pm":null,"min_pm":null,"nominal_pm":null}]}
```

SHA-256: `157054921f38ece592ec4626e9c18f064bea3203ae4e7f8750c64c5859003908`

**V4, V1 with the 660 nm minimum lowered to 632.8 nm** (266 bytes):

```
{"channels":[{"element":"led_660","max_pm":680000,"min_pm":632800,"nominal_pm":660000},{"element":"led_808","max_pm":840000,"min_pm":798000,"nominal_pm":808000},{"element":"led_1064","max_pm":1074000,"min_pm":1054000,"nominal_pm":1064000}]}
```

SHA-256: `4f440bfef328cd768e5f615a6a58e7aba3df0755ca169f3289e2b0b6ba2fdfe4`

**V5, V1 with the 808 nm channel's nominal moved to 810 nm** (266 bytes):

```
{"channels":[{"element":"led_660","max_pm":680000,"min_pm":650000,"nominal_pm":660000},{"element":"led_808","max_pm":840000,"min_pm":798000,"nominal_pm":810000},{"element":"led_1064","max_pm":1074000,"min_pm":1054000,"nominal_pm":1064000}]}
```

SHA-256: `fb19267c2230b9381b55c523b805d48f670bf9a4cbec5aa657e48595483605cf`

**V6, V1 renamed, with a new description, `level: user`, and its channels written in the order 1064, 660, 808.** The
bytes are V1's, so the hash is V1's. This set is release `1.0`.

How the vectors were produced: a short script that builds each string as written above and hashes it, and V1 again
with the `printf` line of §5.2. No implementation exists in the repository to compare against.

### 5.4 The major and minor rule on the same vectors

| Change | Was delivered, now refused or moved? | Result under §4.3 |
|---|---|---|
| V1 → V2 | No. 840 to 860 nm is newly accepted on 808 | minor |
| V1 → V3 | Yes. 1054 to 1074 nm is now refused | major |
| V1 → V4 | No. 632.8 to 650 nm is newly accepted on 660 | minor |
| V1 → V5 | No. No windows overlap, so the nominal decides nothing | minor (the hash changes, the mapping does not) |
| V1 → V6 | The same set | no change |
| V2 → V1 | Yes. 840 to 860 nm is now refused | major |

## 6. Options

| | Version is | Cost | Answers "how was 810 nm mapped?" |
|---|---|---|---|
| **A** | A release counter `major.minor` only | Smallest record (2 bytes). Cannot name a user's edited set, so either editing is removed (it is in §7a) or every edited session records "unknown" | Only for shipped sets, and only with the register |
| **B** | A content hash only | 32 bytes. Names every set. No readable name, and a user set's hash means nothing once the store that held it is gone | Only while some app still holds the set |
| **C** | Release version plus hash | 35 bytes. As B with a readable name for shipped sets | As B |
| **D (recommended)** | Release version, origin, and the windows themselves; the hash is computed from the windows (§4.8) | 39 bytes, once per session. One descriptor field and one fitting-frame field | Always, from the record alone |

**Recommendation: D.** It is the only option under which history stays readable for a user's edited set, which is the
case A cannot name and B and C cannot explain later. Its extra cost over C is four bytes.

## 7. What it would take to build, once decided

Nothing below is started.

1. **Core** (`common/npps-core/src/wavelength.rs`): the canonical form, the hash, the vectors of §5.3 as tests, the
   major/minor comparison, the picometre check. `api.rs` returns the stamp with a compile.
2. **Core, compiler** (`compiler.rs`): the stamp in the descriptor, under the signature. A descriptor format change,
   so the hub's parser changes with it. The protocol content hash (`identity.rs`) does not change.
3. **Hub** (`np_session_fitting`): the stamp joins the fitting frame, with the format bump its header already
   promises.
4. **Language and grammar** (`NP-NPPS-REF-001` §7a, `NP-NPPS-GRAM-001`): a `version` field on the block.
5. **Register of releases** and its check; the first row is `1.0` = V1.
6. **Requirements** (`NP-SW-001` §6.3): `SWR-HV-02` gains the definition of "the version", and new rows cover the
   stamp in the descriptor, the stale-descriptor notice and the register. Each to `CLAUDE.md` §18's standard.
7. **Apps:** show the stamp in history, and the notice of §4.6. Every platform reaches the core for the hash.
8. **Unjustified-choices register** (`CLAUDE.md` §19): the 1 pm resolution has no derivation and needs a row when it
   enters a controlled document or code.

## 8. What the principal needs to answer

- **Q1. What the version is.** Option A, B, C or D of §6? *Recommended: D.*
- **Q2. Identity over the resolved set.** Is the identity the hash of the set in force (defaults merged with the
  user's overrides), leaving out name, level and description (§4.1)? *Recommended: yes.*
- **Q3. Release version.** Does NeurOne assign `major.minor` to shipped sets by the protocol rule, with the present
  defaults as `1.0` and an append-only register (§4.2)? *Recommended: yes.*
- **Q4. Major and minor.** Is the line of §4.3 right as a floor: major when a wavelength that was delivered is
  refused or moves channel, minor otherwise? Or does the case-by-case practice of `OI-UI-KMP-10` P5 stand alone?
  *Recommended: the §4.3 floor.*
- **Q5. Resolution.** Is a 0.001 nm grid acceptable for rule values (§4.9)? The alternative is to hash the shortest
  decimal form of each number, which needs no grid and makes the hash depend on a number-printing routine.
  *Recommended: the grid.*
- **Q6. No stamp.** When an upload states no stamp, does the helmet record "not stated" and run, or refuse the
  upload? `SWR-HV-02` says the record is written and does not say what to do when there is nothing to write.
  *Recommended: refuse, once every shipped app states one; record "not stated" until then.*
- **Q7. Two installs.** Do a user's edited rules stay on the install that made them (today's behaviour), or travel
  with the user (`OI-UI-KMP-11`)? *No recommendation; the scheme works either way, because each session records its
  own windows.*
- **Q8. Where the stamp may go.** Is it confirmed that the stamp is UHDR only and never enters SHDR or a fleet upload
  (§4.7)? *Recommended: yes.*

**Conditions that would reopen this:** a fourth emitter channel (the form fixes three, so the domain line would
change); a rule kind that is not a window (for example a per-wavelength dose correction); a decision that users may
not edit the windows, which would make option A sufficient.

## 9. What to send back

For each of Q1 to Q8: the ruling, a sentence of rationale, and the date. The answer is then recorded in
`docs/status/pending-decisions.md` (`OI-UI-KMP-10`) and `docs/status/completed-decisions.md`, the scheme text moves
into `NP-NPPS-REF-001` §7a and `NP-SW-001` §6.3, and the build of §7 starts.
