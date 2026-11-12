#!/usr/bin/env bun
/**
 * check-sim-seam-routes.ts — every seam of the SW-02 platform contract is accounted for in the twin.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-sim-seam-routes.ts --self-test
 * CI-Scans: the SW-02 platform contract (np_platform_stub.c, NP_SW02_PLATFORM_SYMBOL_COUNT) against firmware/platform_sim/np_sim_seam_routes.txt, the sim layer's definitions and the sources the twin links
 * CI-Scan-Paths: firmware/** scripts/check-sim-seam-routes.ts
 *
 * NP-SIM-FW-001 §30, REQ-SIMFW-02 (OI-SIMFW-02, OI-SIMFW-06, OI-SIMFW-14).
 *
 * ── What it asserts ──────────────────────────────────────────────────────────────────────────────
 *
 * The contract is the set of global functions firmware/platform/src/np_platform_stub.c defines. Their
 * number is NP_SW02_PLATFORM_SYMBOL_COUNT, which the cross-build census asserts with `nm`
 * (firmware/cmake/np_platform_census.cmake). This gate parses the same file, and a parse that does not
 * find exactly that many definitions fails, so the set here is tied to the census and not to this
 * file's reading of C.
 *
 *   1. ONE ROW EACH. firmware/platform_sim/np_sim_seam_routes.txt has exactly one row for every
 *      contract seam and no row for anything else. A seam added to the contract with no row fails; a
 *      seam removed from the contract with its row left behind fails.
 *   2. THE ROW IS TRUE. `model`, `infra` and `unrouted` rows: the seam is defined exactly once in
 *      firmware/platform_sim/src, in the file the row names, and its body reports itself `unrouted`
 *      exactly when the row says so. `firmware-host` rows: not defined in the sim layer, defined in the
 *      device source the row names, and that source is on the twin's link line. `absent` rows: defined
 *      nowhere in the twin, and no source on the twin's link line names the seam (so the row cannot go
 *      stale: the day a linked source calls it, this fails before the link does).
 *   3. NOTHING ELSE. A contract seam defined in the sim layer with an `absent` row, or defined twice,
 *      fails.
 *   4. THE ADDRESS SPACE IS STATED (OI-SIMFW-06). The `space` column is from a closed list, and where
 *      the contract's first parameter is an address (socket_id, pair, side, slot, ch, part, id) the row
 *      must name that space and no other, so a socket-index seam cannot be routed as a pair.
 *   5. (--lib <archive>) THE BUILT LIBRARY AGREES. `nm` over the built libnp_sim_pbm.a: every defined
 *      row is a text symbol in exactly one member, the member the row names, and no absent seam is
 *      defined or referenced by any member.
 *
 * ── What it does NOT assert, and why ─────────────────────────────────────────────────────────────
 *
 * No count. The census constant counts the contract; this gate prints how the contract splits by
 * disposition and asserts no figure for any of them, because the split is a fact of the tree on the
 * day and a constant for it would have no derivation (CLAUDE.md §18, §19).
 *
 * Not that REQ-SIMFW-02 is met as worded. The requirement says every seam has one sim definition and
 * one route. `absent` and `unrouted` rows are the seams for which that is not yet true, each with the
 * open item that would make it true. The gate keeps that list exact; it does not shorten it.
 *
 * Usage: bun scripts/check-sim-seam-routes.ts [--lib <path to libnp_sim_pbm.a>] [--self-test]
 */
import { readFileSync, readdirSync, mkdtempSync, mkdirSync, writeFileSync, rmSync, existsSync } from "fs";
import { join, basename, dirname, normalize } from "path";
import { tmpdir } from "os";
import { spawnSync } from "child_process";

const STUB = "firmware/platform/src/np_platform_stub.c";
const HEADER = "firmware/platform/include/np_sw02_platform_hal.h";
const SIM_DIR = "firmware/platform_sim";
const SIM_SRC = "firmware/platform_sim/src";
const SIM_CMAKE = "firmware/platform_sim/CMakeLists.txt";
const TABLE = "firmware/platform_sim/np_sim_seam_routes.txt";

