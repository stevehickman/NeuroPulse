#!/usr/bin/env bun
/**
 * check-wavelength-register.ts — a released set of wavelength rules is never changed.
 *
 * `common/npps/wavelength-rules-register.json` lists every wavelength-rules set NeurOne has shipped: one row per
 * release, a version `major.minor` that NeurOne assigns and the hash of the set's canonical form that the core computes
 * (`NP-NPPS-REF-001` §7a.1, `OI-WRV-01`, `SWR-HV-03`). A session record carries the windows of the set it ran under and
 * a reader recognises a release by finding its hash here, so a row that is edited, removed or reordered after release
 * changes what an old record says it ran under. The register is append-only, and this gate holds a pull request to that.
 *
 * What it checks, against the register as it stands in the pull request and as it stood at its base:
 *
 *   A. the register is well formed: every version is `major.minor` in whole numbers of at most 255 (the stamp's two
 *      bytes), every hash is 64 lowercase hex digits, no hash is released twice, and the versions strictly go up, so a
 *      version is never reused or lowered;
 *   B. every row the base had is still there, in the same place, with the same version and the same hash. Rows may be
 *      appended and nothing else may happen. Whitespace and the `$comment` are not rows.
 *
 * What it does not check, because other tests own it: that the shipped file `protocols/predefined/00-wavelength-rules.npps`
 * hashes to the last row, or states its version (`common/npps-core/tests/wavelength_version.rs`, which needs the core's
 * hash). This gate needs only the register and git.
 *
 * Without a base (a push to `main`, a local run) it checks A only and says so. On a pull_request merge checkout the base
 * is HEAD^1, the tip of the branch the pull request merges into, so the comparison is exactly the pull request's change.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-wavelength-register.ts --self-test
 * CI-Scans: common/npps/wavelength-rules-register.json, as it is in the pull request and as it is at its base
 * CI-Scan-Paths: common/npps/wavelength-rules-register.json
 */
