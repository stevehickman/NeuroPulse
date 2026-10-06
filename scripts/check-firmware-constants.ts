#!/usr/bin/env bun
/**
 * check-firmware-constants.ts — a firmware constant has one name and one definition.
 *
 * Three mechanical checks over every firmware C source and header outside vendor/:
 *
 *   1. ONE DEFINITION. A `#define NAME value` appears in at most one file. A second definition is a
 *      copy that can drift (the safety SPI wire contract was defined on both processors and kept in
 *      step by a comment). Share it: firmware/common/include/np_shared_constants.h for a constant
 *      several modules need, np_spi_wire_types.h for the hub <-> safety MCU wire contract.
 *   2. UPPER_SNAKE_CASE. Every `#define` constant is named in capitals with underscores.
 *   3. UPPER_SNAKE_CASE for const objects too: a file-scope `const`, or a `static const` at any
 *      scope (tables, magic arrays, fixtures). A pointer to const is a variable and is not read.
 *
 * The single exception: a name defined OUTSIDE this project and merely used here (FreeRTOS, the USB
 * DFU specification, ST CMSIS, the C library). An exception is named exactly, with who defines it.
 *
 * Both have a stated, minimal allowlist (below). Each entry is a name that is legitimately defined
 * twice or follows an outside convention, with the reason; an entry is never a way to silence a new
 * duplicate. Function-like macros are not constants and are not read.
 *
 * Run:  bun scripts/check-firmware-constants.ts [--self-test]
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-firmware-constants.ts --self-test
 * CI-Scans: every C source and header under firmware/ (vendor/ excluded) for a #define repeated across files or not in UPPER_SNAKE_CASE
 * CI-Scan-Paths: firmware/** scripts/check-firmware-constants.ts
 */
import { readFileSync, readdirSync, statSync } from "fs";
import { join, relative } from "path";

const ROOT = join(import.meta.dir, "..");

/** Defined in more than one file ON PURPOSE. Each is a host-test substitute for target hardware or a
 *  per-harness test fixture; the substitute must not be the production definition. */
const DUPLICATE_ALLOWED: Record<string, string> = {
  NP_SNVS_LPGPR2: "np_anon_config.h overrides the MMIO register with a RAM variable under NPTEST_HOST",
  GPIOA: "host fake peripheral: np_hal_fake_regs.h and the gpio-manager test ports are separate harnesses",
  GPIOB: "host fake peripheral: np_hal_fake_regs.h and the gpio-manager test ports are separate harnesses",
  NP_HAL_OTP_BASE: "np_hal_fake_regs.h substitutes a RAM array for the OTP address on the host",
  NP_HAL_UID_BASE: "np_hal_fake_regs.h substitutes a RAM array for the UID address on the host",
  NP_FW_PUBLIC_KEY_INIT: "bootloader test key (np_bootloader_test_key.h) stands in for the production key",
  NP_TIER_AUTHORITY_PUBKEY_INIT: "safety MCU test key (np_tier_test_key.h) stands in for the production key",
  N_LINES: "sizeof() of a per-test-file table; the two tables differ",
  PBM_TILE_N: "sizeof() of a per-test-file fixture array; the two arrays differ",
};

/** The ONLY exception to UPPER_SNAKE_CASE: a name defined OUTSIDE this project and merely used here.
 *  Never a project constant, whatever its history; an entry names who defines it. */
const EXTERNAL_FILES: Record<string, string> = {
  "firmware/hub_control/include/FreeRTOSConfig.h": "FreeRTOS defines every name in its config header",
};
const EXTERNAL_PREFIXES: [string, string][] = [
  ["DFU_STATUS_err", "USB DFU 1.1 specification status names"],
];
const EXTERNAL_NAMES: Record<string, string> = {
  GPIOA: "ST CMSIS peripheral name (stm32g0xx.h), mirrored by the host fake registers",
  GPIOB: "ST CMSIS peripheral name (stm32g0xx.h), mirrored by the host fake registers",
  SPI_CR2_DS_Pos: "ST CMSIS bit-position name (stm32g0xx.h), mirrored by the host fake registers",
  memset_explicit: "C23 library function name, aliased to the project's implementation",
};
const isExternal = (path: string, name: string): boolean =>
  EXTERNAL_FILES[path] !== undefined ||
  EXTERNAL_NAMES[name] !== undefined ||
  EXTERNAL_PREFIXES.some(([p]) => name.startsWith(p));

