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

#include "np_shared_constants.h"

/* ── Channel count (common to both frame types) ─────────────────────────── */

/* Must match the s_charge_nc[] array size in np_charge_monitor.c and the
 * number of NP_SAFETY_EN_* bits defined in np_safety_protocol.h.            */
#define NP_SAFETY_MAX_CHANNELS      14U

/* ── Safety wire constants (one definition for both processors) ─────────── */
/* Defined here and nowhere else: the hub (np_hub_config.h) and the safety MCU
 * (np_safety_protocol.h, np_safety_config.h) both include this file, so the
 * two sides cannot drift.                                                    */

/* ── Magic bytes ──────────────────────────────────────────────────────────── */
#define NP_SAFETY_FRAME_LEN     8U      /* MCU reply frame; heartbeat RX is NP_SAFETY_RX_EXT_FRAME_LEN */
#define NP_SAFETY_BEAT_MAGIC_0  0xBEU
#define NP_SAFETY_BEAT_MAGIC_1  0xA7U

/* ── Enable bitmask bits ──────────────────────────────────────────────────── */
/*
 * Bit 0 gates ALL cranial PBM (NP-HW-HUB-001 Rev 3 §7.2).  It replaces the five
 * per-zone bits of Rev 2, which were retired for two independent reasons:
 *
 *  1. "Zone" is not a firmware concept.  A zone is a human-authored set of
 *     sockets in protocols/predefined/00-zones.npps; changing that membership
 *     needs no hardware change, so under the §4.5.1 discriminator (quoted at
 *     length in hub_control/include/np_module_map.h) firmware must not model it.
 *     Cranial PBM as a whole IS a hardware property, so one bit for it is legal.
 *  2. Five was the retired zone-slot count, and the mask happened to have five
 *     spare bits — an artifact, not a derived safety requirement.
 *
 * Why one bit is sufficient, not merely convenient: the safety MCU's function is
 * the interlock — cut stimulation — not dose selectivity.  Cutting all cranial
 * PBM is always a safe response; over-cutting is a usability cost, never a
 * hazard.  Per-socket thermal response does not go through this MCU anyway (the
 * per-tile 62 °C junction limit is a hardware current throttle, and per-socket
 * dose shutdown is a Class B duty=0 write, NP-FW-PBM1064-001 §6.5).  Physical
 * gating still distributes — one gate transistor per cluster on each cluster's
 * LED drive rail (NP-HW-HEXTILE-001 D-8, 18 high-side switches) — but fanned out
 * from this ONE policy bit (§7.4).
 *
 * ACCEPTED CONSEQUENCE, confirmed by safety review: a safety-layer cut is
 * all-or-nothing across the cranial lattice.  A single socket cannot be
 * safety-cut without the whole lattice.
 *
 * OI-HUB-C07 / OI-HEXTILE-13 CLOSED 2026-08-16 — per-cluster POLICY bits are
 * DECIDED AGAINST (NP-HW-HUB-001 Rev 4 §7.2.1, NP-HW-HEXTILE-001 Rev 5 §8.4.1).
 * This enable word is therefore FINAL as laid out below: it is NOT widened to
 * uint32_t, and no per-cluster enable bits are added.  Two reasons, either
 * sufficient:
 *
 *  - No hazard in the tree has an extent of one cluster.  Extents are physical
 *    — a tile heats (RISK-26, per-tile NTC throttle, Class B), a modality
 *    accumulates charge (its own bit here), a rail collapses (watchdog, all
 *    bits) — while a cluster is a clamp-plate/FPC boundary.  A control at a
 *    granularity matching no hazard's extent is not a safety control.
 *  - 18 cluster bits would require this MCU to hold a socket->cluster map, which
 *    is topological and moves with MECH-2 / REG-1.  Behind the Class C boundary
 *    a re-clustering becomes a recertification, and a stale map can cut the
 *    wrong cluster while leaving the faulted one energised.
 *
 * The 18 per-cluster gates still exist in hardware — they are IEC 62304 Class B
 * availability gates, commanded by the hub (HUB-REQ-C05), in series with this
 * bit.  R-11 holds: this MCU still physically owns the enable path, and no
 * Class B fault can re-energise a lattice this bit has cut.
 *
 * Reopening requires one of: a physical process with a one-cluster extent; the
 * cluster ceasing to be topological; or the Class B tier proving unable to
 * deliver per-cluster availability.  See NP-HW-HEXTILE-001 §8.4.1.
 */
