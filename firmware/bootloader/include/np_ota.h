/*
 * NeuroPulse Bootloader — OTA Update State Machine
 * Document: NP-FW-EMMC-001 Rev A §8.3 (9-step OTA sequence)
 *
 * OTA flow (steps 1–9 from spec):
 *  Step 1  App signals OTA intent → Safety MCU confirmed
 *  Step 2  App downloads firmware image into Scratch partition (via USB-C or BT)
 *  Step 3  App calls np_ota_verify_scratch() → Ed25519 + SHA-256 verification
 *  Step 4  App calls np_ota_write_bank() → write Scratch → inactive bank
 *          with sector-level SHA-256 readback verification
 *  Step 5  App calls np_ota_stage_swap(target_bank) → sets SNVS_LPGPR0 flags
 *  Step 6  np_ota_stage_swap() also writes np_ota_state_t to Config partition
 *  Step 7  App triggers system reset
 *  Step 8  Bootloader reads SNVS, verifies new bank, boots if good
 *  Step 9  App calls np_ota_confirm_boot() to finalise; bootloader handles
 *          rollback automatically after NP_BOOT_MAX_ATTEMPTS failures
 *
 * Steps 1–7 are application-side (app calls into this API).
 * Steps 8–9 are bootloader-side (np_ota_handle_boot_verification()).
 */

#ifndef NP_OTA_H
#define NP_OTA_H

#include "np_types.h"

/* ── Application-side OTA API ────────────────────────────────────────────── */

/* Step 3: Verify a complete firmware image staged in the Scratch partition.  */
/* Returns NP_OK, or NP_ERR_BAD_MAGIC / _SIGNATURE / _IMAGE_HASH on failure. */
np_status_t np_ota_verify_scratch(void);

/* Step 4: Copy verified image from Scratch to `target` bank with readback.  */
/* Erases the target bank first, then writes and verifies every sector.       */
/* Returns NP_ERR_EMMC_WRITE on any write failure.                           */
np_status_t np_ota_write_bank(np_bank_t target);

/* Step 5+6: Arm the SNVS swap flag and persist OTA state to Config.          */
/* After this call, the system should be reset immediately.                   */
np_status_t np_ota_stage_swap(np_bank_t target);

/* Step 9 (app side): Confirm that the booted OTA firmware is healthy.        */
/* Clears OTA_PENDING in SNVS and erases the OTA state record in Config.      */
/* Must be called within NP_OTA_CONFIRM_TIMEOUT_S seconds of system reset.   */
np_status_t np_ota_confirm_boot(void);

/* ── Bootloader-side OTA API ─────────────────────────────────────────────── */

/* Step 8+9 (bootloader side): Called by main boot flow when OTA_PENDING set. */
/* Reads OTA state from Config, verifies the staged bank, handles rollback    */
/* if attempt counter >= NP_BOOT_MAX_ATTEMPTS.                                */
/* Returns NP_OK and sets *boot_bank if a valid bank was selected.            */
/* Returns NP_ERR_MAX_ATTEMPTS after rollback is performed.                  */
np_status_t np_ota_handle_boot_verification(np_bank_t *boot_bank);

/* Read the persisted OTA state from the Config partition.                    */
/* Returns NP_OK and fills *state, or error if state is absent/corrupt.       */
np_status_t np_ota_read_state(np_ota_state_t *state);

/* Write the OTA state record to the Config partition.                        */
np_status_t np_ota_write_state(const np_ota_state_t *state);

/* Erase the OTA state record from Config (called on confirm or rollback).    */
np_status_t np_ota_erase_state(void);

#endif /* NP_OTA_H */
