#!/usr/bin/env bun
/**
 * check-wavelength-rules-register.ts — a released wavelength rule set is never edited, and a shipped window never
 * changes without a release (SWR-HV-03, NP-NPPS-REF-001 §7a.1, OI-WRV-01, GitHub #564).
 *
 * The register (common/npps/wavelength-rules-register.json) is append-only: one row per release NeurOne shipped. The Rust
 * core's own test (common/npps-core/tests/rules_register.rs) holds what one tree can show: the rows are well formed and
 * rise, no set is released twice, and the last row is the shipped file's hash. It cannot see a change over time, so a
 * released row edited in place together with the shipped file passes it. This compares the tree with the merge base.
 *
 *   W1  Every row of the base register is, unchanged and in the same place, a row of this register. Rows may be appended.
 *       An edited, removed or reordered released row fails.
 *   W2  If a window of the shipped defaults (protocols/predefined/00-wavelength-rules.npps: nominal_nm, min_nm, max_nm of
 *       a channel) differs from the base's, this register has more rows than the base's. A shipped window changed with no
 *       release recorded fails ("do not change a shipped window: no version exists to record the change under").
 *   W3  This register parses and lists a release, and the shipped file states windows for all three channels, so a
 *       comparison cannot go quiet because the thing compared was not read.
 *
 * Reach, stated narrowly: it compares against one base, so it is run for a pull request (the merge base), and with no base
 * it reports that and checks W3 only. It reads window numbers by text, not the core's parser, so a window written in a form
 * the pattern does not read fails W3 rather than passing. It does not hash anything: that is the core test's.
 *
 * Usage: bun scripts/check-wavelength-rules-register.ts [--base <git ref>] [--self-test]
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-wavelength-rules-register.ts --self-test
 * CI-Scans: the wavelength-rules register and the shipped defaults file, against the merge base (W1 released rows unchanged, W2 a changed window has a new release, W3 both were read)
 * CI-Self-Test-Reads-Tree: one case reads the real register and shipped file to prove the reader sees three channels and a release, which a fixture cannot show
 * CI-Scan-Paths: common/npps/wavelength-rules-register.json protocols/predefined/00-wavelength-rules.npps scripts/check-wavelength-rules-register.ts
 */
import { execFileSync } from "child_process";
import { readFileSync } from "fs";
import { join, resolve } from "path";

export const REGISTER = "common/npps/wavelength-rules-register.json";
export const SHIPPED = "protocols/predefined/00-wavelength-rules.npps";

export interface Row {
  major: number;
  minor: number;
  hash: string;
}
export interface Finding {
  rule: string;
  message: string;
}
export interface Side {
  register: string | null;
  shipped: string | null;
}

export function rowsOf(text: string): Row[] | string {
  let v: any;
  try {
    v = JSON.parse(text);
  } catch (e) {
    return `the register is not JSON: ${(e as Error).message}`;
  }
  if (!Array.isArray(v?.releases)) return "the register has no `releases` array";
  return v.releases.map((r: any) => ({ major: r.major, minor: r.minor, hash: r.hash }));
}

/** channel -> "nominal,min,max", from the `channel "led_N" { … }` blocks. */
export function windowsOf(text: string): Map<string, string> {
  const out = new Map<string, string>();
  for (const m of text.matchAll(/^\s*channel\s+"(led_\d+)"\s*\{([^}]*)\}/gm)) {
    const n = (k: string) => new RegExp(`${k}\\s*:\\s*([0-9]+(?:\\.[0-9]+)?)`).exec(m[2]!)?.[1];
    const w = ["nominal_nm", "min_nm", "max_nm"].map(n);
    if (w.every((x) => x !== undefined)) out.set(m[1]!, w.map((x) => Number(x)).join(","));
  }
  return out;
}

