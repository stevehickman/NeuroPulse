# Localization — generator mechanics, placeholder + plural rules, key conventions

> Relocated from CLAUDE.md §17 (Rev 43) to slim the always-loaded core. Content is verbatim; the
> section number is unchanged, so an inbound `CLAUDE.md §17` citation lands on the same material.
> CLAUDE.md §17 keeps the rule itself — user-facing text lives in `locales/*.json`, code carries a
> key, the per-platform files are git-ignored build outputs, firmware is exempt — plus the surface
> table, because that is what a line of app code needs at the moment it is written.
>
> **Read this file when:** adding, renaming or deleting a locale key; changing a generator, a build
> hook or a CI leg that compiles an app; writing a string with a placeholder or a plural; or
> answering *why* the arrangement is shaped this way.
>
> Two gates enforce it: `bun scripts/check-locale-strings.ts` (no user-facing string in source;
> no unverified or altered translation, §17.7) and `bun scripts/sync-locales.ts --verify-untracked`
> (no generated artifact tracked).

### 17.1 Adding a key

**Add a key to `locales/*.json` — all eleven** — then reference it. Nothing else is edited, and
there is no generated file in the tree to edit by mistake. Run the generator by hand only to
inspect its output; a build does it anyway. After adding a key or editing the English, `bun scripts/translations.ts fill`
copies the English into every locale not yet translated. A translated locale keeps its value, and
the key shows as stale there (§17.7).

**An unused key is deleted from every locale file**, `_metadata.json` and `_translations.json` included. A key referenced
by nothing is untranslated weight that translators are still asked to pay for.

### 17.2 Placeholders and plurals

**Placeholders are `{0}`, `{1}`** in canonical → `%1$@` for Apple, `%1$s` for Android. On Apple
**a numeric argument must be converted at the call site** (`String(count)`), because `%@` takes an
object; Android's `%s` accepts any type. Plural keys take `_ONE` / `_OTHER` (`_ZERO` is optional
and falls back to `_OTHER`), and **`{0}` must be the count** — all three generators map `{0}` to
the plural argument. A trailing `_ONE` is reserved for plurals: `sync-locales` rejects a family
with no sibling category rather than emitting a one-item plural nothing can resolve.

**A locale may carry its own plural categories** (`OI-I18N-03`). en.json's families hold English's
forms. CLDR gives Russian `one / few / many / other` and Arabic `zero / one / two / few / many /
other`, so `ru.json` may add `_FEW` and `_MANY` to a family en.json defines, and `ar.json` may add
`_ZERO`, `_TWO`, `_FEW` and `_MANY`. **French, Latin-American Spanish and Catalan also have a
`many`** (`one / many / other`): it selects exact multiples of a million, where the noun takes *de*
("1 000 000 de lignes", "1.000.000 de líneas", "1.000.000 de línies"). So `fr.json`, `es-419.json`
and `ca.json` may add `_MANY`, and the export issues it with the example count 1000000. Among the
eleven, only `en`, `en-GB`, `hi` and `bn` (`one / other`) and `zh-Hans` and `id` (`other` only) add
nothing. The list is `Intl.PluralRules(locale).resolvedOptions().pluralCategories`, read by the gate
and the export, not a table kept here. **That is the only extra key a locale may carry.** A category the
language does not have, or one on a family en.json lacks, fails `check-locale-strings.ts`. An extra
category is translated from the family's `_OTHER`. It has no English of its own, so it never holds
English: it exists only as a verified translation (§17.7). All three platforms choose the category
by the locale's CLDR rules. Android `<plurals>` and Apple string-catalog variations do it natively,
and web's `tPlural` uses `Intl.PluralRules`. A category the file does not carry yet falls back to
`_OTHER`. An explicit `_ZERO` is read for a count of exactly 0 in every locale.

