# Vendored FreeRTOS-Kernel — NeurOne integration notes

FreeRTOS-Kernel **V11.3.0** (FreeRTOS-LTS `202604.00-LTS`), the RTOS for the
**SW-02 main processor** (NXP i.MX RT1062, Cortex-M7 @ 600 MHz). IEC 62304
Class B SOUP — the formal record is `VERSION` and NP-SW-001 §9.4.

## Why vendored (not a submodule)

This mirrors the existing `firmware/crypto/vendor/monocypher` pattern: a
byte-exact, in-tree copy of the exact upstream tag gives a reproducible,
change-controlled SOUP artefact for the DHF. The upstream lives at
`https://github.com/FreeRTOS/FreeRTOS-LTS`; only the subset SW-02 needs is
vendored (see `VERSION`).

## Layout

```
vendor/freertos/
├── VERSION                      SOUP record
├── LICENSE.md                   MIT (upstream)
├── README-NEURONE.md            this file
├── CMakeLists.txt               np_freertos (cross) + smoke test (host)
├── kernel/                      tasks/list/queue/event_groups/timers/stream_buffer + include/
├── portable/
│   ├── GCC/ARM_CM7/r0p1/        SHIPPED target port (port.c, portmacro.h)
│   ├── MemMang/heap_4.c         SHIPPED heap manager
│   └── ThirdParty/GCC/Posix/    HOST-TEST-ONLY port (never in the device image)
└── test/np_freertos_smoke_tests.c
```

The application config and hooks are **not** here — they are first-party code in
the hub control program:

- `firmware/hub_control/include/FreeRTOSConfig.h` — kernel tuning (600 MHz, 1 ms
  tick, priorities 1–5, heap_4 64 KiB, stack-overflow + malloc-failed hooks,
  `configASSERT` → `np_freertos_assert_failed`, Cortex-M NVIC priority setup, and
  handler aliases `SVC_Handler`/`PendSV_Handler`/`SysTick_Handler`).
- `firmware/hub_control/src/np_hub_freertos_hooks.c` — the three hooks. They fail
  safe by halting the main processor, which stops the SPI heartbeat so the
  STM32G071 safety MCU independently cuts all stimulation (<50 ms).

## Verification

Host (no ARM toolchain needed) — compiles the real kernel + our config + hooks
against the POSIX port and runs the scheduler:

```
cmake -B build/host-tests -DNP_BUILD_TESTS=ON firmware
cmake --build build/host-tests
ctest --test-dir build/host-tests -R np_freertos_smoke_tests --output-on-failure
```

## Remaining for on-target bring-up (NOT FreeRTOS-kernel work)

`np_freertos` is wired into the cross build, but producing a runnable device
image still needs the platform layer, which is separate from the kernel:

1. NXP MCUXpresso SDK for MIMXRT1062 (CMSIS device header, clock init) added to
   `NP_PLATFORM_INCLUDE_DIRS` (see the root `firmware/CMakeLists.txt`).
2. Startup / vector table naming `SVC_Handler`, `PendSV_Handler`, `SysTick_Handler`
   (already aliased in `FreeRTOSConfig.h`).
3. A linker script placing the FreeRTOS heap and task stacks in SRAM/LPSDR4, and
   a final linked application target that links `np_freertos` + `np_hub_control`
   + the module libraries + startup.
4. The platform HAL stubs the hub tasks call (SPI, transport, module drivers,
   eMMC log backing) — tracked separately in the hub `OI-*` items.
