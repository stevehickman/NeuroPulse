/*
 * NeurOne Firmware — Shared SPI Wire-Format Types
 * Document: NP-FW-HUB-001 Rev 1 §7, NP-SW-001 Rev 1
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

/* ── Heartbeat session_status bits (shared) ─────────────────────────────── */
/* Bit 0 (NP_SESSION_STATUS_ACTIVE) and bit 1 (NP_SESSION_STATUS_CVNS_REENABLE)
 * are defined per-side in np_safety_protocol.h / np_hub_config.h.  Bit 2 is
 * defined HERE so both the safety MCU and the hub share one source of truth. */
#define NP_SESSION_STATUS_GEOM_REQUIRED  (1U << 2)  /* OI-CHARGE-03: session needs an
                                                     * electrode-geometry override; safety
                                                     * MCU must not grant CLIN_STIM until a
                                                     * valid area command has been applied */

/* OI-CHARGE-04: the same fail-closed gate for the T1 tDCS channel.  A SEPARATE
 * bit, not a widening of bit 2, because the gate is per-channel: bit 2 gates
 * CLIN_STIM only and bit 3 gates TDCS only.  One shared "geometry required"
 * bit would have coupled them — a session carrying T1 tDCS (geometry declared)
 * alongside clinical tACS (which shares the CLIN_STIM enable bit and declares
 * no geometry) would have had clinical tACS gated off by the tDCS declaration.
 * Both bits are advertised on EVERY heartbeat, so a lost frame cannot disarm
 * either gate.                                                               */
#define NP_SESSION_STATUS_GEOM_REQ_TDCS  (1U << 3)  /* OI-CHARGE-04: session needs a tDCS
                                                     * electrode-geometry declaration; safety
                                                     * MCU must not grant TDCS until a valid
                                                     * area command has been applied     */

/* ── Session signature command constants ────────────────────────────────── */

#define NP_SAFETY_CMD_MAGIC_0       0xC0U   /* distinguishes from heartbeat 0xBE */
#define NP_SAFETY_CMD_MAGIC_1       0xDEU
#define NP_SAFETY_CMD_SESSION_SIG   0x01U   /* cmd_type: deliver hash + sig */

/* 2 (magic) + 1 (type) + 1 (rsvd) + 32 (hash) + 64 (sig) + 2 (checksum) = 102 */
#define NP_SAFETY_CMD_FRAME_LEN     102U
#define NP_SESSION_HASH_LEN         32U     /* SHA-256 hash of session descriptor */
#define NP_ED25519_SIG_LEN          64U     /* Ed25519 signature */

/* ── Per-channel electrode-geometry charge limit command (OI-CHARGE-02) ──── */

#define NP_SAFETY_CMD_CHAN_LIMIT    0x02U   /* cmd_type: per-channel electrode area */

/* 2 (magic) + 1 (type) + 1 (rsvd) + 28 (area[14]) + 2 (checksum) = 34 */
#define NP_SAFETY_CHAN_LIMIT_FRAME_LEN  34U

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
 * ── Privacy gate (NP-FW-EMMC-001 Rev 1 §12) ──────────────────────────────
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

