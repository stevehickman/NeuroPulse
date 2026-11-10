/*
 * NeurOne Hub Control — the protocol signature check (NP-FW-HUB-001 §4.4).
 *
 * One definition and one declaration for the check np_protocol_verify_and_parse() makes, so the call and the
 * function cannot drift apart the way the earlier separate `extern` in np_protocol.c did (OI-SIMFW-42).
 *
 * IEC 62304 Class B — SW-02 hub control.
 */
#ifndef NP_PROTO_VERIFY_H
#define NP_PROTO_VERIFY_H

#include <stdint.h>

/*
 * Verifies an Ed25519 signature (RFC 8032) over a protocol blob's signed bytes.
 *
 *   pubkey   — the 32-byte protocol trust anchor
 *   msg      — the signed bytes (the blob without its trailing signature)
 *   msg_len  — at most NP_HUB_PROTO_BLOB_MAX
 *   sig      — the 64-byte signature
 *
 * Returns 0 for a valid signature, -1 for anything else (an invalid signature, an all-zero key, a message over the
 * bound).  It does not use np_ed25519_verify of firmware/crypto: that function refuses a message over
 * NP_CRYPTO_ED25519_MSG_MAX_LEN (256 bytes), which a protocol does not fit within, and it is shared with the safety MCU
 * and the bootloader (IEC 62304 Class C), so its limit is not changed for the hub.  Both call the same vendored
 * crypto_ed25519_check.
 */
int np_proto_verify_signature(const uint8_t *pubkey, const uint8_t *msg, uint32_t msg_len, const uint8_t *sig);

#endif /* NP_PROTO_VERIFY_H */
