/*
 * NeurOne Firmware-in-the-Loop Simulator — EEG front-end seams (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 * HOST ONLY; see np_sim_bus.h.
 *
 * Defines the eight np_mod_eeg_hal_* seams of np_sw02_platform_hal.h for the ADS1299.  np_mod_eeg.c is the
 * device's own source; this file is the silicon.
 *
 * The one device here with state the driver reads back is the register file, so the model is a register file: a
 * write lands in it, a read returns it, and the ID register reads the value the scenario gave the part.  The driver's
 * configuration (gain per channel, the powered-down channels, the sample rate) is therefore visible as what it
 * wrote, not as a flag the simulator computed.
 *
 *   ID register        the value the scenario supplies.  The ADS1299 datasheet gives 0x3E (ID[4:0] = 11110b), and
 *                      np_mod_eeg_detect() accepts exactly ID[4:0] = 0x1E; a part that is absent reads 0.
 *   impedance, band    the values the scenario sets per channel and per band.  No default: the layer invents no
 *                      reading a scenario would then take for a measurement (CLAUDE.md §19).
 *   dominant band      the first band with the highest power among the five the scenario set.  The seam says
 *                      "dominant" and nothing else; this is the obvious reading of it, and the gap list says so.
 *
 * What an injected response means here.  Three of the eight seams return nothing or a byte, so a response code
 * cannot be carried back.  An armed response makes the CALL not happen: a write is dropped, a start is
 * ignored, a calibration does not run, and a read returns 0.  Each is reported as injected with the code.
 * A stop returns the code (OI-SIMFW-16), so the driver sees the lost stop.
 */
#include "np_sim_bus.h"

#include "np_sw02_platform_hal.h"

#include <math.h>
#include <stdio.h>

#define EEG_REGS     32U
#define EEG_CHANNELS 8U
#define EEG_BANDS    5U

static np_sim_module_t *resolve(const char *op_name)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_EEG, 0U);
    if (m == NULL || !m->present) {
        np_sim_report_absent(NP_SIM_KIND_EEG, 0U, op_name);
        return NULL;
    }
    return m;
}

void np_mod_eeg_hal_spi_write_reg(uint8_t reg, uint8_t val)
{
    np_sim_module_t *m = resolve("eeg_spi_write");
    if (m == NULL) { return; }
    char args[32];
    (void)snprintf(args, sizeof args, "[%u,%u]", reg, val);
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_EEG_SPI_WRITE, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_EEG_SPI_WRITE, args, code, "injected");
        return;
    }
    if (reg < EEG_REGS && reg != 0U) { m->eeg_regs[reg] = val; }     /* the ID register is read-only */
    np_sim_report_hal(m, NP_SIM_OP_EEG_SPI_WRITE, args, (int)NP_HUB_OK, "default");
}

uint8_t np_mod_eeg_hal_spi_read_reg(uint8_t reg)
{
    np_sim_module_t *m = resolve("eeg_spi_read");
    if (m == NULL) { return 0U; }
    char args[32];
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_EEG_SPI_READ, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_EEG_SPI_READ, "[]", code, "injected");
        return 0U;
    }
    const np_sim_override_t *o = &m->ov[NP_SIM_OP_EEG_SPI_READ];
    const uint8_t v = o->has_reading ? (uint8_t)o->reading : (reg < EEG_REGS ? m->eeg_regs[reg] : 0U);
    (void)snprintf(args, sizeof args, "[%u,%u]", reg, v);
    np_sim_report_hal(m, NP_SIM_OP_EEG_SPI_READ, args, (int)NP_HUB_OK, o->has_reading ? "injected" : "default");
    return v;
}

void np_mod_eeg_hal_start_continuous(void)
{
    np_sim_module_t *m = resolve("eeg_start");
    if (m == NULL) { return; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_EEG_START, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_EEG_START, "[]", code, "injected");
        return;
    }
    m->eeg_running = true;
    np_sim_report_hal(m, NP_SIM_OP_EEG_START, "[]", (int)NP_HUB_OK, "default");
}

