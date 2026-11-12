# The ruleset on `main`: what it holds today, and five settings nobody has decided

**Status:** DRAFT, not sent. **From:** a Claude Code session working on GitHub #446. **To:** the principal
(repository admin; interim Quality authority). Open item: `OI-SWCI-54` (`docs/np_sw_ci_001.md` §7).

**Nothing in this file has been applied.** The session that wrote it read the settings and changed none. Every
step in §5 is for the principal to run, or not.

## 1. The questions

GitHub #446 asked for branch protection on `main` and exact required-check contexts. **Both are already in
force** (§2). What is left is five settings that are live on GitHub and that no record decided, or that a record
decided before something changed. Pick one option for each:

| # | Question | Options |
|---|----------|---------|
| Q1 | The GitHub App that assigns revision numbers bypasses the ruleset in mode **Always**. It can push anything to `main` with no pull request and no check. Keep it? | **A.** Keep, and accept it (§3.1). **B.** Keep, and add an audit of what it pushed (§3.1). **C.** Remove the bypass; the App opens a pull request instead (§3.1) |
| Q2 | Which more checks should be required to merge? Ten are required today, all firmware and simulator. No privacy gate, no document gate and no app build is. | **A.** None; leave the ten. **B.** Add the two SHDR privacy gates and the redaction gate (§3.2, set P). **C.** Add set P and the repository-wide guards (§3.2, set G). **D.** Another list, named by the principal |
| Q3 | `require_extra_approval_for_unattributed_changes` is `true` on the pull-request rule. The settings page set it; `NP-SW-CI-001` §6.13.5 recorded that nothing decided it. Keep it? | **A.** Keep `true`. **B.** Set `false` |
| Q4 | A `copilot_code_review` rule is in force on `main` (`review_on_push: false`, `review_draft_pull_requests: true`). No record mentions it. Keep it? | **A.** Keep. **B.** Remove |
| Q5 | Should a pull request have to be up to date with `main` before it merges (`strict_required_status_checks_policy`, `false` today)? This is `OI-SWCI-53` in settings form. | **A.** Leave `false`. **B.** Set `true` |

**Recommendations, each with its reason, are in §3. They are recommendations and nothing acts on them.**

## 2. What is known

Read from GitHub on 2026-11-11 with `gh api repos/stevehickman/NeuroPulse/rulesets/16412379` and
`gh api repos/stevehickman/NeuroPulse/rules/branches/main`. The session's token could see the bypass list.

| Setting | Value on GitHub | Decided by |
|---------|-----------------|------------|
| Ruleset | `Safety` (`16412379`), `active`, targets `~DEFAULT_BRANCH`, no exclusion, last edited 2026-11-09 | `NP-SW-CI-001` §6.7.8 |
| `deletion`, `non_fast_forward` | in force | before `NP-SW-CI-001` |
| `pull_request` | in force: 0 approvals, no code-owner review, no last-push approval, no thread resolution, merge + squash + rebase | `NP-SW-CI-001` §6.13.1 |
| `pull_request.require_extra_approval_for_unattributed_changes` | `true` | **no record** (Q3) |
| `required_status_checks` | 10 contexts, each bound to App `15368` (GitHub Actions); `strict` is `false` | `NP-SW-CI-001` §6.7.8, §6.12, §6.13; the two simulator contexts by `ci/required-checks.txt` |
| `copilot_code_review` | in force | **no record** (Q4) |
| Bypass: `RepositoryRole 5` (admin) | `pull_request` mode | `NP-SW-CI-001` §6.13.1 |
| Bypass: `Integration 5585393` | `always` | `docs/revisions/README.md` records that the App `RevNumberFixer` was put on the bypass list (2026-11-02). **Not confirmed:** that id `5585393` is that App. This session could not read the Apps endpoint. The mode was not recorded (Q1) |
| Classic branch protection | not read: `GET /branches/main/protection` returned 403 to this session. It was 404 (none) on 2026-08-10 (`NP-SW-CI-001` §7, `OI-SWCI-22`) | — |

The ten required contexts, and the job that reports each. They equal `ci/required-checks.txt` exactly:
`bun scripts/check-main-ruleset.ts --live` passed on 2026-11-11.

| Context (exact string) | Workflow : job |
|---|---|
| `Class B scope` | `firmware-cross-build.yml` : `changes` |
| `CMake host tests (Class B)` | `firmware-cross-build.yml` : `host-tests` |
| `CMake host tests (Class B, ASan + UBSan)` | `firmware-cross-build.yml` : `host-tests-sanitized` |
| `Bootloader (i.MX RT1062)` | `firmware-cross-build.yml` : `bootloader` |
| `Main firmware (i.MX RT1062, Class B)` | `firmware-cross-build.yml` : `main-firmware` |
| `Class C scope` | `safety-mcu-ci.yml` : `changes` |
| `Safety MCU host tests (Class C)` | `safety-mcu-ci.yml` : `host-tests` |
| `Cross-build and link (STM32G071, Cortex-M0+, Class C)` | `safety-mcu-ci.yml` : `cross-build` |
| `Simulator scope` | `simulator-ci.yml` : `changes` |
| `Simulator build and tests (host only)` | `simulator-ci.yml` : `simulator-tests` |

