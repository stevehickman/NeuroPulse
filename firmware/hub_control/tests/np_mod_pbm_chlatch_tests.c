/*
 * NeurOne Hub Control — PBM Per-Channel Disable Latch Host Tests (OI-PBMCH-03)
 * Document: NP-FW-PBM1064-001 §6.5; NP-FEAS-PBMCH-001 §5.3, §9.2
 *
 * §6.5 disables ONE channel on its per-wavelength dose limit and lets the
 * session continue on the rest. A base tile (T1-A) has no CH_ENABLE, so on the
 * socket path that shutdown had no implementation. These tests pin the
 * properties that make the latch mean something:
 *
 *   - on a base tile, a latched channel is written at setpoint 0 and the other
 *     channel keeps its commanded current;
 *   - the latch survives a re-drive and a stop, so a later command cannot
 *     re-light a channel that reached its limit;
 *   - on a smart tile, the latched bit is cleared from CH_ENABLE and masked
 *     out of every later drive;
 *   - only session load clears it.
 *
 * Setpoint 0 on a base tile is NOMINAL zero; whether it is provably zero
 * emission is OI-PBMCH-01's question, not these tests'.
 *
 * OI-FWHUB-10 adds the socket path's pre-drive checks to the same target: a hot
 * or unreadable NTC, or no metering slot, refuses the drive before the tile is
 * written, and a driver fault reaches SHDR as modality + code with no socket.
 *
 * np_mod_pbm.c is linked real; every HAL, driver and logging seam below it is a
 * double. No FreeRTOS, no hardware. IEC 62304 Class B — SW-02 hub control.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "np_hub_types.h"
#include "np_socket_dispatch.h"
#include "np_pbm_config.h"
#include "np_pbm_detect.h"
#include "np_pbm_dose.h"
#include "np_pbm_drive.h"
#include "np_pbm_hal.h"
#include "np_session_log.h"
#include "np_safety_spi.h"
#include "np_sw02_platform_hal.h"

static int g_failures = 0;

static void check(int cond, const char *name)
{
    if (cond) {
        printf("  PASS  %s\n", name);
    } else {
        printf("  FAIL  %s\n", name);
        g_failures++;
    }
}

/* ── Doubles: base-tile PWM seam ───────────────────────────────────────────── */

typedef struct { uint16_t socket; uint8_t cur_a, cur_b, freq, duty; } pwm_write_t;
static pwm_write_t g_pwm_last;
static int         g_pwm_writes;
static bool        g_pwm_fail;

np_hub_status_t np_mod_pbm_hal_socket_pwm_set(uint16_t socket_id, uint8_t cur_a,
                                              uint8_t cur_b, uint8_t freq_code,
                                              uint8_t duty)
{
    g_pwm_last = (pwm_write_t){ socket_id, cur_a, cur_b, freq_code, duty };
    g_pwm_writes++;
    return g_pwm_fail ? NP_HUB_ERR_GENERIC : NP_HUB_OK;
}

/* ── Doubles: smart-tile driver ────────────────────────────────────────────── */

static uint8_t g_startup_mask, g_freq_mask, g_duty_mask, g_ch_enable_written;
static int     g_ch_enable_writes;

np_pbm_status_t np_pbm_drive_startup(uint8_t slot, np_pbm_drv_slot_t *drv,
                                     const np_pbm_preset_t *preset)
{
    (void)slot;
    g_startup_mask   = preset->channel_mask;
    drv->ch_enable   = preset->channel_mask;
    drv->initialized = true;
    return NP_PBM_OK;
}

np_pbm_status_t np_pbm_drive_set_freq(uint8_t slot, np_pbm_drv_slot_t *drv,
                                      uint8_t ch_mask, uint8_t freq_code)
{
    (void)slot; (void)drv; (void)freq_code;
    g_freq_mask = ch_mask;
    return NP_PBM_OK;
}

