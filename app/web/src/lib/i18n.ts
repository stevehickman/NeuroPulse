import { findLocale, type LocaleInfo } from "../locales/supportedLocales";
import enTranslations from "../generated/locales/en.json";

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
    const mod = await import(`../generated/locales/${bcp47}.json`);
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

const pluralRulesCache = new Map<string, Intl.PluralRules>();

function pluralRules(bcp47: string): Intl.PluralRules {
  let rules = pluralRulesCache.get(bcp47);
  if (!rules) {
    rules = new Intl.PluralRules(bcp47);
    pluralRulesCache.set(bcp47, rules);
  }
  return rules;
}

/**
 * Which member of a plural family to render (OI-I18N-03).
 *
 * The category comes from the locale's own CLDR rules, not from English's
 * one-or-other: Russian needs `_FEW` for 2–4 and 22–24 and `_MANY` for 5–20,
 * Arabic has `_TWO`, and Chinese has only `_OTHER` — its count of 1 is not
 * `_ONE`. A locale file may carry those extra categories (§17.2); one it does
 * not yet carry falls back to `_OTHER`, never to the raw key.
 *
 * `_ZERO` for a count of exactly 0 is checked first in every locale. It is an
 * optional explicit form ("No sockets selected"), not only Arabic's CLDR zero,
 * so a family that defines it gets it wherever it is defined.
 */
export function pluralKey(
  baseKey: string,
  count: number,
  bcp47: string,
  has: (key: string) => boolean,
): string {
  const zero = `${baseKey}_ZERO`;
  if (count === 0 && has(zero)) return zero;
  const category = `${baseKey}_${pluralRules(bcp47).select(count).toUpperCase()}`;
  return has(category) ? category : `${baseKey}_OTHER`;
}

export function tPlural(
  baseKey: string,
  count: number,
  params?: Record<string, string | number>,
): string {
  const resolved = pluralKey(
    baseKey,
    count,
    currentLocale.bcp47,
    (k) => k in currentTranslations || k in FALLBACK,
  );
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
