/*
 * NeurOne Hub Control Program — Type Definitions
 * Document: NP-FW-HUB-001 Rev A
 *
 * Defines the protocol binary format, module type enum, per-module parameter
 * structs, session state, telemetry records, and all shared types used by
 * np_module_registry, np_protocol, np_session_runner, and np_session_log.
 */

#ifndef NP_HUB_TYPES_H
#define NP_HUB_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "np_hub_config.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 * Status codes
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    NP_HUB_OK                       =  0,
    NP_HUB_ERR_GENERIC              = -1,
    NP_HUB_ERR_INVALID_ARG          = -2,
    NP_HUB_ERR_NOT_PRESENT          = -3,   /* module not detected */
    NP_HUB_ERR_BAD_MAGIC            = -4,
    NP_HUB_ERR_BAD_VERSION          = -5,
    NP_HUB_ERR_BAD_SIGNATURE        = -6,   /* Ed25519 verification failed */
    NP_HUB_ERR_WRONG_DEVICE         = -7,   /* protocol serial != this device */
    NP_HUB_ERR_CMD_TOO_MANY         = -8,
    NP_HUB_ERR_PARAMS_TOO_LONG      = -9,
    NP_HUB_ERR_SAFETY_REJECTED      = -10,  /* safety MCU denied enable */
    NP_HUB_ERR_SAFETY_FAULT         = -11,  /* safety MCU reported fault */
    NP_HUB_ERR_TIMEOUT              = -12,
    NP_HUB_ERR_SESSION_ACTIVE       = -13,
    NP_HUB_ERR_NO_SESSION           = -14,
    NP_HUB_ERR_LOG_FULL             = -15,
    NP_HUB_ERR_MOD_INIT             = -16,
    NP_HUB_ERR_MOD_FAULT            = -17,
} np_hub_status_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Module types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    NP_MOD_NONE         = 0x00,
    NP_MOD_PBM_BASE     = 0x01,  /* base zone module: 660nm + 808nm */
    NP_MOD_PBM_SMART    = 0x02,  /* smart zone module: +1064nm (ATtiny402 I2C) */
    NP_MOD_INTRANASAL   = 0x03,  /* intranasal Y-probe: 660nm + 808nm bilateral */
    NP_MOD_EEG          = 0x04,  /* ADS1299 8-ch semi-dry, 500Hz, 24-bit */
    NP_MOD_BES_TACS     = 0x05,  /* brainwave entrainment / tACS */
    NP_MOD_TDCS         = 0x06,  /* cortical priming / tDCS */
    NP_MOD_VNS_HRV      = 0x07,  /* auricular VNS + PPG HRV clip */
    NP_MOD_AUDIO        = 0x08,  /* planar magnetic + mastoid bone conduction */
    NP_MOD_VISUAL       = 0x09,  /* 108 micro-LED/lens goggles + EMDR */
    NP_MOD_CVNS         = 0x0A,  /* cervical VNS (T2 accessory) */
    /* ── T2 modalities ─────────────────────────────────────────────────────── */
    NP_MOD_QEEG_21CH    = 0x0B,  /* T2: 21-ch 10-20 wet gel + sLORETA */
    NP_MOD_TMS          = 0x0C,  /* T2: focal figure-8 coil, rTMS/TBS */
    NP_MOD_PBM_1170NM   = 0x0D,  /* T2: 1170nm laser diodes, 35–40mm depth */
    NP_MOD_CLIN_TACS    = 0x0E,  /* T2: 21-ch arbitrary waveform tACS ≤4mA */
    NP_MOD_HD_TDCS      = 0x0F,  /* T2: sLORETA-guided 4×1 ring HD-tDCS */
    /* ── Accessory ──────────────────────────────────────────────────────────── */
    NP_MOD_VIBROTACTILE = 0x10,  /* Accessory: mastoid LRA 40Hz ± 0.5Hz (provisional) */
    NP_MOD_TYPE_COUNT   = 0x11,
} np_hub_mod_type_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Protocol binary layout (NP Hub Protocol v2)
 *
 * Binary blob received from the app (BLE GATT / USB-C CDC).
 * Ed25519 signature covers bytes [0 .. total_len - NP_HUB_PROTO_SIG_LEN - 1].
 * Hub verifies before parsing any command or enabling any module.
 *
 * Per-command layout (v2):
 *     [np_proto_cmd_hdr_t : 14] [target : target_len] [params : params_len]
 *
 * v1 had no target block and a 12-byte command header; the version word is the
 * discriminator and v1 blobs are rejected outright (NP_HUB_ERR_BAD_VERSION).
 * No device has shipped, so there is no v1 fleet to stay compatible with — see
 * NP-HEX-ZM-001 §4 and the target-block note in np_hub_config.h.
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct __attribute__((packed)) {
    uint32_t magic;                              /* NP_HUB_PROTO_MAGIC             */
    uint16_t version;                            /* NP_HUB_PROTO_VERSION           */
    uint8_t  flags;                              /* bit0=T2_tier; bit1=autonomous  */
    uint8_t  cmd_count;                          /* number of commands that follow  */
    uint8_t  session_uuid[NP_HUB_PROTO_UUID_LEN];/* UHDR key — random, from app   */
    uint32_t compiled_at_unix;                   /* UHDR session timestamp          */
    uint8_t  device_serial[NP_HUB_PROTO_SERIAL_LEN]; /* replay guard               */
    uint32_t session_duration_ms;                /* total session length            */
} np_proto_header_t;

