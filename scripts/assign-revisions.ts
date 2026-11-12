#!/usr/bin/env bun
/**
 * assign-revisions.ts — assign revision numbers when a change merges, instead of by the author.
 *
 * The scheme (docs/revisions/README.md; the decision: docs/human-input/np-qms-dc-001-revision-assignment.md,
 * option A, the principal, 2026-11-02): a PR does not write a revision number. It adds one fragment,
 * docs/revisions/pending/<slug>.md, naming the documents it bumps and the history text for each. When the PR
 * merges, the assign-revisions workflow runs this script on `main`, once per fragment, oldest first. For each
 * document named it sets `**Revision:**` (current + 1) and `**Date:**`, the document's row in the NP-DHF-001 index,
 * and appends the history row (in the linked docs/reference/*-revision-history.md if there is one, else in the
 * document; a document with no history table at all gets one, started at its end). The DHF is bumped once per fragment. The fragment is deleted. Numbers are never chosen by an author,
 * so two PRs cannot take the same one.
 *
 * Fragment:
 *
 *     date: 2026-11-02                 (optional; default: today, UTC)
 *     ## DHF
 *     author: NeurOne Firmware Engineering
 *     text: **{NP-SW-001 from → to}: what changed.** …
 *     ## NP-SW-001
 *     author: NeurOne Software Engineering
 *     text: **§6.3: …**
 *
 * `{X from}`, `{X to}` and `{X from → to}` (X is a serial, or DHF) become the numbers assigned. They work in a
 * history text and, for a bumped document, anywhere in the document's own text (a "> **Rev {X to} (date)**" callout).
 *
 * Usage:
 *     bun scripts/assign-revisions.ts --mode      print the policy mode (hand | assign-at-merge)
 *     bun scripts/assign-revisions.ts --peek      print the oldest pending fragment; exit 3 if there is none
 *     bun scripts/assign-revisions.ts --next      assign the oldest pending fragment; exit 3 if there is none
 *     bun scripts/assign-revisions.ts --convert   turn this branch's hand-numbered bump into a fragment
 *                                                 (after `git merge origin/main`; --base <ref> to change the base)
 *     bun scripts/assign-revisions.ts --self-test
 *
 * --next changes files and does not commit. It runs check-dhf-index.ts and check-md-tables.ts afterwards and exits 1
 * if either fails, so the workflow does not push a result that breaks them.
 *
 * CI-Kind: report
 * CI-Self-Test: bun scripts/assign-revisions.ts --self-test
 */
import { spawnSync } from "child_process";
import { existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, statSync, unlinkSync, writeFileSync } from "fs";
import { tmpdir } from "os";
import { dirname, join } from "path";
import {
  DATE, DATE_LINE, DHF_FILE, DHF_SERIAL, HISTORY_ROW, PENDING_DIR, REV_LINE, SERIAL,
  controlledDocs, git, historyFile, parseFragment, readMode, resolveTokens, revOf, serialOf, show,
  type Fragment,
} from "./revision-lib";

/* The serial cell may be bold (`| **NP-RISK-002** |`: the DHF index bolds some rows); the bold is matched and kept. */
const INDEX_ROW = new RegExp(String.raw`^\| (?:\*\*)?(${SERIAL})(?:\*\*)?(?: \([^)]*\))? \|.*\| \d+ \| ${DATE} \| \[`);
/** The serial in an index row's first cell, without the bold some rows carry. */
const rowSerial = (l: string): string => l.split("|")[1].trim().replace(/^\*\*(.*)\*\*$/, "$1");
const read = (root: string, f: string): string => readFileSync(join(root, f), "utf8");
const write = (root: string, f: string, t: string): void => writeFileSync(join(root, f), t);

export function pendingFragments(root: string): string[] {
  const dir = join(root, PENDING_DIR);
  if (!existsSync(dir)) return [];
  const added = (f: string): number => {
    try { return Number(git(root, "log", "-1", "--diff-filter=A", "--format=%ct", "--", f).trim()) || 0; } catch { return 0; }
  };
  return readdirSync(dir)
    .filter((f) => f.endsWith(".md") && f.toLowerCase() !== "readme.md")
    .map((f) => `${PENDING_DIR}/${f}`)
    .sort((a, b) => added(a) - added(b) || a.localeCompare(b));
}

