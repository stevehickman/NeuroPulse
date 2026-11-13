/*
 * NeurOne Firmware-in-the-Loop Simulator — the safety MCU's host HAL and its scheduler (NP-SIM-FW-001 §22, OI-SIMFW-09, REQ-SIMFW-14)
 * HOST ONLY; see np_sim_safety.h for the design.
 *
 * Defines every np_hal_* symbol of firmware/safety_mcu/include/np_safety_hal.h (clock, GPIO, ADC, SPI slave, TIM2, R-peak edge,
 * independent watchdog, impedance, OTP, flash) for the safety MCU's own sources, which are linked unmodified.  np_safety_main.c's
 * main() is renamed np_sim_safety_main() by the build (a compile definition on that one file) and runs on a thread; the pacing
 * point it lacks is np_hal_iwdg_refresh(), the one place an iteration ends (np_safety_main.c: "The ONLY IWDG refresh").
 *
 * What each seam answers is what np_safety_hal.h and platform/np_hal_*.c say the device's does, so the MCU's modules see the
 * contract they were written to:
 *   SPI     per-type holding buffers, newest-wins with an overrun count; a transfer of no known length is discarded and counted;
 *           the reply is the MCU's last send_reply, clocked out during the hub's next transfer (np_hal_spi.c).
 *   GPIO    active-LOW, any non-zero state is HIGH = disabled (np_safety_hal.h, open question 6).  Every line starts HIGH: the
 *           external pull-ups hold it so until np_gpio_mgr_init().
 *   OTP     an all-0xFF root key window reads as the all-zero "unprovisioned" sentinel; the CRC word and the tier window verbatim.
 *   flash   NOR rules: erase sets all ones, a program can only clear bits.
 *   IMP     ready 50 ms after the start (the HAL owns the settling, np_hal_impedance.c); an unset channel reads the HAL's
 *           open-circuit value, 999999 ohm.
 * What none of it does: model a peripheral register, a bus timing or the part's own timing.  The clock is virtual.
 */
#include "np_sim_safety.h"
#include "np_sim_safety_priv.h"

#include "np_crypto.h"
#include "np_safety_config.h"
#include "np_safety_hal.h"
#include "np_safety_protocol.h"
#include "monocypher-ed25519.h"

#include <pthread.h>
#include <string.h>
#include <sys/mman.h>

/* The two opaque port identities the force-included np_gpio_mgr_test_ports.h declares: GPIOA and GPIOB resolve to their addresses. */
int np_test_port_a;
int np_test_port_b;

extern int np_sim_safety_main(void);        /* np_safety_main.c's main(), renamed by the build */
extern void np_fault_latch_commit(const np_safety_state_t *state);     /* np_safety_main.c declares it the same way; no header does */

/* ── The part's silicon: OTP and flash, shared with forked children ──────────────────────────────────────────────────── */
typedef struct {
    uint8_t  otp_root[NP_ED25519_PUBKEY_SIZE];
    uint32_t otp_root_crc;
    uint8_t  otp_tier[NP_TIER_RECORD_LEN];
    uint8_t  uid[NP_DEVICE_UID_LEN];
    uint32_t flash[NP_NV_PAGE_COUNT][NP_NV_SLOTS_PER_PAGE][2];
    /* The fault latch's retention across a warm reset (np_fault_latch.c: "explicitly NOT zeroed by startup code on warm reset").
     * The MCU's latch is a file-scope static, so it dies with the process; this is what the model of the part keeps of it when the
     * independent watchdog resets the MCU, and a power-on reset (np_sim_safety_power_on_reset) forgets it. */
    bool     latch_kept;
    uint8_t  latch_status;
    uint8_t  latch_slot;
    uint32_t latch_count;   /* 32-bit like the firmware's fault count (OI-UINT16-01) */
} silicon_t;

static silicon_t *g_si;

