#!/usr/bin/env bun
/**
 * check-ci-scope.ts — a relevance list must cover what the job it gates actually reads.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-ci-scope.ts --self-test
 * CI-Scans: every PR-triggered workflow job, against the CMake build graph it configures and the CI-Scan-Paths of the gates it runs; ci/required-checks.txt against job names; ci/host-test-partition.txt against add_test() and every workflow's partition env
 * CI-Scan-Paths: .github/workflows/** firmware/** scripts/** ci/**
 *   (Declared here, not at the end: gatesOf() and check-gate-coverage.ts read
 *   only a file's first 80 lines, and this header outgrew them.)
 *
 * NP-SW-CI-001 §5.0, OI-SWCI-08. The governing principle is "build only what the
 * change could have affected", and its cost is enumerated path lists that can
 * drift from the thing they are meant to describe. Five instances of that drift
 * are on the record, EVERY ONE of them found by a human diffing a declaration
 * against a relevance list by hand:
 *
 *   check-doc-filenames  scans docs/ + docs/superseded/    its list saw scripts/** only
 *   check-gate-coverage  scans scripts/, ci/, workflows/   its list saw scripts/** + one workflow
 *   check-section-refs   scans the tracked tree            its list was web-ci.yml's
 *   sync-locales --check reads the iOS String Catalog      no list anywhere contained it
 *   (and the shape OI-SWCI-08 was originally raised for: a module in the CMake
 *    build graph missing from the `paths:` list of the workflow that builds it)
 *
 * Audit-dependence is the defect, not any one of those five. This file is the
 * mechanical comparison that was missing.
 *
 * ── The five correspondences it checks ───────────────────────────────────────
 *
 * A. BUILD GRAPH ↔ relevance list. For every PR-triggered job that runs `cmake
 *    -B`, the add_subdirectory() closure of the CMakeLists it configures is
 *    computed for that job's actual -D flags, and every module in it must be in
 *    scope for at least one PR-triggered job that builds it.
 *
 * B. CI-Scan-Paths ↔ relevance list. `CI-Scans:` is prose and cannot be
 *    compared to anything. Each gate therefore also declares `CI-Scan-Paths:`,
 *    the same population in the matcher's two pattern shapes, and every pattern
 *    must be covered by the relevance list of a PR-triggered job that runs the
 *    gate. `<tree>` is the limiting case — a gate that walks from the repository
 *    root — and is satisfiable ONLY by a job with no relevance gate at all,
 *    which is exactly §5.0's argument for check-section-refs made checkable.
 *
 * C. Required status-check contexts ↔ job names (OI-SWCI-26, OI-SWCI-23).
 *    ci/required-checks.txt lists the exact strings the `Safety` ruleset
 *    requires. Each must be the `name:` of exactly one job in a PR-triggered,
 *    un-`paths:`-filtered workflow, with no `strategy:`; none may name a job
 *    that never runs on pull_request (build-all.yml). Every job carrying the
 *    in-file "required-status-check CONTEXT" warning must be listed, and every
 *    listed job must carry it. A rename that used to un-gate the branch with no
 *    error anywhere now fails here. The LIVE ruleset is not read — CI holds no
 *    admin token — so manifest ↔ ruleset stays a runbook step (§6.7.8).
 *
 * D. Every relevance list covers its own workflow file (OI-SWCI-25). A
 *    workflow edit is the change most likely to break that workflow.
 *
 * E. Host-test partition (OI-SWCI-37, OI-SWCI-14). ci/host-test-partition.txt
 *    names every host test and its class. It must equal the add_test() names
 *    of the NP_BUILD_TESTS graph; every workflow's NP_SAFETY_TESTS must be
 *    byte-identical; that regex must put each name in its listed half; and
 *    every NP_{SAFETY,CLASS_B,TOTAL}_TEST_COUNT copy — build-all.yml's
 *    included, though it is not PR-triggered, because a stale copy there is
 *    the second OI-SWCI-37 instance — must equal the manifest's count.
 *
 * ── Where it is deliberately weaker than it could be ─────────────────────────
 *
 * The union rule in A ("gated by SOME job that builds it") is not "gated by the
 * job that builds it". That is not a compromise, it is the property that
 * matters: both firmware host-test jobs configure the whole super-project, so
 * each builds modules the other owns, and §5.0 decided ON PURPOSE that a
 * safety_mcu-only change must not rebuild the bootloader. Demanding per-job
 * coverage would report that decision as a defect. What must never be true is
 * that a module is built by jobs and gated by NONE — that is the silent
 * staleness build-all.yml exists to backstop, and this catches it on the PR.
 *
 * Only PR-triggered workflows count on both sides. build-all.yml runs weekly
 * and unconditionally; letting it satisfy a coverage claim would mean "this is
 * checked" where the truth is "this is checked within seven days".
 *
 * Coverage is compared PATTERN to PATTERN, not by matching files. `docs/**`
 * covers `docs/superseded/**` because of the matcher's prefix shape; it does not
 * cover `<tree>`, and no exact path covers a prefix.
 *
 * This does NOT check that a relevance list is minimal. Over-broad scoping costs
 * runner minutes; under-broad scoping costs coverage, silently. Only the second
 * is a correctness question, and only the second is checked here.
 */
import {
  readFileSync, readdirSync, existsSync, mkdtempSync, mkdirSync, writeFileSync, rmSync,
} from "fs";
import { join } from "path";
import { tmpdir } from "os";

// ── Pattern algebra (the two shapes scripts/ci-changed-scope.sh supports) ─────
//
// `prefix/**` and an exact path, and nothing else. A third shape appearing in a
// list is a hard error there; here it is a hard error too, for the same reason —
// a pattern this file does not understand would silently cover nothing and turn
// a coverage claim into a green tick.

const TREE = "<tree>";

