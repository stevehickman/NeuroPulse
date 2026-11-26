# Simulator Page SOUP Inventory — Three.js and Fonts (Helmet Simulator)

**Project:** NeurOne
**Document:** NP-SOUP-SIM-001
**Revision:** 1
**Date:** 2026-11-26
**Status:** DRAFT
**Effective Date:** —
**Author:** NeurOne Software Engineering
**Approved By:** — (DRAFT, not approved)
**References:** `simulator/vendor/VENDOR.md` (the sources and each file's SHA-256), `simulator/index.html` (the importmap and the policy), `NP-SIM-FW-001` §35 and `REQ-SIMFW-27`, `NP-SOUP-NATIVE-001` and `NP-SOUP-KOTLIN-001` (the method precedent; §2a of the latter for the selection rules), `CLAUDE.md` §22
**Related Issues:** `OI-PRIV-44` (`docs/status/pending-decisions.md`), GitHub #974 (the privacy review of `simulator/`, Finding S4)
**Gate:** N/A — the simulator is a development and demonstration tool, not part of the device
**IEC 62304 Class:** N/A — the page is never linked into a device image (`NP-SIM-FW-001` `REQ-SIMFW-01`) and is never verification evidence (`REQ-SIMFW-06`)
**Supersedes:** None
**Change Summary:** Rev 1 (2026-11-26) — first issue: Three.js 0.164.1 with OrbitControls, Inter 4.001 and JetBrains Mono 2.211, vendored into `simulator/vendor/` from the CDN loads they replace (`OI-PRIV-44`).
**Review Cadence:** On any change to a file under `simulator/vendor/`

---

## 1. What this record is, and is not

The helmet simulator's page (`simulator/index.html`) is plain JavaScript modules written by NeurOne, plus the four
third-party items below. Until 2026-11-26 the page fetched them from `cdn.jsdelivr.net` and `fonts.googleapis.com` on
every load; they are now committed in `simulator/vendor/` and served with the page (`OI-PRIV-44`, `NP-SIM-FW-001` §35).
This record **lists** them and says what has and has not been checked. The NPPS core the page runs as WebAssembly is
`NP-SOUP-RUST-001`; the bridge server's npm dependency (`ws`, `simulator/server/package.json`) is not page code and is
not covered here.

## 2. Inventory

| Item | Version | Files | Shipped with the page | Licence | Role |
|------|---------|-------|-----------------------|---------|------|
| Three.js (`three`) | **0.164.1**, the npm tarball (registry `dist.shasum` matched) | `three/build/three.module.js` | Yes | MIT (`three/LICENSE`, the package's own file) | The 3D helmet view (`simulator/js/app.js`, `simulator/js/helmet.js`) |
| Three.js OrbitControls | with Three.js 0.164.1 | `three/examples/jsm/controls/OrbitControls.js` | Yes | MIT (same package) | Camera orbit by mouse and touch (`simulator/js/app.js`) |
| Inter | **4.001** (`git-66647c0bb`), Google Fonts `v20` | `fonts/Inter-*.woff2` (7 subsets) | Yes | SIL OFL 1.1 (`fonts/OFL-Inter.txt`) | UI text |
| JetBrains Mono | **2.211**, Google Fonts `v24` | `fonts/JetBrainsMono-*.woff2` (6 subsets) | Yes | SIL OFL 1.1 (`fonts/OFL-JetBrainsMono.txt`) | Metrics and timer readouts |

The versions are the ones the page was loading: Three.js was pinned at 0.164.1 in the importmap; the fonts are what
Google served for the page's own request on 2026-11-26, the version read from each file's name table. The files are
byte for byte what was served, renamed by subset; `fonts/fonts.css`, the stylesheet that names them, is NeurOne's
(Google's stylesheet with each `url()` made local). Each file's SHA-256 is in `simulator/vendor/VENDOR.md` and is
checked by `simulator/js/page-policy.test.mjs` (`REQ-SIMFW-27`#d).

**Licences.** MIT and the SIL Open Font License 1.1 are permissive: neither obliges NeurOne's own code to be copyleft
(`CLAUDE.md` §22; `NP-SOUP-KOTLIN-001` §2a, rule 5). The OFL's conditions bind the fonts themselves (they may be
bundled and redistributed with software, are not sold on their own, and a modified font may not use the reserved
name); the files are unmodified and their licence texts travel with them. The MIT notice travels in `three/LICENSE`.
The licences were read from the package's own file and from `google/fonts` (`ofl/inter/OFL.txt`,
`ofl/jetbrainsmono/OFL.txt`); they are **declared**, not checked by counsel.

## 3. Selection rules applied

1. **No unused library.** Each item is imported or named by the page's stylesheet. Only the two Three.js files the page
   imports are vendored, not the package.
2. **No pre-release.** 0.164.1 is a release; the fonts are Google Fonts' released builds.
3. **Cryptographic and real-time work is the core's** (`CLAUDE.md` §22). None of these does either.
4. **No managed-runtime code on the helmet.** The page runs in a browser on a developer's or presenter's computer.
5. **Copyleft only if it does not reach NeurOne's code.** None is copyleft (§2).

## 4. What was not done

- **No vulnerability scan.** No scan job of `NP-SOUP-KOTLIN-001` §6.2 reads `simulator/vendor/`. Three.js's public
  advisories were not read.
- **No anomaly list** for Three.js 0.164.1 beyond its release, which is not current; it is kept at the version the page
  used, and moving it is a change of its own.
- **No check against upstream** beyond the npm tarball's registry checksum for Three.js. The font files were taken from
  Google's service, not built from the projects' sources.

## 5. Hazard reading

| Item | Fault | Effect | Held by |
|------|-------|--------|---------|
| Three.js, OrbitControls | A defect, or a changed copy, runs with the page's access | The page's engine, `window.__sim` and its WebSocket to the bridge are reachable to that code; in firmware mode the bridge accepts fault injection (`OI-SIMFW-07`) | The copy is fixed in the repository with its hash (`REQ-SIMFW-27`#d); the CSP refuses any script not from the page's origin (`REQ-SIMFW-27`#b). Not held: a defect in the vendored code itself. The page is a development tool and drives no hardware |
| Inter, JetBrains Mono | A malformed font | A rendering fault in the browser | The browser's font sanitiser; the files are fixed by hash |
| all | A load from a third party | A viewer's network address and the page's URL reach that service | Removed: nothing is fetched from a third party (`REQ-SIMFW-27`#a) |

## 6. Keeping this record true

- A change to a file under `simulator/vendor/` changes `VENDOR.md` and §2 in the same change, and for Three.js the
  importmap and its hash in the CSP of `simulator/index.html`.
- A new third-party item the page uses is vendored and added here; it is never loaded from a CDN (`REQ-SIMFW-27`).
