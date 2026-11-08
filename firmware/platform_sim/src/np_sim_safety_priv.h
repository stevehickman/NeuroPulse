/*
 * NeurOne Firmware-in-the-Loop Simulator — safety MCU, internal seam between its two halves (NP-SIM-FW-001 §22)
 * HOST ONLY.  np_sim_safety_hal.c includes the safety MCU's headers and np_sim_safety_link.c the hub's; the two name some of the
 * same wire constants through different include paths, so they meet here in plain types.
 */
#ifndef NP_SIM_SAFETY_PRIV_H
#define NP_SIM_SAFETY_PRIV_H

#include <stdbool.h>
#include <stdint.h>

/* One SPI transfer, hub side to the MCU.  `tx` is `len` bytes the hub clocked out; `rx` is `len` bytes it clocked in.  Returns false
 * when the transfer did not happen (MCU in reset, or an injected timeout). */
bool np_sim_safety_spi_exchange(const uint8_t *tx, uint16_t len, uint8_t *rx);

/* The bus functions this half uses, declared as plain C: np_sim_bus.h pulls in np_hub_types.h, whose np_safety_rx_frame_t and
 * np_safety_tx_frame_t are the hub's view of the same wire and conflict with the safety MCU's own (np_safety_protocol.h). */
uint32_t np_sim_now_ms(void);
void     np_sim_advance_ms(uint32_t ms);
void     np_sim_emit(const char *fmt, ...);
void     np_sim_set_tick_hook(void (*fn)(void));

#endif