/** Strip `#` comments and blanks exactly as ci-changed-scope.sh does. */
function cleanList(block: string): string[] {
  return block
    .split("\n")
    .map((l) => l.replace(/#.*$/, "").trim())
    .filter((l) => l.length > 0);
}

function assertShape(p: string, where: string): void {
  if (p === TREE) return;
  if (p.endsWith("/**")) return;
  if (/[*?[\]!]/.test(p)) {
    throw new Error(`${where}: unsupported pattern shape '${p}' (only 'prefix/**', an exact path, or ${TREE})`);
  }
}

/** Does list entry `entry` cover everything `target` denotes? */
function entryCovers(entry: string, target: string): boolean {
  if (target === TREE) return false; // only an ungated job covers the tree
  if (entry === TREE) return true;
  if (entry.endsWith("/**")) {
    const prefix = entry.slice(0, -2); // keeps the trailing slash — the boundary case
    const t = target.endsWith("/**") ? target.slice(0, -2) : target;
    return t.startsWith(prefix);
  }
  return !target.endsWith("/**") && entry === target;
}

const listCovers = (list: string[], target: string): boolean =>
  list.some((e) => entryCovers(e, target));

// ── Workflow model ───────────────────────────────────────────────────────────
//
// Line-based, like check-gate-coverage.ts, and for the same reason: no YAML
// dependency is worth adding to a repository mid-way through a Class C SOUP
// exercise for a shape this regular. Anything it cannot parse throws rather
// than being skipped.

type Job = {
  wf: string;
  id: string;
  /** The job's own `name:` — the check context GitHub reports. null = none (the id is used). */
  name: string | null;
  /** A `strategy:` renames the check to "name (value)" per leg. */
  hasStrategy: boolean;
  /** Carries the in-file "required-status-check CONTEXT" warning comment. */
  requiredMarker: boolean;
  ifExpr: string;
  runs: string[];
  /** Raw NP_SCOPE_* block scalars declared on this job. */
  scopeEnv: Map<string, string[]>;
  /** Job outputs that resolve to a relevance list (the `changes` job only). */
  outputs: Map<string, string[]>;
};

type Workflow = {
  file: string;
  prTriggered: boolean;
  /** Workflow-level `pull_request: paths:` — applies to every job in the file. */
  prPaths: string[] | null;
  /** Top-level `env:` scalars (NP_SAFETY_TESTS and the test counts live here). */
  env: Map<string, string>;
  /** Partition variables assigned anywhere BUT top-level env — invisible to E. */
  nestedPartitionEnv: string[];
  jobs: Job[];
};

function blockScalar(lines: string[], start: number, keyIndent: number): string {
  const body: string[] = [];
  for (let i = start; i < lines.length; i++) {
    const l = lines[i]!;
    if (l.trim() === "") { body.push(""); continue; }
    if (l.search(/\S/) <= keyIndent) break;
    body.push(l.trim());
  }
  return body.join("\n");
}

/** Every `run:` command in a job, with backslash continuations joined. */
function runCommands(lines: string[]): string[] {
  const raw: string[] = [];
  let inRun = false;
  let runIndent = 0;
  for (const line of lines) {
    if (/^\s*#/.test(line)) continue; // a command named in a comment is not run
    const m = /^(\s*)-?\s*run:\s*(.*)$/.exec(line);
    if (m) {
      inRun = true;
      runIndent = m[1]!.length;
      if (m[2] && m[2] !== "|" && m[2] !== ">") raw.push(m[2]);
      continue;
    }
    if (inRun) {
      const indent = line.search(/\S/);
      if (indent > runIndent) raw.push(line.trim());
      else if (line.trim() !== "") inRun = false;
    }
  }
  const joined: string[] = [];
  let acc = "";
  for (const r of raw) {
    if (r.endsWith("\\")) { acc += r.slice(0, -1).trim() + " "; continue; }
    joined.push((acc + r).trim());
    acc = "";
  }
  if (acc.trim()) joined.push(acc.trim());
  return joined;
}

function parseWorkflow(file: string, text: string): Workflow {
  const lines = text.split("\n");

  // ── on: ─────────────────────────────────────────────────────────────────
  let prTriggered = false;
  let prPaths: string[] | null = null;
  const onIdx = lines.findIndex((l) => /^on:\s*$/.test(l));
  if (onIdx >= 0) {
    for (let i = onIdx + 1; i < lines.length; i++) {
      const l = lines[i]!;
      if (l.trim() === "" || /^\s*#/.test(l)) continue;
      if (l.search(/\S/) === 0) break;
      if (/^  pull_request:\s*$/.test(l)) {
        prTriggered = true;
        for (let j = i + 1; j < lines.length; j++) {
          const k = lines[j]!;
          if (k.trim() === "" || /^\s*#/.test(k)) continue;
          if (k.search(/\S/) <= 2) break;
          if (/^\s*paths:\s*$/.test(k)) {
            const got: string[] = [];
            for (let m = j + 1; m < lines.length; m++) {
              const e = lines[m]!;
              if (e.trim() === "" || /^\s*#/.test(e)) continue;
              const item = /^\s*-\s*(.+?)\s*$/.exec(e);
              if (!item || e.search(/\S/) <= k.search(/\S/)) break;
              got.push(item[1]!.replace(/^['"]|['"]$/g, ""));
            }
            prPaths = got;
          }
        }
      }
      if (/^  pull_request:\s*\S/.test(l)) prTriggered = true;
    }
  }

  // ── jobs: ───────────────────────────────────────────────────────────────
  const jobs: Job[] = [];
  const jobsIdx = lines.findIndex((l) => /^jobs:\s*$/.test(l));
  if (jobsIdx >= 0) {
    const starts: { id: string; at: number }[] = [];
    for (let i = jobsIdx + 1; i < lines.length; i++) {
      const l = lines[i]!;
      if (l.trim() === "") continue;
      if (l.search(/\S/) === 0) break;
      const m = /^  ([A-Za-z0-9_-]+):\s*$/.exec(l);
      if (m) starts.push({ id: m[1]!, at: i });
    }
    for (let s = 0; s < starts.length; s++) {
      const from = starts[s]!.at;
      const to = s + 1 < starts.length ? starts[s + 1]!.at : lines.length;
      const body = lines.slice(from + 1, to);
      const ifLine = body.find((l) => /^\s{4}if:\s/.test(l)) ?? "";
      const nameLine = body.map((l) => /^ {4}name:\s*(.+?)\s*$/.exec(l)).find((m) => m);
      const name = nameLine ? nameLine[1]!.replace(/^(['"])(.*)\1$/, "$2") : null;
      const hasStrategy = body.some((l) => /^ {4}strategy:/.test(l));
      const requiredMarker = body.some((l) => /^\s*#.*required-status-check CONTEXT/.test(l));

      const scopeEnv = new Map<string, string[]>();
      for (let i = 0; i < body.length; i++) {
        const m = /^(\s+)(NP_SCOPE_[A-Z0-9_]+):\s*\|\s*$/.exec(body[i]!);
        if (m) scopeEnv.set(m[2]!, cleanList(blockScalar(body, i + 1, m[1]!.length)));
      }

      const runs = runCommands(body);

      // Resolve the `changes`-job plumbing: env var → scope file → job output.
      const fileOfEnv = new Map<string, string>();
      const outputs = new Map<string, string[]>();
      for (const cmd of runs) {
        const w = /printf\s+'%s\\n'\s+"\$([A-Z0-9_]+)"\s*>\s*(\S+)/.exec(cmd);
        if (w) fileOfEnv.set(w[2]!, w[1]!);
      }
      const bind = (out: string, scopeFile: string) => {
        const env = fileOfEnv.get(scopeFile);
        if (!env) throw new Error(`${file}: job '${starts[s]!.id}' emits output '${out}' from ${scopeFile}, which no step writes`);
        const list = scopeEnv.get(env);
        if (!list) throw new Error(`${file}: ${scopeFile} is written from $${env}, which this job does not declare`);
        outputs.set(out, list);
      };
      for (const cmd of runs) {
        // Both binding forms name the scope FILE literally. `--relevant "$2"`
        // inside the shell helper that tooling-ci.yml defines is the helper, not
        // a binding, and matching it would bind an output to a shell parameter.
        const SCOPE_FILE = "(scope[A-Za-z0-9._-]*\\.paths)";
        const e = new RegExp(`^emit\\s+([a-z0-9_]+)\\s+${SCOPE_FILE}\\s*$`).exec(cmd);
        if (e) bind(e[1]!, e[2]!);
        const d = new RegExp(
          `^([a-z0-9_]+)=\\$\\(scripts/ci-changed-scope\\.sh\\s+--relevant\\s+${SCOPE_FILE}\\s+changed\\.txt\\)`,
        ).exec(cmd);
        if (d) bind(d[1]!, d[2]!);
      }

      jobs.push({
        wf: file, id: starts[s]!.id, name, hasStrategy, requiredMarker,
        ifExpr: ifLine, runs, scopeEnv, outputs,
      });
    }
  }

  // ── top-level env: ──────────────────────────────────────────────────────
  const env = new Map<string, string>();
  const envIdx = lines.findIndex((l) => /^env:\s*$/.test(l));
  if (envIdx >= 0) {
    for (let i = envIdx + 1; i < lines.length; i++) {
      const l = lines[i]!;
      if (l.trim() === "" || /^\s*#/.test(l)) continue;
      if (l.search(/\S/) === 0) break;
      const m = /^  ([A-Za-z0-9_]+):\s*(.*?)\s*$/.exec(l);
      if (m) env.set(m[1]!, m[2]!.replace(/^(['"])(.*)\1$/, "$2"));
    }
  }
  const nestedPartitionEnv = lines
    .filter((l) => /^ {3,}(NP_SAFETY_TESTS|NP_[A-Z_]+_TEST_COUNT):/.test(l))
    .map((l) => l.trim().split(":")[0]!);
  return { file, prTriggered, prPaths, env, nestedPartitionEnv, jobs };
}

/**
 * The filters a job is actually subject to. Empty = the job always runs, which
 * is the only thing that covers `<tree>`.
 *
 * A job under both a workflow-level `paths:` and a job-level `if:` must satisfy
 * BOTH, so coverage is the conjunction: a target is covered only if every
 * filter covers it.
 */
function filtersFor(wf: Workflow, job: Job): string[][] {
  const out: string[][] = [];
  if (wf.prPaths) out.push(wf.prPaths);
  const refs = [...job.ifExpr.matchAll(/needs\.([A-Za-z0-9_-]+)\.outputs\.([A-Za-z0-9_]+)/g)];
  for (const r of refs) {
    const producer = wf.jobs.find((j) => j.id === r[1]!);
    if (!producer) throw new Error(`${wf.file}: job '${job.id}' gates on needs.${r[1]}.outputs.${r[2]}, but there is no job '${r[1]}'`);
    const list = producer.outputs.get(r[2]!);
    if (!list) throw new Error(`${wf.file}: job '${job.id}' gates on needs.${r[1]}.outputs.${r[2]}, which resolves to no relevance list`);
    out.push(list);
  }
  return out;
}

const jobCovers = (filters: string[][], target: string): boolean =>
  filters.every((f) => listCovers(f, target));

// ── CMake build graph ────────────────────────────────────────────────────────

const truthy = (v: string | undefined): boolean =>
  v !== undefined && !/^(0|off|false|no|n|ignore|notfound|)$/i.test(v.trim());

function evalCond(cond: string, defs: Map<string, string>): boolean {
  const c = cond.trim();
  let m = /^([A-Za-z0-9_]+)$/.exec(c);
  if (m) return truthy(defs.get(m[1]!));
  m = /^NOT\s+([A-Za-z0-9_]+)$/.exec(c);
  if (m) return !truthy(defs.get(m[1]!));
  // Anything more complex is treated as taken. Over-approximating the build
  // graph demands MORE coverage, never less — the safe direction for a guard
  // whose failure mode is a module nobody gates.
  return true;
}

const normalise = (p: string): string => {
  const out: string[] = [];
  for (const seg of p.split("/")) {
    if (seg === "" || seg === ".") continue;
    if (seg === "..") { out.pop(); continue; }
    out.push(seg);
  }
  return out.join("/");
};

/** add_subdirectory() targets of one CMakeLists, for the given -D settings. */
function subdirsOf(
  text: string, dirRel: string, defs: Map<string, string>, tests: string[] = [],
): string[] {
  const found: string[] = [];
  const stack: { active: boolean; taken: boolean }[] = [];
  const active = () => stack.every((s) => s.active);
  for (const rawLine of text.split("\n")) {
    const line = rawLine.replace(/#.*$/, "").trim();
    if (!line) continue;
    const argsOf = () => line.slice(line.indexOf("(") + 1).replace(/\)\s*$/, "").trim();
    if (/^if\s*\(/.test(line)) {
      const v = evalCond(argsOf(), defs);
      stack.push({ active: v, taken: v });
      continue;
    }
    if (/^elseif\s*\(/.test(line)) {
      const top = stack[stack.length - 1];
      if (!top) continue;
      const v = !top.taken && evalCond(argsOf(), defs);
      top.active = v;
      top.taken = top.taken || v;
      continue;
    }
    if (/^else\s*\(/.test(line)) {
      const top = stack[stack.length - 1];
      if (!top) continue;
      top.active = !top.taken;
      top.taken = true;
      continue;
    }
    if (/^endif\s*\(/.test(line)) { stack.pop(); continue; }
    // `return()` inside the NP_BUILD_TESTS branch is what makes the host-test
    // and cross-compile module sets DIFFERENT sets. Missing it would merge them.
    if (/^return\s*\(/.test(line)) { if (active()) break; continue; }
    // add_test(NAME x …) — correspondence E compares these to
    // ci/host-test-partition.txt. Only the literal NAME form is read; a test
    // registered through a variable, foreach() or function() is invisible here,
    // and build-all.yml's diff against `ctest -N` is the backstop for that.
    const t = /^add_test\s*\(\s*NAME\s+([A-Za-z0-9_.-]+)/.exec(line);
    if (t) { if (active()) tests.push(t[1]!); continue; }
    if (/^add_subdirectory\s*\(/.test(line)) {
      if (!active()) continue;
      const arg = argsOf().split(/\s+/)[0]!.replace(/^["']|["']$/g, "");
      const resolved = arg.replace(/\$\{CMAKE_CURRENT_SOURCE_DIR\}/g, dirRel);
      if (resolved.includes("${")) {
        throw new Error(`${dirRel}/CMakeLists.txt: add_subdirectory(${arg}) uses a variable this parser cannot resolve`);
      }
      found.push(normalise(resolved.startsWith(dirRel) ? resolved : `${dirRel}/${resolved}`));
    }
  }
  return found;
}

/** option() defaults, so NP_BUILD_BOOTLOADER is ON without anyone saying so. */
function optionDefaults(text: string, into: Map<string, string>): void {
  for (const line of text.split("\n")) {
    const m = /^\s*option\s*\(\s*([A-Za-z0-9_]+)\s+"[^"]*"\s+([A-Za-z0-9_]+)\s*\)/.exec(line);
    if (m && !into.has(m[1]!)) into.set(m[1]!, m[2]!);
  }
}

type Build = {
  job: Job;
  srcRel: string;
  modules: string[];
  /** Configured with NP_BUILD_TESTS on: the host-test build. */
  hostTests: boolean;
  /** add_test() names registered by the configured graph. */
  tests: string[];
};

/** The `cmake -B <bin> [flags] <src>` invocations in a job, resolved to modules. */
function buildsOf(root: string, job: Job): Build[] {
  const out: Build[] = [];
  for (const cmd of job.runs) {
    const m = /(?:^|\s)cmake\s+-B\s+(\S+)\s+(.+)$/.exec(cmd);
    if (!m) continue;
    const tokens = m[2]!.trim().split(/\s+/);
    const srcRel = tokens[tokens.length - 1]!;
    if (srcRel.startsWith("-")) {
      throw new Error(`${job.wf}: cannot find the source directory in: ${cmd}`);
    }
    if (!existsSync(join(root, srcRel, "CMakeLists.txt"))) {
      throw new Error(`${job.wf}: job '${job.id}' configures '${srcRel}', which has no CMakeLists.txt`);
    }
    const defs = new Map<string, string>();
    for (const t of tokens) {
      const d = /^-D([A-Za-z0-9_]+)=(.*)$/.exec(t);
      if (d) defs.set(d[1]!, d[2]!);
    }
    // A toolchain file IS the cross-compile signal; CMake sets
    // CMAKE_CROSSCOMPILING itself and no workflow passes it explicitly.
    if (defs.has("CMAKE_TOOLCHAIN_FILE")) defs.set("CMAKE_CROSSCOMPILING", "1");

    const modules: string[] = [];
    const tests: string[] = [];
    const seen = new Set<string>();
    const visit = (dirRel: string) => {
      const f = join(root, dirRel, "CMakeLists.txt");
      if (!existsSync(f)) return;
      const text = readFileSync(f, "utf8");
      optionDefaults(text, defs);
      for (const child of subdirsOf(text, dirRel, defs, tests)) {
        if (seen.has(child)) continue;
        seen.add(child);
        modules.push(child);
        visit(child);
      }
    };
    visit(srcRel);
    out.push({ job, srcRel, modules, hostTests: truthy(defs.get("NP_BUILD_TESTS")), tests });
  }
  return out;
}

// ── Gate declarations ────────────────────────────────────────────────────────

type Gate = { file: string; scanPaths: string[] | null };

function gatesOf(root: string): Gate[] {
  const out: Gate[] = [];
  for (const [dir, re] of [
    ["scripts", /^(check-.*\.(ts|sh)|ci-changed-scope\.sh)$/],
    ["ci", /^(test_.*\.py|run_.*\.sh)$/],
  ] as const) {
    let entries: string[];
    try { entries = readdirSync(join(root, dir)); } catch { continue; }
    for (const e of entries.sort()) {
      if (!re.test(e)) continue;
      const rel = `${dir}/${e}`;
      const head = readFileSync(join(root, rel), "utf8").split("\n").slice(0, 80);
      const field = (name: string, pattern = "(.+?)"): string | null => {
        for (const line of head) {
          const m = new RegExp(`^\\s*(?:#|\\*|//)?\\s*${name}:\\s*${pattern}\\s*$`).exec(line);
          if (m) return m[1]!;
        }
        return null;
      };
      // A bare token to end of line, for check-gate-coverage.ts's reason: its
      // own header explains the three kinds in `CI-Kind: gate  <prose>` shape,
      // and a looser pattern reads that explanation as its declaration.
      if (field("CI-Kind", "([a-z-]+)") !== "gate") continue;
      const raw = field("CI-Scan-Paths");
      out.push({ file: rel, scanPaths: raw === null ? null : raw.split(/\s+/).filter(Boolean) });
    }
  }
  return out;
}

/**
 * Does this command INVOKE the gate, as opposed to mentioning its path?
 *
 * Substring matching is not good enough here, and the difference is not
 * academic: tooling-ci.yml's `changes` job carries the line
 *
 *     assert scope.docnaming.paths true  scripts/check-doc-filenames.ts
 *
 * which names the gate as test DATA. `changes` has no relevance gate, so
 * reading that as an invocation would make an ungated job appear to run the
 * doc-naming guard, and every coverage question about it would then answer
 * "covered" no matter what its real list said — the guard would go green over
 * precisely the drift it exists to find.
 *
 * Also excluded: the gate's own falsification. `check-x --self-test` satisfying
 * "the gate runs" is exactly backwards — it reports a gate as wired up when CI
 * only ever runs its self-test.
 */
function invokes(cmd: string, gate: string): boolean {
  if (cmd.includes("--self-test")) return false;
  const esc = gate.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  return new RegExp(`^(?:(?:bun|python3|python|node|sh|bash)\\s+)*${esc}(?:\\s|$)`).test(cmd);
}

// ── Checked-in manifests (correspondences C and E) ───────────────────────────

const REQUIRED_CHECKS = "ci/required-checks.txt";
const TEST_PARTITION = "ci/host-test-partition.txt";

/** Non-comment, non-blank lines of a manifest, or null if it does not exist. */
function readManifest(root: string, rel: string): string[] | null {
  const f = join(root, rel);
  if (!existsSync(f)) return null;
  return cleanList(readFileSync(f, "utf8"));
}

const dupes = (xs: string[]): string[] =>
  [...new Set(xs.filter((x, i) => xs.indexOf(x) !== i))];

// ── The audit ────────────────────────────────────────────────────────────────

type Report = {
  violations: string[];
  workflows: Workflow[];
  builds: Build[];
  gates: Gate[];
  moduleOwners: Map<string, string[]>;
  gateOwners: Map<string, string[]>;
  /** C — required context → the one job that reports it. */
  contexts: Map<string, string>;
  /** E — host-test partition as checked, or null if there is no manifest. */
  partition: { c: number; b: number; regexCopies: string[]; countCopies: string[] } | null;
};

function audit(root: string): Report {
  const dir = join(root, ".github/workflows");
  let files: string[] = [];
  try {
    files = readdirSync(dir).filter((f) => f.endsWith(".yml") || f.endsWith(".yaml")).sort();
  } catch { /* no workflows — reported as vacuous below */ }

  const workflows = files.map((f) => parseWorkflow(f, readFileSync(join(dir, f), "utf8")));
  const violations: string[] = [];
  const gates = gatesOf(root);

  for (const wf of workflows) {
    if (wf.prPaths) wf.prPaths.forEach((p) => assertShape(p, `${wf.file} pull_request paths`));
    for (const j of wf.jobs) {
      for (const [env, list] of j.scopeEnv) list.forEach((p) => assertShape(p, `${wf.file} ${env}`));
    }
  }

  const pr = workflows.filter((w) => w.prTriggered);

  // Resolve every gating expression up front, not lazily where it happens to be
  // needed. An `if:` naming an output no step produces is the check-name hazard
  // of OI-SWCI-26 one layer down: it does not error anywhere, it simply stops
  // meaning what it says, and a job whose gate silently evaluates to nothing is
  // the thing this file exists to notice.
  for (const wf of pr) for (const j of wf.jobs) filtersFor(wf, j);

  // ── A. build graph ↔ relevance list ────────────────────────────────────
  const builds: Build[] = [];
  const moduleOwners = new Map<string, string[]>();   // module → jobs that GATE it
  const moduleBuilders = new Map<string, string[]>(); // module → jobs that BUILD it
  for (const wf of pr) {
    for (const j of wf.jobs) {
      const bs = buildsOf(root, j);
      if (!bs.length) continue;
      const filters = filtersFor(wf, j);
      const where = `${wf.file}:${j.id}`;
      for (const b of bs) {
        builds.push(b);
        // The configure root's own CMakeLists is a dependency of every module
        // under it: it is where add_subdirectory() and add_test() live.
        const rootFile = `${b.srcRel}/CMakeLists.txt`;
        for (const target of [rootFile, ...b.modules.map((m) => `${m}/**`)]) {
          const key = target;
          (moduleBuilders.get(key) ?? moduleBuilders.set(key, []).get(key)!).push(where);
          if (jobCovers(filters, target)) {
            (moduleOwners.get(key) ?? moduleOwners.set(key, []).get(key)!).push(where);
          }
        }
      }
    }
  }
  for (const [target, builders] of [...moduleBuilders].sort()) {
    if (!(moduleOwners.get(target) ?? []).length) {
      violations.push(
        `A: ${target} is in the CMake build graph of ${builders.join(", ")} and in scope for NO ` +
          `PR-triggered job — an edit to it builds nothing, and the first PR to notice will be an ` +
          `unrelated one (NP-SW-CI-001 §5.0, OI-SWCI-08)`,
      );
    }
  }

  // A2 — the other direction: a firmware module named by a list nothing builds.
  for (const wf of pr) {
    for (const j of wf.jobs) {
      for (const [env, list] of j.scopeEnv) {
        for (const p of list) {
          if (!p.endsWith("/**")) continue;
          const d = p.slice(0, -3);
          if (!existsSync(join(root, d, "CMakeLists.txt"))) continue;
          if (!moduleBuilders.has(p) && !moduleBuilders.has(`${d}/CMakeLists.txt`)) {
            violations.push(
              `A: ${wf.file} ${env} lists ${p}, which is a CMake module that no PR-triggered ` +
                `job builds — the entry describes a build graph that no longer exists`,
            );
          }
        }
      }
    }
  }

  // ── B. CI-Scan-Paths ↔ relevance list ──────────────────────────────────
  const gateOwners = new Map<string, string[]>();
  for (const g of gates) {
    if (g.scanPaths === null) {
      violations.push(
        `B: ${g.file}: CI-Kind gate but no CI-Scan-Paths — CI-Scans is prose and cannot be ` +
          `compared to a relevance list. Declare the same population in the matcher's shapes, ` +
          `or ${TREE} if it walks from the repository root`,
      );
      continue;
    }
    if (!g.scanPaths.length) {
      violations.push(`B: ${g.file}: CI-Scan-Paths is empty`);
      continue;
    }
    g.scanPaths.forEach((p) => assertShape(p, `${g.file} CI-Scan-Paths`));

    const runners: { where: string; filters: string[][] }[] = [];
    for (const wf of pr) {
      for (const j of wf.jobs) {
        if (!j.runs.some((c) => invokes(c, g.file))) continue;
        runners.push({ where: `${wf.file}:${j.id}`, filters: filtersFor(wf, j) });
      }
    }
    if (!runners.length) {
      violations.push(
        `B: ${g.file}: no PR-triggered job runs it — whatever else runs it cannot gate a pull request`,
      );
      continue;
    }
    for (const target of g.scanPaths) {
      const owners = runners.filter((r) => jobCovers(r.filters, target)).map((r) => r.where);
      if (!owners.length) {
        violations.push(
          `B: ${g.file} declares it scans ${target}, but every PR-triggered job that runs it ` +
            `(${runners.map((r) => r.where).join(", ")}) is gated by a relevance list that does ` +
            `not cover it — the gate is skipped on precisely the changes it exists to catch`,
        );
      } else {
        const key = `${g.file} ${target}`;
        gateOwners.set(key, owners);
      }
    }
  }

  // ── C. required contexts ↔ job names (OI-SWCI-26, OI-SWCI-23) ────────
  //
  // A required context is an exact string. A job renamed, or given a matrix,
  // stops reporting under it, and GitHub says nothing. The failure is a PR
  // waiting forever, or — once someone edits the ruleset to match — the old
  // gate silently gone. This makes the in-repo half of that contract checked.
  const contexts = new Map<string, string>();
  const marked = workflows.flatMap((w) => w.jobs.filter((j) => j.requiredMarker).map((j) => ({ w, j })));
  const required = readManifest(root, REQUIRED_CHECKS);
  if (required === null) {
    for (const { w, j } of marked) {
      violations.push(
        `C: ${w.file}:${j.id} carries the required-status-check CONTEXT warning, but ` +
          `${REQUIRED_CHECKS} does not exist — the warning is a convention nothing checks`,
      );
    }
  } else {
    for (const d of dupes(required)) violations.push(`C: ${REQUIRED_CHECKS} lists '${d}' more than once`);
    const ctxOf = (j: Job) => j.name ?? j.id;
    for (const ctx of new Set(required)) {
      const hits = pr.flatMap((w) => w.jobs.filter((j) => ctxOf(j) === ctx).map((j) => ({ w, j })));
      const offPr = workflows
        .filter((w) => !w.prTriggered)
        .flatMap((w) => w.jobs.filter((j) => ctxOf(j) === ctx).map((j) => `${w.file}:${j.id}`));
      if (!hits.length) {
        violations.push(
          offPr.length
            ? `C: '${ctx}' is required, but it is the name of ${offPr.join(", ")}, which does not run ` +
                `on pull_request and so never reports on any PR — requiring it blocks every PR ` +
                `permanently (OI-SWCI-23)`
            : `C: '${ctx}' is required, but no pull_request-triggered job is named that — it was ` +
                `renamed or removed, and every PR now waits forever for a check that cannot report ` +
                `(OI-SWCI-26). Rename it back, or change ${REQUIRED_CHECKS} and the Safety ruleset together`,
        );
        continue;
      }
      if (hits.length > 1) {
        violations.push(
          `C: '${ctx}' is the name of ${hits.length} jobs (${hits.map((h) => `${h.w.file}:${h.j.id}`).join(", ")}) ` +
            `— either one reporting satisfies the requirement, so it gates neither`,
        );
        continue;
      }
      const { w, j } = hits[0]!;
      const where = `${w.file}:${j.id}`;
      contexts.set(ctx, where);
      if (w.prPaths) {
        violations.push(
          `C: '${ctx}' (${where}) is required, but ${w.file} has a workflow-level pull_request paths: ` +
            `filter — on an out-of-scope PR it never reports, and the PR waits forever (§6.7.1)`,
        );
      }
      if (j.hasStrategy) {
        violations.push(
          `C: '${ctx}' (${where}) is required, but the job has a strategy: — a matrix reports as ` +
            `'${ctx} (<value>)', which never matches the required string (§6.7.8)`,
        );
      }
      if (!j.requiredMarker) {
        violations.push(
          `C: '${ctx}' (${where}) is required, but the job does not carry the in-file ` +
            `"required-status-check CONTEXT" warning — the next person to rename it will not know`,
        );
      }
    }
    const req = new Set(required);
    for (const { w, j } of marked) {
      if (!req.has(ctxOf(j))) {
        violations.push(
          `C: ${w.file}:${j.id} ('${ctxOf(j)}') carries the required-status-check CONTEXT warning ` +
            `but is not in ${REQUIRED_CHECKS} — either the warning is stale or the manifest is`,
        );
      }
    }
  }

  // ── D. every relevance list re-runs on an edit to its own workflow (OI-SWCI-25)
  //
  // A workflow edit is the change most likely to break that workflow. A list
  // that does not cover its own file skips its jobs on exactly that PR — the
  // web-ci.yml case observed on PR #261.
  for (const wf of pr) {
    const self = `.github/workflows/${wf.file}`;
    for (const j of wf.jobs) {
      for (const [env, list] of j.scopeEnv) {
        if (!listCovers(list, self)) {
          violations.push(
            `D: ${wf.file}:${j.id} ${env} does not cover ${self} — an edit to this workflow ` +
              `skips the jobs it gates, on the one PR most likely to break them (OI-SWCI-25)`,
          );
        }
      }
    }
  }

  // ── E. host-test partition ↔ add_test() ↔ NP_SAFETY_TESTS ↔ counts ────
  //   (OI-SWCI-37, OI-SWCI-14)
  //
  // The regex and three counts each live in several workflow files. Twice a
  // copy was missed and a guard caught the symptom a round-trip later, or not
  // at all for a week. Counts alone cannot see a rename or a swap.
  let partition: Report["partition"] = null;
  // E compares top-level copies only. A copy moved into a job's or a step's
  // env: would drop out of the comparison without a word, which is the silent
  // shrinkage this correspondence exists to stop — so it is refused outright.
  for (const w of workflows) {
    for (const k of w.nestedPartitionEnv) {
      violations.push(
        `E: ${w.file} assigns ${k} below the top level — correspondence E reads only the ` +
          `workflow's top-level env:, so this copy is compared with nothing. Move it up`,
      );
    }
  }
  const regexCopies = workflows.filter((w) => w.env.has("NP_SAFETY_TESTS"));
  const manifest = readManifest(root, TEST_PARTITION);
  if (manifest === null) {
    if (regexCopies.length) {
      violations.push(
        `E: ${regexCopies.map((w) => w.file).join(", ")} declare NP_SAFETY_TESTS, but ` +
          `${TEST_PARTITION} does not exist — the partition is checked against nothing`,
      );
    }
  } else {
    const entries: { cls: "C" | "B"; name: string }[] = [];
    for (const line of manifest) {
      const m = /^([CB])\s+([A-Za-z0-9_.-]+)$/.exec(line);
      if (!m) { violations.push(`E: ${TEST_PARTITION}: malformed line '${line}' (want 'C <name>' or 'B <name>')`); continue; }
      entries.push({ cls: m[1] as "C" | "B", name: m[2]! });
    }
    const names = entries.map((e) => e.name);
    for (const d of dupes(names)) violations.push(`E: ${TEST_PARTITION} lists ${d} more than once`);

    const hostBuilds = builds.filter((b) => b.hostTests);
    if (!hostBuilds.length) {
      violations.push(`E: no PR-triggered job configures the host-test build (-DNP_BUILD_TESTS=ON) — ${TEST_PARTITION} is compared against nothing`);
    } else {
      const registered = new Set(hostBuilds.flatMap((b) => b.tests));
      const listed = new Set(names);
      for (const t of [...registered].sort()) {
        if (!listed.has(t)) {
          violations.push(`E: ${t} is registered by add_test() but not in ${TEST_PARTITION} — say which half owns it (C or B), and move the count(s)`);
        }
      }
      for (const t of [...listed].sort()) {
        if (!registered.has(t)) {
          violations.push(`E: ${TEST_PARTITION} lists ${t}, which no add_test() registers — renamed or removed without the manifest (OI-SWCI-14)`);
        }
      }
    }

    const values = [...new Set(regexCopies.map((w) => w.env.get("NP_SAFETY_TESTS")!))];
    if (!regexCopies.length) {
      violations.push(`E: ${TEST_PARTITION} exists but no workflow declares NP_SAFETY_TESTS — nothing selects by it`);
    } else if (values.length > 1) {
      violations.push(
        `E: NP_SAFETY_TESTS differs between workflows — ` +
          regexCopies.map((w) => `${w.file}: ${w.env.get("NP_SAFETY_TESTS")}`).join(" | ") +
          ` — the Class C and Class B selections no longer partition the suite (OI-SWCI-37)`,
      );
    } else {
      let re: RegExp | null = null;
      try { re = new RegExp(values[0]!); } catch (e) {
        violations.push(`E: NP_SAFETY_TESTS is not a valid regex: ${(e as Error).message}`);
      }
      if (re) {
        for (const e of entries) {
          const isC = re.test(e.name);
          if (isC !== (e.cls === "C")) {
            violations.push(
              `E: ${e.name} is class ${e.cls} in ${TEST_PARTITION}, but NP_SAFETY_TESTS ` +
                `${isC ? "selects" : "does not select"} it — it runs in the ${isC ? "Class C" : "Class B"} ` +
                `workflow (OI-SWCI-37)`,
            );
          }
        }
      }
    }

    const nC = entries.filter((e) => e.cls === "C").length;
    const nB = entries.length - nC;
    const countCopies: string[] = [];
    for (const [key, want] of [
      ["NP_SAFETY_TEST_COUNT", nC], ["NP_CLASS_B_TEST_COUNT", nB], ["NP_TOTAL_TEST_COUNT", nC + nB],
    ] as const) {
      for (const w of workflows) {
        const got = w.env.get(key);
        if (got === undefined) continue;
        countCopies.push(`${w.file}:${key}`);
        if (got !== String(want)) {
          violations.push(
            `E: ${w.file} ${key} is '${got}', but ${TEST_PARTITION} gives ${want} — ` +
              `one copy of the count was moved and this one was not (OI-SWCI-37)`,
          );
        }
      }
    }
    partition = { c: nC, b: nB, regexCopies: regexCopies.map((w) => w.file), countCopies };
  }

  return { violations, workflows, builds, gates, moduleOwners, gateOwners, contexts, partition };
}

// ── Self-test ────────────────────────────────────────────────────────────────
// Placed BEFORE any read of the real tree, deliberately: check-section-refs.ts
// put its self-test after a top-level read of the production CLAUDE.md and
// therefore reported on production state while claiming to report on fixtures.
// Every case below runs audit() against a fixture root and nothing else, which
// is what makes `bun check-ci-scope.ts --self-test` pass from outside the repo.
if (process.argv.includes("--self-test")) {
  const box = mkdtempSync(join(tmpdir(), "np-ciscope-"));
  const build = (files: Record<string, string>): string => {
    const root = mkdtempSync(join(box, "t-"));
    for (const [rel, body] of Object.entries(files)) {
      const d = rel.split("/").slice(0, -1).join("/");
      if (d) mkdirSync(join(root, d), { recursive: true });
      writeFileSync(join(root, rel), body);
    }
    return root;
  };

  /** A workflow in the shape every scoped workflow in this repo actually has. */
  const WF = (opts: {
    pr?: boolean;
    prPaths?: string[];
    list?: string[];
    gatedRuns?: string[];
    ungatedRuns?: string[];
    /** Omit the self-entry correspondence D requires — only D's own cases want this. */
    noSelf?: boolean;
  }) => {
    const on = opts.prPaths
      ? `on:\n  pull_request:\n    paths:\n${opts.prPaths.map((p) => `      - '${p}'`).join("\n")}\n`
      : `on:\n${opts.pr === false ? "  schedule:\n    - cron: '0 0 * * 0'\n" : "  pull_request:\n"}`;
    let s = `name: t\n${on}jobs:\n`;
    if (opts.list) {
      s +=
        `  changes:\n    runs-on: ubuntu-latest\n    outputs:\n      relevant: x\n    env:\n` +
        `      NP_SCOPE_PATHS: |\n${[...opts.list, ...(opts.noSelf ? [] : [".github/workflows/**"])]
          .map((p) => `        ${p}`).join("\n")}\n` +
        `    steps:\n      - run: printf '%s\\n' "$NP_SCOPE_PATHS" > scope.paths\n` +
        `      - run: |\n          relevant=$(scripts/ci-changed-scope.sh --relevant scope.paths changed.txt)\n`;
    }
    if (opts.gatedRuns) {
      s +=
        `  work:\n    runs-on: ubuntu-latest\n    needs: changes\n` +
        `    if: \${{ !cancelled() && needs.changes.outputs.relevant != 'false' }}\n    steps:\n` +
        opts.gatedRuns.map((c) => `      - run: ${c}`).join("\n") + "\n";
    }
    if (opts.ungatedRuns) {
      s +=
        `  always:\n    runs-on: ubuntu-latest\n    steps:\n` +
        opts.ungatedRuns.map((c) => `      - run: ${c}`).join("\n") + "\n";
    }
    return s;
  };

  const CONFIGURE = "cmake -B build/h -G Ninja -DNP_BUILD_TESTS=ON -DCMAKE_CROSSCOMPILING=OFF firmware";
  const ROOT_CMAKE = (subdirs: string[], crossOnly: string[] = [], tests: string[] = []) =>
    `cmake_minimum_required(VERSION 3.20)\nproject(t C)\n` +
    `option(NP_BUILD_TESTS "t" OFF)\n` +
    `if(NP_BUILD_TESTS)\n${subdirs.map((s) => `    add_subdirectory(${s})`).join("\n")}\n` +
    `${tests.map((t) => `    add_test(NAME ${t}\n        COMMAND ${t})`).join("\n")}\n    return()\nendif()\n` +
    `${crossOnly.map((s) => `add_subdirectory(${s})`).join("\n")}\n`;
  const MODULE = "add_library(x STATIC x.c)\n";
  const GATE = (scanPaths: string | null) =>
    `# CI-Kind: gate\n# CI-Self-Test: bun scripts/check-x.ts --self-test\n# CI-Scans: things\n` +
    (scanPaths === null ? "" : `# CI-Scan-Paths: ${scanPaths}\n`);

  const failures: string[] = [];
  const expect = (label: string, root: string, needle: string | null) => {
    let violations: string[];
    try {
      violations = audit(root).violations;
    } catch (e) {
      failures.push(`${label} — audit threw: ${(e as Error).message}`);
      return;
    }
    if (needle === null) {
      if (violations.length) failures.push(`${label} — expected clean, got: ${violations[0]}`);
    } else if (!violations.some((x) => x.includes(needle))) {
      failures.push(`${label} — no violation matching ${JSON.stringify(needle)}; got ${JSON.stringify(violations)}`);
    }
  };
  const expectThrow = (label: string, root: string, needle: string) => {
    try {
      audit(root);
      failures.push(`${label} — expected a hard error, got none`);
    } catch (e) {
      if (!(e as Error).message.includes(needle)) {
        failures.push(`${label} — error did not mention ${JSON.stringify(needle)}: ${(e as Error).message}`);
      }
    }
  };

  // ── A. build graph ↔ relevance list ────────────────────────────────────
  expect(
    "a module in the graph and on the list passes",
    build({
      "firmware/CMakeLists.txt": ROOT_CMAKE(["alpha"]),
      "firmware/alpha/CMakeLists.txt": MODULE,
      ".github/workflows/w.yml": WF({
        list: ["firmware/alpha/**", "firmware/CMakeLists.txt"],
        gatedRuns: [CONFIGURE],
      }),
    }),
    null,
  );
  // THE defect OI-SWCI-08 was raised for: add_subdirectory() lands, paths: does not.
  expect(
    "a module in the graph and on NO list is caught",
    build({
      "firmware/CMakeLists.txt": ROOT_CMAKE(["alpha", "beta"]),
      "firmware/alpha/CMakeLists.txt": MODULE,
      "firmware/beta/CMakeLists.txt": MODULE,
      ".github/workflows/w.yml": WF({
        list: ["firmware/alpha/**", "firmware/CMakeLists.txt"],
        gatedRuns: [CONFIGURE],
      }),
    }),
    "firmware/beta/** is in the CMake build graph",
  );
  // The union rule: §5.0 decided on purpose that each firmware workflow builds
  // the whole super-project while gating only its own class. That must PASS.
  expect(
    "a module gated by the OTHER workflow that builds it passes",
    build({
      "firmware/CMakeLists.txt": ROOT_CMAKE(["alpha", "beta"]),
      "firmware/alpha/CMakeLists.txt": MODULE,
      "firmware/beta/CMakeLists.txt": MODULE,
      ".github/workflows/a.yml": WF({
        list: ["firmware/alpha/**", "firmware/CMakeLists.txt"],
        gatedRuns: [CONFIGURE],
      }),
      ".github/workflows/b.yml": WF({
        list: ["firmware/beta/**", "firmware/CMakeLists.txt"],
        gatedRuns: [CONFIGURE],
      }),
    }),
    null,
  );
  // …and a weekly backstop must NOT be able to satisfy that claim.
  expect(
    "a module gated only by a non-PR workflow is still uncovered",
    build({
      "firmware/CMakeLists.txt": ROOT_CMAKE(["alpha", "beta"]),
      "firmware/alpha/CMakeLists.txt": MODULE,
      "firmware/beta/CMakeLists.txt": MODULE,
      ".github/workflows/a.yml": WF({
        list: ["firmware/alpha/**", "firmware/CMakeLists.txt"],
        gatedRuns: [CONFIGURE],
      }),
      ".github/workflows/weekly.yml": WF({ pr: false, ungatedRuns: [CONFIGURE] }),
    }),
    "firmware/beta/** is in the CMake build graph",
  );
  // The mode split is real: a module reachable only from the cross-compile
  // branch must still be seen. `return()` inside if(NP_BUILD_TESTS) is what
  // separates the two sets, and ignoring it would merge them.
  expect(
    "a cross-only module is seen by the cross-only configure",
    build({
      "firmware/CMakeLists.txt": ROOT_CMAKE(["alpha"], ["gamma"]),
      "firmware/alpha/CMakeLists.txt": MODULE,
      "firmware/gamma/CMakeLists.txt": MODULE,
      ".github/workflows/w.yml": WF({
        list: ["firmware/alpha/**", "firmware/CMakeLists.txt"],
        gatedRuns: [
          CONFIGURE,
          "cmake -B build/c -DCMAKE_TOOLCHAIN_FILE=/t.cmake -DCMAKE_BUILD_TYPE=Release firmware",
        ],
      }),
    }),
    "firmware/gamma/** is in the CMake build graph",
  );
  expect(
    "the host-test configure does NOT drag in the cross-only module",
    build({
      "firmware/CMakeLists.txt": ROOT_CMAKE(["alpha"], ["gamma"]),
      "firmware/alpha/CMakeLists.txt": MODULE,
      "firmware/gamma/CMakeLists.txt": MODULE,
      ".github/workflows/w.yml": WF({
        list: ["firmware/alpha/**", "firmware/CMakeLists.txt"],
        gatedRuns: [CONFIGURE],
      }),
    }),
    null,
  );
  // The configure root's own CMakeLists carries add_subdirectory and add_test:
  // dropping it from the list takes every module out of scope at once.
  expect(
    "the configure root's CMakeLists must itself be in scope",
    build({
      "firmware/CMakeLists.txt": ROOT_CMAKE(["alpha"]),
      "firmware/alpha/CMakeLists.txt": MODULE,
      ".github/workflows/w.yml": WF({ list: ["firmware/alpha/**"], gatedRuns: [CONFIGURE] }),
    }),
    "firmware/CMakeLists.txt is in the CMake build graph",
  );
  // Prefix semantics, pattern against pattern — the boundary ci-changed-scope.sh
  // asserts for files, asserted here for patterns.
  expect(
    "a parent prefix covers a nested module",
    build({
      "firmware/CMakeLists.txt": ROOT_CMAKE(["vendor/freertos"]),
      "firmware/vendor/freertos/CMakeLists.txt": MODULE,
      ".github/workflows/w.yml": WF({
        list: ["firmware/vendor/**", "firmware/CMakeLists.txt"],
        gatedRuns: [CONFIGURE],
      }),
    }),
    null,
  );
  expect(
    "a sibling-looking prefix does NOT cover it",
    build({
      "firmware/CMakeLists.txt": ROOT_CMAKE(["vendor/freertos"]),
      "firmware/vendor/freertos/CMakeLists.txt": MODULE,
      ".github/workflows/w.yml": WF({
        list: ["firmware/vend/**", "firmware/CMakeLists.txt"],
        gatedRuns: [CONFIGURE],
      }),
    }),
    "firmware/vendor/freertos/** is in the CMake build graph",
  );
  expect(
    "a list entry naming a module nothing builds is caught",
    build({
      "firmware/CMakeLists.txt": ROOT_CMAKE(["alpha"]),
      "firmware/alpha/CMakeLists.txt": MODULE,
      "firmware/retired/CMakeLists.txt": MODULE,
      ".github/workflows/w.yml": WF({
        list: ["firmware/alpha/**", "firmware/retired/**", "firmware/CMakeLists.txt"],
        gatedRuns: [CONFIGURE],
      }),
    }),
    "which is a CMake module that no PR-triggered job builds",
  );

  // ── B. CI-Scan-Paths ↔ relevance list ──────────────────────────────────
  expect(
    "a gate whose list covers its declared population passes",
    build({
      "scripts/check-x.ts": GATE("docs/**"),
      ".github/workflows/w.yml": WF({
        list: ["docs/**"],
        gatedRuns: ["bun scripts/check-x.ts --self-test", "bun scripts/check-x.ts"],
      }),
    }),
    null,
  );
  // The #306 shape: the doc-naming guard gated by a list that saw scripts/ only.
  expect(
    "a gate scanning outside its relevance list is caught",
    build({
      "scripts/check-x.ts": GATE("docs/**"),
      ".github/workflows/w.yml": WF({
        list: ["scripts/**"],
        gatedRuns: ["bun scripts/check-x.ts --self-test", "bun scripts/check-x.ts"],
      }),
    }),
    "declares it scans docs/**",
  );
  // A prefix on the list covers a nested declared population.
  expect(
    "docs/** on the list covers a gate declaring docs/superseded/**",
    build({
      "scripts/check-x.ts": GATE("docs/superseded/**"),
      ".github/workflows/w.yml": WF({
        list: ["docs/**"],
        gatedRuns: ["bun scripts/check-x.ts --self-test", "bun scripts/check-x.ts"],
      }),
    }),
    null,
  );
  // …and not the reverse.
  expect(
    "docs/superseded/** on the list does NOT cover a gate declaring docs/**",
    build({
      "scripts/check-x.ts": GATE("docs/**"),
      ".github/workflows/w.yml": WF({
        list: ["docs/superseded/**"],
        gatedRuns: ["bun scripts/check-x.ts --self-test", "bun scripts/check-x.ts"],
      }),
    }),
    "declares it scans docs/**",
  );
  // The OI-SWCI-38 case: a repo-wide gate is honest only if nothing gates it.
  expect(
    "a <tree> gate in an ungated job passes",
    build({
      "scripts/check-x.ts": GATE(TREE),
      ".github/workflows/w.yml": WF({
        ungatedRuns: ["bun scripts/check-x.ts --self-test", "bun scripts/check-x.ts"],
      }),
    }),
    null,
  );
  expect(
    "a <tree> gate behind ANY relevance list is caught",
    build({
      "scripts/check-x.ts": GATE(TREE),
      ".github/workflows/w.yml": WF({
        list: ["docs/**", "scripts/**"],
        gatedRuns: ["bun scripts/check-x.ts --self-test", "bun scripts/check-x.ts"],
      }),
    }),
    `declares it scans ${TREE}`,
  );
  expect(
    "a workflow-level paths: filter gates its jobs too",
    build({
      "scripts/check-x.ts": GATE("ci/**"),
      ".github/workflows/w.yml": WF({
        prPaths: ["ci/shdr/x.sql"],
        ungatedRuns: ["bun scripts/check-x.ts --self-test", "bun scripts/check-x.ts"],
      }),
    }),
    "declares it scans ci/**",
  );
  expect(
    "an undeclared CI-Scan-Paths is caught",
    build({
      "scripts/check-x.ts": GATE(null),
      ".github/workflows/w.yml": WF({
        ungatedRuns: ["bun scripts/check-x.ts --self-test", "bun scripts/check-x.ts"],
      }),
    }),
    "no CI-Scan-Paths",
  );
  // Running only the falsification is not running the gate.
  expect(
    "a gate only ever run as --self-test is caught",
    build({
      "scripts/check-x.ts": GATE("docs/**"),
      ".github/workflows/w.yml": WF({ ungatedRuns: ["bun scripts/check-x.ts --self-test"] }),
    }),
    "no PR-triggered job runs it",
  );
  // The tooling-ci.yml `changes` shape: an UNGATED job that merely names the
  // gate in a scope assertion must not read as running it — that would make
  // every coverage question about that gate answer "covered" for free.
  expect(
    "a gate named as data in an ungated job is not run by it",
    build({
      "scripts/check-x.ts": GATE("docs/**"),
      ".github/workflows/w.yml": WF({
        list: ["scripts/**"],
        gatedRuns: ["bun scripts/check-x.ts --self-test", "bun scripts/check-x.ts"],
        ungatedRuns: ["assert scope.docnaming.paths true  scripts/check-x.ts"],
      }),
    }),
    "declares it scans docs/**",
  );
  expect(
    "a gate run only by a non-PR workflow is caught",
    build({
      "scripts/check-x.ts": GATE("docs/**"),
      ".github/workflows/weekly.yml": WF({ pr: false, ungatedRuns: ["bun scripts/check-x.ts"] }),
    }),
    "no PR-triggered job runs it",
  );

  // ── C. required contexts ↔ job names ──────────────────────────────────
  const MARK = "    # ⚠ This string is a required-status-check CONTEXT (NP-SW-CI-001 §6.7.8).\n";
  /** A workflow of named jobs, in the firmware workflows' shape. */
  const NAMED = (opts: {
    pr?: boolean;
    prPaths?: string[];
    jobs: { id: string; name?: string; marker?: boolean; matrix?: boolean }[];
  }) => {
    const on = opts.prPaths
      ? `on:\n  pull_request:\n    paths:\n${opts.prPaths.map((p) => `      - '${p}'`).join("\n")}\n`
      : `on:\n${opts.pr === false ? "  schedule:\n    - cron: '0 0 * * 0'\n" : "  push:\n  pull_request:\n"}`;
    return `name: t\n${on}jobs:\n` + opts.jobs.map((j) =>
      `  ${j.id}:\n` +
      (j.name ? `    name: ${j.name}\n` : "") +
      (j.marker ? MARK : "") +
      `    runs-on: ubuntu-latest\n` +
      (j.matrix ? `    strategy:\n      matrix:\n        leg: [a, b]\n` : "") +
      `    steps:\n      - name: Checkout\n        run: true\n`).join("");
  };
  const REQ = (...ctx: string[]) => `# header\n\n${ctx.join("\n")}\n`;
  expect(
    "a required context naming one marked PR job passes",
    build({
      "ci/required-checks.txt": REQ("Class C scope", "Safety MCU host tests (Class C)"),
      ".github/workflows/s.yml": NAMED({ jobs: [
        { id: "changes", name: "Class C scope", marker: true },
        { id: "host-tests", name: "Safety MCU host tests (Class C)", marker: true },
      ] }),
    }),
    null,
  );
  // THE OI-SWCI-26 defect: a job renamed (here, a count put back in its name).
  expect(
    "a required context no job reports any more is caught",
    build({
      "ci/required-checks.txt": REQ("Safety MCU host tests (Class C)"),
      ".github/workflows/s.yml": NAMED({ jobs: [
        { id: "host-tests", name: "Safety MCU host tests (12 targets)", marker: true },
      ] }),
    }),
    "no pull_request-triggered job is named that",
  );
  expect(
    "a matrix on a required job is caught",
    build({
      "ci/required-checks.txt": REQ("Cross"),
      ".github/workflows/s.yml": NAMED({ jobs: [{ id: "x", name: "Cross", marker: true, matrix: true }] }),
    }),
    "has a strategy:",
  );
  // OI-SWCI-23: build-all.yml added "for completeness".
  expect(
    "a required context that only a non-PR workflow reports is caught",
    build({
      "ci/required-checks.txt": REQ("CMake host tests (unfiltered)"),
      ".github/workflows/build-all.yml": NAMED({ pr: false, jobs: [{ id: "h", name: "CMake host tests (unfiltered)" }] }),
    }),
    "OI-SWCI-23",
  );
  expect(
    "a required job in a paths:-filtered workflow is caught",
    build({
      "ci/required-checks.txt": REQ("Cross"),
      ".github/workflows/s.yml": NAMED({ prPaths: ["firmware/**"], jobs: [{ id: "x", name: "Cross", marker: true }] }),
    }),
    "workflow-level pull_request paths",
  );
  expect(
    "two PR jobs sharing a required name are caught",
    build({
      "ci/required-checks.txt": REQ("Cross"),
      ".github/workflows/a.yml": NAMED({ jobs: [{ id: "x", name: "Cross", marker: true }] }),
      ".github/workflows/b.yml": NAMED({ jobs: [{ id: "y", name: "Cross", marker: true }] }),
    }),
    "is the name of 2 jobs",
  );
  expect(
    "a required job without the in-file warning is caught",
    build({
      "ci/required-checks.txt": REQ("Cross"),
      ".github/workflows/s.yml": NAMED({ jobs: [{ id: "x", name: "Cross" }] }),
    }),
    "does not carry the in-file",
  );
  expect(
    "a marked job missing from the manifest is caught",
    build({
      "ci/required-checks.txt": REQ("Cross"),
      ".github/workflows/s.yml": NAMED({ jobs: [
        { id: "x", name: "Cross", marker: true },
        { id: "y", name: "Other", marker: true },
      ] }),
    }),
    "is not in ci/required-checks.txt",
  );
  expect(
    "a marked job with no manifest at all is caught",
    build({ ".github/workflows/s.yml": NAMED({ jobs: [{ id: "x", name: "Cross", marker: true }] }) }),
    "does not exist",
  );

  // ── D. a relevance list covers its own workflow ────────────────────────
  // The web-ci.yml shape observed on PR #261.
  expect(
    "a relevance list that omits its own workflow is caught",
    build({ ".github/workflows/w.yml": WF({ list: ["app/web/**"], noSelf: true, gatedRuns: ["true"] }) }),
    "does not cover .github/workflows/w.yml",
  );
  expect(
    "an exact self-entry satisfies it",
    build({ ".github/workflows/w.yml": WF({ list: ["app/web/**", ".github/workflows/w.yml"], noSelf: true, gatedRuns: ["true"] }) }),
    null,
  );
  expect(
    "another workflow's file does not",
    build({ ".github/workflows/w.yml": WF({ list: ["app/web/**", ".github/workflows/v.yml"], noSelf: true, gatedRuns: ["true"] }) }),
    "does not cover .github/workflows/w.yml",
  );

  // ── E. host-test partition ─────────────────────────────────────────────
  const SAFETY_RE = "^np_(alpha|beta)_tests$";
  /** A host-test workflow in the firmware shape: top-level env, gated configure. */
  const PART_WF = (env: Record<string, string>) =>
    `name: t\non:\n  pull_request:\nenv:\n` +
    Object.entries(env).map(([k, v]) => `  ${k}: '${v}'\n`).join("") +
    WF({ list: ["firmware/CMakeLists.txt", "firmware/alpha/**"], gatedRuns: [CONFIGURE] })
      .replace(/^name: t\non:\n  pull_request:\n/, "");
  const PART = (lines: string[]) => `# header\n${lines.join("\n")}\n`;
  const partTree = (o: {
    tests?: string[]; manifest?: string[] | null; safety?: string; safety2?: string;
    cCount?: string; bCount?: string; total?: string;
  }) => {
    const files: Record<string, string> = {
      "firmware/CMakeLists.txt": ROOT_CMAKE(["alpha"], [], o.tests ?? ["np_alpha_tests", "np_beta_tests", "np_gamma_tests"]),
      "firmware/alpha/CMakeLists.txt": MODULE,
      ".github/workflows/c.yml": PART_WF({ NP_SAFETY_TESTS: o.safety ?? SAFETY_RE, NP_SAFETY_TEST_COUNT: o.cCount ?? "2" }),
      ".github/workflows/b.yml": PART_WF({ NP_SAFETY_TESTS: o.safety2 ?? SAFETY_RE, NP_CLASS_B_TEST_COUNT: o.bCount ?? "1" }),
      ".github/workflows/all.yml": `name: t\non:\n  schedule:\n    - cron: '0 0 * * 0'\nenv:\n` +
        `  NP_SAFETY_TESTS: '${SAFETY_RE}'\n  NP_TOTAL_TEST_COUNT: '${o.total ?? "3"}'\njobs:\n` +
        `  h:\n    runs-on: ubuntu-latest\n    steps:\n      - run: true\n`,
    };
    if (o.manifest !== null) {
      files["ci/host-test-partition.txt"] = PART(o.manifest ?? ["C np_alpha_tests", "C np_beta_tests", "B np_gamma_tests"]);
    }
    return build(files);
  };
  expect("a consistent partition passes", partTree({}), null);
  // OI-SWCI-14: a rename keeps every count true and changes the suite.
  expect(
    "a renamed test is caught although every count still holds",
    partTree({ tests: ["np_alpha_tests", "np_beta_tests", "np_delta_tests"] }),
    "np_delta_tests is registered by add_test() but not in",
  );
  expect(
    "a manifest entry nothing registers is caught",
    partTree({ tests: ["np_alpha_tests", "np_beta_tests", "np_delta_tests"] }),
    "lists np_gamma_tests, which no add_test() registers",
  );
  // OI-SWCI-37, the phase-7 miss: one copy of the regex not updated.
  expect(
    "one workflow's NP_SAFETY_TESTS drifting from the others is caught",
    partTree({ safety2: "^np_(alpha)_tests$" }),
    "NP_SAFETY_TESTS differs between workflows",
  );
  expect(
    "a regex that puts a test in the other half from the manifest is caught",
    partTree({ manifest: ["C np_alpha_tests", "B np_beta_tests", "C np_gamma_tests"], cCount: "2", bCount: "1" }),
    "np_gamma_tests is class C",
  );
  // OI-SWCI-37 second instance: the total's copy in build-all.yml left behind.
  expect(
    "a stale count copy in a non-PR workflow is caught",
    partTree({ total: "2" }),
    "all.yml NP_TOTAL_TEST_COUNT is '2'",
  );
  expect("a stale Class B count is caught", partTree({ bCount: "2" }), "NP_CLASS_B_TEST_COUNT is '2'");
  expect("a missing manifest while the regex exists is caught", partTree({ manifest: null }), "does not exist");
  expect(
    "a partition variable hidden in a job-level env: is caught",
    (() => {
      const root = partTree({});
      const f = join(root, ".github/workflows/b.yml");
      writeFileSync(f, readFileSync(f, "utf8").replace(
        "    needs: changes\n", "    needs: changes\n    env:\n      NP_CLASS_B_TEST_COUNT: '7'\n"));
      return root;
    })(),
    "assigns NP_CLASS_B_TEST_COUNT below the top level",
  );
  expect(
    "a test registered in an if() the host build does not take is not expected",
    build({
      "firmware/CMakeLists.txt": ROOT_CMAKE(["alpha"], [], ["np_alpha_tests", "np_beta_tests", "np_gamma_tests"])
        .replace("    return()", "    if(NP_NEVER)\n        add_test(NAME np_off_tests COMMAND x)\n    endif()\n    return()"),
      "firmware/alpha/CMakeLists.txt": MODULE,
      "ci/host-test-partition.txt": PART(["C np_alpha_tests", "C np_beta_tests", "B np_gamma_tests"]),
      ".github/workflows/c.yml": PART_WF({ NP_SAFETY_TESTS: SAFETY_RE }),
    }),
    null,
  );

  // ── Shapes this file refuses to guess at ───────────────────────────────
  expectThrow(
    "an unsupported pattern shape in a relevance list is a hard error",
    build({
      "scripts/check-x.ts": GATE("docs/**"),
      ".github/workflows/w.yml": WF({
        list: ["docs/*.md"],
        gatedRuns: ["bun scripts/check-x.ts --self-test", "bun scripts/check-x.ts"],
      }),
    }),
    "unsupported pattern shape",
  );
  expectThrow(
    "a job gating on an output that resolves to no list is a hard error",
    build({
      ".github/workflows/w.yml":
        "name: t\non:\n  pull_request:\njobs:\n  changes:\n    runs-on: ubuntu-latest\n    steps:\n      - run: true\n" +
        "  work:\n    runs-on: ubuntu-latest\n    needs: changes\n" +
        "    if: ${{ needs.changes.outputs.relevant != 'false' }}\n    steps:\n      - run: true\n",
      "scripts/check-x.ts": GATE("docs/**"),
    }),
    "resolves to no relevance list",
  );

  // Vacuity: an empty tree must find nothing rather than pass over nothing.
  const empty = audit(build({}));
  if (empty.violations.length) failures.push("empty tree — expected no violations");
  if (empty.gates.length || empty.builds.length) failures.push("empty tree — expected nothing found");

  rmSync(box, { recursive: true, force: true });
  console.log("check-ci-scope self-test");
  if (failures.length) {
    console.error(`\nSELF-TEST FAIL — ${failures.length} assertion(s):`);
    for (const f of failures) console.error("  " + f);
    process.exit(1);
  }
  console.log("  45 case(s): all five correspondences proven to fire and proven NOT to over-fire —");
  console.log("  A/B: the union rule, the weekly-backstop exclusion, the NP_BUILD_TESTS/cross split,");
  console.log("  prefix coverage in both directions, <tree>, and two unparseable shapes;");
  console.log("  C: rename, matrix, non-PR (build-all), paths:-filtered, duplicate, marker both ways;");
  console.log("  D: own workflow omitted, exact self-entry, another workflow's file;");
  console.log("  E: rename under unchanged counts, regex drift, class mismatch, stale count copies, a copy hidden below top level");
  console.log("SELF-TEST PASS — the checker has teeth.");
  process.exit(0);
}

// ── Production run ───────────────────────────────────────────────────────────

const ROOT = join(import.meta.dir, "..");
const { violations, workflows, builds, gates, moduleOwners, gateOwners, contexts, partition } = audit(ROOT);

const prWorkflows = workflows.filter((w) => w.prTriggered);
const prJobs = prWorkflows.reduce((n, w) => n + w.jobs.length, 0);

// A guard that finds nothing to check is the failure it exists to prevent.
if (prJobs === 0 || gates.length === 0) {
  console.error(
    "check-ci-scope: found no PR-triggered jobs or no gates at all — refusing to pass vacuously.",
  );
  process.exit(2);
}
// C and E are silent on a tree with no manifest and no marker. In THIS tree
// both exist, and losing either must fail rather than quietly switch the
// correspondence off.
for (const rel of [REQUIRED_CHECKS, TEST_PARTITION]) {
  if (!existsSync(join(ROOT, rel))) {
    console.error(`check-ci-scope: ${rel} is missing — refusing to pass with its correspondence switched off.`);
    process.exit(2);
  }
}

console.log(
  `scanned: ${prJobs} PR-triggered job(s) in ${prWorkflows.length} workflow(s) · ` +
    `${builds.length} cmake configure(s) · ${moduleOwners.size} build-graph target(s) · ` +
    `${gates.length} gate(s)`,
);
console.log(
  `  (${workflows.length - prWorkflows.length} workflow(s) do not trigger on pull_request and ` +
    `neither require nor provide coverage)`,
);

console.log("\nA — CMake build graph → the PR-triggered job(s) that gate it:");
for (const [target, owners] of [...moduleOwners].sort()) {
  console.log(`  ${target.padEnd(42)} ${[...new Set(owners)].join(", ")}`);
}
console.log("\nB — CI-Scan-Paths → the PR-triggered job(s) that gate it:");
for (const [key, owners] of [...gateOwners].sort()) {
  console.log(`  ${key.padEnd(58)} ${[...new Set(owners)].join(", ")}`);
}

console.log(`\nC — required status-check contexts (${REQUIRED_CHECKS}) → the one job that reports each:`);
for (const [ctx, where] of contexts) console.log(`  ${ctx.padEnd(48)} ${where}`);
console.log(`\nD — every relevance list in a PR-triggered workflow covers its own workflow file`);
if (partition) {
  console.log(
    `\nE — host-test partition (${TEST_PARTITION}): ${partition.c} Class C + ${partition.b} Class B = ` +
      `${partition.c + partition.b}; NP_SAFETY_TESTS identical in ${partition.regexCopies.join(", ")}; ` +
      `counts agree in ${partition.countCopies.join(", ")}`,
  );
}

if (violations.length) {
  console.error(`\n${violations.length} scope-drift violation(s):\n`);
  for (const v of violations) console.error("  " + v);
  console.error(
    "\nNP-SW-CI-001 §5.0: a gate's relevance list must cover the population its CI-Scans\n" +
      "declares, and a module in the build graph must be in scope for a job that builds it.\n" +
      "Widen the list, or move the gate to a job whose list is its population.",
  );
  process.exit(1);
}
console.log(
  "\nEvery build-graph module and every declared scan population is gated; every required\n" +
    "context names one reportable job; the host-test partition agrees everywhere it is written. PASS",
);
process.exit(0);
