/*
 * FreeRTOSConfig.h — NeurOne Hub Control (SW-02, main processor)
 * Document: NP-FW-HUB-001, NP-SW-001 §9.4 (SOUP: FreeRTOS-Kernel V11.3.0)
 *
 * Target: NXP i.MX RT1062, Cortex-M7 @ 600 MHz, GCC/ARM_CM7/r0p1 port.
 *
 * This is a NeurOne-authored application configuration — NOT vendored kernel
 * source. It is the single source of truth for kernel tuning on SW-02.
 *
 * Host verification: the host smoke test (firmware/vendor/freertos, built with
 * -DNP_BUILD_TESTS=ON) compiles this SAME file against the POSIX port with
 * NP_FREERTOS_POSIX_HOST defined, so the configuration is exercised natively.
 * Sections that are meaningful only on Cortex-M are guarded out for the host.
 *
 * IEC 62304: SW-02 is Class B. Stack-overflow detection and malloc-failed
 * detection are enabled (NP-SW-001 §9.4 SOUP verification claim). Real-time
 * task creation happens once at startup (np_hub_control_app_main); no heap
 * allocation occurs on the safety-critical heartbeat path.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ── Scheduler ─────────────────────────────────────────────────────────────── */
#define configUSE_PREEMPTION                        1
#define configUSE_TIME_SLICING                      1
#define configIDLE_SHOULD_YIELD                     1
#define configUSE_TICKLESS_IDLE                     0

/* CLZ-based task selection is a Cortex-M optimisation; the POSIX port has no
 * equivalent, so fall back to the generic selection for the host build. */
#if defined( NP_FREERTOS_POSIX_HOST )
    #define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#else
    #define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#endif

/* ── Clocks and ticks ──────────────────────────────────────────────────────── */
#if defined( NP_FREERTOS_POSIX_HOST )
    #define configCPU_CLOCK_HZ                      ( 1000000000UL ) /* ignored by POSIX port */
#else
    #define configCPU_CLOCK_HZ                      ( 600000000UL )  /* i.MX RT1062 core clock */
#endif
#define configTICK_RATE_HZ                          ( ( TickType_t ) 1000 )  /* 1 ms tick */
#define configTICK_TYPE_WIDTH_IN_BITS               TICK_TYPE_WIDTH_32_BITS

/* ── Priorities ────────────────────────────────────────────────────────────── */
/* Hub tasks use priorities 1..4 (see np_hub_config.h NP_HUB_TASK_PRIO_*).
 * The software-timer service task sits above them at configMAX_PRIORITIES-1. */
#define configMAX_PRIORITIES                        ( 6 )
#define configMINIMAL_STACK_SIZE                    ( ( configSTACK_DEPTH_TYPE ) 128 )
#define configMAX_TASK_NAME_LEN                     ( 16 )

/* ── Memory allocation ─────────────────────────────────────────────────────── */
#define configSUPPORT_STATIC_ALLOCATION             0
#define configSUPPORT_DYNAMIC_ALLOCATION            1
#define configAPPLICATION_ALLOCATED_HEAP            0
#define configTOTAL_HEAP_SIZE                       ( ( size_t ) ( 64 * 1024 ) )
#define configSTACK_DEPTH_TYPE                      uint32_t
#define configMESSAGE_BUFFER_LENGTH_TYPE            size_t

/* ── Synchronisation primitives ────────────────────────────────────────────── */
#define configUSE_MUTEXES                           1
#define configUSE_RECURSIVE_MUTEXES                 1
#define configUSE_COUNTING_SEMAPHORES               1
#define configUSE_QUEUE_SETS                        0
#define configQUEUE_REGISTRY_SIZE                   8
#define configUSE_TASK_NOTIFICATIONS                1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES       3
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS     0

