#!/usr/bin/env bun
/**
 * check-bench-boundary.ts — bench mode never reaches the Class C tier.
 *
 * NP-FW-BENCH-001 §5 classifies the head-presence gate and its bench/service
 * bypass as SW-02, IEC 62304 Class B, and leg (iii) of that argument is a
 * prohibition: the Class C tier must not learn that a bypass exists. NP-SW-001
 * §3.2's Class B basis for SW-02 rests on SW-01 being an INDEPENDENT backstop,
 * and a bypass predicate crossing the SPI link would make SW-01's behaviour a
 * function of an SW-02 policy decision. §7.2 states that this is enforced by the
 * absence of a wire, and that the absence "is checkable mechanically, and per
 * NP-CONV-001 §8 it should be". This is that check (OI-BENCH-04, RISK-BENCH-02).
 *
 *   bun scripts/check-bench-boundary.ts
 *   bun scripts/check-bench-boundary.ts --self-test
 *
 * ── What is checked ──────────────────────────────────────────────────────────
 *
 *  A. NO BENCH IDENTIFIER IN THE CLASS C BUILD. Every file under
 *     firmware/safety_mcu/ and firmware/common/, plus the SW-01 toolchain file
 *     firmware/cmake/stm32g071.cmake, with comments and string literals
 *     stripped (C, linker script, assembly) or comments stripped (CMake, where
 *     a quoted `-DNP_BENCH_MODE` is exactly the thing to catch). No identifier
 *     may match BENCH_PATTERNS. firmware/common/ is in scope because it is
 *     compiled INTO SW-01 — np_spi_wire_types.h defines np_safety_rx_ext_frame_t,
 *     the frame §7.2 says bench mode appears in no field of.
 *
 *  B. SESSION_STATUS GAINS NO BIT. Every `#define NP_SESSION_STATUS_*` in the
 *     firmware tree is compared, by value, against ALLOCATION — the byte as
 *     NP-FW-BENCH-001 Rev 2 §7.2 records it: bits 0–4 are five predicates and
 *     bits 5–7 are the heartbeat sequence counter, so the byte is FULL. A new
 *     name, a changed value, a name that disappeared, or two definitions that
 *     disagree all fail. Every `session_status` struct field must stay
 *     `uint8_t`: widening the field gains bits without a new define, which the
 *     define comparison alone could not see.
 *
 * ── The reach, stated narrowly ───────────────────────────────────────────────
 *
 * Clause A is a NAME check. A bypass carried under an innocuous name
 * (`flags2`, `NP_SAFETY_EN_ALT`) passes it. That is why clause B exists: with
 * session_status full, a new predicate on the link needs a new wire field, and
 * a new field in np_safety_rx_ext_frame_t changes the frame that
 * check-hub-wire-format.ts and the safety MCU's own frame-size assertions pin.
 * Neither gate alone closes the boundary; together they make crossing it an
 * edit a reviewer sees flagged, which is the property §7.2 asks for. It does
 * not — cannot — prove that no SW-01 behaviour depends on SW-02 state in some
 * other way; that is §5's argument, not a grep.
 *
 * Hub-side bench identifiers are REQUIRED to be possible: bench mode is an
 * SW-02 predicate and will be written under firmware/hub_control/. The
 * self-test proves a hub-side identifier passes.
 *
 * Changing ALLOCATION or BENCH_PATTERNS is the decision this gate exists to
 * force. Revise NP-FW-BENCH-001 §7.2 in the same change.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-bench-boundary.ts --self-test
 * CI-Scans: the SW-01 build (firmware/safety_mcu, firmware/common, the G071 toolchain file) for bench identifiers, and every NP_SESSION_STATUS_* define and session_status field in firmware/
 * CI-Scan-Paths: firmware/** scripts/check-bench-boundary.ts
 */
import { readFileSync, readdirSync, statSync, mkdtempSync, mkdirSync, writeFileSync, rmSync } from "fs";
import { join, resolve, relative } from "path";
import { tmpdir } from "os";

