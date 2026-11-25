#!/usr/bin/env bun
/**
 * check-main-pushes.ts — what reached `main` without a pull request is only what the revision App is for.
 *
 * NP-SW-CI-001 §6.21, OI-SWCI-54 Q1 (option B). The App that assigns revision numbers bypasses the `Safety`
 * ruleset in mode `always`: it can push anything to `main` with no pull request and no check. A ruleset
 * bypass cannot be limited to a path, so the limit it was granted for (it commits under `docs/` only,
 * `git add -A docs` in assign-revisions.yml) is not enforced by GitHub. This script is the detection:
 * after the fact, not prevention.
 *
 * A commit on `main` in the window either
 *   (a) arrived by a merged pull request (GitHub: `GET /repos/R/commits/SHA/pulls` names a PR with a
 *       `merged_at`; for a commit on the default branch that is the pull request that introduced it), or
 *   (b) did not. A commit that did not may touch files under `docs/`, and `CLAUDE.md` only on its
 *       `**Revision:** N (current)` line, and nothing else. A rename counts both its old and its new path,
 *       so moving a file out of `docs/` or into it from elsewhere fails.
 *
 * Why `CLAUDE.md`, and why only that line: since #1003 the App commits `git add -A docs CLAUDE.md`, because a
 * fragment's `## CLAUDE.md` section sets CLAUDE.md's revision line at merge (scripts/assign-revisions.ts, which
 * changes nothing else in that file). CLAUDE.md is the file every agent session loads, so any other change to it
 * by the App is reported. The line pattern is CLAUDE_REV_LINE from scripts/revision-lib.ts, the one the App uses; it is
 * written here so the self-test stays hermetic, and the offline mode fails if the two differ.
 * The first --live run (2026-11-25) reported three such revision bumps as violations because this rule still said
 * `docs/` only; nothing compared it with the workflow. The offline mode now does.
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
 * Default mode (offline, every pull request that touches ci/ or a workflow): while the expected ruleset
 * (ci/main-ruleset.json) lets an App bypass in mode `always`, build-all.yml must still run `--live` on a
 * schedule. Without that, deleting the job would leave the bypass with no detection and nothing red.
 *
 * Usage: bun scripts/check-main-pushes.ts [--self-test | --live [--days N] | --from <records.json>]
 *   records.json is [{ "sha", "date", "pulls": [{ "number", "merged_at" }], "files": [{ "filename", "previous_filename"? }] }]
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-main-pushes.ts --self-test
 * CI-Scan-Probe: bun scripts/check-main-pushes.ts
 * CI-Scans: the App bypass in ci/main-ruleset.json against the audit job in build-all.yml, what assign-revisions.yml commits against the audit's allow-list, and (live) the commits on main in the last 14 days that arrived without a merged pull request
 * CI-Scan-Paths: ci/** .github/workflows/** scripts/check-main-pushes.ts
 */
import { existsSync, readFileSync } from "fs";
import { join, resolve } from "path";

/** The directory a commit that bypassed the pull request may touch freely (assign-revisions.yml: `git add -A docs CLAUDE.md`). */
export const ALLOWED_PREFIX = "docs/";
/** Files outside it the App may change only on their revision line, and the line each may change. */
/** CLAUDE_REV_LINE's source in scripts/revision-lib.ts. The offline mode compares them. */
export const CLAUDE_REV_LINE_SOURCE = String.raw`^\*\*Revision:\*\*\s*(\d+) \(current\)`;
export const REVISION_LINE_FILES: Record<string, RegExp> = {
  "CLAUDE.md": new RegExp(`${CLAUDE_REV_LINE_SOURCE}\\s*$`),
};
export const REVISION_LIB = "scripts/revision-lib.ts";
/** Two periods of build-all.yml's weekly schedule. */
export const WINDOW_DAYS = 14;

export type Pull = { number: number; merged_at: string | null };
/** `previous_filename` is `null`, not absent, for a file that was not renamed: `jq` writes every key it is asked for
 *  (`{filename, previous_filename}` in live()), so a missing key arrives as null. */
