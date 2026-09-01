#!/usr/bin/env bun
/**
 * check-redaction-shape.ts — a reporting path must not branch on a sensitive predicate.
 *
 * CLAUDE.md §5.1 states the rule this enforces, and #272 is why it exists:
 *
 *   > A redaction applied conditionally on a sensitive predicate leaks that
 *   > predicate. It must be unconditional, or the predicate must not be
 *   > inferable from the pattern of redaction.
 *
 * The fault latch reported `count` unconditionally but zeroed `tick_ms` only for
 * NP_SAFETY_STATUS_CARDIAC, from two independent accessors. That made the
 * observable pair `count > 0 && tick_ms == 0` a one-bit cardiac oracle —
 * *strictly worse than not redacting at all*, because a bare relative SysTick
 * value is meaningless without a session record SHDR does not hold, whereas the
 * redaction PATTERN was self-interpreting.
 *
 * #272's own verification was structural: zero CARDIAC references survived
 * comment-stripping in the reporting path. That was done once, by hand, and kept
 * nowhere. This is that check, generalised and re-run.
 *
 * ── What is actually checked ─────────────────────────────────────────────────
 *
 *   1. Every NP_SAFETY_STATUS_* bit is classified sensitive or device-condition.
 *      An unclassified bit is a violation: adding one must be a decision. The
 *      classification is the point — "is this predicate about the person or the
 *      device?" is CLAUDE.md §5.1's defining test, asked at the one place a new
 *      status bit gets added.
 *   2. No declared reporting path mentions a sensitive predicate at all, with
 *      comments and string literals stripped. Not "no branch" — no reference.
 *      A path that cannot name the predicate cannot condition on it.
 *
 * ── The reach, stated narrowly ───────────────────────────────────────────────
 *
 * This reads the FUNCTION BODY only. It does not follow calls, so a reporting
 * path that delegates to a helper which branches on a sensitive predicate would
 * pass. That is a real limit, not a claim of completeness: it holds today
 * because the declared paths are flat marshallers by design (#272 made
 * np_fault_latch_build_report a single fixed-shape record precisely so there
 * would be nothing to follow), and it would stop holding the moment one grows a
 * private helper. Widening to the call graph needs a C parser, which this is
 * deliberately not.
 *
 * Branching on a sensitive predicate in a CONTROL path is required, not
 * forbidden — the cardiac interlock must act on NP_SAFETY_STATUS_CARDIAC, and
 * np_cvns_reenable.c and np_mod_cvns.c both do. Only reporting paths are listed.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-redaction-shape.ts --self-test
 * CI-Scans: declared SHDR reporting paths, against the classified status bits
 * CI-Scan-Paths: firmware/**
 */
import { readFileSync, mkdtempSync, mkdirSync, writeFileSync, rmSync } from "fs";
import { join } from "path";
import { tmpdir } from "os";

/** Where the status bits are defined. Classification completeness is checked
 *  against this file, so a new bit cannot be added without a decision here. */
const STATUS_HEADER = "firmware/hub_control/include/np_hub_config.h";

type Sensitivity =
  /** Tells you something about the PERSON. Must never reach a reporting path. */
  | { kind: "sensitive"; why: string }
  /** Tells you something about the DEVICE. Free to appear anywhere. */
  | { kind: "device"; why: string };

/**
 * CLAUDE.md §5.1's defining test, applied bit by bit: does this record tell us
 * something about the person? If yes → sensitive.
 */