/* compile-time check: header is exactly 64 bytes
 * Layout: 4+2+1+1+16+4+32+4 = 64 bytes (packed, no reserved padding). */
typedef char _np_proto_hdr_size[(sizeof(np_proto_header_t) == 64U) ? 1 : -1];

/* ── Command target kind ─────────────────────────────────────────────────────
 * How to read a command's target block. Values are wire constants: frozen once
 * written, append only, never renumbered.
 *
 * An unknown kind is REJECTED at parse rather than skipped. Skipping would mean
 * running a stimulation command whose target the hub admits it does not
 * understand — the target is the one field that must never be interpreted
 * leniently. Forward compatibility is bought with the version word, not by
 * ignoring fields.
 */
typedef enum {
    /* target_len == 0; slot_id in NP_HUB_SLOT_FIRST_VALID .. NP_HUB_SLOT_MAX-1.
     * Dispatch by slot_id (fixed/accessory devices). */
    NP_PROTO_TARGET_SLOT        = 0x00,
    /* target_len == NP_HUB_SOCKET_MASK_BYTES; slot_id == NP_HUB_SLOT_NONE.
     * One bit per socket, LSB-first:
     * socket_id S is byte S/8, bit S%8. socket_id is 0-BASED, matching
     * np_hex_addr_t; the app's 1-based NPPS socket ids are converted at the
     * compiler boundary (see the numbering-base note in np_module_map.h). */
    NP_PROTO_TARGET_SOCKET_MASK = 0x01,
} np_proto_target_kind_t;

/*
 * Variable-length command record, packed in protocol binary after the header.
 * target_len bytes of target block, then params_len bytes of module-specific
 * parameters, follow each command header.
 */
typedef struct __attribute__((packed)) {
    uint8_t  mod_type;     /* np_hub_mod_type_t                                    */
    uint8_t  slot_id;      /* slot index, or NP_HUB_SLOT_NONE when socket-addressed*/
    uint32_t start_ms;     /* session-relative start time                          */
    uint32_t duration_ms;  /* command active duration; 0 = until next command      */
    uint16_t params_len;   /* byte count of params[] that follow; 0 = stop         */
    uint8_t  target_kind;  /* np_proto_target_kind_t                               */
    uint8_t  target_len;   /* byte count of target[] between header and params[]   */
} np_proto_cmd_hdr_t;

