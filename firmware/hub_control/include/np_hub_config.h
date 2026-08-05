/*
 * NeurOne Hub Control Program — Configuration Constants
 * Document: NP-FW-HUB-001 Rev A
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz), FreeRTOS
 */

#ifndef NP_HUB_CONFIG_H
#define NP_HUB_CONFIG_H

/* ── Protocol binary format ───────────────────────────────────────────────────── */

#define NP_HUB_PROTO_MAGIC          0x4E504850UL   /* "NPHP" — NeurOne Hub Protocol */
#define NP_HUB_PROTO_VERSION        0x0002U        /* v2: socket-addressed targets */
#define NP_HUB_PROTO_UUID_LEN       16U            /* session UUID (UHDR key) */
#define NP_HUB_PROTO_SERIAL_LEN     32U            /* ASCII device serial — replay guard */
#define NP_HUB_PROTO_SIG_LEN        64U            /* Ed25519 signature */
#define NP_HUB_PROTO_CMD_MAX        64U            /* max commands per session */
#define NP_HUB_PROTO_PARAMS_MAX     64U            /* max params bytes per command */

/* ── Command target block (v2) ────────────────────────────────────────────────
 * v1 addressed every command with the single `slot_mask` byte, so a cranial
 * command could name only the five legacy zone-module slots. The helmet is now
 * a lattice of 78 sockets (NP-HEX-ZM-001; firmware addressing in
 * np_module_map.h), which does not fit in five bits and is not a slot at all.
 *
 * v2 therefore gives each command an optional variable-length TARGET BLOCK,
 * carried between the command header and params[]:
 *
 *     [np_proto_cmd_hdr_t] [target_len bytes] [params_len bytes]
 *
 * `target_kind` selects how the block is read (np_proto_target_kind_t in
 * np_hub_types.h). NP_PROTO_TARGET_SLOT keeps target_len 0 and dispatches by
 * the header's `slot_id` — correct for the genuinely fixed slots below. Cranial
 * modules use NP_PROTO_TARGET_SOCKET_MASK: a bitmap with one bit per socket.
 *
 * WHY slot_id AND NOT v1's slot_mask. v1 addressed slots with a uint8_t bitmask
 * and dispatched by `(slot_mask >> slot) & 1`, so slots 8 and above could not be
 * named at all — the VNS clip, the intranasal probe, cervical VNS and every T2
 * unit were unreachable, and the only multi-slot use of the mask was the zone
 * mask that sockets now replace. Each remaining slot is a distinct device with
 * its own parameter struct, so a command has exactly one slot or none. A plain
 * index says that; a mask only invited the 0x01 catch-all that sent every
 * modality to the PBM driver in slot 0.
 *
 * WHY A BITMAP AND NOT AN ADDRESS LIST. Under the inclusive zone-membership
 * rule (np_module_map.h), a socket may belong to more than one zone — the ten
 * midline sockets are in BOTH hemisphere zones of their lobe. A protocol
 * naming "Frontal Left" and "Frontal Right" therefore names sockets 1, 5 and 13
 * twice. In a list that is two entries and two drives of the same module: double
 * J/cm² on a real PBM protocol. In a bitmap the duplicate is not expressible.
 * The dedup guarantee stops being something every producer must remember.
 *
 * Sized to NP_HEXMAP_MAX_SOCKETS (128, the full 7-bit socket domain), NOT to the
 * 78 sockets this shell wires — the wire format must not need a revision when a
 * shell wires more of the domain it already addresses.
 *
 * BIT POSITION IS INDEX SPACE, NOT A SOCKET NUMBER. Bit 0 selects socket 1. A
 * socket NUMBER is 1-based project-wide (NUMBER-1, docs/np_hex_zm_001.md §3.3);
 * this bitmap is exempt because a bit position is an offset, and because the
 * exemption is what makes the sizing work — mapping sockets 1..128 onto bits
 * 1..128 would need a 129th bit, i.e. a 17th byte, or a lattice capped at 127.
 * Producers convert once (hubCompiler.socketBitmap, NPSocketMask) and consumers
 * add the base back before any socket number is shown or logged.
 */
