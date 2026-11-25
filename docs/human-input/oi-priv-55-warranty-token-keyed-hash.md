# Question for the principal: OI-PRIV-55, the raw warranty token in both databases

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** none yet. One should be opened when this is sent (README rule 7). It could share #961 if Q1 is decided together with `OI-PRIV-33` (§6).
**From:** NeurOne Software Engineering (`OI-PRIV-55` in `docs/status/pending-decisions.md`; `docs/np_priv_analysis_005.md` F5)
**To:** the principal (the Privacy Lead as second reader; Quality or Regulatory for Q2; Firmware and Infrastructure for the costs)
**Response needed:** an option for Q1, answers to Q2 and Q3, a short rationale, anything that would reopen it, and your name, role and the date. The decision is then recorded in `NP-FW-EMMC-002` §A, which closes the item.

## 1. The question

The warranty registry holds each owner's name, email and address beside the helmet's 256-bit warranty token
(`ci/warranty/warranty_registration_schema.sql`). The fleet database (System Health Data Record, SHDR) is keyed on the same
raw token in every table (`ci/shdr/shdr_fleet_schema.sql`, `devices.warranty_token` and the 22 tables that reference it). So
anyone who can read both runs one equality join and has SHDR linked to a person, which is what `CLAUDE.md` §5.1 says SHDR
never is. Three things stand in the way: separate cloud projects with no shared IAM role, a code-review checklist line, and a
static check of the two DDL files (`ci/test_warranty_nojoin.py`, `OI-EMMC2-06`). That is the documented design
(`NP-FW-EMMC-002` §A.4 (3): "joining them is prohibited in production"). So this is a decision, not a defect.

Plain form: **should the fleet database stop holding the value the warranty registry holds, and if so, who should be able
to link the two?**

- **Q1.** Which option in §5 (A to E)?
- **Q2.** Must NeurOne be able to find the owner of a given fleet record, or a given owner's fleet record, **without the
  owner taking part** (no helmet in hand, no app attaching anything)? §4 lists every purpose found in the documents. This
  is the question that decides between D and B or E. Quality or Regulatory should answer it for T2 (FDA correction and
  removal, complaint handling).
- **Q3.** `NP-FW-EMMC-002` §A.5 (2) promises "a CI test [that] attempts a cross-database join and confirms it fails with an
  authorisation error". No such test exists. `ci/test_warranty_nojoin.py` reads the two DDL files and connects to nothing.
  Should the live test be built against the deployed projects once they exist, or should §A.5 be reworded to say what the
  gate does? This holds under every option, A included.

## 2. What is known: where the raw token goes today (checked on `main`, 2026-11-25)

