/*
 * NeurOne Hub Control Program — the session's hardware fitting (SWR-HV-02)
 * Document: NP-SW-001 §6.3 (SWR-HV-02); OI-UI-KMP-10 H2
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * At the start of each session the helmet writes down the hardware that carries it out: which module
 * is in each socket, the helmet hardware version, and the wavelength rules the session's descriptor was compiled
 * under (SWR-HV-03).  The record is written on the helmet, so it states
 * what was fitted, not what the protocol assumed.  A UHDR record names the protocol but, without this,
 * not the hardware, and a socket address names a place only on a given layout, so a reader could not tell
 * which parts of the head were reached.
 *
 * This is UHDR (CLAUDE.md §5.1: where on a person's head light was applied).  The shape does not depend on
 * what is fitted: one byte for EVERY wired socket, empty ones included, so the record's length and layout
 * say nothing about the fitting (§5.1 rule 2).
 *
 * Frame (NP_LOG_TAG_UHDR_FITTING, then), NP_FITTING_FORMAT 2:
 *   [0]    format            NP_FITTING_FORMAT.  Format 1 was the frame before the rules stamp (no format byte); it
 *          was never written on a shipped device (NP-FW-HUB-001 4.5: no session has been created on any device).
 *   [1]    hardware version  1..15 (np_safety_spi_get_hardware_version; 1 when the safety MCU holds no
 *          verified version, SWR-HV-07)
 *   [2..40] the wavelength-rules stamp, NP_HUB_PROTO_RULES_STAMP_LEN (39) bytes, copied unchanged from the signed
 *          descriptor the session is running (SWR-HV-03, NP-NPPS-REF-001 7a.1): origin (0 not stated, 1 a NeurOne
 *          release, 2 an owner set), release major, release minor, 36 bytes of windows.  Fixed length, so the
 *          record's shape says nothing about the rules (CLAUDE.md 5.1 rule 2).  The hub does not read the
 *          windows or look the release up: it records what the descriptor states, and origin 0 when it stated
 *          nothing, never an invented version.
 *   [41]   n                 wired sockets (np_module_map_socket_count)
 *   [42..] n bytes, index = hub socket index (0-based): NP_FITTING_EMPTY, NP_MOD_PBM_BASE, NP_MOD_PBM_SMART,
 *          or NP_FITTING_OTHER for a module that is not a PBM tile (an electrode tile)
 *
 * The stamp is UHDR with the rest of the frame (principal, OI-WRV-01 Q8): it records how the requested wavelength
 * was applied in this person's session.  The rule definition itself is owner configuration and is not here.
 */

#ifndef NP_SESSION_FITTING_H
#define NP_SESSION_FITTING_H

#include <stddef.h>
#include <stdint.h>

#include "np_hub_types.h"

#define NP_FITTING_EMPTY      0x00U
#define NP_FITTING_OTHER      0xFFU
#define NP_FITTING_FORMAT     2U
#define NP_FITTING_IDX_HW     1U
#define NP_FITTING_IDX_STAMP  2U
#define NP_FITTING_IDX_COUNT  (NP_FITTING_IDX_STAMP + NP_HUB_PROTO_RULES_STAMP_LEN)   /* 41 */
#define NP_FITTING_HDR_LEN    (NP_FITTING_IDX_COUNT + 1U)                             /* 42: through n */
#define NP_FITTING_MAX_LEN    (NP_FITTING_HDR_LEN + 128U)   /* NP_HEXMAP_MAX_SOCKETS */

/*
 * np_fitting_build — fill `out` with the frame for the helmet as it is now.  `rules_stamp` is the
 * NP_HUB_PROTO_RULES_STAMP_LEN bytes of the descriptor being run (np_session_desc_t.rules_stamp).  Returns the
 * length, or 0 when the buffer is too small, the stamp is missing, or the module map is not bound (a session is
 * not run on an unbound map).
 */
size_t np_fitting_build(uint8_t hw_version, const uint8_t *rules_stamp, uint8_t *out, size_t cap);

#endif /* NP_SESSION_FITTING_H */