#define NP_HUB_SOCKET_MASK_BYTES    16U   /* 128 bits == NP_HEXMAP_MAX_SOCKETS */
#define NP_HUB_PROTO_TARGET_MAX     NP_HUB_SOCKET_MASK_BYTES

/* ── Module slot identifiers ─────────────────────────────────────────────────── */

/*
 * Slots 0-4: RETIRED zone-module FPC connectors. They named the five legacy
 * zone modules, which the socket lattice replaces. They are kept only so slot
 * numbering (and therefore the NP_SAFETY_EN_* bit assignment and the registry
 * probe table) does not shift under the fixed slots. A v2 protocol MUST NOT set
 * slot_id to one of them; np_protocol_verify_and_parse rejects such a blob, so a
 * stale zone target can no longer reach a module driver. Cranial targeting is by
 * socket mask — see the target block above.
 *
 * Slots 5-18: fixed or accessory-port modules detected by their own mechanisms.
 * These are real single-instance devices, not helmet positions, so they remain
 * slot-addressed.
 */
#define NP_HUB_SLOT_ZONE_0          0U    /* RETIRED — see note above */
#define NP_HUB_SLOT_ZONE_1          1U    /* RETIRED */
#define NP_HUB_SLOT_ZONE_2          2U    /* RETIRED */
#define NP_HUB_SLOT_ZONE_3          3U    /* RETIRED */
#define NP_HUB_SLOT_ZONE_4          4U    /* RETIRED */
#define NP_HUB_SLOT_EEG             5U    /* ADS1299 — fixed hardware, always present */
#define NP_HUB_SLOT_AUDIO           6U    /* planar mag + bone conduction — fixed */
#define NP_HUB_SLOT_VISUAL          7U    /* visual goggles — Hall sensor + IR proximity */
#define NP_HUB_SLOT_VNS_HRV         8U    /* auricular VNS+HRV clip — impedance check */
#define NP_HUB_SLOT_INTRANASAL      9U    /* intranasal Y-probe — optical code + pogo */
#define NP_HUB_SLOT_CVNS            10U   /* cervical VNS (T2) — accessory port */
#define NP_HUB_SLOT_QEEG            11U   /* T2: 21-ch qEEG wet-gel cap */
#define NP_HUB_SLOT_TMS             12U   /* T2: TMS focal figure-8 coil */
#define NP_HUB_SLOT_PBM_1170NM      13U   /* T2: 1170nm deep PBM laser unit */
#define NP_HUB_SLOT_CLIN_TACS       14U   /* T2: 21-ch clinical tACS (≤4mA) */
#define NP_HUB_SLOT_HD_TDCS         15U   /* T2: sLORETA-guided 4×1 HD-tDCS (shares CLIN_TACS HW) */
#define NP_HUB_SLOT_VIBROTACTILE    16U   /* Accessory: mastoid LRA 40Hz ± 0.5Hz pad */
/* BES/tACS and tDCS share one driver (np_mod_stim.c) but are separate slots:
 * the safety MCU gates them on separate enable bits (NP_SAFETY_EN_BES_TACS /
 * NP_SAFETY_EN_TDCS) and they carry different parameter structs. Appended at the
 * end so no existing slot number — and so no NP_SAFETY_EN_* bit assignment or
 * registry probe-table index — shifts. */
#define NP_HUB_SLOT_BES_TACS        17U   /* brainwave entrainment / tACS */
#define NP_HUB_SLOT_TDCS            18U   /* cortical priming / tDCS */
#define NP_HUB_SLOT_MAX             19U
#define NP_HUB_ZONE_SLOT_COUNT      5U

/* First slot a v2 protocol may name: slots below this are the retired zone
 * slots, whose work sockets now do. */
#define NP_HUB_SLOT_FIRST_VALID     NP_HUB_ZONE_SLOT_COUNT

