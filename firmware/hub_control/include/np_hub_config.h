/*
 * NeurOne Hub Control Program — Configuration Constants
 * Document: NP-FW-HUB-001 Rev 1
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz), FreeRTOS
 */

#ifndef NP_HUB_CONFIG_H
#define NP_HUB_CONFIG_H

/* ── Protocol binary format ───────────────────────────────────────────────────── */

#define NP_HUB_PROTO_MAGIC          0x4E504850UL   /* "NPHP" — NeurOne Hub Protocol */
/* 1 until a descriptor has shipped.  No session has been created yet, so there
 * is no older blob for the hub to tell apart: the socket-mask target block and
 * np_mod_tdcs_params_t's electrode_area_mcm2 (OI-CHARGE-04), once v2 and v3, are
 * part of v1.  REQ-FWHUB-10's bump-on-length-change applies from the first
 * shipped descriptor (NP-FW-HUB-001 §4.5).                                   */
#define NP_HUB_PROTO_VERSION        0x0001U
/* NP_HUB_PROTO_UUID_LEN and NP_HUB_PROTO_SIG_LEN come from np_app_wire_constants.h, generated from
 * common/npps/constants.json (group HubDescriptorWire), the one source the apps size the blob from. */
#define NP_HUB_PROTO_CMD_MAX        64U            /* max commands per session */
#define NP_HUB_PROTO_PARAMS_MAX     64U            /* max params bytes per command */

/* ── Command target block (v2) ────────────────────────────────────────────────
 * v1 addressed every command with the single `slot_mask` byte, so a cranial
 * command could name only the five legacy zone-module slots. The helmet is now
 * a lattice of 80 sockets (NP-HEX-ZM-001; firmware addressing in
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
 * 80 sockets this shell wires — the wire format must not need a revision when a
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

#define NP_SAFETY_SPI_TIMEOUT_MS    10U
#include "../../common/include/np_spi_wire_types.h"
#include "../../common/include/np_app_wire_constants.h"

/* The NP_SAFETY_EN_* enable bits are in np_spi_wire_types.h — one definition
 * for both processors.  The safety MCU owns the GPIO that physically gates each
 * channel; the main processor cannot enable stimulation without it granting.
 * Audio alone is hub-side: it is not safety-MCU-gated.                       */
#define NP_SAFETY_EN_AUDIO          0U    /* audio not safety-MCU-gated */

/* HD-tDCS electrode geometry for the charge-limit command (OI-CHARGE-02).
 * 3.5mm Ag/AgCl sintered electrode area = 0.0962 cm² (NP_HD_ELECTRODE_AREA_CM2
 * in sloreta_hdtdcs/include/np_hd_config.h).  Expressed in milli-cm² and
 * FLOORED (0.0962 × 1000 = 96.2 → 96) so the safety MCU's derived limit
 * (40µC/cm² × 96 = 3840nC = 3.84µC) never exceeds the true 40µC/cm² ceiling.  */
#define NP_HD_SMALL_ELECTRODE_AREA_MCM2  96U

/* ── Electrode geometry for the channels that do not author their own ────────
 *
 * ⚠ PROVISIONAL — NOT MEASURED.  Recorded as OI-CHARGE-07, and marked here in
 * the same style as NP_IMP_SENSE_R_OHM ("UNCALIBRATED placeholder") and the
 * NTC pin map, because a number that looks like a datasheet value and is not
 * one is worse than an obviously provisional one.
 *
 * WHY THESE ARE FIRMWARE CONSTANTS AT ALL, when OI-CHARGE-04 exists precisely
 * to stop software assuming an electrode area: the tDCS pad is a CONSUMABLE
 * THE USER CHOOSES, so its area is a protocol-authoring fact and must travel
 * in the signed descriptor.  The BES pads, the auricular clip and the cervical
 * collar are FIXED PARTS OF THE PRODUCT — the user cannot substitute them —
 * so their area is a property of the device, which firmware may legitimately
 * hold.  That distinction, not convenience, is the line.
 *
 * FAIL-SAFE DIRECTION: a SMALLER declared area yields a SMALLER charge budget,
 * so these values may be revised DOWN freely and may only be revised UP
 * against a measurement.  They are deliberately at or below the plausible
 * physical size for that reason.
 *
 * What each is up against, at the per-phase ceiling and the modality's rated
 * maximum — the margin is large everywhere except the bottom of the tACS band,
 * which is a real physical result and not a units artifact:
 *   BES/tACS  1 mA, 0.5 Hz sine  → 636 µC/phase ÷ 25 cm² = 25.5 µC/cm² (64% of 40)
 *   BES/tACS  1 mA, 40 Hz sine   →   8 µC/phase ÷ 25 cm² =  0.3 µC/cm²
 *   VNS       2 mA, 250 µs pulse → 0.5 µC/phase ÷ 0.5 cm² = 1.0 µC/cm²
 *   CVNS      2 mA, 1000 µs      → 2.0 µC/phase ÷ 2.0 cm² = 1.0 µC/cm²
 */
