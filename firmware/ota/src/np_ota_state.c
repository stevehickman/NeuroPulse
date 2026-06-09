/*
 * NeuroPulse OTA — State record CRC + validation implementation.
 * Document: NP-FW-EMMC-001 Rev A §8.3
 *
 * No hardware dependencies — compiles on host for unit tests.
 */

#include "np_ota_state.h"
#include <string.h>

/* ── CRC32 (IEEE 802.3, reflected) ──────────────────────────────────────── */

uint32_t np_ota_state_crc32(const np_ota_state_t *state)
{
    const uint8_t *data = (const uint8_t *)state;
    uint32_t crc = 0xFFFFFFFFUL;

    for (size_t i = 0; i < NP_OTA_CRC_OFFSET; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 1U) {
                crc = (crc >> 1U) ^ NP_OTA_CRC32_POLY;
            } else {
                crc >>= 1U;
            }
        }
    }

    return crc ^ 0xFFFFFFFFUL;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void np_ota_state_init(np_ota_state_t       *state,
                       uint8_t               target_bank,
                       const uint8_t         sha256[NP_OTA_SHA256_SIZE])
{
    memset(state, 0, sizeof(*state));
    state->magic        = NP_OTA_STATE_MAGIC;
    state->target_bank  = target_bank;
    state->boot_attempts = 0;
    state->reserved     = 0;
    memcpy(state->staged_sha256, sha256, NP_OTA_SHA256_SIZE);
    state->crc32        = np_ota_state_crc32(state);
}

np_ota_state_result_t np_ota_state_validate(const np_ota_state_t *state)
{
    if (state->magic != NP_OTA_STATE_MAGIC) {
        return NP_OTA_STATE_BAD_MAGIC;
    }

    if (state->target_bank > 1U) {
        return NP_OTA_STATE_INVALID_BANK;
    }

    uint32_t expected_crc = np_ota_state_crc32(state);
    if (state->crc32 != expected_crc) {
        return NP_OTA_STATE_BAD_CRC;
    }

    /* Check attempt counter AFTER CRC so a corrupt counter doesn't trigger rollback. */
    if (state->boot_attempts >= NP_OTA_BOOT_MAX_ATTEMPTS) {
        return NP_OTA_STATE_MAX_ATTEMPTS;
    }

    return NP_OTA_STATE_OK;
}

bool np_ota_state_increment_attempts(np_ota_state_t *state)
{
    state->boot_attempts++;
    state->crc32 = np_ota_state_crc32(state);
    return (state->boot_attempts < NP_OTA_BOOT_MAX_ATTEMPTS);
}
