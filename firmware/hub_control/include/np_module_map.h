/*
 * NeurOne Hub Control Program — Hexagonal Zone-Module Addressing + NVRAM Map
 * Design study: hexagonal standardized zone-module redesign (2026-07-15 session).
 *
 * NOTE ON STATUS: this implements the PROPOSED hexagonal-tiling addressing
 * scheme from the module-redesign design study — it is NOT yet a baselined
 * NP-FW spec. The legacy fixed-slot control-dispatch registry (np_module_registry)
 * is unchanged; this map is the addressing/NVRAM layer the tiling architecture
 * needs and can be adopted independently of the mechanical decision (rigid vs
 * semi-flex hexagons).
 *
 * ── What this module is ──────────────────────────────────────────────────────
 *
 * Two-level hardware element addressing for a helmet interior tiled with
 * standardized hexagonal modules:
 *
 *     HW address = (socket_id : element_id)
 *       socket_id  (major, 7-bit) — WHICH socket the module is plugged into;
 *                                    fixed to a physical helmet location.
 *       element_id (minor, 7-bit) — WHICH element on that module.
 *
 * Three maps compose here:
 *
 *   1. GEOMETRY (fixed helmet property, const, lives in flash):
 *        socket_id → { lobe, side, x_mm, y_mm }.
 *      Because a socket's asymmetric key forces one mount orientation, the
 *      socket position fully determines physical location; element_id indexes
 *      within that module. Any finer intra-module element offset is a per-type
 *      layout concern and is out of scope here — protocol/group resolution
 *      operates at socket+type granularity, which this map delivers.
 *
 *   2. INVENTORY (per-socket, cached in NVRAM):
 *        socket_id → { module UID, health, element type[element_id] }.
 *      Rebuilt lazily: at power-on the hub polls every socket for its module
 *      UID + health; only when a socket's UID does NOT match the UID last
 *      stored for that socket does the module stream its element-type inventory,
 *      which is written to NVRAM. Unchanged modules are never re-inventoried.
 *
 *   3. GROUPS (predefined + user-defined):
 *        a set of (socket:element) addresses, resolved from a lobe+side, an
 *        explicit socket list, or an explicit address list, with an optional
 *        element-type include/exclude filter. Eight predefined groups (L/R ×
 *        frontal/temporal/parietal/occipital) plus arbitrary user groups.
 *
 * ── Privacy ──────────────────────────────────────────────────────────────────
 *
 * This module handles NO user biology. Module UID is a COMPONENT identifier
 * (device-linked, analogous to accessory authentication — SHDR class). The
 * geometry and element inventory are device configuration. Nothing here is
 * UHDR; nothing here is keyed to user identity.
 *
 * ── Class / testability ──────────────────────────────────────────────────────
 *
 * Pure C, no FreeRTOS dependency — host-testable (np_module_map_tests).
 * NVRAM I/O is behind an injected HAL (np_hexmap_nvram_read/write) so the whole
 * map is exercised on host. IEC 62304 Class B (SW-02 hub control).
 */

#ifndef NP_MODULE_MAP_H
#define NP_MODULE_MAP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "np_hub_types.h"

/* ── Address field sizing ─────────────────────────────────────────────────────
 * Socket field ≥7 bits so addressing never binds before geometry does (the
 * design study caps a fully-tiled vault at ~54-64 hexes). Element field 7 bits
 * covers the densest 40 mm tri-wavelength tile (~90 elements). */

#define NP_HEXMAP_SOCKET_BITS      7
#define NP_HEXMAP_ELEM_BITS        7
#define NP_HEXMAP_MAX_SOCKETS      64      /* physical geometry ceiling          */
#define NP_HEXMAP_MAX_ELEMENTS     128     /* 7-bit minor domain                 */
#define NP_HEXMAP_UID_LEN          8       /* 64-bit module UID                  */

/* ── NVRAM blob sizing ────────────────────────────────────────────────────────
 * Serialized layout: header + fixed-size per-socket records + CRC-32 trailer.
 * NP_HEXMAP_NVRAM_MAX_BYTES bounds any serialize buffer and the Config-partition
 * region the map is stored in. */
