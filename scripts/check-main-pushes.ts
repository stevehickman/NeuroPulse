#!/usr/bin/env bun
/**
 * check-main-pushes.ts — what reached `main` without a pull request is only what the revision App is for.
 *
 * NP-SW-CI-001 §6.21, OI-SWCI-54 Q1 (option B). The App that assigns revision numbers bypasses the `Safety`
 * ruleset in mode `always`: it can push anything to `main` with no pull request and no check. A ruleset
 * bypass cannot be limited to a path, so the limit it was granted for is not enforced by GitHub. This script
 * is the detection: after the fact, not prevention.
 *
 * What the App may touch is `allowed_paths` on its bypass entry in ci/main-ruleset.json: `docs` and
 * `CLAUDE.md` (the principal, 2026-11-26, §6.21.6). It is the paths `assign-revisions.yml` stages
 * (`git add -A docs CLAUDE.md`), because a fragment's `## CLAUDE.md` section sets CLAUDE.md's revision line.
 * A path is allowed when it equals an entry or lies under it, so `docs` allows `docs/x.md` and not `docs-x/y`.
 *
 * A commit on `main` in the window either
 *   (a) arrived by a merged pull request (GitHub: `GET /repos/R/commits/SHA/pulls` names a PR with a
 *       `merged_at`; for a commit on the default branch that is the pull request that introduced it), or
 *   (b) did not. A commit that did not may touch the allowed paths and nothing else. A rename counts both
 *       its old and its new path, so moving a file out of an allowed path, or into one from elsewhere, fails.
 *
 * Why the file list is read from GitHub and not from the committer: committer and author names are set
 * by whoever pushes, so "the App made it" is not something a commit can prove.
 *
 * The window is the last 14 days: two periods of the weekly schedule in build-all.yml, so one missed or
 * failed run leaves no commit unaudited. It is derived from the schedule, and is `--days` if you need
 * another.
 *
 * What this does NOT do: prevent a push (that is option C of the brief, not chosen), read a commit
 * outside the window, or tell a rebase-merged pull request's earlier commits from a direct push if GitHub
 * does not list the pull request for them. In that case the commit is reported, and the report is wrong
 * in the safe direction.
 *
 * Default mode (offline, every pull request that touches ci/ or a workflow). While ci/main-ruleset.json lets
 * an App bypass in mode `always`: (1) build-all.yml must still run `--live` on a schedule, or deleting the
 * job would leave the bypass with no detection and nothing red; (2) the bypass must declare `allowed_paths`;
 * (3) those paths must equal what the App's workflow stages with `git add -A`. Widening what the App
 * commits is then a visible edit to the recorded decision, and a change to one file alone fails.
 *
 * Usage: bun scripts/check-main-pushes.ts [--self-test | --live [--days N] | --from <records.json>]
 *   records.json is [{ "sha", "date", "pulls": [{ "number", "merged_at" }], "files": [{ "filename", "previous_filename"? }] }]
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-main-pushes.ts --self-test
 * CI-Scan-Probe: bun scripts/check-main-pushes.ts
 * CI-Scans: the App bypass in ci/main-ruleset.json against the audit job in build-all.yml and the paths assign-revisions.yml stages, and (live) the commits on main in the last 14 days that arrived without a merged pull request
 * CI-Scan-Paths: ci/** .github/workflows/** scripts/check-main-pushes.ts
 */
import { existsSync, readFileSync } from "fs";
import { join, resolve } from "path";

/** Two periods of build-all.yml's weekly schedule. */
export const WINDOW_DAYS = 14;

export type Pull = { number: number; merged_at: string | null };
/** `previous_filename` is `null`, not absent, for a file that was not renamed: `jq` writes every key it is asked for
 *  (`{filename, previous_filename}` in live()), so a missing key arrives as null. */
