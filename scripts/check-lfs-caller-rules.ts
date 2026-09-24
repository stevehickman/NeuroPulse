#!/usr/bin/env bun
/**
 * check-lfs-caller-rules.ts — the littlefs caller rules stay true of the code.
 *
 * ── Why this file exists ─────────────────────────────────────────────────────
 *
 * NP-SOUP-LFS-001 found, twice and by two routes, that what keeps littlefs a
 * Class B component on SW-02 is not littlefs. §6.2: the classification rests on
 * np_module_map's reject-and-rebuild policy, and Map 3 is specified to need the
 * opposite policy — so REQ-LFS-01 says no value that bounds an emission may be
 * stored under a tail-additive policy. §11: the applicable anomalies in v2.11.3
 * are stopped by caller rules — one handle per file (488e84bb), content
 * verified on every read (#1205, #1164), bounded create/delete churn (#1210).
 *
 * Rev 4 wrote those callers (firmware/hub_control/src/np_cfg_store.c) and made
 * each rule a property of its API. An API property holds only while nothing
 * goes around the API, and going around it is one `lfs_file_opencfg(` away.
 * OI-LFS-03 asked for REQ-LFS-01 to be "observable rather than remembered";
 * OI-LFS-06 asked for one-handle-per-file to be "checkable". This is both.
 *
 * NP-CONV-001 §8: a probe must be falsified before it is trusted.
 * `--self-test` proves every rule rejects, and that the vacuity paths refuse.
 *
 * ── The rules ────────────────────────────────────────────────────────────────
 *
 * Population: every .c and .h under firmware/, except firmware/vendor/ (the
 * component itself) and any tests/ directory (host tests act behind the store
 * on purpose — they are how a filesystem defect is simulated).
 *
 *   R1  Only the files in LFS_CALLERS call littlefs at all. Everything else
 *       reaches storage through np_cfg_store or a log-instance glue that has
 *       been added here, with its reason, on the record. (OI-LFS-06, -09)
 *   R2  lfs_file_open() is called nowhere (LFS_NO_MALLOC makes it allocate),
 *       and lfs_file_opencfg() exactly once, inside np_cfg_store.c's single
 *       opener cfg_open() — the open-handle registry. (OI-LFS-06)
 *   R3  The Config store calls no lfs_remove, lfs_rename or lfs_stat: no
 *       create/delete churn (#1210, OI-LFS-08), and presence is never taken as
 *       durability (#1164, OI-LFS-09).
 *   R4  The Config file table's TAIL_ADDITIVE rows are exactly
 *       PINNED_TAIL_ADDITIVE, and every NP_CFG_FILE_* has a row. A second
 *       journal, or a rebuild cache quietly moved to tail-additive, is a
 *       REQ-LFS-01 decision and fails here until it is made. (OI-LFS-03)
 *   R5  np_cfg_store_journal_read() — the only reader of a tail-additive file —
 *       is called only from JOURNAL_READERS, each listed with why what it
 *       reads is history and not a limit. (OI-LFS-03)
 *   R6  No emission-limit consumer (LIMIT_CONSUMERS) names the journal at all:
 *       not its file id, not the journal API, not its path. A consumer that
 *       cannot name Map 3 cannot read a limit out of it. (OI-LFS-03)
 *
 * ── The reach, stated narrowly ───────────────────────────────────────────────
 *
 * Textual, like its siblings. R5 and R6 see a direct call and a direct name;
 * they do not follow a value through a helper, so a JOURNAL_READERS entry that
 * handed a journal value to a limit consumer would pass. That is why every
 * JOURNAL_READERS entry must carry its reason — the gate forces the question
 * to be asked where a reader is added, which is the only place a text check
 * can ask it. Widening to data flow needs a C parser, which this is not.
 *
 *   bun scripts/check-lfs-caller-rules.ts
 *   bun scripts/check-lfs-caller-rules.ts --self-test
 *
 * Exits 1 listing each violation; exits 2 rather than passing vacuously if the
 * store, its file table, its opener or its enum cannot be found.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-lfs-caller-rules.ts --self-test
 * CI-Scans: every littlefs call site in firmware/, the Config store's file table, and every reader of the Map 3 journal
 * CI-Scan-Paths: firmware/** scripts/check-lfs-caller-rules.ts
 */

import { readFileSync, readdirSync, statSync, mkdtempSync, mkdirSync, writeFileSync, rmSync, existsSync } from "fs";
import { join, relative, resolve, dirname } from "path";
import { tmpdir } from "os";