#define NP_HEXMAP_HDR_BYTES   8u    /* magic(4) + version(2) + n_sockets(2)          */
#define NP_HEXMAP_REC_BYTES   (NP_HEXMAP_UID_LEN + 3u + NP_HEXMAP_MAX_ELEMENTS)
                                    /* uid + present(1) + health(1) + count(1) + types[] */
#define NP_HEXMAP_CRC_BYTES   4u
#define NP_HEXMAP_NVRAM_MAX_BYTES                                                  \
    ((size_t)NP_HEXMAP_HDR_BYTES +                                                 \
     (size_t)NP_HEXMAP_MAX_SOCKETS * (size_t)NP_HEXMAP_REC_BYTES +                 \
     (size_t)NP_HEXMAP_CRC_BYTES)

/* ── Element types (6-bit domain, ≤64) ────────────────────────────────────── */

typedef enum {
    NP_ELEM_NONE          = 0,   /* unpopulated element slot on the module      */
    NP_ELEM_LED_660       = 1,
    NP_ELEM_LED_808       = 2,
    NP_ELEM_LED_1064      = 3,
    NP_ELEM_LED_1170      = 4,
    NP_ELEM_EEG_ELECTRODE = 5,
    NP_ELEM_TES_ELECTRODE = 6,   /* dual-rated tDCS/tACS/HD-tDCS electrode      */
    NP_ELEM_VNS_CONTACT   = 7,
    NP_ELEM_NTC           = 8,   /* thermistor                                  */
    NP_ELEM_PD_FORWARD    = 9,   /* PD1 behind PDMS window                      */
    NP_ELEM_PD_BACK       = 10,  /* PD2 scalp-facing                            */
    NP_ELEM_IR_PROX       = 11,
    NP_ELEM_HALL          = 12,
    NP_ELEM_DUAL_ELECTRODE= 13,  /* Ag/AgCl: records EEG AND delivers tES (T1-B) */
    NP_ELEM_TYPE_COUNT    = 14,
    /* Enum values are frozen once written to NVRAM — append only, never renumber. */
} np_elem_type_t;

/* Bit for a type in a type-filter mask (uint64_t). */
#define NP_ELEM_BIT(t)  ((uint64_t)1u << (unsigned)(t))

/* ── Lobe / side ─────────────────────────────────────────────────────────────
 * NP_SIDE_MIDLINE used as a QUERY side means "any hemisphere" (wildcard). */

typedef enum {
    NP_LOBE_NONE      = 0,
    NP_LOBE_FRONTAL   = 1,
    NP_LOBE_TEMPORAL  = 2,
    NP_LOBE_PARIETAL  = 3,
    NP_LOBE_OCCIPITAL = 4,
    NP_LOBE_COUNT     = 5,
} np_lobe_t;

typedef enum {
    NP_SIDE_MIDLINE = 0,
    NP_SIDE_LEFT    = 1,
    NP_SIDE_RIGHT   = 2,
} np_side_t;

/* ── Two-level address ────────────────────────────────────────────────────── */

typedef struct {
    uint8_t socket_id;    /* major, 0 .. NP_HEXMAP_MAX_SOCKETS-1  */
    uint8_t element_id;   /* minor, 0 .. NP_HEXMAP_MAX_ELEMENTS-1 */
} np_hex_addr_t;

/* Pack to a 14-bit wire value: [13:7]=socket, [6:0]=element. */
static inline uint16_t np_hex_addr_pack(np_hex_addr_t a)
{
    return (uint16_t)(((uint16_t)(a.socket_id & 0x7Fu) << NP_HEXMAP_ELEM_BITS) |
                      (uint16_t)(a.element_id & 0x7Fu));
}

static inline np_hex_addr_t np_hex_addr_unpack(uint16_t v)
{
    np_hex_addr_t a;
    a.socket_id  = (uint8_t)((v >> NP_HEXMAP_ELEM_BITS) & 0x7Fu);
    a.element_id = (uint8_t)(v & 0x7Fu);
    return a;
}

/* ── Module UID (component identifier — SHDR class) ──────────────────────────── */

typedef struct {
    uint8_t b[NP_HEXMAP_UID_LEN];
} np_module_uid_t;

bool np_module_uid_equal(const np_module_uid_t *a, const np_module_uid_t *b);
bool np_module_uid_is_zero(const np_module_uid_t *a);   /* zero UID = empty socket */

