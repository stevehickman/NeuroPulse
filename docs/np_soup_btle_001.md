# btleplug SOUP Inventory — Desktop Hub Link (Companion App)

**Project:** NeurOne
**Document:** NP-SOUP-BTLE-001
**Revision:** 1
**Date:** 2026-10-08
**Status:** DRAFT
**Effective Date:** —
**Author:** NeurOne Software Engineering
**Approved By:** — (DRAFT, not approved)
**References:** `common/Cargo.lock` (the pin), `common/btle-jni/Cargo.toml` (the direct dependencies), `NP-SOUP-LFS-001` and `NP-SOUP-CMSIS-001` (method precedent), `NP-SW-001` §9.4 (the firmware SOUP register, which this record does not extend)
**Related Issues:** `OI-UI-KMP-03` item (4) (`docs/status/pending-decisions.md`)
**Gate:** G2
**IEC 62304 Class:** Not assigned. See §3.
**Supersedes:** None
**Change Summary:** Rev 1 (2026-10-08) — initial revision. The inventory of the Rust crates that `neurone-btle-jni` links, with licences. **The anomaly evaluation IEC 62304 §8.1.2 asks for has not been done.**
**Review Cadence:** On any change to `common/Cargo.lock` that touches the tree below, and at G2

---

## 1. What this record is, and is not

`neurone-btle-jni` (`common/btle-jni`) is the desktop Bluetooth LE central of the shared app (macOS, Windows, Linux). It
is btleplug behind JNI. Everything it links that NeurOne did not write is SOUP. This record **lists** that SOUP. It is the
inventory half of the work and no more.

**Not done:** the evaluation of each component's published anomaly list for anomalies that could lead to a hazardous
situation, which `NP-SOUP-LFS-001` and `NP-SOUP-CMSIS-001` perform for firmware components. It is raised as
`OI-UI-KMP-03` item (4), still open. Writing "no known hazardous anomaly" here would claim a check nobody ran.

## 2. Direct dependencies

| Crate | Version | Role | Licence |
|-------|---------|------|---------|
| btleplug | 0.12.0 | The BLE central. Reaches CoreBluetooth (macOS), WinRT (Windows) and BlueZ over D-Bus (Linux). | MIT / Apache-2.0 / BSD-3-Clause |
| jni | 0.21.1 | The JNI surface. | MIT OR Apache-2.0 |
| tokio | 1.53.2 | The runtime that carries the command queue. | MIT |
| futures | 0.3.34 | Stream handling over btleplug's event stream. | MIT OR Apache-2.0 |
| uuid | 1.27.0 | Characteristic identifiers. | MIT OR Apache-2.0 |

The first-party crate links no other dependency. The platform libraries each backend reaches (CoreBluetooth, WinRT,
`libdbus-1`) belong to the operating system and are not SOUP of this record.

## 3. Class

No IEC 62304 class is assigned. The crate is on the companion-app link, not on the hub, and the hub's safety MCU owns
every stimulation enable line (CLAUDE.md §4.2), so a fault here cannot enable stimulation. What a fault *can* do is deliver
a protocol upload out of order or truncated, which the hub rejects by signature, or show wrong session state. Assigning a
class is a software-safety-classification decision for the companion app and is left to that decision.

## 4. Transitive inventory

Every crate in `common/Cargo.lock` reachable from `neurone-btle-jni` through normal (non-dev, non-build) dependencies, over
all targets: 98 crates. Generated with `cargo metadata --locked`. A crate appears once even where several
platforms need it, so the Linux build links fewer than the list.