np_pbm_status_t np_pbm_drive_set_duty(uint8_t slot, np_pbm_drv_slot_t *drv,
                                      uint8_t ch_mask, uint8_t duty)
{
    (void)slot; (void)drv; (void)duty;
    g_duty_mask = ch_mask;
    return NP_PBM_OK;
}

np_pbm_status_t np_pbm_drive_set_ch_enable(uint8_t slot, np_pbm_drv_slot_t *drv,
                                           uint8_t ch_enable_mask)
{
    (void)slot;
    drv->ch_enable      = ch_enable_mask;
    g_ch_enable_written = ch_enable_mask;
    g_ch_enable_writes++;
    return NP_PBM_OK;
}

np_pbm_status_t np_pbm_drive_disable_all(uint8_t slot, np_pbm_drv_slot_t *drv)
{
    (void)slot;
    drv->ch_enable = 0U;
    return NP_PBM_OK;
}

/* ── Doubles: seams only the retired slot path reaches ─────────────────────── */

static int     g_faults;
static uint8_t g_fault_slot, g_fault_type, g_fault_code;
void np_log_shdr_fault(uint8_t slot, np_hub_mod_type_t type,
                       uint8_t fault_code, uint32_t session_ms)
{
    (void)session_ms;
    g_faults++;
    g_fault_slot = slot; g_fault_type = (uint8_t)type; g_fault_code = fault_code;
}
void np_safety_spi_request_disable(uint16_t channel_mask) { (void)channel_mask; }

/* OI-FWHUB-10: the pre-drive NTC check and the metering slot. A cool NTC and
 * a tracker that always has room keep these tests on the latch. */
static float g_ntc = 25.0f;
static bool  g_ntc_fail, g_track_full;
static int   g_tracked, g_untracked, g_refusals, g_drive_faults;
np_hub_status_t np_mod_pbm_hal_socket_ntc_read(uint16_t socket_id, float *temp_c_out)
{
    (void)socket_id;
    if (g_ntc_fail) { return NP_HUB_ERR_GENERIC; }
    *temp_c_out = g_ntc;
    return NP_HUB_OK;
}
np_hub_status_t np_pst_track(uint16_t socket_id, np_hub_mod_type_t mod_type)
{
    (void)socket_id; (void)mod_type;
    if (g_track_full) { return NP_HUB_ERR_GENERIC; }
    g_tracked++;
    return NP_HUB_OK;
}
void np_pst_untrack_driving(uint16_t socket_id) { (void)socket_id; g_untracked++; }
void np_pst_note_predrive_refusal(void) { g_refusals++; }
void np_pst_note_drive_fault(void) { g_drive_faults++; }
np_hub_status_t np_mod_pbm_hal_pwm_set(uint8_t slot, uint8_t cur_a, uint8_t cur_b,
                                       uint8_t freq_code, uint8_t duty)
{
    (void)slot; (void)cur_a; (void)cur_b; (void)freq_code; (void)duty;
    return NP_HUB_OK;
}
float    np_mod_pbm_hal_ntc_read(uint8_t slot) { (void)slot; return 25.0f; }
uint16_t np_mod_pbm_hal_pd_read(uint8_t slot, uint8_t wl_idx) { (void)slot; (void)wl_idx; return 0U; }
bool np_pbm_hal_adc_read_zone_id(uint8_t slot, uint16_t *counts_out)
{
    (void)slot; (void)counts_out;
    return false;
}
np_slot_type_t np_pbm_classify_adc(uint16_t counts) { (void)counts; return (np_slot_type_t)0; }
np_cal_source_t np_pbm_dose_load_cal(const np_pbm_module_uid_t *uid,
                                     np_pbm_cal_t cal_out[NP_PBM_WL_COUNT])
{
    (void)uid; (void)cal_out;
    return NP_CAL_DEFAULT;
}
np_pbm_status_t np_pbm_dose_tick(uint8_t socket_id, const np_pbm_cal_t cal[NP_PBM_WL_COUNT],
                                 np_pbm_dose_state_t *dose)
{
    (void)socket_id; (void)cal; (void)dose;
    return NP_PBM_OK;
}

