/*
 * NeuroPulse Hub Control Program — Safety MCU SPI Heartbeat Implementation
 * Document: NP-FW-HUB-001 Rev A §7
 */

#include "np_safety_spi.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* ── Internal state ───────────────────────────────────────────────────────────── */

static volatile uint16_t s_requested_mask = 0U;
static volatile uint16_t s_granted_mask   = 0U;
static volatile uint8_t  s_mcu_status     = NP_SAFETY_STATUS_OK;

/* s_requested_mask is protected by the FreeRTOS task-level critical section
 * (taskENTER_CRITICAL / taskEXIT_CRITICAL).  These functions are called from
 * task context only — never from an ISR — so the task variants are correct. */

/* ── Checksum ─────────────────────────────────────────────────────────────────── */

static uint16_t compute_checksum(const uint8_t *buf, size_t len)
{
    uint16_t sum = 0U;
    for (size_t i = 0U; i < len; i++) {
        sum += buf[i];
    }
    return sum;
}

/* ── Public API ───────────────────────────────────────────────────────────────── */

np_hub_status_t np_safety_spi_init(void)
{
    /* OI-SAF-01: platform SPI peripheral initialisation placeholder.
     * The real implementation configures LPSPI3 on i.MX RT1062:
     *   - CPOL=0, CPHA=0, 8-bit, MSB first
     *   - Clock ≤ 10 MHz
     *   - GPIO_B0_04..08 driven LOW (GAIN_SEL[0..4], OI-PBM-HW-01)
     */
    s_requested_mask = 0U;
    s_granted_mask   = 0U;
    s_mcu_status     = NP_SAFETY_STATUS_OK;
    return NP_HUB_OK;
}

np_hub_status_t np_safety_spi_heartbeat(np_session_state_t session_state,
                                          uint16_t           requested_enable_mask)
{
    np_safety_tx_frame_t tx;
    np_safety_rx_frame_t rx;
    np_hub_status_t      rc;

    memset(&tx, 0, sizeof(tx));
    memset(&rx, 0, sizeof(rx));

    tx.magic[0]       = NP_SAFETY_BEAT_MAGIC_0;
    tx.magic[1]       = NP_SAFETY_BEAT_MAGIC_1;
    tx.session_status = (uint8_t)session_state;
    tx.enable_lo      = (uint8_t)(requested_enable_mask & 0xFFU);
    tx.enable_hi      = (uint8_t)((requested_enable_mask >> 8) & 0xFFU);
    tx.reserved       = 0U;
    tx.checksum       = compute_checksum((const uint8_t *)&tx, 6U);

    rc = np_safety_hal_spi_transfer((const uint8_t *)&tx,
                                    (uint8_t *)&rx,
                                    NP_SAFETY_FRAME_LEN);
    if (rc != NP_HUB_OK) {
        return NP_HUB_ERR_TIMEOUT;
    }

    /* Verify reply checksum — if bad, treat as safety fault.
     * Zero s_granted_mask so callers do not act on a stale grant. */
    uint16_t expected = compute_checksum((const uint8_t *)&rx, 6U);
    if (rx.checksum != expected) {
        s_granted_mask = 0U;
        s_mcu_status   = NP_SAFETY_STATUS_FAULT;
        return NP_HUB_ERR_SAFETY_FAULT;
    }

    s_granted_mask = (uint16_t)((uint16_t)rx.granted_lo |
                                 ((uint16_t)rx.granted_hi << 8));
    s_mcu_status   = rx.status;

    if (rx.status & (NP_SAFETY_STATUS_FAULT | NP_SAFETY_STATUS_CUTOFF)) {
        return NP_HUB_ERR_SAFETY_FAULT;
    }

    return NP_HUB_OK;
}

np_hub_status_t np_safety_spi_send_session_sig(const uint8_t *hash,
                                                const uint8_t *sig)
{
    np_safety_sig_cmd_t cmd;
    /* rx_dummy: discard the MCU's concurrent transmission during the cmd frame.
     * The definitive result is the SIG_PENDING bit in the next heartbeat reply. */
    uint8_t rx_dummy[NP_SAFETY_CMD_FRAME_LEN];

    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd_magic[0] = NP_SAFETY_CMD_MAGIC_0;
    cmd.cmd_magic[1] = NP_SAFETY_CMD_MAGIC_1;
    cmd.cmd_type     = NP_SAFETY_CMD_SESSION_SIG;
    cmd.reserved     = 0U;
    memcpy(cmd.session_hash, hash, NP_SESSION_HASH_LEN);
    memcpy(cmd.session_sig,  sig,  NP_ED25519_SIG_LEN);
    cmd.checksum = compute_checksum((const uint8_t *)&cmd,
                                    NP_SAFETY_CMD_FRAME_LEN - 2U);

    np_hub_status_t rc = np_safety_hal_spi_transfer((const uint8_t *)&cmd,
                                                    rx_dummy,
                                                    NP_SAFETY_CMD_FRAME_LEN);
    if (rc != NP_HUB_OK) {
        return NP_HUB_ERR_TIMEOUT;
    }

    return NP_HUB_OK;
}

void np_safety_spi_request_enable(uint16_t channel_mask)
{
    taskENTER_CRITICAL();
    s_requested_mask |= channel_mask;
    taskEXIT_CRITICAL();
}

void np_safety_spi_request_disable(uint16_t channel_mask)
{
    taskENTER_CRITICAL();
    s_requested_mask &= ~channel_mask;
    taskEXIT_CRITICAL();
}

void np_safety_spi_disable_all(void)
{
    taskENTER_CRITICAL();
    s_requested_mask = 0U;
    taskEXIT_CRITICAL();
}

uint16_t np_safety_spi_get_granted_mask(void)
{
    return s_granted_mask;
}

uint8_t np_safety_spi_get_status(void)
{
    return s_mcu_status;
}
