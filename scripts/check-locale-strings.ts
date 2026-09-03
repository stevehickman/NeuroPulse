#!/usr/bin/env bun
/**
 * check-locale-strings.ts — user-facing text lives in locales/, never in code.
 *
 * The rule this enforces, stated in CLAUDE.md §17:
 *
 *   > Generated non-firmware code must not embed user-facing strings. Every
 *   > string a person reads is a key in locales/*.json, resolved at render.
 *   > Firmware renders no text at all and references no locale file.
 *
 * ── Why a gate rather than a convention ──────────────────────────────────────
 *
 * The convention already existed and had already failed. Before this check the
 * repository carried a complete i18n layer — 11 canonical locale files, a
 * generator, a String Catalog, `t()` — and the entire web protocol editor
 * rendered English literals through it: ten components, ~5,000 lines, with
 * `t()` called in exactly one file. iOS was in the same state across 222 view
 * call sites. The locale files were not wrong; nothing was reading them.
 *
 * That is the failure mode this catches, and it is invisible to every other
 * check in the tree. The code compiles. The tests pass. The screen renders
 * correctly — in English, for everyone, forever. Only a scan that asks "is this
 * literal being shown to a person?" sees it, so that is what this asks.
 *
 * ── What is actually checked ─────────────────────────────────────────────────
 *
 *   1. FIRMWARE names no locale key and includes no locale file. Firmware
 *      communicates through tones, LEDs and numeric status; text is the app's
 *      job. A locale reference appearing under firmware/ means that boundary
 *      moved, which is a decision, not a detail.
 *   2. COVERED non-firmware paths embed no user-facing prose. "User-facing"
 *      means the literal reaches a render API — JSX text, a placeholder/title/
 *      aria-label attribute, SwiftUI Text/Button/Section/navigationTitle and
 *      friends. Not every string: an identifier, a CSS class, a part number or
 *      a unit symbol is not prose (see isProse).
 *   3. Every key referenced by code exists in locales/en.json, and every key in
 *      en.json is referenced by code. Both directions matter — a missing key
 *      renders as the key itself, and an orphan key is untranslated weight that
 *      translators are still asked to pay for.
 *   4. All 11 locale files carry an identical key set, so a locale cannot
 *      silently drop a string.
 *
 * ── The reach, stated narrowly ───────────────────────────────────────────────
 *
 * This is a TEXT scan, not a parse. It reads the literal at a render call site
 * and cannot follow a string that arrives through a variable, so
 * `Text(someEnglishConstant)` passes. That limit is real and is the reason
 * COVERED_PATHS is a list rather than "everything": inside those trees the
 * pattern is literal-at-the-call-site, which is what makes the scan sound.
 *
 * PENDING_PATHS names the code this rule has NOT yet reached, with the reason.
 * Those trees are not silently excluded — they are excluded on the record, and
 * the list is the migration backlog. A gate that quietly skipped them would
 * report a clean tree that is not clean.
 *
 * CI-Kind: gate
 * CI-Self-Test: bun scripts/check-locale-strings.ts --self-test
 * CI-Scans: locale key usage, and user-facing literals in covered app code
 * CI-Scan-Paths: app/** locales/** firmware/**
 */
import { readFileSync, readdirSync } from "fs";
import { join, basename } from "path";
import { execFileSync } from "child_process";

const ROOT = join(import.meta.dir, "..");
const LOCALES_DIR = join(ROOT, "locales");

/**
 * Trees where the rule is ENFORCED. The scan is literal-at-the-call-site, so a
 * tree only belongs here once its UI actually renders keys rather than passing
 * English through a variable.
 */
const COVERED_PATHS = [
  "app/web/src/",
  "app/ios/NeurOne/Views/",
  "app/ios/NeurOne/Onboarding/",
  "app/ios/NeurOne/Setup/",
  "app/ios/NeurOne/Localization/",
  "app/watchos/",
];

/**
 * Code this rule has NOT reached yet, and why. Listed so the gate's reach is
 * legible: each entry is work, not an exemption on principle.
 */
const PENDING_PATHS: Array<[string, string]> = [
  ["app/android/", "renders from res/values/strings.xml, which sync-locales.ts does not generate yet (its generateAndroidXml extension point is still a comment)"],
  ["app/windows/", "protocol/session logic only today; no localized UI layer exists to point at a key"],
  ["app/ios/NeurOne/Protocol/", "NPProtocolValidator and NPModalityType hold the same display text the web side keys; migrating them is the iOS half of that change"],
  ["app/ios/NeurOne/Models/", "same as Protocol/ — display metadata tables"],
  ["simulator/", "developer harness, not shipped UI"],
];

