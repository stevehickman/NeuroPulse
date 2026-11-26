#!/usr/bin/env bun
/**
 * check-lfs-caller-rules.ts — the littlefs caller rules stay true of the code.
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-lfs-caller-rules.ts --self-test
 * CI-Scans: every littlefs call site in firmware/, and the Config store's file table and its policies
 * CI-Scan-Paths: firmware/** scripts/check-lfs-caller-rules.ts
 *   (declared here, inside check-gate-coverage's 80-line header window)
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
 *   R4  Every NP_CFG_FILE_* has a row in the Config file table, and every
 *       row's policy is one of ADMITTED_POLICIES (REBUILD, REPLICATED). There
 *       is no tail-additive Config file, so no file is kept under the policy
 *       REQ-LFS-01 forbids a limit in. A tail-additive file returning, a
 *       rebuild cache moved to another policy, or a policy nobody has admitted
 *       is a REQ-LFS-01 decision to record in NP-SOUP-LFS-001 first, and fails
 *       here until it is. (OI-LFS-03; narrowed by OI-NVRAM-21, 2026-11-26,
 *       from "the tail-additive rows are exactly map3.jrn" when that file, never
 *       written, was removed with the store's journal API.)
 *   R5  RETIRED (OI-NVRAM-21). It allowed np_cfg_store_journal_read() and
 *       np_cfg_store_journal_read_rows() to be called only from a listed
 *       JOURNAL_READERS entry, each with why what it read was history. Both
 *       readers are removed with the journal, and R4 admits no policy such a
 *       reader could read. A tail-additive file admitted by a recorded
 *       decision brings a reader rule back with it.
 *   R6  RETIRED (OI-NVRAM-21). It forbade every emission-limit consumer to
 *       name the journal: its file id, its API or its path. None of them
 *       exists, and R4 forbids the policy they belonged to. Map 3's record,
 *       now REPLICATED, has its guard in R8 (OI-LFS-15).
 *   R7  The struct lfs_file_config that cfg_open() passes to
 *       lfs_file_opencfg() has static storage duration: the last argument is
 *       `&name`, and `name` is declared at file scope or `static` inside the
 *       opener. littlefs keeps that pointer and reads attrs/attr_count through
 *       it at every sync and close (lfs.h: "must remain allocated while the
 *       file is open"). A stack copy was the OI-LFS-13 defect (#472): every
 *       close read a dead frame as its attribute list, and LFS_MKTAG's
 *       unmasked size can turn that into a CRC-valid tag of another type.
 *       Anything R7 cannot resolve to a static object — a compound literal, a
 *       pointer parameter, a name it cannot find — fails, rather than being
 *       assumed safe. (OI-LFS-13)
 *   R8  An emission-limit consumer (LIMIT_CONSUMERS) may feed a REPLICATED
 *       record but never read one. (a) It never calls
 *       np_cfg_store_replicated_read(), and names a file id whose row is
 *       REPLICATED only as the first argument of
 *       np_cfg_store_replicated_write(). (b) Every name it uses from an
 *       owner's header (an owner is any firmware .c that calls
 *       np_cfg_store_replicated_read() or _write(); its header is the
 *       same-named .h) is listed in REPLICATED_API_USES for that consumer,
 *       as "write" (it feeds the record and returns nothing read from it)
 *       or "read" with why the value bounds no emission. An entry nothing
 *       uses any more fails, so none pre-authorises a later read. Keyed on
 *       the policy and the owner, never on a record's layout. §6.2 needs
 *       every emission bound in the REBUILD cache, and a REPLICATED record
 *       can come back as an older valid copy (E4, §13.18) that no integrity
 *       check sees. Each REPLICATED row says it bounds no emission
 *       (REQ-LFS-01); R8 makes that true of the limit consumers. (OI-LFS-15)
 *
 * ── The reach, stated narrowly ───────────────────────────────────────────────
 *
 * Textual, like its siblings. R4 reads the policy each row of the file table
 * names; it does not check what a policy's API does, which np_cfg_store_tests
 * case 1 does. R8 sees a name a limit consumer uses, not a value an owner
 * pushes into another module (np_ppx_config.c sets np_ppx_policy's override),
 * and not a value relayed through a file outside LIMIT_CONSUMERS. Whether a
 * REPLICATED record may gate an emission at all is REQ-LFS-01's scope and
 * Firmware's decision (OI-LFS-15; docs/human-input/oi-lfs-15-replicated-
 * records-and-emission.md). Widening to data flow needs a C parser, which
 * this is not.
 *
 *   bun scripts/check-lfs-caller-rules.ts
 *   bun scripts/check-lfs-caller-rules.ts --self-test
 *
 * Exits 1 listing each violation; exits 2 rather than passing vacuously if the
 * store, its file table, its opener or its enum cannot be found.
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
const SIM_STORAGE_C = "firmware/platform_sim/src/np_sim_storage.c";

/** R1 — who may call littlefs, and why. */
const LFS_CALLERS: Record<string, string> = {
  [STORE_C]: "the Config instance's only caller — every caller rule is a property of its API",
  "firmware/hub_control/src/np_lfs_log_instance.c":
    "lfs_fs_gc() in np_lfs_log_prime_allocator() only; the log instances' file I/O is OI-LOG-05..07 and must be added here when it exists",
  [SIM_STORAGE_C]:
    "the simulator's host-only glue for the log partitions (NP-SIM-FW-001 §33, REQ-SIMFW-25): the file layer OI-LOG-05..07 describes, which the device's board layer does not implement yet. It is never built for a device (REQ-SIMFW-01), keeps one open handle per partition, and is the one non-store file allowed lfs_file_opencfg (R2)",
};