export function check(base: Side | null, cur: Side): Finding[] {
  const f: Finding[] = [];
  const curRows = cur.register === null ? "the register is missing" : rowsOf(cur.register);
  const curWin = cur.shipped === null ? new Map<string, string>() : windowsOf(cur.shipped);
  if (typeof curRows === "string") f.push({ rule: "W3", message: curRows });
  else if (curRows.length === 0) f.push({ rule: "W3", message: "the register lists no release" });
  if (curWin.size !== 3) f.push({ rule: "W3", message: `${SHIPPED}: ${curWin.size} channel window(s) read, expected 3 (led_660, led_808, led_1064)` });
  if (base === null || typeof curRows === "string") return f;

  const baseRows = base.register === null ? [] : rowsOf(base.register);
  if (typeof baseRows === "string") {
    f.push({ rule: "W1", message: `the base's register could not be read (${baseRows}); it cannot be compared` });
    return f;
  }
  baseRows.forEach((b, i) => {
    const c = curRows[i];
    if (!c) f.push({ rule: "W1", message: `released row ${i + 1} (${b.major}.${b.minor}) is gone: a released row is never removed` });
    else if (c.major !== b.major || c.minor !== b.minor || c.hash !== b.hash)
      f.push({ rule: "W1", message: `released row ${i + 1} was ${b.major}.${b.minor} ${b.hash.slice(0, 12)}… and is now ${c.major}.${c.minor} ${c.hash.slice(0, 12)}…: a released row is never edited or reordered; append a release instead` });
  });
  if (base.shipped !== null && curWin.size === 3) {   // an unreadable shipped file is W3's finding, not a window change
    const baseWin = windowsOf(base.shipped);
    const changed = [...new Set([...baseWin.keys(), ...curWin.keys()])].filter((k) => baseWin.get(k) !== curWin.get(k));
    if (changed.length > 0 && curRows.length <= baseRows.length)
      f.push({ rule: "W2", message: `${SHIPPED}: the window of ${changed.join(", ")} changed (${changed.map((k) => `${baseWin.get(k) ?? "none"} -> ${curWin.get(k) ?? "none"}`).join("; ")}) and no release was appended to the register: a shipped window changes only in a release (NP-NPPS-REF-001 §7a.1)` });
  }
  return f;
}

function git(root: string, ...args: string[]): string {
  return execFileSync("git", args, { cwd: root, encoding: "utf8", stdio: ["ignore", "pipe", "pipe"] });
}
function show(root: string, ref: string, path: string): string | null {
  try {
    return git(root, "show", `${ref}:${path}`);
  } catch {
    return null;   // absent at the base: the file is being introduced
  }
}

