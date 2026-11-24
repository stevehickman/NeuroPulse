# Question for the principal: OI-PRIV-33 (NP-PRIV-ANALYSIS-004), who may read the hub's warranty token

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#961](https://github.com/stevehickman/NeuroPulse/issues/961)
**From:** NeurOne Software Engineering (`OI-PRIV-33` (NP-PRIV-ANALYSIS-004) in `docs/status/pending-decisions.md`; firmware privacy review `docs/np_priv_analysis_004.md`, Finding M7)
**To:** the principal (Firmware and Software for the costs; the Privacy Lead if you want a second reader)
**Response needed:** an option for Q1, answers to Q2 and Q3, a short rationale, anything that would reopen it, and your name, role and the date.

## 1. The question

The hub hands its warranty token to any phone that has paired with it. The token is the one value that links the helmet's fleet records (System Health Data Record, SHDR) to the warranty registration, which holds the owner's name, email and address (`NP-FW-EMMC-002` §A.1, §A.4). With unauthenticated (Just Works) pairing, a patient's or a borrower's phone on a clinic helmet gets the clinic's token.

`CLAUDE.md` §6.0 treats the warranty owner and the user as two separate consent subjects. This finding weakens that split.

Plain form: **who should the hub release its warranty token to, and in what form?**

- **Q1.** Which option in §4 (A to E)?
- **Q2.** Is the warranty owner, as far as the hub knows, the same party as the clinic admin pairing of the `OI-SIMFW-71` ruling Q8?
- **Q3.** May the registration API ever treat the token as a credential (read the registration back, change it, or claim the helmet)? §3, harm 2.

## 2. What is known (checked 2026-11-21 on `main`)

### 2.1 The finding holds

| Place | What it does |
|-------|--------------|
| `firmware/hub_control/src/np_gatt_server.c:114-115` | `WARRANTY_TOKEN` (0x0010) is `NP_GATT_PROP_READ \| NP_GATT_PROP_ENC`. The only condition on reading it is an encrypted link. |
| `np_gatt_server.c` (the `NP_GATT_PROP_ENC` check) | It checks the flag the stack reports (`np_gatt_set_link_encrypted`). The hub is not told how the link was paired, and nothing checks it (`OI-SIMFW-72`, `NP-FW-HUB-001` §2.5). |
| `firmware/hub_control/include/np_warranty_token.h:7-10` | *"the only link between a device's SHDR uploads and its warranty registration … The app reads it over GATT and sends it … to the registration API (§A.4)."* |
| `np_warranty_token.c` | The token is made at its first read, and only a factory reset replaces it (§A.6). It is the same value on every read, for every phone. |
| The hub's model of who is connected | It has none. No bond table, no roles, and nothing tells the owner's link from anyone else's. `OI-FWHUB-32` and `D-39` (`NP-FW-HUB-001`) already say this about the owner-only settings: *"the hub accepts the write from any bonded, encrypted link, so restricting it to the warranty owner is the app's."* |

Anyone in range can pair by Just Works, because it needs no secret. Any generic BLE tool can then read 0x0010. The NeurOne app is not needed.

### 2.2 How the apps use the token today

