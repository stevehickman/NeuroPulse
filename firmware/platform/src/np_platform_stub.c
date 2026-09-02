/*
 * NeurOne SW-02 Platform Layer — trap definitions
 * Document: NP-SW-CI-001 §4.8 (phase 8, closes OI-SWCI-21)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * ── What this file is ───────────────────────────────────────────────────────
 *
 * One definition for every symbol the SW-02 platform layer owes, and not one of
 * them is a driver.  Each body calls np_platform_unimplemented(), which masks
 * interrupts and halts; see include/np_platform_trap.h for why that is the
 * right failure and a plausible return value is not.
 *
 * Its purpose is to make the gap MEASURABLE, which it was not before.  Until
 * np_application existed, no SW-02 link ran, so "zero unresolved symbols" on
 * the main-firmware CI leg was true and vacuous for SW-02 — the exact finding
 * OI-SWCI-21 records.  With a link, the gap has a number: 93, asserted against
 * NP_SW02_PLATFORM_SYMBOL_COUNT by the cross-build.  Writing a real driver
 * deletes a definition from this file and decrements that constant, so the
 * remaining distance to a runnable image is a figure in the build output rather
 * than a thing someone has to remember.
 *
 * ── Why the whole layer is here rather than a driver per module ─────────────
 *
 * NP-SW-CI-001 §4.4 makes the argument in its Class C form: writing drivers to
 * turn a build leg green is worse than leaving the leg red, because the drivers
 * would be written against silicon nobody has run and hardware decisions nobody
 * has made.  That argument is stronger on SW-02 than it was on SW-01.  The hub
 * cluster-controller fan-out these seams would drive is unimplemented and has
 * nineteen open items against it (OI-HUB-C01..C19); the hex-tile emitters the
 * PBM seams would modulate are not selected (OI-HEXTILE-02); the EEG net the
 * impedance seams would read is not sized (REG-1).  A driver written today
 * would encode guesses about all three and would look, in the diff, exactly
 * like one that had been designed.
 *
 * ── Ordering ────────────────────────────────────────────────────────────────
 *
 * Symbols appear in the order of their declarations, and every (void) cast is
 * present so this file compiles under -Wall -Wextra -Werror with
 * -Wunused-parameter fully in force.  It would have been shorter to switch that
 * warning off for this translation unit; NP-SW-CI-001 §4.4.2 retired exactly
 * that suppression on SW-01 with the reasoning that an ignored argument in a
 * stimulation seam is the diagnostic that matters most.  A stub layer is where
 * the suppression is most tempting and least affordable.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "np_platform_trap.h"

/* The 63 symbols with no other declaring header. */
#include "np_sw02_platform_hal.h"

/* The 30 already single-sourced by the module that owns them.  Included rather
 * than restated so the compiler — not this file — is what checks them. */
#include "np_anon_scratch.h"     /* np_anon_hal_*          (5) */
#include "np_cvns_reenable.h"    /* np_cvns_hal_impedance_ (2) */
#include "np_factory_reset.h"    /* np_factory_reset_hal_* (5) */
#include "np_module_map.h"       /* np_hexmap_nvram_*      (2) */
#include "np_log_backend.h"      /* np_log_hal_part_*      (3) */
#include "np_safety_spi.h"       /* np_safety_hal_spi_*    (1) */
#include "np_uhdr_key.h"         /* np_uhdr_hal_*          (8) */
#include "np_zone_announce.h"    /* np_za_platform_*       (4) */

/* ────────────────────────────────────────────────────────────────────────────
 * Hub control — status LED and session count (OI-HUB-MAIN-02, -03)
 * ──────────────────────────────────────────────────────────────────────────*/

void np_hal_status_led_set(np_led_state_t state)
{
    (void)state;
    NP_PLATFORM_TRAP();
}

uint32_t np_hal_get_device_session_count(void)
{
    NP_PLATFORM_TRAP();
}

/* ────────────────────────────────────────────────────────────────────────────
 * Protocol identity (np_protocol.c)
 * ──────────────────────────────────────────────────────────────────────────*/

