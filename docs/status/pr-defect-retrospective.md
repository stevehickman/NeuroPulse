# PR defect retrospective — errors made, caught, and corrected

**Status:** Process record. Not a controlled document — no serial, no revision letter.
**Scope reviewed:** 240 pull requests (#1–#301, with gaps) `[census]`, 199 merged via merge commit
`[census]`, 736 commits on `main` as of `7bf6464^` `[census, pinned]`.
**Date:** 2026-08-30 · provenance pass 2026-08-31

---

## 0. Method, and what this record can and cannot see

### 0.1 How to read the provenance tags

§2.4 argues that a number which constrains a decision must carry its evidence class, because
#273 eliminated a hardware candidate on a range whose disclaimer had been lost in transit. That
rule applies to this document's own prose, and it was not applied when the document was written
— which is how the review-thread claim corrected below got in.

Load-bearing claims now carry one of four tags. Unmarked prose is argument, not evidence.

| Tag | Means |
|---|---|
| `[census]` | Every item was enumerated. The number is exact, and the command that produced it is reproducible from the tree. |
| `[sample n]` | n items were checked and are named. Says nothing about the ones that were not. |
| `[PR body]` | The PR's own description says so. Not independently verified against the diff. |
| `[code]` | Verified by reading or running the code as it stands today. |

A count tagged `[census]` is pinned to a revision where it would otherwise drift: "736 commits"
was exact when written and is 743 now, because this document's own branch added seven.

### 0.2 Sources

Three sources were used, in decreasing order of reliability:

1. **Follow-up PRs that correct already-merged work.** The strongest evidence: an error
   that reached `main` and had to be undone by a later PR. These are unambiguous.
2. **Fix-up commits inside a PR branch.** An error caught between opening and merging.
   **86 of 199 merged PRs (43%) needed more than one commit** `[census]` — counted over
   merge commits matching `Merge pull request #`. Counting *all* 212 merges instead gives
   99 of 212; that figure includes plain branch merges and is the wrong denominator.
3. **PR bodies that name the defect explicitly.** Sixteen commits apply enumerated
   review findings `[census]`. Nine PR bodies — #112 (6), #113 (12), #124 (14), #126 (19),
   #144 (8), #147 (10), #149 (7), #173 (6), #174 (4) — enumerate **86 individually numbered
   findings** between them `[PR body, sample 9]`.

   > **Correction (2026-08-31).** This read "roughly 100" and was not arithmetic — the nine
   > PRs it names sum to 86. The figure is now the sum, with the per-PR counts shown so it can
   > be checked. It counts only these nine bodies, so it is a floor for the corpus, not an
   > estimate of it.

**What this record cannot see.** Inline review threads are close to absent: `get_review_comments`
returned empty for **all 10 PRs sampled** (#3, #126, #127, #159, #174, #204, #205, #249, #250,
#272). Review in this project happened overwhelmingly in conversation or as a self-review pass,
leaving a trace only where a commit or PR body recorded it. So the counts below are a **floor**,
and errors corrected silently inside a single squashed commit are invisible here.

> **Correction (2026-08-31).** An earlier revision of this paragraph claimed *zero* inline review
> threads across *all 240* PRs. That was generalised from two samples, using `get_review_comments`,
> which returns threads and not reviews — so it answered a narrower question than the one asked of
> it. **It is false.** `get_reviews` on #204 returns a review from `github-advanced-security[bot]`,
> and commit `345b188` — *"Potential fix for pull request finding 'CodeQL / Workflow does not
> contain permissions'"* — exists only because that bot raised a finding. So automated review did
> occur, did find something, and was acted on. The claim is now stated as what was measured, with
> the sample named. This is §2.4's rule turned on this document's own prose, and the error it
> corrects is a Class 2 instance committed by the file that catalogues Class 2.

One further limit worth stating plainly: this is a self-assessment of my own output, drawn
from records I also wrote. The PR bodies are unusually candid — several volunteer their own
errors under headings like "corrections made to this work in-flight" — but they are not an
independent audit, and a class of error I was systematically blind to would not appear in
them.

---

## 1. The error catalogue

Ten classes, ordered by how much damage the class did rather than how often it occurred.

**Provenance for this whole section:** every row is `[PR body]` unless tagged otherwise — the
defect is described as its own PR described it, not re-derived from the diff. Four are stronger
because they were re-verified against the tree while writing the prevention measures in §2, and
are tagged `[code]` in place: #118's duplicated parser, the §6.0 teardown invariant behind #277,
#272's fixed-shape marshaller, and #299's uncovered `scripts/`. The rest inherit the candour of
the bodies, which §0 already notes is not independent audit.

### Class 1 — Gates and tests that passed without testing anything

The most serious class, because every instance is a **guard that reported green while
guarding nothing**. Five reached `main`.

| # | What happened | Caught by |
|---|---|---|
| **#118** `[code]` | `skip_re` in `ci/test_shdr_schema.py` had an unanchored alternation branch and the call site used `.search()`. Every column line containing `REFERENCES`/`UNIQUE`/`PRIMARY KEY` as an *inline* modifier was silently dropped before `col_re` saw it. All 15 `warranty_token` columns in the fleet schema are declared that way, so `check_warranty_token_type()` iterated an empty list. **TOKEN-01 passed vacuously regardless of what type the column was declared as.** | later PR |
| **#174 (1)** | The regression test for the charge-density latch rebuilt a `fresh_state()` after calling reset — "which is exactly why it never caught this." The test was structurally incapable of observing the bug it existed to catch. | code review |
| **#201** | The OI-EMMC2-07 no-raw-accelerometer gate is a *negative* gate: it passes when nothing prohibited is present. It had never been proven to fail on bad input, and had no guard against the parser silently ceasing to see `shdr_accel_records` at all. Both silent-failure modes would let health-inferrable motion data into the fleet DB with CI green. | later PR |
| **#299** `[code]` | `web-ci.yml` runs `tsc` with `working-directory: app/web`, whose tsconfig is `"include": ["src"]`. **`scripts/` had never been type-checked by anything.** Two of the 66 uncovered files were themselves guards — `check-doc-filenames.ts` enforces NP-CONV-001 §4.0 across the whole document set and nothing ran it. | later PR |
| **#238** | The HD01 test stimulus was a DC constant, which carries no variance — the covariance path under test could not be exercised by its own input. | in-PR |
| **#289** | `NEW_PROTOCOL_TEMPLATE`, whose doc comment calls it "a minimal valid NPPS template", had two hard parse errors. It survived Rev 12 because **the template has no test coverage** — it is referenced only as `initialText`, so nothing parses it. Every Android user tapping "new protocol" since Rev 12 got a template that fails against the app's own parser. | later PR |

Two instances show the correct behaviour and are the model for the fix in §2:

- **#299** wrote the naive gate first (`node --check <path>`), **measured it, and falsified it** —
  on Node 24 it exits 0 on a file with `function broken( {` appended, because Node parses a
  path argument as CommonJS. The shipped gate reads source on stdin with an explicit
  `--input-type`.
- **#272** pinned `count`/`slot` to *positive* expected values in both arms of its central
  assertion, explicitly so that "a do-nothing marshaller fails. A bare 'the two are equal'
  assertion would have passed on an empty implementation."

### Class 2 — Numbers asserted without a source; estimates hardened into constraints

| # | What happened |
|---|---|
| **#273** | `§13.2d` ruled out the **only in-window NIR candidate** on one electrical ground: a "~1.6–2.2 V" architecture assumption. Traced to source, **it has none.** The range was welded from two different columns *and two different channels* of one estimate table — `1.6` is the NIR channel's lower bound, `2.2` is the 660 nm channel's upper bound — and both owning documents disclaim the values in their own words ("confirm from datasheet", "All estimates"). The real constraint was a **15 V rail** that D-6/OI-HUB-C17b had already replaced with 24 V. A decision was taken against a fabricated constraint anchored on retired hardware. |
| **#273** | Same PR, three more: the candidate's centroid is **810 nm** per the manufacturer's own spec, not the "820nm" carried throughout the document set; `Vf=3.55V @1A` was not a datasheet value; and "2000mA DC (11× margin)" is the **surge** rating (2 A at t_p ≤ 200 µs, D = 0.005), not a DC rating. The PR notes this is the "same error class already caught once here" — GH CSSRM5.24's "700 mA" binning current. |
| **#273** | Caught *in-flight* by an advisor pass: a **back-solved** `V_f = 3.2` (= 22.4 ÷ 7) was being presented as a datasheet finding. Discarded. |
| **CLAUDE.md Rev 36** | `drop_detected` and `maintenance_alert` are computed from `15.0f` g and "3 drop-bearing gaps" — both guessed before any hardware existed, while §5.1 simultaneously prohibited collecting the evidence that could validate them. The spec foreclosed the evidence needed to make itself correct. |
| **#223** | Two live marketing claims ("50× more transcranial LEDs than Vielight", "300 LEDs per wavelength") depended on a retired fixed LED count. Correctly **flagged NEEDS RECOMPUTE rather than silently rewritten or guessed at** — the right handling, recorded here as the positive case. |

### Class 3 — Renames and relocations, and sweeps that did not sweep

| # | What happened |
|---|---|
| **#249** | CLAUDE.md Rev 33 relocated §7–§15 and left **every inbound pointer intact**. 100 citations — IEC 62304 headers, the DHF, design specs — silently stopped resolving, and nothing caught it for a whole revision. **The brief's own grep reported 94.** It missed one citation in `docs/ABBREVIATIONS.md` that wrote the section sign, a space, then the number — which `§[0-9]` cannot match — and **five Kotlin files, because every grep enumerated `.md/.c/.h/.ts/.tsx/.swift/.js/.npps` and stopped.** A hand-written extension list was wrong, and the count derived from it was wrong. |
| **#289** | Rev 13 §(iii) swept §11's grammar summary for retired `zones:` selectors. **Three instances survived the sweep** — including the *teaching* example for a syntax rule, which illustrated it with a value the parser rejects, and the complete worked protocol, so anyone copying it got a file that would not load. |
| **#223, #224, #227, #230, #236** | Retired 5-zone architecture references surviving across docs, firmware, iOS, `editscripts/` and the simulator, corrected over five PRs across five weeks. |
| **#154** | `np_signature.c` cited NP-FW-EMMC-001 §6.2 (UHDR encryption) for SHA-256 image integrity; the correct section is §8.2 (bootloader OTA). Plus "4 more stale section references" in a follow-up commit to the same PR — the first sweep was incomplete. |
| **#293** | The R-5 citation pointed at §5 (photodiodes) instead of §2 (requirements). |
| **#216** | `com.neurone` → `life.neurone`, then a second commit for segmented reverse-DNS in the `sync-conditions.ts` output path. The first fix was incomplete. |
| **#150, #273** | ID hygiene: a duplicate `OI-CHARGE-01`; and a first attempt in #273 **reused closed IDs OI-HEXTILE-16/17 and clobbered their retained struck-through records**, which NP-CONV-001 §6 forbids. Reverted and renumbered. |

### Class 4 — Errors of privacy or safety *reasoning*, where the code did what it said

These are the ones no linter reaches. Each was a defensible-looking design that was wrong
on analysis.

| # | What happened |
|---|---|
| **#272** `[code]` | `tick_ms` was suppressed to `0` **only** for `NP_SAFETY_STATUS_CARDIAC`, while `count` was reported unconditionally by an independent accessor. The observable pair `count > 0 && tick_ms == 0` was a **one-bit cardiac oracle, readable with certainty and requiring no correlation work** — *strictly worse than not redacting at all*, because a raw `tick_ms` is a relative SysTick value meaningless without a session record SHDR structurally does not hold, whereas the redaction pattern is self-interpreting. Two further findings: the suppression **never protected the predicate** (the hub already publishes `CVNS_HR_CUTOFF` to SHDR deliberately), and fault timing was **already** UHDR unconditionally. The stated rationale was void the day it was written. |
| **#277** `[code]` | `withdrawBlanketResearchConsent()` was correct and tested **on both platforms** — and **nothing on iOS called it from the UI.** Turning blanket consent off in Research Preferences committed through `updateResearchConsent()` and skipped the analytics teardown; Android routed it correctly. A tested method with no caller. |
| **#174 (3)** | `UHDRKeyManager.saltFromKeychain()` ignored the `SecItemAdd` result. A failed persist still returned the ephemeral salt; the next launch generated a different salt → different Argon2id key → **all prior EEG/HRV records permanently undecryptable.** |
| **#174 (1,2)** | Two safety-MCU latches that never cleared: a per-session charge cutoff left `CHARGE` set in persistent state forever, and thermal cutoffs were edge-triggered with no cooling recovery. Both disabled *all* stimulation until power-cycle. Both fail safe, both broke stated re-arm behaviour. |
| **#174 (4)** | `hubCompiler.compileProtocol()` silently dropped commands past `PROTO_CMD_MAX` — entire later modalities vanished from a *signed* session. The overflow `throw` was dead code. |
| **#155, #202, #196** | UHDR privacy bug in watchOS/shared code; a fault-slot collision in firmware; `consumable_counts` with no one-row-per-device constraint; SHDR timestamps not minimized to the largest needed granularity. |

### Class 5 — Platform and environment facts assumed rather than verified

| # | What happened |
|---|---|
| **#192** | The entire watchOS Phase 3 haptic feature was specified and written against **Core Haptics, which does not exist on watchOS.** It had been wrapped in `#if canImport(CoreHaptics)` and reduced to a no-op stub — so the feature was silently non-functional while appearing implemented, and two open items (OI-WA-01, OI-WA-04) existed to characterise an API that was never there. |
| **#237** | `loadLocales()` iterated `readdirSync()` directly and `Map` iteration is insertion order, so the generator's output depended on **filesystem directory order**. It passed on macOS/APFS and failed on the Linux runner for byte-identical inputs. **The generator was never a pure function of its inputs**; the committed catalogue simply had one machine's directory order baked into it. |
| **#299** | `node --check <path>` does not do what it appears to on Node 24. |
| **#127, #159** | Xcode/simulator/runner availability on GitHub-hosted macOS runners — see Class 7. |

### Class 6 — A destructive script shipped as the documented workflow

**#237** stands alone. On `origin/main`, running the documented `bun scripts/sync-locales.ts`
rewrote the committed iOS String Catalog with **459 insertions and 2,305 deletions**:

- **26 keys existed only in the committed catalogue, so regenerating deleted them.** 18 were
  live, referenced via `String(localized:)` from six views — deleting them breaks that UI in
  all 11 languages.
- **Two live strings would have *regressed* to describing retired hardware** — telling users
  to listen for a bone-conduction tone the firmware no longer emits, and physically cannot
  deliver (seating a module requires the helmet off the head).
- Every `en` entry flipped `translated` → `new` on each run, because canonical had nowhere
  to record state.
- **The script had no `--check` flag and silently *wrote* files when passed one.**

### Class 7 — Unbounded trial-and-error against a remote runner

**#127 needed 15 commits; 13 of them were consecutive guesses at the iOS simulator
destination** (`macos-15` → `generic/platform` → `iPhone 16 / 18.0` → `iPhone 15 / 17.5` →
`OS=latest` → drop `OS=latest` → `OS=26.2` → …). **#159 needed 13 commits, 12 of them fixes**,
six on the same problem. **#128** added seven, four of them CI config. Across the three PRs,
**24 of 35 commits match a CI-configuration pattern** — 14 · 6 · 4 `[census, pattern-matched]`,
grepping subjects for `simulator|xcodebuild|destination|OS=|runner|macos-1|downloadPlatform|Xcode`.
No local reproduction, no hypothesis, each push a several-minute round trip. Nothing was learned
that a single `xcodebuild -showdestinations` or a runner-image README would not have given.

> **Correction (2026-08-31).** This said "roughly 19 commits", which was a hand count, not a
> measurement. The pattern-matched figure is 24. The matcher is deliberately generous — it will
> catch a legitimate CI commit alongside the guesses — so 24 bounds the behaviour from above and
> "how many were *pure* guesswork" stays a judgement, which is why it is no longer asserted as a
> number.

### Class 8 — Build-time caches drifting from the source of truth

| # | What happened |
|---|---|
| **#285, #287** | The simulator, iOS, Android and Windows each shipped a build-time cache of protocol content — `protocols.generated.js` alone was 2,931 lines — so editing a canonical `.npps` did not change what users saw. This violated NP-NPPS-REF-001 §1.6 and had to be undone across 53 files. |
| **#239** | The hex-tile cluster count was carried forward as a constant instead of derived. |
| **#242** | Electrode→driver channel was resolved from **two tables** that could disagree, instead of one accessor. |

### Class 9 — Algorithms that looked right and were not

**#244** computed sLORETA band power from **bin-count ratios rather than a real spectrum**.
**#243** blended the covariance **once per sample instead of once per epoch**. **#240** failed
to enforce electrode distinctness across both rings of the bilateral montage. **#245** found
that peak selection was not a real argmax. **#90/#155** found NPPS lexer bugs on
digit-prefixed identifiers and on `660_808nm` tokenization. Each of these is a plausible
implementation that produces plausible-looking output — none would fail a smoke test.

### Class 10 — Branch hygiene

**#220** needed "Remove stray file accidentally committed to this branch" *and* "Remove
unrelated files accidentally carried onto this branch". **#156** needed "Remove duplicate
files created during rebase". **#212** needed a post-merge breakage fix.

---

## 2. What actually prevents these

Ordered by expected value. Items marked **[gate]** are mechanizable and should be CI jobs;
items marked **[rule]** belong in `CLAUDE.md` or `NP-CONV-001` because no gate can catch them.

### 2.1 Every gate ships with a proof that it fails **[gate]** — IMPLEMENTED

> **Status 2026-08-30: enforced by `scripts/check-gate-coverage.ts`.** Every check script in
> `scripts/` and `ci/` now declares `CI-Kind: gate | report | self-test`; every gate declares a
> `CI-Self-Test` that a workflow runs before the gate, and a `CI-Scans` naming its population.
> 13 scripts — 8 gates, 3 reports, 2 self-tests. The rule below is the reasoning; the script is
> the enforcement.

Class 1 is the highest-value target because a broken guard is worse than no guard — it
converts an unknown risk into a false assurance, and #118 held that false assurance across
every merge until it was found.

The project has already invented the fix three times independently, ad hoc:
`ci/test_shdr_schema_selftest.py` (#201), the pre-fix-tree run in #249, the mutation test
in #272, the falsified `node --check` in #299. **Make it the standing rule rather than a
good habit.**

Concretely:

- A new `check-*` script or CI gate does not merge without a companion falsification that
  **fails on a mutated input and is run in CI alongside the gate.**
- A `ci/selftest-coverage` job enumerates every `scripts/check-*.ts`, `scripts/check-*.sh`
  and `ci/test_*.py` and fails if any has no paired self-test. This is the gate that
  would have caught the fact that `check-doc-filenames.ts` was never executed (#299).
- **Retrofit first to the gates that have never been falsified**, since #118 shows the
  failure is silent and indefinite.

### 2.2 Every negative gate asserts it scanned something **[gate]** — IMPLEMENTED

> **Status 2026-08-30: enforced, and the count is checked rather than claimed.**
>
> Every gate emits a machine-readable `scanned: <int> …` line on both its PASS and FAIL paths, and
> `check-gate-coverage.ts` **runs each gate and reads that number back**. A gate that prints no
> such line, or that reports `scanned: 0` while exiting 0 — the #118 shape exactly — fails the
> meta-gate. A gate that genuinely cannot run in the checking environment declares
> `CI-Scan-Probe: external — <reason>`; the reason is mandatory, and three do (a live PostgreSQL,
> CI-generated relevance lists, and the prober itself, which would otherwise re-enter).
>
> That is what separates this from a documentation field: `CI-Scans` is prose and can say
> anything, but the probed integer is what the gate actually computed.
>
> Two in-gate vacuity guards were added where the checks iterate a parsed population:
> `ci/test_warranty_nojoin.py` (`VACUITY-01`/`VACUITY-02` — no columns, no tables, or no
> `warranty_token` visible) and `ci/test_shdr_schema.py` (`VACUITY-01`, now 15 checks). The
> warranty one is falsified against the real defect: reintroducing #118's parser bug takes the gate
> from PASS to FAIL (`227 → 88` columns, `23 → 0` tokens) where it previously stayed green.
>
> Current populations: 110 documents · 740 files and 645 citations · 13 check scripts · 10
> simulator files · 248 and 227 schema columns.

A gate that passes when nothing prohibited is present must additionally assert that its
parser saw the expected population — a **nonzero, named count**. `check-section-refs.ts`
already does this ("refusing to pass vacuously"); `test_shdr_schema.py` did not, and that is
precisely the difference between #118 and #201.

Cheap universal form: every gate prints `checked N items` and fails if `N` is zero or drops
below a committed floor. #118 would have surfaced immediately as `checked 0 warranty_token
columns`.

### 2.3 Test inputs must be capable of producing failure **[rule]**

Three distinct instances (#174's `fresh_state()`, #238's DC constant, #272's do-nothing
marshaller) share one root: the test was written to demonstrate the happy path, and its
setup destroyed the very condition under test.

The check is one question at review time: **"if I delete the implementation, does this test
fail?"** — and for a differential assertion, **"could an empty implementation satisfy this?"**
#272's practice of pinning both arms to *positive* expected values is the general answer and
should be the documented pattern.

### 2.4 A provenance tag on every number that constrains a decision **[rule]**

Class 2 is where the most expensive error lives: **#273 shows a design candidate eliminated
on a constraint that did not exist.** The mechanism was not carelessness — it was a
plausible range assembled from real numbers in a real table, which then survived because
nothing in the format distinguished a datasheet value from a design target from a guess.

The rule that would have stopped it, in three parts:

1. **Every numeric constraint carries its evidence class inline** — one of
   `[datasheet]`, `[measured]`, `[design-target]`, `[estimate]`, `[assumed]` — plus the
   document and section it came from. `NP-PROC-FPC-001` §2.3 *did* say "confirm from
   datasheet"; the disclaimer was simply lost in transit.
2. **An estimate may never be widened into an architectural constraint.** A range spanning
   two different channels of one estimate table is not a constraint; it is an artifact of
   how the table was read.
3. **Before a candidate is eliminated on a numeric ground, that number is re-traced to
   source.** #273's whole finding is what one trace produced.

A partial **[gate]** exists: flag numeric values in `docs/status/pending-decisions.md`
decision rows that carry no evidence-class tag. It cannot judge correctness, but it forces
the disclaimer to travel with the number.

### 2.5 Renames and relocations follow a fixed protocol **[gate]** + **[rule]**

#249 is simultaneously the largest instance of Class 3 and the model for fixing it. Three
rules, all demonstrated there:

1. **Derive the file set from `git ls-files`, never from a hand-written extension glob.**
   The hand-written list is what lost five Kotlin files and produced a count that was
   wrong by six.
2. **Prove the count on the pre-fix tree.** Running the finished guard against the base
   commit reported exactly 100 — that tests *recall*. Injecting one bad reference only
   tests precision, "which a much weaker checker would also pass."
3. **Leave a guard behind, deriving its valid set from the source document at runtime.**
   `check-section-refs.ts` re-derives valid sections from CLAUDE.md's own headings on every
   run — "deliberately NOT a hardcoded list, which would rot exactly the way the references
   it guards did."

The gap #289 exposes: **§-reference integrity is guarded, but example code in documentation
is not.** Add a gate that parses every complete `.npps` block in `NP-NPPS-REF-001` and every
`*_TEMPLATE` constant in app code with the real parser. #289 ran exactly this sweep by hand;
make it permanent.

### 2.6 Generators are pure, and every generator has `--check` in CI **[gate]**

From #237, three properties, each of which failed there:

- **Deterministic:** sort every directory read; never let `readdir` order reach the output.
- **`--check` writes nothing** and exits non-zero listing stale files. #237's script silently
  *wrote* when passed `--check`, which is worse than not having the flag.
- **CI runs `--check` on every PR touching the inputs**, with `paths:` triggers deliberately
  wider than strictly needed — per #249, "a guard that can't see the edit that breaks it
  isn't a guard."

Add a fourth, from the substance of #237: **when generated output and canonical input
disagree, determine the direction before regenerating.** The near-miss there was that two
strings were correct in the *generated* file and stale in canonical — regenerating would
have propagated a description of hardware that cannot work. A `--check` failure means
"these disagree", never "canonical wins".

And the structural lesson: state that regeneration cannot preserve — translation status in
#237 — must be **derived from canonical content**, not stored beside it. Anything stored only
in a generated file will be destroyed.

### 2.7 No build-time cache of content that has a canonical source **[gate]**

NP-NPPS-REF-001 §1.6 now states the rule; #287 enforced it across four platforms. Keep it
enforced: a gate asserting no `*.generated.*` file contains protocol content (zones,
conditions, protocols), and that generated files carry lattice/structure only.

The same principle covers #239 and #242 — **derive, don't carry forward; one accessor, not
two tables.** Two tables that can disagree will.

### 2.8 Verify platform and environment facts before specifying against them **[rule]**

#192 is cheap to prevent and expensive to miss: an API's availability on the target platform
is checked *before* a spec is written against it, and a `#if canImport(...)` that reduces a
feature to a no-op stub is **a defect to escalate, not a portability accommodation**. The
stub is what let a non-functional feature look implemented for a whole phase.

Generalize: when a build guard silences a feature rather than a warning, that is a finding.

### 2.9 Bound CI trial-and-error explicitly **[rule]**

Class 7 cost ~19 commits and produced no durable knowledge. The rule:

- **Two failed remote attempts at the same configuration problem, then stop guessing** and
  go read the runner image manifest, the tool's own `-showdestinations`/`--help`, or the
  vendor docs.
- Prefer a local or containerized reproduction. Where genuinely impossible — #289 could not
  resolve the Android Gradle plugin offline, #237 could not run SwiftLint locally — **say so
  in the PR body and name the proxy used.** Both of those PRs did exactly this, and that is
  the standard: an unverifiable claim is labelled, not quietly asserted.

### 2.10 Two review questions that no tool will ever ask **[rule]** — BOTH MECHANIZED

> **Status 2026-08-30.** The heading is now wrong in a useful way: both questions turned out to
> have a checkable structural core, and both are enforced.
>
> **Question 2 → `scripts/check-consent-reachability.ts`.** The two `ConsentStore` surfaces must be
> identical (11 methods each), and every method carries a declared reachability that holds —
> `ui`, `internal`, `superseded-by`, or `pending`. Deleting the iOS caller of
> `withdrawBlanketResearchConsent` reproduces the Rev 37 defect and the gate names it. Waivers are
> per-platform, must name an open item, are printed on every run, and become violations once a
> caller exists, so they cannot outlive the gap. It raised **OI-CONSENT-01/02/03** on first use —
> Android has no consent dashboard, so five methods are unreachable there.
>
> **Question 1 → `scripts/check-redaction-shape.ts`.** Every `NP_SAFETY_STATUS_*` bit is classified
> sensitive or device-condition (an unclassified bit is a violation, so adding one forces §5.1's
> defining test to be applied), and no declared SHDR reporting path may reference a sensitive
> predicate at all, comments and string literals stripped. Reintroducing #272's conditional
> redaction into `np_fault_latch_build_report()` turns it red.
>
> **The reach is narrower than the rule.** The redaction gate reads function bodies, not the call
> graph, so a reporting path that delegated to a branching helper would pass. That holds today only
> because #272 made the declared paths flat marshallers by design. Branching on a sensitive
> predicate in a *control* path remains required — the cardiac interlock must act on it — so only
> reporting paths are listed. Neither gate replaces the review question; each removes the cases a
> reviewer should never have had to catch by eye.

### The original rule

Class 4 is the class that matters most for this product and the least automatable. Two
questions, each earned from a specific defect, belong on every privacy- or safety-touching
review:

1. **"Is any field's presence, value, or shape conditional on a sensitive predicate?"**
   CLAUDE.md §5.1 now carries the general rule from #272 — *a redaction applied conditionally
   on a sensitive predicate leaks that predicate.* The structural fix generalizes too: #272
   replaced two independent accessors with **one fixed-shape marshaller**, because "a
   compile-time-fixed struct cannot have a field present for one fault kind and absent for
   another." Prefer a fixed shape over a correct branch.

2. **"Does this privileged method have a caller on every platform that exposes it?"**
   #277 is the whole argument: correct, tested, on both platforms, and unreachable from the
   iOS UI. Tests proved the method worked; nothing proved it ran. This is mechanizable
   **[gate]** for the consent surface specifically — enumerate the exported
   consent/teardown methods and assert each has a UI call site on each platform — and it is
   worth doing, because the failure is invisible in both the test suite and the diff.

A third, from #174: **a swallowed return value on a persistence call is a data-loss bug until
proven otherwise.** `SecItemAdd`'s ignored status would have irreversibly destroyed every
user's health record.

### 2.11 Branch hygiene **[gate]**

Class 10 is trivial and trivially preventable: review `git status` and the full file list of
the diff before the first push. A `changed-files` summary in the PR template, or a gate that
fails when a PR touches files outside a declared scope, closes it.

---

## 3. The three changes worth making first

If only three things are done, these three account for most of the damage above:

1. **§2.1 + §2.2 — falsify every gate, and make every gate report what it scanned.**
   Class 1 is the only class where the error *hides other errors*. #118 alone invalidated a
   privacy gate on the fleet database for an unknown number of merges, and #299 found that
   the document-set filename guard had never once run.

2. **§2.4 — provenance tags on constraining numbers.**
   #273 eliminated the only in-window candidate on a constraint that did not exist, anchored
   on a rail that had already been replaced. This is the class with the highest cost per
   instance, and the tag is nearly free.

3. **§2.10 — the two review questions.**
   #272 and #277 are both cases where the code did exactly what it said and the design was
   still wrong. No linter, type checker or test suite in this repository would have found
   either. They were found by someone asking what the observable behaviour *implies*.

---

## 4. What the record shows working

Recorded so the practices are kept rather than rediscovered:

- **#272** — falsified its own test by reintroducing the retired conditional and confirming
  the assertion fails, then reverting to green.
- **#249** — proved recall by running the finished guard against the pre-fix tree; deviated
  from its own brief in three places *because following it literally would have introduced an
  error*, and said so.
- **#273** — disclosed three corrections made to its own work in flight, including a
  back-solved number it had nearly presented as a datasheet finding.
- **#223** — flagged two live marketing claims as NEEDS RECOMPUTE rather than guessing new
  numbers, and separated genuinely stale documentation from documentation that correctly
  described current firmware.
- **#289, #237** — named the verification they could *not* run, and the proxy used instead.

The common thread is that each states the strength of its own evidence. That is the habit
worth generalizing, and §2.4 is the attempt to make it structural rather than optional.
