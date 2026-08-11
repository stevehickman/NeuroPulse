/**
 * NeurOne Safety MCU — Startup Code
 * Target: STM32G071 (Cortex-M0+)
 * Document: NP-SW-001 Rev A — SW-01 Class C
 *
 * Minimal startup: copy .data from Flash, zero .bss (excluding fault_latch
 * region), then branch to main.
 *
 * SystemInit IS NOT CALLED, AND THAT IS DELIBERATE (closes OI-SWCI-19).
 * Until phase 7 this comment claimed a SystemInit call that the code below has
 * never made; the code was the record, and OI-SWCI-19 flagged the discrepancy
 * rather than let either side be "corrected" silently, because the two possible
 * fixes were materially different decisions — delete four words, or add a call
 * to clock-init code that did not exist yet.
 *
 * Phase 7 resolves it in the direction the code already chose: clock bring-up
 * is np_hal_clock_init(), which np_safety_main.c calls as its FIRST statement,
 * before every other HAL call and every module init.  ST's system_stm32g0xx.c
 * is therefore still not vendored (consistent with OI-SWCI-06, headers only) —
 * and would not have helped: ST's SystemInit does not configure the PLL, so it
 * would leave the core at HSI16 and the 64 MHz claim unmet.
 *
 * CONSEQUENCE: the code between reset and main() runs at HSI16 = 16 MHz.
 * Nothing here is timing-dependent (a .data copy, a .bss zero, one branch), but
 * anything ADDED to Reset_Handler will run at 16 MHz, not 64 MHz.
 *
 * The fault_latch section at the top of SRAM is intentionally NOT zeroed
 * on warm reset — SW01-M08 reads it to detect prior fault state.
 */

    .syntax unified
    .cpu    cortex-m0plus
    .thumb

/* External symbols from linker script */
    .extern _sidata
    .extern _sdata
    .extern _edata
    .extern _sbss
    .extern _ebss
    .extern _estack

/* ── Vector table ─────────────────────────────────────────────────────────── */
    .section .isr_vector, "a", %progbits
    .type g_pfnVectors, %object

g_pfnVectors:
    .word  _estack                     /* Initial stack pointer */
    .word  Reset_Handler               /* Reset handler */
    .word  NMI_Handler                 /* NMI */
    .word  HardFault_Handler           /* Hard fault */
    .word  0                           /* Reserved */
    .word  0                           /* Reserved */
    .word  0                           /* Reserved */
    .word  0                           /* Reserved */
    .word  0                           /* Reserved */
    .word  0                           /* Reserved */
    .word  0                           /* Reserved */
    .word  SVC_Handler                 /* SVCall */
    .word  0                           /* Reserved */
    .word  0                           /* Reserved */
    .word  PendSV_Handler              /* PendSV */
    .word  SysTick_Handler             /* SysTick */
    /* External interrupts (32 for STM32G071) */
    .word  WWDG_IRQHandler
    .word  PVD_IRQHandler
    .word  RTC_TAMP_IRQHandler
    .word  FLASH_IRQHandler
    .word  RCC_IRQHandler
    .word  EXTI0_1_IRQHandler
    .word  EXTI2_3_IRQHandler
    .word  EXTI4_15_IRQHandler
    .word  0
    .word  DMA1_Channel1_IRQHandler
    .word  DMA1_Channel2_3_IRQHandler
    .word  DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler
    .word  ADC1_COMP_IRQHandler
    .word  TIM1_BRK_UP_TRG_COM_IRQHandler
    .word  TIM1_CC_IRQHandler
    .word  TIM2_IRQHandler
    .word  TIM3_IRQHandler
    .word  TIM6_DAC_LPTIM1_IRQHandler
    .word  TIM7_LPTIM2_IRQHandler
    .word  TIM14_IRQHandler
    .word  TIM15_IRQHandler
    .word  TIM16_IRQHandler
    .word  TIM17_IRQHandler
    .word  I2C1_IRQHandler
    .word  I2C2_IRQHandler
    .word  SPI1_IRQHandler
    .word  SPI2_IRQHandler
    .word  USART1_IRQHandler
    .word  USART2_IRQHandler
    .word  USART3_4_LPUART1_IRQHandler
    .word  CEC_IRQHandler
    .word  AES_RNG_IRQHandler

    .size g_pfnVectors, .-g_pfnVectors

