/**
 * revision-lib.ts — the parts of the revision scheme that assign-revisions.ts and check-revision-fragments.ts share.
 *
 * Not a gate and not run on its own. The scheme: docs/revisions/README.md.
 */
import { execFileSync } from "child_process";
import { existsSync, readdirSync, readFileSync } from "fs";
import { join } from "path";

export const DATE = String.raw`\d{4}-\d{2}-\d{2}`;
export const HISTORY_ROW = new RegExp(String.raw`^\| (\d+) \| (${DATE}) \| ([^|]*) \| (.*) \|\s*$`);
export const REV_LINE = /^\*\*Revision:\*\*\s*(\d+)\s*$/;
export const DATE_LINE = /^\*\*Date:\*\*\s*\d{4}-\d{2}-\d{2}/;
export const SERIAL = String.raw`NP-[A-Z0-9-]+`;
export const DHF_SERIAL = "NP-DHF-001";
export const DHF_FILE = "docs/np_dhf_001.md";
export const PENDING_DIR = "docs/revisions/pending";
export const POLICY_FILE = "docs/revisions/policy.json";

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
export interface Fragment { path: string; date: string | null; sections: Section[] }

const TOKEN = new RegExp(String.raw`\{(DHF|${SERIAL}) (from → to|from|to)\}`, "g");

/** Parse a fragment, or return the reason it is not one. */
export function parseFragment(path: string, text: string): Fragment | string {
  const frag: Fragment = { path, date: null, sections: [] };
  let cur = null as { target: string; author?: string; text?: string } | null;
  const close = (): string | null => {
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
  if (frag.sections.length === 0) return `${path}: no sections`;
  if (!frag.sections.some((s) => s.target === "DHF")) return `${path}: no "## DHF" section (every bump bumps the DHF once)`;
  const targets = new Set(frag.sections.map((s) => s.target));
  for (const s of frag.sections) {
    for (const m of s.text.matchAll(TOKEN)) {
      if (!targets.has(m[1])) return `${path}: the text of ${s.target} uses {${m[1]} ${m[2]}} but ${m[1]} has no section`;
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