/** The Class C build's source population, relative to the repo root. */
const CLASS_C_DIRS = ["firmware/safety_mcu", "firmware/common"];
const CLASS_C_FILES = ["firmware/cmake/stm32g071.cmake"];

/**
 * Identifier fragments that name the bypass or the gate it bypasses. Matched
 * case-insensitively against whole identifiers, so `NP_BENCH_MODE`,
 * `s_bench_active` and `benchMode` all hit.
 *
 * `head_present` is here as well as `bench` because §5 (i) and (iii) put the
 * GATE on the Class B side too: routing head presence into SW-01 is the class
 * escalation §5 argues against, and must arrive as a revision of that section,
 * not as a new input nobody flagged.
 */
const BENCH_PATTERNS: { re: RegExp; why: string }[] = [
  { re: /bench/i, why: "the bypass itself (NP-FW-BENCH-001 §6)" },
  { re: /service_?(mode|auth|cred|key|window)/i, why: "the D-5 service credential and its window" },
  { re: /svc_?(mode|auth|cred|key|window)/i, why: "the D-5 service credential, abbreviated" },
  { re: /head_?presen/i, why: "the head-presence gate, which §5 places in SW-02" },
  { re: /(^|_)[zt]_hp($|_)/i, why: "the gate's thresholds Z_HP / T_HP (§4.3)" },
];

/**
 * session_status as of NP-FW-BENCH-001 Rev 2 §7.2. Bits 0–4 are predicates,
 * bits 5–7 the sequence counter (NP-FMEA-001 OI-FMEA-12 (a)). SHIFT is a
 * position, not a mask, and is excluded from the coverage sum.
 */
const ALLOCATION: Record<string, { value: number; mask: boolean }> = {
  NP_SESSION_STATUS_ACTIVE: { value: 1 << 0, mask: true },
  NP_SESSION_STATUS_CVNS_REENABLE: { value: 1 << 1, mask: true },
  NP_SESSION_STATUS_GEOM_REQUIRED: { value: 1 << 2, mask: true },
  NP_SESSION_STATUS_GEOM_REQ_TDCS: { value: 1 << 3, mask: true },
  NP_SESSION_STATUS_GEOM_REQ_BES: { value: 1 << 4, mask: true },
  NP_SESSION_STATUS_SEQ_SHIFT: { value: 5, mask: false },
  NP_SESSION_STATUS_SEQ_MASK: { value: 7 << 5, mask: true },
};

const SOURCE_EXT = /\.(c|h|s|S|ld)$/;
const CMAKE_FILE = /(^|\/)(CMakeLists\.txt|[^/]+\.cmake)$/;

// ── Source handling ──────────────────────────────────────────────────────────

/** Strip C comments, string and char literals, keeping line structure. */
function stripC(src: string, asm: boolean): string {
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
    } else if ((c === "/" && d === "/") || (asm && c === "@")) {
      while (i < n && src[i] !== "\n") i++;
    } else if (c === '"' || c === "'") {
      i++;
      while (i < n && src[i] !== c && src[i] !== "\n") i += src[i] === "\\" ? 2 : 1;
      i++;
      out += " ";
    } else {
      out += c;
      i++;
    }
  }
  return out;
}

