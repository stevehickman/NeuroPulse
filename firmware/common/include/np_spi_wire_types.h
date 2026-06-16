/*
 * NeuroPulse Firmware — Shared SPI Wire-Format Types
 * Document: NP-FW-HUB-001 Rev A §7, NP-SW-001 Rev A
 *
 * Single source of truth for the 102-byte session signature command frame
 * shared between the STM32G071 safety MCU (SW-01, Class C) and the i.MX RT1062
 * hub control program (SW-02, Class B).
 *
 * Include from BOTH sides:
 *   firmware/safety_mcu/include/np_safety_protocol.h
 *   firmware/hub_control/include/np_hub_types.h
 *
 * No MCU-specific or hub-specific headers may be included from this file.
 * Only <stdint.h> is permitted.
 *
 * OI-SW01-M07-01 CLOSED — struct and constants originally in:
 *   np_safety_protocol.h (MCU) and np_hub_types.h (hub); now single definition.
 */

#ifndef NP_SPI_WIRE_TYPES_H
#define NP_SPI_WIRE_TYPES_H

#include <stdint.h>

/* ── Session signature command constants ────────────────────────────────── */

#define NP_SAFETY_CMD_MAGIC_0       0xC0U   /* distinguishes from heartbeat 0xBE */
#define NP_SAFETY_CMD_MAGIC_1       0xDEU
#define NP_SAFETY_CMD_SESSION_SIG   0x01U   /* cmd_type: deliver hash + sig */

/* 2 (magic) + 1 (type) + 1 (rsvd) + 32 (hash) + 64 (sig) + 2 (checksum) = 102 */
#define NP_SAFETY_CMD_FRAME_LEN     102U
#define NP_SESSION_HASH_LEN         32U     /* SHA-256 hash of session descriptor */
#define NP_ED25519_SIG_LEN          64U     /* Ed25519 signature */

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
