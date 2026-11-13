# Principal decision for review: OI-UI-KMP-10, four rules about releases and variants that the shared core now reports and does not decide

**Status:** DRAFT, ready to send; not yet sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#569](https://github.com/stevehickman/NeuroPulse/issues/569)
**From:** NeurOne Software Engineering (`OI-UI-KMP-10` F1, F2, F4, F6; `SWR-FM-01`, `SWR-FM-02` in `docs/np_sw_001.md` §6.3; GitHub #569 package 2)
**To:** The principal
**Response needed:** a ruling on each question in section 3, a short rationale, conditions that would reopen it, name, role and date.
**Why now:** nothing is blocked. The shared core (`common/npps-core/src/library.rs`) now reads a family as releases and variants (`familyTree`) and says which family an arriving protocol belongs to (`importFamilies`). Each case below is one where it reports what it finds and takes no action, because what to do was not decided. The protocol library's stores and screens (package 4) need the answers before they are built.

## 1. The questions in one line each

1. May a protocol's `version` be any text, or must it be `major.minor`?
2. Is "one variant for each hardware version" a rule for a release, or for a whole family?
3. What does the library do when it holds two entries of one release for the same hardware version?
4. What does an import do with an arriving protocol whose content the family already holds?

## 2. What is known

- `SWR-FM-02`: a family has releases, a release has one variant for each hardware version, and a release is a `major.minor` version of a predefined protocol. `NP-NPPS-REF-001` §3 says `version` is `major.minor`, both whole numbers.
- **The parser does not enforce that.** `version` is read as a string, and any text is accepted, for a predefined protocol and for a user's. `common/lib/predefinedVersion.test.ts` checks the shipped files only. All 81 shipped files are at `"1.0"`.
- `familyTree` treats `major.minor` in whole numbers as a release and orders releases by number. Any other text is listed as `unreleased` and is in no release.
- `addVariantCheck` (`SWR-FM-06`, built earlier under package 4a) refuses a new variant when **any** entry of the family already supports that hardware version. It does not look at the release. With releases 1.0 and 1.1 of one family both for hardware version 1, it would refuse the second.
- `familyTree` reports two entries of one release for one hardware version as a `conflict` and lists neither as the variant. It does not choose.
- `importFamilies` reports, for each arriving entry, whether its family id is already held and which local members of that family have the same content hash (`sameContentAs`). `SWR-HV-01` says identical compiled content is the same version. What the library stores is not decided.
- No release has been done, one hardware version exists, and no user protocol exists outside a test, so no stored data depends on any answer.

**Not known:** whether a user's own protocol is meant to have releases at all. `SWR-FM-02` defines a release for a predefined protocol only.

## 3. Questions

**Q1. Should the parser refuse a `version` that is not `major.minor` in whole numbers?**

- **A. Refuse it for every protocol and composite.** The grammar matches its reference. A user file with `version: "2"` or `"1.0-beta"` stops loading; no such file is known to exist. It is a new parse error on every platform, with new golden cases.
- **B. Refuse it for predefined protocols only** (the build check that exists today), and leave a user's version free. A user protocol with free text is in no release and is listed as unreleased.
- **C. Change nothing.** Any text loads; `familyTree` lists what is not a release.

**Q2. Is a variant unique for a hardware version within a release, or within the whole family?**

- **A. Within a release.** Releases 1.0 and 1.1 may each have a variant for hardware version 1. `addVariantCheck` changes to look inside one release. This is how `SWR-FM-02` reads.
- **B. Within the family.** What `addVariantCheck` does today. A second release for the same hardware replaces or cannot coexist with the first, which conflicts with `SWR-PL-05` (the content of a superseded release stays retrievable).

**Q3. When the library holds two entries of one release for one hardware version, what happens?**

- **A. It is an error the person resolves:** the screen shows both and the person keeps one or gives one a new family. Nothing is chosen for them.
- **B. A save or import that would create it is refused,** so the state cannot arise. Needs Q2 answered first.
- **C. Both stay and both are offered,** latest modified first, as `SWR-HV-04` offers variants and never picks one.

**Q4. An arriving protocol has the family id and the content hash of one the library holds. What does the import do?**

- **A. Keep the local one and drop the arriving copy,** merging the used flag by OR (`SWR-WO-03`). One entry per content.
- **B. Keep both entries.** The names or other metadata may differ, and metadata is outside the hash.
- **C. Ask the person.**

## 4. What to send back

For each of Q1 to Q4: the option, a short rationale, anything that would reopen it, and your name, role and date. The answers are then recorded in `OI-UI-KMP-10` (`docs/status/pending-decisions.md`), the affected rows of `docs/np_sw_001.md` §6.3 and, for Q1, `docs/np_npps_ref_001.md` §3.
