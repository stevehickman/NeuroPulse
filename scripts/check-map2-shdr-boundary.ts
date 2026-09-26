#!/usr/bin/env bun
/**
 * check-map2-shdr-boundary.ts — Map 2 never feeds SHDR, and no sync boundary
 * or per-window delta is ever uploaded.
 *
 * NP-FW-NVRAM-001 §8.2 found that Map 3's natural output, a delta "since the
 * last sync", becomes a timestamped count in the hands of the one party that can
 * date the sync: the control software, which initiates every sync (§6.1) and
 * holds a clock. A stream of (uid, sessions_in_window, window_end) rows
 * reconstructs per-socket usage over time, which is treatment geography over
 * time, with no clock field anywhere in it. TIME-01 inspects the schema for
 * time TYPES, and an ordinal is an integer, so TIME-01 is walked around rather
 * than breached. NP-MOD-ID-001 §7.5.1.1's registrant-scoped consent model rests
 * on exactly that absence.
 *
 * Two decisions follow, and this gate is their check (OI-NVRAM-09,
 * RISK-NVRAM-02):
 *
 *   D-18  no field that identifies a sync boundary, and no value expressed as a
 *         per-window delta, may be uploaded. SHDR receives running totals.
 *   D-19  Map 2 is app-side, beside a clock and the user's identity, so it takes
 *         UHDR-class handling: no code may read Map 2 and write SHDR. The same
 *         code-structural independence CLAUDE.md §6.0 requires between
 *         SHDRUploader and ConsentStore, applied to a new pair.
 *
 *   bun scripts/check-map2-shdr-boundary.ts
 *   bun scripts/check-map2-shdr-boundary.ts --self-test
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-map2-shdr-boundary.ts --self-test
 * CI-Scans: every non-test source file under app/ and firmware/ for Map 2 and SHDR identifiers together, every SHDR source's identifiers, and every column of the SHDR fleet schema, for sync-boundary and per-window-delta names
 * CI-Scan-Paths: app/** firmware/** ci/shdr/shdr_fleet_schema.sql scripts/check-map2-shdr-boundary.ts
 *
 * ── What is checked ──────────────────────────────────────────────────────────
 *
 *  A. NO SOURCE FILE NAMES BOTH MAP 2 AND SHDR (D-19). Every source file under
 *     app/ and firmware/ (vendored code, build output and tests excluded) is
 *     tokenised with comments and string literals stripped, but with its
 *     #include / import / require targets and its own path kept. A file whose
 *     tokens include a Map 2 word AND an SHDR word fails.
 *
 *     A Map 2 word is `map2`, or `map` followed by `2`, as a word of an
 *     identifier split on `_` and camelCase: `Map2Store`, `moduleMap2`,
 *     `np_map_2_read`, `map2.ts`. That is a NAMING RULE this gate depends on,
 *     recorded as D-26 in NP-FW-NVRAM-001: Map 2's code carries `Map2` in the
 *     name of every file, type and module that holds it. `bitmap2d` is one
 *     word, so it is not Map 2.
 *
 *     An SHDR word is any identifier word containing `shdr`: SHDRUploader,
 *     ShdrUploader, np_shdr_*, shdr_fleet_schema.
 *
 *  B. NO SYNC BOUNDARY OR PER-WINDOW DELTA IN THE SHDR POPULATION (D-18).
 *     B1: every column of ci/shdr/shdr_fleet_schema.sql, which is what the
 *         fleet ingests the upload into, so it is the upload's field list.
 *     B2: every identifier in a device-side SHDR source: any non-test file
 *         under firmware/ whose path contains `shdr`. The app uploaders forward
 *         an opaque staged blob, so the device's record structs are where an
 *         upload field is first named. The app's SHDR code is deliberately NOT
 *         in B2. `SHDRUploader.lastUploadedAt` is the phone dating its own
 *         uploads, which §8.2 says the app can always do. The rule is that no
 *         such date, or the boundary it marks, travels in the upload, and B1
 *         checks that.
 *     A name matching SYNC_PATTERNS fails. The patterns are the §8.1 fields
 *     marked "never uploaded in any form" (a Map 3 journal `seq`, synced_upto,
 *     retired_upto), anything naming a sync, a watermark or a window's ends, the
 *     Map 3 ordinal, and, in the schema only, a bare `delta`. A rate
 *     (`*_delta_per_cycle`, `*_delta_per_session`) is a slope, not a window
 *     increment, and passes. Some patterns apply to the schema only, because
 *     their words are ordinary in code while in a fleet column they have one
 *     meaning. `sync` and `delta` are two (`lfs_file_sync`, a loop's `delta`).
 *     `window` is the third: firmware/shdr/ counts §G's rolling maintenance
 *     window and §H's record-budget characterisation window, neither of which
 *     is a sync window and neither of which is uploaded as a count.
 *
 * ── The reach, stated narrowly ───────────────────────────────────────────────
 *
 * Clause A is a per-FILE co-occurrence check. It catches a Map 2 reader that
 * also names an SHDR writer, and an import of one from the other. It does not
 * follow a call graph: Map 2 read in file X, passed to a helper in file Y, and
 * written to SHDR in Y passes it if X names no SHDR symbol and Y names no Map 2
 * symbol. The value in transit has to be carried by something. D-18's clause B
 * is what catches that something when it reaches the schema. Neither clause
 * alone closes D-19; together they make crossing it an edit a reviewer sees
 * flagged. That is the property §8.3 asks for, and it is the same reach
 * check-bench-boundary.ts states for its own boundary.
 *
 * Clause B is a NAME check. A per-window delta uploaded as `module_count_b`
 * passes it. What stops that is §8.2's rule itself, applied in review. This
 * gate stops the natural names, which is where the mistake is most likely to
 * be made in good faith.
 *
 * Map 2 does not exist in code yet, so clause A has no Map 2 file to inspect
 * on today's tree. It is a PREVENTIVE gate. The self-test proves it fires on
 * the shapes Map 2 is expected to take. Its vacuity guard asserts that the
 * SHDR writer population is being seen, because a check that saw no SHDR files
 * would pass for ever.
 *
 * Changing namesMap2, SYNC_PATTERNS or the populations is the decision this gate
 * exists to force. Revise NP-FW-NVRAM-001 §8.2–§8.3 in the same change.
 */
