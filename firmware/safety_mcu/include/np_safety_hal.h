/*
 * NeurOne Safety MCU — SW-01 Platform (HAL) Contract
 * Target: STM32G071 (Cortex-M0+, 64 MHz), bare metal
 * Document: NP-SW-001 Rev 1 — SW-01 Class C; NP-SW-CI-001 §4.4 (Defect E)
 *
 * ┌──────────────────────────────────────────────────────────────────────────┐
 * │  IMPLEMENTED AT PHASE 7 — firmware/safety_mcu/platform/  (OI-SWCI-17)    │
 * │                                                                          │
 * │  This file remains DECLARATION-ONLY (rule 3 below), but the declarations  │
 * │  are no longer unbacked.  STM32G071 register drivers for clock/timebase, │
 * │  GPIO, ADC, SPI slave, TIM2 + R-peak capture, impedance and OTP now live  │
 * │  in platform/, written against vendored CMSIS device headers with no ST   │
 * │  HAL or LL (OI-SWCI-06).  `np_safety_mcu.elf` LINKS; the gated CI target  │
 * │  is the image, not `np_safety_mcu_objs`.                                  │
 * │                                                                          │
 * │  The warning that stood here still governs, and phase 7 was built to      │
 * │  satisfy it rather than to route around it: a linkable image made of      │
 * │  stubs is worse than no image.  So the drivers touch real registers, and  │
 * │  the behavioural claims this header could not enforce are enforced by     │
 * │  np_hal_platform_tests — which compiles the REAL driver sources against a │
 * │  plain-C register file and asserts on what they write.  Inverting the     │
 * │  enable polarity now fails a test instead of shipping.                    │
 * │                                                                          │
 * │  WHAT PHASE 7 DID NOT DO: bench validation.  No hardware exists (design   │
 * │  phase, no tooling committed), so nothing here has been measured on       │
 * │  silicon.  Register sequences, the NTC pin map, and the entire impedance  │
 * │  analog front end are DESIGN-REVIEW ARTIFACTS awaiting the G1 gate —      │
 * │  OI-SWCI-27..OI-SWCI-34.  Read "it links and its unit tests pass" as      │
 * │  exactly that, and no further.                                            │
 * └──────────────────────────────────────────────────────────────────────────┘
 *
 * WHY THIS FILE EXISTS (closes OI-SWCI-18)
 * ----------------------------------------
 * Before it, each of these 25 symbols was declared `extern` at the top of
 * whichever module called it, and defined again as a test double inside each
 * host-test file that needed it.  `np_hal_get_tick_ms` alone existed as NINE
 * independent copies of one contract — five declarations in src/, four
 * definitions in tests/.  They agreed, but nothing enforced that they keep
 * agreeing, and a mismatch between an `extern` declaration and the real
 * definition is undefined behaviour the compiler cannot see across translation
 * units.  Same defect shape as Defect C (§4.3): one value, several places,
 * kept equal by hand.
 *
 * Every consumer — production module and host-test double alike — now includes
 * this header, so a signature change here is a COMPILE ERROR at both ends
 * rather than a silently mislinked call.  Change a contract here, not there.
 *
 * WHAT IS AND IS NOT ENFORCED — read this before trusting the file
 * ----------------------------------------------------------------
 * What is single-sourced and compiler-checked is the ABI: return type,
 * parameter types, arity.  What is NOT checked by anything is the BEHAVIOURAL
 * contract below — units, polarity, blocking, buffer layout.  Those are prose.
 * A driver that implements np_hal_gpio_write_pin() with 1 = ENABLED has an
 * identical signature, compiles clean, links clean, and inverts every
 * stimulation enable line.  Prose is a design-review artifact, not a
 * diagnostic.  Treat the comments here as requirements to be verified against
 * an implementation, not as protection from a wrong one.
 *
 * Three rules that keep this file doing its job:
 *
 *   1. THE DRIVER TU MUST INCLUDE THIS HEADER.  Nothing in the build system
 *      can compel it.  A driver that defines these symbols without including
 *      this file links fine with wrong signatures and puts us back at the
 *      original defect with a header sitting next to it looking like
 *      protection.  This was an acceptance criterion for OI-SWCI-17 and it is
 *      MET: every TU in platform/ includes np_hal_internal.h, which includes
 *      this file, so all 25 definitions are checked against these declarations.
 *
 *   2. THE HOST-TEST DOUBLES MUST NEVER BE ADDED TO THE FIRMWARE LINK TARGET.
 *      Since they are ABI-guaranteed against this header, the doubles in
 *      tests/ are a drop-in definition set for exactly the symbols the image
 *      needs — i.e. the shortest path to a green, useless link.  Now that the
 *      image DOES link, this rule matters more than it did when it was written,
 *      not less: the difference between the real platform layer and the doubles
 *      is no longer "links vs does not link".  It is asserted rather than
 *      trusted — safety-mcu-ci.yml's "Assert no test double reached the image"
 *      step greps the link map for any object built from tests/ and fails the
 *      job.  (Spelled out rather than written as a glob: a literal
 *      tests-slash-star-dot-c-dot-obj here opens a nested comment and is an
 *      error under -Werror=comment, which is how this very line was caught.)
 *
 *   3. THIS HEADER STAYS DECLARATION-ONLY.  No static inline, no register
 *      typedef, no vendor/CMSIS include.  Host tests now depend on it, so any
 *      of those would pull target-only content into a host build.
 *
 * SELF-CONTAINMENT
 * ----------------
 * This header pulls in <stdint.h>, <stdbool.h> and np_safety_protocol.h and
 * nothing else.  In particular it does NOT include the CMSIS device headers:
 * `np_hal_gpio_write_pin` takes `void *port` deliberately, so the six host
 * tests can build natively without an STM32G071 device header.  Do not
 * "strengthen" that parameter to `GPIO_TypeDef *` — it would drag CMSIS into
 * every consumer and break the host suite.  The concrete ports are the
 * NP_EN_*_PORT macros in np_safety_config.h, which do expand to CMSIS types.
 *
 * IEC 62304 Class C — MISRA C:2012.  C11, no GNU extensions.
 */