Each of the ten is the `name:` of exactly one job, in a workflow that runs on `pull_request` with no `paths:`
filter, with no `strategy:` (`scripts/check-ci-scope.ts` correspondence C, run on this tree: PASS). A job that
its relevance list switches off still reports, as `skipped`, and GitHub counts `skipped` as passing
(`NP-SW-CI-001` §6.7.5, measured).

**Two statements in `NP-SW-CI-001` are out of date and are corrected by §6.17 of that document:** §6.13.5 says
the bypass list has one entry and that a direct push to `main` "is now refused for every actor". Since
2026-11-02 there are two entries, and the App can push.

**What is assumed, not known:** that the weekly `build-all.yml` job can read the ruleset with the job token.
It has not run on GitHub. If the token cannot see the bypass list, the script says so and compares the rest.

## 3. The options, and what each costs

### 3.1 Q1: the App's bypass

The workflow `assign-revisions.yml` pushes commits that touch `docs/` only (`git add -A docs`). The bypass
does not know that: it allows any push by anyone holding the App's private key (the repository secret
`REVBOT_PRIVATE_KEY`). A ruleset bypass cannot be limited to a path.

| Option | What it is | Cost | What it leaves |
|---|---|---|---|
| **A. Keep** | No change | None | One identity that can put any content on `main` unchecked. Its pushes do start CI on `main`, and none of those runs blocks anything |
| **B. Keep and audit** | A scheduled job fails if any commit on `main` that did not arrive by a merge touches a file outside `docs/`. Needs the GitHub API to tell a merged pull request from a push, because committer names can be set by whoever pushes | One job and its self-test, built on request | Detection after the fact, not prevention |
| **C. Pull request instead** | The App opens a pull request with the numbers and merges it once the required checks pass; the bypass is removed | `main` holds new content under the old revision for minutes, not about one. Two pull requests per change. A stuck bot pull request blocks every later assignment | No identity can push to `main` |

**Recommendation: B.** It keeps the 2026-11-02 decision (option A of
`docs/human-input/np-qms-dc-001-revision-assignment.md`) and makes its one side effect visible. C undoes part
of a decision eight days old and is offered only for completeness.

### 3.2 Q2: more required contexts

`bun scripts/check-main-ruleset.ts --contexts` lists every job and says whether its name can be required as
it stands. On this tree: 70 jobs, 10 required, 38 eligible, 22 not. **Requiring a context that no pull request
reports blocks every merge, with no error anywhere.** These can never be required as they stand:

| Job names | Why not |
|---|---|
| `Relevance` (ten workflows share this name) | A context is a string. One string cannot name ten jobs. Rename each first (for example `Android scope`) |
| `Analyze (${{ matrix.language }})` (`codeql.yml`), `${{ matrix.name }}` (`desktop-package.yml`) | A matrix job reports as `name (value)` |
| Every job in `dependency-scan.yml` and `crate-release-watch.yml` | The workflow has a `paths:` filter and never reports on a pull request outside it |
| Every job in `build-all.yml` and `assign-revisions.yml` | Not run on pull requests (`OI-SWCI-23`) |

Candidate sets, all eligible today:

| Set | Contexts (exact strings) | Why these |
|---|---|---|
| **P** (privacy) | `SHDR Schema Privacy Gate` · `Warranty No-Join Privacy Gate` · `reporting paths cannot leak a sensitive predicate (§5.1)` | They enforce CLAUDE.md §5: no user biology in SHDR, no join from warranty to user, no conditional redaction. A red one merged is a privacy defect on `main` |
| **G** (repository-wide guards) | `every gate declares its kind and is proven to fail` · `relevance lists cover what they gate (OI-SWCI-08)` · `CLAUDE.md section citations resolve (repo-wide)` · `document filenames match their serials (NP-CONV-001 §4.0)` · `scripts/ type-check` | They guard the checks themselves and the document set. The fourth runs the revision-fragment and DHF-index gates |

**A cost that applies to every added context:** the string becomes a contract. Several tooling job names carry
a revision or an item number (`consent methods are reachable (CLAUDE.md Rev 37)`). Renaming a required job
needs the ruleset and the workflow changed in one operation (`NP-SW-CI-001` §6.13.3). Sets P and G avoid the
names that carry a revision number.

**Recommendation: B (set P).** Three contexts, each a locked-rule gate, each already reporting on every pull
request. G is reasonable and is the principal's call on how much friction a solo workflow should carry: the
admin can still merge over a red check by bypass, on the pull request, visibly.

### 3.3 Q3, Q4, Q5