/**
 * R4 — the only policies a Config file may be kept under. Admitting another,
 * a tail-additive one above all, is a REQ-LFS-01 decision recorded in
 * NP-SOUP-LFS-001 before it is made here.
 */
const ADMITTED_POLICIES = ["NP_CFG_POLICY_REBUILD", "NP_CFG_POLICY_REPLICATED"];

/**
 * R8 — the files on the path to an emission magnitude. R6's list (retired with
 * the journal, OI-NVRAM-21), restored for OI-LFS-15 and widened from two hub
 * modules to every hub module and the two stimulation libraries, each of which
 * drives an output (light, current, sound). The safety MCU is listed because it
 * owns every limit that is resident, and a stored value reaching it would be
 * the breach. Paths ending in "/" are directory prefixes. An entry that names
 * nothing fails, so a renamed consumer cannot leave the rule silently.
 */
const LIMIT_CONSUMERS = [
  "firmware/hub_control/src/np_module_map.c",
  "firmware/hub_control/src/np_hexmap_nvram.c", // npmp.bin, the emission-path cache (REBUILD)
  "firmware/hub_control/src/np_pbm_power_gov.c",
  "firmware/hub_control/src/np_pbm_cal_bridge.c",
  "firmware/hub_control/src/np_socket_dispatch.c",
  "firmware/hub_control/src/np_session_runner.c",
  "firmware/hub_control/src/np_chan_decl.c",
  "firmware/hub_control/modules/",
  "firmware/cervical_vns/",
  "firmware/sloreta_hdtdcs/",
  "firmware/pbm/",
  "firmware/safety_mcu/",
];

type ReplicatedUse = { kind: "write" | "read"; why: string };

/**
 * R8 — each name a limit consumer uses from a REPLICATED record's owner, with
 * its kind. "write": feeds the record, or is a pure helper over the caller's
 * own values, and returns nothing read from a record. "read": returns a stored
 * value, and `why` says why that value bounds no emission. Adding a "read" here
 * is the REQ-LFS-01 question, asked where it can be asked: a value that bounds
 * an emission belongs in the REBUILD cache, not here.
 */
const REPLICATED_API_USES: Record<string, Record<string, ReplicatedUse>> = {
  "firmware/hub_control/src/np_session_runner.c": {
    // Map 3 (NP_CFG_FILE_MODULE_USAGE), REQ-FWHUB-93: the runner commits the
    // running totals at session end and reads none of them back.
    np_mu_session_begin: { kind: "write", why: "opens the session's tally in RAM" },
    np_mu_session_end: { kind: "write", why: "commits the totals; returns the store status only" },
    np_mu_slot_on: { kind: "write", why: "starts a slot's drive-time count" },
    np_mu_slot_off: { kind: "write", why: "stops a slot's drive-time count" },
    np_mu_mark_present: { kind: "write", why: "counts a module present this session" },
    np_mu_add_used: { kind: "write", why: "adds the session's drive time" },
    np_mu_add_dose: { kind: "write", why: "adds the session's metered dose" },
    np_mu_note_throttle: { kind: "write", why: "counts a throttle" },
    np_mu_key_uid: { kind: "write", why: "pure: builds a row key from the caller's UID; reads no record" },
    np_mu_key_port: { kind: "write", why: "pure: builds a row key from the caller's slot and type; reads no record" },
    np_mu_dose_mJ_cm2: { kind: "write", why: "pure: converts the caller's own session dose; reads no record" },
    // Consumables (NP_CFG_FILE_CONSUMABLES), OI-ACC-08.
    np_cons_on_session_end: { kind: "write", why: "counts the session against each consumable; returns nothing" },
    // Pause limit (NP_CFG_FILE_PAUSE_LIMIT), SWR-SS-05.
    np_pause_limit_seconds: {
      kind: "read",
      why:
        "how long a paused session may stay paused before it ends. It is read when a pause starts or restarts, with " +
        "every enable withdrawn (do_pause, resume), and its expiry ends a session that is emitting nothing, so an " +
        "older or lost value changes when an idle session closes, never how much is emitted",
    },
  },
  "firmware/hub_control/modules/np_mod_cvns.c": {
    // Cardiac cutoff sensitivity (NP_CFG_FILE_CARDIAC_SENS), REQ-FWHUB-74, REQ-CVNS-14.
    np_cardiac_sens_level: {
      kind: "read",
      why:
        "relayed to the safety MCU, which range-checks it. Level 0 is the validated cutoff and no level is less " +
        "sensitive, so any value read, older or lost (0), leaves the cutoff at least as protective as validated; " +
        "the MCU lowers a level only at a new cervical VNS request",
    },
  },
};

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

