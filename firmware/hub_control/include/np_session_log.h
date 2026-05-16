/*
 * NeuroPulse Hub Control Program — Session Logger (UHDR / SHDR)
 * Document: NP-FW-HUB-001 Rev A §6
 *
 * Routes telemetry and session records to the correct eMMC partition per the
 * data classification table in NP-FW-EMMC-001 Rev A §12.
 * UHDR writes are AES-256-XTS encrypted with the user biometric-derived key.
 * SHDR writes use the HKDF manufacturing key.
 *
 * HAL stubs OI-LOG-01 through OI-LOG-04 require platform implementation:
 *   OI-LOG-01: np_log_hal_uhdr_append(buf, len) → LittleFS UHDR partition
 *   OI-LOG-02: np_log_hal_shdr_append(buf, len) → LittleFS SHDR partition
 *   OI-LOG-03: np_log_hal_uhdr_flush()           → fsync UHDR
 *   OI-LOG-04: np_log_hal_shdr_flush()           → fsync SHDR
 */

#ifndef NP_SESSION_LOG_H
#define NP_SESSION_LOG_H

#include "np_hub_types.h"

/* ── Log record type tags (written as first byte of each serialised record) ───── */

#define NP_LOG_TAG_UHDR_SESSION_START   0x10U
#define NP_LOG_TAG_UHDR_SESSION_END     0x11U
#define NP_LOG_TAG_UHDR_EEG_BAND        0x12U
#define NP_LOG_TAG_UHDR_PBM_DOSE        0x13U
#define NP_LOG_TAG_UHDR_VNS_HRV         0x14U
#define NP_LOG_TAG_UHDR_STIM            0x15U
#define NP_LOG_TAG_UHDR_VISUAL          0x16U
#define NP_LOG_TAG_UHDR_EEG_IMPEDANCE   0x17U

#define NP_LOG_TAG_SHDR_SESSION_END     0x80U
#define NP_LOG_TAG_SHDR_PBM_HEALTH      0x81U
#define NP_LOG_TAG_SHDR_FAULT           0x82U
#define NP_LOG_TAG_SHDR_ZONE_AUTH       0x83U
#define NP_LOG_TAG_SHDR_NTC_PEAK        0x84U
#define NP_LOG_TAG_SHDR_EEG_CAL         0x85U

/* ── API ─────────────────────────────────────────────────────────────────────── */

/*
 * np_log_init — initialise logger buffers; call once at hub startup.
 * Reads device_session_count from SHDR for SHDR record headers.
 */
void np_log_init(uint32_t device_session_count);

/*
 * np_log_session_start — write UHDR session-start record.
 * Call immediately before np_runner_run() begins execution.
 */
void np_log_session_start(const np_session_uhdr_record_t *rec);

/*
 * np_log_session_end — write UHDR session-end and SHDR session-end records.
 * Call after np_runner_run() returns.
 */
void np_log_session_end(const np_session_uhdr_record_t *uhdr_rec,
                         const np_session_shdr_record_t *shdr_rec);

/*
 * np_log_telemetry — route a module telemetry snapshot to the correct partition.
 * EEG waveform data is logged separately via np_log_eeg_sample_block().
 */
void np_log_telemetry(const np_telem_record_t *rec);

/*
 * np_log_eeg_sample_block — append a block of raw EEG samples to the UHDR
 * EDF+ channel file.  samples[] is 24-bit big-endian ADS1299 output,
 * n_samples × NP_EEG_CHANNELS × NP_EEG_SAMPLE_BYTES bytes.
 */
void np_log_eeg_sample_block(const uint8_t *samples,
                              uint16_t       n_samples,
                              uint32_t       session_ms);

/*
 * np_log_shdr_zone_auth — write zone module authentication result to SHDR.
 * Called by np_mod_reg_scan() shdr_log_cb.
 */
void np_log_shdr_zone_auth(uint8_t slot, np_hub_mod_type_t type, bool pass);

/*
 * np_log_shdr_fault — write a module fault event to SHDR.
 * Contains only device-condition data: no HR, no EEG values.
 */
void np_log_shdr_fault(uint8_t slot, np_hub_mod_type_t type,
                        uint8_t fault_code, uint32_t session_ms);

/*
 * np_log_flush — flush any pending UHDR and SHDR buffers to eMMC.
 * Called on session end and periodically by the hub control task.
 */
void np_log_flush(void);

/* ── HAL stubs (OI-LOG-01 through OI-LOG-04) ─────────────────────────────────── */

extern np_hub_status_t np_log_hal_uhdr_append(const uint8_t *buf, size_t len);
extern np_hub_status_t np_log_hal_shdr_append(const uint8_t *buf, size_t len);
extern np_hub_status_t np_log_hal_uhdr_flush(void);
extern np_hub_status_t np_log_hal_shdr_flush(void);

#endif /* NP_SESSION_LOG_H */
