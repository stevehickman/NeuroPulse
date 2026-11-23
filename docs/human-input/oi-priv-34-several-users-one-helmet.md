# Question for the principal: OI-PRIV-34 (NP-PRIV-ANALYSIS-004), several people's UHDR on one helmet

**Status:** PARTLY ANSWERED 2026-11-23 by the principal (Q1, Q2, Q4; Q3 came back as a question, answered in §9 with three follow-up questions). Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#962](https://github.com/stevehickman/NeuroPulse/issues/962)
**From:** NeurOne Firmware Engineering (`OI-PRIV-34` (NP-PRIV-ANALYSIS-004) in `docs/status/pending-decisions.md`; firmware privacy review `NP-PRIV-ANALYSIS-004`, Finding M8)
**To:** the principal, with Firmware and Software; Regulatory for Q4
**Response needed:** an option for Q1, and for Q2 to Q4 where they apply; a short rationale; anything that would reopen it; your name, role and the date.

## 1. The question

**May one helmet hold the User Health Data Record (UHDR) of more than one person, each person's sealed from the
others? Or is a helmet one person's, and everyone who uses it is told that each of them can read all of it?**

The finding was marked *inferred*. It was checked against the code and the documents on 2026-11-21 and **it holds**
(section 2). Nothing in the repository decides it, and the two ways out cost different things and tell a clinic
different things. That makes it a product and privacy decision, not an implementation detail.

## 2. What is known (verified 2026-11-21)

### 2.1 The helmet has one UHDR key and one namespace

| Where | What it shows |
|-------|---------------|
| `NP-FW-EMMC-002` §C.2, §C.3 | One UHDR master key (UKMD), a 32-byte TRNG value, **used directly as the XTS key for the whole UHDR partition**. One wrapped copy, in one Config record (`ukmd.rec`), wrapped under one wrapper key (WKMD) derived from one credential. |
| `firmware/uhdr_key/include/np_uhdr_key_types.h` | `np_ukmd_record_t` has no profile, slot or user field. The 84 reserved bytes are unused. |
| `firmware/uhdr_key/include/np_uhdr_key.h` | `np_uhdr_key_provision`, `_unlock` and `_change_credential` take a credential and nothing that names a person. `np_uhdr_hal_mount_uhdr(ukmd)` mounts the one partition. |
| `NP-FW-EMMC-002` §C.5 | A credential change re-wraps **the same** UKMD. So does a second person "joining" a helmet: the only way in is the one credential. |
| `firmware/hub_control/include/np_log_backend.h:40` | UHDR session files are `/uhdr/sessions/<counter>`, the counter being the **device-wide** session count. No user or profile in the path. |

So whoever can unlock the helmet mounts the whole partition and reads every session on it, whoever ran it.

### 2.2 Several people per helmet is already assumed elsewhere

- **`CLAUDE.md` §6.0:** a clinic may register a helmet, and *"each patient on a clinic device has their own
  `ConsentStore`"*. That store lives on the phone. Nothing on the helmet mirrors it.
- **`NP-SW-FAULTMSG-001` §9.1 and §9.2 (the principal, 2026-09-22):** a cervical VNS cardiac cutoff is held **per
  user**, with an opaque 32-bit active-user tag, a blanket warning to every user of the device, and a profile picker
  on both apps (`ProfilesScreen.kt`, *"Who is using the device"*). The safety model is multi-user; the UHDR store under
  it is not.
- **`NP-SW-FAULTMSG-001` §9.6:** the hub's cervical fault summary filters records to the active user and the
  unattributed ones, *"never another named user's"*. That filter is on the GATT frame only. The UHDR session records
  it is built from sit in the one shared namespace.
- **`NP-FW-EMMC-002` §G, §H and `NP-FW-NVRAM-001` §3.5:** wear, window and cadence figures are stated for a clinic at
  12 sessions a day.

### 2.3 No document decides it