const PREDICATES: Record<string, Sensitivity> = {
  NP_SAFETY_STATUS_OK: { kind: "device", why: "absence of any fault" },
  NP_SAFETY_STATUS_FAULT: { kind: "device", why: "a fault occurred; kind unspecified" },
  NP_SAFETY_STATUS_WATCHDOG: { kind: "device", why: "SPI heartbeat lapsed — host-side liveness" },
  NP_SAFETY_STATUS_CUTOFF: { kind: "device", why: "stimulation was cut; says nothing about why" },
  NP_SAFETY_STATUS_IMPEDANCE: {
    kind: "sensitive",
    why:
      "electrode impedance is a tissue property. CLAUDE.md §5.1 routes raw VNS and EEG " +
      "impedance to UHDR and only the derived trend to SHDR, so a redaction conditioned " +
      "on this bit would leak a contact/tissue state the boundary keeps out",
  },
  NP_SAFETY_STATUS_THERMAL: { kind: "device", why: "NTC over-temperature — device condition" },
  NP_SAFETY_STATUS_CHARGE: { kind: "device", why: "charge-density ceiling reached — a dose limit, not a measurement of the wearer" },
  NP_SAFETY_STATUS_CARDIAC: {
    kind: "sensitive",
    why:
      "an HR change >15 BPM within 5 s of stimulation, i.e. a cardiac event in the wearer. " +
      "This is the #272 predicate. Note the hub deliberately publishes the cutoff itself to " +
      "SHDR as fault_log.fault_type 'CVNS_HR_CUTOFF' under the locked safety-interlock rule — " +
      "that disclosure is a separate decision. What is forbidden is a reporting path whose " +
      "SHAPE varies with this bit, which discloses it a second time and without the decision",
  },
  NP_SAFETY_STATUS_SIG_PENDING: { kind: "device", why: "signature verification in flight" },
};

/**
 * The paths whose output shape must not vary with a sensitive predicate.
 * Control paths are deliberately absent — the interlocks must branch on cardiac.
 */
const REPORTING_PATHS: { file: string; fn: string; note: string }[] = [
  {
    file: "firmware/safety_mcu/src/np_fault_latch.c",
    fn: "np_fault_latch_build_report",
    note: "#272's fixed-shape marshaller — the single function deciding what leaves the device",
  },
  {
    file: "firmware/hub_control/src/np_session_log.c",
    fn: "np_log_shdr_fault",
    note: "writes fault_code to SHDR and discards session_ms for every caller, unconditionally",
  },
  {
    file: "firmware/hub_control/src/np_session_log.c",
    fn: "np_log_shdr_zone_auth",
    note: "accessory authentication pass/fail → SHDR",
  },
  {
    file: "firmware/pbm/src/np_pbm_session.c",
    fn: "np_pbm_session_build_shdr_summary",
    note: "per-session PBM device metrics → SHDR",
  },
  {
    file: "firmware/pbm/src/np_pbm_hal.c",
    fn: "np_pbm_hal_shdr_log_fault",
    note: "PBM fault entry → SHDR",
  },
];

/** Remove comments and string/char literals so only live code is searched. */
export function stripNonCode(src: string): string {
  let out = "";
  let i = 0;
  while (i < src.length) {
    const two = src.slice(i, i + 2);
    if (two === "/*") {
      const end = src.indexOf("*/", i + 2);
      i = end === -1 ? src.length : end + 2;
      out += " ";
    } else if (two === "//") {
      const end = src.indexOf("\n", i);
      i = end === -1 ? src.length : end;
      out += " ";
    } else if (src[i] === '"' || src[i] === "'") {
      const q = src[i]!;
      i++;
      while (i < src.length && src[i] !== q) i += src[i] === "\\" ? 2 : 1;
      i++;
      out += " ";
    } else {
      out += src[i];
      i++;
    }
  }
  return out;
}

/**
 * Body of a C function definition, brace-matched. Returns null when the
 * definition is not found — which the caller treats as a violation, never as
 * "nothing to check": a renamed function must not silently stop being guarded.
 */
export function functionBody(src: string, fn: string): string | null {
  const code = stripNonCode(src);
  // A definition starts at column 0 in this codebase's style; a call never does.
  const def = new RegExp(`^[A-Za-z_][\\w \\t\\*]*\\b${fn}\\s*\\(`, "m").exec(code);
  if (!def) return null;
  const open = code.indexOf("{", def.index);
  if (open === -1) return null;
  let depth = 0;
  for (let i = open; i < code.length; i++) {
    if (code[i] === "{") depth++;
    else if (code[i] === "}") {
      depth--;
      if (depth === 0) return code.slice(open + 1, i);
    }
  }
  return null;
}