/** A const OBJECT (not a pointer to const): file scope, or `static` at any scope. */
const CONST_OBJECT =
  /^(\s*)(static\s+)?(?:volatile\s+)?const\s+(?:struct\s+)?[A-Za-z_]\w*(?:\s+[A-Za-z_]\w*)*?\s*(\*\s*const\s*|\*\s*)?([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*(?:=|;)/;

function walk(dir: string, out: string[]): void {
  for (const e of readdirSync(dir)) {
    const p = join(dir, e);
    if (e === "vendor" || e === "build" || e === "node_modules") continue;
    if (statSync(p).isDirectory()) walk(p, out);
    else if (/\.(c|h)$/.test(e)) out.push(p);
  }
}

export function audit(files: { path: string; text: string }[]): string[] {
  const defs = new Map<string, Set<string>>();
  const out: string[] = [];
  for (const f of files) {
    f.text.split("\n").forEach((line, i) => {
      const c = CONST_OBJECT.exec(line);
      if (c) {
        const star = (c[3] ?? "").trim();
        const atFileScopeOrStatic = c[1] === "" || c[2] !== undefined;
        const objectItself = star === "" || star.includes("const"); // `T *p` is a variable
        const cname = c[4]!;
        if (atFileScopeOrStatic && objectItself && !/^[A-Z][A-Z0-9_]*$/.test(cname) && !isExternal(f.path, cname)) {
          out.push(`${f.path}:${i + 1}: const ${cname} is not UPPER_SNAKE_CASE`);
        }
      }
      const m = /^\s*#\s*define\s+([A-Za-z_]\w*)(?!\()\s+(\S.*)$/.exec(line);
      if (!m) return;
      const name = m[1]!;
      if (/_H_?$/.test(name) || name.startsWith("_")) return;
      (defs.get(name) ?? defs.set(name, new Set()).get(name)!).add(`${f.path}:${i + 1}`);
      if (!/^[A-Z][A-Z0-9_]*$/.test(name) && !isExternal(f.path, name)) {
        out.push(`${f.path}:${i + 1}: ${name} is not UPPER_SNAKE_CASE`);
      }
    });
  }
  for (const [name, sites] of defs) {
    const files = new Set([...sites].map((s) => s.replace(/:\d+$/, "")));
    if (files.size > 1 && DUPLICATE_ALLOWED[name] === undefined) {
      out.push(`${name} is defined in ${files.size} files (${[...files].join(", ")}) — define it once and share it`);
    }
  }
  return out;
}

function load(): { path: string; text: string }[] {
  const paths: string[] = [];
  walk(join(ROOT, "firmware"), paths);
  return paths.map((p) => ({ path: relative(ROOT, p), text: readFileSync(p, "utf8") }));
}

if (process.argv.includes("--self-test")) {
  const fail: string[] = [];
  const expect = (label: string, files: { path: string; text: string }[], needle: string | null) => {
    const v = audit(files);
    if (needle === null ? v.length : !v.some((x) => x.includes(needle))) fail.push(label);
  };
  expect("clean passes", [{ path: "a.h", text: "#define NP_X 1U\n" }], null);
  expect("a second definition is caught", [
    { path: "a.h", text: "#define NP_X 1U\n" },
    { path: "b.h", text: "#define NP_X 1U\n" },
  ], "NP_X is defined in 2 files");
  expect("a lower-case constant is caught", [{ path: "a.h", text: "#define kLimit 3\n" }], "not UPPER_SNAKE_CASE");
  expect("an allowlisted host substitute passes", [
    { path: "a.h", text: "#define NP_HAL_OTP_BASE 1UL\n" },
    { path: "b.h", text: "#define NP_HAL_OTP_BASE x\n" },
  ], null);
  expect("a lower-case static const table is caught", [{ path: "a.c", text: "static const uint8_t k_tab[] = { 1 };\n" }], "const k_tab is not UPPER_SNAKE_CASE");
  expect("a lower-case file-scope const is caught", [{ path: "a.c", text: "const uint16_t np_x = 3U;\n" }], "const np_x is not UPPER_SNAKE_CASE");
  expect("a pointer variable to const data is not a constant", [{ path: "a.c", text: "static const struct lfs_config *s_cfg;\n" }], null);
  expect("a plain local const variable is not read", [{ path: "a.c", text: "    const uint32_t n = 3U;\n" }], null);
  expect("an externally defined name passes only by exact name", [{ path: "a.h", text: "#define SPI_CR2_DS_Pos 8U\n#define SPI_CR1_Pos 2U\n" }], "SPI_CR1_Pos is not UPPER_SNAKE_CASE");
  expect("a function-like macro is not read", [{ path: "a.h", text: "#define np_min(a,b) a\n" }], null);
  if (fail.length) {
    console.error("check-firmware-constants self-test FAILED:\n" + fail.map((f) => "  - " + f).join("\n"));
    process.exit(1);
  }
  console.log("check-firmware-constants self-test: PASS (10 cases)");
} else {
  const files = load();
  const violations = audit(files);
  if (violations.length) {
    console.error("Firmware constant violations:\n" + violations.map((v) => "  - " + v).join("\n"));
    process.exit(1);
  }
  console.log(`scanned: ${files.length} firmware file(s) — every constant has one definition and an UPPER_SNAKE_CASE name. PASS`);
}