| # | Place | What holds or sees the raw token | Source |
|---|-------|----------------------------------|--------|
| 1 | Hub, Config partition | Made by the TRNG on the first read of GATT 0x0010, stored replicated (`wtoken.rec`). Only a factory reset replaces it | `NP-FW-EMMC-002` §A.2, §A.3; `firmware/hub_control/src/np_warranty_token.c` |
| 2 | Hub, GATT `WARRANTY_TOKEN` 0x0010 | Served to **any** paired phone on an encrypted link. Who it should go to is undecided | `OI-PRIV-33` (NP-PRIV-ANALYSIS-004), brief `oi-priv-33-warranty-token-release.md` |
| 3 | App | Kept sealed per helmet, only for a helmet whose warranty consent is open on that install (`REQ-SHDR-08` (e)). Until the hub token ships, an **interim** token the app made for that helmet takes its place (`REQ-SHDR-05`). The interim token is never registered, so SHDR rows under it already join to no one | `common/core/.../analytics/FleetUploader.kt`, `FleetConfig.kt` (`X-NP-Device-Token`) |
| 4 | Upload, phone to relay | Sealed inside Oblivious HTTP. The relay sees ciphertext and the phone's address | `REQ-SHDR-07` (b); `infra/fleet/relay/Caddyfile` |
| 5 | Gateway | Opens the message and holds the token in memory. Forwards it as `X-NP-Device-Token` to the ingest **over plain HTTP** on the internal network | `infra/fleet/gateway/src/lib.rs` `forward()`, `FORWARDED_HEADERS`; `OI-PRIV-49` |
| 6 | Ingest edge | Receives it, logs no request header (`REQ-SHDR-06` (a)), passes it to the ingest service. The deployed edge is not in the repository | `infra/fleet/Caddyfile`; `OI-FLEET-01` (e) |
| 7 | Ingest service, fleet database | Writes it as the primary key of `devices` and as a column of every other table. **The service does not authenticate it.** Any 64-hex value is accepted, and the gateway key ships in every app | `ci/shdr/shdr_fleet_schema.sql`; `oi-priv-33-warranty-token-release.md` §2.2 |
| 8 | Registration API, warranty registry | The app sends the token. The registry stores it `UNIQUE` beside name, email, address and serial. **No registration API or flow exists yet** | `NP-FW-EMMC-002` §A.4; `OI-PRIV-20` (3); `OI-PRIV-54` |
| 9 | Warranty claims | `warranty_claims.warranty_token` references the registration | `ci/warranty/warranty_registration_schema.sql` |
| 10 | Complaint records (PMS) | "device identification (warranty token …)" is a field of every complaint record. **This is a third store holding the raw token**, and F5 did not list it | `NP-PMS-001` §3.2 |
| 11 | Factory reset or transfer | R-9 mints a new token. §A.6 says the old token's SHDR record is "archived … and flagged `device_transferred: true`". **Nothing sets that flag.** The fleet never learns that a token retired, and it could learn that only by linking the old token to the new one, which is the link the reset exists to break | `NP-FW-EMMC-002` §A.6, §B.3; `devices.device_transferred` |
| 12 | Service network | No tier's workflow (`docs/reference/service-network.md` §8.1) looks a device up in the fleet. A Tier B or Depot unit in hand can be read over GATT, so it gives the raw token | §8.1; row 2 |

**What no option here changes:** the module-UID self-join inside SHDR (`OI-EMMC2-08`). After a reset, or across a clinic's
helmets, the same about 80 module UIDs re-link two fleet pseudonyms to each other. That links devices to devices, not to
people, and it is independent of how the token is stored. One join this brief adds to that class: **a warranty claim with
a returned tile** gives the depot the claimant's name and the tile's UID, and the UID finds the fleet rows with no token at
all. Only the `OI-EMMC2-08` alternative (`module_ref = HMAC(device-local key, uid)`) closes that.

## 3. Who the barrier is against

| | Threat | A (today) | B, C2, E | D |
|---|--------|:-:|:-:|:-:|
| T1 | A copy of both databases (a breach of both, two backups, a careless export to an analyst) | **Joins** | Needs a key as well | Cannot join |
| T2 | An insider granted read on both projects | **Joins**, unlogged unless read auditing is added | Needs the key's service as well, one logged call per device | Cannot join |
| T3 | NeurOne as an organisation, or a legal demand made to NeurOne | Joins | Joins, but only through the logged service | Can link only devices whose owner takes part, or whose helmet NeurOne has in hand |
| T4 | Someone holding the helmet, or a phone paired with it | Has the token | Has the token | Can read both values from the hub until `OI-PRIV-33` A is built. That links one device, not the fleet |
| T5 | Writing forged fleet records (`OI-PRIV-33` harm 1) | A registry leak gives every device's write value | B: same as A. C2: an SHDR leak does | A registry leak gives none |

## 4. What legitimately needs the link (Q2)

| Purpose | Direction | Can the owner take part? | Source |
|---------|-----------|--------------------------|--------|
| Complaint investigation that uses fleet history | Owner → fleet | Yes. The complaint comes from the owner, and their app holds the helmet's fleet value | `NP-PMS-001` §3.1, §3.2 |
| Warranty claim. **Whether fleet data may be used to judge a claim is stated nowhere.** For example, a `drop_detected` used to refuse a claim would turn SHDR against its owner | Owner → fleet | Yes, if it is allowed at all | Not found; part of Q2 |
| Returned unit (Tier B, Depot) | Unit → fleet | The unit is in hand | Row 12 |
| Field safety corrective action (FSCA) | Fleet → owners | Not if an individual must be reached. A cohort (model, hardware revision, module lot) can be told through the registry's own `device_model` and `serial_number`, by email, and through the app on every affected helmet | `NP-PMS-001` §5 step 3 |
| Per-device predictive-maintenance results (§5.2 Phase 3 "personalisation") | Fleet → device | Yes. It is addressed to the device's fleet value, never to a person | `docs/reference/data-architecture-detail.md` §5.2 |
| Setting `device_transferred` at a reset | Old token → retired | Yes. The owner's app holds the old value when the owner starts the reset (`OI-PRIV-27`) | Row 11 |