/* ── Per-channel charge-limit command frame (hub → MCU, 34 bytes) ────────── */
/*
 * OI-CHARGE-02.  Sent once per session, during session setup, when the
 * descriptor contains a modality whose electrode geometry differs from the
 * default 25cm² tDCS pad (notably T2 HD-tDCS 3.5mm Ag/AgCl electrodes).
 *
 * The hub delivers per-channel electrode AREA in milli-cm² (1 unit = 0.001cm²);
 * it does NOT deliver a pre-computed charge limit.  Both charge-density
 * constants (NP_CHARGE_PHASE_LIMIT_UC_CM2, NP_CHARGE_DC_LIMIT_MC_CM2) stay
 * resident on the Class C safety MCU, which converts area→limits:
 *   per-phase limit_nc  = NP_CHARGE_PHASE_LIMIT_UC_CM2 × area_mcm2
 *   per-session limit_nc = NP_CHARGE_DC_LIMIT_MC_CM2 × 1000 × area_mcm2
 * A value of 0 for a channel means "keep the current (default) limit".
 * OI-CHARGE-05: which of the two applies is the companion
 * np_safety_chan_wave_cmd_t declaration below, not anything in this frame.
 *
 * The hub floors the area (truncates toward zero) so the derived limit can
 * never exceed the true 40µC/cm² ceiling — conservative by construction.
 *
 * Distinguished from the 8/38/102-byte frames by its NSS-delineated transfer
 * length (34).  Shares the 0xC0/0xDE command magic with cmd_type discriminator.
 *
 * Checksum: additive sum of bytes [0..NP_SAFETY_CHAN_LIMIT_FRAME_LEN-3] (all
 * except the 2 checksum bytes at the end), wrapping uint16.
 *
 * Privacy: area_mcm2[] is fixed device geometry (SHDR) — carries no user data.
 */
typedef struct __attribute__((packed)) {
    uint8_t  cmd_magic[2];                        /* NP_SAFETY_CMD_MAGIC_0 / _1 */
    uint8_t  cmd_type;                            /* NP_SAFETY_CMD_CHAN_LIMIT (0x02) */
    uint8_t  reserved;                            /* 0x00 */
    uint16_t area_mcm2[NP_SAFETY_MAX_CHANNELS];   /* electrode area, milli-cm²; 0 = keep default */
    uint16_t checksum;                            /* sum of bytes [0..31], wrapping uint16 */
} np_safety_chan_limit_cmd_t;                     /* 2+1+1+28+2 = 34 bytes */

/* Compile-time size assertion */
typedef char _np_spi_chan_limit_cmd_size_check[
    (sizeof(np_safety_chan_limit_cmd_t) == NP_SAFETY_CHAN_LIMIT_FRAME_LEN) ? 1 : -1
];

/* ── Per-channel WAVEFORM-CLASS declaration (OI-CHARGE-05 (b)) ───────────── */
/*
 * Sent once per session during setup, alongside np_safety_chan_limit_cmd_t,
 * for every electrical channel the descriptor will command.
 *
 * WHY THIS FRAME EXISTS.  The safety MCU has to apply a DIFFERENT ceiling to a
 * DC channel than to a charge-balanced one — a per-session integral of |I|·dt
 * against a mC/cm² budget for the first, a per-phase amplitude × phase-width
 * product against a µC/cm² budget for the second (see np_safety_config.h).  It
 * cannot derive which is which on its own: `current_ua[]` in the heartbeat is
 * a magnitude and carries no waveform information, and the MCU deliberately
 * holds no modality→channel map (that map moves with the descriptor grammar,
 * and putting it behind the Class C boundary would make a grammar change a
 * recertification — the same argument NP-HW-HUB-001 §7.2.1 made against
 * per-cluster enable bits).
 *
 * WHAT STAYS ON THE MCU.  Only the CLASSIFICATION and the phase DURATION
 * cross this boundary.  Both ceilings — NP_CHARGE_PHASE_LIMIT_UC_CM2 and
 * NP_CHARGE_DC_LIMIT_MC_CM2 — stay resident on the Class C MCU and are never
 * transmitted, exactly as OI-CHARGE-02 established for the density constant.
 * The hub says "this channel is sinusoidal with a 12,500 µs half-period"; the
 * MCU alone decides what that is allowed to cost.
 *
 * FAIL-CLOSED.  A wave_class of 0 means "not declared".  np_charge_monitor.c
 * clears any ELECTRICAL channel with an undeclared class from granted_mask
 * (np_charge_monitor_decl_gate), so a lost or corrupt frame leaves the
 * modality disabled rather than unmonitored.  This is the OI-CHARGE-03
 * precedent applied to the waveform axis, and it needs no new heartbeat bit:
 * "declared" is per-channel state the MCU already holds, and the electrical
 * channel set is fixed at compile time (NP_SAFETY_CH_ELECTRICAL_MASK).
 *
 * Privacy: both fields are authored protocol parameters (SHDR — what was
 * asked for), never measurements.  Same classification as current_ua[].
 *
 * Distinguished from the 8/34/38/102-byte frames by its NSS-delineated
 * transfer length (76).  Shares the 0xC0/0xDE command magic.
 *
 * Checksum: additive sum of bytes [0..NP_SAFETY_CHAN_WAVE_FRAME_LEN-3],
 * wrapping uint16.
 */