#define NP_SAFETY_EN_PBM_CRANIAL    (1U << 0)

/*
 * Bits 1–4: RESERVED — NOT REUSED.  Formerly NP_SAFETY_EN_PBM_ZONE_1..4.
 * Excluded from NP_SAFETY_EN_ALL_MASK, so np_spi_watchdog_tick strips them and
 * they can never enable anything.
 *
 * What that exclusion is, and is not (corrected 2026-08-12, NP-HW-HUB-001
 * Rev 4 §7.2): NO hub can set one of these bits.  No hub hardware exists, and
 * the NP_SAFETY_EN_PBM_ZONE_0..4 macros were DELETED in the same 2026-08-05
 * change that created these holes — they survive only in "Formerly ..."
 * comments like this one.  So the mask exclusion is defence in depth against a
 * FUTURE authoring error re-introducing those positions, not compatibility with
 * a deployed or legacy hub.  Stated precisely because a guard described as
 * mitigating a live hazard reads as load-bearing when it is currently vacuous.
 * The holes themselves are NOT vacuous — see reason (b) below, which binds now.
 *
 * Two independent reasons the holes stay holes:
 *
 *  a. SHDR fault records.  Enable-bit positions appear in SHDR device-health
 *     fault records; silently recycling a position would make historical logs
 *     misread (NP-HW-HUB-001 Rev 3 §7.2).  CAVEAT, current as of 2026-08-05:
 *     no SHDR fault records have been generated yet (principal, 2026-08-04,
 *     NP-HW-HEXTILE-001 §8.4.2 finding 4), so THIS rationale does not bind
 *     today.  It begins binding the moment a real fault record exists.
 *  b. Bit position IS a charge-monitor channel index — and this one binds now.
 *     NP_SAFETY_CH_CLIN_STIM below is defined as a bit position; np_safety_main.c
 *     tests `granted_mask & (1U << ch)` against rx.current_ua[ch]; and
 *     s_charge_nc[]/NP_SAFETY_MAX_CHANNELS are sized off the enable-bit count.
 *     Enable-bit position ≡ current_ua[] slot ≡ charge accumulator index is a
 *     three-way identity, and it is Class C — the 40 µC/cm² limit rests on it.
 *     Compacting the word would move every modality bit down four positions and
 *     change what every current_ua[i] slot means (NP-HW-HEXTILE-001 §8.4.2,
 *     raised as OI-HEXTILE-14).
 *
 * Reason (b) is why the no-SHDR-records finding does NOT make recycling safe.
 */
#define NP_SAFETY_EN_BES_TACS       (1U << 5)
#define NP_SAFETY_EN_TDCS           (1U << 6)
#define NP_SAFETY_EN_VNS_HRV        (1U << 7)
#define NP_SAFETY_EN_VISUAL         (1U << 8)
#define NP_SAFETY_EN_INTRANASAL     (1U << 9)
#define NP_SAFETY_EN_CVNS           (1U << 10)
#define NP_SAFETY_EN_TMS            (1U << 11)
#define NP_SAFETY_EN_PBM_1170NM     (1U << 12)
#define NP_SAFETY_EN_CLIN_STIM      (1U << 13)

/* Charge-monitor channel INDEX for CLIN_STIM (= bit position of the enable
 * bit above).  HD-tDCS accumulates charge on this channel and is subject to
 * the OI-CHARGE-03 fail-safe geometry gate.  See the reserved-bits note above:
 * this identity is why bit positions above 4 must never shift.               */
#define NP_SAFETY_CH_CLIN_STIM      13U

/* Charge-monitor channel INDICES for the remaining electrical channels
 * (= bit positions of the NP_SAFETY_EN_* bits above).  Added with OI-CHARGE-05
 * because these channels now carry per-channel declarations and per-channel
 * commanded current, so the hub needs to name their slots — the same
 * bit-position ≡ current_ua[] slot ≡ accumulator index identity documented in
 * the reserved-bits note above, which is why positions above 4 must never
 * shift.                                                                     */
