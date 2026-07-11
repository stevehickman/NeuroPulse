/*
 * np_freertos_smoke_tests.c — host verification of the vendored FreeRTOS kernel
 * Document: NP-SW-001 §9.4 (SOUP verification)
 *
 * Compiles the ACTUAL vendored FreeRTOS-Kernel V11.3.0 sources against NeurOne's
 * FreeRTOSConfig.h (firmware/hub_control/include/FreeRTOSConfig.h, with
 * NP_FREERTOS_POSIX_HOST defined) using the POSIX port, then starts the real
 * scheduler and exercises the exact primitives the hub control program relies on:
 *   - multiple prioritised tasks,
 *   - an event group (xEventGroupSetBits / xEventGroupWaitBits), the mechanism
 *     task_hub_control uses for NP_EV_SESSION_START,
 *   - vTaskDelayUntil (the heartbeat/telemetry cadence primitive).
 *
 * A pass proves the kernel + our configuration + our application hooks build and
 * the scheduler runs on the host. It is NOT a substitute for on-target bring-up
 * (ARM_CM7 port + startup + linker script), which requires the ARM toolchain.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#include <stdio.h>
#include <stdlib.h>

#define BIT_PING            ( ( EventBits_t ) 0x01 )
#define EXPECTED_PINGS      ( 5 )
#define PING_INTERVAL_MS    ( 10 )

static EventGroupHandle_t g_ev;
static volatile int       g_ping_count = 0;
static volatile int       g_pong_count = 0;

/* Consumer: mirrors task_hub_control waiting on NP_EV_SESSION_START. */
static void prvConsumerTask( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        EventBits_t bits = xEventGroupWaitBits( g_ev, BIT_PING,
                                                pdTRUE,  /* clear on exit  */
                                                pdTRUE,  /* wait all bits  */
                                                pdMS_TO_TICKS( 200 ) );
        if( ( bits & BIT_PING ) != 0 )
        {
            g_pong_count++;
        }
        else
        {
            /* Timed out — producer has stopped. */
            vTaskDelete( NULL );
        }
    }
}

/* Producer: mirrors a periodic hub task using vTaskDelayUntil, then verifies
 * and terminates the process deterministically (independent of end-scheduler
 * semantics across ports). */
static void prvProducerTask( void * pvParameters )
{
    ( void ) pvParameters;

    TickType_t xLastWake = xTaskGetTickCount();

    for( int i = 0; i < EXPECTED_PINGS; i++ )
    {
        vTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( PING_INTERVAL_MS ) );
        g_ping_count++;
        ( void ) xEventGroupSetBits( g_ev, BIT_PING );
    }

    /* Let the consumer drain the last set bit. */
    vTaskDelay( pdMS_TO_TICKS( 100 ) );

    if( ( g_ping_count == EXPECTED_PINGS ) && ( g_pong_count >= EXPECTED_PINGS ) )
    {
        printf( "PASS: FreeRTOS V11.3.0 scheduler ran (ping=%d pong=%d)\n",
                g_ping_count, g_pong_count );
        fflush( stdout );
        exit( 0 );
    }

    printf( "FAIL: ping=%d pong=%d (expected >= %d each)\n",
            g_ping_count, g_pong_count, EXPECTED_PINGS );
    fflush( stdout );
    exit( 1 );
}

int main( void )
{
    g_ev = xEventGroupCreate();

    if( g_ev == NULL )
    {
        printf( "FAIL: xEventGroupCreate returned NULL\n" );
        return 1;
    }

    if( xTaskCreate( prvConsumerTask, "cons", 512, NULL, 2, NULL ) != pdPASS )
    {
        printf( "FAIL: could not create consumer task\n" );
        return 1;
    }

    if( xTaskCreate( prvProducerTask, "prod", 512, NULL, 3, NULL ) != pdPASS )
    {
        printf( "FAIL: could not create producer task\n" );
        return 1;
    }

    vTaskStartScheduler();

    /* Reached only if the scheduler failed to start (e.g. heap too small). */
    printf( "FAIL: vTaskStartScheduler returned unexpectedly\n" );
    return 1;
}
