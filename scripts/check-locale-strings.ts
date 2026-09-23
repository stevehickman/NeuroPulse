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
 *   4. All 11 locale files carry every en.json key, so a locale cannot
 *      silently drop a string — and nothing more, except the extra CLDR plural
 *      categories of a family en.json defines (OI-I18N-03).
 *   5. TRANSLATIONS enter only verified, and stay as verified (#191). A locale
 *      value that differs from the English must have an entry in
 *      locales/_translations.json, and must still hash to what was verified;
 *      a current translation must keep the English's placeholders. See
 *      the ledger section below for the states and scripts/translations.ts for the
 *      only path that writes one.
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
import { createHash } from "crypto";
import { existsSync, readFileSync, readdirSync, writeFileSync } from "fs";
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
  "app/ios/NeurOne/Protocol/",
  "app/ios/NeurOne/Models/",
  "app/watchos/",
  "app/android/app/src/main/kotlin/life/neurone/app/ui/",
];

/**
 * Code this rule has NOT reached yet, and why. Listed so the gate's reach is
 * legible: each entry is work, not an exemption on principle.
 */
const PENDING_PATHS: Array<[string, string]> = [
  ["app/android/core/", "a pure-JVM module by design (no Android plugin, ISC-2..4), so it cannot reference R.string at all; its display text needs a key-to-resource indirection first"],
  ["app/android/app/ (outside ui/)", "BLE, upload and signing code — diagnostics and protocol constants, not rendered text"],
  ["app/windows/", "protocol/session logic only today; no localized UI layer exists to point at a key"],
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
  "app/ios/NeurOne/Protocol/NPProtocolScripting.swift",
  "app/ios/NeurOne/Protocol/NPNamespace.swift",
  "app/ios/NeurOne/Protocol/NPBundledProtocols.swift",
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
    .replace(/\$\{[^}]*\}/g, " ")        // TS/JS and Kotlin ${...}
    .replace(/\$[A-Za-z_]\w*/g, " ")      // Kotlin's bare $identifier form
    .replace(/\\\([^)]*\)/g, " ");       // Swift interpolation
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

/**
 * Render sites in Compose. `stringResource(...)` is itself @Composable, so the
 * literal at one of these is always inside a composable and always replaceable.
 */
const KOTLIN_RENDER =
  /\b(Text|Button|OutlinedButton|TextButton|Label|TextField|OutlinedTextField|Badge|Tab|AlertDialog|Snackbar)\(\s*"([^"]{2,})"/g;

/** Render sites in SwiftUI. */
const SWIFT_RENDER =
  /\b(Text|Button|Label|TextField|SecureField|Toggle|Picker|Section|NavigationLink|Link|LabeledContent|navigationTitle|navigationBarTitle|alert|confirmationDialog|accessibilityLabel|accessibilityHint|help)\(\s*"([^"]{2,})"/g;

/**
 * Prose reaching a view through a NAMED ARGUMENT rather than a SwiftUI view's
 * first positional one. The project's own field wrappers take `label:`,
 * `title:`, `message:`, `detail:` — SWIFT_RENDER never sees those, which is how
 * 102 English strings in LimitsSettingsView, ModalityEditorView, Under16View
 * and the portal passed the gate. `systemName:`/`icon:`/`param:`/`key:` are
 * excluded on purpose: SF Symbol names and field identifiers, not text.
 */
