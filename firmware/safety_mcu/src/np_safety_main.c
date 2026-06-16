/*
 * NeuroPulse Safety MCU — Main Entry Point
 * Target: STM32G071 (Cortex-M0+, 64 MHz), bare metal
 * Document: NP-SW-001 Rev A — SW-01 Class C
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
 *   1. Valid heartbeat received with magic + checksum pass
 *   2. Watchdog has not timed out (np_spi_watchdog_check passes)
 *   3. All active interlock checks pass for requested channels
 *   4. Session descriptor signature verified (if session_active bit set)
 */

#include "np_safety_config.h"
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

/* ── Forward declarations for module tick / action functions ─────────────── */
extern void np_spi_watchdog_tick(np_safety_state_t *state,
                                 const np_safety_rx_frame_t *rx,
                                 np_safety_tx_frame_t *tx);
extern void np_spi_watchdog_check(np_safety_state_t *state);   /* MUST call every loop */
extern void np_charge_monitor_tick(np_safety_state_t *state);
extern void np_charge_monitor_reset_session(void);
extern void np_thermal_interlock_tick(np_safety_state_t *state);
extern void np_cardiac_interlock_tick(np_safety_state_t *state);
extern void np_cardiac_interlock_reenable(np_safety_state_t *state);
extern void np_impedance_check_request(uint16_t requested_mask);
extern void np_impedance_check_poll(np_safety_state_t *state);
extern void np_session_sig_reset(np_safety_state_t *state);
extern np_safe_status_t np_session_sig_verify(np_safety_state_t *state,
                                               const uint8_t *hash,
                                               const uint8_t *sig);
extern void np_gpio_mgr_apply(const np_safety_state_t *state);
extern void np_fault_latch_commit(const np_safety_state_t *state);

/* ── HAL stubs (platform-specific — to be implemented against STM32G071 HAL) */
extern void np_hal_clock_init(void);         /* configure PLL for 64 MHz */
extern void np_hal_systick_init(void);       /* 1ms SysTick */
extern void np_hal_gpio_init(void);          /* configure all GPIO banks */
extern void np_hal_spi_slave_init(void);     /* SPI1 slave, 8-byte frames */
extern void np_hal_adc_init(void);           /* ADC1 for NTC channels */
extern void np_hal_tim2_init(void);          /* TIM2 for R-peak capture */
/* Heartbeat frame (8 bytes) — called every main-loop iteration */
extern bool np_hal_spi_frame_ready(void);
extern void np_hal_spi_get_frame(np_safety_rx_frame_t *rx_out);
extern void np_hal_spi_send_frame(const np_safety_tx_frame_t *tx);
/* Session signature command frame (102 bytes) — called when hub delivers sig.
 * HAL distinguishes from heartbeat by NSS-delineated transfer length.
 * np_hal_spi_cmd_ready() returns true when a 102-byte frame is buffered.
 * np_hal_spi_get_cmd() copies the buffered frame into *cmd_out.            */
extern bool np_hal_spi_cmd_ready(void);
extern void np_hal_spi_get_cmd(np_safety_sig_cmd_t *cmd_out);

/* ── Shared safety state ─────────────────────────────────────────────────── */
static np_safety_state_t s_state;
static bool              s_prev_session_active = false;
/* s_prior_latch_reported: true once prior fault is reported to hub on the
 * first heartbeat reply.  Prevents np_fault_latch_commit() from overwriting
 * the preserved latch data (slot + specific status bits) with the generic
 * NP_SAFETY_STATUS_FAULT=0x01 that init places in s_state.status. */
static bool              s_prior_latch_reported = false;

