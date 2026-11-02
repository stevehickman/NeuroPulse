#!/usr/bin/env bun
/**
 * Verify that a pull request follows the revision scheme in force (docs/revisions/policy.json).
 *
 * The decision: docs/human-input/np-qms-dc-001-revision-assignment.md (option A, the principal, 2026-11-02). Hand-
 * assigned revision numbers collided whenever two sessions worked in parallel (125 commits touched the DHF in
 * two weeks). In mode `assign-at-merge` a PR writes no revision number; it adds one fragment
 * (docs/revisions/pending/<slug>.md) and the assign-revisions workflow writes the numbers when the PR merges
 * (scripts/assign-revisions.ts). This gate holds the PR to that. In mode `hand` (the mode the repository ships in,
 * until the workflow's GitHub App secrets exist) it checks only that any fragment present can be read.
 *
 * What it checks, against the PR's own changes (the diff from the base, HEAD^1 on a pull_request merge checkout):
 *
 *   F. every fragment under docs/revisions/pending/ parses (assign-revisions.ts would accept it);
 *   G. a PR adds at most one fragment (one DHF bump per PR);
 *   H. a PR does not write an assigned number: it changes no `**Revision:**` of an existing controlled document, adds no
 *      history row (a row whose number the base lacked) to a document or to a docs/reference/*-revision-history.md, and
 *      changes no `Rev` cell of the NP-DHF-001 index. New documents (absent from the base) start at their first revision
 *      by hand, and the text of an existing history row may be corrected;
 *   J. every existing controlled document whose text changed is named by a `## <serial>` section, and the DHF, if it
 *      changed or any other section exists, by `## DHF` (the principal, 2026-11-02: a change without a bump fails).
 *
 * Only for pull requests: on `main` the assign-revisions workflow's own commits change exactly what H forbids.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-revision-fragments.ts --self-test
 * CI-Self-Test-Reads-Tree: imports scripts/revision-lib.ts, shared with assign-revisions.ts, which a lone copy of this file does not have; every fixture is built in a temporary repository
 * CI-Scans: every controlled document under docs/ and every fragment under docs/revisions/pending/
 * CI-Scan-Paths: docs/**
 */
import { mkdirSync, mkdtempSync, readdirSync, rmSync, writeFileSync } from "fs";
import { existsSync, readFileSync } from "fs";
import { tmpdir } from "os";
import { dirname, join } from "path";
import {
  DHF_FILE, DHF_SERIAL, HISTORY_ROW, PENDING_DIR, REV_LINE, SERIAL, DATE,
  controlledDocs, git, parseFragment, readMode, revOf, show, type Fragment,
} from "./revision-lib";

const INDEX_REV = new RegExp(String.raw`^\| (${SERIAL})(?: \([^)]*\))? \|.*\| (\d+) \| ${DATE} \| \[`);

