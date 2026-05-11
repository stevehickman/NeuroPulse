/*
 * NeuroPulse Cervical VNS — Cardiac Interlock (Main Processor Side)
 * Document: NP-FW-CVNS-001 Rev A §6
 *
 * Implements Pan-Tompkins R-peak detection on the PPG signal from the cervical
 * VNS accessory, maintains a rolling R-R interval buffer, and computes baseline
 * HR for cross-validation with the safety MCU.
 *
 * Safety MCU (STM32G071, bare-metal, IEC 62304 Class C) independently monitors
 * the R-peak GPIO line and holds the cervical VNS enable GPIO.  The cutoff
 * latency guarantee (< 100 ms) is enforced by the safety MCU's 5 ms TIM6 ISR,
 * not by this module.
 */

#include "np_cvns_interlock.h"
#include <string.h>
#include <math.h>

/* ── Platform HAL stubs ──────────────────────────────────────────────────────── */

static void platform_rpeak_gpio_pulse(void)
{
    /* Platform HAL: assert RPEAK_IN GPIO high for 5 ms to signal safety MCU.   */
    /* np_platform_cvns_rpeak_gpio_pulse(); */
    /* OI-CVNS-01: implement before integration testing.                        */
}

static void platform_spi_send(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    /* Platform HAL: initiate SPI transfer to safety MCU.                       */
    /* np_platform_cvns_spi_transfer(cmd, payload, len, NULL, 0);               */
    (void)cmd;
    (void)payload;
    (void)len;
}

/* ── R-R buffer helpers ──────────────────────────────────────────────────────── */

static void rr_push(np_cvns_rr_buf_t *buf, uint16_t rr_ms)
{
    buf->rr_ms[buf->head] = rr_ms;
    buf->head = (uint8_t)((buf->head + 1U) % NP_CVNS_RR_WINDOW_SIZE);
    if (buf->count < NP_CVNS_RR_WINDOW_SIZE) {
        buf->count++;
    }
}

static float rr_mean_bpm(const np_cvns_rr_buf_t *buf, uint8_t n)
{
    if (buf->count == 0U || n == 0U) {
        return 0.0f;
    }
    if (n > buf->count) {
        n = buf->count;
    }

    uint32_t sum_ms = 0U;
    uint8_t  start  = (uint8_t)(((uint16_t)buf->head +
                                   NP_CVNS_RR_WINDOW_SIZE - n) %
                                  NP_CVNS_RR_WINDOW_SIZE);
    for (uint8_t i = 0U; i < n; i++) {
        uint8_t idx = (uint8_t)((start + i) % NP_CVNS_RR_WINDOW_SIZE);
        sum_ms += buf->rr_ms[idx];
    }

    float mean_ms = (float)sum_ms / (float)n;
    return (mean_ms > 0.0f) ? (60000.0f / mean_ms) : 0.0f;
}

/* ── Pan-Tompkins detection pipeline ─────────────────────────────────────────── */

static bool pt_process_sample(np_cvns_interlock_ctx_t *ctx,
                                float                    sample,
                                uint32_t                 timestamp_ms)
{
    /* Step 1: simple first-order IIR bandpass (0.5–40 Hz) approximation.       */
    /* A full FIR implementation requires the firmware team per OI-CVNS-01.     */
    /* This single-pole approximation is sufficient for host-CI plumbing tests. */
    float alpha = 0.1f;  /* 0.5 Hz pole at 200 Hz sample rate                  */
    ctx->pt_filtered = ctx->pt_filtered + alpha * (sample - ctx->pt_filtered);

    /* Step 2: first difference. */
    float diff = sample - ctx->pt_filtered;

    /* Step 3: square. */
    float sq = diff * diff;

    /* Step 4: moving window integrate (approximated by IIR). */
    float beta = 1.0f / (float)(NP_CVNS_PPG_SAMPLE_RATE_HZ *
                                  NP_CVNS_PT_INTEGRATE_WIN_MS / 1000U);
    ctx->pt_integrated = ctx->pt_integrated * (1.0f - beta) + sq * beta;

    /* Step 5: update running maximum over last NP_CVNS_PT_HISTORY_S seconds. */
    if (ctx->pt_integrated > ctx->pt_running_max) {
        ctx->pt_running_max = ctx->pt_integrated;
    }

    /* Decay running max (approximately exponential over PT_HISTORY_S). */
    float decay = 1.0f / (float)(NP_CVNS_PPG_SAMPLE_RATE_HZ * NP_CVNS_PT_HISTORY_S);
    ctx->pt_running_max -= ctx->pt_running_max * decay;

    /* Step 6: adaptive threshold. */
    ctx->pt_threshold = NP_CVNS_PT_THRESHOLD_FRAC * ctx->pt_running_max;

    /* Step 7: peak detection with refractory period. */
    bool peak_detected = false;
    uint32_t since_last = timestamp_ms - ctx->pt_last_peak_ms;

    if (ctx->pt_integrated > ctx->pt_threshold &&
        since_last >= NP_CVNS_PT_REFRACTORY_MS) {

        /* Compute R-R interval. */
        uint32_t rr_ms = since_last;

        if (rr_ms >= NP_CVNS_RR_MIN_VALID_MS &&
            rr_ms <= NP_CVNS_RR_MAX_VALID_MS) {

            rr_push(&ctx->rr_buf, (uint16_t)rr_ms);
            if (ctx->baseline_beats_accumulated < UINT8_MAX) {
                ctx->baseline_beats_accumulated++;
            }
            peak_detected = true;
        }

        ctx->pt_last_peak_ms = timestamp_ms;
        ctx->last_rpeak_ms   = timestamp_ms;

        /* Reset running max post-peak so next threshold adapts quickly. */
        ctx->pt_running_max = ctx->pt_integrated;
    }

    return peak_detected;
}