/* compile-time check: command header is exactly 14 bytes
 * Layout: 1+1+4+4+2+1+1 = 14 bytes (packed, no padding). */
typedef char _np_proto_cmd_hdr_size[(sizeof(np_proto_cmd_hdr_t) == 14U) ? 1 : -1];

/* The socket bitmap must cover the whole socket domain np_module_map addresses.
 * If NP_HEXMAP_MAX_SOCKETS ever grows past the bitmap, this fails the build
 * rather than letting high sockets become silently unaddressable on the wire.
 * (np_module_map.h is not included here — np_hub_types.h is the lower layer —
 * so the domain is pinned by value, with the same 128 the header declares.) */
typedef char _np_proto_socket_mask_covers_domain
    [(NP_HUB_SOCKET_MASK_BYTES * 8U >= 128U) ? 1 : -1];

/*
 * Maximum size of a complete signed protocol blob: header + every command
 * (each a command header plus a full target block plus a full params[] payload)
 * + Ed25519 signature.  Sizes the protocol receive/reassembly buffers.  Used
 * only for array sizing, so the sizeof() operands are legal here.
 */
#define NP_HUB_PROTO_BLOB_MAX  (sizeof(np_proto_header_t) +                       \
    (size_t)NP_HUB_PROTO_CMD_MAX * (sizeof(np_proto_cmd_hdr_t) +                  \
                                    NP_HUB_PROTO_TARGET_MAX +                     \
                                    NP_HUB_PROTO_PARAMS_MAX) +                    \
    NP_HUB_PROTO_SIG_LEN)

/* ── Parsed command (held in LPSDR4 RAM after protocol parse) ─────────────────── */

typedef struct {
    np_hub_mod_type_t mod_type;
    uint8_t           slot_id;      /* NP_HUB_SLOT_NONE unless target_kind is SLOT */
    uint32_t          start_ms;
    uint32_t          duration_ms;
    uint16_t          params_len;
    uint8_t           params[NP_HUB_PROTO_PARAMS_MAX];
    /* Target block. target_kind is always one of np_proto_target_kind_t (the
     * parser rejects anything else). socket_mask is meaningful only when
     * target_kind == NP_PROTO_TARGET_SOCKET_MASK, and is zeroed otherwise so a
     * caller that forgets to check the kind reads an empty target, not a stale
     * one — the fail-closed direction. */
    uint8_t           target_kind;
    uint8_t           socket_mask[NP_HUB_SOCKET_MASK_BYTES];
} np_session_cmd_t;

/* ── Parsed session descriptor ────────────────────────────────────────────────── */

typedef struct {
    uint8_t           session_uuid[NP_HUB_PROTO_UUID_LEN];
    uint32_t          compiled_at_unix;
    uint32_t          duration_ms;
    uint8_t           flags;
    uint8_t           cmd_count;
    np_session_cmd_t  cmds[NP_HUB_PROTO_CMD_MAX];
} np_session_desc_t;

/* ── Protocol flags ───────────────────────────────────────────────────────────── */

#define NP_PROTO_FLAG_T2_TIER       (1U << 0)
#define NP_PROTO_FLAG_AUTONOMOUS    (1U << 1)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Per-module command parameter structs
 * All packed — these map directly over the params[] array in np_session_cmd_t.
 * params_len == 0 is the stop signal (hub calls control(slot, NULL, 0)).
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* ── PBM base zone module (NP_MOD_PBM_BASE) ─────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t freq_code;     /* 0x00=CW 0x01=2Hz 0x06=6Hz 0x0A=10Hz 0x14=20Hz 0x28=40Hz */
    uint8_t duty;          /* duty register value ≤ 0x32 (25%); firmware-enforced */
    uint8_t cur_a;         /* 660nm LED current register */
    uint8_t cur_b;         /* 808nm LED current register */
} np_mod_pbm_base_params_t;