#define NP_SAFETY_CMD_CHAN_WAVE     0x03U   /* cmd_type: per-channel waveform class */

/* 2 (magic) + 1 (type) + 1 (rsvd) + 14 (class[14]) + 56 (phase_us[14]) + 2 = 76 */
#define NP_SAFETY_CHAN_WAVE_FRAME_LEN   76U

/* Waveform-class bits.  A channel may carry MORE THAN ONE: NP_SAFETY_CH_CLIN_STIM
 * is shared by HD-tDCS (DC) and clinical tACS (AC), and a session containing
 * both must be held to BOTH ceilings rather than to whichever was written
 * last.  The monitor ORs the declarations and runs every check that applies. */
#define NP_CHARGE_WAVE_DC       (1U << 0)  /* direct current — per-session mC/cm²  */
#define NP_CHARGE_WAVE_PULSE    (1U << 1)  /* rectangular biphasic — per-phase µC/cm² */
#define NP_CHARGE_WAVE_SINE     (1U << 2)  /* sinusoidal — per-phase, 2/π of I×T   */
#define NP_CHARGE_WAVE_ALL      (NP_CHARGE_WAVE_DC | NP_CHARGE_WAVE_PULSE | \
                                 NP_CHARGE_WAVE_SINE)

/* Longest phase duration either side will honour: 2,000,000 µs = 0.25 Hz.
 * Below the 0.5 Hz BES/tACS floor (CLAUDE.md §3), so it clamps nothing real.
 *
 * Shared rather than per-side because both ends clamp: the hub so the frame it
 * transmits already says what the MCU will conclude, and the MCU because it
 * must not depend on the hub having done so — a corrupt or hostile phase_us
 * would otherwise overflow the uint64 per-phase arithmetic.  Two independent
 * clamps against one number is only meaningful if it IS one number.
 *
 * Note the direction: this clamps a declaration DOWN, and a shorter declared
 * phase yields a SMALLER computed charge.  So the clamp is not itself a safety
 * control — it is bounds-checking, and what makes an under-declared phase safe
 * is the fail-closed declaration gate, not this.                            */
#define NP_CHARGE_MAX_PHASE_US      2000000UL

typedef struct __attribute__((packed)) {
    uint8_t  cmd_magic[2];                          /* NP_SAFETY_CMD_MAGIC_0 / _1 */
    uint8_t  cmd_type;                              /* NP_SAFETY_CMD_CHAN_WAVE (0x03) */
    uint8_t  reserved;                              /* 0x00 */
    uint8_t  wave_class[NP_SAFETY_MAX_CHANNELS];    /* NP_CHARGE_WAVE_* bits; 0 = undeclared */
    uint32_t phase_us[NP_SAFETY_MAX_CHANNELS];      /* phase duration µs; 0 for pure DC */
    uint16_t checksum;                              /* sum of bytes [0..73], wrapping uint16 */
} np_safety_chan_wave_cmd_t;                        /* 2+1+1+14+56+2 = 76 bytes */

/* Compile-time size assertion */
typedef char _np_spi_chan_wave_cmd_size_check[
    (sizeof(np_safety_chan_wave_cmd_t) == NP_SAFETY_CHAN_WAVE_FRAME_LEN) ? 1 : -1
];

