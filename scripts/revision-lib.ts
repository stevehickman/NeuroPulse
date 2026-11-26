/**
 * revision-lib.ts — the parts of the revision scheme that assign-revisions.ts and check-revision-fragments.ts share.
 *
 * Not a gate and not run on its own. The scheme: docs/revisions/README.md.
 */
import { execFileSync } from "child_process";
import { createHash } from "crypto";
import { existsSync, readdirSync, readFileSync } from "fs";
import { join } from "path";

export const DATE = String.raw`\d{4}-\d{2}-\d{2}`;
/**
 * A history row: `| Rev | Date | Author | Description |`, or `| Rev | Date | Description |` when the document's table has
 * no Author column (NP-FW-EMMC-002). Group 1 is the revision, 2 the date, 3 the author (undefined when the table has none), 4 the description. The Rev and Date cells
 * may be bold (the newest row of NP-FW-EMMC-002 was), and a Date cell may carry a note (`2026-06-02 (§G: 2026-06-03)`).
 */
export const HISTORY_ROW = new RegExp(String.raw`^\| (?:\*\*)?(\d+)(?:\*\*)? \| (?:\*\*)?(${DATE})(?:\*\*)?(?: \([^)|]*\))? \| (?:([^|]*) \| )?(.*) \|\s*$`);
export const REV_LINE = /^\*\*Revision:\*\*\s*(\d+)\s*$/;
export const DATE_LINE = /^\*\*Date:\*\*\s*\d{4}-\d{2}-\d{2}/;
export const SERIAL = String.raw`NP-[A-Z0-9-]+`;
export const DHF_SERIAL = "NP-DHF-001";
export const DHF_FILE = "docs/np_dhf_001.md";
export const PENDING_DIR = "docs/revisions/pending";
export const POLICY_FILE = "docs/revisions/policy.json";
/**
 * The append-only decision log. In mode assign-at-merge a PR does not append to it: its fragment's `## LOG` section
 * holds the entry and the workflow appends it when the PR merges, so two PRs never both add to the file's last line.
 */
export const DECISIONS_FILE = "docs/status/completed-decisions.md";
/**
 * CLAUDE.md is not a controlled document under docs/ (no `**Document:**` serial), but its revision, its row in the DHF
 * index and its revision history collided the same way. A fragment's `## CLAUDE.md` section holds the history entry
 * (one `text:` line per paragraph); the workflow sets `**Revision:** N (current)`, the DHF row and inserts the entry
 * at the top of the history's "Current revision" section, newest first.
 */
export const CLAUDE_TARGET = "CLAUDE.md";
export const CLAUDE_FILE = "CLAUDE.md";
export const CLAUDE_HISTORY = "docs/reference/claude-md-revision-history.md";
export const CLAUDE_REV_LINE = /^\*\*Revision:\*\*\s*(\d+) \(current\)/m;
/** A history entry's first paragraph: `**Rev 86 (2026-11-21) — …`. */
export const CLAUDE_ENTRY = /^\*\*Rev (\d+) \(\d{4}-\d{2}-\d{2}\)/;
/** The DHF index row for CLAUDE.md: its first cell is "—", as it has no serial. */
export const CLAUDE_DHF_ROW = new RegExp(String.raw`^\| — \| CLAUDE\.md — Project Design Memory \| (\d+) \| (${DATE}) \|`);
/** What the first paragraph of a `## CLAUDE.md` section starts with, before its tokens are resolved. */
export const CLAUDE_ENTRY_HEAD = "**Rev {CLAUDE.md to} ({date}) — ";
/**
 * The open-items status page. It is edited in place, not appended, and PRs that changed neighbouring items (one item
 * per line) conflicted, because git refuses adjacent changed lines. In mode assign-at-merge a PR leaves it unchanged and
 * its fragment carries the edit as `## OPEN` hunks, which the workflow applies in place at merge. A hunk is found by
 * the content of the lines it replaces, never by line number or by its neighbours, so an edit to the next item does not
 * disturb it. `bun scripts/assign-revisions.ts --convert-open` writes the hunks from an ordinary edit of the file.
 */
