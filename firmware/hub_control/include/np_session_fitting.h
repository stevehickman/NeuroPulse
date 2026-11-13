/*
 * NeurOne Hub Control Program — the session's hardware fitting (SWR-HV-02)
 * Document: NP-SW-001 §6.3 (SWR-HV-02); OI-UI-KMP-10 H2
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * At the start of each session the helmet writes down the hardware that carries it out: which module
 * is in each socket, and the helmet hardware version.  The record is written on the helmet, so it states
 * what was fitted, not what the protocol assumed.  A UHDR record names the protocol but, without this,
 * not the hardware, and a socket address names a place only on a given layout, so a reader could not tell
 * which parts of the head were reached.
 *
 * This is UHDR (CLAUDE.md §5.1: where on a person's head light was applied).  The shape does not depend on
 * what is fitted: one byte for EVERY wired socket, empty ones included, so the record's length and layout
 * say nothing about the fitting (§5.1 rule 2).
 *
 * Frame (NP_LOG_TAG_UHDR_FITTING, then):
 *   [0]    format            NP_FITTING_FORMAT
 *   [1]    hardware version  1..15 (np_safety_spi_get_hardware_version; 1 when the safety MCU holds no
 *          verified version, SWR-HV-07)
 *   [2]    n                 wired sockets (np_module_map_socket_count)
 *   [3..]  n bytes, index = hub socket index (0-based): NP_FITTING_EMPTY, NP_MOD_PBM_BASE, NP_MOD_PBM_SMART,
 *          or NP_FITTING_OTHER for a module that is not a PBM tile (an electrode tile)
 *   [3+n..] the 39-byte rules stamp (NP_HUB_RULES_STAMP_LEN)
 *
 * Format 2 (SWR-HV-03, GitHub #564) leads with the format byte and ends with the 39-byte wavelength-rules
 * stamp, copied verbatim from the signed descriptor: the hub holds no rules and no register, so it records
 * what the descriptor states and never a version of its own.  A descriptor with no stamp is all zero bytes
 * (origin 0, "not stated").  The stamp carries the windows, so the record reads with no store to look up.
 * It is UHDR with the rest of the frame and never reaches SHDR, a fleet upload or the diagnostic log.
 */

#ifndef NP_SESSION_FITTING_H
#define NP_SESSION_FITTING_H

#include <stddef.h>
#include <stdint.h>

#include "np_hub_types.h"

#define NP_FITTING_EMPTY      0x00U
#define NP_FITTING_OTHER      0xFFU
#define NP_FITTING_FORMAT     2U
#define NP_FITTING_HDR_LEN    3U
#define NP_FITTING_MAX_LEN    (NP_FITTING_HDR_LEN + 128U + NP_HUB_RULES_STAMP_LEN)   /* NP_HEXMAP_MAX_SOCKETS */

/*
 * np_fitting_build — fill `out` with the frame for the helmet as it is now.  `rules_stamp` is the descriptor's
 * NP_HUB_RULES_STAMP_LEN bytes (NULL writes the not-stated stamp, all zero).  Returns the length, or 0 when
 * the buffer is too small or the module map is not bound (a session is not run on an unbound map).
 */
size_t np_fitting_build(uint8_t hw_version, const uint8_t *rules_stamp, uint8_t *out, size_t cap);

#endif /* NP_SESSION_FITTING_H */
