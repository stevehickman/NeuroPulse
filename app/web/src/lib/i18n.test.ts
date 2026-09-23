import { describe, it, expect } from "vitest";
import { readdirSync, readFileSync } from "fs";
import { join } from "path";

/**
 * Canonical, not the generated copy under src/generated/locales.
 *
 * Every assertion below — parses, key count, key parity across locales — is a
 * property of the source of truth. Pointed at the generated tree they would
 * only restate what sync-locales.ts already guarantees by construction, and
 * would pass just as happily against a stale copy left behind by an older
 * build. locales/*.json is the file a person edits, so it is the file to test.
 */
const LOCALES_DIR = join(__dirname, "..", "..", "..", "..", "locales");

describe("i18n locale files", () => {
  it("all locale JSON files parse without error", () => {
    const files = readdirSync(LOCALES_DIR).filter(
      (f) => f.endsWith(".json") && !f.startsWith("_"),
    );
    expect(files.length).toBeGreaterThanOrEqual(11);
    for (const file of files) {
      const raw = readFileSync(join(LOCALES_DIR, file), "utf-8");
      expect(() => JSON.parse(raw)).not.toThrow();
    }
  });

  it("en.json contains at least 50 keys", () => {
    const en = JSON.parse(
      readFileSync(join(LOCALES_DIR, "en.json"), "utf-8"),
    );
    expect(Object.keys(en).length).toBeGreaterThanOrEqual(50);
  });

  // Every locale carries every en.json key. The only extra keys allowed are
  // the locale's own CLDR plural categories on a family en.json defines
  // (OI-I18N-03) — Russian _FEW / _MANY, Arabic _ZERO / _TWO / _FEW / _MANY.
  it("all locale files carry en.json's keys, plus only their own plural categories", () => {
    const en = JSON.parse(
      readFileSync(join(LOCALES_DIR, "en.json"), "utf-8"),
    );
    const enKeys = Object.keys(en).sort();
    const bases = enKeys.filter((k) => k.endsWith("_OTHER")).map((k) => k.slice(0, -6));
    const files = readdirSync(LOCALES_DIR).filter(
      (f) => f.endsWith(".json") && !f.startsWith("_") && f !== "en.json",
    );
    for (const file of files) {
      const code = file.slice(0, -5);
      const loc = JSON.parse(
        readFileSync(join(LOCALES_DIR, file), "utf-8"),
      );
      const cats = new Intl.PluralRules(code)
        .resolvedOptions()
        .pluralCategories.map((c) => c.toUpperCase());
      const allowedExtra = new Set(bases.flatMap((b) => cats.map((c) => `${b}_${c}`)));
      const locKeys = Object.keys(loc);
      for (const k of enKeys) expect(locKeys, `${file} missing ${k}`).toContain(k);
      const extras = locKeys.filter((k) => !(k in en));
      for (const k of extras) expect(allowedExtra.has(k), `${file} extra ${k}`).toBe(true);
    }
  });
});

describe("t() function", () => {
  it("returns key when translation is missing", async () => {
    const { t } = await import("./i18n");
    expect(t("NONEXISTENT_KEY_12345")).toBe("NONEXISTENT_KEY_12345");
  });

  it("supports interpolation with named params", async () => {
    const { t } = await import("./i18n");
    const result = t("TEST_WITH_PARAM", { "0": "hello" });
    expect(result).not.toContain("{0}");
  });
});

describe("pluralKey()", () => {
  const family = (...cats: string[]) => {
    const keys = new Set(cats.map((c) => `N_${c}`));
    return (k: string) => keys.has(k);
  };

  it("English: one and other", async () => {
    const { pluralKey } = await import("./i18n");
    const has = family("ONE", "OTHER");
    expect(pluralKey("N", 1, "en", has)).toBe("N_ONE");
    expect(pluralKey("N", 2, "en", has)).toBe("N_OTHER");
    expect(pluralKey("N", 0, "en", has)).toBe("N_OTHER");
  });

  it("an explicit _ZERO wins for 0 in every locale", async () => {
    const { pluralKey } = await import("./i18n");
    const has = family("ZERO", "ONE", "OTHER");
    expect(pluralKey("N", 0, "en", has)).toBe("N_ZERO");
    expect(pluralKey("N", 0, "ru", has)).toBe("N_ZERO");
  });

  it("Russian selects few and many once the locale carries them", async () => {
    const { pluralKey } = await import("./i18n");
    const has = family("ONE", "FEW", "MANY", "OTHER");
    expect(pluralKey("N", 1, "ru", has)).toBe("N_ONE");
    expect(pluralKey("N", 21, "ru", has)).toBe("N_ONE");
    expect(pluralKey("N", 3, "ru", has)).toBe("N_FEW");
    expect(pluralKey("N", 22, "ru", has)).toBe("N_FEW");
    expect(pluralKey("N", 5, "ru", has)).toBe("N_MANY");
    expect(pluralKey("N", 11, "ru", has)).toBe("N_MANY");
  });

  it("a category the locale does not carry yet falls back to _OTHER, never the raw key", async () => {
    const { pluralKey } = await import("./i18n");
    const has = family("ONE", "OTHER");
    expect(pluralKey("N", 3, "ru", has)).toBe("N_OTHER");
    expect(pluralKey("N", 2, "ar", has)).toBe("N_OTHER");
  });

  it("Arabic two; Chinese never selects _ONE", async () => {
    const { pluralKey } = await import("./i18n");
    expect(pluralKey("N", 2, "ar", family("ONE", "TWO", "OTHER"))).toBe("N_TWO");
    expect(pluralKey("N", 1, "zh-Hans", family("ONE", "OTHER"))).toBe("N_OTHER");
  });

  it("tPlural renders the count through the selected member", async () => {
    const { tPlural } = await import("./i18n");
    expect(tPlural("SCRIPT_LINE_COUNT", 1)).toBe("1 line");
    expect(tPlural("SCRIPT_LINE_COUNT", 3)).toBe("3 lines");
  });
});

describe("supportedLocales", () => {
  it("exports exactly 11 locales", async () => {
    const { supportedLocales } = await import(
      "../locales/supportedLocales"
    );
    expect(supportedLocales).toHaveLength(11);
  });

  it("every locale has non-empty bcp47 and displayName", async () => {
    const { supportedLocales } = await import(
      "../locales/supportedLocales"
    );
    for (const loc of supportedLocales) {
      expect(loc.bcp47.length).toBeGreaterThan(0);
      expect(loc.displayName.length).toBeGreaterThan(0);
      expect(["ltr", "rtl"]).toContain(loc.direction);
    }
  });

  it("Arabic has direction rtl", async () => {
    const { supportedLocales } = await import(
      "../locales/supportedLocales"
    );
    const ar = supportedLocales.find((l) => l.bcp47 === "ar");
    expect(ar).toBeDefined();
    expect(ar!.direction).toBe("rtl");
  });

  it("findLocale falls back to en for unknown locale", async () => {
    const { findLocale } = await import("../locales/supportedLocales");
    const result = findLocale("xx-YY");
    expect(result.bcp47).toBe("en");
  });

  it("findLocale matches es-MX to es-419", async () => {
    const { findLocale } = await import("../locales/supportedLocales");
    const result = findLocale("es-MX");
    expect(result.bcp47).toBe("es-419");
  });
});
