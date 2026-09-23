#!/usr/bin/env bun
/**
 * translations.ts — hand strings to translators and take verified translations back (#191).
 *
 *   bun scripts/translations.ts status
 *   bun scripts/translations.ts fill
 *   bun scripts/translations.ts export <locale> [--out <file>]
 *   bun scripts/translations.ts import <file> --verified-by <reviewer> [--legal-reviewed] [--replace]
 *
 * ── The two rules #191 sets, and how each is held ────────────────────────────
 *
 * "Translated into the various languages and verified by a native speaker."
 * A value enters a locale file as a translation ONLY through `import`, which
 * requires `--verified-by` and records the verification in the ledger
 * (`locales/_translations.json`, see the ledger section of check-locale-strings.ts). The gate
 * (`check-locale-strings.ts`) rejects any locale value that differs from the
 * English without a ledger entry, so a translation pasted in by hand, or a
 * machine translation nobody reviewed, cannot land.
 *
 * "If new strings are added after translation, the already translated strings
 * should not be modified." `export` issues only untranslated and stale keys, and
 * `import` refuses to overwrite a current translation unless `--replace` is given
 * — a correction is a deliberate act, never a side effect of a new batch. An
 * English edit leaves the existing translation in place and marks it stale.
 *
 * ── What an export carries ───────────────────────────────────────────────────
 *
 * Per key: the English, its hash (so an import can tell the English moved
 * underneath the translator), the placeholders the translation must keep, the
 * previous translation when stale, and `legalReviewRequired` for the keys
 * `_metadata.json` marks — consent, age-gate and BIPA copy, which `import` takes
 * only with `--legal-reviewed`. Keys `_metadata.json` marks `"translate": false`
 * (modality names, CLAUDE.md §17 / localization.md §17.3) are not exported.
 *
 * `{0}`, `{1}` may be reordered freely: every generator emits them positionally
 * (§17.2). Plural members are exported as separate keys, each with its CLDR
 * category and counts that select it in this language — including the
 * categories English lacks (`_FEW` / `_MANY` for Russian), translated from the
 * family's `_OTHER` (OI-I18N-03). A member the language never selects (`_ONE`
 * in Chinese) is not exported; it keeps the English, which nothing reads.
 *
 * `fill` is the step after an English edit or a new key: it copies the English
 * into every locale where the key is untranslated, which is what §17.1's "add
 * to all eleven" means for a locale nobody has translated yet. It never touches
 * a translated or stale value.
 *
 * This is a tool, not a gate: nothing here runs in CI. The invariants it writes
 * are checked by check-locale-strings.ts.
 */
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "fs";
import { dirname, join, resolve } from "path";
import {
  PLURAL_MEMBER,
  SOURCE_LOCALE,
  hashText,
  isUnusedPluralMember,
  loadLedger,
  placeholderMismatch,
  placeholders,
  pluralExamples,
  saveLedger,
  sourcesFor,
  stateOf,
  targetLocales,
  type TranslationState,
} from "./check-locale-strings";

const ROOT = resolve(import.meta.dir, "..");
const LOCALES_DIR = join(ROOT, "locales");

interface KeyMeta { legal_review_required?: boolean; translate?: boolean }

interface ExportedString {
  source: string;
  sourceHash: string;
  status: "untranslated" | "stale";
  placeholders: string[];
  /** Plural members only: the CLDR category, and counts that select it here. */
  pluralCategory?: string;
  pluralExamples?: number[];
  previousTranslation?: string;
  legalReviewRequired?: true;
  translation: string;
}

interface ExportFile {
  locale: string;
  sourceLocale: string;
  exportedOn: string;
  strings: Record<string, ExportedString>;
}

function readJson<T>(p: string): T {
  return JSON.parse(readFileSync(p, "utf-8")) as T;
}

function loadLocale(code: string): Record<string, string> {
  return readJson(join(LOCALES_DIR, `${code}.json`));
}

function saveLocale(code: string, data: Record<string, string>): void {
  const sorted: Record<string, string> = {};
  for (const k of Object.keys(data).sort()) sorted[k] = data[k]!;
  writeFileSync(join(LOCALES_DIR, `${code}.json`), JSON.stringify(sorted, null, 2) + "\n");
}

function loadMeta(): Record<string, KeyMeta> {
  const p = join(LOCALES_DIR, "_metadata.json");
  return existsSync(p) ? readJson(p) : {};
}

/** An extra plural category inherits its family's `_OTHER` metadata. */
function metaOf(meta: Record<string, KeyMeta>, key: string): KeyMeta | undefined {
  const m = key.match(PLURAL_MEMBER);
  return meta[key] ?? (m ? meta[`${key.slice(0, -m[0].length)}_OTHER`] : undefined);
}

