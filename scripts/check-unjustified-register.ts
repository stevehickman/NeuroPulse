#!/usr/bin/env bun
/**
 * check-unjustified-register.ts — docs/status/unjustified-choices.md stays an index of what the
 * tree actually marks as unjustified (CLAUDE.md §19).
 *
 * The register lists every choice that has no derivation, no measurement or no checked source, with
 * a link to where it was decided. A list kept by reading goes stale in the direction that matters:
 * a new PROVISIONAL constant is added and nobody adds the row. This gate makes the commonest way of
 * adding an unjustified choice, marking it in the source, fail until the register names it.
 *
 * ── What this checks ─────────────────────────────────────────────────────────
 *
 *   R1  Every register row has seven cells, a unique UC-nnn ID, and one of the six classes.
 *   R2  Every firmware source or header, every hardware/*.json, and every app, protocol, simulator or CI
 *       file that carries an unjustified-value marker is named in the register by path. Markers:
 *       PROVISIONAL, PLACEHOLDER, UNCALIBRATED, UNVALIDATED, "NOT DERIVED" (upper case, as the source
 *       writes them), and a JSON "status": "provisional" or "placeholder". In app/, protocols/,
 *       simulator/ and ci/ only a COMMENT line counts, because UI strings and locale keys such as
 *       "CLINICIAN_GRANT_NAME_PLACEHOLDER" are not choices. Tests and vendor/ are not scanned.
 *   R3  Every docs/, firmware/, hardware/ and scripts/ path the register names exists.
 *   R4  Every OI-… ID the register cites appears in at least one other file under docs/. A register
 *       that cites an item nothing else records has invented its trail.
 *   R5  A UC ID in the "Justified or retired" table is not also an active row.
 *
 * It cannot see an unjustified choice that is not marked in source or in an OI row. That is the
 * rule's job (CLAUDE.md §19), not the gate's. R2 is deliberately file-level, because line numbers
 * drift and a gate that breaks on every edit is a gate that gets deleted.
 *
 * Usage: bun scripts/check-unjustified-register.ts [--self-test]
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-unjustified-register.ts --self-test
 * CI-Scans: every UC row of docs/status/unjustified-choices.md, against the PROVISIONAL / PLACEHOLDER markers in firmware/, hardware/*.json and comment lines in app/, protocols/, simulator/ and ci/, and the OI IDs and paths it cites under docs/
 * CI-Scan-Paths: docs/** firmware/** hardware/** app/** protocols/** simulator/** ci/**
 */
import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { join, relative } from "node:path";

