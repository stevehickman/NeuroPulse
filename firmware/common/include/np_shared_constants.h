/*
 * NeurOne Firmware — Shared Constants
 *
 * One definition for every constant that more than one firmware module needs.
 * A constant is defined here, once, and named identically everywhere it is
 * used; a module that needs one includes this file and never redefines it or
 * invents a second spelling.  (np_spi_wire_types.h includes this file, so the
 * hub and the safety MCU already have it.)
 *
 * Only preprocessor constants may live here (no includes): this header is
 * included by the bootloader, the safety MCU (IEC 62304 Class C) and the hub.
 */

#ifndef NP_SHARED_CONSTANTS_H
#define NP_SHARED_CONSTANTS_H

/* ── Cryptographic primitive sizes (bytes) ──────────────────────────────── */
#define NP_SHA256_SIZE              32U
#define NP_SHA512_SIZE              64U
#define NP_ED25519_PUBKEY_SIZE      32U
#define NP_ED25519_SIG_SIZE         64U

/* ── CRC-32 (IEEE 802.3, reflected) ─────────────────────────────────────── */
#define NP_CRC32_POLY               0xEDB88320UL

/* ── Bootloader ↔ application OTA contract ──────────────────────────────── */
#define NP_OTA_STATE_MAGIC          0x4E504F54UL  /* "NPOT" */
#define NP_BOOT_MAX_ATTEMPTS        3U

/* ── i.MX RT1062 SNVS register block base ───────────────────────────────── */
#define NP_SNVS_BASE                0x400D4000UL

/* ── Main processor ↔ safety MCU SPI heartbeat timing ───────────────────── */
#define NP_SAFETY_HEARTBEAT_MS      200U   /* main processor heartbeat period */
#define NP_SAFETY_WATCHDOG_MS       1500U  /* safety MCU watchdog; cutoff on expiry */

/* ── EEG front end (ADS1299) ────────────────────────────────────────────── */
#define NP_EEG_CHANNELS             8U     /* T1: Fp1/2 F3/4 C3/4 P3/4 semi-dry */
#define NP_EEG_SAMPLE_RATE_HZ       500U

/* ── Cardiac interlock re-enable lockout ────────────────────────────────── */
/* The safety MCU holds cVNS off this long after a cardiac cutoff; the hub
 * observes the cutoff up to one heartbeat later and waits the same period, so
 * its window always contains the MCU's.  One definition keeps them equal. */
#define NP_CARDIAC_LOCKOUT_MS       30000U

/* ── Auricular VNS (incl. taVNS) current ceiling ────────────────────────── */
#define NP_VNS_MAX_CURRENT_UA       2000U  /* 2 mA absolute limit */

/* ── Physiological R-R interval validity (HRV and cVNS R-peak) ──────────── */
#define NP_RR_MIN_MS                300U   /* ≈200 BPM upper HR limit */
#define NP_RR_MAX_MS                2000U  /* ≈30 BPM lower HR limit */

/* ── ZONE_ID ADC front end on the hub PCB (PBM + zone announce) ─────────── */
#define NP_ZONE_ID_ADC_BITS         12U
#define NP_ZONE_ID_ADC_VREF_MV      3300U
#define NP_ZONE_ID_ADC_PULLUP_OHMS  10000U  /* 10 kΩ pull-up on hub PCB */

#endif /* NP_SHARED_CONSTANTS_H */