Only the FSCA, and only if an individual owner must be reached from a fleet signal, needs NeurOne to link without the
owner. **[assumed]** T1 is wellness-exempt. **[not checked]** Whether a T2 recall or correction under 21 CFR 806 needs
per-owner reach beyond a registry cohort. If it does, the answer to Q2 is yes for T2.

## 5. Options

| | Option | What the fleet stores | What the registry stores | Who can link | Cost | Depends on |
|---|---|---|---|---|---|---|
| **A** | **Keep the design, and harden it.** Read auditing on both projects (every query logged with the reader), a break-glass role for anyone needing both, and the live IAM test of Q3 | `t` | `t` | Anyone with read on both | Low: configuration and a procedure. No code changes | Nothing |
| **B** | **Keyed hash at the gateway, with a lookup service** (the option on record) | `HMAC(k, t)` | `t` | Whoever can use `k`: the gateway on every upload, the lookup service one token at a time | One more service, a managed key, a gateway change, a schema rename, the gate rewritten | Nothing upstream. Best before the schema freeze |
| **C** | **Keyed hash held by the warranty side.** C1: the fleet stores `HMAC(k_w, t)`, computed by the warranty side for each upload. C2: the fleet keeps `t`, and the registry stores `HMAC(k_w, t)`, hashing on receipt | C1: `HMAC(k_w, t)`. C2: `t` | C1: `t`. C2: `HMAC(k_w, t)` | Whoever can use `k_w` | C1: an online call from the fleet path to the warranty side. C2: low, inside the registration API, which is not built | C2: the registration API |
| **D** | **The hub derives one value per purpose; the raw token never leaves the hub.** The hub sends `f = HMAC(t, "fleet")` on the upload path and `r = HMAC(t, "registration")` to registration. This is `OI-PRIV-33` option B | `f` | `r` | Only the hub, so only with the helmet or the owner's participation | Small in firmware, nothing to run | `OI-PRIV-33` Q1 (one firmware change serves both) |
| **E** | **D, plus an escrow for a case lookup.** At registration the app also sends `f` sealed to a lookup service's public key. The registry stores `r` and the sealed `f` | `f` | `r` and `seal(f)` | The lookup service, one registration at a time | D's cost, plus one small service whose key is used only for lookups | D. Worth adding only if Q2 is yes |

### 5.1 A: keep the design

**What changes:** nothing in the data. Both projects turn on data-access audit logging. A role that reads both exists only
as break-glass, with Privacy Lead sign-off (§A.5 (3) already asks for that sign-off for code). The live test of Q3 runs
once the projects exist.

**What it leaves:** T1 and T2 join in one query. Auditing detects a join afterwards. It does not prevent one, and it sees
nothing in a copy of the data.

### 5.2 B: keyed hash at the gateway with a lookup service

**Where the hash is made.** At the gateway, not the ingest edge. The gateway is NeurOne's own Rust
(`infra/fleet/gateway`, `CLAUDE.md` §22) and already holds the token alone (§2 row 5). Caddy cannot compute an HMAC without
a plugin. The gateway would replace `X-NP-Device-Token` with the reference before `forward()`, so neither the ingest, nor
its logs, nor the plain-HTTP hop of `OI-PRIV-49` ever carries `t`. That hop would still carry records (`OI-PRIV-49` stays
open), but they would be keyed by a value the registry does not hold.

**Where `k` lives and who can use it.** It is a non-exportable HMAC key in a cloud key-management service, in a third
project. "Use" is granted to two service identities: the gateway (compute only) and the lookup service. No person holds
`k`. **[not checked in this session]** Both major clouds offer HMAC keys that can be used but not exported. Every use is in
the key service's own audit log, so a bulk join (one call per registered token) shows as volume.