/* ── Reset handler ────────────────────────────────────────────────────────── */
    .section .text.Reset_Handler
    .weak Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    /* Copy .data from Flash to SRAM */
    ldr  r0, =_sdata
    ldr  r1, =_edata
    ldr  r2, =_sidata
    movs r3, #0
    b    data_copy_test
data_copy_loop:
    ldr  r4, [r2, r3]
    str  r4, [r0, r3]
    adds r3, r3, #4
data_copy_test:
    adds r4, r0, r3
    cmp  r4, r1
    bcc  data_copy_loop

    /* Zero .bss (does NOT touch .fault_latch) */
    ldr  r2, =_sbss
    ldr  r4, =_ebss
    movs r3, #0
    b    bss_zero_test
bss_zero_loop:
    str  r3, [r2]
    adds r2, r2, #4
bss_zero_test:
    cmp  r2, r4
    bcc  bss_zero_loop

    /* Call main */
    bl   main
    /* Should never return; loop if it does */
infinite_loop:
    b    infinite_loop

    .size Reset_Handler, .-Reset_Handler

/* ── Default weak handlers ────────────────────────────────────────────────── */
    .macro WEAK_IRQ name
    .section .text.\name
    .weak \name
    .type \name, %function
\name:
    b .
    .size \name, .-\name
    .endm

    WEAK_IRQ NMI_Handler
    WEAK_IRQ HardFault_Handler
    WEAK_IRQ SVC_Handler
    WEAK_IRQ PendSV_Handler
    WEAK_IRQ SysTick_Handler
    WEAK_IRQ WWDG_IRQHandler
    WEAK_IRQ PVD_IRQHandler
    WEAK_IRQ RTC_TAMP_IRQHandler
    WEAK_IRQ FLASH_IRQHandler
    WEAK_IRQ RCC_IRQHandler
    WEAK_IRQ EXTI0_1_IRQHandler
    WEAK_IRQ EXTI2_3_IRQHandler
    WEAK_IRQ EXTI4_15_IRQHandler
    WEAK_IRQ DMA1_Channel1_IRQHandler
    WEAK_IRQ DMA1_Channel2_3_IRQHandler
    WEAK_IRQ DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler
    WEAK_IRQ ADC1_COMP_IRQHandler
    WEAK_IRQ TIM1_BRK_UP_TRG_COM_IRQHandler
    WEAK_IRQ TIM1_CC_IRQHandler
    WEAK_IRQ TIM2_IRQHandler
    WEAK_IRQ TIM3_IRQHandler
    WEAK_IRQ TIM6_DAC_LPTIM1_IRQHandler
    WEAK_IRQ TIM7_LPTIM2_IRQHandler
    WEAK_IRQ TIM14_IRQHandler
    WEAK_IRQ TIM15_IRQHandler
    WEAK_IRQ TIM16_IRQHandler
    WEAK_IRQ TIM17_IRQHandler
    WEAK_IRQ I2C1_IRQHandler
    WEAK_IRQ I2C2_IRQHandler
    WEAK_IRQ SPI1_IRQHandler
    WEAK_IRQ SPI2_IRQHandler
    WEAK_IRQ USART1_IRQHandler
    WEAK_IRQ USART2_IRQHandler
    WEAK_IRQ USART3_4_LPUART1_IRQHandler
    WEAK_IRQ CEC_IRQHandler
    WEAK_IRQ AES_RNG_IRQHandler
    WEAK_IRQ TIM6_IRQHandler
