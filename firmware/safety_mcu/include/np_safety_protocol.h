/*
 * NeuroPulse Safety MCU — SPI Protocol Types
 * Document: NP-FW-HUB-001 Rev A §7, NP-SW-001 Rev A
 *
 * Three SPI frame types, distinguished by transfer length (NSS-delineated):
 *
 *   Extended heartbeat frame (38 bytes, every 200ms):
 *     Hub → MCU: np_safety_rx_ext_frame_t  (magic 0xBE 0xA7, in np_spi_wire_types.h)
 *     MCU → Hub: np_safety_tx_frame_t      (8-byte reply, simultaneous full-duplex)
 *     OI-CHARGE-01 CLOSED — frame carries current_ua[14] enabling charge monitor.
 *
 *   Session signature command frame (102 bytes, once per session):
 *     Hub → MCU: np_safety_sig_cmd_t   (magic 0xC0 0xDE)
 *     MCU → Hub: status byte + 101 zero padding (hub discards)
 *     OI-SW01-M07-01 CLOSED.
 *
 *   MCU reply frame (8 bytes, sent simultaneously with every hub TX):
 *     MCU → Hub: np_safety_tx_frame_t  (status + granted_mask + fault_slot + checksum)
 *
 * Backward compat: np_safety_rx_frame_t (the old 8-byte heartbeat base) is
 * kept here for the test suite and to document the layout origin.  The active
 * heartbeat frame type in np_safety_main.c is now np_safety_rx_ext_frame_t.
 *
 * SPI timing:
 *   - Heartbeat period: 200ms (NP_SAFETY_HEARTBEAT_EXP_MS)
 *   - Watchdog timeout: 1500ms (NP_SAFETY_WDG_TIMEOUT_MS)
 *
 * NP_SAFETY_FRAME_LEN        = 8   (MCU reply frame; np_safety_tx_frame_t)
 * NP_SAFETY_RX_EXT_FRAME_LEN = 38  (extended heartbeat; np_safety_rx_ext_frame_t)
 * NP_SAFETY_CMD_FRAME_LEN    = 102 (sig command)
 */

#ifndef NP_SAFETY_PROTOCOL_H
#define NP_SAFETY_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "np_safety_config.h"
#include "../../common/include/np_spi_wire_types.h"

/* ── Magic bytes (must match hub_control/np_hub_config.h) ─────────────────── */
#define NP_SAFETY_BEAT_MAGIC_0  0xBEU
#define NP_SAFETY_BEAT_MAGIC_1  0xA7U

/* ── Enable bitmask bits (must match hub_control/np_hub_config.h) ─────────── */
#define NP_SAFETY_EN_PBM_ZONE_0     (1U << 0)
#define NP_SAFETY_EN_PBM_ZONE_1     (1U << 1)
#define NP_SAFETY_EN_PBM_ZONE_2     (1U << 2)
#define NP_SAFETY_EN_PBM_ZONE_3     (1U << 3)
#define NP_SAFETY_EN_PBM_ZONE_4     (1U << 4)
#define NP_SAFETY_EN_BES_TACS       (1U << 5)
#define NP_SAFETY_EN_TDCS           (1U << 6)
#define NP_SAFETY_EN_VNS_HRV        (1U << 7)
#define NP_SAFETY_EN_VISUAL         (1U << 8)
#define NP_SAFETY_EN_INTRANASAL     (1U << 9)
#define NP_SAFETY_EN_CVNS           (1U << 10)
#define NP_SAFETY_EN_TMS            (1U << 11)
#define NP_SAFETY_EN_PBM_1170NM     (1U << 12)
#define NP_SAFETY_EN_CLIN_STIM      (1U << 13)
#define NP_SAFETY_EN_ALL_MASK       0x3FFFU

/* Charge-monitor channel INDEX for CLIN_STIM (= bit position of the enable
 * bit above).  HD-tDCS accumulates charge on this channel and is subject to
 * the OI-CHARGE-03 fail-safe geometry gate.                                 */
#define NP_SAFETY_CH_CLIN_STIM      13U

/* ── Frame lengths ────────────────────────────────────────────────────────── */
/* NP_SAFETY_FRAME_LEN is the MCU reply frame length (defined in np_safety_config.h as 8).
 * The heartbeat RX frame is NP_SAFETY_RX_EXT_FRAME_LEN (38 bytes).                      */
