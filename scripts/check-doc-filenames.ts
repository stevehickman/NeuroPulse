#!/usr/bin/env bun
/**
 * Verify NP-CONV-001 §4.0 on the whole document set.
 *
 * Authority for "what serial does this file carry?":
 *   .md            → the **Document:** field inside the file (self-describing)
 *   .docx / .pdf   → the NP-DHF-001 register row that links to it (binary; the
 *                    body text is unreliable — its first NP- token is usually a
 *                    citation, which is how an earlier probe concluded the
 *                    bibliography was NP-REG-PBM1064-001)
 *
 * Checks both halves of the rule:
 *   A. every serialed document's filename == lower_snake(serial) + ext
 *   B. every serial-shaped filename belongs to a serialed document  (exclusivity)
 */
import { readdirSync, readFileSync, statSync } from "fs";
import { join, basename, extname } from "path";

const EXEMPT = new Set(["README.md", "ABBREVIATIONS.md", "FRONT_MATTER_TEMPLATES.md"]);
const SERIAL_SHAPE = /^np_[a-z0-9]+(?:_[a-z0-9]+)*_\d{3}$/;

const dhf = new Map<string, string>();
for (const r of readFileSync("docs/np_dhf_001.md", "utf8").split("\n")) {
  if (!/^\|\s*\*{0,2}NP-[A-Z0-9-]+/.test(r)) continue;
  const c = r.split("|");
  const id = (c[1] ?? "").replace(/\*/g, "").trim().split(/\s/)[0];
  const l = (c[5] ?? "").match(/\]\(\.\/([^)]+)\)/);
  if (/^NP-[A-Z0-9-]+-\d{3}$/.test(id) && l) dhf.set("docs/" + l[1], id);
}

const serialOf = (p: string): string | null => {
  if (extname(p) === ".md") {
    const m = readFileSync(p, "utf8").slice(0, 4000).match(/^\*\*Document:\*\*\s*([A-Z0-9-]+)\s*$/m);
    if (m) return m[1].trim();
  }
  return dhf.get(p) ?? null;
};
const wantName = (id: string, ext: string) => id.toLowerCase().replace(/-/g, "_") + ext;

const violA: string[] = [], violB: string[] = [];
let checked = 0, serialed = 0;
for (const d of ["docs", "docs/superseded"]) {
  for (const e of readdirSync(d)) {
    const p = join(d, e);
    if (statSync(p).isDirectory() || !/\.(md|docx|pdf)$/.test(e) || EXEMPT.has(e)) continue;
    checked++;
    const id = serialOf(p);
    const base = basename(e, extname(e));
    if (id) {
      serialed++;
      const w = wantName(id, extname(e));
      if (e !== w) violA.push(`A: ${p} carries ${id} but is not named ${w}`);
    } else if (SERIAL_SHAPE.test(base)) {
      violB.push(`B: ${p} has a serial-shaped name but no serial of record`);
    }
  }
}
console.log(`checked ${checked} files · ${serialed} carry a serial`);
console.log(`A (name == serial):        ${violA.length ? "FAIL" : "PASS"}`);
violA.forEach((v) => console.log("   " + v));
console.log(`B (only serials look like serials): ${violB.length ? "FAIL" : "PASS"}`);
violB.forEach((v) => console.log("   " + v));
process.exit(violA.length + violB.length ? 1 : 0);
