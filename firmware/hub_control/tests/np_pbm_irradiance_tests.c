/*
 * NeurOne Hub Control — Transcranial PBM irradiance resolver + duty clamp tests
 * Document: NP-HW-HEXTILE-001 Rev 14 §4.3.3 (OI-HEXTILE-25)
 *
 * Properties:
 *   - irradiance is refused, never clamped: over R-4 for its mode (CW 200,
 *     pulsed 400), over R-5 on a three-channel tile (600), beyond the tile's
 *     reach, rounding to a zero code, or on a channel the tile has / enables not;
 *   - CW is continuous: 100 % duty whatever was requested; pulsed ≤ 25 %;
 *   - conversion never asks for more than the request (floor at the
 *     design-target flux);
 *   - np_pbm_drive clamps each channel by its OWN frequency code, and a channel
 *     leaving CW is brought under 25 % before its frequency changes.
 *
 * No FreeRTOS, no hardware. IEC 62304 Class B — SW-02 hub control.
 */

#include <stdio.h>
#include <string.h>

#include "np_pbm_irradiance.h"
#include "np_pbm_config.h"
#include "np_pbm_drive.h"
#include "np_pbm_hal.h"

static int g_failures = 0;

static void check(int cond, const char *name)
{
    if (cond) {
        printf("PASS: %s\n", name);
    } else {
        printf("FAIL: %s\n", name);
        g_failures++;
    }
}

/* ── Fake driver bus: records every register write in order ─────────────────── */

typedef struct { uint8_t reg; uint8_t val; } wr_t;
static wr_t     g_wr[64];
static unsigned g_nwr;

np_pbm_status_t np_pbm_hal_i2c_write(uint8_t slot, uint8_t reg_addr,
                                     const uint8_t *data, uint8_t len)
{
    (void)slot; (void)len;
    if (g_nwr < 64U) { g_wr[g_nwr].reg = reg_addr; g_wr[g_nwr].val = data[0]; g_nwr++; }
    return NP_PBM_OK;
}

np_pbm_status_t np_pbm_hal_i2c_read(uint8_t slot, uint8_t reg_addr,
                                    uint8_t *data, uint8_t len)
{
    (void)slot; (void)reg_addr; (void)len;
    data[0] = 0U;
    return NP_PBM_OK;
}

void np_pbm_hal_shdr_log_fault(const np_pbm_shdr_fault_entry_t *entry) { (void)entry; }

static int index_of(uint8_t reg, uint8_t val)
{
    for (unsigned i = 0; i < g_nwr; i++) {
        if (g_wr[i].reg == reg && g_wr[i].val == val) { return (int)i; }
    }
    return -1;
}

/* ── Resolver ──────────────────────────────────────────────────────────────── */

static np_hub_status_t base(uint8_t fc, uint8_t duty, uint16_t a, uint16_t b,
                            np_pbm_drive_cmd_t *out)
{
    const uint16_t irr[3] = { a, b, 0U };
    return np_pbm_irr_resolve(NP_MOD_PBM_BASE, fc, duty, irr, 0U, out);
}

static np_hub_status_t smart(uint8_t fc, uint8_t duty, uint16_t a, uint16_t b,
                             uint16_t c, uint8_t mask, np_pbm_drive_cmd_t *out)
{
    const uint16_t irr[3] = { a, b, c };
    return np_pbm_irr_resolve(NP_MOD_PBM_SMART, fc, duty, irr, mask, out);
}

static void test_ceilings_by_mode(void)
{
    np_pbm_drive_cmd_t c;
    check(base(0x28, 0x32, 400, 400, &c) == NP_HUB_OK, "pulsed 400 on T1-A admitted (R-4 peak)");
    check(base(0x28, 0x32, 401, 0, &c) == NP_HUB_ERR_INVALID_ARG, "pulsed 401 refused (R-4 peak)");
    check(base(0x00, 0x00, 200, 200, &c) == NP_HUB_OK, "CW 200 admitted (R-4 CW)");
    check(base(0x00, 0x00, 201, 0, &c) == NP_HUB_ERR_INVALID_ARG, "CW 201 refused (R-4 CW)");
    check(base(0x00, 0x00, 322, 322, &c) == NP_HUB_ERR_INVALID_ARG,
          "CW 322 (Vascular Baseline as written) refused, not run at 200");
    check(c.cur[0] == 0U && c.cur[1] == 0U, "a refused command leaves a zeroed output");
}

