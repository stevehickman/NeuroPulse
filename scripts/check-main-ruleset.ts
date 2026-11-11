#!/usr/bin/env bun
/**
 * check-main-ruleset.ts — the protection on `main` is written down, and GitHub is compared with it.
 *
 * NP-SW-CI-001 §6.17, OI-SWCI-54. The `Safety` ruleset is what stops a change reaching `main`
 * without its checks: a pull-request rule, the required status-check contexts, and a bypass list.
 * It lives in GitHub's settings, where no pull request shows an edit to it. Until this file, the
 * in-repo half was ci/required-checks.txt (the contexts only) and one `diff` in that file's header,
 * run by hand by whoever remembered. A context removed in the settings page, the pull-request rule
 * switched off, or the admin bypass put back to "always" would each have left every check green.
 *
 * Two files state the expected protection:
 *
 *   ci/required-checks.txt   the required contexts, one per line (scripts/check-ci-scope.ts
 *                            correspondence C proves each is reportable; it is not restated here)
 *   ci/main-ruleset.json     everything else: the rule types in force, the pull-request rule's
 *                            parameters, the bypass actors, and the App the contexts are bound to
 *
 * Default mode (offline, every pull request that touches ci/ or a workflow). The expected state
 * must itself be a protection:
 *
 *   R1  the ruleset is `active` and targets the default branch, with no exclusion
 *   R2  `deletion`, `non_fast_forward`, `pull_request` and `required_status_checks` are all expected
 *   R3  no person or role may bypass in mode `always`: only `pull_request` (OI-SWCI-22). An
 *       `always` bypass pushes to `main` with no pull request and so with no check
 *   R4  an App (`Integration`) that bypasses names the workflow that needs it, and that file
 *       exists. A bypass does not outlive the workflow it was granted for
 *   R5  the manifest is not empty, has no duplicate, and the contexts are bound to one App id
 *   R6  the expected pull-request rule states every parameter this gate compares
 *
 * `--live` (weekly in build-all.yml, and by hand after any settings change). GitHub must equal the
 * expected state, in both directions, and each direction says what it costs:
 *
 *   L1  a context GitHub requires that the manifest lacks: nothing proves a job reports it, and a
 *       required context nobody reports blocks every merge
 *   L2  a manifest context GitHub does not require: that gate has silently stopped applying
 *   L3  a context bound to another App id, or to none (any App could then satisfy it)
 *   L4  a rule type expected but not in force on `main` (the protection shrank), or in force but
 *       not expected (the settings changed with no record)
 *   L5  a pull-request parameter or the strict flag that differs
 *   L6  enforcement, target or branch conditions that differ
 *   L7  the bypass list differs. GitHub returns it only to a token that may edit the ruleset, so a
 *       read with less says "not visible" and passes, unless --require-bypass is given
 *   L8  a rule on `main` that comes from a different ruleset
 *
 * `--live-from <ruleset.json> <rules.json>` runs the same comparison on two saved responses
 * (`gh api repos/O/R/rulesets/ID` and `gh api repos/O/R/rules/branches/main`).
 *
 * `--contexts` prints every job of every workflow with whether its name could be made a required
 * context, and why not. It is a report for the person editing the ruleset and decides nothing.
 *
 * What this does NOT do: change a setting. It reads. Applying a change is the principal's, by the
 * runbook in docs/human-input/oi-swci-54-main-ruleset.md.
 *
 * Usage: bun scripts/check-main-ruleset.ts [--self-test | --live [--require-bypass] |
 *        --live-from <ruleset.json> <rules.json> [--require-bypass] | --contexts]
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-main-ruleset.ts --self-test
 * CI-Scans: the expected protection of main (ci/main-ruleset.json, ci/required-checks.txt) and the workflow each bypassing App names
 * CI-Scan-Paths: ci/** .github/workflows/** scripts/check-main-ruleset.ts
 */
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from "fs";
import { join, resolve } from "path";
import { tmpdir } from "os";

export const EXPECTED_FILE = "ci/main-ruleset.json";
export const MANIFEST_FILE = "ci/required-checks.txt";
const CORE_RULES = ["deletion", "non_fast_forward", "pull_request", "required_status_checks"];
const PR_PARAMS = [
  "required_approving_review_count", "dismiss_stale_reviews_on_push", "require_code_owner_review",
  "require_last_push_approval", "required_review_thread_resolution", "allowed_merge_methods",
];

