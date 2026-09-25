/*
 * NeurOne Safety MCU — Main Entry Point
 * Target: STM32G071 (Cortex-M0+, 64 MHz), bare metal
 * Document: NP-SW-001 Rev 1 — SW-01 Class C
 *
 * Execution flow:
 *   Reset → startup (copy .data, zero .bss, skip fault_latch)
 *         → main() (below)
 *         → peripheral init
 *         → fault latch check (prior fault persists until cleared by app)
 *         → infinite main loop: poll SPI + tick all safety modules
 *
 * All stimulation GPIO is driven HIGH (disabled) at startup.
 * Active-LOW open-drain: LOW = stimulation enabled; HIGH = disabled.
 * np_hal_gpio_init() + np_gpio_mgr_init() set all enables HIGH immediately.
 * Stimulation can only be enabled after:
 *   1. Valid heartbeat received with magic + checksum pass, and its sequence
 *      counter on a forward run (np_spi_watchdog_seq_accept, OI-FMEA-12 (a))
 *   2. Watchdog has not timed out (np_spi_watchdog_check passes)
 *   3. All active interlock checks pass for requested channels
 *   4. Session descriptor signature verified (if session_active bit set)
 *   5. For a T2 line, the unit's signed tier identity is T2 (SW01-M10)
 */

#include "np_nv_state.h"
#include "np_safety_config.h"
#include "np_safety_hal.h"
#include "np_safety_protocol.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── Forward declarations for module init ────────────────────────────────── */
extern np_safe_status_t np_gpio_mgr_init(void);
extern np_safe_status_t np_spi_watchdog_init(void);
extern np_safe_status_t np_charge_monitor_init(void);
extern np_safe_status_t np_thermal_interlock_init(void);
extern np_safe_status_t np_cardiac_interlock_init(void);
extern np_safe_status_t np_impedance_check_init(void);
extern np_safe_status_t np_session_sig_init(void);
extern np_safe_status_t np_fault_latch_init(bool *prior_fault_out);
extern np_safe_status_t np_tier_identity_init(void);

/* ── Forward declarations for module tick / action functions ─────────────── */
extern void np_spi_watchdog_tick(np_safety_state_t             *state,
                                 const np_safety_rx_ext_frame_t *rx,
                                 np_safety_tx_frame_t           *tx);
extern void np_spi_watchdog_check(np_safety_state_t *state);          /* MUST call every loop */
extern bool np_spi_watchdog_seq_accept(uint8_t seq);
extern void np_spi_watchdog_tick_liveness(np_safety_state_t *state);   /* MUST call every loop */
extern void np_charge_monitor_accumulate(uint8_t channel, uint32_t current_ua, uint32_t dt_us);
extern void np_charge_monitor_tick(np_safety_state_t *state);
extern void np_charge_monitor_reset_session(np_safety_state_t *state);
extern void np_charge_monitor_set_channel_area_mcm2(uint8_t channel, uint16_t area_mcm2);
extern void np_charge_monitor_set_channel_waveform(uint8_t channel, uint8_t wave_class,
                                                   uint32_t phase_us);
extern void np_charge_monitor_geom_gate(np_safety_state_t *state);
extern void np_charge_monitor_decl_gate(np_safety_state_t *state);
extern bool np_charge_monitor_is_dc(uint8_t channel);
extern void np_charge_monitor_phase_tick(np_safety_state_t *state,
                                         const uint16_t    *current_ua,
                                         uint8_t            channel_count);
extern void np_thermal_interlock_tick(np_safety_state_t *state);
extern void np_cardiac_interlock_tick(np_safety_state_t *state);
extern void np_cardiac_interlock_reenable(np_safety_state_t *state);
extern void np_cardiac_interlock_restore(bool cutoff_pending);
extern void np_cardiac_interlock_arm_reset(void);
extern bool np_cardiac_interlock_nv_request(bool *pending_out);
extern void np_cardiac_interlock_nv_done(bool written);
extern void np_cardiac_interlock_user_changed(np_safety_state_t *state, bool new_user_blocked);
extern void np_impedance_check_request(uint16_t requested_mask);
extern void np_impedance_check_poll(np_safety_state_t *state);
extern void np_impedance_check_reset_session(void);
extern void np_impedance_check_gate(np_safety_state_t *state);
extern bool np_impedance_check_build_cvns_report(np_safety_imp_report_t *out);
extern void np_session_sig_reset(np_safety_state_t *state);
extern void np_session_sig_reenable(np_safety_state_t *state);
extern np_safe_status_t np_session_sig_verify(np_safety_state_t *state,
                                               const uint8_t *hash,
                                               const uint8_t *sig);
