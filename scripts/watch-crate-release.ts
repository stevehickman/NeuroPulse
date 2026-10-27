#!/usr/bin/env bun
/**
 * watch-crate-release.ts — has a crate published a STABLE release at or above a version?
 *
 *   bun scripts/watch-crate-release.ts <crate> <floor>      ask crates.io; print the answer
 *   bun scripts/watch-crate-release.ts --self-test          check the version logic, offline
 *
 * Written for GitHub #689. `spake2` (the direct-link pairing crate) has no stable release on the
 * RustCrypto digest 0.11 generation, which is the one thing keeping `block-buffer` 0.10.4 (and so
 * the OSV finding GHSA-qwgh-2vcv-g2f7) in common/Cargo.lock. The weekly workflow
 * .github/workflows/crate-release-watch.yml runs this and, when the answer turns to "found",
 * comments once on #689. Nothing here changes the repository.
 *
 * "Stable" means: not yanked, and no pre-release suffix (`-pre`, `-rc`, `-alpha` ...). The floor
 * is the first version of the generation being waited for (0.5.0 for spake2), so a patch release
 * of the old generation (0.4.1) is not mistaken for the one wanted.
 *
 * Output: one line on stdout, and (when GITHUB_OUTPUT is set) `found`, `version`, `published`
 * for the workflow. Exit 0 when the answer is "found" or "none"; exit 2 when crates.io could not
 * be read or answered something that is not a version list, so a broken watch fails visibly
 * instead of reporting "none" for ever.
 *
 * CI-Kind: report
 */
import { appendFileSync } from "fs";

export interface CrateVersion {
  num: string;
  yanked: boolean;
  created_at?: string;
}

/** `1.2.3` to [1, 2, 3]; null for anything that is not three numeric parts (a pre-release is rejected here too). */
export function parseStable(v: string): [number, number, number] | null {
  const m = /^(\d+)\.(\d+)\.(\d+)$/.exec(v);
  return m ? [Number(m[1]), Number(m[2]), Number(m[3])] : null;
}

/** Numeric, part by part: 0.10.0 is above 0.9.0 (a string comparison gets that wrong). */
export function compare(a: [number, number, number], b: [number, number, number]): number {
  for (let i = 0; i < 3; i++) if (a[i] !== b[i]) return a[i] - b[i];
  return 0;
}

/** The highest stable, un-yanked version at or above `floor`, or null. */
export function newestStableAtLeast(versions: CrateVersion[], floor: string): CrateVersion | null {
  const f = parseStable(floor);
  if (!f) throw new Error(`floor '${floor}' is not a stable version (major.minor.patch)`);
  let best: { v: CrateVersion; p: [number, number, number] } | null = null;
  for (const v of versions) {
    if (v.yanked) continue;
    const p = parseStable(v.num);
    if (!p || compare(p, f) < 0) continue;
    if (!best || compare(p, best.p) > 0) best = { v, p };
  }
  return best ? best.v : null;
}

function selfTest(): void {
  const v = (num: string, yanked = false): CrateVersion => ({ num, yanked });
  const cases: Array<[string, CrateVersion[], string | null]> = [
    ["today's spake2: a stable 0.4.0 and a pre-release 0.5.0-pre.0 is not an answer", [v("0.4.0"), v("0.5.0-pre.0")], null],
    ["a release candidate is not stable", [v("0.5.0-rc.1")], null],
    ["a stable 0.5.0 is the answer", [v("0.4.0"), v("0.5.0-pre.0"), v("0.5.0")], "0.5.0"],
    ["a yanked 0.5.0 is not", [v("0.4.0"), v("0.5.0", true)], null],
    ["a patch of the old generation (0.4.9) is below the floor", [v("0.4.0"), v("0.4.9")], null],
    ["0.10.0 is above 0.9.0 (numeric, not text order)", [v("0.9.0"), v("0.10.0")], "0.10.0"],
    ["1.0.0 counts, and the highest wins", [v("0.5.0"), v("1.0.0"), v("0.7.2")], "1.0.0"],
    ["junk version strings are ignored", [v("abc"), v("0.5"), v("0.5.0.1")], null],
    ["an empty list is none", [], null],
  ];
  let bad = 0;
  for (const [name, versions, want] of cases) {
    const got = newestStableAtLeast(versions, "0.5.0")?.num ?? null;
    if (got !== want) {
      bad++;
      console.error(`FAIL ${name}: got ${got}, want ${want}`);
    }
  }
  let threw = false;
  try { newestStableAtLeast([], "0.5.0-pre.0"); } catch { threw = true; }
  if (!threw) { bad++; console.error("FAIL a pre-release floor must be refused"); }
  if (bad) { console.error(`${bad} case(s) wrong`); process.exit(1); }
  console.log(`watch-crate-release self-test: ${cases.length + 1} cases, all correct.`);
}

async function main(): Promise<void> {
  const args = process.argv.slice(2);
  if (args[0] === "--self-test") return selfTest();
  const [crate, floor] = args;
  if (!crate || !floor) {
    console.error("usage: bun scripts/watch-crate-release.ts <crate> <floor>   |   --self-test");
    process.exit(2);
  }
  let versions: CrateVersion[];
  try {
    const res = await fetch(`https://crates.io/api/v1/crates/${encodeURIComponent(crate)}`, {
      headers: { "User-Agent": "NeuroPulse crate-release-watch (https://github.com/stevehickman/NeuroPulse)" },
    });
    if (!res.ok) throw new Error(`crates.io answered ${res.status}`);
    const body = (await res.json()) as { versions?: CrateVersion[] };
    if (!Array.isArray(body.versions) || body.versions.length === 0) throw new Error("no version list in the reply");
    versions = body.versions;
  } catch (e) {
    console.error(`watch-crate-release: could not read ${crate}: ${(e as Error).message}`);
    process.exit(2);
  }
  const hit = newestStableAtLeast(versions, floor);
  const out = process.env.GITHUB_OUTPUT;
  if (hit) {
    const published = (hit.created_at ?? "").slice(0, 10);
    console.log(`found ${crate} ${hit.num} (stable, published ${published || "date unknown"})`);
    if (out) appendFileSync(out, `found=true\nversion=${hit.num}\npublished=${published}\n`);
  } else {
    const newest = versions.map((x) => x.num).filter((n) => parseStable(n) && !versions.find((x) => x.num === n)?.yanked)
      .sort((a, b) => compare(parseStable(b)!, parseStable(a)!))[0];
    console.log(`none: ${crate} has no stable release at or above ${floor} (newest stable: ${newest ?? "none"})`);
    if (out) appendFileSync(out, "found=false\n");
  }
}

if (import.meta.main) await main();
