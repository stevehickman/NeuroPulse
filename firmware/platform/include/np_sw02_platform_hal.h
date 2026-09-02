/*
 * NeurOne SW-02 Platform HAL — single declaration point
 * Document: NP-SW-CI-001 §4.8 (phase 8, closes OI-SWCI-21)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * ── What this header is ─────────────────────────────────────────────────────
 *
 * The SW-02 counterpart of firmware/safety_mcu/include/np_safety_hal.h, and it
 * exists for the same reason that one does (NP-SW-CI-001 §4.4.1, OI-SWCI-18):
 * a platform symbol declared by an `extern` line inside the .c file that calls
 * it is a contract with exactly one party.  Nothing cross-checks the definition
 * against the declaration, because the definition is in a different translation
 * unit and C has no mangling — a driver written with the arguments in the wrong
 * order compiles clean and links clean.
 *
 * Every symbol here is declared once.  Both the caller and firmware/platform/
 * include this file, so the compiler compares them.
 *
 * ── What this header is NOT ─────────────────────────────────────────────────
 *
 * It is not the whole SW-02 platform contract.  Thirty further symbols are
 * declared in the module header that owns them and are already single-sourced
 * there — np_anon_hal_* (np_anon_scratch.h), np_cvns_hal_impedance_{start,poll}
 * (np_cvns_reenable.h), np_factory_reset_hal_* (np_factory_reset.h),
 * np_hexmap_nvram_* (np_module_map.h), np_log_hal_part_* (np_log_backend.h),
 * np_safety_hal_spi_transfer (np_safety_spi.h), np_uhdr_hal_* (np_uhdr_key.h)
 * and np_za_platform_* (np_zone_announce.h).  They are deliberately NOT
 * restated here: a second declaration of an already single-sourced symbol is a
 * second place to get it wrong.  firmware/platform/src/np_platform_stub.c
 * includes those eight headers directly.
 *
 * The full SW-02 platform contract is therefore this file plus those eight
 * headers, and the count that tracks it is NP_SW02_PLATFORM_SYMBOL_COUNT below.
 */

#ifndef NP_SW02_PLATFORM_HAL_H
#define NP_SW02_PLATFORM_HAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "np_hub_types.h"     /* np_hub_status_t */
#include "np_accel_shdr.h"    /* np_accel_status_t and the SHDR record types */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Total number of symbols the SW-02 platform layer owes: the 63 declared in
 * this file plus the 30 declared in the eight module headers named above.
 *
 * This is not decoration.  firmware/platform/ defines all 93 as traps rather
 * than drivers, and the cross-build asserts that the number of definitions it
 * emits equals this constant (NP-SW-CI-001 §4.8).  The count can only change by
 * editing this line, which is the point: a platform symbol appearing or
 * disappearing is a design event, not a build detail.
 */
#define NP_SW02_PLATFORM_SYMBOL_COUNT   93

/* ── Status LED (OI-HUB-MAIN-02) ─────────────────────────────────────────────
 *
 * The enum moved here from np_hub_control_main.c on 2026-09-01.  It had to:
 * the type is part of the ABI of np_hal_status_led_set(), and an anonymous
 * enum typedef'd in one .c file cannot be named by the definition in another.
 * CLAUDE.md §4.7 owns the meanings — left temple green power LED, red blink on
 * fault. */
typedef enum {
    NP_LED_IDLE    = 0,
    NP_LED_SESSION = 1,
    NP_LED_FAULT   = 2
} np_led_state_t;

extern void     np_hal_status_led_set(np_led_state_t state);        /* OI-HUB-MAIN-02 */
extern uint32_t np_hal_get_device_session_count(void);              /* OI-HUB-MAIN-03 */

/* ── Protocol identity (np_protocol.c) ───────────────────────────────────────
 * The public key returned by np_proto_hal_get_proto_pubkey() is 32 bytes and
 * is what makes "headset rejects unsigned or corrupted protocols" (CLAUDE.md
 * §4.2) true.  A platform layer that returns a key of its own choosing defeats
 * the signature check completely, which is why this symbol in particular must
 * never be left to an unchecked declaration. */
