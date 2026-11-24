# Privacy Analysis and Repair — Shared Code (`common/`), Second Pass

**Project:** NeurOne
**Document:** NP-PRIV-ANALYSIS-007
**Revision:** 1
**Date:** 2026-11-21
**Status:** DRAFT
**Effective Date:** —
**Author:** NeurOne Software Engineering (read-only code review)
**Approved By:** Pending — Privacy Lead review; dispositions in §6 are for the principal
**References:** `CLAUDE.md` §5.1, §5.3, §6.0, §6.1, §6.2, §16, §22, §23; `NP-APP-PRESENCE-001` §3, §4; `NP-PRIV-ANALYSIS-002`; `NP-PRIV-AUDIT-001`; the reviews of the same day `NP-PRIV-ANALYSIS-004` (firmware), `-005` (CI gates and infrastructure) and `-006` (companion apps); `docs/status/pending-decisions.md` (the privacy review of `common/`, 2026-11-15: `OI-PRIV-01` to `OI-PRIV-20`; the privacy review of 2026-10-18: `OI-ANALYTICS-01`, `-02`, `OI-FLEET-01`, `OI-BACKUP-01`, `-02`, `OI-EDF-01`); `docs/reference/consent-engine.md` §6.4
**Related Issues:** GitHub #565, #725, #886, #887, #888, #889, #903; `OI-PRIV-62` to `OI-PRIV-66` (raised by this document)
**Gate:** N/A
**IEC 62304 Class:** SW-03 Class B (companion apps; shared core)
**Jurisdiction Scope:** Global — US federal (FTC Act §5, HBNR, HIPAA for T2), US state (BIPA IL, WA MHMD, CCPA/CPRA CA), EU/EEA GDPR, UK GDPR + DPA 2018, Canada PIPEDA + Quebec Law 25 (the scope of `NP-PRIV-ANALYSIS-002`)
**Change Summary:** Rev 1 — new document. A second read-only privacy review of `common/` at commit `201b992`, compared finding by finding against every privacy item already raised. 2 High, 13 Medium, 22 Low. Raises `OI-PRIV-62` to `OI-PRIV-66` and adds the other findings to the open items that already hold their subject (§6). No code, requirement or classification is changed.

---

## 1. Scope and method

**Target.** Every source directory under `common/` at commit `201b992` (2026-11-21): `core`, `npps-core`, `npps-ffi`, `npps-jni`, `cloudauth`, `cloudstore`, `btle-jni`, `lib`, `types`, `npps`. About 37,000 lines of non-test source. Tests were read only to understand intent. Where a finding depends on how a host wires the shared code, the host file under `app/NeurOneUI/` was read and is cited.

**Method.** Four read-only passes, one per slice: (1) analytics, the SHDR (System Health Data Record) fleet upload, Oblivious HTTP, identity and the tier record; (2) consent, research, session history, models, consumables, set-up, OTA, BLE parsing and the protocol consent gate; (3) storage, the presence gate, keys, migration and the core boundary; (4) cloud sign-in, cloud stores, SigV4, the Bluetooth central and backup routes. Nothing was run, built or changed.

**Status words.** As in `docs/status/pending-decisions.md`: **checked** means the cited lines were re-read by the session that wrote this document; **reported** means one pass reported it and nobody has re-read it, so confirm it before fixing. **Latent** means the code path exists but nothing reaches it today (for example, no host sets `OFFER_PRESENCE_GATE`, nothing calls `track`, no host stages an SHDR record).

**Comparison.** Each finding carries one of: **NEW** (no existing item covers it); **GAP** (an existing item, open or closed, covers the area but not this defect); **TRACKED** (already open; re-confirmed only). The baseline is the open-items log, `NP-PRIV-AUDIT-001`, `NP-PRIV-ANALYSIS-002` and the open GitHub privacy issues. Three other reviews landed the same day: `NP-PRIV-ANALYSIS-004` (firmware, `OI-PRIV-21` to `-41`), `-005` (CI and infrastructure, `OI-PRIV-51` to `-58`; the simulator and infrastructure pass filed `OI-PRIV-42` to `-50`) and `-006` (the companion apps, `OI-PRIV-59` to `-61`). This document was compared with them before its items were raised: M3 is `OI-PRIV-59` and M13 is `OI-PRIV-41` (4), and both are recorded as TRACKED below.

**Records.** UHDR is the User Health Data Record (the person's; never accessed by NeurOne) and SHDR the System Health Data Record (the device's condition; warranty-owner consent), per `CLAUDE.md` §5.1.