**A translation keeps every placeholder, with one exception:** a plural member whose category matches
exactly one count may leave out the count `{0}`, because the word already says it. Arabic
zero/one/two and English one are like that (Arabic dual "سطران" means "two lines"). A category that
covers many counts may not: Russian `one` covers 1, 21 and 31, so without `{0}` it would say "1" for
21. The gate and `import` apply the same rule (`placeholderMismatch`).

**Consequence while a locale is untranslated:** a language whose only category is `other` (Chinese,
Indonesian) renders the English `_OTHER` for a count of 1 ("1 lines") until it is translated. That
is correct CLDR selection over English placeholder text, and it goes away with the translation.

**No printf conversion appears in a canonical value** — not `%@`, `%d`, `%.1f` or `%1$d`.
`{n}` is positional by construction, so a translation can reorder its arguments; a bare `%d` is
consumed in source order, and web's `t()` renders it literally. Precision and padding are the call
site's job (`NPNumberFormatter.decimal1`, `"%.1f".format(x)` on Android). `check-locale-strings.ts`
rejects all of them (`OI-I18N-01`, `OI-I18N-02`). A literal percent sign (`{0}%`) is fine.

### 17.3 Key conventions

**Module-level tables hold KEYS, not text** (`MODALITY_META.displayNameKey`, `ELEMENT_TYPE_LABEL`,
`PRESETS.labelKey`). A constant initialised at import time captures English before `initI18n()`
resolves; resolve with `t()` at the point of render.

**One name and one description per modality type — `MODALITY_<ID>_NAME` and
`MODALITY_<ID>_DESC`, and no others.** The name is the modality's `.npps` grammar token with
underscores replaced by spaces and each word capitalised (`pbm_transcranial` → `PBM Transcranial`;
acronyms and unit symbols keep their conventional casing). Web, iOS and Android all render those
two keys — there is no separate consumer-facing name and no per-platform description — and, like a
product designation, the name carries the same value in all eleven locales. The two regulatory
consumer names §3 locks are carried by the **descriptions** of `bes_tacs` and `tdcs`, which open
with them verbatim — the derivation has no exceptions. **The `.npps` parser and hub compiler are
unaffected**: they keep the lowercase snake_case token, which stays the canonical identifier.

**Not translated, and deliberately literal:** unit symbols and numbers (`Hz`, `mA`, `42%`,
`1064nm`), product/tier designations and part numbers (`T1`, `ZM-PBM-DUAL`), enum and identifier
values, single glyphs used as icons, and `.npps` parser / hub-compiler diagnostics — those name
grammar keywords that are English by definition and read as compiler output.

### 17.4 How the generated files are produced

**`locales/*.json` is the single source of truth, and the only place a user-facing string is
committed.** The three per-platform files are build outputs: git-ignored, and regenerated from
canonical by each app's own build — a Vite plugin (`canonicalLocales`) for web, the `syncLocales`
Gradle task for Android, and on iOS a **scheme build pre-action** plus the NeurOne target's first
build phase. All shell out to the one generator, `bun scripts/sync-locales.ts`.

**A generated resource must exist before the build plan is computed, not merely before the phase
that consumes it.** This is why iOS takes two hooks and not one. Xcode plans the build first, so a
git-ignored `Localizable.xcstrings` that does not yet exist is never in the plan and never
compiled into the bundle — a run-script phase that creates it afterwards writes a file nothing
reads, and every `String(localized:)` then renders its raw key. The first CI run of this
arrangement proved it: the phase logged "11 locales, 1420 keys" and eight tests still failed
asserting on rendered text. The **pre-action** (and the explicit generate step in `ios-ci.yml`,
which must precede *any* `xcodebuild` invocation) creates the file in time; the target phase keeps
it fresh within an open session. Gradle needs no equivalent because a generated res `srcDir` is a
declared task output, and Vite's `buildStart` runs before module resolution.