Searched on 2026-11-21: `NP-FW-EMMC-002` §C, `docs/np_app_presence_001.md`, `docs/np_sw_faultmsg_001.md`,
`docs/np_fw_nvram_001.md`, `docs/reference/consent-engine.md`, `docs/reference/data-architecture-detail.md`, and the
repository for "multi-user", "several users", "shared device", "single-user", "per user".

- **`NP-APP-PRESENCE-001`** gates the **phone's** copy of UHDR behind the person's presence on that phone. It says
  nothing about the helmet's partition or about a second person.
- **Two open questions touch it and neither answers it:**
  - `OI-IFU-09` Q1 (`docs/human-input/oi-ifu-09-body-contact-cleaning.md`, draft): *is each headset a
    single-person device?* Asked for hygiene, not data.
  - `OI-SIMFW-71` Q8 (`docs/human-input/oi-simfw-71-secure-channel-design.md`, draft): on a clinic hub, does one
    clinic admin app mediate every patient, or does each patient pair their own phone? Its proposed answer is one
    admin app, with *"admin pairing is not consent to read any patient's data"*. Under one UKMD, that sentence cannot
    be true if the admin app holds the credential.

### 2.4 What follows in practice today

The first person to provision a helmet sets the credential. A second person either cannot unlock it (so their sessions
are not logged to UHDR at all, because UHDR is written only inside an unlocked session) or is given the first person's
credential and then reads the first person's whole history. On a clinic helmet, the clinic's phone holding the
credential reads every patient's record. `CLAUDE.md` §5.1 allows a clinician *"per-element, per-use-case,
time-limited, audited, revocable"* access. This is all of it, for every patient, for as long as the clinic holds the
credential.

### 2.5 Related findings from the same review (not part of this item)

- **M9, `OI-PRIV-35` (NP-PRIV-ANALYSIS-004):** the "AES-256-XTS" key is 32 bytes, so it is AES-128-XTS or K1 = K2.
  Any per-profile key below inherits whatever that item decides.
- **M10, `OI-PRIV-35` (NP-PRIV-ANALYSIS-004):** Argon2id at 64 MiB does not fit the hub's RAM (1 MB on-chip SRAM
  plus 32 MB SDRAM, `hardware-detail.md` §4.1), and there is no failed-unlock limit. **So there is today no Argon2id
  budget on the hub to cost a per-profile scheme against.** The costs below are stated relative to one derivation per
  unlock, whatever that turns out to be.
- **M2, `OI-PRIV-29` (NP-PRIV-ANALYSIS-004):** the active-user tag is written by any paired central without
  authentication. It may *name* a slot, but it must never *open* one: only the profile's credential can.

## 3. Why the answer is not ours to give

Option A changes the eMMC layout (`NP-FW-EMMC-001` is a `.docx`, so through an ECR), the key scheme the cryptography
review (`OI-EMMC2-02`) is asked to sign, and how a clinic works with patients. Option B is a disclosure to clinics and
households that their users' health records are mutually readable, and it fixes the product as one person's. Either
is a §5.1 matter and a product decision. Making UHDR protection stronger needs no ruling (`CLAUDE.md` §5.1), but
*which* stronger design, at what cost, and what is said in the meantime does.

## 4. The options

### Option A: one key and one namespace per profile on the helmet

Each person who uses the helmet has their own UHDR key, wrapped under their own credential, and their own files. The
app names the slot (by the active-user tag, or a slot index the app keeps per helmet); the credential opens it. The
hub never tries every slot: that would cost one Argon2id derivation per slot per unlock and would reveal by timing how
many slots are in use.

Two ways to build it:

| | **A1: fixed slices** | **A2: one partition, per-profile file keys** |
|---|---|---|
| How | The UHDR partition is cut into N fixed slices, each its own LittleFS instance and XTS key. One slice is mounted at a time | The partition keeps one XTS key, unwrapped by any profile (key slots, as LUKS does). Each profile's files are also sealed with that profile's own key (AEAD per file or per block) |
| Isolation | Complete, including how much another person has recorded and when | Contents sealed. **File count, sizes and names stay visible to every profile**, which shows another person's session cadence. Names must be opaque and per-profile counters used, or the device-wide counter interleaves people (cf. `OI-PRIV-41`, L3) |
| Config | N records of 192 B (1.5 KiB at N = 8) in a 16 MiB partition | The same |
| eMMC space | Fixed per person: 6,903 MiB ÷ N. At N = 8, about 863 MiB, about 21 h of EEG at the ≈12 kB/s in `np_log_backend.h`, against about 168 h for the whole partition. A household with one heavy user wastes most of it | Shared; no waste |
| RAM | Unchanged if one slice is mounted at a time (the LittleFS caches are per mounted instance) | A per-file AEAD context in the logger; small |
| Argon2id | One derivation per unlock, as today | One per unlock, as today |
| New code | Partition table, slice mount, slot selection, per-slot provision and erase. The existing XTS mount path is reused per slice | A file-encryption layer in the Class B logger (crypto on the hub goes through the CAAM/DCP HAL), key slots for the shared XTS key, opaque naming |
| Erase one person | Destroy their slot record (crypto-erase) and reformat their slice | Destroy their slot record; their files stay as unreadable ciphertext until deleted |
| Spec work | `NP-FW-EMMC-002` §C rewritten; `NP-FW-EMMC-001` partition clauses by ECR; `OI-EMMC2-02` scope widened | The same, plus a metadata-leak ruling under `CLAUDE.md` §5.1 rule 2 |

Both need, beyond the helmet:

- **Who holds each profile's credential.** For A to isolate anything on a clinic helmet, **each patient must hold their
  own credential** (their own phone, or a PIN they enter). If the clinic's admin app holds every patient's credential,
  the slots isolate nothing from the clinic. This must agree with `OI-SIMFW-71` Q8.
- **Backup per profile.** `CLAUDE.md` §5.1's nightly backup under a user-held key becomes one backup per profile.
- **The profile cap N.** The safety MCU already caps pending cardiac users at 8 (`NP-SW-FAULTMSG-001` §9.4), which is a
  natural ceiling. N would be a choice with no derivation and would get a row in `docs/status/unjustified-choices.md`.

### Option B: a recorded single-person decision, with what a clinic and a household are told

The helmet stays as built: one key, one namespace. The decision is recorded, and every person who uses a shared
helmet is told plainly that **each person who can unlock it can read the health records of everyone who has used
it**. A clinic would be told to use one helmet per patient, or to erase (the `NP-FW-EMMC-002` §B factory reset) and
re-provision between patients, which destroys the previous patient's helmet copy unless they have a backup.

- **Cost:** no firmware. Text in the IFU, the privacy notice and the warranty registration flow (Regulatory to review),
  and an app warning when a second profile is added to a helmet.
- **What it conflicts with:** `CLAUDE.md` §6.0's per-patient model and `NP-SW-FAULTMSG-001` §9's per-user cardiac
  model both assume a shared helmet. Under B those still work, but their privacy promise stops at the phone.
  `OI-SIMFW-71` Q8's *"admin pairing is not consent to read any patient's data"* would have to be withdrawn for the
  helmet's copy.
- **It is not a weakening.** It records what is already true; nothing is lowered (`REQ-PRES-09`, `CLAUDE.md` §5.1).
  It also does not preclude A later: the 84 reserved bytes and the HAL seams leave room.

### Not offered: refuse a second profile's sessions in firmware

The hub could decline to run, or to log, sessions for any tag other than the one present at provisioning. The tag is
unauthenticated (M2), so this would be a label rather than a control, and it would stop logging a real second user's
sessions while appearing to protect them.

## 5. Recommendation

**Q1: A, built as A1, with B's disclosure in force until A1 ships.** The product already treats a helmet as shared in
its safety model and its consent model, so B would be a step back from what the principal decided on 2026-09-22 and
what §6.0 states. A1 is the only option that hides from one person how much another has recorded, and it reuses the
existing XTS mount path instead of adding a file-encryption layer to Class B code. Its cost is fixed space per person,
which is acceptable at N = 8 and can be revisited.

