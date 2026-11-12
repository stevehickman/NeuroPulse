/*
 * NeurOne Firmware-in-the-Loop Simulator — what the POSIX kernel port lacks (HOST ONLY)
 * Document: NP-SIM-FW-001 §30 (OI-SIMFW-03).  See include/np_sim_port_shim.h.
 *
 * Nothing here is verification evidence (REQ-SIMFW-06).
 */
#include "np_sim_port_shim.h"

/* A depth and not a flag, so a handler delivered from inside a handler does not clear it early. */
static unsigned g_isr_depth;

void np_sim_isr_enter(void) { g_isr_depth++; }

void np_sim_isr_exit(void)
{
    if (g_isr_depth > 0U) { g_isr_depth--; }
}

BaseType_t xPortIsInsideInterrupt(void) { return (g_isr_depth > 0U) ? pdTRUE : pdFALSE; }
