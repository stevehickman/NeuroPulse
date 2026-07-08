/*
 * NeurOne HRV Biofeedback — PPG Signal Processing
 * Document: NP-FW-HRV-001 §2
 */

#include "np_hrv_ppg.h"
#include <string.h>
#include <math.h>

void np_hrv_ppg_init(np_ppg_detector_t *det, int32_t initial_threshold)
{
    memset(det, 0, sizeof(*det));
    det->adaptive_threshold  = initial_threshold;
    det->last_peak_time_ms   = 0U;
    det->refractory_count    = 0U;
    det->above_threshold     = false;
    det->peak_candidate      = 0;
    det->peak_candidate_time_ms = 0U;
}

/* ── R-R buffer helpers ─────────────────────────────────────────────────────── */

void np_hrv_rr_push(np_rr_buffer_t *buf, uint16_t rr_ms, uint32_t timestamp_ms)
{
    if (rr_ms < NP_RR_MIN_MS || rr_ms > NP_RR_MAX_MS) {
        return;
    }
    buf->intervals_ms[buf->head]  = rr_ms;
    buf->timestamps_ms[buf->head] = timestamp_ms;
    buf->head = (uint16_t)((buf->head + 1U) % NP_RR_BUFFER_SIZE);
    if (buf->count < NP_RR_BUFFER_SIZE) {
        buf->count++;
    }
}

uint16_t np_hrv_rr_get(const np_rr_buffer_t *buf, uint16_t idx)
{
    if (idx >= buf->count) {
        return 0U;
    }
    /* Oldest entry is at (head - count) mod BUFFER_SIZE. */
    uint16_t pos = (uint16_t)((buf->head + NP_RR_BUFFER_SIZE - buf->count + idx)
                               % NP_RR_BUFFER_SIZE);
    return buf->intervals_ms[pos];
}

uint32_t np_hrv_rr_get_timestamp(const np_rr_buffer_t *buf, uint16_t idx)
{
    if (idx >= buf->count) {
        return 0U;
    }
    uint16_t pos = (uint16_t)((buf->head + NP_RR_BUFFER_SIZE - buf->count + idx)
                               % NP_RR_BUFFER_SIZE);
    return buf->timestamps_ms[pos];
}

/* ── PPG adaptive peak detector ─────────────────────────────────────────────── */

bool np_hrv_ppg_process_sample(np_ppg_detector_t *det,
                                np_rr_buffer_t    *rr_buf,
                                int32_t            sample,
                                uint32_t           timestamp_ms)
{
    bool new_rr = false;

    /* IIR threshold update: threshold tracks the signal envelope from below.   */
    /* alpha = NP_PPG_ADAPTIVE_THRESH_ALPHA (0.125 = 1/8, avoids division).     */
    det->adaptive_threshold += (int32_t)((float)(sample - det->adaptive_threshold)
                                          * NP_PPG_ADAPTIVE_THRESH_ALPHA);

    /* Refractory period: ignore samples immediately after a confirmed peak.    */
    if (det->refractory_count > 0U) {
        det->refractory_count--;
        /* Still update threshold tracking so it doesn't lag. */
        return false;
    }

    int32_t threshold = det->adaptive_threshold;

    if (!det->above_threshold) {
        /* Rising edge detection */
        if (sample > threshold) {
            det->above_threshold        = true;
            det->peak_candidate         = sample;
            det->peak_candidate_time_ms = timestamp_ms;
        }
    } else {
        /* Track peak of current excursion */
        if (sample > det->peak_candidate) {
            det->peak_candidate         = sample;
            det->peak_candidate_time_ms = timestamp_ms;
        }
        /* Falling edge: excursion has ended */
        if (sample <= threshold) {
            det->above_threshold = false;

            /* Enforce minimum R-R separation */
            uint32_t dt = det->peak_candidate_time_ms - det->last_peak_time_ms;
            if (dt >= NP_RR_MIN_MS) {
                uint16_t rr_ms = (dt > 0xFFFFU) ? 0xFFFFU : (uint16_t)dt;
                np_hrv_rr_push(rr_buf, rr_ms, det->peak_candidate_time_ms);
                det->last_peak_time_ms = det->peak_candidate_time_ms;
                det->refractory_count  = NP_PPG_REFRACTORY_SAMPLES;
                new_rr = true;
            }

            det->peak_candidate         = 0;
            det->peak_candidate_time_ms = 0U;
        }
    }

    return new_rr;
}

/* ── RMSSD ───────────────────────────────────────────────────────────────────── */

np_hrv_status_t np_hrv_ppg_rmssd(const np_rr_buffer_t *buf, float *rmssd_ms_out)
{
    if (buf->count < 2U) {
        return NP_HRV_ERR_NOT_READY;
    }

    uint16_t n = (buf->count < NP_RR_RMSSD_WINDOW) ? buf->count : NP_RR_RMSSD_WINDOW;
    /* Use the most recent `n` intervals: indices (count-n) .. (count-1). */
    uint16_t start = buf->count - n;

    float sum_sq = 0.0f;
    for (uint16_t i = start; i < buf->count - 1U; i++) {
        float d = (float)np_hrv_rr_get(buf, (uint16_t)(i + 1U))
                - (float)np_hrv_rr_get(buf, i);
        sum_sq += d * d;
    }

    uint16_t pairs = n - 1U;
    *rmssd_ms_out = sqrtf(sum_sq / (float)pairs);
    return NP_HRV_OK;
}