/* ── Lifecycle ───────────────────────────────────────────────────────────────── */

np_cvns_status_t np_cvns_interlock_init(np_cvns_interlock_ctx_t   *ctx,
                                         np_cvns_interlock_config_t config,
                                         np_cvns_fault_cb_t         fault_cb)
{
    if (!ctx || !fault_cb) {
        return NP_CVNS_ERR_INVALID_ARG;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->state    = NP_CVNS_INTERLOCK_IDLE;
    ctx->fault_cb = fault_cb;

    (void)config;  /* sample rate recorded for future FIR coefficient computation */
    return NP_CVNS_OK;
}

void np_cvns_interlock_deinit(np_cvns_interlock_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    /* Ensure safety MCU is disabled before clearing context. */
    uint8_t cmd = NP_CVNS_SPI_CMD_DISABLE;
    platform_spi_send(cmd, NULL, 0U);
    memset(ctx, 0, sizeof(*ctx));
}

/* ── PPG feed-in ─────────────────────────────────────────────────────────────── */

void np_cvns_interlock_push_ppg(np_cvns_interlock_ctx_t *ctx,
                                  uint32_t                  sample,
                                  uint32_t                  timestamp_ms)
{
    if (!ctx) {
        return;
    }

    bool peak = pt_process_sample(ctx, (float)sample, timestamp_ms);

    if (peak) {
        /* Pulse the R-peak GPIO to the safety MCU. */
        platform_rpeak_gpio_pulse();

        /* Once enough beats have accumulated, update baseline. */
        if (!ctx->baseline_valid &&
            ctx->baseline_beats_accumulated >= NP_CVNS_BASELINE_BEATS_MIN) {
            ctx->baseline_hr_bpm = rr_mean_bpm(&ctx->rr_buf,
                                                 NP_CVNS_BASELINE_BEATS_MIN);
            if (ctx->baseline_hr_bpm > 0.0f) {
                ctx->baseline_valid = true;
            }
        }
    }
}

/* ── Session control ─────────────────────────────────────────────────────────── */

np_cvns_status_t np_cvns_interlock_request_enable(np_cvns_interlock_ctx_t *ctx,
                                                    uint32_t                  now_s)
{
    if (!ctx) {
        return NP_CVNS_ERR_INVALID_ARG;
    }
    if (ctx->state == NP_CVNS_INTERLOCK_ENABLED ||
        ctx->state == NP_CVNS_INTERLOCK_PRE_SESSION) {
        return NP_CVNS_ERR_SESSION_ACTIVE;
    }
    if (ctx->state == NP_CVNS_INTERLOCK_FAULT) {
        /* Must use request_reenable() after a fault. */
        return NP_CVNS_ERR_LOCKOUT;
    }
    if (!ctx->baseline_valid) {
        return NP_CVNS_ERR_BASELINE_INVALID;
    }

    ctx->state = NP_CVNS_INTERLOCK_PRE_SESSION;

    /* Push baseline HR to safety MCU for cross-validation. */
    uint16_t hr_x10 = (uint16_t)(ctx->baseline_hr_bpm * 10.0f);
    uint8_t  payload[2] = { (uint8_t)(hr_x10 >> 8), (uint8_t)(hr_x10 & 0xFFu) };
    platform_spi_send(NP_CVNS_SPI_CMD_HR_BASELINE_SET, payload, sizeof(payload));

    /* Request enable; safety MCU will grant only if its baseline agrees. */
    platform_spi_send(NP_CVNS_SPI_CMD_ENABLE, NULL, 0U);

    (void)now_s;
    return NP_CVNS_OK;
}

void np_cvns_interlock_disable(np_cvns_interlock_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    platform_spi_send(NP_CVNS_SPI_CMD_DISABLE, NULL, 0U);
    ctx->state = NP_CVNS_INTERLOCK_IDLE;
}

np_cvns_status_t np_cvns_interlock_request_reenable(np_cvns_interlock_ctx_t *ctx,
                                                      bool                     app_confirmed,
                                                      uint32_t                 now_s)
{
    if (!ctx) {
        return NP_CVNS_ERR_INVALID_ARG;
    }
    if (ctx->state != NP_CVNS_INTERLOCK_FAULT) {
        return NP_CVNS_ERR_NO_SESSION;
    }
    if (!app_confirmed) {
        return NP_CVNS_ERR_SAFETY_REJECTED;
    }

    uint32_t elapsed_s = now_s - ctx->fault_time_s;
    if (elapsed_s < NP_CVNS_REENABLE_LOCKOUT_S) {
        return NP_CVNS_ERR_LOCKOUT;
    }

    /* Baseline must have been re-established during the lockout period. */
    if (!ctx->baseline_valid ||
        ctx->baseline_beats_accumulated < NP_CVNS_BASELINE_BEATS_MIN) {
        return NP_CVNS_ERR_BASELINE_INVALID;
    }

    ctx->state = NP_CVNS_INTERLOCK_IDLE;
    return np_cvns_interlock_request_enable(ctx, now_s);
}

/* ── Safety MCU SPI response handler ────────────────────────────────────────── */

void np_cvns_interlock_spi_response(np_cvns_interlock_ctx_t *ctx,
                                     uint8_t                   cmd,
                                     const uint8_t            *payload,
                                     uint8_t                   len)
{
    if (!ctx) {
        return;
    }

    switch (cmd) {

    case NP_CVNS_SPI_RSP_ENABLE_GRANTED:
        if (ctx->state == NP_CVNS_INTERLOCK_PRE_SESSION) {
            ctx->state = NP_CVNS_INTERLOCK_ENABLED;
        }
        break;

    case NP_CVNS_SPI_RSP_ENABLE_REJECTED:
        ctx->state        = NP_CVNS_INTERLOCK_FAULT;
        ctx->fault_reason = NP_CVNS_FAULT_SAFETY_MCU;
        if (ctx->fault_cb) {
            ctx->fault_cb(ctx, NP_CVNS_FAULT_SAFETY_MCU);
        }
        break;

    case NP_CVNS_SPI_RSP_FAULT_NOTIFY:
        /*
         * Safety MCU triggered cutoff.  reason byte in payload[0] if present.
         * The cutoff itself (GPIO) has already happened in hardware — this
         * message is the asynchronous notification to the main processor.
         */
        ctx->state = NP_CVNS_INTERLOCK_FAULT;

        if (len >= 1U && payload) {
            ctx->fault_reason = (np_cvns_fault_reason_t)payload[0];
        } else {
            ctx->fault_reason = NP_CVNS_FAULT_HR_CHANGE;
        }

        if (ctx->fault_cb) {
            ctx->fault_cb(ctx, ctx->fault_reason);
        }
        break;

    default:
        break;
    }
}

/* ── Periodic tick ───────────────────────────────────────────────────────────── */

void np_cvns_interlock_tick(np_cvns_interlock_ctx_t *ctx,
                              uint32_t                  now_ms,
                              uint32_t                  now_s)
{
    if (!ctx || ctx->state != NP_CVNS_INTERLOCK_ENABLED) {
        return;
    }

    /* Data-loss detection: if no R-peak seen for NP_CVNS_DATA_LOSS_TIMEOUT_S, */
    /* issue a soft cutoff.  The safety MCU's watchdog provides backup.         */
    uint32_t gap_ms = now_ms - ctx->last_rpeak_ms;
    if (gap_ms > (uint32_t)NP_CVNS_DATA_LOSS_TIMEOUT_S * 1000U) {
        platform_spi_send(NP_CVNS_SPI_CMD_DISABLE, NULL, 0U);
        ctx->state        = NP_CVNS_INTERLOCK_FAULT;
        ctx->fault_reason = NP_CVNS_FAULT_DATA_LOSS;
        ctx->fault_time_s = now_s;
        /* Reset baseline so it must be re-established before re-enable. */
        ctx->baseline_valid               = false;
        ctx->baseline_beats_accumulated   = 0U;
        if (ctx->fault_cb) {
            ctx->fault_cb(ctx, NP_CVNS_FAULT_DATA_LOSS);
        }
    }
}

/* ── State accessors ─────────────────────────────────────────────────────────── */

np_cvns_interlock_state_t np_cvns_interlock_state(const np_cvns_interlock_ctx_t *ctx)
{
    return ctx ? ctx->state : NP_CVNS_INTERLOCK_IDLE;
}

float np_cvns_interlock_baseline_hr(const np_cvns_interlock_ctx_t *ctx)
{
    return (ctx && ctx->baseline_valid) ? ctx->baseline_hr_bpm : 0.0f;
}

bool np_cvns_interlock_baseline_valid(const np_cvns_interlock_ctx_t *ctx)
{
    return ctx ? ctx->baseline_valid : false;
}

np_cvns_fault_reason_t np_cvns_interlock_fault_reason(const np_cvns_interlock_ctx_t *ctx)
{
    return ctx ? ctx->fault_reason : NP_CVNS_FAULT_NONE;
}

uint32_t np_cvns_interlock_reenable_lockout_remaining_s(
                                     const np_cvns_interlock_ctx_t *ctx,
                                     uint32_t                       now_s)
{
    if (!ctx || ctx->state != NP_CVNS_INTERLOCK_FAULT) {
        return 0U;
    }
    uint32_t elapsed = now_s - ctx->fault_time_s;
    return (elapsed >= NP_CVNS_REENABLE_LOCKOUT_S) ? 0U :
           (NP_CVNS_REENABLE_LOCKOUT_S - elapsed);
}
