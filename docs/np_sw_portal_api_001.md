# Device-Facing Consent Channel — Study Descriptors and Clinician Portal

**Project:** NeurOne
**Document:** NP-SW-PORTAL-API-001
**Revision:** 2
**Date:** 2026-09-14
**Status:** DRAFT
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (new document)
**Revision note:** Rev 2 (2026-09-14) adds §4a — the canonical signing form and the Ed25519 verifier that implements it. No Rev 1 conclusion changed; §4(a)'s key-management finding is unaffected, and `OI-CONSENT-07` stays open for the reason it gave.
**References:** CLAUDE.md §4.6, §5.1, §5.3, §6.0, §6.1, §6.2, §6.3, §17 · NP-CONV-001 · `docs/reference/consent-engine.md` · `docs/reference/data-architecture-detail.md` §5.1/§5.3 · `docs/status/pending-decisions.md` (OI-CONSENT-03, -05, -07)
**Related Issues:** Issue #338; PR #326 (OI-CONSENT-02), PR #329 (OI-CONSENT-03)
**Gate:** None yet — this document is the precondition for one, not a gate itself.
**IEC 62304 Class:** Class B (main processor / app). The channel owns no stimulation enable line; the safety MCU's §4.2 interlocks are unaffected by anything here.
**Supersedes:** None.
**Parent Document:** None.

---

> **⚠ This specifies a channel that does not exist, and it does not build one.**
> `OI-CONSENT-03` and `OI-CONSENT-05` record the same absence at two ends: study invitations
> have no ingestion path, and an access-expansion request can be answered but never received or
> replied to. `OI-CONSENT-07` records the third: the transport itself. `pending-decisions.md`
> observes that the two items "likely share one transport", and §2 below concludes that they do.
>
> **The reason none of them closes by writing an HTTP client is in §4.** The trust anchor is a
> signing key the product does not have yet, and it cannot be delivered over the channel it
> authenticates. Everything else here is downstream of that.

---

## §0 — Scope

**In scope:** what crosses the boundary between a NeurOne app and NeurOne's services for the two
consent channels; what each side may learn from the exchange; and which of those properties are
consequences of locked decisions rather than choices left to the implementer.

**Out of scope:** the services' own internals, the clinician-facing portal UI, storage, the
research suggestion portal of §6.3, and anything the headset does. The headset is not a party here at all — see §1.

## §1 — The device is not the client; the app is

CLAUDE.md §4.6 gives the headset four modes, none of which is "talks to a server": connectivity is
USB-C first with the antennas in the control hub, and Mode 3 runs a full closed loop with no phone
present. Every consent surface — `ConsentStore`, `ConsentEngine`, the dashboards — is app-side on
both platforms.

So "device-facing API" means **app ↔ NeurOne service**. This is not a naming quibble. It is why
§5.3's *all anonymisation happens on-device, in the app, before anything leaves the device* is
implementable at all, and it is what makes the app, not the headset, the last gate on a descriptor.

## §2 — One transport, two payload families

`OI-CONSENT-03` and `OI-CONSENT-05` are different counterparties — NeurOne inviting a subject to a
study, versus a clinician asking to widen a grant they already hold — and `pending-decisions.md`
records that as the reason they are separate items. They are nonetheless one transport, for three
reasons that are properties of the design rather than convenience:

1. **Both are pull-only, and for the same reason** (§3).
2. **Both carry a signed object whose signature, not the transport, is what admits it** (§4).
3. **Both must answer identically whether or not the user has consented to anything** (§5). A
   second transport would have to re-derive that property, and a transport that re-derives it
   differently is a leak.

They remain two payload families on one channel: `study-descriptor` and `clinician-request`. What
differs is the signer and the schema, not the mechanics.

## §3 — Pull-only, unconditional, on a schedule the user's state does not affect

**No push.** A push token is a stable identifier held by NeurOne and addressable to one device.
§5.1 links SHDR to "device ID + opaque TRNG warranty token **only** — never to user identity", and
§6.0 keeps the warranty owner and the user distinct precisely because the registering entity may be
a clinic. A push token for consent traffic would be an identifier that is neither: it addresses the
*user's* app, and NeurOne would hold it. There is no way to hold it that satisfies §5.

**Polling is unconditional.** The app polls on foreground whether or not the user has L1 contact
consent, whether or not any category is selected, and whether or not blanket consent is on. This is
CLAUDE.md §5.1's second general rule applied to a network trace rather than a record: *a redaction
applied conditionally on a sensitive predicate leaks that predicate*. An app that polls only when
the user is contactable tells the server the user is contactable, by the fact of the request. The
filtering is on-device and after receipt, which is where §5.3 already puts it.

**The schedule carries no signal either.** Fixed interval with jitter; no back-off that varies with
what was returned, and no immediate re-poll after a user decision. "Polled again right after we
sent descriptor X" is an acceptance oracle.