static void test_cw_is_continuous(void)
{
    np_pbm_drive_cmd_t c;
    check(base(0x00, 0x32, 100, 0, &c) == NP_HUB_OK && c.duty == NP_PBM_DUTY_FULL_REG,
          "CW with a 25 % duty field still runs at 100 %");
    check(base(0x00, 0x00, 100, 0, &c) == NP_HUB_OK && c.duty == NP_PBM_DUTY_FULL_REG,
          "CW with duty 0 runs at 100 %");
    check(base(0x28, 0xC8, 100, 0, &c) == NP_HUB_OK && c.duty == NP_PBM_DUTY_MAX_REG,
          "pulsed at 100 % requested is clamped to 25 %");
    check(base(0x0A, 0x20, 100, 0, &c) == NP_HUB_OK && c.duty == 0x20U,
          "pulsed under the ceiling passes through");
}

static void test_conversion(void)
{
    np_pbm_drive_cmd_t c;
    /* T1-A: 483.6 mW/cm² at code 255.  400 → floor(210.9) = 210. */
    check(base(0x28, 0x32, 400, 0, &c) == NP_HUB_OK && c.cur[0] == 210U,
          "T1-A 400 mW/cm² → code 210 (never above the request)");
    check((uint32_t)c.cur[0] * NP_PBM_IRR_FS_BASE_AB_DMW <= 400U * 10U * 255U,
          "code 210 is at or under 400 mW/cm² at design-target flux");
    /* T1-C 660/808: 322.8 at 255.  CW 200 → floor(157.99) = 157. */
    check(smart(0x00, 0, 200, 0, 0, 0x01, &c) == NP_HUB_OK && c.cur[0] == 157U,
          "T1-C CW 200 mW/cm² → code 157");
    /* T1-C 1064: 33.6 at 255.  28 → floor(212.5) = 212. */
    check(smart(0x00, 0, 0, 0, 28, 0x04, &c) == NP_HUB_OK && c.cur[2] == 212U && c.ch_mask == 0x04U,
          "T1-C 1064nm CW 28 mW/cm² → code 212, only CH_C lit");
    check(base(0x28, 0x32, 36, 36, &c) == NP_HUB_OK && c.cur[0] == 18U && c.cur[1] == 18U,
          "36 mW/cm² (Cassano) on T1-A → code 18 on both channels");
}

static void test_unreachable_refused(void)
{
    np_pbm_drive_cmd_t c;
    check(smart(0x28, 0x32, 330, 0, 0, 0x01, &c) == NP_HUB_ERR_INVALID_ARG,
          "330 mW/cm² on a T1-C 660nm channel (reach 322.8) refused");
    check(smart(0x00, 0, 0, 0, 34, 0x04, &c) == NP_HUB_ERR_INVALID_ARG,
          "34 mW/cm² on 1064nm (reach 33.6) refused — no silent under-delivery");
    check(base(0x28, 0x32, 1, 0, &c) == NP_HUB_ERR_INVALID_ARG,
          "1 mW/cm² rounds to code 0 on T1-A — refused, not silently dark");
}

