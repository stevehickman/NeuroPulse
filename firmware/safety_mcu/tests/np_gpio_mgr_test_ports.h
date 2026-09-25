/*
 * NeurOne Safety MCU — test-only GPIO port identities for np_gpio_mgr_tests
 *
 * Force-included (-include) into the np_gpio_mgr_tests target ONLY, so that
 * np_safety_config.h's NP_EN_*_PORT macros (which expand to GPIOA / GPIOB)
 * resolve to two distinct host addresses without editing the Class C source.
 * The unit under test only ever passes a port through to
 * np_hal_gpio_write_pin(void *port, ...), so an opaque address is all it needs.
 *
 * The cross build never sees this file: it is referenced from one host-test
 * target in firmware/safety_mcu/CMakeLists.txt, below the NP_BUILD_TESTS gate.
 */
#ifndef NP_GPIO_MGR_TEST_PORTS_H
#define NP_GPIO_MGR_TEST_PORTS_H

#if defined(STM32G071xx)
#  error "np_gpio_mgr_test_ports.h is host-test only"
#endif

extern int np_test_port_a;
extern int np_test_port_b;

#define GPIOA  ((void *)&np_test_port_a)
#define GPIOB  ((void *)&np_test_port_b)

#endif /* NP_GPIO_MGR_TEST_PORTS_H */