export const OPEN_FILE = "docs/status/pending-decisions.md";
/**
 * One edit of OPEN_FILE. Each anchor is `n` consecutive lines whose text, joined by newlines, hashes to `hash`, and it
 * must occur exactly once in the file when the edit is applied. `was`: replace those lines with `lines` (none: delete
 * them). Otherwise insert `lines` right after the `after` lines or, if those are gone (another PR edited them), right
 * before the `before` lines: an insertion has one or both.
 */
export interface Anchor { hash: string; n: number }
export interface Hunk { label: string; was?: Anchor; after?: Anchor; before?: Anchor; lines: string[] }
export const lineHash = (t: string): string => createHash("sha256").update(t).digest("hex").slice(0, 16);

/** Apply `hunks` to `text` in order, or throw naming the hunk that does not apply (nothing is half-applied: the input is not mutated). */
export function applyOpen(text: string, hunks: Hunk[], where: string): string {
  const lines = text.split("\n");
  for (const h of hunks) {
    const fail = (why: string): never => {
      throw new Error(`${where}: the ${OPEN_FILE} edit "${h.label}" ${why}; it was written against an older ${OPEN_FILE}. Merge main, run \`bun scripts/assign-revisions.ts --expand-open\`, redo the edit, and run --convert-open again`);
    };
    const find = (a: Anchor): number[] => {
      const at: number[] = [];
      for (let i = 0; i + a.n <= lines.length; i++) if (lineHash(lines.slice(i, i + a.n).join("\n")) === a.hash) at.push(i);
      return at;
    };
    if (h.was) {
      const at = find(h.was);
      if (at.length !== 1) fail(at.length === 0 ? "replaces lines that are no longer there" : `replaces lines that occur ${at.length} times`);
      lines.splice(at[0], h.was.n, ...h.lines);
    } else {
      const after = h.after ? find(h.after) : [], before = h.before ? find(h.before) : [];
      if (after.length === 1) lines.splice(after[0] + (h.after as Anchor).n, 0, ...h.lines);
      else if (before.length === 1) lines.splice(before[0], 0, ...h.lines);
      else fail("inserts next to lines that are no longer there, on either side");
    }
  }
  return lines.join("\n");
}

/** `{date}` in a LOG entry becomes the date the fragment is assigned (its `date:` line, else the merge date). */
export const DATE_TOKEN = "{date}";

export type Mode = "hand" | "assign-at-merge";

export function readMode(root: string): Mode {
  const p = join(root, POLICY_FILE);
  if (!existsSync(p)) return "hand";
  const m = (JSON.parse(readFileSync(p, "utf8")) as { mode?: string }).mode;
  if (m !== "hand" && m !== "assign-at-merge") throw new Error(`${POLICY_FILE}: mode must be "hand" or "assign-at-merge"`);
  return m;
}

export function git(cwd: string, ...args: string[]): string {
  return execFileSync("git", args, { cwd, encoding: "utf8", stdio: ["ignore", "pipe", "pipe"] });
}
export function show(cwd: string, ref: string, path: string): string | null {
  try { return git(cwd, "show", `${ref}:${path}`); } catch { return null; }
}

/** The serial a controlled document carries, and its revision, from its text. */
export const serialOf = (t: string): string | null => /^\*\*Document:\*\*\s*(NP-[A-Z0-9-]+)/m.exec(t)?.[1] ?? null;
export const revOf = (t: string): number | null => {
  const m = /^\*\*Revision:\*\*\s*(\d+)/m.exec(t);
  return m ? Number(m[1]) : null;
};

/** Every controlled document under docs/: relative path → serial. */
export function controlledDocs(root: string): Map<string, string> {
  const out = new Map<string, string>();
  for (const f of readdirSync(join(root, "docs"))) {
    if (!/^np_[a-z0-9_]+\.md$/.test(f)) continue;
    const text = readFileSync(join(root, "docs", f), "utf8");
    const s = serialOf(text);
    if (s !== null && revOf(text) !== null) out.set(`docs/${f}`, s);
  }
  return out;
}

/** The file that holds a document's history table: its linked history file if it has one, else the document. */
export function historyFile(root: string, serial: string, docFile: string): string {
  const linked = `docs/reference/${serial.toLowerCase()}-revision-history.md`;
  return existsSync(join(root, linked)) ? linked : docFile;
}

