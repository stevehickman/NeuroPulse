#!/usr/bin/env bun
/**
 * sync-locales.ts — Canonical JSON → platform-native locale file generator.
 *
 * Source of truth: locales/*.json (flat key-value per BCP 47 locale)
 * Targets:
 *   - Apple: app/ios/NeurOne/Localizable.xcstrings (String Catalog)
 *   - Web:   app/web/src/locales/*.json (copy)
 *
 * Pass --check to fail without writing when a generated file is stale (CI).
 *
 * ── Translation state is DERIVED, not stored ────────────────────────────────
 *
 * A String Catalog records a `state` per string per locale. Canonical
 * locales/*.json is a flat key->string map with nowhere to put that, so an
 * earlier version of this generator emitted a constant ("new" for en,
 * "needs_review" otherwise) and regeneration overwrote whatever Xcode had
 * written — every en entry flipped translated -> new on every run.
 *
 * The fix is not to give canonical somewhere to store state, nor to merge state
 * back out of the generated catalogue. Both make the .xcstrings a second source
 * of truth for something, which is precisely how this file drifted: the
 * NP-HFE-002 rewording of SETUP_INSTR_ZONE_MODULES / SETUP_ERROR_ZONE_MISSING
 * was done in the generated catalogue and never back-ported, so regenerating
 * would have reverted two live strings to describing retired hardware.
 *
 * Instead the state is a pure function of the canonical content:
 *
 *   - source language (en), value present      -> "translated"
 *   - other locale, value differs from en      -> "translated"
 *   - other locale, value identical to en      -> "needs_review"
 *
 * A non-source locale still carrying the English source text has, by
 * definition, not been translated yet; one carrying something else has. This
 * keeps canonical the single source of truth, keeps the generator
 * deterministic and idempotent (so --check is a plain equality test), and lets
 * a locale flip to "translated" the moment real translations land in its JSON
 * file — no second store to keep in sync.
 *
 * Known limitation: a translation that is legitimately identical to the English
 * source stays at "needs_review". This mostly affects en-GB (where many strings
 * are correctly identical) and short proper nouns such as "EEG". The state
 * field is advisory metadata for translators and drives no runtime behaviour,
 * so the failure mode is a benign prompt to re-check, never a wrong string.
 *
 * Future extension point: generateAndroidXml(locales, outputDir)
 */

import { readFileSync, writeFileSync, readdirSync, mkdirSync, existsSync } from "fs";
import { join, basename } from "path";

const LOCALES_DIR = join(import.meta.dir, "..", "locales");
const XCSTRINGS_OUT = join(import.meta.dir, "..", "app", "ios", "NeurOne", "Localizable.xcstrings");
const WEB_LOCALES_OUT = join(import.meta.dir, "..", "app", "web", "src", "locales");

const SOURCE_LANGUAGE = "en";

const REQUIRED_LOCALES = [
  "en", "en-GB", "es-419", "ca", "zh-Hans", "ar", "fr", "hi", "bn", "id", "ru",
];

interface LocaleData {
  [key: string]: string;
}

function loadLocales(): Map<string, LocaleData> {
  const locales = new Map<string, LocaleData>();
  for (const file of readdirSync(LOCALES_DIR)) {
    if (!file.endsWith(".json") || file.startsWith("_")) continue;
    const locale = basename(file, ".json");
    const data: LocaleData = JSON.parse(readFileSync(join(LOCALES_DIR, file), "utf-8"));
    locales.set(locale, data);
  }
  return locales;
}

function validateLocales(locales: Map<string, LocaleData>): void {
  const missing = REQUIRED_LOCALES.filter((l) => !locales.has(l));
  if (missing.length > 0) {
    console.error(`Missing required locale files: ${missing.join(", ")}`);
    process.exit(1);
  }

  const enKeys = Object.keys(locales.get("en")!);
  for (const [locale, data] of locales) {
    if (locale === "en") continue;
    const localeKeys = Object.keys(data);
    const missingKeys = enKeys.filter((k) => !localeKeys.includes(k));
    if (missingKeys.length > 0) {
      console.warn(`${locale}: ${missingKeys.length} keys missing from en.json`);
    }
  }
}

// --- Apple .xcstrings generator ---

interface XCStringUnit {
  state: string;
  value: string;
}

interface XCLocalization {
  stringUnit: XCStringUnit;
}

interface XCStringEntry {
  extractionState?: string;
  localizations: Record<string, XCLocalization>;
}

interface XCStringsFile {
  sourceLanguage: string;
  strings: Record<string, XCStringEntry>;
  version: string;
}

function isPluralSuffix(key: string): boolean {
  return /_(?:ZERO|ONE|TWO|FEW|MANY|OTHER)$/.test(key);
}

