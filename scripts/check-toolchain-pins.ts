#!/usr/bin/env bun
/**
 * check-toolchain-pins.ts — CI names a toolchain version or a runner image in one file, and nowhere else.
 *
 * .github/toolchain.json holds the macOS image and the exact version of every toolchain a job installs
 * (JDK, Gradle, Bun, Rust, Python, .NET, the Arm GNU toolchain). The setup-toolchain action
 * (.github/actions/setup-toolchain) is the only place that reads those versions to install anything, and
 * the jobs that run on macOS read the image through their Relevance job's `macos` output, because
 * `runs-on` cannot read a file. A workflow that writes one of these itself is one build that can drift
 * from the others: the macOS 14 image was retired under one workflow while the iOS ones sat on macOS 15,
 * and `bun-version: latest` stood in 33 places.
 *
 * Seven rules over the workflows, the action and the pins file:
 *
 *   1. NO MACOS LITERAL. No `runs-on:` or `os:` in a workflow names a macOS image (`macos-26`,
 *      `macos-latest`). A comment may.
 *   2. NO DIRECT SETUP ACTION. No workflow `uses:` actions/setup-java, gradle/actions/setup-gradle,
 *      oven-sh/setup-bun, actions/setup-python, actions/setup-dotnet or the Arm GNU toolchain action.
 *   3. NO VERSION KEY, NO RUSTUP. No workflow carries a `bun-version:`, `java-version:`, `gradle-version:`,
 *      `python-version:` or `dotnet-version:` key, `rustup toolchain install` or `rustup default`.
 *   4. EXACT PINS. toolchain.json has every key, and `bun`, `rust` and `gradle` are exact x.y.z (never
 *      `latest` or `stable`), and `macos` is a numbered image, never `macos-latest`.
 *   5. THE ACTION READS THE FILE. Inside the setup-toolchain action, every `*-version:` and `release:`
 *      value is an expression, never a literal.
 *   6. THE OUTPUT EXISTS. A workflow that reads `needs.changes.outputs.macos` defines that output.
 *   7. CHECKOUT FIRST. Within a job, the checkout step comes before the first use of the action, because
 *      the action reads the checked-out file.
 *
 * Out of scope, stated rather than omitted: the versions of the actions themselves (`actions/checkout@v4`),
 * which are references and not toolchains; packages fetched by a package manager (`brew install`, `apt-get`,
 * `cargo install`), which a lockfile or a flag pins where one exists; and the Linux and Windows runner
 * images (`ubuntu-latest`, `windows-latest`), which float by choice.
 *
 * Usage: bun scripts/check-toolchain-pins.ts [--self-test]
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-toolchain-pins.ts --self-test
 * CI-Scans: every workflow for a hardcoded macOS image, toolchain version or setup action; the setup-toolchain action for a literal version; toolchain.json for a floating pin
 * CI-Scan-Paths: .github/workflows/** .github/actions/** .github/toolchain.json scripts/check-toolchain-pins.ts
 */
import { readFileSync, readdirSync, statSync } from "fs";
import { join, resolve } from "path";

export const PINS_FILE = ".github/toolchain.json";
export const ACTION_FILE = ".github/actions/setup-toolchain/action.yml";
export const ACTION_REF = "./.github/actions/setup-toolchain";

const REQUIRED_KEYS = ["macos", "java", "javaDistribution", "gradle", "bun", "rust", "python", "dotnet", "armGcc"];
const EXACT_KEYS = ["gradle", "bun", "rust"];
const SEMVER = /^\d+\.\d+\.\d+$/;
const MACOS_IMAGE = /^macos-\d+(-intel|-large|-xlarge)?$/;

const SETUP_ACTIONS = [
  "actions/setup-java", "gradle/actions/setup-gradle", "oven-sh/setup-bun", "actions/setup-python",
  "actions/setup-dotnet", "carlosperate/arm-none-eabi-gcc-action",
];
// Anywhere on a line, so the inline `with: { java-version: '17' }` form is seen as well as the block form.
const VERSION_KEY = /\b(bun|java|gradle|python|dotnet)-version\s*:/;
const RUSTUP = /\brustup\s+(toolchain\s+install|default)\b/;
// `macos-` followed by a name in the VALUE of runs-on or os: `macos-26`, `macos-latest`. The output expression `outputs.macos`
// has no hyphen, and the value stops at a comma or brace so a sibling key (`dir: macos-arm64`) is not read as an image.
const MACOS_RUNNER = /\b(runs-on|os)\s*:[^,}\n]*\bmacos-[a-z0-9.]+/;

