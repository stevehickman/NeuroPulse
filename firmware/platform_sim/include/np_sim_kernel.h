/*
 * NeurOne Firmware-in-the-Loop Simulator — the hub's task layer on a virtual clock (NP-SIM-FW-001 §31, OI-SIMFW-45, OI-SIMFW-50)
 * HOST ONLY.
 *
 * WHAT A TASK, A TICK AND A TIMEOUT MEAN ON THE HOST (the decision of this item, UC-124):
 *
 *   - A task is the device's own task function (np_hub_control_main.c's five), run on a host thread.  Exactly one thread runs at a
 *     time, and a switch happens only at a kernel call (a delay, a wait for an event or a semaphore, a wake of a higher-priority
 *     task).  So a run is repeatable, and no task is preempted in the middle of a statement: the device preempts on a tick, this
 *     does not (a task that never calls the kernel would hang the twin; the hub's tasks all block).
 *   - A tick is one virtual millisecond (configTICK_RATE_HZ is 1000).  Virtual time moves only inside np_sim_kernel_run_ms(), and
 *     only when every task is blocked: it jumps to the earliest wake (a delay's end, an event wait's timeout) and moves the clock
 *     there through np_sim_advance_ms_raw(), a millisecond at a time while the safety MCU's loop is attached (np_sim_set_tick_hook),
 *     so the MCU sees every millisecond.  A 200 ms heartbeat is 200 virtual ms, whatever the host's load.
 *   - A timeout is a deadline on that clock.  Nothing here is the FreeRTOS kernel: the scheduler is this file's model of the calls
 *     the hub makes (priorities are honoured, round-robin among equals only at a block, priority inheritance is NOT modelled).
 *     The FreeRTOS POSIX port is not used, because it ticks on the host's wall clock, so its timing could not be repeated or tied to
 *     the safety MCU's clock domain (REQ-SIMFW-22).
 *
 * What this is NOT: timing evidence.  Nothing says a task meets a deadline on the device (REQ-SIMFW-06).
 *
 * With no scheduler started, the kernel calls keep the one-thread behaviour the scenario suites rely on: a delay advances the clock in
 * the caller, an event wait with nothing posted advances it by the timeout.
 */
#ifndef NP_SIM_KERNEL_H
#define NP_SIM_KERNEL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Run `app_main` (the hub's np_hub_control_app_main) on a boot thread until it calls vTaskStartScheduler().  True when the scheduler
 * started; false if app_main returned or never started it.  Once only. */
bool np_sim_kernel_boot(void (*app_main)(void));

bool np_sim_kernel_running(void);

/* Move virtual time forward `ms` milliseconds, running every task that is ready, in priority order, at each instant.  Returns the
 * virtual ms actually advanced (always `ms` once the scheduler is running). */
uint32_t np_sim_kernel_run_ms(uint32_t ms);

/* Introspection, for the status event and the tests. */
unsigned    np_sim_kernel_task_count(void);
const char *np_sim_kernel_task_name(unsigned i);
unsigned    np_sim_kernel_task_prio(unsigned i);
const char *np_sim_kernel_task_state(unsigned i);   /* "ready", "running", "delay", "event", "semaphore" or "done" */
uint32_t    np_sim_kernel_context_switches(void);

#ifdef __cplusplus
}
#endif

#endif /* NP_SIM_KERNEL_H */
