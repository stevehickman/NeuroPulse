# Question for the principal: OI-PRIV-68, a forged high count can stop a person's moves from retiring their old device

**Status:** ANSWERED 2026-11-26: option B, recommendation accepted (the principal). Originally DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** #1085
**From:** NeurOne Software Engineering (`OI-PRIV-68` in `docs/status/pending-decisions.md`; found 2026-11-25 while building the device's catch-up, `SWR-MG-35`)
**To:** the principal (the Privacy Lead and a cryptography reviewer may advise)
**Response needed:** an option, a short rationale, anything that would reopen it, and your name, role and the date.

## Answer

**Option B** (the principal, 2026-11-26): each notice carries the public key, and the server verifies before it stores,
keeping the highest verified count per key id. Rationale: the recommendation as written in §3, accepted. Recorded in
`NP-SW-PORTAL-API-001` §6a (call 4), `data-architecture-detail.md` §5.3 step 1a and `OI-PRIV-68`. Nothing stated would
reopen it beyond what §3 lists as unsolved (a flood of notices for fresh keys).

## 1. The question

Your ruling of 2026-11-21 on notice order and retention says the server keeps **only the highest-count notice per key
id**, never expires one, and drops a notice that a higher count replaces. Your ruling on junk notices, the same day, says
the server **never drops a notice it holds**, so that a real notice is never lost.

The two rules meet badly. The server holds no key and cannot tell a real notice from junk. **Anyone who knows a person's
key id can send a junk notice for it with a count at the top of the range.** The server keeps it, as the highest, and
drops the person's real notice. Every later real move by that person has a lower count and is dropped too. Their old
device verifies nothing it is sent, so it never retires, and it stays current for that person, extracting for someone
it no longer serves.

Key ids are not secret. Every notice is broadcast to every subscribed device, so anyone who runs the app sees each key
id that has ever moved. Only a person who has moved at least once is exposed, and only from the moment their key id is
broadcast.

Plain form: **which notices per key id should the server keep, so that a notice nobody can verify cannot displace one
that is real?**

## 2. What is known

- **Devices are safe from forgery and stay safe.** A device checks every notice against the key it holds
  (`SWR-MG-34` (c)), so a junk notice retires no one. The harm is suppression: a real notice that never arrives.
- **The device's half is built and does not depend on this answer.** `supersededCatchUp` (`SWR-MG-35`) takes the
  highest count that verifies among everything in a page, so it works whether the server keeps one notice per key id
  or several.
- **The server's store is not built.** Nothing has to be undone.
- **What each notice carries today:** key id, count, signature (`NP-SW-PORTAL-API-001` §6a). The key id is the SHA-256
  of the person's current-device public key. The public key itself is not sent.

## 3. Options

**A. The server keeps every notice per key id, and drops none.** The device takes the highest count that verifies, so
junk with a high count no longer hides a real one. *Cost:* storage per key id is no longer bounded by one; a flood
aimed at one key id is bounded only by the store's total and the relay's per-address limit you already ruled; every
device fetches the junk.

**B. Each notice also carries the public key, and the server verifies before it stores (recommended).** The server
checks that the key id is the SHA-256 of the key sent and that the signature is valid under the strict check, the same
code the devices run (`common/npps-core`, `CLAUDE.md` §22), then keeps the highest verified count per key id. A junk
notice for a real key id fails, because only the holder of the person's private key can sign one. *What the server
learns:* nothing it does not already have. It already keeps a record per key id, and the key id is a hash of the
public key, so the public key links no more than the key id does. *What it does not solve:* a flood of notices for
fresh keys made for the purpose; those verify, and stay bounded by the relay limit and the store's total you already
ruled. *Change:* 32 bytes on each notice; the signed form `NP-SUPERSEDED-V1` is unchanged. Your earlier objection to
the server holding keys was that it would bring back a per-person record; here the server keeps no record it does not
keep already.

**C. Keep the rule as ruled and accept the gap.** *Cost:* anyone can suppress any moved person's later moves, and the
ruling that a real notice is never lost does not hold.

## 4. What is assumed

- That the relay's per-address limit on `supersede` (ruled 2026-11-21) does not stop this, because the attack needs one
  call per person attacked.
- That a person whose moves are suppressed has no way to notice: their old device gives no sign, and the new one sees
  its own notice sent.

## 5. What to send back

The option (A, B or C, or another), a short rationale, what would reopen it, and your name, role and the date. Until it
is answered, `NP-SW-PORTAL-API-001` §6a says nothing about which notices per key id the server keeps, and the server's
store is not built.