/* slot_id value meaning "this command is not slot-addressed". Chosen outside the
 * slot domain so it can never be mistaken for slot 0, the way a zeroed mask
 * could. */
#define NP_HUB_SLOT_NONE            0xFFU

/* ── FreeRTOS task parameters ────────────────────────────────────────────────── */

#define NP_HUB_TASK_PRIO_HEARTBEAT  4U    /* must never miss the 200ms SPI window */
#define NP_HUB_TASK_PRIO_CONTROL    3U    /* session runner and command dispatch */
#define NP_HUB_TASK_PRIO_TELEMETRY  2U    /* telemetry reads and log routing */
#define NP_HUB_TASK_PRIO_DETECT     1U    /* module insertion scan — idle only */

#define NP_HUB_TASK_STACK_HEARTBEAT 256U  /* words */
#define NP_HUB_TASK_STACK_CONTROL   1024U /* words */
#define NP_HUB_TASK_STACK_TELEMETRY 512U  /* words */
#define NP_HUB_TASK_STACK_DETECT    512U  /* words */

/* ── Safety MCU SPI ──────────────────────────────────────────────────────────── */

#define NP_SAFETY_HEARTBEAT_MS      200U  /* main processor sends heartbeat period */
#define NP_SAFETY_WATCHDOG_MS       1500U /* safety MCU watchdog; cutoff on expiry */
#define NP_SAFETY_SPI_TIMEOUT_MS    10U
#define NP_SAFETY_FRAME_LEN         8U    /* MCU reply frame size (heartbeat RX is NP_SAFETY_RX_EXT_FRAME_LEN = 38) */
#define NP_SAFETY_BEAT_MAGIC_0      0xBEU
#define NP_SAFETY_BEAT_MAGIC_1      0xA7U

/* Session signature command frame constants: NP_SAFETY_CMD_MAGIC_0/1,
 * NP_SAFETY_CMD_SESSION_SIG, NP_SAFETY_CMD_FRAME_LEN, NP_SESSION_HASH_LEN,
 * NP_ED25519_SIG_LEN, and np_safety_sig_cmd_t are in
 * firmware/common/include/np_spi_wire_types.h (included via np_hub_types.h). */
#include "../../common/include/np_spi_wire_types.h"

/*
 * Enable bitmask sent to safety MCU; one bit per stimulation channel.
 * Safety MCU owns the GPIO that physically gates each channel — the main
 * processor cannot enable stimulation without the safety MCU granting it.
 */
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
#define NP_SAFETY_EN_TMS            (1U << 11)  /* gates TMS coil; EMF cancellation gated off per NP-FW-HUB §4.2 */
#define NP_SAFETY_EN_PBM_1170NM     (1U << 12)  /* gates 1170nm laser diodes */
#define NP_SAFETY_EN_CLIN_STIM      (1U << 13)  /* gates 21-ch tACS driver (covers CLIN_TACS + HD_TDCS) */
#define NP_SAFETY_EN_AUDIO          0U    /* audio not safety-MCU-gated */

/* Safety-MCU charge-monitor channel INDEX for CLIN_STIM (= bit position of
 * NP_SAFETY_EN_CLIN_STIM).  HD-tDCS accumulates charge on this channel against
 * the anode (peak per-electrode) current.  OI-CHARGE-02.                      */
#define NP_SAFETY_CH_CLIN_STIM      13U

/* HD-tDCS electrode geometry for the charge-limit command (OI-CHARGE-02).
 * 3.5mm Ag/AgCl sintered electrode area = 0.0962 cm² (NP_HD_ELECTRODE_AREA_CM2
 * in sloreta_hdtdcs/include/np_hd_config.h).  Expressed in milli-cm² and
 * FLOORED (0.0962 × 1000 = 96.2 → 96) so the safety MCU's derived limit
 * (40µC/cm² × 96 = 3840nC = 3.84µC) never exceeds the true 40µC/cm² ceiling.  */
#define NP_HD_SMALL_ELECTRODE_AREA_MCM2  96U