/* OI-MMSOCK-02 (2026-09-23): BES/tACS is now GATED on this area — the hub arms
 * NP_SESSION_STATUS_GEOM_REQ_BES for every BES/tACS session and always sends it
 * (np_chan_decl.c), so the safety MCU never applies its own fallback to BES.
 * It stays a device constant because T1 tES stays on the fixed pads
 * (NP-FW-MMSOCK-001 P-1); a lattice electrode's area comes with a tES socket
 * target (OI-MMSOCK-10), not from here.                                      */
#define NP_BES_ELECTRODE_AREA_MCM2   25000U  /* 25 cm² pad — PROVISIONAL */
#define NP_VNS_ELECTRODE_AREA_MCM2     500U  /* 0.5 cm² auricular clip pad — PROVISIONAL */
#define NP_CVNS_ELECTRODE_AREA_MCM2   2000U  /* 2 cm² cervical collar pad — PROVISIONAL */

/* HD-tDCS montage codes (np_mod_hd_tdcs_params_t.montage).  Ring and bilateral
 * 4×1 use the 3.5mm small electrodes; standard 2-electrode uses the default
 * 25cm² pad (no charge-limit override needed).                               */
#define NP_HD_MONTAGE_RING_4X1       0U
#define NP_HD_MONTAGE_BILATERAL_4X1  1U
#define NP_HD_MONTAGE_STANDARD_2EL   2U

/* ── Cervical VNS re-enable manager (OI-CVNS-HUB-01) ──────────────────────────
 * The hub asserts NP_SESSION_STATUS_CVNS_REENABLE only when ALL THREE gates
 * pass: 30s lockout elapsed since the hub observed the cardiac cutoff +
 * explicit app confirmation + fresh hub-side impedance check passed.        */

/* Gate 1: hub-side lockout, measured from the heartbeat reply in which the hub
 * first observed NP_SAFETY_STATUS_CARDIAC.  The duration is the safety MCU's own
 * NP_CARDIAC_LOCKOUT_MS (np_shared_constants.h, one definition); because the hub observation lags the MCU cutoff by
 * ≤1 heartbeat (200ms), the hub window always contains the MCU window.      */
/* (No separate hub constant: the lockout is NP_CARDIAC_LOCKOUT_MS.)         */

/* Gate 3: max wait for the hub-side impedance measurement to complete before
 * failing closed (back to AWAIT_CONFIRM).                                   */
#define NP_CVNS_REENABLE_IMP_TIMEOUT_MS    5000U

/* Bounded assertion: max time the re-enable bit may stay asserted while the
 * safety MCU has not cleared CARDIAC (10 heartbeats).  On expiry the hub
 * deasserts and requires a fresh confirmation + impedance pass.             */
#define NP_CVNS_REENABLE_ASSERT_TIMEOUT_MS 2000U

/* SHDR lifecycle event codes for np_log_shdr_fault() (safety interlock log →
 * SHDR).  Flags only — no HR/RR values.  Callers pass 0 for the timestamp
 * argument, but note that is belt-and-braces, NOT the mechanism: np_log_shdr_fault()
 * discards `session_ms` for EVERY caller and every fault type (see
 * np_session_log.c), because fault event timing is UHDR.  The suppression here
 * is therefore UNCONDITIONAL — it does not depend on the fault being cardiac,
 * and so discloses nothing about which fault occurred.  Contrast the fault-latch
 * defect corrected 2026-08-12, where zeroing a field only for cardiac faults made
 * the redaction itself a cardiac oracle (CLAUDE.md §5.1).                    */