/**
 * Translation state for one string in one locale. See the header for why this
 * is derived from the values rather than stored. `sourceValue` is the en text
 * after the same Apple transform has been applied, so the comparison is between
 * like and like.
 */
function deriveState(locale: string, value: string, sourceValue: string | undefined): string {
  if (locale === SOURCE_LANGUAGE) return "translated";
  return value === sourceValue ? "needs_review" : "translated";
}

function generateXCStrings(locales: Map<string, LocaleData>): XCStringsFile {
  const en = locales.get(SOURCE_LANGUAGE)!;
  const strings: Record<string, XCStringEntry> = {};

  const processedPluralBases = new Set<string>();

  for (const key of Object.keys(en).sort()) {
    if (isPluralSuffix(key)) {
      const base = key.replace(/_(?:ZERO|ONE|TWO|FEW|MANY|OTHER)$/, "");
      if (processedPluralBases.has(base)) continue;
      processedPluralBases.add(base);

      const localizations: Record<string, any> = {};
      for (const [locale, data] of locales) {
        const variations: Record<string, { stringUnit: XCStringUnit }> = {};
        for (const suffix of ["zero", "one", "two", "few", "many", "other"]) {
          const suffixKey = `${base}_${suffix.toUpperCase()}`;
          if (data[suffixKey]) {
            // Compared per plural category: a locale counts as translated for a
            // category only if that category's text differs from the English.
            const value = canonicalToApplePlural(data[suffixKey]);
            const sourceValue =
              en[suffixKey] === undefined ? undefined : canonicalToApplePlural(en[suffixKey]);
            variations[suffix] = {
              stringUnit: { state: deriveState(locale, value, sourceValue), value },
            };
          }
        }
        if (Object.keys(variations).length > 0) {
          localizations[locale] = {
            variations: { plural: variations },
          };
        }
      }
      strings[base] = {
        extractionState: "manual",
        localizations,
      };
      continue;
    }

    const localizations: Record<string, XCLocalization> = {};
    const sourceValue = canonicalToApple(en[key]);
    for (const [locale, data] of locales) {
      if (data[key] !== undefined) {
        const value = canonicalToApple(data[key]);
        localizations[locale] = {
          stringUnit: { state: deriveState(locale, value, sourceValue), value },
        };
      }
    }
    strings[key] = { extractionState: "manual", localizations };
  }

  return { sourceLanguage: SOURCE_LANGUAGE, strings, version: "1.0" };
}

function canonicalToApple(value: string): string {
  return value.replace(/\{(\d+)\}/g, "%$1\\$@");
}

function canonicalToApplePlural(value: string): string {
  return value.replace(/\{0\}/, "%lld").replace(/\{(\d+)\}/g, "%$1\\$@");
}

// --- Web locale copy ---

function webOutputs(locales: Map<string, LocaleData>): Array<[string, string]> {
  return [...locales].map(([locale, data]): [string, string] => [
    join(WEB_LOCALES_OUT, `${locale}.json`),
    JSON.stringify(data, null, 2) + "\n",
  ]);
}

// --- Future: Android XML generator (extension point) ---
// function generateAndroidXml(locales: Map<string, LocaleData>, outputDir: string): void {
//   for (const [locale, data] of locales) {
//     const androidLocale = locale.replace("-", "-r");
//     const dir = locale === "en" ? "values" : `values-${androidLocale}`;
//     // ... generate <resources><string name="key">value</string></resources>
//   }
// }

// --- Main ---

function main(): void {
  const checkOnly = process.argv.includes("--check");

  const locales = loadLocales();
  validateLocales(locales);

  const keyCount = Object.keys(locales.get(SOURCE_LANGUAGE)!).length;

  const outputs: Array<[string, string]> = [
    [XCSTRINGS_OUT, JSON.stringify(generateXCStrings(locales), null, 2) + "\n"],
    ...webOutputs(locales),
  ];

  if (!checkOnly && !existsSync(WEB_LOCALES_OUT)) {
    mkdirSync(WEB_LOCALES_OUT, { recursive: true });
  }

  let stale = 0;
  for (const [path, content] of outputs) {
    let existing: string | null = null;
    try {
      existing = readFileSync(path, "utf-8");
    } catch {
      existing = null;
    }

    if (existing === content) continue;

    if (checkOnly) {
      console.error(`STALE: ${path}`);
      stale++;
    } else {
      writeFileSync(path, content, "utf-8");
      console.log(`wrote ${path}`);
    }
  }

  if (checkOnly) {
    if (stale > 0) {
      console.error(`\n${stale} generated file(s) out of date — run: bun scripts/sync-locales.ts`);
      process.exit(1);
    }
    console.log(`${locales.size} locales, ${keyCount} keys — all generated files up to date.`);
    return;
  }

  console.log(`sync-locales: ${locales.size} locales, ${keyCount} keys — done.`);
}

main();
