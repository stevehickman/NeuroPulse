/*
 * NeurOne Safety MCU — SW-01 Platform Layer: SPI1 slave (to i.MX RT1062)
 * Target: STM32G071 (Cortex-M0+, 64 MHz), bare metal, CMSIS registers only
 * Document: NP-SW-001 Rev 1 — SW-01 Class C; NP-SW-CI-001 §4.4 (OI-SWCI-17)
 *           CLAUDE.md §4.2 (200 ms heartbeat, 1.5 s watchdog); OI-CVNS-HUB-11
 *
 * Implements the eight SPI symbols in np_safety_hal.h plus SPI1_IRQHandler.
 *
 * Three inbound frame types share one link and are told apart BY NSS-DELINEATED
 * TRANSFER LENGTH, not by any in-band tag (np_safety_hal.h):
 *
 *     38 bytes   extended heartbeat   np_safety_rx_ext_frame_t
 *    102 bytes   session signature    np_safety_sig_cmd_t
 *     34 bytes   per-channel limits   np_safety_chan_limit_cmd_t
 *
 * ── WHY NSS IS POLLED AND NOT AN EXTI SOURCE ─────────────────────────────────
 * Closing a frame requires knowing when NSS went high, and STM32 SPI slaves
 * raise no NSS-rising interrupt.  The obvious fix is EXTI on PA4 — which works,
 * because EXTI samples the pad regardless of the pin's alternate-function mode.
 * It is not used here for two reasons.  First, PA4 (EXTI line 4) and PA8 (the
 * R-peak input, line 8) share the EXTI4_15 vector, so it would put the SPI
 * frame boundary and the cardiac capture in one handler and make an R-peak
 * timestamp wait behind SPI work.  Second, the three `_ready()` predicates are
 * polled from the main loop anyway, so an interrupt would buy nothing: the loop
 * has to come back and ask regardless.
 *
 * Instead np_hal_spi_poll() reads the NSS pin level through GPIOA->IDR (valid in
 * AF mode) and closes the frame when NSS is high with bytes buffered.  Every
 * `_ready()` calls it first.  This keeps EXTI4_15 single-owner (np_hal_rpeak.c)
 * and keeps the classification on the main-loop side where a mis-sized frame
 * can be counted rather than dropped in an ISR.
 *
 * TIMING REQUIREMENT this creates, stated because it is a real constraint and
 * not free: the main loop must call a `_ready()` predicate at least once
 * between the end of one transfer and the start of the next.  The hub's
 * heartbeat period is NP_SAFETY_HEARTBEAT_EXP_MS = 200 ms and np_safety_main.c
 * calls np_hal_spi_cmd_ready() unconditionally at the top of every iteration,
 * so the margin is the whole 200 ms against a loop whose slowest element is a
 * ~65 µs six-channel ADC sweep.  Recorded as OI-SWCI-31 so a future long-
 * running addition to the loop is measured against it rather than assumed safe.
 *
 * ── ARRIVAL SEMANTICS (answers np_safety_hal.h open question 7) ──────────────
 * The header lists as unspecified: whether the three predicates are mutually
 * exclusive, whether more than one frame can be buffered, and what happens to
 * an un-drained frame when the next arrives.  Defined here:
 *
 *  · NOT mutually exclusive.  Each type has its OWN holding buffer and its own
 *    ready flag, so a session-signature command and a heartbeat can both be
 *    pending, and np_safety_main.c — which tests all three in one iteration —
 *    drains them in its own order.  A single shared buffer would let a 102-byte
 *    signature command destroy a 38-byte heartbeat that had not been read yet,
 *    which would stall the watchdog on a link that was working.
 *  · ONE frame per type is buffered.  A second arrival of the SAME type before
 *    the first is drained is NEWEST-WINS, and increments an overrun counter.
 *    Newest-wins is right for all three: a heartbeat is a periodic state
 *    snapshot where the stale copy is worthless; a signature command and a
 *    channel-limit command are both re-sent by the hub on retry, so the newest
 *    is the one the hub is waiting on.
 *  · A transfer whose length matches NONE of 38/102/34 is discarded and counted.
 *    It is not passed to any consumer — every consumer memcpy's a fixed-size
 *    struct out of it, so a short frame would read past what was received.
 *
 * ── CPOL/CPHA ────────────────────────────────────────────────────────────────
 * Mode 0 (CPOL=0, CPHA=0).  Nothing in this repository records the hub's SPI
 * mode — np_safety_config.h names the instance and the pins but not the clock
 * polarity or phase, and the hub-side driver is on the other side of the SPI
 * boundary.  Mode 0 is the conventional default and is what the i.MX RT1062
 * ECSPI comes up in.  A mismatch here does not fail quietly in a dangerous
 * direction — it corrupts every byte, so every frame fails its magic and
 * checksum, no enable is ever granted, and the watchdog cuts stimulation after
 * 1.5 s.  It is nonetheless an assumption and is recorded as OI-SWCI-32 for
 * cross-check against the hub driver.
 *
 * IEC 62304 Class C — MISRA C:2012.  C11, no GNU extensions.
 */