np_hub_status_t np_proto_hal_get_device_serial(uint8_t *buf, size_t len)
{
    (void)buf;
    (void)len;
    NP_PLATFORM_TRAP();
}

np_hub_status_t np_proto_hal_get_proto_pubkey(uint8_t *pub_key_out)
{
    (void)pub_key_out;
    NP_PLATFORM_TRAP();
}

/* ────────────────────────────────────────────────────────────────────────────
 * PBM scalp tiles (OI-PBM-HAL-01..03)
 * ──────────────────────────────────────────────────────────────────────────*/

np_hub_status_t np_mod_pbm_hal_pwm_set(uint8_t slot,
                                       uint8_t cur_a, uint8_t cur_b,
                                       uint8_t freq_code, uint8_t duty)
{
    (void)slot;
    (void)cur_a;
    (void)cur_b;
    (void)freq_code;
    (void)duty;
    NP_PLATFORM_TRAP();
}

float np_mod_pbm_hal_ntc_read(uint8_t slot)
{
    (void)slot;
    NP_PLATFORM_TRAP();
}

uint16_t np_mod_pbm_hal_pd_read(uint8_t slot, uint8_t wl_idx)
{
    (void)slot;
    (void)wl_idx;
    NP_PLATFORM_TRAP();
}

/* ────────────────────────────────────────────────────────────────────────────
 * PBM intranasal
 * ──────────────────────────────────────────────────────────────────────────*/

bool np_mod_ins_hal_auth_check(void)
{
    NP_PLATFORM_TRAP();
}

bool np_mod_ins_hal_pd_contact(void)
{
    NP_PLATFORM_TRAP();
}

np_hub_status_t np_mod_ins_hal_pwm_set(uint8_t side,
                                       uint8_t cur_660, uint8_t cur_808,
                                       uint8_t freq_code, uint8_t duty)
{
    (void)side;
    (void)cur_660;
    (void)cur_808;
    (void)freq_code;
    (void)duty;
    NP_PLATFORM_TRAP();
}

void np_mod_ins_hal_pwm_stop(uint8_t side)
{
    (void)side;
    NP_PLATFORM_TRAP();
}

uint16_t np_mod_ins_hal_pd_read(uint8_t side, uint8_t wl)
{
    (void)side;
    (void)wl;
    NP_PLATFORM_TRAP();
}

/* ────────────────────────────────────────────────────────────────────────────
 * EEG front end (ADS1299)
 * ──────────────────────────────────────────────────────────────────────────*/

void np_mod_eeg_hal_spi_write_reg(uint8_t reg, uint8_t val)
{
    (void)reg;
    (void)val;
    NP_PLATFORM_TRAP();
}

uint8_t np_mod_eeg_hal_spi_read_reg(uint8_t reg)
{
    (void)reg;
    NP_PLATFORM_TRAP();
}

void np_mod_eeg_hal_start_continuous(void)
{
    NP_PLATFORM_TRAP();
}

void np_mod_eeg_hal_stop_continuous(void)
{
    NP_PLATFORM_TRAP();
}

float np_mod_eeg_hal_read_impedance(uint8_t ch)
{
    (void)ch;
    NP_PLATFORM_TRAP();
}

float np_mod_eeg_hal_get_band_power(uint8_t band)
{
    (void)band;
    NP_PLATFORM_TRAP();
}

uint8_t np_mod_eeg_hal_get_dominant_band(void)
{
    NP_PLATFORM_TRAP();
}

void np_mod_eeg_hal_self_calibrate(void)
{
    NP_PLATFORM_TRAP();
}

/* ────────────────────────────────────────────────────────────────────────────
 * BES / tACS + tDCS drive
 * ──────────────────────────────────────────────────────────────────────────*/

np_hub_status_t np_mod_stim_hal_dac_set(uint8_t pair, uint8_t waveform,
                                        uint16_t freq_mhz, uint16_t amp_ua)
{
    (void)pair;
    (void)waveform;
    (void)freq_mhz;
    (void)amp_ua;
    NP_PLATFORM_TRAP();
}