type Audit = { violations: string[]; pathsChecked: number; sensitive: string[] };

function audit(root: string): Audit {
  const violations: string[] = [];
  const sensitive = Object.entries(PREDICATES)
    .filter(([, v]) => v.kind === "sensitive")
    .map(([k]) => k);

  // 1. Classification completeness — both directions.
  let defined: string[] = [];
  try {
    const header = readFileSync(join(root, STATUS_HEADER), "utf8");
    defined = [...header.matchAll(/^#define\s+(NP_SAFETY_STATUS_\w+)/gm)].map((m) => m[1]!);
  } catch {
    violations.push(`${STATUS_HEADER}: cannot be read — the predicate classification cannot be checked`);
  }
  for (const bit of defined) {
    if (!PREDICATES[bit]) {
      violations.push(
        `${bit}: defined but not classified — decide whether it tells you about the ` +
          `person (sensitive) or the device (device), per CLAUDE.md §5.1`,
      );
    }
  }
  for (const bit of Object.keys(PREDICATES)) {
    if (defined.length && !defined.includes(bit)) {
      violations.push(`${bit}: classified here but no longer defined in ${STATUS_HEADER} — stale entry`);
    }
  }

  // 2. No reporting path may mention a sensitive predicate.
  let pathsChecked = 0;
  for (const p of REPORTING_PATHS) {
    let src: string;
    try {
      src = readFileSync(join(root, p.file), "utf8");
    } catch {
      violations.push(`${p.file}: declared reporting path but the file does not exist`);
      continue;
    }
    const body = functionBody(src, p.fn);
    if (body === null) {
      violations.push(
        `${p.file}: ${p.fn}() is a declared reporting path but its definition was not found — ` +
          `if it was renamed, rename it here too; a guarded path must not stop being guarded silently`,
      );
      continue;
    }
    pathsChecked++;
    for (const bit of sensitive) {
      if (new RegExp(`\\b${bit}\\b`).test(body)) {
        violations.push(
          `${p.file}: ${p.fn}() references ${bit} — a reporting path whose shape can vary ` +
            `with a sensitive predicate leaks that predicate (CLAUDE.md §5.1, #272)`,
        );
      }
    }
  }

  return { violations, pathsChecked, sensitive };
}

// ── Self-test ────────────────────────────────────────────────────────────────
if (process.argv.includes("--self-test")) {
  const failures: string[] = [];

  // The extractor is the part that can silently under-report: a body it fails to
  // find, or over-strips, would make the search vacuous.
  const SAMPLE = `
/* NP_SAFETY_STATUS_CARDIAC in a comment must not count */
void np_x_report(np_report_t *out)
{
    const char *s = "NP_SAFETY_STATUS_CARDIAC in a string must not count";
    out->status = get_status();
    if (nested()) { out->slot = 1; }
}

void np_other(void) { NP_SAFETY_STATUS_CARDIAC; }
`;
  const body = functionBody(SAMPLE, "np_x_report");
  if (body === null) failures.push("extractor — failed to find a plain function body");
  else {
    if (/NP_SAFETY_STATUS_CARDIAC/.test(body)) {
      failures.push("extractor — comment or string literal survived stripping");
    }
    if (!/out->status/.test(body)) failures.push("extractor — live code was stripped");
    // Brace matching must stop at the function's own closing brace, not the
    // first one: otherwise np_other's body would be searched as part of this.
    if (/np_other/.test(body)) failures.push("extractor — body ran past the closing brace");
    if (!/out->slot/.test(body)) failures.push("extractor — nested block truncated the body");
  }
  if (functionBody(SAMPLE, "np_absent") !== null) {
    failures.push("extractor — reported a body for a function that is not defined");
  }
  // A call is not a definition.
  if (functionBody("void f(void) {\n    np_x_report(&r);\n}\n", "np_x_report") !== null) {
    failures.push("extractor — treated a call site as a definition");
  }

  // End-to-end, on fixture trees.
  const box = mkdtempSync(join(tmpdir(), "np-redaction-"));
  const build = (files: Record<string, string>): string => {
    const root = mkdtempSync(join(box, "t-"));
    for (const [rel, body2] of Object.entries(files)) {
      mkdirSync(join(root, rel.split("/").slice(0, -1).join("/")), { recursive: true });
      writeFileSync(join(root, rel), body2);
    }
    return root;
  };
  const HEADER = Object.keys(PREDICATES).map((b, i) => `#define ${b} (1U << ${i})`).join("\n") + "\n";
  const clean = (fn: string) => `void ${fn}(void)\n{\n    out->count = c;\n}\n`;
  const leaky = (fn: string) =>
    `void ${fn}(void)\n{\n    if (s & NP_SAFETY_STATUS_CARDIAC) { out->t = 0U; }\n}\n`;
  const allPaths = (mk: (fn: string) => string) =>
    Object.fromEntries(
      REPORTING_PATHS.map((p) => [
        p.file,
        REPORTING_PATHS.filter((q) => q.file === p.file).map((q) => mk(q.fn)).join("\n"),
      ]),
    );

  const expect = (label: string, root: string, needle: string | null) => {
    const { violations } = audit(root);
    if (needle === null) {
      if (violations.length) failures.push(`${label} — expected clean, got: ${violations[0]}`);
    } else if (!violations.some((x) => x.includes(needle))) {
      failures.push(`${label} — no violation matching ${JSON.stringify(needle)}`);
    }
  };

  expect(
    "clean reporting paths pass",
    build({ [STATUS_HEADER]: HEADER, ...allPaths(clean) }),
    null,
  );
  // The #272 defect itself.
  expect(
    "a reporting path branching on a sensitive predicate is caught",
    build({ [STATUS_HEADER]: HEADER, ...allPaths(leaky) }),
    "leaks that predicate",
  );
  expect(
    "an unclassified status bit is caught",
    build({
      [STATUS_HEADER]: HEADER + "#define NP_SAFETY_STATUS_NEWTHING (1U << 9)\n",
      ...allPaths(clean),
    }),
    "defined but not classified",
  );
  expect(
    "a renamed reporting path is caught rather than silently unguarded",
    build({
      [STATUS_HEADER]: HEADER,
      ...allPaths(clean),
      [REPORTING_PATHS[0]!.file]: clean("np_renamed_away"),
    }),
    "definition was not found",
  );

  rmSync(box, { recursive: true, force: true });
  console.log("check-redaction-shape self-test");
  if (failures.length) {
    console.error(`\nSELF-TEST FAIL — ${failures.length} assertion(s):`);
    for (const f of failures) console.error("  " + f);
    process.exit(1);
  }
  console.log("  extractor proven on comments, strings, nesting, calls and absence;");
  console.log("  the #272 shape, an unclassified bit and a renamed path all proven to fire");
  console.log("SELF-TEST PASS — the checker has teeth.");
  process.exit(0);
}

const ROOT = join(import.meta.dir, "..");
const result = audit(ROOT);

if (result.pathsChecked === 0) {
  console.error("check-redaction-shape: no reporting path body was found — refusing to pass vacuously.");
  process.exit(2);
}

console.log(
  `scanned: ${result.pathsChecked} reporting path(s) of ${REPORTING_PATHS.length} declared, ` +
    `against ${result.sensitive.length} sensitive predicate(s) ` +
    `(${Object.keys(PREDICATES).length} status bit(s) classified)`,
);

if (result.violations.length) {
  console.error(`\n${result.violations.length} redaction-shape violation(s):\n`);
  for (const v of result.violations) console.error("  " + v);
  console.error(
    "\nCLAUDE.md §5.1: a redaction applied conditionally on a sensitive predicate\n" +
      "leaks that predicate. It must be unconditional, or the predicate must not be\n" +
      "inferable from the pattern of redaction.",
  );
  process.exit(1);
}
console.log("No reporting path can vary its shape with a sensitive predicate. PASS");
process.exit(0);