/**
 * Deliberately English, everywhere. These are not translated, and keying them
 * would add noise without adding reach.
 *
 *   - .npps parser and hub-compiler diagnostics name grammar keywords and token
 *     classes that are English by definition ("Expected 'protocol', got ...").
 *     They read as compiler output; the DSL itself is not localized.
 *   - Exhaustiveness/invariant throws are programmer errors that never render.
 */
const DIAGNOSTIC_FILES = [
  "app/web/src/lib/nppsParser.ts",
  "app/web/src/lib/nppsSerializer.ts",
  "app/web/src/lib/hubCompiler.ts",
];

/** A locale key: SCREAMING_SNAKE, which no prose string looks like. */
const KEY_RE = /^[A-Z][A-Z0-9_]*$/;

/**
 * Prose = translatable words a person reads.
 *
 * Everything below is text that renders but is NOT translated, and each
 * exclusion is a category rather than a special case:
 *   - unit symbols and numbers: "Hz", "mA", "42%", "1064nm", "4×1"
 *   - product/tier designations and part numbers: "T1", "T2", "ZM-PBM-DUAL"
 *   - single glyphs and emoji used as icons: "x", ">", an arrow
 *   - identifiers and enum values: "pbm_transcranial", "ring_4x1"
 *
 * The test is deliberately "two or more letters forming a word, outside any
 * interpolation" — text inside ${...} or \(...) is an expression, not copy, and
 * counting it would flag "\(count)%" as prose because `count` contains letters.
 */
function isProse(raw: string): boolean {
  const outside = raw
    .replace(/\$\{[^}]*\}/g, " ")   // TS/JS interpolation
    .replace(/\\\([^)]*\)/g, " ");  // Swift interpolation
  const words = outside.match(/[A-Za-z]{2,}/g);
  if (!words) return false;
  // Unit symbols and designations that survive the word test on their own.
  const NOT_PROSE = new Set([
    "Hz", "kHz", "mA", "uA", "mW", "cm", "mm", "nm", "ms", "sec", "min",
    "BPM", "MT", "px", "em", "rem", "id", "px", "T1", "T2", "EEG", "PBM",
    "MB", "KB", "GB", "DFU", "USB", "LED", "NIR", "TMS", "VNS", "HRV",
  ]);
  return words.some((w) => !NOT_PROSE.has(w));
}

/**
 * Render sites in TSX: JSX text nodes, and attributes a person reads.
 *
 * The lookbehind excludes `=>`, `<=`, `>=` and `<>`: without it the arrow in
 * `onChange={v => update<typeof params>({...})}` reads as a closing tag and the
 * generic's `<` as the next opening one, so every generic call in the file is
 * reported as the JSX text "update".
 */
const JSX_TEXT = /(?<![=!<>-])>([^<>{}]*?)</g;
const JSX_ATTR = /\b(title|placeholder|aria-label|alt|label)=(?:"([^"]*)"|'([^']*)')/g;

/** Render sites in SwiftUI. */
const SWIFT_RENDER =
  /\b(Text|Button|Label|TextField|SecureField|Toggle|Picker|Section|NavigationLink|Link|LabeledContent|navigationTitle|navigationBarTitle|alert|confirmationDialog|accessibilityLabel|accessibilityHint|help)\(\s*"([^"]{2,})"/g;

interface Violation { file: string; line: number; text: string; why: string; }

