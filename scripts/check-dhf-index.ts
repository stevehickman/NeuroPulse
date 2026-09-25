#!/usr/bin/env bun
/**
 * Verify that NP-DHF-001 indexes every controlled Markdown document at the
 * revision the document itself carries.
 *
 * NP-CONV-001 OI-CONV-07 (closed 2026-09-25, GitHub #394). The interim rule was
 * "the DHF is the source of truth; where it and docs/status/document-register.md
 * disagree, the DHF wins". Reconciling the registers against the files found that
 * rule resting on an index with no row at all for ~20 controlled documents
 * (among them NP-CONV-001 and the IEC 62304 SOUP record NP-SOUP-LFS-001) and Rev
 * cells behind their files. A rule that says which index wins is worth nothing
 * if the winning index is not kept true, and nothing kept it true. This does.
 *
 * Authority. For a Markdown document the file's own `**Revision:**` field is the
 * revision — the file IS the document. The DHF row is an index entry and must
 * agree with it. Two checks:
 *
 *   A. every docs/*.md carrying `**Document:**` and an integer `**Revision:**`
 *      has a master-index row in NP-DHF-001: first cell = its serial, and a
 *      link `](./<file>)` in the row;
 *   B. that row's Rev cell begins with the same integer.
 *
 * Out of scope, deliberately:
 *   - .docx / .pdf: the revision text inside a Word file is not reliably
 *     extractable (NP-CONV-001 OI-CONV-04), so there is nothing to compare to.
 *   - docs/superseded/: retired records keep the label they were written with
 *     (NP-CONV-001 §1.1, rename forward never backward), and are indexed by
 *     check-doc-filenames.ts's rules, not this one's.
 *   - A struck-through row (`| ~~NP-…~~ |`) is history, not an index entry.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-dhf-index.ts --self-test
 * CI-Scans: docs/*.md front matter and docs/np_dhf_001.md
 * CI-Scan-Paths: docs/**
 */
import { readdirSync, readFileSync, statSync, mkdirSync, mkdtempSync, writeFileSync, rmSync } from "fs";
import { join, basename } from "path";
import { tmpdir } from "os";

// ── Self-test ────────────────────────────────────────────────────────────────
// Drives the real entry point against fixture trees, in both directions, before
// the checker is trusted (NP-CONV-001 §8).
if (process.argv.includes("--self-test")) {
  const root = mkdtempSync(join(tmpdir(), "np-dhfindex-"));
  const docs = join(root, "docs");
  mkdirSync(docs, { recursive: true });
  const write = (rel: string, body: string) => writeFileSync(join(docs, rel), body);
  const doc = (id: string, rev: string) => `# T\n\n**Document:** ${id}\n**Revision:** ${rev}\n\n---\n`;
  const dhfRow = (id: string, rev: string, file: string) =>
    `| ${id} | T | ${rev} | 2026-01-01 | [${file}](./${file}) | ACTIVE | QMS |\n`;
  const reset = () => {
    for (const e of readdirSync(docs)) rmSync(join(docs, e), { force: true, recursive: true });
  };
  const run = () => {
    const r = Bun.spawnSync([process.execPath, import.meta.path], { cwd: root, stdout: "pipe", stderr: "pipe" });
    return { code: r.exitCode, out: new TextDecoder().decode(r.stdout) + new TextDecoder().decode(r.stderr) };
  };
  const failures: string[] = [];
  const expect = (label: string, want: 0 | 1, needle?: string) => {
    const { code, out } = run();
    if (code !== want) failures.push(`${label} — expected exit ${want}, got ${code}\n${out}`);
    else if (needle && !out.includes(needle)) failures.push(`${label} — output lacked ${JSON.stringify(needle)}`);
  };
  const DHF_HEAD = "**Document:** NP-DHF-001\n**Revision:** 1\n\n| ID | T | Rev | Date | File | S | C |\n|---|---|---|---|---|---|---|\n";

  // Conforming: both documents indexed at their revision (DHF indexes itself).
  reset();
  write("np_foo_001.md", doc("NP-FOO-001", "3"));
  write("np_dhf_001.md", DHF_HEAD + dhfRow("NP-DHF-001", "1", "np_dhf_001.md") + dhfRow("**NP-FOO-001**", "**3**", "np_foo_001.md"));
  expect("conforming tree accepted", 0, "B (Rev agrees with file):  PASS");

  // Rule A: a controlled document with no row.
  reset();
  write("np_foo_001.md", doc("NP-FOO-001", "3"));
  write("np_dhf_001.md", DHF_HEAD + dhfRow("NP-DHF-001", "1", "np_dhf_001.md"));
  expect("rule A rejects an unindexed document", 1, "NP-FOO-001 has no master-index row");

  // Rule A: a struck-through row is history and does not count.
  reset();
  write("np_foo_001.md", doc("NP-FOO-001", "3"));
  write("np_dhf_001.md", DHF_HEAD + dhfRow("NP-DHF-001", "1", "np_dhf_001.md") + dhfRow("~~NP-FOO-001~~", "3", "np_foo_001.md"));
  expect("rule A ignores a struck-through row", 1, "NP-FOO-001 has no master-index row");

  // Rule B: stale Rev cell. 13 must not match 1 by prefix.
  reset();
  write("np_foo_001.md", doc("NP-FOO-001", "13"));
  write("np_dhf_001.md", DHF_HEAD + dhfRow("NP-DHF-001", "1", "np_dhf_001.md") + dhfRow("NP-FOO-001", "1", "np_foo_001.md"));
  expect("rule B rejects a stale Rev cell", 1, "NP-FOO-001: DHF Rev 1, file Rev 13");

  // Empty scope must not read as success.
  reset();
  write("np_dhf_001.md", "no front matter\n");
  const e = run();
  if (e.code === 0) failures.push("a DHF with no front matter was accepted");

  rmSync(root, { recursive: true, force: true });
  console.log("check-dhf-index self-test");
  if (failures.length) {
    console.error(`\nSELF-TEST FAIL — ${failures.length} assertion(s):`);
    for (const f of failures) console.error("  " + f);
    process.exit(1);
  }
  console.log("  rules A and B each proven to reject; conforming tree proven to pass");
  console.log("SELF-TEST PASS — the checker has teeth.");
  process.exit(0);
}