## §4 — The trust anchor, and why `OI-CONSENT-07` cannot close without it

§5.3 has studies arriving as **cryptographically signed descriptors**. The signature is what admits
a descriptor; the transport is not. Three consequences, in the order they bind:

**(a) The verifying key ships with the app build, pinned.** It may not be fetched over this
channel. A channel that delivers both the object and the key that authenticates it authenticates
nothing — it proves only that the same party sent both, which is what TLS already proves. This is
the single fact that makes `OI-CONSENT-07` a key-management item rather than a coding item, and it
is why `RefusingStudyDescriptorVerifier` refuses by default rather than accepting and logging: a
transport cannot be wired up without also supplying the check.

**(b) Transport authentication is not descriptor authentication.** TLS with SPKI pinning (the
`SHDRUploader` precedent) protects the *conversation*. It says nothing about whether the study
behind a descriptor was reviewed, and a descriptor that arrives over an authenticated channel from
a compromised service is still a forged descriptor. The signature check runs on the parsed
descriptor, after receipt, on every payload, regardless of how it arrived.

**(c) The clinician channel needs its own anchor, and it is not the same one.** A study descriptor
is signed by NeurOne's study-review key. An expansion request is made by a clinician, whose
authority to ask comes from holding an active grant on *this* device. The device already knows
which grants it issued; the request is therefore bound to a grant ID the device can check locally,
and a request naming a grant the device did not issue is refused without any network opinion being
consulted. What the portal's signature adds is that the *clinician* asked — that the request was
not forged by the transport. Until a clinician-identity key exists, the honest default is the same
one `OI-CONSENT-03` took: refuse.

## §4a — What a signature actually covers, and the one schema decision this document takes

§0 says no endpoint and no schema is specified here, and §7 says why. **The canonical signing form
is the exception, and it has to be**: §5.3 locks that descriptors are *cryptographically signed*,
which is not implementable until something says **what** is signed. A verifier cannot exist without
it, and two platforms cannot agree without it being written down rather than inferred from whichever
serializer each happens to use.

It is specified in code — `StudyDescriptorCanonicalForm` on both platforms — and pinned by its exact
bytes in a test on each, because a form described in prose twice is a form that drifts.

```
NP-STUDY-DESCRIPTOR-V1\n
<byte length>:<studyID>\n
<byte length>:<studyTitle>\n
<byte length>:<researchCategories, in issued order, comma-joined>\n
<byte length>:<requestedElements, sorted, comma-joined>\n
<byte length>:<kAnonymity>\n
<byte length>:<dateRoundingDays>\n
<byte length>:<issued day, YYYY-MM-DD, UTC>\n
```

Four properties are load-bearing, and each is a decision rather than a formatting choice.

**Length-prefixed, not delimited.** `studyTitle` is the one field that is server-supplied text and
stays text, so it can contain any byte including the separator. A delimiter-joined encoding would
let a title carrying that delimiter shift the field boundaries and produce the same bytes as a
different descriptor — and a signature over an ambiguous encoding signs more than one message. Both
test suites carry the forgery attempt this prevents.

**Sets are sorted; lists are not re-ordered.** `requestedElements` is a set with no inherent order,
so the encoding must impose one or the same descriptor signs differently depending on iteration
order. `researchCategories` is a list and keeps the order the issuer sent.

**Categories and elements are named by a stable wire spelling**, not by whatever each platform's
serializer emits. On iOS that is `rawValue`, already the persisted Codable value; on Android it is
now an explicit `wireName` with `displayName` derived from it, so that when localization reaches
`:core` (CLAUDE.md §17) the display side can become a key lookup without moving the wire. Both
suites pin all twenty strings: changing one invalidates every descriptor already issued, which
should be a decision and not a rename.

**Dates are UTC days, not instants** — and this one is a finding, not a preference. iOS models the
issue date as a `Date` and Android as `"yyyy-MM-dd"`, so the coarser representation is the only one
both platforms can produce; anything finer would have one of them computing different bytes for the
same descriptor and rejecting every signature. The consequence is worth stating rather than
discovering: **time of day is not covered by the signature**, so an issuer must not rely on it to
distinguish two descriptors. `studyID` is what does that. The day is rendered in UTC on a fixed
Gregorian calendar, so a descriptor cannot verify in one time zone and fail in another.

**The audit-trail hash digests the signed bytes**, not the parsed object: `sha256:<hex>` over
exactly the sequence above. Anything else would record a hash of the device's *reading* of a
descriptor rather than of the descriptor, which is not what §5.3's audit trail is for.

