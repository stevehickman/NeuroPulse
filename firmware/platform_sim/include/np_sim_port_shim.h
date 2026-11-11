/*
 * NeurOne Firmware-in-the-Loop Simulator — what the POSIX kernel port lacks (HOST ONLY)
 * Document: NP-SIM-FW-001 §29 (OI-SIMFW-03).  REQ-SIMFW-01: never built for the device.
 *
 * The vendored FreeRTOS POSIX port (firmware/vendor/freertos/portable/ThirdParty/GCC/Posix, SOUP,
 * not edited) has no xPortIsInsideInterrupt().  The Cortex-M7 port defines it inline in its
 * portmacro.h, and np_hub_control_main.c calls it (log_in_isr).  This header declares it and
 * np_sim_port_shim.c defines it, so a device source that calls it compiles and links in the twin
 * with no change to the source or to the kernel: force-include this header on that source
 * (`-include np_sim_port_shim.h`), as the safety MCU's test ports are.
 *
 * The twin has no interrupt controller.  What it has is the places where the sim layer calls a
 * handler the firmware registered as an interrupt handler (the goggle Hall edge, np_sim_visual.c).
 * Those calls are bracketed with np_sim_isr_enter() and np_sim_isr_exit(), and
 * xPortIsInsideInterrupt() reports whether one is in progress.  A shim that always answered
 * "not in an interrupt" would hide the error the firmware's assert exists to catch: task-only code
 * reached from a handler.
 */
#ifndef NP_SIM_PORT_SHIM_H
#define NP_SIM_PORT_SHIM_H

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

BaseType_t xPortIsInsideInterrupt(void);

/* Bracket a call the sim layer makes to a handler the firmware registered as an interrupt handler. */
void np_sim_isr_enter(void);
void np_sim_isr_exit(void);

#ifdef __cplusplus
}
#endif

#endif /* NP_SIM_PORT_SHIM_H */