/* ── Fixtures ──────────────────────────────────────────────────────────────── */

static const np_mod_pbm_base_params_t BASE = {
    .freq_code = 0x11U, .duty = 0x20U, .cur_a = 190U, .cur_b = 200U,
};

static np_mod_pbm_smart_params_t smart_params(uint8_t ch_mask)
{
    return (np_mod_pbm_smart_params_t){
        .freq_code = 0x11U, .duty = 0x20U,
        .cur_a = 190U, .cur_b = 200U, .cur_c = 210U, .ch_mask = ch_mask,
    };
}

static void setup(void)
{
    np_mod_pbm_socket_clear_channel_latches();
    for (uint16_t s = 0U; s < NP_HEXMAP_MAX_SOCKETS; s++) {
        (void)np_mod_pbm_socket_stop(s);
    }
    memset(&g_pwm_last, 0, sizeof g_pwm_last);
    g_pwm_writes = 0;
    g_pwm_fail   = false;
    g_startup_mask = g_freq_mask = g_duty_mask = g_ch_enable_written = 0U;
    g_ch_enable_writes = 0;
    g_ntc = 25.0f; g_ntc_fail = false; g_track_full = false;
    g_tracked = g_untracked = g_refusals = g_drive_faults = 0;
    g_faults = 0; g_fault_slot = g_fault_type = g_fault_code = 0U;
}

static np_hub_status_t drive_base(uint16_t s)
{
    return np_mod_pbm_socket_drive(s, NP_MOD_PBM_BASE, &BASE, sizeof BASE);
}

static np_hub_status_t drive_smart(uint16_t s, uint8_t ch_mask)
{
    np_mod_pbm_smart_params_t p = smart_params(ch_mask);
    return np_mod_pbm_socket_drive(s, NP_MOD_PBM_SMART, &p, sizeof p);
}

/* ── Tests ─────────────────────────────────────────────────────────────────── */

static void test_base_disable_zeroes_only_that_channel(void)
{
    printf("\n[base tile: disabling 660 nm leaves 808 nm driven]\n");
    setup();
    check(drive_base(40U) == NP_HUB_OK, "base drive accepted");
    check(g_pwm_last.cur_a == 190U && g_pwm_last.cur_b == 200U,
          "both channels at their commanded currents before the latch");

    int before = g_pwm_writes;
    check(np_mod_pbm_socket_disable_channel(40U, NP_PBM_CH_A_EN) == NP_HUB_OK,
          "disable CH_A returns OK");
    check(g_pwm_writes == before + 1, "the disable is written to the tile immediately");
    check(g_pwm_last.socket == 40U, "written to the socket it names");
    check(g_pwm_last.cur_a == 0U, "660 nm (CH_A) at setpoint 0");
    check(g_pwm_last.cur_b == 200U, "808 nm (CH_B) keeps its commanded current");
    check(g_pwm_last.freq == 0x11U && g_pwm_last.duty == 0x20U,
          "frequency and duty are unchanged");
    check(np_mod_pbm_socket_disabled_channels(40U) == NP_PBM_CH_A_EN,
          "the latch reports CH_A");
}

static void test_base_latch_survives_redrive_and_stop(void)
{
    printf("\n[base tile: the latch survives a re-drive and a stop]\n");
    setup();
    (void)drive_base(7U);
    (void)np_mod_pbm_socket_disable_channel(7U, NP_PBM_CH_B_EN);

    (void)drive_base(7U);
    check(g_pwm_last.cur_a == 190U && g_pwm_last.cur_b == 0U,
          "a re-drive with both currents set does not re-light CH_B");

    (void)np_mod_pbm_socket_stop(7U);
    (void)drive_base(7U);
    check(g_pwm_last.cur_b == 0U, "nor does a drive after a stop");

    check(g_pwm_last.cur_a == 190U, "CH_A is still driven throughout");
}

