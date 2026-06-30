import { findLocale, type LocaleInfo } from "../locales/supportedLocales";

type Translations = Record<string, string>;

const cache = new Map<string, Translations>();

function detectLocale(): LocaleInfo {
  const browserLang =
    typeof navigator !== "undefined" && navigator.language
      ? navigator.language
      : "en";
  return findLocale(browserLang);
}

async function loadTranslations(bcp47: string): Promise<Translations> {
  if (cache.has(bcp47)) return cache.get(bcp47)!;
  try {
    const mod = await import(`../locales/${bcp47}.json`);
    const translations: Translations = mod.default ?? mod;
    cache.set(bcp47, translations);
    return translations;
  } catch {
    if (bcp47 !== "en") return loadTranslations("en");
    return {};
  }
}

let currentLocale: LocaleInfo = detectLocale();
let currentTranslations: Translations = {};
let initPromise: Promise<void> | null = null;

export function initI18n(): Promise<void> {
  if (!initPromise) {
    initPromise = loadTranslations(currentLocale.bcp47).then((t) => {
      currentTranslations = t;
    });
  }
  return initPromise;
}

export function t(key: string, params?: Record<string, string | number>): string {
  let value = currentTranslations[key] ?? key;
  if (params) {
    for (const [k, v] of Object.entries(params)) {
      value = value.replace(`{${k}}`, String(v));
    }
  }
  return value;
}

export function tPlural(
  baseKey: string,
  count: number,
  params?: Record<string, string | number>,
): string {
  const suffix = count === 0 ? "_ZERO" : count === 1 ? "_ONE" : "_OTHER";
  const key = `${baseKey}${suffix}`;
  return t(key, { "0": count, ...params });
}

export function useTranslation() {
  const { useMemo } = require("react");
  const locale = useMemo(() => currentLocale, []);
  return { t, tPlural, locale };
}

export function getCurrentLocale(): LocaleInfo {
  return currentLocale;
}

export function setLocale(bcp47: string): Promise<void> {
  currentLocale = findLocale(bcp47);
  initPromise = null;
  return initI18n();
}