/** The heading and table a document gets when it has no history table to append to (its first row is the one assigned). */
const NEW_TABLE_HEAD = "## Revision history\n\nRevisions before the first row below were not recorded in a table in this document.\n\n| Rev | Date | Author | Description |\n|---|---|---|---|";

/**
 * Insert a history row after the last row of the run that holds the highest number. A document with no history table
 * at all (`startTable`: it has no linked history file either) gets one, at its end, holding this row.
 */
function insertRow(text: string, row: string, n: number, where: string, startTable = false): string {
  const lines = text.split("\n");
  let max = -1, at = -1;
  lines.forEach((l, i) => {
    const m = HISTORY_ROW.exec(l);
    if (m && Number(m[1]) >= max) { max = Number(m[1]); at = i; }
  });
  if (at < 0) {
    if (!startTable) throw new Error(`${where}: no history row to append after`);
    if (/^#+ +Revision history\s*$/im.test(text)) throw new Error(`${where}: has a "Revision history" heading but no history row to append after; fix the table by hand`);
    return `${text.replace(/\s+$/, "")}\n\n${NEW_TABLE_HEAD}\n${row}\n`;
  }
  if (max >= n) throw new Error(`${where}: its history already reaches ${max}, so revision ${n} would not be the newest`);
  let end = at;
  while (end + 1 < lines.length && HISTORY_ROW.test(lines[end + 1])) end++;
  lines.splice(end + 1, 0, row);
  return lines.join("\n");
}

/** The highest revision a history table holds (0 if it has no rows). */
function newestHistoryRow(text: string): number {
  let max = 0;
  for (const l of text.split("\n")) { const m = HISTORY_ROW.exec(l); if (m) max = Math.max(max, Number(m[1])); }
  return max;
}

function setRevision(text: string, rev: number, date: string): string {
  return text.split("\n").map((l) => (REV_LINE.test(l) ? `**Revision:** ${rev}` : DATE_LINE.test(l) ? l.replace(new RegExp(DATE), date) : l)).join("\n");
}

/** Set the Rev and Date cells of the DHF index row whose first cell is exactly `serial`. */
function setIndexRow(dhf: string, serial: string, rev: number, date: string): string {
  let hit = 0;
  const out = dhf.split("\n").map((l) => {
    if (!INDEX_ROW.test(l)) return l;
    const cells = l.split("|");
    if (rowSerial(l) !== serial) return l;
    hit++;
    const k = cells.findIndex((c, i) => i >= 2 && /^\s*\d+\s*$/.test(c) && new RegExp(`^\\s*${DATE}\\s*$`).test(cells[i + 1] ?? ""));
    cells[k] = ` ${rev} `;
    cells[k + 1] = ` ${date} `;
    return cells.join("|");
  });
  if (hit !== 1) throw new Error(`${DHF_FILE}: expected one index row for ${serial}, found ${hit}`);
  return out.join("\n");
}

export type Assigned = { status: "none" } | { status: "done"; fragment: string; revisions: Record<string, string> };

export function assignNext(root: string, validate = true): Assigned {
  const pending = pendingFragments(root);
  if (pending.length === 0) return { status: "none" };
  const path = pending[0];
  const frag = parseFragment(path, read(root, path));
  if (typeof frag === "string") throw new Error(frag);
  const docs = controlledDocs(root);
  const fileOf = new Map([...docs].map(([f, s]) => [s, f]));
  const date = frag.date ?? new Date().toISOString().slice(0, 10);

  const revs = new Map<string, { from: number; to: number }>();
  const texts = new Map<string, string>();
  const get = (f: string): string => texts.get(f) ?? (texts.set(f, read(root, f)), texts.get(f) as string);
  const serialFor = (t: string): string => (t === "DHF" ? DHF_SERIAL : t);
  for (const s of frag.sections) {
    const file = fileOf.get(serialFor(s.target));
    if (file === undefined) throw new Error(`${path}: ${s.target} is not a controlled document under docs/`);
    const line = revOf(get(file));
    if (line === null) throw new Error(`${file}: no revision`);
    // The newest history row is what happened; the Revision line can be behind it when two hand-numbered changes
    // set the same line (git merges an identical edit without a conflict) and each added its own row. Never reuse a
    // number the history already has: the revision is the higher of the two, and this change takes the next.
    const cur = Math.max(line, newestHistoryRow(get(historyFile(root, serialFor(s.target), file))));
    revs.set(s.target, { from: cur, to: cur + 1 });
  }
  for (const s of frag.sections) {
    const serial = serialFor(s.target), file = fileOf.get(serial) as string, r = revs.get(s.target) as { from: number; to: number };
    texts.set(file, resolveTokens(setRevision(get(file), r.to, date), revs));
    const hf = historyFile(root, serial, file);
    texts.set(hf, insertRow(get(hf), `| ${r.to} | ${date} | ${s.author} | ${resolveTokens(s.text, revs)} |`, r.to, hf, hf === file));
  }
  for (const s of frag.sections) {
    const r = revs.get(s.target) as { from: number; to: number };
    texts.set(DHF_FILE, setIndexRow(get(DHF_FILE), serialFor(s.target), r.to, date));
  }
  for (const [f, t] of texts) write(root, f, t);
  unlinkSync(join(root, path));

  if (validate) {
    for (const g of ["check-dhf-index.ts", "check-md-tables.ts"]) {
      const r = spawnSync("bun", [join(import.meta.dir, g)], { cwd: root, encoding: "utf8" });
      if (r.status !== 0) throw new Error(`${g} fails after assigning ${path}:\n${r.stdout}${r.stderr}`);
    }
  }
  return { status: "done", fragment: path, revisions: Object.fromEntries([...revs].map(([k, v]) => [k, `${v.from} → ${v.to}`])) };
}

// ── Converting a hand-numbered branch ────────────────────────────────────────

/** After `git merge origin/main`: undo this branch's hand-assigned numbers and write the fragment that assigns them. */
export function convertBranch(root: string, base = "origin/main"): string {
  const mb = git(root, "merge-base", "HEAD", base).trim();
  const docs = controlledDocs(root);
  const bumped = new Map<string, { file: string; from: number; to: number }>();
  for (const [file, serial] of docs) {
    const b = show(root, mb, file), to = revOf(read(root, file));
    const from = b === null ? null : revOf(b);
    if (from !== null && to !== null && to > from) bumped.set(serial, { file, from, to });
  }
  if (bumped.size === 0) throw new Error("this branch bumps no controlled document: nothing to convert");
  if (!bumped.has(DHF_SERIAL)) throw new Error(`this branch bumps documents but not ${DHF_SERIAL}: the scheme needs the DHF bumped once`);
  const key = (serial: string): string => (serial === DHF_SERIAL ? "DHF" : serial);
  const tokenize = (text: string): string => {
    let t = text;
    for (const [s, b] of bumped) {
      t = t.split(`${s} Rev ${b.from} → ${b.to}`).join(`${s} Rev {${key(s)} from → to}`);
      t = t.split(`${s} Rev ${b.to}`).join(`${s} Rev {${key(s)} to}`);
    }
    return t;
  };
  const sections: string[] = [];
  const order = [DHF_SERIAL, ...[...bumped.keys()].filter((s) => s !== DHF_SERIAL).sort()];
  const files = new Map<string, string>();
  const get = (f: string): string => files.get(f) ?? (files.set(f, read(root, f)), files.get(f) as string);
  for (const serial of order) {
    const b = bumped.get(serial) as { file: string; from: number; to: number };
    const hf = historyFile(root, serial, b.file);
    const lines = get(hf).split("\n");
    const added = lines.map((l, i) => ({ m: HISTORY_ROW.exec(l), i })).filter((x) => x.m && Number(x.m[1]) > b.from && Number(x.m[1]) <= b.to);
    if (added.length !== 1) throw new Error(`${hf}: expected one history row for ${serial} between Rev ${b.from} and ${b.to}, found ${added.length}`);
    const m = added[0].m as RegExpExecArray;
    sections.push(`## ${key(serial)}\nauthor: ${m[3]}\ntext: ${tokenize(m[4])}\n`);
    lines.splice(added[0].i, 1);
    files.set(hf, lines.join("\n"));
    const baseText = show(root, mb, b.file) as string;
    const dateBase = /^\*\*Date:\*\*\s*(\d{4}-\d{2}-\d{2})/m.exec(baseText)?.[1] ?? "";
    files.set(b.file, setRevision(get(b.file), b.from, dateBase));
  }
  let dhf = get(DHF_FILE);
  const baseDhf = show(root, mb, DHF_FILE) as string;
  for (const serial of order) {
    const row = baseDhf.split("\n").find((l) => INDEX_ROW.test(l) && rowSerial(l) === serial);
    if (row === undefined) continue;
    const cells = row.split("|");
    const k = cells.findIndex((c, i) => i >= 2 && /^\s*\d+\s*$/.test(c) && new RegExp(`^\\s*${DATE}\\s*$`).test(cells[i + 1] ?? ""));
    dhf = setIndexRow(dhf, serial, Number(cells[k]), cells[k + 1].trim());
  }
  files.set(DHF_FILE, dhf);
  for (const [f, t] of files) write(root, f, t);
  const slug = (git(root, "rev-parse", "--abbrev-ref", "HEAD").trim().replace(/[^A-Za-z0-9._-]+/g, "-") || "change").slice(0, 60);
  const out = `${PENDING_DIR}/${slug}.md`;
  mkdirSync(join(root, PENDING_DIR), { recursive: true });
  write(root, out, sections.join("\n"));
  return out;
}

// ── Self-test ────────────────────────────────────────────────────────────────

function selfTest(): void {
  let failures = 0;
  const check = (ok: boolean, msg: string): void => { if (!ok) { failures++; console.error(`FAIL: ${msg}`); } else console.log(`PASS: ${msg}`); };
  const row = (n: number, text: string): string => `| ${n} | 2026-10-0${Math.min(n, 9)} | Eng | ${text} |`;
  const hist = (rows: string[]): string => `| Rev | Date | Author | Description |\n|---|---|---|---|\n${rows.join("\n")}\n`;
  const doc = (serial: string, rev: number, rows: string[] | null, body = "text"): string =>
    `**Document:** ${serial}\n**Revision:** ${rev}\n**Date:** 2026-10-01  \n\n${body}\n\n${rows ? hist(rows) : ""}`;
  const idx = (dhf: number, sw: number, fw: number): string =>
    `**Document:** NP-DHF-001\n**Revision:** ${dhf}\n**Date:** 2026-10-01\n\n| Serial | Title | Rev | Date | File | Status | Owner |\n|---|---|---|---|---|---|---|\n` +
    `| NP-DHF-001 | DHF | ${dhf} | 2026-10-01 | [a](./np_dhf_001.md) | ACTIVE | QMS |\n` +
    `| NP-SW-001 | SW | ${sw} | 2026-10-01 | [b](./np_sw_001.md) | ACTIVE | QMS |\n` +
    `| NP-FW-HUB-001 | HUB | ${fw} | 2026-10-01 | [c](./np_fw_hub_001.md) | DRAFT | FW |\n`;

  const mk = (): string => {
    const r = mkdtempSync(join(tmpdir(), "assign-"));
    const w = (f: string, t: string): void => { mkdirSync(dirname(join(r, f)), { recursive: true }); writeFileSync(join(r, f), t); };
    git(r, "init", "-q", "-b", "main");
    w("docs/np_dhf_001.md", idx(2, 2, 1));
    w("docs/reference/np-dhf-001-revision-history.md", hist([row(1, "a"), row(2, "b")]));
    w("docs/np_sw_001.md", doc("NP-SW-001", 2, [row(1, "a"), row(2, "b")]));
    w("docs/np_fw_hub_001.md", doc("NP-FW-HUB-001", 1, null, "> **Rev {NP-FW-HUB-001 to} (2026-11-02) — the callout.**"));
    w("docs/reference/np-fw-hub-001-revision-history.md", hist([row(1, "a")]));
    return r;
  };
  const commit = (r: string, m: string): void => { git(r, "add", "-A"); git(r, "-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "-m", m); };
  const frag = (r: string, name: string, body: string): void => { mkdirSync(join(r, PENDING_DIR), { recursive: true }); writeFileSync(join(r, PENDING_DIR, name), body); };

  {
    const bold = "| **NP-RISK-002** | Risk File | 16 | 2026-10-08 | [r](./np_risk_002.md) | ACTIVE | RISK |";
    check(setIndexRow(bold, "NP-RISK-002", 17, "2026-11-02") === "| **NP-RISK-002** | Risk File | 17 | 2026-11-02 | [r](./np_risk_002.md) | ACTIVE | RISK |",
      "an index row whose serial cell is bold is found, its Rev and Date set, and its bold kept");
  }
  const r = mk();
  try {
    commit(r, "base");
    frag(r, "a-first.md", "date: 2026-11-02\n## DHF\nauthor: Eng A\ntext: **{NP-SW-001 from → to}, {NP-FW-HUB-001 from → to}: first.**\n## NP-SW-001\nauthor: Eng A\ntext: **first** (now Rev {NP-SW-001 to})\n## NP-FW-HUB-001\nauthor: Eng A\ntext: **first** hub\n");
    frag(r, "b-second.md", "date: 2026-11-03\n## DHF\nauthor: Eng B\ntext: **{NP-SW-001 from → to}: second.**\n## NP-SW-001\nauthor: Eng B\ntext: **second**\n");
    commit(r, "fragments");
    const one = assignNext(r, false);
    check(one.status === "done" && one.fragment.endsWith("a-first.md"), "the oldest fragment is assigned first (ties by name)");
    let dhf = read(r, DHF_FILE), sw = read(r, "docs/np_sw_001.md"), fw = read(r, "docs/np_fw_hub_001.md");
    const dh = read(r, "docs/reference/np-dhf-001-revision-history.md"), fh = read(r, "docs/reference/np-fw-hub-001-revision-history.md");
    check(/^\*\*Revision:\*\* 3$/m.test(dhf) && /\| NP-DHF-001 \| DHF \| 3 \| 2026-11-02 \|/.test(dhf), "the DHF is bumped once: header and its own index row");
    check(/\| NP-SW-001 \| SW \| 3 \| 2026-11-02 \|/.test(dhf) && /\| NP-FW-HUB-001 \| HUB \| 2 \| 2026-11-02 \|/.test(dhf), "each named document's index row carries its new Rev and the date");
    check(/^\*\*Revision:\*\* 3$/m.test(sw) && /^\*\*Date:\*\* 2026-11-02  $/m.test(sw), "a document's Revision and Date are set, and the trailing spaces of the Date line are kept");
    check(/\| 3 \| 2026-11-02 \| Eng A \| \*\*NP-SW-001 2 → 3|\| 3 \| 2026-11-02 \| Eng A \| \*\*2 → 3, 1 → 2: first/.test(dh), "the DHF history row is appended with the tokens resolved");
    check(/\| 3 \| 2026-11-02 \| Eng A \| \*\*first\*\* \(now Rev 3\) \|/.test(sw), "the document's own inline history takes the row, and a token in it resolves");
    check(/\| 2 \| 2026-11-02 \| Eng A \| \*\*first\*\* hub \|/.test(fh), "a document with a linked history file gets the row there");
    check(/> \*\*Rev 2 \(2026-11-02\) — the callout\.\*\*/.test(fw), "a token in a bumped document's own text resolves");
    check(!existsSync(join(r, PENDING_DIR, "a-first.md")) && existsSync(join(r, PENDING_DIR, "b-second.md")), "the assigned fragment is deleted and the next stays");
    const two = assignNext(r, false);
    dhf = read(r, DHF_FILE); sw = read(r, "docs/np_sw_001.md");
    check(two.status === "done" && /^\*\*Revision:\*\* 4$/m.test(dhf) && /^\*\*Revision:\*\* 4$/m.test(sw), "the second fragment takes the next numbers: DHF 4, SW 4");
    check(/\| 4 \| 2026-11-03 \| Eng B \|/.test(sw) && !/\| 4 \|[^\n]*first/.test(sw), "and its row follows the first's");
    check(assignNext(r, false).status === "none", "with nothing pending it reports so");
  } finally { rmSync(r, { recursive: true, force: true }); }

  const bad = mk();
  try {
    commit(bad, "base");
    frag(bad, "x.md", "## DHF\nauthor: A\ntext: t\n## NP-NOPE-001\nauthor: A\ntext: t\n");
    commit(bad, "frag");
    const before = read(bad, DHF_FILE);
    let msg = "";
    try { assignNext(bad, false); } catch (e) { msg = (e as Error).message; }
    check(/not a controlled document/.test(msg) && read(bad, DHF_FILE) === before && existsSync(join(bad, PENDING_DIR, "x.md")), "an unknown document stops it with nothing changed and the fragment kept");
  } finally { rmSync(bad, { recursive: true, force: true }); }

  // A Revision line behind its own history (two hand-numbered changes set the same line): the history is the truth.
  const heal = mk();
  try {
    const w = (f: string, t: string): void => writeFileSync(join(heal, f), t);
    w("docs/np_sw_001.md", doc("NP-SW-001", 2, [row(1, "a"), row(2, "b"), row(3, "an earlier change whose Revision edit was lost")]));
    commit(heal, "base");
    frag(heal, "h.md", "date: 2026-11-02\n## DHF\nauthor: A\ntext: **{NP-SW-001 from → to}: healed.**\n## NP-SW-001\nauthor: A\ntext: **next**\n");
    commit(heal, "fragment");
    const res = assignNext(heal, false);
    const sw = read(heal, "docs/np_sw_001.md"), dh = read(heal, "docs/reference/np-dhf-001-revision-history.md");
    check(res.status === "done" && /^\*\*Revision:\*\* 4$/m.test(sw) && /\| 4 \| 2026-11-02 \| A \| \*\*next\*\* \|/.test(sw), "a Revision line behind its history takes the next number after the history's newest row (3 -> 4, not 2 -> 3)");
    check(/\*\*3 → 4: healed\./.test(dh) && /\| NP-SW-001 \| SW \| 4 \|/.test(read(heal, DHF_FILE)), "and the text and the DHF index follow it");
  } finally { rmSync(heal, { recursive: true, force: true }); }

  // A document with no history table anywhere: the assigner starts one.
  const nt = mk();
  try {
    unlinkSync(join(nt, "docs/reference/np-fw-hub-001-revision-history.md"));
    commit(nt, "base");
    frag(nt, "n.md", "date: 2026-11-02\n## DHF\nauthor: A\ntext: **{NP-FW-HUB-001 from → to}: started.**\n## NP-FW-HUB-001\nauthor: A\ntext: **first recorded row**\n");
    commit(nt, "fragment");
    const res = assignNext(nt, false);
    const fw = read(nt, "docs/np_fw_hub_001.md");
    check(res.status === "done" && /^\*\*Revision:\*\* 2$/m.test(fw), "a document with no history table is bumped from its Revision line (1 -> 2)");
    check(/\n## Revision history\n\nRevisions before the first row below were not recorded[^\n]*\n\n\| Rev \| Date \| Author \| Description \|\n\|---\|---\|---\|---\|\n\| 2 \| 2026-11-02 \| A \| \*\*first recorded row\*\* \|\n$/.test(fw), "and gets a history table at its end holding the row");
    check(/> \*\*Rev 2 \(2026-11-02\) — the callout\.\*\*/.test(fw), "with its own tokens resolved as before");
    const nt2 = read(nt, DHF_FILE);
    check(/\| NP-FW-HUB-001 \| HUB \| 2 \| 2026-11-02 \|/.test(nt2), "and its DHF index row follows");
    // The next change appends to the table the first one started.
    frag(nt, "m.md", "date: 2026-11-03\n## DHF\nauthor: B\ntext: **{NP-FW-HUB-001 from → to}: again.**\n## NP-FW-HUB-001\nauthor: B\ntext: **second row**\n");
    commit(nt, "fragment 2");
    assignNext(nt, false);
    const fw2 = read(nt, "docs/np_fw_hub_001.md");
    check((fw2.match(/## Revision history/g) ?? []).length === 1 && /\| 2 \|[^\n]*\n\| 3 \| 2026-11-03 \| B \| \*\*second row\*\* \|\n$/.test(fw2), "the next change appends to that table, and no second table is started");
  } finally { rmSync(nt, { recursive: true, force: true }); }

  // Converting a hand-numbered branch gives back the same numbers when assigned.
  const cv = mk();
  try {
    commit(cv, "base");
    git(cv, "checkout", "-q", "-b", "claude/topic");
    const w = (f: string, t: string): void => writeFileSync(join(cv, f), t);
    w("docs/np_dhf_001.md", idx(3, 3, 1).replace("| NP-SW-001 | SW | 3 | 2026-10-01 |", "| NP-SW-001 | SW | 3 | 2026-11-02 |").replace("| NP-DHF-001 | DHF | 3 | 2026-10-01 |", "| NP-DHF-001 | DHF | 3 | 2026-11-02 |").replace("**Date:** 2026-10-01", "**Date:** 2026-11-02"));
    w("docs/reference/np-dhf-001-revision-history.md", hist([row(1, "a"), row(2, "b"), "| 3 | 2026-11-02 | Eng A | **NP-SW-001 Rev 2 → 3: the change.** |"]));
    w("docs/np_sw_001.md", doc("NP-SW-001", 3, [row(1, "a"), row(2, "b"), "| 3 | 2026-11-02 | Eng A | **the change** |"]).replace("**Date:** 2026-10-01", "**Date:** 2026-11-02"));
    commit(cv, "hand-numbered");
    const hand = [DHF_FILE, "docs/np_sw_001.md", "docs/reference/np-dhf-001-revision-history.md"].map((f) => read(cv, f));
    const out = convertBranch(cv, "main");
    const conv = read(cv, out);
    check(/## DHF\nauthor: Eng A\ntext: \*\*NP-SW-001 Rev \{NP-SW-001 from → to\}: the change/.test(conv), "converting turns the numbers in the text into tokens");
    check(/^\*\*Revision:\*\* 2$/m.test(read(cv, DHF_FILE)) && !/\| 3 \|/.test(read(cv, "docs/np_sw_001.md")), "and puts back the base's revisions and removes the hand-added rows");
    // The fixture's hand-numbered files carry 2026-11-02; a fragment with no date takes today's, so pin it.
    writeFileSync(join(cv, out), `date: 2026-11-02\n${conv}`);
    const res = assignNext(cv, false);
    const again = [DHF_FILE, "docs/np_sw_001.md", "docs/reference/np-dhf-001-revision-history.md"].map((f) => read(cv, f));
    check(res.status === "done" && again.every((t, i) => t === hand[i]), "assigning the converted fragment reproduces the hand-numbered files exactly");
  } finally { rmSync(cv, { recursive: true, force: true }); }

  // The parser refuses what would write a bad row.
  check(typeof parseFragment("f.md", "## DHF\nauthor: A\ntext: a | b\n") === "string", "a '|' in the text is refused");
  check(typeof parseFragment("f.md", "## NP-SW-001\nauthor: A\ntext: t\n") === "string", "a fragment without a DHF section is refused");
  check(typeof parseFragment("f.md", "## DHF\nauthor: A\ntext: {NP-SW-001 to}\n") === "string", "a token naming a document with no section is refused");
  check(typeof parseFragment("f.md", "## NP-DHF-001\nauthor: A\ntext: t\n") === "string", "the DHF is named DHF, not by serial");
  check(typeof parseFragment("f.md", "## DHF\nauthor: A\ntext: t\n") !== "string", "a minimal fragment parses");
  void statSync; void serialOf;

  if (failures > 0) { console.error(`\nFAILED (${failures})`); process.exit(1); }
  console.log("\nALL PASS");
}

if (import.meta.main) {
  const a = process.argv.slice(2);
  try {
    if (a.includes("--self-test")) selfTest();
    else if (a.includes("--mode")) console.log(readMode(process.cwd()));
    else if (a.includes("--peek")) {
      const p = pendingFragments(process.cwd());
      if (p.length === 0) process.exit(3);
      console.log(p[0]);
    } else if (a.includes("--next")) {
      const r = assignNext(process.cwd());
      if (r.status === "none") process.exit(3);
      console.log(`assigned ${r.fragment}: ${JSON.stringify(r.revisions)}`);
    } else if (a.includes("--convert")) {
      const i = a.indexOf("--base");
      console.log(`wrote ${convertBranch(process.cwd(), i >= 0 ? a[i + 1] : "origin/main")}`);
    } else { console.error("usage: assign-revisions.ts --mode | --peek | --next | --convert [--base ref] | --self-test"); process.exit(1); }
  } catch (e) {
    console.error(String((e as Error).message));
    process.exit(1);
  }
}