/* ── Extended MCU→hub impedance report (OI-CVNS-HUB-11) ──────────────────── */
/*
 * The safety MCU's contact-impedance check (SW01-M06) measures the cervical VNS
 * electrode impedance independently of the hub's own per-electrode measurement
 * (OI-CVNS-HUB-09).  To let the hub cross-validate the two numerically —
 * mirroring the cardiac-baseline cross-check (NP_CVNS_BASELINE_CROSSVAL_BPM,
 * main-processor vs safety-MCU HR) — the MCU reports its per-electrode kΩ back
 * to the hub.
 *
 * The report is carried in the SPARE bytes of the 38-byte heartbeat window.
 * During each heartbeat the MCU (SPI slave) clocks out its 8-byte reply frame
 * in bytes [0..7] and this 8-byte report in bytes [8..15] on MISO; the hub reads
 * the whole 38-byte receive buffer.  This adds NO new SPI transfer, NO new
 * NSS-delineated length, and does not touch the base 8-byte reply frame or its
 * checksum.  Bytes [16..19] carry the cardiac-status report below
 * (np_safety_nv_report_t); bytes [20..37] stay zero (unused).
 *
 * ── Privacy gate (NP-FW-EMMC-001 Rev 1 §12) ──────────────────────────────
 * cvns_kohm_x100[] is measured tissue impedance → UHDR (user biology).  It is
 * transferred device-internally (MCU → hub) purely for cross-validation and is
 * NEVER written to SHDR.  Only a per-device DIVERGENCE FLAG (a device-condition
 * signal, no biology) may be recorded to SHDR.  The hub already measures and
 * records the same impedance into the library's UHDR fields (OI-CVNS-HUB-09);
 * this transfer keeps impedance device-internal and adds no new SHDR exposure.
 */
#define NP_SAFETY_IMP_REPORT_MAGIC     0x5AU  /* marks bytes [8..] as a valid report */
#define NP_SAFETY_IMP_CVNS_ELECTRODES  2U     /* MUST equal NP_CVNS_ELECTRODE_COUNT   */
#define NP_SAFETY_IMP_REPORT_OFFSET    8U     /* byte offset within 38-byte MISO window */

/* flags bits */
#define NP_SAFETY_IMP_FLAG_CVNS_VALID  (1U << 0)  /* cvns_kohm_x100[] holds a completed
                                                   * per-electrode CVNS measurement    */

/* 1 (magic) + 1 (flags) + 4 (kohm[2]) + 2 (checksum) = 8 bytes */
typedef struct __attribute__((packed)) {
    uint8_t  magic;        /* NP_SAFETY_IMP_REPORT_MAGIC when populated, else 0 */
    uint8_t  flags;        /* NP_SAFETY_IMP_FLAG_* */
    uint16_t cvns_kohm_x100[NP_SAFETY_IMP_CVNS_ELECTRODES]; /* [0]=left, [1]=right; kΩ×100 */
    uint16_t checksum;     /* additive sum of bytes [0..5], wrapping uint16 */
} np_safety_imp_report_t;  /* 8 bytes; fits window bytes [8..15] */

#define NP_SAFETY_IMP_REPORT_LEN  8U

/* Compile-time size assertions */
typedef char _np_spi_imp_report_size_check[
    (sizeof(np_safety_imp_report_t) == NP_SAFETY_IMP_REPORT_LEN) ? 1 : -1
];
/* The report must fit in the MISO window after the 8-byte base reply. */
typedef char _np_spi_imp_report_fits_check[
    ((NP_SAFETY_IMP_REPORT_OFFSET + NP_SAFETY_IMP_REPORT_LEN)
        <= NP_SAFETY_RX_EXT_FRAME_LEN) ? 1 : -1
];

