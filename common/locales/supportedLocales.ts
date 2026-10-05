export interface LocaleInfo {
  bcp47: string;
  displayName: string;
  direction: "ltr" | "rtl";
}

export const supportedLocales: readonly LocaleInfo[] = [
  { bcp47: "en", displayName: "English (US)", direction: "ltr" },
  { bcp47: "en-GB", displayName: "English (UK)", direction: "ltr" },
  { bcp47: "es-419", displayName: "Español (Latinoamérica)", direction: "ltr" },
  { bcp47: "ca", displayName: "Català", direction: "ltr" },
  { bcp47: "zh-Hans", displayName: "简体中文", direction: "ltr" },
  { bcp47: "ar", displayName: "العربية", direction: "rtl" },
  { bcp47: "fr", displayName: "Français", direction: "ltr" },
  { bcp47: "hi", displayName: "हिन्दी", direction: "ltr" },
  { bcp47: "bn", displayName: "বাংলা", direction: "ltr" },
  { bcp47: "id", displayName: "Bahasa Indonesia", direction: "ltr" },
  { bcp47: "ru", displayName: "Русский", direction: "ltr" },
] as const;

export function findLocale(languageCode: string): LocaleInfo {
  const exact = supportedLocales.find((l) => l.bcp47 === languageCode);
  if (exact) return exact;

  const prefix = languageCode.split("-")[0];
  const prefixMatch = supportedLocales.find((l) => l.bcp47.startsWith(prefix));
  return prefixMatch ?? supportedLocales[0];
}