np_hub_status_t np_mod_stim_hal_dac_stop(uint8_t pair)
{
    (void)pair;
    NP_PLATFORM_TRAP();
}

np_hub_status_t np_mod_stim_hal_dc_set(uint8_t pair, uint16_t current_ua,
                                       uint8_t polarity)
{
    (void)pair;
    (void)current_ua;
    (void)polarity;
    NP_PLATFORM_TRAP();
}

np_hub_status_t np_mod_stim_hal_dc_ramp(uint8_t pair, uint16_t target_ua,
                                        uint8_t polarity, uint16_t ramp_s)
{
    (void)pair;
    (void)target_ua;
    (void)polarity;
    (void)ramp_s;
    NP_PLATFORM_TRAP();
}

float np_mod_stim_hal_read_impedance(uint8_t pair)
{
    (void)pair;
    NP_PLATFORM_TRAP();
}

float np_mod_stim_hal_read_current(uint8_t pair)
{
    (void)pair;
    NP_PLATFORM_TRAP();
}

uint16_t np_mod_stim_hal_read_charge(uint8_t pair)
{
    (void)pair;
    NP_PLATFORM_TRAP();
}

/* ────────────────────────────────────────────────────────────────────────────
 * Auricular VNS + PPG / HRV
 * ──────────────────────────────────────────────────────────────────────────*/

float np_mod_vns_hal_impedance_check(void)
{
    NP_PLATFORM_TRAP();
}

np_hub_status_t np_mod_vns_hal_stim_set(uint8_t side, uint16_t freq_mhz,
                                        uint16_t amp_ua, uint8_t pw_us)
{
    (void)side;
    (void)freq_mhz;
    (void)amp_ua;
    (void)pw_us;
    NP_PLATFORM_TRAP();
}

np_hub_status_t np_mod_vns_hal_stim_stop(uint8_t side)
{
    (void)side;
    NP_PLATFORM_TRAP();
}

void np_mod_vns_hal_ppg_start(void)
{
    NP_PLATFORM_TRAP();
}

void np_mod_vns_hal_ppg_stop(void)
{
    NP_PLATFORM_TRAP();
}

void np_mod_vns_hal_eeg_ref_route(bool enable)
{
    (void)enable;
    NP_PLATFORM_TRAP();
}

float np_mod_vns_hal_get_rmssd(void)
{
    NP_PLATFORM_TRAP();
}

float np_mod_vns_hal_get_coherence(void)
{
    NP_PLATFORM_TRAP();
}

float np_mod_vns_hal_get_hr_bpm(void)
{
    NP_PLATFORM_TRAP();
}

uint32_t np_mod_vns_hal_now_ms(void)
{
    NP_PLATFORM_TRAP();
}

/* ────────────────────────────────────────────────────────────────────────────
 * Neural audio entrainment
 * ──────────────────────────────────────────────────────────────────────────*/

np_hub_status_t np_mod_audio_hal_codec_init(void)
{
    NP_PLATFORM_TRAP();
}

void np_mod_audio_hal_set_binaural(uint16_t carrier_hz,
                                   uint16_t beat_mhz, uint8_t vol_pct)
{
    (void)carrier_hz;
    (void)beat_mhz;
    (void)vol_pct;
    NP_PLATFORM_TRAP();
}

void np_mod_audio_hal_set_isochronic(uint16_t freq_hz, uint8_t vol_pct)
{
    (void)freq_hz;
    (void)vol_pct;
    NP_PLATFORM_TRAP();
}

void np_mod_audio_hal_set_noise(uint8_t type, uint8_t vol_pct)
{
    (void)type;
    (void)vol_pct;
    NP_PLATFORM_TRAP();
}

void np_mod_audio_hal_stop_planar(void)
{
    NP_PLATFORM_TRAP();
}

void np_mod_audio_hal_bone_set(uint16_t freq_hz, uint8_t vol_pct)
{
    (void)freq_hz;
    (void)vol_pct;
    NP_PLATFORM_TRAP();
}