const DHF = "docs/np_dhf_001.md";
const front = (p: string) => {
  const head = readFileSync(p, "utf8").slice(0, 6000).split(/\n---/)[0];
  const id = head.match(/^\*\*Document:\*\*\s*(NP-[A-Z0-9-]+)\s*$/m)?.[1];
  const rev = head.match(/^\*\*Revision:\*\*\s*([0-9]+)\s*$/m)?.[1];
  return { id, rev };
};

const self = front(DHF);
if (!self.id || !self.rev) {
  console.error(`${DHF}: no **Document:**/**Revision:** front matter — cannot establish the index`);
  process.exit(1);
}

// Master-index rows: first cell is a serial (optionally bold), not struck through.
const rows = new Map<string, string[]>(); // file -> [revCell...] for rows whose serial matches
for (const r of readFileSync(DHF, "utf8").split("\n")) {
  const m = r.match(/^\|\s*\*{0,2}(NP-[A-Z0-9-]+?)\*{0,2}\s*\|/);
  if (!m) continue;
  const link = r.match(/\]\(\.\/([^)\s]+)\)/);
  if (!link) continue;
  const cells = r.split(" | ");
  const key = `${m[1]}@${link[1]}`;
  rows.set(key, [...(rows.get(key) ?? []), (cells[2] ?? "").replace(/[*\s]/g, "")]);
}

const violA: string[] = [], violB: string[] = [];
let scanned = 0;
for (const e of readdirSync("docs").sort()) {
  const p = join("docs", e);
  if (!e.endsWith(".md") || statSync(p).isDirectory()) continue;
  const { id, rev } = front(p);
  if (!id || !rev) continue;
  scanned++;
  const revs = rows.get(`${id}@${basename(p)}`);
  if (!revs) {
    violA.push(`A: ${id} has no master-index row in ${DHF} linking ./${basename(p)}`);
    continue;
  }
  for (const cell of revs) {
    const lead = cell.match(/^[0-9]+/)?.[0];
    if (lead !== rev) violB.push(`B: ${id}: DHF Rev ${lead ?? JSON.stringify(cell)}, file Rev ${rev}`);
  }
}

console.log(`scanned: ${scanned} controlled Markdown documents against ${DHF} Rev ${self.rev}`);
console.log(`A (indexed):               ${violA.length ? "FAIL" : "PASS"}`);
violA.forEach((v) => console.log("   " + v));
console.log(`B (Rev agrees with file):  ${violB.length ? "FAIL" : "PASS"}`);
violB.forEach((v) => console.log("   " + v));
if (violA.length + violB.length) {
  console.log("\nFix: set the row's Rev and Date from the file's front matter, or add the row.");
  console.log("editscripts/patch_conv07_dhf_reconcile.py does both mechanically.");
}
process.exit(violA.length + violB.length ? 1 : 0);
