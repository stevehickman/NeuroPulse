# Principal ruling: OI-PRIV-78, an erase on phones

**Status:** DRAFT, ready to send; not yet sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#1060](https://github.com/stevehickman/NeuroPulse/issues/1060)
**From:** NeurOne Software (`OI-PRIV-78` in `docs/status/pending-decisions.md`; `NP-PRIV-ANALYSIS-006` F-13)
**To:** The principal (no other addressee named)
**Response needed:** a ruling on Q1 to Q3, and the date.
**Why this is a human decision and not a code one:** `SWR-MG-17` (`docs/np_sw_001.md`) decided that phones are not offered the erase the desktop apps have, on the premise that "a phone's data goes with the app". This asks whether to reverse that scope. Offering an erase makes protection stronger, never weaker (`CLAUDE.md` §5.1), but it changes a decided requirement and adds a screen people will rely on.

## 1. The question

Should a person be able to erase everything the app keeps on a phone from inside the app, as they can on a computer?

## 2. What is known

Removing the app does not do everything the in-app erase does:

| What the erase does (`SWR-MG-17`, on a computer) | What removing the app does on iOS | What it does on Android |
|---|---|---|
| Destroys the data key and the presence gate's key | **Leaves them in the Keychain** (`life.neurone.data-key`; `life.neurone.uhdr-key` once the gate is offered) | Expected to remove them with the app (to be confirmed on the `OI-PRES-06` device run) |
| Overwrites and deletes the files and stored values | Deletes the app container (`NSUserDefaults`, `Documents/`) | Deletes the app's data (cloud backup is already excluded) |
| Sends the analytics ID to the erasure relay first (`REQ-SW03-19` (d)) | **Nothing is sent**: the vendor keeps the events until its 30-day retention | Same as iOS |
| Revokes Google and Dropbox grants (`SWR-MG-30` (d)) | **Nothing is revoked**: the grant stays valid at the provider | Same as iOS |

**What is assumed.** That iOS Keychain items survive an uninstall. The behaviour is widely reported and Apple does not document it. It will be confirmed on the iOS device run (`OI-PRES-07`). No device was available to this session.

**What is not at stake.** The key left in the iOS Keychain protects nothing that remains, because the sealed data went with the container. It is a key that protected erased data, which `OI-PRES-10` (closed) rules an erase does not leave behind. The real exposures are the analytics events and the cloud grant, and they arise on both phones.

## 3. Questions

**Q1. Offer the erase on phones?**
- **A. Yes, on iOS and Android** (recommended). `EraseDeviceModel` is already common code. It brings the analytics drain, the cloud revocation and, on iOS, the gated key's deletion (#935, fixed by #944). The cost is that a phone app cannot end its own run, as the desktop apps do after an erase, so the phone host has to rebuild its state in place. That is design and test work, perhaps one to two days, plus the eleven-locale strings.
- **B. No.** Keep `SWR-MG-17` as it is, and record the uninstall gaps of §2 as accepted.

**Q2. On iOS, purge the Keychain items on the first launch after a reinstall?**
- **A. Yes** (recommended, alongside Q1 A). A marker kept in the app container is missing while a Keychain item exists, so the app deletes the items before it opens anything. It clears the leftover key only when the app is installed again, and does nothing for the analytics ID or the cloud grant.
- **B. No.**

**Q3. For the removal that cannot run code, tell people beforehand?**
- **A. Yes** (recommended). The analytics and cloud-account screens say to turn analytics off, or erase, before removing the app, and that otherwise the service deletes the events when its 30-day retention ends.
- **B. No.** Accept the retention without a notice.

## 4. What we need back

1. A ruling on Q1 to Q3.
2. If Q1 is B, a sentence on why the gaps in §2 are acceptable (`CLAUDE.md` §18).
3. The date.

## 5. What happens next

The ruling is recorded in `docs/status/completed-decisions.md` and in `SWR-MG-17`, which then names the hosts that offer the erase. What is chosen is built with its tests, and `OI-PRIV-78` closes. Nothing in the repository acts as if an answer has been given.