#ifndef NP_SAFETY_HAL_H
#define NP_SAFETY_HAL_H

#include "np_safety_protocol.h"
#include <stdbool.h>
#include <stdint.h>

/* ── Clock and timebase ───────────────────────────────────────────────────────
 *
 * np_hal_clock_init()    Configure the PLL for 64 MHz core clock.  Called once,
 *                        first, from main() before any other HAL call.
 *
 * np_hal_systick_init()  Start the 1 ms SysTick that backs np_hal_get_tick_ms().
 *
 * np_hal_get_tick_ms()   Free-running monotonic millisecond counter.  UNITS ARE
 *                        MILLISECONDS — this is load-bearing for three Class C
 *                        timings that are all expressed in ms against it:
 *                          · the SPI heartbeat watchdog, NP_SAFETY_WDG_TIMEOUT_MS
 *                            = 1500 ms (np_spi_watchdog.c)
 *                          · the cardiac observation window, NP_CARDIAC_OBS_MS
 *                            = 5000 ms, and the ±15 BPM cutoff evaluated inside
 *                            it (np_cardiac_interlock.c)
 *                          · the cervical-VNS re-enable lockout,
 *                            NP_CARDIAC_LOCKOUT_MS = 30000 ms
 *                        A HAL that returned any other unit would not fail to
 *                        link and would not warn; it would silently rescale
 *                        every stimulation timeout.
 *                        The uint32_t counter wraps at ~49.7 days.  All callers
 *                        compare via unsigned subtraction (`now - then`), which
 *                        is wrap-correct; preserve that when implementing.
 */
void     np_hal_clock_init(void);
void     np_hal_systick_init(void);
uint32_t np_hal_get_tick_ms(void);

/* ── GPIO ─────────────────────────────────────────────────────────────────────
 *
 * np_hal_gpio_init()       Configure direction and mode for all GPIO banks.
 *                          Direction ONLY — it does not set output levels.
 *                          np_gpio_mgr_init() sets the initial (disabled) state
 *                          immediately afterwards via np_hal_gpio_write_pin().
 *
 * np_hal_gpio_write_pin()  Drive one stimulation enable line.
 *                          port   one of the NP_EN_*_PORT macros
 *                                 (np_safety_config.h).  Typed `void *` on
 *                                 purpose — see SELF-CONTAINMENT above.
 *                          pin    the matching NP_EN_*_PIN macro.
 *                          state  ALL TEN ENABLE LINES ARE ACTIVE-LOW
 *                                 open-drain with external pull-ups:
 *                                   1 = HIGH = stimulation DISABLED (safe)
 *                                   0 = LOW  = stimulation ENABLED
 *                                 Inverting this polarity would enable every
 *                                 stimulation channel at reset.  np_gpio_mgr.c
 *                                 is the only module that calls this.
 */