/* NP_SAFETY_CMD_MAGIC_0/1, NP_SAFETY_CMD_SESSION_SIG, NP_SAFETY_CMD_FRAME_LEN,
 * NP_SESSION_HASH_LEN, NP_ED25519_SIG_LEN, and np_safety_sig_cmd_t are in
 * firmware/common/include/np_spi_wire_types.h (included above).               */

/* ── Status flags returned in heartbeat TX frame ────────────────────────────── */
#define NP_SAFETY_STATUS_OK         0x00U
#define NP_SAFETY_STATUS_FAULT      (1U << 0)   /* any active fault */
#define NP_SAFETY_STATUS_WATCHDOG   (1U << 1)   /* watchdog fired since last beat */
#define NP_SAFETY_STATUS_CUTOFF     (1U << 2)   /* stimulation cut by safety MCU */
#define NP_SAFETY_STATUS_IMPEDANCE  (1U << 3)   /* impedance check failed */
#define NP_SAFETY_STATUS_THERMAL    (1U << 4)   /* thermal interlock active */
#define NP_SAFETY_STATUS_CHARGE     (1U << 5)   /* charge limit reached */
#define NP_SAFETY_STATUS_CARDIAC    (1U << 6)   /* cardiac interlock fired */
/* SIG_PENDING: set when session starts (sig_reset), cleared when sig verified.
 * Blocks grant_mask until hub delivers the session descriptor signature via
 * np_safety_sig_cmd_t.  Not a fault — no CUTOFF is implied.                  */
#define NP_SAFETY_STATUS_SIG_PENDING (1U << 7)  /* awaiting session signature delivery */

/* ── session_status byte bit definitions ───────────────────────────────── */
#define NP_SESSION_STATUS_ACTIVE        (1U << 0)  /* session underway */
#define NP_SESSION_STATUS_CVNS_REENABLE (1U << 1)  /* explicit CVNS re-enable after cardiac cutoff */

/* ── Received frame (main processor → safety MCU) ───────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  magic[2];        /* NP_SAFETY_BEAT_MAGIC_0 / _1 */
    uint8_t  session_status;  /* NP_SESSION_STATUS_* bit flags (2 bits used) */
    uint8_t  enable_lo;       /* requested enable bitmask bits 0-7 */
    uint8_t  enable_hi;       /* bits 8-15 */
    uint8_t  reserved;
    uint16_t checksum;        /* sum of bytes [0..5], wrapping uint16 */
} np_safety_rx_frame_t;       /* "received" from the perspective of safety MCU */

/* ── Transmit frame (safety MCU → main processor) ───────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  status;          /* NP_SAFETY_STATUS_* flags */
    uint8_t  granted_lo;      /* actually-granted enable bitmask bits 0-7 */
    uint8_t  granted_hi;
    uint8_t  fault_slot;      /* slot that caused fault; 0xFF = none */
    uint8_t  reserved[2];
    uint16_t checksum;        /* sum of bytes [0..5], wrapping uint16 */
} np_safety_tx_frame_t;       /* "transmitted" from the perspective of safety MCU */

/* ── Safety MCU state (shared across modules) ───────────────────────────── */
typedef struct {
    uint16_t requested_mask;   /* last requested enable mask from main processor */
    uint16_t granted_mask;     /* currently granted mask (after all interlock checks) */
    uint8_t  status;           /* current NP_SAFETY_STATUS_* bitmask */
    uint8_t  fault_slot;       /* slot that caused most recent fault (0xFF = none) */
    bool     session_active;   /* session underway */
    bool     cvns_active;      /* cervical VNS enabled this session */
    bool     geom_required;    /* OI-CHARGE-03: hub declared this session needs an
                                * electrode-geometry override (from heartbeat
                                * NP_SESSION_STATUS_GEOM_REQUIRED bit)            */
} np_safety_state_t;

/* np_safety_sig_cmd_t, NP_SAFETY_CMD_MAGIC_0/1, NP_SAFETY_CMD_SESSION_SIG,
 * NP_SAFETY_CMD_FRAME_LEN, NP_SESSION_HASH_LEN, NP_ED25519_SIG_LEN are all
 * provided by firmware/common/include/np_spi_wire_types.h (included above). */

/* ── Module init/update return codes ─────────────────────────────────────── */
typedef enum {
    NP_SAFE_OK           = 0,
    NP_SAFE_ERR_FAULT    = -1,  /* interlock condition — stimulation cut */
    NP_SAFE_ERR_TIMEOUT  = -2,  /* hardware peripheral timeout */
    NP_SAFE_ERR_HW       = -3,  /* hardware init failure */
} np_safe_status_t;

#endif /* NP_SAFETY_PROTOCOL_H */
