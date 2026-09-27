#!/usr/bin/env bun
/**
 * Per-target executed-line floor for the CMake host tests (NP-SW-CI-001
 * OI-SWCI-14, the second half).
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-host-test-floors.ts --self-test
 * CI-Scan-Probe: external — needs a --coverage host-test build, which only the host-test jobs configure
 * CI-Scans: every CMake host-test target of the selected class, run alone on a --coverage build, against ci/host-test-floors.txt
 * CI-Scan-Paths: firmware/safety_mcu/** firmware/crypto/** firmware/common/** firmware/CMakeLists.txt firmware/bootloader/** firmware/hub_control/** firmware/vendor/** firmware/ota/** firmware/zone_announce/** firmware/hrv_biofeedback/** firmware/pbm/** firmware/cervical_vns/** firmware/sloreta_hdtdcs/** firmware/anon/** firmware/edf/** firmware/factory_reset/** firmware/uhdr_key/** firmware/shdr/** firmware/platform/** firmware/application/** ci/host-test-floors.txt scripts/check-host-test-floors.ts
 *
 * Why this exists. ci/host-test-partition.txt catches a target renamed,
 * swapped or moved between halves. It cannot catch EROSION INSIDE a target: a
 * case deleted, an assertion loop cut short, a `return 0;` left at the top of
 * main(). The target still registers, runs and passes, and every count and
 * name check stays green.
 *
 * Why lines and not cases. The suites share no harness: they report "ALL
 * TESTS PASSED", "OK: 0 failure(s)", "86 checks, 0 failure(s)", or nothing per
 * case at all. A case-count floor would first need every suite rewritten to
 * one reporting form. The executed-line count needs nothing from the suite:
 * build with --coverage, clear every .gcda, run ONE target, and count the
 * distinct in-repo (file, line) pairs gcov reports executed. Deleting a case
 * removes the lines only it reached (its own body at least), so the count
 * falls. Falsified on np_thermal_interlock_tests: dropping the call to one of
 * its six cases took it from 138 to 126 lines.
 *
 * What it cannot see. A case whose every line is still reached by another case
 * (a pure duplicate) can go without the count moving. An assertion weakened on
 * the same line (`==` to `>=`) executes the same lines. Those need mutation
 * testing, which this is not.
 *
 * The floor is a FLOOR, not an exact value. A target above its floor passes and
 * the surplus is printed, so adding cases never fails anything. Lowering a
 * floor is a visible diff to ci/host-test-floors.txt in the PR that removed the
 * coverage, which is the point: shrinkage stops being silent. A toolchain
 * change (a new gcc on the runner image) can also move the count; build-all.yml
 * is where that shows first.
 *
 * Usage (on a --coverage build of the host-test super-project):
 *   bun scripts/check-host-test-floors.ts <build-dir> [--class C|B]   check
 *   bun scripts/check-host-test-floors.ts <build-dir> --write         re-measure every floor
 *   bun scripts/check-host-test-floors.ts --self-test
 *
 * scripts/check-ci-scope.ts (correspondence E) checks on every PR that the
 * floor file names exactly the targets in ci/host-test-partition.txt, so a new
 * target cannot arrive without a floor.
 */
import { readFileSync, writeFileSync, readdirSync, rmSync, statSync } from "fs";
import { join, resolve } from "path";
import { spawnSync } from "child_process";

const PARTITION = "ci/host-test-partition.txt";
const FLOORS = "ci/host-test-floors.txt";