Until A1 is built, nothing on the helmet isolates anyone, so B's notice is what is true and should be shown. This
also needs `OI-IFU-09` Q1 and `OI-SIMFW-71` Q8 answered consistently with whatever is chosen here.

## 6. Questions

**Q1.** May several people's UHDR be on one helmet, each sealed from the others (**A**), or is a helmet one person's,
with the disclosure (**B**)?

**Q2.** If A: **A1** (fixed slices) or **A2** (one partition, per-profile file keys)? And the profile cap N (8
proposed, matching the safety MCU's pending-user cap)?

**Q3.** If A: on a clinic helmet, does **each patient hold their own credential** (recommended; without it the slots
isolate nothing from the clinic), or may the clinic's admin app hold them? Answer together with `OI-SIMFW-71` Q8.

**Q4.** Until A is built, or for good under B: is B's notice to be shown, where (IFU, privacy notice, warranty
registration, the app when a second profile is added), and does Regulatory need to review it?

## 7. What happens with the answer

The answer is recorded in `docs/status/pending-decisions.md` (`OI-PRIV-34` (NP-PRIV-ANALYSIS-004)) and
`docs/status/completed-decisions.md`, and then:

- **A1 or A2:** `NP-FW-EMMC-002` §C gains the slot and namespace design and its requirements; an ECR to
  `NP-FW-EMMC-001` for the partition layout; `OI-EMMC2-02`'s scope widened; the firmware and host tests follow.
- **B:** the decision goes in `NP-FW-EMMC-002` §C and `CLAUDE.md` §5.1's detail file, and the notice text goes to the
  owners of the IFU and the privacy notice.

## 8. Answers received (the principal, 2026-11-23)

| Q | Answer | What follows |
|---|--------|--------------|
| Q1 | **Shared.** Several people's UHDR may be on one helmet, each person's sealed from the others. | Option B is rejected. |
| Q2 | **Whichever approach makes backup and restore easiest.** A user of a shared helmet may want to move their UHDR to their own helmet. | **A2 meets that, and A1 does not** (see below). §5's recommendation of A1 is superseded. |
| Q3 | Answered with a question: *what kind of credential?* The principal added a requirement: **the warranty owner (a clinic) may need to replace a helmet, or a device that stores UHDR for all its clients, so it must be able to back up and restore all the UHDR on a device without having access to any of it.** | §9 answers the question and asks three narrower ones. |
| Q4 | **No interim notice: no device has been deployed.** | Nothing is told to anyone in the meantime, and the layout can change freely before first provisioning. |

**Why Q2's criterion selects A2.** An A1 slice is a block image under an XTS key that depends on the block's position.
It can be backed up and restored only as a whole slice, into a slice of the same size, and it cannot be merged into a
profile on another helmet. A2 seals each file under that person's own key, so each file is a self-contained ciphertext
object. It can be copied anywhere, by anyone, without being readable, and it can be added to another helmet as that
person's profile. That covers both journeys the principal names: a person moving to their own helmet, and a clinic
moving everyone to a replacement.

**A2's weakness (§4) mostly goes away under this design.** The concern was that every profile could see the others'
file counts and sizes. Under the design below, no profile mounts the partition. The hub firmware mediates every access,
so another person's directory is never listed to an app. The partition's outer XTS layer is keyed by the helmet itself
(bound to the device-fused key), which protects names and sizes against an eMMC image. One leak remains: a clinic that
exports the store sees how much each profile has recorded (§9.4).

## 9. Q3: what kind of credential

### 9.1 Three secrets, not one

Today one credential does everything: it opens the single key, on this helmet only (`NP-FW-EMMC-002` §C.4). The wrap
is bound to the helmet's fused key (`np_uhdr_hal_hw_bind`, privacy review F1), so a copy of `ukmd.rec` is useless on
any other helmet. That binding is what stops an offline attack on an eMMC image. It is also why nothing can be moved
today. The principal's requirements separate **who can read** from **who can move**, so the design needs three secrets.