void np_mod_audio_hal_bone_stop(void)
{
    NP_PLATFORM_TRAP();
}

float np_mod_audio_hal_mesh_impedance(void)
{
    NP_PLATFORM_TRAP();
}

/* ────────────────────────────────────────────────────────────────────────────
 * Visual stimulation
 * ──────────────────────────────────────────────────────────────────────────*/

np_hub_status_t np_mod_visual_hal_led_set(uint16_t zone_mask, uint8_t wl_sel,
                                          uint8_t freq_hz, uint8_t duty_pct)
{
    (void)zone_mask;
    (void)wl_sel;
    (void)freq_hz;
    (void)duty_pct;
    NP_PLATFORM_TRAP();
}

void np_mod_visual_hal_led_stop(void)
{
    NP_PLATFORM_TRAP();
}

bool np_mod_visual_hal_ir_eye_open(void)
{
    NP_PLATFORM_TRAP();
}

bool np_mod_visual_hal_hall_lifted(void)
{
    NP_PLATFORM_TRAP();
}

void np_mod_visual_hal_emdr_set(uint8_t rate_mhz)
{
    (void)rate_mhz;
    NP_PLATFORM_TRAP();
}

bool np_mod_visual_hal_mpe_check(void)
{
    NP_PLATFORM_TRAP();
}

/* ────────────────────────────────────────────────────────────────────────────
 * Cervical VNS bridge (T2)
 * ──────────────────────────────────────────────────────────────────────────*/

bool np_cvns_hal_accessory_present(void)
{
    NP_PLATFORM_TRAP();
}

uint32_t np_mod_cvns_hal_now_ms(void)
{
    NP_PLATFORM_TRAP();
}

uint32_t np_mod_cvns_hal_now_unix(void)
{
    NP_PLATFORM_TRAP();
}

np_hub_status_t np_cvns_hal_impedance_measure_start(void)
{
    NP_PLATFORM_TRAP();
}

bool np_cvns_hal_impedance_measure_poll(float kohm_out[])
{
    (void)kohm_out;
    NP_PLATFORM_TRAP();
}

/* ────────────────────────────────────────────────────────────────────────────
 * SHDR accelerometer path (NP-FW-EMMC-002 §G/§H)
 * ──────────────────────────────────────────────────────────────────────────*/

np_accel_status_t np_imu_read_gap_resultants(float *dst, size_t cap, size_t *out_n)
{
    (void)dst;
    (void)cap;
    (void)out_n;
    NP_PLATFORM_TRAP();
}

np_accel_status_t np_shdr_write_accel_record(const np_shdr_accel_record_t *rec)
{
    (void)rec;
    NP_PLATFORM_TRAP();
}

np_accel_status_t np_shdr_write_accel_char_record(const np_shdr_accel_char_record_t *rec)
{
    (void)rec;
    NP_PLATFORM_TRAP();
}

np_accel_status_t np_config_read_char_enrolment(np_accel_char_enrolment_t *enr)
{
    (void)enr;
    NP_PLATFORM_TRAP();
}

np_accel_status_t np_config_write_char_enrolment(const np_accel_char_enrolment_t *enr)
{
    (void)enr;
    NP_PLATFORM_TRAP();
}

np_accel_status_t np_config_read_accel_state(np_accel_shdr_state_t *state)
{
    (void)state;
    NP_PLATFORM_TRAP();
}

np_accel_status_t np_config_write_accel_state(const np_accel_shdr_state_t *state)
{
    (void)state;
    NP_PLATFORM_TRAP();
}

/* ────────────────────────────────────────────────────────────────────────────
 * Research anonymisation Scratch (OI-ANON-AES-01..05) — np_anon_scratch.h
 * ──────────────────────────────────────────────────────────────────────────*/

np_anon_status_t np_anon_hal_trng_generate(uint8_t *buf, size_t len)
{
    (void)buf;
    (void)len;
    NP_PLATFORM_TRAP();
}