#define NP_SAFETY_CH_BES_TACS       5U
#define NP_SAFETY_CH_VNS_HRV        7U
#define NP_SAFETY_CH_CVNS           10U

/* Charge-monitor channel INDEX for the T1 tDCS channel (= bit position of
 * NP_SAFETY_EN_TDCS above).  OI-CHARGE-04: tDCS now declares its electrode
 * geometry in the signed session descriptor, the hub delivers that area on
 * this channel, and the fail-safe geometry gate covers it — so the 25 cm²
 * NP_ELECTRODE_AREA_CM2 fallback is never what a tDCS session actually runs
 * against.  Same bit-position ≡ current_ua[] slot ≡ accumulator index
 * identity as CLIN_STIM; see the reserved-bits note above.                  */
#define NP_SAFETY_CH_TDCS           6U

/* The bit-position ≡ channel-index identity, asserted rather than commented.
 * It is Class C (both charge ceilings rest on it) and it is now relied on by
 * the hub as well as this MCU, so a shifted enable bit must be a compile
 * error and not a silently mis-indexed accumulator.  Same C99-compatible
 * idiom as the frame-size checks in np_spi_wire_types.h.                    */
typedef char _np_safety_ch_index_check[
    ((NP_SAFETY_EN_BES_TACS  == (1U << NP_SAFETY_CH_BES_TACS))  &&
     (NP_SAFETY_EN_TDCS      == (1U << NP_SAFETY_CH_TDCS))      &&
     (NP_SAFETY_EN_VNS_HRV   == (1U << NP_SAFETY_CH_VNS_HRV))   &&
     (NP_SAFETY_EN_CVNS      == (1U << NP_SAFETY_CH_CVNS))      &&
     (NP_SAFETY_EN_CLIN_STIM == (1U << NP_SAFETY_CH_CLIN_STIM))) ? 1 : -1
];

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

/* ── Heartbeat session_status bits (shared) ─────────────────────────────── */
/* Bits 0 and 1 (NP_SESSION_STATUS_ACTIVE, NP_SESSION_STATUS_CVNS_REENABLE) are
 * defined above; bit 2 and up follow.  All are shared by the safety MCU and the
 * hub.  These are BIT FLAGS — never write an np_session_state_t enum value
 * into the session_status byte (use np_safety_session_status_bits()).        */
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

/* OI-MMSOCK-02 (NP-FW-MMSOCK-001 §3.6.1, §5.3 C-1): the same fail-closed gate
 * for the BES/tACS channel.  Before this bit, BES/tACS was the one electrical
 * channel with no declared geometry: it was always checked against the 25 cm²
 * NP_ELECTRODE_AREA_CM2 fallback, which is ~24x permissive for a ≤1.02 cm²
 * T1-B lattice electrode.  Its own bit, for the same per-channel reason bit 3
 * is separate from bit 2.                                                   */
#define NP_SESSION_STATUS_GEOM_REQ_BES   (1U << 4)  /* OI-MMSOCK-02: session needs a BES/tACS
                                                     * electrode-geometry declaration; safety
                                                     * MCU must not grant BES_TACS until a
                                                     * valid area command has been applied */

/* NP-FMEA-001 OI-FMEA-12 (a), FMEA-M02-03: bits 5–7 carry a 3-bit heartbeat
 * sequence counter.  The hub advances it by one on EVERY heartbeat it sends,
 * including one whose transfer failed, so a retry is never a repeat.  Magic
 * and checksums show a frame is well formed, not that it is new: a hung hub
 * whose SPI keeps re-sending its last buffer would otherwise hold the 1.5 s
 * watchdog off indefinitely.  The safety MCU resets the watchdog only on a
 * forward run of the counter (np_spi_watchdog_seq_accept()).  The field sits
 * inside the base checksum (bytes [0..5]), and the frame length is unchanged. */
#define NP_SESSION_STATUS_SEQ_SHIFT      5U
#define NP_SESSION_STATUS_SEQ_MASK       (7U << NP_SESSION_STATUS_SEQ_SHIFT)
#define NP_HEARTBEAT_SEQ_MODULUS         8U

/* ── Session signature command constants ────────────────────────────────── */