/** CMake: strip `#` comments only. A quoted -D flag is live and must be seen. */
function stripCMake(src: string): string {
  return src
    .split("\n")
    .map((l) => l.replace(/(^|\s)#(?!\[\[).*$/, "$1"))
    .join("\n");
}

function walk(root: string, rel: string, out: string[]): void {
  let entries: string[];
  try {
    entries = readdirSync(join(root, rel));
  } catch {
    return;
  }
  for (const e of entries.sort()) {
    const r = `${rel}/${e}`;
    if (statSync(join(root, r)).isDirectory()) walk(root, r, out);
    else out.push(r);
  }
}

/** Evaluate a #define body made of integer literals, <<, |, parens and known names. */
function evalDefine(body: string, known: Map<string, number>): number | null {
  const expr = body
    .replace(/\b(0[xX][0-9a-fA-F]+|\d+)[uUlL]*\b/g, "$1")
    .replace(/\b[A-Za-z_]\w*\b/g, (id) => (known.has(id) ? String(known.get(id)) : `§${id}`));
  if (!/^[\s\d xXa-fA-F()<|]+$/.test(expr) || /§/.test(expr)) return null;
  try {
    const v = Function(`"use strict"; return (${expr});`)() as unknown;
    return typeof v === "number" && Number.isInteger(v) ? v : null;
  } catch {
    return null;
  }
}

// ── The audit ────────────────────────────────────────────────────────────────

type Report = { violations: string[]; classCFiles: number; defines: number; fields: number };

function audit(root: string): Report {
  const violations: string[] = [];

  // A. Bench identifiers in the Class C build.
  const classC: string[] = [];
  for (const d of CLASS_C_DIRS) walk(root, d, classC);
  for (const f of CLASS_C_FILES) {
    try {
      statSync(join(root, f));
      classC.push(f);
    } catch {
      violations.push(`A: ${f} is listed as part of the SW-01 build but does not exist — update CLASS_C_FILES`);
    }
  }
  let classCFiles = 0;
  for (const f of classC) {
    const isCMake = CMAKE_FILE.test(f);
    if (!isCMake && !SOURCE_EXT.test(f)) continue;
    classCFiles++;
    const raw = readFileSync(join(root, f), "utf8");
    const code = isCMake ? stripCMake(raw) : stripC(raw, /\.(s|S)$/.test(f));
    code.split("\n").forEach((line, idx) => {
      for (const id of line.match(/[A-Za-z_]\w*/g) ?? []) {
        const hit = BENCH_PATTERNS.find((p) => p.re.test(id));
        if (hit) {
          violations.push(
            `A: ${f}:${idx + 1}: identifier \`${id}\` names ${hit.why} — bench mode and the ` +
              `head-presence gate are SW-02 only (NP-FW-BENCH-001 §5 (iii), §7.2)`,
          );
        }
      }
    });
  }
  if (CLASS_C_DIRS.some((d) => !classC.some((f) => f.startsWith(d + "/")))) {
    violations.push(`A: a Class C directory (${CLASS_C_DIRS.join(", ")}) contributed no files — refusing to pass vacuously`);
  }

  // B. session_status allocation and width, across the whole firmware tree
  //    except vendored code.
  const all: string[] = [];
  walk(root, "firmware", all);
  const sources = all.filter((f) => /\.(c|h)$/.test(f) && !f.startsWith("firmware/vendor/"));

  type Def = { file: string; line: number; body: string };
  const defs = new Map<string, Def[]>();
  let fields = 0;
  for (const f of sources) {
    const code = stripC(readFileSync(join(root, f), "utf8"), false);
    code.split("\n").forEach((line, idx) => {
      const m = /^\s*#\s*define\s+(NP_SESSION_STATUS_\w+)\s+(.+?)\s*$/.exec(line);
      if (m) {
        const list = defs.get(m[1]!) ?? [];
        list.push({ file: f, line: idx + 1, body: m[2]! });
        defs.set(m[1]!, list);
      }
    });
    // A declaration, not a use: preceded by a line start, `{` or `;`, and
    // with a type in front. `return session_status;` is the one use shaped
    // like a declaration, and is skipped.
    for (const m of code.matchAll(/(?:^|[{;])[ \t]*([A-Za-z_][\w \t]*?)[ \t]+session_status\s*(\[[^\]]*\])?\s*(:\s*\d+)?\s*;/gm)) {
      const type = m[1]!.replace(/\s+/g, " ").trim();
      if (type === "return") continue;
      fields++;
      const line = code.slice(0, m.index).split("\n").length;
      if (type !== "uint8_t" || m[2] || m[3]) {
        violations.push(
          `B: ${f}:${line}: session_status is \`${type}${m[2] ?? ""}${m[3] ?? ""}\`, not a plain ` +
            `uint8_t — widening the field gains bits no NP_SESSION_STATUS_* define would show`,
        );
      }
    }
  }
  if (fields === 0) violations.push("B: no session_status field found anywhere — refusing to pass vacuously");

  // Resolve in dependency order: SEQ_MASK refers to SEQ_SHIFT.
  const known = new Map<string, number>();
  for (let pass = 0; pass < 4; pass++) {
    for (const [name, list] of defs) {
      if (known.has(name)) continue;
      const v = evalDefine(list[0]!.body, known);
      if (v !== null) known.set(name, v);
    }
  }
  let defines = 0;
  for (const [name, list] of defs) {
    const want = ALLOCATION[name];
    for (const d of list) {
      defines++;
      const got = evalDefine(d.body, known);
      if (!want) {
        violations.push(
          `B: ${d.file}:${d.line}: ${name} is not in the Rev 2 allocation — session_status is full ` +
            `(NP-FW-BENCH-001 §7.2); a new bit needs that section revised, not a define`,
        );
      } else if (got === null) {
        violations.push(`B: ${d.file}:${d.line}: ${name} = \`${d.body}\` could not be evaluated`);
      } else if (got !== want.value) {
        violations.push(
          `B: ${d.file}:${d.line}: ${name} = 0x${got.toString(16)}, allocation says ` +
            `0x${want.value.toString(16)}`,
        );
      }
    }
  }
  for (const name of Object.keys(ALLOCATION)) {
    if (!defs.has(name)) {
      violations.push(`B: ${name} is allocated but defined nowhere — renamed or removed without revising ALLOCATION`);
    }
  }
  // The table itself: masks disjoint and covering the byte exactly. A fault
  // here is in this script, and it would make every other B verdict meaningless.
  let covered = 0;
  for (const [name, a] of Object.entries(ALLOCATION)) {
    if (!a.mask) continue;
    if (covered & a.value) violations.push(`B: ALLOCATION overlaps at ${name}`);
    covered |= a.value;
  }
  if (covered !== 0xff) violations.push(`B: ALLOCATION covers 0x${covered.toString(16)}, not 0xff`);

  return { violations, classCFiles, defines, fields };
}

// ── Self-test ────────────────────────────────────────────────────────────────

if (process.argv.includes("--self-test")) {
  const failures: string[] = [];

  // Stripper, in isolation.
  const stripped = stripC(
    '/* bench */ int a; // bench\nconst char *s = "bench \\" mode"; char c = \'b\';\nint live_bench;\n',
    false,
  );
  if (/bench/.test(stripped.split("\n").slice(0, 2).join("\n"))) {
    failures.push("stripper — a comment or string literal survived");
  }
  if (!/live_bench/.test(stripped)) failures.push("stripper — live code was stripped");
  if (stripped.split("\n").length !== 4) failures.push("stripper — line structure not preserved");
  if (/bench/.test(stripC("  ldr r0, =x  @ bench vector\n", true))) {
    failures.push("stripper — an assembly `@` comment survived");
  }
  if (!/NP_BENCH/.test(stripCMake('add_compile_definitions("-DNP_BENCH") # bench\n'))) {
    failures.push("cmake stripper — removed a quoted -D flag, which is live");
  }

  // Fixture trees, built from nothing: a minimal SW-01 tree, the shared wire
  // header, and a hub header, each shaped like the real one.
  const box = mkdtempSync(join(tmpdir(), "np-bench-boundary-"));
  const WIRE = [
    "#define NP_SESSION_STATUS_GEOM_REQUIRED  (1U << 2)",
    "#define NP_SESSION_STATUS_GEOM_REQ_TDCS  (1U << 3)",
    "#define NP_SESSION_STATUS_GEOM_REQ_BES   (1U << 4)",
    "#define NP_SESSION_STATUS_SEQ_SHIFT      5U",
    "#define NP_SESSION_STATUS_SEQ_MASK       (7U << NP_SESSION_STATUS_SEQ_SHIFT)",
    "typedef struct {",
    "    uint8_t  session_status;    /* NP_SESSION_STATUS_* bits */",
    "} np_safety_rx_ext_frame_t;",
    "",
  ].join("\n");
  const PAIR = [
    "#define NP_SESSION_STATUS_ACTIVE        (1U << 0)",
    "#define NP_SESSION_STATUS_CVNS_REENABLE (1U << 1)",
    "typedef struct { uint8_t session_status; } np_frame_t;",
    "",
  ].join("\n");
  const BASE: Record<string, string> = {
    "firmware/common/include/np_spi_wire_types.h": WIRE,
    "firmware/safety_mcu/include/np_safety_protocol.h": PAIR,
    "firmware/safety_mcu/src/np_spi_watchdog.c":
      "/* Hardware bench test required; service the watchdog. */\n" +
      'static const char *k = "bench";\nvoid np_spi_watchdog_tick(void) { granted = requested & mask; }\n',
    "firmware/safety_mcu/startup/startup.s": "  b Reset_Handler  @ bench-verified vector\n",
    "firmware/safety_mcu/CMakeLists.txt": "# no bench flags here\nadd_executable(np_safety_mcu main.c)\n",
    "firmware/cmake/stm32g071.cmake": "set(CMAKE_C_FLAGS_INIT \"-mcpu=cortex-m0plus\")\n",
    "firmware/hub_control/include/np_hub_config.h": PAIR,
  };
  const build = (patch: Record<string, string>): string => {
    const root = mkdtempSync(join(box, "t-"));
    for (const [rel, body] of Object.entries({ ...BASE, ...patch })) {
      mkdirSync(join(root, rel, ".."), { recursive: true });
      writeFileSync(join(root, rel), body);
    }
    return root;
  };
  const expect = (label: string, patch: Record<string, string>, needle: string | null) => {
    const { violations } = audit(build(patch));
    if (needle === null) {
      if (violations.length) failures.push(`${label} — expected clean, got: ${violations[0]}`);
    } else if (!violations.some((v) => v.includes(needle))) {
      failures.push(`${label} — no violation matching ${JSON.stringify(needle)}; got ${JSON.stringify(violations)}`);
    }
  };
  const add = (rel: string, extra: string) => ({ [rel]: (BASE[rel] ?? "") + extra });

  // Must pass — the direction that keeps the gate from being switched off.
  expect("the baseline passes, with `bench` in comments, strings and asm comments", {}, null);
  expect(
    "a bench identifier on the HUB side passes — bench mode is an SW-02 predicate",
    { "firmware/hub_control/src/np_bench_mode.c": "static bool s_bench_mode;\nbool np_head_present(void);\n" },
    null,
  );

  // Must fail — clause A.
  expect(
    "a bench identifier in a safety-MCU translation unit is caught (§7.2's own falsification)",
    add("firmware/safety_mcu/src/np_spi_watchdog.c", "static bool s_bench_mode;\n"),
    "s_bench_mode",
  );
  expect(
    "a bench field in the shared SPI frame is caught",
    { "firmware/common/include/np_spi_wire_types.h": WIRE.replace("session_status;", "session_status;\n    uint8_t bench_flags;") },
    "bench_flags",
  );
  expect(
    "a head-presence input in SW-01 is caught",
    add("firmware/safety_mcu/include/np_safety_protocol.h", "#define NP_HEAD_PRESENT_PIN 3\n"),
    "NP_HEAD_PRESENT_PIN",
  );
  expect(
    "a service-credential identifier in SW-01 is caught",
    add("firmware/safety_mcu/src/np_spi_watchdog.c", "extern int np_svc_auth_ok(void);\n"),
    "np_svc_auth_ok",
  );
  expect(
    "a quoted -D bench flag in the SW-01 CMake is caught",
    add("firmware/safety_mcu/CMakeLists.txt", 'target_compile_definitions(np_safety_mcu PRIVATE "NP_BENCH_BUILD=1")\n'),
    "NP_BENCH_BUILD",
  );
  expect(
    "a bench flag in the G071 toolchain file is caught",
    add("firmware/cmake/stm32g071.cmake", "add_compile_definitions(NP_ALLOW_BENCH)\n"),
    "NP_ALLOW_BENCH",
  );

  // Must fail — clause B.
  expect(
    "a new session_status define, under an innocuous name, is caught",
    add("firmware/hub_control/include/np_hub_config.h", "#define NP_SESSION_STATUS_OVERRIDE (1U << 5)\n"),
    "NP_SESSION_STATUS_OVERRIDE is not in the Rev 2 allocation",
  );
  expect(
    "shrinking the sequence counter to free a bit is caught",
    { "firmware/common/include/np_spi_wire_types.h": WIRE.replace("(7U <<", "(3U <<") },
    "NP_SESSION_STATUS_SEQ_MASK = 0x60",
  );
  expect(
    "two sides disagreeing about a bit is caught",
    { "firmware/safety_mcu/include/np_safety_protocol.h": PAIR.replace("(1U << 1)", "(1U << 5)") },
    "NP_SESSION_STATUS_CVNS_REENABLE = 0x20",
  );
  expect(
    "widening session_status is caught",
    { "firmware/common/include/np_spi_wire_types.h": WIRE.replace("uint8_t  session_status", "uint16_t session_status") },
    "session_status is `uint16_t`",
  );
  expect(
    "a removed allocation is caught rather than silently unguarded",
    { "firmware/common/include/np_spi_wire_types.h": WIRE.replace(/^#define NP_SESSION_STATUS_GEOM_REQ_BES.*$/m, "") },
    "NP_SESSION_STATUS_GEOM_REQ_BES is allocated but defined nowhere",
  );
  expect(
    "a one-line struct's session_status is seen, and `return session_status;` is not a field",
    add(
      "firmware/safety_mcu/src/np_spi_watchdog.c",
      "typedef struct { uint32_t session_status; } np_x_t;\nuint8_t f(void) { return session_status; }\n",
    ),
    "session_status is `uint32_t`",
  );
  {
    const root = mkdtempSync(join(box, "empty-"));
    mkdirSync(join(root, "firmware/cmake"), { recursive: true });
    writeFileSync(join(root, "firmware/cmake/stm32g071.cmake"), "\n");
    const { violations } = audit(root);
    if (!violations.some((v) => v.includes("refusing to pass vacuously"))) {
      failures.push("a tree with no Class C sources and no session_status passed");
    }
  }

  rmSync(box, { recursive: true, force: true });
  console.log("check-bench-boundary self-test");
  if (failures.length) {
    console.error(`\nSELF-TEST FAIL — ${failures.length} assertion(s):`);
    for (const f of failures) console.error("  " + f);
    process.exit(1);
  }
  console.log("  passes: prose, strings and asm comments in SW-01; bench identifiers in SW-02");
  console.log("  fails:  bench/head-presence/service identifiers in SW-01 C, frame, CMake and toolchain;");
  console.log("          a new, changed, disagreeing or removed session_status define; a widened field;");
  console.log("          an empty population");
  console.log("SELF-TEST PASS — the checker has teeth, in both directions.");
  process.exit(0);
}

const rootFlag = process.argv.indexOf("--root");
const ROOT =
  rootFlag >= 0 && process.argv[rootFlag + 1] ? resolve(process.argv[rootFlag + 1]!) : join(import.meta.dir, "..");
const r = audit(ROOT);

console.log(
  `scanned: ${r.classCFiles} SW-01 build file(s) for bench identifiers; ` +
    `${r.defines} NP_SESSION_STATUS_* definition(s) and ${r.fields} session_status field(s) ` +
    `in ${relative(process.cwd(), join(ROOT, "firmware")) || "firmware"}/`,
);
if (r.violations.length) {
  console.error(`\n${r.violations.length} bench-boundary violation(s):\n`);
  for (const v of r.violations) console.error("  " + v);
  console.error(
    "\nNP-FW-BENCH-001 §5 (iii) / §7.2: the Class C tier must not learn that a bypass exists.\n" +
      "Bench mode and the head-presence gate live in SW-02; session_status is full.",
  );
  process.exit(1);
}
console.log("Bench mode cannot reach SW-01 by name, and session_status has gained no bit. PASS");
process.exit(0);