import { readFileSync, readdirSync, statSync, mkdtempSync, mkdirSync, writeFileSync, rmSync } from "fs";
import { join, resolve } from "path";
import { tmpdir } from "os";

const SCAN_ROOTS = ["app", "firmware"];
const SCHEMA = "ci/shdr/shdr_fleet_schema.sql";

const SOURCE_EXT = /\.(c|h|ts|tsx|js|mjs|swift|kt|kts|cs)$/;

/** Paths never scanned: vendored, generated or built code, and dependencies. */
const EXCLUDED_DIR = /(^|\/)(vendor|node_modules|build|dist|\.build|\.gradle|DerivedData|bin|obj)(\/|$)/;

/**
 * Test sources are excluded from both clauses. A test that proves Map 2 and
 * SHDR are independent has to name both. Test code ships in no image and
 * uploads nothing.
 */
const TEST_PATH = /(^|\/)(tests?|__tests__|[A-Za-z]*Tests|androidTest)(\/|$)|(Tests?|_tests?|\.test|\.spec)\.[a-z]+$/;

/**
 * Names that identify a sync boundary or a per-window delta. Matched against
 * the snake_case form of a column or identifier, so `syncedUpto` and
 * `synced_upto` are one name. `schemaOnly` patterns are applied to B1 only.
 */