**The lookup service.** Its input is a raw token from a registration, a claim or a complaint record, plus a case reference.
Its output is the fleet reference, or the fleet rows themselves. It answers one at a time, is rate-limited, and writes an
audit log that someone other than the caller reviews. A request covering many tokens (an FSCA) is break-glass with a
second approver. The rate limit and the review interval have no derivation yet and would be registered when it is built
(`CLAUDE.md` §19).

**Rotation.** This is the opposite of the gateway transport keys in `NP-INFRA-001` §3.3c. Those can rotate cheaply because
nothing at rest depends on them. Every stored fleet row depends on `k`. An old reference cannot be turned into a new one
without `t`, which the fleet does not hold. So rotation means versioning: a `k_id` column; for a transition period the
gateway computes both references on each upload and records the pair; devices that do not upload again stay under the old
key, which must be kept as long as their rows are. **A compromised `k` drops the system back to option A, not below it.**
Rotation then repairs only the devices that upload again.

**What changes elsewhere:** `REQ-SHDR-07` (a), where the gateway would pass a reference instead of the token.
`REQ-SHDR-02`, where the wire stays the same and the stored key changes. The schema renames `warranty_token` to a device
reference in all 23 tables; the type stays a 32-byte `BYTEA`. `ci/test_warranty_nojoin.py` TOKEN-SHDR-01 asserts today that the
fleet holds `warranty_token`; it would assert the opposite, a gate that sees the property instead of the column names. The
complaint record (row 10) keeps `t` and reaches the fleet through the lookup service. The registry still holds `t`, so a
registry leak still lets someone forge fleet records for every device (T5).

**What breaks:** nothing found. Every purpose in §4 still works, through the service.

### 5.3 C: keyed hash held by the warranty side

**C1** puts the warranty project, the one that knows names, in the upload path. To hash each upload it sees `t` at the
moment each named owner's helmet uploads, so the registry learns every owner's upload times. That is a use timeline
beside a name, which is worse than today's static join. **Not recommended. It is here only so it is not proposed again.**

**C2** costs almost nothing while the registration API is unbuilt. The registry hashes on receipt under `k_w`, held in the
warranty project's key service. Going from a registration to its fleet rows then needs `t` from the owner or the helmet,
because the registry no longer has it. Going from a fleet record to its owner needs the warranty side to hash `t` and look
it up. **Weaknesses:** the raw token stays in the fleet database, the larger and more copied store (Phase 2 training, PMS
trending), and stays on the plain-HTTP hop. Anyone on the warranty side who obtains a fleet copy joins it. Claims and
complaints would hold the hash.

### 5.4 D: the hub derives one value per purpose

**What changes, place by place (§2 rows):**
- **1, 2.** The hub keeps `t` and never sends it. It serves `f` and `r`, by two characteristics or one read that names its
  purpose. **Firmware cost:** HMAC-SHA-512 is already vendored (`firmware/crypto/vendor/monocypher`,
  `crypto_sha512_hmac`), truncated to 32 bytes. No new library is needed in C. Whether to write it as `no_std` Rust instead
  is a firmware detail, as in `OI-PRIV-32` (key) Q3.
- **3.** The app stores `f` where it stores the hub token today and sends it as `X-NP-Device-Token`. It sends `r` to
  registration. It never holds `t`. The custody check of `REQ-SHDR-09` (d) works unchanged, because `f` changes whenever
  `t` does. Interim tokens are unaffected.
- **4 to 7.** No change. The gateway, the edge and the ingest carry `f`. The plain-HTTP hop (`OI-PRIV-49`) carries a value
  the registry does not hold.
- **8 to 10.** The registry, claims and complaint records hold `r`. A complaint that needs fleet history asks the owner's
  app to attach `f`, a per-complaint consent that `NP-PMS-001` §3.2's "only what's needed for the specific investigation"
  already describes.
- **11.** A reset makes a new `t`, so a new `f` and `r`, as §A.6 intends. The owner's app could send a signed "retired"
  notice for the old `f` when the owner starts the reset, which would give `device_transferred` a setter without linking
  old to new. That is a design point. It is not asked here.
- **12.** A unit in hand gives `f` and `r` over GATT.

**No key for NeurOne to hold, rotate or lose.** Each helmet's `t` is its own key. **Migration:** none before the first
helmet is registered, since no registered token exists. After that, every registration would have to be re-keyed through
its helmet.

