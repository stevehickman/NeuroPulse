# How is a red `build-all` heard?

**Status:** DRAFT, not sent. **From:** a Claude Code session working on GitHub #446. **To:** the principal
(repository owner). Open item: `OI-SWCI-10` (`docs/np_sw_ci_001.md` §7), open since 2026-08-08.

## 1. The question

`build-all.yml` runs weekly and on demand. It is not a pull-request check and blocks nothing, so a red run is
seen only by someone who looks. **How should a red run reach a person?** Pick one:

| Option | What it is | Cost |
|---|---|---|
| **A. An issue** | A last job, `if: failure()`, opens one issue labelled `build-all-red`, or adds a comment to it if it is open. GitHub's own notifications deliver it | `issues: write` on that one job. One issue to close by hand when green again |
| **B. GitHub's failed-workflow email** | Nothing is built. The principal turns on Settings → Notifications → Actions → "Send notifications for failed workflows only" | None in the repository. It notifies the person who last edited the schedule only, and stops when that person changes |
| **C. A chat or email webhook** | The last job posts to a webhook | A secret, and a service to keep |
| **D. Leave it** | Someone looks | The run was red for a month once, unseen (`NP-SW-CI-001` §4.10.7) |

**Recommendation: A.** It needs no secret and no outside service, and the issue is a record.

## 2. What is known

- The one recorded unseen failure was copy drift between workflow files (`NP-SW-CI-001` §4.10.7). That now
  fails on the pull request (`check-ci-scope.ts` correspondence E), so `build-all` is left catching what no
  pull request changes: a runner image, a toolchain, and, since `NP-SW-CI-001` §6.17, the ruleset on `main`.
- The new `main-ruleset-live` job in `build-all.yml` is only as loud as this answer. A required context removed
  in the settings page would turn that job red and nothing else.
- The same question applies to a red run on a push to `main` (every workflow) and to a failed
  `assign-revisions.yml` run, which leaves a revision unassigned. Option A can cover them with the same job.
  Say if it should.

## 3. What I need back

The option; for A, whether pushes to `main` and `assign-revisions.yml` are covered too; name, role and date.
Nothing is built until then.