## 2. Summary

The shared code's privacy architecture holds: the analytics allow-list is literal and fails closed, the warranty gates never reach `ConsentStore`, `admit()` checks in a sound order, storage is sealed with authenticated encryption under per-install and per-file keys, and cloud sign-in uses PKCE with `state` checked first. **No closed item was found reopened outright.**

The review found **2 High, 13 Medium and 22 Low**; two of them (M3, M13) were also raised the same day by the companion-app and firmware reviews. Pure privacy failures (an authorised party learning more than was agreed) predominate over security failures. Six findings are gaps left behind by fixes recorded as closed (`OI-PRIV-03`, `-04`, `-06` three times, `-12` twice).

**The highest-leverage change:** before any host turns on the presence gate, take the gate's own two records out of the sealed UHDR tier and make the ungated store fail closed over values sealed under the gated key (H1, H2). As written, the gate's first real use locks the person out of their UHDR on the desktop and silently drops the gate on iOS.

## 3. Findings

### 3.1 High

#### H1 — The presence gate's own records are sealed in the UHDR tier · NEW · checked · latent

**Where:** `common/npps/migration-keys.json` (`perInstall` holds `np.presence.enabled` and `np.presence.passphrase-wrap`, so both are in `sealedAtRest`, and neither is in `deviceTier`); `common/core/.../common/ProtectedStorage.kt:78` (`val uhdrKeys = all - deviceKeys`); `common/core/.../common/TieredKeyValueStore.kt:39` (`uhdr.migrate()` at each unlock); `common/core/.../common/EncryptedKeyValueStore.kt:181-195` (`migrate` seals any plain string, boolean or integer under that tier's key).
**Category:** Both.
**Issue:** The first unlock seals the gate's own records under the gated key. On the desktop, `PassphraseGatedSecretKeyStore` reads `np.presence.passphrase-wrap` raw from the same `FileKeyValueStore` (`app/NeurOneUI/desktop/.../DesktopPlatformServices.kt:57`): after the first unlock the wrap record is sealed under the data key it wraps, `isEnabled` still answers true (`:76`), `readRecord` (`:137-140`) cannot parse `np-enc:1:…`, and every later unlock is `FAILED`. All UHDR is lost. On iOS, `IosGatedSecretKeyStore.isEnabled` is `NSUserDefaults.boolForKey("np.presence.enabled")` (`app/NeurOneUI/shared/src/iosMain/.../IosGatedSecretKeyStore.kt:73`) on the same defaults the key-value store uses (`IosPlatformServices.kt:29`); the sealed text reads as false, so the next launch opens ungated and H2 follows. `NP-APP-PRESENCE-001` §3 calls the wrap record "the plain host key … read before anyone is present", which the code does not honour. No test restarts the app after an unlock.
**Reference:** `CLAUDE.md` §5.1 (protection only goes up; `REQ-PRES-09`); availability is a privacy property (GDPR Art. 32(1)(b)–(c)).
**Remediation:** List the gate's records as host-plain, outside `sealedAtRest`, or subtract them from `uhdrKeys` in `openGated`. Add a test that turns the gate on, unlocks, restarts and unlocks again, on each `GatedSecretKeyStore`.

#### H2 — The ungated store does not fail closed over values sealed under the gated key · GAP (`OI-PRIV-03`) · checked · latent

**Where:** `app/NeurOneUI/shared/.../AppServices.kt:86, 95-103` (chooses `openGated` or `open` from `gated.isEnabled`); `common/core/.../common/ProtectedStorage.kt:95-104` (`openInternal`); `EncryptedKeyValueStore.kt:101-107`.
**Category:** Security.
**Issue:** `open()` builds an `EncryptedKeyValueStore` over the device-only key. A value sealed under the gated key does not open under it and reads as absent, and the next write re-seals that key under a key the device releases without the person. This is the downgrade `REQ-PRES-09` forbids. `OI-PRIV-03` fixed the `openGated` side only; the choice between the two paths rests on a flag that is easy to lose (H1).
**Reference:** `CLAUDE.md` §5.1; OWASP Top 10 Privacy Risks P8 (insecure data storage); Defence in Depth.
**Remediation:** In `open()`, if any protected value carries the sealed prefix and does not open under the device key, run `LOCKED` (memory only). Derive "the gate is on" from the existence of the wrap record or the gated key, not from a separate boolean.

### 3.2 Medium

#### M1 — A staged SHDR record is not bound to a helmet · GAP (`OI-PRIV-04`, `REQ-SHDR-08`) · checked · latent

**Where:** `app/NeurOneUI/shared/.../AppServices.kt:438-444`; `common/core/.../analytics/FleetUploader.kt:48-54, 107-110`.
**Category:** Pure privacy.
**Issue:** `ShdrStaging` is one file, `shdr_staging.bin`, with no record of which helmet staged it. The warranty gate is checked against whichever helmet is connected when the upload signal arrives, then whatever is on disk is sent, so helmet A's record can leave under helmet B's owner's consent and token. `upload` reads the `@Volatile` `hubIdentifier` once for `isOpen` and again inside `deviceToken`, so the gate and the token can name different helmets. A revoke leaves the staged record in place, so a later grant sends records made before it.
**Reference:** `CLAUDE.md` §6.0 (warranty-owner consent is per helmet); Purpose Creep.
**Remediation:** Stage one file per salted hub hash, or have the hub write its identity into the blob and check it against the connected hub. Take one snapshot of the hub id for the gate, the token and the read. Delete that helmet's staged record on revoke.

#### M2 — The interim fleet token is per phone per helmet · GAP (`OI-FLEET-01` (h)) · checked

**Where:** `common/core/.../analytics/FleetUploader.kt:89-93`.
**Category:** Pure privacy.
**Issue:** `REQ-SHDR-05` stops one phone joining many helmets; it does not stop the reverse. A clinic helmet used with N patients' phones uploads under N tokens, so the fleet database can count a helmet's wearers and split its records by person. When the hub token arrives, any device field in the payload links the interim token to the helmet's lifetime record.
**Reference:** `CLAUDE.md` §5.1 (SHDR is never user-linked); LINDDUN Linkability.
**Remediation:** Upload nothing until the hub-provisioned token is present, or have the fleet service drop an interim token's history when the hub token for that helmet first appears.

#### M3 — One consent store and one analytics identity per install, not per wearer · TRACKED (`OI-PRIV-59`, `NP-PRIV-ANALYSIS-006` F-03) · checked

**Where:** `app/NeurOneUI/shared/.../AppServices.kt:216` (one `ConsentStore`); `common/core/.../analytics/ResearchAnalyticsGate.kt:32-33`; `PostHogAnalyticsBackend.kt:83`; against `NPLimitsStore.activeProfileId`, which names the person on the device.
**Category:** Pure privacy.
**Issue:** `REQ-CONSENT-01` (c) requires each patient on a clinic device to have their own `ConsentStore`. On a shared install, wearer A's research opt-in opens analytics for wearer B, both wearers' events merge under one pseudonym, and one wearer's withdrawal speaks for another.
**Reference:** `CLAUDE.md` §6.0; GDPR Art. 7 (consent is the data subject's own); Bundled Consent.
**Remediation:** Key the research-consent state, the analytics flag and the analytics id by profile, and close the gate when no profile is active. Related: GitHub #565. **Already held by `OI-PRIV-59`**, raised the same day from the companion-app review; this pass adds that the analytics `distinct_id` is per install as well, so the wearers' events merge under one pseudonym.

#### M4 — A change of custody keeps the old owner's fleet consent and enrolment · GAP (`OI-PRIV-20` (6)) · checked

**Where:** `common/core/.../analytics/FleetUploader.kt:96-100` (`upgradeDeviceToken` overwrites silently); `CharacterisationEnrolmentGate.kt:110-113` (`clearForDeviceTransfer`, no caller).
**Category:** Pure privacy.
**Issue:** A new 32-byte hub token arriving where a different one is stored is what a factory reset or a change of owner produces, but the helmet's fleet consent, enrolment and epoch are kept.
**Reference:** `CLAUDE.md` §6.0 (`REQ-SHDR-09`).
**Remediation:** Treat "a stored hub token exists and a different one arrives" as a transfer: clear that helmet's consent, enrolment and epoch, and delete its staged record.

#### M5 — Widening a clinician grant gives the whole new tier · NEW (related to `NP-PRIV-ANALYSIS-006` F-08) · checked

**Where:** `common/core/.../consent/ConsentStore.kt:295` (`elements = newTier.uhdrElements - grant.approvedElements`); `common/core/.../models/ConsentModels.kt:151-158` (`ClinicianAccessExpansionRequest` carries only `toTier`).
**Category:** Pure privacy.
**Issue:** An initial grant is limited to its use cases; an expansion hands over every element of the new tier, including elements no use case needs. A request for the grant's own tier also counts as a widening.
**Reference:** `CLAUDE.md` §6.1 (clinicians select use cases, never data elements); `REQ-CONSENT-15` (a); HIPAA Minimum Necessary.
**Remediation:** Put `useCaseIds` on the request; grant `minimumNecessaryElements(ids) ∩ toTier.uhdrElements − approved`; have `accessDifferential` show that set; append the ids to the grant. `NP-PRIV-ANALYSIS-006` F-08 found that the use-case tables list each tier's whole set; this is the code path, which ignores the use cases even where the tables are minimal.
**Status, 2026-11-22: fixed (`OI-PRIV-64`; `REQ-CONSENT-15` (f)).** The request carries `useCaseIds`; `accessDifferential` takes the request and shows `minimumNecessaryElements(ids) ∩ toTier.uhdrElements − approved`, which is what approval applies; the ids join the grant. A request with no use case, one the tier does not offer, or a lower tier has no differential. The use-case tables are unchanged (F-08, `OI-CONSENT-08`).

#### M6 — While the research record is quarantined, a commit with L3 off is not a withdrawal · GAP (`OI-PRIV-06` (3), `REQ-CONSENT-20` (c)) · checked

**Where:** `common/core/.../consent/ConsentStore.kt:324-347` (`updateResearchConsent`), compared with `withdrawBlanketResearchConsent` (`:366-388`).
**Category:** Pure privacy.
**Issue:** `isWithdrawal` is `wasBlanketGranted && !state.blanketConsentGranted`, and `wasBlanketGranted` reads false on the defaults the store falls back to when the record is unreadable. The screen commit then writes no `blanketConsentWithdrawnOnDay`, does not call `revokeResearchAnalytics()` (the analytics flag is a separate key and may still be true), and does not forget the portal token. `withdrawBlanketResearchConsent` handles the same state with `mayHaveBeenGranted`.
**Reference:** `CLAUDE.md` §6.0 ("never asked" and "said stop" stay distinguishable), §6.2 (teardown on true→false).
**Remediation:** When `consentRecordUnreadable` is set and the commit has L3 off, apply the same "may have been granted" rule. Whether a commit with L3 off is "saying stop" in that state may need a ruling; the existing test covers only ticking a category.
**Status, 2026-11-22: fixed in part (`OI-PRIV-64`; `REQ-CONSENT-20` (c)).** Such a commit always revokes research analytics and forgets the portal token, and records the blanket withdrawal when it consents to no category. One that consents to a category records none, because a recorded blanket withdrawal outranks the L2 ticks (`REQ-CONSENT-04` (e)) and would bar the scope committed in the same step; whether it is "saying stop" stays the open ruling.

#### M7 — Only three per-person signals are barred from research extracts · GAP (`OI-PRIV-06` (5) ruling) · checked

**Where:** `common/core/.../consent/ConsentEngine.kt:189-191`.
**Category:** Pure privacy (re-identification).
**Issue:** `ELEMENTS_NO_EXTRACT_MAY_CARRY` holds EEG waveforms, the PPG optical signal and the HRV time series, on the reasoning that a recorded signal is one the person alone has. The same holds for `CLOSED_LOOP_EVENTS` (trigger sequences with session offsets), `EYE_STATE_LOGS`, per-zone `PBM_DOSE_LOGS` (the montage implies the indication) and `SESSION_TIMESTAMPS` sequences. A study may ask for any of them today. The risk depends on whether the extractor, not yet built (`OI-CONSENT-07`), sends series or aggregates.
**Reference:** `CLAUDE.md` §5.3; Re-identification via Join (Netflix Prize).
**Remediation:** A ruling by the principal with a privacy adviser on these elements, or a required extractor output shape per element before any of them is admitted.

#### M8 — The consent quarantine keeps an erased contact address for good · GAP (`REQ-CONSENT-23` (c)) · checked

**Where:** `common/core/.../consent/ConsentStore.kt:812-816` (quarantine, kept for good); `ConsentModels.kt:442-450`.
**Category:** Pure privacy (retention).
**Issue:** An unreadable `np.consent.research` record is kept verbatim, including `contactMethod`. Turning L1 off erases the address from the live record only. Decided and expired invitations and decided expansion requests also have no retention limit. All are sealed in the UHDR tier.
**Reference:** GDPR Art. 5(1)(e) (storage limitation); Permanent Storage.
**Remediation:** Scrub contact fields from quarantined research blobs when L1 goes off, or drop a blob once its withdrawal marks are recovered and the person has decided again. Set a retention rule for closed invitations.
**Status, 2026-11-22: fixed in part (`OI-PRIV-64`; `REQ-CONSENT-23` (c)).** Turning L1 off, by a commit or a restored withdrawal, blanks the contact method and channel in every quarantined research record (and in an unreadable quarantine list's text), keeping every withdrawal mark. A retention rule for closed invitations and decided expansions needs a period with a derivation, and is proposed in `OI-PRIV-64`, not set.

#### M9 — The background lock timeout uses the wall clock · NEW · checked · latent

**Where:** `common/core/.../common/PresenceLock.kt:121` (`nowMs() - since >= backgroundTimeoutMs`); `app/NeurOneUI/shared/.../AppServices.kt:68` (`epochSeconds() * 1000`).
**Category:** Security.
**Issue:** Whoever holds the phone unlocked at the operating-system level can set the clock back while the app is in the background; the elapsed time is negative and the app never locks, so UHDR stays readable without the person.
**Reference:** `CLAUDE.md` §5.1 (presence); `REQ-PRES-03`.
**Remediation:** Use a monotonic clock (boot time), and treat a negative elapsed time as expired.

#### M10 — Every UHDR write while unlocked leaves fresh unzeroed key copies · GAP (`OI-PRIV-12`) · checked · latent

**Where:** `TieredKeyValueStore.kt:57` (`applyPending()` on every UHDR write) → `EncryptedKeyValueStore.kt:115, 124-125` (`resealFromLegacy` → `legacyKey()`) → `PassphraseGatedSecretKeyStore.kt:88-90` (`load()` returns `dataKey?.copyOf()`, never filled by the caller).
**Category:** Security.
**Issue:** Each write leaves one copy of the gated key and one of the device key on the heap, past `lock()` (which zeroes only the cached copy), and re-decrypts every protected key.
**Reference:** `CLAUDE.md` §22; `SWR-MG-28`.
**Remediation:** Wipe the legacy key after use, and run the re-seal once per unlock behind a done marker.

#### M11 — Writes made while locked live only in memory · overlaps `OI-PRIV-19` · checked · latent

**Where:** `common/core/.../common/TieredKeyValueStore.kt:7-9, 20, 40-45, 50, 57`.
**Category:** Pure privacy (consent integrity).
**Issue:** The file says a withdrawal made while locked must survive, but `pending` is a `LinkedHashMap` and is lost when the process dies. On unlock each held write replaces the whole sealed value, and while locked `getString` returns the held value. The whole-value replace is `OI-PRIV-19`; the loss on process death is not recorded anywhere.
**Reference:** `CLAUDE.md` §6.0.
**Remediation:** Persist held writes, sealed to a public key whose private half is gated (the core has HPKE), and merge per item at unlock.

#### M12 — The passphrase wrap record can be attacked offline · partly `UC-087` · reported · latent

**Where:** `common/core/.../common/PassphraseGatedSecretKeyStore.kt:196`.
**Category:** Security.
**Issue:** The Argon2id parameters and the wrapped data key sit in the plain host store; a copied file is protected by Argon2id and a 12-character minimum (`UC-087`, provisional) with no strength check.
**Reference:** OWASP Password Storage Cheat Sheet; Defence in Depth.
**Remediation:** Also wrap the record under the platform keystore key that `openGated` already requires, so a copied file is useless without the device.

#### M13 — The Bluetooth scan requires a product-specific service UUID in advertisements · TRACKED (`OI-PRIV-41` (4)) · reported (firmware advertising not built)

**Where:** `common/btle-jni/src/central.rs:277, 388`; the UUID base `4E455550-…` (ASCII "NEUP", `common/npps/constants.json:296`).
**Category:** Pure privacy.
**Issue:** The scan filters on the NeurOne service, so the hub must put that UUID in every advertisement. Any passive scanner learns that someone nearby wears a neuromodulation device and, with a static address, can follow them.
**Reference:** LINDDUN Detectability and Linkability; Bluetooth Core Specification LE Privacy (resolvable private addresses).
**Remediation:** Advertise the service only during a pairing window; use resolvable private addresses; reconnect bonded hubs by identity resolution, not a service-UUID scan. **`OI-PRIV-41` (4)** records the UUID from the firmware side; this pass adds that the central's scan filter makes advertising it a requirement.

### 3.3 Low

| # | Finding | Where | Category | Comparison |
|---|---------|-------|----------|------------|
| L1 | OneDrive free space fetches `/me/drive` with no `$select`; the reply includes the owner's name and id, against `SWR-MG-30` (c). Checked. | `common/cloudstore/.../OneDriveObjectStore.kt:176` | Pure privacy | NEW |
| L2 | The Cloud Storage scope `devstorage.read_write` reaches every bucket the Google account can reach; Azure `user_impersonation` every storage account. Neither is documented as broad. Checked. | `common/cloudauth/.../CloudAuthTypes.kt:16, 22` | Security | NEW |
| L3 | Sign-out deletes the credential, then revokes once; a failed revocation cannot be retried. Checked. | `common/cloudauth/.../CloudAuth.kt:487-510` | Security | GAP (`OI-PRIV-15` (4)) |
| L4 | No HTTPS check in the platform transports, on `CloudKitConfig.baseUrl`, or when `webDavLogin` uses the stored address without re-parsing it. Reported. | `common/cloudstore/.../JdkHttpTransport.kt:25`; `CloudAuth.kt:266, 376` | Security | GAP (`OI-PRIV-15` (2)) |
| L5 | A `localhost` redirect is accepted while the listener binds IPv4 only; RFC 8252 recommends the literal address. Checked. | `common/cloudauth/.../RedirectRules.kt:58`; `LoopbackAuthorizationLauncher.kt:52, 173` | Security | NEW |
| L6 | The hub-hash salt is stored in the same value as the hashes, so a reader of the value can brute-force a 48-bit address; the table is in the device tier. Checked. | `common/core/.../analytics/HubTokenTable.kt:67` | Security | GAP (`REQ-SHDR-05` (d)) |
| L7 | `onboarding_step` still allows `consent_flow_step_N` and `consent_flow_complete`: the timeline `OI-PRIV-05` (2) removed from `screen_name`. Checked. | `common/npps/analytics-properties.json:53-54` | Pure privacy | GAP (`OI-PRIV-05`) |
| L8 | The launch count is not reset on withdrawal, so a re-consented new id starts out "established". Reported. | `common/core/.../analytics/EngagementTier.kt` | Pure privacy | GAP (`OI-PRIV-05` (4)) |
| L9 | `reset()` neither cancels nor joins in-flight captures, so an event can reach the vendor after the erase. Reported. | `common/core/.../analytics/PostHogAnalyticsBackend.kt:68-70` | Pure privacy | GAP (`OI-ANALYTICS-01`) |
| L10 | Revoking a clinician grant deletes it, which loses the record of who had access; `CLAUDE.md` §5.1 says clinician access is audited. Checked. **Fixed 2026-11-22 (`OI-PRIV-64`, `REQ-CONSENT-26`):** a revoked grant is kept, inactive, with its day. | `common/core/.../consent/ConsentStore.kt:157-158` | Pure privacy | NEW |
| L11 | `grantClinicianAccess` does not check scopes against the use cases and tier, silently replaces a grant with the same id, and has no expiry maximum. Checked. **Fixed 2026-11-22 (`OI-PRIV-64`, `REQ-CONSENT-15` (g), `REQ-CONSENT-26` (d)):** scopes and use cases are validated and a held id is refused; no expiry maximum, which would be an underived number. | `ConsentStore.kt:149-155` | Pure privacy | NEW |
| L12 | The clinician-portal channel does not tie a verified request to the grant's clinician, and its refusals differ in shape after verification. Reported; latent. | `common/core/.../consent/ClinicianPortalChannel.kt:27, 91-94` | Both | NEW (the `OI-CONSENT-05` successors) |
| L13 | Generated `toString` still prints UHDR in `HRVData`, `SessionState`, `ResearchConsentState` (contact), `StudyInvitation`, `StudyParticipationRecord`, `ClinicianConsentGrant`, the cervical status classes and `ResearchSuggestion`. Reported. | `common/core/.../models/` | Pure privacy | GAP (`OI-PRIV-12` (6)) |
| L14 | `isLocalOrPrivateHost` lets `127.1`, `0x7f.0.0.1`, `::ffff:127.0.0.1` and 100.64/10 through. Reported; latent. | `common/core/.../protocol/NPConditionLinkPolicy.kt:186-200` | Security | NEW |
| L15 | A stop caused by consent withdrawal is recorded as `Declined` on the invitation; `CLAUDE.md` §6.2 says an opt-out is a withdrawal, not a decline. Reported. **Fixed 2026-11-22 (`OI-PRIV-64`, `REQ-CONSENT-11` (c)):** recorded as `Withdrawn`, as is an L3 opt-out. | `ConsentStore.kt:457-460` | Pure privacy | NEW |
| L16 | An undecodable session history reads as empty and the next write overwrites the sealed history. Reported. | `common/core/.../session/SessionHistoryStore.kt:286` | Pure privacy (integrity) | NEW |
| L17 | A user's migration clear deletes `np.warranty-owner.records`, so one user's transfer destroys a clinic's helmet records. Reported. | `common/npps-core/src/migration_flow.rs:559-563` | Pure privacy | GAP (`OI-PRIV-20`) |
| L18 | Moving a key into the device tier has no in-code allowlist; the doc comment says the tier holds three keys where the list has five; `np.warranty.consent-granted` is listed and no longer read. Checked. | `common/npps-core/src/migration.rs:89-91`; `TieredKeyValueStore.kt:36` | Pure privacy | comment NEW; stale key `OI-PRIV-20` (4) |
| L19 | `authenticatedPairing` defaults to true, so a host-written direct route is offered as paired. Reported. | `common/npps-core/src/migration.rs:269`; `common/core/.../migration/BackupRoute.kt:28` | Security | NEW |
| L20 | A stored value's associated data is its name only, so restoring an older host store on the same device (an iOS device backup) brings back an older consent state. Reported; plausible. | `common/npps-core/src/at_rest.rs:27-31` | Both | NEW |
| L21 | The automatic backup keeps the person's passphrase itself as hex. It is the person's documented choice; a derived key would avoid storing the human secret. Checked. | `common/core/.../data/BundleKey.kt:67` | Security | NEW (design note; related `OI-PRIV-61`, the same secret in the clear on an unsealed run) |
| L22 | Fixed object names give one namespace per account, so two subjects' bundles collide, and the planned warranty-owner bundle would collide with a user's. Reported. | `common/npps-core/src/object_route.rs:39-43` | Pure privacy | adds to `OI-PRIV-09` (1) |

## 4. Already tracked and re-confirmed

| Item | What this pass saw, still open |
|------|-------------------------------|
| `OI-PRIV-09` (1), `OI-BACKUP-01` (1) | The bundle is not padded; object and folder names identify the product; the clear header shows the kind |
| `OI-PRIV-05` (5) | Analytics reaches its host with no relay (option A decided, not built) |
| `OI-FLEET-01` (a), (b), (h) | Relay operator, gateway key and the interim token |
| ISC-54, `OI-EDF-01` | `shdr_staging.bin` is plain and, on iOS and the desktop, not excluded from backup |
| `OI-SIMFW-62` | No payload encryption on USB-C CDC |
| `SWR-MG-32` (h), `OI-UI-KMP-05` | Keys given as hex are still accepted |
| `OI-PRIV-07` | The participation record is labelled SHDR; this pass adds that its `withdrawnAtDay` and `enrolledOnDay` duplicate the UHDR-class `studyWithdrawals` |
| `OI-PRIV-08` (3) | Votes, intents and pledges are not gated on the portal opt-in |
| `OI-PRIV-13`, `OI-PRIV-18` | CloudKit sign-in binding |
| `OI-PRIV-14` | The desktop Google client secret |
| `OI-PRIV-15` (1), (iv) | No maximum SAS lifetime; Google cross-service revocation |
| `OI-PRIV-17` | The other Bluetooth centrals |
| `OI-PRIV-19` | Whole-value stores under the gate |
| `OI-PRIV-20` (4) | The warranty-owner value is sealed in the user's tier |

## 5. What looks good

- **Analytics:** a literal allow-list for names, keys and values; refusals never repeat a value; error codes held above 255 so no fault slot passes; no device properties, no client timestamp, `$geoip_disable`; an erasure id dropped only on a 2xx.
- **Fleet:** Oblivious HTTP per RFC 9458, padded, with the response secret used once and wiped; the warranty gates, the warranty-owner store and the enrolment gate never reference `ConsentStore` or `ResearchAnalyticsGate`; no helmet named means closed.
- **Consent:** `admit()` checks signature, floors, expiry, barred elements, record readability, withdrawal, L1 and L2/L3 in order; the complement is computed; suppression is the device's rule; withdrawal marks are the store's to write; the contact channel is typed and erased with L1.
- **Session history:** day only, whole minutes, one decimal place, retention chosen by the person.
- **Storage:** AES-256-GCM with the name as associated data; the Argon2id floor enforced on open; a key of its own per archive file; a bundle epoch; SPAKE2 pairing with per-direction keys; key handles in the core and a wiping allocator.
- **Cloud:** PKCE with `state` checked before anything else; app-folder scopes for Drive, OneDrive and Dropbox; a credential vault that refuses an unsealed store; SigV4 in Rust.
- **Bluetooth:** only error kinds are printed; a refused write is an event.

## 6. Disposition

The findings that no open item held are raised in `docs/status/pending-decisions.md` as `OI-PRIV-62` to `OI-PRIV-66`. The others are added, dated and cited to this document, to the open item that already holds their subject.

| Finding | Disposition |
|---------|-------------|
| H1, H2, M9, M10, M11 | **Raised as `OI-PRIV-62`**: the presence gate, closing before any host sets `OFFER_PRESENCE_GATE` (beside `OI-PRIV-19`) |
| M1, M4, L17 | **Raised as `OI-PRIV-63`**: helmet custody of the SHDR record and its consent, before any host stages a record (related `OI-PRIV-20`) |
| M2 | Added to `OI-FLEET-01` |
| M3 | Added to `OI-PRIV-59` (the analytics id) |
| M5, M6, M8, L10, L11, L15 | **Raised as `OI-PRIV-64`**: consent-store defects, each fixed with a test (`REQ-CONSENT-15`, `-20`, `-23`) |
| M7 | Added to `OI-CONSENT-07`, as a question for the principal with a privacy adviser |
| M12, L21 | **Raised as `OI-PRIV-65`**: the passphrase record and the automatic-backup secret (related `OI-PRIV-61`) |
| M13 | Added to `OI-PRIV-41` (4) |
| L1 to L5 | Added to `OI-PRIV-15` |
| L6 | Added to `OI-FLEET-01`, which holds the interim token and its table (the claim to correct is `REQ-SHDR-05` (d)) |
| L7, L8 | Added to `OI-PRIV-05` |
| L9 | Added to `OI-ANALYTICS-01` |
| L12 | Added to `OI-CONSENT-07`, which holds the portal transport and its trust anchor (`NP-SW-PORTAL-API-001` §4) |
| L13 | Added to `OI-PRIV-12` |
| L14, L16, L18, L19, L20 | **Raised as `OI-PRIV-66`**: hygiene in the shared core |
| L22 | Added to `OI-PRIV-09` |

## 7. What was not reviewed

- The Swift app (`app/ios`), the Windows project and the React app; the Swift twins of the shared classes.
- Server-side infrastructure (PostHog, the relay, the gateway, the fleet ingest), and whether it is configured as documented.
- Firmware: hub advertising and pairing mode, the SHDR blob's content, and whether the warranty-token characteristic requires an encrypted link.
- The platform keystore classes in depth, and the User-Agent each non-JDK transport sends.
- Which callers write UHDR while locked (M11).
- Runtime behaviour: no test was run, so every finding is from reading.

## 8. Recommended next steps

1. **H1 and H2, before anyone sets `OFFER_PRESENCE_GATE`:** take the two gate records out of the sealed and UHDR sets, make `open()` fail closed over gated-sealed values, and add the restart-after-unlock test. Small; without it the gate's first real use destroys data.
2. **M1 and M4, before any host stages an SHDR record:** stage per helmet, take one hub-id snapshot, and clear a helmet's consent on a change of custody.
3. **M5, M6 and L10:** scope clinician expansion to use cases, record a quarantined L3-off commit as a withdrawal, and keep revoked grants as inactive records. Each is small, with tests.
4. **Decide the open questions:** per-wearer consent on shared installs (M3, `OI-PRIV-59`), the extract bar (M7, `OI-CONSENT-07`), BLE advertising (M13, `OI-PRIV-41`) and the broad cloud scopes (L2, `OI-PRIV-15`).
5. **Cloud hygiene:** `$select=quota` (L1), retried revocation (L3), HTTPS at the transport (L4), a literal loopback redirect (L5).

About fifteen further Low items are in §3.3.

## Revision history

| Rev | Date | Author | Summary |
|-----|------|--------|---------|
| 1 | 2026-11-21 | NeurOne Software Engineering | New document. Second read-only privacy review of `common/` at `201b992`, compared with every privacy item already raised, including the firmware, CI and companion-app reviews of the same day: 2 High, 13 Medium, 22 Low; raises `OI-PRIV-62` to `OI-PRIV-66` and adds the rest to existing items. |