extern void np_gpio_mgr_apply(const np_safety_state_t *state);
extern void np_fault_latch_commit(const np_safety_state_t *state);
extern void np_tier_identity_gate(np_safety_state_t *state);
extern void np_tier_identity_build_report(np_safety_tier_report_t *out,
                                          uint16_t                 requested_mask);

/* ── HAL ──────────────────────────────────────────────────────────────────
 * Platform symbols come from np_safety_hal.h (included above) — the single
 * declaration point for all 25.  No implementation exists: Defect E,
 * NP-SW-CI-001 §4.4, OI-SWCI-17.  Declare nothing locally.                 */

/* ── Shared safety state ─────────────────────────────────────────────────── */
static np_safety_state_t s_state;
static bool              s_prev_session_active = false;
static bool              s_prev_cvns_active    = false;   /* CVNS request edge → arm reset */
static uint8_t           s_bad_cmd_count = 0U; /* consecutive bad-magic/checksum sig frames */
/* s_prior_latch_reported: true once prior fault is reported to hub on the
 * first heartbeat reply.  Prevents np_fault_latch_commit() from overwriting
 * the preserved latch data (slot + specific status bits) with the generic
 * NP_SAFETY_STATUS_FAULT=0x01 that init places in s_state.status. */
static bool              s_prior_latch_reported = false;

/* Consecutive failed attempts to persist the cardiac-cutoff state
 * (NP-SW-FAULTMSG-001 P1).  After NP_NV_WRITE_ATTEMPTS the safety MCU stops
 * retrying and raises NP_FAULT_SLOT_NVSTATE: each attempt can stall the core
 * for up to one page erase, so retries are bounded, and a flash that cannot
 * record a cutoff is a device fault to surface, not to hide. */
static uint8_t           s_nv_fail_count = 0U;

/* ── Checksum verification (matches hub_control np_safety_spi.c) ──────────── */

/* Base checksum: additive sum of bytes [0..5] (magic through channel_count).
 * Covers the same field range as the original 8-byte heartbeat checksum.    */
static bool frame_checksum_ok(const np_safety_rx_ext_frame_t *f)
{
    uint16_t sum = 0U;
    const uint8_t *b = (const uint8_t *)f;
    uint8_t i;
    for (i = 0U; i < 6U; i++) {
        sum += b[i];
    }
    return sum == f->checksum;
}

/* Ext checksum: additive sum of bytes [8..35] (current_ua[14] only).
 * Detects corruption of the current magnitude array independent of the base. */
static bool ext_checksum_ok(const np_safety_rx_ext_frame_t *f)
{
    uint16_t sum = 0U;
    const uint8_t *b = (const uint8_t *)f;
    uint8_t i;
    for (i = 8U; i < 36U; i++) {
        sum += b[i];
    }
    return sum == f->ext_checksum;
}

static void build_tx_checksum(np_safety_tx_frame_t *f)
{
    uint16_t sum = 0U;
    const uint8_t *b = (const uint8_t *)f;
    for (uint8_t i = 0U; i < 6U; i++) {
        sum += b[i];
    }
    f->checksum = sum;
}

/* Verify checksum of a 102-byte session-sig command frame.
 * Checksum covers bytes [0..NP_SAFETY_CMD_FRAME_LEN-3] (all except the 2
 * checksum bytes at the end).                                              */
static bool cmd_checksum_ok(const np_safety_sig_cmd_t *c)
{
    uint16_t sum = 0U;
    const uint8_t *b = (const uint8_t *)c;
    uint8_t i;
    for (i = 0U; i < (NP_SAFETY_CMD_FRAME_LEN - 2U); i++) {
        sum += b[i];
    }
    return sum == c->checksum;
}

/* Verify checksum of a 34-byte per-channel charge-limit command frame.
 * Checksum covers bytes [0..NP_SAFETY_CHAN_LIMIT_FRAME_LEN-3] (all except the
 * 2 checksum bytes at the end).                                            */
