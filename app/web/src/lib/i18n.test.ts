import { describe, it, expect } from "vitest";
import { readdirSync, readFileSync } from "fs";
import { join } from "path";

const LOCALES_DIR = join(__dirname, "../locales");

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

  it("all locale files have the same keys as en.json", () => {
    const en = JSON.parse(
      readFileSync(join(LOCALES_DIR, "en.json"), "utf-8"),
    );
    const enKeys = Object.keys(en).sort();
    const files = readdirSync(LOCALES_DIR).filter(
      (f) => f.endsWith(".json") && !f.startsWith("_") && f !== "en.json",
    );
    for (const file of files) {
      const loc = JSON.parse(
        readFileSync(join(LOCALES_DIR, file), "utf-8"),
      );
      const locKeys = Object.keys(loc).sort();
      expect(locKeys).toEqual(enKeys);
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