function selfTest(): number {
  const H = (c: string) => c.repeat(64);
  const reg = (...rows: [number, number, string][]) => JSON.stringify({ releases: rows.map(([major, minor, hash]) => ({ major, minor, hash })) });
  const npps = (w808max = 840) => `wavelength_rules "d" {\n level: global\n channel "led_660" { nominal_nm: 660 min_nm: 650 max_nm: 680 }\n channel "led_808" {\n  nominal_nm: 808\n  min_nm: 798\n  max_nm: ${w808max}\n }\n channel "led_1064" { nominal_nm: 1064 min_nm: 1054 max_nm: 1074 }\n}\n`;
  const side = (register: string | null, shipped: string | null = npps()): Side => ({ register, shipped });
  const base = side(reg([1, 0, H("a")]));
  const rules = (b: Side | null, c: Side) => check(b, c).map((x) => x.rule).join();
  const failures: string[] = [];
  const t = (name: string, ok: boolean) => ok || failures.push(name);

  t("an unchanged tree passes", rules(base, base) === "");
  t("appending a release with a changed window passes", rules(base, side(reg([1, 0, H("a")], [1, 1, H("b")]), npps(860))) === "");
  t("appending a release with unchanged windows passes (the core test judges the hash)", rules(base, side(reg([1, 0, H("a")], [1, 1, H("b")]))) === "");
  t("an edited released row fails (W1) even when the shipped file moves with it (and W2: no release was appended)", rules(base, side(reg([1, 0, H("b")]), npps(860))) === "W1,W2");
  t("an edited released row with the shipped file unmoved fails (W1)", rules(base, side(reg([1, 0, H("b")]))) === "W1");
  t("an edited version fails (W1)", rules(base, side(reg([1, 1, H("a")]))) === "W1");
  t("a removed released row fails (W1)", rules(side(reg([1, 0, H("a")], [1, 1, H("b")])), side(reg([1, 0, H("a")]))) === "W1");
  t("a reordered released row fails (W1, both rows)", rules(side(reg([1, 0, H("a")], [1, 1, H("b")])), side(reg([1, 1, H("b")], [1, 0, H("a")]))) === "W1,W1");
  t("a changed window with no new release fails (W2)", rules(base, side(reg([1, 0, H("a")]), npps(860))) === "W2");
  t("a window comment or whitespace change is not a window change", rules(base, side(reg([1, 0, H("a")]), "# note\n" + npps().replace(/ {2}/g, "   "))) === "");
  t("no base checks W3 only and passes a good tree", rules(null, base) === "");
  t("a register that does not parse fails (W3) with no base too", rules(null, side("{")) === "W3");
  t("an empty register fails (W3)", rules(null, side(reg())) === "W3");
  t("a shipped file whose windows cannot be read fails (W3): the comparison must not go quiet", rules(base, side(reg([1, 0, H("a")]), "wavelength_rules \"d\" {}")) === "W3");
  t("a file introduced by this change passes (nothing released before it)", rules(side(null, null), base) === "");
  t("a base whose register cannot be read fails (W1)", rules(side("{"), base) === "W1");
  t("the real tree has a release and three windows the reader can see", (() => {
    const root = resolve(import.meta.dir, "..");
    const r = rowsOf(readFileSync(join(root, REGISTER), "utf8"));
    return typeof r !== "string" && r.length >= 1 && windowsOf(readFileSync(join(root, SHIPPED), "utf8")).size === 3;
  })());

  failures.forEach((x) => console.error(`FAIL ${x}`));
  console.log(failures.length ? `SELF-TEST FAIL (${failures.length})` : "SELF-TEST PASS — each rule goes red on an input broken in its way");
  return failures.length ? 1 : 0;
}

function main(argv: string[]): number {
  if (argv.includes("--self-test")) return selfTest();
  const root = resolve(import.meta.dir, "..");
  const bi = argv.indexOf("--base");
  let base: string | null = bi >= 0 ? argv[bi + 1]! : null;
  if (base === null) {
    try {
      git(root, "rev-parse", "-q", "--verify", "HEAD^2");
      base = "HEAD^1";   // a pull_request merge checkout
    } catch {
      base = null;
    }
  }
  const cur: Side = { register: readFileSync(join(root, REGISTER), "utf8"), shipped: readFileSync(join(root, SHIPPED), "utf8") };
  let baseSide: Side | null = null;
  if (base !== null) {
    try {
      git(root, "rev-parse", "-q", "--verify", `${base}^{commit}`);
    } catch {
      console.error(`FAIL the base ${base} does not resolve: fetch it (actions/checkout fetch-depth: 0), or pass --base`);
      return 1;
    }
    baseSide = { register: show(root, base, REGISTER), shipped: show(root, base, SHIPPED) };
  }
  const findings = check(baseSide, cur);
  const rows = rowsOf(cur.register!);
  console.log(`scanned: ${typeof rows === "string" ? 0 : rows.length} release(s), ${windowsOf(cur.shipped!).size} channel window(s), ${base === null ? "no base to compare (W3 only)" : `against ${base}`}`);
  for (const x of findings) console.error(`FAIL ${x.rule} ${x.message}`);
  if (findings.length) return 1;
  console.log("check-wavelength-rules-register: ok");
  return 0;
}

if (import.meta.main) process.exit(main(process.argv.slice(2)));