**What it gives up:** NeurOne can never, by itself, find the owner of a fleet record (the FSCA row of §4 when an individual
must be reached). Until `OI-PRIV-33` A is built, any paired phone can read both `f` and `r` from the hub and link that one
helmet (T4). That links one device, not the fleet.

### 5.5 E: D plus an escrow

At registration the app also sends `seal(f)`, HPKE to the lookup service's public key, through the same shared-core sealing
the fleet upload uses (`common/npps-core` `ohttp.rs` builds on HPKE). The registry stores `r` and `seal(f)` and can open
neither. The lookup service opens one registration's `seal(f)` for a case, logged as in B. An FSCA that must reach
individuals opens many: break-glass. **Its advantage over B:** the key is used only for lookups, not on every upload; the
gateway is unchanged; and a registry leak gives no write value. **Rotation** is easier than B's, because the escrow can be
reversed: the service opens old seals and re-seals them under a new key, and the public key reaches the apps by a build
setting or a signed list like §3.3c. **Cost:** D's firmware change, one service, and an HPKE key pair.

## 6. Recommendation

**Q2 first. If the answer is no: D, with A's read auditing on both projects, and decide it together with `OI-PRIV-33`.
If the answer is yes: E. B only if no firmware change can be made in time.**

- **D** makes §A.4's "prohibited" a property. No query, copy or insider can join the two stores. It needs no service and
  no key for NeurOne to hold, and it matches the line the programme already takes elsewhere: in §6.3 NeurOne builds no list
  and the device decides, and in §23 the two ends hold the keys. It is the same firmware change as `OI-PRIV-33` option B,
  which that brief already recommends beside A. **Choosing D here is choosing B in `OI-PRIV-33` Q1.**
- **E** keeps D's properties for everything except a logged case lookup. It is the one to take if Quality or Regulatory say
  NeurOne must reach an individual owner without the owner.
- **B** does what the option on record says and needs no firmware. Its key is used on every upload, the registry keeps a
  value that can forge fleet records, and rotation is partial. It is the right fallback if the hub cannot change before
  the first registrations.
- **A's auditing** is cheap and worth having under every option, because the fleet database is still read by people.
- **C1: no.** **C2** is the cheapest change, but it leaves the raw token in the larger store.
- **Q3:** reword §A.5 (2) now to say what the gate does, and build the live test when the projects exist. The promise and
  the gate should not disagree in a controlled document.
- **Timing.** Every option except A is cheapest now. No registration API exists, no helmet is registered, and the fleet
  schema is not frozen (`OI-EMMC2-08`, `-11`). After the first registration, B and C2 are migrations, and D and E need every
  helmet to re-register.

## 7. What is assumed and what was not checked

- **[assumed]** The registry will be built as §A.4 describes and will hold name, email and address.
- **[assumed]** The fleet ingest will keep accepting any token. A hub-signed upload would shrink T5 under every option and
  change nothing else.
- **[not checked]** That cloud key services offer non-exportable HMAC keys with per-use audit (B). No web source was read in
  this session.
- **[not checked]** Whether a T2 correction or removal under 21 CFR 806 needs per-owner reach beyond a registry cohort (Q2).
- **[not checked]** Whether the serial number encodes manufacture data (`OI-PRIV-58`). If it does, the registry's serial
  narrows the fleet's `devices` rows under every option.
- **Found while writing this, not raised as items:** (1) §A.5 (2)'s live test does not exist (Q3). (2) The complaint record
  (`NP-PMS-001` §3.2) is a third store of the raw token. (3) `device_transferred` has no setter, and setting it as §A.6
  describes needs the link the reset breaks. (4) A returned tile joins a claimant to fleet rows through the module UID
  (beside `OI-EMMC2-08`). Each is named in the `OI-PRIV-55` entry, so it is not lost.

## 8. What to send back

- Q1: A, B, C2, D or E (and, for D or E, confirmation that `OI-PRIV-33` Q1 includes its option B).
- Q2: yes or no, with who answered it for T2.
- Q3: build the live test, reword §A.5 (2), or both.
- A short rationale, anything that would reopen the decision (for example a regulatory need for per-owner reach found
  later), and your name, role and the date.