static void erase_silicon(void)
{
    memset(g_si->otp_root, 0xFF, sizeof g_si->otp_root);
    g_si->otp_root_crc = 0xFFFFFFFFU;
    memset(g_si->otp_tier, 0xFF, sizeof g_si->otp_tier);
    memset(g_si->uid, 0, sizeof g_si->uid);
    memset(g_si->flash, 0xFF, sizeof g_si->flash);
    g_si->latch_kept = false;
}

void np_sim_safety_silicon(void)
{
    if (g_si != NULL) { return; }
    void *p = mmap(NULL, sizeof(silicon_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) { return; }
    g_si = (silicon_t *)p;
    erase_silicon();
}

void np_sim_safety_silicon_erase(void)
{
    np_sim_safety_silicon();
    if (g_si != NULL) { erase_silicon(); }
}

/* ── Scheduler ───────────────────────────────────────────────────────────────────────────────────────────────────────── */
static pthread_t       g_thread;
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_cv = PTHREAD_COND_INITIALIZER;
static bool            g_attached;
static bool            g_running;           /* the MCU thread holds the baton */
static bool            g_credit;            /* one iteration granted */

static uint32_t g_iterations;
static bool     g_iwdg_started;
static uint32_t g_iwdg_last_ms;
static bool     g_held_in_reset;
static uint32_t g_iwdg_resets;
static bool     g_stall;
static bool     g_systick_frozen;
static uint32_t g_systick_at;

/* Fault counters. */
static uint32_t g_nv_erase_fail, g_nv_prog_fail, g_spi_timeout_n, g_spi_flip_n, g_spi_flip_byte, g_cmd_flip_n, g_cmd_flip_byte, g_nv_prog_skip, g_nv_programs;

/* ── Inputs of the part's world ──────────────────────────────────────────────────────────────────────────────────────── */
#define SIM_IMP_OPEN_OHM  999999UL      /* np_hal_impedance.c's NP_HAL_IMP_FAILSAFE_OHM: an open contact */
static uint16_t g_adc[NP_SIM_SAFETY_NTC] = { 4095U, 4095U, 4095U, 4095U, 4095U, 4095U };
static uint32_t g_imp_ohm[NP_SIM_SAFETY_IMP] = { SIM_IMP_OPEN_OHM, SIM_IMP_OPEN_OHM, SIM_IMP_OPEN_OHM, SIM_IMP_OPEN_OHM };
static uint32_t g_cvns_el_ohm[2] = { SIM_IMP_OPEN_OHM, SIM_IMP_OPEN_OHM };
static uint32_t g_imp_start_ms[NP_SIM_SAFETY_IMP];
static bool     g_imp_started[NP_SIM_SAFETY_IMP];
static bool     g_rpeak_pending;
static uint32_t g_rpeak_capture_us;

/* ── Pins ────────────────────────────────────────────────────────────────────────────────────────────────────────────── */
typedef struct { void *port; uint16_t pin; const char *name; } line_t;
static const line_t LINES[NP_SIM_SAFETY_LINES] = {
    { NP_EN_PBM_CRANIAL_PORT, NP_EN_PBM_CRANIAL_PIN, "pbm_cranial" },
    { NP_EN_BES_PORT,         NP_EN_BES_PIN,         "bes" },
    { NP_EN_TDCS_PORT,        NP_EN_TDCS_PIN,        "tdcs" },
    { NP_EN_VNS_PORT,         NP_EN_VNS_PIN,         "vns" },
    { NP_EN_VISUAL_PORT,      NP_EN_VISUAL_PIN,      "visual" },
    { NP_EN_INTRANASAL_PORT,  NP_EN_INTRANASAL_PIN,  "intranasal" },
    { NP_EN_CVNS_PORT,        NP_EN_CVNS_PIN,        "cvns" },
    { NP_EN_TMS_PORT,         NP_EN_TMS_PIN,         "tms" },
    { NP_EN_PBM_1170_PORT,    NP_EN_PBM_1170_PIN,    "pbm_1170" },
    { NP_EN_CLIN_STIM_PORT,   NP_EN_CLIN_STIM_PIN,   "clin_stim" },
};
static bool g_pin_high[NP_SIM_SAFETY_LINES] = { true, true, true, true, true, true, true, true, true, true };

const char *np_sim_safety_line_name(uint8_t line) { return line < NP_SIM_SAFETY_LINES ? LINES[line].name : "?"; }

bool np_sim_safety_line_enabled(uint8_t line)
{
    if (line >= NP_SIM_SAFETY_LINES || g_held_in_reset) { return false; }
    return !g_pin_high[line];
}

/* ── SPI holding buffers ─────────────────────────────────────────────────────────────────────────────────────────────── */
static uint8_t g_b_hb[NP_SAFETY_RX_EXT_FRAME_LEN], g_b_cmd[NP_SAFETY_CMD_FRAME_LEN], g_b_lim[NP_SAFETY_CHAN_LIMIT_FRAME_LEN];
static uint8_t g_b_wave[NP_SAFETY_CHAN_WAVE_FRAME_LEN], g_b_user[NP_SAFETY_USER_FRAME_LEN], g_b_hr[NP_SAFETY_HR_REPORT_FRAME_LEN];
static bool    g_r_hb, g_r_cmd, g_r_lim, g_r_wave, g_r_user, g_r_hr;
static uint8_t g_tx[NP_SAFETY_RX_EXT_FRAME_LEN];
static uint32_t g_overruns, g_discarded;

static void hold(uint8_t *buf, bool *ready, const uint8_t *src, uint16_t n)
{
    if (*ready) { g_overruns++; }                   /* newest-wins */
    memcpy(buf, src, n);
    *ready = true;
}

bool np_sim_safety_spi_exchange(const uint8_t *tx, uint16_t len, uint8_t *rx)
{
    if (g_held_in_reset) { return false; }
    if (g_spi_timeout_n > 0U) { g_spi_timeout_n--; return false; }
    uint8_t frame[NP_SAFETY_CMD_FRAME_LEN];
    if (len > sizeof frame) { g_discarded++; memset(rx, 0, len); return true; }
    memcpy(frame, tx, len);
    if (len == NP_SAFETY_RX_EXT_FRAME_LEN && g_spi_flip_n > 0U) {
        g_spi_flip_n--;
        if (g_spi_flip_byte < len) { frame[g_spi_flip_byte] ^= 0xFFU; }
    }
    if (len == NP_SAFETY_CMD_FRAME_LEN && g_cmd_flip_n > 0U) {
        g_cmd_flip_n--;
        if (g_cmd_flip_byte < len) { frame[g_cmd_flip_byte] ^= 0xFFU; }
    }
    /* The reply clocked out during this transfer is the one staged by an earlier iteration; the hub discards it unless the
     * transfer is a heartbeat. */
    memset(rx, 0, len);
    if (len == NP_SAFETY_RX_EXT_FRAME_LEN) { memcpy(rx, g_tx, len); }

    if      (len == NP_SAFETY_RX_EXT_FRAME_LEN)      { hold(g_b_hb,   &g_r_hb,   frame, len); }
    else if (len == NP_SAFETY_CMD_FRAME_LEN)         { hold(g_b_cmd,  &g_r_cmd,  frame, len); }
    else if (len == NP_SAFETY_CHAN_LIMIT_FRAME_LEN)  { hold(g_b_lim,  &g_r_lim,  frame, len); }
    else if (len == NP_SAFETY_CHAN_WAVE_FRAME_LEN)   { hold(g_b_wave, &g_r_wave, frame, len); }
    else if (len == NP_SAFETY_USER_FRAME_LEN)        { hold(g_b_user, &g_r_user, frame, len); }
    else if (len == NP_SAFETY_HR_REPORT_FRAME_LEN)   { hold(g_b_hr,   &g_r_hr,   frame, len); }
    else                                             { g_discarded++; }
    return true;
}

/* ── np_safety_hal.h: clock and timebase ─────────────────────────────────────────────────────────────────────────────── */
void np_hal_clock_init(void)   { }
void np_hal_systick_init(void) { }

uint32_t np_hal_get_tick_ms(void)
{
    return g_systick_frozen ? g_systick_at : np_sim_now_ms();
}

void     np_hal_tim2_init(void) { }
uint32_t np_hal_tim2_now_us(void) { return np_sim_now_ms() * 1000U; }
uint32_t np_hal_tim2_get_capture(void) { return g_rpeak_capture_us; }
bool     np_hal_rpeak_edge_pending(void) { return g_rpeak_pending; }
void     np_hal_rpeak_edge_clear(void) { g_rpeak_pending = false; }

/* ── GPIO ────────────────────────────────────────────────────────────────────────────────────────────────────────────── */
void np_hal_gpio_init(void) { }

void np_hal_gpio_write_pin(void *port, uint16_t pin, int state)
{
    for (uint8_t i = 0U; i < NP_SIM_SAFETY_LINES; i++) {
        if (LINES[i].port == port && LINES[i].pin == pin) { g_pin_high[i] = (state != 0); return; }
    }
    np_sim_emit("{\"ev\":\"safety_pin_unknown\",\"t\":%u,\"pin\":%u}", (unsigned)np_sim_now_ms(), (unsigned)pin);
}

/* ── ADC ─────────────────────────────────────────────────────────────────────────────────────────────────────────────── */
void     np_hal_adc_init(void) { }
uint16_t np_hal_adc_read_channel(uint8_t channel) { return channel < NP_SIM_SAFETY_NTC ? g_adc[channel] : 0U; }

/* ── SPI slave ───────────────────────────────────────────────────────────────────────────────────────────────────────── */
void np_hal_spi_slave_init(void) { }

bool np_hal_spi_frame_ready(void)       { return g_r_hb; }
bool np_hal_spi_cmd_ready(void)         { return g_r_cmd; }
bool np_hal_spi_chan_limit_ready(void)  { return g_r_lim; }
bool np_hal_spi_chan_wave_ready(void)   { return g_r_wave; }
bool np_hal_spi_user_ready(void)        { return g_r_user; }
bool np_hal_spi_hr_report_ready(void)   { return g_r_hr; }

void np_hal_spi_get_ext_frame(np_safety_rx_ext_frame_t *o)     { memcpy(o, g_b_hb,   sizeof *o); g_r_hb   = false; }
void np_hal_spi_get_cmd(np_safety_sig_cmd_t *o)                { memcpy(o, g_b_cmd,  sizeof *o); g_r_cmd  = false; }
void np_hal_spi_get_chan_limit(np_safety_chan_limit_cmd_t *o)  { memcpy(o, g_b_lim,  sizeof *o); g_r_lim  = false; }
void np_hal_spi_get_chan_wave(np_safety_chan_wave_cmd_t *o)    { memcpy(o, g_b_wave, sizeof *o); g_r_wave = false; }
void np_hal_spi_get_user(np_safety_user_cmd_t *o)              { memcpy(o, g_b_user, sizeof *o); g_r_user = false; }
void np_hal_spi_get_hr_report(np_safety_hr_report_cmd_t *o)    { memcpy(o, g_b_hr,   sizeof *o); g_r_hr   = false; }

void np_hal_spi_send_reply(const uint8_t *buf, uint8_t len)
{
    if (buf == NULL) { return; }
    const uint8_t n = (len > NP_SAFETY_RX_EXT_FRAME_LEN) ? (uint8_t)NP_SAFETY_RX_EXT_FRAME_LEN : len;
    memcpy(g_tx, buf, n);
    memset(&g_tx[n], 0, sizeof g_tx - n);
}

void np_hal_spi_send_frame(const np_safety_tx_frame_t *tx)
{
    uint8_t staging[NP_SAFETY_RX_EXT_FRAME_LEN];
    memset(staging, 0, sizeof staging);
    if (tx != NULL) { memcpy(staging, tx, sizeof *tx); }
    np_hal_spi_send_reply(staging, (uint8_t)sizeof staging);
}

/* ── Independent watchdog: the pacing point ──────────────────────────────────────────────────────────────────────────── */
void np_hal_iwdg_start(void)
{
    g_iwdg_started = true;
    g_iwdg_last_ms = np_sim_now_ms();
}

void np_hal_iwdg_refresh(void)
{
    pthread_mutex_lock(&g_mu);
    g_iwdg_last_ms = np_sim_now_ms();
    g_iterations++;
    g_running = false;                              /* the iteration is over: hand the baton back */
    pthread_cond_broadcast(&g_cv);
    while (!g_credit) { pthread_cond_wait(&g_cv, &g_mu); }
    g_credit = false;
    g_running = true;
    pthread_mutex_unlock(&g_mu);
}

/* ── Impedance ───────────────────────────────────────────────────────────────────────────────────────────────────────── */
void np_hal_impedance_start_test(uint8_t channel)
{
    if (channel >= NP_SIM_SAFETY_IMP) { return; }
    g_imp_started[channel] = true;
    g_imp_start_ms[channel] = np_sim_now_ms();
}

bool np_hal_impedance_result_ready(uint8_t channel)
{
    if (channel >= NP_SIM_SAFETY_IMP || !g_imp_started[channel]) { return false; }
    return (np_sim_now_ms() - g_imp_start_ms[channel]) >= (uint32_t)NP_IMPEDANCE_TEST_MS;
}

uint32_t np_hal_impedance_read_ohm(uint8_t channel)
{
    return channel < NP_SIM_SAFETY_IMP ? g_imp_ohm[channel] : SIM_IMP_OPEN_OHM;
}

uint32_t np_hal_impedance_read_cvns_electrode_ohm(uint8_t electrode)
{
    return electrode < 2U ? g_cvns_el_ohm[electrode] : SIM_IMP_OPEN_OHM;
}

/* ── OTP ─────────────────────────────────────────────────────────────────────────────────────────────────────────────── */
void np_hal_otp_read_pubkey(uint8_t *buf, uint8_t len)
{
    if (buf == NULL) { return; }
    if (len > NP_ED25519_PUBKEY_SIZE) { len = (uint8_t)NP_ED25519_PUBKEY_SIZE; }
    bool erased = true;
    if (g_si != NULL) {
        for (uint8_t i = 0U; i < NP_ED25519_PUBKEY_SIZE; i++) { if (g_si->otp_root[i] != 0xFFU) { erased = false; break; } }
    }
    if (g_si == NULL || erased) { memset(buf, 0, len); return; }       /* erased OTP reads as the unprovisioned sentinel */
    memcpy(buf, g_si->otp_root, len);
}

uint32_t np_hal_otp_read_pubkey_crc(void) { return g_si != NULL ? g_si->otp_root_crc : 0xFFFFFFFFU; }

void np_hal_otp_read_tier_record(uint8_t *buf, uint8_t len)
{
    if (buf == NULL) { return; }
    if (len > NP_TIER_RECORD_LEN) { len = (uint8_t)NP_TIER_RECORD_LEN; }
    if (g_si == NULL) { memset(buf, 0xFF, len); return; }
    memcpy(buf, g_si->otp_tier, len);
}

void np_hal_read_device_uid(uint8_t *buf, uint8_t len)
{
    if (buf == NULL) { return; }
    if (len > NP_DEVICE_UID_LEN) { len = (uint8_t)NP_DEVICE_UID_LEN; }
    if (g_si == NULL) { memset(buf, 0, len); return; }
    memcpy(buf, g_si->uid, len);
}

/* ── Flash ───────────────────────────────────────────────────────────────────────────────────────────────────────────── */
void np_hal_nv_read_dword(uint8_t page, uint16_t slot, uint32_t *lo, uint32_t *hi)
{
    if (g_si == NULL || page >= NP_NV_PAGE_COUNT || slot >= NP_NV_SLOTS_PER_PAGE) { *lo = 0xFFFFFFFFU; *hi = 0xFFFFFFFFU; return; }
    *lo = g_si->flash[page][slot][0];
    *hi = g_si->flash[page][slot][1];
}

bool np_hal_nv_erase_page(uint8_t page)
{
    if (g_si == NULL || page >= NP_NV_PAGE_COUNT) { return false; }
    if (g_nv_erase_fail > 0U) { g_nv_erase_fail--; return false; }
    memset(g_si->flash[page], 0xFF, sizeof g_si->flash[page]);
    return true;
}

bool np_hal_nv_program_dword(uint8_t page, uint16_t slot, uint32_t lo, uint32_t hi)
{
    if (g_si == NULL || page >= NP_NV_PAGE_COUNT || slot >= NP_NV_SLOTS_PER_PAGE) { return false; }
    if (g_nv_prog_skip > 0U) { g_nv_prog_skip--; }                       /* the first `arg` programs after the injection succeed */
    else if (g_nv_prog_fail > 0U) { g_nv_prog_fail--; return false; }
    g_nv_programs++;
    g_si->flash[page][slot][0] &= lo;               /* programming can only clear bits */
    g_si->flash[page][slot][1] &= hi;
    return true;
}

/* ── Scheduler: attach, tick ─────────────────────────────────────────────────────────────────────────────────────────── */
static void *mcu_thread(void *arg)
{
    (void)arg;
    (void)np_sim_safety_main();
    return NULL;
}

static void step_one(void)
{
    pthread_mutex_lock(&g_mu);
    g_credit = true;
    pthread_cond_broadcast(&g_cv);
    while (g_credit || g_running) { pthread_cond_wait(&g_cv, &g_mu); }
    pthread_mutex_unlock(&g_mu);
}

/* The independent watchdog's reset is a warm reset: the part keeps the fault latch (np_fault_latch.c) and clears everything else.
 * What it keeps is read through the firmware's own report, which is the one place the latch's reportable fields are marshalled
 * (the tick is device-internal and is not kept). */
static void keep_latch(void)
{
    np_fault_latch_report_t rep;
    np_fault_latch_build_report(&rep);
    if (g_si == NULL) { return; }
    g_si->latch_status = rep.status;
    g_si->latch_slot = rep.slot;
    g_si->latch_count = rep.count;
    g_si->latch_kept = (rep.count > 0U);
}

/* The next MCU in this silicon starts from the latch the last one left: the latch is written back through the firmware's own
 * commit, so its magic, status, slot and count are what the part's retained memory would hold.  Consecutive commits alternate
 * their status, so each is a transition and the count lands on the kept value. */
static void restore_latch(void)
{
    if (g_si == NULL || !g_si->latch_kept) { return; }
    np_safety_state_t s;
    memset(&s, 0, sizeof s);
    for (uint32_t k = 1U; k <= g_si->latch_count; k++) {
        if (k == g_si->latch_count) { s.status = g_si->latch_status; s.fault_slot = g_si->latch_slot; }
        else                        { s.status = (uint8_t)(g_si->latch_status ^ ((k & 1U) != 0U ? 0x80U : 0x40U)); s.fault_slot = 0U; }
        np_fault_latch_commit(&s);
    }
}

void np_sim_safety_power_on_reset(void)
{
    np_sim_safety_silicon();
    g_si->latch_kept = false;
}

static void enter_reset(void)
{
    keep_latch();
    g_held_in_reset = true;
    g_iwdg_resets++;
    for (uint8_t i = 0U; i < NP_SIM_SAFETY_LINES; i++) { g_pin_high[i] = true; }   /* the reset path holds every enable disabled */
    np_sim_emit("{\"ev\":\"safety_iwdg_reset\",\"t\":%u,\"resets\":%u}", (unsigned)np_sim_now_ms(), (unsigned)g_iwdg_resets);
}

static void tick(void)
{
    if (!g_attached || g_held_in_reset) { return; }
    if (g_stall) {
        if (g_iwdg_started && (np_sim_now_ms() - g_iwdg_last_ms) >= (uint32_t)NP_SAFETY_IWDG_TIMEOUT_MS) { enter_reset(); }
        return;
    }
    step_one();
}

bool np_sim_safety_attached(void) { return g_attached; }

bool np_sim_safety_attach(void)
{
    if (g_attached) { return false; }
    np_sim_safety_silicon();
    restore_latch();
    np_sim_advance_ms(0U);                          /* the clock is virtual from here */
    g_running = true;
    g_credit = false;
    if (pthread_create(&g_thread, NULL, mcu_thread, NULL) != 0) { return false; }
    pthread_detach(g_thread);
    pthread_mutex_lock(&g_mu);
    while (g_running) { pthread_cond_wait(&g_cv, &g_mu); }  /* init and the first iteration */
    pthread_mutex_unlock(&g_mu);
    g_attached = true;
    np_sim_set_tick_hook(tick);
    return true;
}

const char *np_sim_safety_label(void) { return g_attached ? "in_loop" : "not_in_loop"; }

/* ── Scenario-side inputs ────────────────────────────────────────────────────────────────────────────────────────────── */
void np_sim_safety_set_adc(uint8_t channel, uint16_t counts) { if (channel < NP_SIM_SAFETY_NTC) { g_adc[channel] = counts; } }
void np_sim_safety_set_impedance_ohm(uint8_t channel, uint32_t ohm) { if (channel < NP_SIM_SAFETY_IMP) { g_imp_ohm[channel] = ohm; } }
void np_sim_safety_set_cvns_electrode_ohm(uint8_t e, uint32_t ohm) { if (e < 2U) { g_cvns_el_ohm[e] = ohm; } }

void np_sim_safety_rpeak(void)
{
    g_rpeak_capture_us = np_hal_tim2_now_us();
    g_rpeak_pending = true;
}

void np_sim_safety_inject(np_sim_safety_fault_t what, uint32_t n, uint32_t arg)
{
    switch (what) {
    case NP_SIM_SAFETY_FAULT_NV_ERASE:        g_nv_erase_fail = n; break;
    case NP_SIM_SAFETY_FAULT_NV_PROGRAM:      g_nv_prog_fail = n; g_nv_prog_skip = arg; break;
    case NP_SIM_SAFETY_FAULT_SPI_TIMEOUT:     g_spi_timeout_n = n; break;
    case NP_SIM_SAFETY_FAULT_SPI_FLIP:        g_spi_flip_n = n; g_spi_flip_byte = arg; break;
    case NP_SIM_SAFETY_FAULT_CMD_FLIP:        g_cmd_flip_n = n; g_cmd_flip_byte = arg; break;
    case NP_SIM_SAFETY_FAULT_STALL:           g_stall = (n != 0U); break;
    case NP_SIM_SAFETY_FAULT_SYSTICK_FREEZE:
        g_systick_frozen = (n != 0U);
        g_systick_at = np_sim_now_ms();
        break;
    }
}

void np_sim_safety_info(np_sim_safety_info_t *o)
{
    memset(o, 0, sizeof *o);
    o->attached = g_attached;
    o->held_in_reset = g_held_in_reset;
    o->iwdg_resets = g_iwdg_resets;
    o->iterations = g_iterations;
    o->spi_overruns = g_overruns;
    o->spi_discarded = g_discarded;
    for (uint8_t i = 0U; i < NP_SIM_SAFETY_LINES; i++) { if (np_sim_safety_line_enabled(i)) { o->lines_enabled_mask |= (uint16_t)(1U << i); } }
    np_safety_tx_frame_t f;
    memcpy(&f, g_tx, sizeof f);
    o->last_reply_status = f.status;
    o->last_reply_granted = (uint16_t)((uint16_t)f.granted_lo | ((uint16_t)f.granted_hi << 8));
    o->last_reply_fault_slot = f.fault_slot;
    np_fault_latch_report_t rep;
    np_fault_latch_build_report(&rep);
    o->nv_programs = g_nv_programs;
    o->latch_status = rep.status;
    o->latch_slot = rep.slot;
    o->latch_count = rep.count;
}

/* ── Manufacture: OTP programming and the app's signature ───────────────────────────────────────────────────────────── */
static uint8_t g_root_sk[64];
static bool    g_root_known;

void np_sim_safety_otp_program_root(const uint8_t seed[32], bool write_crc)
{
    np_sim_safety_silicon();
    uint8_t s[32], pk[32];
    memcpy(s, seed, 32);
    crypto_ed25519_key_pair(g_root_sk, pk, s);      /* wipes s */
    g_root_known = true;
    memcpy(g_si->otp_root, pk, 32);
    g_si->otp_root_crc = write_crc ? np_crc32(pk, 32U) : 0xFFFFFFFFU;
}

void np_sim_safety_otp_corrupt_root_bit(void)
{
    np_sim_safety_silicon();
    g_si->otp_root[0] ^= 0x01U;                     /* a bit that no longer matches the CRC programmed beside it */
}

void np_sim_safety_set_uid(const uint8_t uid[12])
{
    np_sim_safety_silicon();
    memcpy(g_si->uid, uid, NP_DEVICE_UID_LEN);
}

void np_sim_safety_otp_program_tier(uint8_t tier, uint8_t hw_version)
{
    np_sim_safety_silicon();
    uint8_t seed[32], sk[64], pk[32], msg[NP_TIER_SIG_MSG_LEN];
    for (uint8_t i = 0U; i < 32U; i++) { seed[i] = (uint8_t)(0xA0U + i); }      /* the TEST tier authority np_tier_test_key.h names */
    crypto_ed25519_key_pair(sk, pk, seed);
    uint8_t *rec = g_si->otp_tier;
    memset(rec, 0, NP_TIER_RECORD_LEN);
    rec[0] = NP_TIER_RECORD_MAGIC_0; rec[1] = NP_TIER_RECORD_MAGIC_1; rec[2] = NP_TIER_RECORD_MAGIC_2; rec[3] = NP_TIER_RECORD_MAGIC_3;
    rec[4] = NP_TIER_RECORD_VERSION;
    rec[5] = (uint8_t)(((hw_version & 0x0FU) << NP_TIER_BYTE_HWVER_SHIFT) | (tier & NP_TIER_BYTE_TIER_MASK));
    memcpy(msg, NP_TIER_SIG_DOMAIN, NP_TIER_SIG_DOMAIN_LEN);
    memcpy(&msg[NP_TIER_SIG_DOMAIN_LEN], rec, NP_TIER_RECORD_BODY_LEN);
    memcpy(&msg[NP_TIER_SIG_DOMAIN_LEN + NP_TIER_RECORD_BODY_LEN], g_si->uid, NP_DEVICE_UID_LEN);
    crypto_ed25519_sign(&rec[NP_TIER_RECORD_BODY_LEN], sk, msg, sizeof msg);
}

bool np_sim_safety_sign_session(const uint8_t hash[32], uint8_t sig[64])
{
    if (!g_root_known) { return false; }
    crypto_ed25519_sign(sig, g_root_sk, hash, 32U);
    return true;
}