const SYNC_PATTERNS: { re: RegExp; why: string; schemaOnly?: boolean }[] = [
  { re: /(^|_)(synced|retired)_up_?to($|_)/, why: "a Map 3 watermark (§8.1: never uploaded in any form)" },
  { re: /(^|_)watermarks?($|_)/, why: "a sync watermark (§6.1)" },
  { re: /(^|_)since_(the_)?(last_)?(sync|upload|dock|connect)/, why: "a count since a sync, which is a per-window delta (§8.2)" },
  { re: /(^|_)sync_(boundary|point|epoch|ordinal|window|index|seq|count|id)($|_)/, why: "a sync boundary (§8.2)" },
  { re: /(^|_)last_(sync|upload)/, why: "a sync boundary (§8.2)" },
  { re: /(^|_)window_(start|end|begin|close|open|ordinal|index|seq|delta|count|sessions?)($|_)/, why: "a window's ends or its increment (§8.2)", schemaOnly: true },
  { re: /(^|_)(per|in|this|each)_window($|_)/, why: "a per-window value (§8.2)", schemaOnly: true },
  { re: /(^|_)ordinals?($|_)/, why: "the Map 3 ordinal (§5.2, §8.1)" },
  { re: /(^|_)(map_?3|jrn|journal)($|_)/, why: "a Map 3 journal field (§8.1: seq, synced_upto and retired_upto are device-internal)" },
  { re: /(^|_)(synced?|syncs|syncing)($|_)/, why: "a sync, named (§8.2)", schemaOnly: true },
  { re: /(^|_)deltas?($|_)(?!per_(cycle|session|mate)($|_))/, why: "a delta that is not a per-cycle or per-session rate (§8.2)", schemaOnly: true },
];

// ── Tokenising ───────────────────────────────────────────────────────────────

/**
 * Strip comments and string literals, keeping line structure. `#` comments are
 * not handled because none of the scanned languages uses them outside a C
 * preprocessor line, which must stay live.
 */
function stripCode(src: string): string {
  let out = "";
  let i = 0;
  const n = src.length;
  while (i < n) {
    const c = src[i]!;
    const d = src[i + 1];
    if (c === "/" && d === "*") {
      const end = src.indexOf("*/", i + 2);
      const stop = end < 0 ? n : end + 2;
      out += src.slice(i, stop).replace(/[^\n]/g, " ");
      i = stop;
    } else if (c === "/" && d === "/") {
      while (i < n && src[i] !== "\n") i++;
    } else if (c === '"' || c === "'" || c === "`") {
      i++;
      while (i < n && src[i] !== c && (c === "`" || src[i] !== "\n")) i += src[i] === "\\" ? 2 : 1;
      i++;
      out += " ";
    } else {
      out += c;
      i++;
    }
  }
  return out;
}