static void test_base_both_channels_off(void)
{
    printf("\n[base tile: both channels latched]\n");
    setup();
    (void)drive_base(3U);
    (void)np_mod_pbm_socket_disable_channel(3U, NP_PBM_CH_A_EN);
    (void)np_mod_pbm_socket_disable_channel(3U, NP_PBM_CH_B_EN);
    check(g_pwm_last.cur_a == 0U && g_pwm_last.cur_b == 0U, "both setpoints at 0");
}

static void test_base_has_no_ch_c(void)
{
    printf("\n[base tile: CH_C is recorded, nothing is written]\n");
    setup();
    (void)drive_base(9U);
    int before = g_pwm_writes;
    check(np_mod_pbm_socket_disable_channel(9U, NP_PBM_CH_C_EN) == NP_HUB_OK,
          "disabling a channel the tile lacks is satisfied");
    check(g_pwm_writes == before, "and writes nothing");
    check(np_mod_pbm_socket_disabled_channels(9U) == NP_PBM_CH_C_EN,
          "the latch still records it, for a smart tile placed there later");
}

static void test_base_write_failure_still_latches(void)
{
    printf("\n[base tile: a failed write still leaves the channel latched]\n");
    setup();
    (void)drive_base(11U);
    g_pwm_fail = true;
    check(np_mod_pbm_socket_disable_channel(11U, NP_PBM_CH_A_EN) == NP_HUB_ERR_MOD_FAULT,
          "the failure reaches the caller");
    g_pwm_fail = false;
    (void)drive_base(11U);
    check(g_pwm_last.cur_a == 0U, "the next drive writes CH_A at 0");
}

static void test_smart_disable_clears_ch_enable(void)
{
    printf("\n[smart tile: CH_ENABLE loses the bit, later drives are masked]\n");
    setup();
    check(drive_smart(20U, NP_PBM_CH_ALL_EN) == NP_HUB_OK, "smart drive accepted");
    check(np_mod_pbm_socket_disable_channel(20U, NP_PBM_CH_A_EN) == NP_HUB_OK,
          "disable CH_A returns OK");
    check(g_ch_enable_writes == 1, "one CH_ENABLE write");
    check(g_ch_enable_written == (NP_PBM_CH_B_EN | NP_PBM_CH_C_EN),
          "CH_ENABLE keeps CH_B and CH_C");

    (void)drive_smart(20U, NP_PBM_CH_ALL_EN);
    check(g_startup_mask == (NP_PBM_CH_B_EN | NP_PBM_CH_C_EN),
          "a re-drive asking for all channels starts without CH_A");
    check(g_freq_mask == g_startup_mask && g_duty_mask == g_startup_mask,
          "frequency and duty are applied to the masked set only");
}

static void test_idle_socket_latches_only(void)
{
    printf("\n[idle socket: latched, nothing written, applied on first drive]\n");
    setup();
    check(np_mod_pbm_socket_disable_channel(55U, NP_PBM_CH_B_EN) == NP_HUB_OK,
          "latching an idle socket is accepted");
    check(g_pwm_writes == 0 && g_ch_enable_writes == 0, "and drives no hardware");
    (void)drive_base(55U);
    check(g_pwm_last.cur_b == 0U, "the first drive honours it");
}

static void test_clear_is_the_only_release(void)
{
    printf("\n[session load clears every latch]\n");
    setup();
    (void)drive_base(1U);
    (void)np_mod_pbm_socket_disable_channel(1U, NP_PBM_CH_A_EN);
    np_mod_pbm_socket_clear_channel_latches();
    check(np_mod_pbm_socket_disabled_channels(1U) == 0U, "no channel latched");
    (void)drive_base(1U);
    check(g_pwm_last.cur_a == 190U, "CH_A is driven again in the new session");
}