type Bypass = { actor_type: string; actor_id: number | null; bypass_mode: string; who?: string; workflow?: string };
export type Expected = {
  repository: string;
  ruleset_id: number;
  name: string;
  enforcement: string;
  target: string;
  include: string[];
  exclude: string[];
  rule_types: string[];
  status_checks: { strict: boolean; integration_id: number };
  pull_request: Record<string, unknown>;
  bypass_actors: Bypass[];
};

/** The manifest's contexts: every line that is not blank and not a `#` comment, verbatim. */
export function manifestContexts(text: string): string[] {
  return text.split("\n").map((l) => l.replace(/\r$/, "")).filter((l) => l.trim() !== "" && !l.startsWith("#"));
}

const same = (a: unknown, b: unknown): boolean => {
  const norm = (v: unknown): unknown => (Array.isArray(v) ? [...v].map(norm).sort() : v);
  return JSON.stringify(norm(a)) === JSON.stringify(norm(b));
};
const bypassKey = (b: { actor_type: string; actor_id: number | null; bypass_mode: string }): string =>
  `${b.actor_type} ${b.actor_id ?? "-"} (${b.bypass_mode})`;

/** R1–R6: the expected state is itself a protection. `has(path)` says whether a file exists in the tree. */
export function checkExpected(e: Expected, contexts: string[], has: (rel: string) => boolean): string[] {
  const v: string[] = [];
  if (e.enforcement !== "active") v.push(`R1: enforcement is '${e.enforcement}', not 'active': a disabled or evaluate-only ruleset blocks nothing`);
  if (e.target !== "branch" || !e.include?.includes("~DEFAULT_BRANCH")) v.push("R1: the ruleset does not target the default branch (~DEFAULT_BRANCH)");
  if ((e.exclude ?? []).length > 0) v.push(`R1: the ruleset excludes ${e.exclude.join(", ")}: an excluded ref is unprotected`);
  for (const t of CORE_RULES) {
    if (!(e.rule_types ?? []).includes(t)) v.push(`R2: rule '${t}' is not expected: without it ${t === "pull_request" ? "a direct push to main skips every check (OI-SWCI-22)" : t === "required_status_checks" ? "no check is required" : "history on main can be rewritten or the branch deleted"}`);
  }
  for (const b of e.bypass_actors ?? []) {
    if (b.actor_type !== "Integration" && b.bypass_mode !== "pull_request") {
      v.push(`R3: ${bypassKey(b)} may bypass outside a pull request: that actor can push to main with no check (OI-SWCI-22). Only 'pull_request' mode is allowed for a person or a role`);
    }
    if (b.actor_type === "Integration") {
      if (!b.workflow) v.push(`R4: App ${b.actor_id} bypasses but names no workflow that needs it`);
      else if (!has(b.workflow)) v.push(`R4: App ${b.actor_id} bypasses for ${b.workflow}, which does not exist: remove the bypass with the workflow`);
    }
  }
  if (contexts.length === 0) v.push(`R5: ${MANIFEST_FILE} lists no context: nothing would be required`);
  const dup = contexts.filter((c, i) => contexts.indexOf(c) !== i);
  if (dup.length) v.push(`R5: ${MANIFEST_FILE} lists ${[...new Set(dup)].map((d) => `'${d}'`).join(", ")} more than once`);
  if (!Number.isInteger(e.status_checks?.integration_id)) v.push("R5: status_checks.integration_id is not a number: a context bound to no App can be reported by any App");
  if (typeof e.status_checks?.strict !== "boolean") v.push("R5: status_checks.strict is not stated");
  for (const p of PR_PARAMS) if (!(p in (e.pull_request ?? {}))) v.push(`R6: pull_request.${p} is not stated, so a change to it on GitHub would not be seen`);
  return v;
}

type LiveRule = { type: string; ruleset_id?: number; parameters?: Record<string, any> };
type LiveRuleset = { name?: string; enforcement?: string; target?: string; conditions?: { ref_name?: { include?: string[]; exclude?: string[] } }; bypass_actors?: Bypass[] };