/* ── PBM smart zone module (NP_MOD_PBM_SMART) ───────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t freq_code;
    uint8_t duty;          /* ≤ 0x32 for all channels; firmware-enforced */
    uint8_t cur_a;         /* 660nm */
    uint8_t cur_b;         /* 808nm */
    uint8_t cur_c;         /* 1064nm */
    uint8_t ch_mask;       /* channel enable: bit0=660nm, bit1=808nm, bit2=1064nm */
} np_mod_pbm_smart_params_t;

/* ── Intranasal Y-probe (NP_MOD_INTRANASAL) ─────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t side;          /* 0=bilateral, 1=left, 2=right */
    uint8_t freq_code;
    uint8_t duty;          /* ≤ 0x32 */
    uint8_t cur_660;       /* 660nm current register */
    uint8_t cur_808;       /* 808nm current register */
} np_mod_intranasal_params_t;

/* ── EEG (NP_MOD_EEG) ───────────────────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t channel_mask;  /* bit n = 1 → enable channel n (n=0..7: Fp1/2 F3/4 C3/4 P3/4) */
    uint8_t gain;          /* ADS1299 PGA: 0=1× 1=2× 2=4× 3=6× 4=8× 5=12× 6=24× */
    uint8_t notch;         /* 0=none, 1=50Hz, 2=60Hz */
    uint8_t ref_mode;      /* 0=linked-ear (A1+A2), 1=CSD */
    uint8_t adaptive_out;  /* 0=none, 1→PBM freq, 2→audio freq, 3→both */
} np_mod_eeg_params_t;

/* ── BES / tACS (NP_MOD_BES_TACS) ─────────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  electrode_pair; /* 0=F3-F4 1=P3-P4 2=Fz-Pz (see pinout) */
    uint16_t freq_mhz;       /* frequency in milli-Hz (1000=1Hz, 40000=40Hz) */
    uint16_t amplitude_ua;   /* peak amplitude µA; firmware cap ≤ 1000 (1mA) */
    uint8_t  waveform;       /* 0=sinusoidal, 1=biphasic square */
    uint8_t  phase_deg;      /* relative phase 0–359 */
} np_mod_bes_tacs_params_t;

/* ── tDCS (NP_MOD_TDCS) ─────────────────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  electrode_pair; /* channel pair — see tDCS montage table */
    uint16_t current_ua;     /* DC current µA; firmware cap ≤ 2000 (2mA) */
    uint8_t  polarity;       /* 0=anode at pair-A, 1=cathode at pair-A */
    uint16_t ramp_s;         /* ramp duration; firmware minimum 30s enforced */
} np_mod_tdcs_params_t;

/* ── VNS + HRV auricular clip (NP_MOD_VNS_HRV) ─────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  side;            /* 0=left, 1=right, 2=bilateral */
    uint16_t freq_mhz;        /* stim freq mHz; range 1000–25000 (1–25Hz) */
    uint16_t amplitude_ua;    /* µA; firmware cap ≤ 2000 */
    uint8_t  pulse_width_us;  /* biphasic pulse width µs; 0=default 250µs */
    uint8_t  ppg_enable;      /* 1=enable PPG HRV monitoring (808-830nm PPG) */
    uint8_t  eeg_ref_enable;  /* 1=route A1/A2 clip pads to EEG reference */
    uint8_t  hrv_proto;       /* 0=VNS only, 1=VNS+HRV, 2=RSA sync, 3=dual biofb */
} np_mod_vns_hrv_params_t;