/* ── Fixed helmet geometry (const table, one entry per socket) ──────────────── */

typedef struct {
    bool      present_in_helmet;  /* false = socket not wired in this shell      */
    np_lobe_t lobe;
    np_side_t side;
    int16_t   x_mm;               /* unrolled-vault coords (simulator selection)  */
    int16_t   y_mm;
} np_socket_geom_t;

/* ── Physical resolution result ─────────────────────────────────────────────── */

typedef struct {
    np_lobe_t      lobe;
    np_side_t      side;
    int16_t        x_mm;
    int16_t        y_mm;
    np_elem_type_t elem_type;
} np_physical_loc_t;

/* ── Group query ─────────────────────────────────────────────────────────────── */

typedef enum {
    NP_GROUP_KIND_LOBE       = 0,  /* all elements in sockets matching lobe+side  */
    NP_GROUP_KIND_SOCKET_SET = 1,  /* all elements in an explicit socket list      */
    NP_GROUP_KIND_ADDR_SET   = 2,  /* explicit (socket:element) address list       */
} np_group_kind_t;

typedef struct {
    np_group_kind_t kind;

    /* KIND_LOBE */
    np_lobe_t lobe;
    np_side_t side;                 /* NP_SIDE_MIDLINE = any hemisphere (wildcard)  */

    /* KIND_SOCKET_SET */
    const uint16_t *sockets;
    uint16_t        socket_count;

    /* KIND_ADDR_SET */
    const np_hex_addr_t *addrs;
    uint16_t             addr_count;

    /* Type filter (all kinds). type_mask == 0 → all types. */
    uint64_t type_mask;
    bool     type_exclude;          /* false: include only listed; true: exclude   */
} np_group_query_t;

/* Predefined lobe groups. */
typedef enum {
    NP_PGROUP_FRONTAL_L = 0, NP_PGROUP_FRONTAL_R,
    NP_PGROUP_TEMPORAL_L,    NP_PGROUP_TEMPORAL_R,
    NP_PGROUP_PARIETAL_L,    NP_PGROUP_PARIETAL_R,
    NP_PGROUP_OCCIPITAL_L,   NP_PGROUP_OCCIPITAL_R,
    NP_PGROUP_COUNT,
} np_pgroup_t;

/* ── Inventory callback ───────────────────────────────────────────────────────
 * Called by np_module_map_apply_poll ONLY when a socket's module UID changed
 * (a module new to that socket). Fetches the module's element-type inventory
 * (over the module's I2C/1-wire link). Fills types_out[0..count-1] with
 * np_elem_type_t values; sets *count_out. Returns NP_HUB_OK on success. */
typedef np_hub_status_t (*np_hexmap_inventory_fn)(uint16_t  socket_id,
                                                  void     *ctx,
                                                  uint8_t  *types_out,
                                                  uint8_t   max,
                                                  uint8_t  *count_out);

/* ═══════════════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * np_module_map_init — bind the fixed helmet geometry and zero all inventory
 * records. geom must point at a table of n_sockets entries that outlives the
 * map (const flash table on target). n_sockets ≤ NP_HEXMAP_MAX_SOCKETS.
 */
np_hub_status_t np_module_map_init(const np_socket_geom_t *geom, uint16_t n_sockets);

/*
 * np_module_map_apply_poll — apply one socket's power-on / hot-plug poll result.
 *
 *   reported_uid : UID the socket's module returned (zero = socket empty).
 *   health       : opaque module health byte.
 *   inv_cb       : called ONLY when the module is new to this socket (UID change
 *                  to a non-zero UID). May be NULL only if no new module can
 *                  appear; a new module with inv_cb == NULL fails closed.
 *   changed_out  : set true iff stored inventory state changed (caller ORs these
 *                  across all sockets and persists NVRAM once if any changed).
 *
 * Fail-closed: if inv_cb fails or reports more than NP_HEXMAP_MAX_ELEMENTS, the
 * socket is left with module_present == false (the module is not usable until a
 * clean inventory succeeds).
 */
np_hub_status_t np_module_map_apply_poll(uint16_t                 socket_id,
                                         const np_module_uid_t   *reported_uid,
                                         uint8_t                  health,
                                         np_hexmap_inventory_fn   inv_cb,
                                         void                    *inv_ctx,
                                         bool                    *changed_out);

