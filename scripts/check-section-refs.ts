#!/usr/bin/env bun
/**
 * check-section-refs.ts — section references stay resolvable and machine-parseable.
 *
 * Two rules, both about the same underlying property: a section citation should be
 * something a regex can find and a reader can follow.
 *
 * ── Rule 1: every CLAUDE.md section citation resolves ────────────────────────
 *
 * CLAUDE.md Rev 33 relocated sections 7-15 into docs/status/ and docs/reference/
 * and left every inbound pointer intact. 95 citations in firmware headers, the
 * DHF, and design specs silently stopped resolving, and nothing caught it for a
 * whole revision. Those citations are the regulatory record: a 510(k) reviewer
 * walking the traceability chain lands on nothing.
 *
 * Valid section numbers are re-derived from CLAUDE.md's own top-level headings on
 * every run. This is deliberately NOT a hardcoded list — a hardcoded list would
 * rot exactly the way the references it guards did.
 *
 * Scope: the MAJOR section number only. A citation of subsection 13.4 is valid
 * iff CLAUDE.md still has a section 13. Validating subsections would mean
 * modelling heading depth across nine relocated files for very little signal.
 *
 * ── Rule 2: section markings are written without a space ─────────────────────
 *
 * The canonical form is the section sign immediately followed by the number.
 * The spaced form is what let the original count read 94 instead of 95:
 * docs/ABBREVIATIONS.md wrote one citation with a space and no grep for the
 * section sign followed by a digit could see it. One shape means one regex.
 *
 *   bun scripts/check-section-refs.ts
 *
 * Exits non-zero listing file:line for each violation of either rule.
 */

import { readFileSync, readdirSync, statSync } from "fs";
import { join, relative } from "path";

const ROOT = join(import.meta.dir, "..");
const CLAUDE_MD = join(ROOT, "CLAUDE.md");
const SECTION = "§";

const EXTENSIONS = [
  ".md", ".c", ".h", ".ts", ".tsx", ".swift", ".js", ".npps", ".kt", ".cs", ".yml",
];
const SKIP_DIRS = new Set([
  ".git", "node_modules", "dist", "build", ".next", "out",
  "Pods", ".build", "DerivedData", ".venv", "__pycache__", ".claude",
]);

/** Section numbers CLAUDE.md actually has, from its own `## ` headings. */
function validSections(): Set<number> {
  const text = readFileSync(CLAUDE_MD, "utf8");
  const found = new Set<number>();
  for (const line of text.split("\n")) {
    // "## 4. HARDWARE SPECIFICATIONS" -> 4. Headings without a leading number
    // (the Document Map) contribute nothing, which is correct.
    const m = /^##\s+(\d+)\./.exec(line);
    if (m) found.add(Number(m[1]));
  }
  return found;
}

function* walk(dir: string): Generator<string> {
  for (const entry of readdirSync(dir)) {
    if (SKIP_DIRS.has(entry)) continue;
    const full = join(dir, entry);
    let st;
    try {
      st = statSync(full);
    } catch {
      continue; // broken symlink
    }
    if (st.isDirectory()) yield* walk(full);
    else if (EXTENSIONS.some((e) => entry.endsWith(e))) yield full;
  }
}

// Rule 1 still tolerates the spaced form on purpose: a dangling reference must be
// reported as dangling even while Rule 2 is separately reporting its spacing.
const CITATION = new RegExp(`CLAUDE\\.md ?${SECTION} ?(\\d+)(\\.\\d+)*`, "g");
const SPACED = new RegExp(`${SECTION} \\d`, "g");

const valid = validSections();
if (valid.size === 0) {
  console.error(
    "check-section-refs: parsed zero numbered sections from CLAUDE.md — refusing to pass vacuously.",
  );
  process.exit(2);
}

type Hit = { file: string; line: number; text: string; section?: number };
const unresolved: Hit[] = [];
const spaced: Hit[] = [];

for (const file of walk(ROOT)) {
  const rel = relative(ROOT, file);
  const lines = readFileSync(file, "utf8").split("\n");
  lines.forEach((line, i) => {
    for (const m of line.matchAll(CITATION)) {
      const section = Number(m[1]);
      if (!valid.has(section)) {
        unresolved.push({ file: rel, line: i + 1, text: m[0], section });
      }
    }
    for (const m of line.matchAll(SPACED)) {
      spaced.push({ file: rel, line: i + 1, text: m[0] });
    }
  });
}

if (unresolved.length === 0 && spaced.length === 0) {
  const list = [...valid].sort((a, b) => a - b).join(", ");
  console.log(
    `All CLAUDE.md section citations resolve (sections present: ${list}), ` +
      `and all section markings use the canonical ${SECTION}N form.`,
  );
  process.exit(0);
}

if (unresolved.length > 0) {
  console.error(`${unresolved.length} unresolvable CLAUDE.md section citation(s):\n`);
  for (const b of unresolved) console.error(`  ${b.file}:${b.line}  ${b.text}`);
  const missing = [...new Set(unresolved.map((b) => b.section))].sort((a, b) => a! - b!);
  console.error(
    `\nCLAUDE.md has no section ${missing.join(", ")}. If content moved, repoint the`,
  );
  console.error(
    `citation at the file that now owns it (see CLAUDE.md's Document Map table),`,
  );
  console.error(`preserving the subsection number wherever it survived the move.\n`);
}

if (spaced.length > 0) {
  console.error(`${spaced.length} section marking(s) written with a space:\n`);
  for (const b of spaced) console.error(`  ${b.file}:${b.line}  "${b.text}"`);
  console.error(
    `\nWrite the section sign immediately before the number (${SECTION}4.2, not ` +
      `${SECTION} 4.2). One shape keeps these greppable — the spaced form is why an` +
      ` earlier audit counted 94 dangling references instead of 95.`,
  );
}

process.exit(1);
