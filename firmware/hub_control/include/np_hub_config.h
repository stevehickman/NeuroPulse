/*
 * NeuroPulse Hub Control Program — Configuration Constants
 * Document: NP-FW-HUB-001 Rev A
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz), FreeRTOS
 */

#ifndef NP_HUB_CONFIG_H
#define NP_HUB_CONFIG_H

/* ── Protocol binary format ───────────────────────────────────────────────────── */

#define NP_HUB_PROTO_MAGIC          0x4E504850UL   /* "NPHP" — NeuroPulse Hub Protocol */
#define NP_HUB_PROTO_VERSION        0x0001U
#define NP_HUB_PROTO_UUID_LEN       16U            /* session UUID (UHDR key) */
#define NP_HUB_PROTO_SERIAL_LEN     32U            /* ASCII device serial — replay guard */
#define NP_HUB_PROTO_SIG_LEN        64U            /* Ed25519 signature */
#define NP_HUB_PROTO_CMD_MAX        64U            /* max commands per session */
#define NP_HUB_PROTO_PARAMS_MAX     64U            /* max params bytes per command */

/* ── Module slot identifiers ─────────────────────────────────────────────────── */

/*
 * Slots 0-4: zone module FPC connectors (detected by ZONE_ID ADC, RISK-18).
 * Slots 5-10: fixed or accessory-port modules detected by their own mechanisms.
 */
#define NP_HUB_SLOT_ZONE_0          0U
#define NP_HUB_SLOT_ZONE_1          1U
#define NP_HUB_SLOT_ZONE_2          2U
#define NP_HUB_SLOT_ZONE_3          3U
#define NP_HUB_SLOT_ZONE_4          4U
#define NP_HUB_SLOT_EEG             5U    /* ADS1299 — fixed hardware, always present */
#define NP_HUB_SLOT_AUDIO           6U    /* planar mag + bone conduction — fixed */
#define NP_HUB_SLOT_VISUAL          7U    /* visual goggles — Hall sensor + IR proximity */
#define NP_HUB_SLOT_VNS_HRV         8U    /* auricular VNS+HRV clip — impedance check */
#define NP_HUB_SLOT_INTRANASAL      9U    /* intranasal Y-probe — optical code + pogo */
#define NP_HUB_SLOT_CVNS            10U   /* cervical VNS (T2) — accessory port */
#define NP_HUB_SLOT_QEEG            11U   /* T2: 21-ch qEEG wet-gel cap */
#define NP_HUB_SLOT_TMS             12U   /* T2: TMS focal figure-8 coil */
#define NP_HUB_SLOT_PBM_1170NM      13U   /* T2: 1170nm deep PBM laser unit */
#define NP_HUB_SLOT_CLIN_TACS       14U   /* T2: 16-ch clinical tACS (≤4mA) */
#define NP_HUB_SLOT_HD_TDCS         15U   /* T2: sLORETA-guided 4×1 HD-tDCS (shares CLIN_TACS HW) */
#define NP_HUB_SLOT_VIBROTACTILE    16U   /* Accessory: mastoid LRA 40Hz ± 0.5Hz pad */
#define NP_HUB_SLOT_MAX             17U
#define NP_HUB_ZONE_SLOT_COUNT      5U

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
#define NP_SAFETY_FRAME_LEN         8U    /* bytes per heartbeat SPI exchange */
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
#define NP_SAFETY_EN_CLIN_STIM      (1U << 13)  /* gates 16-ch tACS driver (covers CLIN_TACS + HD_TDCS) */
#define NP_SAFETY_EN_AUDIO          0U    /* audio not safety-MCU-gated */

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

#endif /* NP_HUB_CONFIG_H */
