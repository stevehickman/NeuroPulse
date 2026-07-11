/*
 * np_hub_freertos_hooks.c — FreeRTOS application hooks for SW-02 (main processor)
 * Document: NP-FW-HUB-001, NP-SW-001 §9.4
 *
 * FreeRTOSConfig.h enables:
 *   configCHECK_FOR_STACK_OVERFLOW = 2  → vApplicationStackOverflowHook
 *   configUSE_MALLOC_FAILED_HOOK   = 1  → vApplicationMallocFailedHook
 *   configASSERT( x )                   → np_freertos_assert_failed
 *
 * Fault philosophy (NP-FW-HUB-001 §2): the STM32G071 safety MCU independently
 * owns every stimulation-enable GPIO and runs a 1.5 s SPI-heartbeat watchdog.
 * If the main-processor RTOS faults and stops beating, the safety MCU cuts all
 * stimulation within <50 ms regardless of what the main processor does. These
 * hooks therefore fail safe by halting the main processor: interrupts off, spin.
 * That deliberately stops the heartbeat and hands control to the safety backstop.
 *
 * The same translation unit is compiled into the host smoke test (POSIX port,
 * NP_FREERTOS_POSIX_HOST) where halting is replaced by print + abort() so the
 * failure surfaces to ctest.
 */

#include "FreeRTOS.h"
#include "task.h"

#if defined( NP_FREERTOS_POSIX_HOST )
    #include <stdio.h>
    #include <stdlib.h>
#endif

/* ── configASSERT target ──────────────────────────────────────────────────── */
void np_freertos_assert_failed( const char * pcFile, unsigned long ulLine )
{
#if defined( NP_FREERTOS_POSIX_HOST )
    fprintf( stderr, "FreeRTOS configASSERT failed: %s:%lu\n",
             ( pcFile != NULL ) ? pcFile : "?", ulLine );
    fflush( stderr );
    abort();
#else
    ( void ) pcFile;
    ( void ) ulLine;
    taskDISABLE_INTERRUPTS();
    for( ;; )
    {
        /* Halt. Safety MCU watchdog cuts all stimulation on heartbeat loss. */
    }
#endif
}

/* ── Stack overflow (configCHECK_FOR_STACK_OVERFLOW = 2) ───────────────────── */
void vApplicationStackOverflowHook( TaskHandle_t xTask, char * pcTaskName )
{
#if defined( NP_FREERTOS_POSIX_HOST )
    ( void ) xTask;
    fprintf( stderr, "FreeRTOS stack overflow in task: %s\n",
             ( pcTaskName != NULL ) ? pcTaskName : "?" );
    fflush( stderr );
    abort();
#else
    ( void ) xTask;
    ( void ) pcTaskName;
    taskDISABLE_INTERRUPTS();
    for( ;; )
    {
        /* Halt — hand off to safety MCU backstop. */
    }
#endif
}

/* ── Heap exhaustion (configUSE_MALLOC_FAILED_HOOK = 1) ────────────────────── */
void vApplicationMallocFailedHook( void )
{
#if defined( NP_FREERTOS_POSIX_HOST )
    fprintf( stderr, "FreeRTOS malloc failed (configTOTAL_HEAP_SIZE exhausted)\n" );
    fflush( stderr );
    abort();
#else
    taskDISABLE_INTERRUPTS();
    for( ;; )
    {
        /* Halt — all task/queue allocation happens at startup, so reaching here
         * means configTOTAL_HEAP_SIZE is mis-sized and the device must not run. */
    }
#endif
}