/* ── Software timers ───────────────────────────────────────────────────────── */
/* event_groups.c references xTimerPendFunctionCall; keep timers enabled. */
#define configUSE_TIMERS                            1
#define configTIMER_TASK_PRIORITY                   ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                    10
#define configTIMER_TASK_STACK_DEPTH                ( 256 )

/* ── Co-routines (deprecated; sources not vendored) ────────────────────────── */
#define configUSE_CO_ROUTINES                       0
#define configMAX_CO_ROUTINE_PRIORITIES             1

/* ── Hooks / safety instrumentation ────────────────────────────────────────── */
#define configUSE_IDLE_HOOK                         0
#define configUSE_TICK_HOOK                         0
#define configUSE_DAEMON_TASK_STARTUP_HOOK          0
#define configCHECK_FOR_STACK_OVERFLOW              2   /* pattern check on context switch */
#define configUSE_MALLOC_FAILED_HOOK                1

/* ── Run-time stats / trace (off in production) ────────────────────────────── */
#define configGENERATE_RUN_TIME_STATS               0
#define configUSE_TRACE_FACILITY                    0
#define configUSE_STATS_FORMATTING_FUNCTIONS        0

/* ── Misc ──────────────────────────────────────────────────────────────────── */
#define configUSE_NEWLIB_REENTRANT                  0
#define configENABLE_BACKWARD_COMPATIBILITY         0

/* ── configASSERT ──────────────────────────────────────────────────────────── */
/* Routed to a NeurOne handler (firmware/hub_control/src/np_hub_freertos_hooks.c).
 * On target it disables interrupts and halts — the SPI heartbeat then stops and
 * the safety MCU independently cuts all stimulation (NP-FW-HUB-001 §2, watchdog).
 * On host it prints and aborts so ctest reports the failure. */
extern void np_freertos_assert_failed( const char * pcFile, unsigned long ulLine );
#define configASSERT( x ) \
    do { if( ( x ) == 0 ) { np_freertos_assert_failed( __FILE__, (unsigned long) __LINE__ ); } } while( 0 )

/* ── Optional API inclusion ────────────────────────────────────────────────── */
#define INCLUDE_vTaskPrioritySet                    1
#define INCLUDE_uxTaskPriorityGet                   1
#define INCLUDE_vTaskDelete                         1
#define INCLUDE_vTaskSuspend                        1
#define INCLUDE_vTaskDelayUntil                     1
#define INCLUDE_vTaskDelay                          1
#define INCLUDE_xTaskGetSchedulerState              1
#define INCLUDE_xTaskGetCurrentTaskHandle           1
#define INCLUDE_uxTaskGetStackHighWaterMark         1
#define INCLUDE_eTaskGetState                       1
#define INCLUDE_xTimerPendFunctionCall              1
#define INCLUDE_xEventGroupSetBitFromISR            1
#define INCLUDE_xTaskGetIdleTaskHandle              0
#define INCLUDE_xTaskAbortDelay                     0
#define INCLUDE_xTaskGetHandle                      0
#define INCLUDE_xSemaphoreGetMutexHolder            0

/* ── Cortex-M interrupt priority configuration (target only) ───────────────── */
#if !defined( NP_FREERTOS_POSIX_HOST )

    /* i.MX RT1062 NVIC implements 4 priority bits (__NVIC_PRIO_BITS = 4). */
    #define configPRIO_BITS                                 4
    #define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
    #define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    2

    #define configKERNEL_INTERRUPT_PRIORITY \
        ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )
    #define configMAX_SYSCALL_INTERRUPT_PRIORITY \
        ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )

    /* Map the kernel handlers onto the CMSIS / i.MX RT1062 vector-table names,
     * so the startup vector table needs no FreeRTOS-specific aliases. */
    #define vPortSVCHandler                                 SVC_Handler
    #define xPortPendSVHandler                              PendSV_Handler
    #define xPortSysTickHandler                             SysTick_Handler

#endif /* !NP_FREERTOS_POSIX_HOST */

#endif /* FREERTOS_CONFIG_H */