/** The function and extern object names a header declares (comments and strings already stripped). */
function headerApi(code: string): Set<string> {
  const names = new Set<string>();
  for (const m of code.matchAll(/\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*;/g)) names.add(m[1]!);
  for (const m of code.matchAll(/\bextern\s+[^;(){}]*?\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*;/g)) names.add(m[1]!);
  for (const kw of ["if", "while", "for", "switch", "return", "sizeof", "defined", "void", "char", "short", "int", "long",
    "float", "double", "signed", "unsigned", "const", "volatile", "static", "struct", "enum", "union", "typedef", "bool"]) {
    names.delete(kw);
  }
  return names;
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
  const table = /CFG_FILES\s*\[\s*NP_CFG_FILE_COUNT\s*\]\s*=\s*\{([\s\S]*?)\n\};/.exec(storeCode);
  if (!table) return refuse("np_cfg_store.c has no CFG_FILES[NP_CFG_FILE_COUNT] table");
  const rows = new Map<string, string>();
  for (const m of table[1]!.matchAll(/\[\s*(NP_CFG_FILE_\w+)\s*\]\s*=\s*\{\s*(NP_CFG_POLICY_\w+)/g)) {
    rows.set(m[1]!, m[2]!);
  }
  if (rows.size === 0) return refuse("the CFG_FILES[] table has no parseable rows");
  const enumBody = /typedef\s+enum\s*\{([\s\S]*?)\}\s*np_cfg_file_t\s*;/.exec(stripNonCode(headerSrc));
  if (!enumBody) return refuse("np_cfg_store.h declares no np_cfg_file_t enum");
  const members = [...enumBody[1]!.matchAll(/\b(NP_CFG_FILE_\w+)/g)]
    .map((m) => m[1]!)
    .filter((n) => n !== "NP_CFG_FILE_COUNT");

  const violations: string[] = [];
  for (const m of members) {
    if (!rows.has(m)) violations.push(`R4 ${STORE_C}: ${m} has no row in CFG_FILES[] — every Config file must be kept under a declared policy`);
  }
  for (const [f, p] of rows) {
    if (!ADMITTED_POLICIES.includes(p)) {
      violations.push(
        `R4 ${STORE_C}: ${f} is kept under ${p}, which is not an admitted policy [${ADMITTED_POLICIES.join(", ")}]. ` +
          "REQ-LFS-01: no value that bounds an emission may be stored under a tail-additive policy, and the store has none; " +
          "admitting a policy is a decision to record in NP-SOUP-LFS-001 before it is made here",
      );
    }
  }

  // ── R2 (part): the single opener ──────────────────────────────────────────
  const opener = functionSpan(storeCode, /static\s+np_hub_status_t\s+cfg_open\s*\(/);
  if (!opener) return refuse("np_cfg_store.c has no cfg_open() — the open-handle registry cannot be located");

  // ── R7: the file config outlives the handle ────────────────────────────────
  const openerBody = storeCode.slice(opener[0], opener[1]);
  const call = /\blfs_file_opencfg\s*\(/.exec(openerBody);
  if (call) {
    const at = `${STORE_C}:${lineOf(storeCode, opener[0] + call.index)}`;
    const args: string[] = [];
    let depth = 0;
    let cur = "";
    for (let i = call.index + call[0].length; i < openerBody.length; i++) {
      const c = openerBody[i]!;
      if ((c === "," || c === ")") && depth === 0) {
        args.push(cur.trim());
        cur = "";
        if (c === ")") break;
        continue;
      }
      if (c === "(" || c === "[" || c === "{") depth++;
      if (c === ")" || c === "]" || c === "}") depth--;
      cur += c;
    }
    const last = args[args.length - 1] ?? "";
    const named = /^&\s*([A-Za-z_]\w*)$/.exec(last);
    if (args.length !== 5 || !named) {
      violations.push(
        `R7 ${at}: lfs_file_opencfg()'s config argument is \`${last}\`, not \`&name\` — its storage duration cannot be established, ` +
          "and littlefs reads it again at every sync and close (OI-LFS-13)",
      );
    } else {
      const id = named[1]!;
      const decl = new RegExp(`\\b(static\\s+)?(?:const\\s+)?struct\\s+lfs_file_config\\s+${id}\\b`);
      const local = decl.exec(openerBody);
      let fileScope: RegExpExecArray | null = null;
      for (const m of storeCode.matchAll(new RegExp(decl.source, "g"))) {
        const before = storeCode.slice(0, m.index!);
        if ((before.match(/\{/g) ?? []).length === (before.match(/\}/g) ?? []).length) {
          fileScope = m as RegExpExecArray;
          break;
        }
      }
      if (local && !local[1]) {
        violations.push(
          `R7 ${at}: \`${id}\` is an automatic struct lfs_file_config in cfg_open() — littlefs keeps the pointer and reads it at every ` +
            "sync and close, after this frame is gone. Declare it at file scope or `static` (OI-LFS-13, #472)",
        );
      } else if (!local && !fileScope) {
        violations.push(
          `R7 ${at}: no declaration of \`${id}\` as a struct lfs_file_config at file scope or static in cfg_open() — ` +
            "its storage duration cannot be established (OI-LFS-13)",
        );
      }
    }
  }

  // ── R1, R2, R3 across the population ───────────────────────────────────────
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
        if (rel === SIM_STORAGE_C) { /* the twin's host-only log glue: its own static file configurations, one handle per partition */ }
        else if (inOpener) opencfgInOpener++;
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

  }

  // ── R8: limit consumers feed REPLICATED records and never read one ──────────
  const replicatedIds = [...rows].filter(([, p]) => p === "NP_CFG_POLICY_REPLICATED").map(([f]) => f);
  const codeOf = new Map<string, string>();
  for (const abs of files) codeOf.set(relative(root, abs).split("\\").join("/"), stripNonCode(readFileSync(abs, "utf8")));
  // Owners: every .c but the store, and but a limit consumer (which (a) holds to
  // writing), that reads or writes a REPLICATED record.
  const ownerApi = new Map<string, string>(); // declared name -> owner header
  const owners: string[] = [];
  for (const [rel, code] of codeOf) {
    if (rel === STORE_C || !rel.endsWith(".c") || isLimitConsumer(rel)) continue;
    if (!/\bnp_cfg_store_replicated_(?:read|write)\s*\(/.test(code)) continue;
    owners.push(rel);
    const base = rel.slice(rel.lastIndexOf("/") + 1).replace(/\.c$/, ".h");
    const hdr = [...codeOf.keys()].filter((r) => r.endsWith("/" + base));
    if (hdr.length !== 1) {
      violations.push(
        `R8 ${rel}: owns a REPLICATED record but ${hdr.length === 0 ? "has no" : "has more than one"} ${base} — ` +
          "its API cannot be listed, so what a limit consumer reads from it cannot be checked (OI-LFS-15)",
      );
      continue;
    }
    for (const n of headerApi(codeOf.get(hdr[0]!)!)) ownerApi.set(n, hdr[0]!);
  }
  if (replicatedIds.length > 0 && owners.length === 0) {
    return refuse("the store has REPLICATED rows but no firmware file reads or writes one — R8 found no owner to check");
  }
  const used = new Set<string>();
  for (const [rel, code] of codeOf) {
    if (!isLimitConsumer(rel)) continue;
    // (a) the store's own API, keyed on the policy each row names
    for (const m of code.matchAll(/\bnp_cfg_store_replicated_read\b/g)) {
      violations.push(
        `R8 ${rel}:${lineOf(code, m.index!)}: an emission-limit consumer reads a REPLICATED record (np_cfg_store_replicated_read) — ` +
          "a value that bounds an emission is kept in the REBUILD cache, and a REPLICATED copy can come back older and valid (E4); REQ-LFS-01, OI-LFS-15",
      );
    }
    for (const id of replicatedIds) {
      for (const m of code.matchAll(new RegExp(`\\b${id}\\b`, "g"))) {
        if (/\bnp_cfg_store_replicated_write\s*\(\s*$/.test(code.slice(Math.max(0, m.index! - 80), m.index!))) continue;
        violations.push(
          `R8 ${rel}:${lineOf(code, m.index!)}: an emission-limit consumer names ${id}, a REPLICATED record, other than as ` +
            "the file np_cfg_store_replicated_write() writes — REQ-LFS-01, OI-LFS-15",
        );
      }
    }
    // (b) an owner's API, unless this consumer is listed for that name
    const allowed = REPLICATED_API_USES[rel] ?? {};
    for (const m of code.matchAll(/\b([A-Za-z_]\w*)\b/g)) {
      const name = m[1]!;
      const hdr = ownerApi.get(name);
      if (!hdr) continue;
      if (Object.hasOwn(allowed, name)) { used.add(`${rel}\u0000${name}`); continue; }
      violations.push(
        `R8 ${rel}:${lineOf(code, m.index!)}: an emission-limit consumer uses ${name} from ${hdr}, the API of a REPLICATED ` +
          "record's owner, and REPLICATED_API_USES does not list it for this file. A writer is listed as \"write\"; " +
          "a reader only with why the value bounds no emission, and one that bounds an emission belongs in the REBUILD cache (REQ-LFS-01, OI-LFS-15)",
      );
    }
  }
  for (const [rel, uses] of Object.entries(REPLICATED_API_USES)) {
    for (const name of Object.keys(uses)) {
      if (!used.has(`${rel}\u0000${name}`)) {
        violations.push(
          `R8 REPLICATED_API_USES lists ${name} for ${rel}, which ` +
            (isLimitConsumer(rel) ? "does not use it from a REPLICATED record's owner" : "is not an emission-limit consumer") +
            " — remove the stale entry, so it cannot pre-authorise a later read (OI-LFS-15)",
        );
      }
    }
  }
  for (const c of LIMIT_CONSUMERS) {
    if (!existsSync(join(root, c))) {
      violations.push(`R8 LIMIT_CONSUMERS names ${c}, which does not exist — a renamed consumer has left the rule`);
    }
  }

  if (storeCalls === 0) return refuse("np_cfg_store.c calls littlefs nowhere — the scan found nothing to check");
  if (opencfgInOpener !== 1) {
    violations.push(`R2 ${STORE_C}: cfg_open() must contain exactly one lfs_file_opencfg() call, found ${opencfgInOpener}`);
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
      `one opener; no remove/rename/stat in the Config store; ${rows.size} Config files, each under an admitted policy ` +
      `[${ADMITTED_POLICIES.join(", ")}] (no tail-additive file); the opener's file config is static; ` +
      `${LIMIT_CONSUMERS.length} emission-limit consumer entries read none of ${replicatedIds.length} REPLICATED records ` +
      `(${owners.length} owners; ${Object.values(REPLICATED_API_USES).reduce((a, u) => a + Object.keys(u).length, 0)} listed uses).`,
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
    `typedef enum {\n    NP_CFG_FILE_NPMP = 0,\n    NP_CFG_FILE_UKMD,\n    NP_CFG_FILE_MODULE_USAGE,\n${extra}    NP_CFG_FILE_COUNT\n} np_cfg_file_t;\n`;
  const store = (o: { npmpPolicy?: string; extraRow?: string; body?: string; opener?: string; fcfg?: string } = {}) =>
    `static const np_cfg_file_desc_t CFG_FILES[NP_CFG_FILE_COUNT] = {\n` +
    `    [NP_CFG_FILE_NPMP] = { ${o.npmpPolicy ?? "NP_CFG_POLICY_REBUILD"}, { "npmp.bin", NULL } },\n` +
    `    [NP_CFG_FILE_UKMD] = { NP_CFG_POLICY_REPLICATED, { "ra/ukmd.rec", "rb/ukmd.rec" } },\n` +
    `    [NP_CFG_FILE_MODULE_USAGE] = { NP_CFG_POLICY_REPLICATED, { "ra/modusage.rec", "rb/modusage.rec" } },\n` +
    (o.extraRow ?? "") +
    `};\n\n` +
    (o.fcfg ?? `static struct lfs_file_config s_fcfg;\n\n`) +
    `static np_hub_status_t cfg_open(np_cfg_file_t f, unsigned c, lfs_file_t *h, int fl)\n{\n` +
    (o.opener ?? `    int err = lfs_file_opencfg(s_lfs, h, CFG_FILES[f].path[c], fl, &s_fcfg);\n    return err;\n`) +
    `}\n\nstatic int reader(void)\n{\n    lfs_file_read(s_lfs, 0, 0, 0);\n    lfs_file_close(s_lfs, 0);\n${o.body ?? ""}    return 0;\n}\n`;
  // R8 fixtures: one owner module whose header declares every name
  // REPLICATED_API_USES lists, plus Map 3's reader np_mu_snapshot and an extern;
  // each listed consumer uses exactly its listed names.
  const OWNER_C = "firmware/hub_control/src/np_module_usage.c";
  const OWNER_H = "firmware/hub_control/include/np_module_usage.h";
  const listedNames = [...new Set(Object.values(REPLICATED_API_USES).flatMap((u) => Object.keys(u)))];
  const ownerH = (extra = "") =>
    listedNames.map((n) => `unsigned ${n}(const void *a, unsigned n);\n`).join("") +
    "np_hub_status_t np_mu_snapshot(np_mu_row_t *out, size_t cap, size_t *n_out);\n" +
    "extern const unsigned np_mu_totals_ro[4];\n" + extra;
  const ownerC = "unsigned w(void) { return np_cfg_store_replicated_write(NP_CFG_FILE_MODULE_USAGE, 0, 0); }\n" +
    "unsigned r(void) { return np_cfg_store_replicated_read(NP_CFG_FILE_MODULE_USAGE, 0, 0); }\n";
  const consumerBody = (rel: string, extra = "", drop = "") =>
    "#include \"np_module_usage.h\"\nvoid c_(void)\n{\n" +
    Object.keys(REPLICATED_API_USES[rel] ?? {}).filter((n) => n !== drop).map((n) => `    (void)${n}(0, 0);\n`).join("") +
    extra + "}\n";
  const consumers = (): Tree => Object.fromEntries(
    LIMIT_CONSUMERS.map((c) => [c.endsWith("/") ? `${c}src/placeholder.c` : c, "int c_(void) { return 0; }\n"]),
  );
  const RUNNER = "firmware/hub_control/src/np_session_runner.c";
  const GOV = "firmware/hub_control/src/np_pbm_power_gov.c";
  const HEXNV = "firmware/hub_control/src/np_hexmap_nvram.c";
  const base = (): Tree => ({
    ...consumers(),
    ...Object.fromEntries(Object.keys(REPLICATED_API_USES).map((rel) => [rel, consumerBody(rel)])),
    [OWNER_C]: ownerC,
    [OWNER_H]: ownerH(),
    [HEXNV]: "int h(void) { return np_cfg_store_read(NP_CFG_FILE_NPMP, 0, 0, 0, 0, 0); }\n",
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

  // R4 — no Config file outside the admitted policies (REQ-LFS-01; OI-NVRAM-21)
  expect("R4 rejects the rebuild cache moved to tail-additive",
    edit({ [STORE_C]: store({ npmpPolicy: "NP_CFG_POLICY_TAIL_ADDITIVE" }) }), 1,
    "NP_CFG_FILE_NPMP is kept under NP_CFG_POLICY_TAIL_ADDITIVE, which is not an admitted policy");
  expect("R4 rejects a tail-additive journal coming back (the removed map3.jrn)",
    edit({
      [STORE_H]: header("    NP_CFG_FILE_MAP3,\n"),
      [STORE_C]: store({ extraRow: `    [NP_CFG_FILE_MAP3] = { NP_CFG_POLICY_TAIL_ADDITIVE, { "map3.jrn", NULL } },\n` }),
    }), 1, "NP_CFG_FILE_MAP3 is kept under NP_CFG_POLICY_TAIL_ADDITIVE");
  expect("R4 rejects a policy nobody admitted, whatever its name",
    edit({
      [STORE_H]: header("    NP_CFG_FILE_CAL,\n"),
      [STORE_C]: store({ extraRow: `    [NP_CFG_FILE_CAL] = { NP_CFG_POLICY_APPEND_LOG, { "cal.log", NULL } },\n` }),
    }), 1, "NP_CFG_FILE_CAL is kept under NP_CFG_POLICY_APPEND_LOG");
  expect("R4 accepts a new file under an admitted policy",
    edit({
      [STORE_H]: header("    NP_CFG_FILE_CAL,\n"),
      [STORE_C]: store({ extraRow: `    [NP_CFG_FILE_CAL] = { NP_CFG_POLICY_REPLICATED, { "ra/cal.rec", "rb/cal.rec" } },\n` }),
    }), 0, "OK");
  expect("R4 rejects a Config file with no policy row",
    edit({ [STORE_H]: header("    NP_CFG_FILE_CAL,\n") }), 1, "NP_CFG_FILE_CAL has no row");
  expect("R4 ignores a tail-additive row that is only a comment",
    edit({ [STORE_C]: store({ extraRow: `    /* [NP_CFG_FILE_MAP3] = { NP_CFG_POLICY_TAIL_ADDITIVE, { "map3.jrn", NULL } }, removed */\n` }) }),
    0, "OK");

  // R7 — the file config outlives the handle (OI-LFS-13)
  const opencfg = (arg: string, decl = "") =>
    `${decl}    int err = lfs_file_opencfg(s_lfs, h,\n        CFG_FILES[f].path[c], fl,\n        ${arg});\n    return err;\n`;
  expect("R7 rejects a stack config — the #472 shape",
    edit({ [STORE_C]: store({ fcfg: "", opener: opencfg("&fcfg", "    struct lfs_file_config fcfg;\n    memset(&fcfg, 0, sizeof(fcfg));\n") }) }),
    1, "is an automatic struct lfs_file_config");
  expect("R7 accepts a function-scope static",
    edit({ [STORE_C]: store({ fcfg: "", opener: opencfg("&fcfg", "    static struct lfs_file_config fcfg;\n") }) }), 0, "OK");
  expect("R7 accepts a file-scope config without `static`",
    edit({ [STORE_C]: store({ fcfg: "struct lfs_file_config g_fcfg;\n\n", opener: opencfg("&g_fcfg") }) }), 0, "OK");
  expect("R7 rejects a compound literal",
    edit({ [STORE_C]: store({ opener: opencfg("&(struct lfs_file_config){ .buffer = s_cache }") }) }), 1, "not `&name`");
  expect("R7 rejects a config passed through a pointer",
    edit({ [STORE_C]: store({ opener: opencfg("cfgp") }) }), 1, "not `&name`");
  expect("R7 rejects a name it cannot find",
    edit({ [STORE_C]: store({ opener: opencfg("&nowhere") }) }), 1, "no declaration of `nowhere`");
  expect("R7 is not satisfied by `static` in a comment",
    edit({ [STORE_C]: store({ fcfg: "", opener: opencfg("&fcfg", "    /* static */ struct lfs_file_config fcfg;\n") }) }),
    1, "is an automatic struct lfs_file_config");
  expect("R7 is not satisfied by a same-named static in another function",
    edit({ [STORE_C]: store({ fcfg: "", opener: opencfg("&fcfg", "    struct lfs_file_config fcfg;\n"),
      body: "    static struct lfs_file_config fcfg;\n" }) }),
    1, "is an automatic struct lfs_file_config");

  // R8 — a limit consumer feeds a REPLICATED record and never reads one (OI-LFS-15)
  expect("R8 rejects a limit consumer reading Map 3's running totals through its owner",
    edit({ [RUNNER]: consumerBody(RUNNER, "    np_mu_row_t rows[4]; size_t n; (void)np_mu_snapshot(rows, 4, &n);\n") }), 1,
    `R8 ${RUNNER}:`);
  expect("R8 names the read it rejects",
    edit({ [RUNNER]: consumerBody(RUNNER, "    (void)np_mu_snapshot(0, 0, 0);\n") }), 1, "uses np_mu_snapshot from " + OWNER_H);
  expect("R8 rejects an owner's read taken as a function pointer, not called",
    edit({ [GOV]: "#include \"np_module_usage.h\"\nstatic void *fp = (void *)&np_mu_snapshot;\n" }), 1, `R8 ${GOV}:2`);
  expect("R8 rejects an owner's extern object read by a limit consumer",
    edit({ [GOV]: "int g(void) { return (int)np_mu_totals_ro[0]; }\n" }), 1, "uses np_mu_totals_ro");
  expect("R8 allows the listed writer: the session runner commits Map 3's totals",
    edit({ [RUNNER]: consumerBody(RUNNER, "    (void)np_mu_session_end(1000);\n    (void)np_mu_add_dose(0, 5);\n") }), 0, "OK");
  expect("R8 allows a limit consumer to write a REPLICATED record through the store",
    edit({ [RUNNER]: consumerBody(RUNNER, "    (void)np_cfg_store_replicated_write(NP_CFG_FILE_MODULE_USAGE, 0, 0);\n") }), 0, "OK");
  expect("R8 holds a writer to the consumer it is listed for (the runner's writes do not license the governor's)",
    edit({ [GOV]: "#include \"np_module_usage.h\"\nvoid g(void) { (void)np_mu_add_dose(0, 5); }\n" }), 1,
    `R8 ${GOV}:2: an emission-limit consumer uses np_mu_add_dose`);
  expect("R8 rejects np_cfg_store_replicated_read() in a limit consumer",
    edit({ [GOV]: "int g(void) { return np_cfg_store_replicated_read(NP_CFG_FILE_MODULE_USAGE, 0, 0); }\n" }), 1,
    "reads a REPLICATED record (np_cfg_store_replicated_read)");
  expect("R8 rejects a REPLICATED file id handed to another store reader",
    edit({ [GOV]: "int g(void) { return np_cfg_store_read(NP_CFG_FILE_MODULE_USAGE, 0, 0, 0, 0, 0); }\n" }), 1,
    "names NP_CFG_FILE_MODULE_USAGE, a REPLICATED record");
  expect("R8 is keyed on the policy: the REBUILD cache read by its limit consumer passes",
    base(), 0, "OK");
  expect("R8 is keyed on the policy: the same read fails once that file's row is REPLICATED",
    edit({ [STORE_C]: store({ npmpPolicy: "NP_CFG_POLICY_REPLICATED" }) }), 1, `names NP_CFG_FILE_NPMP, a REPLICATED record`);
  expect("R8 finds a new owner by its store calls and holds its reader to the rule",
    edit({
      [STORE_H]: header("    NP_CFG_FILE_CAL,\n"),
      [STORE_C]: store({ extraRow: `    [NP_CFG_FILE_CAL] = { NP_CFG_POLICY_REPLICATED, { "ra/cal.rec", "rb/cal.rec" } },\n` }),
      "firmware/hub_control/src/np_cal_rec.c": "int k(void) { return np_cfg_store_replicated_read(NP_CFG_FILE_CAL, 0, 0); }\n",
      "firmware/hub_control/include/np_cal_rec.h": "unsigned np_cal_rec_gain(void);\n",
      "firmware/hub_control/modules/np_mod_pbm.c": "unsigned d(void) { return np_cal_rec_gain(); }\n",
    }), 1, "uses np_cal_rec_gain from firmware/hub_control/include/np_cal_rec.h");
  expect("R8 rejects an owner whose API has no header to read",
    edit({ "firmware/hub_control/src/np_cal_rec.c": "int k(void) { return np_cfg_store_replicated_write(NP_CFG_FILE_UKMD, 0, 0); }\n" }), 1,
    "owns a REPLICATED record but has no np_cal_rec.h");
  expect("R8 rejects a stale REPLICATED_API_USES entry",
    edit({ [RUNNER]: consumerBody(RUNNER, "", "np_pause_limit_seconds") }), 1,
    `lists np_pause_limit_seconds for ${RUNNER}, which does not use it`);
  expect("R8 rejects a LIMIT_CONSUMERS entry that no longer exists",
    (() => { const t = base(); delete t[GOV]; return t; })(), 1, `LIMIT_CONSUMERS names ${GOV}, which does not exist`);
  expect("R8 ignores an owner's read named only in a limit consumer's comment",
    edit({ [GOV]: "/* np_mu_snapshot(rows) is the app's, never ours */\nint g(void) { return 0; }\n" }), 0, "OK");
  expect("R8 does not reach a file outside LIMIT_CONSUMERS",
    edit({ "firmware/hub_control/src/np_gatt_server.c": "#include \"np_module_usage.h\"\nint s(void) { return np_mu_snapshot(0, 0, 0); }\n" }), 0, "OK");
  expect("refuses when REPLICATED rows exist but no owner does",
    (() => { const t = base(); delete t[OWNER_C]; return t; })(), 2, "R8 found no owner to check");

  // Vacuity
  expect("refuses when the store is absent", mkdtempSync(join(box, "empty-")), 2, "refusing to pass vacuously");
  expect("refuses when the file table is absent", edit({ [STORE_C]: "int x;\n" }), 2, "no CFG_FILES");
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
  console.log(`  ${cases} case(s): R1–R4, R7 and R8 each proven to reject (R5, R6 retired), R8 proven to allow a listed`);
  console.log("  writer, prose proven not to count, tests/ and vendor/ proven out of population, and 5 vacuity refusals.");
  console.log("SELF-TEST PASS — the checker has teeth.");
  process.exit(0);
}

const { code, lines } = run(ROOT);
for (const l of lines) (code === 0 ? console.log : console.error)(l);
process.exit(code);