/** L1–L8: GitHub against the expected state. `rules` is what is in force on main, `ruleset` the ruleset itself. */
export function compareLive(
  e: Expected, contexts: string[], ruleset: LiveRuleset, rules: LiveRule[], requireBypass = false,
): { violations: string[]; notes: string[] } {
  const v: string[] = [];
  const notes: string[] = [];
  const checks = rules.filter((r) => r.type === "required_status_checks");
  const live = checks.flatMap((r) => (r.parameters?.required_status_checks ?? []) as { context: string; integration_id?: number | null }[]);
  const liveNames = live.map((c) => c.context);
  for (const c of liveNames) {
    if (!contexts.includes(c)) v.push(`L1: GitHub requires '${c}', which ${MANIFEST_FILE} does not list: nothing proves a job reports it, and a required context nobody reports blocks every merge`);
  }
  for (const c of contexts) {
    if (!liveNames.includes(c)) v.push(`L2: '${c}' is in ${MANIFEST_FILE} but GitHub does not require it: that gate does not apply to main`);
  }
  for (const c of live) {
    if (c.integration_id !== e.status_checks.integration_id) v.push(`L3: '${c.context}' is bound to App ${c.integration_id ?? "none"}, expected ${e.status_checks.integration_id}`);
  }
  const liveTypes = [...new Set(rules.map((r) => r.type))];
  for (const t of e.rule_types) if (!liveTypes.includes(t)) v.push(`L4: rule '${t}' is expected but not in force on main: the protection shrank`);
  for (const t of liveTypes) if (!e.rule_types.includes(t)) v.push(`L4: rule '${t}' is in force on main but not in ${EXPECTED_FILE}: the settings changed with no record`);
  for (const r of checks) {
    const strict = r.parameters?.strict_required_status_checks_policy;
    if (strict !== e.status_checks.strict) v.push(`L5: strict_required_status_checks_policy is ${strict}, expected ${e.status_checks.strict}`);
  }
  for (const r of rules.filter((x) => x.type === "pull_request")) {
    const keys = new Set([...Object.keys(e.pull_request), ...Object.keys(r.parameters ?? {})]);
    for (const k of keys) {
      if (!same(e.pull_request[k], r.parameters?.[k])) v.push(`L5: pull_request.${k} is ${JSON.stringify(r.parameters?.[k])}, expected ${JSON.stringify(e.pull_request[k])}`);
    }
  }
  if (ruleset.enforcement !== e.enforcement) v.push(`L6: enforcement is '${ruleset.enforcement}', expected '${e.enforcement}'`);
  if (ruleset.name !== e.name) v.push(`L6: the ruleset is named '${ruleset.name}', expected '${e.name}'`);
  if (ruleset.target !== e.target) v.push(`L6: target is '${ruleset.target}', expected '${e.target}'`);
  const ref = ruleset.conditions?.ref_name ?? {};
  if (!same(ref.include ?? [], e.include)) v.push(`L6: the ruleset includes ${JSON.stringify(ref.include ?? [])}, expected ${JSON.stringify(e.include)}`);
  if (!same(ref.exclude ?? [], e.exclude)) v.push(`L6: the ruleset excludes ${JSON.stringify(ref.exclude ?? [])}, expected ${JSON.stringify(e.exclude)}`);
  if (ruleset.bypass_actors === undefined) {
    const msg = "the bypass list is not visible to this token (GitHub returns it only to a token that may edit the ruleset)";
    if (requireBypass) v.push(`L7: ${msg}, and --require-bypass was given`);
    else notes.push(`L7 not compared: ${msg}`);
  } else {
    const want = e.bypass_actors.map(bypassKey);
    const got = ruleset.bypass_actors.map(bypassKey);
    for (const g of got) if (!want.includes(g)) v.push(`L7: ${g} is on the bypass list but not in ${EXPECTED_FILE}`);
    for (const w of want) if (!got.includes(w)) v.push(`L7: ${w} is expected on the bypass list but is not there`);
  }
  for (const r of rules) {
    if (r.ruleset_id !== undefined && r.ruleset_id !== e.ruleset_id) v.push(`L8: rule '${r.type}' on main comes from ruleset ${r.ruleset_id}, not ${e.ruleset_id}`);
  }
  return { violations: v, notes };
}

// ── --contexts: which job names could be required ────────────────────────────

export type JobRow = { file: string; id: string; name: string; eligible: boolean; why: string };

/**
 * Read the workflows as text. A job's check context is its rendered `name:`. It can be required only
 * if a pull request always gets a report under exactly that string: the workflow runs on pull_request
 * with no `paths:` filter, the job has no `strategy:`, the name holds no expression, and no other
 * pull-request job has the same name. A skipped job still reports, so an `if:` is not a bar.
 */
