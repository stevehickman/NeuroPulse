#!/usr/bin/env bun
/**
 * req-trace.ts — requirements ↔ tests traceability report.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/req-trace.ts --self-test
 * CI-Scans: requirement definitions in docs/*.md against every test file's @verifies tags, and the untraced-test baseline
 * CI-Scan-Paths: docs/** firmware/** app/** common/** ci/**
 *
 * Answers three questions, from the tracked tree alone:
 *   1. How many tests are associated with each requirement?
 *   2. Which requirements (or declared parts of a requirement) have no test?
 *   3. Which tests are associated with no requirement?
 *
 * ── Where requirements come from ────────────────────────────────────────────
 * docs/*.md (not docs/superseded/). A requirement is DEFINED where its ID leads
 *   · a table row            | **`REQ-FWTILE-03`** | text | ... |
 *   · a blockquote           > **`REQ-NET-01`** — text ...
 * IDs: REQ-<AREA>-<NN> and <PREFIX>-REQ-<ID> (e.g. HUB-REQ-C06). A second table
 * row for an ID in the same file is kept as its documented *verification* note.
 * A requirement struck through (~~) or whose text opens RETIRED is retired and
 * is neither expected to have tests nor counted as a gap.
 *
 * Parts. A requirement's text that enumerates (a) (b) (c) … or (1) (2) … is
 * split into those parts automatically. `docs/status/requirement-parts.json`
 * overrides or adds parts by hand (a requirement written as one long sentence
 * with several limits needs this) and also records requirements verified by
 * non-test means (inspection, analysis, demonstration). See that file's header.
 *
 * ── How a test names the requirement it verifies ───────────────────────────
 * Put a tag in the comment block directly above the test (any language):
 *
 *     // @verifies REQ-UPG-01 REQ-UPG-02#b
 *
 * `REQ-X-NN` verifies the requirement as a whole; `REQ-X-NN#b` verifies part b
 * (several parts: `REQ-X-NN#a,b`). Tags attach to the next test unit (see
 * TEST UNITS) found within 12 lines. Two more tags:
 *
 *     @verifies-file REQ-...      every test in this file (file header)
 *     @verifies-none <reason>     this test (or, as @verifies-none-file, every
 *                                 test in the file) traces to no requirement,
 *                                 and says why. A reason is mandatory.
 *
 * A requirement whose parts are declared but which is only tagged as a whole is
 * reported UNSCOPED: a whole-requirement tag cannot show every part is tested.
 *
 * TEST UNITS: C `static void test_*(…)` · Kotlin `@Test fun` · Swift
 * `func test*` / `@Test func` · TypeScript `it(` / `test(` · Rust `#[test] fn` ·
 * C# `[Fact]`/`[Theory]`/`[Test]`/`[TestMethod]` · Python `def test_*`.
 * PARAMETERIZED tests (it.each, @ParameterizedTest, @Test(arguments:), #[rstest] / #[test_case], [Theory],
 * pytest.mark.parametrize) are one unit each, however many cases they run: a tag above one covers every
 * case, and the report marks it "parameterized" because its test count is a floor.
 * Vendored code (vendor/, node_modules/) is not scanned.
 *
 * ── Usage ───────────────────────────────────────────────────────────────────
 *   bun scripts/req-trace.ts                      Markdown report to stdout
 *   bun scripts/req-trace.ts --out report.md      write it to a file
 *   bun scripts/req-trace.ts --json [--out f]     machine-readable
 *   bun scripts/req-trace.ts --check              exit 1 on a bad tag or on a
 *                                                 file gaining untraced tests
 *   bun scripts/req-trace.ts --update-baseline    rewrite ci/req-trace-baseline.txt
 *   bun scripts/req-trace.ts --self-test
 *
 * `--check` never fails on a requirement having no tests: that is the report's
 * finding, not a defect in the tree. It fails on things a author can fix in one
 * change: a tag naming an unknown, retired or part-less requirement; a
 * `@verifies-none` with no reason; and a test file whose untraced-test count is
 * above ci/req-trace-baseline.txt (the ratchet: new tests must be traced).
 */
import { execFileSync } from "child_process";
import { existsSync, readFileSync, readdirSync, writeFileSync } from "fs";
import { dirname, join, resolve } from "path";

const ROOT = resolve(dirname(new URL(import.meta.url).pathname), "..");
const BASELINE = "ci/req-trace-baseline.txt";
const PARTS_FILE = "docs/status/requirement-parts.json";

// ── Types ───────────────────────────────────────────────────────────────────
export interface Requirement {
  id: string;
  file: string;
  line: number;
  text: string;
  retired: boolean;
  parts: Record<string, string>; // part id → description
  partsSource: "enumerated" | "declared" | "none";
  verification: string[]; // documented verification rows
  nonTest?: { method: string; reason: string };
}
export interface TestUnit {
  file: string;
  line: number;
  name: string;
  verifies: Ref[];
  none?: string; // @verifies-none reason
  fileLevel: boolean; // association came (at least partly) from a file-level tag
  parameterized?: boolean; // one unit, many cases: the test count is a floor
}
export interface Ref {
  id: string;
  parts: string[]; // empty = whole requirement
}
export interface TagProblem {
  file: string;
  line: number;
  message: string;
}

const ID = String.raw`(?:[A-Z][A-Z0-9]*-)?REQ-[A-Z0-9]+-\d+[a-z]?|[A-Z][A-Z0-9]*-REQ-[A-Z]\d+`;
const ID_RE = new RegExp(`^(${ID})$`);
const ID_G = new RegExp(`\\b(?:${ID})\\b`, "g");