/* ── Audio (NP_MOD_AUDIO) ───────────────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  mode;           /* 0=binaural, 1=isochronic, 2=pink, 3=brown, 4=off */
    uint16_t carrier_hz;     /* carrier frequency Hz (binaural/isochronic) */
    uint16_t beat_mhz;       /* beat / modulation frequency mHz */
    uint8_t  volume_pct;     /* 0–100 */
    uint8_t  bone_conduct_en;/* 1=enable mastoid bone conduction */
    uint8_t  eeg_adaptive;   /* 1=lock beat_mhz to EEG dominant band output */
} np_mod_audio_params_t;

/* ── Visual stimulation goggles (NP_MOD_VISUAL) ─────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  mode;           /* 0=photic, 1=EMDR, 2=retinalPBM, 3=combined */
    uint8_t  freq_hz;        /* photic drive Hz (0.5–100); 0 for EMDR/PBM only */
    uint8_t  duty_pct;       /* duty cycle 0–100 */
    uint8_t  wl_select;      /* 0=660nm, 1=808nm, 2=both */
    uint8_t  zone_mask_lo;   /* zones 0–7 (lo byte of 12-bit per-eye zone mask) */
    uint8_t  zone_mask_hi;   /* zones 8–11 (hi nibble) */
    uint8_t  emdr_rate_mhz;  /* EMDR L/R alternation rate mHz */
    uint8_t  mode_f_enable;  /* 1=Mode F (NIR retinal walk, 808-830nm) */
    uint8_t  shade_req;      /* 0=none, 1=S1_opaque */
} np_mod_visual_params_t;

/* ── Cervical VNS T2 (NP_MOD_CVNS) ─────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  side;            /* 0=right, 1=left, 2=bilateral */
    uint16_t freq_mhz;        /* mHz; safety MCU enforces cardiac interlock */
    uint16_t amplitude_ua;    /* µA; firmware cap ≤ 2000 */
    uint16_t pulse_width_us;  /* µs; firmware clamps to 200–1000 (0 → 250 default) */
    uint16_t ramp_s;          /* ramp; firmware minimum 10s enforced */
    uint8_t  baseline_req;    /* 1=safety MCU must establish HR baseline before enable */
} np_mod_cvns_params_t;

/* ── T2: 21-ch qEEG (NP_MOD_QEEG_21CH) ─────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint32_t channel_mask;  /* 21 LSBs; bit n = channel n; 0x1FFFFF = all */
    uint8_t  gain;          /* ADS1299 PGA: 0=1× 1=2× 2=4× 3=6× 4=8× 5=12× 6=24× */
    uint8_t  notch;         /* 0=none, 1=50Hz, 2=60Hz */
    uint8_t  ref_mode;      /* 0=linked_ear (A1+A2), 1=Cz, 2=average */
    uint8_t  sloreta_en;    /* 1=run sLORETA source imaging; target map written to LPSDR4 */
} np_mod_qeeg_21ch_params_t;

/* ── T2: TMS focal figure-8 coil (NP_MOD_TMS) ──────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  protocol;          /* 0=rTMS, 1=TBS, 2=iTBS */
    uint8_t  target;            /* 0=DLPFC_L 1=DLPFC_R 2=VLPFC_L 3=ACC 4=MPFC 5=M1_L 6=M1_R */
    uint16_t freq_mhz;          /* stim freq mHz (10000=10Hz, 50000=50Hz for TBS) */
    uint8_t  intensity_pct_mt;  /* % motor threshold; safety MCU enforces ceiling */
    uint16_t pulse_count;       /* total pulses this command */
    uint16_t inter_train_ms;    /* inter-train interval ms */
    uint8_t  tbs_burst_hz;      /* burst freq for TBS/iTBS (50); 0 for rTMS */
} np_mod_tms_params_t;

/* ── T2: 1170nm deep PBM laser (NP_MOD_PBM_1170NM) ────────────────────────── */

typedef struct __attribute__((packed)) {
    uint16_t intensity_mw_cm2;  /* mW/cm²; firmware cap ≤ 1000 */
    uint8_t  freq_code;         /* 0x00=CW; freq in Hz for pulsed modes */
    uint8_t  duty;              /* ≤ 0x32 (25%); firmware-enforced */
    uint8_t  tec_target_c;      /* TEC stabilization target °C; 0=auto (37°C) */
} np_mod_pbm_1170nm_params_t;

