#!/usr/bin/env bun
/**
 * check-ci-scope.ts — a relevance list must cover what the job it gates actually reads.
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
 * ── The two correspondences it checks ────────────────────────────────────────
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
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-ci-scope.ts --self-test
 * CI-Scans: every PR-triggered workflow job, against the CMake build graph it configures and the CI-Scan-Paths of the gates it runs
 * CI-Scan-Paths: .github/workflows/** firmware/** scripts/** ci/**
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

      jobs.push({ wf: file, id: starts[s]!.id, ifExpr: ifLine, runs, scopeEnv, outputs });
    }
  }
  return { file, prTriggered, prPaths, jobs };
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
function subdirsOf(text: string, dirRel: string, defs: Map<string, string>): string[] {
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

type Build = { job: Job; srcRel: string; modules: string[] };

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
    const seen = new Set<string>();
    const visit = (dirRel: string) => {
      const f = join(root, dirRel, "CMakeLists.txt");
      if (!existsSync(f)) return;
      const text = readFileSync(f, "utf8");
      optionDefaults(text, defs);
      for (const child of subdirsOf(text, dirRel, defs)) {
        if (seen.has(child)) continue;
        seen.add(child);
        modules.push(child);
        visit(child);
      }
    };
    visit(srcRel);
    out.push({ job, srcRel, modules });
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

// ── The audit ────────────────────────────────────────────────────────────────

type Report = {
  violations: string[];
  workflows: Workflow[];
  builds: Build[];
  gates: Gate[];
  moduleOwners: Map<string, string[]>;
  gateOwners: Map<string, string[]>;
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

  return { violations, workflows, builds, gates, moduleOwners, gateOwners };
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
  }) => {
    const on = opts.prPaths
      ? `on:\n  pull_request:\n    paths:\n${opts.prPaths.map((p) => `      - '${p}'`).join("\n")}\n`
      : `on:\n${opts.pr === false ? "  schedule:\n    - cron: '0 0 * * 0'\n" : "  pull_request:\n"}`;
    let s = `name: t\n${on}jobs:\n`;
    if (opts.list) {
      s +=
        `  changes:\n    runs-on: ubuntu-latest\n    outputs:\n      relevant: x\n    env:\n` +
        `      NP_SCOPE_PATHS: |\n${opts.list.map((p) => `        ${p}`).join("\n")}\n` +
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
  const ROOT_CMAKE = (subdirs: string[], crossOnly: string[] = []) =>
    `cmake_minimum_required(VERSION 3.20)\nproject(t C)\n` +
    `option(NP_BUILD_TESTS "t" OFF)\n` +
    `if(NP_BUILD_TESTS)\n${subdirs.map((s) => `    add_subdirectory(${s})`).join("\n")}\n    return()\nendif()\n` +
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
  console.log("  21 case(s): both correspondences proven to fire and proven NOT to over-fire —");
  console.log("  the union rule, the weekly-backstop exclusion, the NP_BUILD_TESTS/cross split,");
  console.log("  prefix coverage in both directions, <tree>, and two unparseable shapes");
  console.log("SELF-TEST PASS — the checker has teeth.");
  process.exit(0);
}

// ── Production run ───────────────────────────────────────────────────────────

const ROOT = join(import.meta.dir, "..");
const { violations, workflows, builds, gates, moduleOwners, gateOwners } = audit(ROOT);

const prWorkflows = workflows.filter((w) => w.prTriggered);
const prJobs = prWorkflows.reduce((n, w) => n + w.jobs.length, 0);

// A guard that finds nothing to check is the failure it exists to prevent.
if (prJobs === 0 || gates.length === 0) {
  console.error(
    "check-ci-scope: found no PR-triggered jobs or no gates at all — refusing to pass vacuously.",
  );
  process.exit(2);
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
console.log("\nEvery build-graph module and every declared scan population is gated. PASS");
process.exit(0);