export function jobRows(files: { file: string; text: string }[]): JobRow[] {
  const rows: (JobRow & { pr: boolean })[] = [];
  for (const { file, text } of files) {
    const lines = text.split("\n");
    const top = (key: string): number => lines.findIndex((l) => new RegExp(`^${key}:`).test(l));
    const block = (start: number): string[] => {
      const out: string[] = [];
      for (let i = start + 1; i < lines.length && !/^\S/.test(lines[i]!); i++) out.push(lines[i]!);
      return out;
    };
    const onAt = top("on");
    const on = onAt < 0 ? [] : [lines[onAt]!, ...block(onAt)];
    const prAt = on.findIndex((l) => /^\s{2}pull_request:/.test(l) || /^on:.*\bpull_request\b/.test(l));
    const pr = prAt >= 0;
    let filtered = false;
    for (let i = prAt + 1; pr && i < on.length && /^\s{4,}\S|^\s*(#.*)?$/.test(on[i]!); i++) {
      if (/^\s{4}paths(-ignore)?:/.test(on[i]!)) filtered = true;
    }
    const jobsAt = top("jobs");
    if (jobsAt < 0) continue;
    const body = block(jobsAt);
    for (let i = 0; i < body.length; i++) {
      const m = /^ {2}([A-Za-z0-9_-]+):\s*$/.exec(body[i]!);
      if (!m) continue;
      let name = m[1]!;
      let matrix = false;
      for (let j = i + 1; j < body.length && !/^ {2}\S/.test(body[j]!); j++) {
        const n = /^ {4}name:\s*(.+?)\s*$/.exec(body[j]!);
        if (n) name = n[1]!.replace(/^(['"])(.*)\1$/, "$2");
        if (/^ {4}strategy:/.test(body[j]!)) matrix = true;
      }
      const why = !pr ? "the workflow does not run on pull_request, so it never reports on one"
        : filtered ? "the workflow has a paths: filter, so it does not report on a pull request outside it"
        : matrix ? "the job has a strategy:, so its check is named 'name (value)'"
        : name.includes("${{") ? "the name holds an expression"
        : "";
      rows.push({ file, id: m[1]!, name, eligible: why === "", why, pr });
    }
  }
  for (const r of rows) {
    if (!r.eligible) continue;
    const twins = rows.filter((o) => o.pr && o.name === r.name);
    if (twins.length > 1) {
      r.eligible = false;
      r.why = `${twins.length} pull-request jobs are named '${r.name}' (${[...new Set(twins.map((t) => t.file))].join(", ")}): rename before requiring`;
    }
  }
  return rows.map(({ pr: _pr, ...r }) => r);
}

function workflowFiles(root: string): { file: string; text: string }[] {
  const dir = join(root, ".github/workflows");
  return readdirSync(dir).filter((f) => /\.ya?ml$/.test(f)).sort()
    .map((f) => ({ file: f, text: readFileSync(join(dir, f), "utf8") }));
}

// ── Running it ───────────────────────────────────────────────────────────────

function load(root: string): { expected: Expected; contexts: string[] } {
  for (const f of [EXPECTED_FILE, MANIFEST_FILE]) {
    if (!existsSync(join(root, f))) {
      console.error(`check-main-ruleset: ${f} is missing. Without it this gate would compare nothing and pass.`);
      process.exit(1);
    }
  }
  return {
    expected: JSON.parse(readFileSync(join(root, EXPECTED_FILE), "utf8")) as Expected,
    contexts: manifestContexts(readFileSync(join(root, MANIFEST_FILE), "utf8")),
  };
}

function report(violations: string[], notes: string[], ok: string): never {
  for (const n of notes) console.log(`note: ${n}`);
  if (violations.length) {
    for (const x of violations) console.error(`  ✗ ${x}`);
    console.error(`check-main-ruleset: ${violations.length} violation(s)`);
    process.exit(1);
  }
  console.log(ok);
  process.exit(0);
}

function ghJson(path: string): unknown {
  const r = Bun.spawnSync(["gh", "api", path], { stdout: "pipe", stderr: "pipe" });
  if (r.exitCode !== 0) {
    console.error(`check-main-ruleset: 'gh api ${path}' failed (${new TextDecoder().decode(r.stderr).trim().split("\n").pop()}). Nothing was compared.`);
    process.exit(2);
  }
  return JSON.parse(new TextDecoder().decode(r.stdout));
}

function main(): void {
  const args = process.argv.slice(2);
  if (args[0] === "--self-test") return selfTest();
  const root = resolve(import.meta.dir, "..");

  if (args[0] === "--contexts") {
    const rows = jobRows(workflowFiles(root));
    const required = existsSync(join(root, MANIFEST_FILE)) ? manifestContexts(readFileSync(join(root, MANIFEST_FILE), "utf8")) : [];
    for (const r of rows) {
      const state = required.includes(r.name) ? "REQUIRED " : r.eligible ? "eligible " : "NOT      ";
      console.log(`${state} ${r.file} :: ${r.name}${r.why ? `\n            ${r.why}` : ""}`);
    }
    console.log(`${rows.length} jobs: ${rows.filter((r) => required.includes(r.name)).length} required, ${rows.filter((r) => r.eligible && !required.includes(r.name)).length} eligible and not required, ${rows.filter((r) => !r.eligible).length} cannot be required as they stand`);
    return;
  }

  const { expected, contexts } = load(root);
  const offline = checkExpected(expected, contexts, (rel) => existsSync(join(root, rel)));
  const requireBypass = args.includes("--require-bypass");

  if (args[0] === "--live" || args[0] === "--live-from") {
    let ruleset: LiveRuleset, rules: LiveRule[];
    if (args[0] === "--live-from") {
      if (!args[1] || !args[2]) { console.error("usage: --live-from <ruleset.json> <rules.json>"); process.exit(2); }
      ruleset = JSON.parse(readFileSync(args[1], "utf8"));
      rules = JSON.parse(readFileSync(args[2], "utf8"));
    } else {
      ruleset = ghJson(`repos/${expected.repository}/rulesets/${expected.ruleset_id}`) as LiveRuleset;
      rules = ghJson(`repos/${expected.repository}/rules/branches/main`) as LiveRule[];
    }
    const { violations, notes } = compareLive(expected, contexts, ruleset, rules, requireBypass);
    report([...offline, ...violations], notes,
      `check-main-ruleset: OK — GitHub requires the ${contexts.length} manifest contexts on main, and the ruleset matches ${EXPECTED_FILE}\nscanned: ${contexts.length + rules.length}`);
  }

  report(offline, [],
    `check-main-ruleset: OK — the expected protection holds ${contexts.length} contexts, ${expected.rule_types.length} rule types and ${expected.bypass_actors.length} bypass actors (offline; --live compares GitHub)\nscanned: ${contexts.length + expected.rule_types.length + expected.bypass_actors.length}`);
}

// ── Self-test ────────────────────────────────────────────────────────────────

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
  const clone = <T>(x: T): T => JSON.parse(JSON.stringify(x));

  const E: Expected = {
    repository: "o/r", ruleset_id: 7, name: "Safety", enforcement: "active", target: "branch",
    include: ["~DEFAULT_BRANCH"], exclude: [],
    rule_types: ["deletion", "non_fast_forward", "pull_request", "required_status_checks"],
    status_checks: { strict: false, integration_id: 15368 },
    pull_request: {
      required_approving_review_count: 0, dismiss_stale_reviews_on_push: false, require_code_owner_review: false,
      require_last_push_approval: false, required_review_thread_resolution: false,
      allowed_merge_methods: ["merge", "squash", "rebase"],
    },
    bypass_actors: [
      { actor_type: "RepositoryRole", actor_id: 5, bypass_mode: "pull_request" },
      { actor_type: "Integration", actor_id: 99, bypass_mode: "always", workflow: ".github/workflows/bot.yml" },
    ],
  };
  const C = ["Scope", "Host tests (Class C)"];
  const has = (rel: string): boolean => rel === ".github/workflows/bot.yml";
  const off = (mut: (e: Expected) => void, ctx = C, h = has): string[] => { const e = clone(E); mut(e); return checkExpected(e, ctx, h); };

  // Offline. The unchanged fixture passes; each single mutation fails on its own rule.
  expect("offline-clean", off(() => {}), null);
  expect("R1-disabled", off((e) => { e.enforcement = "disabled"; }), /^R1: enforcement/);
  expect("R1-evaluate", off((e) => { e.enforcement = "evaluate"; }), /^R1: enforcement/);
  expect("R1-other-branch", off((e) => { e.include = ["refs/heads/release"]; }), /^R1: .*default branch/);
  expect("R1-exclude", off((e) => { e.exclude = ["refs/heads/main"]; }), /^R1: .*excludes/);
  expect("R2-no-pull-request", off((e) => { e.rule_types = e.rule_types.filter((t) => t !== "pull_request"); }), /^R2: rule 'pull_request'.*OI-SWCI-22/);
  expect("R2-no-status-checks", off((e) => { e.rule_types = e.rule_types.filter((t) => t !== "required_status_checks"); }), /^R2: rule 'required_status_checks'/);
  expect("R2-no-force-push-rule", off((e) => { e.rule_types = e.rule_types.filter((t) => t !== "non_fast_forward"); }), /^R2: rule 'non_fast_forward'/);
  expect("R3-admin-always", off((e) => { e.bypass_actors[0]!.bypass_mode = "always"; }), /^R3: RepositoryRole 5 \(always\)/);
  expect("R3-team-always", off((e) => { e.bypass_actors.push({ actor_type: "Team", actor_id: 3, bypass_mode: "always" }); }), /^R3: Team 3/);
  expect("R4-app-no-workflow", off((e) => { delete e.bypass_actors[1]!.workflow; }), /^R4: App 99 bypasses but names no workflow/);
  expect("R4-app-workflow-gone", off(() => {}, C, () => false), /^R4: App 99 bypasses for .*does not exist/);
  expect("R5-empty-manifest", off(() => {}, []), /^R5: .*lists no context/);
  expect("R5-duplicate", off(() => {}, ["Scope", "Scope"]), /^R5: .*more than once/);
  expect("R5-unbound", off((e) => { (e.status_checks as any).integration_id = null; }), /^R5: status_checks.integration_id/);
  expect("R6-param-unstated", off((e) => { delete e.pull_request.required_approving_review_count; }), /^R6: pull_request.required_approving_review_count/);

  // The manifest reader: comments and blank lines are not contexts, and a context keeps its punctuation.
  const m = manifestContexts("# header\n\nScope\n# note\nHost tests (Class C)\r\n");
  if (JSON.stringify(m) !== JSON.stringify(C)) { failed++; console.error(`FAIL manifest-reader: got ${JSON.stringify(m)}`); }

  // Live. The fixture is the shape GitHub returns (read 2026-11-11), cut to two contexts.
  const RS: LiveRuleset = {
    name: "Safety", enforcement: "active", target: "branch",
    conditions: { ref_name: { include: ["~DEFAULT_BRANCH"], exclude: [] } },
    bypass_actors: [
      { actor_type: "RepositoryRole", actor_id: 5, bypass_mode: "pull_request" },
      { actor_type: "Integration", actor_id: 99, bypass_mode: "always" },
    ],
  };
  const RL: LiveRule[] = [
    { type: "deletion", ruleset_id: 7 },
    { type: "non_fast_forward", ruleset_id: 7 },
    { type: "required_status_checks", ruleset_id: 7, parameters: {
      strict_required_status_checks_policy: false,
      required_status_checks: [{ context: "Scope", integration_id: 15368 }, { context: "Host tests (Class C)", integration_id: 15368 }] } },
    { type: "pull_request", ruleset_id: 7, parameters: clone(E.pull_request) },
  ];
  const live = (mut: (rs: LiveRuleset, rl: LiveRule[]) => void, req = false, ctx = C): string[] => {
    const rs = clone(RS), rl = clone(RL); mut(rs, rl); return compareLive(E, ctx, rs, rl, req).violations;
  };
  const sc = (rl: LiveRule[]): any => rl.find((r) => r.type === "required_status_checks")!.parameters!;
  const prr = (rl: LiveRule[]): any => rl.find((r) => r.type === "pull_request")!.parameters!;

  expect("live-clean", live(() => {}), null);
  expect("live-clean-reordered", live((_rs, rl) => { sc(rl).required_status_checks.reverse(); prr(rl).allowed_merge_methods.reverse(); }), null);
  expect("L1-github-requires-unknown", live((_rs, rl) => { sc(rl).required_status_checks.push({ context: "Build all", integration_id: 15368 }); }), /^L1: GitHub requires 'Build all'.*blocks every merge/);
  expect("L1-renamed-on-github", live((_rs, rl) => { sc(rl).required_status_checks[1].context = "Host tests (Class C) "; }), /^L1: /);
  expect("L2-context-removed", live((_rs, rl) => { sc(rl).required_status_checks.pop(); }), /^L2: 'Host tests \(Class C\)'.*does not apply/);
  expect("L2-manifest-ahead", live(() => {}, false, [...C, "New gate"]), /^L2: 'New gate'/);
  expect("L3-other-app", live((_rs, rl) => { sc(rl).required_status_checks[0].integration_id = 1; }), /^L3: 'Scope' is bound to App 1/);
  expect("L3-any-source", live((_rs, rl) => { delete sc(rl).required_status_checks[0].integration_id; }), /^L3: 'Scope' is bound to App none/);
  expect("L4-pull-request-rule-off", live((_rs, rl) => { rl.splice(rl.findIndex((r) => r.type === "pull_request"), 1); }), /^L4: rule 'pull_request' is expected but not in force/);
  expect("L4-status-checks-off", live((_rs, rl) => { rl.splice(rl.findIndex((r) => r.type === "required_status_checks"), 1); }), /^L4: rule 'required_status_checks' is expected but not in force/);
  expect("L4-force-push-allowed", live((_rs, rl) => { rl.splice(rl.findIndex((r) => r.type === "non_fast_forward"), 1); }), /^L4: rule 'non_fast_forward'/);
  expect("L4-undeclared-rule", live((_rs, rl) => { rl.push({ type: "required_signatures", ruleset_id: 7 }); }), /^L4: rule 'required_signatures' is in force.*no record/);
  expect("L5-approvals", live((_rs, rl) => { prr(rl).required_approving_review_count = 1; }), /^L5: pull_request.required_approving_review_count is 1, expected 0/);
  expect("L5-merge-methods", live((_rs, rl) => { prr(rl).allowed_merge_methods = ["squash"]; }), /^L5: pull_request.allowed_merge_methods/);
  expect("L5-new-parameter", live((_rs, rl) => { prr(rl).require_extra_approval_for_unattributed_changes = true; }), /^L5: pull_request.require_extra_approval_for_unattributed_changes is true, expected undefined/);
  expect("L5-strict", live((_rs, rl) => { sc(rl).strict_required_status_checks_policy = true; }), /^L5: strict_required_status_checks_policy is true/);
  expect("L6-disabled", live((rs) => { rs.enforcement = "disabled"; }), /^L6: enforcement is 'disabled'/);
  expect("L6-exclude", live((rs) => { rs.conditions!.ref_name!.exclude = ["refs/heads/main"]; }), /^L6: the ruleset excludes/);
  expect("L6-retargeted", live((rs) => { rs.conditions!.ref_name!.include = ["refs/heads/dev"]; }), /^L6: the ruleset includes/);
  expect("L7-admin-back-to-always", live((rs) => { rs.bypass_actors![0]!.bypass_mode = "always"; }), /^L7: RepositoryRole 5 \(always\) is on the bypass list/);
  expect("L7-new-actor", live((rs) => { rs.bypass_actors!.push({ actor_type: "Team", actor_id: 4, bypass_mode: "pull_request" }); }), /^L7: Team 4/);
  expect("L7-actor-gone", live((rs) => { rs.bypass_actors!.pop(); }), /^L7: Integration 99 \(always\) is expected/);
  expect("L7-hidden-tolerated", live((rs) => { delete rs.bypass_actors; }), null);
  expect("L7-hidden-required", live((rs) => { delete rs.bypass_actors; }, true), /^L7: the bypass list is not visible/);
  expect("L8-second-ruleset", live((_rs, rl) => { rl.push({ type: "deletion", ruleset_id: 8 }); }), /^L8: rule 'deletion' on main comes from ruleset 8/);
  {
    const rs = clone(RS); delete rs.bypass_actors;
    const n = compareLive(E, C, rs, clone(RL)).notes;
    if (!n.some((x) => /^L7 not compared/.test(x))) { failed++; console.error("FAIL L7-hidden-note: a hidden bypass list must be said, not passed in silence"); }
  }

  // --contexts. One workflow of each kind, and the shared name that ten workflows here carry.
  const wf = (on: string, jobs: string): string => `name: W\n\non:\n${on}\n\npermissions:\n  contents: read\n\njobs:\n${jobs}\n`;
  const PR = "  push:\n    branches: [main]\n  pull_request:\n  workflow_dispatch:";
  const rows = jobRows([
    { file: "a.yml", text: wf(PR, "  changes:\n    name: Relevance\n    runs-on: x\n  build:\n    name: 'Build (Class C)'\n    if: ${{ always() }}\n    runs-on: x") },
    { file: "b.yml", text: wf(PR, "  changes:\n    name: Relevance\n    runs-on: x\n  m:\n    name: Package\n    strategy:\n      matrix:\n        os: [a, b]\n    runs-on: x\n  e:\n    name: Analyze (${{ matrix.language }})\n    runs-on: x\n  bare:\n    runs-on: x") },
    { file: "c.yml", text: wf("  pull_request:\n    # why\n    paths:\n      - 'x/**'\n  schedule:\n    - cron: '0 6 * * 1'", "  scan:\n    name: Scan\n    runs-on: x") },
    { file: "d.yml", text: wf("  schedule:\n    - cron: '0 6 * * 1'\n  workflow_dispatch:", "  all:\n    name: Build all\n    runs-on: x") },
    { file: "e.yml", text: wf("  pull_request:\n    branches: [main]\n  push:\n    paths:\n      - 'y/**'", "  only:\n    name: Branch-filtered only\n    runs-on: x") },
  ]);
  const row = (file: string, id: string): JobRow => rows.find((r) => r.file === file && r.id === id)!;
  const want = (label: string, r: JobRow, eligible: boolean, why: RegExp | null): void => {
    cases++;
    if (!r || r.eligible !== eligible || (why && !why.test(r.why))) { failed++; console.error(`FAIL contexts-${label}: ${JSON.stringify(r)}`); }
  };
  want("plain-job", row("a.yml", "build"), true, null);
  want("quoted-name", { ...row("a.yml", "build"), eligible: row("a.yml", "build").name === "Build (Class C)", why: "" }, true, null);
  want("shared-name", row("a.yml", "changes"), false, /2 pull-request jobs are named 'Relevance'/);
  want("matrix", row("b.yml", "m"), false, /strategy/);
  want("expression", row("b.yml", "e"), false, /expression/);
  want("unnamed-uses-id", { ...row("b.yml", "bare"), eligible: row("b.yml", "bare").name === "bare" && row("b.yml", "bare").eligible, why: "" }, true, null);
  want("paths-filter", row("c.yml", "scan"), false, /paths: filter/);
  want("not-pull-request", row("d.yml", "all"), false, /does not run on pull_request/);
  // A branches: filter on pull_request, and a paths: filter on push, do not stop a pull request reporting.
  want("push-paths-is-not-pr-paths", row("e.yml", "only"), true, null);
  if (rows.length !== 9) { failed++; console.error(`FAIL contexts-count: expected 9 jobs, got ${rows.length}`); }

  // End to end, through the files, in a tree of its own: a missing file must fail, not pass empty.
  const box = mkdtempSync(join(tmpdir(), "np-ruleset-"));
  try {
    mkdirSync(join(box, "ci"));
    mkdirSync(join(box, "scripts"));
    writeFileSync(join(box, "scripts/check-main-ruleset.ts"), readFileSync(import.meta.path));
    const run = (...a: string[]): number => Bun.spawnSync(["bun", "scripts/check-main-ruleset.ts", ...a], { cwd: box, stdout: "pipe", stderr: "pipe" }).exitCode;
    if (run() !== 1) { failed++; console.error("FAIL e2e-missing-files: an absent expected file must exit 1"); }
    writeFileSync(join(box, EXPECTED_FILE), JSON.stringify({ ...E, bypass_actors: [E.bypass_actors[0]] }));
    writeFileSync(join(box, MANIFEST_FILE), "# c\nScope\nHost tests (Class C)\n");
    if (run() !== 0) { failed++; console.error("FAIL e2e-clean: a sound expected state must exit 0"); }
    writeFileSync(join(box, "rs.json"), JSON.stringify({ ...RS, bypass_actors: [RS.bypass_actors![0]] }));
    writeFileSync(join(box, "rl.json"), JSON.stringify(RL));
    if (run("--live-from", "rs.json", "rl.json") !== 0) { failed++; console.error("FAIL e2e-live-clean: matching responses must exit 0"); }
    writeFileSync(join(box, "rl.json"), JSON.stringify(RL.filter((r) => r.type !== "pull_request")));
    if (run("--live-from", "rs.json", "rl.json") !== 1) { failed++; console.error("FAIL e2e-live-shrunk: a missing pull_request rule must exit 1"); }
    writeFileSync(join(box, MANIFEST_FILE), "# only a comment\n");
    if (run() !== 1) { failed++; console.error("FAIL e2e-empty-manifest: an empty manifest must exit 1"); }
  } finally {
    rmSync(box, { recursive: true, force: true });
  }

  if (failed) { console.error(`check-main-ruleset: ${failed} self-test assertion(s) FAILED`); process.exit(1); }
  console.log(`check-main-ruleset: self-test OK (${cases} rule cases, each the passing fixture or one mutation of it, and 5 end-to-end runs)`);
  process.exit(0);
}

if (import.meta.main) main();
