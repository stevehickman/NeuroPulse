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
 *   B. that row's Rev cell begins with the same integer;
 *   C. NP-DHF-001's own document-history table (the first table under a
 *      `## … History` heading) has a row for the DHF's current revision.
 *
 * C was added after PR #451 (2026-09-26): the Rev 102 history row landed in the
 * master index instead of the history table, and A and B, which read only rows
 * whose first cell is a serial, could not see it. The misplaced row itself is
 * caught by check-md-tables.ts (wrong cell count). C catches the other half:
 * the history table left without an entry for the revision being published,
 * whether the row was misplaced or never written.
 *
 * D–G were added at NP-CONV-001 Rev 11 (2026-10-01), which states what
 * the Title and Status columns hold (§4.4). With no rule, each PR that bumped a
 * document's revision pasted a "**Rev N (date): …**" note into its row, the next
 * PR stacked its note in front, and cells grew to 6,000 characters. They read
 * only §5, the master index:
 *
 *   D. no Title or Status cell carries a revision change note ("Rev N (date",
 *      "Rev N:", "Rev N →"). That history belongs in the document's own
 *      history table and in NP-DHF-001 §9;
 *   E. a Title cell is at most TITLE_MAX characters and a Status cell at most
 *      STATUS_MAX, so a summary or a history in prose is caught as well;
 *   F. a serial has at most one live row. A second row for the same serial is
 *      either a stale copy or needs its serial cell disambiguated
 *      ("NP-X-001 (variant)"), as NP-HW-FPC-001 and NP-COORD-001 already do.
 *   G. for a row whose File cell links a Markdown document directly under docs/
 *      (`[…](./np_x_001.md)`), the Title equals that file's first `# ` heading,
 *      bold and runs of whitespace ignored (principal, 2026-09-30). An exact match
 *      leaves no gray area for a summary to creep back in. `.docx` rows and
 *      docs/superseded/ rows are exempt: a Word file's heading is not reliably
 *      extractable (OI-CONV-04), and a retired record keeps what it was written as.
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
import { existsSync } from "fs";
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
  const HIST = (rev: string) => `\n## 9. Document History\n\n| Rev | Date | Author | Description |\n|---|---|---|---|\n| ${rev} | 2026-01-01 | A | d |\n`;

  // Conforming: both documents indexed at their revision (DHF indexes itself).
  reset();
  write("np_foo_001.md", doc("NP-FOO-001", "3"));
  write("np_dhf_001.md", DHF_HEAD + dhfRow("NP-DHF-001", "1", "np_dhf_001.md") + dhfRow("**NP-FOO-001**", "**3**", "np_foo_001.md") + HIST("1"));
  expect("conforming tree accepted", 0, "B (Rev agrees with file):  PASS");

  // Rule A: a controlled document with no row.
  reset();
  write("np_foo_001.md", doc("NP-FOO-001", "3"));
  write("np_dhf_001.md", DHF_HEAD + dhfRow("NP-DHF-001", "1", "np_dhf_001.md") + HIST("1"));
  expect("rule A rejects an unindexed document", 1, "NP-FOO-001 has no master-index row");

  // Rule A: a struck-through row is history and does not count.
  reset();
  write("np_foo_001.md", doc("NP-FOO-001", "3"));
  write("np_dhf_001.md", DHF_HEAD + dhfRow("NP-DHF-001", "1", "np_dhf_001.md") + dhfRow("~~NP-FOO-001~~", "3", "np_foo_001.md") + HIST("1"));
  expect("rule A ignores a struck-through row", 1, "NP-FOO-001 has no master-index row");

  // Rule B: stale Rev cell. 13 must not match 1 by prefix.
  reset();
  write("np_foo_001.md", doc("NP-FOO-001", "13"));
  write("np_dhf_001.md", DHF_HEAD + dhfRow("NP-DHF-001", "1", "np_dhf_001.md") + dhfRow("NP-FOO-001", "1", "np_foo_001.md") + HIST("1"));
  expect("rule B rejects a stale Rev cell", 1, "NP-FOO-001: DHF Rev 1, file Rev 13");

  // Rule C: the history row for the current revision was put in the index (#451).
  reset();
  write("np_dhf_001.md", DHF_HEAD.replace("**Revision:** 1", "**Revision:** 2")
    + dhfRow("NP-DHF-001", "2", "np_dhf_001.md") + "| 2 | 2026-01-02 | A | misplaced |\n" + HIST("1"));
  expect("rule C rejects a history table with no row for the current Rev", 1, "no row for Rev 2");

  // Rule C: no history table at all.
  reset();
  write("np_dhf_001.md", DHF_HEAD + dhfRow("NP-DHF-001", "1", "np_dhf_001.md"));
  expect("rule C rejects a DHF with no document-history table", 1, "no document-history table");

  // Rules D–G read §5 only. A §5 fixture with one clean row, plus a row under test.
  const S5 = (extra: string) => "# T\n\n" + DHF_HEAD.replace("| ID |", "## 5. Master Document Index\n\n| ID |")
    + dhfRow("NP-DHF-001", "1", "np_dhf_001.md") + extra + "\n## 6. Other\n" + HIST("1");
  const row5 = (id: string, title: string, status: string) =>
    `| ${id} | ${title} | 1 | 2026-01-01 | [x](./none.md) | ${status} | QMS |\n`;

  reset();
  write("np_dhf_001.md", S5(row5("NP-BAR-001", "Bar Spec (base module)", "**SUPERSEDED 2026-09-25 by NP-BAR-002**")));
  expect("a clean title with a parenthetical and a supersession pointer is accepted", 0, "D (no revision notes):");

  reset();
  write("np_dhf_001.md", S5(row5("NP-BAR-001", "Bar Spec. **Rev 2 (2026-09-22):** closed OI-BAR-01", "DRAFT")));
  expect("rule D rejects a revision note in Title", 1, "NP-BAR-001 D:Title");

  reset();
  write("np_dhf_001.md", S5(row5("NP-BAR-001", "Bar Spec", "ACTIVE — Rev 2: field replaced")));
  expect("rule D rejects a revision note in Status", 1, "NP-BAR-001 D:Status");

  reset();
  write("np_dhf_001.md", S5(row5("NP-BAR-001", "Bar Spec — " + "x".repeat(200), "DRAFT")));
  expect("rule E rejects an over-long Title", 1, "NP-BAR-001 E:Title");

  reset();
  write("np_dhf_001.md", S5(row5("NP-BAR-001", "Bar Spec", "DRAFT — " + "y".repeat(100))));
  expect("rule E rejects an over-long Status", 1, "NP-BAR-001 E:Status");

  reset();
  write("np_dhf_001.md", S5(row5("NP-BAR-001", "Bar Spec", "DRAFT") + row5("**NP-BAR-001**", "Bar Spec", "DRAFT")));
  expect("rule F rejects a serial with two live rows", 1, "NP-BAR-001 F");

  reset();
  write("np_dhf_001.md", S5(row5("NP-BAR-001", "Bar Spec", "DRAFT") + row5("~~NP-BAR-001~~", "Bar Spec", "DRAFT")
    + row5("NP-BAR-001 (variant)", "Bar Spec variant", "DRAFT")));
  expect("rule F accepts a struck-through or disambiguated second row", 0, "F (one row per serial):");

  // Rows outside §5 are not index rows for D–G.
  reset();
  write("np_dhf_001.md", S5("") .replace("\n## 6. Other\n", "\n## 6. Other\n\n" + "| a | b | c | d | e | f | g |\n|---|---|---|---|---|---|---|\n"
    + row5("NP-BAR-001", "Bar. Rev 2 (2026-01-01): note", "DRAFT") + row5("NP-BAR-001", "Bar", "DRAFT")));
  expect("rules D–G ignore rows outside §5", 0, "D (no revision notes):");

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
  console.log("  rules A–G each proven to reject; conforming tree proven to pass");
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

