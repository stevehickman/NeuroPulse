/*
 * NeurOne Firmware-in-the-Loop Simulator — the safety MCU in the loop (NP-SIM-FW-001 §22, OI-SIMFW-09, REQ-SIMFW-14)
 * HOST ONLY; see np_sim_bus.h.
 *
 * The safety MCU's own sources (the .c files under firmware/safety_mcu/src, SW-01 Class C), unmodified, run as the device runs them: np_safety_main.c's
 * main() is the polling loop, on a thread of its own, with the np_hal_* seams of np_safety_hal.h defined here.  The hub's own
 * np_safety_spi.c talks to it through np_safety_hal_spi_transfer, defined here, which is a frame into the MCU's per-type buffers
 * and the staged reply back.  Nothing else of the MCU is a model: the interlocks, the signature check, the charge monitor, the
 * cardiac detector and the GPIO manager are the device's code.
 *
 * Time.  The MCU's loop and the hub side never run at once.  The loop is paced at one iteration per virtual millisecond (the
 * device's loop period is unmeasured, OI-SWCI-49; this is a choice, listed in unjustified-choices.md), by the one place a loop
 * iteration ends: np_hal_iwdg_refresh() blocks until the harness grants the next iteration.  np_sim_advance_ms() grants one per
 * millisecond through the bus's tick hook.  A run is therefore repeatable.
 *
 * One safety MCU per process.  The MCU's modules keep their state in file-scope statics, which a power cycle clears on the device
 * and a thread restart would not.  A scenario that needs a clean MCU, or a power-on reset with its flash and OTP kept, starts a
 * process (np_sim_safety_tests.c forks).  The flash and the OTP live in memory shared with forked children (np_sim_safety_silicon).
 */
#ifndef NP_SIM_SAFETY_H
#define NP_SIM_SAFETY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NP_SIM_SAFETY_LOOP_MS   1U      /* one iteration of the MCU's polling loop per virtual millisecond */
#define NP_SIM_SAFETY_LINES     10U     /* the ten stimulation enable lines */
#define NP_SIM_SAFETY_NTC       6U
#define NP_SIM_SAFETY_IMP       4U

/* The flash and OTP of the part.  Allocated in memory shared with child processes, so a power-cycled MCU (a new process) finds
 * what the last one left.  Idempotent.  Erased on creation: OTP all 0xFF, flash all 0xFF. */
void np_sim_safety_silicon(void);

/* Programming the OTP the way manufacture does (REQ-UPG-02: no field route writes it).  The root key is derived from `seed` by
 * Monocypher; the sim keeps the seed so it can sign sessions.  A TEST key, never a production one (REQ-SIMFW-08). */
void np_sim_safety_otp_program_root(const uint8_t seed[32], bool write_crc);
void np_sim_safety_otp_corrupt_root_bit(void);
void np_sim_safety_otp_program_tier(uint8_t tier, uint8_t hw_version);      /* signed by the test tier authority, bound to the UID */
void np_sim_safety_set_uid(const uint8_t uid[12]);

/* The MCU starts: the clock, GPIO, SPI, ADC and timer init, the module inits, the iwdg start, and one polling iteration.  false if
 * already attached. */
bool np_sim_safety_attach(void);
bool np_sim_safety_attached(void);

/* Sign a session hash with the root seed programmed above (the app's part of the protocol).  False before a root is programmed. */
bool np_sim_safety_sign_session(const uint8_t hash[32], uint8_t sig[64]);

/* Scenario-side inputs of the part's analogue and digital world.  Nothing here has a default that reads as a measurement: an
 * unset NTC reads as a count of 4095 (a cold, open sensor), an unset impedance channel as the HAL's open-circuit value. */
void np_sim_safety_set_adc(uint8_t channel, uint16_t counts);
void np_sim_safety_set_impedance_ohm(uint8_t channel, uint32_t ohm);        /* 0..3: VNS_HRV, tDCS, BES_TACS, CVNS */
void np_sim_safety_set_cvns_electrode_ohm(uint8_t electrode, uint32_t ohm);
void np_sim_safety_rpeak(void);                                             /* an R-peak edge now */

/* Faults the part can have. */
typedef enum {
    NP_SIM_SAFETY_FAULT_NV_ERASE = 0,       /* the next n page erases fail */
    NP_SIM_SAFETY_FAULT_NV_PROGRAM,         /* the next n double-word programs fail */
    NP_SIM_SAFETY_FAULT_SPI_TIMEOUT,        /* the next n hub transfers time out */
    NP_SIM_SAFETY_FAULT_SPI_FLIP,           /* the next n heartbeat transfers arrive with byte `arg` XOR 0xFF */
    NP_SIM_SAFETY_FAULT_STALL,              /* the polling loop hangs (the independent watchdog's case) */
    NP_SIM_SAFETY_FAULT_SYSTICK_FREEZE      /* SysTick stops; TIM2 keeps running (OI-FMEA-12 (b)) */
} np_sim_safety_fault_t;
void np_sim_safety_inject(np_sim_safety_fault_t what, uint32_t n, uint32_t arg);

/* What the part is doing, for scenarios and the inspector. */
typedef struct {
    bool     attached;
    bool     held_in_reset;         /* the independent watchdog expired: the MCU is in reset, every enable line disabled */
    uint32_t iwdg_resets;
    uint32_t iterations;
    uint32_t spi_overruns;
    uint32_t spi_discarded;
    uint16_t lines_enabled_mask;    /* bit n set: the line is LOW (stimulation enabled); n indexes np_sim_safety_line_name */
    uint16_t last_reply_granted;
    uint8_t  last_reply_status;
    uint8_t  last_reply_fault_slot;
} np_sim_safety_info_t;
void        np_sim_safety_info(np_sim_safety_info_t *out);
const char *np_sim_safety_line_name(uint8_t line);
bool        np_sim_safety_line_enabled(uint8_t line);

/* The label every module's result carries for the safety MCU: "in_loop" once attached, "not_in_loop" otherwise (REQ-SIMFW-07). */
const char *np_sim_safety_label(void);

#ifdef __cplusplus
}
#endif
#endif