| Crate | Version | Licence (as declared) |
|-------|---------|-----------------------|
| async-trait | 0.1.92 | MIT OR Apache-2.0 |
| bitflags | 2.13.2 | MIT OR Apache-2.0 |
| block2 | 0.5.1 | MIT |
| bluez-async | 0.8.2 | MIT OR Apache-2.0 |
| bluez-generated | 0.4.0 | MIT OR Apache-2.0 |
| btleplug | 0.12.0 | MIT/Apache-2.0/BSD-3-Clause |
| bumpalo | 3.20.3 | MIT OR Apache-2.0 |
| bytes | 1.12.1 | MIT |
| cesu8 | 1.1.0 | Apache-2.0/MIT |
| cfg-if | 1.0.5 | MIT OR Apache-2.0 |
| combine | 4.6.8 | MIT |
| crossbeam-utils | 0.8.23 | MIT OR Apache-2.0 |
| dashmap | 6.2.1 | MIT |
| dbus | 0.9.12 | Apache-2.0/MIT |
| dbus-tokio | 0.7.6 | Apache-2.0/MIT |
| either | 1.19.0 | MIT OR Apache-2.0 |
| futures | 0.3.34 | MIT OR Apache-2.0 |
| futures-channel | 0.3.34 | MIT OR Apache-2.0 |
| futures-core | 0.3.34 | MIT OR Apache-2.0 |
| futures-executor | 0.3.34 | MIT OR Apache-2.0 |
| futures-io | 0.3.34 | MIT OR Apache-2.0 |
| futures-macro | 0.3.34 | MIT OR Apache-2.0 |
| futures-sink | 0.3.34 | MIT OR Apache-2.0 |
| futures-task | 0.3.34 | MIT OR Apache-2.0 |
| futures-util | 0.3.34 | MIT OR Apache-2.0 |
| hashbrown | 0.14.5 | MIT OR Apache-2.0 |
| itertools | 0.14.0 | MIT OR Apache-2.0 |
| jni | 0.19.0 | MIT/Apache-2.0 |
| jni | 0.21.1 | MIT/Apache-2.0 |
| jni-sys | 0.3.1 | MIT OR Apache-2.0 |
| jni-sys | 0.4.1 | MIT OR Apache-2.0 |
| jni-sys-macros | 0.4.1 | MIT OR Apache-2.0 |
| js-sys | 0.3.106 | MIT OR Apache-2.0 |
| libc | 0.2.190 | MIT OR Apache-2.0 |
| libdbus-sys | 0.2.7 | Apache-2.0/MIT |
| lock_api | 0.4.14 | MIT OR Apache-2.0 |
| log | 0.4.34 | MIT OR Apache-2.0 |
| memchr | 2.8.3 | Unlicense OR MIT |
| mio | 1.2.4 | MIT |
| objc-sys | 0.3.5 | MIT |
| objc2 | 0.5.3 | MIT |
| objc2-core-bluetooth | 0.2.2 | MIT |
| objc2-encode | 4.1.0 | MIT |
| objc2-foundation | 0.2.2 | MIT |
| once_cell | 1.21.4 | MIT OR Apache-2.0 |
| parking_lot_core | 0.9.12 | MIT OR Apache-2.0 |
| pin-project-lite | 0.2.17 | Apache-2.0 OR MIT |
| proc-macro2 | 1.0.107 | MIT OR Apache-2.0 |
| quote | 1.0.47 | MIT OR Apache-2.0 |
| redox_syscall | 0.5.18 | MIT |
| scopeguard | 1.2.0 | MIT OR Apache-2.0 |
| serde | 1.0.229 | MIT OR Apache-2.0 |
| serde-xml-rs | 0.8.2 | MIT |
| serde_core | 1.0.229 | MIT OR Apache-2.0 |
| serde_derive | 1.0.229 | MIT OR Apache-2.0 |
| slab | 0.4.12 | MIT |
| smallvec | 1.16.2 | MIT OR Apache-2.0 |
| socket2 | 0.6.5 | MIT OR Apache-2.0 |
| static_assertions | 1.1.0 | MIT OR Apache-2.0 |
| syn | 2.0.119 | MIT OR Apache-2.0 |
| syn | 3.0.6 | MIT OR Apache-2.0 |
| thiserror | 1.0.69 | MIT OR Apache-2.0 |
| thiserror | 2.0.21 | MIT OR Apache-2.0 |
| thiserror-impl | 1.0.69 | MIT OR Apache-2.0 |
| thiserror-impl | 2.0.21 | MIT OR Apache-2.0 |
| tokio | 1.53.2 | MIT |
| tokio-macros | 2.7.2 | MIT |
| tokio-stream | 0.1.19 | MIT |
| tokio-util | 0.7.19 | MIT |
| unicode-ident | 1.0.26 | (MIT OR Apache-2.0) AND Unicode-3.0 |
| uuid | 1.27.0 | Apache-2.0 OR MIT |
| wasi | 0.11.1+wasi-snapshot-preview1 | Apache-2.0 WITH LLVM-exception OR Apache-2.0 OR MIT |
| wasm-bindgen | 0.2.129 | MIT OR Apache-2.0 |
| wasm-bindgen-macro | 0.2.129 | MIT OR Apache-2.0 |
| wasm-bindgen-macro-support | 0.2.129 | MIT OR Apache-2.0 |
| wasm-bindgen-shared | 0.2.129 | MIT OR Apache-2.0 |
| windows | 0.62.2 | MIT OR Apache-2.0 |
| windows-collections | 0.3.2 | MIT OR Apache-2.0 |
| windows-core | 0.62.2 | MIT OR Apache-2.0 |
| windows-future | 0.3.2 | MIT OR Apache-2.0 |
| windows-implement | 0.60.2 | MIT OR Apache-2.0 |
| windows-interface | 0.59.3 | MIT OR Apache-2.0 |
| windows-link | 0.2.1 | MIT OR Apache-2.0 |
| windows-numerics | 0.3.1 | MIT OR Apache-2.0 |
| windows-result | 0.4.1 | MIT OR Apache-2.0 |
| windows-strings | 0.5.1 | MIT OR Apache-2.0 |
| windows-sys | 0.45.0 | MIT OR Apache-2.0 |
| windows-sys | 0.61.2 | MIT OR Apache-2.0 |
| windows-targets | 0.42.2 | MIT OR Apache-2.0 |
| windows-threading | 0.2.1 | MIT OR Apache-2.0 |
| windows_aarch64_gnullvm | 0.42.2 | MIT OR Apache-2.0 |
| windows_aarch64_msvc | 0.42.2 | MIT OR Apache-2.0 |
| windows_i686_gnu | 0.42.2 | MIT OR Apache-2.0 |
| windows_i686_msvc | 0.42.2 | MIT OR Apache-2.0 |
| windows_x86_64_gnu | 0.42.2 | MIT OR Apache-2.0 |
| windows_x86_64_gnullvm | 0.42.2 | MIT OR Apache-2.0 |
| windows_x86_64_msvc | 0.42.2 | MIT OR Apache-2.0 |
| xml | 1.4.0 | MIT |

**Licences.** Every declared licence is permissive (MIT, Apache-2.0, BSD-3-Clause, Unlicense, Unicode-3.0). None is
copyleft. A declared
licence is a crate's own claim and is not checked against its source here.

## 5. Open

1. The anomaly evaluation (§1), per component, against each upstream's published issue list and advisories (RustSec for
   the crates, the project trackers for btleplug).
2. The software safety class (§3).
3. Build-script and procedural-macro crates are SOUP at build time only and are outside the normal-dependency tree above.
4. Registering this record in the companion-app SOUP list, which does not exist yet.