// ── Requirement extraction ──────────────────────────────────────────────────
const stripCell = (s: string) => s.replace(/[*`~]/g, "").replace(/\s+/g, " ").trim();

function splitRow(line: string): string[] {
  // Split on unescaped pipes outside backticks.
  const cells: string[] = [];
  let cur = "", tick = false;
  for (let i = 1; i < line.length; i++) {
    const c = line[i];
    if (c === "`") tick = !tick;
    if (c === "|" && !tick && line[i - 1] !== "\\") { cells.push(cur); cur = ""; } else cur += c;
  }
  if (cur.trim()) cells.push(cur);
  return cells;
}

export function enumerateParts(text: string): Record<string, string> {
  const out: Record<string, string> = {};
  for (const [kind, re] of [["a", /\(([a-z])\)/g], ["1", /\((\d{1,2})\)/g]] as const) {
    const marks: { id: string; at: number }[] = [];
    for (const m of text.matchAll(re)) marks.push({ id: m[1], at: m.index! });
    // must run unbroken from the first letter/number, at least two long
    const seq: typeof marks = [];
    for (const m of marks) {
      const want = kind === "a" ? String.fromCharCode(97 + seq.length) : String(seq.length + 1);
      if (m.id === want) seq.push(m);
    }
    if (seq.length >= 2) {
      seq.forEach((m, i) => {
        const end = i + 1 < seq.length ? seq[i + 1].at : text.length;
        out[m.id] = text.slice(m.at + 3 + (kind === "1" && m.id.length > 1 ? 1 : 0), end).trim().replace(/[;,.]\s*$/, "").slice(0, 160);
      });
      return out;
    }
  }
  return out;
}