const SWIFT_NAMED_ARG = /\b(label|title|message|detail|placeholder|prompt|caption|footer|header)\s*:\s*"([^"]{2,})"/g;

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

  if (file.endsWith(".kt")) {
    for (const m of body.matchAll(KOTLIN_RENDER)) {
      const text = m[2]!;
      if (accept(text)) out.push({ file, line: lineOf(body, m.index!), text, why: `${m[1]}(...)` });
    }
    return out;
  }

  if (file.endsWith(".swift")) {
    for (const m of body.matchAll(SWIFT_RENDER)) {
      const text = m[2]!;
      if (accept(text)) out.push({ file, line: lineOf(body, m.index!), text, why: `${m[1]}(...)` });
    }
    for (const m of body.matchAll(SWIFT_NAMED_ARG)) {
      const text = m[2]!;
      if (accept(text)) out.push({ file, line: lineOf(body, m.index!), text, why: `${m[1]}:` });
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
  if (!/\.(ts|tsx|swift|kt)$/.test(f)) return false;
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
  // This file is excluded for the same reason as Markdown: its own comments
  // spell out the lookup forms (`t('KEY')`, `tPlural('BASE', n)`) as examples,
  // and a checker must not read its own documentation as a call site.
  //
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
      !f.includes("/test/") &&
      f !== "scripts/check-locale-strings.ts",
  );
  let blob = "";
  for (const f of sources) {
    try { blob += readFileSync(join(ROOT, f), "utf-8") + "\n"; } catch { /* binary */ }
  }

  // A plural member is used when its BASE is referenced: tPlural('X', n) picks
  // X_ONE / X_OTHER at runtime, so neither member ever appears literally.
  const PLURAL = /_(ZERO|ONE|TWO|FEW|MANY|OTHER)$/;

  // Android never names the canonical key: sync-locales lowercases it into a
  // resource name, and Kotlin says R.string.tab_history / @string/tab_history.
  // Collect those so a key used only by Android does not read as an orphan.
  const androidRefs = new Set<string>();
  for (const m of blob.matchAll(/(?:R\.string\.|R\.plurals\.|@string\/|@plurals\/)([a-z0-9_]+)/g)) {
    androidRefs.add(m[1]!);
  }
  const usedByAndroid = (k: string) => androidRefs.has(k.toLowerCase());

  const orphans = [...keys].filter((k) => {
    if (blob.includes(k)) return false;
    if (usedByAndroid(k)) return false;
    if (PLURAL.test(k)) {
      const base = k.replace(PLURAL, "");
      if (blob.includes(base) || usedByAndroid(base)) return false;
    }
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
  const lowerKeys = new Set([...keys].map((k) => k.toLowerCase()));
  const pluralBases = new Set(
    [...keys].filter((k) => PLURAL.test(k)).map((k) => k.replace(PLURAL, "").toLowerCase()),
  );
  for (const name of [...androidRefs].sort()) {
    if (lowerKeys.has(name) || pluralBases.has(name)) continue;
    errs.push(`Android references @string/${name}, which no canonical key generates`);
  }

  for (const k of [...referenced].sort()) {
    if (keys.has(k)) continue;
    if (PLURAL.test(k) ? keys.has(k.replace(PLURAL, "")) : false) continue;
    // tPlural names a base; its members carry the suffixes.
    if ([...keys].some((x) => x.startsWith(k + "_"))) continue;
    errs.push(`code references key ${k}, which locales/en.json does not define`);
  }
  return errs;
}

/**
 * (4) Every locale carries every en.json key, so a locale cannot silently drop
 * a string. The one thing it may add is a plural category its own language
 * has and English does not (OI-I18N-03): `_FEW` / `_MANY` for Russian, `_ZERO`
 * / `_TWO` / `_FEW` / `_MANY` for Arabic, on a family en.json already defines.
 * Anything else extra is an orphan.
 */
export function localeParityErrors(
  canonical: Record<string, string>,
  locale: string,
  values: Record<string, string>,
): string[] {
  const errs: string[] = [];
  const allowed = sourcesFor(canonical, locale);
  for (const k of Object.keys(canonical)) {
    if (!(k in values)) errs.push(`locales/${locale}.json: missing key ${k}`);
  }
  for (const k of Object.keys(values)) {
    if (k in allowed) continue;
    errs.push(
      PLURAL_MEMBER.test(k)
        ? `locales/${locale}.json: extra key ${k} — not a plural family in en.json, or not a CLDR ` +
            `plural category of "${locale}" (it has ${localePluralCategories(locale).join(", ")})`
        : `locales/${locale}.json: extra key ${k} not in en.json`,
    );
  }
  return errs;
}

function checkLocaleParity(canonical: Record<string, string>): string[] {
  const errs: string[] = [];
  for (const f of readdirSync(LOCALES_DIR).filter((x) => x.endsWith(".json") && !x.startsWith("_"))) {
    const code = basename(f, ".json");
    if (code === "en") continue;
    const d = JSON.parse(readFileSync(join(LOCALES_DIR, f), "utf-8"));
    errs.push(...localeParityErrors(canonical, code, d));
  }
  return errs;
}

/**
 * A canonical value must never carry a platform's own interpolation syntax.
 * `String(localized: "K")` and `stringResource(R.string.k)` are lookups, not
 * string literals — a `\\(expr)` or `${expr}` that survives into the value is
 * rendered to the user verbatim, and the argument it names is silently dropped
 * at the call site. Placeholders are `{0}`, `{1}` (§17); `${0}` is a literal
 * dollar sign in front of one, which is fine.
 *
 * The `@` conversion is the same class of defect and the reason this rule was
 * widened (OI-I18N-01). `%@` and `%1$@` are Apple-only spellings, and
 * `sync-locales.ts` converts placeholders by matching `{n}` — so a value
 * carrying `@` matches nothing, both generators copy it through verbatim, and
 * it lands in `Localizable.xcstrings` as exactly what `String(format:)` wants.
 * Apple therefore renders it correctly and nothing fails. Android's
 * `Resources.getString(id, args)` runs `java.util.Formatter`, where `@` is an
 * unknown conversion: `UnknownFormatConversionException`, a crash rather than a
 * mis-render. Ten keys sat in that state for months because the only platform
 * reading them was the one it happens to be correct on.
 *
 * The rest of the printf family followed (OI-I18N-02). `%d` and `%.1f` are
 * valid in both `String(format:)` and `java.util.Formatter`, so they never
 * crashed — but a bare `%d` is consumed in source order, so a translator could
 * not reorder arguments, and web's `t()` rendered it literally. Fourteen keys
 * were converted before translation started (#191) so none has to be
 * re-translated; the rule now rejects every conversion, not only `@`.
 */
function checkInterpolationSyntax(canonical: Record<string, string>): string[] {
  const errs: string[] = [];
  for (const [k, v] of Object.entries(canonical)) {
    if (v.includes("\\(")) {
      errs.push(`${k}: carries Swift interpolation \\(…) — use {0} and String(format:) at the call site`);
    }
    if (/\$\{[^0-9]/.test(v)) {
      errs.push(`${k}: carries \${…} interpolation — use {0} and pass the value as an argument`);
    }
    const apple = v.match(/%(?:\d+\$)?@/g);
    if (apple) {
      errs.push(
        `${k}: carries the Apple-only ${apple[0]} conversion — use {0} (Android's Formatter ` +
          `throws UnknownFormatConversionException on @, so this crashes rather than mis-renders). ` +
          `Convert EVERY conversion in the value, not just this one: the generator escapes a ` +
          `literal % only once a {n} is present, so a half-converted value emits %%1$d.`,
      );
    }
    // OI-I18N-02: the rest of the printf family. %d / %.1f / %1$d are valid on
    // both native platforms, so they never crash — what they cost is §17.2's
    // purpose. A canonical {n} is positional by construction, so a translator
    // may reorder arguments; a bare %d is consumed in source order, and web's
    // t() substitutes {n} and nothing else, so it renders the specifier itself.
    // No space in the flag set: a literal percent before a word ("{0}% of")
    // and the %MT unit are text, not conversions.
    const printf = v.match(/%(?:\d+\$)?[-#+0,(]*\d*(?:\.\d+)?[diouxXeEfgGcs]/g);
    if (printf && !apple) {
      errs.push(
        `${k}: carries the printf conversion ${printf[0]} — use {0}/{1} so a translation can reorder ` +
          `its arguments, and format the value at the call site (on Apple, String(…) or ` +
          `NPNumberFormatter; {n} generates %n$@, which takes an object). Convert EVERY ` +
          `conversion in the value.`,
      );
    }
    // A lone backslash is the signature of a literal that was split mid-escape:
    // `delete \"\\(name)\"` keyed only as far as the escaped quote leaves the
    // value ending in `\`, and the remainder stranded as code at the call site.
    if (v.includes("\\")) {
      errs.push(`${k}: contains a backslash — a keyed value is plain text, so this is a literal truncated mid-escape`);
    }
  }
  return errs;
}

// ─── Translation ledger (#191) ─────────────────────────────────────────────────

/**
 * Which locale values are verified translations (#191). Exported for
 * scripts/translations.ts, the only writer; kept in this file so the gate's
 * self-test stays hermetic.
 *
 * Every locale file carries the full key set (CLAUDE.md §17), and until a key is
 * translated its value is the English. That makes "is this string translated?"
 * unanswerable from the locale files alone: a French value equal to the English
 * may be untranslated or may be a product name that is the same in French, and a
 * French value that differs may be a verified translation or somebody's guess.
 *
 * The ledger, `locales/_translations.json`, answers it. One entry per
 * (locale, key) that a native speaker has verified, holding two hashes:
 *
 *   source  hash of the en.json value the translation was made FROM
 *   value   hash of the locale value that was verified
 *
 * From those, every (locale, key) has exactly one state:
 *
 *   untranslated  no entry. The value must still equal the English — an
 *                 unverified translation cannot enter a locale file.
 *   translated    source matches en.json today; value matches the file.
 *   stale         the English has changed since. The translation stays in place
 *                 (#191: adding or editing strings never modifies a translated
 *                 one) and is re-issued to translators by the next export.
 *   tampered      the locale value no longer matches what was verified — it was
 *                 edited outside `scripts/translations.ts import`. A gate failure.
 *
 * `en` is the source locale and never has entries. A key deleted from en.json
 * must also leave the ledger, or the gate reports the orphan.
 *
 * Hash, not text: the ledger must not become a second copy of any string (the
 * §17.5 failure), and a hash says "this exact value" without saying it twice.
 */
export const SOURCE_LOCALE = "en";

export interface LedgerEntry {
  /** hash of the en.json value translated from */
  source: string;
  /** hash of the verified locale value */
  value: string;
  /** who verified it as a native speaker — an identifier the importer supplies */
  verifiedBy: string;
  /** ISO date (YYYY-MM-DD) of the import */
  verifiedOn: string;
}

/** locale → key → entry */
export type Ledger = Record<string, Record<string, LedgerEntry>>;

export type TranslationState = "untranslated" | "translated" | "stale" | "tampered";

export function hashText(s: string): string {
  return createHash("sha256").update(s, "utf8").digest("hex").slice(0, 16);
}

export function ledgerPath(localesDir: string): string {
  return join(localesDir, "_translations.json");
}

export function loadLedger(localesDir: string): Ledger {
  const p = ledgerPath(localesDir);
  if (!existsSync(p)) return {};
  return JSON.parse(readFileSync(p, "utf-8")) as Ledger;
}

/** Sorted at both levels, so a write is a pure function of content. */
export function saveLedger(localesDir: string, ledger: Ledger): void {
  const out: Ledger = {};
  for (const loc of Object.keys(ledger).sort()) {
    const entries = ledger[loc]!;
    if (Object.keys(entries).length === 0) continue;
    out[loc] = {};
    for (const k of Object.keys(entries).sort()) out[loc]![k] = entries[k]!;
  }
  writeFileSync(ledgerPath(localesDir), JSON.stringify(out, null, 2) + "\n");
}

/** The target locale codes: every locale file except the source and `_` files. */
export function targetLocales(localesDir: string): string[] {
  return readdirSync(localesDir)
    .filter((f) => f.endsWith(".json") && !f.startsWith("_"))
    .map((f) => basename(f, ".json"))
    .filter((c) => c !== SOURCE_LOCALE)
    .sort();
}

export function stateOf(
  source: string,
  value: string,
  entry: LedgerEntry | undefined,
): TranslationState {
  if (!entry) return "untranslated";
  if (entry.value !== hashText(value)) return "tampered";
  if (entry.source !== hashText(source)) return "stale";
  return "translated";
}

/** `{0}`, `{1}` … in first-appearance order, deduplicated and sorted. */
export function placeholders(s: string): string[] {
  return [...new Set(s.match(/\{\d+\}/g) ?? [])].sort();
}

// ─── Plural categories (OI-I18N-03) ───────────────────────────────────────────
//
// en.json's plural families carry English's categories: `_ONE` / `_OTHER`, and
// sometimes an explicit `_ZERO` (count === 0 exactly, in every language). CLDR
// gives other languages more — Russian one/few/many/other, Arabic
// zero/one/two/few/many/other — and a family that can only hold English's two
// forces one wrong sentence for 2, 5 or 21. A locale may therefore carry the
// extra CLDR categories of a family en.json defines, and nothing else extra.
// An extra category has no English of its own; it is translated FROM the
// family's `_OTHER`, which is what the source hash is taken of.

export const PLURAL_MEMBER = /_(ZERO|ONE|TWO|FEW|MANY|OTHER)$/;
const CATEGORY_ORDER = ["ZERO", "ONE", "TWO", "FEW", "MANY", "OTHER"];

/** The locale's CLDR plural categories, upper-case, in a fixed order. */
export function localePluralCategories(locale: string): string[] {
  const cats = new Intl.PluralRules(locale).resolvedOptions().pluralCategories.map((c) => c.toUpperCase());
  return CATEGORY_ORDER.filter((c) => cats.includes(c));
}

/** Bases of the plural families in canonical — those with an `_OTHER` member. */
export function pluralBases(canonical: Record<string, string>): string[] {
  return Object.keys(canonical)
    .filter((k) => k.endsWith("_OTHER"))
    .map((k) => k.slice(0, -"_OTHER".length))
    .sort();
}

/**
 * Every key a locale may carry, mapped to the English it is translated from:
 * en.json itself, plus each extra CLDR category → the family's `_OTHER`.
 */
export function sourcesFor(canonical: Record<string, string>, locale: string): Record<string, string> {
  const out: Record<string, string> = { ...canonical };
  if (locale === SOURCE_LOCALE) return out;
  for (const base of pluralBases(canonical)) {
    for (const cat of localePluralCategories(locale)) {
      const k = `${base}_${cat}`;
      if (!(k in out)) out[k] = canonical[`${base}_OTHER`]!;
    }
  }
  return out;
}

/**
 * An English plural member no selector ever reads in this locale: `_ONE` in
 * Chinese or Indonesian, whose only category is `other`. `_ZERO` is never
 * unused — every platform's explicit-zero rule reads it for count 0.
 */
export function isUnusedPluralMember(canonical: Record<string, string>, locale: string, key: string): boolean {
  const m = key.match(PLURAL_MEMBER);
  if (!m || !(key in canonical) || m[1] === "ZERO") return false;
  if (!(`${key.slice(0, -m[0].length)}_OTHER` in canonical)) return false;
  return !localePluralCategories(locale).includes(m[1]!);
}

/**
 * A few counts that select `category` in `locale`, for a translator. Whole
 * numbers first; a category no whole number reaches (Russian `other`) gets
 * fractional examples instead, so the translator sees why it exists.
 */
export function pluralExamples(locale: string, category: string): number[] {
  const rules = new Intl.PluralRules(locale);
  const out: number[] = [];
  for (let n = 0; n <= 1_000_000 && out.length < 6; n = n < 1000 ? n + 1 : n * 10) {
    if (rules.select(n).toUpperCase() === category) out.push(n);
  }
  for (let n = 0.5; n < 10 && out.length === 0; n++) {
    if (rules.select(n).toUpperCase() === category) out.push(n, n + 1);
  }
  return out;
}

/**
 * Does `category` select exactly one whole number in `locale`? Arabic zero,
 * one and two do (0, 1, 2), and so does English one; Russian one does not
 * (1, 21, 31 …), and nor does French many (1,000,000, 2,000,000 …).
 */
function selectsOneCount(locale: string, category: string): boolean {
  const rules = new Intl.PluralRules(locale);
  const probe = [...Array.from({ length: 2001 }, (_, n) => n), 10_000, 100_000, 1_000_000, 2_000_000, 10_000_000];
  return probe.filter((n) => rules.select(n).toUpperCase() === category).length === 1;
}

/**
 * Why a translation's placeholders do not fit its source, or null if they do.
 * Every `{n}` must survive — with one exception: a plural member whose category
 * selects exactly one count may drop the count `{0}`, because the word already
 * says it (Arabic dual "سطران", "two lines"). Where the category spans many
 * counts (Russian one: 1, 21, 31) the number must stay.
 */
export function placeholderMismatch(
  canonical: Record<string, string>,
  locale: string,
  key: string,
  source: string,
  translation: string,
): string | null {
  const want = placeholders(source);
  const got = placeholders(translation);
  if (want.join(",") === got.join(",")) return null;
  const m = key.match(PLURAL_MEMBER);
  const isFamily = m !== null && `${key.slice(0, -m[0].length)}_OTHER` in canonical;
  const onlyCountDropped =
    want.filter((p) => !got.includes(p)).join(",") === "{0}" && got.every((p) => want.includes(p));
  if (isFamily && onlyCountDropped && selectsOneCount(locale, m![1]!)) return null;
  return `placeholders [${got.join(",")}] do not match the English [${want.join(",")}]`;
}

/**
 * The ledger's own invariants, for the gate. Returns one message per violation.
 * Stale entries are NOT violations: an English copy edit must never be blocked
 * on a translator.
 */
export function checkLedger(
  canonical: Record<string, string>,
  locales: Record<string, Record<string, string>>,
  ledger: Ledger,
): string[] {
  const errs: string[] = [];
  if (ledger[SOURCE_LOCALE]) {
    errs.push(`_translations.json: "${SOURCE_LOCALE}" is the source locale and cannot carry entries`);
  }
  for (const loc of Object.keys(ledger)) {
    if (loc !== SOURCE_LOCALE && !locales[loc]) {
      errs.push(`_translations.json: entries for "${loc}", which has no locale file`);
    }
  }
  for (const [loc, values] of Object.entries(locales)) {
    const entries = ledger[loc] ?? {};
    const sources = sourcesFor(canonical, loc);
    for (const k of Object.keys(entries)) {
      if (!(k in sources)) {
        errs.push(`_translations.json: ${loc}/${k} — key no longer exists in en.json; remove the entry`);
      }
    }
    for (const [k, en] of Object.entries(sources)) {
      const v = values[k];
      if (v === undefined) continue; // parity is reported by its own check
      const st = stateOf(en, v, entries[k]);
      if (st === "untranslated" && !(k in canonical)) {
        // An extra plural category has no English to hold while untranslated,
        // so it exists only as a verified translation.
        errs.push(
          `locales/${loc}.json: ${k} is a plural category English does not have, with no verified ` +
            `translation on record — it enters only through \`bun scripts/translations.ts import\``,
        );
      } else if (st === "untranslated" && v !== en) {
        errs.push(
          `locales/${loc}.json: ${k} differs from en.json but has no verified translation on record. ` +
            `If the English was edited, run \`bun scripts/translations.ts fill\`; a translation enters ` +
            `only through \`bun scripts/translations.ts import\` (#191)`,
        );
      } else if (st === "tampered") {
        errs.push(
          `locales/${loc}.json: ${k} is a verified translation that has been edited since it was ` +
            `verified — restore it, or re-import the corrected value with its reviewer`,
        );
      }
      // Placeholder parity on a current translation. A translation that drops
      // {1} silently loses an argument on every platform. Not on a stale one:
      // the English may have gained a placeholder the translator has not seen.
      if (st === "translated") {
        const why = placeholderMismatch(canonical, loc, k, en, v);
        if (why) errs.push(`locales/${loc}.json: ${k} — ${why}`);
      }
    }
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
    ['            OptionalDoubleField(label: "Max Intensity", unit: "%", value: b)', true],
    ['            OptionalDoubleField(label: String(localized: "LIMITS_MAX_INTENSITY"), unit: "%", value: b)', false],
    ['            Image(systemName: "waveform.path.ecg")', false],   // SF Symbol, not text
    ['            InfoRow(title: "HRV Biofeedback", detail: "Coherence training.")', true],
  ];
  for (const [src, shouldFlag] of swiftCases) {
    const flagged = scanSource("x.swift", src).length > 0;
    if (flagged !== shouldFlag) {
      console.error(`self-test FAILED: ${JSON.stringify(src)} -> flagged=${flagged}, expected ${shouldFlag}`);
      bad++;
    }
  }
  const kotlinCases: Array<[string, boolean]> = [
    ['            Text("Save Changes")', true],
    ['            Text(stringResource(R.string.web_save_changes))', false],
    ['            OutlinedButton(onClick = {}) { Text("Add Helmet") }', true],
    // Kotlin's bare $identifier form: the only prose here is the variable name.
    ['            Text("+ $protocolName")', false],
    ['            Text("${count}%")', false],
  ];
  for (const [src, shouldFlag] of kotlinCases) {
    const flagged = scanSource("x.kt", src).length > 0;
    if (flagged !== shouldFlag) {
      console.error(`self-test FAILED: ${JSON.stringify(src)} -> flagged=${flagged}, expected ${shouldFlag}`);
      bad++;
    }
  }
  // checkInterpolationSyntax reads canonical values rather than source lines, so
  // it needs its own fixture. Without these the `@` rule could be inverted and
  // every case above would still pass.
  const canonicalCases: Array<[string, boolean]> = [
    // [canonical value, should it be flagged]
    ["Coherence {0}", false],
    ["Coherence %@", true],                    // OI-I18N-01: the un-positional spelling
    ["%1$@, %2$@ module connected", true],     // and the positional one — same crash
    ["Socket {0} — {1}", false],
    ["Electrode %d", true],                    // OI-I18N-02: valid on both platforms, but not reorderable
    ["%1$d of %2$d sockets populated", true],  // positional printf is still printf
    ["Coherence %.1f", true],                  // precision is formatted at the call site, not here
    ["%d:%02d into session", true],            // flags and width
    ["{0}% exceeds limit", false],             // literal percent followed by a space
    ["±{0} %MT", false],                       // unit symbol, not a conversion
    ["Duty cycle must be ≤ {0}%", false],      // literal percent, escaped by the generator
    ["Order {0}", false],
    ["delete \\(name)", true],                 // pre-existing rule: Swift interpolation
    ["${name} is not defined", true],          // pre-existing rule: template interpolation
    ["A zone named “{0}” already exists.", false],
  ];
  for (const [value, shouldFlag] of canonicalCases) {
    const flagged = checkInterpolationSyntax({ SELF_TEST_KEY: value }).length > 0;
    if (flagged !== shouldFlag) {
      console.error(`self-test FAILED: ${JSON.stringify(value)} -> flagged=${flagged}, expected ${shouldFlag}`);
      bad++;
    }
  }
  // checkLedger: every state the ledger distinguishes, driven directly. Without
  // these, a checkLedger that returned [] would pass every run on today's tree,
  // where nothing is translated yet.
  const en = { K: "Socket {0}" };
  const entry = (source: string, value: string) =>
    ({ source: hashText(source), value: hashText(value), verifiedBy: "t", verifiedOn: "2026-01-01" });
  const ledgerCases: Array<[string, Record<string, string>, Record<string, ReturnType<typeof entry>>, boolean]> = [
    ["untranslated, still English", { K: "Socket {0}" }, {}, false],
    ["unverified translation", { K: "Prise {0}" }, {}, true],
    ["verified translation", { K: "Prise {0}" }, { K: entry("Socket {0}", "Prise {0}") }, false],
    ["verified, identical to English", { K: "Socket {0}" }, { K: entry("Socket {0}", "Socket {0}") }, false],
    ["stale: English moved on", { K: "Prise {0}" }, { K: entry("Old socket {0}", "Prise {0}") }, false],
    ["tampered: edited after verification", { K: "Prise n° {0}" }, { K: entry("Socket {0}", "Prise {0}") }, true],
    ["verified but dropped a placeholder", { K: "Prise" }, { K: entry("Socket {0}", "Prise") }, true],
    ["entry for a deleted key", { K: "Socket {0}" }, { GONE: entry("x", "y") }, true],
  ];
  for (const [name, fr, entries, shouldFlag] of ledgerCases) {
    const flagged = checkLedger(en, { fr }, { fr: entries }).length > 0;
    if (flagged !== shouldFlag) {
      console.error(`self-test FAILED: ledger case "${name}" -> flagged=${flagged}, expected ${shouldFlag}`);
      bad++;
    }
  }
  // Parity with plural categories (OI-I18N-03), driven directly with a fixture.
  const pluralEn = { N_ONE: "{0} file", N_OTHER: "{0} files", PLAIN: "Save" };
  const parityCases: Array<[string, string, Record<string, string>, boolean]> = [
    ["ru carries en's key set", "ru", { ...pluralEn }, false],
    ["ru adds its own FEW and MANY", "ru", { ...pluralEn, N_FEW: "{0} файла", N_MANY: "{0} файлов" }, false],
    ["ru adds TWO, which Russian does not have", "ru", { ...pluralEn, N_TWO: "{0}" }, true],
    ["ar adds ZERO and TWO", "ar", { ...pluralEn, N_ZERO: "{0}", N_TWO: "{0}" }, false],
    ["fr adds FEW, which French does not have", "fr", { ...pluralEn, N_FEW: "{0}" }, true],
    ["a category on a family en.json lacks", "ru", { ...pluralEn, M_FEW: "{0}" }, true],
    ["a plain extra key", "ru", { ...pluralEn, EXTRA: "x" }, true],
    ["a missing en key", "ru", { N_ONE: "{0}", N_OTHER: "{0}" }, true],
  ];
  for (const [name, loc, values, shouldFlag] of parityCases) {
    const flagged = localeParityErrors(pluralEn, loc, values).length > 0;
    if (flagged !== shouldFlag) {
      console.error(`self-test FAILED: parity case "${name}" -> flagged=${flagged}, expected ${shouldFlag}`);
      bad++;
    }
  }
  // An extra category is translated FROM _OTHER, and exists only verified.
  const ruExtra = { ...pluralEn, N_FEW: "{0} файла" };
  const pluralLedgerCases: Array<[string, Record<string, ReturnType<typeof entry>>, boolean]> = [
    ["extra category with no ledger entry", {}, true],
    ["extra category verified against _OTHER", { N_FEW: entry("{0} files", "{0} файла") }, false],
    ["extra category verified against _ONE (wrong source) reads stale, not failed", { N_FEW: entry("{0} file", "{0} файла") }, false],
  ];
  for (const [name, entries, shouldFlag] of pluralLedgerCases) {
    const flagged = checkLedger(pluralEn, { ru: ruExtra }, { ru: entries }).length > 0;
    if (flagged !== shouldFlag) {
      console.error(`self-test FAILED: plural ledger case "${name}" -> flagged=${flagged}, expected ${shouldFlag}`);
      bad++;
    }
  }
  const placeholderCases: Array<[string, string, string, string, boolean]> = [
    // [locale, key, translation, name, should it be flagged]
    ["ar", "N_TWO", "ملفان", "Arabic dual drops the count", false],
    ["ar", "N_ZERO", "لا ملفات", "Arabic zero drops the count", false],
    ["ru", "N_ONE", "файл", "Russian one drops the count — 21 would read as 1", true],
    ["fr", "N_MANY", "de fichiers", "French many spans 1e6, 2e6 …", true],
    ["ru", "PLAIN", "", "not a plural member", true],
    ["ru", "N_FEW", "{0} {1} файла", "an invented placeholder", true],
  ];
  const phEn = { ...pluralEn, PLAIN: "Save {0}" };
  for (const [loc, key, tr, name, shouldFlag] of placeholderCases) {
    const flagged = placeholderMismatch(phEn, loc, key, phEn[key as keyof typeof phEn] ?? phEn.N_OTHER, tr) !== null;
    if (flagged !== shouldFlag) {
      console.error(`self-test FAILED: placeholder case "${name}" -> flagged=${flagged}, expected ${shouldFlag}`);
      bad++;
    }
  }
  const unusedCases: Array<[string, string, boolean]> = [
    ["zh-Hans", "N_ONE", true],   // Chinese has only `other`
    ["ru", "N_ONE", false],
    ["zh-Hans", "N_OTHER", false],
    ["zh-Hans", "PLAIN", false],
  ];
  for (const [loc, key, want] of unusedCases) {
    if (isUnusedPluralMember(pluralEn, loc, key) !== want) {
      console.error(`self-test FAILED: isUnusedPluralMember(${loc}, ${key}) !== ${want}`);
      bad++;
    }
  }
  if (bad > 0) { console.error(`\n${bad} self-test case(s) failed.`); process.exit(1); }
  console.log(
    `check-locale-strings self-test: ` +
      `${cases.length + swiftCases.length + kotlinCases.length + canonicalCases.length + ledgerCases.length +
        parityCases.length + pluralLedgerCases.length + placeholderCases.length + unusedCases.length} cases, all correct.`,
  );
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
  const parity = checkLocaleParity(canonical);
  const interp = checkInterpolationSyntax(canonical);
  const locales: Record<string, Record<string, string>> = {};
  for (const loc of targetLocales(LOCALES_DIR)) {
    locales[loc] = JSON.parse(readFileSync(join(LOCALES_DIR, `${loc}.json`), "utf-8"));
  }
  const ledger = checkLedger(canonical, locales, loadLedger(LOCALES_DIR));

  let failed = 0;

  if (interp.length) {
    console.error("\nRAW INTERPOLATION in a canonical value (CLAUDE.md §17):");
    for (const e of interp) console.error(`  ${e}`);
    failed += interp.length;
  }

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

  if (ledger.length) {
    console.error("\nTRANSLATION LEDGER (#191 — see scripts/translations.ts):");
    for (const e of ledger.slice(0, 40)) console.error(`  ${e}`);
    if (ledger.length > 40) console.error(`  ... and ${ledger.length - 40} more`);
    failed += ledger.length;
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

// Imported by scripts/translations.ts for the ledger; run only when invoked.
if (import.meta.main) main();