| Secret | What it is | Who holds it | What it allows |
|--------|------------|--------------|----------------|
| **Profile key** | A random key per person; it seals that person's files | The helmet only, wrapped twice (below) | Reading that person's UHDR |
| **Unlock credential** | Opens the profile key on this helmet for a session. Wrap 1 = profile key under `hw_bind(Argon2id(credential), fused key)`, as today | The person | Using the helmet. Bound to this helmet, so the offline protection is kept |
| **Recovery code** | Wrap 2 = profile key under a key derived from a code the person holds | The person. Never the clinic, never NeurOne | Re-binding the profile key to another helmet |

### 9.2 How the principal's two journeys work

- **The clinic replaces a helmet.** The helmet exports every profile as it is stored: the sealed files and the profile
  key under wrap 2. It sends them over the encrypted transport (`CLAUDE.md` §23) to the clinic's app, which needs no
  user present. The clinic stores the export and imports it into the replacement helmet, and reads nothing at any
  point. Each person's profile wakes the first time that person presents their recovery code on the new helmet (or
  their phone does it for them, if it holds the code; see Q3b). That re-wraps the profile key under the new helmet's
  wrap 1.
- **A person moves to their own helmet.** The same export, of one profile, started by the person. On their own helmet
  it becomes their profile. If they already have one there, the helmet re-seals the imported files under it once both
  are open.

The clinic's export is the users' UHDR as opaque blobs. It is not the warranty owner's own data, and it is kept as a
separate object from the owner's backup (`CLAUDE.md` §6.0, `REQ-SHDR-10`: the owner's data never travels with a
user's).

### 9.3 The unlock credential: options

| | What the person uses | Good | Costs |
|---|---|---|---|
| **C1** | Their own phone. The presence-gated key on the phone yields the helmet credential (`NP-APP-PRESENCE-001`) | Strongest; nothing is typed; reuses what is built | Each person needs a phone with the app |
| **C2** | A passphrase typed on the clinic's tablet | Works for a patient with no phone | It is typed on a device the clinic controls. An attempt limit on the helmet is needed (`OI-PRIV-35` (NP-PRIV-ANALYSIS-004)) |
| **C3** | A security key or NFC card per person (FIDO2 `hmac-secret`), tapped on the phone or tablet | Nothing typed and no phone needed; the tablet relays a response it cannot reuse | Cost; cards get lost; NFC on the tablet; most new code |

**Recommended: C1, with C2 as the fallback.** It is the same pattern the presence gate already uses (a device credential
where there is one, a passphrase where there is none, `REQ-PRES-06`). C3 can be added later without changing the store.

### 9.4 The recovery code

The recovery code is the price of "the clinic can back it up but cannot read it". Whoever holds an export can try
recovery codes against it offline, and a clinic holds every patient's export. So the code is **generated by the helmet
and high-entropy** (for example 128 bits, shown as words or a QR code), **never chosen by the person**. A short code
would make an export brute-forceable, and that would weaken protection (`CLAUDE.md` §5.1). If a person loses their code,
and the helmet holding their profile is lost or replaced, their helmet copy cannot be recovered by anyone. Their phone's
own backup, if they have one, is unaffected.

The leak that remains is coarse metadata. The clinic's export shows how many profiles there are and how much each has
recorded. Padding each profile's export to a size bucket reduces that. The clinic usually ran those sessions itself.

### 9.5 Follow-up questions

**Q3a.** Unlock credential: C1 with C2 as fallback (recommended), C3 as well, or something else?

**Q3b.** Where does the person keep the recovery code? Options: (i) printed or saved by the person only; (ii) also kept
inside their own phone's presence-gated store and its backup (`SWR-MG-02`), so their phone can re-bind them
automatically (recommended where they have a phone); (iii) both. Accept that a lost code with a lost helmet means that
helmet copy is gone?

**Q3c.** Is it acceptable that the clinic's export shows how much each profile has recorded, padded to coarse size
buckets? Or must each profile be padded to one fixed size, at a storage cost?

