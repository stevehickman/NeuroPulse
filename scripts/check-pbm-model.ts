#!/usr/bin/env bun
/**
 * check-pbm-model.ts — the PBM power/thermal constants have one source, and the
 * C runtime and the TypeScript audits both read it.
 *
 * NP-PWRSRC-001 §11 item 5, OI-PWRSRC-13 (GitHub #440). check-pbm-power.ts owns
 * the demand model and three scripts import it; the runtime governor (OI-FWHUB-09)
 * is a fourth consumer, in C. "That is a real divergence risk and the
 * countermeasure is not care." It had already happened inside one language: the
 * non-PBM overhead was 8.0 in one audit and 7.0 in two others, three literals
 * citing one 6–8 W band, and FACE_LIMIT_C and both CEM43 reference lines were
 * declared twice.
 *
 *   bun scripts/check-pbm-model.ts             # the gate
 *   bun scripts/check-pbm-model.ts --write     # regenerate the C header from the JSON
 *   bun scripts/check-pbm-model.ts --self-test # prove every rule below can fail
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-pbm-model.ts --self-test
 * CI-Scans: hardware/np_pbm_model.json, the C header emitted from it, and every TypeScript file in scripts/ for a literal re-declaration of a model constant
 * CI-Scan-Paths: hardware/np_pbm_model.json firmware/hub_control/include/np_pbm_model.generated.h scripts/**
 *
 * ── What is checked ──────────────────────────────────────────────────────────
 *
 *  A. THE MODEL IS WELL-FORMED. hardware/np_pbm_model.json parses; every constant
 *     has an UPPER_SNAKE name, a known unit, a status and a source; and every
 *     value is a whole number in the unit the C header carries it in (mW, 0.1 °C,
 *     s, m°C/W, CEM43×10). Integers, so the comparison in B has no tolerance.
 *
 *  B. THE C HEADER AGREES WITH THE MODEL. Two ways, because they fail
 *     differently. B1: the committed header is byte-identical to what --write
 *     would emit — catches a stale header and a hand edit alike. B2: the header's
 *     own #defines are parsed independently, and every constant must appear with
 *     its scaled value, NP_PBM_MODEL_VERSION must match, and no NP_PBM_ macro may
 *     exist that the model does not name. B2 is what says WHICH number disagrees,
 *     and the self-test proves it is not vacuous behind B1.
 *
 *  C. NO SCRIPT FORKS A CONSTANT. No scripts/*.ts other than pbm-model.ts may
 *     declare a model constant's TypeScript name bound to a numeric or object
 *     literal (`const OVERHEAD_W = 7.0`, `…, CEM43_CONCERN_LINE = 40.0`,
 *     `TILE_W = { … }`). Binding the name to an import is fine, and is the point.
 *     Comments are stripped first, so prose quoting an old literal does not fire.
 *
 * ── What it does NOT check ───────────────────────────────────────────────────
 *
 *  Whether any value is RIGHT. Every constant is provisional or a placeholder,
 *  and the JSON says so per constant. This gate guarantees one number, not a
 *  correct one: replacing a value from OI-PWR-01's CFD is an edit to the JSON and
 *  a --write, and both runtimes move together.
 *
 *  That a C file USES the header. The only C consumer, np_pbm_power_gov.c,
 *  includes it but admits nothing (OI-FWHUB-09). When that body is written it
 *  must read these macros and no literal; a C-side fork rule is worth adding
 *  then, against real code, rather than guessed at now.
 *
 *  Names other than those in TS_NAMES below. check-thermal-multitile.ts has its
 *  own FACE_LIMIT (the same 42 °C, a different model) and the thermal-network
 *  scripts carry many resistances; they are not this model's constants.
 */
import { readFileSync, writeFileSync, readdirSync, existsSync, mkdtempSync, mkdirSync, rmSync } from "fs";
import { join } from "path";
import { tmpdir } from "os";

// ── A: the model's shape ─────────────────────────────────────────────────────
// Defined here, not in pbm-model.ts, because the self-test must run from a copy
// of this one file outside the repository (check-gate-coverage.ts, hermeticity):
// it can import nothing that reads the tree. pbm-model.ts imports these from
// here — safe, because this module does nothing on import but define.

export const PBM_MODEL_PATH = join("hardware", "np_pbm_model.json");

/** Units the model may use, and how each is carried into C: the scale that makes
 *  every value an exact integer, and the macro suffix that names the scaled unit.
 *  Integers, not floats: the Class B governor compares watts against a PD
 *  contract negotiated in integer units, and an exact-integer rule makes the
 *  header comparable to the JSON with no tolerance to argue about. */