void np_hal_gpio_init(void);
void np_hal_gpio_write_pin(void *port, uint16_t pin, int state);

/* ── ADC (NTC thermal sense) ──────────────────────────────────────────────────
 *
 * np_hal_adc_init()          Bring up ADC1 for the NTC channels.
 *
 * np_hal_adc_read_channel()  Return a raw 12-bit conversion, 0..4095, at
 *                            Vref = 3.3 V.  `channel` indexes the NTC sense
 *                            domains, 0 .. NP_NTC_CHANNEL_COUNT-1 (6 = five
 *                            cranial domains + hub).  np_thermal_interlock.c
 *                            maps counts to °C through a 10 kΩ / B=3950 K
 *                            divider table and cuts at NP_NTC_CUTOFF_DEG_C
 *                            (62 °C junction), so the count is a temperature
 *                            in disguise: the scale is part of the contract,
 *                            not an implementation detail.
 */
void     np_hal_adc_init(void);
uint16_t np_hal_adc_read_channel(uint8_t channel);

/* ── SPI slave (to i.MX RT1062 main processor) ────────────────────────────────
 *
 * SPI1 in slave mode.  Three distinct inbound frame types share the link and
 * are told apart BY NSS-DELINEATED TRANSFER LENGTH, not by any in-band tag:
 *
 *     38 bytes   extended heartbeat   np_safety_rx_ext_frame_t
 *    102 bytes   session signature    np_safety_sig_cmd_t
 *     34 bytes   per-channel limits   np_safety_chan_limit_cmd_t
 *
 * Each type has a `_ready()` predicate and a `_get_*()` drain.  The `_get_*()`
 * calls are only valid when the matching `_ready()` returned true; calling one
 * otherwise reads a stale or partial buffer.
 *
 * np_hal_spi_send_reply()  Stage the full MISO payload clocked out during the
 *                          master's NEXT 38-byte heartbeat transfer
 *                          (OI-CVNS-HUB-11).  Layout, len = 38:
 *                            buf[0..7]    the 8-byte MCU reply frame
 *                            buf[8..15]   extended per-electrode impedance
 *                                         report (np_safety_imp_report_t, at
 *                                         NP_SAFETY_IMP_REPORT_OFFSET)
 *                            buf[16..37]  zero
 *                          np_safety_main.c is the only caller and always
 *                          passes NP_SAFETY_RX_EXT_FRAME_LEN.
 *
 * np_hal_spi_send_frame()  DECLARED BUT NEVER CALLED anywhere in this firmware.
 *                          It is the only one of the 25 with no call site.
 *                          Before phase 7 that was why the linker reported 24
 *                          undefined np_hal_* symbols while 25 were declared —
 *                          an unreferenced declaration produces no undefined
 *                          reference.  The same fact now shows up at the other
 *                          end: it IS implemented in platform/np_hal_spi.c and
 *                          IS in the object file, but -Wl,--gc-sections drops
 *                          it from the linked image, so a symbol census over
 *                          np_safety_mcu.elf finds 24 of the 25 and that is
 *                          correct rather than a gap.  It
 *                          appears to have been superseded by
 *                          np_hal_spi_send_reply() when OI-CVNS-HUB-11 widened
 *                          the reply window from 8 bytes to 38.  Retained
 *                          verbatim rather than deleted, because "delete the
 *                          dead TX path" and "the real TX path was never wired
 *                          up" are materially different conclusions and only
 *                          the Defect E design review can tell them apart.
 *                          NOTE: because it has no call site and no test
 *                          double, it is the one symbol here that NO consumer
 *                          binds — its signature is unverified by any
 *                          compiler check, unlike the other 24.
 */
void np_hal_spi_slave_init(void);