np_hub_status_t np_mod_eeg_hal_stop_continuous(void)
{
    np_sim_module_t *m = resolve("eeg_stop");
    if (m == NULL) { return NP_HUB_ERR_NOT_PRESENT; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_EEG_STOP, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_EEG_STOP, "[]", code, "injected");
        return (np_hub_status_t)code;       /* a lost stop: the converter keeps running */
    }
    m->eeg_running = false;
    np_sim_report_hal(m, NP_SIM_OP_EEG_STOP, "[]", (int)NP_HUB_OK, "default");
    return NP_HUB_OK;
}

float np_mod_eeg_hal_read_impedance(uint8_t ch)
{
    np_sim_module_t *m = resolve("eeg_read_impedance");
    if (m == NULL || ch >= EEG_CHANNELS) { return NAN; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_EEG_READ_IMPEDANCE, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_EEG_READ_IMPEDANCE, "[]", code, "injected");
        return NAN;
    }
    const np_sim_override_t *o = &m->ov[NP_SIM_OP_EEG_READ_IMPEDANCE];
    const float v = o->has_reading ? o->reading : m->eeg_impedance_kohm[ch];
    char args[40];
    (void)snprintf(args, sizeof args, "[%u,%.1f]", ch, (double)v);
    np_sim_report_hal(m, NP_SIM_OP_EEG_READ_IMPEDANCE, args, (int)NP_HUB_OK, o->has_reading ? "injected" : "default");
    return v;
}

float np_mod_eeg_hal_get_band_power(uint8_t band)
{
    np_sim_module_t *m = resolve("eeg_band_power");
    if (m == NULL || band >= EEG_BANDS) { return NAN; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_EEG_BAND_POWER, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_EEG_BAND_POWER, "[]", code, "injected");
        return NAN;
    }
    const np_sim_override_t *o = &m->ov[NP_SIM_OP_EEG_BAND_POWER];
    const float v = o->has_reading ? o->reading : m->eeg_band_db[band];
    char args[40];
    (void)snprintf(args, sizeof args, "[%u,%.1f]", band, (double)v);
    np_sim_report_hal(m, NP_SIM_OP_EEG_BAND_POWER, args, (int)NP_HUB_OK, o->has_reading ? "injected" : "default");
    return v;
}

uint8_t np_mod_eeg_hal_get_dominant_band(void)
{
    np_sim_module_t *m = resolve("eeg_dominant_band");
    if (m == NULL) { return 0U; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_EEG_DOMINANT_BAND, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_EEG_DOMINANT_BAND, "[]", code, "injected");
        return 0U;
    }
    const np_sim_override_t *o = &m->ov[NP_SIM_OP_EEG_DOMINANT_BAND];
    uint8_t best = 0U;
    for (uint8_t b = 1U; b < EEG_BANDS; b++) {
        if (m->eeg_band_db[b] > m->eeg_band_db[best]) { best = b; }
    }
    const uint8_t v = o->has_reading ? (uint8_t)o->reading : best;
    char args[24];
    (void)snprintf(args, sizeof args, "[%u]", v);
    np_sim_report_hal(m, NP_SIM_OP_EEG_DOMINANT_BAND, args, (int)NP_HUB_OK, o->has_reading ? "injected" : "default");
    return v;
}

void np_mod_eeg_hal_self_calibrate(void)
{
    np_sim_module_t *m = resolve("eeg_self_calibrate");
    if (m == NULL) { return; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_EEG_SELF_CALIBRATE, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_EEG_SELF_CALIBRATE, "[]", code, "injected");
        return;                             /* the calibration did not run */
    }
    m->eeg_calibrations++;
    np_sim_report_hal(m, NP_SIM_OP_EEG_SELF_CALIBRATE, "[]", (int)NP_HUB_OK, "default");
}