export const UNITS = {
  W: { scale: 1000, suffix: "MW", cUnit: "milliwatts" },
  degC: { scale: 10, suffix: "DC", cUnit: "0.1 °C" },
  min: { scale: 60, suffix: "S", cUnit: "seconds" },
  "degC/W": { scale: 1000, suffix: "MC_PER_W", cUnit: "0.001 °C per W" },
  CEM43: { scale: 10, suffix: "X10", cUnit: "CEM43 × 10" },
} as const;
export type Unit = keyof typeof UNITS;

export const STATUSES = ["provisional", "placeholder", "external-limit"] as const;
export type Status = (typeof STATUSES)[number];

export type Constant = { name: string; value: number; unit: Unit; status: Status; source: string };
export type PbmModel = { modelVersion: number; constants: Constant[] };

/** Parse and validate the model text. Returns the model and a list of problems;
 *  the caller decides whether a problem is fatal. Pure, so the gate's self-test
 *  can drive it on fixtures. */
export function parseModel(text: string): { model: PbmModel | null; problems: string[] } {
  const problems: string[] = [];
  let raw: unknown;
  try {
    raw = JSON.parse(text);
  } catch (e) {
    return { model: null, problems: [`not valid JSON: ${(e as Error).message}`] };
  }
  const r = raw as { modelVersion?: unknown; constants?: unknown };
  if (!Number.isInteger(r.modelVersion) || (r.modelVersion as number) < 1) {
    problems.push("modelVersion must be a positive integer");
  }
  if (!Array.isArray(r.constants) || r.constants.length === 0) {
    problems.push("constants must be a non-empty array");
    return { model: null, problems };
  }
  const seen = new Set<string>();
  const constants: Constant[] = [];
  for (const [i, c] of (r.constants as Record<string, unknown>[]).entries()) {
    const where = typeof c.name === "string" ? c.name : `constants[${i}]`;
    if (typeof c.name !== "string" || !/^[A-Z][A-Z0-9_]*$/.test(c.name)) {
      problems.push(`${where}: name must be UPPER_SNAKE_CASE`);
      continue;
    }
    if (seen.has(c.name)) problems.push(`${where}: duplicate name`);
    seen.add(c.name);
    if (typeof c.value !== "number" || !Number.isFinite(c.value)) {
      problems.push(`${where}: value must be a finite number`);
      continue;
    }
    if (typeof c.unit !== "string" || !(c.unit in UNITS)) {
      problems.push(`${where}: unit must be one of ${Object.keys(UNITS).join(", ")}`);
      continue;
    }
    if (!STATUSES.includes(c.status as Status)) {
      problems.push(`${where}: status must be one of ${STATUSES.join(", ")}`);
    }
    if (typeof c.source !== "string" || c.source.trim() === "") {
      problems.push(`${where}: source must name where the value comes from`);
    }
    const scaled = c.value * UNITS[c.unit as Unit].scale;
    if (Math.abs(scaled - Math.round(scaled)) > 1e-6) {
      problems.push(
        `${where}: ${c.value} ${c.unit} is not a whole number of ${UNITS[c.unit as Unit].cUnit} ` +
        `(×${UNITS[c.unit as Unit].scale} = ${scaled}); the C header carries integers`,
      );
    }
    constants.push({
      name: c.name, value: c.value, unit: c.unit as Unit,
      status: c.status as Status, source: String(c.source ?? ""),
    });
  }
  return { model: { modelVersion: r.modelVersion as number, constants }, problems };
}

/** The scaled integer the C header carries for a constant. */
export const scaledValue = (c: Constant): number => Math.round(c.value * UNITS[c.unit].scale);
/** The C macro name for a constant. */
export const macroName = (c: Constant): string => `NP_PBM_${c.name}_${UNITS[c.unit].suffix}`;

const HEADER_PATH = join("firmware", "hub_control", "include", "np_pbm_model.generated.h");
const SCRIPTS_DIR = "scripts";

/** The TypeScript names the model's constants are exported under from
 *  pbm-model.ts. A literal binding of any of these in scripts/ is a fork. */
export const TS_NAMES = [
  "TILE_W", "AVAILABLE_W", "OVERHEAD_W", "OVERHEAD_W_MIN", "OVERHEAD_W_MAX", "OVERHEAD_W_MID",
  "BUDGET_W", "THERMAL_BUDGET_W", "FACE_LIMIT_C", "T_AMBIENT_NOMINAL_C", "TAU_FACE_MIN",
  "TAU_FACE_MAX", "R_CAVITY_CONSERVATIVE", "R_CAVITY_OPTIMISTIC", "CALIB_FACE_RISE_C",
  "CALIB_TILE_W", "CEM43_REVIEW_LINE", "CEM43_CONCERN_LINE",
];