bool np_hal_spi_frame_ready(void);
void np_hal_spi_get_ext_frame(np_safety_rx_ext_frame_t *rx_out);
void np_hal_spi_send_frame(const np_safety_tx_frame_t *tx);
void np_hal_spi_send_reply(const uint8_t *buf, uint8_t len);

bool np_hal_spi_cmd_ready(void);
void np_hal_spi_get_cmd(np_safety_sig_cmd_t *cmd_out);

bool np_hal_spi_chan_limit_ready(void);
void np_hal_spi_get_chan_limit(np_safety_chan_limit_cmd_t *cmd_out);

/* ── TIM2 capture (R-peak interval timing) ────────────────────────────────────
 *
 * np_hal_tim2_init()         TIM2 as a free-running 1 MHz up-counter with input
 *                            capture on RPEAK_IN (PA8).  1 MHz is the contract:
 *                            np_cardiac_interlock.c treats each count as one
 *                            microsecond when converting R-R intervals to BPM.
 *
 * np_hal_tim2_get_capture()  The count latched at the most recent R-peak edge,
 *                            in microseconds.  Free-running, so it wraps; the
 *                            caller takes unsigned differences.
 */
void     np_hal_tim2_init(void);
uint32_t np_hal_tim2_get_capture(void);

/* ── R-peak edge flag ─────────────────────────────────────────────────────────
 *
 * np_hal_rpeak_edge_pending()  True when a new R-peak edge has been captured
 *                              since the last clear.  Polled from the cardiac
 *                              interlock tick, not consumed in an ISR.
 *
 * np_hal_rpeak_edge_clear()    Acknowledge the edge.  The caller reads
 *                              np_hal_tim2_get_capture() BEFORE clearing.
 */
bool np_hal_rpeak_edge_pending(void);
void np_hal_rpeak_edge_clear(void);

/* ── Contact impedance ────────────────────────────────────────────────────────
 *
 * 1 kHz AC pre-session contact check, run while stimulation output is disabled.
 * `channel` is the impedance-check index used by np_impedance_check.c:
 *     0 = VNS_HRV   1 = tDCS   2 = BES_TACS   3 = CVNS
 * (distinct from the enable-line slot numbers 7/6/5/10 in the same module).
 *
 * np_hal_impedance_start_test()   Begin a measurement on one channel.
 * np_hal_impedance_result_ready() True once that channel's result is available.
 * np_hal_impedance_read_ohm()     Contact impedance in OHMS.  The interlock
 *                                 rejects enable above NP_IMPEDANCE_MAX_OHM
 *                                 (10 000 Ω), so the unit is the gate.
 * np_hal_impedance_read_cvns_electrode_ohm()
 *                                 Per-electrode cervical-VNS impedance in ohms.
 *                                 electrode 0 = left, 1 = right.  Feeds the hub
 *                                 cross-validation report only (OI-CVNS-HUB-11);
 *                                 the enable GATE remains the single-value
 *                                 np_hal_impedance_read_ohm() check, so this is
 *                                 additive and does not alter the interlock.
 *                                 PRIVACY: raw ohms are UHDR and must be
 *                                 transferred device-internally only — only the
 *                                 divergence FLAG reaches SHDR (CLAUDE.md §5.1).
 */
void     np_hal_impedance_start_test(uint8_t channel);
bool     np_hal_impedance_result_ready(uint8_t channel);
uint32_t np_hal_impedance_read_ohm(uint8_t channel);
uint32_t np_hal_impedance_read_cvns_electrode_ohm(uint8_t electrode);

/* ── OTP ──────────────────────────────────────────────────────────────────────
 *
 * np_hal_otp_read_pubkey()  Read the manufacturing root Ed25519 public key out
 *                           of one-time-programmable memory into `buf`.
 *                           len is always NP_ED25519_PUB_KEY_LEN (32); the
 *                           single caller, np_session_sig_init(), passes it.
 *                           An ALL-ZERO key is the agreed sentinel for "OTP was
 *                           never programmed" (unprovisioned device); the
 *                           caller then faults with NP_FAULT_SLOT_UNPROV on the
 *                           first session attempt rather than at init.
 */
void np_hal_otp_read_pubkey(uint8_t *buf, uint8_t len);

