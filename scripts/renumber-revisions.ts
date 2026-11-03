#!/usr/bin/env bun
/**
 * renumber-revisions.ts — move this branch's revision numbers past the ones main took, after a merge.
 *
 * Why. Every PR that edits a controlled document hand-assigns the next integer: the document's own
 * `**Revision:**`, its row in its history table, its `Rev` cell in the NP-DHF-001 index, and (because the DHF
 * index changed) the DHF's own revision, its index row and its row in the DHF history file. Two sessions
 * working in parallel take the same next integer. The DHF alone was touched by 125 commits in two weeks, and
 * NP-SW-001 by 97. Only changing who assigns the number removes the clash (a decision for NP-QMS-DC-001's
 * owner, not made here). This tool makes the clash one command to repair instead of a hand edit.
 *
 * Use, from the branch, after fetching:
 *
 *     git merge origin/main        # may stop on conflicts in the files below
 *     bun scripts/renumber-revisions.ts
 *     git add -A && git commit
 *
 * What it does. For every controlled document that BOTH sides bumped (base rev rb, ours ro, theirs rt, both
 * above rb), this branch's numbers are shifted by rt - rb, so they start after main's: the document's
 * `**Revision:**`, the history rows this branch added (in the document or in its linked
 * docs/reference/*-revision-history.md), the `Rev` cell of its row in the NP-DHF-001 index, `Rev a → b` and
 * `**Rev n (date)` in the lines this branch added. Only lines this branch added are touched, so main's rows
 * keep their numbers. Conflict hunks made up only of revision lines, dates, index rows and history rows are
 * resolved (history rows: main's, then this branch's; the rest: this branch's, then renumbered). Any other
 * conflict is left in place and reported, and the exit code is 2.
 *
 * What it does not do. It does not decide anything: the shift is arithmetic. It does not stage or commit. It
 * does not cover CLAUDE.md's revision, which has no `**Document:**`. A number this branch quotes for a document
 * it did NOT bump is not shifted. Run check-dhf-index.ts and req-trace.ts afterwards.
 *
 * CI-Kind: report
 * CI-Self-Test: bun scripts/renumber-revisions.ts --self-test
 */
