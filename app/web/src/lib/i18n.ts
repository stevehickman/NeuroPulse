import { findLocale, type LocaleInfo } from "../locales/supportedLocales";
import enTranslations from "../locales/en.json";

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

/**
 * English is bundled and used as the standing fallback rather than starting
 * from {}. Two things broke without it: every t() call made before the async
 * initI18n() resolves rendered the raw key ("WEB_SAVE_CHANGES") for a frame,
 * and any module that formats text outside a React render — protocolValidator,
 * protocolEligibility — returned raw keys in contexts that never call
 * initI18n() at all, unit tests among them. It also means a locale that is
 * missing a key falls back to readable English instead of the key itself.
 */
const FALLBACK: Translations = enTranslations as Translations;

let currentLocale: LocaleInfo = detectLocale();
let currentTranslations: Translations = FALLBACK;
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
  let value = currentTranslations[key] ?? FALLBACK[key] ?? key;
  if (params) {
    for (const [k, v] of Object.entries(params)) {
      value = value.replace(`{${k}}`, String(v));
    }
  }
  return value;
}

/**
 * `_ZERO` is an optional extra category, not a CLDR plural category for
 * English — most keys only define `_ONE` and `_OTHER`. Selecting `_ZERO` and
 * stopping there would render the literal key ("..._ZERO") on screen for every
 * count of 0, so an absent category falls back to `_OTHER` before t() gets a
 * chance to echo the key back.
 */
export function tPlural(
  baseKey: string,
  count: number,
  params?: Record<string, string | number>,
): string {
  const suffix = count === 0 ? "_ZERO" : count === 1 ? "_ONE" : "_OTHER";
  const key = `${baseKey}${suffix}`;
  const resolved =
    key in currentTranslations || key in FALLBACK ? key : `${baseKey}_OTHER`;
  return t(resolved, { "0": count, ...params });
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
