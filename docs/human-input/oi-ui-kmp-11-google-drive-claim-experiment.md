# Request to run a test: OI-UI-KMP-11 (GitHub #566), can a pre-generated file ID serve as an atomic claim on Google Drive?

**Status:** DRAFT, not sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#566](https://github.com/stevehickman/NeuroPulse/issues/566)
**From:** NeurOne Software Engineering (`OI-UI-KMP-11` M9; GitHub #566; `NP-SW-CLOUDSTORE-001` §7.4 and §7.6 item 5)
**To:** Whoever holds, or will open, the Google test account. **Unnamed:** who opens test accounts is Q3 of `oi-ui-kmp-11-cloud-storage-build-or-adopt.md`, which is open.
**Response needed:** the filled results table of section 6, with the runner's name, role, the date of the run and the kind of Google Cloud project used.
**Why now:** nothing is blocked. On 2026-11-11 the principal ruled that Google Drive stays backup-only for now (Q4 of the same request). This test is what would let that be looked at again.

**This is a request to run a test. It is not a decision and it has not been run.** No Google account, project or client exists for it, and nothing in the repository may act as if any result below were known. **Never put a real token, client secret, account name or file content into this file or into what is sent back.**

## 1. The question

Can two phones, each signed in to the same person's Google account through a **different OAuth client**, use one pre-generated Drive file ID so that exactly one of them succeeds in creating a claim file, and can the claim be released afterwards?

If yes, a migration claim on Google Drive might be designed. If no, Drive stays backup-only for good, and the design is not worth starting.

## 2. Why it is not known

Where each statement comes from is marked: **[page]** is a Google page read on 2026-11-11 (listed in section 8, with the page's own date); **[code]** is this repository; **[open]** is not stated on any page read.

1. **[page]** A Drive file name "isn't necessarily unique within a folder", so a claim cannot be made by name. **[code]** That is why `GoogleDriveObjectStore.createIfAbsent` always throws.
2. **[page]** An ID from `files.generateIds` "can be passed to the create or copy method through the id field", and "If the file is successfully created or copied, subsequent retries return a 409 Conflict HTTP status code response and duplicate files aren't created."
3. **[open]** That sentence describes a **retry** of a create that succeeded. Whether a **different client** using the same ID is refused the same way is not stated (`NP-SW-CLOUDSTORE-001` §7.6 item 5).
4. **[open]** Whether an ID can be used again after its file is deleted is not stated (the same item). A claim must be releasable.
5. **[open] Whether two OAuth clients see the same application data folder.** The page says the folder "is only accessible by your app and its contents are hidden from the user and from other Google Drive apps". It does not say whether "your app" is one OAuth client, one Google Cloud project, or something else. **[code]** The apps register one client per platform, so the Android app and the iOS app are different clients. Android to iOS is the case that raised #566. **If client B cannot see client A's files, the idea is dead for that case, so this is tested first (E1).**
6. **[page]** `files.generateIds` takes a `space` whose "Supported values are drive and appDataFolder". **[open]** That an ID made for `appDataFolder` behaves as item 2 says there has not been tried.
7. **[open]** Whether Drive accepts an ID the client made up, not taken from `generateIds`. If it did, two phones could derive the ID from the bundle and no ID would need carrying.
8. **[open]** Whether a `files.get` by ID, and a `files.list`, show a file straight after it is created (read-after-write). No consistency statement for Drive was found.
9. **[open]** What status and error a malformed ID gets, so the adapter could tell it from "already exists".
10. **[open]** What the body of the 409 looks like. The error-handling page read has a 404 `notFound` example and, in what was read, no 409 example.

## 3. What it needs

| Item | Detail |
|------|--------|
| A Google account | **A throwaway account made for this test. Never an account that holds anyone's real data.** |
| A Google Cloud project | One project, with the Google Drive API enabled and an OAuth consent screen in testing mode with the throwaway account as a test user. (How the console labels these steps was not read; follow the console.) |
| Two OAuth clients in that project | Call them **client A** and **client B**. "Desktop app" type is the simplest to get a token for by hand **[page]**. The real apps would use Android and iOS client types, which cannot be driven by hand; **whether the client type changes the result is [open]**, and is a limit of this test. |
| A second run with two projects | If the first run passes E1, repeat E0, E1 and E4 with client B in a **different** project, because whether the two apps would share a project is a registration choice not yet made. |
| Scope | **`https://www.googleapis.com/auth/drive.appdata` only.** Ask for nothing else, so the tokens cannot reach the account's ordinary files. |
| Tools | A browser, and `curl` (or anything that sends the same requests). |
| Time | See section 7. |

**What the test must not touch:** no health data, no real backup or bundle, no file outside `appDataFolder`, no account other than the throwaway one. Every file it creates is empty or holds the word `test`.

### 3.1 Getting an access token for each client

Do this once for client A and once for client B, signed in as the throwaway account. The endpoints are from Google's "OAuth 2.0 for Desktop Apps" page **[page]**; the hand procedure around them is the sender's and **has not been run**.

1. Open this address in a browser (one line; replace `CLIENT_ID`; the port is any free one):

   ```
   https://accounts.google.com/o/oauth2/v2/auth?client_id=CLIENT_ID&redirect_uri=http://127.0.0.1:8765&response_type=code&scope=https://www.googleapis.com/auth/drive.appdata&state=test
   ```

2. Approve. The browser is sent to `http://127.0.0.1:8765/?state=test&code=...` and shows a connection error, because nothing listens there. Copy the `code` value from the address bar.
3. Exchange it:

   ```
   curl -s https://oauth2.googleapis.com/token \
     -d code=CODE -d client_id=CLIENT_ID -d client_secret=CLIENT_SECRET \
     -d redirect_uri=http://127.0.0.1:8765 -d grant_type=authorization_code
   ```

   The reply's `access_token` is `TOKEN_A` (or `TOKEN_B`) below. Keep it in a shell variable, not in a file.

If Google refuses step 1 or 3 (for example because it wants a PKCE `code_challenge`, which this hand form leaves out and which was **not checked**), use the sample for a desktop app in one of Google's client libraries, which that page recommends, and note that in the results. Any way of getting a token for each client with only the `drive.appdata` scope serves.

## 4. The steps

Run them in order. Record the **exact** HTTP status and the whole JSON body of every reply that is not a plain success. In the commands, `TOKEN_A`, `TOKEN_B`, `ID1`, `ID2` and so on are placeholders. Add `-i` to a `curl` to see the status line.

"Predicted" is filled in only where a page read says it. **"Not stated" means no page read says what happens, and the result is what the test is for.**

| Step | What to do | Predicted | What it means |
|------|-----------|-----------|---------------|
| **E0** Setup check | With each token, list the folder (command 4.1). | 200 and a `files` array, empty on a new account **[page: the list call and the `spaces` value]** | The token and scope work. Anything else: stop and fix the setup. |
| **E1** Cross-client visibility. **Run first; it can end the test.** | Client A creates a file with no ID given (4.2). Client B lists the folder (4.1 with `TOKEN_B`) and gets the file by its ID (4.3 with `TOKEN_B`). | **Not stated.** | B sees it: the two clients share the folder; go on. B gets 404 or an empty list: each client has its own folder, and **the claim is dead for a migration between platforms. Stop, clean up (E10), report.** |
| **E2** IDs for the folder | Client A asks for IDs for the `appDataFolder` space (4.4). | 200 with an `ids` array; `space` supports `appDataFolder` **[page]** | Take `ID1` to `ID6` from it. A refusal ends the idea as designed. |
| **E3** First create | Client A creates a file with `ID1` (4.5 with `TOKEN_A`). | Success, and the file has that ID **[page]** | The base case. |
| **E4** Same ID, other client | Client B creates a file with `ID1` (4.5 with `TOKEN_B`). **Record status, `error.errors[0].reason`, `error.message` and the whole body.** | **Not stated** for a different client. (409 is documented for a retry.) | 409: the refusal holds across clients, which is what a claim needs. Success: two files or an overwrite; **dead**. Anything else: record it; the adapter would have to tell it from "exists". |
| **E5** Same ID, same client | Client A creates with `ID1` again (4.5 with `TOKEN_A`). | **409 Conflict**, no duplicate **[page]** | The documented retry case. Its body is the reference for E4: are they the same? |
| **E6** Race | For each of 20 fresh IDs (take them with 4.4, `count=20`), start one create from A and one from B at the same moment (4.6). Record, per round, the two statuses. Then list the folder and count files per ID. | **Not stated.** | Every round one success and one 409, and one file per ID: consistent with an atomic claim. **Any round with two successes, or two files for one ID: dead.** Two failures in a round: record the bodies; the claim would need a retry rule. |
| **E7** Reuse after delete | Client A deletes the `ID1` file (4.7). Then client B creates with `ID1` (4.5). If refused, wait one minute and try again, then ten minutes and try again; record each. | Delete is permanent **[page]**. The create afterwards: **not stated.** | Success: a release is a delete, and the same ID serves again. 409 at every attempt: the ID is spent once used, so a released claim needs a new ID. Success only after a wait: a release is not immediate, and for how long must be recorded. |
| **E8** IDs not from Google | (a) Client A creates with an ID it made up, of the same length and alphabet as the ones Google returned but not one of them. (b) Client A creates with a malformed ID (`!!`). | **Not stated**, both. | (a) accepted: phones could derive the ID from the bundle; refused: the ID must be carried in the bundle. (b) fixes what "bad ID" looks like, so it is not read as "exists". |
| **E9** Read after write | Ten times with a fresh ID each (`ID2` on, or new ones): client A creates (4.5) and, at once, client B gets it by ID (4.3) and lists the folder (4.1). | **Not stated.** | Every get and list shows the file: read-after-create held ten times (evidence, not proof). A 404 or a missing entry even once: a phone could fail to see a claim that exists, and the flow's read-back cannot rely on Drive. |
| **E4u** The upload form | Repeat E3 and E4 with a fresh ID using the upload address (4.8). | **Not stated** for a different client. | The adapter writes through the upload address, so the refusal must hold there too. |
| **E10** Cleanup | List the folder with each token and delete every file (4.7). Revoke both tokens (4.9). Delete the OAuth clients, or the project, unless a second run will use them. | List then shows nothing. | Nothing is left in the account. |

On the 20 in E6 and the 10 in E9: **neither number has a derivation.** One bad round is enough to end the idea, and no number of good rounds proves it; 20 and 10 are what one person can run and write down in a sitting. Run more if it is easy.

### 4.1 List the folder

```
curl -s -H "Authorization: Bearer TOKEN_A" \
  "https://www.googleapis.com/drive/v3/files?spaces=appDataFolder&fields=files(id,name,createdTime)&pageSize=100"
```

### 4.2 Create a file, letting Google choose the ID

```
curl -s -X POST -H "Authorization: Bearer TOKEN_A" -H "Content-Type: application/json" \
  -d '{"name":"e1-probe","parents":["appDataFolder"]}' \
  "https://www.googleapis.com/drive/v3/files?fields=id,name"
```

### 4.3 Get a file by ID

```
curl -s -i -H "Authorization: Bearer TOKEN_B" \
  "https://www.googleapis.com/drive/v3/files/FILE_ID?fields=id,name"
```

### 4.4 Ask for IDs for the application data folder

```
curl -s -H "Authorization: Bearer TOKEN_A" \
  "https://www.googleapis.com/drive/v3/files/generateIds?space=appDataFolder&count=10"
```

### 4.5 Create a file with a given ID

```
curl -s -i -X POST -H "Authorization: Bearer TOKEN_A" -H "Content-Type: application/json" \
  -d '{"id":"ID1","name":"claim-test","parents":["appDataFolder"]}' \
  "https://www.googleapis.com/drive/v3/files?fields=id,name"
```

### 4.6 Two creates at the same moment

Put the IDs one per line in a file `ids.txt`, then (a POSIX shell; both tokens in variables):

```
while read ID; do
  for T in "$TOKEN_A" "$TOKEN_B"; do
    curl -s -o /dev/null -w "$ID %{http_code}\n" -X POST \
      -H "Authorization: Bearer $T" -H "Content-Type: application/json" \
      -d "{\"id\":\"$ID\",\"name\":\"race-test\",\"parents\":[\"appDataFolder\"]}" \
      "https://www.googleapis.com/drive/v3/files?fields=id" &
  done
  wait
done < ids.txt
```

Each round prints two lines. This starts the two requests within milliseconds of each other from one machine; it does not guarantee they reach Google at the same instant. Two machines started by hand are a fairer test if two are to hand, and that should be said in the results.

### 4.7 Delete a file

```
curl -s -i -X DELETE -H "Authorization: Bearer TOKEN_A" \
  "https://www.googleapis.com/drive/v3/files/FILE_ID"
```

### 4.8 Create with a given ID through the upload address

```
curl -s -i -X POST -H "Authorization: Bearer TOKEN_A" \
  -F 'metadata={"id":"ID7","name":"claim-upload-test","parents":["appDataFolder"]};type=application/json' \
  -F 'file=test;type=text/plain' \
  "https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart&fields=id,name"
```

The upload address is on Google's page **[page]**; this multipart form of the call was **not read on a page this session** and is written from the sender's knowledge of the API. If it is refused for its form, say so; that is a fault of this request, not a result.

### 4.9 Revoke a token

```
curl -s -X POST "https://oauth2.googleapis.com/revoke" -d token=TOKEN_A
```

The revoke address is on Google's page **[page]**; the parameter form was not checked.

## 5. What each outcome decides

| E1 clients share the folder | E4, E4u refused across clients | E6 race | E7 ID after delete | E9 read-after-write | What it decides |
|---|---|---|---|---|---|
| **No** | any | any | any | any | **Dead for a migration between platforms.** Drive stays backup-only. Only if the second run (two clients in one project versus two projects) differs is there a registration choice that could change it; report both. |
| Yes | **No** (success, or two files) | any | any | any | **Dead.** The documented 409 is a retry guard for one client, not a claim. |
| Yes | Yes | **A round with two successes** | any | any | **Dead.** Not atomic. |
| Yes | Yes | Clean | **Reusable at once** | Held | **The design is worth drawing up:** the bundle writer takes an ID from Google, the bundle carries it, a restorer creates the claim file with it, and a release is a delete. |
| Yes | Yes | Clean | **Spent** (409 after delete at every attempt) | Held | Workable only if a release makes a new ID. The ID is inside the sealed bundle, so that means **writing the bundle again** after an abandoned migration. A larger design, and the principal's call. |
| Yes | Yes | Clean | **Reusable only after a wait** | Held | A release is not immediate. Workable only with a stated wait, which then needs its own measurement. |
| Yes | Yes | Clean | any | **A miss** | The create may be atomic, but a phone can fail to read a claim that exists. The flow's read-back cannot rely on Drive; the design would have to trust the create's own answer alone. The principal's call. |

E8 does not decide viability. It decides whether the ID must be carried in the bundle (refused) or could be derived by both phones (accepted). E5 gives the reference reply for "exists".

**Even a clean pass of everything does not make Google Drive carry a migration.** It makes the design worth starting. A route may declare an exclusive create only under the rule proposed in Q3 of the main request, part (b) of which is the contract suite run against Google through the real adapter from two separate clients (`NP-SW-CLOUDSTORE-001` §7.8), and that rule is itself not yet confirmed. Until then `GoogleDriveObjectStore.createIfAbsent` keeps throwing and Drive stays backup-only.

## 6. Recording the results

Fill one row per request made (E6 and E9 have a row per round). **No tokens, no client secrets, no account name.** File IDs made for the test may be given; they point at files that E10 deletes.

| Step | Date and time (UTC) | Client | HTTP status | `error.errors[0].reason` | Notes (the message; anything unexpected) |
|------|---------------------|--------|-------------|--------------------------|------------------------------------------|
| E0 | | A | | | |
| E0 | | B | | | |
| E1 create | | A | | | |
| E1 list | | B | | | |
| E1 get | | B | | | |
| E2 | | A | | | |
| E3 | | A | | | |
| E4 | | B | | | |
| E5 | | A | | | |
| E6 round 1 (to 20) | | A and B | | | |
| E7 delete | | A | | | |
| E7 create at once | | B | | | |
| E7 create after 1 minute | | B | | | |
| E7 create after 10 minutes | | B | | | |
| E8 (a) made-up ID | | A | | | |
| E8 (b) malformed ID | | A | | | |
| E9 round 1 (to 10): get, list | | B | | | |
| E4u first create | | A | | | |
| E4u same ID | | B | | | |
| E10 | | A and B | | | |

**Send back:** the filled table; the whole JSON body of the E4, E5 and E8 replies; the runner's name and role and the date; **the kind of project** (a personal Google Cloud project or one inside an organization; consent screen in testing or published; the two client types; whether the second run used one project or two); whether E6 ran from one machine or two; and anything in section 3.1 or 4 that had to be changed to make it work.

## 7. Cost, and what reopens Q4

- **Whose time:** the account holder's, not engineering's. Setting up the account, project, consent screen and two clients is the larger part, and the sender has not done it and cannot time it; the steps themselves are about forty requests and two waits (one minute and ten minutes). **No estimate is given, because none would have a basis.** Half a day is the sender's guess and nothing more.
- **Money:** not known to be any. Whether a Google Cloud project with only the Drive API enabled costs anything, and whether one needs a billing account, was not read.
- **Risk to data:** none if the account is a throwaway and the scope is `drive.appdata` only.
- **What reopens Q4:** the result of this test. A "dead" row of section 5 closes the question: Drive stays backup-only. A "worth drawing up" row, or one of the two conditional rows, is the ground for putting Q4 to the principal again with a design and its cost. Nothing else reopens it, and the test passing does not by itself change anything in the repository.

## 8. Pages read (2026-11-11), with each page's own date

- [Create and manage files](https://developers.google.com/workspace/drive/api/guides/create-file) (last updated 2026-09-04): pre-generated IDs, the 409 on a retry.
- [files.generateIds](https://developers.google.com/workspace/drive/api/reference/rest/v3/files/generateIds) (2025-08-26): `count`, `space` (`drive`, `appDataFolder`), `type`.
- [files resource](https://developers.google.com/workspace/drive/api/reference/rest/v3/files) (2026-07-14): `id`; a name is not necessarily unique.
- [files.create](https://developers.google.com/workspace/drive/api/reference/rest/v3/files/create) (2026-03-20): the metadata and upload addresses.
- [files.get](https://developers.google.com/workspace/drive/api/reference/rest/v3/files/get) (2026-03-20), [files.list](https://developers.google.com/workspace/drive/api/reference/rest/v3/files/list) (2026-07-07): `spaces` supports `appDataFolder`. [files.delete](https://developers.google.com/workspace/drive/api/reference/rest/v3/files/delete) (2026-05-21): "Permanently deletes a file owned by the user without moving it to the trash".
- [Store application-specific data](https://developers.google.com/workspace/drive/api/guides/appdata) (2026-09-03): the folder "is only accessible by your app"; files in it cannot be trashed; the `drive.appdata` scope.
- [Resolve errors](https://developers.google.com/workspace/drive/api/guides/handle-errors) (2026-09-03): the 404 `notFound` body.
- [OAuth 2.0 for Desktop Apps](https://developers.google.com/identity/protocols/oauth2/native-app) (2026-10-30): the authorization, token and revoke addresses; the loopback redirect.

**Not from a page read, and marked so above:** the hand procedure for getting a token (3.1), including whether PKCE is required; the multipart form of 4.8; the parameter of the revoke call (4.9); the console steps for the project and consent screen; whether a project or the Drive API costs anything; the `fields` and `pageSize` parameters in the commands (standard for this API as the sender knows it; `fields` appears in Google's own samples on the pages read).
