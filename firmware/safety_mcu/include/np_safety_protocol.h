/*
 * NeurOne Safety MCU — SPI Protocol Types
 * Document: NP-FW-HUB-001 Rev 1 §7, NP-SW-001 Rev 1
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
 *   - Heartbeat period: 200ms (NP_SAFETY_HEARTBEAT_MS)
 *   - Watchdog timeout: 1500ms (NP_SAFETY_WATCHDOG_MS)
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

/* NP_SAFETY_BEAT_MAGIC_0/1 and the NP_SAFETY_EN_* enable bits are in
 * firmware/common/include/np_spi_wire_types.h (included above), with the
 * long-form notes on bit allocation.                                        */
/* 0x3FFF with bits 1–4 (0x001E) cleared: 10 allocated bits, 4 reserved holes. */
#define NP_SAFETY_EN_ALL_MASK       0x3FE1U

/* ── T2 enable set (NP-REG-UPG-001 §7.5, REQ-UPG-01, OI-UPG-01) ───────────── */
/*
 * The enable lines a unit whose tier identity is not a verified T2 never has
 * asserted, whatever is attached: cervical VNS (A14, on the hub accessory port
 * every T1 hub has), TMS, 1170 nm deep PBM (T2-D, which fits any socket) and
 * clinical tACS / HD-tDCS.  np_tier_identity_gate() strips them from
 * granted_mask.  Fixed at compile time and derived from nothing the hub sends:
 * a hub that stays silent cannot exempt a line from the gate.
 *
 * Deliberately NOT in this set: every T1 line, including the ones a T1
 * module drives on a T2 unit (REQ-UPG-03, modules carry over).  The 21-channel
 * qEEG cap has no enable line at all — it senses, it does not stimulate — so it
 * is refused by the hub at protocol load (np_protocol_tier_admit()), not here.
 */
#define NP_SAFETY_EN_T2_MASK \
    (NP_SAFETY_EN_CVNS | NP_SAFETY_EN_TMS | NP_SAFETY_EN_PBM_1170NM | \
     NP_SAFETY_EN_CLIN_STIM)

/* ── Electrode-bearing channel set (OI-CHARGE-05) ─────────────────────────── */
/*
 * The channels that drive current through skin-contact electrodes, and are
 * therefore the ones a charge ceiling means anything for.  Fixed at compile
 * time and deliberately NOT derived from anything the hub sends: it is what
 * lets np_charge_monitor_decl_gate() insist on a waveform declaration without
 * a per-session heartbeat bit to arm it — a hub that stays silent cannot
 * thereby exempt a channel from being monitored.
 *
 * Excluded, because they inject no charge through electrodes: PBM cranial,
 * PBM intranasal, PBM 1170nm, visual, TMS.  (TMS induces current by
 * induction; its coil hazards are SW01-M07's, not this module's.)
 */
#define NP_SAFETY_CH_ELECTRICAL_MASK \
    (NP_SAFETY_EN_BES_TACS | NP_SAFETY_EN_TDCS | NP_SAFETY_EN_VNS_HRV | \
     NP_SAFETY_EN_CVNS     | NP_SAFETY_EN_CLIN_STIM)

/* NP_SAFETY_CH_* charge-monitor channel indices and the bit-position ≡ index
 * compile-time check are in firmware/common/include/np_spi_wire_types.h.     */

/* ── Frame lengths ────────────────────────────────────────────────────────── */
/* NP_SAFETY_FRAME_LEN is the MCU reply frame length (defined in np_spi_wire_types.h as 8).
 * The heartbeat RX frame is NP_SAFETY_RX_EXT_FRAME_LEN (38 bytes).                      */
/* NP_SAFETY_CMD_MAGIC_0/1, NP_SAFETY_CMD_SESSION_SIG, NP_SAFETY_CMD_FRAME_LEN,
 * NP_SESSION_HASH_LEN, NP_ED25519_SIG_SIZE, and np_safety_sig_cmd_t are in
 * firmware/common/include/np_spi_wire_types.h (included above).               */

/* NP_SAFETY_STATUS_* and NP_SESSION_STATUS_ACTIVE / _CVNS_REENABLE are in
 * firmware/common/include/np_spi_wire_types.h (included above).              */

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

/* ── Fault-latch SHDR-reportable projection (SW01-M08) ──────────────────────
 * The complete set of latch fields any hub-facing read may surface, as ONE
 * fixed-shape record.  Every member is populated for every fault type; no
 * member's value is conditioned on WHICH fault is latched.  The shape is fixed
 * at compile time, so a field cannot be present for one fault kind and absent
 * for another — which is what made the retired per-field accessors leak.
 * `tick_ms` is deliberately NOT a member: fault event timing is UHDR.
 * Rationale and evidence: np_fault_latch.c, block comment above
 * np_fault_latch_build_report().                                            */
typedef struct {
    uint8_t  status;   /* latched NP_SAFETY_STATUS_* bitmask (OK if no valid latch) */
    uint8_t  slot;     /* latched fault slot (0xFF = none / no valid latch)         */
    uint16_t count;    /* distinct fault transitions since power-on (0 if no latch) */
} np_fault_latch_report_t;

void np_fault_latch_build_report(np_fault_latch_report_t *out);

/* ── Safety MCU state (shared across modules) ───────────────────────────── */
typedef struct {
    uint16_t requested_mask;   /* last requested enable mask from main processor */
    uint16_t granted_mask;     /* currently granted mask (after all interlock checks) */
    uint8_t  status;           /* current NP_SAFETY_STATUS_* bitmask */
    uint8_t  fault_slot;       /* slot that caused most recent fault (0xFF = none) */
    bool     session_active;   /* session underway */
    bool     cvns_active;      /* cervical VNS enabled this session */
    bool     geom_required;    /* OI-CHARGE-03: hub declared this session needs an
                                * electrode-geometry override on CLIN_STIM (from
                                * heartbeat NP_SESSION_STATUS_GEOM_REQUIRED bit)  */
    bool     geom_required_tdcs; /* OI-CHARGE-04: hub declared this session needs an
                                * electrode-geometry declaration on TDCS (from
                                * heartbeat NP_SESSION_STATUS_GEOM_REQ_TDCS bit)  */
    bool     geom_required_bes; /* OI-MMSOCK-02: hub declared this session needs an
                                * electrode-geometry declaration on BES_TACS (from
                                * heartbeat NP_SESSION_STATUS_GEOM_REQ_BES bit)   */
} np_safety_state_t;

/* np_safety_sig_cmd_t, NP_SAFETY_CMD_MAGIC_0/1, NP_SAFETY_CMD_SESSION_SIG,
 * NP_SAFETY_CMD_FRAME_LEN, NP_SESSION_HASH_LEN, NP_ED25519_SIG_SIZE are all
 * provided by firmware/common/include/np_spi_wire_types.h (included above). */

/* ── Module init/update return codes ─────────────────────────────────────── */
typedef enum {
    NP_SAFE_OK           = 0,
    NP_SAFE_ERR_FAULT    = -1,  /* interlock condition — stimulation cut */
    NP_SAFE_ERR_TIMEOUT  = -2,  /* hardware peripheral timeout */
    NP_SAFE_ERR_HW       = -3,  /* hardware init failure */
} np_safe_status_t;

#endif /* NP_SAFETY_PROTOCOL_H */
