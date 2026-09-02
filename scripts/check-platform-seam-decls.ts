#!/usr/bin/env bun
/**
 * Assert that every platform seam is declared in exactly ONE place — its header.
 *
 * NP-SW-CI-001 §4.4.1 (OI-SWCI-18, SW-01) and §4.8.4 (OI-SWCI-40, SW-02).
 *
 * The property both items established is the same one: a platform symbol
 * declared by an `extern` line inside the .c that calls it is a contract with
 * exactly one party. The definition lives in another translation unit, C has no
 * mangling, and so a caller whose declaration has drifted from the definition
 * compiles clean AND LINKS CLEAN. That is not hypothetical here — it was
 * measured while closing OI-SWCI-40: declaring np_mod_eeg_hal_read_impedance()
 * as returning `int` when the definition returns `float` built rc=0 and linked
 * rc=0 on the pre-change tree.
 *
 * Deleting the local `extern`s is what makes the compiler compare caller against
 * definition. Nothing stops them coming back one at a time, which is what this
 * gate is for: the property was expensive to establish and is one careless
 * `extern` from being silently lost, in a file nobody re-reads.
 *
 * ── Why the seam names are parsed and never listed here ──────────────────────
 *
 * The names come out of the headers at run time. Restating them in this file
 * would be the one-value-two-places shape that NP-SW-CI-001 §4.3 (Defect C) and
 * §4.6 (Defect D) both are, in a checker written to enforce single-sourcing —
 * and it would fail the wrong way, going quiet as seams were added.
 *
 * ── What this does NOT check ────────────────────────────────────────────────
 *
 * Only seams the headers declare. `extern`s for symbols with no header at all
 * are a different and larger finding (OI-SWCI-43) and are deliberately out of
 * scope: there is no declaration to compare against, so there is nothing this
 * gate could assert beyond "a header should exist", which is a design decision
 * and not a build property.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-platform-seam-decls.ts --self-test
 * CI-Scans: every .c under firmware/hub_control, firmware/application and firmware/safety_mcu/src, for local extern declarations of symbols their platform header already declares
 * CI-Scan-Paths: firmware/hub_control/** firmware/application/** firmware/safety_mcu/** firmware/platform/**
 */
import { readdirSync, readFileSync, statSync, mkdtempSync, mkdirSync, writeFileSync, rmSync } from "fs";
import { join, dirname } from "path";
import { tmpdir } from "os";

/**
 * The (header, tree) pairs. Each says: these seams are single-sourced by this
 * header, so no .c under this tree may declare one itself.
 */
const CONTRACTS = [
  {
    item: "OI-SWCI-40",
    header: "firmware/platform/include/np_sw02_platform_hal.h",
    tree: "firmware/hub_control",
  },
  {
    item: "OI-SWCI-18",
    header: "firmware/safety_mcu/include/np_safety_hal.h",
    tree: "firmware/safety_mcu/src",
  },
  {
    // Added 2026-09-02 with the clock seam (NP-SW-CI-001 §4.11, OI-SWCI-41).
    // firmware/application became the first tree outside hub_control to CALL a
    // platform seam: np_app_main.c calls np_platform_clock_init().  It includes
    // the header rather than declaring the symbol, so this entry passes on the
    // day it is added — which is the point.  Without it the property this gate
    // exists to hold would simply not cover the newest caller, and an `extern`
    // re-added there would reopen OI-SWCI-40 in a file nothing scans.
    item: "OI-SWCI-40",
    header: "firmware/platform/include/np_sw02_platform_hal.h",
    tree: "firmware/application",
  },
];

/** Strip comments and string literals so neither can contribute a match. */
function decomment(src: string): string {
  return src
    .replace(/\/\*[\s\S]*?\*\//g, " ")
    .replace(/\/\/[^\n]*/g, " ")
    .replace(/"(\\.|[^"\\])*"/g, '""');
}

