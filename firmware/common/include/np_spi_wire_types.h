/*
 * NeuroPulse Firmware — Shared SPI Wire-Format Types
 * Document: NP-FW-HUB-001 Rev A §7, NP-SW-001 Rev A
 *
 * Single source of truth for SPI frame types shared between the STM32G071
 * safety MCU (SW-01, Class C) and the i.MX RT1062 hub control program
 * (SW-02, Class B).
 *
 * Three SPI frame types, distinguished by NSS-delineated transfer length:
 *
 *   (1) Extended heartbeat frame   (38 bytes, hub→MCU every 200ms)
 *         np_safety_rx_ext_frame_t  — MCU's receive perspective
 *         np_safety_tx_ext_frame_t  — hub's transmit perspective (typedef alias)
 *
 *   (2) Session signature command  (102 bytes, hub→MCU once per session)
 *         np_safety_sig_cmd_t
 *
 *   (3) MCU reply frame            (8 bytes, MCU→hub, simultaneous with each tx)
 *         Defined in np_safety_protocol.h (MCU) / np_hub_types.h (hub).
 *         Not in this file — the reply frame is not a shared type.
 *
 * Include from BOTH sides:
 *   firmware/safety_mcu/include/np_safety_protocol.h
 *   firmware/hub_control/include/np_hub_types.h
 *
 * No MCU-specific or hub-specific headers may be included from this file.
 * Only <stdint.h> is permitted.
 *
 * OI-SW01-M07-01 CLOSED — sig cmd frame originally in separate per-side headers.
 * OI-CHARGE-01   CLOSED — ext heartbeat frame introduced; current_ua[] wired.
 */

#ifndef NP_SPI_WIRE_TYPES_H
#define NP_SPI_WIRE_TYPES_H

#include <stdint.h>

/* ── Channel count (common to both frame types) ─────────────────────────── */

/* Must match the s_charge_nc[] array size in np_charge_monitor.c and the
 * number of NP_SAFETY_EN_* bits defined in np_safety_protocol.h.            */
#define NP_SAFETY_MAX_CHANNELS      14U

/* ── Session signature command constants ────────────────────────────────── */

#define NP_SAFETY_CMD_MAGIC_0       0xC0U   /* distinguishes from heartbeat 0xBE */
#define NP_SAFETY_CMD_MAGIC_1       0xDEU
#define NP_SAFETY_CMD_SESSION_SIG   0x01U   /* cmd_type: deliver hash + sig */

/* 2 (magic) + 1 (type) + 1 (rsvd) + 32 (hash) + 64 (sig) + 2 (checksum) = 102 */
#define NP_SAFETY_CMD_FRAME_LEN     102U
#define NP_SESSION_HASH_LEN         32U     /* SHA-256 hash of session descriptor */
#define NP_ED25519_SIG_LEN          64U     /* Ed25519 signature */

/* ── Extended heartbeat frame length ────────────────────────────────────── */

/* Layout: 8-byte base (magic+session_status+enables+channel_count+checksum)
 *        + 28-byte current array (14 × uint16_t)
 *        + 2-byte ext_checksum
 *        = 38 bytes total.
 *
 * The base 8 bytes are layout-identical to the old np_safety_rx_frame_t so
 * that existing base-checksum code (covering bytes [0..5]) continues to work
 * unchanged.  The channel_count byte repurposes the former `reserved` byte at
 * offset 5.                                                                   */
#define NP_SAFETY_RX_EXT_FRAME_LEN  38U