**Algorithm: Ed25519, and one key format.** Deterministic signatures — no per-signature entropy for
the signing side to get wrong — small keys, and available on the JDK since 15, which matters because
Android's `:core` is pure-JVM by design and may not reach for an Android provider. The key is the
**raw 32 bytes** on both platforms, even though the JDK's `KeyFactory` wants SubjectPublicKeyInfo:
if each side accepted what its own library prefers, "the NeurOne study-signing key" would be two
different artifacts and a build could ship the wrong one to one platform. The DER wrapper is a fixed
12-byte prefix for this algorithm, so that conversion belongs in the verifier, not in whoever holds
the key.

**None of this closes `OI-CONSENT-07`.** `Ed25519StudyDescriptorVerifier` cannot be constructed
without a public key and there is no key to construct it with, so
`RefusingStudyDescriptorVerifier` remains what `ConsentStore` defaults to — see §4(a), which is the
reason. A key that is absent, malformed or the wrong length yields **no verifier at all** rather
than one that throws at the first descriptor, so a misconfigured build falls back to refusing.
What exists now is that the canonical form is executable, the two platforms are pinned against each
other, and whoever eventually supplies the key is supplying only a key.

## §5 — Fixed response shapes

§5.1's rule 2 governs the wire as it governs a record: a response whose *shape* varies with a
sensitive predicate leaks the predicate, and the "no such user" versus "wrong password" failure
shape is the canonical case.

- **One response schema per payload family**, populated identically whether the app will end up
  showing the object to the user or discarding it on the floor. The service does not learn which.
- **No acknowledgement of receipt that is conditional on the user's state.** The app acknowledges
  *delivery*, never *admission*: "received descriptor X" is fine and is needed for retry; "admitted
  descriptor X" is a consent oracle, and "refused descriptor X for integrity" is the same oracle by
  complement, because the service knows what it sent.
- **No per-refusal reason reaches the service.** `ConsentEngine.admit()`'s refusal reasons are
  legible on-device and in a test, and stay there.
- **The SHDR audit trail is unchanged by this document.** §5.3 already logs study ID, descriptor
  hash, transmission timestamp and byte count, and already says they are never shared with
  researchers.

## §6 — The outbound direction, and the one thing NeurOne must not read

§6.1's third response to an expansion request is *asks questions*. The question is free text the
user wrote about their own care, addressed to their clinician. By §5.1's defining test — *does this
record tell us something about the person?* — it is UHDR, and §5 puts it permanently beyond
NeurOne's reach.

It therefore cannot be relayed as plaintext through a NeurOne service, which is the obvious
implementation and the wrong one. The question must be end-to-end encrypted to the clinician, with
NeurOne carrying ciphertext and routing metadata only, or the feature must not ship. Both are
acceptable; relaying plaintext is not. **This is an open decision, not a specification** — it needs
a clinician key distribution story, which is the clinician-identity anchor of §4(c) again, from the
other side.

The approve / deny answers are different: they are facts about a grant NeurOne's *clinician* holds,
and they carry a grant ID rather than anything about the user's biology. They may be sent as-is.

## §7 — What this leaves open

| Item | What is still needed | Owner |
|------|---------------------|-------|
| `OI-CONSENT-07` | The study-review signing key: who holds it, how it is rotated, and how it reaches an app build. §4(a). The verification side is built (§4a); the key is not. | App Lead + Security |
| `OI-CONSENT-05` (inbound) | A clinician-identity anchor so a request can be shown to have come from the clinician who holds the grant. §4(c). | App Lead + Security |
| `OI-CONSENT-05` (outbound) | E2E encryption of the user's question to the clinician, or a decision not to ship the question path. §6. | App Lead |
| Endpoint + schema | Deliberately not specified here. Writing a URL and a JSON body before §4 is settled would make the unsolved part look solved, which is the failure `RefusingStudyDescriptorVerifier` exists to prevent. | — |

## §8 — What stands in for the channel until it ships

Both platforms carry a **refusing port** rather than an absence:

- `StudyDescriptorVerifier` / `RefusingStudyDescriptorVerifier` (`OI-CONSENT-03`) — no descriptor is
  admitted, and the missing signing key is the reason given. `Ed25519StudyDescriptorVerifier` is the
  implementation waiting behind it (§4a); it is not the default and cannot be built without a key.
- `ClinicianPortalChannel` / `RefusingClinicianPortalChannel` (this document, §4(c)) — no expansion
  request is ingested, and the missing clinician-identity anchor is the reason given.

A port with a refusing default is not a stub. It is the shape that makes the gap impossible to
close by accident: the type system requires an implementation before a transport can exist, and the
default one refuses, so a half-built channel fails closed rather than admitting whatever arrives.

The decision surfaces the user reaches — approve, deny, ask, accept, decline, withdraw — are
reachable and exercised from the dashboards today, and `scripts/check-consent-reachability.ts`
holds that. What is missing is only the thing that puts an object in front of them.