/** Function names declared by a header. */
function seamsOf(headerSrc: string): Set<string> {
  const out = new Set<string>();
  for (const stmt of decomment(headerSrc).split(";")) {
    if (!stmt.includes("(")) continue;
    // The declarator name is the identifier immediately before the first '('.
    const m = /([A-Za-z_]\w*)\s*\(/.exec(stmt);
    if (m) out.add(m[1]);
  }
  return out;
}

/**
 * Local `extern` declarations of a function, as [name, line] pairs.
 * Multi-line declarations are handled by splitting on ';' and counting the
 * newlines consumed so far, so the reported line is the `extern` keyword's.
 */
function localExternDecls(src: string): Array<{ name: string; line: number }> {
  const found: Array<{ name: string; line: number }> = [];
  const clean = decomment(src);
  let consumed = 0;
  for (const stmt of clean.split(";")) {
    const before = consumed;
    consumed += (stmt.match(/\n/g) || []).length;
    if (!/\bextern\b/.test(stmt) || !stmt.includes("(")) continue;
    // Skip function-pointer variables and anything that is not a plain
    // function declaration: require `extern` and a declarator before '('.
    const m = /\bextern\b[\s\S]*?([A-Za-z_]\w*)\s*\(/.exec(stmt);
    if (!m) continue;
    const leading = (stmt.slice(0, stmt.indexOf("extern")).match(/\n/g) || []).length;
    found.push({ name: m[1], line: before + leading + 1 });
  }
  return found;
}

function cFilesUnder(dir: string): string[] {
  const out: string[] = [];
  const walk = (d: string) => {
    let entries: string[];
    try {
      entries = readdirSync(d);
    } catch {
      return;
    }
    for (const e of entries) {
      const p = join(d, e);
      if (statSync(p).isDirectory()) walk(p);
      else if (e.endsWith(".c")) out.push(p);
    }
  };
  walk(dir);
  return out.sort();
}

function run(root: string): number {
  const violations: string[] = [];
  // Distinct seams, not seams-per-contract: one header can appear in more than
  // one contract (np_sw02_platform_hal.h governs both firmware/hub_control and
  // firmware/application), and summing sizes would report 153 for 89 seams —
  // a population probe that overstates the population is the failure it exists
  // to prevent.  Keyed by header so two contracts sharing one cannot inflate it.
  const seamsSeen = new Set<string>();
  let fileTotal = 0;

  for (const c of CONTRACTS) {
    const headerPath = join(root, c.header);
    let headerSrc: string;
    try {
      headerSrc = readFileSync(headerPath, "utf8");
    } catch {
      // A missing header is a failure, never a silent pass: this gate must not
      // be able to succeed by finding nothing to look at (§6.6, the
      // `find … | xargs -r` step that passed on zero files for six phases).
      console.log(`FAIL: contract header not found: ${c.header}`);
      violations.push(`${c.header}: missing`);
      continue;
    }
    const seams = seamsOf(headerSrc);
    if (seams.size === 0) {
      console.log(`FAIL: ${c.header} declares no seams — the parse is broken`);
      violations.push(`${c.header}: parsed zero seams`);
      continue;
    }
    for (const n of seams) seamsSeen.add(`${c.header}::${n}`);

    const files = cFilesUnder(join(root, c.tree));
    if (files.length === 0) {
      // Same reasoning as the missing header above: a walk that finds nothing
      // must fail, not pass. This is the `find … | xargs -r` shape (§6.6) —
      // rename or move the tree and the gate goes green on zero files.
      console.log(`FAIL: ${c.tree} contains no .c files — the walk is broken`);
      violations.push(`${c.tree}: walked zero translation units`);
    }
    fileTotal += files.length;
    for (const f of files) {
      const rel = f.slice(root.length + 1);
      for (const d of localExternDecls(readFileSync(f, "utf8"))) {
        if (!seams.has(d.name)) continue;
        violations.push(
          `${rel}:${d.line}: local \`extern\` for ${d.name}, which ${c.header} ` +
            `already declares — include the header instead (${c.item})`,
        );
      }
    }
    console.log(
      `${c.item}: ${seams.size} seams declared by ${c.header}; scanned ${files.length} .c files under ${c.tree}`,
    );
  }

  // The population probe (NP-CONV-001 §4.0.1a): the number this run actually
  // looked at, so a gate that has quietly stopped seeing anything reports zero
  // instead of passing. Translation units, not seams — the seam count can be
  // healthy while the file walk finds nothing.
  const seamTotal = seamsSeen.size;
  console.log(`scanned: ${fileTotal} translation units against ${seamTotal} single-sourced seams`);
  console.log(
    `single-sourced seam declarations: ${violations.length ? "FAIL" : "PASS"} ` +
      `(${seamTotal} seams, ${fileTotal} translation units)`,
  );
  violations.forEach((v) => console.log("   " + v));
  return violations.length ? 1 : 0;
}

// ── Self-test ────────────────────────────────────────────────────────────────
// Falsified in both directions before it is trusted, and the falsification is
// kept and re-run rather than done once by hand — §4.0.1a's rule, and the
// answer to §6.6's vacuous-probe family: a gate that has only ever been seen to
// pass has not been shown to be capable of failing.
if (process.argv.includes("--self-test")) {
  const root = mkdtempSync(join(tmpdir(), "np-seamdecls-"));
  const write = (rel: string, body: string) => {
    mkdirSync(dirname(join(root, rel)), { recursive: true });
    writeFileSync(join(root, rel), body);
  };

  const HEADERS: Array<[string, string]> = [
    [CONTRACTS[0].header, "extern float np_mod_eeg_hal_read_impedance(uint8_t ch);\n"],
    [CONTRACTS[1].header, "extern int np_hal_gpio_write_pin(int port, int pin, int state);\n"],
  ];
  const clean = () => {
    rmSync(root, { recursive: true, force: true });
    for (const [p, body] of HEADERS) write(p, body);
    write(`${CONTRACTS[0].tree}/modules/np_mod_eeg.c`,
      '#include "np_sw02_platform_hal.h"\nfloat f(void) { return np_mod_eeg_hal_read_impedance(0); }\n');
    write(`${CONTRACTS[1].tree}/np_gpio_mgr.c`,
      '#include "np_safety_hal.h"\nvoid g(void) { np_hal_gpio_write_pin(0, 0, 1); }\n');
    // Every remaining contract tree needs at least one translation unit, because
    // this gate deliberately FAILS a tree it walked zero files in (see the
    // zero-files case below).  Written from the contract list rather than named
    // one by one, so adding a contract cannot silently leave its tree empty and
    // turn the whole self-test red — which is exactly what adding
    // firmware/application did on 2026-09-02 before this loop existed.
    for (const c of CONTRACTS.slice(2)) {
      write(`${c.tree}/np_seamdecls_selftest.c`,
        '#include "np_sw02_platform_hal.h"\nfloat f2(void) { return np_mod_eeg_hal_read_impedance(0); }\n');
    }
  };

  let failures = 0;
  const expect = (label: string, want: number, got: number) => {
    const ok = want === got;
    if (!ok) failures++;
    console.log(`  ${ok ? "ok  " : "FAIL"}  ${label} (want rc=${want}, got rc=${got})`);
  };

  console.log("self-test:");
  clean();
  expect("clean tree passes", 0, run(root));

  // Direction 1: a re-introduced local extern is caught (SW-02).
  clean();
  write(`${CONTRACTS[0].tree}/modules/np_mod_eeg.c`,
    '#include "np_sw02_platform_hal.h"\nextern float np_mod_eeg_hal_read_impedance(uint8_t ch);\n');
  expect("SW-02 local extern is caught", 1, run(root));

  // Direction 1b: caught even when it has drifted (the actual hazard), and
  // even spread across lines, which is how the real ones were written.
  clean();
  write(`${CONTRACTS[0].tree}/modules/np_mod_eeg.c`,
    '#include "np_sw02_platform_hal.h"\nextern int\nnp_mod_eeg_hal_read_impedance(uint8_t ch);\n');
  expect("SW-02 drifted multi-line extern is caught", 1, run(root));

  // Direction 2: the SW-01 contract is guarded by the same gate.
  clean();
  write(`${CONTRACTS[1].tree}/np_gpio_mgr.c`,
    '#include "np_safety_hal.h"\nextern int np_hal_gpio_write_pin(int port, int pin, int state);\n');
  expect("SW-01 local extern is caught", 1, run(root));

  // A commented-out extern is not a declaration and must not be reported.
  clean();
  write(`${CONTRACTS[0].tree}/modules/np_mod_eeg.c`,
    '#include "np_sw02_platform_hal.h"\n/* extern float np_mod_eeg_hal_read_impedance(uint8_t ch); */\n');
  expect("commented-out extern is not a violation", 0, run(root));

  // An extern for a symbol NO header declares is out of scope (OI-SWCI-43),
  // and must not be reported — otherwise this gate quietly becomes a different
  // and much larger rule than the one it documents.
  clean();
  write(`${CONTRACTS[0].tree}/modules/np_mod_eeg.c`,
    '#include "np_sw02_platform_hal.h"\nextern void np_mod_eeg_init(void);\n');
  expect("extern with no header declaration is out of scope", 0, run(root));

  // The gate must fail rather than pass when it has nothing to look at —
  // both halves: no header to read, and no translation unit to scan.
  rmSync(root, { recursive: true, force: true });
  mkdirSync(root, { recursive: true });
  expect("missing headers fail rather than pass vacuously", 1, run(root));

  clean();
  rmSync(join(root, CONTRACTS[0].tree), { recursive: true, force: true });
  expect("an empty source tree fails rather than passing on zero files", 1, run(root));

  rmSync(root, { recursive: true, force: true });
  console.log(failures ? `\nself-test: ${failures} FAILED` : "\nself-test: all passed");
  process.exit(failures ? 1 : 0);
}

process.exit(run(process.cwd()));