**A build needs `bun` on `PATH`** — that is now true of the Android and iOS builds, not just the
web one, and CI installs it on every leg that compiles either (`android-ci`, `ios-ci`, and both
compiled CodeQL legs). A canonical edit also triggers those workflows, which it no longer would by
path alone.

### 17.5 Why the generated files are not committed

A generated file under version control is a second source of truth whether or not anyone means it
to be. The committed String Catalog became exactly that: 26 keys existed only there, 18 of them
referenced by iOS source, and the NP-HFE-002 rewording of two setup strings was applied to the
catalogue alone — regenerating would have reverted live copy to describing retired hardware. A
staleness check caught drift after the fact; it could not stop the edit being made in the wrong
file, because the wrong file was sitting in the working tree, tracked and editable. Removing them
makes the hand-edit unrepresentable rather than merely detectable. `bun scripts/sync-locales.ts
--verify-untracked` fails CI if one is committed again.

### 17.6 Reach of the gate

`bun scripts/check-locale-strings.ts` enforces all of the above and fails CI on a violation; its
`PENDING_PATHS` names the code the rule has not yet reached — the pure-JVM `:core` Android module
(no Android plugin by design, so it cannot name `R.string`), Windows, and the simulator — so the
gate's reach stays legible. Those three, and watchOS, generate nothing today because they read no
locale file yet; each becomes a fourth generator target when it does, not a fourth committed copy.

### 17.7 Translation (GitHub #191)

Every locale file starts with the English value for every key. Translation happens as late as
possible before shipping, so strings can still be added without being translated twice. #191 sets
two rules, and the tooling below holds both:

1. **A translation is verified by a native speaker before it lands.**
2. **Adding or editing strings never modifies a translation that already exists.**

**The ledger, `locales/_translations.json`,** records each verified (locale, key). It stores a hash
of the English the translation was made from, a hash of the verified value, the reviewer, and the
date. It stores hashes and not text, so it cannot become a second copy of any string (§17.5). Each
key in each locale is in exactly one state:

| State | Meaning | Gate |
|-------|---------|------|
| untranslated | no ledger entry; the value must still equal the English | fails if the value differs |
| translated | English unchanged since; value as verified | passes; placeholders must match the English |
| stale | the English has changed since translation; the old translation stays in place | passes (an English edit is never blocked on a translator); re-issued by the next export |
| tampered | value edited after verification, outside `import` | fails |

**The workflow — `bun scripts/translations.ts`:**

- `status`: counts per locale.
- `export <locale> [--out <file>]`: writes the untranslated and stale keys to
  `translation-export/<locale>.json` (git-ignored). Each key comes with its English text, a hash of
  it, the placeholders the translation must keep, the previous translation if it is stale, and
  `legalReviewRequired` from `_metadata.json`. Keys marked `"translate": false` (the modality names,
  §17.3) are left out. Plural members carry `pluralCategory` and `pluralExamples` (counts that
  select that category in this language, such as `few: 2, 3, 4, 22, 23, 24` for Russian). The
  export includes the categories English lacks and leaves out the ones the language never selects,
  such as `_ONE` in Chinese (§17.2).
- `import <file> --verified-by <reviewer> [--legal-reviewed] [--replace]`: takes only the
  `translation` fields that are filled in. It refuses: a key whose English changed after the export;
  a placeholder set that differs from the English; a printf conversion; consent, age-gate or BIPA
  copy without `--legal-reviewed`; and any key that already has a current translation unless
  `--replace` is given. A correction is always a deliberate step, never a side effect of a new batch.
- `fill`: after adding a key or editing English, copies the English into every locale where the key
  is untranslated. It never touches a translated or stale value.

All eleven locales can go to translators. `ru` and `ar` were blocked on `OI-I18N-03` until the
plural categories above existed. `fr`, `es-419` and `ca` were affected too, though the item did not
say so at the time: each has a `many` category for exact millions (§17.2), so their exports carry
`_MANY` keys as well.
