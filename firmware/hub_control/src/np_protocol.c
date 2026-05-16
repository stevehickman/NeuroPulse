/*
 * NeuroPulse Hub Control Program — Protocol Parser and Verifier
 * Document: NP-FW-HUB-001 Rev A §4
 *
 * Ed25519 verification reuses the self-contained verifier from
 * firmware/bootloader/src/np_signature.c (same key derivation; different
 * public key stored in Config partition as NP_CFG_PROTO_PUB_KEY).
 *
 * HAL stub OI-PROTO-01: np_proto_hal_get_device_serial(buf, NP_HUB_PROTO_SERIAL_LEN)
 *   Reads the device serial from Config partition.
 * HAL stub OI-PROTO-02: np_proto_hal_get_proto_pubkey(buf, 32)
 *   Reads the 32-byte Ed25519 public key used to verify session protocols.
 */

#include "np_protocol.h"
#include <string.h>

/* ── HAL stubs ────────────────────────────────────────────────────────────────── */

extern np_hub_status_t np_proto_hal_get_device_serial(uint8_t *buf, size_t len);
extern np_hub_status_t np_proto_hal_get_proto_pubkey(uint8_t *pub_key_out); /* 32 bytes */

/* Ed25519 verify — same API as bootloader np_signature.c */
extern int np_ed25519_verify(const uint8_t *msg, size_t msg_len,
                              const uint8_t *sig,
                              const uint8_t *pub_key);

/* ── Internal helpers ─────────────────────────────────────────────────────────── */

/*
 * Walk the command body and return total byte count consumed by cmd_count commands,
 * or 0 if the body is malformed (params_len exceeds max or body is too short).
 */
static size_t measure_cmd_body(const uint8_t *body, size_t body_len,
                                uint8_t cmd_count)
{
    size_t offset = 0U;

    for (uint8_t i = 0U; i < cmd_count; i++) {
        if (offset + sizeof(np_proto_cmd_hdr_t) > body_len) {
            return 0U;
        }
        const np_proto_cmd_hdr_t *hdr = (const np_proto_cmd_hdr_t *)(body + offset);
        if (hdr->params_len > NP_HUB_PROTO_PARAMS_MAX) {
            return 0U;
        }
        offset += sizeof(np_proto_cmd_hdr_t) + hdr->params_len;
        if (offset > body_len) {
            return 0U;
        }
    }
    return offset;
}

/* ── Public API ───────────────────────────────────────────────────────────────── */

np_hub_status_t np_protocol_verify_and_parse(const uint8_t    *buf,
                                              size_t            len,
                                              np_session_desc_t *desc_out)
{
    if (buf == NULL || desc_out == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    /* Minimum: header + signature */
    if (len < sizeof(np_proto_header_t) + NP_HUB_PROTO_SIG_LEN) {
        return NP_HUB_ERR_BAD_MAGIC;
    }

    const np_proto_header_t *hdr = (const np_proto_header_t *)buf;

    /* Magic and version */
    if (hdr->magic != NP_HUB_PROTO_MAGIC) {
        return NP_HUB_ERR_BAD_MAGIC;
    }
    if (hdr->version != NP_HUB_PROTO_VERSION) {
        return NP_HUB_ERR_BAD_VERSION;
    }
    if (hdr->cmd_count > NP_HUB_PROTO_CMD_MAX) {
        return NP_HUB_ERR_CMD_TOO_MANY;
    }

    /* Ed25519 signature is the last NP_HUB_PROTO_SIG_LEN bytes of buf. */
    const uint8_t *sig = buf + len - NP_HUB_PROTO_SIG_LEN;
    size_t         msg_len = len - NP_HUB_PROTO_SIG_LEN;

    uint8_t pub_key[32];
    if (np_proto_hal_get_proto_pubkey(pub_key) != NP_HUB_OK) {
        return NP_HUB_ERR_BAD_SIGNATURE;
    }

    if (np_ed25519_verify(buf, msg_len, sig, pub_key) != 0) {
        return NP_HUB_ERR_BAD_SIGNATURE;
    }

    /* Device serial replay guard */
    uint8_t stored_serial[NP_HUB_PROTO_SERIAL_LEN];
    if (np_proto_hal_get_device_serial(stored_serial,
                                        NP_HUB_PROTO_SERIAL_LEN) == NP_HUB_OK) {
        if (memcmp(hdr->device_serial, stored_serial, NP_HUB_PROTO_SERIAL_LEN) != 0) {
            return NP_HUB_ERR_WRONG_DEVICE;
        }
    }

    /* Validate command body length */
    const uint8_t *cmd_body     = buf + sizeof(np_proto_header_t);
    size_t         cmd_body_len = msg_len - sizeof(np_proto_header_t);

    size_t measured = measure_cmd_body(cmd_body, cmd_body_len, hdr->cmd_count);
    if (measured == 0U && hdr->cmd_count > 0U) {
        return NP_HUB_ERR_PARAMS_TOO_LONG;
    }

    /* Populate desc_out */
    memcpy(desc_out->session_uuid, hdr->session_uuid, NP_HUB_PROTO_UUID_LEN);
    desc_out->compiled_at_unix = hdr->compiled_at_unix;
    desc_out->duration_ms      = hdr->session_duration_ms;
    desc_out->flags            = hdr->flags;
    desc_out->cmd_count        = hdr->cmd_count;

    size_t offset = 0U;
    for (uint8_t i = 0U; i < hdr->cmd_count; i++) {
        const np_proto_cmd_hdr_t *ch =
            (const np_proto_cmd_hdr_t *)(cmd_body + offset);

        desc_out->cmds[i].mod_type   = (np_hub_mod_type_t)ch->mod_type;
        desc_out->cmds[i].slot_mask  = ch->slot_mask;
        desc_out->cmds[i].start_ms   = ch->start_ms;
        desc_out->cmds[i].duration_ms= ch->duration_ms;
        desc_out->cmds[i].params_len = ch->params_len;

        if (ch->params_len > 0U) {
            memcpy(desc_out->cmds[i].params,
                   (const uint8_t *)ch + sizeof(np_proto_cmd_hdr_t),
                   ch->params_len);
        }
        offset += sizeof(np_proto_cmd_hdr_t) + ch->params_len;
    }

    np_protocol_sort_cmds(desc_out);
    return NP_HUB_OK;
}

size_t np_protocol_compute_expected_len(const np_proto_header_t *hdr,
                                         const uint8_t           *cmd_body,
                                         size_t                   cmd_body_len)
{
    if (hdr == NULL) {
        return 0U;
    }
    size_t cmd_size = measure_cmd_body(cmd_body, cmd_body_len, hdr->cmd_count);
    if (cmd_size == 0U && hdr->cmd_count > 0U) {
        return 0U;
    }
    return sizeof(np_proto_header_t) + cmd_size + NP_HUB_PROTO_SIG_LEN;
}

/* Insertion sort on start_ms — cmd_count ≤ 64 so O(n²) is fine. */
void np_protocol_sort_cmds(np_session_desc_t *desc)
{
    for (uint8_t i = 1U; i < desc->cmd_count; i++) {
        np_session_cmd_t key = desc->cmds[i];
        int8_t j = (int8_t)(i - 1U);
        while (j >= 0 && desc->cmds[j].start_ms > key.start_ms) {
            desc->cmds[j + 1] = desc->cmds[j];
            j--;
        }
        desc->cmds[j + 1] = key;
    }
}
