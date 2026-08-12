/*
 * NeurOne Bootloader — Boot Bank Selector (SNVS_LPGPR0)
 * Document: NP-FW-EMMC-001 Rev 1 §8.2
 *
 * SNVS_LPGPR0 persists through warm resets and is the authoritative
 * source of boot bank selection and OTA attempt counting.
 */

#ifndef NP_BOOT_SELECTOR_H
#define NP_BOOT_SELECTOR_H

#include "np_types.h"

/* ── Public API ───────────────────────────────────────────────────────────── */

/* Read current requested boot bank from SNVS_LPGPR0.                         */
np_bank_t   np_boot_selector_get_bank(void);

/* Return the bank opposite to the current active bank.                       */
np_bank_t   np_boot_selector_other_bank(np_bank_t bank);

/* Read the current OTA boot-attempt counter (0–3) from SNVS_LPGPR0.         */
uint8_t     np_boot_selector_get_attempts(void);

/* Increment the OTA boot-attempt counter. Called by bootloader on each       */
/* boot of an OTA candidate that has not yet been confirmed by the app.       */
void        np_boot_selector_inc_attempts(void);

/* Reset the attempt counter and OTA_PENDING flag.                            */
/* Called after rollback decision is made.                                    */
void        np_boot_selector_clear_ota_pending(void);

/* Stage an OTA swap: set BOOT_BANK to `target`, set OTA_PENDING, clear       */
/* attempt counter.  Called by application before triggering system reset.   */
void        np_boot_selector_stage_ota(np_bank_t target);

/* Confirm a successful OTA boot. Clears OTA_PENDING and attempt counter.     */
/* Called by application firmware after successful startup + self-test.       */
void        np_boot_selector_confirm_boot(void);

/* Query whether OTA_PENDING is set (new candidate awaiting boot trial).      */
bool        np_boot_selector_ota_pending(void);

/* Query whether DFU_FORCED is set (app or test jig requested DFU entry).     */
bool        np_boot_selector_dfu_forced(void);

/* Clear the DFU_FORCED flag.                                                 */
void        np_boot_selector_clear_dfu_forced(void);

/* Perform rollback: switch BOOT_BANK to the stable (non-OTA) bank, clear     */
/* OTA_PENDING and attempt counter.                                           */
void        np_boot_selector_rollback(np_bank_t stable_bank);

#endif /* NP_BOOT_SELECTOR_H */