/* HD-tDCS montage codes (np_mod_hd_tdcs_params_t.montage).  Ring and bilateral
 * 4×1 use the 3.5mm small electrodes; standard 2-electrode uses the default
 * 25cm² pad (no charge-limit override needed).                               */
#define NP_HD_MONTAGE_RING_4X1       0U
#define NP_HD_MONTAGE_BILATERAL_4X1  1U
#define NP_HD_MONTAGE_STANDARD_2EL   2U

/* ── Heartbeat session_status bits (hub side) ─────────────────────────────────
 * Bits 0 and 1 are defined per-side (here and in safety_mcu/np_safety_protocol.h)
 * and MUST match; bit 2 (NP_SESSION_STATUS_GEOM_REQUIRED) is shared via
 * np_spi_wire_types.h.  These are BIT FLAGS — never write an np_session_state_t
 * enum value into the session_status byte (use np_safety_session_status_bits()). */
#define NP_SESSION_STATUS_ACTIVE        (1U << 0)  /* session underway */
#define NP_SESSION_STATUS_CVNS_REENABLE (1U << 1)  /* explicit CVNS re-enable after cardiac cutoff */

/* ── Cervical VNS re-enable manager (OI-CVNS-HUB-01) ──────────────────────────
 * The hub asserts NP_SESSION_STATUS_CVNS_REENABLE only when ALL THREE gates
 * pass: 30s lockout elapsed since the hub observed the cardiac cutoff +
 * explicit app confirmation + fresh hub-side impedance check passed.        */

/* Gate 1: hub-side lockout, measured from the heartbeat reply in which the hub
 * first observed NP_SAFETY_STATUS_CARDIAC.  Value matches the safety MCU's
 * NP_CARDIAC_LOCKOUT_MS; because the hub observation lags the MCU cutoff by
 * ≤1 heartbeat (200ms), the hub window always contains the MCU window.      */
#define NP_CVNS_REENABLE_LOCKOUT_MS        30000U

/* Gate 3: max wait for the hub-side impedance measurement to complete before
 * failing closed (back to AWAIT_CONFIRM).                                   */
#define NP_CVNS_REENABLE_IMP_TIMEOUT_MS    5000U

/* Bounded assertion: max time the re-enable bit may stay asserted while the
 * safety MCU has not cleared CARDIAC (10 heartbeats).  On expiry the hub
 * deasserts and requires a fresh confirmation + impedance pass.             */
#define NP_CVNS_REENABLE_ASSERT_TIMEOUT_MS 2000U

/* SHDR lifecycle event codes for np_log_shdr_fault() (safety interlock log →
 * SHDR).  Flags only — no HR/RR values, and the cutoff event is logged with
 * a suppressed (0) timestamp so no session-relative time co-locates with a
 * UHDR-class cardiac event (see CLAUDE.md fault-latch privacy gate).        */
#define NP_CVNS_SHDR_EV_CUTOFF          0xC1U
#define NP_CVNS_SHDR_EV_CONFIRM_OPEN    0xC2U
#define NP_CVNS_SHDR_EV_IMP_FAILED      0xC3U
#define NP_CVNS_SHDR_EV_IMP_TIMEOUT     0xC4U
#define NP_CVNS_SHDR_EV_REENABLED       0xC5U
#define NP_CVNS_SHDR_EV_ASSERT_TIMEOUT  0xC6U
/* OI-CVNS-HUB-11: hub-side and safety-MCU per-electrode impedance measurements
 * disagreed beyond NP_CVNS_IMPEDANCE_CROSSVAL_KOHM.  Device-condition flag only
 * (no kΩ values); logged with a suppressed (0) timestamp (fault-latch privacy
 * gate).  Raw per-electrode kΩ is UHDR and is NEVER written to SHDR.           */
#define NP_CVNS_SHDR_EV_IMP_CROSSVAL    0xC7U