#define NP_SAFETY_CMD_MAGIC_0       0xC0U   /* distinguishes from heartbeat 0xBE */
#define NP_SAFETY_CMD_MAGIC_1       0xDEU
#define NP_SAFETY_CMD_SESSION_SIG   0x01U   /* cmd_type: deliver hash + sig */

/* 2 (magic) + 1 (type) + 1 (rsvd) + 32 (hash) + 64 (sig) + 2 (checksum) = 102 */
#define NP_SAFETY_CMD_FRAME_LEN     102U
#define NP_SESSION_HASH_LEN         NP_SHA256_SIZE  /* SHA-256 hash of session descriptor */

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
    uint8_t  session_sig[NP_ED25519_SIG_SIZE];   /* Ed25519 signature (64 bytes) */
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

/* ── Hub heart-rate report (OI-CVNS-14, NP-FW-CVNS-001 Rev 14 §14.7) ─────────── */
/*
 * The main processor's own heart-rate estimate, sent while cervical VNS is in
 * use so the safety MCU can compare it with the rate it measures itself from
 * RPEAK_IN.  A plausible false rhythm on RPEAK_IN (a free-running pulse source,
 * a SW-02 timer fault) is invisible to the MCU alone: staleness never fires and
 * every interval is in range.  This frame is the second, independent observation
 * that case 5 of §14.4.1 needed.
 *
 * hr_x10 is the mean of the last NP_CARDIAC_XCHECK_HR_INTERVALS R-R intervals
 * the hub's detector accepted, in 0.1 BPM — the same span as the MCU's own mean
 * (np_cardiac_interlock.c, NP_RR_BUF_SIZE), so a real change moves both
 * estimates over the same number of beats.  age_ms is how old the newest of
 * those beats was when the frame was built; the MCU backdates the report by it.
 * flags bit 0 clear means the hub has no estimate: the MCU treats that exactly
 * like no report.
 *
 * Distinguished from the 8/10/34/38/76/102-byte frames by its NSS-delineated
 * transfer length (12).  Shares the 0xC0/0xDE command magic.  Accepted at any
 * time (it carries no state the MCU must protect) and newest-wins.
 * Checksum: additive sum of bytes [0..9], wrapping uint16.
 *
 * Privacy: hr_x10 is the wearer's heart rate — UHDR.  It is device-internal
 * (hub → MCU), is never logged to SHDR, and is never forwarded.  The MCU keeps
 * it in RAM only, and drops it with the session.
 */
#define NP_SAFETY_CMD_HR_REPORT       0x05U   /* cmd_type: hub heart-rate estimate */
#define NP_SAFETY_HR_REPORT_FRAME_LEN 12U
#define NP_SAFETY_HR_FLAG_VALID       (1U << 0)

/* Averaging span of hr_x10 on the hub side: the number of most recent R-R
 * intervals.  Shared so the hub's span and the MCU's own mean (NP_RR_BUF_SIZE,
 * np_cardiac_interlock.c, which static-asserts equality) cannot drift apart. */
#define NP_SAFETY_HR_REPORT_INTERVALS 8U

typedef struct __attribute__((packed)) {
    uint8_t  cmd_magic[2];     /* NP_SAFETY_CMD_MAGIC_0 / _1 */
    uint8_t  cmd_type;         /* NP_SAFETY_CMD_HR_REPORT    */
    uint8_t  flags;            /* NP_SAFETY_HR_FLAG_*        */
    uint16_t hr_x10;           /* hub HR, 0.1 BPM, little-endian */
    uint16_t age_ms;           /* age of the newest beat behind hr_x10 */
    uint16_t reserved;         /* 0 */
    uint16_t checksum;         /* sum of bytes [0..9], wrapping uint16 */
} np_safety_hr_report_cmd_t;   /* 2+1+1+2+2+2+2 = 12 bytes */