- **Q3.** `NP-SW-CI-001` §6.13.5 left it to be observed on the next pull request. Pull requests have gone on
  merging under the rule with no approval (#833 on 2026-11-11 is one), so it does not block the owner's own
  pull requests. What it does on a pull request from a
  fork or with unattributed commits was not tested. **Recommendation: A (keep)**, now recorded.
- **Q4.** It asks Copilot for a review and requires nothing. **Recommendation: A (keep)** if it is wanted;
  the point of asking is that it is now written down either way.
- **Q5.** `true` means every pull request must merge `main` and re-run its checks whenever `main` moves. With
  parallel sessions merging many times a day, each merge would send every other open pull request round
  again. `false` leaves the gap `OI-SWCI-53` describes: checks passed against an older `main`.
  **Recommendation: A (leave `false`)**; the push to `main` and the weekly `build-all.yml` are the backstop,
  and that only holds if a red run on `main` is heard (`docs/human-input/oi-swci-10-build-all-red.md`).

## 4. What I need back

For each of Q1 to Q5: the option, a one-line reason, and any condition that would reopen it. For Q2 option D,
the exact list. Also: whether App id `5585393` is `RevNumberFixer` (Settings → Rules → Safety → Bypass list
shows the name). Name, role and date.

## 5. Runbook: applying an answer (the principal, in the GitHub UI or with `gh api`)

Rules are edited at https://github.com/stevehickman/NeuroPulse/settings/rules → **Safety**.

**Order.** Tree first for an added context, settings first for a removed or renamed one:

| Change | Order | Why |
|---|---|---|
| Add a required context (Q2) | 1. Merge a pull request that adds the line to `ci/required-checks.txt`. 2. Check that open pull requests show the check (run or `skipped`). 3. Add the context in the ruleset | A context required before any pull request reports it blocks every merge. Between steps 1 and 3 the weekly live check reports rule L2 for that context, which is correct and clears at step 3 |
| Remove or rename a required context | 1. Edit the ruleset. 2. Merge the pull request that changes the workflow and the manifest (`NP-SW-CI-001` §6.13.3) | The other order leaves a required context nothing reports |
| Change a parameter or a bypass (Q1, Q3, Q4, Q5) | 1. Edit the ruleset. 2. Merge a pull request that changes `ci/main-ruleset.json` to match | These block nothing in either order; the file is the record |

**Step by step.**

1. **Save the state before.** `gh api repos/stevehickman/NeuroPulse/rulesets/16412379 > before.json`. Keep the file:
   it is the rollback.
2. **Make the edit.**
   - *UI:* Bypass list → the App → remove it (Q1 C). *Require status checks to pass* → **Add checks** → type the
     context exactly, pick the **GitHub Actions** source, never "any source" (Q2). *Require a pull request* →
     the unattributed-changes box (Q3). The Copilot review rule (Q4). *Require branches to be up to date* (Q5).
     **Save changes.**
   - *API:* `PUT` replaces the whole ruleset, so send every rule again. For Q2 set P:
     ```bash
     jq '{name,target,enforcement,conditions,bypass_actors,
          rules: [.rules[] | if .type == "required_status_checks"
            then .parameters.required_status_checks += [
              {context: "SHDR Schema Privacy Gate", integration_id: 15368},
              {context: "Warranty No-Join Privacy Gate", integration_id: 15368},
              {context: "reporting paths cannot leak a sensitive predicate (§5.1)", integration_id: 15368}]
            else . end]}' before.json \
       | gh api --method PUT repos/stevehickman/NeuroPulse/rulesets/16412379 --input -
     ```
3. **Verify.** From a checkout of `main` after the matching pull request merged:
   ```bash
   bun scripts/check-main-ruleset.ts --live --require-bypass
   ```
   It must print `OK`. It reads two endpoints and changes nothing. A line starting `L1` means GitHub requires
   a context the tree does not list: fix that first, it can block every merge. `--require-bypass` makes it
   fail if the token cannot see the bypass list, so an admin run always compares it.
4. **Verify on a pull request.** Open or update any pull request. Every required context must reach a final
   state (pass, or `skipped`). One that stays "Expected — waiting for status to be reported" is the deadlock:
   remove that context from the ruleset at once.
5. **Record.** Set this file's status to `ANSWERED`, record the answer in `NP-SW-CI-001` §6.17 and
   `docs/status/completed-decisions.md`, close `OI-SWCI-54`.

**Rollback.** Any step: `jq '{name,target,enforcement,conditions,bypass_actors,rules}' before.json | gh api
--method PUT repos/stevehickman/NeuroPulse/rulesets/16412379 --input -`, then revert the pull request that
changed `ci/required-checks.txt` or `ci/main-ruleset.json`. If a required context is blocking merges and the
revert itself cannot merge, the admin merges it by bypass on the pull request (the admin's `pull_request`
bypass exists for this), or removes the one context in the UI first.

**What must never be done:** require a `build-all.yml` or `assign-revisions.yml` job; require a `Relevance`
job before it is renamed; set the admin bypass back to *Always allow* (`check-main-ruleset.ts` rule R3 fails the
tree that records it, and L7 the settings that have it); remove the App's bypass while
`docs/revisions/policy.json` says `assign-at-merge` and the App still pushes (every assignment would be refused;
set the policy to `hand` first, or build option C).