/* ── T2: 21-ch clinical tACS (NP_MOD_CLIN_TACS) ────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint16_t freq_mhz;          /* mHz; adaptive EMF notch enforced by safety MCU */
    uint16_t amplitude_ua;      /* peak µA per channel; firmware cap ≤ 4000 */
    uint8_t  channel_mask_lo;   /* channels 0–7 enable bitmask */
    uint8_t  channel_mask_hi;   /* channels 8–15 enable bitmask */
    uint8_t  waveform;          /* 0=sinusoidal, 1=biphasic square, 2=triangular */
} np_mod_clin_tacs_params_t;

/* ── T2: sLORETA-guided 4×1 HD-tDCS (NP_MOD_HD_TDCS) ──────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  target;            /* same 7-target enum as NP_MOD_TMS */
    uint8_t  montage;           /* 0=ring_4x1, 1=bilateral_4x1, 2=standard_2el */
    uint16_t current_ua;        /* DC µA; cap ≤ 2000; safety MCU enforces 40µC/cm² */
    uint16_t ramp_s;            /* ramp seconds; firmware minimum 30s enforced */
} np_mod_hd_tdcs_params_t;

/* ── Accessory: mastoid LRA 40Hz vibrotactile (NP_MOD_VIBROTACTILE) ─────────── */

typedef struct __attribute__((packed)) {
    uint8_t  gain_reg;          /* DRV2605L gain register 0x00–0x7F (0x00≈0.6G, 0x7F≈1.2G) */
    uint8_t  sync_flags;        /* bit0=sync_to_audio, bit1=sync_to_visual */
    uint16_t freq_mhz;          /* drive freq mHz; firmware locks to 40000 ± 500 */
} np_mod_vibrotactile_params_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Session state
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    NP_SESSION_IDLE       = 0,
    NP_SESSION_LOADING    = 1,   /* receiving protocol blob from app */
    NP_SESSION_VERIFYING  = 2,   /* Ed25519 verify in progress */
    NP_SESSION_RUNNING    = 3,
    NP_SESSION_PAUSED     = 4,
    NP_SESSION_STOPPING   = 5,   /* graceful shutdown of all active modules */
    NP_SESSION_COMPLETE   = 6,
    NP_SESSION_FAULT      = 7,
} np_session_state_t;

/* ── Session abort reason (SHDR-safe) ────────────────────────────────────────── */

typedef enum {
    NP_ABORT_NONE           = 0,
    NP_ABORT_USER           = 1,   /* user stopped session via app */
    NP_ABORT_SAFETY_MCU     = 2,   /* safety MCU asserted fault */
    NP_ABORT_WATCHDOG       = 3,   /* heartbeat watchdog fired */
    NP_ABORT_THERMAL        = 4,   /* over-temperature on any module */
    NP_ABORT_POWER          = 5,   /* USB-C PD negotiation failed */
    NP_ABORT_EEG_SEIZURE    = 6,   /* photoparoxysmal detection at Oz */
    NP_ABORT_MOD_FAULT      = 7,   /* module reported unrecoverable fault */
    NP_ABORT_LOG_FAIL       = 8,   /* UHDR write failure (eMMC) */
} np_abort_reason_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Telemetry records
 *
 * Each module's np_mod_telemetry_fn fills one np_telemetry_record_t per tick.
 * The session logger routes it to UHDR or SHDR per the data classification
 * table in NP-FW-EMMC-001 Rev A §12.
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* PD1/PD2 ratio per zone — SHDR (device condition; no user biology). */
    float   pd_ratio[NP_HUB_ZONE_SLOT_COUNT];
    /* Per-zone NTC temperature peak — SHDR. */
    float   ntc_peak_c[NP_HUB_ZONE_SLOT_COUNT];
    /* Cumulative dose per zone (sum of wavelengths) — UHDR. */
    float   dose_J_cm2[NP_HUB_ZONE_SLOT_COUNT];
    /* Aggregate irradiance mW/cm² — UHDR. */
    float   irradiance_mW_cm2;
    /* Throttle event count since session start — SHDR. */
    uint16_t throttle_events;
    uint8_t  fault_mask;   /* bit n = 1 → slot n has active fault (SHDR) */
} np_telem_pbm_t;