// Rule C: the document-history table carries the DHF's current revision.
const violC: string[] = [];
{
  const L = readFileSync(DHF, "utf8").split("\n");
  const h = L.findIndex((l) => /^##\s.*History\s*$/.test(l));
  let t = h < 0 ? -1 : L.findIndex((l, i) => i > h && l.trimStart().startsWith("|"));
  if (t < 0) violC.push(`C: ${DHF} has no document-history table (a table under a "## … History" heading)`);
  else {
    const revs: string[] = [];
    for (let i = t + 2; i < L.length && L[i].trimStart().startsWith("|"); i++) {
      revs.push(L[i].split("|")[1]?.replace(/[*\s]/g, "") ?? "");
    }
    if (!revs.includes(self.rev)) {
      violC.push(`C: ${DHF} is Rev ${self.rev}, and its document-history table (line ${t + 1}) has no row for Rev ${self.rev}`);
    }
  }
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


// Rules D–G: what the §5 master index's Title and Status cells hold (NP-CONV-001 §4.4).
const TITLE_MAX = 150;
const STATUS_MAX = 80;
// "Rev N (2026-…", "Rev N:", "Rev N →", with or without bold. A bare "(Rev 11)" or
// "at Rev 2" is not matched; E catches a history written in prose.
const REV_NOTE = /\bRev\.?\s+[0-9A-Z][0-9A-Z.]*\**\s*(?:\(\s*\d{4}-\d{2}-\d{2}|:|→)/;
const violD: string[] = [], violE: string[] = [], violF: string[] = [], violG: string[] = [];
const plain = (x: string) => x.replace(/\*\*/g, "").replace(/\s+/g, " ").trim();
const flag = (bucket: string[], key: string, msg: string) => bucket.push(`${key} — ${msg}`);
{
  const L = readFileSync(DHF, "utf8").split("\n");
  const s5 = L.findIndex((l) => /^##\s+5\.\s/.test(l));
  const e5 = s5 < 0 ? -1 : L.findIndex((l, i) => i > s5 && /^##\s/.test(l));
  const seen = new Map<string, number>();
  for (let i = s5 + 1; s5 >= 0 && i < (e5 < 0 ? L.length : e5); i++) {
    const m = L[i].match(/^\|\s*\*{0,2}(NP-[A-Z0-9-]+?)\*{0,2}\s*\|/);
    if (!m) continue;
    const id = m[1], at = `line ${i + 1}`;
    const c = L[i].split(/(?<!\\)\|/).slice(1, -1).map((x) => x.trim());
    const title = c[1] ?? "", status = c[5] ?? "";
    if (REV_NOTE.test(title)) flag(violD, `${id} D:Title`, `${at}: Title carries a revision note: "${title.match(REV_NOTE)![0]}"`);
    if (REV_NOTE.test(status)) flag(violD, `${id} D:Status`, `${at}: Status carries a revision note: "${status.match(REV_NOTE)![0]}"`);
    if (title.length > TITLE_MAX) flag(violE, `${id} E:Title`, `${at}: Title is ${title.length} characters (max ${TITLE_MAX})`);
    if (status.length > STATUS_MAX) flag(violE, `${id} E:Status`, `${at}: Status is ${status.length} characters (max ${STATUS_MAX})`);
    const md = (c[4] ?? "").match(/^\[[^\]]*\]\(\.\/([a-z0-9_]+\.md)\)$/)?.[1];
    if (md && existsSync(join("docs", md))) {
      const h1 = readFileSync(join("docs", md), "utf8").split("\n").find((l) => l.startsWith("# "));
      const want = plain((h1 ?? "").slice(2));
      if (plain(title) !== want) flag(violG, `${id} G:Title`, `${at}: Title is not ${md}'s heading "${want}"`);
    }
    if (seen.has(id)) flag(violF, `${id} F`, `${at}: second live row for this serial (first at line ${seen.get(id)! + 1})`);
    else seen.set(id, i);
  }
}

console.log(`scanned: ${scanned} controlled Markdown documents against ${DHF} Rev ${self.rev}`);
console.log(`A (indexed):               ${violA.length ? "FAIL" : "PASS"}`);
violA.forEach((v) => console.log("   " + v));
console.log(`B (Rev agrees with file):  ${violB.length ? "FAIL" : "PASS"}`);
violB.forEach((v) => console.log("   " + v));
console.log(`C (DHF history has its Rev): ${violC.length ? "FAIL" : "PASS"}`);
violC.forEach((v) => console.log("   " + v));
console.log(`D (no revision notes):     ${violD.length ? "FAIL" : "PASS"}`);
violD.forEach((v) => console.log("   " + v));
console.log(`E (cell length):           ${violE.length ? "FAIL" : "PASS"}`);
violE.forEach((v) => console.log("   " + v));
console.log(`F (one row per serial):    ${violF.length ? "FAIL" : "PASS"}`);
violF.forEach((v) => console.log("   " + v));
console.log(`G (Title is the file's heading): ${violG.length ? "FAIL" : "PASS"}`);
violG.forEach((v) => console.log("   " + v));
if (violD.length + violE.length + violF.length + violG.length) {
  console.log("\nFix (D–G): the Title is the document's own heading and the Status is its status word plus at most");
  console.log("one short pointer (NP-CONV-001 §4.4). Change notes go in the document's history and NP-DHF-001 §9.");
}
if (violA.length + violB.length + violC.length) {
  console.log("\nFix: set the row's Rev and Date from the file's front matter, or add the row.");
  console.log("editscripts/patch_conv07_dhf_reconcile.py does both mechanically.");
}
const fails = violA.length + violB.length + violC.length + violD.length + violE.length + violF.length + violG.length;
process.exit(fails ? 1 : 0);