import { execFileSync } from "child_process";
import { mkdtempSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "fs";
import { tmpdir } from "os";
import { dirname, join } from "path";

const REGISTER = "common/npps/wavelength-rules-register.json";

export type Row = { version: string; hash: string };
/** `base` says what there was to compare: no base ref at all, a base without the register (it is new in this change), or the base's rows. */
export type Verdict = { violations: string[]; rows: number; baseRows: number | null; base: "none" | "absent" | "present" };

function git(cwd: string, ...args: string[]): string {
  return execFileSync("git", args, { cwd, encoding: "utf8", stdio: ["ignore", "pipe", "pipe"] });
}
function show(cwd: string, ref: string, path: string): string | null {
  try { return git(cwd, "show", `${ref}:${path}`); } catch { return null; }
}

/** The rows of a register text, or the reason it has none. */
export function parse(text: string): { rows: Row[] } | { error: string } {
  let v: unknown;
  try { v = JSON.parse(text); } catch (e) { return { error: `is not JSON: ${(e as Error).message}` }; }
  const list = (v as { releases?: unknown } | null)?.releases;
  if (!Array.isArray(list)) return { error: "has no `releases` array" };
  const rows: Row[] = [];
  for (const [i, r] of list.entries()) {
    if (typeof r?.version !== "string" || typeof r?.hash !== "string") return { error: `row ${i + 1} has no string version and hash` };
    rows.push({ version: r.version, hash: r.hash });
  }
  return { rows };
}

/** `major.minor` as two whole numbers of at most 255, or null. */
function release(v: string): [number, number] | null {
  const m = /^(0|[1-9]\d*)\.(0|[1-9]\d*)$/.exec(v);
  if (!m) return null;
  const a = Number(m[1]), b = Number(m[2]);
  return a <= 255 && b <= 255 ? [a, b] : null;
}

export function wellFormed(rows: Row[]): string[] {
  const out: string[] = [];
  if (rows.length === 0) out.push("A: the register has no release");
  rows.forEach((r, i) => {
    const at = `row ${i + 1} (${r.version})`;
    const v = release(r.version);
    if (!v) out.push(`A: ${at}: the version is not major.minor in whole numbers of at most 255`);
    if (!/^[0-9a-f]{64}$/.test(r.hash)) out.push(`A: ${at}: the hash is not 64 lowercase hex digits`);
    if (rows.slice(0, i).some((p) => p.hash === r.hash)) out.push(`A: ${at}: this hash is already released`);
    const p = i > 0 ? release(rows[i - 1]!.version) : null;
    if (v && p && (v[0] < p[0] || (v[0] === p[0] && v[1] <= p[1]))) {
      out.push(`A: ${at}: a version never goes down and is never reused (the row before it is ${rows[i - 1]!.version})`);
    }
  });
  return out;
}

export function evaluate(root: string, base: string | null): Verdict {
  const violations: string[] = [];
  const head = parse(readFileSync(join(root, REGISTER), "utf8"));
  if ("error" in head) return { violations: [`A: ${REGISTER} ${head.error}`], rows: 0, baseRows: null, base: "none" };
  violations.push(...wellFormed(head.rows));
  let baseRows: number | null = null;
  const baseText = base === null ? null : show(root, base, REGISTER);
  const state: Verdict["base"] = base === null ? "none" : baseText === null ? "absent" : "present";
  if (baseText !== null) {
    const b = parse(baseText);
    if ("error" in b) return { violations, rows: head.rows.length, baseRows: null, base: state };   // a base that was already broken has nothing to hold
    baseRows = b.rows.length;
    b.rows.forEach((was, i) => {
      const now = head.rows[i];
      if (!now) {
        violations.push(`B: row ${i + 1} (${was.version}) was released and is gone: a released row is never removed`);
      } else if (now.version !== was.version || now.hash !== was.hash) {
        const same = head.rows.some((r) => r.version === was.version && r.hash === was.hash);
        violations.push(
          `B: row ${i + 1} was released as ${was.version} ${was.hash} and is now ${now.version} ${now.hash}: ` +
          (same ? "a released row is never moved" : "a released row is never edited, and a new set is a new row at the end"),
        );
      }
    });
  }
  return { violations, rows: head.rows.length, baseRows, base: state };
}

function selfTest(): void {
  let failures = 0;
  const check = (ok: boolean, name: string): void => { console.log(`${ok ? "PASS" : "FAIL"}: ${name}`); if (!ok) failures++; };
  const h = (c: string): string => c.repeat(64);
  const reg = (rows: Row[], comment = "c"): string => JSON.stringify({ $comment: comment, releases: rows }, null, 2);
  const R10: Row = { version: "1.0", hash: h("a") };
  const R11: Row = { version: "1.1", hash: h("b") };
  const R20: Row = { version: "2.0", hash: h("c") };

  /** A repository whose main holds `baseText` (or no register) and whose topic branch holds `headText`. */
  const scenario = (baseText: string | null, headText: string): Verdict => {
    const r = mkdtempSync(join(tmpdir(), "wlreg-"));
    try {
      const w = (p: string, t: string): void => { mkdirSync(dirname(join(r, p)), { recursive: true }); writeFileSync(join(r, p), t); };
      const c = (m: string): void => { git(r, "add", "-A"); git(r, "-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "--allow-empty", "-m", m); };
      git(r, "init", "-q", "-b", "main");
      w("README", "x");
      if (baseText !== null) w(REGISTER, baseText);
      c("base");
      git(r, "checkout", "-q", "-b", "topic");
      w(REGISTER, headText);
      c("topic");
      return evaluate(r, "main");
    } finally { rmSync(r, { recursive: true, force: true }); }
  };
  const codes = (v: Verdict): string => v.violations.map((s) => s[0]).sort().join("");
  const base = reg([R10, R11]);

  check(codes(scenario(base, base)) === "", "an unchanged register passes");
  check(scenario(base, base).baseRows === 2 && scenario(base, base).rows === 2, "and the gate reports the rows it compared");
  check(codes(scenario(base, reg([R10, R11, R20]))) === "", "appending a release passes");
  check(codes(scenario(base, reg([R10, R11], "a different comment"))) === "", "a changed $comment is not a row");
  check(codes(scenario(base, JSON.stringify({ releases: [R10, R11] }))) === "", "nor is whitespace or key order");
  check(codes(scenario(base, reg([{ ...R10, hash: h("d") }, R11]))) === "B", "editing a released hash fails (B)");
  check(codes(scenario(base, reg([R10, { ...R11, version: "1.2" }]))) === "B", "editing a released version fails (B)");
  check(codes(scenario(base, reg([R10]))) === "B", "removing the last released row fails (B)");
  check(codes(scenario(base, reg([R11]))) !== "", "removing the first released row fails");
  check(codes(scenario(base, reg([R11, R10]))).includes("B"), "reordering released rows fails (B)");
  check(scenario(base, reg([R11, R10])).violations.some((s) => /never moved/.test(s)), "and says a row was moved, not edited");
  check(scenario(base, reg([R10, R11, { version: "2.0", hash: R10.hash }])).violations.some((s) => /already released/.test(s)), "appending a hash that is already released fails (A)");
  check(codes(scenario(base, reg([R10, R11, { version: "1.1", hash: h("e") }]))) === "A", "appending a reused version fails (A)");
  check(codes(scenario(base, reg([R10, R11, { version: "1.0", hash: h("e") }]))) === "A", "appending a lower version fails (A)");
  check(codes(scenario(base, reg([R10, R11, { version: "2.0", hash: "ABC" }]))) === "A", "appending a hash that is not 64 lowercase hex digits fails (A)");
  check(codes(scenario(base, reg([R10, R11, { version: "2.0", hash: h("E") }]))) === "A", "uppercase hex fails (A)");
  for (const bad of ["2", "2.0.1", "v2.0", "02.0", "2.256", "", "2.x"]) {
    check(codes(scenario(base, reg([R10, R11, { version: bad, hash: h("e") }]))) === "A", `appending the version '${bad}' fails (A)`);
  }
  check(codes(scenario(null, reg([R10]))) === "" && scenario(null, reg([R10])).base === "absent", "a register with no base file (its first commit) passes if well formed, and the gate says the register is new");
  check(codes(scenario(null, reg([{ version: "1.0", hash: "short" }]))) === "A", "and fails if it is not");
  check(codes(scenario(base, "not json")) === "A", "a register that is not JSON fails (A)");
  check(codes(scenario(base, JSON.stringify({ releases: [] }))).includes("A"), "an empty register fails (A)");
  check(codes(scenario(base, JSON.stringify({ rows: [R10] }))) === "A", "a register with no releases array fails (A)");

  const noBase = ((): Verdict => {
    const r = mkdtempSync(join(tmpdir(), "wlreg-"));
    try {
      mkdirSync(join(r, "common/npps"), { recursive: true });
      writeFileSync(join(r, REGISTER), reg([R10, R11]));
      return evaluate(r, null);
    } finally { rmSync(r, { recursive: true, force: true }); }
  })();
  check(codes(noBase) === "" && noBase.baseRows === null && noBase.base === "none", "with no base only well-formedness is checked, and the gate says it had no base");

  if (failures > 0) { console.error(`\nFAILED (${failures})`); process.exit(1); }
  console.log("\nALL PASS");
}

if (import.meta.main) {
  const a = process.argv.slice(2);
  if (a.includes("--self-test")) selfTest();
  else {
    const root = process.cwd();
    const bi = a.indexOf("--base");
    let base: string | null = bi >= 0 ? a[bi + 1]! : null;
    if (base === null) {
      try { git(root, "rev-parse", "-q", "--verify", "HEAD^2"); base = "HEAD^1"; } catch { base = null; }   // a pull_request merge checkout
    }
    const v = evaluate(root, base);
    console.log(
      `scanned: ${v.rows} released set(s) in ${REGISTER}` +
        (v.base === "none" ? ", no base to compare (well-formedness only)"
          : v.base === "absent" ? `, new since ${base} (well-formedness only: there is no earlier row to hold)`
          : `, against ${v.baseRows} at ${base}`),
    );
    if (v.violations.length) {
      console.error("\nThe wavelength-rules register is append-only (NP-NPPS-REF-001 §7a.1):");
      for (const x of v.violations) console.error(`  - ${x}`);
      process.exit(1);
    }
    console.log("The register is well formed and every released row is as it was released: PASS");
  }
}