typedef struct {
    /* EEG waveform ring buffer (DMA-driven, NOT in this snapshot) — UHDR. */
    /* Impedance per channel (kohm) — UHDR. */
    float    impedance_kohm[NP_EEG_CHANNELS];
    /* EEG adaptive output: current dominant band — UHDR. */
    uint8_t  dominant_band;  /* np_eeg_band_t value */
    float    band_power_db[5]; /* delta/theta/alpha/beta/gamma (UHDR) */
} np_telem_eeg_t;

typedef struct {
    float    rmssd_ms;          /* HRV RMSSD — UHDR */
    float    coherence_score;   /* LF/(LF+HF)×10 — UHDR */
    float    hr_bpm;            /* instantaneous HR — UHDR */
    uint16_t impedance_ohm;     /* electrode contact impedance — UHDR */
    bool     contact_confirmed; /* SHDR-safe: bool only */
} np_telem_vns_hrv_t;

typedef struct {
    float    current_ua;        /* actual delivered current — UHDR */
    float    impedance_kohm;    /* electrode impedance — UHDR */
    uint16_t charge_uC;         /* cumulative charge this session — UHDR */
    bool     ramp_active;       /* SHDR */
} np_telem_stim_t;

typedef struct {
    uint8_t  active_mode;       /* current mode — SHDR */
    uint8_t  hall_state;        /* 0=goggles on, 1=lifted — SHDR */
    bool     eye_open;          /* IR proximity — UHDR */
    bool     ppx_halt;          /* photoparoxysmal halt in effect — SHDR */
} np_telem_visual_t;

typedef struct {
    /* Per-channel impedance (UHDR); 21 elements for T2 wet-gel cap. */
    float    impedance_kohm[NP_QEEG_CHANNELS];
    uint32_t channel_mask;      /* active channel bitmask — SHDR */
    bool     sloreta_active;    /* SHDR */
} np_telem_qeeg_t;

typedef struct {
    uint16_t pulse_count;       /* pulses delivered this command — SHDR */
    uint8_t  coil_temp_c;       /* coil temperature °C — SHDR */
    bool     emf_gate_active;   /* Helmholtz cancellation gate state — SHDR */
} np_telem_tms_t;

typedef struct {
    float    intensity_mw_cm2;  /* actual delivered irradiance — UHDR */
    float    dose_J_cm2;        /* cumulative dose this session — UHDR */
    float    tec_temp_c;        /* TEC measured temperature — SHDR */
    bool     throttle_active;   /* SHDR */
} np_telem_pbm_1170nm_t;

typedef struct {
    float    current_ua;        /* actual delivered current — UHDR */
    float    impedance_kohm;    /* electrode impedance — UHDR */
    bool     ramp_active;       /* SHDR */
} np_telem_clin_tacs_t;

typedef struct {
    uint8_t  gain_reg;          /* DRV2605L actual gain register — SHDR */
    bool     sync_locked;       /* sync signal detected — SHDR */
} np_telem_vibrotactile_t;