typedef char _np_spi_hr_report_size_check[
    (sizeof(np_safety_hr_report_cmd_t) == NP_SAFETY_HR_REPORT_FRAME_LEN) ? 1 : -1
];
/* Field offsets are part of the wire format; both sides compile against this. */
typedef char _np_spi_hr_report_layout_check[
    (__builtin_offsetof(np_safety_hr_report_cmd_t, hr_x10)   == 4U &&
     __builtin_offsetof(np_safety_hr_report_cmd_t, age_ms)   == 6U &&
     __builtin_offsetof(np_safety_hr_report_cmd_t, checksum) == NP_SAFETY_HR_REPORT_FRAME_LEN - 2U)
        ? 1 : -1
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

/* ── MCU→hub tier-identity report (NP-REG-UPG-001 §7.5, REQ-UPG-01/-02, OI-UPG-01) ─
 *
 * The device's tier, as the SAFETY MCU established it at power-on from the
 * signed tier-identity record in its OTP (src/np_tier_identity.c).  Carried in
 * the spare MISO window bytes right after the cardiac-status report — no new
 * transfer, no new length.
 *
 * The safety MCU is the authority: it withholds every T2 enable line on a unit
 * whose identity is not a verified T2, whatever the hub requests.  The hub reads
 * this report only to refuse a T2 protocol at load, before anything is asked
 * of the MCU, and to present that refusal as F4 (NP-PWRSRC-001 §6.3) rather
 * than as a missing module.  A hub that ignored this report would still get no
 * T2 enable.
 *
 * Tier codes.  NP_TIER_UNKNOWN is never sent by the MCU; it is the hub's value
 * before a valid report has arrived.  Anything that is not exactly NP_TIER_T2
 * is treated as T1 by every consumer — fail closed (REQ-UPG-01).
 *
 * REFUSED: the hub requested a T2 enable on THIS beat and the MCU withheld it
 * because the unit is not T2.  That is a fact about what someone tried to run,
 * not about the device's condition, so it goes to the user's own app for the
 * F4 message and is NEVER written to SHDR.  `tier` and `reason` are device
 * facts (set once, at manufacture) and carry no user biology.
 *
 * `reason` is diagnostic: why the MCU reached the tier it reports.  A T1 unit
 * reporting NP_TIER_REASON_BLANK was never given its identity at manufacture —
 * behaviourally identical to a signed T1, and exactly what final acceptance
 * must catch (REQ-UPG-02).
 */
#define NP_TIER_UNKNOWN                 0x00U   /* hub only: no valid report yet   */
#define NP_TIER_T1                      0x01U   /* NeurOne Home (wellness)         */
#define NP_TIER_T2                      0x02U   /* NeurOne Pro (510(k) target)     */

#define NP_TIER_REASON_OK               0x00U   /* signed record verified          */
#define NP_TIER_REASON_NO_AUTHORITY     0x01U   /* image carries no tier-authority key */
#define NP_TIER_REASON_BLANK            0x02U   /* OTP record window never programmed */
#define NP_TIER_REASON_FORMAT           0x03U   /* bad magic/version/reserved/tier code */
#define NP_TIER_REASON_SIGNATURE        0x04U   /* signature does not verify for THIS device */

#define NP_SAFETY_TIER_REPORT_MAGIC     0x7DU
#define NP_SAFETY_TIER_REPORT_OFFSET    (NP_SAFETY_NV_REPORT_OFFSET + NP_SAFETY_NV_REPORT_LEN)
#define NP_SAFETY_TIER_FLAG_REFUSED     (1U << 0)

typedef struct __attribute__((packed)) {
    uint8_t  magic;            /* NP_SAFETY_TIER_REPORT_MAGIC        */
    uint8_t  tier;             /* NP_TIER_T1 / NP_TIER_T2            */
    uint8_t  flags;            /* NP_SAFETY_TIER_FLAG_*              */
    uint8_t  reason;           /* NP_TIER_REASON_*                   */
    uint16_t checksum;         /* additive sum of bytes [0..3]       */
} np_safety_tier_report_t;     /* 6 bytes; window bytes [20..25]     */

#define NP_SAFETY_TIER_REPORT_LEN  6U

typedef char _np_spi_tier_report_size_check[
    (sizeof(np_safety_tier_report_t) == NP_SAFETY_TIER_REPORT_LEN) ? 1 : -1
];
typedef char _np_spi_tier_report_fits_check[
    ((NP_SAFETY_TIER_REPORT_OFFSET + NP_SAFETY_TIER_REPORT_LEN)
        <= NP_SAFETY_RX_EXT_FRAME_LEN) ? 1 : -1
];

#endif /* NP_SPI_WIRE_TYPES_H */