export type FileRef = { filename: string; previous_filename?: string | null; patch?: string | null };
/** `files` is absent when the commit arrived by a merged pull request: it is not read then. */
export type CommitRecord = { sha: string; date: string; pulls: Pull[]; files?: FileRef[] };

export const viaPullRequest = (r: CommitRecord): boolean => r.pulls.some((p) => p.merged_at !== null && p.merged_at !== undefined);

/** True when every line a patch adds or removes is the file's revision line. A missing patch (GitHub omits one
 *  for a large or binary change) is not shown to be one, so it fails. */
export function revisionLineOnly(patch: string | null | undefined, line: RegExp): boolean {
  if (typeof patch !== "string") return false;
  const changed = patch.split("\n").filter((l) => (l.startsWith("+") || l.startsWith("-")) && !l.startsWith("+++") && !l.startsWith("---"));
  return changed.length > 0 && changed.every((l) => line.test(l.slice(1)));
}

/** Paths a commit touches that it may not: not under docs/, and not a revision-line-only change to a listed file.
 *  A rename names both ends, so a renamed revision-line file is outside. */
export function outsideDocs(files: FileRef[]): string[] {
  const out: string[] = [];
  for (const f of files) {
    const renamed = f.previous_filename !== undefined && f.previous_filename !== null && f.previous_filename !== f.filename;
    for (const p of [f.filename, f.previous_filename]) {
      if (p === undefined || p === null) continue;
      if (p.startsWith(ALLOWED_PREFIX)) continue;
      const line = REVISION_LINE_FILES[p];
      if (line !== undefined && !renamed && revisionLineOnly(f.patch, line)) continue;
      const shown = line !== undefined ? `${p} (beyond its revision line)` : p;
      if (!out.includes(shown)) out.push(shown);
    }
  }
  return out;
}

export const ASSIGN_REVISIONS = ".github/workflows/assign-revisions.yml";

/** What the App commits must be what the audit allows: every path in the workflow's `git add` is docs/ or a listed
 *  revision-line file. Widening one without the other fails here, at the pull request, not weeks later on main. */