/* Union of all module telemetry types, discriminated by mod_type. */
typedef struct {
    np_hub_mod_type_t mod_type;
    uint8_t           slot;
    uint32_t          session_ms;   /* session-relative timestamp */
    union {
        np_telem_pbm_t          pbm;
        np_telem_eeg_t          eeg;
        np_telem_vns_hrv_t      vns_hrv;
        np_telem_stim_t         stim;
        np_telem_visual_t       visual;
        np_telem_qeeg_t         qeeg;
        np_telem_tms_t          tms;
        np_telem_pbm_1170nm_t   pbm_1170nm;
        np_telem_clin_tacs_t    clin_tacs;   /* also used for hd_tdcs (shared HW) */
        np_telem_vibrotactile_t vibrotactile;
    } data;
} np_telem_record_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * UHDR / SHDR session record (written at session end)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t  session_uuid[NP_HUB_PROTO_UUID_LEN]; /* from protocol (UHDR key) */
    uint32_t start_unix;         /* session start epoch (UHDR) */
    uint32_t duration_s;         /* actual elapsed (UHDR) */
    uint8_t  abort_reason;       /* np_abort_reason_t (UHDR) */
    uint32_t mods_active_mask;   /* bitmask: bit n = NP_MOD_* n was active (UHDR);
                                  * uint32 required — NP_MOD_VIBROTACTILE = 0x10 */
} np_session_uhdr_record_t;

typedef struct {
    uint32_t device_session_count; /* unsigned; no timestamp (SHDR) */
    uint32_t duration_s;
    uint8_t  abort_reason;
    uint8_t  firmware_version[4];  /* semver packed */
    uint32_t mods_active_mask;     /* same bit encoding as UHDR record */
} np_session_shdr_record_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Module control function pointer types
 * ═══════════════════════════════════════════════════════════════════════════════ */

/*
 * init:     Called once when module is detected; allocates HAL resources.
 * control:  Called by session runner when a command becomes due.
 *           params==NULL, params_len==0 → graceful stop (with ramp-down).
 * telemetry:Called by telemetry task; fills *out with a snapshot.
 * shutdown: Called on session abort or module removal; immediate stop.
 */
typedef np_hub_status_t (*np_mod_init_fn)    (uint8_t slot);
typedef np_hub_status_t (*np_mod_control_fn) (uint8_t slot,
                                               const void *params,
                                               uint16_t    params_len);
typedef np_hub_status_t (*np_mod_telemetry_fn)(uint8_t          slot,
                                                np_telem_record_t *out);
typedef np_hub_status_t (*np_mod_shutdown_fn)(uint8_t slot);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Safety MCU SPI frame
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct __attribute__((packed)) {
    uint8_t  magic[2];        /* NP_SAFETY_BEAT_MAGIC_0 / _1 */
    uint8_t  session_status;  /* NP_SESSION_STATUS_* bit flags (2 bits used) */
    uint8_t  enable_lo;       /* requested enable bitmask bits 0-7 */
    uint8_t  enable_hi;       /* bits 8-15 */
    uint8_t  reserved;
    uint16_t checksum;        /* sum of bytes [0..5]; wrapping uint16 */
} np_safety_tx_frame_t;

typedef struct __attribute__((packed)) {
    uint8_t  status;          /* NP_SAFETY_STATUS_* flags */
    uint8_t  granted_lo;      /* actually-granted enable bitmask bits 0-7 */
    uint8_t  granted_hi;
    uint8_t  fault_slot;      /* slot that caused fault; 0xFF=none */
    uint8_t  reserved[2];
    uint16_t checksum;
} np_safety_rx_frame_t;

/* np_safety_sig_cmd_t and all session-signature command frame constants
 * (NP_SAFETY_CMD_MAGIC_0/1, NP_SAFETY_CMD_SESSION_SIG, NP_SAFETY_CMD_FRAME_LEN,
 * NP_SESSION_HASH_LEN, NP_ED25519_SIG_LEN) come from:
 *   firmware/common/include/np_spi_wire_types.h
 * which is included transitively via np_hub_config.h above.
 * Single source of truth — no duplicate definition in this file. */

#endif /* NP_HUB_TYPES_H */