export type FileRef = { filename: string; previous_filename?: string | null };
/** `files` is absent when the commit arrived by a merged pull request: it is not read then. */
export type CommitRecord = { sha: string; date: string; pulls: Pull[]; files?: FileRef[] };
/** An entry of `bypass_actors` in ci/main-ruleset.json, as far as this script reads it. */
export type BypassEntry = { actor_type: string; bypass_mode: string; workflow?: string; allowed_paths?: string[] };

export const viaPullRequest = (r: CommitRecord): boolean => r.pulls.some((p) => p.merged_at !== null && p.merged_at !== undefined);

/** A path is allowed when it is an allowed entry or lies under one: `docs` allows `docs/x.md`, never `docs-x/y`. */
export const pathAllowed = (p: string, allowed: string[]): boolean => allowed.some((a) => p === a || p.startsWith(`${a}/`));

/** Paths a commit touches that are not allowed. A rename names both ends. */
export function outsideAllowed(files: FileRef[], allowed: string[]): string[] {
  const out: string[] = [];
  for (const f of files) {
    for (const p of [f.filename, f.previous_filename]) {
      if (p === undefined || p === null) continue;
      if (!pathAllowed(p, allowed) && !out.includes(p)) out.push(p);
    }
  }
  return out;
}

export function audit(records: CommitRecord[], allowed: string[]): string[] {
  const v: string[] = [];
  const list = allowed.join(", ");
  for (const r of records) {
    if (viaPullRequest(r)) continue;
    const short = r.sha.slice(0, 7);
    if (r.files === undefined) {
      v.push(`${short} (${r.date}) reached main with no merged pull request and its files were not read: nothing shows it stayed within ${list}`);
      continue;
    }
    const bad = outsideAllowed(r.files, allowed);
    if (bad.length) v.push(`${short} (${r.date}) reached main with no merged pull request and touches outside ${list}: ${bad.slice(0, 5).join(", ")}${bad.length > 5 ? `, and ${bad.length - 5} more` : ""}`);
  }
  return v;
}

export const BUILD_ALL = ".github/workflows/build-all.yml";
export const EXPECTED_FILE = "ci/main-ruleset.json";

