# PHC Argon2 Reference Library — Vendor Record

## Source

- **Repository:** https://github.com/P-H-C/phc-winner-argon2
- **Upstream commit:** vendored from reference implementation (2021 release)
- **License:** CC0 1.0 Universal / Apache 2.0 (dual-licensed) — user's choice
- **SOUP entry:** NP-SW-001 §7, SOUP table row "phc-winner-argon2"

## NeuroPulse usage

Provides `argon2id_hash_raw()` called via `np_bridge.c` → `np_argon2id_hash()`.
Parameters fixed in `Argon2Bridge.swift`: m=65536 KiB, t=4, p=1, output=64 bytes.
Single-threaded build (`ARGON2_NO_THREADS` defined in Package.swift cSettings).

## Files vendored

| File | SHA-256 |
|------|---------|
| include/argon2.h | 25ed629feca91ca9d361441160c6fbc10318bb0fb3757555b418ed47b705b35b |
| src/argon2.c | b1289ec7134e8502e9113396fdac89402bf2575ee1b35e33fb7410f2fb63bb6d |
| src/core.h | 32f6ab8c0c313d9336d2731a001426b68d246bba5b362fabeaf593c333da7d37 |
| src/core.c | bb5d67ff26d2ff44fd4d9be69691616209062ee523a30cb3a4ce8a4565804d5d |
| src/ref.c | 9ac347fd8dc737af69bbb93d56ac8b4ab5488152f606880c8d7fc4592e207647 |
| src/encoding.h | a4e0681ef4b0eb229a35760b603b7a32e9019cfe98c31732f747f087e5e39828 |
| src/encoding.c | d415edb6deddd5d33b0f4e44842f7c5ace194beaab59d71fbf1af2e8df733254 |
| src/thread.h | 650e713fb584de2e6aeb307e64228f95cef733ea667faa0bb111960aaace30ef |
| src/blake2/blake2.h | e8abb959602fb17d01c3161ae397ba374a4791d12baed5dffd447e12be0153f8 |
| src/blake2/blake2-impl.h | 866ad32f6beae712564162e8db0fce5dd928347dd06f11169539461f91424daa |
| src/blake2/blake2b.c | 1b392a6e671e999cdaa9e2a7707748ddeb4d1e98760a1749a300846561036d22 |
| src/blake2/blamka-round-ref.h | 1fa844646049e4fd2782b7c4e03dac85f5823f040bcacf0d1431556c43bb6f22 |

## Local modifications

1. `src/core.c` — added explanatory comment in `secure_wipe_memory()` volatile-pointer fallback
   confirming it is the active path on Apple/clang (Cato CR-004).

No functional changes to upstream code.
