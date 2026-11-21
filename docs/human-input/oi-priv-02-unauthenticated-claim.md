# OI-PRIV-02: what should a restore do with a claim marker that no phone holding the backup's key made?

**Status:** ANSWERED 2026-11-21 by the principal: option A (2026-11-20); release control on the restore screen and removal of an earlier bundle's marker by a new migration backup (2026-11-21); the residual race accepted. Recorded in `docs/status/completed-decisions.md`. **From:** a Claude Code session working for the principal (2026-11-15). **To:** the principal.
**GitHub issue:** [#886](https://github.com/stevehickman/NeuroPulse/issues/886)
**Owning item:** `OI-PRIV-02` (`docs/status/pending-decisions.md`). **Requirement:** `SWR-MG-07` (`docs/np_sw_001.md` §6.3).
Related: `OI-PRIV-01` (closed, the keyed confirmation, PR #885), `OI-PRIV-09`, `OI-UI-KMP-11` and GitHub #566 (which routes
may carry a migration).

**This is a proposal, not a decision.** No code, requirement or string was changed for it.

## 1. The question

A restore of a migration bundle first writes a claim marker `{token, bundleHash}` at the backup location, by an exclusive
create-if-absent write, so of two phones restoring at once exactly one proceeds (`SWR-MG-07`). A restore that finds a
marker that is not its own refuses with reason `claimed` ("This backup has already been claimed by another phone").

Nothing in the marker shows who wrote it. The bundle hash is SHA-256 of ciphertext the location already holds and the
token is any text. **Anyone who can write to the location can plant a marker, and the restore then refuses and tells the
person something untrue.** No data is lost or disclosed; the restore is blocked until the marker is removed.

Since `OI-PRIV-01` the restoring phone can tell. It opens the bundle before the claim steps, so it holds the bundle key
at that point, and a tag only a key holder can make (as `confirmationTag` does for the confirmation) could be carried in
the claim. **What should the restore do when the marker carries no valid tag?** Pick one:

| Option | What the restore does | What it costs |
|---|---|---|
| **A. Tag and refuse, with its own reason** | The claim becomes `{token, bundleHash, tag}`. A marker with no tag or a wrong tag, or one for another bundle, refuses with a new reason (`claim_unauthentic`) and a message that says a marker is in the way that no phone holding this backup made. The marker is not touched. Release stays the person's explicit act. | Rust core only: `claimTag`, the two decisions, the flow. One reason code, one string in eleven locales (English in all until verified, `OI-I18N-04`), one row in `migration_text.rs`. The four cross-language tests that assert the claim's shape. `SWR-MG-07`'s text gains the tag and the reason; its atomic rule is unchanged. **The restore is still blocked**; what changes is that the person is told the truth and can release knowing no other phone took the backup. |
| **B. Tag and replace** | As A for the tag. On an unauthentic marker the restore deletes it, creates its own and reads back. | As A, less the new reason, plus a change to the atomic rule: a delete by name cannot say "only if it is still the marker I read", so two restores can each delete the other's claim and both proceed (§3). Closing that needs a conditional delete, which is not one of the six object calls and which a folder does not have. `SWR-MG-07` says a claim is released only by an explicit user action; B needs that sentence to except a marker that is not a claim. |
| **D. Name the claim object from the key, and tag it** (not in the item as raised; found while writing this) | The claim object's name ends in a value only a key holder can compute, so a marker planted under any other name is never met. Create-if-absent is unchanged and nothing is deleted, so there is no race. Once one genuine claim has been written its name is visible, so a later plant at that name is still possible and is refused as in A. | As A, plus: the route's claim calls take a name, which changes the `BackupRoute` role in Kotlin, TypeScript, Swift and C#, so it is not a core-only change; a release has to find the object by listing the prefix; the direct link's serving phone must accept the new names. A marker left by an earlier bundle stops blocking a new one (§2, finding 2), which is a behaviour change to state in `SWR-MG-07`. |
| **C. Accept the gap** | Nothing changes. | The untrue message stays. The entry closes with the reason recorded, to be reopened if a route whose operator is not the person is offered for migration. |

**Recommendation: A now, and D decided together with GitHub #566 Q3.** A is small, stays inside the core, leaves the
atomic rule alone and ends the untrue message. B's gain over A is one tap, and it buys that with a race in the one rule
whose purpose is to stop two restores. D is the only option that actually stops a plant from blocking a first restore,
and it is worth its cost only when the writer at the location is someone other than the person, which is the cloud
question #566 Q3 still holds open. C is defensible today (§2, "who can plant") and is the cheapest; I do not recommend it
because the message it leaves is false in a way that would alarm the person: it says another phone has their data.

## 2. What is known, and what is not

**Read from the code on 2026-11-15; nothing was run for this brief.** Re-read on 2026-11-16 by a second session against PR #885's
head (the route declarations, `claim_decision`, the backup flow's `put` step, the absence of a `releaseClaim` caller): each
statement below still holds.

- **Who can plant today.** The core carries a migration only on a route that declares an atomic claim. Those are the
  direct phone-to-phone link, where the serving phone holds the store, and folders on the device or computer
  (`LinkTransport.kt`, `IosPlatformServices.kt`, `DesktopFolderPlaces.kt`). Cloud places are backup-only
  (`SWR-MG-21` (c)) and a UHDR-only backup is never claimed. So today the writer is someone who can write to that folder:
  another account on the computer, or a member of a folder the person put in a synced or shared location. That grows to
  the storage provider if a cloud route is ever offered for migration.
- **What no option fixes.** A writer at the location can delete the bundle, which blocks every restore, and can delete a
  genuine claim, which lets a second restore proceed. A writer who copied a genuine claim while it stood can put it back
  after the person releases it, and it verifies under any tag, because the restoring phone keeps no record that would
  show the token is old. So no option makes a restore certain, or the one-restore rule hold, against a writer at the
  location. `SWR-MG-07` guards against the accident (the person restoring on two phones); the options differ in whether a
  plant by someone without the key works and in what the person is told.
- **Finding 1: nothing in the shared app releases a claim.** `BackupRoute.releaseClaim` exists and I found no caller under
  `app/NeurOneUI`. A person who meets `claimed` today, truthfully or not, has no control to release it; they would have
  to delete the file themselves. Under A or D the new message would tell them a marker is in the way with no way to act
  on it in the app. Hence question 2.
- **Finding 2: a marker left by an earlier bundle blocks a new one.** The backup flow writes a new bundle and does not
  touch an existing marker, and `claim_decision` refuses any marker that is not this restore's own, whatever bundle hash
  it carries. A second migration backup to a place where an earlier restore was claimed and never released is therefore
  refused as "already claimed by another phone". No tag is needed to see that case (the hashes differ). Hence question 3.
- **The tag I would build for A, B or D:** HMAC-SHA-256 over `token ":" bundleHash` under an HKDF-SHA-256 key from the
  bundle key with its own label (`np-migration-claim-v1`), compared in constant time as the confirmation tag is. It is
  not a secret: it is published at the location.
- **Assumed:** no released build has written a claim, so a marker without a tag need not be honoured as an older
  build's. If that is wrong, a restore begun on an older build and resumed on a newer one would be refused.
- **Not known:** whether any offered folder is in practice a shared one; whether #566 Q3 will put a migration on a cloud
  route.

## 3. The race in option B

Two phones, X and Y, both hold the key and restore at once; a marker P that neither made is at the location.

1. X reads P. Y reads P. Both find it unauthentic.
2. X deletes P, creates its claim, reads it back, owns it and starts restoring.
3. Y deletes "the marker", which is now X's claim, creates its own, reads it back, owns it and restores.

Both phones hold the data, which is the failure `SWR-MG-07` exists to prevent. The store's create-if-absent is no longer
the only arbiter, because the delete before it is unconditional. It takes a marker neither phone made and two restores
at the same moment. A writer at the location does not need this race (they can delete a claim directly), so what B adds
is an accidental path: a stale or damaged marker (finding 2) and a person restoring on two phones together.

## 4. What I need back

1. **The option** (A, B, C or D), and for D whether it waits for #566 Q3.
2. **If A or D: how the person releases.** (a) a release control on the restore screen, shown with the refusal, after the
   passphrase has opened the bundle; (b) the message only, and the person removes the file themselves; (c) something
   else. (a) is UI work outside the core and would also serve a genuine `claimed`.
3. **A marker for another bundle (finding 2):** the same refusal as an unauthentic one, or should a new migration backup
   to a place remove the earlier bundle's marker when it replaces the bundle?
4. A short reason for each (`CLAUDE.md` §18 wants one where `SWR-MG-07` changes), anything that would reopen it, and
   your name, role and date.

Until an answer is recorded, `claim_decision`, `claim_readback`, the claim's shape and `SWR-MG-07` stay as they are.
