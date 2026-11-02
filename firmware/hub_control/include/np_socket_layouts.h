/*
 * NeurOne Hub Control Program — Socket Layouts by Hardware Version
 * Document: NP-FW-HUB-001 Rev 33 §3, REQ-FWHUB-64, OI-FWHUB-27
 *
 * ── What this is ─────────────────────────────────────────────────────────────
 * The number of sockets a helmet has, and which addresses name one, are properties of its HARDWARE
 * VERSION (the Principal, 2026-11-01 and 2026-11-02): a version may change them or leave them as they
 * were, so no count is fixed in code. This module is the table the hub looks a version up in.
 *
 * The rows are generated, not written: scripts/sync-socket-map.ts reads every hardware/np_socket_map_v<N>.json
 * (np_socket_map_v<N>.json is hardware version N, version 1 included) and writes
 * np_socket_layouts.generated.c. The app side's table is LAYOUT_FILES in common/npps-core/src/library.rs, and
 * the generator fails if the two name different files.
 *
 * ── Valid addresses ──────────────────────────────────────────────────────────
 * A firmware socket_id is a 0-based index (np_module_map.h). An index is a VALID ADDRESS for a layout only if
 * its entry is present_in_helmet. Counting the set bits of a protocol's mask is not a count of valid addresses
 * and this module does not count masks.
 *
 * ── What this does not do ────────────────────────────────────────────────────
 * Nothing calls it yet. The hub still binds no geometry at boot (np_module_map_init() has no caller outside
 * tests and the simulator), so bring-up binding the row for np_safety_spi_get_hardware_version() is the next
 * step (OI-FWHUB-27). A version with no row returns NULL, and the caller refuses, not assumes (the Principal, 2026-11-02).
 *
 * Pure C, no FreeRTOS: host-testable (np_socket_layouts_tests). IEC 62304 Class B — SW-02 hub control.
 */

#ifndef NP_SOCKET_LAYOUTS_H
#define NP_SOCKET_LAYOUTS_H

#include <stdbool.h>
#include <stdint.h>

#include "np_module_map.h"   /* np_socket_geom_t */

typedef struct {
    uint8_t                 hardware_version;  /* 1 .. 15 (NP-REG-UPG-001 §7.8)                      */
    uint16_t                socket_count;      /* valid addresses: entries with present_in_helmet    */
    uint16_t                table_len;         /* length of geom: highest valid index + 1            */
    const np_socket_geom_t *geom;              /* const flash table, indexed by firmware socket_id   */
} np_socket_layout_t;

/* Generated (np_socket_layouts.generated.c), ordered by hardware_version. */
extern const np_socket_layout_t NP_SOCKET_LAYOUTS[];
extern const uint8_t            NP_SOCKET_LAYOUT_COUNT;

/*
 * The row for a hardware version, or NULL if the hub holds none. NULL is an answer, not an error to work
 * around: a helmet whose version has no layout has no known valid addresses.
 */
const np_socket_layout_t *np_socket_layout_find(uint8_t hardware_version);

/* True iff `socket_index` is a valid address in `layout`. A NULL layout has none. */
bool np_socket_layout_valid_address(const np_socket_layout_t *layout, uint16_t socket_index);

#endif /* NP_SOCKET_LAYOUTS_H */