np_anon_status_t np_anon_hal_aes_ctr_crypt(const uint8_t *key,
                                           const uint8_t *nonce,
                                           uint32_t block_offset,
                                           const uint8_t *in,
                                           uint8_t *out,
                                           size_t len)
{
    (void)key;
    (void)nonce;
    (void)block_offset;
    (void)in;
    (void)out;
    (void)len;
    NP_PLATFORM_TRAP();
}

np_anon_status_t np_anon_hal_emmc_write_scratch(uint32_t block_offset,
                                                const uint8_t *data,
                                                size_t len)
{
    (void)block_offset;
    (void)data;
    (void)len;
    NP_PLATFORM_TRAP();
}

np_anon_status_t np_anon_hal_emmc_read_scratch(uint32_t block_offset,
                                               uint8_t *data,
                                               size_t len)
{
    (void)block_offset;
    (void)data;
    (void)len;
    NP_PLATFORM_TRAP();
}

np_anon_status_t np_anon_hal_emmc_sanitize_scratch(void)
{
    NP_PLATFORM_TRAP();
}

/* ────────────────────────────────────────────────────────────────────────────
 * Cervical VNS re-enable impedance (OI-CVNS-HUB-02) — np_cvns_reenable.h
 * ──────────────────────────────────────────────────────────────────────────*/

np_hub_status_t np_cvns_hal_impedance_start(void)
{
    NP_PLATFORM_TRAP();
}

bool np_cvns_hal_impedance_poll(bool *passed_out)
{
    (void)passed_out;
    NP_PLATFORM_TRAP();
}

/* ────────────────────────────────────────────────────────────────────────────
 * Factory reset (R-07..R-10) — np_factory_reset.h
 * ──────────────────────────────────────────────────────────────────────────*/

np_reset_status_t np_factory_reset_hal_sanitize_uhdr(void)
{
    NP_PLATFORM_TRAP();
}

np_reset_status_t np_factory_reset_hal_zero_shdr(void)
{
    NP_PLATFORM_TRAP();
}

np_reset_status_t np_factory_reset_hal_zero_config(void)
{
    NP_PLATFORM_TRAP();
}

np_reset_status_t np_factory_reset_hal_trng_generate(uint8_t *buf, size_t len)
{
    (void)buf;
    (void)len;
    NP_PLATFORM_TRAP();
}

np_reset_status_t np_factory_reset_hal_write_config_defaults(
    const uint8_t *trng_salt, const uint8_t *warranty_token)
{
    (void)trng_salt;
    (void)warranty_token;
    NP_PLATFORM_TRAP();
}

/* ────────────────────────────────────────────────────────────────────────────
 * Hex module-map NVRAM — np_module_map.h
 * ──────────────────────────────────────────────────────────────────────────*/

np_hub_status_t np_hexmap_nvram_read(uint8_t *buf, size_t len, size_t *read_len)
{
    (void)buf;
    (void)len;
    (void)read_len;
    NP_PLATFORM_TRAP();
}

np_hub_status_t np_hexmap_nvram_write(const uint8_t *buf, size_t len)
{
    (void)buf;
    (void)len;
    NP_PLATFORM_TRAP();
}

/* ────────────────────────────────────────────────────────────────────────────
 * Session-log partition backing (OI-LOG-05..07) — np_log_backend.h
 * ──────────────────────────────────────────────────────────────────────────*/

np_hub_status_t np_log_hal_part_open(np_log_part_t part)
{
    (void)part;
    NP_PLATFORM_TRAP();
}

np_hub_status_t np_log_hal_part_append(np_log_part_t part,
                                       const uint8_t *buf, size_t len)
{
    (void)part;
    (void)buf;
    (void)len;
    NP_PLATFORM_TRAP();
}

np_hub_status_t np_log_hal_part_sync(np_log_part_t part)
{
    (void)part;
    NP_PLATFORM_TRAP();
}

/* ────────────────────────────────────────────────────────────────────────────
 * Safety-MCU SPI (OI-SAF-01) — np_safety_spi.h
 * ──────────────────────────────────────────────────────────────────────────*/