/** Module targets named by the file: they are dependencies, so they count. */
function importTargets(src: string): string[] {
  const out: string[] = [];
  for (const m of src.matchAll(/^\s*#\s*include\s*[<"]([^>"]+)[>"]/gm)) out.push(m[1]!);
  for (const m of src.matchAll(/\bfrom\s*["'`]([^"'`]+)["'`]/g)) out.push(m[1]!);
  for (const m of src.matchAll(/\b(?:require|import)\s*\(\s*["'`]([^"'`]+)["'`]\s*\)/g)) out.push(m[1]!);
  for (const m of src.matchAll(/^\s*import\s+["'`]([^"'`]+)["'`]/gm)) out.push(m[1]!);
  return out;
}

/** Split an identifier into lower-case words on `_`, `.`, `/`, `-` and camelCase. */
function words(id: string): string[] {
  return id
    .replace(/([a-z0-9])([A-Z])/g, "$1_$2")
    .replace(/([A-Z]+)([A-Z][a-z])/g, "$1_$2")
    .toLowerCase()
    .split(/[^a-z0-9]+/)
    .filter(Boolean);
}

function snake(id: string): string {
  return words(id).join("_");
}

/** A Map 2 word: `map2`, or `map` then `2`. */
function namesMap2(id: string): boolean {
  const w = words(id);
  return w.some((x, i) => x === "map2" || (x === "map" && w[i + 1] === "2"));
}

function namesShdr(id: string): boolean {
  return /shdr/i.test(id);
}

function identifiers(code: string): string[] {
  return code.match(/[A-Za-z_][A-Za-z0-9_]*/g) ?? [];
}

// ── Schema columns ───────────────────────────────────────────────────────────

type Column = { table: string; column: string; line: number };

const NOT_A_COLUMN = /^(constraint|primary|unique|check|foreign|exclude|like)$/i;

function parseColumns(sql: string): Column[] {
  const cols: Column[] = [];
  const noComments = sql.replace(/--[^\n]*/g, "").replace(/\/\*[\s\S]*?\*\//g, (m) => m.replace(/[^\n]/g, " "));
  const re = /CREATE\s+TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?([A-Za-z_][\w.]*)\s*\(/gi;
  let m: RegExpExecArray | null;
  while ((m = re.exec(noComments))) {
    const table = m[1]!;
    // The body runs to the matching close paren.
    let depth = 1;
    let j = re.lastIndex;
    const start = j;
    while (j < noComments.length && depth > 0) {
      if (noComments[j] === "(") depth++;
      else if (noComments[j] === ")") depth--;
      j++;
    }
    const body = noComments.slice(start, j - 1);
    const baseLine = noComments.slice(0, start).split("\n").length;
    // Split on top-level commas.
    let d = 0;
    let seg = "";
    let segLine = baseLine;
    let line = baseLine;
    const flush = () => {
      const t = seg.trim();
      const cm = /^"?([A-Za-z_][\w]*)"?\s+[A-Za-z]/.exec(t);
      if (cm && !NOT_A_COLUMN.test(cm[1]!)) {
        cols.push({ table, column: cm[1]!, line: segLine + (seg.slice(0, seg.indexOf(t[0] ?? "")).split("\n").length - 1) });
      }
      seg = "";
      segLine = line;
    };
    for (const ch of body) {
      if (ch === "(") d++;
      else if (ch === ")") d--;
      if (ch === "," && d === 0) flush();
      else seg += ch;
      if (ch === "\n") line++;
    }
    flush();
    re.lastIndex = j;
  }
  return cols;
}

// ── The audit ────────────────────────────────────────────────────────────────

function walk(root: string, rel: string, out: string[]): void {
  let entries: string[];
  try {
    entries = readdirSync(join(root, rel));
  } catch {
    return;
  }
  for (const e of entries.sort()) {
    const r = `${rel}/${e}`;
    if (EXCLUDED_DIR.test(r)) continue;
    let st;
    try {
      st = statSync(join(root, r));
    } catch {
      continue;
    }
    if (st.isDirectory()) walk(root, r, out);
    else out.push(r);
  }
}

type Report = { violations: string[]; files: number; shdrFiles: number; columns: number; tables: number };

function syncHit(name: string, schema: boolean) {
  const s = snake(name);
  return SYNC_PATTERNS.find((p) => (schema || !p.schemaOnly) && p.re.test(s));
}

function audit(root: string): Report {
  const violations: string[] = [];
  const all: string[] = [];
  for (const r of SCAN_ROOTS) walk(root, r, all);
  const sources = all.filter((f) => SOURCE_EXT.test(f) && !TEST_PATH.test(f));

  let shdrFiles = 0;
  for (const f of sources) {
    const raw = readFileSync(join(root, f), "utf8");
    const ids = [...identifiers(stripCode(raw)), ...importTargets(raw), f];

    // A. Map 2 and SHDR in one file.
    const m2 = ids.find(namesMap2);
    const sh = ids.find(namesShdr);
    if (m2 && sh) {
      violations.push(
        `A: ${f}: names Map 2 (\`${m2}\`) and SHDR (\`${sh}\`) — no code may read Map 2 and ` +
          `write SHDR (NP-FW-NVRAM-001 §8.3, D-19)`,
      );
    }

    // B2. Sync-boundary names in a device-side SHDR source.
    if (f.startsWith("firmware/") && /shdr/i.test(f)) {
      shdrFiles++;
      const code = stripCode(raw).split("\n");
      const seen = new Set<string>();
      code.forEach((line, idx) => {
        for (const id of identifiers(line)) {
          const hit = syncHit(id, false);
          if (hit && !seen.has(id)) {
            seen.add(id);
            violations.push(
              `B: ${f}:${idx + 1}: identifier \`${id}\` names ${hit.why} — SHDR carries running ` +
                `totals only (NP-FW-NVRAM-001 §8.2, D-18)`,
            );
          }
        }
      });
    }
  }
  if (shdrFiles === 0) {
    violations.push("B: no SHDR source file found under firmware/ — refusing to pass vacuously");
  }

  // B1. The fleet schema's columns.
  let columns = 0;
  let tables = 0;
  let sql: string | null = null;
  try {
    sql = readFileSync(join(root, SCHEMA), "utf8");
  } catch {
    violations.push(`B: ${SCHEMA} not found — the upload's field list cannot be checked`);
  }
  if (sql !== null) {
    const cols = parseColumns(sql);
    columns = cols.length;
    tables = new Set(cols.map((c) => c.table)).size;
    for (const c of cols) {
      const hit = syncHit(c.column, true);
      if (hit) {
        violations.push(
          `B: ${SCHEMA}:${c.line}: column ${c.table}.${c.column} names ${hit.why} — SHDR carries ` +
            `running totals only (NP-FW-NVRAM-001 §8.2, D-18)`,
        );
      }
    }
    if (!cols.some((c) => c.column === "warranty_token")) {
      violations.push(`B: ${SCHEMA} parsed to ${columns} column(s) with no warranty_token — the parser has stopped seeing the schema`);
    }
  }

  return { violations, files: sources.length, shdrFiles, columns, tables };
}

// ── Self-test ────────────────────────────────────────────────────────────────

if (process.argv.includes("--self-test")) {
  const failures: string[] = [];

  // Word splitting and the two word predicates, in isolation.
  const cases: [string, boolean][] = [
    ["Map2Store", true], ["moduleMap2", true], ["np_map2_read", true], ["np_map_2_read", true],
    ["MAP2_MAGIC", true], ["app/web/src/lib/map2.ts", true], ["bitmap2d", false], ["heatmap2", false],
    ["np_module_map", false], ["map3", false], ["mapping2", false],
  ];
  for (const [id, want] of cases) {
    if (namesMap2(id) !== want) failures.push(`namesMap2(${id}) should be ${want}`);
  }
  if (snake("syncedUpto") !== "synced_upto") failures.push(`snake(syncedUpto) = ${snake("syncedUpto")}`);
  if (snake("SHDRUploader") !== "shdr_uploader") failures.push(`snake(SHDRUploader) = ${snake("SHDRUploader")}`);

  // Stripper: prose and strings do not count, includes do.
  const stripped = stripCode('// Map2 is app-side\nlet s = "Map2 SHDR";\nlet x = `map2`;\nint live;\n');
  if (/map2/i.test(stripped)) failures.push("stripper — a comment or string literal survived");
  if (stripped.split("\n").length !== 5) failures.push("stripper — line structure not preserved");
  if (!importTargets('#include "np_map2.h"\n').includes("np_map2.h")) failures.push("importTargets — missed a C include");
  if (!importTargets("import { a } from './shdr/upload';\n").includes("./shdr/upload")) failures.push("importTargets — missed a TS import");

  // Schema parser against a shape like the real one.
  const parsed = parseColumns(
    "-- c\nCREATE TABLE t (\n  a BYTEA NOT NULL, -- x, y\n  b NUMERIC(6,2),\n  CONSTRAINT k CHECK (b > 0),\n  PRIMARY KEY (a)\n);\n",
  );
  if (parsed.map((c) => c.column).join(",") !== "a,b") failures.push(`parseColumns — got ${JSON.stringify(parsed)}`);

  const box = mkdtempSync(join(tmpdir(), "np-map2-shdr-"));
  const SCHEMA_BASE = [
    "CREATE TABLE devices (",
    "    warranty_token          BYTEA        NOT NULL PRIMARY KEY,  -- no sync, delta or window here",
    "    session_count           INTEGER      NOT NULL,",
    "    last_seen_month         DATE",
    ");",
    "CREATE TABLE socket_part_type_wear (",
    "    warranty_token          BYTEA        NOT NULL,",
    "    contact_resistance_delta_per_cycle NUMERIC(8,4),",
    "    occupancy_session_count INTEGER,",
    "    CONSTRAINT w CHECK (occupancy_session_count >= 0)",
    ");",
    "CREATE TABLE shdr_accel_characterisation (",
    "    warranty_token          BYTEA NOT NULL,",
    "    char_record_seq         INTEGER,",
    "    gap_index               INTEGER",
    ");",
    "",
  ].join("\n");
  const BASE: Record<string, string> = {
    [SCHEMA]: SCHEMA_BASE,
    "app/ios/NeurOne/Data/SHDRUploader.swift":
      "// Map2 must never be read here.\nfinal class SHDRUploader {\n  func upload() { queue.sync { send(staged) } }\n}\n",
    "app/android/app/src/main/kotlin/life/neurone/app/data/ShdrUploader.kt":
      "class ShdrUploader { fun upload() { val delta = 1; val msg = \"Map2 never\" } }\n",
    "firmware/shdr/src/np_shdr_accel.c":
      '#include "np_shdr_accel.h"\nstatic uint32_t s_gap_index;\nvoid f(void) { (void)lfs_file_sync; }\n',
    "app/web/src/lib/helmetInventory.ts": "export const uid = 1; // SHDR-class component identifier\n",
  };
  const build = (patch: Record<string, string | null>): string => {
    const root = mkdtempSync(join(box, "t-"));
    for (const [rel, body] of Object.entries({ ...BASE, ...patch })) {
      if (body === null) continue;
      mkdirSync(join(root, rel, ".."), { recursive: true });
      writeFileSync(join(root, rel), body);
    }
    return root;
  };
  const expect = (label: string, patch: Record<string, string | null>, needle: string | null) => {
    const { violations } = audit(build(patch));
    if (needle === null) {
      if (violations.length) failures.push(`${label} — expected clean, got: ${violations[0]}`);
    } else if (!violations.some((v) => v.includes(needle))) {
      failures.push(`${label} — no violation matching ${JSON.stringify(needle)}; got ${JSON.stringify(violations)}`);
    }
  };
  const col = (table: string, name: string) => ({
    [SCHEMA]: SCHEMA_BASE.replace(`CREATE TABLE ${table} (\n`, `CREATE TABLE ${table} (\n    ${name} INTEGER,\n`),
  });

  // Must pass: the direction that keeps the gate from being switched off.
  expect("the baseline passes, with Map2 in comments/strings beside SHDR code, and sync/delta as code words", {}, null);
  expect(
    "a Map 2 store with no SHDR reference passes: Map 2 is allowed to exist",
    { "app/ios/NeurOne/Modules/Map2Store.swift": "final class Map2Store { var totals: [UInt64: UInt32] = [:] }\n" },
    null,
  );
  expect(
    "a test naming both passes: the independence test has to",
    { "app/ios/NeurOneTests/Map2IndependenceTests.swift": "let s = Map2Store(); let u = SHDRUploader()\n" },
    null,
  );
  expect(
    "the app dating its own uploads passes: the rule is about the upload, not the phone",
    { "app/ios/NeurOne/Data/SHDRUploader.swift": "final class SHDRUploader { var lastUploadedAt: Date? }\n" },
    null,
  );
  expect(
    "firmware SHDR's own §G/§H windows pass: they are not sync windows",
    { "firmware/shdr/src/np_accel_shdr.c": "static bool window_open;\nstatic uint8_t drops_in_window;\n" },
    null,
  );
  expect("a vendored file naming both is not scanned", { "firmware/vendor/x/map2_shdr.c": "int map2_shdr;\n" }, null);
  expect(
    "a per-cycle and a per-session rate pass: a slope is not a window increment",
    col("devices", "pd_ratio_delta_per_session"),
    null,
  );

  // Must fail: clause A.
  expect(
    "a Map 2 reader that calls the SHDR uploader is caught (Swift)",
    { "app/ios/NeurOne/Modules/Map2Sync.swift": "func push(_ m: Map2Store, _ u: SHDRUploader) { u.stage(m.totals) }\n" },
    "Map2Sync.swift",
  );
  expect(
    "the SHDR uploader importing Map 2 is caught (Kotlin)",
    {
      "app/android/app/src/main/kotlin/life/neurone/app/data/ShdrUploader.kt":
        "import life.neurone.core.modules.ModuleMap2\nclass ShdrUploader { fun upload() {} }\n",
    },
    "ModuleMap2",
  );
  expect(
    "a TS module importing map2 into an SHDR path is caught by its import alone",
    { "app/web/src/lib/shdrExport.ts": "import { totals } from './map2';\nexport const x = totals;\n" },
    "./map2",
  );
  expect(
    "a C include of a Map 2 header in firmware SHDR code is caught",
    { "firmware/shdr/src/np_shdr_accel.c": '#include "np_map2.h"\n#include "np_shdr_accel.h"\n' },
    "np_map2.h",
  );
  expect(
    "a file whose NAME is Map 2 and that references SHDR is caught",
    { "app/web/src/lib/map2.ts": "export function f(u: ShdrQueue) { u.push(1); }\n" },
    "ShdrQueue",
  );

  // Must fail: clause B1, the schema.
  for (const [name, needle] of [
    ["synced_upto", "Map 3 watermark"],
    ["retired_upto", "Map 3 watermark"],
    ["sessions_since_sync", "since a sync"],
    ["sessions_since_last_upload", "since a sync"],
    ["window_end_ordinal", "window's ends"],
    ["sessions_in_window", "per-window"],
    ["map3_seq", "Map 3 journal"],
    ["journal_ordinal", "Map 3 ordinal"],
    ["last_sync_index", "sync boundary"],
    ["sync_watermark", "watermark"],
    ["synced", "a sync, named"],
    ["session_count_delta", "not a per-cycle"],
    ["delta_sessions", "not a per-cycle"],
  ] as const) {
    expect(`schema column ${name} is caught`, col("socket_part_type_wear", name), needle);
  }

  // Must fail: clause B2, an SHDR source.
  expect(
    "a sync-boundary field in a firmware SHDR record struct is caught",
    { "firmware/shdr/include/np_shdr_module.h": "typedef struct { uint32_t synced_upto; uint16_t total; } np_shdr_module_t;\n" },
    "synced_upto",
  );
  expect(
    "a camelCase count since the last sync in a firmware SHDR source is caught",
    { "firmware/shdr/src/np_shdr_module.c": "static uint16_t sessionsSinceLastSync;\n" },
    "sessionsSinceLastSync",
  );
  expect(
    "the Map 3 ordinal in a firmware SHDR source is caught",
    { "firmware/shdr/src/np_shdr_module.c": "typedef struct { uint32_t map3_ordinal; } np_shdr_row_t;\n" },
    "map3_ordinal",
  );

  // Must fail: vacuity.
  expect(
    "no SHDR source anywhere refuses to pass",
    { "firmware/shdr/src/np_shdr_accel.c": null },
    "refusing to pass vacuously",
  );
  expect("a missing schema is reported", { [SCHEMA]: null }, "not found");
  expect("a schema the parser cannot read is reported", { [SCHEMA]: "CREATE TABLE x ( );\n" }, "stopped seeing the schema");

  rmSync(box, { recursive: true, force: true });
  if (failures.length) {
    console.error(`SELF-TEST FAIL — ${failures.length} case(s):`);
    for (const f of failures) console.error(`  ✗ ${f}`);
    process.exit(1);
  }
  console.log("SELF-TEST PASS — both clauses fail on every seeded violation, and pass on the legitimate shapes.");
  process.exit(0);
}

// ── Main ─────────────────────────────────────────────────────────────────────

const root = resolve(import.meta.dir, "..");
const r = audit(root);
console.log(
  `scanned: ${r.files} source file(s), ${r.shdrFiles} SHDR source(s), ${r.columns} schema column(s) in ${r.tables} table(s)`,
);
if (r.violations.length) {
  console.error(`FAIL — ${r.violations.length} violation(s):`);
  for (const v of r.violations) console.error(`  ✗ ${v}`);
  process.exit(1);
}
console.log("PASS — no file names Map 2 and SHDR together, and no SHDR name identifies a sync boundary or per-window delta.");