static void test_channel_rules(void)
{
    np_pbm_drive_cmd_t c;
    const uint16_t with_c[3] = { 100, 0, 10 };
    check(np_pbm_irr_resolve(NP_MOD_PBM_BASE, 0x28, 0x32, with_c, 0U, &c) == NP_HUB_ERR_INVALID_ARG,
          "1064nm irradiance on a base tile refused");
    check(smart(0x28, 0x32, 100, 100, 0, 0x01, &c) == NP_HUB_ERR_INVALID_ARG,
          "irradiance on a smart channel its ch_mask does not enable refused");
    check(smart(0x28, 0x32, 100, 0, 0, 0x09, &c) == NP_HUB_ERR_INVALID_ARG,
          "ch_mask bit 3 refused");
    check(smart(0x28, 0x32, 300, 300, 30, 0x07, &c) == NP_HUB_ERR_INVALID_ARG,
          "T1-C 300+300+30 = 630 > R-5 600 refused");
    check(smart(0x28, 0x32, 280, 290, 30, 0x07, &c) == NP_HUB_OK,
          "T1-C 280+290+30 = 600 admitted (R-5 inclusive)");
    check(base(0x28, 0x32, 400, 400, &c) == NP_HUB_OK,
          "R-5 is not applied to a two-channel base tile (OI-HEXTILE-20 open)");
    check(np_pbm_irr_resolve(NP_MOD_NONE, 0x28, 0x32, with_c, 0U, &c) == NP_HUB_ERR_INVALID_ARG,
          "non-PBM module type refused");
    check(np_pbm_irr_resolve(NP_MOD_PBM_BASE, 0x28, 0x32, NULL, 0U, &c) == NP_HUB_ERR_INVALID_ARG,
          "NULL irradiance refused");
}

/* ── Drive layer: per-channel, mode-aware duty clamp ─────────────────────────── */

static void test_drive_duty_by_mode(void)
{
    np_pbm_drv_slot_t drv;
    memset(&drv, 0, sizeof drv);
    drv.freq_code[0] = NP_PBM_FREQ_CODE_CW;
    drv.freq_code[1] = NP_PBM_FREQ_CODE_40HZ;
    drv.freq_code[2] = NP_PBM_FREQ_CODE_CW;

    g_nwr = 0U;
    (void)np_pbm_drive_set_duty(0U, &drv, NP_PBM_CH_ALL_EN, 0xFFU);
    check(drv.duty[0] == NP_PBM_DUTY_FULL_REG, "CW channel may be written to 100 %");
    check(drv.duty[1] == NP_PBM_DUTY_MAX_REG, "pulsed channel clamped to 25 % in the same call");
    check(drv.duty[2] == NP_PBM_DUTY_FULL_REG, "second CW channel 100 %");

    (void)np_pbm_drive_set_duty(0U, &drv, NP_PBM_CH_ALL_EN, 0x10U);
    check(drv.duty[0] == 0x10U && drv.duty[1] == 0x10U, "values under both ceilings pass through");
}

static void test_leaving_cw_reclamps_first(void)
{
    np_pbm_drv_slot_t drv;
    memset(&drv, 0, sizeof drv);
    drv.freq_code[0] = NP_PBM_FREQ_CODE_CW;
    (void)np_pbm_drive_set_duty(0U, &drv, NP_PBM_CH_A_EN, NP_PBM_DUTY_FULL_REG);

    g_nwr = 0U;
    (void)np_pbm_drive_set_freq(0U, &drv, NP_PBM_CH_A_EN, NP_PBM_FREQ_CODE_10HZ);
    int duty_at = index_of(NP_PBM_REG_DUTY_A, NP_PBM_DUTY_MAX_REG);
    int freq_at = index_of(NP_PBM_REG_PWM_FREQ_A, NP_PBM_FREQ_CODE_10HZ);
    check(drv.duty[0] == NP_PBM_DUTY_MAX_REG, "CW → 10 Hz leaves the channel at 25 %");
    check(duty_at >= 0 && freq_at >= 0 && duty_at < freq_at,
          "the duty is lowered BEFORE the frequency changes (never pulsed above 25 %)");

    g_nwr = 0U;
    (void)np_pbm_drive_set_freq(0U, &drv, NP_PBM_CH_A_EN, NP_PBM_FREQ_CODE_CW);
    check(index_of(NP_PBM_REG_DUTY_A, NP_PBM_DUTY_MAX_REG) < 0,
          "entering CW writes no duty (the caller sets it)");
}

int main(void)
{
    test_ceilings_by_mode();
    test_cw_is_continuous();
    test_conversion();
    test_unreachable_refused();
    test_channel_rules();
    test_drive_duty_by_mode();
    test_leaving_cw_reclamps_first();

    printf("\n%s: %d failure(s)\n", g_failures ? "FAILED" : "PASSED", g_failures);
    return g_failures ? 1 : 0;
}
