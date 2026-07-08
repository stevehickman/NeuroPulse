/*
 * NeurOne Hub Control Program — Protocol Parser and Verifier
 * Document: NP-FW-HUB-001 Rev A §4
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
 *   - total buf size is consistent with cmd_count and params_len values
 *
 * Returns NP_HUB_OK on success.  desc_out is only valid on NP_HUB_OK.
 * Any failure leaves desc_out in an undefined state — caller must not use it.
 */
np_hub_status_t np_protocol_verify_and_parse(const uint8_t   *buf,
                                              size_t           len,
                                              np_session_desc_t *desc_out);

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