- **Compose app / Kotlin core.** `NeurOneGattManager` reads 0x0010 on every connection, whatever the consent state, and exposes it as `warrantyToken`. `AppServices` passes it to `FleetUploadWiring.applyWarrantyToken` and then to `FleetUploader.upgradeDeviceToken`. Until this PR, that stored it in the `np.shdr.device-token` table (`HubTokenTable`, keyed by a salted hash of the helmet's identifier) on every phone that connected. That key is per install, so it is never carried by a user's migration, and it is in the device tier (`common/npps/migration-keys.json` `deviceTier`), so a locked phone can open it without the person.
- **Uploads.** `FleetUploader.upload` sends only when the warranty consent for the connected helmet is open on that install (`REQ-SHDR-08`). The token goes as `X-NP-Device-Token` inside an Oblivious HTTP request (`REQ-SHDR-07`): the relay sees the address, the gateway sees the token, and the gateway passes the token on to ingest unchecked. **The fleet service does not authenticate the token.** Any 64-hex value is accepted, and the gateway key ships in every app.
- **Registration.** No registration flow or API exists in the repository (`OI-PRIV-20` (3)). §A.4 is the design: the app sends the token, and the registration database stores it beside the owner's name, email and address.
- **Swift app.** When this brief was drafted, the SwiftUI app (`SHDRUploader.swift`) held the hub token in memory for the link and did not persist it. It was retired on 2026-11-22 (`OI-UI-KMP-05`, #946), so the shared Kotlin `FleetUploader` above is now the only uploader.

### 2.3 What this PR changes (no decision needed)

**`REQ-SHDR-08` (e)** (`docs/reference/data-architecture-detail.md` §5.4): the app stores a hub-delivered token only for a helmet whose warranty consent is open on that install. Otherwise it holds the token in memory for that link, drops it on disconnect, and stores it at the first upload after consent opens. Three new tests and three adjusted ones are in `FleetUploaderTests`. This applies to the interim token the rule that (a) already applies, and nothing moves toward SHDR.

**What it does not do:** stop anyone from reading 0x0010. It only limits where an honest app keeps the token.

## 3. What a holder of the token can do

Each harm is ranked by whether it needs something not yet built.

1. **Write into the owner's fleet record.** The fleet ingest accepts any token (§2.2). With the real one, a holder can post forged SHDR under the helmet's pseudonym, and that record is the warranty history §A.6 keeps. *Works today* once a gateway is deployed.
2. **Act on the registration.** If the registration API lets a token holder claim an unregistered helmet, read a registration back, or change it, then the token is a bearer credential to the owner's name, email and address. For an individual who lends a helmet, that links the borrower to the owner's identity. *Depends on an API not designed.* Q3 asks you to rule it out now, while it costs nothing.
3. **Override the owner's fleet decision.** A patient's install can grant warranty consent for the clinic's helmet: nothing on an install says who the owner is (`OI-PRIV-20` (3)). Uploads then go under the clinic's own token, linked to the clinic's registration, against a refusal the clinic made on its own phone. With the interim token those uploads at least landed under a pseudonym nobody links to the registration. *Works today.* It is partly `OI-PRIV-20`'s problem, but the token makes it worse.
4. **Hold a stable, registration-linked identifier for the helmet.** A non-owner's phone keeps it at rest in a device-tier key. *Closed for the honest app by `REQ-SHDR-08` (e); still open to a direct GATT read.*

No UHDR is exposed by any of these. The harm is to the owner's privacy (2), to the integrity of the owner's record (1) and to the §6.0 split (3).

## 4. Options

| | Option | What it closes | Cost | Depends on |
|---|---|---|---|---|
| **A** | **Release the token only to the owner's authenticated pairing.** The `OI-SIMFW-71` secure channel (SPAKE2 on the printed code, Noise, an 8-entry pairing table) gets a role bit, set for the admin pairing of ruling Q8. The hub serves 0x0010, or its payload-layer equivalent, only on a session whose pairing has that bit. | 1, 2, 3 and 4 against anyone without the printed code or the admin pairing. A patient on a clinic helmet is not the admin, so they never see it. | Small on top of `OI-SIMFW-71`: one bit per table entry, one check in the read handler, and a host test. The secure channel itself is the large cost and is not built (Noise crate unchecked, ruling Q6). **Nothing closes until it ships.** | `OI-SIMFW-71` built; Q2 answered yes. |
| **B** | **Derive what crosses the air, per purpose.** The hub sends `HMAC(token, "fleet")` for uploads and `HMAC(token, "registration")` for registration, never the stored token. | Makes the §A.5 no-join rule cryptographic: neither database's value is the other's, and only the hub can link them. **On its own it does not close harms 1 to 4,** because any paired phone still reads both values. | Small in firmware: `npps-core` already depends on `hmac` 0.12 and `sha2` 0.10, and the hub's main processor may run `no_std` Rust (`CLAUDE.md` §22); whether those crates build `no_std` for the hub target was not checked. The apps change nothing beyond the bytes. **Before any helmet ships it costs no migration**, since no registered token exists. | Nothing. Pairs with A or C. |
| **C** | **Per-owner derived value.** The hub sends `HMAC(token, owner_secret)`, where `owner_secret` is set when the owner first registers (for example from the owner's pairing key under A). A non-owner gets either a different value or nothing. | Same as A. | Needs an owner secret on the hub, which is A's mechanism under another name. Changing owners changes the value, which suits §A.6 (a transfer starts a new pseudonym). If the secret is per phone, the fleet record splits per phone, which breaks `REQ-SHDR-05`'s "per helmet". | A's pairing table. |
| **D** | **Withdraw 0x0010 until A exists.** The apps keep their interim per-helmet tokens. | Harms 1, 3 and 4 for the hub token. Harm 2 waits for registration, which needs a token: §A.4 would have to send the interim one. | Reverses the `OI-BLE-01` ruling that the interim token retires when the hub token ships. Interim tokens are per install, so one helmet's fleet record splits across every phone that uploads for it. | Your ruling, because it changes `OI-BLE-01`. |
| **E** | **Accept for T1.** Record the token as readable by any paired phone, in §6.0's detail file and in the warranty owner's notice. | Nothing. | No engineering. The notice must say that anyone who pairs with the helmet can read the value that links it to the registration. | Your ruling, and an ISO 14971 view if harm 1 bears on any maintenance reminder that is safety-critical (§5.2). |

**Tie release to the warranty-owner page at registration (`OI-PRIV-20`)** is not a separate option. The page is where the owner's role would be set, by pairing the owner's app as admin (A) or by setting the owner secret (C). The page is not built, and on its own it cannot stop a direct GATT read.

## 5. Recommendation

**A, with B, and Q3 answered "never".**

- **A** is the only option that closes the finding, and it costs little beyond the secure channel you have already ruled on (`OI-SIMFW-71` Q8 already gives the hub an admin pairing). The same role bit answers `OI-FWHUB-32`'s open question about who may set the owner-only settings (`PPX_CONFIG`, `CARDIAC_SENSITIVITY`, maintenance tests). That is one mechanism, not two.
- **B** is cheap while no token is registered, and becomes a migration once tokens are. It turns §A.5's prohibition into a property.
- **Q3 "never"** costs nothing now and removes harm 2 whatever happens to pairing. The registration API would take a token in, and would never answer, change or claim anything by it. Owner actions would be authenticated by the owner's account.
- **Until A ships:** the hub keeps serving 0x0010 (this is not option D), `REQ-SHDR-08` (e) keeps honest non-owner apps from storing it, and the gap is listed in `NP-FW-HUB-001` §2.5.

If A is too far off for the first release, **D** is the protective interim. Its price is a split fleet history per phone, and it changes `OI-BLE-01`, so it is yours to choose.

## 6. What is assumed

- **[assumed]** The registration API will be built as §A.4 describes and holds name, email and address.
- **[assumed]** The fleet ingest will continue not to authenticate the token. If it starts to (for example with a hub-signed upload), harm 1 shrinks, but nothing else changes.
- **[not checked]** Whether any maintenance reminder computed from fleet data is safety-critical (§5.2), which would make harm 1 an ISO 14971 question.
