#!/usr/bin/env bun
/**
 * sync-locales.ts — Canonical JSON → platform-native locale file generator.
 *
 * Source of truth: locales/*.json (flat key-value per BCP 47 locale)
 * Targets:
 *   - Apple: app/ios/NeuroPulse/Localizable.xcstrings (String Catalog)
 *   - Web:   app/web/src/locales/*.json (copy)
 *
 * Future extension point: generateAndroidXml(locales, outputDir)
 */

import { readFileSync, writeFileSync, readdirSync, mkdirSync, existsSync, copyFileSync } from "fs";
import { join, basename } from "path";

const LOCALES_DIR = join(import.meta.dir, "..", "locales");
const XCSTRINGS_OUT = join(import.meta.dir, "..", "app", "ios", "NeuroPulse", "Localizable.xcstrings");
const WEB_LOCALES_OUT = join(import.meta.dir, "..", "app", "web", "src", "locales");

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

function generateXCStrings(locales: Map<string, LocaleData>): XCStringsFile {
  const en = locales.get("en")!;
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
            variations[suffix] = {
              stringUnit: {
                state: locale === "en" ? "new" : "needs_review",
                value: canonicalToApple(data[suffixKey]),
              },
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
    for (const [locale, data] of locales) {
      if (data[key] !== undefined) {
        localizations[locale] = {
          stringUnit: {
            state: locale === "en" ? "new" : "needs_review",
            value: canonicalToApple(data[key]),
          },
        };
      }
    }
    strings[key] = { extractionState: "manual", localizations };
  }

  return { sourceLanguage: "en", strings, version: "1.0" };
}

function canonicalToApple(value: string): string {
  return value.replace(/\{(\d+)\}/g, "%$1\\$@");
}

// --- Web locale copy ---

function copyToWeb(locales: Map<string, LocaleData>): void {
  if (!existsSync(WEB_LOCALES_OUT)) mkdirSync(WEB_LOCALES_OUT, { recursive: true });
  for (const [locale, data] of locales) {
    const outPath = join(WEB_LOCALES_OUT, `${locale}.json`);
    writeFileSync(outPath, JSON.stringify(data, null, 2) + "\n");
  }
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
  console.log("sync-locales: reading canonical locale files...");
  const locales = loadLocales();
  validateLocales(locales);

  console.log(`sync-locales: ${locales.size} locales, ${Object.keys(locales.get("en")!).length} keys`);

  const xcstrings = generateXCStrings(locales);
  writeFileSync(XCSTRINGS_OUT, JSON.stringify(xcstrings, null, 2) + "\n");
  console.log(`sync-locales: wrote ${XCSTRINGS_OUT}`);

  copyToWeb(locales);
  console.log(`sync-locales: copied ${locales.size} locale files to ${WEB_LOCALES_OUT}`);

  console.log("sync-locales: done.");
}

main();
