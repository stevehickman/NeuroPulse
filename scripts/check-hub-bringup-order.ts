#!/usr/bin/env bun
/**
 * check-hub-bringup-order.ts — the hub's bring-up sequence stays in the order
 * its correctness depends on, and its banner stays honest about the task set.
 *
 * ── Why this file exists ─────────────────────────────────────────────────────
 *
 * `np_hub_control_app_main()` is ARM-cross-only: `firmware/hub_control/src/`
 * is in HUB_SOURCES, which compiles under the arm-none-eabi toolchain and in no
 * host test. So the ordering below cannot be asserted by a unit test, and until
 * 2026-09-14 it was asserted by nothing at all — which is how OI-FWHUB-07
 * survived: `np_mod_reg_scan()` ran BEFORE `np_log_init()`, so every boot-time
 * SHDR zone-auth record was stamped with `s_device_session_count` while it was
 * still 0, and was then discarded outright when `np_log_init()` set
 * `s_shdr_pos = 0U`. Boot-time module authentication never reached SHDR, for two
 * independent reasons, and the comment three lines below the scan asserted the
 * opposite — that the log files open "before the session logger writes any
 * record".
 *
 * NP-CONV-001 §8: a convention worth writing down is worth a script, and a probe
 * must be falsified before it is trusted. `--self-test` below does that.
 *
 * ── Rule 1: four ordering constraints ────────────────────────────────────────
 *
 * Each is a real dependency, not a style preference. Stated as "A must appear
 * before B inside np_hub_control_app_main()":
 *
 *   np_safety_spi_init  →  np_mod_reg_scan
 *     GAIN_SEL[0..4] float at reset (OI-PBM-HW-01). A zone probe with them
 *     undriven reads an indeterminate transimpedance gain, so the detect result
 *     is a function of board leakage rather than of what is plugged in.
 *
 *   np_log_backend_init →  np_log_init
 *     The backend opens both partition log files; the logger assumes they are
 *     open (np_session_log.h).
 *
 *   np_log_init         →  np_mod_reg_scan
 *     THE OI-FWHUB-07 CONSTRAINT. Modules initialised by the scan write SHDR
 *     auth records (intranasal, cervical VNS — the scan's own per-zone-slot
 *     callback was removed by OI-FWHUB-05); they must carry the true device
 *     session count and must not be in a buffer np_log_init() is about to zero.
 *
 *   np_mod_reg_init     →  np_mod_reg_scan
 *     The registry must be zeroed before it is populated.
 *
 * ── Rule 2: the banner's task count matches the code ─────────────────────────
 *
 * The file banner said "Four tasks" from 2026-05-16 to 2026-09-14 while
 * app_main created five — task_protocol_rx was omitted — and the document
 * register inherited the undercount verbatim (OI-FWHUB-08). A miscount in the
 * one comment a reader starts from is cheap to make and invisible to every
 * other check, so it is counted here instead of read.
 *
 *   bun scripts/check-hub-bringup-order.ts
 *   bun scripts/check-hub-bringup-order.ts --self-test
 *
 * Exits non-zero listing each violation; exits 2 rather than passing vacuously
 * if the entry point or any named call cannot be found.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-hub-bringup-order.ts --self-test
 * CI-Scans: np_hub_control_app_main()'s call order and the file banner's task count
 * CI-Scan-Paths: firmware/hub_control/** scripts/check-hub-bringup-order.ts
 */

import { readFileSync, mkdtempSync, mkdirSync, writeFileSync, rmSync } from "fs";
import { join, resolve } from "path";
import { tmpdir } from "os";

const rootFlag = process.argv.indexOf("--root");
const ROOT =
  rootFlag >= 0 && process.argv[rootFlag + 1]
    ? resolve(process.argv[rootFlag + 1]!)
    : join(import.meta.dir, "..");

const MAIN_C = "firmware/hub_control/src/np_hub_control_main.c";
const ENTRY = "np_hub_control_app_main";

/** A must be called before B, and the reason the order matters. */
const ORDER: ReadonlyArray<readonly [string, string, string]> = [
  ["np_safety_spi_init", "np_mod_reg_scan", "GAIN_SEL[0..4] must be driven LOW before any zone probe (OI-PBM-HW-01)"],
  ["np_log_backend_init", "np_log_init", "the partition log files must be open before the logger is initialised"],
  ["np_log_init", "np_mod_reg_scan", "SHDR auth records written during the scan must carry the true session count and survive np_log_init()'s buffer reset (OI-FWHUB-07)"],
  ["np_mod_reg_init", "np_mod_reg_scan", "the registry must be zeroed before it is populated"],
];

/** Strip comments and string literals so a call named only in prose never counts. */
function stripNonCode(src: string): string {
  let out = "";
  for (let i = 0; i < src.length; i++) {
    const two = src.slice(i, i + 2);
    if (two === "/*") {
      const end = src.indexOf("*/", i + 2);
      const skipped = src.slice(i, end < 0 ? src.length : end + 2);
      // Preserve newlines so line numbers survive.
      out += skipped.replace(/[^\n]/g, " ");
      i = (end < 0 ? src.length : end + 1);
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
      const quote = c;
      let j = i + 1;
      while (j < src.length && src[j] !== quote) j += src[j] === "\\" ? 2 : 1;
      out += " ".repeat(Math.min(j, src.length - 1) - i + 1);
      i = j;
      continue;
    }
    out += c;
  }
  return out;
}