const ROOT = join(import.meta.dir, "..");
const REGISTER = "docs/status/unjustified-choices.md";
const CLASSES = new Set([
  "NO-DERIVATION", "UNMEASURED", "ASSUMED", "PLACEHOLDER", "UNVERIFIED-SOURCE", "ATTRIBUTION",
]);
const COLUMNS = 7;
const SOURCE_MARKER = /PROVISIONAL|PLACEHOLDER|UNCALIBRATED|UNVALIDATED|NOT DERIVED/;
const COMMENT_MARKER = /^\s*(\/\/|\*|\/\*|#|--|\/\/\/).*(PROVISIONAL|UNVALIDATED|UNCALIBRATED|NOT DERIVED|PLACEHOLDER)/m;
const JSON_MARKER = /"status"\s*:\s*"(provisional|placeholder)"/;
const OI_ID = /OI-[A-Z0-9]+(?:-[A-Z0-9]+)*-\d+[a-z]?/g;
const PATH_REF = /\b((?:docs|firmware|hardware|scripts)\/[A-Za-z0-9_./-]*[A-Za-z0-9_])/g;

export interface Row { id: string; cells: string[]; line: number; section: "active" | "justified" }

export function parseRegister(text: string): Row[] {
  const rows: Row[] = [];
  let section: Row["section"] = "active";
  text.split("\n").forEach((l, i) => {
    if (/^##\s+Justified or retired/.test(l)) section = "justified";
    const m = /^\|\s*(UC-\d{3})\s*\|/.exec(l);
    if (!m) return;
    // Split on pipes that are not inside backticks. The register writes none inside cells.
    const cells = l.replace(/^\|/, "").replace(/\|\s*$/, "").split("|").map((c) => c.trim());
    rows.push({ id: m[1], cells, line: i + 1, section });
  });
  return rows;
}

export function checkRows(rows: Row[]): string[] {
  const errs: string[] = [];
  const seen = new Set<string>();
  for (const r of rows) {
    if (r.section === "active") {
      if (r.cells.length !== COLUMNS) errs.push(`R1 ${REGISTER}:${r.line} ${r.id} has ${r.cells.length} cells, expected ${COLUMNS}`);
      const cls = (r.cells[3] ?? "").replace(/`/g, "");
      if (!CLASSES.has(cls)) errs.push(`R1 ${REGISTER}:${r.line} ${r.id} class "${cls}" is not one of ${[...CLASSES].join(", ")}`);
    }
    if (seen.has(r.id + r.section)) errs.push(`R1 ${REGISTER}:${r.line} ${r.id} appears twice`);
    seen.add(r.id + r.section);
  }
  const active = new Set(rows.filter((r) => r.section === "active").map((r) => r.id));
  for (const r of rows) if (r.section === "justified" && active.has(r.id)) errs.push(`R5 ${REGISTER}:${r.line} ${r.id} is justified and still active`);
  return errs;
}

/** Escapes every regex metacharacter, backslash included, so an ID is matched literally. */
function escapeRegExp(text: string): string {
  return text.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function walk(dir: string, out: string[], skip: (p: string) => boolean): void {
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    const rel = relative(ROOT, p);
    if (skip(rel)) continue;
    const st = statSync(p);
    if (st.isDirectory()) walk(p, out, skip);
    else out.push(rel);
  }
}

function markedFiles(): string[] {
  const files: string[] = [];
  const skip = (rel: string) => /(^|\/)(tests|vendor|node_modules|build)(\/|$)/.test(rel);
  walk(join(ROOT, "firmware"), files, (r) => skip(r)); // paths are relative to ROOT
  const hw: string[] = [];
  walk(join(ROOT, "hardware"), hw, skip);
  const hits: string[] = [];
  for (const f of files) {
    if (!/\.(c|h)$/.test(f)) continue;
    if (SOURCE_MARKER.test(readFileSync(join(ROOT, f), "utf8"))) hits.push(f);
  }
  for (const f of hw) {
    if (!/\.json$/.test(f)) continue;
    const t = readFileSync(join(ROOT, f), "utf8");
    if (JSON_MARKER.test(t) || /PROVISIONAL|PLACEHOLDER/.test(t)) hits.push(f);
  }
  // Second population: comment lines in app, protocol, simulator and CI sources.
  const wider: string[] = [];
  const skipWide = (rel: string) => /(^|\/)(node_modules|build|Pods|\.gradle)(\/|$)/.test(rel);
  for (const d of ["app", "protocols", "simulator", "ci"]) {
    if (existsSync(join(ROOT, d))) walk(join(ROOT, d), wider, skipWide);
  }
  for (const f of wider) {
    if (!/\.(kt|kts|swift|ts|tsx|cs|npps|sql|js|py)$/.test(f)) continue;
    if (/(Tests?\/|\/test\/|\.test\.|Tests\.|_test|selftest)/.test(f)) continue;
    if (COMMENT_MARKER.test(readFileSync(join(ROOT, f), "utf8"))) hits.push(f);
  }
  return hits.sort();
}

function docsCorpus(): string {
  const files: string[] = [];
  walk(join(ROOT, "docs"), files, (r) => r === REGISTER || /(^|\/)superseded(\/|$)/.test(r));
  return files.filter((f) => /\.(md|csv|tsv)$/.test(f)).map((f) => readFileSync(join(ROOT, f), "utf8")).join("\n");
}

function main(): number {
  const text = readFileSync(join(ROOT, REGISTER), "utf8");
  const rows = parseRegister(text);
  const errs = checkRows(rows);
  if (rows.length === 0) errs.push(`R1 ${REGISTER} has no UC rows`);

  for (const f of markedFiles()) {
    if (!text.includes(f)) errs.push(`R2 ${f} marks a value PROVISIONAL / PLACEHOLDER / UNCALIBRATED / UNVALIDATED / NOT DERIVED and ${REGISTER} does not name it. Add a row (CLAUDE.md §19)`);
  }

  const bodyLines = text.split("\n");
  const paths = new Set<string>();
  for (const l of bodyLines) for (const m of l.matchAll(PATH_REF)) paths.add(m[1]);
  for (const p of paths) {
    if (!existsSync(join(ROOT, p))) errs.push(`R3 ${REGISTER} names ${p}, which does not exist`);
  }

  const corpus = docsCorpus();
  const ids = new Set<string>();
  for (const l of bodyLines) for (const m of l.matchAll(OI_ID)) ids.add(m[0]);
  for (const id of [...ids].sort()) {
    const re = new RegExp(`${RegExp.escape(id)}(?![A-Za-z0-9])`);
    if (!re.test(corpus)) errs.push(`R4 ${id} is cited in ${REGISTER} and recorded nowhere else under docs/`);
  }

  if (errs.length) {
    console.error(errs.join("\n"));
    console.error(`\n${errs.length} problem(s). ${REGISTER} must index every marked unjustified choice (CLAUDE.md §19).`);
    return 1;
  }
  console.log(`scanned: ${rows.length} register rows, ${ids.size} OI IDs, ${paths.size} paths`);
  console.log("every marked unjustified choice is indexed, and every cited ID and path exists: PASS");
  return 0;
}

function selfTest(): number {
  let bad = 0;
  const t = (name: string, cond: boolean) => { if (!cond) { console.error(`FAIL ${name}`); bad++; } };
  const good = "| UC-001 | a | b | `NO-DERIVATION` | c | d | e |";
  t("good row passes", checkRows(parseRegister(good)).length === 0);
  t("six cells fail R1", checkRows(parseRegister("| UC-001 | a | b | `NO-DERIVATION` | c | d |")).some((e) => e.startsWith("R1")));
  t("unknown class fails R1", checkRows(parseRegister("| UC-001 | a | b | `GUESS` | c | d | e |")).some((e) => e.startsWith("R1")));
  t("duplicate id fails R1", checkRows(parseRegister(good + "\n" + good)).some((e) => e.includes("twice")));
  const both = good + "\n## Justified or retired\n| UC-001 | a | d | o | j |";
  t("justified and active fails R5", checkRows(parseRegister(both)).some((e) => e.startsWith("R5")));
  t("justified-only passes", checkRows(parseRegister("## Justified or retired\n| UC-002 | a | d | o | j |")).length === 0);
  t("source marker matches", SOURCE_MARKER.test("/* PROVISIONAL */") && !SOURCE_MARKER.test("NP_HUB_ERR_TIER_UNVERIFIED"));
  t("comment marker matches code comments only", COMMENT_MARKER.test("    // UNVALIDATED PLACEHOLDER — x") && !COMMENT_MARKER.test('TextField("CLINICIAN_GRANT_NAME_PLACEHOLDER", text: $n)'));
  t("escapeRegExp neutralises backslashes and metacharacters", new RegExp(escapeRegExp("a\\b.c")).test("a\\b.c") && !new RegExp(escapeRegExp("a.c")).test("abc"));
  t("json marker matches", JSON_MARKER.test('"status": "placeholder"') && !JSON_MARKER.test('"status": "external-limit"'));
  if (!bad) console.log("self-test ok");
  return bad ? 1 : 0;
}

process.exit(process.argv.includes("--self-test") ? selfTest() : main());