const historyNumbers = (t: string | null): Set<number> => {
  const s = new Set<number>();
  for (const l of (t ?? "").split("\n")) { const m = HISTORY_ROW.exec(l); if (m) s.add(Number(m[1])); }
  return s;
};
const indexRevs = (t: string | null): Map<string, string> => {
  const m = new Map<string, string>();
  for (const l of (t ?? "").split("\n")) {
    if (!INDEX_REV.test(l)) continue;
    const c = l.split("|");
    m.set(c[1].trim(), l);
  }
  return m;
};
const revCell = (l: string): string => /\| (\d+) \| \d{4}-\d{2}-\d{2} \| \[/.exec(l)?.[1] ?? "";

export interface Verdict { violations: string[]; documents: number; fragments: number }

export function evaluate(root: string, base: string | null): Verdict {
  const v: string[] = [];
  const docs = controlledDocs(root);
  const dir = join(root, PENDING_DIR);
  const fragFiles = existsSync(dir) ? readdirSync(dir).filter((f) => f.endsWith(".md") && f.toLowerCase() !== "readme.md") : [];
  const frags: Fragment[] = [];
  for (const f of fragFiles) {
    const p = parseFragment(`${PENDING_DIR}/${f}`, readFileSync(join(dir, f), "utf8"));
    if (typeof p === "string") v.push(`F: ${p}`); else frags.push(p);
  }
  const verdict = (): Verdict => ({ violations: v, documents: docs.size, fragments: fragFiles.length });
  if (readMode(root) === "hand" || base === null) return verdict();

  const changed = git(root, "diff", "--name-only", base, "HEAD").split("\n").filter(Boolean);
  const addedFrags = changed.filter((f) => f.startsWith(`${PENDING_DIR}/`) && f.endsWith(".md") && f.toLowerCase() !== `${PENDING_DIR}/readme.md` && existsSync(join(root, f)));
  if (addedFrags.length > 1) v.push(`G: this PR adds ${addedFrags.length} fragments (${addedFrags.join(", ")}); one DHF bump per PR means one fragment`);
  const named = new Set<string>();
  for (const f of frags) if (addedFrags.includes(f.path)) for (const s of f.sections) named.add(s.target);

  for (const [file, serial] of docs) {
    const b = show(root, base, file);
    if (b === null) continue;                                     // a new document: first revision by hand
    const h = readFileSync(join(root, file), "utf8");
    if (b === h) continue;
    const rb = revOf(b), rh = revOf(h);
    if (rb !== rh) v.push(`H: ${file} changes its **Revision:** from ${rb} to ${rh}; the workflow assigns it (docs/revisions/README.md)`);
    const newRows = [...historyNumbers(h)].filter((n) => !historyNumbers(b).has(n));
    if (newRows.length > 0) v.push(`H: ${file} adds history row(s) ${newRows.join(", ")}; the workflow writes them from the fragment`);
    const target = serial === DHF_SERIAL ? "DHF" : serial;
    if (file === DHF_FILE) {
      const bi = indexRevs(b), hi = indexRevs(h);
      for (const [s, line] of hi) {
        const old = bi.get(s);
        if (old !== undefined && revCell(old) !== revCell(line)) v.push(`H: ${DHF_FILE} changes the Rev cell of ${s} from ${revCell(old)} to ${revCell(line)}; the workflow assigns it`);
      }
    }
    if (!named.has(target)) v.push(`J: ${file} changed but no fragment of this PR has a "## ${target}" section: add docs/revisions/pending/<slug>.md`);
  }
  for (const f of changed) {
    const m = /^docs\/reference\/(np-[a-z0-9-]+)-revision-history\.md$/.exec(f);
    if (!m || !existsSync(join(root, f))) continue;
    const b = show(root, base, f);
    const newRows = [...historyNumbers(readFileSync(join(root, f), "utf8"))].filter((n) => !historyNumbers(b).has(n));
    if (newRows.length > 0) v.push(`H: ${f} adds history row(s) ${newRows.join(", ")}; the workflow writes them from the fragment`);
  }
  if (named.size > 0 && !named.has("DHF")) v.push(`J: the fragment has no "## DHF" section`);
  return verdict();
}

// ── Self-test ────────────────────────────────────────────────────────────────

function selfTest(): void {
  let failures = 0;
  const check = (ok: boolean, msg: string): void => { if (!ok) { failures++; console.error(`FAIL: ${msg}`); } else console.log(`PASS: ${msg}`); };
  const row = (n: number): string => `| ${n} | 2026-10-0${Math.min(n, 9)} | Eng | text ${n} |`;
  const hist = (n: number): string => `| Rev | Date | Author | Description |\n|---|---|---|---|\n${Array.from({ length: n }, (_, i) => row(i + 1)).join("\n")}\n`;
  const doc = (rev: number, body: string): string => `**Document:** NP-SW-001\n**Revision:** ${rev}\n**Date:** 2026-10-01\n\n${body}\n\n${hist(rev)}`;
  const dhf = (dr: number, sr: number): string =>
    `**Document:** NP-DHF-001\n**Revision:** ${dr}\n**Date:** 2026-10-01\n\n| Serial | Title | Rev | Date | File | Status | Owner |\n|---|---|---|---|---|---|---|\n` +
    `| NP-DHF-001 | DHF | ${dr} | 2026-10-01 | [a](./np_dhf_001.md) | ACTIVE | QMS |\n| NP-SW-001 | SW | ${sr} | 2026-10-01 | [b](./np_sw_001.md) | ACTIVE | QMS |\n`;
  const frag = "## DHF\nauthor: A\ntext: **{NP-SW-001 from → to}: x.**\n## NP-SW-001\nauthor: A\ntext: **x**\n";

  /** Build a repo whose main is the base, then a branch with the edit `f` applied and committed. */
  const scenario = (mode: string, f: (w: (p: string, t: string) => void, r: string) => void): Verdict => {
    const r = mkdtempSync(join(tmpdir(), "revfrag-"));
    try {
      const w = (p: string, t: string): void => { mkdirSync(dirname(join(r, p)), { recursive: true }); writeFileSync(join(r, p), t); };
      const c = (m: string): void => { git(r, "add", "-A"); git(r, "-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "-m", m); };
      git(r, "init", "-q", "-b", "main");
      w("docs/np_sw_001.md", doc(2, "body"));
      w("docs/np_dhf_001.md", dhf(2, 2));
      w("docs/reference/np-dhf-001-revision-history.md", hist(2));
      w("docs/revisions/policy.json", JSON.stringify({ mode }));
      c("base");
      git(r, "checkout", "-q", "-b", "topic");
      f(w, r);
      c("topic");
      return evaluate(r, "main");
    } finally { rmSync(r, { recursive: true, force: true }); }
  };
  const codes = (x: Verdict): string => x.violations.map((s) => s[0]).sort().join("");
  const A = "assign-at-merge";

  check(codes(scenario(A, (w) => { w("docs/np_sw_001.md", doc(2, "new body")); w(`${PENDING_DIR}/x.md`, frag); })) === "", "a content change with its fragment passes");
  check(codes(scenario(A, (w) => { w("docs/np_sw_001.md", doc(2, "new body")); w(`${PENDING_DIR}/x.md`, frag); })) === "" &&
        scenario(A, (w) => { w("docs/np_sw_001.md", doc(2, "new body")); w(`${PENDING_DIR}/x.md`, frag); }).fragments === 1, "and the gate reports the population it read");
  check(codes(scenario(A, (w) => { w("docs/np_sw_001.md", doc(2, "new body")); })).includes("J"), "a controlled document changed with no fragment fails (J)");
  check(codes(scenario(A, (w) => { w("docs/np_sw_001.md", doc(3, "new body")); w(`${PENDING_DIR}/x.md`, frag); })).includes("H"), "a hand-bumped Revision fails (H)");
  check(scenario(A, (w) => { w("docs/np_sw_001.md", doc(2, "body").replace("**Revision:** 2", "**Revision:** 3")); w(`${PENDING_DIR}/x.md`, frag); }).violations.some((s) => /changes its \*\*Revision/.test(s)), "a Revision line changed on its own fails (H)");
  check(scenario(A, (w) => { w("docs/np_sw_001.md", doc(3, "body")); w(`${PENDING_DIR}/x.md`, frag); }).violations.some((s) => /adds history row/.test(s)), "a hand-added history row fails (H)");
  check(codes(scenario(A, (w) => { w("docs/np_dhf_001.md", dhf(2, 3)); w(`${PENDING_DIR}/x.md`, frag); })).includes("H"), "a hand-changed DHF Rev cell fails (H)");
  check(codes(scenario(A, (w) => { w("docs/reference/np-dhf-001-revision-history.md", hist(3)); w(`${PENDING_DIR}/x.md`, frag); })).includes("H"), "a hand-added DHF history row fails (H)");
  check(codes(scenario(A, (w) => { w("docs/np_sw_001.md", doc(2, "new body")); w(`${PENDING_DIR}/x.md`, frag); w(`${PENDING_DIR}/y.md`, frag); })).includes("G"), "two fragments fail (G)");
  check(codes(scenario(A, (w) => { w("docs/np_sw_001.md", doc(2, "new body")); w(`${PENDING_DIR}/x.md`, "## DHF\nauthor: A\ntext: a | b\n"); })).includes("F"), "a fragment that cannot be read fails (F)");
  check(codes(scenario(A, (w) => { w("docs/np_sw_001.md", doc(2, "new body")); w(`${PENDING_DIR}/x.md`, "## DHF\nauthor: A\ntext: t\n"); })).includes("J"), "a changed document the fragment does not name fails (J)");
  check(codes(scenario(A, (w) => { w("docs/np_dhf_001.md", dhf(2, 2).replace("DHF |", "DHF index |")); })).includes("J"), "a hand edit to the DHF with no fragment fails (J)");
  check(codes(scenario(A, (w) => { w("docs/new.txt", "x"); })) === "", "a PR that touches no controlled document passes");
  check(codes(scenario(A, (w) => { w("docs/np_new_001.md", "**Document:** NP-NEW-001\n**Revision:** 1\n"); })) === "", "a new document starts at its first revision by hand");
  check(codes(scenario("hand", (w) => { w("docs/np_sw_001.md", doc(3, "new body")); })) === "", "in mode hand a hand-numbered change passes");
  check(codes(scenario("hand", (w) => { w(`${PENDING_DIR}/x.md`, "nonsense"); })).includes("F"), "in mode hand a fragment that cannot be read still fails");

  void REV_LINE;
  if (failures > 0) { console.error(`\nFAILED (${failures})`); process.exit(1); }
  console.log("\nALL PASS");
}

if (import.meta.main) {
  const a = process.argv.slice(2);
  if (a.includes("--self-test")) selfTest();
  else {
    const root = process.cwd();
    const bi = a.indexOf("--base");
    let base: string | null = bi >= 0 ? a[bi + 1] : null;
    if (base === null) {
      try { git(root, "rev-parse", "-q", "--verify", "HEAD^2"); base = "HEAD^1"; } catch { base = null; }   // a pull_request merge checkout
    }
    const v = evaluate(root, base);
    console.log(`scanned: ${v.documents} controlled document(s) and ${v.fragments} pending fragment(s), mode ${readMode(root)}, ${base === null ? "no base to compare (fragments read only)" : `against ${base}`}`);
    for (const s of v.violations) console.error(`::error::${s}`);
    if (v.violations.length > 0) { console.error(`\n${v.violations.length} violation(s). Scheme: docs/revisions/README.md`); process.exit(1); }
    console.log("PASS");
  }
}