/*
 * np_module_map_resolve — resolve a (socket:element) address to its physical
 * location + element type. Returns NP_HUB_ERR_NOT_PRESENT if the socket is
 * empty / not in the helmet, or element_id ≥ the module's element count.
 */
np_hub_status_t np_module_map_resolve(np_hex_addr_t addr, np_physical_loc_t *out);

/*
 * np_module_map_resolve_group — expand a group query to the list of
 * (socket:element) addresses it selects, after applying the type filter.
 * NP_ELEM_NONE elements are always skipped. On overflow, fills out[0..max-1],
 * sets *count_out = max, and returns NP_HUB_ERR_CMD_TOO_MANY.
 */
np_hub_status_t np_module_map_resolve_group(const np_group_query_t *q,
                                            np_hex_addr_t          *out,
                                            uint16_t                max,
                                            uint16_t               *count_out);

/*
 * np_module_map_predefined — resolve one of the eight predefined lobe groups,
 * with an optional type filter (type_mask == 0 → all types).
 */
np_hub_status_t np_module_map_predefined(np_pgroup_t     g,
                                         uint64_t        type_mask,
                                         bool            type_exclude,
                                         np_hex_addr_t  *out,
                                         uint16_t        max,
                                         uint16_t       *count_out);

/* ── Placement validation ──────────────────────────────────────────────────────
 * "Are the appropriate modules in the appropriate sockets?" Because tiles are
 * type-agnostic (any type inserts in any socket) and the helmet auto-inventories
 * whatever is plugged, safety-critical and montage requirements are enforced in
 * software against the live inventory — e.g. visual stimulation requires an EEG
 * element at the Oz socket (photoparoxysmal halt); tES requires electrodes at all
 * montage sockets. A requirement is satisfied when the named socket holds at
 * least one element whose type is in `type_mask`. */

typedef struct {
    uint8_t  socket_id;   /* socket that must be satisfied                        */
    uint64_t type_mask;   /* acceptable element types (NP_ELEM_BIT(...) OR'd)     */
} np_placement_req_t;

/*
 * np_module_map_check_placement — validate the current inventory against reqs.
 * For each requirement, the socket must be present and hold ≥1 element whose type
 * is in type_mask (NP_ELEM_NONE never counts). Unmet requirements are appended to
 * failed_sockets[] (up to max; may be NULL). *fail_count is the TOTAL number of
 * failures (may exceed max → list truncated).
 * Returns NP_HUB_OK if all satisfied, NP_HUB_ERR_NOT_PRESENT if any is unmet.
 */
np_hub_status_t np_module_map_check_placement(const np_placement_req_t *reqs,
                                              uint16_t                  n,
                                              uint8_t                  *failed_sockets,
                                              uint16_t                  max,
                                              uint16_t                 *fail_count);

/* ── NVRAM persistence ───────────────────────────────────────────────────────── */

/* Exact serialized byte count for the current geometry (header+records+crc). */
size_t np_module_map_serialized_size(void);

/* Serialize the inventory to buf. Returns bytes written, or negative on error. */
int np_module_map_serialize(uint8_t *buf, size_t buf_len);

/* Load inventory from buf (validates magic/version/socket-count/CRC-32). */
np_hub_status_t np_module_map_load(const uint8_t *buf, size_t buf_len);

/* Persist current inventory via the NVRAM HAL (serialize → np_hexmap_nvram_write). */
np_hub_status_t np_module_map_persist(void);

/* Restore inventory via the NVRAM HAL (np_hexmap_nvram_read → load). */
np_hub_status_t np_module_map_restore(void);

/* ── NVRAM HAL (OI-HEXMAP-01 — platform-provided; test-injected) ─────────────── */

/*
 * Read/write the module-map region of the Config/NVRAM partition. On target
 * these wrap the LittleFS Config partition access; tests inject stubs.
 * np_hexmap_nvram_read sets *read_len to the number of bytes read.
 */
extern np_hub_status_t np_hexmap_nvram_read(uint8_t *buf, size_t len, size_t *read_len);
extern np_hub_status_t np_hexmap_nvram_write(const uint8_t *buf, size_t len);

#endif /* NP_MODULE_MAP_H */
