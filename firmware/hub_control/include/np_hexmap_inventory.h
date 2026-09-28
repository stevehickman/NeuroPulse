/*
 * NeurOne Hub Control — Hex-tile identity + element inventory over the socket I2C seam
 * Document: NP-HEX-ZM-001 Rev 9 §4.1 (closes OI-HEXMAP-02)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * ── What this is ─────────────────────────────────────────────────────────────
 *
 * np_module_map_apply_poll() takes a socket's reported UID and, only when that
 * UID is new to the socket, an np_hexmap_inventory_fn to fetch the module's
 * element types. OI-HEXMAP-02 was that callback: how a module reports its UID
 * and its elements. This unit is it, plus the one-socket poll that composes
 * the UID read, the callback and apply_poll():
 *
 *   np_hexmap_poll_socket()  read the identity header (UID + element count)
 *                            and the health byte, then apply_poll() with
 *                            np_hexmap_inventory_read as the callback.
 *   np_hexmap_inventory_read the np_hexmap_inventory_fn. Re-reads the whole
 *                            identity block and accepts it only if it is
 *                            intact, the same module, and fully known.
 *
 * Every tile type carries a driver MCU (NP-HW-HEXTILE-001 D-3), so identity is
 * a register read on every type (NP-DRV-SHELL-002 §5.1.4): there is no UID
 * EEPROM and no ZONE_ID ladder.
 *
 * ── Topology-agnostic by construction ────────────────────────────────────────
 *
 * All module I/O goes through np_pbm_hal_i2c_read(socket_id, …), which is
 * socket-indexed (NP-HW-HUB-001 §9.2). Whether the target HAL tunnels through a
 * cluster controller (HUB §5.2) or drives HEXTILE D-7's LPI2C/PCA9548A tree
 * with UID-derived addresses is OI-HUB-C15's, and nothing here changes with
 * it. That is why this unit can be closed before the topology is.
 *
 * ── The identity block (tile-side contract, binding on OI-HEXTILE-07) ────────
 *
 * The tile register map (NP-FW-PBM1064-001 §5.1, 0x00–0x0D) has no identity
 * registers. This block is the hub's half of the contract; the on-module
 * firmware spec (OI-HEXTILE-07) must implement it byte for byte. It starts at
 * NP_HEXMAP_ID_REG, clear of 0x00–0x0D and of the PD readback registers D-4
 * adds after them, and the tile auto-increments the register pointer across it:
 *
 *   off  len  field
 *   0    1    format        NP_HEXMAP_ID_FORMAT (0x01)
 *   1    8    uid           64-bit module UID, factory-written, never all-0x00
 *                           or all-0xFF
 *   9    1    elem_count    0 .. NP_HEXMAP_MAX_ELEMENTS
 *   10   n    elem_type[n]  np_elem_type_t values, element_id order
 *   10+n 4    crc32         CRC-32 (IEEE 802.3, reflected), little-endian,
 *                           over bytes 0 .. 9+n
 *
 * The block is immutable for the life of the module (it describes the part,
 * not its state), so a read can be repeated and must return the same bytes.
 * Health is NOT in it: it is the STATUS register (0x00), read per poll.
 *
 * ── Why the inventory re-reads the header ────────────────────────────────────
 *
 * The poll reads only the 10-byte header, every socket, every pass: that is
 * what keeps an unchanged module from being re-inventoried. The CRC covers the
 * whole block, so the header alone is unchecked. When the header names a UID
 * new to the socket, the callback re-reads the entire block from offset 0 and
 * accepts it only if (a) the CRC holds, (b) its UID equals the UID the poll is
 * about to store and (c) its count equals the header's. (b) is the one that
 * matters: a module swapped, or a UID mis-read, between the two reads would
 * otherwise have its neighbour's element types stored under its UID, and
 * np_module_map resolves PBM and tES targets from those types.
 *
 * ── Fail-closed rules ────────────────────────────────────────────────────────
 *
 *   - Any I2C failure, a wrong format byte, an all-0x00 or all-0xFF UID, or a
 *     count above NP_HEXMAP_MAX_ELEMENTS: the socket is reported EMPTY to
 *     apply_poll(), which clears its record. A socket the hub cannot read is
 *     never left answering from its previous record.
 *   - An element type >= NP_ELEM_TYPE_COUNT rejects the module whole. It is
 *     not mapped to NP_ELEM_NONE: the record would then be cached under the
 *     module's UID and never re-inventoried after a hub update that knows the
 *     type, and a type this image cannot name is one it cannot place-check.
 *
 * Nothing here is user biology: a module UID is a component identifier
 * (SHDR class). This unit writes no record anywhere; np_module_map holds the
 * result and np_module_map_persist() decides what reaches Config.
 */