#include "np_hal_internal.h"

#include <string.h>

/* SPI1 pins, all AF0 on STM32G0: PA4 NSS, PA5 SCK, PA6 MISO, PA7 MOSI. */
#define NP_HAL_SPI_AF        0U
#define NP_HAL_SPI_NSS_PORT  GPIOA
#define NP_HAL_SPI_NSS_PIN   (1U << 4)
#define NP_HAL_SPI_SCK_PIN   (1U << 5)
#define NP_HAL_SPI_MISO_PIN  (1U << 6)
#define NP_HAL_SPI_MOSI_PIN  (1U << 7)

/* The longest frame on the link.  Sized from the protocol constants rather than
 * written as 102, so a wire-format change is a compile-time resize. */
#define NP_HAL_SPI_RX_MAX  NP_SAFETY_CMD_FRAME_LEN

_Static_assert(NP_SAFETY_CMD_FRAME_LEN >= NP_SAFETY_RX_EXT_FRAME_LEN,
               "RX buffer must hold the longest frame");
_Static_assert(NP_SAFETY_CMD_FRAME_LEN >= NP_SAFETY_CHAN_LIMIT_FRAME_LEN,
               "RX buffer must hold the longest frame");

/* Frame classification.  Pure and separately named so np_hal_platform_tests.c
 * can drive it over every length — including the ones that must be rejected —
 * without an SPI peripheral.  np_safety_hal.h makes length the ONLY
 * discriminator, so this three-way comparison is the whole protocol demux and
 * deserves a test rather than a glance. */
typedef enum {
    NP_HAL_FRAME_NONE = 0,
    NP_HAL_FRAME_HEARTBEAT,
    NP_HAL_FRAME_SIG_CMD,
    NP_HAL_FRAME_CHAN_LIMIT
} np_hal_frame_kind_t;

np_hal_frame_kind_t np_hal_spi_classify(uint16_t len);
np_hal_frame_kind_t np_hal_spi_classify(uint16_t len)
{
    if (len == NP_SAFETY_RX_EXT_FRAME_LEN)     { return NP_HAL_FRAME_HEARTBEAT; }
    if (len == NP_SAFETY_CMD_FRAME_LEN)        { return NP_HAL_FRAME_SIG_CMD; }
    if (len == NP_SAFETY_CHAN_LIMIT_FRAME_LEN) { return NP_HAL_FRAME_CHAN_LIMIT; }
    return NP_HAL_FRAME_NONE;
}

/* ── Buffers ───────────────────────────────────────────────────────────────── */

/* In-flight transfer, written by the RXNE interrupt. */
static volatile uint8_t  s_rx[NP_HAL_SPI_RX_MAX];
static volatile uint16_t s_rx_len = 0U;

/* One holding buffer per frame type — see ARRIVAL SEMANTICS above. */
static uint8_t s_hb[NP_SAFETY_RX_EXT_FRAME_LEN];
static uint8_t s_cmd[NP_SAFETY_CMD_FRAME_LEN];
static uint8_t s_clim[NP_SAFETY_CHAN_LIMIT_FRAME_LEN];

static volatile bool s_hb_ready   = false;
static volatile bool s_cmd_ready  = false;
static volatile bool s_clim_ready = false;

/* Diagnostics.  Not currently reported over the wire — the TX frame has no
 * spare field — but kept because they are the only evidence that would
 * distinguish "the hub is silent" from "the hub is talking and every frame is
 * the wrong length", which look identical from the watchdog's point of view.
 * Exposed through np_hal_spi_stats() for the host tests and for a future
 * diagnostic frame; recorded as OI-SWCI-33. */
static volatile uint32_t s_overrun_count = 0U;
static volatile uint32_t s_badlen_count  = 0U;

/* Staged MISO payload for the NEXT transfer (OI-CVNS-HUB-11). */
static volatile uint8_t  s_tx[NP_SAFETY_RX_EXT_FRAME_LEN];
static volatile uint16_t s_tx_len = 0U;
static volatile uint16_t s_tx_idx = 0U;

void np_hal_spi_stats(uint32_t *overrun_out, uint32_t *badlen_out);
void np_hal_spi_stats(uint32_t *overrun_out, uint32_t *badlen_out)
{
    if (overrun_out != NULL) { *overrun_out = s_overrun_count; }
    if (badlen_out  != NULL) { *badlen_out  = s_badlen_count; }
}

