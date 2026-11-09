/*
 * NeurOne Firmware-in-the-Loop Simulator — constants the safety MCU scenario suites share (NP-SIM-FW-001 §22, §24)
 * HOST TEST CODE ONLY.  Status bits are np_spi_wire_types.h's NP_SAFETY_STATUS_*; line numbers are the order np_gpio_mgr.c writes
 * the enable lines (PBM, BES, tDCS, VNS, visual, intranasal, cervical VNS).  Defined once (scripts/check-firmware-constants.ts).
 */
#ifndef NP_SIM_SAFETY_TEST_CONSTS_H
#define NP_SIM_SAFETY_TEST_CONSTS_H

#define STATUS_THERMAL 0x10U
#define STATUS_CHARGE  0x20U

#define LINE_PBM  0
#define LINE_TDCS 2

#endif /* NP_SIM_SAFETY_TEST_CONSTS_H */
