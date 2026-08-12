/*
 * NeurOne Hub Control Program — Safety MCU SPI Heartbeat Implementation
 * Document: NP-FW-HUB-001 Rev 1 §7
 */

#include "np_safety_spi.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* ── Internal state ───────────────────────────────────────────────────────────── */

static volatile uint16_t s_requested_mask = 0U;
static volatile uint16_t s_granted_mask   = 0U;
static volatile uint8_t  s_mcu_status     = NP_SAFETY_STATUS_OK;
/* OI-CHARGE-03: set true by the session runner while a small-electrode modality
 * (HD-tDCS ring/bilateral) is in the session.  Every heartbeat then carries
 * NP_SESSION_STATUS_GEOM_REQUIRED so the safety MCU keeps CLIN_STIM blocked
 * until it has applied the electrode-area command (fail-safe).              */
static volatile bool     s_geom_required  = false;
/* OI-CVNS-HUB-01: mirrored from np_cvns_reenable_bit_active() by the heartbeat
 * task each beat.  True only while the re-enable state machine is ASSERTING
 * (all three gates passed).  Every heartbeat then carries
 * NP_SESSION_STATUS_CVNS_REENABLE so the safety MCU clears its cardiac latch. */
static volatile bool     s_cvns_reenable  = false;

/* OI-CVNS-HUB-11: latest per-electrode CVNS impedance reported by the safety MCU
 * in the spare bytes of the heartbeat reply window.  Consumed by the CVNS module
 * (via np_safety_spi_get_cvns_impedance → the heartbeat task's publish) to
 * cross-validate against the hub's own per-electrode measurement.  Raw kΩ is
 * UHDR and is NEVER written to SHDR — only a divergence flag is.               */
static volatile float    s_cvns_imp_kohm[NP_SAFETY_IMP_CVNS_ELECTRODES];
static volatile bool     s_cvns_imp_valid = false;

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
    /* OI-SAF-01: platform SPI peripheral initialization placeholder.
     * The real implementation configures LPSPI3 on i.MX RT1062:
     *   - CPOL=0, CPHA=0, 8-bit, MSB first
     *   - Clock ≤ 10 MHz
     *   - GPIO_B0_04..08 driven LOW (GAIN_SEL[0..4], OI-PBM-HW-01)
     */
    s_requested_mask = 0U;
    s_granted_mask   = 0U;
    s_mcu_status     = NP_SAFETY_STATUS_OK;
    s_geom_required  = false;
    s_cvns_reenable  = false;
    s_cvns_imp_valid = false;
    for (uint8_t e = 0U; e < NP_SAFETY_IMP_CVNS_ELECTRODES; e++) {
        s_cvns_imp_kohm[e] = 0.0f;
    }
    return NP_HUB_OK;
}

/*
 * parse_cvns_impedance_report — OI-CVNS-HUB-11.  Extract the safety MCU's
 * per-electrode CVNS impedance from the spare bytes of the 38-byte heartbeat
 * receive buffer.  Validates the report magic, checksum, and valid flag; on a
 * good report stores per-electrode kΩ and sets s_cvns_imp_valid, otherwise
 * clears s_cvns_imp_valid (so a corrupt/absent report never cross-validates).
 */
static void parse_cvns_impedance_report(const uint8_t *rx_raw)
{
    np_safety_imp_report_t rep;
    uint16_t sum = 0U;
    const uint8_t *b = (const uint8_t *)&rep;
    uint8_t i;

    memcpy(&rep, rx_raw + NP_SAFETY_IMP_REPORT_OFFSET, sizeof(rep));

    if (rep.magic != NP_SAFETY_IMP_REPORT_MAGIC ||
        (rep.flags & NP_SAFETY_IMP_FLAG_CVNS_VALID) == 0U) {
        s_cvns_imp_valid = false;
        return;
    }
    for (i = 0U; i < (NP_SAFETY_IMP_REPORT_LEN - 2U); i++) {
        sum += b[i];
    }
    if (sum != rep.checksum) {
        s_cvns_imp_valid = false;
        return;
    }
    for (i = 0U; i < NP_SAFETY_IMP_CVNS_ELECTRODES; i++) {
        s_cvns_imp_kohm[i] = (float)rep.cvns_kohm_x100[i] / 100.0f;
    }
    s_cvns_imp_valid = true;
}