/** Files in scripts/ that rule C does not scan: the one place the names are
 *  bound (to the JSON), and this file, whose self-test fixtures contain forks. */
const RULE_C_EXEMPT = new Set(["pbm-model.ts", "check-pbm-model.ts"]);

// ── B: render and parse the header ───────────────────────────────────────────

export function renderHeader(model: PbmModel): string {
  const out: string[] = [];
  out.push(
    "/*",
    " * NeurOne Hub Control Program — PBM power and thermal model constants",
    " * GENERATED FILE — DO NOT EDIT. Source: hardware/np_pbm_model.json.",
    " * Regenerate: bun scripts/check-pbm-model.ts --write",
    " * Checked by: scripts/check-pbm-model.ts (tooling-ci.yml `pbm-model`)",
    " *",
    " * NP-PWRSRC-001 OI-PWRSRC-13: one source of truth for TILE_W, the non-PBM",
    " * overhead and the thermal constants, read by the TypeScript audits and by the",
    " * runtime governor (OI-FWHUB-09). Every value is PROVISIONAL or a PLACEHOLDER;",
    " * the JSON records each one's source. None is measured.",
    " *",
    " * Values are scaled integers; the macro suffix names the unit:",
  );
  for (const u of Object.values(UNITS)) out.push(` *   _${u.suffix.padEnd(9)} ${u.cUnit}`);
  out.push(" */", "", "#ifndef NP_PBM_MODEL_GENERATED_H", "#define NP_PBM_MODEL_GENERATED_H", "");
  out.push(`#define NP_PBM_MODEL_VERSION ${model.modelVersion}U`, "");
  for (const c of model.constants) {
    out.push(`/* ${c.value} ${c.unit} — ${c.status} */`);
    out.push(`#define ${macroName(c)} ${scaledValue(c)}${scaledValue(c) >= 0 ? "U" : ""}`);
  }
  out.push("", "#endif /* NP_PBM_MODEL_GENERATED_H */", "");
  return out.join("\n");
}

/** Every `#define NP_PBM_* <integer>` in a header, read without reference to the
 *  renderer — so B2 disagrees with B1 when the renderer is wrong, too. */