export interface Section { target: string; author: string; text: string }
/** `log`: the entries of the fragment's `## LOG` section, one per `text:` line, in order, without the leading "- ". */
/** `claude`: the paragraphs of the fragment's `## CLAUDE.md` section (the CLAUDE.md history entry), or empty. */
export interface Fragment { path: string; date: string | null; sections: Section[]; log: string[]; claude: string[]; open: Hunk[] }

const TOKEN = new RegExp(String.raw`\{(DHF|CLAUDE\.md|${SERIAL}) (from → to|from|to)\}`, "g");

/** Parse a fragment, or return the reason it is not one. */
export function parseFragment(path: string, text: string): Fragment | string {
  const frag: Fragment = { path, date: null, sections: [], log: [], claude: [], open: [] };
  let hunk = null as Hunk | null;
  let cur = null as { target: string; author?: string; text?: string } | null;
  let inLog = false, sawLog = false, inClaude = false, sawClaude = false;
  const close = (): string | null => {
    if (hunk !== null) {
      const h = hunk;
      hunk = null;
      if (h.was ? h.after || h.before : !h.after && !h.before) return `${path}: section OPEN ${h.label} needs "was:", or "after:" and/or "before:"`;
      if (!h.was && h.lines.length === 0) return `${path}: section OPEN ${h.label} inserts no "text:" line`;
      frag.open.push(h);
      return null;
    }
    if (inLog) {
      inLog = false;
      return frag.log.length === 0 ? `${path}: section LOG has no "text:" line` : null;
    }
    if (inClaude) {
      inClaude = false;
      if (frag.claude.length === 0) return `${path}: section CLAUDE.md has no "text:" line`;
      if (!frag.claude[0].startsWith(CLAUDE_ENTRY_HEAD)) return `${path}: section CLAUDE.md: its first "text:" must start "${CLAUDE_ENTRY_HEAD}"`;
      return null;
    }
    if (cur === null) return null;
    if (!cur.author) return `${path}: section ${cur.target} has no "author:" line`;
    if (!cur.text) return `${path}: section ${cur.target} has no "text:" line`;
    if (cur.text.includes("|")) return `${path}: section ${cur.target}: the text holds a "|", which would split the table row`;
    frag.sections.push({ target: cur.target, author: cur.author, text: cur.text });
    return null;
  };
  for (const raw of text.split("\n")) {
    const line = raw.trimEnd();
    const openHead = /^## OPEN(?: (.+))?$/.exec(line);
    if (openHead) {
      const e = close();
      if (e) return e;
      cur = null;
      hunk = { label: openHead[1] ?? "edit", lines: [] };
      continue;
    }
    if (hunk !== null && !line.startsWith("## ")) {
      // A line of the page is taken exactly as written after "text: ", trailing spaces and "|" included; "text:" alone
      // is a blank line.
      if (raw === "text:" || raw.startsWith("text: ")) { hunk.lines.push(raw.slice(6)); continue; }
      if (line === "") continue;
      const m = /^(was|after|before): ([0-9a-f]{16})(?: (\d+))?$/.exec(line);
      if (!m) return `${path}: section OPEN ${hunk.label}: cannot read the line "${line.slice(0, 60)}"`;
      if (!m[3] || Number(m[3]) < 1) return `${path}: section OPEN ${hunk.label}: "${m[1]}:" takes the hash and the number of lines`;
      if (hunk[m[1] as "was" | "after" | "before"]) return `${path}: section OPEN ${hunk.label}: "${m[1]}:" appears twice`;
      hunk[m[1] as "was" | "after" | "before"] = { hash: m[2], n: Number(m[3]) };
      continue;
    }
    if (line === "" || line.startsWith("#") && !line.startsWith("## ")) continue;
    if (line === "## LOG") {
      const e = close();
      if (e) return e;
      if (sawLog) return `${path}: section LOG appears twice`;
      cur = null; inLog = true; sawLog = true;
      continue;
    }
    if (line === `## ${CLAUDE_TARGET}`) {
      const e = close();
      if (e) return e;
      if (sawClaude) return `${path}: section ${CLAUDE_TARGET} appears twice`;
      cur = null; inClaude = true; sawClaude = true;
      continue;
    }
    const head = /^## (DHF|NP-[A-Z0-9-]+)$/.exec(line);
    if (head) {
      const e = close();
      if (e) return e;
      if (head[1] === DHF_SERIAL) return `${path}: name the DHF as "## DHF", not by its serial`;
      if (frag.sections.some((s) => s.target === head[1]) || cur?.target === head[1]) return `${path}: section ${head[1]} appears twice`;
      cur = { target: head[1] };
      continue;
    }
    const kv = /^(date|author|text): (.+)$/.exec(line);
    if (!kv) return `${path}: cannot read the line "${line.slice(0, 60)}"`;
    if (inLog) {
      // A LOG entry is a list item, not a table row, so it may hold a "|"; it has no author (git records the PR's).
      if (kv[1] !== "text") return `${path}: section LOG takes only "text:" lines (one per entry)`;
      frag.log.push(kv[2].replace(/^- +/, ""));
      continue;
    }
    if (inClaude) {
      // A paragraph of the history entry: prose, not a table row, so it may hold a "|"; the history has no author.
      if (kv[1] !== "text") return `${path}: section ${CLAUDE_TARGET} takes only "text:" lines (one per paragraph)`;
      frag.claude.push(kv[2]);
      continue;
    }
    if (cur === null) {
      if (kv[1] !== "date") return `${path}: "${kv[1]}:" before the first section`;
      if (!new RegExp(`^${DATE}$`).test(kv[2])) return `${path}: date must be YYYY-MM-DD`;
      frag.date = kv[2];
    } else if (kv[1] === "author") cur.author = kv[2];
    else if (kv[1] === "text") cur.text = kv[2];
    else return `${path}: "date:" belongs before the first section`;
  }
  const e = close();
  if (e) return e;
  if (frag.sections.length === 0 && frag.log.length === 0 && frag.claude.length === 0 && frag.open.length === 0) return `${path}: no sections`;
  // A fragment that only logs a decision bumps nothing; one that names any document, or CLAUDE.md (whose DHF index row
  // it changes), bumps the DHF once.
  if ((frag.sections.length > 0 || frag.claude.length > 0) && !frag.sections.some((s) => s.target === "DHF")) return `${path}: no "## DHF" section (every bump bumps the DHF once)`;
  const targets = new Set(frag.sections.map((s) => s.target));
  if (frag.claude.length > 0) targets.add(CLAUDE_TARGET);
  const texts = [
    ...frag.sections.map((s) => ({ where: s.target, text: s.text })),
    ...frag.log.map((text) => ({ where: "LOG", text })),
    ...frag.claude.map((text) => ({ where: CLAUDE_TARGET, text })),
    ...frag.open.flatMap((h) => h.lines.map((text) => ({ where: `OPEN ${h.label}`, text }))),
  ];
  for (const s of texts) {
    for (const m of s.text.matchAll(TOKEN)) {
      if (!targets.has(m[1])) return `${path}: the text of ${s.where} uses {${m[1]} ${m[2]}} but ${m[1]} has no section`;
    }
  }
  return frag;
}

/** Replace {X from}, {X to} and {X from → to} with the numbers assigned (numbers only: "Rev {X from → to}" reads "Rev 141 → 142"). */
export function resolveTokens(text: string, revs: Map<string, { from: number; to: number }>): string {
  return text.replace(TOKEN, (_m, who: string, what: string) => {
    const r = revs.get(who);
    if (!r) throw new Error(`no revision assigned to ${who}`);
    return what === "from" ? String(r.from) : what === "to" ? String(r.to) : `${r.from} → ${r.to}`;
  });
}

export const hasTokens = (text: string): boolean => new RegExp(TOKEN.source).test(text);

/** The `- ` list items of the decision log, one per entry (an entry is one line). */
export const logEntries = (t: string | null): string[] => (t ?? "").split("\n").filter((l) => l.startsWith("- "));

/** The fragments waiting under docs/revisions/pending/, oldest first by the commit that added each (ties by name). */
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
