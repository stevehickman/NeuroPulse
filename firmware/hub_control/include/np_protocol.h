/*
 * NeurOne Hub Control Program — Protocol Parser and Verifier
 * Document: NP-FW-HUB-001 Rev 1 §4
 *
 * Parses a signed protocol binary blob received from the app into an in-RAM
 * np_session_desc_t.  Verifies the Ed25519 signature using the same RFC 8032
 * verifier as the bootloader (np_signature.c) before accepting any command.
 */

#ifndef NP_PROTOCOL_H
#define NP_PROTOCOL_H

#include "np_hub_types.h"

/* ── API ─────────────────────────────────────────────────────────────────────── */

/*
 * np_protocol_verify_and_parse
 *
 * Verifies the Ed25519 signature over buf[0 .. len - NP_HUB_PROTO_SIG_LEN - 1]
 * using the device's stored protocol public key, then parses the header and
 * command list into *desc_out.
 *
 * Also validates:
 *   - magic word and version
 *   - device serial matches the stored serial in Config partition
 *   - cmd_count ≤ NP_HUB_PROTO_CMD_MAX
 *   - each command's params_len ≤ NP_HUB_PROTO_PARAMS_MAX
 *   - each command's target_kind is a known np_proto_target_kind_t, and its
 *     target_len is exactly the length that kind requires
 *   - slot_id agrees with target_kind: a SLOT target names a slot in
 *     NP_HUB_SLOT_FIRST_VALID .. NP_HUB_SLOT_MAX-1 (so a retired zone slot,
 *     which is what a v1 zone target would decode to, never reaches a module
 *     driver); any other target kind carries NP_HUB_SLOT_NONE
 *   - start_ms + duration_ms does not wrap uint32 (a wrapped auto-stop deadline
 *     can land on the runner's "no stop pending" sentinel)
 *   - the command body is EXACTLY consumed by cmd_count commands — trailing
 *     bytes mean cmd_count under-counts, which silently drops the tail, and the
 *     tail of an interval protocol is its STOP commands
 *
 * Returns NP_HUB_OK on success.  desc_out is only valid on NP_HUB_OK.
 * Any failure leaves desc_out in an undefined state — caller must not use it.
 */
np_hub_status_t np_protocol_verify_and_parse(const uint8_t   *buf,
                                              size_t           len,
                                              np_session_desc_t *desc_out);

/* ── Socket-target accessors ──────────────────────────────────────────────────
 * These read the parsed socket bitmap of a NP_PROTO_TARGET_SOCKET_MASK command.
 * On a command of any other target kind they behave as though no socket is
 * selected (false / 0 / empty list) — the fail-closed reading.
 *
 * The values here are INDEX SPACE, not socket numbers: 0-based, directly usable
 * as np_hex_addr_t.socket_id and as np_group_query_t.sockets entries. A socket
 * NUMBER is 1-based project-wide (NUMBER-1, docs/np_hex_zm_001.md §3.3), so
 * anything that displays, logs or transmits one of these must add the base first
 * — index 0 is socket 1. Nothing in this module performs that conversion.
 */

/* True iff socket_id is selected by this command. Out-of-domain ids are false. */
bool np_protocol_socket_is_set(const np_session_cmd_t *cmd, uint8_t socket_id);

/* Number of sockets this command selects. */
uint16_t np_protocol_socket_count(const np_session_cmd_t *cmd);

/*
 * np_protocol_socket_expand — expand the bitmap into the ascending socket list
 * np_module_map consumes.
 *
 * The output is exactly what np_group_query_t wants for NP_GROUP_KIND_SOCKET_SET:
 *
 *     uint16_t sockets[NP_HUB_SOCKET_MASK_BYTES * 8];
 *     uint16_t n;
 *     np_protocol_socket_expand(cmd, sockets, (uint16_t)(sizeof sockets / sizeof *sockets), &n);
 *     np_group_query_t q = { .kind = NP_GROUP_KIND_SOCKET_SET,
 *                            .sockets = sockets, .socket_count = n,
 *                            .type_mask = NP_ELEM_BIT(NP_ELEM_LED_660) };
 *     np_module_map_resolve_group(&q, addrs, max, &addr_count);
 *
 * The list is ascending and duplicate-free, both inherited from the bitmap.
 * On overflow, fills out[0..max-1], sets *count_out = max, and returns
 * NP_HUB_ERR_CMD_TOO_MANY (same convention as np_module_map_resolve_group).
 */
np_hub_status_t np_protocol_socket_expand(const np_session_cmd_t *cmd,
                                           uint16_t               *out,
                                           uint16_t                max,
                                           uint16_t               *count_out);

/*
 * np_protocol_compute_expected_len
 *
 * Returns the expected total byte count for a protocol blob with the given
 * header and cmd list.  Used to pre-validate buf length before parsing.
 * Returns 0 on overflow or invalid input.
 */
size_t np_protocol_compute_expected_len(const np_proto_header_t *hdr,
                                         const uint8_t           *cmd_body,
                                         size_t                   cmd_body_len);

/*
 * np_protocol_sort_cmds
 *
 * Sorts desc->cmds[0..cmd_count-1] by start_ms ascending.
 * Called after a successful parse so the session runner can iterate in order.
 */
void np_protocol_sort_cmds(np_session_desc_t *desc);

#endif /* NP_PROTOCOL_H */
