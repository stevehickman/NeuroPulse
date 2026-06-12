#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Argon2id key derivation — RFC 9106 compliant, parallelism=1 only.
// Returns 0 on success, negative on error.
int np_argon2id_hash(
    const uint8_t *password, uint32_t password_len,
    const uint8_t *salt,     uint32_t salt_len,
    uint32_t memory_kib,
    uint32_t iterations,
    uint8_t  *output,
    uint32_t  output_len
);

// Configurable memory/time for unit tests (avoids 64 MB in CI).
// The *implementation* is only compiled in debug builds (NP_ARGON2_CUSTOM_ENABLED
// in np_bridge.c, set via Package.swift cSettings .when(configuration: .debug)).
// The *declaration* is unconditional so Swift's clang importer can see it;
// Swift callers must gate usage with #if DEBUG. In release builds Swift never
// emits a call (the #if DEBUG body is stripped), so there is no linker
// reference to the absent symbol.
int np_argon2id_hash_custom(
    const uint8_t *password, uint32_t password_len,
    const uint8_t *salt,     uint32_t salt_len,
    uint32_t memory_kib,
    uint32_t iterations,
    uint8_t  *output,
    uint32_t  output_len
);

#ifdef __cplusplus
}
#endif
