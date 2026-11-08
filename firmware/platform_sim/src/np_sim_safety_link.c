/*
 * NeurOne Firmware-in-the-Loop Simulator — the hub's SPI link to the safety MCU (NP-SIM-FW-001 §22, REQ-SIMFW-14)
 * HOST ONLY.  Defines np_safety_hal_spi_transfer, the hub-side seam np_safety_spi.c calls for every frame.  With no safety MCU
 * attached it is what it was before: an "unrouted" event and a timeout, so a scenario that reaches it sees the absence.
 */
#include "np_sim_bus.h"
#include "np_sim_safety.h"
#include "np_sim_safety_priv.h"

#include "np_safety_spi.h"

#include <string.h>

np_hub_status_t np_safety_hal_spi_transfer(const uint8_t *tx, uint8_t *rx, uint8_t len)
{
    if (!np_sim_safety_attached()) {
        np_sim_emit("{\"ev\":\"unrouted\",\"t\":%u,\"symbol\":\"np_safety_hal_spi_transfer\"}", (unsigned)np_sim_now_ms());
        return NP_HUB_ERR_TIMEOUT;
    }
    if (tx == NULL || rx == NULL) { return NP_HUB_ERR_TIMEOUT; }
    return np_sim_safety_spi_exchange(tx, len, rx) ? NP_HUB_OK : NP_HUB_ERR_TIMEOUT;
}