const rootFlag = process.argv.indexOf("--root");
const ROOT =
  rootFlag >= 0 && process.argv[rootFlag + 1]
    ? resolve(process.argv[rootFlag + 1]!)
    : join(import.meta.dir, "..");

const STORE_C = "firmware/hub_control/src/np_cfg_store.c";
const STORE_H = "firmware/hub_control/include/np_cfg_store.h";

/** R1 — who may call littlefs, and why. */
const LFS_CALLERS: Record<string, string> = {
  [STORE_C]: "the Config instance's only caller — every caller rule is a property of its API",
  "firmware/hub_control/src/np_lfs_log_instance.c":
    "lfs_fs_gc() in np_lfs_log_prime_allocator() only; the log instances' file I/O is OI-LOG-05..07 and must be added here when it exists",
};

/** R4 — the only tail-additive Config files. Adding one is a REQ-LFS-01 decision. */
const PINNED_TAIL_ADDITIVE = ["NP_CFG_FILE_MAP3"];

/** R5 — who may read a tail-additive journal, and why what it reads is history. */
const JOURNAL_READERS: Record<string, string> = {
  // None yet: Map 3 has no consumer in the tree (NP-FW-NVRAM-001 §4.2 is
  // specified, not implemented). The first one is added here with its reason.
};

/**
 * R6 — the files on the path to an emission magnitude. §5 of NP-SOUP-LFS-001
 * names the consumer family; the safety MCU is listed because it owns every
 * limit that is resident, and a stored value reaching it would be the breach.
 * Paths ending in "/" are directory prefixes.
 */
const LIMIT_CONSUMERS = [
  "firmware/hub_control/src/np_module_map.c",
  "firmware/hub_control/src/np_pbm_power_gov.c",
  "firmware/hub_control/src/np_pbm_cal_bridge.c",
  "firmware/hub_control/src/np_socket_dispatch.c",
  "firmware/hub_control/src/np_session_runner.c",
  "firmware/hub_control/src/np_chan_decl.c",
  "firmware/hub_control/modules/np_mod_pbm.c",
  "firmware/hub_control/modules/np_mod_stim.c",
  "firmware/pbm/",
  "firmware/safety_mcu/",
];

/** Strip comments and string literals; newlines survive so line numbers do. */
function stripNonCode(src: string): string {
  let out = "";
  for (let i = 0; i < src.length; i++) {
    const two = src.slice(i, i + 2);
    if (two === "/*") {
      const end = src.indexOf("*/", i + 2);
      out += src.slice(i, end < 0 ? src.length : end + 2).replace(/[^\n]/g, " ");
      i = end < 0 ? src.length : end + 1;
      continue;
    }
    if (two === "//") {
      const end = src.indexOf("\n", i);
      out += " ".repeat((end < 0 ? src.length : end) - i);
      i = (end < 0 ? src.length : end) - 1;
      continue;
    }
    const c = src[i]!;
    if (c === '"' || c === "'") {
      let j = i + 1;
      while (j < src.length && src[j] !== c) j += src[j] === "\\" ? 2 : 1;
      out += " ".repeat(Math.min(j, src.length - 1) - i + 1);
      i = j;
      continue;
    }
    out += c;
  }
  return out;
}

/** Comments only stripped — for R6's path check, which must see string literals. */
function stripComments(src: string): string {
  return src.replace(/\/\*[\s\S]*?\*\//g, (m) => m.replace(/[^\n]/g, " ")).replace(/\/\/[^\n]*/g, "");
}

function walk(dir: string, acc: string[]): string[] {
  if (!existsSync(dir)) return acc;
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    const st = statSync(p);
    if (st.isDirectory()) {
      if (name === "tests" || name === "vendor" || name === "build" || name.startsWith(".")) continue;
      walk(p, acc);
    } else if (/\.(c|h)$/.test(name)) {
      acc.push(p);
    }
  }
  return acc;
}

const lineOf = (text: string, idx: number) => text.slice(0, idx).split("\n").length;

function isLimitConsumer(rel: string): boolean {
  return LIMIT_CONSUMERS.some((c) => (c.endsWith("/") ? rel.startsWith(c) : rel === c));
}

/** Brace-matched body of the function whose definition starts at `sig`. */
function functionSpan(code: string, sig: RegExp): [number, number] | null {
  const m = sig.exec(code);
  if (!m) return null;
  const open = code.indexOf("{", m.index + m[0].length);
  if (open < 0) return null;
  let depth = 0;
  for (let i = open; i < code.length; i++) {
    if (code[i] === "{") depth++;
    else if (code[i] === "}" && --depth === 0) return [open, i];
  }
  return null;
}

