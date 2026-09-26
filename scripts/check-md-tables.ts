#!/usr/bin/env bun
/**
 * Verify that every Markdown table row in docs/ has as many cells as its header.
 *
 * Why this exists. A controlled document's tables are its records: the DHF
 * index, the open-item lists, the FMEA rows. A row with the wrong number of
 * cells still reads as plausible in the raw file, but GitHub renders it
 * wrongly: a short row leaves columns blank, and cells beyond the header are
 * DROPPED from the rendered page. Nothing reported either.
 *
 * Found by PR #451 (2026-09-26). A scripted edit located "the revision-history
 * row for Rev 101" by the text `| 101 | 2026-09-25 |`, which NP-DHF-001's own
 * master-index row also contains. The Rev 102 history row (4 cells) went into
 * the master index (7 columns), was merged, and every existing check passed:
 * check-dhf-index.ts reads only rows whose first cell is a serial. Sweeping
 * docs/ with this rule then found ten more malformed rows already on main:
 * owner cells in 3-column open-item tables, two rows fused on one line, a
 * revision note written past the last column, and a stale copy of a DHF row's
 * tail fused onto the current row. All repaired in the same change.
 *
 * The rule (GFM tables): a header line starting with `|`, followed by a
 * delimiter line (`|---|:--:|`); every following line starting with `|` is a
 * row, and must split into the header's cell count. Cells split on `|` outside
 * code spans and not escaped as `\|`, which is how GFM splits them. Fenced code
 * blocks are skipped.
 *
 * Out of scope, deliberately:
 *   - docs/status/document-register.md. Frozen history (NP-CONV-001 OI-CONV-07,
 *     closed 2026-09-25): it is retained as written, not maintained. Its four
 *     malformed rows are left as they are and named in EXEMPT so they stay
 *     visible. A new malformed row in any other file fails.
 *   - Markdown outside docs/. None of it has a malformed table today. Scanning
 *     it would put this gate's population outside the docnaming job's
 *     relevance list (check-ci-scope.ts).
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-md-tables.ts --self-test
 * CI-Scans: every table in docs/**.md
 * CI-Scan-Paths: docs/**
 */
import { readdirSync, readFileSync, statSync, mkdirSync, mkdtempSync, writeFileSync, rmSync } from "fs";
import { join, relative } from "path";
import { tmpdir } from "os";

const EXEMPT = new Set(["docs/status/document-register.md"]);

/** GFM cell count: split on `|` outside code spans and not escaped as `\|`. */
export function cellCount(line: string): number {
  const s = line.trim();
  const cells: string[] = [];
  let cur = "", fence = 0;
  for (let i = 0; i < s.length; i++) {
    const c = s[i];
    if (c === "\\" && s[i + 1] === "|") { cur += "\\|"; i++; continue; }
    if (c === "`") {
      let k = 0;
      while (s[i + k] === "`") k++;
      if (!fence) fence = k; else if (fence === k) fence = 0;
      cur += s.slice(i, i + k); i += k - 1; continue;
    }
    if (c === "|" && !fence) { cells.push(cur); cur = ""; continue; }
    cur += c;
  }
  cells.push(cur);
  if (cells.length && cells[0].trim() === "") cells.shift();
  if (cells.length && cells[cells.length - 1].trim() === "") cells.pop();
  return cells.length;
}

const DELIM = /^\s*\|?\s*:?-{3,}:?\s*(\|\s*:?-{3,}:?\s*)*\|?\s*$/;
const isRow = (l: string) => l.trimStart().startsWith("|");