/** The body of `void <ENTRY>(void) { ... }`, brace-matched. Null if absent. */
function entryBody(code: string): string | null {
  const sig = new RegExp(`\\b${ENTRY}\\s*\\([^)]*\\)\\s*\\{`).exec(code);
  if (!sig) return null;
  let depth = 0;
  const start = sig.index + sig[0].length;
  for (let i = start - 1; i < code.length; i++) {
    if (code[i] === "{") depth++;
    else if (code[i] === "}") {
      depth--;
      if (depth === 0) return code.slice(start, i);
    }
  }
  return null;
}

/** Character offset of the first call to `fn(`, or -1. */
function callAt(body: string, fn: string): number {
  const m = new RegExp(`\\b${fn}\\s*\\(`).exec(body);
  return m ? m.index : -1;
}

/** The task count the file banner declares, or null if it declares none. */
function bannerTaskCount(src: string): number | null {
  const words: Record<string, number> = {
    one: 1, two: 2, three: 3, four: 4, five: 5, six: 6, seven: 7, eight: 8,
  };
  const m = /^\s*\*\s*(\w+)\s+tasks?\b/im.exec(src);
  if (!m) return null;
  const w = m[1]!.toLowerCase();
  if (w in words) return words[w]!;
  return /^\d+$/.test(w) ? Number(w) : null;
}

