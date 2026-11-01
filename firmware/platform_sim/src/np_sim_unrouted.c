/*
 * NeurOne Firmware-in-the-Loop Simulator — seams with no route yet
 * NP-SIM-FW-001 Rev 2 §3.2, REQ-SIMFW-02.  HOST ONLY.
 *
 * Every platform symbol the spike's link needs that the PBM socket path does
 * not use.  Each reports itself as "unrouted" and returns a failure, so a
 * scenario that reaches one sees it rather than a plausible answer
 * (np_platform_trap.h's argument, applied to a layer that must not halt).
 * REQ-SIMFW-02's census will replace this file with a route for each seam.
 */
#include "np_sim_bus.h"

#include "np_sw02_platform_hal.h"
#include "np_pbm_hal.h"
#include "np_safety_spi.h"

static void unrouted(const char *symbol)
{
    np_sim_emit("{\"ev\":\"unrouted\",\"t\":%u,\"symbol\":\"%s\"}",
                (unsigned)np_sim_now_ms(), symbol);
}

/* retired zone-slot PBM seams (np_mod_pbm.c slot path) */
np_hub_status_t np_mod_pbm_hal_pwm_set(uint8_t slot, uint8_t a, uint8_t b, uint8_t f, uint8_t d)
{ (void)slot; (void)a; (void)b; (void)f; (void)d; unrouted(__func__); return NP_HUB_ERR_GENERIC; }
float np_mod_pbm_hal_ntc_read(uint8_t slot)
{ (void)slot; unrouted(__func__); return 1000.0f; /* reads as too hot: fail closed */ }
uint16_t np_mod_pbm_hal_pd_read(uint8_t slot, uint8_t wl)
{ (void)slot; (void)wl; unrouted(__func__); return 0U; }
np_hub_status_t np_mod_pbm_hal_socket_selftest_pd1(uint16_t s, uint8_t wl, uint16_t *c)
{ (void)s; (void)wl; (void)c; unrouted(__func__); return NP_HUB_ERR_GENERIC; }

/* smart-tile register tunnel and detection (firmware/pbm) */
bool np_pbm_hal_adc_read_zone_id(uint8_t slot, uint16_t *c)
{ (void)slot; (void)c; unrouted(__func__); return false; }
bool np_pbm_hal_adc_read_pd(uint8_t slot, uint8_t ch, uint16_t *c)
{ (void)slot; (void)ch; (void)c; unrouted(__func__); return false; }
np_pbm_status_t np_pbm_hal_i2c_mux_enable(uint8_t slot, bool en)
{ (void)slot; (void)en; unrouted(__func__); return NP_PBM_ERR_I2C_WRITE; }
np_pbm_status_t np_pbm_hal_i2c_write(uint8_t slot, uint8_t reg, const uint8_t *d, uint8_t n)
{ (void)slot; (void)reg; (void)d; (void)n; unrouted(__func__); return NP_PBM_ERR_I2C_WRITE; }
np_pbm_status_t np_pbm_hal_i2c_read(uint8_t slot, uint8_t reg, uint8_t *d, uint8_t n)
{ (void)slot; (void)reg; (void)d; (void)n; unrouted(__func__); return NP_PBM_ERR_I2C_WRITE; }
bool np_pbm_hal_i2c_probe(uint8_t slot, uint8_t addr, uint32_t t)
{ (void)slot; (void)addr; (void)t; unrouted(__func__); return false; }
np_pbm_status_t np_pbm_hal_tia_gain_set(uint8_t slot, np_tia_gain_t g)
{ (void)slot; (void)g; unrouted(__func__); return NP_PBM_ERR_I2C_WRITE; }
void np_pbm_hal_shdr_log_fault(const np_pbm_shdr_fault_entry_t *e)
{ (void)e; unrouted(__func__); }
void np_pbm_hal_zone_announce(uint8_t slot) { (void)slot; unrouted(__func__); }
uint32_t np_pbm_hal_now_ms(void) { return np_sim_now_ms(); }

/* SPI to the safety MCU: no safety MCU is in the loop in the spike. */
np_hub_status_t np_safety_hal_spi_transfer(const uint8_t *tx, uint8_t *rx, uint8_t len)
{ (void)tx; (void)rx; (void)len; unrouted(__func__); return NP_HUB_ERR_TIMEOUT; }
