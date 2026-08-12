/*
 * NeurOne Hub Control Program — Protocol Parser and Verifier
 * Document: NP-FW-HUB-001 Rev 1 §4
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
 * target_len_for_kind — the exact target-block length a target kind requires.
 *
 * Exact, not maximum: a socket-mask command carrying 15 bytes is not a short
 * mask to be zero-extended, it is a producer that disagrees with this firmware
 * about the socket domain. Zero-extending it would silently drop the top
 * sockets from the target. Returns false for an unknown kind so the caller
 * rejects rather than guesses.
 */
static bool target_len_for_kind(uint8_t kind, uint8_t *len_out)
{
    switch (kind) {
        case NP_PROTO_TARGET_SLOT:
            *len_out = 0U;
            return true;
        case NP_PROTO_TARGET_SOCKET_MASK:
            *len_out = NP_HUB_SOCKET_MASK_BYTES;
            return true;
        default:
            return false;
    }
}

/*
 * Walk the command body, validating each command header, and report the total
 * byte count consumed by cmd_count commands via *consumed_out.
 *
 * Returns the specific reason on failure so a malformed target is not reported
 * as an over-long params block; *consumed_out is only valid on NP_HUB_OK.
 */
static np_hub_status_t measure_cmd_body(const uint8_t *body, size_t body_len,
                                         uint8_t cmd_count, size_t *consumed_out)
{
    size_t offset = 0U;

    for (uint8_t i = 0U; i < cmd_count; i++) {
        if (offset + sizeof(np_proto_cmd_hdr_t) > body_len) {
            return NP_HUB_ERR_PARAMS_TOO_LONG;
        }
        const np_proto_cmd_hdr_t *hdr = (const np_proto_cmd_hdr_t *)(body + offset);

        if (hdr->params_len > NP_HUB_PROTO_PARAMS_MAX) {
            return NP_HUB_ERR_PARAMS_TOO_LONG;
        }

        uint8_t expect_len = 0U;
        if (!target_len_for_kind(hdr->target_kind, &expect_len)) {
            return NP_HUB_ERR_INVALID_ARG;
        }
        if (hdr->target_len != expect_len) {
            return NP_HUB_ERR_INVALID_ARG;
        }

        /* slot_id must agree with the target kind.
         *
         * A slot target naming a retired zone slot (0-4) is a v1 zone target
         * that survived the socket migration — slot 0 is the PBM zone-0 driver,
         * so honouring it would drive a module the protocol never named. A
         * socket target carrying anything but NP_HUB_SLOT_NONE is a producer
         * asking for two destinations at once. Both are rejected. */
        if (hdr->target_kind == NP_PROTO_TARGET_SLOT) {
            if (hdr->slot_id < NP_HUB_SLOT_FIRST_VALID ||
                hdr->slot_id >= NP_HUB_SLOT_MAX) {
                return NP_HUB_ERR_INVALID_ARG;
            }
        } else if (hdr->slot_id != NP_HUB_SLOT_NONE) {
            return NP_HUB_ERR_INVALID_ARG;
        }

        /* The auto-stop deadline is computed as start_ms + duration_ms in a
         * uint32 (np_session_runner). If that sum wraps it can land on 0, which
         * the runner reads as "no stop pending" — the module would then run to
         * session end with its duration silently discarded. Reject the wrap
         * here, where the blob is still rejectable. */
        if (hdr->duration_ms > (UINT32_MAX - hdr->start_ms)) {
            return NP_HUB_ERR_INVALID_ARG;
        }

        offset += sizeof(np_proto_cmd_hdr_t) + hdr->target_len + hdr->params_len;
        if (offset > body_len) {
            return NP_HUB_ERR_PARAMS_TOO_LONG;
        }
    }

    /* The body must be fully consumed. A cmd_count that under-counts the
     * commands actually present would otherwise parse cleanly with the tail
     * silently dropped — and because commands sort by start_ms, the tail of an
     * interval protocol is exactly its STOP commands. Dropping those leaves
     * stimulation running with no scheduled stop. */
    if (offset != body_len) {
        return NP_HUB_ERR_PARAMS_TOO_LONG;
    }

    *consumed_out = offset;
    return NP_HUB_OK;
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

    size_t          measured = 0U;
    np_hub_status_t body_rc  =
        measure_cmd_body(cmd_body, cmd_body_len, hdr->cmd_count, &measured);
    if (body_rc != NP_HUB_OK) {
        return body_rc;
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
        desc_out->cmds[i].slot_id    = ch->slot_id;
        desc_out->cmds[i].start_ms   = ch->start_ms;
        desc_out->cmds[i].duration_ms= ch->duration_ms;
        desc_out->cmds[i].params_len = ch->params_len;
        desc_out->cmds[i].target_kind = ch->target_kind;

        const uint8_t *target = (const uint8_t *)ch + sizeof(np_proto_cmd_hdr_t);

        /* Always zero the mask first: a non-socket command must not expose the
         * previous session's bits to a caller that skipped the kind check. */
        memset(desc_out->cmds[i].socket_mask, 0,
               sizeof desc_out->cmds[i].socket_mask);
        if (ch->target_kind == NP_PROTO_TARGET_SOCKET_MASK) {
            /* measure_cmd_body already pinned target_len to exactly
             * NP_HUB_SOCKET_MASK_BYTES for this kind. */
            memcpy(desc_out->cmds[i].socket_mask, target, NP_HUB_SOCKET_MASK_BYTES);
        }

        if (ch->params_len > 0U) {
            memcpy(desc_out->cmds[i].params, target + ch->target_len, ch->params_len);
        }
        offset += sizeof(np_proto_cmd_hdr_t) + ch->target_len + ch->params_len;
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
    size_t cmd_size = 0U;
    if (measure_cmd_body(cmd_body, cmd_body_len, hdr->cmd_count, &cmd_size) != NP_HUB_OK) {
        return 0U;
    }
    return sizeof(np_proto_header_t) + cmd_size + NP_HUB_PROTO_SIG_LEN;
}

/* ── Socket-target accessors ──────────────────────────────────────────────────── */

/* Total sockets the bitmap can express — the wire domain, not this shell's count. */
#define PROTO_SOCKET_DOMAIN  (NP_HUB_SOCKET_MASK_BYTES * 8U)

bool np_protocol_socket_is_set(const np_session_cmd_t *cmd, uint8_t socket_id)
{
    if (cmd == NULL || cmd->target_kind != NP_PROTO_TARGET_SOCKET_MASK) {
        return false;
    }
    if ((unsigned)socket_id >= PROTO_SOCKET_DOMAIN) {
        return false;
    }
    return ((cmd->socket_mask[socket_id / 8U] >> (socket_id % 8U)) & 1U) != 0U;
}

uint16_t np_protocol_socket_count(const np_session_cmd_t *cmd)
{
    if (cmd == NULL || cmd->target_kind != NP_PROTO_TARGET_SOCKET_MASK) {
        return 0U;
    }
    uint16_t n = 0U;
    for (unsigned byte = 0U; byte < NP_HUB_SOCKET_MASK_BYTES; byte++) {
        uint8_t bits = cmd->socket_mask[byte];
        while (bits != 0U) {
            bits &= (uint8_t)(bits - 1U);   /* clear lowest set bit */
            n++;
        }
    }
    return n;
}

np_hub_status_t np_protocol_socket_expand(const np_session_cmd_t *cmd,
                                           uint16_t               *out,
                                           uint16_t                max,
                                           uint16_t               *count_out)
{
    if (cmd == NULL || out == NULL || count_out == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    uint16_t n = 0U;
    if (cmd->target_kind != NP_PROTO_TARGET_SOCKET_MASK) {
        *count_out = 0U;
        return NP_HUB_OK;
    }

    for (unsigned s = 0U; s < PROTO_SOCKET_DOMAIN; s++) {
        if (((cmd->socket_mask[s / 8U] >> (s % 8U)) & 1U) == 0U) {
            continue;
        }
        if (n >= max) {
            *count_out = max;
            return NP_HUB_ERR_CMD_TOO_MANY;
        }
        out[n++] = (uint16_t)s;
    }

    *count_out = n;
    return NP_HUB_OK;
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