export type Finding = { file: string; line: number; rule: number; text: string };

/** A string as a literal inside a RegExp: every metacharacter, backslash included, is escaped. */
export const escapeRe = (s: string): string => s.replace(/[.*+?^${}()|[\]\\/]/g, "\\$&");

/** A line without its trailing comment. A `#` inside a YAML flow value is not used in these files. */
function code(line: string): string {
  const m = line.match(/(^|\s)#/);
  return m ? line.slice(0, m.index) : line;
}

/** Split a workflow into its jobs: [startLine, text] pairs, keyed by the two-space `name:` under `jobs:`. */
function jobs(lines: string[]): { start: number; lines: string[] }[] {
  const at = lines.findIndex((l) => /^jobs:\s*$/.test(l));
  if (at < 0) return [];
  const out: { start: number; lines: string[] }[] = [];
  for (let i = at + 1; i < lines.length; i++) {
    if (/^  [A-Za-z0-9_-]+:\s*$/.test(lines[i])) out.push({ start: i, lines: [] });
    if (out.length) out[out.length - 1].lines.push(lines[i]);
  }
  return out;
}

export function checkWorkflow(file: string, text: string): Finding[] {
  const lines = text.split("\n");
  const out: Finding[] = [];
  const add = (i: number, rule: number, t: string) => out.push({ file, line: i + 1, rule, text: t });
  lines.forEach((raw, i) => {
    const l = code(raw);
    if (!l.trim()) return;
    if (MACOS_RUNNER.test(l)) add(i, 1, `names a macOS image: ${l.trim()}. Use needs.changes.outputs.macos`);
    for (const a of SETUP_ACTIONS) {
      if (new RegExp(`uses:\\s*${escapeRe(a)}@`).test(l)) add(i, 2, `uses ${a} directly. Use ${ACTION_REF}`);
    }
    if (VERSION_KEY.test(l)) add(i, 3, `carries a toolchain version: ${l.trim()}. The version is in ${PINS_FILE}`);
    if (RUSTUP.test(l)) add(i, 3, `installs Rust itself: ${l.trim()}. Use ${ACTION_REF} with rust: 'true'`);
  });
  const readsMacos = lines.some((l) => /needs\.changes\.outputs\.macos\b/.test(code(l)));
  const definesMacos = lines.some((l) => /^\s+macos:\s*\$\{\{\s*steps\.[a-z]+\.outputs\.macos\s*\}\}/.test(l));
  if (readsMacos && !definesMacos) add(0, 6, "reads needs.changes.outputs.macos but its changes job defines no `macos:` output");
  for (const j of jobs(lines)) {
    const use = j.lines.findIndex((l) => l.includes(`uses: ${ACTION_REF}`));
    if (use < 0) continue;
    const co = j.lines.findIndex((l) => /uses:\s*actions\/checkout@/.test(l));
    if (co < 0 || co > use) add(j.start + use, 7, `uses ${ACTION_REF} before a checkout step in the same job`);
  }
  return out;
}

export function checkAction(file: string, text: string): Finding[] {
  const out: Finding[] = [];
  text.split("\n").forEach((raw, i) => {
    const l = code(raw);
    const m = l.match(/^\s*(?:- )?([a-z-]*-version|release)\s*:\s*(.+?)\s*$/);
    if (m && !/^\$\{\{.*\}\}$/.test(m[2])) {
      out.push({ file, line: i + 1, rule: 5, text: `${m[1]}: ${m[2]} is a literal. It must be read from ${PINS_FILE}` });
    }
  });
  return out;
}

export function checkPins(file: string, text: string): Finding[] {
  const out: Finding[] = [];
  const add = (t: string) => out.push({ file, line: 1, rule: 4, text: t });
  let p: Record<string, unknown>;
  try { p = JSON.parse(text); } catch (e) { add(`not valid JSON: ${(e as Error).message}`); return out; }
  for (const k of REQUIRED_KEYS) if (typeof p[k] !== "string" || !(p[k] as string).trim()) add(`missing key "${k}"`);
  for (const k of EXACT_KEYS) {
    const v = p[k];
    if (typeof v === "string" && !SEMVER.test(v)) add(`"${k}" is "${v}": it must be an exact x.y.z, never a moving name`);
  }
  if (typeof p.macos === "string" && !MACOS_IMAGE.test(p.macos)) add(`"macos" is "${p.macos}": it must be a numbered image such as macos-26, never macos-latest`);
  return out;
}

export function check(files: Map<string, string>): Finding[] {
  const out: Finding[] = [];
  for (const [path, text] of files) {
    if (path === PINS_FILE) out.push(...checkPins(path, text));
    else if (path === ACTION_FILE) out.push(...checkAction(path, text));
    else if (/^\.github\/workflows\/[^/]+\.ya?ml$/.test(path)) out.push(...checkWorkflow(path, text));
  }
  if (!files.has(PINS_FILE)) out.push({ file: PINS_FILE, line: 1, rule: 4, text: "the pins file does not exist" });
  if (!files.has(ACTION_FILE)) out.push({ file: ACTION_FILE, line: 1, rule: 5, text: "the setup-toolchain action does not exist" });
  return out;
}

function selfTest(): void {
  const pins = JSON.stringify({ macos: "macos-26", java: "17", javaDistribution: "temurin", gradle: "8.11.1", bun: "1.4.4", rust: "1.97.0", python: "3.11", dotnet: "8.0.x", armGcc: "13.2.Rel1" });
  const action = "runs:\n  steps:\n    - uses: actions/setup-java@v4\n      with:\n        java-version: ${{ steps.pins.outputs.java }}\n";
  const wf = [
    "jobs:",
    "  changes:",
    "    outputs:",
    "      macos: ${{ steps.pins.outputs.macos }}",
    "  build:",
    "    runs-on: ${{ needs.changes.outputs.macos }}",
    "    steps:",
    "      - uses: actions/checkout@v4",
    `      - uses: ${ACTION_REF}`,
    "        with:",
    "          bun: 'true'",
    "",
  ].join("\n");
  const base = new Map<string, string>([[PINS_FILE, pins], [ACTION_FILE, action], [".github/workflows/a.yml", wf]]);
  const with_ = (path: string, text: string) => new Map([...base, [path, text]]);
  const expect = (name: string, files: Map<string, string>, want: number, rule?: number) => {
    const f = check(files);
    const got = rule === undefined ? f.length : f.filter((x) => x.rule === rule).length;
    if (got !== want) {
      console.error(`self-test FAILED: ${name}: expected ${want} finding(s)${rule ? ` of rule ${rule}` : ""}, got ${got}`);
      for (const x of f) console.error(`  ${x.file}:${x.line} rule ${x.rule}: ${x.text}`);
      process.exit(1);
    }
  };
  const wfPath = ".github/workflows/a.yml";
  // The helper that builds the setup-action patterns escapes a backslash as well as the other metacharacters.
  const esc = escapeRe("a\\b.c/d");
  if (esc !== "a\\\\b\\.c\\/d" || !new RegExp(`^${esc}$`).test("a\\b.c/d") || new RegExp(`^${esc}$`).test("a\\b-c/d")) {
    console.error(`self-test FAILED: escapeRe(${JSON.stringify("a\\b.c/d")}) gave ${JSON.stringify(esc)}`);
    process.exit(1);
  }
  expect("clean tree passes", base, 0);
  expect("a macOS runner fails", with_(wfPath, wf.replace("${{ needs.changes.outputs.macos }}", "macos-26")), 1, 1);
  expect("macos-latest fails", with_(wfPath, wf.replace("${{ needs.changes.outputs.macos }}", "macos-latest")), 1, 1);
  expect("a macOS matrix os fails", with_(wfPath, wf + "      matrix: { os: macos-15 }\n"), 1, 1);
  expect("a macOS image in an expression fails", with_(wfPath, wf + "    runs-on: ${{ (x && 'macos-latest') || 'ubuntu-latest' }}\n"), 1, 1);
  expect("a sibling key naming macos-arm64 passes", with_(wfPath, wf + "      - { name: macOS arm64, os: '${{ needs.changes.outputs.macos }}', dir: macos-arm64 }\n"), 0);
  expect("a flow-mapping os with a macOS image fails", with_(wfPath, wf + "      - { name: macOS arm64, os: macos-15, dir: macos-arm64 }\n"), 1, 1);
  expect("a macOS image in a comment passes", with_(wfPath, wf + "      # was macos-15\n"), 0);
  expect("ubuntu-latest passes", with_(wfPath, wf.replace("${{ needs.changes.outputs.macos }}", "ubuntu-latest")), 0);
  expect("setup-bun used directly fails", with_(wfPath, wf + "      - uses: oven-sh/setup-bun@v2\n"), 1, 2);
  expect("setup-java used directly fails", with_(wfPath, wf + "      - uses: actions/setup-java@v4\n"), 1, 2);
  expect("the Arm GCC action used directly fails", with_(wfPath, wf + "      - uses: carlosperate/arm-none-eabi-gcc-action@v1\n"), 1, 2);
  expect("actions/checkout is not a toolchain", with_(wfPath, wf + "      - uses: actions/checkout@v4\n"), 0);
  expect("bun-version latest fails", with_(wfPath, wf + "        with:\n          bun-version: latest\n"), 1, 3);
  expect("an inline java-version fails", with_(wfPath, wf + "        with: { distribution: temurin, java-version: '17' }\n"), 1, 3);
  expect("a list-item version key fails", with_(wfPath, wf + "          - java-version: '17'\n"), 1, 3);
  expect("rustup toolchain install fails", with_(wfPath, wf + "        run: rustup toolchain install stable\n"), 1, 3);
  expect("rustup default fails", with_(wfPath, wf + "        run: rustup default stable\n"), 1, 3);
  expect("rustup target add passes", with_(wfPath, wf + "        run: rustup target add wasm32-unknown-unknown\n"), 0);
  expect("a missing macos output fails", with_(wfPath, wf.replace("      macos: ${{ steps.pins.outputs.macos }}\n", "")), 1, 6);
  expect("the action before checkout fails", with_(wfPath, wf.replace("      - uses: actions/checkout@v4\n", "") + ""), 1, 7);
  expect("checkout after the action fails", with_(wfPath, wf.replace("      - uses: actions/checkout@v4\n", "").replace("          bun: 'true'\n", "          bun: 'true'\n      - uses: actions/checkout@v4\n")), 1, 7);
  expect("a literal version in the action fails", with_(ACTION_FILE, action + "        gradle-version: '8.11.1'\n"), 1, 5);
  expect("a literal release in the action fails", with_(ACTION_FILE, action + "        release: 13.2.Rel1\n"), 1, 5);
  expect("an expression version in the action passes", with_(ACTION_FILE, action), 0);
  expect("bun latest in the pins fails", with_(PINS_FILE, pins.replace('"1.4.4"', '"latest"')), 1, 4);
  expect("rust stable in the pins fails", with_(PINS_FILE, pins.replace('"1.97.0"', '"stable"')), 1, 4);
  expect("macos-latest in the pins fails", with_(PINS_FILE, pins.replace('"macos-26"', '"macos-latest"')), 1, 4);
  expect("a missing pins key fails", with_(PINS_FILE, pins.replace('"python":"3.11",', "")), 1, 4);
  expect("invalid pins JSON fails", with_(PINS_FILE, "{"), 1, 4);
  const noPins = new Map(base); noPins.delete(PINS_FILE);
  expect("a missing pins file fails", noPins, 1, 4);
  const noAction = new Map(base); noAction.delete(ACTION_FILE);
  expect("a missing action fails", noAction, 1, 5);
  console.log("check-toolchain-pins self-test: ok");
}

function walk(dir: string, out: string[] = []): string[] {
  for (const n of readdirSync(dir)) {
    const p = join(dir, n);
    if (statSync(p).isDirectory()) walk(p, out); else out.push(p);
  }
  return out;
}

if (import.meta.main) {
  if (process.argv.includes("--self-test")) { selfTest(); process.exit(0); }
  const root = resolve(import.meta.dir, "..");
  const files = new Map<string, string>();
  const rel = (p: string) => p.slice(root.length + 1).split("\\").join("/");
  for (const p of [...walk(join(root, ".github/workflows")), ...walk(join(root, ".github/actions"))]) files.set(rel(p), readFileSync(p, "utf8"));
  try { files.set(PINS_FILE, readFileSync(join(root, PINS_FILE), "utf8")); } catch { /* reported by check() */ }
  const findings = check(files);
  console.log(`scanned: ${files.size} file(s): every workflow, the setup-toolchain action and ${PINS_FILE}`);
  if (findings.length === 0) {
    console.log(`check-toolchain-pins: no workflow names a macOS image or a toolchain version itself; ${PINS_FILE} is the one place.`);
    process.exit(0);
  }
  console.error(`${findings.length} toolchain-pin violation(s):\n`);
  for (const f of findings) console.error(`  ${f.file}:${f.line}  rule ${f.rule}: ${f.text}`);
  console.error(`\nVersions and the macOS image live in ${PINS_FILE}; ${ACTION_FILE} installs them.`);
  process.exit(1);
}