import { execFileSync } from "child_process";
import { existsSync, mkdtempSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "fs";
import { tmpdir } from "os";
import { dirname, join } from "path";

const DOC_RE = /^docs\/np_[a-z0-9_]+\.md$/;
const DATE = String.raw`\d{4}-\d{2}-\d{2}`;
const HISTORY_ROW = new RegExp(String.raw`^\| (\d+) \| ${DATE} \|`);
const INDEX_ROW = new RegExp(String.raw`^\| (NP-[A-Z0-9-]+)(?: \([^)]*\))? \|(.*)\| (\d+) \| ${DATE} \| \[`);
const REV_LINE = /^\*\*Revision:\*\*\s*(\d+)\s*$/;
const DATE_LINE = /^\*\*Date:\*\*/;
const DHF = "docs/np_dhf_001.md";

function git(cwd: string, ...args: string[]): string {
  return execFileSync("git", args, { cwd, encoding: "utf8", stdio: ["ignore", "pipe", "pipe"] });
}
function show(cwd: string, ref: string, path: string): string | null {
  try { return git(cwd, "show", `${ref}:${path}`); } catch { return null; }
}
const revOf = (t: string | null): number | null => {
  const m = t === null ? null : /^\*\*Revision:\*\*\s*(\d+)/m.exec(t);
  return m ? Number(m[1]) : null;
};
const serialOf = (t: string | null): string | null => {
  const m = t === null ? null : /^\*\*Document:\*\*\s*(NP-[A-Z0-9-]+)/m.exec(t);
  return m ? m[1] : null;
};

/** docs/reference/np-fw-hub-001-revision-history.md → docs/np_fw_hub_001.md */
function ownerOfHistory(path: string): string | null {
  const m = /^docs\/reference\/(np-[a-z0-9-]+)-revision-history\.md$/.exec(path);
  return m ? `docs/${m[1].replace(/-/g, "_")}.md` : null;
}

interface Plan { offset: Map<string, number>; base: Map<string, number>; serial: Map<string, string> }

/** A shift for each controlled document both sides bumped. */
function plan(cwd: string, ours: string, theirs: string, base: string): Plan {
  const offset = new Map<string, number>(), baseRev = new Map<string, number>(), serial = new Map<string, string>();
  const files = new Set(git(cwd, "ls-tree", "-r", "--name-only", theirs, "docs").split("\n").filter((f) => DOC_RE.test(f)));
  for (const f of files) {
    const tt = show(cwd, theirs, f), ot = show(cwd, ours, f), bt = show(cwd, base, f);
    const s = serialOf(ot) ?? serialOf(tt);
    if (s !== null) serial.set(f, s);
    const rb = revOf(bt), ro = revOf(ot), rt = revOf(tt);
    if (rb === null || ro === null || rt === null) continue;
    baseRev.set(f, rb);
    if (ro > rb && rt > rb) offset.set(f, rt - rb);
  }
  return { offset, base: baseRev, serial };
}

/** Lines this branch added to a file: its lines minus the base's, as a multiset. */
function addedLines(ours: string | null, base: string | null): Set<string> {
  const left = new Map<string, number>();
  for (const l of (base ?? "").split("\n")) left.set(l, (left.get(l) ?? 0) + 1);
  const out = new Set<string>();
  for (const l of (ours ?? "").split("\n")) {
    const n = left.get(l) ?? 0;
    if (n > 0) left.set(l, n - 1); else out.add(l);
  }
  return out;
}

const OURS_MARK = /^<<<<<<< /, MID_MARK = /^=======$/, END_MARK = /^>>>>>>> /;

/** Resolve the hunks that hold nothing but revision lines, dates, index rows and history rows. */
function resolveHunks(lines: string[]): { lines: string[]; left: number } {
  const out: string[] = [];
  let left = 0;
  for (let i = 0; i < lines.length; i++) {
    if (!OURS_MARK.test(lines[i])) { out.push(lines[i]); continue; }
    let mid = i + 1;
    while (mid < lines.length && !MID_MARK.test(lines[mid])) mid++;
    let end = mid + 1;
    while (end < lines.length && !END_MARK.test(lines[end])) end++;
    const ours = lines.slice(i + 1, mid), theirs = lines.slice(mid + 1, end);
    const kind = (l: string): string | null =>
      l === "" ? "blank" : REV_LINE.test(l) ? "rev" : DATE_LINE.test(l) ? "date" : INDEX_ROW.test(l) ? "index" : HISTORY_ROW.test(l) ? "history" : null;
    const kinds = new Set([...ours, ...theirs].map(kind));
    const plain = !kinds.has(null) && [...kinds].every((k) => k !== "blank" || kinds.size > 1);
    if (!plain || end >= lines.length) { out.push(...lines.slice(i, end + 1)); left++; i = end; continue; }
    if (kinds.has("history")) {
      out.push(...theirs, ...ours);                    // main's rows, then this branch's (renumbered next)
    } else {
      const key = (l: string): string => (INDEX_ROW.exec(l)?.[1] ?? l);
      const mine = new Map(ours.map((l) => [key(l), l]));
      for (const l of theirs) out.push(mine.get(key(l)) ?? l);
      for (const l of ours) if (!theirs.some((t) => key(t) === key(l))) out.push(l);
    }
    i = end;
  }
  return { lines: out, left };
}

/** Shift one added line by the offsets that apply to it. */
function shiftLine(line: string, file: string, p: Plan): string {
  const owner = DOC_RE.test(file) ? file : ownerOfHistory(file);
  const own = owner === null ? 0 : (p.offset.get(owner) ?? 0);
  const rev = REV_LINE.exec(line);
  if (rev && DOC_RE.test(file)) return `**Revision:** ${Number(rev[1]) + own}`;
  const hist = HISTORY_ROW.exec(line);
  const bySerial = (s: string): [number, number] => {
    const f = [...p.serial].find(([, v]) => v === s)?.[0];
    return f === undefined ? [0, Infinity] : [p.offset.get(f) ?? 0, p.base.get(f) ?? Infinity];
  };
  let out = line;
  if (hist && owner !== null) out = out.replace(HISTORY_ROW, (m, n: string) => m.replace(`| ${n} |`, `| ${Number(n) + own} |`));
  if (file === DHF) {
    const ix = INDEX_ROW.exec(out);
    if (ix) {
      const [off] = bySerial(ix[1]);
      if (off > 0) out = out.replace(new RegExp(String.raw`\| ${ix[3]} \| (${DATE}) \| \[`), `| ${Number(ix[3]) + off} | $1 | [`);
    }
  }
  out = out.replace(/(NP-[A-Z0-9-]+) Rev (\d+)(?: → (\d+))?/g, (m, s: string, a: string, b?: string) => {
    const [off, rb] = bySerial(s);
    if (off === 0 || Number(a) < rb) return m;
    return b === undefined ? `${s} Rev ${Number(a) + off}` : `${s} Rev ${Number(a) + off} → ${Number(b) + off}`;
  });
  if (DOC_RE.test(file) && own > 0) {
    const rb = p.base.get(file) ?? Infinity;
    out = out.replace(/\*\*Rev (\d+) \(/g, (m, n: string) => (Number(n) > rb ? `**Rev ${Number(n) + own} (` : m));
  }
  return out;
}

export interface Result { shifted: string[]; unresolved: string[]; warnings: string[] }

export function renumber(cwd: string): Result {
  try { git(cwd, "rev-parse", "-q", "--verify", "MERGE_HEAD"); } catch { throw new Error("no merge in progress: run `git merge origin/main` first"); }
  const ours = "HEAD", theirs = "MERGE_HEAD";
  const base = git(cwd, "merge-base", ours, theirs).trim();
  const p = plan(cwd, ours, theirs, base);
  const touched = new Set<string>();
  for (const f of p.offset.keys()) { touched.add(f); }
  for (const f of git(cwd, "diff", "--name-only", "--diff-filter=U").split("\n").filter(Boolean)) touched.add(f);
  if (p.offset.size > 0) {
    touched.add(DHF);
    for (const f of git(cwd, "ls-tree", "-r", "--name-only", theirs, "docs/reference").split("\n")) {
      const o = ownerOfHistory(f);
      if (o !== null && p.offset.has(o)) touched.add(f);
    }
  }
  const res: Result = { shifted: [], unresolved: [], warnings: [] };
  for (const f of [...touched].sort()) {
    const path = join(cwd, f);
    if (!existsSync(path)) continue;
    const added = addedLines(show(cwd, ours, f), show(cwd, base, f));
    const { lines, left } = resolveHunks(readFileSync(path, "utf8").split("\n"));
    if (left > 0) res.unresolved.push(`${f}: ${left} conflict hunk(s) hold more than revision numbers and rows`);
    let changed = false;
    const next = lines.map((l) => {
      if (!added.has(l) || l.startsWith("<<<<<<<") || l.startsWith(">>>>>>>")) return l;
      const s = shiftLine(l, f, p);
      if (s !== l) changed = true;
      return s;
    });
    writeFileSync(path, next.join("\n"));
    if (changed) res.shifted.push(f);
    const nums = next.map((l) => HISTORY_ROW.exec(l)).filter((m): m is RegExpExecArray => m !== null).map((m) => Number(m[1]));
    for (let i = 1; i < nums.length; i++) {
      if (nums[i] <= nums[i - 1] && (DOC_RE.test(f) || ownerOfHistory(f) !== null) && nums[i] >= nums[nums.length - 1] - 5) {
        res.warnings.push(`${f}: history rows not ascending near ${nums[i]}`);
        break;
      }
    }
  }
  return res;
}

// ── Self-test ────────────────────────────────────────────────────────────────

function selfTest(): void {
  let failures = 0;
  const check = (ok: boolean, msg: string): void => { if (!ok) { failures++; console.error(`FAIL: ${msg}`); } else console.log(`PASS: ${msg}`); };
  const doc = (serial: string, rev: number, rows: string[]): string =>
    `**Document:** ${serial}\n**Revision:** ${rev}\n**Date:** 2026-10-01\n\ntext\n\n| Rev | Date | Author | Description |\n|---|---|---|---|\n${rows.join("\n")}\n`;
  const idx = (dhf: number, sw: number, fw: number): string =>
    `**Document:** NP-DHF-001\n**Revision:** ${dhf}\n**Date:** 2026-10-01\n\n| Serial | Title | Rev | Date | File | Status | Owner |\n|---|---|---|---|---|---|---|\n` +
    `| NP-DHF-001 | DHF | ${dhf} | 2026-10-01 | [a](./np_dhf_001.md) | ACTIVE | QMS |\n` +
    `| NP-SW-001 | SW | ${sw} | 2026-10-01 | [b](./np_sw_001.md) | ACTIVE | QMS |\n` +
    `| NP-FW-HUB-001 | HUB | ${fw} | 2026-10-01 | [c](./np_fw_hub_001.md) | DRAFT | FW |\n`;
  const hist = (rows: string[]): string => `| Rev | Date | Author | Description |\n|---|---|---|---|\n${rows.join("\n")}\n`;
  const row = (n: number, text: string): string => `| ${n} | 2026-10-0${Math.min(n, 9)} | Eng | ${text} |`;

  const repo = mkdtempSync(join(tmpdir(), "renum-"));
  try {
    const w = (f: string, t: string): void => { mkdirSync(dirname(join(repo, f)), { recursive: true }); writeFileSync(join(repo, f), t); };
    const commit = (m: string): void => { git(repo, "add", "-A"); git(repo, "-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "-m", m); };
    git(repo, "init", "-q", "-b", "main");
    const swRows = [row(1, "a"), row(2, "b")];
    const dhfRows = [row(1, "a"), row(2, "b")];
    w("docs/np_dhf_001.md", idx(2, 2, 1));
    w("docs/reference/np-dhf-001-revision-history.md", hist(dhfRows));
    w("docs/np_sw_001.md", doc("NP-SW-001", 2, swRows));
    w("docs/np_fw_hub_001.md", doc("NP-FW-HUB-001", 1, [row(1, "a")]));
    commit("base");
    git(repo, "checkout", "-q", "-b", "ours");
    // ours: bumps the DHF, SW (2 -> 3) and the hub (1 -> 2)
    w("docs/np_dhf_001.md", idx(3, 3, 2));
    w("docs/reference/np-dhf-001-revision-history.md", hist([...dhfRows, row(3, "**NP-FW-HUB-001 Rev 1 → 2, NP-SW-001 Rev 2 → 3: ours.**")]));
    w("docs/np_sw_001.md", doc("NP-SW-001", 3, [...swRows, row(3, "ours")]));
    w("docs/np_fw_hub_001.md", doc("NP-FW-HUB-001", 2, [row(1, "a"), row(2, "ours")]));
    commit("ours");
    git(repo, "checkout", "-q", "main");
    // main: bumps the DHF and SW (2 -> 3) too
    w("docs/np_dhf_001.md", idx(3, 3, 1));
    w("docs/reference/np-dhf-001-revision-history.md", hist([...dhfRows, row(3, "**NP-SW-001 Rev 2 → 3: main.**")]));
    w("docs/np_sw_001.md", doc("NP-SW-001", 3, [...swRows, row(3, "main")]));
    commit("main");
    git(repo, "checkout", "-q", "ours");
    let conflicted = false;
    try { git(repo, "merge", "main"); } catch { conflicted = true; }
    check(conflicted, "the merge stops on the colliding numbers");
    const r = renumber(repo);
    const rd = (f: string): string => readFileSync(join(repo, f), "utf8");
    const dhf = rd("docs/np_dhf_001.md"), dh = rd("docs/reference/np-dhf-001-revision-history.md"), sw = rd("docs/np_sw_001.md"), fw = rd("docs/np_fw_hub_001.md");
    check(r.unresolved.length === 0, "nothing is left unresolved");
    check(!/<<<<<<<|>>>>>>>/.test(dhf + dh + sw + fw), "no conflict markers remain");
    check(/^\*\*Revision:\*\* 4$/m.test(dhf) && /\| NP-DHF-001 \| DHF \| 4 \|/.test(dhf), "the DHF is Rev 4, past main's 3, in the header and its own index row");
    check(/\| NP-SW-001 \| SW \| 4 \|/.test(dhf) && /^\*\*Revision:\*\* 4$/m.test(sw), "NP-SW-001 is Rev 4 in the file and in the index");
    check(/\| NP-FW-HUB-001 \| HUB \| 2 \|/.test(dhf) && /^\*\*Revision:\*\* 2$/m.test(fw), "the hub, which main did not bump, keeps Rev 2");
    check(/\| 3 \|[^\n]*main/.test(dh) && /\| 4 \|[^\n]*ours/.test(dh), "the DHF history keeps main's row 3 and puts this branch's row at 4");
    check(/NP-SW-001 Rev 3 → 4/.test(dh) && /NP-FW-HUB-001 Rev 1 → 2/.test(dh), "the row's text follows the shift for the SW document and not for the hub");
    check(/\| 3 \|[^\n]*main/.test(sw) && /\| 4 \|[^\n]*ours/.test(sw), "NP-SW-001's own history is shifted the same way");
    check(!/\| 3 \|[^\n]*ours/.test(dh + sw), "no row of this branch keeps the number main took");
    check(r.warnings.length === 0, "no ordering warning");
    // a second run changes nothing it should not: nothing new is added by this branch past main
    git(repo, "add", "-A"); git(repo, "-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "-m", "merge");
  } finally { rmSync(repo, { recursive: true, force: true }); }

  // A conflict that holds prose is left alone and reported.
  const r2 = mkdtempSync(join(tmpdir(), "renum-"));
  try {
    const w = (f: string, t: string): void => { mkdirSync(dirname(join(r2, f)), { recursive: true }); writeFileSync(join(r2, f), t); };
    const commit = (m: string): void => { git(r2, "add", "-A"); git(r2, "-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "-m", m); };
    git(r2, "init", "-q", "-b", "main");
    const d = (rev: number, body: string): string => `**Document:** NP-SW-001\n**Revision:** ${rev}\n**Date:** 2026-10-01\n\n${body}\n`;
    w("docs/np_sw_001.md", d(1, "one"));
    commit("base");
    git(r2, "checkout", "-q", "-b", "ours");
    w("docs/np_sw_001.md", d(2, "ours prose"));
    commit("ours");
    git(r2, "checkout", "-q", "main");
    w("docs/np_sw_001.md", d(2, "main prose"));
    commit("main");
    git(r2, "checkout", "-q", "ours");
    try { git(r2, "merge", "main"); } catch { /* conflict expected */ }
    const r = renumber(r2);
    check(r.unresolved.length === 1 && /<<<<<<< /.test(readFileSync(join(r2, "docs/np_sw_001.md"), "utf8")), "a conflict in prose is left in place and reported");
  } finally { rmSync(r2, { recursive: true, force: true }); }

  const r3 = mkdtempSync(join(tmpdir(), "renum-"));
  try {
    git(r3, "init", "-q", "-b", "main");
    let threw = false;
    try { renumber(r3); } catch { threw = true; }
    check(threw, "with no merge in progress it refuses");
  } finally { rmSync(r3, { recursive: true, force: true }); }

  if (failures > 0) { console.error(`\nFAILED (${failures})`); process.exit(1); }
  console.log("\nALL PASS");
}

if (import.meta.main) {
  if (process.argv.includes("--self-test")) selfTest();
  else {
    try {
      const r = renumber(process.cwd());
      for (const f of r.shifted) console.log(`renumbered: ${f}`);
      if (r.shifted.length === 0) console.log("nothing to renumber: no document was bumped by both sides");
      for (const w of r.warnings) console.log(`warning: ${w}`);
      for (const u of r.unresolved) console.error(`unresolved: ${u}`);
      console.log("next: bun scripts/check-dhf-index.ts && bun scripts/req-trace.ts --check, then git add -A && git commit");
      process.exit(r.unresolved.length > 0 ? 2 : 0);
    } catch (e) {
      console.error(String((e as Error).message));
      process.exit(1);
    }
  }
}