export function extractRequirements(file: string, src: string): Requirement[] {
  const lines = src.split("\n");
  const reqs = new Map<string, Requirement>();
  const add = (id: string, line: number, text: string, retiredMark: boolean) => {
    const prior = reqs.get(id);
    if (prior) { prior.verification.push(text.slice(0, 300)); return; }
    reqs.set(id, {
      id, file, line, text, retired: retiredMark || /^\W*(retired|superseded)\b/i.test(text),
      parts: {}, partsSource: "none", verification: [],
    });
  };
  for (let i = 0; i < lines.length; i++) {
    const l = lines[i];
    if (l.startsWith("|")) {
      const cells = splitRow(l);
      const first = stripCell(cells[0] ?? "");
      const m = first.match(new RegExp(`^(${ID})(?:\\s*\\([^)]*\\))?$`));
      if (m) add(m[1], i + 1, stripCell(cells[1] ?? ""), (cells[0] ?? "").includes("~~"));
    } else {
      // Blockquote  > **`REQ-X-01` — text**   or   > **`REQ-X-01`** — text
      // Paragraph   **REQ-X-01:** text        or   **Requirement HUB-REQ-C01:** text
      const q = l.match(new RegExp(String.raw`^>\s*\*\*\x60?(${ID})\x60?(?:\s*\([a-z ]+\))?:?\*{0,2}\s*[—–:-]?\s*(.*)`));
      const p = q ? null : l.match(new RegExp(String.raw`^\*\*(?:Requirement\s+)?\x60?(${ID})\x60?(?:\s*\([a-z ]+\))?:?\*\*:?\s*(.*)`));
      const m = q ?? p;
      if (m) {
        const quoted = !!q;
        let text = m[2];
        for (let j = i + 1; j < lines.length; j++) {
          const nx = quoted ? (lines[j].startsWith(">") ? lines[j].replace(/^>\s*/, "") : "") : lines[j];
          if (nx.trim() === "" || (!quoted && text.trim() && /^(\||#|\*\*)/.test(nx))) break;
          text += " " + nx;
        }
        add(m[1], i + 1, stripCell(text), false);
        // A "Clause" table straight after the definition lists its parts: | (a) | text | ...
        let k = i + 1;
        while (k < lines.length && lines[k].trim() === "") k++;
        if (/^\|\s*Clause\s*\|/i.test(lines[k] ?? "")) {
          const parts: Record<string, string> = {};
          for (k += 2; k < lines.length && lines[k].startsWith("|"); k++) {
            const c = splitRow(lines[k]);
            const key = stripCell(c[0] ?? "").replace(/^\(|\)$/g, "");
            if (key) parts[key] = stripCell(c[1] ?? "").slice(0, 160);
          }
          const r = reqs.get(m[1]);
          if (r && !r.text.trim() && Object.keys(parts).length) { r.parts = parts; r.text = Object.values(parts).join("; "); }
        }
      }
    }
  }
  for (const r of reqs.values()) {
    if (!Object.keys(r.parts).length) r.parts = enumerateParts(r.text);
    if (Object.keys(r.parts).length) r.partsSource = "enumerated";
  }
  return [...reqs.values()];
}

// ── Test discovery ──────────────────────────────────────────────────────────
const EXT = /\.(c|kt|swift|ts|rs|cs|py)$/;
const SKIP = /(^|\/)(vendor|node_modules|build|editscripts|docs|\.git)(\/|$)/;
const TEST_PATH = /(^|\/)(tests?|__tests__|[A-Za-z]*Tests?|jvmTest|commonTest|desktopTest|androidTest)(\/|$)|\.test\.ts$|_tests?\.c$|Tests?\.(kt|swift|cs)$|(^|\/)test_[^/]*\.py$/;

export function isTestFile(path: string): boolean {
  return EXT.test(path) && !SKIP.test(path) && TEST_PATH.test(path);
}

interface Unit { line: number; name: string; parameterized?: boolean }

/**
 * Parameterized tests are ONE unit however many cases they run: the tag above them covers every case, and
 * the report marks the unit "parameterized" so a reader knows its test count is a floor. Recognised:
 *   TypeScript  it.each([...])('title') / it.each`table`('title') / test.each, title on a later line
 *   Kotlin      @ParameterizedTest, @TestFactory, @RepeatedTest
 *   Swift       @Test(arguments: ...)
 *   Rust        #[rstest], #[test_case(...)] (several attributes on one fn are one unit)
 *   C#          [Theory] (the [InlineData] lines between it and the method are skipped)
 *   Python      def test_* under @pytest.mark.parametrize (the def is the unit)
 */
export function findUnits(path: string, lines: string[]): Unit[] {
  const out: Unit[] = [];
  const ext = path.slice(path.lastIndexOf(".") + 1);
  const fnUnit = new Map<number, Unit>();
  const pendingAttr = (re: RegExp, nameRe: RegExp, param: RegExp) => {
    for (let i = 0; i < lines.length; i++) {
      if (!re.test(lines[i])) continue;
      for (let j = i; j < Math.min(i + 12, lines.length); j++) {
        if (j > i && /^\s*(\[|#\[|@)/.test(lines[j]) && !nameRe.test(lines[j])) continue; // more attributes
        const m = lines[j].match(nameRe);
        if (!m) continue;
        const prior = fnUnit.get(j);
        if (prior) { prior.parameterized = true; break; } // several #[test_case] on one fn
        // the unit starts at the first attribute of the run, so a tag above the run attaches to it
        let start = i;
        while (start > 0 && /^\s*(\[|#\[|@)/.test(lines[start - 1]) && !/@verifies/.test(lines[start - 1])) start--;
        const attrs = lines.slice(start, j).join("\n");
        const u: Unit = { line: start + 1, name: m[1], parameterized: param.test(attrs) || undefined };
        fnUnit.set(j, u);
        out.push(u);
        break;
      }
    }
  };
  lines.forEach((l, i) => {
    let m: RegExpMatchArray | null = null;
    if (ext === "c" && (m = l.match(/^(?:static\s+)?(?:void|int)\s+(test_\w+)\s*\(/))) out.push({ line: i + 1, name: m[1] });
    else if (ext === "swift" && (m = l.match(/^\s*(?:@Test\b.*?)?func\s+(test\w*|`[^`]+`|\w+)\s*\(/)) && (/func\s+test/.test(l) || /@Test\b/.test(lines[i - 1] ?? "") || /@Test\b/.test(l)))
      out.push({ line: i + 1, name: m[1], parameterized: /@Test\([^)]*arguments:/.test(l + (lines[i - 1] ?? "")) || undefined });
    else if (ext === "ts" && (m = l.match(/^\s*(?:it|test)\.each\b/))) {
      // title: the first `)('title'` / `` `('title' `` within the next 60 lines (the table may be long)
      let name = "";
      for (let j = i; j < Math.min(i + 60, lines.length) && !name; j++) {
        const t = (j === i ? l.slice(m[0].length) : lines[j]).match(/[)`]\s*\(\s*(['"`])(.+?)\1/);
        if (t) name = t[2];
      }
      out.push({ line: i + 1, name: name || "(parameterized)", parameterized: true });
    }
    else if (ext === "ts" && (m = l.match(/^\s*(?:it|test)(?:\.\w+)?\(\s*(['"`])(.+?)\1/))) out.push({ line: i + 1, name: m[2] });
    else if (ext === "py" && (m = l.match(/^\s*(?:async\s+)?def\s+(test_\w+)\s*\(/))) {
      let k = i - 1;
      while (k >= 0 && /^\s*@/.test(lines[k])) k--;
      const param = lines.slice(k + 1, i).some((x) => /pytest\.mark\.parametrize/.test(x));
      out.push({ line: i + 1, name: m[1], parameterized: param || undefined });
    }
  });
  if (ext === "kt") pendingAttr(/^\s*@(?:Test|ParameterizedTest|TestFactory|RepeatedTest)\b/, /\bfun\s+(`[^`]+`|\w+)/, /@(?:ParameterizedTest|TestFactory|RepeatedTest)/);
  if (ext === "rs") pendingAttr(/^\s*#\[(?:(?:\w+::)*test|rstest|test_case)\b/, /\bfn\s+(\w+)/, /#\[(?:rstest|test_case)/);
  if (ext === "cs") pendingAttr(/^\s*\[(?:Fact|Theory|Test|TestMethod|TestCase)\b/, /^\s*(?:public|internal|private|protected)?\s*(?:static\s+)?(?:async\s+)?[\w<>\[\],?]+\s+(\w+)\s*\(/, /\[(?:Theory|TestCase)\b/);
  return out.sort((a, b) => a.line - b.line);
}

// ── Tags ────────────────────────────────────────────────────────────────────
interface Tag { line: number; kind: "verifies" | "verifies-file" | "none" | "none-file"; refs: Ref[]; reason?: string }

export function parseRefs(s: string): { refs: Ref[]; bad: string[] } {
  const refs: Ref[] = [], bad: string[] = [];
  for (const tok of s.split(/[\s,]+(?=[A-Z])|\s+/).filter(Boolean)) {
    const m = tok.replace(/[.;)\]]+$/, "").match(/^([A-Z0-9-]+?\d[a-z]?|[A-Z0-9-]+-REQ-[A-Z]\d+)(?:#([A-Za-z0-9,]+))?$/);
    if (m && ID_RE.test(m[1])) refs.push({ id: m[1], parts: m[2] ? m[2].split(",").filter(Boolean) : [] });
    else bad.push(tok);
  }
  return { refs, bad };
}

function readTags(lines: string[], problems: TagProblem[], file: string): Tag[] {
  const tags: Tag[] = [];
  const re = /@verifies(-none)?(-file)?\b(.*)$/;
  lines.forEach((l, i) => {
    const m = l.match(re);
    if (!m) return;
    const rest = m[3].replace(/\*\/\s*$/, "").replace(/^[\s:]+/, "").trim();
    if (m[1]) {
      if (!rest) problems.push({ file, line: i + 1, message: "@verifies-none needs a reason" });
      tags.push({ line: i + 1, kind: m[2] ? "none-file" : "none", refs: [], reason: rest });
      return;
    }
    // stop at the first token that is not an ID, so prose after the list is allowed
    const ids: string[] = [];
    for (const tok of rest.split(/[\s,]+/).filter(Boolean)) {
      if (/^[A-Z][A-Z0-9-]*\d[a-z]?(#[A-Za-z0-9,]+)?[.;)]?$/.test(tok) || /-REQ-/.test(tok)) ids.push(tok); else break;
    }
    const { refs, bad } = parseRefs(ids.join(" "));
    if (!refs.length) problems.push({ file, line: i + 1, message: `@verifies${m[2] ?? ""} names no requirement ID` });
    for (const b of bad) problems.push({ file, line: i + 1, message: `malformed requirement reference "${b}"` });
    tags.push({ line: i + 1, kind: m[2] ? "verifies-file" : "verifies", refs });
  });
  return tags;
}

export function scanTestFile(file: string, src: string, problems: TagProblem[]): TestUnit[] {
  const lines = src.split("\n");
  const units = findUnits(file, lines);
  const tags = readTags(lines, problems, file);
  const fileRefs = tags.filter((t) => t.kind === "verifies-file").flatMap((t) => t.refs);
  const fileNone = tags.find((t) => t.kind === "none-file")?.reason;
  const out: TestUnit[] = units.map((u) => ({ file, line: u.line, name: u.name, verifies: [...fileRefs], fileLevel: fileRefs.length > 0, none: fileNone, parameterized: u.parameterized }));
  for (const t of tags) {
    if (t.kind !== "verifies" && t.kind !== "none") continue;
    // attach to the first unit starting after the tag, within 12 lines
    const idx = out.findIndex((u) => u.line >= t.line && u.line - t.line <= 12);
    if (idx < 0) { problems.push({ file, line: t.line, message: `${t.kind === "none" ? "@verifies-none" : "@verifies"} is not within 12 lines above a test` }); continue; }
    if (t.kind === "none") out[idx].none = t.reason; else out[idx].verifies.push(...t.refs);
  }
  return out;
}

// ── Analysis ────────────────────────────────────────────────────────────────
export type Status = "covered" | "partial" | "unscoped" | "untested" | "non-test" | "retired";
export interface Row {
  req: Requirement;
  tests: TestUnit[]; // associated, whole or part
  partTests: Record<string, TestUnit[]>;
  wholeOnly: TestUnit[];
  untestedParts: string[];
  status: Status;
}

export function analyse(reqs: Requirement[], tests: TestUnit[], problems: TagProblem[]) {
  const byId = new Map(reqs.map((r) => [r.id, r]));
  const rows = new Map<string, Row>();
  for (const r of reqs) rows.set(r.id, { req: r, tests: [], partTests: Object.fromEntries(Object.keys(r.parts).map((p) => [p, []])), wholeOnly: [], untestedParts: [], status: "untested" });
  const unknown = new Map<string, TestUnit[]>();
  for (const t of tests) for (const ref of t.verifies) {
    const row = rows.get(ref.id);
    if (!row) { (unknown.get(ref.id) ?? unknown.set(ref.id, []).get(ref.id)!).push(t); problems.push({ file: t.file, line: t.line, message: `${ref.id} is not a defined requirement` }); continue; }
    if (row.req.retired) problems.push({ file: t.file, line: t.line, message: `${ref.id} is retired` });
    if (!row.tests.includes(t)) row.tests.push(t);
    if (!ref.parts.length) { if (!row.wholeOnly.includes(t)) row.wholeOnly.push(t); continue; }
    for (const p of ref.parts) {
      if (!(p in row.partTests)) { problems.push({ file: t.file, line: t.line, message: `${ref.id} declares no part "${p}"` }); continue; }
      row.partTests[p].push(t);
    }
  }
  for (const row of rows.values()) {
    const parts = Object.keys(row.req.parts);
    row.untestedParts = parts.filter((p) => !row.partTests[p].length);
    if (row.req.retired) row.status = "retired";
    else if (!row.tests.length) row.status = row.req.nonTest ? "non-test" : "untested";
    else if (!parts.length) row.status = "covered";
    else if (row.untestedParts.length === 0) row.status = "covered";
    else if (row.untestedParts.length < parts.length) row.status = "partial";
    else row.status = row.wholeOnly.length ? "unscoped" : "partial";
  }
  void byId;
  return { rows: [...rows.values()], unknown };
}

// ── Loading ─────────────────────────────────────────────────────────────────
function trackedFiles(root: string): string[] {
  try {
    return execFileSync("git", ["ls-files"], { cwd: root, encoding: "utf8", maxBuffer: 1 << 28 }).split("\n").filter(Boolean);
  } catch {
    const out: string[] = [];
    const walk = (d: string) => { for (const e of readdirSync(join(root, d), { withFileTypes: true })) { const p = d ? `${d}/${e.name}` : e.name; if (e.isDirectory()) { if (!SKIP.test(p) && e.name !== ".git") walk(p); } else out.push(p); } };
    walk("");
    return out;
  }
}

function loadParts(root: string, reqs: Requirement[], problems: TagProblem[]) {
  const p = join(root, PARTS_FILE);
  if (!existsSync(p)) return;
  const j = JSON.parse(readFileSync(p, "utf8"));
  const byId = new Map(reqs.map((r) => [r.id, r]));
  for (const [id, parts] of Object.entries<Record<string, string>>(j.parts ?? {})) {
    const r = byId.get(id);
    if (!r) { problems.push({ file: PARTS_FILE, line: 0, message: `parts: ${id} is not a defined requirement` }); continue; }
    r.parts = parts; r.partsSource = "declared";
  }
  for (const [id, v] of Object.entries<{ method: string; reason: string }>(j.nonTest ?? {})) {
    const r = byId.get(id);
    if (!r) { problems.push({ file: PARTS_FILE, line: 0, message: `nonTest: ${id} is not a defined requirement` }); continue; }
    if (!v.method || !v.reason) problems.push({ file: PARTS_FILE, line: 0, message: `nonTest: ${id} needs a method and a reason` });
    r.nonTest = v;
  }
}

/** IDs cited in docs/ that no definition site introduces (retired, prose-only or mis-formatted). */
export function undefinedCitations(root: string, files: string[], defined: Set<string>) {
  const out = new Map<string, string>();
  for (const f of files.filter((f) => /^docs\/[^/]+\.md$/.test(f) && f !== "docs/np_dhf_001.md")) {
    const lines = readFileSync(join(root, f), "utf8").split("\n");
    lines.forEach((l, i) => { for (const m of l.matchAll(ID_G)) if (!defined.has(m[0]) && !/^NP-/.test(m[0]) && !out.has(m[0])) out.set(m[0], `${f}:${i + 1}`); });
  }
  return out;
}

export function load(root = ROOT) {
  const files = trackedFiles(root);
  const problems: TagProblem[] = [];
  const reqs: Requirement[] = [];
  const seen = new Map<string, string>();
  for (const f of files.filter((f) => /^docs\/[^/]+\.md$/.test(f) && f !== "docs/np_dhf_001.md")) {
    for (const r of extractRequirements(f, readFileSync(join(root, f), "utf8"))) {
      const prior = seen.get(r.id);
      if (prior) { const p = reqs.find((x) => x.id === r.id)!; p.verification.push(`${f}:${r.line} ${r.text.slice(0, 200)}`); continue; }
      seen.set(r.id, f); reqs.push(r);
    }
  }
  loadParts(root, reqs, problems);
  const tests: TestUnit[] = [];
  const testFiles = files.filter(isTestFile);
  for (const f of testFiles) tests.push(...scanTestFile(f, readFileSync(join(root, f), "utf8"), problems));
  const undef = undefinedCitations(root, files, new Set(reqs.map((r) => r.id)));
  return { reqs: reqs.sort((a, b) => a.id.localeCompare(b.id, "en", { numeric: true })), tests, testFiles, problems, undef };
}

// ── Baseline (ratchet on untraced tests per file) ───────────────────────────
const untraced = (t: TestUnit) => !t.verifies.length && !t.none;

function untracedByFile(tests: TestUnit[]) {
  const m = new Map<string, number>();
  for (const t of tests) if (untraced(t)) m.set(t.file, (m.get(t.file) ?? 0) + 1);
  return m;
}
function readBaseline(root: string) {
  const m = new Map<string, number>();
  const p = join(root, BASELINE);
  if (!existsSync(p)) return m;
  for (const l of readFileSync(p, "utf8").split("\n")) { const x = l.match(/^(\S+)\s+(\d+)$/); if (x) m.set(x[1], +x[2]); }
  return m;
}

// ── Report ──────────────────────────────────────────────────────────────────
const ICON: Record<Status, string> = { covered: "🟢", partial: "🟠", unscoped: "🟡", untested: "🔴", "non-test": "🔵", retired: "⚪" };
const LABEL: Record<Status, string> = { covered: "covered", partial: "PARTIAL", unscoped: "UNSCOPED", untested: "UNTESTED", "non-test": "non-test", retired: "retired" };
const esc = (s: string) => s.replace(/\\/g, "\\\\").replace(/\|/g, "\\|").replace(/\n/g, " ");
const clip = (s: string, n: number) => (s.length > n ? s.slice(0, n - 1) + "…" : s);

export function renderMarkdown(d: ReturnType<typeof load>, a: ReturnType<typeof analyse>): string {
  const rows = a.rows;
  const active = rows.filter((r) => r.status !== "retired");
  const n = (s: Status) => rows.filter((r) => r.status === s).length;
  const traced = d.tests.filter((t) => t.verifies.length).length;
  const waived = d.tests.filter((t) => !t.verifies.length && t.none).length;
  const unt = d.tests.filter(untraced);
  const o: string[] = [];
  o.push("# Requirements ↔ tests traceability report", "");
  o.push("_Generated by `bun scripts/req-trace.ts`. Do not edit; regenerate._", "");
  o.push("## Summary", "", "| | |", "|---|---:|");
  o.push(`| Requirements defined (active) | ${active.length} |`);
  o.push(`| 🟢 covered — every part tested | ${n("covered")} |`);
  o.push(`| 🟠 partial — some declared parts untested | ${n("partial")} |`);
  o.push(`| 🟡 unscoped — has parts, tagged only as a whole | ${n("unscoped")} |`);
  o.push(`| 🔴 untested — no test | ${n("untested")} |`);
  o.push(`| 🔵 verified by declared non-test means | ${n("non-test")} |`);
  o.push(`| ⚪ retired (excluded) | ${n("retired")} |`);
  o.push(`| Test cases found | ${d.tests.length} in ${d.testFiles.length} files |`);
  o.push(`| …of which parameterized (one unit, many cases) | ${d.tests.filter((t) => t.parameterized).length} |`);
  o.push(`| …traced to ≥1 requirement | ${traced} |`);
  o.push(`| …declared \`@verifies-none\` | ${waived} |`);
  o.push(`| …**untraced** | ${unt.length} |`);
  o.push(`| Tags naming an undefined requirement | ${a.unknown.size} |`);
  o.push(`| Requirement IDs cited in docs but never defined | ${d.undef.size} |`, "");

  const gaps = rows.filter((r) => ["untested", "partial", "unscoped"].includes(r.status));
  o.push("## ⚠ Gaps — requirements not fully tested", "");
  if (!gaps.length) o.push("None.", "");
  else {
    o.push("| Status | Requirement | Not tested | Defined in |", "|---|---|---|---|");
    const order: Status[] = ["partial", "unscoped", "untested"];
    for (const r of [...gaps].sort((x, y) => order.indexOf(x.status) - order.indexOf(y.status) || x.req.id.localeCompare(y.req.id, "en", { numeric: true }))) {
      const what = r.status === "untested" ? clip(esc(r.req.text), 110)
        : r.status === "unscoped" ? `parts ${Object.keys(r.req.parts).join(", ")} (only a whole-requirement tag)`
        : r.untestedParts.map((p) => `**${p}** ${clip(esc(r.req.parts[p]), 70)}`).join("<br>");
      o.push(`| ${ICON[r.status]} ${LABEL[r.status]} | \`${r.req.id}\` | ${what} | ${r.req.file}:${r.req.line} |`);
    }
    o.push("");
  }

  const unknownParts = rows.filter((r) => r.tests.length && !Object.keys(r.req.parts).length && r.req.text.split(/(?<=[.;])\s/).length >= 3);
  if (unknownParts.length) {
    o.push("## Review — tested, but part coverage unknown", "",
      "These requirements have at least one test and a multi-sentence text, but no declared parts, so \"covered\" below means *some* test exists, not that every obligation is tested. Declare parts in `docs/status/requirement-parts.json`.", "",
      unknownParts.map((r) => `\`${r.req.id}\``).join(", "), "");
  }

  o.push("## Coverage by requirement", "", "| | Requirement | Tests | Parts tested | Defined in |", "|---|---|---:|---|---|");
  for (const r of rows) {
    const parts = Object.keys(r.req.parts);
    const pt = parts.length ? `${parts.length - r.untestedParts.length}/${parts.length}` + (r.untestedParts.length ? ` (missing ${r.untestedParts.join(",")})` : "") : "—";
    o.push(`| ${ICON[r.status]} | \`${r.req.id}\` | ${r.tests.length} | ${pt} | ${r.req.file}:${r.req.line} |`);
  }
  o.push("");

  const fam = new Map<string, Row[]>();
  for (const r of active) { const k = r.req.id.replace(/-\d+[a-z]?$|-[A-Z]\d+$/, ""); (fam.get(k) ?? fam.set(k, []).get(k)!).push(r); }
  o.push("## Coverage by requirement family", "", "| Family | Requirements | Covered | Partial/unscoped | Untested | Tests |", "|---|---:|---:|---:|---:|---:|");
  for (const [k, rs] of [...fam].sort()) {
    const c = (s: Status) => rs.filter((r) => r.status === s).length;
    o.push(`| ${k} | ${rs.length} | ${c("covered")} | ${c("partial") + c("unscoped")} | ${c("untested")} | ${new Set(rs.flatMap((r) => r.tests)).size} |`);
  }
  o.push("");

  const withTests = rows.filter((r) => r.tests.length);
  if (withTests.length) {
    o.push("## Tests per requirement", "");
    for (const r of withTests) {
      o.push(`### \`${r.req.id}\` — ${r.tests.length} test${r.tests.length === 1 ? "" : "s"}`, "");
      for (const t of r.tests) {
        const refs = t.verifies.filter((x) => x.id === r.req.id).flatMap((x) => (x.parts.length ? x.parts.map((p) => "#" + p) : ["(whole)"]));
        o.push(`- \`${t.file}:${t.line}\` ${clip(esc(t.name), 80)} — ${[...new Set(refs)].join(" ")}${t.fileLevel ? " _(file-level tag)_" : ""}${t.parameterized ? " _(parameterized)_" : ""}`);
      }
      o.push("");
    }
  }

  if (a.unknown.size) {
    o.push("## Tags naming a requirement that is not defined", "");
    for (const [id, ts] of a.unknown) o.push(`- \`${id}\`: ${ts.map((t) => `${t.file}:${t.line}`).join(", ")}`);
    o.push("");
  }

  if (d.undef.size) {
    o.push("## Requirement IDs cited in docs but not defined", "", "No table row or block definition introduces these (retired, defined only in prose, or mis-formatted). They cannot be traced to until defined.", "");
    for (const [id, at] of d.undef) o.push(`- \`${id}\` — first cited at ${at}`);
    o.push("");
  }

  o.push("## Untraced tests", "", `${unt.length} of ${d.tests.length} test cases name no requirement and declare no \`@verifies-none\`. By file:`, "", "| Tests | File |", "|---:|---|");
  for (const [f, c] of [...untracedByFile(d.tests)].sort((x, y) => y[1] - x[1] || x[0].localeCompare(y[0]))) o.push(`| ${c} | ${f} |`);
  o.push("");
  return o.join("\n");
}

// ── CLI ─────────────────────────────────────────────────────────────────────
function check(d: ReturnType<typeof load>, a: ReturnType<typeof analyse>, root: string): string[] {
  const fails = d.problems.map((p) => `${p.file}:${p.line}: ${p.message}`);
  const base = readBaseline(root);
  for (const [f, c] of untracedByFile(d.tests)) {
    const allowed = base.get(f) ?? 0;
    if (c > allowed) fails.push(`${f}: ${c} untraced test(s), baseline allows ${allowed}. Tag new tests with @verifies <REQ-ID> or @verifies-none <reason>.`);
  }
  void a;
  return [...new Set(fails)];
}

function selfTest(): void {
  let bad = 0;
  const eq = (name: string, got: unknown, want: unknown) => { if (JSON.stringify(got) !== JSON.stringify(want)) { bad++; console.error(`FAIL ${name}\n  got  ${JSON.stringify(got)}\n  want ${JSON.stringify(want)}`); } };
  const doc = [
    "| **`REQ-AA-01`** | Limit (a) 5 mm, and (b) 10 N | why |",
    "| ~~**REQ-AA-02**~~ | gone | |",
    "| `REQ-AA-03` | RETIRED — nothing requires it | |",
    "| REQ-AA-01 | Host test of the register layer | |",
    "> **`REQ-AA-04`** — First line",
    "> continues here.",
    "",
    "| HUB-REQ-C06 | polarity | |",
  ].join("\n");
  const rs = extractRequirements("docs/x.md", doc);
  eq("ids", rs.map((r) => r.id), ["REQ-AA-01", "REQ-AA-02", "REQ-AA-03", "REQ-AA-04", "HUB-REQ-C06"]);
  eq("retired", rs.map((r) => r.retired), [false, true, true, false, false]);
  eq("parts", Object.keys(rs[0].parts), ["a", "b"]);
  eq("verification row", rs[0].verification.length, 1);
  eq("blockquote continues", rs[3].text, "First line continues here.");
  const ct = extractRequirements("docs/y.md", "**HUB-REQ-C07:**\n\n| Clause | Requirement |\n|---|---|\n| (a) | sense it |\n| (b) | feed it |\n\nafter");
  eq("clause table parts", Object.keys(ct[0].parts), ["a", "b"]);

  const c = ["// @verifies REQ-AA-01#a", "static void test_one(void) {}", "", "static void test_two(void) {}", "// @verifies-none helper only", "static void test_three(void) {}"].join("\n");
  const pr: TagProblem[] = [];
  const ts = scanTestFile("firmware/x/tests/x_tests.c", c, pr);
  eq("c units", ts.map((t) => t.name), ["test_one", "test_two", "test_three"]);
  eq("c tag", ts.map((t) => t.verifies.length), [1, 0, 0]);
  eq("c none", ts.map((t) => t.none ?? null), [null, null, "helper only"]);

  const kt = ["// @verifies REQ-AA-01 REQ-AA-04", "@Test", "fun `does a thing`() {}", "@Test fun other() {}"].join("\n");
  const k = scanTestFile("app/x/src/jvmTest/FooTest.kt", kt, pr);
  eq("kt units", k.map((t) => t.name), ["`does a thing`", "other"]);
  eq("kt tag", k.map((t) => t.verifies.map((r) => r.id)), [["REQ-AA-01", "REQ-AA-04"], []]);

  const ts2 = scanTestFile("app/web/src/lib/a.test.ts", "// @verifies-file REQ-AA-04\nit('x', () => {});\ntest(\"y\", () => {});", pr);
  eq("ts file-level", ts2.map((t) => t.verifies.length), [1, 1]);
  eq("rs", scanTestFile("common/npps-core/tests/a.rs", "#[test]\nfn good() {}", pr).map((t) => t.name), ["good"]);
  eq("swift", scanTestFile("app/ios/NeurOneTests/ATests.swift", "func testFoo() {}\nfunc helper() {}", pr).map((t) => t.name), ["testFoo"]);
  // parameterized forms: one unit each, flagged
  const each = scanTestFile("app/web/src/lib/b.test.ts", "// @verifies REQ-AA-01#a\nit.each([\n  ['x', 1],\n  ['y', 2],\n])('refuses %s', (a) => {});\nit.each(ids)('%s round-trips', (id) => {});\nit.each`\n a | b\n`('table', () => {});", pr);
  eq("ts each units", each.map((t) => [t.name, !!t.parameterized, t.verifies.length]), [["refuses %s", true, 1], ["%s round-trips", true, 0], ["table", true, 0]]);
  const rsP = scanTestFile("common/x/tests/p.rs", "// @verifies REQ-AA-01\n#[test_case(1)]\n#[test_case(2)]\nfn many(n: u8) {}\n#[rstest]\n#[case(1)]\nfn cased(n: u8) {}\n#[test]\nfn plain() {}", pr);
  eq("rust param", rsP.map((t) => [t.name, !!t.parameterized, t.verifies.length]), [["many", true, 1], ["cased", true, 0], ["plain", false, 0]]);
  const cs = scanTestFile("app/windows/T.Tests/A.cs", "// @verifies REQ-AA-04\n[Theory]\n[InlineData(1)]\n[InlineData(2)]\npublic void Sums(int n) {}\n[Fact]\npublic void One() {}", pr);
  eq("c# theory", cs.map((t) => [t.name, !!t.parameterized, t.verifies.length]), [["Sums", true, 1], ["One", false, 0]]);
  const sw = scanTestFile("app/ios/T/ATests.swift", "@Test(arguments: [1, 2])\nfunc sums(_ n: Int) {}\nfunc testPlain() {}", pr);
  eq("swift param", sw.map((t) => [t.name, !!t.parameterized]), [["sums", true], ["testPlain", false]]);
  const ktP = scanTestFile("app/x/src/jvmTest/PTest.kt", "// @verifies REQ-AA-04\n@ParameterizedTest\n@ValueSource(ints = [1])\nfun sums(n: Int) {}", pr);
  eq("kotlin param", ktP.map((t) => [t.name, !!t.parameterized, t.verifies.length]), [["sums", true, 1]]);
  const py = scanTestFile("ci/test_p.py", "@pytest.mark.parametrize('a', [1])\ndef test_a(a): pass\ndef test_b(): pass", pr);
  eq("python param", py.map((t) => [t.name, !!t.parameterized]), [["test_a", true], ["test_b", false]]);
  eq("problems so far", pr.length, 0);

  const bad1: TagProblem[] = [];
  scanTestFile("a_tests.c", "// @verifies-none\nstatic void test_x(void){}\n// @verifies REQ-AA-01\n", bad1);
  eq("none without reason / orphan tag", bad1.map((p) => p.message), ["@verifies-none needs a reason", "@verifies is not within 12 lines above a test"]);

  const probs: TagProblem[] = [];
  const reqs2 = extractRequirements("docs/x.md", doc);
  const tests2: TestUnit[] = [
    { file: "t", line: 1, name: "a", verifies: [{ id: "REQ-AA-01", parts: ["a"] }], fileLevel: false },
    { file: "t", line: 2, name: "b", verifies: [{ id: "REQ-AA-04", parts: [] }, { id: "REQ-NOPE-01", parts: [] }, { id: "REQ-AA-02", parts: [] }], fileLevel: false },
  ];
  const an = analyse(reqs2, tests2, probs);
  const st = Object.fromEntries(an.rows.map((r) => [r.req.id, r.status]));
  eq("status", st, { "REQ-AA-01": "partial", "REQ-AA-02": "retired", "REQ-AA-03": "retired", "REQ-AA-04": "covered", "HUB-REQ-C06": "untested" });
  eq("unknown", [...an.unknown.keys()], ["REQ-NOPE-01"]);
  eq("problems", probs.map((p) => p.message), ["REQ-NOPE-01 is not a defined requirement", "REQ-AA-02 is retired"]);
  const unscoped = analyse(extractRequirements("d.md", doc), [{ file: "t", line: 1, name: "a", verifies: [{ id: "REQ-AA-01", parts: [] }], fileLevel: false }], []);
  eq("unscoped", unscoped.rows[0].status, "unscoped");

  if (bad) { console.error(`${bad} self-test failure(s)`); process.exit(1); }
  console.log("req-trace self-test: ok");
}

if (import.meta.main) {
  const args = process.argv.slice(2);
  const flag = (f: string) => args.includes(f);
  const val = (f: string) => { const i = args.indexOf(f); return i >= 0 ? args[i + 1] : undefined; };
  if (flag("--self-test")) selfTest();
  else {
    const d = load();
    const a = analyse(d.reqs, d.tests, d.problems);
    if (flag("--update-baseline")) {
      const lines = [...untracedByFile(d.tests)].sort().map(([f, c]) => `${f} ${c}`);
      writeFileSync(join(ROOT, BASELINE), [
        "# ci/req-trace-baseline.txt — untraced test cases per file (ratchet).",
        "#",
        "# scripts/req-trace.ts --check fails when a file has MORE untraced tests than listed here",
        "# (a file not listed allows 0). Trace a test with `@verifies <REQ-ID>` or declare",
        "# `@verifies-none <reason>`, then lower the count with `bun scripts/req-trace.ts --update-baseline`.",
        "# Never raise a count to admit a new untraced test.",
        "",
        ...lines, "",
      ].join("\n"));
      console.log(`wrote ${BASELINE}: ${lines.length} files`);
    } else if (flag("--check")) {
      const fails = check(d, a, ROOT);
      const base = readBaseline(ROOT), now = untracedByFile(d.tests);
      const stale = [...base].filter(([f, c]) => (now.get(f) ?? 0) < c);
      if (stale.length) console.log(`note: ${stale.length} baseline entr${stale.length === 1 ? "y is" : "ies are"} above the current count; run --update-baseline to tighten.`);
      if (fails.length) { console.error(fails.join("\n")); console.error(`req-trace: ${fails.length} problem(s)`); process.exit(1); }
      console.log(`req-trace: ok — ${d.reqs.length} requirements, ${d.tests.length} tests, ${d.tests.filter(untraced).length} untraced (within baseline)`);
    } else {
      const out = flag("--json")
        ? JSON.stringify({ requirements: a.rows.map((r) => ({ id: r.req.id, status: r.status, file: r.req.file, line: r.req.line, text: r.req.text, retired: r.req.retired, parts: r.req.parts, untestedParts: r.untestedParts, tests: r.tests.map((t) => ({ file: t.file, line: t.line, name: t.name })) })), untracedTests: d.tests.filter(untraced).map((t) => ({ file: t.file, line: t.line, name: t.name })), problems: d.problems }, null, 2)
        : renderMarkdown(d, a);
      const dest = val("--out");
      if (dest) writeFileSync(resolve(dest), out + "\n"); else console.log(out);
    }
  }
}