export function grantViolations(workflow: string | null): string[] {
  if (workflow === null) return [];
  const adds = [...workflow.matchAll(/^\s*git add\b([^\n#]*)/gm)];
  if (adds.length === 0) return [`${ASSIGN_REVISIONS} has no 'git add' line this check can read: what the App commits is not compared with what the audit allows`];
  const v: string[] = [];
  for (const m of adds) {
    for (const tok of m[1]!.trim().split(/\s+/)) {
      if (tok === "" || tok.startsWith("-")) continue;
      const path = tok.replace(/^\.\//, "").replace(/\/+$/, "");
      if (path === ALLOWED_PREFIX.replace(/\/$/, "") || path.startsWith(ALLOWED_PREFIX)) continue;
      if (REVISION_LINE_FILES[path] !== undefined) continue;
      v.push(`${ASSIGN_REVISIONS} commits '${path}', which check-main-pushes.ts does not allow a direct push to touch: change the workflow and the audit's allow-list in one pull request (NP-SW-CI-001 §6.21.3)`);
    }
  }
  return v;
}

export function audit(records: CommitRecord[]): string[] {
  const v: string[] = [];
  for (const r of records) {
    if (viaPullRequest(r)) continue;
    const short = r.sha.slice(0, 7);
    if (r.files === undefined) {
      v.push(`${short} (${r.date}) reached main with no merged pull request and its files were not read: nothing shows it stayed under ${ALLOWED_PREFIX}`);
      continue;
    }
    const bad = outsideDocs(r.files);
    if (bad.length) v.push(`${short} (${r.date}) reached main with no merged pull request and touches outside ${ALLOWED_PREFIX}: ${bad.slice(0, 5).join(", ")}${bad.length > 5 ? `, and ${bad.length - 5} more` : ""}`);
  }
  return v;
}

export const BUILD_ALL = ".github/workflows/build-all.yml";
export const EXPECTED_FILE = "ci/main-ruleset.json";

/** The App bypass has its detection: while one may bypass in mode `always`, build-all.yml runs `--live` on a schedule. */
export function wiringViolations(bypass: { actor_type: string; bypass_mode: string }[], buildAll: string | null): string[] {
  if (!bypass.some((b) => b.actor_type === "Integration" && b.bypass_mode === "always")) return [];
  if (buildAll === null) return [`${EXPECTED_FILE} lets an App bypass in mode always, and ${BUILD_ALL} does not exist: nothing audits what it pushes`];
  const v: string[] = [];
  if (!/check-main-pushes\.ts --live/.test(buildAll)) v.push(`${EXPECTED_FILE} lets an App bypass in mode always, and ${BUILD_ALL} no longer runs check-main-pushes.ts --live: nothing audits what it pushes (NP-SW-CI-001 §6.21)`);
  if (!/^  schedule:/m.test(buildAll)) v.push(`${BUILD_ALL} has no schedule: trigger, so the audit of the App's pushes never runs by itself`);
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
    if (!viaPullRequest(r)) r.files = gh(`repos/${repo}/commits/${c.sha}`, `.files[]? | {filename, previous_filename, patch}`).map((l) => JSON.parse(l) as FileRef);
    return r;
  });
}

function report(records: CommitRecord[], ok: string): never {
  const violations = audit(records);
  if (violations.length) {
    for (const x of violations) console.error(`  ✗ ${x}`);
    console.error(`check-main-pushes: ${violations.length} violation(s). The revision App's bypass (NP-SW-CI-001 §6.21, OI-SWCI-54 Q1) is for ${ALLOWED_PREFIX} only.`);
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
  const merged: Pull[] = [{ number: 1, merged_at: "2026-11-20T00:00:00Z" }];
  const rec = (sha: string, pulls: Pull[], files?: FileRef[]): CommitRecord => ({ sha: sha.padEnd(40, "0"), date: "2026-11-20", pulls, files });
  const f = (...names: string[]): FileRef[] => names.map((filename) => ({ filename }));

  expect("a merged pull request may touch anything", audit([rec("aaaaaaa", merged, undefined)]), null);
  expect("a direct push under docs/ only", audit([rec("bbbbbbb", [], f("docs/np_x_001.md", "docs/status/log.md"))]), null);
  expect("a direct push with no files", audit([rec("ccccccc", [], [])]), null);
  expect("a direct push that adds firmware", audit([rec("ddddddd", [], f("docs/np_x_001.md", "firmware/hub_control/src/a.c"))]), /ddddddd.*firmware\/hub_control\/src\/a\.c/);
  expect("the prefix is a directory, not a string", audit([rec("eeeeeee", [], f("docs-extra/x.md"))]), /docs-extra\/x\.md/);
  expect("a rename out of an outside path into docs/", audit([rec("fffffff", [], [{ filename: "docs/moved.md", previous_filename: "scripts/moved.ts" }])]), /scripts\/moved\.ts/);
  expect("a rename out of docs/", audit([rec("1111111", [], [{ filename: "app/moved.md", previous_filename: "docs/moved.md" }])]), /app\/moved\.md/);
  expect("an open pull request is not a merge", audit([rec("2222222", [{ number: 9, merged_at: null }], f("firmware/a.c"))]), /2222222/);
  expect("no pull request and no file list fails closed", audit([rec("3333333", [])]), /files were not read/);
  expect("an empty window is clean", audit([]), null);
  // The shape gh's --jq '{filename, previous_filename}' returns: the key is present and null when nothing was renamed.
  // The first --live run on GitHub crashed on it (TypeError on null.startsWith), because every case above omits the key.
  expect("a direct push under docs/ as gh returns it (previous_filename: null)", audit([rec("8888888", [], [{ filename: "docs/np_x_001.md", previous_filename: null }])]), null);
  expect("a direct push outside docs/ as gh returns it (previous_filename: null)", audit([rec("9999999", [], [{ filename: "scripts/x.ts", previous_filename: null }])]), /9999999.*scripts\/x\.ts/);
  const three = audit([rec("4444444", merged), rec("5555555", [], f("ci/x.txt")), rec("6666666", [], f("docs/y.md"))]);
  expect("only the bad commit is named", three.filter((x) => /4444444|6666666/.test(x)), null);
  expect("and it is named", three, /5555555.*ci\/x\.txt/);
  expect("many paths are shortened, not dropped", audit([rec("7777777", [], f("a1", "a2", "a3", "a4", "a5", "a6", "a7"))]), /and 2 more/);
  // CLAUDE.md: the App sets its revision line (assign-revisions.yml since #1003); nothing else in it.
  const bump = "@@ -1,7 +1,7 @@\n # CLAUDE.md\n \n-**Revision:** 87 (current)  \n+**Revision:** 88 (current)  \n **Status:** x";
  const cm = (patch: string | null, extra: Partial<FileRef> = {}): FileRef[] => [{ filename: "CLAUDE.md", previous_filename: null, patch, ...extra }];
  expect("the App's CLAUDE.md revision bump (as eebd299 on 2026-11-23)", audit([rec("aaaa001", [], [...cm(bump), { filename: "docs/np_dhf_001.md", previous_filename: null }])]), null);
  expect("CLAUDE.md with any other line changed", audit([rec("aaaa002", [], cm(bump + "\n-- old rule\n+- new rule"))]), /aaaa002.*CLAUDE\.md \(beyond its revision line\)/);
  expect("CLAUDE.md changed with no revision line at all", audit([rec("aaaa003", [], cm("@@ -9 +9 @@\n-a\n+b"))]), /aaaa003.*CLAUDE\.md/);
  expect("CLAUDE.md with no patch from GitHub fails closed", audit([rec("aaaa004", [], cm(null))]), /aaaa004.*CLAUDE\.md/);
  expect("CLAUDE.md with an empty patch fails closed", audit([rec("aaaa005", [], cm("@@ -1 +1 @@\n context"))]), /aaaa005.*CLAUDE\.md/);
  expect("a revision line elsewhere is not a CLAUDE.md pass", audit([rec("aaaa006", [], [{ filename: "README.md", previous_filename: null, patch: bump }])]), /aaaa006.*README\.md/);
  expect("a file renamed onto CLAUDE.md", audit([rec("aaaa007", [], cm(bump, { previous_filename: "scripts/x.md" }))]), /aaaa007.*scripts\/x\.md/);
  expect("CLAUDE.md renamed away", audit([rec("aaaa008", [], [{ filename: "NOTES.md", previous_filename: "CLAUDE.md", patch: bump }])]), /aaaa008.*NOTES\.md/);
  expect("the workflow commits what the audit allows", grantViolations("      - run: |\n          git add -A docs CLAUDE.md\n"), null);
  expect("the workflow widened without the audit", grantViolations("          git add -A docs CLAUDE.md scripts\n"), /commits 'scripts'/);
  expect("the workflow commits everything", grantViolations("          git add -A .\n"), /commits '\.'/);
  expect("a workflow whose git add cannot be read", grantViolations("steps: []\n"), /no 'git add' line/);
  const appAlways = [{ actor_type: "Integration", bypass_mode: "always" }];
  const wired = "on:\n  schedule:\n    - cron: '0 3 * * 0'\njobs:\n  a:\n    steps:\n      - run: bun scripts/check-main-pushes.ts --live\n";
  expect("an App bypass with its audit wired", wiringViolations(appAlways, wired), null);
  expect("an App bypass whose audit step was deleted", wiringViolations(appAlways, wired.replace("check-main-pushes.ts --live", "true")), /no longer runs/);
  expect("an App bypass whose schedule was deleted", wiringViolations(appAlways, wired.replace("  schedule:", "  workflow_dispatch:")), /no schedule/);
  expect("an App bypass with no build-all.yml", wiringViolations(appAlways, null), /does not exist/);
  expect("no App bypass needs no audit", wiringViolations([{ actor_type: "RepositoryRole", bypass_mode: "pull_request" }], "on:\n"), null);
  expect("an App bypass in pull_request mode needs no audit", wiringViolations([{ actor_type: "Integration", bypass_mode: "pull_request" }], "on:\n"), null);
  cases++;
  if (sinceIso(new Date("2026-11-21T12:00:00Z"), 14) !== "2026-11-07T12:00:00.000Z") { failed++; console.error("FAIL since: 14 days before 2026-11-21T12:00Z"); }

  if (failed) {
    console.error(`check-main-pushes self-test: ${failed} of ${cases} case(s) FAILED`);
    process.exit(1);
  }
  console.log(`check-main-pushes self-test: ${cases} cases passed`);
  process.exit(0);
}

function main(): void {
  const args = process.argv.slice(2);
  if (args[0] === "--self-test") return selfTest();
  if (args[0] === "--from") {
    if (!args[1]) { console.error("usage: --from <records.json>"); process.exit(2); }
    return report(JSON.parse(readFileSync(args[1], "utf8")) as CommitRecord[], `check-main-pushes: OK — every commit in the file arrived by a merged pull request or touches only ${ALLOWED_PREFIX} and revision lines`);
  }
  if (args[0] === "--live") {
    const root = resolve(import.meta.dir, "..");
    const expectedFile = join(root, "ci/main-ruleset.json");
    if (!existsSync(expectedFile)) { console.error("check-main-pushes: ci/main-ruleset.json is missing; it names the repository"); process.exit(2); }
    const repo = (JSON.parse(readFileSync(expectedFile, "utf8")) as { repository: string }).repository;
    const i = args.indexOf("--days");
    const days = i >= 0 ? Number(args[i + 1]) : WINDOW_DAYS;
    if (!Number.isFinite(days) || days <= 0) { console.error("check-main-pushes: --days must be a positive number"); process.exit(2); }
    return report(live(repo, days), `check-main-pushes: OK — every commit on main in the last ${days} days arrived by a merged pull request or touches only ${ALLOWED_PREFIX} and revision lines`);
  }
  if (args.length === 0) {
    const root = resolve(import.meta.dir, "..");
    const expectedFile = join(root, EXPECTED_FILE);
    if (!existsSync(expectedFile)) { console.error(`check-main-pushes: ${EXPECTED_FILE} is missing`); process.exit(2); }
    const e = JSON.parse(readFileSync(expectedFile, "utf8")) as { bypass_actors?: { actor_type: string; bypass_mode: string }[] };
    const ba = join(root, BUILD_ALL);
    const ar = join(root, ASSIGN_REVISIONS);
    const v = [
      ...wiringViolations(e.bypass_actors ?? [], existsSync(ba) ? readFileSync(ba, "utf8") : null),
      ...grantViolations(existsSync(ar) ? readFileSync(ar, "utf8") : null),
    ];
    // The revision line the audit allows is the one the App writes.
    const lib = join(root, REVISION_LIB);
    const m = existsSync(lib) ? /export const CLAUDE_REV_LINE = \/(.*)\/m;/.exec(readFileSync(lib, "utf8")) : null;
    if (m === null) v.push(`${REVISION_LIB} has no CLAUDE_REV_LINE this check can read: the CLAUDE.md line the audit allows is not compared with the one the App writes`);
    else if (m[1] !== CLAUDE_REV_LINE_SOURCE) v.push(`CLAUDE_REV_LINE in ${REVISION_LIB} is /${m[1]}/ and check-main-pushes.ts allows /${CLAUDE_REV_LINE_SOURCE}/: change them together`);
    if (v.length) {
      for (const x of v) console.error(`  ✗ ${x}`);
      console.error(`check-main-pushes: ${v.length} violation(s)`);
      process.exit(1);
    }
    console.log(`check-main-pushes: OK — the App bypass in ${EXPECTED_FILE} has its audit in ${BUILD_ALL}, and what ${ASSIGN_REVISIONS} commits is what the audit allows (offline; --live reads GitHub)`);
    console.log(`scanned: ${(e.bypass_actors ?? []).length}`);
    process.exit(0);
  }
  console.error("usage: bun scripts/check-main-pushes.ts [--self-test | --live [--days N] | --from <records.json>]");
  process.exit(2);
}

if (import.meta.main) main();