function run(root: string): { code: number; lines: string[] } {
  const out: string[] = [];
  let src: string;
  try {
    src = readFileSync(join(root, MAIN_C), "utf8");
  } catch {
    return { code: 2, lines: [`check-hub-bringup-order: cannot read ${MAIN_C} — refusing to pass vacuously.`] };
  }

  const body = entryBody(stripNonCode(src));
  if (body === null) {
    return {
      code: 2,
      lines: [`check-hub-bringup-order: could not find ${ENTRY}()'s body in ${MAIN_C} — refusing to pass vacuously.`],
    };
  }

  // Vacuity guard: every call this gate reasons about must actually be there.
  // A renamed or deleted call must fail loudly, not silently satisfy an
  // ordering constraint by being absent from both sides of it.
  const named = new Set<string>(ORDER.flatMap(([a, b]) => [a, b]));
  const missing = [...named].filter((fn) => callAt(body, fn) < 0);
  if (missing.length > 0) {
    return {
      code: 2,
      lines: [
        `check-hub-bringup-order: ${ENTRY}() does not call ${missing.join(", ")} — refusing to pass vacuously.`,
        "If a call was deliberately renamed or removed, update ORDER in this file and say why in NP-FW-HUB-001 §2.1.",
      ],
    };
  }

  const violations: string[] = [];
  for (const [a, b, why] of ORDER) {
    if (callAt(body, a) > callAt(body, b)) {
      violations.push(`  ${a}() must be called before ${b}() — ${why}`);
    }
  }

  const declared = bannerTaskCount(src);
  const actual = (body.match(/\bxTaskCreate\s*\(/g) ?? []).length;
  if (declared === null) {
    violations.push(
      `  the file banner declares no task count — it must, so a task added without updating it fails here (OI-FWHUB-08)`,
    );
  } else if (declared !== actual) {
    violations.push(
      `  the file banner says ${declared} task(s); ${ENTRY}() makes ${actual} xTaskCreate() call(s) (OI-FWHUB-08)`,
    );
  }

  out.push(
    `scanned: ${ORDER.length + 1} rule(s) — ${ORDER.length} ordering constraint(s) ` +
      `+ 1 task-count rule — against ${ENTRY}() in ${MAIN_C}`,
  );

  if (violations.length === 0) {
    out.push(
      `${ENTRY}() brings subsystems up in the order its correctness depends on, ` +
        `and the banner's task count (${declared}) matches the code.`,
    );
    return { code: 0, lines: out };
  }
  out.push(`check-hub-bringup-order: ${violations.length} violation(s) in ${MAIN_C}:`, ...violations);
  return { code: 1, lines: out };
}

// ── Self-test ────────────────────────────────────────────────────────────────
// Every rule is proven to REJECT, and the vacuity path is proven to refuse.
// The fixtures are built, never pasted, so this file can never contain a
// bring-up sequence that the gate would flag on the real tree.
if (process.argv.includes("--self-test")) {
  const box = mkdtempSync(join(tmpdir(), "np-bringup-"));

  const mainC = (opts: {
    order?: string[];
    banner?: string | null;
    tasks?: number;
    entry?: string;
  }): string => {
    const calls = opts.order ?? [
      "np_safety_spi_init",
      "np_transport_init",
      "np_log_backend_init",
      "np_log_init",
      "np_mod_reg_init",
      "np_mod_reg_scan",
    ];
    const banner =
      opts.banner === null ? "/*\n * NeurOne Hub.\n */\n" : `/*\n * ${opts.banner ?? "Five"} tasks:\n */\n`;
    const entry = opts.entry ?? ENTRY;
    const body =
      calls.map((c) => `    ${c}();`).join("\n") +
      "\n" +
      Array.from({ length: opts.tasks ?? 5 }, () => "    xTaskCreate(t, \"n\", 1, 0, 1, 0);").join("\n");
    return `${banner}void ${entry}(void)\n{\n${body}\n}\n`;
  };

  const build = (body: string): string => {
    const root = mkdtempSync(join(box, "t-"));
    mkdirSync(join(root, "firmware/hub_control/src"), { recursive: true });
    writeFileSync(join(root, MAIN_C), body);
    return root;
  };

  const exec = (root: string) => {
    const r = Bun.spawnSync([process.execPath, import.meta.path, "--root", root], {
      stdout: "pipe",
      stderr: "pipe",
    });
    return {
      code: r.exitCode,
      out: new TextDecoder().decode(r.stdout) + new TextDecoder().decode(r.stderr),
    };
  };

  const failures: string[] = [];
  const expect = (label: string, root: string, want: number, needle: string) => {
    const { code, out } = exec(root);
    if (code !== want) failures.push(`${label} — expected exit ${want}, got ${code}\n${out}`);
    else if (!out.includes(needle)) {
      failures.push(`${label} — exit ${want} but output lacked ${JSON.stringify(needle)}\n${out}`);
    }
  };

  // The correct order passes.
  expect("correct bring-up accepted", build(mainC({})), 0, "order its correctness depends on");

  // Rule 1 — each of the four constraints, inverted one at a time.
  const inverted: Record<string, string[]> = {
    "GAIN_SEL before the probe": [
      "np_log_backend_init", "np_log_init", "np_mod_reg_init",
      "np_mod_reg_scan", "np_safety_spi_init",
    ],
    "log files before the logger": [
      "np_safety_spi_init", "np_log_init", "np_log_backend_init",
      "np_mod_reg_init", "np_mod_reg_scan",
    ],
    // The OI-FWHUB-07 regression, exactly as it stood before 2026-09-14.
    "logger before the scan (OI-FWHUB-07)": [
      "np_safety_spi_init", "np_mod_reg_init", "np_mod_reg_scan",
      "np_log_backend_init", "np_log_init",
    ],
    "registry zeroed before the probe": [
      "np_safety_spi_init", "np_log_backend_init", "np_log_init",
      "np_mod_reg_scan", "np_mod_reg_init",
    ],
  };
  for (const [label, order] of Object.entries(inverted)) {
    expect(`rule 1 rejects: ${label}`, build(mainC({ order })), 1, "must be called before");
  }

  // Rule 2 — the OI-FWHUB-08 regression, and a banner with no count at all.
  expect("rule 2 rejects an undercounted banner", build(mainC({ banner: "Four", tasks: 5 })), 1, "task-count");
  expect("rule 2 rejects a task added without the banner", build(mainC({ banner: "Five", tasks: 6 })), 1, "banner says 5");
  expect("rule 2 rejects a banner with no count", build(mainC({ banner: null })), 1, "declares no task count");

  // Vacuity — a missing entry point, a missing call, and a missing file must all
  // REFUSE (exit 2), never pass. This is the #118 shape the meta-gate guards.
  expect("refuses when the entry point is absent", build(mainC({ entry: "some_other_main" })), 2, "refusing to pass vacuously");
  expect(
    "refuses when a named call is absent",
    build(mainC({ order: ["np_safety_spi_init", "np_log_backend_init", "np_log_init", "np_mod_reg_init"] })),
    2,
    "does not call np_mod_reg_scan",
  );
  expect("refuses when the file is absent", mkdtempSync(join(box, "empty-")), 2, "cannot read");

  // A call named only in a comment must not satisfy an ordering constraint —
  // the whole point of stripNonCode(). Here the comment mentions the scan early
  // and the real call is still last, so this must PASS.
  const withProse = mainC({}).replace(
    "    np_safety_spi_init();",
    "    /* np_mod_reg_scan() runs later; see NP-FW-HUB-001 section 2.1 */\n    np_safety_spi_init();",
  );
  expect("a call named only in a comment does not count", build(withProse), 0, "order its correctness depends on");

  rmSync(box, { recursive: true, force: true });
  console.log("check-hub-bringup-order self-test");
  if (failures.length) {
    console.error(`\nSELF-TEST FAIL — ${failures.length} assertion(s):`);
    for (const f of failures) console.error("  " + f);
    process.exit(1);
  }
  console.log("  11 case(s): all 4 ordering constraints proven to reject, 3 task-count cases,");
  console.log("  3 vacuity refusals, and comment text proven not to count as a call.");
  console.log("SELF-TEST PASS — the checker has teeth.");
  process.exit(0);
}

const { code, lines } = run(ROOT);
for (const l of lines) (code === 0 ? console.log : console.error)(l);
process.exit(code);