static bool chan_limit_checksum_ok(const np_safety_chan_limit_cmd_t *c)
{
    uint16_t sum = 0U;
    const uint8_t *b = (const uint8_t *)c;
    uint8_t i;
    for (i = 0U; i < (NP_SAFETY_CHAN_LIMIT_FRAME_LEN - 2U); i++) {
        sum += b[i];
    }
    return sum == c->checksum;
}

/* Verify checksum of a 76-byte per-channel waveform-class command frame.
 * Checksum covers bytes [0..NP_SAFETY_CHAN_WAVE_FRAME_LEN-3] (all except the
 * 2 checksum bytes at the end).                                            */
static bool user_cmd_checksum_ok(const np_safety_user_cmd_t *c)
{
    const uint8_t *b = (const uint8_t *)c;
    uint16_t sum = 0U;
    uint8_t  i;
    for (i = 0U; i < (uint8_t)(NP_SAFETY_USER_FRAME_LEN - 2U); i++) {
        sum = (uint16_t)(sum + b[i]);
    }
    return (sum == c->checksum) && (c->reserved == 0U);
}

static bool chan_wave_checksum_ok(const np_safety_chan_wave_cmd_t *c)
{
    uint16_t sum = 0U;
    const uint8_t *b = (const uint8_t *)c;
    uint8_t i;
    for (i = 0U; i < (NP_SAFETY_CHAN_WAVE_FRAME_LEN - 2U); i++) {
        sum += b[i];
    }
    return sum == c->checksum;
}