/*
 * ── WHAT THIS HEADER COULD NOT TELL YOU — AND WHERE PHASE 7 ANSWERED IT ──────
 *
 * These ten were listed rather than guessed because they were exactly the
 * questions the Defect E (OI-SWCI-17) design review had to answer.  Phase 7 is
 * that review's implementation, so each now carries its resolution and the file
 * that owns it.  The answers are DESIGN DECISIONS with stated reasoning, not
 * measurements — nothing below has been validated on silicon.
 *
 *   1  BLOCKING/WCET .................. answered (bounded) — np_hal_adc.c
 *   2  tick atomicity ................. answered (safe on ARMv6-M) — np_hal_clock.c
 *   3  out-of-range arguments ......... answered (fail-safe) — adc/impedance
 *   4  impedance settling ownership ... answered (the HAL owns it) — np_hal_impedance.c
 *   5  capture before first edge ...... answered (zero) — np_hal_rpeak.c
 *   6  `state` domain ................. answered (non-zero = HIGH) — np_hal_gpio.c
 *   7  SPI arrival semantics .......... answered (per-type buffers, newest-wins) — np_hal_spi.c
 *   8  OTP failure signalling ......... NOT fixable without a signature change; mitigated
 *   9  np_hal_spi_send_frame .......... STILL OPEN as a protocol question; symbol now bound
 *  10  reset-state guarantees ......... firmware half closed; pull-up sizing OPEN (OI-SWCI-27)
 *
 * The original text of each follows unchanged, so the question a decision was
 * made against is still legible next to the decision.
 *
 *  1. BLOCKING AND WCET.  Not one of the 25 states whether it may block, or
 *     what its worst-case execution time is.  Every one is called from the
 *     single main loop that also services the 1.5 s watchdog and the <100 ms
 *     cervical-VNS cardiac cutoff, so a blocking ADC or OTP read is a missed
 *     deadline in a safety interlock.  WCET bounds are a Class C deliverable.
 *  2. np_hal_get_tick_ms() ATOMICITY.  Cortex-M0+ has no LDREX/STREX and no
 *     guaranteed atomic 32-bit read against a concurrent ISR write.  Whether
 *     the counter read is torn-read-safe is unstated, and a torn read of the
 *     watchdog timebase is a spurious or suppressed stimulation cutoff.
 *  3. OUT-OF-RANGE ARGUMENTS.  All production call sites pass bounded indices,
 *     and the host-test doubles clamp defensively, so the real behaviour for an
 *     out-of-range `channel` or `electrode` has never been specified.
 *  4. np_hal_impedance_start_test() TIMING OWNERSHIP.  NP_IMPEDANCE_TEST_MS
 *     (50 ms) exists, but nothing says whether the HAL enforces the settling
 *     interval internally or whether the caller must.  Both readings are
 *     consistent with the current code, because neither side implements it.
 *  5. np_hal_tim2_get_capture() BEFORE THE FIRST CAPTURE.  Undefined.  The
 *     module carries an s_first_beat_seen guard, which suggests the author
 *     expected garbage, but that is inference, not contract.
 *  6. np_hal_gpio_write_pin() `state` DOMAIN.  Declared `int`; every caller
 *     passes exactly 0 or 1.  Whether any non-zero value means HIGH is
 *     unstated — and on an active-LOW enable line that is a safety question.
 *  7. SPI FRAME ARRIVAL SEMANTICS.  Whether the three `_ready()` predicates are
 *     mutually exclusive, whether more than one frame can be buffered, and what
 *     happens to an un-drained frame when the next arrives, are all unspecified.
 *  8. np_hal_otp_read_pubkey() FAILURE SIGNALLING.  It returns void, so a
 *     genuine OTP read error is indistinguishable from an unprogrammed device.
 *     Both surface as the all-zero sentinel.
 *  9. np_hal_spi_send_frame() DISPOSITION — dead path or unwired path.  See its
 *     note above.
 * 10. RESET-STATE GUARANTEES.  np_hal_gpio_init() is documented as configuring
 *     direction only, so between power-on and np_gpio_mgr_init() the enable
 *     lines are held disabled by external pull-ups alone.  The required pull-up
 *     value and the maximum tolerated window are hardware facts not recorded in
 *     any software document here.
 */

#endif /* NP_SAFETY_HAL_H */