/* ── Checksum verification (matches hub_control np_safety_spi.c) ──────────── */
static bool frame_checksum_ok(const np_safety_rx_frame_t *f)
{
    uint16_t sum = 0U;
    const uint8_t *b = (const uint8_t *)f;
    for (uint8_t i = 0U; i < 6U; i++) {
        sum += b[i];
    }
    return sum == f->checksum;
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

/* ── Main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    bool prior_fault = false;

    /* Peripheral init — must complete before any module init */
    np_hal_clock_init();
    np_hal_systick_init();
    np_hal_gpio_init();    /* drives all stimulation enables LOW immediately */
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

    memset(&s_state, 0, sizeof(s_state));
    s_state.fault_slot = 0xFFU;

    if (prior_fault) {
        /* Prior fault survived warm reset — report it on first heartbeat reply.
         * s_prior_latch_reported stays false until after the first TX frame is
         * sent, preventing np_fault_latch_commit() from immediately overwriting
         * the preserved latch data (fault slot + specific status bits) with the
         * generic status=0x01 / slot=0xFF that s_state was initialised to. */
        s_state.status         |= NP_SAFETY_STATUS_FAULT;
        s_prior_latch_reported  = false;
    } else {
        s_prior_latch_reported = true;  /* no prior fault — commit is safe immediately */
    }

    /* ── Main polling loop ─────────────────────────────────────────────── */
    for (;;) {
        np_safety_rx_frame_t rx;
        np_safety_tx_frame_t tx;
        memset(&tx, 0, sizeof(tx));
        tx.fault_slot = s_state.fault_slot;

        bool valid_frame = false;
        bool cvns_reenable_confirm = false;

        /* ── Session signature command frame (102 bytes) ─────────────────── */
        /* Hub sends this once per session BEFORE the first heartbeat that
         * requests a non-zero enable_mask.  The HAL buffers it separately
         * from the 8-byte heartbeat (distinguished by NSS transfer length). */
        if (np_hal_spi_cmd_ready()) {
            np_safety_sig_cmd_t cmd;
            np_hal_spi_get_cmd(&cmd);

            if (cmd.cmd_magic[0] == NP_SAFETY_CMD_MAGIC_0 &&
                cmd.cmd_magic[1] == NP_SAFETY_CMD_MAGIC_1 &&
                cmd.cmd_type     == NP_SAFETY_CMD_SESSION_SIG &&
                cmd_checksum_ok(&cmd)) {

                np_session_sig_verify(&s_state,
                                      cmd.session_hash,
                                      cmd.session_sig);
                /* Result reflected in NP_SAFETY_STATUS_SIG_PENDING bit:
                 *   success → SIG_PENDING cleared → next heartbeat grants mask
                 *   failure → FAULT + CUTOFF set → hub must abort session     */
            }
            /* Bad magic / bad checksum: silently ignore; SIG_PENDING stays set.
             * Hub will see SIG_PENDING in next heartbeat reply and retry.     */
        }

        /* ── Heartbeat frame (8 bytes) ────────────────────────────────────── */
        if (np_hal_spi_frame_ready()) {
            np_hal_spi_get_frame(&rx);

            if (rx.magic[0] == NP_SAFETY_BEAT_MAGIC_0 &&
                rx.magic[1] == NP_SAFETY_BEAT_MAGIC_1 &&
                frame_checksum_ok(&rx)) {

                valid_frame = true;
                cvns_reenable_confirm =
                    (rx.session_status & NP_SESSION_STATUS_CVNS_REENABLE) != 0U;

                s_state.requested_mask =
                    (uint16_t)rx.enable_lo | ((uint16_t)rx.enable_hi << 8);
                s_state.session_active =
                    (rx.session_status & NP_SESSION_STATUS_ACTIVE) != 0U;
                s_state.cvns_active =
                    (s_state.requested_mask & NP_SAFETY_EN_CVNS) != 0U;

                /* Reset watchdog on valid heartbeat */
                np_spi_watchdog_tick(&s_state, &rx, &tx);
            }
            /* Invalid frame: watchdog continues counting; no enable granted */
        }

        /* Watchdog timeout check — MUST run every iteration regardless of SPI */
        np_spi_watchdog_check(&s_state);

        /* Detect session_active 0→1 transition: reset per-session state */
        if (s_state.session_active && !s_prev_session_active) {
            np_session_sig_reset(&s_state);  /* sets NP_SAFETY_STATUS_SIG_PENDING */
            np_charge_monitor_reset_session();
            np_impedance_check_request(s_state.requested_mask);
        }
        s_prev_session_active = s_state.session_active;

        /* CVNS re-enable after cardiac cutoff:
         * Hub sets NP_SESSION_STATUS_CVNS_REENABLE only when all three
         * conditions are met on the hub side: lockout elapsed + user
         * confirmation + re-established baseline signal.
         * Safety MCU clears CARDIAC status and restarts impedance check.     */
        if (valid_frame && cvns_reenable_confirm &&
            (s_state.status & NP_SAFETY_STATUS_CARDIAC) != 0U) {
            np_cardiac_interlock_reenable(&s_state);
            np_impedance_check_request(NP_SAFETY_EN_CVNS);
        }

        /* Tick safety monitors every main-loop iteration */
        np_thermal_interlock_tick(&s_state);
        np_charge_monitor_tick(&s_state);
        np_impedance_check_poll(&s_state);
        if (s_state.cvns_active) {
            np_cardiac_interlock_tick(&s_state);
        }

        /* Apply granted mask to GPIO */
        np_gpio_mgr_apply(&s_state);

        /* Build reply — send on every iteration so master always has fresh state */
        tx.status     = s_state.status;
        tx.granted_lo = (uint8_t)(s_state.granted_mask & 0xFFU);
        tx.granted_hi = (uint8_t)(s_state.granted_mask >> 8);
        tx.fault_slot = s_state.fault_slot;
        build_tx_checksum(&tx);
        np_hal_spi_send_frame(&tx);

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
    }

    return 0; /* unreachable */
}