np_hub_status_t np_safety_spi_heartbeat(np_session_state_t  session_state,
                                          uint16_t            requested_enable_mask,
                                          const uint16_t     *current_ua,
                                          uint8_t             channel_count)
{
    /* Extended 38-byte frame (hub→MCU).  MCU simultaneously sends back 8 bytes
     * of reply; hub receives NP_SAFETY_RX_EXT_FRAME_LEN bytes total, of which
     * the first sizeof(np_safety_rx_frame_t) (8) are meaningful MCU reply.    */
    np_safety_tx_ext_frame_t tx;
    uint8_t                  rx_raw[NP_SAFETY_RX_EXT_FRAME_LEN];
    np_safety_rx_frame_t     mcu_reply;
    np_hub_status_t          rc;
    uint8_t                  ch;

    memset(&tx,      0, sizeof(tx));
    memset(rx_raw,   0, sizeof(rx_raw));

    /* ── Build base fields (bytes 0–5) ──────────────────────────────────── */
    tx.magic[0]      = NP_SAFETY_BEAT_MAGIC_0;
    tx.magic[1]      = NP_SAFETY_BEAT_MAGIC_1;
    /* session_status is a BIT FLAGS byte, built by the pure helper — never the
     * raw np_session_state_t enum (RUNNING=3 would spuriously assert
     * CVNS_REENABLE; PAUSED=4 would assert GEOM_REQUIRED).  OI-CHARGE-03 and
     * OI-CVNS-HUB-01 flags are advertised EVERY frame so a single lost
     * heartbeat can neither disarm the geometry gate nor drop the re-enable.
     * Set BEFORE the base checksum (which covers bytes [0..5]).             */
    tx.session_status = np_safety_session_status_bits(session_state,
                                                      s_geom_required,
                                                      s_cvns_reenable);
    tx.enable_lo     = (uint8_t)(requested_enable_mask & 0xFFU);
    tx.enable_hi     = (uint8_t)((requested_enable_mask >> 8) & 0xFFU);

    /* channel_count caps at NP_SAFETY_MAX_CHANNELS (14) */
    tx.channel_count = (channel_count <= NP_SAFETY_MAX_CHANNELS)
                         ? channel_count
                         : (uint8_t)NP_SAFETY_MAX_CHANNELS;

    /* Base checksum: bytes [0..5] */
    tx.checksum = compute_checksum((const uint8_t *)&tx, 6U);

    /* ── Build current_ua array (bytes 8–35) ────────────────────────────── */
    /* current_ua[] carries COMMANDED current from the session descriptor.
     * SHDR classification: device metric.  NOT ADC-measured actual current
     * (which would be UHDR — tissue impedance).  See NP-FW-EMMC-001 §12.   */
    if (current_ua != NULL) {
        for (ch = 0U; ch < tx.channel_count; ch++) {
            tx.current_ua[ch] = current_ua[ch];
        }
    }

    /* Ext checksum: bytes [8..35] (current_ua[14] only) */
    tx.ext_checksum = compute_checksum(
        (const uint8_t *)&tx + 8U,
        (size_t)NP_SAFETY_MAX_CHANNELS * sizeof(uint16_t));

    /* ── Transfer ────────────────────────────────────────────────────────── */
    rc = np_safety_hal_spi_transfer((const uint8_t *)&tx,
                                    rx_raw,
                                    NP_SAFETY_RX_EXT_FRAME_LEN);
    if (rc != NP_HUB_OK) {
        return NP_HUB_ERR_TIMEOUT;
    }

    /* Extract the first 8 bytes (meaningful MCU reply) */
    memcpy(&mcu_reply, rx_raw, sizeof(mcu_reply));

    /* Verify reply checksum — if bad, treat as safety fault.
     * Zero s_granted_mask so callers do not act on a stale grant.          */
    uint16_t expected = compute_checksum((const uint8_t *)&mcu_reply, 6U);
    if (mcu_reply.checksum != expected) {
        s_granted_mask   = 0U;
        s_mcu_status     = NP_SAFETY_STATUS_FAULT;
        s_cvns_imp_valid = false;   /* OI-CVNS-HUB-11: distrust a corrupt frame's tail */
        return NP_HUB_ERR_SAFETY_FAULT;
    }

    /* OI-CVNS-HUB-11: the base reply is good — parse the per-electrode CVNS
     * impedance report the MCU clocks out in the spare window bytes [8..15]
     * (independently magic + checksum validated inside). */
    parse_cvns_impedance_report(rx_raw);

    s_granted_mask = (uint16_t)((uint16_t)mcu_reply.granted_lo |
                                 ((uint16_t)mcu_reply.granted_hi << 8));
    s_mcu_status   = mcu_reply.status;

    if (mcu_reply.status & (NP_SAFETY_STATUS_FAULT | NP_SAFETY_STATUS_CUTOFF)) {
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

np_hub_status_t np_safety_spi_send_channel_limits(const uint16_t *area_mcm2,
                                                   uint8_t         count)
{
    np_safety_chan_limit_cmd_t cmd;
    /* rx_dummy: discard the MCU's concurrent transmission during the cmd frame. */
    uint8_t rx_dummy[NP_SAFETY_CHAN_LIMIT_FRAME_LEN];
    uint8_t ch;

    if (area_mcm2 == NULL || count == 0U || count > NP_SAFETY_MAX_CHANNELS) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd_magic[0] = NP_SAFETY_CMD_MAGIC_0;
    cmd.cmd_magic[1] = NP_SAFETY_CMD_MAGIC_1;
    cmd.cmd_type     = NP_SAFETY_CMD_CHAN_LIMIT;
    cmd.reserved     = 0U;
    for (ch = 0U; ch < count; ch++) {
        cmd.area_mcm2[ch] = area_mcm2[ch];
    }
    /* Remaining entries stay 0 (memset) → "keep default" on the MCU. */
    cmd.checksum = compute_checksum((const uint8_t *)&cmd,
                                    NP_SAFETY_CHAN_LIMIT_FRAME_LEN - 2U);

    np_hub_status_t rc = np_safety_hal_spi_transfer((const uint8_t *)&cmd,
                                                    rx_dummy,
                                                    NP_SAFETY_CHAN_LIMIT_FRAME_LEN);
    if (rc != NP_HUB_OK) {
        return NP_HUB_ERR_TIMEOUT;
    }

    return NP_HUB_OK;
}

void np_safety_spi_set_geom_required(bool required)
{
    taskENTER_CRITICAL();
    s_geom_required = required;
    taskEXIT_CRITICAL();
}

void np_safety_spi_set_cvns_reenable(bool active)
{
    taskENTER_CRITICAL();
    s_cvns_reenable = active;
    taskEXIT_CRITICAL();
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
    s_geom_required  = false;   /* OI-CHARGE-03:   clear the geometry flag on abort/end */
    s_cvns_reenable  = false;   /* OI-CVNS-HUB-01: never let a re-enable outlive a session */
    taskEXIT_CRITICAL();
}

uint16_t np_safety_spi_get_granted_mask(void)
{
    return s_granted_mask;
}

uint16_t np_safety_spi_get_requested_mask(void)
{
    return s_requested_mask;
}

uint8_t np_safety_spi_get_status(void)
{
    return s_mcu_status;
}

bool np_safety_spi_get_cvns_impedance(float out_kohm[], bool *valid_out)
{
    bool valid = s_cvns_imp_valid;
    if (out_kohm != NULL) {
        for (uint8_t e = 0U; e < NP_SAFETY_IMP_CVNS_ELECTRODES; e++) {
            out_kohm[e] = s_cvns_imp_kohm[e];
        }
    }
    if (valid_out != NULL) {
        *valid_out = valid;
    }
    return valid;
}