export function parseHeaderDefines(text: string): Map<string, number> {
  const defs = new Map<string, number>();
  for (const m of text.matchAll(/^[ \t]*#[ \t]*define[ \t]+(NP_PBM_[A-Z0-9_]+)[ \t]+\(?[ \t]*(-?\d+)[uUlL]*[ \t]*\)?/gm)) {
    defs.set(m[1], Number(m[2]));
  }
  return defs;
}

// ── C: forks in scripts/ ─────────────────────────────────────────────────────

/** Remove // and block comments. Crude on purpose: it can only remove text, so
 *  a string containing `//` makes the rule miss, never fire falsely. */
export function stripComments(src: string): string {
  return src.replace(/\/\*[\s\S]*?\*\//g, " ").replace(/\/\/[^\n]*/g, " ");
}

export function findForks(src: string): string[] {
  const names = TS_NAMES.join("|");
  const re = new RegExp(
    `(?:\\b(?:const|let|var)\\s+|,\\s*)(${names})\\s*(?::[^=;\\n{}]+?)?=\\s*(?:[-+]?\\.?\\d|\\{)`,
    "g",
  );
  return [...stripComments(src).matchAll(re)].map((m) => m[1]);
}

// ── The check, over any root, so the self-test can run it on fixtures ────────

export function checkTree(root: string, counts = { constants: 0, scripts: 0 }): string[] {
  const problems: string[] = [];
  const modelPath = join(root, PBM_MODEL_PATH);
  if (!existsSync(modelPath)) return [`A: ${PBM_MODEL_PATH} is missing`];
  const { model, problems: mp } = parseModel(readFileSync(modelPath, "utf8"));
  problems.push(...mp.map((p) => `A: ${PBM_MODEL_PATH}: ${p}`));
  if (!model) return problems;
  counts.constants = model.constants.length;

  const headerPath = join(root, HEADER_PATH);
  if (!existsSync(headerPath)) {
    problems.push(`B1: ${HEADER_PATH} is missing — run bun scripts/check-pbm-model.ts --write`);
  } else {
    const header = readFileSync(headerPath, "utf8");
    if (header !== renderHeader(model)) {
      problems.push(`B1: ${HEADER_PATH} is not what ${PBM_MODEL_PATH} emits — edit the JSON, then run bun scripts/check-pbm-model.ts --write`);
    }
    const defs = parseHeaderDefines(header);
    if (defs.get("NP_PBM_MODEL_VERSION") !== model.modelVersion) {
      problems.push(`B2: NP_PBM_MODEL_VERSION is ${defs.get("NP_PBM_MODEL_VERSION")}, the model is version ${model.modelVersion}`);
    }
    const expected = new Set(["NP_PBM_MODEL_VERSION"]);
    for (const c of model.constants) {
      const name = macroName(c);
      expected.add(name);
      const got = defs.get(name);
      if (got === undefined) problems.push(`B2: ${name} (${c.name}) is not defined in the header`);
      else if (got !== scaledValue(c)) {
        problems.push(`B2: ${name} is ${got} in the header; ${c.name} = ${c.value} ${c.unit} means ${scaledValue(c)}`);
      }
    }
    for (const name of defs.keys()) {
      if (!expected.has(name)) problems.push(`B2: ${name} is in the header but names no constant in the model`);
    }
  }

  const dir = join(root, SCRIPTS_DIR);
  if (existsSync(dir)) {
    for (const f of readdirSync(dir).filter((f) => f.endsWith(".ts")).sort()) {
      if (RULE_C_EXEMPT.has(f)) continue;
      counts.scripts++;
      for (const name of findForks(readFileSync(join(dir, f), "utf8"))) {
        problems.push(`C: ${SCRIPTS_DIR}/${f} binds ${name} to a literal — import it from ./pbm-model instead`);
      }
    }
  }
  return problems;
}

// ── Self-test ────────────────────────────────────────────────────────────────

function selfTest(): number {
  let failed = 0;
  const expect = (label: string, problems: string[], rule: string | null) => {
    const ok = rule === null ? problems.length === 0 : problems.some((p) => p.startsWith(`${rule}:`));
    console.log(`${ok ? "ok  " : "FAIL"}  ${label}${ok ? "" : `  (got: ${problems.join(" | ") || "no problems"})`}`);
    if (!ok) failed++;
  };

  const model = {
    modelVersion: 3,
    constants: [
      { name: "TILE_W_X", value: 22.95, unit: "W", status: "provisional", source: "fixture" },
      { name: "FACE_LIMIT_C", value: 42.0, unit: "degC", status: "external-limit", source: "fixture" },
      { name: "R_CAVITY_CONSERVATIVE", value: 0.41, unit: "degC/W", status: "provisional", source: "fixture" },
    ],
  };
  const good = parseModel(JSON.stringify(model)).model!;
  const clean = renderHeader(good);
  const consumer = `import { FACE_LIMIT_C } from "./pbm-model";\n// const OVERHEAD_W = 7.0 was the old literal\nconst BUDGET_W = AVAILABLE_W;\n`;

  const run = (files: Record<string, string>): string[] => {
    const root = mkdtempSync(join(tmpdir(), "pbm-model-"));
    try {
      for (const [p, text] of Object.entries(files)) {
        mkdirSync(join(root, p, ".."), { recursive: true });
        writeFileSync(join(root, p), text);
      }
      return checkTree(root);
    } finally {
      rmSync(root, { recursive: true, force: true });
    }
  };
  const base = { [PBM_MODEL_PATH]: JSON.stringify(model), [HEADER_PATH]: clean, [`${SCRIPTS_DIR}/check-x.ts`]: consumer };

  expect("clean tree passes (a comment quoting a literal does not fire C)", run(base), null);
  expect("A: malformed JSON", run({ ...base, [PBM_MODEL_PATH]: "{" }), "A");
  expect("A: value not a whole number of the C unit (0.4125 °C/W)", run({
    ...base,
    [PBM_MODEL_PATH]: JSON.stringify({ ...model, constants: [{ ...model.constants[2], value: 0.4125 }] }),
  }), "A");
  expect("A: unknown unit", run({
    ...base,
    [PBM_MODEL_PATH]: JSON.stringify({ ...model, constants: [{ ...model.constants[0], unit: "kW" }] }),
  }), "A");
  expect("A: constant with no source", run({
    ...base,
    [PBM_MODEL_PATH]: JSON.stringify({ ...model, constants: [{ ...model.constants[0], source: "" }] }),
  }), "A");
  expect("B1: header missing", run({ [PBM_MODEL_PATH]: base[PBM_MODEL_PATH] }), "B1");
  expect("B1: JSON value changed, header not regenerated", run({
    ...base,
    [PBM_MODEL_PATH]: JSON.stringify({ ...model, constants: [{ ...model.constants[0], value: 25.0 }, ...model.constants.slice(1)] }),
  }), "B1");
  expect("B2: hand-edited header value is named, not just 'stale'",
    run({ ...base, [HEADER_PATH]: clean.replace("22950U", "25000U") }).filter((p) => p.includes("NP_PBM_TILE_W_X_MW is 25000")), "B2");
  expect("B2: macro deleted from header", run({ ...base, [HEADER_PATH]: clean.replace(/^#define NP_PBM_FACE_LIMIT_C_DC.*$/m, "") }), "B2");
  expect("B2: extra NP_PBM_ macro the model does not name", run({ ...base, [HEADER_PATH]: clean.replace("#endif", "#define NP_PBM_STRAY_MW 1U\n#endif") }), "B2");
  expect("B2: version skew", run({ ...base, [HEADER_PATH]: clean.replace("NP_PBM_MODEL_VERSION 3U", "NP_PBM_MODEL_VERSION 2U") }), "B2");
  expect("B2: parser reads a parenthesised define", [...parseHeaderDefines("#define NP_PBM_A_MW (1250U)\n")].length === 1 &&
    parseHeaderDefines("#define NP_PBM_A_MW (1250U)\n").get("NP_PBM_A_MW") === 1250 ? [] : ["B2: parser"], null);
  expect("C: `const OVERHEAD_W = 7.0`", run({ ...base, [`${SCRIPTS_DIR}/check-y.ts`]: "const OVERHEAD_W = 7.0;\n" }), "C");
  expect("C: second declarator `, CEM43_CONCERN_LINE = 40.0`", run({ ...base, [`${SCRIPTS_DIR}/check-y.ts`]: "const A = x, CEM43_CONCERN_LINE = 40.0;\n" }), "C");
  expect("C: typed export `export const TILE_W: Record<string, number> = {`", run({ ...base, [`${SCRIPTS_DIR}/check-y.ts`]: "export const TILE_W: Record<string, number> = {\n  a: 25.0,\n};\n" }), "C");
  expect("C: leading-dot literal `let TAU_FACE_MIN = .5`", run({ ...base, [`${SCRIPTS_DIR}/check-y.ts`]: "let TAU_FACE_MIN = .5;\n" }), "C");
  expect("C: pbm-model.ts itself is exempt", run({ ...base, [`${SCRIPTS_DIR}/pbm-model.ts`]: "export const AVAILABLE_W = 40.0;\n" }), null);
  expect("C: an object-literal key is not a declaration, and the match does not run on to the next line",
    run({ ...base, [`${SCRIPTS_DIR}/check-y.ts`]: "const o = { a: 1, FACE_LIMIT_C: 42.0 };\nconst y = 5;\n" }), null);
  expect("C: an import-bound name is not a fork", run({ ...base, [`${SCRIPTS_DIR}/check-y.ts`]: "const OVERHEAD_W = OVERHEAD_W_MID;\n" }), null);

  console.log(failed ? `\n${failed} self-test case(s) FAILED` : "\ncheck-pbm-model self-test: every rule fails when it should, and passes when it should");
  return failed ? 1 : 0;
}

// ── Main ─────────────────────────────────────────────────────────────────────

if (import.meta.main) {
  const root = join(import.meta.dir, "..");
  if (process.argv.includes("--self-test")) {
    process.exit(selfTest());
  }
  if (process.argv.includes("--write")) {
    const { model, problems } = parseModel(readFileSync(join(root, PBM_MODEL_PATH), "utf8"));
    if (!model || problems.length) {
      console.error(`${PBM_MODEL_PATH} is malformed; not writing:\n  ${problems.join("\n  ")}`);
      process.exit(1);
    }
    writeFileSync(join(root, HEADER_PATH), renderHeader(model));
    console.log(`wrote ${HEADER_PATH} (model version ${model.modelVersion}, ${model.constants.length} constants)`);
    process.exit(0);
  }
  const counts = { constants: 0, scripts: 0 };
  const problems = checkTree(root, counts);
  console.log(`scanned: ${counts.constants} model constant(s) against the C header, ${counts.scripts} script(s) for forks`);
  if (problems.length) {
    console.error(`check-pbm-model: ${problems.length} problem(s)\n  ${problems.join("\n  ")}`);
    process.exit(1);
  }
  console.log(`check-pbm-model: ${PBM_MODEL_PATH}, ${HEADER_PATH} and scripts/ agree — one source, no forks.`);
}