/* ── Extended heartbeat frame (hub → MCU, 38 bytes) ─────────────────────── */
/*
 * Hub sends this every 200ms.  MCU replies simultaneously with the 8-byte TX
 * frame (status + granted_mask + fault_slot + checksum); hub reads the first 8
 * bytes of the 38-byte receive buffer as the MCU reply and discards the rest
 * (which the MCU's SPI slave clocks out as zeros).
 *
 * ── Privacy gate (NP-FW-EMMC-001 Rev A §12) ──────────────────────────────
 * current_ua[] carries COMMANDED current from the session descriptor.
 * Classification: SHDR (device metric — what the protocol requested).
 *
 * This is NOT the ADC-measured actual delivered current, which would reveal
 * tissue impedance and is UHDR-class patient data.  The safety MCU never
 * receives ADC-measured current over SPI — only the commanded session value.
 *
 * ── Checksum scheme ───────────────────────────────────────────────────────
 * checksum     — additive sum of bytes [0..5] (magic through channel_count).
 *                Same algorithm as the old 8-byte heartbeat checksum.
 * ext_checksum — additive sum of bytes [8..35] (current_ua[14] only).
 *                Bytes [6..7] (checksum itself) are NOT included.
 *
 * OI-CHARGE-01 CLOSED — this frame enables np_charge_monitor_accumulate() to
 * be called in np_safety_main.c for every heartbeat that carries current data.
 */
typedef struct __attribute__((packed)) {
    uint8_t  magic[2];          /* NP_SAFETY_BEAT_MAGIC_0 / _1                */
    uint8_t  session_status;    /* NP_SESSION_STATUS_* bits                   */
    uint8_t  enable_lo;         /* requested enable bitmask bits 0-7          */
    uint8_t  enable_hi;         /* bits 8-15                                  */
    uint8_t  channel_count;     /* count of valid current_ua[] entries (0–14) */
    uint16_t checksum;          /* additive sum of bytes [0..5], wrapping u16 */
    uint16_t current_ua[NP_SAFETY_MAX_CHANNELS]; /* commanded µA per channel  */
    uint16_t ext_checksum;      /* additive sum of bytes [8..35], wrapping u16*/
} np_safety_rx_ext_frame_t;    /* 38 bytes; named from MCU's "receive" perspective */

/* Hub-perspective naming alias (hub transmits this frame to MCU) */
typedef np_safety_rx_ext_frame_t np_safety_tx_ext_frame_t;

/* Compile-time size assertions */
typedef char _np_spi_rx_ext_frame_size_check[
    (sizeof(np_safety_rx_ext_frame_t) == NP_SAFETY_RX_EXT_FRAME_LEN) ? 1 : -1
];

/* ── Session signature command frame (hub → MCU, 102 bytes) ──────────────── */
/*
 * Hub sends this once per session BEFORE the heartbeat that first requests a
 * non-zero enable_mask for a new session.
 *
 * Checksum: additive sum of bytes [0..NP_SAFETY_CMD_FRAME_LEN-3] (all except
 * the 2 checksum bytes at the end), stored little-endian, wrapping uint16.
 *
 * MCU concurrent TX during this transfer: all-zeros (hub discards via rx_dummy).
 * The definitive result is the NP_SAFETY_STATUS_SIG_PENDING bit in the next
 * heartbeat reply: cleared = verified, still set = rejected or not yet received.
 */
typedef struct __attribute__((packed)) {
    uint8_t  cmd_magic[2];                      /* NP_SAFETY_CMD_MAGIC_0 / _1 */
    uint8_t  cmd_type;                          /* NP_SAFETY_CMD_SESSION_SIG (0x01) */
    uint8_t  reserved;                          /* 0x00 */
    uint8_t  session_hash[NP_SESSION_HASH_LEN]; /* SHA-256 of session descriptor */
    uint8_t  session_sig[NP_ED25519_SIG_LEN];   /* Ed25519 signature (64 bytes) */
    uint16_t checksum;                          /* sum of bytes [0..99], wrapping uint16 */
} np_safety_sig_cmd_t;                          /* 2+1+1+32+64+2 = 102 bytes */

/* Compile-time size assertion */
typedef char _np_spi_sig_cmd_size_check[
    (sizeof(np_safety_sig_cmd_t) == NP_SAFETY_CMD_FRAME_LEN) ? 1 : -1
];

#endif /* NP_SPI_WIRE_TYPES_H */
