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
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-doc-filenames.ts --self-test
 * CI-Scans: every .md/.docx/.pdf in docs/ and docs/superseded/
 */
import { readdirSync, readFileSync, statSync, mkdirSync, mkdtempSync, writeFileSync, rmSync } from "fs";
import { join, basename, extname } from "path";
import { tmpdir } from "os";

// ── Self-test ────────────────────────────────────────────────────────────────
// §4.0.1a says this checker "was falsified before it was trusted, in both
// directions". That was true, and it was done by hand and thrown away — so the
// claim decayed into an assertion the moment it was written. This is the same
// falsification, kept, and re-run by CI before the checker is trusted.
//
// It drives the real entry point as a subprocess against fixture trees rather
// than calling an extracted helper, because the thing that must not regress is
// what `bun scripts/check-doc-filenames.ts` does in a directory.
if (process.argv.includes("--self-test")) {
  const root = mkdtempSync(join(tmpdir(), "np-docfilenames-"));
  const docs = join(root, "docs");
  mkdirSync(join(docs, "superseded"), { recursive: true });

  const write = (rel: string, body: string) => writeFileSync(join(docs, rel), body);
  const reset = () => {
    for (const e of readdirSync(docs)) {
      if (e !== "superseded") rmSync(join(docs, e), { force: true });
    }
    // The DHF register is the serial authority for .docx/.pdf and is read
    // unconditionally, so every fixture needs it.
    write("np_dhf_001.md", "**Document:** NP-DHF-001\n\n| ID | a | b | c | Link |\n");
  };

  const run = (): { code: number; out: string } => {
    const r = Bun.spawnSync([process.execPath, import.meta.path], {
      cwd: root,
      stdout: "pipe",
      stderr: "pipe",
    });
    return {
      code: r.exitCode,
      out: new TextDecoder().decode(r.stdout) + new TextDecoder().decode(r.stderr),
    };
  };

  const failures: string[] = [];
  const expect = (label: string, want: 0 | 1, needle?: string) => {
    const { code, out } = run();
    if (code !== want) {
      failures.push(`${label} — expected exit ${want}, got ${code}`);
    } else if (needle && !out.includes(needle)) {
      failures.push(`${label} — exit ${want} but output lacked ${JSON.stringify(needle)}`);
    }
  };

  // Conforming tree: a serialed document named for its serial.
  reset();
  write("np_foo_001.md", "**Document:** NP-FOO-001\n");
  expect("conforming tree accepted", 0, "A (name == serial):        PASS");

  // Rule A: carries NP-FOO-001 but is not named np_foo_001.md.
  reset();
  write("np_bar_001.md", "**Document:** NP-FOO-001\n");
  expect("rule A rejects a serial/filename mismatch", 1, "carries NP-FOO-001");

  // Rule B: serial-shaped filename with no serial of record. This is the
  // direction that matters — it is what keeps the name shape usable as evidence.
  reset();
  write("np_baz_001.md", "No Document field here.\n");
  expect("rule B rejects a serial-shaped file with no serial", 1, "no serial of record");

  // A checker that cannot see the tree passes both rules trivially. An empty
  // docs/ must therefore not read as success.
  reset();
  const empty = run();
  if (empty.code === 0 && !empty.out.includes("1 carry a serial")) {
    failures.push("empty tree — passed without reporting what it scanned");
  }

  rmSync(root, { recursive: true, force: true });
  console.log("check-doc-filenames self-test");
  if (failures.length) {
    console.error(`\nSELF-TEST FAIL — ${failures.length} assertion(s):`);
    for (const f of failures) console.error("  " + f);
    process.exit(1);
  }
  console.log("  rules A and B each proven to reject; conforming tree proven to pass");
  console.log("SELF-TEST PASS — the checker has teeth.");
  process.exit(0);
}

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
console.log(`scanned: ${checked} files · ${serialed} carry a serial`);
console.log(`A (name == serial):        ${violA.length ? "FAIL" : "PASS"}`);
violA.forEach((v) => console.log("   " + v));
console.log(`B (only serials look like serials): ${violB.length ? "FAIL" : "PASS"}`);
violB.forEach((v) => console.log("   " + v));
process.exit(violA.length + violB.length ? 1 : 0);
