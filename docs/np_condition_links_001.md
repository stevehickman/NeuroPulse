# NP-COND-LINK-001 — Condition external-link UI

**Status:** Rev A (2026-07-18)
**Applies to:** iOS, macOS, Android, Windows, Web
**Related:** NP-NPPS-REF-001 (grammar), `protocols/predefined/00-conditions.npps` (registry)

---

## 1. Purpose

`00-conditions.npps` pairs each standard condition name with an external
definition link. Its header states the intent:

> When a user selects a condition, the app offers to open the link in an
> external browser.

This document specifies that interaction and the link-safety policy behind it,
so all five platforms behave identically.

## 2. Why an "offer", not a jump

Condition links leave the app for a third-party site. In a neuromodulation app
the *identity of the condition* is health-adjacent: the destination host, and
any network observer, learns that this user looked up (say) Major Depressive
Disorder. That is exactly the class of signal §5 of the program CLAUDE.md keeps
out of SHDR.

Two consequences, both binding:

1. **Confirm before leaving.** Tapping a condition never navigates directly. It
   opens a confirmation surface naming the destination host, so the user makes
   an informed choice to disclose. This is the "offers to open" the registry
   already specifies.
2. **No telemetry.** Condition selection and link opening emit **no** analytics
   event on any platform, and nothing is written to SHDR. A per-device tally of
   which conditions were viewed would be user-biology-adjacent and therefore
   UHDR-class under the §5.1 boundary rule ("when in doubt → UHDR"). The
   cleanest resolution is to not record it at all.

## 3. Link policy

A condition `link` is openable only if **every** rule holds. The verdict is
computed by a pure function mirrored in each language and pinned by the shared
vectors in `npps/fixtures/condition_links.json`.

Evaluation order, after trimming leading/trailing whitespace:

| # | Rule | Reject reason |
|---|------|---------------|
| 1 | No ASCII control character or space anywhere in the string | `malformed` |
| 2 | An RFC 3986 scheme is present | `not_absolute` |
| 3 | Scheme is exactly `https` (case-insensitive) | `scheme_not_https` |
| 4 | Scheme is followed by `//` (an authority is present) | `not_absolute` |
| 5 | Authority is non-empty | `missing_host` |
| 6 | Authority has no userinfo component (`user:pass@host`) | `embedded_credentials` |
| 7 | Host survives port/bracket stripping as non-empty | `missing_host` |
| 8 | Host is not loopback, link-local, `.local`, RFC1918, or single-label | `local_or_private_host` |

Rules 3–8 matter because the link string arrives from a parsed `.npps` file,
and `.npps` files are user-authorable. Handing an arbitrary string to a platform
URL opener is an injection surface: `javascript:` and `data:` are script
execution, `file:` reads local disk, and a custom scheme can deep-link into
another installed app. Restricting to `https` with a public host closes all of
those. Rule 8 additionally prevents a crafted registry from poking at
`127.0.0.1`-bound local services, and rejects single-label hosts such as
`https://intranet/...`, which resolve through the machine's DNS search suffix
onto the LAN rather than the public internet.

### 3.1 Why the scheme and authority are scanned by hand

The implementations do **not** hand the raw string to the platform URL parser
and then inspect the result. Those parsers normalise aggressively and they do
not agree with one another. `https:///wiki/Stroke` is the worked example:

| Parser | Result |
|--------|--------|
| WHATWG (`new URL`, web) | host `wiki` — a path segment silently promoted to a hostname |
| Foundation `URL` (Apple) | empty host |
| `java.net.URI` (Android) | empty host |

A policy layered on that divergence would be three different policies. So rules
2–7 are a plain string scan — identical steps in all four languages — and the
platform parser is used only after the verdict is settled. The host displayed in
the confirmation UI is the one the scan validated, not one the parser derived,
which is what makes the rule 6 anti-spoofing check meaningful.

A blocked link is shown in the UI as disabled with its reason. It is never
silently dropped — a condition whose link fails policy is still listed, because
the condition itself is legitimate protocol metadata.

## 4. Interaction

Identical on all platforms:

1. Conditions appear as tappable chips on the protocol card / detail view.
2. Selecting a chip opens the confirmation surface (sheet on mobile, dialog on
   desktop/web) showing: condition name, ICD-11 code when present, the
   destination **host**, and a line stating the app is about to leave.
3. **Open** hands the URL to the OS browser. **Cancel** dismisses.
4. Blocked links render the reason instead of an Open action.

The destination shown is the host, not the full URL — the host is the security-
relevant part and a full Wikipedia URL is long enough to push the buttons off a
watch-sized sheet.

## 5. Code sharing

Four languages, so "common" means one behavioral contract, not one binary:

| Layer | Shared how |
|-------|-----------|
| Condition data | One file: `protocols/predefined/00-conditions.npps` |
| Parse result | Shared fixture `npps/fixtures/conditions.npps` + `.expected.json` |
| Link policy | Shared vectors `npps/fixtures/condition_links.json`, consumed by all four test suites |
| Interaction spec | This document |
| View code | Shared within the Apple family only — one SwiftUI view serves iOS and macOS |

Per-platform code is limited to the view and the one-line OS open call:

| Platform | Open call |
|----------|-----------|
| iOS / macOS | SwiftUI `@Environment(\.openURL)` |
| Android | `Intent(ACTION_VIEW)` via `CustomTabsIntent`-free plain intent |
| Windows | `Process.Start` with `UseShellExecute = true` |
| Web | `window.open(url, '_blank', 'noopener,noreferrer')` |

`noopener,noreferrer` on web is required: it drops the `Referer` header, so the
destination does not learn which page the user came from.