np_hub_status_t np_safety_hal_spi_transfer(const uint8_t *tx,
                                           uint8_t       *rx,
                                           uint8_t        len)
{
    (void)tx;
    (void)rx;
    (void)len;
    NP_PLATFORM_TRAP();
}

/* ────────────────────────────────────────────────────────────────────────────
 * Two-layer UHDR key scheme (OI-UHDRK-01..09) — np_uhdr_key.h
 * ──────────────────────────────────────────────────────────────────────────*/

np_uhdr_status_t np_uhdr_hal_trng_generate(uint8_t *buf, size_t len)
{
    (void)buf;
    (void)len;
    NP_PLATFORM_TRAP();
}

np_uhdr_status_t np_uhdr_hal_argon2id(const uint8_t *credential,
                                      size_t cred_len,
                                      const uint8_t *salt,
                                      uint32_t version,
                                      uint32_t m_cost,
                                      uint32_t t_cost,
                                      uint32_t parallelism,
                                      uint8_t *out)
{
    (void)credential;
    (void)cred_len;
    (void)salt;
    (void)version;
    (void)m_cost;
    (void)t_cost;
    (void)parallelism;
    (void)out;
    NP_PLATFORM_TRAP();
}

np_uhdr_status_t np_uhdr_hal_hw_bind(const uint8_t *ckey, uint8_t *wkmd_out)
{
    (void)ckey;
    (void)wkmd_out;
    NP_PLATFORM_TRAP();
}

np_uhdr_status_t np_uhdr_hal_aes_gcm_encrypt(const uint8_t *key,
                                             const uint8_t *nonce,
                                             const uint8_t *aad,
                                             size_t aad_len,
                                             const uint8_t *pt,
                                             size_t pt_len,
                                             uint8_t *ct,
                                             uint8_t *tag)
{
    (void)key;
    (void)nonce;
    (void)aad;
    (void)aad_len;
    (void)pt;
    (void)pt_len;
    (void)ct;
    (void)tag;
    NP_PLATFORM_TRAP();
}

np_uhdr_status_t np_uhdr_hal_aes_gcm_decrypt(const uint8_t *key,
                                             const uint8_t *nonce,
                                             const uint8_t *aad,
                                             size_t aad_len,
                                             const uint8_t *ct,
                                             size_t ct_len,
                                             const uint8_t *tag,
                                             uint8_t *pt)
{
    (void)key;
    (void)nonce;
    (void)aad;
    (void)aad_len;
    (void)ct;
    (void)ct_len;
    (void)tag;
    (void)pt;
    NP_PLATFORM_TRAP();
}

np_uhdr_status_t np_uhdr_hal_config_read_ukmd(np_ukmd_record_t *out)
{
    (void)out;
    NP_PLATFORM_TRAP();
}

np_uhdr_status_t np_uhdr_hal_config_write_ukmd(const np_ukmd_record_t *rec)
{
    (void)rec;
    NP_PLATFORM_TRAP();
}

np_uhdr_status_t np_uhdr_hal_mount_uhdr(const uint8_t *ukmd)
{
    (void)ukmd;
    NP_PLATFORM_TRAP();
}

/* ────────────────────────────────────────────────────────────────────────────
 * Zone-announce SAI/ADC (OI-ZA-01..04) — np_zone_announce.h
 * ──────────────────────────────────────────────────────────────────────────*/

bool np_za_platform_adc_read(uint8_t slot_index, uint16_t *out_counts)
{
    (void)slot_index;
    (void)out_counts;
    NP_PLATFORM_TRAP();
}

bool np_za_platform_sai_init(int16_t *dma_buf, uint16_t buf_samples)
{
    (void)dma_buf;
    (void)buf_samples;
    NP_PLATFORM_TRAP();
}

void np_za_platform_sai_start(void)
{
    NP_PLATFORM_TRAP();
}

void np_za_platform_sai_stop(void)
{
    NP_PLATFORM_TRAP();
}