#define NP_CVNS_SHDR_EV_CUTOFF          0xC1U
#define NP_CVNS_SHDR_EV_CONFIRM_OPEN    0xC2U
#define NP_CVNS_SHDR_EV_IMP_FAILED      0xC3U
#define NP_CVNS_SHDR_EV_IMP_TIMEOUT     0xC4U
#define NP_CVNS_SHDR_EV_REENABLED       0xC5U
#define NP_CVNS_SHDR_EV_ASSERT_TIMEOUT  0xC6U
/* OI-CVNS-HUB-11: hub-side and safety-MCU per-electrode impedance measurements
 * disagreed beyond NP_CVNS_IMPEDANCE_CROSSVAL_KOHM.  Device-condition flag only
 * (no kΩ values); no timestamp reaches SHDR, by the unconditional logger rule
 * noted above.  Raw per-electrode kΩ is UHDR and is NEVER written to SHDR.     */
#define NP_CVNS_SHDR_EV_IMP_CROSSVAL    0xC7U

/* ── Commanded-versus-delivered stimulation cross-check (OI-FMEA-09) ──────────
 *
 * np_stim_xcheck.c compares delivered current (OI-STIM-06) against commanded
 * current, and raises NP_STIM_SHDR_EV_DELIVERY_DIVERGENCE on a sustained
 * excess.  FMEA-M03-02's residual score depends on it (NP-FMEA-001 §3.3).
 *
 * ⚠ UNVALIDATED PLACEHOLDERS — NOT DERIVED.  No threshold has been set for this
 * cross-check (NP-HW-TACSDRV-001 §5.3: "follows from the divergence threshold
 * OI-FMEA-09 sets, which is not yet set").  The values below were chosen only
 * to be clear of plausible read-back error and to reject a single noisy
 * sample.  They trace to no measurement, so this block is NOT a requirement
 * (NP-CONV-001 §7.1).  They get replaced once a threshold is derived from the
 * sense path's measured accuracy and the charge headroom the ceiling can spend
 * before the flag.  That derivation is the open part of OI-FMEA-09.
 *
 * Excess = delivered > bound + max(bound × TOL_PCT / 100, FLOOR_UA).        */
#define NP_STIM_XCHECK_TOL_PCT          10U   /* % of commanded — PLACEHOLDER  */
#define NP_STIM_XCHECK_FLOOR_UA         100U  /* µA absolute floor — PLACEHOLDER */
#define NP_STIM_XCHECK_CONSECUTIVE      3U    /* snapshots (1 s apart) — PLACEHOLDER */

/* SHDR event code for np_log_shdr_fault(): delivered stimulation current
 * exceeded commanded on this slot.  A flag only: no current, no magnitude, no
 * timestamp (the logger discards session_ms for every caller).  Distinct from
 * the 0xC_ cervical VNS codes and from the negated np_hub_status_t codes the
 * runner logs for refused commands.                                          */
#define NP_STIM_SHDR_EV_DELIVERY_DIVERGENCE 0xD1U

/* Safety MCU status bits that make a fault NON-recoverable in-session.  A
 * heartbeat reply with NP_SAFETY_STATUS_CARDIAC set and none of these bits is
 * a recoverable cardiac cutoff: the hub holds the session and runs the CVNS
 * re-enable flow instead of aborting.  (CUTOFF and IMPEDANCE are expected
 * companions of a cardiac event and do not force an abort by themselves.)   */
#define NP_CVNS_NONRECOVERABLE_FAULTS   (NP_SAFETY_STATUS_FAULT    | \
                                         NP_SAFETY_STATUS_WATCHDOG | \
                                         NP_SAFETY_STATUS_THERMAL  | \
                                         NP_SAFETY_STATUS_CHARGE)

/* NP_SAFETY_STATUS_* (heartbeat reply flags) are in np_spi_wire_types.h. */

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
#define NP_EEG_SAMPLE_BYTES         3U     /* 24-bit ADS1299 output */

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
 * letter is received. See NP-FW-EMMC-002 Rev 1 §F and NP-REG-PBM1064-001 Q-13. */
#define NP_MODE_F_REGULATORY_CLEARED  0

#endif /* NP_HUB_CONFIG_H */
