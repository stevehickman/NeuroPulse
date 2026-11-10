/*
 * NeurOne Hub Control — the protocol signature check (see np_proto_verify.h).
 * IEC 62304 Class B — SW-02 hub control.
 */
#include "np_proto_verify.h"

#include <stddef.h>

#include "np_hub_types.h"
#include "monocypher-ed25519.h"     /* the vendored SOUP firmware/crypto links; crypto_ed25519_check, RFC 8032 */

int np_proto_verify_signature(const uint8_t *pubkey, const uint8_t *msg, uint32_t msg_len, const uint8_t *sig)
{
    if (pubkey == NULL || msg == NULL || sig == NULL || msg_len > (uint32_t)NP_HUB_PROTO_BLOB_MAX) {
        return -1;
    }
    /*
     * The all-zero key decodes to a point of order 4; with S = 0 the verification equation then accepts any message
     * (the same refusal np_ed25519_verify makes in firmware/crypto/src/np_crypto.c).
     */
    uint32_t or_bytes = 0U;
    for (uint32_t i = 0U; i < 32U; i++) {
        or_bytes |= (uint32_t)pubkey[i];
    }
    if (or_bytes == 0U) {
        return -1;
    }
    /* crypto_ed25519_check hashes the message through crypto_sha512_update in a fixed-size context, so its stack
     * use does not depend on msg_len (vendor/monocypher/monocypher-ed25519.c, hash_reduce). */
    return crypto_ed25519_check(sig, pubkey, msg, (size_t)msg_len);
}