function run(root: string): { code: number; lines: string[] } {
  const lines: string[] = [];
  const refuse = (why: string) => ({ code: 2, lines: [`check-lfs-caller-rules: ${why} — refusing to pass vacuously`] });

  let storeSrc: string;
  let headerSrc: string;
  try {
    storeSrc = readFileSync(join(root, STORE_C), "utf8");
    headerSrc = readFileSync(join(root, STORE_H), "utf8");
  } catch {
    return refuse(`cannot read ${STORE_C} or ${STORE_H}`);
  }
  const storeCode = stripNonCode(storeSrc);

  // ── R4: parse the file table and the enum ──────────────────────────────────
  const table = /s_files\s*\[\s*NP_CFG_FILE_COUNT\s*\]\s*=\s*\{([\s\S]*?)\n\};/.exec(storeCode);
  if (!table) return refuse("np_cfg_store.c has no s_files[NP_CFG_FILE_COUNT] table");
  const rows = new Map<string, string>();
  for (const m of table[1]!.matchAll(/\[\s*(NP_CFG_FILE_\w+)\s*\]\s*=\s*\{\s*(NP_CFG_POLICY_\w+)/g)) {
    rows.set(m[1]!, m[2]!);
  }
  if (rows.size === 0) return refuse("the s_files[] table has no parseable rows");
  const enumBody = /typedef\s+enum\s*\{([\s\S]*?)\}\s*np_cfg_file_t\s*;/.exec(stripNonCode(headerSrc));
  if (!enumBody) return refuse("np_cfg_store.h declares no np_cfg_file_t enum");
  const members = [...enumBody[1]!.matchAll(/\b(NP_CFG_FILE_\w+)/g)]
    .map((m) => m[1]!)
    .filter((n) => n !== "NP_CFG_FILE_COUNT");

  const violations: string[] = [];
  for (const m of members) {
    if (!rows.has(m)) violations.push(`R4 ${STORE_C}: ${m} has no row in s_files[] — every Config file must be kept under a declared policy`);
  }
  const tail = [...rows].filter(([, p]) => p === "NP_CFG_POLICY_TAIL_ADDITIVE").map(([f]) => f).sort();
  const pinned = [...PINNED_TAIL_ADDITIVE].sort();
  if (tail.join(",") !== pinned.join(",")) {
    violations.push(
      `R4 ${STORE_C}: the TAIL_ADDITIVE files are [${tail.join(", ")}], pinned as [${pinned.join(", ")}]. ` +
        "REQ-LFS-01: a file under a tail-additive policy must never hold a value that bounds an emission, " +
        "and moving a file into or out of that policy is a decision to record in NP-SOUP-LFS-001 before it is made here",
    );
  }

  // ── R2 (part): the single opener ──────────────────────────────────────────
  const opener = functionSpan(storeCode, /static\s+np_hub_status_t\s+cfg_open\s*\(/);
  if (!opener) return refuse("np_cfg_store.c has no cfg_open() — the open-handle registry cannot be located");

  // ── R1, R2, R3, R5, R6 across the population ───────────────────────────────
  const files = walk(join(root, "firmware"), []);
  let storeCalls = 0;
  let opencfgInOpener = 0;
  for (const abs of files) {
    const rel = relative(root, abs).split("\\").join("/");
    const src = readFileSync(abs, "utf8");
    const code = stripNonCode(src);

    for (const m of code.matchAll(/\blfs_(\w+)\s*\(/g)) {
      const fn = `lfs_${m[1]}`;
      const at = `${rel}:${lineOf(code, m.index!)}`;
      if (rel === STORE_C) storeCalls++;

      // R1
      if (!(rel in LFS_CALLERS)) {
        violations.push(`R1 ${at}: ${fn}() called outside the littlefs callers — storage goes through np_cfg_store (or a log glue listed in LFS_CALLERS with its reason)`);
        continue;
      }
      // R2
      if (fn === "lfs_file_open") {
        violations.push(`R2 ${at}: lfs_file_open() allocates its cache, and LFS_NO_MALLOC makes that fail — use the store's single opener`);
      }
      if (fn === "lfs_file_opencfg") {
        const inOpener = rel === STORE_C && m.index! > opener[0] && m.index! < opener[1];
        if (inOpener) opencfgInOpener++;
        else violations.push(`R2 ${at}: lfs_file_opencfg() outside cfg_open() — a handle the open-handle registry never saw (OI-LFS-06, upstream 488e84bb)`);
      }
      // R3
      if (rel === STORE_C && (fn === "lfs_remove" || fn === "lfs_rename")) {
        violations.push(`R3 ${at}: ${fn}() in the Config store — create/delete churn is upstream #1210's trigger (OI-LFS-08)`);
      }
      if (rel === STORE_C && fn === "lfs_stat") {
        violations.push(`R3 ${at}: lfs_stat() in the Config store — presence is not durability (upstream #1164, OI-LFS-09)`);
      }
    }

    // R5
    if (rel !== STORE_C && rel !== STORE_H) {
      for (const m of code.matchAll(/\bnp_cfg_store_journal_read\s*\(/g)) {
        if (!(rel in JOURNAL_READERS)) {
          violations.push(
            `R5 ${rel}:${lineOf(code, m.index!)}: np_cfg_store_journal_read() from a file not in JOURNAL_READERS. ` +
              "Map 3's journal is history, never a limit (REQ-LFS-01) — add the file with the reason what it reads is history",
          );
        }
      }
    }

    // R6
    if (isLimitConsumer(rel)) {
      const withStrings = stripComments(src);
      for (const [re, what] of [
        [/\bNP_CFG_FILE_MAP3\b/g, "NP_CFG_FILE_MAP3"],
        [/\bnp_cfg_store_journal_\w+/g, "the journal API"],
        [/map3\.jrn/g, "the journal's path"],
      ] as const) {
        for (const m of withStrings.matchAll(re)) {
          violations.push(`R6 ${rel}:${lineOf(withStrings, m.index!)}: an emission-limit consumer names ${what} — REQ-LFS-01`);
        }
      }
    }
  }

  if (storeCalls === 0) return refuse("np_cfg_store.c calls littlefs nowhere — the scan found nothing to check");
  if (opencfgInOpener !== 1) {
    violations.push(`R2 ${STORE_C}: cfg_open() must contain exactly one lfs_file_opencfg() call, found ${opencfgInOpener}`);
  }
  // A LIMIT_CONSUMERS entry that names nothing would exempt the file it meant
  // to name — silently, for as long as the typo lived.
  for (const c of LIMIT_CONSUMERS) {
    if (!existsSync(join(root, c))) {
      violations.push(`R6 LIMIT_CONSUMERS names ${c}, which does not exist — a renamed consumer has left the rule`);
    }
  }
  for (const reader of Object.keys(JOURNAL_READERS)) {
    if (isLimitConsumer(reader)) violations.push(`R6 ${reader}: listed in JOURNAL_READERS but it is an emission-limit consumer — REQ-LFS-01`);
  }

  // The population actually read, for check-gate-coverage.ts's probe: a gate
  // that passes having scanned nothing is the #118 shape.
  lines.push(`scanned: ${files.length} firmware files`);
  if (violations.length) {
    lines.push(`check-lfs-caller-rules: ${violations.length} violation(s)`);
    for (const v of violations) lines.push("  " + v);
    return { code: 1, lines };
  }
  lines.push(
    `check-lfs-caller-rules: OK — ${files.length} firmware files; littlefs called only by ${Object.keys(LFS_CALLERS).length} listed callers; ` +
      `one opener; no remove/rename/stat in the Config store; tail-additive = [${tail.join(", ")}]; ` +
      `${Object.keys(JOURNAL_READERS).length} journal reader(s); no limit consumer names the journal.`,
  );
  return { code: 0, lines };
}

// ── Self-test ────────────────────────────────────────────────────────────────
// Every rule proven to REJECT, the vacuity paths proven to REFUSE, and prose
// proven not to count. Fixtures are built, never copied from the tree.
if (process.argv.includes("--self-test")) {
  const box = mkdtempSync(join(tmpdir(), "np-lfscallers-"));

  type Tree = Record<string, string>;
  const header = (extra = "") =>
    `typedef enum {\n    NP_CFG_FILE_NPMP = 0,\n    NP_CFG_FILE_MAP3,\n    NP_CFG_FILE_UKMD,\n${extra}    NP_CFG_FILE_COUNT\n} np_cfg_file_t;\n`;
  const store = (o: { npmpPolicy?: string; extraRow?: string; body?: string; opener?: string } = {}) =>
    `static const np_cfg_file_desc_t s_files[NP_CFG_FILE_COUNT] = {\n` +
    `    [NP_CFG_FILE_NPMP] = { ${o.npmpPolicy ?? "NP_CFG_POLICY_REBUILD"}, { "npmp.bin", NULL } },\n` +
    `    [NP_CFG_FILE_MAP3] = { NP_CFG_POLICY_TAIL_ADDITIVE, { "map3.jrn", NULL } },\n` +
    `    [NP_CFG_FILE_UKMD] = { NP_CFG_POLICY_REPLICATED, { "ra/ukmd.rec", "rb/ukmd.rec" } },\n` +
    (o.extraRow ?? "") +
    `};\n\n` +
    `static np_hub_status_t cfg_open(np_cfg_file_t f, unsigned c, lfs_file_t *h, int fl)\n{\n` +
    (o.opener ?? `    int err = lfs_file_opencfg(s_lfs, h, s_files[f].path[c], fl, &fcfg);\n    return err;\n`) +
    `}\n\nstatic int reader(void)\n{\n    lfs_file_read(s_lfs, 0, 0, 0);\n    lfs_file_close(s_lfs, 0);\n${o.body ?? ""}    return 0;\n}\n`;
  const consumers: Tree = Object.fromEntries(
    LIMIT_CONSUMERS.map((c) => [c.endsWith("/") ? `${c}src/placeholder.c` : c, "int c_(void) { return 0; }\n"]),
  );
  const base = (): Tree => ({
    ...consumers,
    [STORE_H]: header(),
    [STORE_C]: store(),
    "firmware/hub_control/src/np_lfs_log_instance.c": "int p(void) { return lfs_fs_gc(0); }\n",
    "firmware/hub_control/src/np_module_map.c": "int m(void) { return np_cfg_store_read(0, 0, 0, 0, 0, 0); }\n",
    "firmware/pbm/src/np_pbm_zone.c": "int z(void) { return 0; }\n",
    "firmware/hub_control/src/np_hub_control_main.c": "int main_(void) { return 0; }\n",
    // Out of population: tests and vendor may do anything.
    "firmware/hub_control/tests/np_cfg_store_tests.c": "int t(void) { lfs_remove(0, \"x\"); return lfs_file_open(0,0,0,0); }\n",
    "firmware/vendor/littlefs/lfs.c": "int lfs_file_open(void) { return lfs_remove(0, 0); }\n",
  });

  const build = (tree: Tree): string => {
    const root = mkdtempSync(join(box, "t-"));
    for (const [p, body] of Object.entries(tree)) {
      mkdirSync(join(root, dirname(p)), { recursive: true });
      writeFileSync(join(root, p), body);
    }
    return root;
  };
  const exec = (root: string) => {
    const r = Bun.spawnSync([process.execPath, import.meta.path, "--root", root], { stdout: "pipe", stderr: "pipe" });
    return { code: r.exitCode, out: new TextDecoder().decode(r.stdout) + new TextDecoder().decode(r.stderr) };
  };
  const failures: string[] = [];
  let cases = 0;
  const expect = (label: string, tree: Tree | string, want: number, needle: string) => {
    cases++;
    const { code, out } = exec(typeof tree === "string" ? tree : build(tree));
    if (code !== want) failures.push(`${label} — expected exit ${want}, got ${code}\n${out}`);
    else if (!out.includes(needle)) failures.push(`${label} — exit ${want} but output lacked ${JSON.stringify(needle)}\n${out}`);
  };
  const edit = (patch: Tree): Tree => ({ ...base(), ...patch });

  expect("the clean tree passes", base(), 0, "OK");

  // R1
  expect("R1 rejects littlefs called from a limit consumer",
    edit({ "firmware/hub_control/src/np_module_map.c": "int m(void) { return lfs_file_read(0,0,0,0); }\n" }), 1, "R1 firmware/hub_control/src/np_module_map.c");
  expect("R1 rejects littlefs called from anywhere unlisted",
    edit({ "firmware/hub_control/src/np_hub_control_main.c": "int main_(void) { return lfs_mount(0,0); }\n" }), 1, "R1 firmware/hub_control/src/np_hub_control_main.c");
  expect("R1 ignores a call named only in a comment",
    edit({ "firmware/hub_control/src/np_hub_control_main.c": "/* lfs_mount(&lfs, &cfg) happens in the store */\nint main_(void) { return 0; }\n" }), 0, "OK");

  // R2
  expect("R2 rejects a second opener in the store",
    edit({ [STORE_C]: store({ body: "    lfs_file_opencfg(s_lfs, 0, \"npmp.bin\", 0, 0);\n" }) }), 1, "outside cfg_open()");
  expect("R2 rejects lfs_file_open in the store",
    edit({ [STORE_C]: store({ body: "    lfs_file_open(s_lfs, 0, \"npmp.bin\", 0);\n" }) }), 1, "R2");
  expect("R2 rejects an opener with no lfs_file_opencfg",
    edit({ [STORE_C]: store({ opener: "    return 0;\n" }) }), 1, "exactly one lfs_file_opencfg()");

  // R3
  for (const fn of ["lfs_remove", "lfs_rename", "lfs_stat"]) {
    expect(`R3 rejects ${fn} in the Config store`,
      edit({ [STORE_C]: store({ body: `    ${fn}(s_lfs, "npmp.tmp", 0);\n` }) }), 1, `R3 ${STORE_C}`);
  }

  // R4
  expect("R4 rejects the rebuild cache moved to tail-additive",
    edit({ [STORE_C]: store({ npmpPolicy: "NP_CFG_POLICY_TAIL_ADDITIVE" }) }), 1, "the TAIL_ADDITIVE files are");
  expect("R4 rejects a second tail-additive file",
    edit({
      [STORE_H]: header("    NP_CFG_FILE_CAL,\n"),
      [STORE_C]: store({ extraRow: `    [NP_CFG_FILE_CAL] = { NP_CFG_POLICY_TAIL_ADDITIVE, { "cal.jrn", NULL } },\n` }),
    }), 1, "the TAIL_ADDITIVE files are");
  expect("R4 rejects a Config file with no policy row",
    edit({ [STORE_H]: header("    NP_CFG_FILE_CAL,\n") }), 1, "NP_CFG_FILE_CAL has no row");

  // R6's population must be real.
  {
    const t = base();
    delete t["firmware/hub_control/src/np_pbm_power_gov.c"];
    expect("R6 rejects a LIMIT_CONSUMERS entry that no longer exists", t, 1, "which does not exist");
  }

  // R5
  expect("R5 rejects an unlisted journal reader",
    edit({ "firmware/hub_control/src/np_hub_control_main.c": "int main_(void) { return np_cfg_store_journal_read(NP_CFG_FILE_MAP3, 0,0,0,0,0,0); }\n" }),
    1, "R5 firmware/hub_control/src/np_hub_control_main.c");

  // R6
  expect("R6 rejects a limit consumer naming the journal id",
    edit({ "firmware/hub_control/src/np_module_map.c": "int m(void) { return NP_CFG_FILE_MAP3; }\n" }), 1, "names NP_CFG_FILE_MAP3");
  expect("R6 rejects a limit consumer naming the journal path in a string",
    edit({ "firmware/pbm/src/np_pbm_zone.c": "const char *p = \"map3.jrn\";\n" }), 1, "names the journal's path");
  expect("R6 ignores the journal named only in a limit consumer's comment",
    edit({ "firmware/pbm/src/np_pbm_zone.c": "/* never read map3.jrn here — REQ-LFS-01 */\nint z(void) { return 0; }\n" }), 0, "OK");

  // Vacuity
  expect("refuses when the store is absent", mkdtempSync(join(box, "empty-")), 2, "refusing to pass vacuously");
  expect("refuses when the file table is absent", edit({ [STORE_C]: "int x;\n" }), 2, "no s_files");
  expect("refuses when the opener is absent",
    edit({ [STORE_C]: store().replace("cfg_open", "open_it") }), 2, "no cfg_open()");
  expect("refuses when the enum is absent", edit({ [STORE_H]: "int y;\n" }), 2, "no np_cfg_file_t");

  rmSync(box, { recursive: true, force: true });
  console.log("check-lfs-caller-rules self-test");
  if (failures.length) {
    console.error(`\nSELF-TEST FAIL — ${failures.length} of ${cases} assertion(s):`);
    for (const f of failures) console.error("  " + f);
    process.exit(1);
  }
  console.log(`  ${cases} case(s): R1–R6 each proven to reject, prose proven not to count,`);
  console.log("  tests/ and vendor/ proven out of population, and 4 vacuity refusals.");
  console.log("SELF-TEST PASS — the checker has teeth.");
  process.exit(0);
}

const { code, lines } = run(ROOT);
for (const l of lines) (code === 0 ? console.log : console.error)(l);
process.exit(code);
