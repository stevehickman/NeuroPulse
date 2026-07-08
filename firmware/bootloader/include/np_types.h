/*
 * NeurOne Bootloader — Types and Status Codes
 * Document: NP-FW-EMMC-001 Rev A §8
 */

#ifndef NP_TYPES_H
#define NP_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "np_config.h"

/* ── Status codes ─────────────────────────────────────────────────────────── */
typedef enum {
    NP_OK                   =  0,
    NP_ERR_GENERIC          = -1,
    NP_ERR_INVALID_ARG      = -2,
    NP_ERR_TIMEOUT          = -3,
    NP_ERR_EMMC_INIT        = -4,
    NP_ERR_EMMC_READ        = -5,
    NP_ERR_EMMC_WRITE       = -6,
    NP_ERR_BAD_MAGIC        = -7,
    NP_ERR_BAD_HEADER_CRC   = -8,
    NP_ERR_BAD_SIGNATURE    = -9,
    NP_ERR_BAD_IMAGE_HASH   = -10,
    NP_ERR_IMAGE_TOO_LARGE  = -11,
    NP_ERR_OTA_DOWNLOAD     = -12,
    NP_ERR_OTA_VERIFY       = -13,
    NP_ERR_DFU_USB          = -14,
    NP_ERR_MAX_ATTEMPTS     = -15,
    NP_ERR_BOTH_BANKS_BAD   = -16,
} np_status_t;

/* ── Boot bank identifier ─────────────────────────────────────────────────── */
typedef enum {
    NP_BANK_A = 0,
    NP_BANK_B = 1,
} np_bank_t;

/* ── Firmware image header ────────────────────────────────────────────────── */
/* Located at offset 0 of each 128 MiB firmware bank partition.               */
/* Total size: NP_IMAGE_HEADER_SIZE (256 bytes).                              */
typedef struct __attribute__((packed)) {
    uint32_t magic;                         /* NP_IMAGE_MAGIC = 0x4E504657    */
    uint32_t version;                       /* Firmware version (monotonic)   */
    uint32_t image_size;                    /* Bytes of firmware after header */
    uint32_t header_crc32;                  /* CRC32 over bytes [0..15]       */
    uint8_t  image_sha256[NP_SHA256_SIZE];  /* SHA-256 of firmware image      */
    uint8_t  signature[NP_ED25519_SIG_SIZE];/* Ed25519 sig over sha256+version+size */
    uint8_t  reserved[NP_IMAGE_HEADER_SIZE
                       - 4U - 4U - 4U - 4U
                       - NP_SHA256_SIZE
                       - NP_ED25519_SIG_SIZE]; /* Pad to 256 bytes            */
} np_image_header_t;

/* Compile-time size check */
typedef char _np_hdr_size_check[
    (sizeof(np_image_header_t) == NP_IMAGE_HEADER_SIZE) ? 1 : -1];

/* ── i.MX RT1062 interrupt vector table (first 8 words used by bootloader) ── */
typedef struct {
    uint32_t initial_sp;    /* Initial stack pointer (MSP)                    */
    uint32_t reset_handler; /* Reset handler address (bit 0 set = Thumb)      */
    /* remaining vectors follow in application firmware */
} np_vtor_t;

/* ── OTA session state (persisted in Config partition) ───────────────────── */
typedef struct __attribute__((packed)) {
    uint32_t magic;                         /* 0x4E504F54 = "NPOT"            */
    uint8_t  target_bank;                   /* Bank being validated (0=A,1=B) */
    uint8_t  boot_attempts;                 /* Attempts since OTA staged       */
    uint16_t reserved;
    uint8_t  staged_sha256[NP_SHA256_SIZE]; /* Hash of staged image (written to bank) */
    uint32_t crc32;                         /* CRC32 over bytes [0..39]        */
} np_ota_state_t;

#define NP_OTA_STATE_MAGIC  0x4E504F54UL

#endif /* NP_TYPES_H */