/** The paths a workflow stages: every `git add -A <paths>` line, comments and ordering ignored. null if it has none. */
export function stagedPaths(workflow: string): string[] | null {
  const found: string[] = [];
  for (const m of workflow.matchAll(/^[ \t]*git add -A[ \t]+([^#\n]+)/gm)) {
    for (const p of m[1].trim().split(/\s+/)) if (p !== "" && !found.includes(p)) found.push(p);
  }
  return found.length ? found.sort() : null;
}

/** The paths the expected ruleset lets the App's bypass touch: the union over every App that bypasses in mode always. */
export const allowedFrom = (bypass: BypassEntry[]): string[] =>
  [...new Set(bypass.filter((b) => b.actor_type === "Integration" && b.bypass_mode === "always").flatMap((b) => b.allowed_paths ?? []))].sort();

/**
 * The App bypass has its detection, and its allowance is the recorded decision: while an App may bypass in mode
 * `always`, build-all.yml runs `--live` on a schedule, the bypass declares `allowed_paths`, and those equal what
 * the App's workflow stages. `readWorkflow` returns a workflow file's text, or null if it does not exist.
 */
export function wiringViolations(bypass: BypassEntry[], buildAll: string | null, readWorkflow: (rel: string) => string | null): string[] {
  const apps = bypass.filter((b) => b.actor_type === "Integration" && b.bypass_mode === "always");
  if (apps.length === 0) return [];
  if (buildAll === null) return [`${EXPECTED_FILE} lets an App bypass in mode always, and ${BUILD_ALL} does not exist: nothing audits what it pushes`];
  const v: string[] = [];
  if (!/check-main-pushes\.ts --live/.test(buildAll)) v.push(`${EXPECTED_FILE} lets an App bypass in mode always, and ${BUILD_ALL} no longer runs check-main-pushes.ts --live: nothing audits what it pushes (NP-SW-CI-001 §6.21)`);
  if (!/^  schedule:/m.test(buildAll)) v.push(`${BUILD_ALL} has no schedule: trigger, so the audit of the App's pushes never runs by itself`);
  for (const b of apps) {
    const who = b.workflow ?? "an App";
    const declared = [...new Set(b.allowed_paths ?? [])].sort();
    if (declared.length === 0) { v.push(`${EXPECTED_FILE}: the App bypass for ${who} declares no allowed_paths, so the audit has nothing to compare a push with (NP-SW-CI-001 §6.21.6)`); continue; }
    if (!b.workflow) { v.push(`${EXPECTED_FILE}: an App bypass declares allowed_paths but names no workflow to compare them with`); continue; }
    const text = readWorkflow(b.workflow);
    if (text === null) { v.push(`${EXPECTED_FILE}: the App bypass names ${b.workflow}, which does not exist`); continue; }
    const staged = stagedPaths(text);
    if (staged === null) { v.push(`${b.workflow} has no \`git add -A <paths>\` line, so what the App commits is not stated anywhere the audit can compare with ${EXPECTED_FILE}`); continue; }
    if (staged.join("\n") !== declared.join("\n")) {
      v.push(`${b.workflow} stages [${staged.join(", ")}] and ${EXPECTED_FILE} allows [${declared.join(", ")}] for the App: change both together, as one decision (NP-SW-CI-001 §6.21.6)`);
    }
  }
  return v;
}

export const sinceIso = (now: Date, days: number): string => new Date(now.getTime() - days * 86_400_000).toISOString();

function gh(path: string, jq: string): string[] {
  const r = Bun.spawnSync(["gh", "api", "--paginate", path, "--jq", jq], { stdout: "pipe", stderr: "pipe" });
  if (r.exitCode !== 0) {
    console.error(`check-main-pushes: 'gh api ${path}' failed (${new TextDecoder().decode(r.stderr).trim().split("\n").pop()}). Nothing was audited.`);
    process.exit(2);
  }
  return new TextDecoder().decode(r.stdout).split("\n").filter((l) => l.trim() !== "");
}

function live(repo: string, days: number): CommitRecord[] {
  const since = sinceIso(new Date(), days);
  const commits = gh(`repos/${repo}/commits?sha=main&since=${since}&per_page=100`, `.[] | {sha, date: .commit.committer.date}`).map((l) => JSON.parse(l) as { sha: string; date: string });
  return commits.map((c) => {
    const pulls = gh(`repos/${repo}/commits/${c.sha}/pulls`, `.[] | {number, merged_at}`).map((l) => JSON.parse(l) as Pull);
    const r: CommitRecord = { sha: c.sha, date: c.date, pulls };
    if (!viaPullRequest(r)) r.files = gh(`repos/${repo}/commits/${c.sha}`, `.files[]? | {filename, previous_filename}`).map((l) => JSON.parse(l) as FileRef);
    return r;
  });
}

function report(records: CommitRecord[], allowed: string[], ok: string): never {
  const violations = audit(records, allowed);
  if (violations.length) {
    for (const x of violations) console.error(`  ✗ ${x}`);
    console.error(`check-main-pushes: ${violations.length} violation(s). The revision App's bypass (NP-SW-CI-001 §6.21, OI-SWCI-54 Q1) is for ${allowed.join(", ")} only.`);
    process.exit(1);
  }
  console.log(ok);
  console.log(`scanned: ${records.length}`);
  process.exit(0);
}

function selfTest(): never {
  let failed = 0;
  let cases = 0;
  const expect = (label: string, got: string[], want: RegExp | null): void => {
    cases++;
    const ok = want === null ? got.length === 0 : got.some((g) => want.test(g));
    if (!ok) {
      failed++;
      console.error(`FAIL ${label}: ${want === null ? `expected clean, got ${JSON.stringify(got)}` : `expected ${want}, got ${JSON.stringify(got)}`}`);
    }
  };
  const equal = (label: string, got: unknown, want: unknown): void => {
    cases++;
    if (JSON.stringify(got) !== JSON.stringify(want)) { failed++; console.error(`FAIL ${label}: expected ${JSON.stringify(want)}, got ${JSON.stringify(got)}`); }
  };
  const A = ["CLAUDE.md", "docs"];
  const aud = (records: CommitRecord[]): string[] => audit(records, A);
  const merged: Pull[] = [{ number: 1, merged_at: "2026-11-20T00:00:00Z" }];
  const rec = (sha: string, pulls: Pull[], files?: FileRef[]): CommitRecord => ({ sha: sha.padEnd(40, "0"), date: "2026-11-20", pulls, files });
  const f = (...names: string[]): FileRef[] => names.map((filename) => ({ filename }));

  expect("a merged pull request may touch anything", aud([rec("aaaaaaa", merged, undefined)]), null);
  expect("a direct push under docs/ only", aud([rec("bbbbbbb", [], f("docs/np_x_001.md", "docs/status/log.md"))]), null);
  expect("a direct push with no files", aud([rec("ccccccc", [], [])]), null);
  expect("a direct push that adds firmware", aud([rec("ddddddd", [], f("docs/np_x_001.md", "firmware/hub_control/src/a.c"))]), /ddddddd.*firmware\/hub_control\/src\/a\.c/);
  expect("the entry is a directory, not a string prefix", aud([rec("eeeeeee", [], f("docs-extra/x.md"))]), /docs-extra\/x\.md/);
  expect("a rename out of an outside path into docs/", aud([rec("fffffff", [], [{ filename: "docs/moved.md", previous_filename: "scripts/moved.ts" }])]), /scripts\/moved\.ts/);
  expect("a rename out of docs/", aud([rec("1111111", [], [{ filename: "app/moved.md", previous_filename: "docs/moved.md" }])]), /app\/moved\.md/);
  expect("an open pull request is not a merge", aud([rec("2222222", [{ number: 9, merged_at: null }], f("firmware/a.c"))]), /2222222/);
  expect("no pull request and no file list fails closed", aud([rec("3333333", [])]), /files were not read/);
  expect("an empty window is clean", aud([]), null);
  // The shape gh's --jq '{filename, previous_filename}' returns: the key is present and null when nothing was renamed.
  // The first --live run on GitHub crashed on it (TypeError on null.startsWith), because every case above omits the key.
  expect("a direct push under docs/ as gh returns it (previous_filename: null)", aud([rec("8888888", [], [{ filename: "docs/np_x_001.md", previous_filename: null }])]), null);
  expect("a direct push outside the allowance as gh returns it (previous_filename: null)", aud([rec("9999999", [], [{ filename: "scripts/x.ts", previous_filename: null }])]), /9999999.*scripts\/x\.ts/);
  const three = aud([rec("4444444", merged), rec("5555555", [], f("ci/x.txt")), rec("6666666", [], f("docs/y.md"))]);
  expect("only the bad commit is named", three.filter((x) => /4444444|6666666/.test(x)), null);
  expect("and it is named", three, /5555555.*ci\/x\.txt/);
  expect("many paths are shortened, not dropped", aud([rec("7777777", [], f("a1", "a2", "a3", "a4", "a5", "a6", "a7"))]), /and 2 more/);

  // The allowance decided 2026-11-26 (§6.21.6): CLAUDE.md beside docs. The three commits the first real read found.
  expect("a revision commit that sets CLAUDE.md's revision line", aud([rec("a1a1a1a", [], [{ filename: "CLAUDE.md", previous_filename: null }, { filename: "docs/np_dhf_001.md", previous_filename: null }])]), null);
  expect("CLAUDE.md alone", aud([rec("a2a2a2a", [], f("CLAUDE.md"))]), null);
  expect("a file named like CLAUDE.md is not CLAUDE.md", aud([rec("a3a3a3a", [], f("CLAUDE.mdx"))]), /CLAUDE\.mdx/);
  expect("a CLAUDE.md below another directory is not the root one", aud([rec("a4a4a4a", [], f("app/CLAUDE.md"))]), /app\/CLAUDE\.md/);
  expect("a rename out of CLAUDE.md", aud([rec("a5a5a5a", [], [{ filename: "elsewhere.md", previous_filename: "CLAUDE.md" }])]), /elsewhere\.md/);
  expect("only docs allowed: the same CLAUDE.md commit is a finding", audit([rec("a6a6a6a", [], f("CLAUDE.md"))], ["docs"]), /a6a6a6a.*CLAUDE\.md/);

  equal("a path is allowed when it equals an entry or lies under it", [pathAllowed("docs", A), pathAllowed("docs/a/b.md", A), pathAllowed("docs2/a", A), pathAllowed("CLAUDE.md/x", A)], [true, true, false, true]);

  // What assign-revisions.yml stages.
  equal("staged paths, sorted, from the line the workflow has", stagedPaths("    git add -A docs CLAUDE.md\n"), ["CLAUDE.md", "docs"]);
  equal("staged paths ignore a comment, even one that names git add", stagedPaths("# git add -A firmware\n    git add -A docs CLAUDE.md   # why\n"), ["CLAUDE.md", "docs"]);
  equal("staged paths from two lines are one set", stagedPaths("git add -A docs\ngit add -A CLAUDE.md docs\n"), ["CLAUDE.md", "docs"]);
  equal("a workflow with no git add -A line", stagedPaths("run: echo hi\n"), null);

  const app = { actor_type: "Integration", bypass_mode: "always", workflow: ".github/workflows/assign-revisions.yml", allowed_paths: ["docs", "CLAUDE.md"] };
  const appAlways = [app];
  const wired = "on:\n  schedule:\n    - cron: '0 3 * * 0'\njobs:\n  a:\n    steps:\n      - run: bun scripts/check-main-pushes.ts --live\n";
  const stagesBoth = (_: string): string | null => "      git add -A docs CLAUDE.md\n";
  expect("an App bypass with its audit wired and its allowance equal to what it stages", wiringViolations(appAlways, wired, stagesBoth), null);
  expect("an App bypass whose audit step was deleted", wiringViolations(appAlways, wired.replace("check-main-pushes.ts --live", "true"), stagesBoth), /no longer runs/);
  expect("an App bypass whose schedule was deleted", wiringViolations(appAlways, wired.replace("  schedule:", "  workflow_dispatch:"), stagesBoth), /no schedule/);
  expect("an App bypass with no build-all.yml", wiringViolations(appAlways, null, stagesBoth), /does not exist/);
  expect("no App bypass needs no audit", wiringViolations([{ actor_type: "RepositoryRole", bypass_mode: "pull_request" }], "on:\n", stagesBoth), null);
  expect("an App bypass in pull_request mode needs no audit", wiringViolations([{ actor_type: "Integration", bypass_mode: "pull_request" }], "on:\n", stagesBoth), null);
  expect("the workflow widens what the App stages and the record does not", wiringViolations(appAlways, wired, () => "git add -A docs CLAUDE.md firmware\n"), /stages \[CLAUDE\.md, docs, firmware\].*allows \[CLAUDE\.md, docs\]/);
  expect("the record widens what the App may touch and the workflow does not", wiringViolations([{ ...app, allowed_paths: ["docs", "CLAUDE.md", "ci"] }], wired, stagesBoth), /stages \[CLAUDE\.md, docs\].*allows \[CLAUDE\.md, ci, docs\]/);
  expect("an App bypass that declares no allowed_paths", wiringViolations([{ ...app, allowed_paths: undefined }], wired, stagesBoth), /declares no allowed_paths/);
  expect("an App bypass that names a workflow which does not exist", wiringViolations(appAlways, wired, () => null), /does not exist/);
  expect("an App workflow with no git add -A line", wiringViolations(appAlways, wired, () => "run: true\n"), /no `git add -A <paths>` line/);
  expect("the order the paths are written in does not matter", wiringViolations([{ ...app, allowed_paths: ["CLAUDE.md", "docs"] }], wired, () => "git add -A CLAUDE.md docs\n"), null);
  equal("the allowance is the union over the Apps that bypass in mode always", allowedFrom([app, { actor_type: "Integration", bypass_mode: "always", allowed_paths: ["ci"] }, { actor_type: "Integration", bypass_mode: "pull_request", allowed_paths: ["firmware"] }]), ["CLAUDE.md", "ci", "docs"]);

  cases++;
  if (sinceIso(new Date("2026-11-21T12:00:00Z"), 14) !== "2026-11-07T12:00:00.000Z") { failed++; console.error("FAIL since: 14 days before 2026-11-21T12:00Z"); }

  if (failed) {
    console.error(`check-main-pushes self-test: ${failed} of ${cases} case(s) FAILED`);
    process.exit(1);
  }
  console.log(`check-main-pushes self-test: ${cases} cases passed`);
  process.exit(0);
}

function loadExpected(root: string): { repository?: string; bypass_actors?: BypassEntry[] } {
  const file = join(root, EXPECTED_FILE);
  if (!existsSync(file)) { console.error(`check-main-pushes: ${EXPECTED_FILE} is missing`); process.exit(2); }
  return JSON.parse(readFileSync(file, "utf8"));
}

function allowedOrExit(e: { bypass_actors?: BypassEntry[] }): string[] {
  const allowed = allowedFrom(e.bypass_actors ?? []);
  if (allowed.length === 0) { console.error(`check-main-pushes: ${EXPECTED_FILE} declares no allowed_paths for an App that bypasses in mode always. Nothing was audited.`); process.exit(2); }
  return allowed;
}

function main(): void {
  const args = process.argv.slice(2);
  if (args[0] === "--self-test") return selfTest();
  const root = resolve(import.meta.dir, "..");
  if (args[0] === "--from") {
    if (!args[1]) { console.error("usage: --from <records.json>"); process.exit(2); }
    const allowed = allowedOrExit(loadExpected(root));
    return report(JSON.parse(readFileSync(args[1], "utf8")) as CommitRecord[], allowed, `check-main-pushes: OK — every commit in the file arrived by a merged pull request or touches only ${allowed.join(", ")}`);
  }
  if (args[0] === "--live") {
    const e = loadExpected(root);
    if (!e.repository) { console.error(`check-main-pushes: ${EXPECTED_FILE} names no repository`); process.exit(2); }
    const allowed = allowedOrExit(e);
    const i = args.indexOf("--days");
    const days = i >= 0 ? Number(args[i + 1]) : WINDOW_DAYS;
    if (!Number.isFinite(days) || days <= 0) { console.error("check-main-pushes: --days must be a positive number"); process.exit(2); }
    return report(live(e.repository, days), allowed, `check-main-pushes: OK — every commit on main in the last ${days} days arrived by a merged pull request or touches only ${allowed.join(", ")}`);
  }
  if (args.length === 0) {
    const e = loadExpected(root);
    const ba = join(root, BUILD_ALL);
    const readWorkflow = (rel: string): string | null => (existsSync(join(root, rel)) ? readFileSync(join(root, rel), "utf8") : null);
    const v = wiringViolations(e.bypass_actors ?? [], existsSync(ba) ? readFileSync(ba, "utf8") : null, readWorkflow);
    if (v.length) {
      for (const x of v) console.error(`  ✗ ${x}`);
      console.error(`check-main-pushes: ${v.length} violation(s)`);
      process.exit(1);
    }
    console.log(`check-main-pushes: OK — the App bypass in ${EXPECTED_FILE} has its audit in ${BUILD_ALL}, and its allowance equals what its workflow stages (offline; --live reads GitHub)`);
    console.log(`scanned: ${(e.bypass_actors ?? []).length}`);
    process.exit(0);
  }
  console.error("usage: bun scripts/check-main-pushes.ts [--self-test | --live [--days N] | --from <records.json>]");
  process.exit(2);
}

if (import.meta.main) main();