function scan(text: string): string[] {
  const L = text.split("\n");
  const out: string[] = [];
  let inFence = false;
  for (let i = 0; i < L.length; i++) {
    if (/^\s*(```|~~~)/.test(L[i])) { inFence = !inFence; continue; }
    if (inFence || i === 0 || !DELIM.test(L[i]) || !isRow(L[i - 1])) continue;
    const want = cellCount(L[i - 1]);
    if (cellCount(L[i]) !== want) out.push(`${i + 1}: delimiter has ${cellCount(L[i])} cells, header ${want}`);
    for (let j = i + 1; j < L.length && isRow(L[j]); j++) {
      const got = cellCount(L[j]);
      if (got !== want) out.push(`${j + 1}: ${got} cells, header has ${want} — ${L[j].slice(0, 70)}…`);
      i = j;
    }
  }
  return out;
}

function walk(dir: string, acc: string[] = []): string[] {
  for (const e of readdirSync(dir).sort()) {
    const p = join(dir, e);
    if (statSync(p).isDirectory()) walk(p, acc);
    else if (e.endsWith(".md")) acc.push(p);
  }
  return acc;
}

// ── Self-test ────────────────────────────────────────────────────────────────
// Drives the real entry point against fixture trees, in both directions, before
// the checker is trusted (NP-CONV-001 §8).
if (process.argv.includes("--self-test")) {
  const root = mkdtempSync(join(tmpdir(), "np-mdtables-"));
  const docs = join(root, "docs");
  const failures: string[] = [];
  const run = (files: Record<string, string>) => {
    rmSync(docs, { recursive: true, force: true });
    for (const [rel, body] of Object.entries(files)) {
      mkdirSync(join(docs, rel, ".."), { recursive: true });
      writeFileSync(join(docs, rel), body);
    }
    const r = Bun.spawnSync([process.execPath, import.meta.path], { cwd: root, stdout: "pipe", stderr: "pipe" });
    return { code: r.exitCode, out: new TextDecoder().decode(r.stdout) + new TextDecoder().decode(r.stderr) };
  };
  const expect = (label: string, want: 0 | 1, files: Record<string, string>, needle?: string) => {
    const { code, out } = run(files);
    if (code !== want) failures.push(`${label} — expected exit ${want}, got ${code}\n${out}`);
    else if (needle && !out.includes(needle)) failures.push(`${label} — output lacked ${JSON.stringify(needle)}\n${out}`);
  };
  const T = "| ID | T | Rev | Date | File | S | C |\n|---|---|---|---|---|---|---|\n";
  const ok = "| NP-A-001 | t | 1 | 2026-01-01 | [a](./a.md) | ACTIVE | QMS |\n";

  expect("conforming table accepted", 0, { "a.md": T + ok + "\ntext | with a pipe\n" }, "PASS");
  // The #451 defect: a 4-cell history row inside the 7-column index.
  expect("short row rejected (the #451 row)", 1,
    { "a.md": T + ok + "| 102 | 2026-09-26 | Author | description |\n" }, "a.md:4: 4 cells, header has 7");
  expect("long row rejected (text past the last column)", 1,
    { "a.md": T + "| NP-A-001 | t | 1 | 2026-01-01 | [a](./a.md) | ACTIVE | QMS note | QMS |\n" }, "8 cells");
  expect("a pipe inside a code span is not a cell", 0,
    { "a.md": "| A | B |\n|---|---|\n| `a | b` | c |\n" });
  expect("an escaped pipe is not a cell", 0, { "a.md": "| A | B |\n|---|---|\n| a \\| b | c |\n" });
  expect("fenced tables are not scanned", 0, { "a.md": "```\n| A | B |\n|---|---|\n| 1 |\n```\n" });
  expect("nested directories are scanned", 1,
    { "sub/b.md": "| A | B |\n|---|---|\n| 1 | 2 | 3 |\n" }, "docs/sub/b.md:3");
  expect("the frozen register is exempt", 0,
    { "status/document-register.md": "| A | B |\n|---|---|\n| 1 | 2 | 3 |\n" });
  expect("an empty scope is not a pass", 1, {});

  rmSync(root, { recursive: true, force: true });
  console.log("check-md-tables self-test");
  if (failures.length) {
    console.error(`\nSELF-TEST FAIL — ${failures.length} assertion(s):`);
    for (const f of failures) console.error("  " + f);
    process.exit(1);
  }
  console.log("  short and long rows proven to fail; code spans, escapes, fences and the exemption proven not to");
  console.log("SELF-TEST PASS — the checker has teeth.");
  process.exit(0);
}

let files: string[] = [];
try { files = walk("docs"); } catch { /* reported below */ }
if (files.length === 0) {
  console.error("no Markdown files under docs/ — refusing to report an empty scan as a pass");
  process.exit(1);
}
const viol: string[] = [];
let tables = 0;
for (const f of files) {
  const rel = relative(".", f);
  if (EXEMPT.has(rel)) continue;
  const text = readFileSync(f, "utf8");
  tables += text.split("\n").filter((l) => DELIM.test(l)).length;
  for (const v of scan(text)) viol.push(`${rel}:${v}`);
}
console.log(`scanned: ${files.length} Markdown files, ${tables} tables under docs/ (${EXEMPT.size} frozen file exempt)`);
console.log(`Every table row has its header's cell count:  ${viol.length ? "FAIL" : "PASS"}`);
viol.forEach((v) => console.log("   " + v));
if (viol.length) {
  console.log("\nA short row renders with blank columns; cells past the header are dropped from the rendered page.");
  console.log("Usually: a row inserted into the wrong table, two rows on one line, or text written past the last `|`.");
}
process.exit(viol.length ? 1 : 0);
