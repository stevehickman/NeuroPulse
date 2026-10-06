/*
 * NeurOne — hub-side enable-word mirror (test support only)
 * Document: NP-HW-HUB-001 Rev 3 §7.2
 *
 * The enable bits and channel indices both processors use are ONE definition in
 * common/include/np_spi_wire_types.h, so there is no cross-side agreement to
 * test.  What this translation unit still exports is the set of constants only
 * the hub holds — the provisional electrode areas and the audio enable — read in
 * a TU that sees only the hub header, as plain data for
 * np_safety_spi_proto_tests.c to pin (fail-safe direction; audio not gated).
 *
 * Test support only.  Not part of any firmware image; the Class C safety MCU
 * never includes a hub header.
 */

#include <stdint.h>

#include "../../hub_control/include/np_hub_config.h"

#include "np_hub_enable_mirror.h"

/* The PROVISIONAL electrode areas for the channels that author no geometry of
 * their own (OI-CHARGE-07).  Exported so the proto test can pin the fail-safe
 * DIRECTION: each must be > 0 (or the MCU falls back to the permissive 25 cm²
 * default) and <= the 25 cm² default (a larger area is a larger charge budget,
 * which may only be justified by a measurement). */
const uint16_t NP_HUB_BES_AREA_MCM2  = (uint16_t)NP_BES_ELECTRODE_AREA_MCM2;
const uint16_t NP_HUB_VNS_AREA_MCM2  = (uint16_t)NP_VNS_ELECTRODE_AREA_MCM2;
const uint16_t NP_HUB_CVNS_AREA_MCM2 = (uint16_t)NP_CVNS_ELECTRODE_AREA_MCM2;

/* Audio is deliberately NOT safety-MCU gated; the hub encodes that as 0 and the
 * session runner skips the enable request.  Pinned so it cannot quietly become
 * a real bit without this test noticing.                                      */
const uint16_t NP_HUB_EN_AUDIO = (uint16_t)NP_SAFETY_EN_AUDIO;