static void test_invalid_args(void)
{
    printf("\n[argument validation]\n");
    setup();
    check(np_mod_pbm_socket_disable_channel(NP_HEXMAP_MAX_SOCKETS, NP_PBM_CH_A_EN)
          == NP_HUB_ERR_INVALID_ARG, "socket out of range refused");
    check(np_mod_pbm_socket_disable_channel(1U, 0U) == NP_HUB_ERR_INVALID_ARG,
          "empty channel set refused");
    check(np_mod_pbm_socket_disable_channel(1U, 0x08U) == NP_HUB_ERR_INVALID_ARG,
          "a bit naming no channel refused");
    check(np_mod_pbm_socket_disabled_channels(1U) == 0U, "and nothing was latched");
    check(np_mod_pbm_socket_disabled_channels(NP_HEXMAP_MAX_SOCKETS) == 0U,
          "out-of-range query reports nothing");
}

/* ── OI-FWHUB-10: the pre-drive checks and the F11 fault ───────────────────── */

static void test_predrive_refusals(void)
{
    printf("\n[pre-drive: a hot or unreadable NTC, or no metering slot, lights nothing]\n");
    setup();
    g_ntc = (float)NP_PBM_THERMAL_CUTOFF_C;
    check(drive_base(12U) == NP_HUB_ERR_MOD_FAULT && g_pwm_writes == 0 &&
          g_refusals == 1 && g_tracked == 0,
          "NTC at the cutoff: refused before any PWM write, counted, not tracked");
    check(g_faults == 0, "the refusal writes no SHDR fault (which socket is UHDR, F8)");

    setup();
    g_ntc_fail = true;
    np_mod_pbm_smart_params_t p = smart_params(NP_PBM_CH_ALL_EN);
    check(np_mod_pbm_socket_drive(12U, NP_MOD_PBM_SMART, &p, sizeof p) == NP_HUB_ERR_MOD_FAULT &&
          g_startup_mask == 0U && g_refusals == 1,
          "unreadable NTC: treated as hot, the smart tile is never started");

    setup();
    g_track_full = true;
    check(drive_base(12U) == NP_HUB_ERR_MOD_FAULT && g_pwm_writes == 0,
          "no metering slot: refused before the tile is lit, so nothing runs unmetered");
}

static void test_drive_fault_is_shdr_without_socket(void)
{
    printf("\n[drive fault: SHDR gets modality + code, never the socket (F11)]\n");
    setup();
    g_pwm_fail = true;
    check(drive_base(77U) == NP_HUB_ERR_MOD_FAULT && g_drive_faults == 1 && g_faults == 1,
          "a failed base drive is counted and logged once");
    check(g_fault_slot == NP_HUB_SLOT_NONE && g_fault_type == NP_MOD_PBM_BASE &&
          g_fault_code == NP_PBM_SHDR_EV_DRIVE_FAULT,
          "the SHDR fault carries NP_HUB_SLOT_NONE, the modality and the drive-fault code");

    setup();
    check(drive_base(77U) == NP_HUB_OK && g_tracked == 1 && g_faults == 0,
          "a good drive is tracked for metering and logs no fault");
    (void)np_mod_pbm_socket_stop(77U);
    check(g_untracked == 1, "a stop tells the meter the socket is dark");
}

int main(void)
{
    printf("=== np_mod_pbm per-channel disable latch (OI-PBMCH-03) ===\n");
    test_base_disable_zeroes_only_that_channel();
    test_base_latch_survives_redrive_and_stop();
    test_base_both_channels_off();
    test_base_has_no_ch_c();
    test_base_write_failure_still_latches();
    test_smart_disable_clears_ch_enable();
    test_idle_socket_latches_only();
    test_clear_is_the_only_release();
    test_invalid_args();
    test_predrive_refusals();
    test_drive_fault_is_shdr_without_socket();
    printf("\n%s — %d failure(s)\n", g_failures ? "FAILED" : "PASSED", g_failures);
    return g_failures ? 1 : 0;
}