const DISPOSITIONS = ["model", "infra", "unrouted", "firmware-host", "absent"] as const;
type Disposition = (typeof DISPOSITIONS)[number];
const SPACES = ["socket-index", "pair", "side", "slot", "channel", "partition", "char-id", "none"];
/** The contract's own parameter names that are addresses, and the space each one is. */
const PARAM_SPACE: Record<string, string> = {
  socket_id: "socket-index", pair: "pair", side: "side", slot: "slot", slot_index: "slot",
  ch: "channel", part: "partition", id: "char-id",
};

type Def = { name: string; file: string; firstParam: string; body: string; isStatic: boolean };
type Row = { seam: string; disposition: string; where: string; space: string; item: string; line: number };

/** Strip comments and string literals so neither can contribute a match. */
function decomment(src: string): string {
  return src
    .replace(/\/\*[\s\S]*?\*\//g, (m) => m.replace(/[^\n]/g, " "))
    .replace(/\/\/[^\n]*/g, " ")
    .replace(/"(\\.|[^"\\])*"/g, '""');
}

/** Top-level function definitions: a declarator that starts in column 0 and is followed by a body. */
export function parseDefs(file: string, text: string): Def[] {
  const src = decomment(text);
  const out: Def[] = [];
  const re = /^([A-Za-z_][\w \t*]*?)\b([A-Za-z_]\w*)[ \t]*\(([^;{}()]*)\)\s*\{/gm;
  let m: RegExpExecArray | null;
  while ((m = re.exec(src)) !== null) {
    const prefix = m[1]!;
    if (/^\s*(if|for|while|switch|return|else|do)\b/.test(prefix) || prefix.trim() === "") continue;
    const open = m.index + m[0].length - 1;
    let depth = 0;
    let end = open;
    for (; end < src.length; end++) {
      if (src[end] === "{") depth++;
      else if (src[end] === "}" && --depth === 0) break;
    }
    const params = m[3]!.trim();
    const p0 = params.split(",")[0]!.trim();
    const firstParam = p0 === "void" || p0 === "" ? "" : (p0.split(/[\s*]+/).pop() ?? "").replace(/\[.*$/, "");
    out.push({ name: m[2]!, file, firstParam, body: src.slice(open, end + 1), isStatic: /\bstatic\b/.test(prefix) });
    re.lastIndex = end;
  }
  return out;
}

export function parseTable(text: string): { rows: Row[]; errors: string[] } {
  const rows: Row[] = [];
  const errors: string[] = [];
  text.split("\n").forEach((raw, i) => {
    const line = raw.trim();
    if (line === "" || line.startsWith("#")) return;
    const f = line.split(/\s+/);
    if (f.length !== 5) { errors.push(`${TABLE}:${i + 1}: a row has 5 columns (seam disposition where space item), this one has ${f.length}`); return; }
    rows.push({ seam: f[0]!, disposition: f[1]!, where: f[2]!, space: f[3]!, item: f[4]!, line: i + 1 });
  });
  return { rows, errors };
}

/** The device sources on the twin's link line: every `../<dir>/….c` the sim CMake file names. */
export function linkedFirmwareSources(cmake: string): string[] {
  const noComments = cmake.split("\n").map((l) => l.replace(/#.*$/, "")).join("\n");
  const out = new Set<string>();
  for (const m of noComments.matchAll(/(?<![\w/.])\.\.\/[\w./-]+\.c\b/g)) out.add(m[0]);
  return [...out].sort();
}

type Tree = {
  stub: string; header: string; table: string; cmake: string;
  simSources: Record<string, string>;                 // basename -> text, firmware/platform_sim/src/*.c
  readFirmware: (relToSim: string) => string | null;  // "../hub_control/src/x.c" -> text
};

export function check(t: Tree): { errors: string[]; counts: Record<string, number>; contract: number } {
  const errors: string[] = [];
  const counts: Record<string, number> = {};

  const cm = /#define[ \t]+NP_SW02_PLATFORM_SYMBOL_COUNT[ \t]+(\d+)/.exec(t.header);
  if (!cm) return { errors: [`${HEADER}: NP_SW02_PLATFORM_SYMBOL_COUNT not found`], counts, contract: 0 };
  const expected = Number(cm[1]);
  const contractDefs = parseDefs(STUB, t.stub).filter((d) => !d.isStatic);
  const contract = new Map(contractDefs.map((d) => [d.name, d]));
  if (contractDefs.length !== expected || contract.size !== expected) {
    errors.push(`${STUB}: parsed ${contractDefs.length} global definitions (${contract.size} distinct), NP_SW02_PLATFORM_SYMBOL_COUNT says ${expected}. ` +
      `The census asserts the constant with nm; this gate must see the same set before it compares anything.`);
    return { errors, counts, contract: contract.size };
  }

  const { rows, errors: tableErrors } = parseTable(t.table);
  errors.push(...tableErrors);
  const byName = new Map<string, Row>();
  for (const r of rows) {
    if (byName.has(r.seam)) errors.push(`${TABLE}:${r.line}: ${r.seam} has a second row (first at line ${byName.get(r.seam)!.line})`);
    else byName.set(r.seam, r);
    if (!contract.has(r.seam)) errors.push(`${TABLE}:${r.line}: ${r.seam} is not in the SW-02 platform contract (np_platform_stub.c does not define it): a seam was removed, or the row is misspelt`);
  }
  for (const name of contract.keys()) {
    if (!byName.has(name)) errors.push(`${TABLE}: no row for ${name}, which the SW-02 platform contract defines: a seam was added, so say what the twin does with it`);
  }

  // The sim layer's own definitions of contract seams.
  const simDefs = new Map<string, Def[]>();
  for (const [file, text] of Object.entries(t.simSources)) {
    for (const d of parseDefs(`src/${file}`, text)) {
      if (d.isStatic || !contract.has(d.name)) continue;
      simDefs.set(d.name, [...(simDefs.get(d.name) ?? []), d]);
    }
  }
  const linked = linkedFirmwareSources(t.cmake);
  const linkedText = new Map<string, string>();
  for (const rel of linked) {
    const text = t.readFirmware(rel);
    if (text === null) errors.push(`${SIM_CMAKE}: names ${rel}, which does not exist`);
    else linkedText.set(rel, decomment(text));
  }

  for (const r of byName.values()) {
    const c = contract.get(r.seam);
    if (!c) continue;
    const at = `${TABLE}:${r.line}: ${r.seam}`;
    if (!(DISPOSITIONS as readonly string[]).includes(r.disposition)) { errors.push(`${at}: disposition '${r.disposition}' is not one of ${DISPOSITIONS.join(", ")}`); continue; }
    const disp = r.disposition as Disposition;
    counts[disp] = (counts[disp] ?? 0) + 1;
    const defs = simDefs.get(r.seam) ?? [];

    if (disp === "model" || disp === "infra" || disp === "unrouted") {
      if (defs.length === 0) errors.push(`${at}: row says '${disp}' but nothing under ${SIM_SRC} defines it`);
      else if (defs.length > 1) errors.push(`${at}: defined ${defs.length} times in the sim layer (${defs.map((d) => d.file).join(", ")}); REQ-SIMFW-02 wants exactly one`);
      else {
        const d = defs[0]!;
        if (d.file !== r.where) errors.push(`${at}: row says it is defined in ${r.where}, the tree defines it in ${d.file}`);
        const reports = /\bunrouted\s*\(/.test(d.body);
        if (disp === "unrouted" && !reports) errors.push(`${at}: row says 'unrouted' but its definition does not report itself unrouted`);
        if (disp !== "unrouted" && reports) errors.push(`${at}: its definition reports itself unrouted but the row says '${disp}'`);
      }
    } else {
      if (defs.length > 0) errors.push(`${at}: row says '${disp}' but the sim layer defines it (${defs.map((d) => d.file).join(", ")})`);
      if (disp === "firmware-host") {
        const text = linkedText.get(r.where);
        if (text === undefined) errors.push(`${at}: row says the device source ${r.where} defines it, and that source is not on the twin's link line (${SIM_CMAKE})`);
        else if (!parseDefs(r.where, text).some((d) => d.name === r.seam && !d.isStatic)) errors.push(`${at}: ${r.where} does not define it`);
      } else {
        if (r.where !== "-") errors.push(`${at}: an absent seam is defined nowhere, so 'where' is '-'`);
        const callers = [...linkedText.entries()].filter(([, text]) => new RegExp(`\\b${r.seam}\\b`).test(text)).map(([rel]) => rel);
        if (callers.length > 0) errors.push(`${at}: row says 'absent' but a source the twin links names it (${callers.join(", ")}): define it once in the sim layer and give it a route`);
      }
    }

    if (!SPACES.includes(r.space)) errors.push(`${at}: space '${r.space}' is not one of ${SPACES.join(", ")}`);
    else {
      const want = PARAM_SPACE[c.firstParam] ?? "none";
      if (r.space !== want) errors.push(`${at}: the contract's first parameter is '${c.firstParam || "void"}', which is space '${want}'; the row says '${r.space}' (OI-SIMFW-06: index space and id space are not to be confused)`);
    }

    const needsItem = disp === "absent" || disp === "unrouted";
    if (needsItem && !/^(OI-[A-Z0-9]+-\d+|none)$/.test(r.item)) errors.push(`${at}: an ${disp} seam names the open item that would route it, or 'none'`);
    if (!needsItem && r.item !== "-") errors.push(`${at}: only unrouted and absent rows carry an item; write '-'`);
  }
  return { errors, counts, contract: contract.size };
}

/** `nm -A` text over the built archive, against the table. Pure, so the self-test can feed it text. */
export function checkNm(rows: Row[], nmText: string): string[] {
  const errors: string[] = [];
  const defined = new Map<string, string[]>();
  const referenced = new Map<string, string[]>();
  for (const line of nmText.split("\n")) {
    const m = /^[^:]*:([^:]+):\s*(?:[0-9a-fA-F]+\s+)?([A-Za-z])\s+(\S+)$/.exec(line.trim());
    if (!m) continue;
    const [, member, kind, name] = m as unknown as [string, string, string, string];
    if (kind === "T") defined.set(name, [...(defined.get(name) ?? []), member]);
    else if (kind === "U") referenced.set(name, [...(referenced.get(name) ?? []), member]);
  }
  if (defined.size === 0) return ["nm reported no defined text symbol: the archive is empty or was not read, and a check that looks at nothing must not pass"];
  for (const r of rows) {
    const members = defined.get(r.seam) ?? [];
    if (r.disposition === "absent") {
      if (members.length > 0) errors.push(`${r.seam}: row says 'absent' but the built library defines it (${members.join(", ")})`);
      const refs = referenced.get(r.seam) ?? [];
      if (refs.length > 0) errors.push(`${r.seam}: row says 'absent' but a member of the built library references it (${refs.join(", ")})`);
    } else {
      const want = `${basename(r.where)}.o`;
      if (members.length !== 1) errors.push(`${r.seam}: the built library defines it ${members.length} times (${members.join(", ") || "nowhere"}); the row says once, in ${want}`);
      else if (members[0] !== want) errors.push(`${r.seam}: the built library defines it in ${members[0]}, the row says ${want}`);
    }
  }
  return errors;
}

function realTree(root: string): Tree {
  const read = (rel: string) => readFileSync(join(root, rel), "utf8");
  const simSources: Record<string, string> = {};
  for (const f of readdirSync(join(root, SIM_SRC)).sort()) if (f.endsWith(".c")) simSources[f] = read(`${SIM_SRC}/${f}`);
  return {
    stub: read(STUB), header: read(HEADER), table: read(TABLE), cmake: read(SIM_CMAKE), simSources,
    readFirmware: (rel) => {
      const p = join(root, normalize(join(SIM_DIR, rel)));
      return existsSync(p) ? readFileSync(p, "utf8") : null;
    },
  };
}

// ── self-test ────────────────────────────────────────────────────────────────────────────────────

function selfTest(): number {
  let failures = 0;
  const expect = (name: string, errors: string[], needle: string | null) => {
    const ok = needle === null ? errors.length === 0 : errors.some((e) => e.includes(needle));
    console.log(`${ok ? "ok  " : "FAIL"} ${name}`);
    if (!ok) { failures++; console.log(`     wanted ${needle === null ? "no error" : `an error containing '${needle}'`}, got:\n     ${errors.join("\n     ") || "(none)"}`); }
  };

  // A small contract in a real directory, read back through realTree(), so the file handling is exercised too.
  const root = mkdtempSync(join(tmpdir(), "np-seam-routes-"));
  try {
    const put = (rel: string, text: string) => { mkdirSync(dirname(join(root, rel)), { recursive: true }); writeFileSync(join(root, rel), text); };
    const stub = [
      "/* a comment naming np_fake_not_a_seam(void) { } */",
      "np_hub_status_t np_a_socket(uint16_t  socket_id,\n                            uint8_t x)\n{ (void)socket_id; (void)x; np_platform_unimplemented(); }",
      "float np_b_pair(uint8_t pair) { (void)pair; np_platform_unimplemented(); }",
      "void np_c_one(void)\n{\n    np_platform_unimplemented();\n}",
      "bool np_d_slot(uint8_t slot) { (void)slot; np_platform_unimplemented(); }",
      "void np_e_part(np_log_part_t part) { (void)part; np_platform_unimplemented(); }",
      "void np_f_gone(void) { np_platform_unimplemented(); }",
      "static void helper(void) { }",
    ].join("\n\n");
    put(STUB, stub);
    put(HEADER, "#define NP_SW02_PLATFORM_SYMBOL_COUNT   6\n");
    put(`${SIM_SRC}/np_sim_x.c`, [
      "static void unrouted(const char *s) { (void)s; }",
      "np_hub_status_t np_a_socket(uint16_t socket_id, uint8_t x) { (void)x; return route(socket_id); }",
      "float np_b_pair(uint8_t pair)\n{ return ask(pair); }",
      "void np_c_one(void) { tick(); }",
    ].join("\n"));
    put(`${SIM_SRC}/np_sim_unrouted.c`, "bool np_d_slot(uint8_t slot) { (void)slot; unrouted(__func__); return false; }\n");
    put("firmware/hub/src/np_log.c", "void use(void) { np_a_socket(0, 0); }\n#ifdef NPTEST_HOST\nvoid np_e_part(np_log_part_t part) { (void)part; }\n#endif\n");
    put("firmware/hub/src/np_other.c", "void other(void) { /* np_f_gone() in a comment is not a call */ }\n");
    put(SIM_CMAKE, "add_library(np_sim_pbm STATIC\n    src/np_sim_x.c\n    ../hub/src/np_log.c   # ../hub/src/np_commented_out.c\n    ../hub/src/np_other.c\n)\n");
    const good = [
      "# comment",
      "np_a_socket  model          src/np_sim_x.c         socket-index  -",
      "np_b_pair    model          src/np_sim_x.c         pair          -",
      "np_c_one     infra          src/np_sim_x.c         none          -",
      "np_d_slot    unrouted       src/np_sim_unrouted.c  slot          OI-SIMFW-53",
      "np_e_part    firmware-host  ../hub/src/np_log.c    partition     -",
      "np_f_gone    absent         -                      none          OI-SIMFW-50",
    ].join("\n") + "\n";
    put(TABLE, good);

    const run = (edit?: (t: Tree) => void) => { const t = realTree(root); edit?.(t); return check(t); };
    const base = run();
    expect("the good tree passes", base.errors, null);
    expect("the good tree counts 6 seams", base.contract === 6 && base.counts.model === 2 && base.counts.absent === 1 ? [] : [`contract=${base.contract} ${JSON.stringify(base.counts)}`], null);

    // A seam ADDED to the contract (stub and constant move together, as the census forces) with no row.
    expect("a seam added to the contract with no row fails", run((t) => {
      t.stub += "\nvoid np_g_new(void) { np_platform_unimplemented(); }\n"; t.header = t.header.replace("6", "7");
    }).errors, "no row for np_g_new");
    // A seam REMOVED from the contract with its row left behind.
    expect("a seam removed from the contract with its row left fails", run((t) => {
      t.stub = t.stub.replace(/void np_f_gone[^\n]*\n/, "\n"); t.header = t.header.replace("6", "5");
    }).errors, "np_f_gone is not in the SW-02 platform contract");
    expect("a stub the parser cannot count against the constant fails", run((t) => { t.header = t.header.replace("6", "7"); }).errors, "NP_SW02_PLATFORM_SYMBOL_COUNT says 7");
    expect("a sim definition removed (a route dropped) fails", run((t) => {
      t.simSources["np_sim_x.c"] = t.simSources["np_sim_x.c"]!.replace(/float np_b_pair[\s\S]*?\}\n/, "");
    }).errors, "row says 'model' but nothing under");
    expect("a sim definition added for an absent seam fails", run((t) => {
      t.simSources["np_sim_x.c"] += "\nvoid np_f_gone(void) { }\n";
    }).errors, "row says 'absent' but the sim layer defines it");
    expect("a seam defined twice in the sim layer fails", run((t) => {
      t.simSources["np_sim_y.c"] = "void np_c_one(void) { }\n";
    }).errors, "defined 2 times in the sim layer");
    expect("a definition in another file than the row names fails", run((t) => { t.table = t.table.replace("np_c_one     infra          src/np_sim_x.c ", "np_c_one     infra          src/np_sim_q.c "); }).errors, "the tree defines it in src/np_sim_x.c");
    expect("an unrouted definition recorded as a model fails", run((t) => { t.table = t.table.replace("np_d_slot    unrouted", "np_d_slot    model   "); }).errors, "reports itself unrouted but the row says 'model'");
    expect("a routed definition recorded as unrouted fails", run((t) => { t.table = t.table.replace("np_c_one     infra          src/np_sim_x.c         none          -", "np_c_one     unrouted       src/np_sim_x.c         none          none"); }).errors, "does not report itself unrouted");
    expect("an absent seam a linked source calls fails", run((t) => {
      const orig = t.readFirmware; t.readFirmware = (rel) => rel === "../hub/src/np_other.c" ? "void other(void) { np_f_gone(); }\n" : orig(rel);
    }).errors, "a source the twin links names it (../hub/src/np_other.c)");
    expect("a firmware-host seam whose source is not linked fails", run((t) => { t.cmake = t.cmake.replace("    ../hub/src/np_log.c   #", "    #"); }).errors, "is not on the twin's link line");
    expect("a firmware-host seam its source does not define fails", run((t) => { t.table = t.table.replace("../hub/src/np_log.c  ", "../hub/src/np_other.c"); }).errors, "../hub/src/np_other.c does not define it");
    expect("a socket-index seam routed as a pair fails", run((t) => { t.table = t.table.replace("socket-index", "pair        "); }).errors, "which is space 'socket-index'; the row says 'pair'");
    expect("a space outside the list fails", run((t) => { t.table = t.table.replace("partition", "lattice-id"); }).errors, "space 'lattice-id' is not one of");
    expect("a second row for one seam fails", run((t) => { t.table += "np_c_one     infra          src/np_sim_x.c         none          -\n"; }).errors, "has a second row");
    expect("an absent row with no item fails", run((t) => { t.table = t.table.replace("OI-SIMFW-50", "-"); }).errors, "names the open item that would route it");
    expect("a malformed row fails", run((t) => { t.table += "np_c_one infra\n"; }).errors, "a row has 5 columns");
    expect("a CMake file naming a missing source fails", run((t) => { t.cmake += "\n../hub/src/np_missing.c\n"; }).errors, "which does not exist");

    // --lib: nm text, fed directly.
    const rows = parseTable(good).rows;
    const nmGood = [
      "lib.a:np_sim_x.c.o:0000000000000000 T np_a_socket", "lib.a:np_sim_x.c.o:0000000000000010 T np_b_pair",
      "lib.a:np_sim_x.c.o:0000000000000020 T np_c_one", "lib.a:np_sim_unrouted.c.o:0000000000000000 T np_d_slot",
      "lib.a:np_log.c.o:0000000000000000 T np_e_part", "lib.a:np_log.c.o:                 U np_a_socket",
    ].join("\n");
    expect("nm: the good library passes", checkNm(rows, nmGood), null);
    expect("nm: an empty listing fails", checkNm(rows, ""), "must not pass");
    expect("nm: a seam missing from the library fails", checkNm(rows, nmGood.replace(/^.*T np_b_pair$/m, "")), "np_b_pair: the built library defines it 0 times");
    expect("nm: a seam in two members fails", checkNm(rows, nmGood + "\nlib.a:np_sim_y.c.o:0000000000000000 T np_c_one"), "np_c_one: the built library defines it 2 times");
    expect("nm: a seam in another member fails", checkNm(rows, nmGood.replace("np_sim_unrouted.c.o:0000000000000000 T np_d_slot", "np_sim_x.c.o:0000000000000030 T np_d_slot")), "the row says np_sim_unrouted.c.o");
    expect("nm: an absent seam the library defines fails", checkNm(rows, nmGood + "\nlib.a:np_sim_x.c.o:0000000000000040 T np_f_gone"), "row says 'absent' but the built library defines it");
    expect("nm: an absent seam a member references fails", checkNm(rows, nmGood + "\nlib.a:np_other.c.o:                 U np_f_gone"), "a member of the built library references it");
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
  console.log(failures === 0 ? "check-sim-seam-routes self-test: ok" : `check-sim-seam-routes self-test: ${failures} FAILED`);
  return failures === 0 ? 0 : 1;
}

// ── main ─────────────────────────────────────────────────────────────────────────────────────────

if (import.meta.main) {
  const args = process.argv.slice(2);
  if (args.includes("--self-test")) process.exit(selfTest());

  const tree = realTree(".");
  const { errors, counts, contract } = check(tree);
  if (contract === 0) errors.push("no contract seam was read: the gate would pass on nothing");
  const libAt = args.indexOf("--lib");
  if (libAt >= 0) {
    const lib = args[libAt + 1];
    if (!lib || !existsSync(lib)) {
      errors.push(`--lib: ${lib ?? "(no path)"} does not exist; build np_sim_pbm first (a check that cannot find its input does not pass)`);
    } else {
      const nm = spawnSync("nm", ["-A", lib], { encoding: "utf8", maxBuffer: 64 * 1024 * 1024 });
      if (nm.status !== 0) errors.push(`nm -A ${lib} failed (${nm.status}): ${nm.stderr || nm.error}`);
      else errors.push(...checkNm(parseTable(tree.table).rows, nm.stdout));
    }
  }
  if (errors.length > 0) {
    for (const e of errors) console.error(`error: ${e}`);
    console.error(`check-sim-seam-routes: ${errors.length} problem(s). NP-SIM-FW-001 §30, REQ-SIMFW-02.`);
    process.exit(1);
  }
  console.log(`scanned: ${contract} contract seam(s) against ${Object.keys(tree.simSources).length} sim source(s) and ${linkedFirmwareSources(tree.cmake).length} linked device source(s)`);
  const split = DISPOSITIONS.map((d) => `${counts[d] ?? 0} ${d}`).join(", ");
  console.log(`check-sim-seam-routes: ok — ${contract} contract seams, one row each: ${split}${libAt >= 0 ? " (built library agrees)" : ""}`);
}
