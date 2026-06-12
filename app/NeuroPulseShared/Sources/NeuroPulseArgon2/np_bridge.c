/*
 * Thin adapter: wraps PHC argon2id_hash_raw() into the np_argon2id_hash() API
 * used by NeuroPulseShared/Argon2Bridge.swift.
 *
 * PHC reference library: https://github.com/P-H-C/phc-winner-argon2
 * License: CC0 1.0 / Apache 2.0
 */
#include "argon2.h"    /* PHC public API (found via phc/include search path) */
#include "np_argon2.h" /* Our public API (publicHeadersPath: "include") */
#include <stdint.h>

int np_argon2id_hash(
    const uint8_t *password, uint32_t password_len,
    const uint8_t *salt,     uint32_t salt_len,
    uint32_t memory_kib,
    uint32_t iterations,
    uint8_t  *output,
    uint32_t  output_len)
{
    return argon2id_hash_raw(
        (uint32_t)iterations,
        (uint32_t)memory_kib,
        1,   /* parallelism = 1 */
        password, (size_t)password_len,
        salt, (size_t)salt_len,
        output, (size_t)output_len
    );
}

/* Only compiled in debug builds (Package.swift cSettings .when(configuration: .debug)).
   Swift caller mirrors this guard with #if DEBUG in Argon2Bridge.swift.
   Never present in release binaries. */
#ifdef NP_ARGON2_CUSTOM_ENABLED
int np_argon2id_hash_custom(
    const uint8_t *password, uint32_t password_len,
    const uint8_t *salt,     uint32_t salt_len,
    uint32_t memory_kib,
    uint32_t iterations,
    uint8_t  *output,
    uint32_t  output_len)
{
    return argon2id_hash_raw(
        (uint32_t)iterations,
        (uint32_t)memory_kib,
        1,   /* parallelism = 1 */
        password, (size_t)password_len,
        salt, (size_t)salt_len,
        output, (size_t)output_len
    );
}
#endif /* NP_ARGON2_CUSTOM_ENABLED */
