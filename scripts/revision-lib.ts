/**
 * revision-lib.ts — the parts of the revision scheme that assign-revisions.ts and check-revision-fragments.ts share.
 *
 * Not a gate and not run on its own. The scheme: docs/revisions/README.md.
 */
import { execFileSync } from "child_process";
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
export interface Fragment { path: string; date: string | null; sections: Section[]; log: string[]; claude: string[] }

const TOKEN = new RegExp(String.raw`\{(DHF|CLAUDE\.md|${SERIAL}) (from → to|from|to)\}`, "g");

/** Parse a fragment, or return the reason it is not one. */
export function parseFragment(path: string, text: string): Fragment | string {
  const frag: Fragment = { path, date: null, sections: [], log: [], claude: [] };
  let cur = null as { target: string; author?: string; text?: string } | null;
  let inLog = false, sawLog = false, inClaude = false, sawClaude = false;
  const close = (): string | null => {
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
  if (frag.sections.length === 0 && frag.log.length === 0 && frag.claude.length === 0) return `${path}: no sections`;
  // A fragment that only logs a decision bumps nothing; one that names any document, or CLAUDE.md (whose DHF index row
  // it changes), bumps the DHF once.
  if ((frag.sections.length > 0 || frag.claude.length > 0) && !frag.sections.some((s) => s.target === "DHF")) return `${path}: no "## DHF" section (every bump bumps the DHF once)`;
  const targets = new Set(frag.sections.map((s) => s.target));
  if (frag.claude.length > 0) targets.add(CLAUDE_TARGET);
  const texts = [
    ...frag.sections.map((s) => ({ where: s.target, text: s.text })),
    ...frag.log.map((text) => ({ where: "LOG", text })),
    ...frag.claude.map((text) => ({ where: CLAUDE_TARGET, text })),
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