extern np_hub_status_t np_proto_hal_get_device_serial(uint8_t *buf, size_t len);
extern np_hub_status_t np_proto_hal_get_proto_pubkey(uint8_t *pub_key_out); /* 32 bytes */

/* ── PBM scalp tiles (np_mod_pbm.c) — OI-PBM-HAL-01..03 ──────────────────── */
extern np_hub_status_t np_mod_pbm_hal_pwm_set(uint8_t slot,
                                              uint8_t cur_a, uint8_t cur_b,
                                              uint8_t freq_code, uint8_t duty);
extern float           np_mod_pbm_hal_ntc_read(uint8_t slot);
extern uint16_t        np_mod_pbm_hal_pd_read(uint8_t slot, uint8_t wl_idx);

/* ── PBM intranasal (np_mod_intranasal.c) ───────────────────────────────── */
extern bool            np_mod_ins_hal_auth_check(void);
extern bool            np_mod_ins_hal_pd_contact(void);
extern np_hub_status_t np_mod_ins_hal_pwm_set(uint8_t side,
                                              uint8_t cur_660, uint8_t cur_808,
                                              uint8_t freq_code, uint8_t duty);
extern void            np_mod_ins_hal_pwm_stop(uint8_t side);
extern uint16_t        np_mod_ins_hal_pd_read(uint8_t side, uint8_t wl);

/* ── EEG front end, ADS1299 (np_mod_eeg.c) ──────────────────────────────── */
extern void     np_mod_eeg_hal_spi_write_reg(uint8_t reg, uint8_t val);
extern uint8_t  np_mod_eeg_hal_spi_read_reg(uint8_t reg);
extern void     np_mod_eeg_hal_start_continuous(void);
extern void     np_mod_eeg_hal_stop_continuous(void);
extern float    np_mod_eeg_hal_read_impedance(uint8_t ch);
extern float    np_mod_eeg_hal_get_band_power(uint8_t band);
extern uint8_t  np_mod_eeg_hal_get_dominant_band(void);
extern void     np_mod_eeg_hal_self_calibrate(void);

/* ── BES/tACS + tDCS drive (np_mod_stim.c) ──────────────────────────────────
 * The 40 uC/cm^2 charge-density limit is enforced by the safety MCU, not here
 * (CLAUDE.md §4.2).  np_mod_stim_hal_dc_set is declared but not yet called by
 * any caller in-tree, so unlike its six siblings it does not appear as an
 * unresolved symbol at link; it is part of the contract regardless. */
extern np_hub_status_t np_mod_stim_hal_dac_set(uint8_t pair, uint8_t waveform,
                                               uint16_t freq_mhz, uint16_t amp_ua);
extern np_hub_status_t np_mod_stim_hal_dac_stop(uint8_t pair);
extern np_hub_status_t np_mod_stim_hal_dc_set(uint8_t pair, uint16_t current_ua,
                                              uint8_t polarity);
extern np_hub_status_t np_mod_stim_hal_dc_ramp(uint8_t pair, uint16_t target_ua,
                                               uint8_t polarity, uint16_t ramp_s);
extern float    np_mod_stim_hal_read_impedance(uint8_t pair);
extern float    np_mod_stim_hal_read_current(uint8_t pair);
extern uint16_t np_mod_stim_hal_read_charge(uint8_t pair);

/* ── Auricular VNS + PPG/HRV (np_mod_vns.c) ─────────────────────────────── */
extern float           np_mod_vns_hal_impedance_check(void);
extern np_hub_status_t np_mod_vns_hal_stim_set(uint8_t side, uint16_t freq_mhz,
                                               uint16_t amp_ua, uint8_t pw_us);
extern np_hub_status_t np_mod_vns_hal_stim_stop(uint8_t side);
extern void            np_mod_vns_hal_ppg_start(void);
extern void            np_mod_vns_hal_ppg_stop(void);
extern void            np_mod_vns_hal_eeg_ref_route(bool enable);
extern float           np_mod_vns_hal_get_rmssd(void);
extern float           np_mod_vns_hal_get_coherence(void);
extern float           np_mod_vns_hal_get_hr_bpm(void);
extern uint32_t        np_mod_vns_hal_now_ms(void);                 /* OI-VNS-10 */