#ifndef NP_HEXMAP_INVENTORY_H
#define NP_HEXMAP_INVENTORY_H

#include <stdint.h>
#include <stdbool.h>
#include "np_hub_types.h"
#include "np_module_map.h"

/* ── Identity block layout (tile-side contract, OI-HEXTILE-07) ──────────────── */

#define NP_HEXMAP_ID_REG          0x40u  /* first register of the identity block  */
#define NP_HEXMAP_STATUS_REG      0x00u  /* NP-FW-PBM1064-001 §5.1 STATUS = health */
#define NP_HEXMAP_ID_FORMAT       0x01u
#define NP_HEXMAP_ID_HDR_BYTES    10u    /* format(1) + uid(8) + elem_count(1)    */
#define NP_HEXMAP_ID_CRC_BYTES    4u
#define NP_HEXMAP_ID_MAX_BYTES    (NP_HEXMAP_ID_HDR_BYTES + NP_HEXMAP_MAX_ELEMENTS + \
                                   NP_HEXMAP_ID_CRC_BYTES)   /* 142 */

/* The whole block must be readable in one np_pbm_hal_i2c_read (uint8_t len),
 * and must not wrap the 8-bit register space. */
typedef char _np_hexmap_id_fits_len[(NP_HEXMAP_ID_MAX_BYTES <= 255u) ? 1 : -1];
typedef char _np_hexmap_id_fits_regs[(NP_HEXMAP_ID_REG + NP_HEXMAP_ID_MAX_BYTES
                                      <= 256u) ? 1 : -1];

/* ── The header one poll reads ─────────────────────────────────────────────── */

typedef struct {
    np_module_uid_t uid;
    uint8_t         elem_count;
} np_hexmap_ident_t;

/*
 * np_hexmap_read_ident — read and sanity-check the identity header of the
 * module in `socket_id`.
 *
 * NP_HUB_OK: *out holds a well-formed UID and count (not CRC-checked; see the
 * header comment). NP_HUB_ERR_NOT_PRESENT: nothing answered. NP_HUB_ERR_BAD_VERSION:
 * wrong format byte. NP_HUB_ERR_BAD_MAGIC: an all-0x00 / all-0xFF UID or a
 * count above NP_HEXMAP_MAX_ELEMENTS. *out is zeroed on every failure.
 */
np_hub_status_t np_hexmap_read_ident(uint16_t socket_id, np_hexmap_ident_t *out);

/*
 * np_hexmap_inventory_read — the np_hexmap_inventory_fn (OI-HEXMAP-02).
 *
 * `ctx` MUST point at the np_hexmap_ident_t the poll read for this socket; the
 * block is accepted only if it names that UID and count. Fills types_out[0 ..
 * count-1] and *count_out only on NP_HUB_OK; on any failure writes neither and
 * returns an error (apply_poll() then leaves the socket not present).
 */
np_hub_status_t np_hexmap_inventory_read(uint16_t  socket_id,
                                         void     *ctx,
                                         uint8_t  *types_out,
                                         uint8_t   max,
                                         uint8_t  *count_out);

/*
 * np_hexmap_poll_socket — one socket's power-on / hot-plug poll.
 *
 * Reads the header and the health byte and hands them to
 * np_module_map_apply_poll() with np_hexmap_inventory_read. A socket whose
 * header or health cannot be read is applied as EMPTY (fail closed), and the
 * read error is returned so a caller can count it; a clean read returns
 * apply_poll()'s status. *changed_out is apply_poll()'s: the caller ORs it
 * across the pass and persists once.
 *
 * Takes no lease and no lock. The caller owns exclusion from a session
 * (REQ-FWHUB-03, np_lease_probe_begin()), as for every idle-side probe.
 */
np_hub_status_t np_hexmap_poll_socket(uint16_t socket_id, bool *changed_out);

#endif /* NP_HEXMAP_INVENTORY_H */