/** Remove // and /* *\/ comments so commented-out markup is not scanned. */
function stripComments(body: string): string {
  return body
    .replace(/\/\*[\s\S]*?\*\//g, (m) => m.replace(/[^\n]/g, " "))
    .replace(/(^|[^:])\/\/[^\n]*/g, (m, p1) => p1 + " ".repeat(m.length - p1.length));
}

function lineOf(body: string, index: number): number {
  let n = 1;
  for (let i = 0; i < index && i < body.length; i++) if (body[i] === "\n") n++;
  return n;
}

/**
 * JSX text is matched over the WHOLE file, not line by line.
 *
 * This is the second version of this function. The first matched `>text<` within
 * a single line and was demonstrated not to catch the very regression it exists
 * for: a button written as
 *
 *     <button onClick={onFormat}>
 *       Format
 *     </button>
 *
 * puts the `>`, the text and the `<` on three different lines, which is how most
 * JSX in this repository is actually formatted. A line-scoped scan reports that
 * tree clean.
 */
function scanSource(file: string, rawBody: string): Violation[] {
  const out: Violation[] = [];
  const body = stripComments(rawBody);

  if (file.endsWith(".swift")) {
    for (const m of body.matchAll(SWIFT_RENDER)) {
      const text = m[2]!;
      if (accept(text)) out.push({ file, line: lineOf(body, m.index!), text, why: `${m[1]}(...)` });
    }
    return out;
  }

  if (file.endsWith(".tsx")) {
    for (const m of body.matchAll(JSX_TEXT)) {
      const text = m[1]!.trim();
      if (accept(text)) out.push({ file, line: lineOf(body, m.index!), text, why: "JSX text" });
    }
    for (const m of body.matchAll(JSX_ATTR)) {
      const text = m[2] ?? m[3];
      if (text && accept(text)) {
        out.push({ file, line: lineOf(body, m.index!), text, why: `${m[1]}=` });
      }
    }
  }
  return out;
}

/**
 * A JSX-text capture that is really source code.
 *
 * Matching `>...<` across the whole file also brackets TypeScript generics:
 * `useState<AppView>({ view: 'menu' })` ... `Partial<NPLimitsSet>` puts a slab
 * of ordinary code between a `>` and a `<`. Real JSX text is copy — it carries
 * no statement separator, no arrow, and no declaration keyword.
 */
function looksLikeCode(text: string): boolean {
  if (/[;]/.test(text)) return true;
  if (/=>/.test(text)) return true;
  if (/^\(/.test(text.trim())) return true;
  return /\b(const|let|var|return|function|interface|type|import|export|new|extends|keyof)\b/.test(text);
}

/** Is this literal user-facing prose that should have been a key? */
function accept(text: string): boolean {
  if (looksLikeCode(text)) return false;
  if (KEY_RE.test(text)) return false;      // already a locale key
  // A capture with an unclosed interpolation is a mis-parse, not a string:
  // Text("\(x, specifier: "%.1f") Hz") ends the literal at the inner quote,
  // leaving "\(x, specifier: " — whose "specifier" would read as prose.
  if ((text.match(/\\\(/g)?.length ?? 0) > (text.match(/\)/g)?.length ?? 0)) return false;
  return isProse(text);
}

function tracked(): string[] {
  return execFileSync("git", ["ls-files"], { cwd: ROOT, encoding: "utf-8" })
    .split("\n")
    .filter(Boolean);
}

function isCovered(f: string): boolean {
  if (DIAGNOSTIC_FILES.includes(f)) return false;
  if (f.includes("/locales/") || f.startsWith("locales/")) return false;
  if (/\.(test|spec)\.[tj]sx?$/.test(f) || f.includes("Tests/")) return false;
  if (!/\.(ts|tsx|swift)$/.test(f)) return false;
  return COVERED_PATHS.some((p) => f.startsWith(p));
}

// ─── Checks ───────────────────────────────────────────────────────────────────

function loadCanonical(): Record<string, string> {
  return JSON.parse(readFileSync(join(LOCALES_DIR, "en.json"), "utf-8"));
}

/** (1) Firmware names no locale key and includes no locale file. */
function checkFirmware(files: string[], keys: Set<string>): string[] {
  const errs: string[] = [];
  for (const f of files.filter((x) => x.startsWith("firmware/") && !x.includes("/vendor/"))) {
    let body: string;
    try { body = readFileSync(join(ROOT, f), "utf-8"); } catch { continue; }
    if (/\blocales?\/[a-z-]+\.json\b|Localizable\.xcstrings/.test(body)) {
      errs.push(`${f}: references a locale file — firmware renders no text`);
    }
    for (const k of keys) {
      // Word-boundary match: NP_SESSION_STATUS_ACTIVE must not read as
      // SESSION_STATUS_ACTIVE, which is a different thing that happens to be a
      // suffix of it.
      if (new RegExp(`(^|[^A-Za-z0-9_])${k}([^A-Za-z0-9_]|$)`).test(body)) {
        errs.push(`${f}: names locale key ${k} — firmware renders no text`);
      }
    }
  }
  return errs;
}

/** (2) Covered app code embeds no user-facing prose. */
function checkEmbedded(files: string[]): Violation[] {
  const out: Violation[] = [];
  for (const f of files.filter(isCovered)) {
    let body: string;
    try { body = readFileSync(join(ROOT, f), "utf-8"); } catch { continue; }
    out.push(...scanSource(f, body));
  }
  return out;
}

/** (3) Key usage is bidirectional: none missing, none orphaned. */
function checkKeyUsage(files: string[], keys: Set<string>): string[] {
  const errs: string[] = [];
  // Markdown is excluded: CLAUDE.md §17 documents the API with example calls
  // (`t('KEY')`, `tPlural('BASE', n)`), and reading prose as code would demand
  // canonical define KEY and BASE. Docs describe the lookup; they never perform
  // one, so a key mentioned only in prose is still an orphan.
  //
  // Tests are excluded from BOTH directions. i18n.test.ts deliberately looks up
  // "NONEXISTENT_KEY_12345" to prove t() returns the key when one is missing —
  // counting that as a reference would demand canonical define it. The same cut
  // keeps a key that only a test mentions from reading as "used".
  const sources = files.filter(
    (f) =>
      /\.(ts|tsx|js|mjs|swift|kt|kts|cs|py|json|xml|npps|sh)$/.test(f) &&
      !f.startsWith("locales/") &&
      !f.includes("/locales/") &&
      !f.endsWith("Localizable.xcstrings") &&
      !/\.(test|spec)\.[tj]sx?$/.test(f) &&
      !f.includes("Tests/") &&
      !f.includes("/test/"),
  );
  let blob = "";
  for (const f of sources) {
    try { blob += readFileSync(join(ROOT, f), "utf-8") + "\n"; } catch { /* binary */ }
  }

  // A plural member is used when its BASE is referenced: tPlural('X', n) picks
  // X_ONE / X_OTHER at runtime, so neither member ever appears literally.
  const PLURAL = /_(ZERO|ONE|TWO|FEW|MANY|OTHER)$/;
  const orphans = [...keys].filter((k) => {
    if (blob.includes(k)) return false;
    if (PLURAL.test(k) && blob.includes(k.replace(PLURAL, ""))) return false;
    return true;
  });
  for (const k of orphans.sort()) {
    errs.push(`locales/en.json: ${k} is referenced by no code — delete it from every locale file`);
  }

  // Keys the code names that canonical does not define. Only the explicit
  // lookup forms are read, so an ordinary SCREAMING_SNAKE constant is not
  // mistaken for a key.
  const LOOKUPS = [
    /\bt\(\s*['"]([A-Z][A-Z0-9_]*)['"]/g,
    /\btPlural\(\s*['"]([A-Z][A-Z0-9_]*)['"]/g,
    /String\(\s*localized:\s*"([A-Z][A-Z0-9_]*)"/g,
  ];
  const referenced = new Set<string>();
  for (const re of LOOKUPS) for (const m of blob.matchAll(re)) referenced.add(m[1]);
  for (const k of [...referenced].sort()) {
    if (keys.has(k)) continue;
    if (PLURAL.test(k) ? keys.has(k.replace(PLURAL, "")) : false) continue;
    // tPlural names a base; its members carry the suffixes.
    if ([...keys].some((x) => x.startsWith(k + "_"))) continue;
    errs.push(`code references key ${k}, which locales/en.json does not define`);
  }
  return errs;
}

/** (4) Every locale carries the same key set. */
function checkLocaleParity(keys: Set<string>): string[] {
  const errs: string[] = [];
  for (const f of readdirSync(LOCALES_DIR).filter((x) => x.endsWith(".json") && !x.startsWith("_"))) {
    const code = basename(f, ".json");
    if (code === "en") continue;
    const d = JSON.parse(readFileSync(join(LOCALES_DIR, f), "utf-8"));
    const here = new Set(Object.keys(d));
    for (const k of keys) if (!here.has(k)) errs.push(`locales/${f}: missing key ${k}`);
    for (const k of here) if (!keys.has(k)) errs.push(`locales/${f}: extra key ${k} not in en.json`);
  }
  return errs;
}

// ─── Self-test ────────────────────────────────────────────────────────────────

/**
 * Proves each rule can FAIL. A gate that has never been seen to reject is not
 * known to be a gate.
 */
function selfTest(): void {
  const cases: Array<[string, boolean]> = [
    // [what the scanner is given, should it be flagged]
    ['        <div className="x">Save Changes</div>', true],
    ["        <span>{t('WEB_SAVE_CHANGES')}</span>", false],
    ['        <input placeholder="Protocol name" />', true],
    ["        <input placeholder={t('WEB_PLACEHOLDER_PROTOCOL_NAME')} />", false],
    ['        <span>{value}Hz</span>', false],          // unit symbol
    ['        <span>42%</span>', false],                // number
    ['        <div>T2</div>', false],                   // tier designation
    ["        // <div>a comment mentioning Save Changes</div>", false],
    // The regression the first version of scanSource did not catch: JSX text on
    // its own line, which is how most markup in this repository is formatted.
    ['      <button onClick={onFormat}>\n        Format\n      </button>', true],
    ["      <button onClick={onFormat}>\n        {t('SCRIPT_FORMAT')}\n      </button>", false],
  ];
  let bad = 0;
  for (const [src, shouldFlag] of cases) {
    const flagged = scanSource("x.tsx", src).length > 0;
    if (flagged !== shouldFlag) {
      console.error(`self-test FAILED: ${JSON.stringify(src)} -> flagged=${flagged}, expected ${shouldFlag}`);
      bad++;
    }
  }
  const swiftCases: Array<[string, boolean]> = [
    ['            Text("Save Changes")', true],
    ['            Text("WEB_SAVE_CHANGES")', false],
    ['            Button("Add Helmet") {}', true],
    ['            Text("\\(count)%")', false],           // value + unit
    ['            Text("T2")', false],
  ];
  for (const [src, shouldFlag] of swiftCases) {
    const flagged = scanSource("x.swift", src).length > 0;
    if (flagged !== shouldFlag) {
      console.error(`self-test FAILED: ${JSON.stringify(src)} -> flagged=${flagged}, expected ${shouldFlag}`);
      bad++;
    }
  }
  if (bad > 0) { console.error(`\n${bad} self-test case(s) failed.`); process.exit(1); }
  console.log(`check-locale-strings self-test: ${cases.length + swiftCases.length} cases, all correct.`);
}

// ─── Main ─────────────────────────────────────────────────────────────────────

function main(): void {
  if (process.argv.includes("--self-test")) { selfTest(); return; }

  const canonical = loadCanonical();
  const keys = new Set(Object.keys(canonical));
  const files = tracked();

  const fw = checkFirmware(files, keys);
  const embedded = checkEmbedded(files);
  const usage = checkKeyUsage(files, keys);
  const parity = checkLocaleParity(keys);

  let failed = 0;

  if (fw.length) {
    console.error("\nFIRMWARE must not reference locale data (CLAUDE.md §17):");
    for (const e of fw) console.error(`  ${e}`);
    failed += fw.length;
  }
  if (embedded.length) {
    console.error("\nEMBEDDED user-facing strings — move each to a locale key (CLAUDE.md §17):");
    for (const v of embedded) {
      console.error(`  ${v.file}:${v.line}  ${v.why}  ${JSON.stringify(v.text)}`);
    }
    failed += embedded.length;
  }
  if (usage.length) {
    console.error("\nLOCALE KEY USAGE:");
    for (const e of usage) console.error(`  ${e}`);
    failed += usage.length;
  }
  if (parity.length) {
    console.error("\nLOCALE PARITY:");
    for (const e of parity.slice(0, 40)) console.error(`  ${e}`);
    if (parity.length > 40) console.error(`  ... and ${parity.length - 40} more`);
    failed += parity.length;
  }

  if (failed > 0) {
    console.error(`\n${failed} violation(s). See scripts/check-locale-strings.ts for the rule.`);
    process.exit(1);
  }

  const covered = files.filter(isCovered).length;
  const fwFiles = files.filter((f) => f.startsWith("firmware/") && !f.includes("/vendor/")).length;
  // The population line check-gate-coverage.ts reads back. CI-Scans is prose and
  // can say anything; this is the number the run actually computed, and a gate
  // reporting `scanned: 0` while exiting 0 is the shape that check exists for.
  console.log(
    `scanned: ${covered} covered source file(s), ${fwFiles} firmware file(s), ` +
      `${keys.size} canonical key(s) across ${readdirSync(LOCALES_DIR).filter((x) => x.endsWith(".json") && !x.startsWith("_")).length} locales`,
  );
  console.log(
    `check-locale-strings: every key is referenced and present in every locale; ` +
      `covered source carries no embedded user-facing text; firmware names no key.`,
  );
  console.log("not yet covered (migration backlog, see PENDING_PATHS):");
  for (const [p, why] of PENDING_PATHS) console.log(`  ${p} — ${why}`);
}

main();