/* ── Active-user command frame (NP-SW-FAULTMSG-001, per-user cardiac scope) ── */
/*
 * Tells the safety MCU which person is using the device, so that a cervical VNS
 * cardiac cutoff is held for THAT person and nobody else (principal decision,
 * 2026-09-22).  The tag is opaque: a random 32-bit value the app derives from
 * its individual profile, carrying no name or identity.  The MCU persists the
 * current tag in flash (np_nv_state.c), so the same user is assumed across
 * sessions and power cycles until the app says otherwise — including in Mode 3.
 *
 * Accepted only between sessions (no session active, nothing granted); a frame
 * that arrives mid-session is ignored.  Checksum: additive sum of bytes [0..7].
 *
 * Two tag values are reserved and never sent by the app:
 *   NP_SAFETY_USER_UNSPECIFIED  no user has ever been named — single-user use
 *   NP_SAFETY_USER_ANY          internal: a cutoff that cannot be attributed to
 *                               one person (a torn flash record, a full table)
 */
#define NP_SAFETY_CMD_ACTIVE_USER     0x04U   /* cmd_type: set the active user tag */
#define NP_SAFETY_USER_FRAME_LEN      10U
#define NP_SAFETY_USER_UNSPECIFIED    0x00000000UL
#define NP_SAFETY_USER_ANY            0xFFFFFFFFUL

typedef struct __attribute__((packed)) {
    uint8_t  cmd_magic[2];     /* NP_SAFETY_CMD_MAGIC_0 / _1 */
    uint8_t  cmd_type;         /* NP_SAFETY_CMD_ACTIVE_USER  */
    uint8_t  reserved;         /* 0x00 */
    uint32_t user_tag;         /* little-endian opaque tag   */
    uint16_t checksum;         /* sum of bytes [0..7], wrapping uint16 */
} np_safety_user_cmd_t;        /* 2+1+1+4+2 = 10 bytes */

typedef char _np_spi_user_cmd_size_check[
    (sizeof(np_safety_user_cmd_t) == NP_SAFETY_USER_FRAME_LEN) ? 1 : -1
];

/* ── MCU→hub cardiac-status report (NP-SW-FAULTMSG-001 P4, blanket warning) ─ */
/*
 * Two facts the app needs at connect, carried in the spare MISO window bytes
 * right after the impedance report — no new transfer, no new length:
 *   USER_BLOCKED  the ACTIVE user has an outstanding cardiac cutoff, so the MCU
 *                 withholds cervical VNS for them;
 *   OUTSTANDING   SOMEONE on this device has one (the active user or another).
 * The second drives the blanket warning every user sees, so switching profiles
 * cannot be used to get around a block unseen.  WHICH user is never reported.
 *
 * Privacy: cardiac events are user biology → UHDR.  These flags go to the
 * user's own app for display only and are NEVER written to SHDR.
 */
#define NP_SAFETY_NV_REPORT_MAGIC       0x6CU
#define NP_SAFETY_NV_REPORT_OFFSET      (NP_SAFETY_IMP_REPORT_OFFSET + NP_SAFETY_IMP_REPORT_LEN)
#define NP_SAFETY_NV_FLAG_USER_BLOCKED  (1U << 0)
#define NP_SAFETY_NV_FLAG_OUTSTANDING   (1U << 1)

typedef struct __attribute__((packed)) {
    uint8_t  magic;            /* NP_SAFETY_NV_REPORT_MAGIC */
    uint8_t  flags;            /* NP_SAFETY_NV_FLAG_*       */
    uint16_t checksum;         /* additive sum of bytes [0..1] */
} np_safety_nv_report_t;       /* 4 bytes; window bytes [16..19] */

#define NP_SAFETY_NV_REPORT_LEN  4U

typedef char _np_spi_nv_report_size_check[
    (sizeof(np_safety_nv_report_t) == NP_SAFETY_NV_REPORT_LEN) ? 1 : -1
];
typedef char _np_spi_nv_report_fits_check[
    ((NP_SAFETY_NV_REPORT_OFFSET + NP_SAFETY_NV_REPORT_LEN)
        <= NP_SAFETY_RX_EXT_FRAME_LEN) ? 1 : -1
];

#endif /* NP_SPI_WIRE_TYPES_H */