const clean = (text: string): string[] =>
  text.split("\n").map((l) => l.replace(/#.*$/, "").trim()).filter((l) => l !== "");

/** Distinct executed (file, line) pairs under root, from `gcov --json-format --stdout` output. */
export function countExecuted(gcovJsonLines: string, cwd: string, root: string): number {
  const hit = new Set<string>();
  for (const line of gcovJsonLines.split("\n")) {
    if (!line.trim()) continue;
    for (const f of JSON.parse(line).files ?? []) {
      const file = resolve(cwd, f.file);
      if (!file.startsWith(root + "/")) continue;   // system headers are not ours to floor
      for (const l of f.lines ?? []) if (l.count > 0) hit.add(`${file}:${l.line_number}`);
    }
  }
  return hit.size;
}

/** Parse the floor file; malformed lines become errors. */
export function parseFloors(text: string): { floors: Map<string, number>; errors: string[] } {
  const floors = new Map<string, number>();
  const errors: string[] = [];
  for (const l of clean(text)) {
    const m = /^(\S+)\s+(\d+)$/.exec(l);
    if (!m) { errors.push(`${FLOORS}: malformed line '${l}' (want '<ctest name> <lines>')`); continue; }
    if (floors.has(m[1]!)) errors.push(`${FLOORS} lists ${m[1]} more than once`);
    floors.set(m[1]!, Number(m[2]));
  }
  return { floors, errors };
}

/** Compare measured counts against floors for the targets checked. */
export function compare(
  checked: string[], floors: Map<string, number>, measured: Map<string, number>,
): { report: string[]; errors: string[] } {
  const report: string[] = [];
  const errors: string[] = [];
  for (const n of checked) {
    const got = measured.get(n);
    const floor = floors.get(n);
    if (got === undefined) { errors.push(`${n} was not measured`); continue; }
    if (floor === undefined) { errors.push(`${n} has no floor in ${FLOORS}`); continue; }
    report.push(`${got < floor ? "✗" : "✓"} ${n.padEnd(34)} ${String(got).padStart(6)} lines  ` +
      `(floor ${floor}${got > floor ? `, +${got - floor}` : ""})`);
    if (got < floor) {
      errors.push(`${n} executed ${got} lines, below its floor of ${floor} — a case or assertion path ` +
        `stopped running. If that was deliberate, lower the floor in ${FLOORS} in the same PR and say why`);
    }
  }
  return { report, errors };
}

// ── Self-test ────────────────────────────────────────────────────────────────
// Hermetic: exercises the parse and compare logic on fixtures. The measuring
// half (gcov on a real build) was falsified by hand; see the header.
if (process.argv.includes("--self-test")) {
  const failures: string[] = [];
  const expect = (label: string, cond: boolean) => { if (!cond) failures.push(label); };
  const F = parseFloors("# c\nnp_a_tests 10\nnp_b_tests 20\n");
  const floors = F.floors;
  expect("well-formed floors parse", F.errors.length === 0 && floors.get("np_b_tests") === 20);
  expect("a floor with no number is caught", parseFloors("np_a_tests\n").errors.some((e) => e.includes("malformed")));
  expect("a duplicate floor is caught", parseFloors("np_a_tests 1\nnp_a_tests 2\n").errors.some((e) => e.includes("more than once")));
  const m = (a: number, b: number) => new Map([["np_a_tests", a], ["np_b_tests", b]]);
  expect("at the floor passes", compare(["np_a_tests", "np_b_tests"], floors, m(10, 20)).errors.length === 0);
  expect("above the floor passes", compare(["np_a_tests", "np_b_tests"], floors, m(11, 99)).errors.length === 0);
  expect("one line below the floor is caught",
    compare(["np_a_tests", "np_b_tests"], floors, m(10, 19)).errors.some((e) => e.includes("np_b_tests executed 19")));
  expect("a target with no floor is caught",
    compare(["np_c_tests"], floors, new Map([["np_c_tests", 5]])).errors.some((e) => e.includes("no floor")));
  expect("a target not measured is caught",
    compare(["np_a_tests"], floors, new Map()).errors.some((e) => e.includes("not measured")));
  const J = (file: string, lines: [number, number][]) =>
    JSON.stringify({ files: [{ file, lines: lines.map(([line_number, count]) => ({ line_number, count })) }] });
  const gcov = [
    J("/r/firmware/a.c", [[1, 3], [2, 0], [3, 1]]),
    J("/r/firmware/h.h", [[5, 1]]),
    J("/r/firmware/h.h", [[5, 2], [6, 1]]),        // a header seen from two TUs counts once per line
    J("/usr/include/stdio.h", [[9, 1]]),            // outside the repo: not counted
    J("rel/b.c", [[1, 1]]),                         // relative to the build dir
  ].join("\n");
  expect("executed lines are counted once, zero-count and out-of-repo lines excluded",
    countExecuted(gcov, "/r/build", "/r") === 5 && countExecuted(gcov, "/elsewhere", "/r") === 4);
  if (failures.length) {
    for (const f of failures) console.error(`✗ ${f}`);
    process.exit(1);
  }
  console.log("SELF-TEST PASS — parse, compare and count all fail where they should.");
  process.exit(0);
}

// ── Measure ──────────────────────────────────────────────────────────────────

const ROOT = resolve(import.meta.dir, "..");
const args = process.argv.slice(2);
const write = args.includes("--write");
const cls = args.includes("--class") ? args[args.indexOf("--class") + 1] : null;
const buildDir = args.find((a, i) => !a.startsWith("--") && args[i - 1] !== "--class");
if (!buildDir || (cls !== null && cls !== "C" && cls !== "B") || (write && cls !== null)) {
  console.error("usage: bun scripts/check-host-test-floors.ts <coverage-build-dir> [--class C|B | --write]");
  process.exit(2);
}
const BUILD = resolve(buildDir);

function walk(dir: string, ext: string, out: string[] = []): string[] {
  for (const e of readdirSync(dir)) {
    const p = join(dir, e);
    if (statSync(p).isDirectory()) walk(p, ext, out);
    else if (e.endsWith(ext)) out.push(p);
  }
  return out;
}

if (walk(BUILD, ".gcno").length === 0) {
  console.error(`✗ ${BUILD} has no .gcno files — configure it with -DCMAKE_C_FLAGS=--coverage`);
  process.exit(2);
}

const partition = clean(readFileSync(join(ROOT, PARTITION), "utf8")).map((l) => l.split(/\s+/));
const names = partition.filter(([c]) => cls === null || c === cls).map(([, n]) => n!);

/** Distinct in-repo (file, line) pairs executed by one ctest target alone. */
function measure(name: string): number {
  for (const f of walk(BUILD, ".gcda")) rmSync(f);
  const run = spawnSync("ctest",
    ["--test-dir", BUILD, "-R", `^${name}$`, "--no-tests=error", "--output-on-failure"], { encoding: "utf8" });
  if (run.status !== 0) {
    console.error(run.stdout + run.stderr);
    throw new Error(`${name} failed or did not run — a floor is only measured on a passing target`);
  }
  const gcda = walk(BUILD, ".gcda");
  if (gcda.length === 0) throw new Error(`${name} wrote no .gcda — was it built with --coverage?`);
  const g = spawnSync("gcov", ["--json-format", "--stdout", ...gcda],
    { cwd: BUILD, encoding: "utf8", maxBuffer: 1 << 30 });
  if (g.status !== 0) throw new Error(`gcov failed for ${name}: ${g.stderr}`);
  return countExecuted(g.stdout, BUILD, ROOT);
}

const measured = new Map<string, number>();
for (const n of names) measured.set(n, measure(n));

if (write) {
  // Keep the leading comment block; replace everything after it.
  const lines = readFileSync(join(ROOT, FLOORS), "utf8").split("\n");
  const end = lines.findIndex((l) => l.trim() !== "" && !l.startsWith("#"));
  const header = (end < 0 ? lines : lines.slice(0, end)).filter((l) => l.startsWith("#"));
  const body = names.map((n) => `${n} ${measured.get(n)}`);
  writeFileSync(join(ROOT, FLOORS), [...header, "", ...body, ""].join("\n"));
  console.log(`wrote ${body.length} floors to ${FLOORS}`);
  process.exit(0);
}

const parsed = parseFloors(readFileSync(join(ROOT, FLOORS), "utf8"));
const { report, errors } = compare(names, parsed.floors, measured);
errors.unshift(...parsed.errors);
console.log(`scanned: ${names.length} host-test target(s)${cls ? ` (Class ${cls})` : ""}`);
for (const r of report) console.log(r);
if (errors.length) {
  for (const e of errors) console.log(`::error::${e}`);
  process.exit(1);
}
console.log(`\n✓ all ${names.length} target(s) at or above their executed-line floor`);