/**
 * The keys a translator is asked for in `loc`, mapped to the English each is
 * translated from: en.json plus the locale's extra CLDR plural categories
 * (OI-I18N-03), minus `"translate": false` and minus English plural members
 * this locale never reads (`_ONE` in Chinese).
 */
function translatable(en: Record<string, string>, loc: string, meta: Record<string, KeyMeta>): Record<string, string> {
  const out: Record<string, string> = {};
  for (const [k, src] of Object.entries(sourcesFor(en, loc))) {
    if (metaOf(meta, k)?.translate === false) continue;
    if (isUnusedPluralMember(en, loc, k)) continue;
    out[k] = src;
  }
  return out;
}

function today(): string {
  return new Date().toISOString().slice(0, 10);
}

function fail(msg: string): never {
  console.error(msg);
  process.exit(1);
}

function argValue(args: string[], flag: string): string | undefined {
  const i = args.indexOf(flag);
  return i >= 0 ? args[i + 1] : undefined;
}

function requireLocale(code: string | undefined): string {
  if (!code) fail("a locale code is required");
  if (code === SOURCE_LOCALE) fail(`"${SOURCE_LOCALE}" is the source locale; it is not translated`);
  if (!targetLocales(LOCALES_DIR).includes(code)) {
    fail(`no locale file locales/${code}.json (have: ${targetLocales(LOCALES_DIR).join(", ")})`);
  }
  return code;
}

// ─── status ───────────────────────────────────────────────────────────────────

function status(): void {
  const en = loadLocale(SOURCE_LOCALE);
  const meta = loadMeta();
  const ledger = loadLedger(LOCALES_DIR);
  // The key count differs per locale: plural categories are the language's own.
  console.log("locale      keys  translated  stale  untranslated  tampered");
  for (const loc of targetLocales(LOCALES_DIR)) {
    const values = loadLocale(loc);
    const src = translatable(en, loc, meta);
    const n: Record<TranslationState, number> = { untranslated: 0, translated: 0, stale: 0, tampered: 0 };
    for (const [k, text] of Object.entries(src)) n[stateOf(text, values[k] ?? "", ledger[loc]?.[k])]++;
    console.log(
      `${loc.padEnd(10)} ${String(Object.keys(src).length).padStart(5)}  ${String(n.translated).padStart(10)}  ` +
        `${String(n.stale).padStart(5)}  ${String(n.untranslated).padStart(12)}  ${String(n.tampered).padStart(8)}`,
    );
  }
}

// ─── fill ─────────────────────────────────────────────────────────────────────

function fill(): void {
  const en = loadLocale(SOURCE_LOCALE);
  const ledger = loadLedger(LOCALES_DIR);
  for (const loc of targetLocales(LOCALES_DIR)) {
    const values = loadLocale(loc);
    let n = 0;
    for (const k of Object.keys(en)) {
      if (ledger[loc]?.[k]) continue; // translated, stale or tampered: never overwritten
      if (values[k] !== en[k]) { values[k] = en[k]!; n++; }
    }
    // A key en.json no longer has goes — but not a plural category the locale
    // is allowed to add (OI-I18N-03), which never had English to begin with.
    const allowed = sourcesFor(en, loc);
    for (const k of Object.keys(values)) if (!(k in allowed)) { delete values[k]; n++; }
    if (n > 0) { saveLocale(loc, values); console.log(`${loc}: ${n} untranslated value(s) set to the English`); }
  }
}

// ─── export ───────────────────────────────────────────────────────────────────

function exportLocale(args: string[]): void {
  const loc = requireLocale(args[0]);
  const out = resolve(argValue(args, "--out") ?? join(ROOT, "translation-export", `${loc}.json`));
  const en = loadLocale(SOURCE_LOCALE);
  const values = loadLocale(loc);
  const meta = loadMeta();
  const entries = loadLedger(LOCALES_DIR)[loc] ?? {};

  const strings: Record<string, ExportedString> = {};
  let tampered = 0;
  const src = translatable(en, loc, meta);
  for (const k of Object.keys(src).sort()) {
    const text = src[k]!;
    const st = stateOf(text, values[k] ?? "", entries[k]);
    if (st === "translated") continue;
    if (st === "tampered") { tampered++; continue; }
    const cat = k.match(PLURAL_MEMBER)?.[1];
    const plural = cat !== undefined && `${k.slice(0, -cat.length - 1)}_OTHER` in en;
    strings[k] = {
      source: text,
      sourceHash: hashText(text),
      status: st,
      placeholders: placeholders(text),
      ...(plural ? { pluralCategory: cat!.toLowerCase(), pluralExamples: pluralExamples(loc, cat!) } : {}),
      ...(st === "stale" && values[k] !== undefined ? { previousTranslation: values[k]! } : {}),
      ...(metaOf(meta, k)?.legal_review_required ? { legalReviewRequired: true as const } : {}),
      translation: "",
    };
  }
  if (tampered > 0) {
    fail(`${loc}: ${tampered} verified translation(s) were edited outside import — run check-locale-strings.ts and resolve them first`);
  }
  const file: ExportFile = { locale: loc, sourceLocale: SOURCE_LOCALE, exportedOn: today(), strings };
  mkdirSync(dirname(out), { recursive: true });
  writeFileSync(out, JSON.stringify(file, null, 2) + "\n");
  const stale = Object.values(strings).filter((s) => s.status === "stale").length;
  console.log(`${loc}: exported ${Object.keys(strings).length} key(s) (${stale} stale) to ${out}`);
}

