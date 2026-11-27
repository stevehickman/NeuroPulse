# OI-ANALYTICS-03: what "pin PostHog to one commit's images" means when no commit has a full set

**Status:** ANSWERED 2026-11-27 by the principal, in session: option A · **From:** the OI-ANALYTICS-03 working session · **To:** the principal
**GitHub issue:** #971 (the erasure-relay issue OI-ANALYTICS-03 grew from; no issue of its own).

## The question

The agreed plan (`OI-ANALYTICS-03` (2)) says: pin PostHog to one commit's images, and take ClickHouse
and every other image whose version PostHog sets from that same commit's compose. The registries do
not publish one image set per commit. **Which of A, B or C is the pin?**

## What was found (2026-11-26, read from the public registries; not run)

- PostHog `master` was `6b9ed0147661f5b28d10193a3bb6722d326ca2f9`. Its `docker-compose.hobby.yml`
  has 48 services and `docker-compose.base.yml` 51.
- **Images PostHog versions itself in the compose** (ClickHouse
  `ghcr.io/posthog/clickhouse-posthog:26.9.5.2-posthog-c73aad4ad67d`, Redpanda `v25.1.9`,
  SeaweedFS `4.29`, Postgres `15.12-alpine`, Valkey `8.1-alpine`, Temporal `1.26.2`, etcd `v3.6.8`,
  and the rest) are fixed by that commit's file. These follow the plan as written: take them as the
  commit sets them.
- **Images PostHog builds itself** (the Django app, `posthog-node`, and the Rust services `capture`,
  `feature-flags`, `personhog-*`, `cymbal`, `livestream`, `property-defs-rs`, …) float on `:master`
  or `:latest`. Their registries tag a build `sha-<7 hex>` (ghcr) or by the full SHA (Docker Hub),
  **but a service is rebuilt only when its own code changes.** None of the eight Rust images checked
  has a tag for `6b9ed01`, and the Docker Hub app image has none for it either. `capture:master` was
  built from `a49d480…` (its `org.opencontainers.image.revision` annotation). So "commit X's images"
  is, for each service, the build from the latest commit at or before X that rebuilt it.
- Every image index carries `org.opencontainers.image.revision`, so which commit a build came from
  can be read and checked against PostHog's history (`git merge-base --is-ancestor`).

## Options

**A. Commit plus digests, resolved and checked (recommended).** Take the compose from commit X.
Pin every PostHog-built image by digest (`image@sha256:…`) to the build its `:master` tag pointed
at when X was chosen, and record each build's revision. A script, run at each upgrade, checks that
every recorded revision is an ancestor of X (or X itself) and refuses the pin otherwise. *Cost:* the
images are "what PostHog would have run at X" rather than "built at X"; a service whose newest build
is *after* X cannot be used, so X is chosen as a recent master commit and checked, not picked
freely. Needs network to the registries at upgrade time, never at run time.

**B. Only commits that rebuilt everything.** Choose X only where every PostHog-built image has a
`sha-<X>` tag. *Cost:* such commits may be rare or absent (none of eight images had the current
head); upgrades would wait on PostHog's build pattern.

**C. Build the images from source at X.** *Cost:* NeurOne builds and hosts 20-odd images, a supply
chain to run and audit; far beyond a reference stack.

In every option ClickHouse and the other externally versioned images come from X's compose
unchanged, and the minimum-version check (`SELECT version()`) sits on top as a guard.

## What is needed back

The option, a line of rationale (`CLAUDE.md` §18), anything that would reopen it, and name, role and date.

## Answer

**Option A** (the principal, in session, 2026-11-27). Recorded in `NP-ANALYTICS-001` §7.3 and the decisions log; built as
`infra/posthog/pin_posthog.py` (`REQ-SW03-41`). Reopens if PostHog publishes per-commit image sets (B becomes free) or stops
recording `org.opencontainers.image.revision` (A can no longer be checked).
