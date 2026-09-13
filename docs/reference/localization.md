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
> Two gates enforce it: `bun scripts/check-locale-strings.ts` (no user-facing string in source) and
> `bun scripts/sync-locales.ts --verify-untracked` (no generated artifact tracked).

### 17.1 Adding a key

**Add a key to `locales/*.json` — all eleven** — then reference it. Nothing else is edited, and
there is no generated file in the tree to edit by mistake. Run the generator by hand only to
inspect its output; a build does it anyway.

**An unused key is deleted from every locale file**, `_metadata.json` included. A key referenced
by nothing is untranslated weight that translators are still asked to pay for.

### 17.2 Placeholders and plurals

**Placeholders are `{0}`, `{1}`** in canonical → `%1$@` for Apple, `%1$s` for Android. On Apple
**a numeric argument must be converted at the call site** (`String(count)`), because `%@` takes an
object; Android's `%s` accepts any type. Plural keys take `_ONE` / `_OTHER` (`_ZERO` is optional
and falls back to `_OTHER`), and **`{0}` must be the count** — all three generators map `{0}` to
the plural argument. A trailing `_ONE` is reserved for plurals: `sync-locales` rejects a family
with no sibling category rather than emitting a one-item plural nothing can resolve.

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