/* ── Main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    bool prior_fault = false;

    /* Peripheral init — must complete before any module init */
    np_hal_clock_init();
    np_hal_systick_init();
    np_hal_gpio_init();    /* drives all stimulation enables HIGH (disabled) immediately */
    np_hal_spi_slave_init();
    np_hal_adc_init();
    np_hal_tim2_init();

    /* Module init — order matters: GPIO first, then watchdog, then others */
    np_gpio_mgr_init();
    np_fault_latch_init(&prior_fault);
    np_spi_watchdog_init();
    np_charge_monitor_init();
    np_thermal_interlock_init();
    np_cardiac_interlock_init();
    np_impedance_check_init();
    np_session_sig_init();
    np_tier_identity_init();   /* OTP tier record verified once; T1 unless proven T2 */

    memset(&s_state, 0, sizeof(s_state));
    s_state.fault_slot = 0xFFU;

    if (prior_fault) {
        /* Prior fault survived warm reset — report it on first heartbeat reply.
         * s_prior_latch_reported stays false until after the first TX frame is
         * sent, preventing np_fault_latch_commit() from immediately overwriting
         * the preserved latch data (fault slot + specific status bits) with the
         * generic status=0x01 / slot=0xFF that s_state was initialized to. */
        s_state.status         |= NP_SAFETY_STATUS_FAULT;
        s_prior_latch_reported  = false;
    } else {
        s_prior_latch_reported = true;  /* no prior fault — commit is safe immediately */
    }

    /* Cervical VNS cardiac cutoff persisted across a power-on reset
     * (NP-SW-FAULTMSG-001 P1, OI-FAULTMSG-01).  Held latent until a session
     * requests CVNS — see np_cardiac_interlock_restore(). */
    np_nv_state_init();
    np_cardiac_interlock_restore(np_nv_cardiac_blocked(np_nv_current_user()));

    /* Independent watchdog: started HERE, after init and immediately before the
     * loop, and refreshed at exactly one point at the loop's end.  Init grants
     * nothing (s_state is zeroed, every enable preset HIGH), so a hang during
     * init leaves stimulation off; starting earlier would put the boot-time
     * Ed25519 tier-record verify inside an unmeasured timeout (OI-SWCI-49).
     * NP-FMEA-001 FMEA-M02-02/-05, NP-RISK-002 OI-RISK2-05.                  */
    np_hal_iwdg_start();

    /* ── Main polling loop ─────────────────────────────────────────────── */
    for (;;) {
        np_safety_rx_ext_frame_t rx;
        np_safety_tx_frame_t tx;
        memset(&tx, 0, sizeof(tx));
        tx.fault_slot = s_state.fault_slot;

        bool valid_frame = false;   /* well formed: magic + both checksums   */
        bool live_frame  = false;   /* well formed AND sequence-accepted      */
        bool cvns_reenable_confirm = false;

        /* ── Session signature command frame (102 bytes) ─────────────────── */
        /* Hub sends this once per session BEFORE the first heartbeat that
         * requests a non-zero enable_mask.  The HAL buffers it separately
         * from the 38-byte heartbeat (distinguished by NSS transfer length). */
        if (np_hal_spi_cmd_ready()) {
            np_safety_sig_cmd_t cmd;
            np_hal_spi_get_cmd(&cmd);

            if (cmd.cmd_magic[0] == NP_SAFETY_CMD_MAGIC_0 &&
                cmd.cmd_magic[1] == NP_SAFETY_CMD_MAGIC_1 &&
                cmd.cmd_type     == NP_SAFETY_CMD_SESSION_SIG &&
                cmd_checksum_ok(&cmd)) {

                s_bad_cmd_count = 0U;  /* well-formed frame — reset corruption counter */
                np_session_sig_verify(&s_state,
                                      cmd.session_hash,
                                      cmd.session_sig);
                /* Result reflected in NP_SAFETY_STATUS_SIG_PENDING bit:
                 *   success → SIG_PENDING cleared → next heartbeat grants mask
                 *   failure → FAULT + CUTOFF set → hub MUST abort session and
                 *             restart (retry without full session teardown is
                 *             insufficient — FAULT blocks all enables until the
                 *             next session 0→1 transition calls sig_reenable)  */
            } else {
                /* Bad magic or bad checksum: SIG_PENDING stays set.
                 * Hub will see SIG_PENDING in next heartbeat reply and may retry.
                 * After NP_SAFETY_SIG_BAD_CMD_MAX consecutive corrupt frames,
                 * FAULT is set to prevent indefinite unverified-session operation. */
                s_bad_cmd_count++;
                if (s_bad_cmd_count >= NP_SAFETY_SIG_BAD_CMD_MAX) {
                    s_state.fault_slot = NP_FAULT_SLOT_SIG_CORRUPT;
                    s_state.status    |= NP_SAFETY_STATUS_FAULT;
                }
            }
        }

        /* ── Extended heartbeat frame (38 bytes) ─────────────────────────── */
        if (np_hal_spi_frame_ready()) {
            np_hal_spi_get_ext_frame(&rx);

            if (rx.magic[0] == NP_SAFETY_BEAT_MAGIC_0 &&
                rx.magic[1] == NP_SAFETY_BEAT_MAGIC_1 &&
                frame_checksum_ok(&rx) &&
                ext_checksum_ok(&rx)) {

                valid_frame = true;
                /* OI-FMEA-12 (a), FMEA-M02-03: well formed is not new.  A
                 * repeated or replayed frame is not acted on and does not
                 * reset the watchdog.  It still counts toward charge below
                 * (valid_frame), because over-counting is the safe direction. */
                live_frame = np_spi_watchdog_seq_accept(
                    (uint8_t)((rx.session_status & NP_SESSION_STATUS_SEQ_MASK)
                              >> NP_SESSION_STATUS_SEQ_SHIFT));
            }

            if (live_frame) {
                cvns_reenable_confirm =
                    (rx.session_status & NP_SESSION_STATUS_CVNS_REENABLE) != 0U;

                s_state.requested_mask =
                    (uint16_t)rx.enable_lo | ((uint16_t)rx.enable_hi << 8);
                s_state.session_active =
                    (rx.session_status & NP_SESSION_STATUS_ACTIVE) != 0U;
                s_state.cvns_active =
                    (s_state.requested_mask & NP_SAFETY_EN_CVNS) != 0U;
                /* OI-CHARGE-03: the hub sets this bit every heartbeat while a
                 * small-electrode modality (HD-tDCS) is in the session, so a
                 * single lost frame cannot disarm the geometry gate.          */
                s_state.geom_required =
                    (rx.session_status & NP_SESSION_STATUS_GEOM_REQUIRED) != 0U;
                /* OI-CHARGE-04: the same, for the T1 tDCS channel's own gate. */
                s_state.geom_required_tdcs =
                    (rx.session_status & NP_SESSION_STATUS_GEOM_REQ_TDCS) != 0U;
                /* OI-MMSOCK-02: and for the BES/tACS channel's own gate.     */
                s_state.geom_required_bes =
                    (rx.session_status & NP_SESSION_STATUS_GEOM_REQ_BES) != 0U;

                /* Reset watchdog on valid heartbeat */
                np_spi_watchdog_tick(&s_state, &rx, &tx);
            }
            /* Invalid or not-live frame: watchdog continues counting; no
             * enable granted */
        }

        /* Watchdog timeout check — MUST run every iteration regardless of SPI */
        np_spi_watchdog_check(&s_state);

        /* SysTick against TIM2 (OI-FMEA-12 (b), FMEA-M02-02): a frozen SysTick
         * would blind the check above.  Every iteration, after the only grant
         * (the tick above) and before the GPIO write. */
        np_spi_watchdog_tick_liveness(&s_state);

        /* Detect session_active 0→1 transition: reset per-session state */
        if (s_state.session_active && !s_prev_session_active) {
            np_session_sig_reenable(&s_state);   /* clear prior sig fault if recoverable */
            np_session_sig_reset(&s_state);      /* sets NP_SAFETY_STATUS_SIG_PENDING */
            np_charge_monitor_reset_session(&s_state);
            np_impedance_check_reset_session();  /* no pass carries over */
            np_impedance_check_request(s_state.requested_mask);
            s_bad_cmd_count = 0U;               /* reset corruption counter for new session */
        }
        s_prev_session_active = s_state.session_active;

        /* ── Per-channel charge-limit command frame (34 bytes, OI-CHARGE-02) ── */
        /* Hub delivers per-channel electrode AREA when a modality's geometry
         * differs from the default 25cm² pad (T2 HD-tDCS 3.5mm electrodes).
         * Applied AFTER the session-start reset block above so it overrides the
         * restored defaults even if delivered in the same loop iteration as the
         * 0→1 transition.  The 40µC/cm² density constant stays on this MCU:
         * only electrode area is received; set_channel_area_mcm2() derives the
         * limit.                                                              */
        if (np_hal_spi_chan_limit_ready()) {
            np_safety_chan_limit_cmd_t clim;
            np_hal_spi_get_chan_limit(&clim);

            if (clim.cmd_magic[0] == NP_SAFETY_CMD_MAGIC_0 &&
                clim.cmd_magic[1] == NP_SAFETY_CMD_MAGIC_1 &&
                clim.cmd_type     == NP_SAFETY_CMD_CHAN_LIMIT &&
                chan_limit_checksum_ok(&clim)) {

                uint8_t ch;
                for (ch = 0U; ch < NP_SAFETY_MAX_CHANNELS; ch++) {
                    /* area 0 means "keep current limit" — set_channel_area_mcm2
                     * ignores it, leaving the reset default in place.          */
                    np_charge_monitor_set_channel_area_mcm2(ch, clim.area_mcm2[ch]);
                }
            }
            /* Bad magic/type/checksum: silently ignored; the hub retries.
             * The channels that would otherwise keep the permissive 1000µC
             * default are held OFF meanwhile by np_charge_monitor_geom_gate():
             * CLIN_STIM on NP_SESSION_STATUS_GEOM_REQUIRED (OI-CHARGE-03) and
             * TDCS on NP_SESSION_STATUS_GEOM_REQ_TDCS (OI-CHARGE-04), and
             * BES_TACS on NP_SESSION_STATUS_GEOM_REQ_BES (OI-MMSOCK-02).  All
             * three bits ride every heartbeat, so a lost command cannot disarm
             * any gate — the modality simply never starts.                    */
        }

        /* ── Per-channel waveform-class frame (76 bytes, OI-CHARGE-05 (b)) ─── */
        /* Hub declares, for every electrical channel the descriptor will
         * command, whether it is DC (per-session mC/cm² budget) or pulsed/AC
         * (per-phase µC/cm² ceiling), and the phase duration for the latter.
         * Both ceilings stay resident here; only the classification crosses.
         *
         * Applied AFTER the session-start reset block, for the same reason the
         * area frame is: reset_session() clears every declaration, so a
         * declaration arriving in the same loop iteration as the 0→1
         * transition must land after it or it would be wiped.
         *
         * An undeclared electrical channel is held OFF by
         * np_charge_monitor_decl_gate() below — so a lost or corrupt frame
         * costs the modality its session rather than its monitoring.          */
        if (np_hal_spi_chan_wave_ready()) {
            np_safety_chan_wave_cmd_t wcmd;
            np_hal_spi_get_chan_wave(&wcmd);

            if (wcmd.cmd_magic[0] == NP_SAFETY_CMD_MAGIC_0 &&
                wcmd.cmd_magic[1] == NP_SAFETY_CMD_MAGIC_1 &&
                wcmd.cmd_type     == NP_SAFETY_CMD_CHAN_WAVE &&
                chan_wave_checksum_ok(&wcmd)) {

                uint8_t ch;
                for (ch = 0U; ch < NP_SAFETY_MAX_CHANNELS; ch++) {
                    /* class 0 means "not declared in this frame" —
                     * set_channel_waveform() ignores it rather than clearing,
                     * so a partial frame cannot un-declare a channel.         */
                    np_charge_monitor_set_channel_waveform(ch,
                                                           wcmd.wave_class[ch],
                                                           wcmd.phase_us[ch]);
                }
            }
        }

        /* CVNS re-enable after cardiac cutoff:
         * Hub sets NP_SESSION_STATUS_CVNS_REENABLE only when all three
         * conditions are met on the hub side: lockout elapsed + user
         * confirmation + re-established baseline signal.
         * Safety MCU clears CARDIAC status and restarts impedance check.
         *
         * Defense-in-depth (OI-CVNS-HUB-01): additionally require
         * session_active.  A well-behaved hub never sends CVNS_REENABLE with
         * ACTIVE clear, but honoring re-enable only inside an active session
         * ensures a stale/teardown frame can never clear the Class C cardiac
         * latch outside a session. */
        if (live_frame && cvns_reenable_confirm && s_state.session_active &&
            (s_state.status & NP_SAFETY_STATUS_CARDIAC) != 0U) {
            np_cardiac_interlock_reenable(&s_state);
            np_impedance_check_request(NP_SAFETY_EN_CVNS);
        }

        /* Tick safety monitors every main-loop iteration.
         * Order is critical: all interlocks that reduce granted_mask must run
         * BEFORE accumulate + charge_tick, so only charge for granted channels
         * is counted.  charge_tick runs last (may further reduce granted_mask). */
        np_thermal_interlock_tick(&s_state);
        np_impedance_check_poll(&s_state);
        if (s_state.cvns_active) {
            /* A new CVNS request re-arms on fresh beats (OI-RISK2-05). */
            if (!s_prev_cvns_active) {
                np_cardiac_interlock_arm_reset();
            }
            np_cardiac_interlock_tick(&s_state);
        }
        s_prev_cvns_active = s_state.cvns_active;

        /* OI-CHARGE-03 fail-safe geometry gate: block CLIN_STIM while the hub
         * requires an electrode-geometry override that has not yet been
         * applied.  Runs BEFORE the accumulate loop so a gated channel accrues
         * no charge while blocked. */
        np_charge_monitor_geom_gate(&s_state);

        /* OI-CHARGE-05 fail-safe declaration gate: block any ELECTRICAL channel
         * whose waveform class was never declared this session.  Also before
         * the accumulate loop, and for the same reason — an unmonitored channel
         * must not be an energised one.                                       */
        np_charge_monitor_decl_gate(&s_state);

        /* Impedance gate (FMEA-M05-07, OI-FMEA-12 (d)): withhold every
         * impedance-checked channel that has not passed this session, pending
         * or failed.  Before the accumulate loop and the GPIO write, like the
         * two gates above.                                                   */
        np_impedance_check_gate(&s_state);

        /* SW01-M10 tier gate (REQ-UPG-01, OI-UPG-01): withhold every T2 enable
         * line unless this unit's signed tier identity is T2.  After the
         * watchdog tick — the only place a grant is made — and before the
         * accumulate loop and the GPIO write, so a withheld line is never
         * energised and accrues no charge.  Nothing below adds a bit.        */
        np_tier_identity_gate(&s_state);

        /* Accumulate charge for currently-granted DC channels carrying a
         * non-zero commanded current.  dt_us is a compile-time constant —
         * NP_SAFETY_HEARTBEAT_EXP_MS × 1000 = 200000 µs — not transmitted
         * over SPI.  current_ua[] are SHDR (commanded, not ADC-measured).
         *
         * DC ONLY (OI-CHARGE-05 (b)).  BES/tACS, VNS, cervical VNS and clinical
         * tACS are charge-balanced biphasic: net delivered charge is ~zero, so
         * integrating |I| over a session measures nothing physical for them and
         * would trip them in 0.4–1.0 s at their rated currents.  Their ceiling
         * is per-phase and is enforced by np_charge_monitor_phase_tick() below. */
        if (valid_frame && s_state.session_active) {
            uint8_t ch;
            for (ch = 0U;
                 ch < rx.channel_count && ch < NP_SAFETY_MAX_CHANNELS;
                 ch++) {
                if (rx.current_ua[ch] > 0U &&
                    np_charge_monitor_is_dc(ch) &&
                    (s_state.granted_mask & (uint16_t)(1U << ch)) != 0U) {
                    np_charge_monitor_accumulate(
                        ch,
                        (uint32_t)rx.current_ua[ch],
                        (uint32_t)NP_SAFETY_HEARTBEAT_EXP_MS * 1000UL);
                }
            }

            /* Per-phase ceiling for the pulsed/AC channels.  A predicate on
             * THIS beat's commanded amplitude, not an integral, so it follows
             * a ramp exactly.  May cut channels.
             *
             * Copied into an aligned local first: rx is __attribute__((packed)),
             * so current_ua[] has alignment 1 and passing &rx.current_ua[0] as
             * a uint16_t* is an unaligned-pointer hazard on Cortex-M
             * (-Waddress-of-packed-member).  The accumulate loop above reads
             * the members by VALUE, which the compiler handles; only taking
             * the address needs this.                                        */
            uint16_t beat_ua[NP_SAFETY_MAX_CHANNELS];
            uint8_t  n;
            for (n = 0U; n < NP_SAFETY_MAX_CHANNELS; n++) {
                beat_ua[n] = (n < rx.channel_count) ? rx.current_ua[n] : 0U;
            }
            np_charge_monitor_phase_tick(&s_state, beat_ua,
                                         (uint8_t)NP_SAFETY_MAX_CHANNELS);
        }

        /* Per-session DC budget enforcement — after accumulate, may cut channels */
        np_charge_monitor_tick(&s_state);

        /* Apply granted mask to GPIO */
        np_gpio_mgr_apply(&s_state);

        /* Build reply — send on every iteration so master always has fresh state */
        tx.status     = s_state.status;
        tx.granted_lo = (uint8_t)(s_state.granted_mask & 0xFFU);
        tx.granted_hi = (uint8_t)(s_state.granted_mask >> 8);
        tx.fault_slot = s_state.fault_slot;
        build_tx_checksum(&tx);

        /* Stage the full 38-byte MISO reply: 8-byte reply frame + the extended
         * per-electrode CVNS impedance report (OI-CVNS-HUB-11) in the spare
         * window bytes.  The report builder writes magic 0 when no CVNS
         * measurement is available, in which case the hub ignores the tail. */
        {
            uint8_t reply[NP_SAFETY_RX_EXT_FRAME_LEN];
            np_safety_imp_report_t imp_report;
            memset(reply, 0, sizeof(reply));
            memcpy(reply, &tx, sizeof(tx));
            np_impedance_check_build_cvns_report(&imp_report);
            memcpy(&reply[NP_SAFETY_IMP_REPORT_OFFSET], &imp_report,
                   sizeof(imp_report));
            /* Cardiac-status report (np_safety_nv_report_t): is the active
             * user's cervical VNS withheld, and does anyone on this device have
             * an outstanding cutoff?  The second drives the blanket warning;
             * WHICH user is never sent.  UHDR — never forwarded to SHDR. */
            {
                np_safety_nv_report_t nv_report;
                nv_report.magic = NP_SAFETY_NV_REPORT_MAGIC;
                nv_report.flags = (uint8_t)(
                    (np_nv_cardiac_blocked(np_nv_current_user())
                         ? NP_SAFETY_NV_FLAG_USER_BLOCKED : 0U) |
                    (np_nv_cardiac_outstanding()
                         ? NP_SAFETY_NV_FLAG_OUTSTANDING : 0U));
                nv_report.checksum = (uint16_t)((uint16_t)nv_report.magic +
                                                (uint16_t)nv_report.flags);
                memcpy(&reply[NP_SAFETY_NV_REPORT_OFFSET], &nv_report,
                       sizeof(nv_report));
            }
            /* Tier report (SW01-M10): the tier, why, and whether a T2 line
             * was withheld on this beat — so the hub can say F4, not F1. */
            {
                np_safety_tier_report_t tier_report;
                np_tier_identity_build_report(&tier_report, s_state.requested_mask);
                memcpy(&reply[NP_SAFETY_TIER_REPORT_OFFSET], &tier_report,
                       sizeof(tier_report));
            }
            np_hal_spi_send_reply(reply, (uint8_t)sizeof(reply));
        }

        /* Persist the cardiac-cutoff state across power-on resets
         * (NP-SW-FAULTMSG-001 P1, OI-FAULTMSG-01).  Placed AFTER the GPIO apply
         * and the reply, and gated on an all-zero granted mask, because a flash
         * erase stalls the core for up to 40 ms and no interlock may be starved
         * while any channel is energised.  At a cutoff only CVNS is dropped at
         * once; the rest follow on the next heartbeat (np_spi_watchdog_tick),
         * so the SET write lands within one heartbeat period of the cutoff.
         * The CLR write follows a re-enable, which happens with every channel
         * already off.                                                       */
        if (s_state.granted_mask == 0U) {
            bool nv_value = false;
            if (np_cardiac_interlock_nv_request(&nv_value)) {
                if (np_nv_cardiac_set(np_nv_current_user(), nv_value)) {
                    np_cardiac_interlock_nv_done(nv_value);
                    s_nv_fail_count = 0U;
                } else {
                    s_nv_fail_count++;
                    if (s_nv_fail_count >= (uint8_t)NP_NV_WRITE_ATTEMPTS) {
                        np_cardiac_interlock_nv_done(nv_value);   /* stop retrying */
                        s_nv_fail_count    = 0U;
                        s_state.fault_slot = NP_FAULT_SLOT_NVSTATE;
                        s_state.status    |= NP_SAFETY_STATUS_FAULT;
                    }
                }
            }
        }

        /* Active-user change (per-user cardiac scope, principal 2026-09-22).
         * Accepted only between sessions — no session active, nothing granted,
         * and no cardiac write still owed for the outgoing user — otherwise
         * ignored; the hub sends it before a session starts.  The tag is
         * opaque and chosen by the app; the two reserved values are refused. */
        if (np_hal_spi_user_ready()) {
            np_safety_user_cmd_t ucmd;
            bool owed = false;
            np_hal_spi_get_user(&ucmd);
            if (ucmd.cmd_magic[0] == NP_SAFETY_CMD_MAGIC_0 &&
                ucmd.cmd_magic[1] == NP_SAFETY_CMD_MAGIC_1 &&
                ucmd.cmd_type     == NP_SAFETY_CMD_ACTIVE_USER &&
                user_cmd_checksum_ok(&ucmd) &&
                ucmd.user_tag != NP_SAFETY_USER_UNSPECIFIED &&
                ucmd.user_tag != NP_SAFETY_USER_ANY &&
                !s_state.session_active &&
                s_state.granted_mask == 0U &&
                !np_cardiac_interlock_nv_request(&owed) &&
                ucmd.user_tag != np_nv_current_user()) {
                if (np_nv_set_current_user(ucmd.user_tag)) {
                    np_cardiac_interlock_user_changed(
                        &s_state, np_nv_cardiac_blocked(ucmd.user_tag));
                } else {
                    s_nv_fail_count++;
                    if (s_nv_fail_count >= (uint8_t)NP_NV_WRITE_ATTEMPTS) {
                        s_nv_fail_count    = 0U;
                        s_state.fault_slot = NP_FAULT_SLOT_NVSTATE;
                        s_state.status    |= NP_SAFETY_STATUS_FAULT;
                    }
                }
            }
        }

        /* Commit fault to latch for warm-reset persistence.
         * Guard: do NOT commit until after the first TX reply is sent when a
         * prior fault was loaded from the latch on init.  Committing immediately
         * would overwrite the preserved latch data (fault slot + specific status
         * bits) with the generic FAULT=0x01 / slot=0xFF from init state. */
        if (!s_prior_latch_reported) {
            /* First iteration after a warm-reset-with-prior-fault: report to hub
             * this iteration, then allow commit from next iteration onward. */
            s_prior_latch_reported = true;
        } else if (s_state.status & NP_SAFETY_STATUS_FAULT) {
            np_fault_latch_commit(&s_state);
        }

        /* The ONLY IWDG refresh.  Reached once per iteration, after
         * np_gpio_mgr_apply() has written this iteration's decision to the pins;
         * an iteration that never gets here resets the MCU. */
        np_hal_iwdg_refresh();
    }

    return 0; /* unreachable */
}