/* ── Neural audio entrainment (np_mod_audio.c) ──────────────────────────── */
extern np_hub_status_t np_mod_audio_hal_codec_init(void);
extern void  np_mod_audio_hal_set_binaural(uint16_t carrier_hz,
                                           uint16_t beat_mhz, uint8_t vol_pct);
extern void  np_mod_audio_hal_set_isochronic(uint16_t freq_hz, uint8_t vol_pct);
extern void  np_mod_audio_hal_set_noise(uint8_t type, uint8_t vol_pct);
extern void  np_mod_audio_hal_stop_planar(void);
extern void  np_mod_audio_hal_bone_set(uint16_t freq_hz, uint8_t vol_pct);
extern void  np_mod_audio_hal_bone_stop(void);
extern float np_mod_audio_hal_mesh_impedance(void);

/* ── Visual stimulation (np_mod_visual.c) ───────────────────────────────────
 * np_mod_visual_hal_hall_lifted() and _ir_eye_open() feed two of the three
 * independent layers behind the IEC 62471 MPE ceiling (CLAUDE.md §4.2).  The
 * third is a hardware current limit that no software can defeat, which is the
 * only reason a trap here is a safe failure rather than an unsafe one. */
extern np_hub_status_t np_mod_visual_hal_led_set(uint16_t zone_mask, uint8_t wl_sel,
                                                 uint8_t freq_hz, uint8_t duty_pct);
extern void np_mod_visual_hal_led_stop(void);
extern bool np_mod_visual_hal_ir_eye_open(void);
extern bool np_mod_visual_hal_hall_lifted(void);
extern void np_mod_visual_hal_emdr_set(uint8_t rate_mhz);
extern bool np_mod_visual_hal_mpe_check(void);

/* ── Cervical VNS bridge (np_mod_cvns.c) — T2 ───────────────────────────────
 * np_mod_cvns_hal_now_ms() is also called by np_session_runner.c and
 * np_hub_control_main.c; the interlock's R-R timing assumes one clock domain
 * across all three, which is exactly the kind of assumption that a shared
 * declaration makes visible and three separate `extern` lines did not. */
extern bool            np_cvns_hal_accessory_present(void);         /* OI-CVNS-HUB-04 */
extern uint32_t        np_mod_cvns_hal_now_ms(void);                /* OI-CVNS-HUB-05 */
extern uint32_t        np_mod_cvns_hal_now_unix(void);              /* OI-CVNS-HUB-06 */
extern np_hub_status_t np_cvns_hal_impedance_measure_start(void);   /* OI-CVNS-HUB-09 */
extern bool            np_cvns_hal_impedance_measure_poll(float kohm_out[]); /* OI-CVNS-HUB-09 */

/* ── SHDR accelerometer path (np_accel_shdr.c) ──────────────────────────────
 * NP-FW-EMMC-002 §G/§H.  np_shdr_write_accel_char_record() is the §H
 * characterisation write and is reachable only while a device is enrolled;
 * np_shdr_write_accel_record() is the §G standing path.  The privacy boundary
 * between them is enforced upstream in np_accel_shdr.c, not by these seams. */
extern np_accel_status_t np_imu_read_gap_resultants(float *dst, size_t cap, size_t *out_n);
extern np_accel_status_t np_shdr_write_accel_record(const np_shdr_accel_record_t *rec);
extern np_accel_status_t np_shdr_write_accel_char_record(const np_shdr_accel_char_record_t *rec);
extern np_accel_status_t np_config_read_char_enrolment(np_accel_char_enrolment_t *enr);
extern np_accel_status_t np_config_write_char_enrolment(const np_accel_char_enrolment_t *enr);
extern np_accel_status_t np_config_read_accel_state(np_accel_shdr_state_t *state);
extern np_accel_status_t np_config_write_accel_state(const np_accel_shdr_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* NP_SW02_PLATFORM_HAL_H */