// ─── import ───────────────────────────────────────────────────────────────────

function importFile(args: string[]): void {
  const path = args[0];
  if (!path || !existsSync(path)) fail("import needs the path of a filled-in export file");
  const verifiedBy = argValue(args, "--verified-by")?.trim();
  if (!verifiedBy) {
    fail("--verified-by <reviewer> is required: #191 takes a translation only once a native speaker has verified it");
  }
  const legalReviewed = args.includes("--legal-reviewed");
  const replace = args.includes("--replace");

  const file = readJson<ExportFile>(path);
  const loc = requireLocale(file.locale);
  const en = loadLocale(SOURCE_LOCALE);
  const values = loadLocale(loc);
  const meta = loadMeta();
  const ledger = loadLedger(LOCALES_DIR);
  const entries = (ledger[loc] ??= {});
  const sources = sourcesFor(en, loc);

  const skipped: string[] = [];
  let imported = 0;
  for (const [k, s] of Object.entries(file.strings ?? {})) {
    const tr = s.translation ?? "";
    if (tr.trim() === "") continue; // not done yet — stays in the next export
    if (!(k in sources)) { skipped.push(`${k}: no longer exists in ${SOURCE_LOCALE}.json`); continue; }
    if (metaOf(meta, k)?.translate === false) { skipped.push(`${k}: marked "translate": false`); continue; }
    if (isUnusedPluralMember(en, loc, k)) { skipped.push(`${k}: a plural category ${loc} never selects`); continue; }
    const text = sources[k]!;
    if (hashText(text) !== s.sourceHash) {
      skipped.push(`${k}: the English changed after this file was exported — re-export and translate the current text`);
      continue;
    }
    const st = stateOf(text, values[k] ?? "", entries[k]);
    if (st === "tampered") { skipped.push(`${k}: the current value was edited outside import — resolve first`); continue; }
    if (st === "translated" && !replace) {
      skipped.push(`${k}: already translated — pass --replace to correct a verified translation deliberately`);
      continue;
    }
    if (metaOf(meta, k)?.legal_review_required && !legalReviewed) {
      skipped.push(`${k}: needs legal review in this language — pass --legal-reviewed once it has had it`);
      continue;
    }
    const why = placeholderMismatch(en, loc, k, text, tr);
    if (why) { skipped.push(`${k}: ${why}`); continue; }
    if (tr.includes("\\") || /%(?:\d+\$)?[-#+0,(]*\d*(?:\.\d+)?[@diouxXeEfgGcs]/.test(tr)) {
      skipped.push(`${k}: carries a backslash or printf conversion — use {0}/{1} only`);
      continue;
    }
    values[k] = tr;
    entries[k] = { source: hashText(text), value: hashText(tr), verifiedBy, verifiedOn: today() };
    imported++;
  }

  saveLocale(loc, values);
  saveLedger(LOCALES_DIR, ledger);
  console.log(`${loc}: imported ${imported} verified translation(s), verified by ${verifiedBy}`);
  if (skipped.length) {
    console.log(`\n${skipped.length} key(s) not imported:`);
    for (const s of skipped) console.log(`  ${s}`);
  }
}

// ─── main ─────────────────────────────────────────────────────────────────────

const [cmd, ...rest] = process.argv.slice(2);
switch (cmd) {
  case "status": status(); break;
  case "fill": fill(); break;
  case "export": exportLocale(rest); break;
  case "import": importFile(rest); break;
  default:
    fail(
      "usage:\n" +
        "  bun scripts/translations.ts status\n" +
        "  bun scripts/translations.ts fill\n" +
        "  bun scripts/translations.ts export <locale> [--out <file>]\n" +
        "  bun scripts/translations.ts import <file> --verified-by <reviewer> [--legal-reviewed] [--replace]",
    );
}