void np_hal_spi_slave_init(void)
{
    uint8_t i;

    np_hal_port_clocks_enable();
    np_hal_pin_config_af(NP_HAL_SPI_NSS_PORT,  NP_HAL_SPI_NSS_PIN,  NP_HAL_SPI_AF);
    np_hal_pin_config_af(NP_HAL_SPI_NSS_PORT,  NP_HAL_SPI_SCK_PIN,  NP_HAL_SPI_AF);
    np_hal_pin_config_af(NP_HAL_SPI_NSS_PORT,  NP_HAL_SPI_MISO_PIN, NP_HAL_SPI_AF);
    np_hal_pin_config_af(NP_HAL_SPI_NSS_PORT,  NP_HAL_SPI_MOSI_PIN, NP_HAL_SPI_AF);

    RCC->APBENR2 |= RCC_APBENR2_SPI1EN;
    (void)RCC->APBENR2;

    NP_SAFETY_SPI_INSTANCE->CR1 = 0UL;          /* disable before configuring */

    /* Slave, hardware NSS (SSM = 0), MSB first, mode 0.  MSTR = 0 and BR are
     * both irrelevant in slave mode — the master supplies the clock — and are
     * left at zero rather than written to a misleading value. */
    NP_SAFETY_SPI_INSTANCE->CR1 = 0UL;

    /* 8-bit data size (DS = 0b0111), RXNE on every byte (FRXTH = 1).  Without
     * FRXTH the FIFO signals RXNE only every 16 bits and a 38-byte frame would
     * be read as 19 half-words with the byte order decided by the FIFO rather
     * than by the wire format. */
    NP_SAFETY_SPI_INSTANCE->CR2 =
        (7UL << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH | SPI_CR2_RXNEIE;

    for (i = 0U; i < NP_SAFETY_RX_EXT_FRAME_LEN; i++) {
        s_tx[i] = 0U;
    }
    s_rx_len = 0U;
    s_tx_len = 0U;
    s_tx_idx = 0U;

    NVIC_ClearPendingIRQ(SPI1_IRQn);
    NVIC_EnableIRQ(SPI1_IRQn);

    NP_SAFETY_SPI_INSTANCE->CR1 |= SPI_CR1_SPE;
}

void SPI1_IRQHandler(void);
void SPI1_IRQHandler(void)
{
    uint32_t sr = NP_SAFETY_SPI_INSTANCE->SR;

    /* Overrun: the master clocked a byte we did not read in time.  The frame is
     * already corrupt, so mark the transfer poisoned by driving the length past
     * every valid size — np_hal_spi_classify() then rejects it and the frame is
     * counted as bad rather than delivered half-right.  A corrupt heartbeat
     * that reached np_safety_main.c would still be caught by its magic and
     * checksum, but a corrupt frame must not depend on a downstream check to
     * be discarded. */
    if ((sr & SPI_SR_OVR) != 0U) {
        (void)NP_SAFETY_SPI_INSTANCE->DR;
        (void)NP_SAFETY_SPI_INSTANCE->SR;
        s_rx_len = (uint16_t)(NP_HAL_SPI_RX_MAX + 1U);
        return;
    }

    if ((sr & SPI_SR_RXNE) != 0U) {
        /* Byte-wide read.  A 32-bit read of DR would pop two bytes out of the
         * FIFO and silently halve the frame length. */
        uint8_t b = *(volatile uint8_t *)(&NP_SAFETY_SPI_INSTANCE->DR);

        if (s_rx_len < (uint16_t)NP_HAL_SPI_RX_MAX) {
            s_rx[s_rx_len] = b;
            s_rx_len++;
        } else {
            /* Longer than the longest legal frame — keep counting so the length
             * stays out of the valid set and the transfer is rejected whole. */
            if (s_rx_len < 0xFFFFU) {
                s_rx_len++;
            }
        }

        /* Clock out the next staged MISO byte.  Zero once the staged payload is
         * exhausted, which is what the hub expects in the 22 pad bytes of the
         * 38-byte window (np_safety_hal.h: "buf[16..37] zero"). */
        {
            uint8_t out = 0U;
            if (s_tx_idx < s_tx_len) {
                out = s_tx[s_tx_idx];
                s_tx_idx++;
            }
            *(volatile uint8_t *)(&NP_SAFETY_SPI_INSTANCE->DR) = out;
        }
    }
}

/*
 * Close a completed transfer.  Called from every `_ready()` predicate.
 * NSS high with bytes buffered means the master has deselected us and the frame
 * is whole.
 */
static void np_hal_spi_poll(void)
{
    uint16_t len;

    if (!np_hal_pin_read(NP_HAL_SPI_NSS_PORT, NP_HAL_SPI_NSS_PIN)) {
        return;                          /* NSS low — transfer still in flight */
    }

    len = s_rx_len;
    if (len == 0U) {
        return;                          /* idle */
    }

    switch (np_hal_spi_classify(len)) {
    case NP_HAL_FRAME_HEARTBEAT:
        if (s_hb_ready) { s_overrun_count++; }         /* newest-wins */
        memcpy(s_hb, (const void *)s_rx, sizeof(s_hb));
        s_hb_ready = true;
        break;
    case NP_HAL_FRAME_SIG_CMD:
        if (s_cmd_ready) { s_overrun_count++; }
        memcpy(s_cmd, (const void *)s_rx, sizeof(s_cmd));
        s_cmd_ready = true;
        break;
    case NP_HAL_FRAME_CHAN_LIMIT:
        if (s_clim_ready) { s_overrun_count++; }
        memcpy(s_clim, (const void *)s_rx, sizeof(s_clim));
        s_clim_ready = true;
        break;
    case NP_HAL_FRAME_NONE:
    default:
        s_badlen_count++;
        break;
    }

    s_rx_len = 0U;
    s_tx_idx = 0U;                       /* next transfer restarts the reply */
}

bool np_hal_spi_frame_ready(void)
{
    np_hal_spi_poll();
    return s_hb_ready;
}

void np_hal_spi_get_ext_frame(np_safety_rx_ext_frame_t *rx_out)
{
    if (rx_out == NULL) {
        return;
    }
    /* sizeof the DESTINATION, so this cannot over-copy even if the holding
     * buffer is ever resized independently (np_safety_hal.h: the `_get_*()`
     * calls are only valid when the matching `_ready()` returned true). */
    memcpy(rx_out, s_hb, sizeof(*rx_out));
    s_hb_ready = false;
}

bool np_hal_spi_cmd_ready(void)
{
    np_hal_spi_poll();
    return s_cmd_ready;
}

void np_hal_spi_get_cmd(np_safety_sig_cmd_t *cmd_out)
{
    if (cmd_out == NULL) {
        return;
    }
    memcpy(cmd_out, s_cmd, sizeof(*cmd_out));
    s_cmd_ready = false;
}

bool np_hal_spi_chan_limit_ready(void)
{
    np_hal_spi_poll();
    return s_clim_ready;
}

void np_hal_spi_get_chan_limit(np_safety_chan_limit_cmd_t *cmd_out)
{
    if (cmd_out == NULL) {
        return;
    }
    memcpy(cmd_out, s_clim, sizeof(*cmd_out));
    s_clim_ready = false;
}

void np_hal_spi_send_reply(const uint8_t *buf, uint8_t len)
{
    uint16_t n;
    uint16_t i;

    if (buf == NULL) {
        return;
    }

    n = (len > (uint8_t)NP_SAFETY_RX_EXT_FRAME_LEN)
            ? (uint16_t)NP_SAFETY_RX_EXT_FRAME_LEN
            : (uint16_t)len;

    /* Stage the whole payload, then publish the length.  Publishing the length
     * first would let the RXNE interrupt clock out bytes from a half-written
     * buffer — the hub would receive a reply whose granted_mask and checksum
     * came from different iterations, and it would pass the checksum. */
    for (i = 0U; i < n; i++) {
        s_tx[i] = buf[i];
    }
    for (i = n; i < (uint16_t)NP_SAFETY_RX_EXT_FRAME_LEN; i++) {
        s_tx[i] = 0U;
    }
    s_tx_len = n;
}

/*
 * np_hal_spi_send_frame — the one symbol in np_safety_hal.h with no call site.
 *
 * The header records the open question (§ open question 9): dead path, or TX
 * path that was never wired up?  Phase 7 does not answer it — that is a design
 * question about the hub protocol, not about the driver — so this is
 * implemented rather than resolved, as the narrowest thing that is
 * unambiguously correct: stage the 8-byte reply frame through exactly the same
 * path np_hal_spi_send_reply() uses, leaving the remaining window bytes zero.
 *
 * Implementing it costs nothing at runtime — nothing calls it, so
 * -Wl,--gc-sections drops it from the image — and it buys the one thing the
 * header says is missing: the symbol is now BOUND by a definition, so its
 * signature is checked by a compiler instead of being "verified by nothing".
 */
void np_hal_spi_send_frame(const np_safety_tx_frame_t *tx)
{
    uint8_t staging[NP_SAFETY_RX_EXT_FRAME_LEN];

    if (tx == NULL) {
        return;
    }
    memset(staging, 0, sizeof(staging));
    memcpy(staging, tx, sizeof(*tx));
    np_hal_spi_send_reply(staging, (uint8_t)sizeof(staging));
}