/* Safety MCU status bits that make a fault NON-recoverable in-session.  A
 * heartbeat reply with NP_SAFETY_STATUS_CARDIAC set and none of these bits is
 * a recoverable cardiac cutoff: the hub holds the session and runs the CVNS
 * re-enable flow instead of aborting.  (CUTOFF and IMPEDANCE are expected
 * companions of a cardiac event and do not force an abort by themselves.)   */
#define NP_CVNS_NONRECOVERABLE_FAULTS   (NP_SAFETY_STATUS_FAULT    | \
                                         NP_SAFETY_STATUS_WATCHDOG | \
                                         NP_SAFETY_STATUS_THERMAL  | \
                                         NP_SAFETY_STATUS_CHARGE)

/* Safety MCU status flags (returned in each heartbeat reply). */
#define NP_SAFETY_STATUS_OK          0x00U
#define NP_SAFETY_STATUS_FAULT       (1U << 0)
#define NP_SAFETY_STATUS_WATCHDOG    (1U << 1) /* watchdog fired since last beat */
#define NP_SAFETY_STATUS_CUTOFF      (1U << 2) /* stimulation was cut by safety MCU */
#define NP_SAFETY_STATUS_IMPEDANCE   (1U << 3)
#define NP_SAFETY_STATUS_THERMAL     (1U << 4)
#define NP_SAFETY_STATUS_CHARGE      (1U << 5)
#define NP_SAFETY_STATUS_CARDIAC     (1U << 6)
/* SIG_PENDING: hub must call np_safety_spi_send_session_sig() to clear.
 * Set when session_active goes 0→1; cleared when sig verified.
 * If set after send_session_sig(), the signature was rejected — abort session. */
#define NP_SAFETY_STATUS_SIG_PENDING (1U << 7)

/* ── Session runner ───────────────────────────────────────────────────────────── */

#define NP_RUNNER_TICK_MS           5U    /* runner loop granularity */
#define NP_RUNNER_TELEM_INTERVAL_MS 1000U /* per-module telemetry snapshot rate */
#define NP_RUNNER_SHUTDOWN_MS       5000U /* max wait for graceful module shutdown */

/* ── Logging ──────────────────────────────────────────────────────────────────── */

#define NP_LOG_UHDR_FLUSH_MS        30000U /* UHDR flush to eMMC interval */
#define NP_LOG_SHDR_FLUSH_MS        5000U  /* SHDR flush interval */
#define NP_LOG_RECORD_MAX_LEN       512U

/* EEG ring buffer — 500Hz × 8ch × 3 bytes (24-bit) = 12000 bytes/s. */
#define NP_EEG_RING_SAMPLES         4000U  /* ~8s headroom before oldest data overwritten */
#define NP_EEG_CHANNELS             8U     /* T1: Fp1/2 F3/4 C3/4 P3/4 semi-dry */
#define NP_EEG_SAMPLE_BYTES         3U     /* 24-bit ADS1299 output */
#define NP_EEG_SAMPLE_RATE_HZ       500U

/* T2 qEEG — 21-channel wet-gel 10-20 + FC3/4 + Oz + A1/A2 */
#define NP_QEEG_CHANNELS            21U
#define NP_QEEG_SAMPLE_RATE_HZ      500U

/* ── Module detection (idle-mode polling intervals) ───────────────────────────── */

#define NP_DETECT_ZONE_POLL_MS      50U
#define NP_DETECT_ACCESSORY_POLL_MS 500U  /* VNS, intranasal, CVNS */
#define NP_DETECT_VISUAL_POLL_MS    200U  /* Hall sensor poll */

/* ── Mode F regulatory gate ───────────────────────────────────────────────────── */

/* Mode F (NIR retinal walk, 808-830nm daily retinal PBM) must be gated by this
 * compile-time flag. Must remain 0 until the RISK-03 Q-13 regulatory opinion
 * letter is received. See NP-FW-EMMC-002 Rev A §F and NP-REG-PBM1064-001 Q-13. */
#define NP_MODE_F_REGULATORY_CLEARED  0

#endif /* NP_HUB_CONFIG_H */
