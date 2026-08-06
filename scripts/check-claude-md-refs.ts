#!/usr/bin/env bun
/**
 * check-claude-md-refs.ts — every CLAUDE.md section citation must resolve.
 *
 * CLAUDE.md Rev 33 relocated sections 7-15 into docs/status/ and docs/reference/
 * and left every inbound pointer intact. 95 citations across firmware headers,
 * the DHF, and design specs silently stopped resolving, and nothing caught it
 * for a whole revision. Those citations are the regulatory record: a 510(k)
 * reviewer walking the traceability chain lands on nothing.
 *
 * This check re-derives the valid section numbers from CLAUDE.md's own top-level
 * headings on every run. It is deliberately NOT a hardcoded list — a hardcoded
 * list would rot exactly the way the references it guards did.
 *
 * Scope: the MAJOR section number only. A citation of subsection 13.4 is valid
 * iff CLAUDE.md still has a section 13. Validating subsections would require
 * this script to model heading depth in nine relocated files for very little
 * extra signal.
 *
 * Both spacings are matched. The repo contains both shapes, and the space
 * variant is why the original count was under-reported: docs/ABBREVIATIONS.md
 * wrote its citation with a space and no one's grep could see it.
 *
 *   bun scripts/check-claude-md-refs.ts
 *
 * Exits non-zero listing file:line for each unresolvable citation.
 */

import { readFileSync, readdirSync, statSync } from "fs";
import { join, relative } from "path";

const ROOT = join(import.meta.dir, "..");
const CLAUDE_MD = join(ROOT, "CLAUDE.md");

const EXTENSIONS = [".md", ".c", ".h", ".ts", ".tsx", ".swift", ".js", ".npps"];
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

// Optional space after the filename and after the section sign.
const CITATION = /CLAUDE\.md ?§ ?(\d+)(\.\d+)*/g;

const valid = validSections();
if (valid.size === 0) {
  console.error("check-claude-md-refs: parsed zero numbered sections from CLAUDE.md — refusing to pass vacuously.");
  process.exit(2);
}

type Bad = { file: string; line: number; text: string; section: number };
const bad: Bad[] = [];

for (const file of walk(ROOT)) {
  const lines = readFileSync(file, "utf8").split("\n");
  lines.forEach((line, i) => {
    for (const m of line.matchAll(CITATION)) {
      const section = Number(m[1]);
      if (!valid.has(section)) {
        bad.push({ file: relative(ROOT, file), line: i + 1, text: m[0], section });
      }
    }
  });
}

if (bad.length === 0) {
  const list = [...valid].sort((a, b) => a - b).join(", ");
  console.log(`All CLAUDE.md section citations resolve. Sections present: ${list}.`);
  process.exit(0);
}

console.error(`${bad.length} unresolvable CLAUDE.md section citation(s):\n`);
for (const b of bad) console.error(`  ${b.file}:${b.line}  ${b.text}`);

const missing = [...new Set(bad.map((b) => b.section))].sort((a, b) => a - b);
console.error(
  `\nCLAUDE.md has no section ${missing.join(", ")}. If content moved, repoint the`,
);
console.error(
  `citation at the file that now owns it (see CLAUDE.md's Document Map table),`,
);
console.error(`preserving the subsection number wherever it survived the move.`);
process.exit(1);
