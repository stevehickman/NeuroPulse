/*
 * NeurOne Bootloader — firmware anti-rollback (OI-PRIV-32, NP-PRIV-ANALYSIS-004 Finding M5)
 * Requirement: REQ-SW02-12 (NP-SW-001 §5.2.2)
 *
 * The image header's `version` is covered by the Ed25519 signature, so it can
 * be trusted once the signature verifies. Before this module nothing compared
 * it with anything, so any older signed image — one carrying a flaw a later
 * release fixed — installed over USB DFU or OTA.
 *
 * The rule:
 *   - A monotonic minimum version (the floor) is held by a store behind the
 *     np_rollback_store.h seam.
 *   - An image is admitted for installation only if its signed version is at
 *     or above the floor. Equal is admitted, so the running release can be
 *     reinstalled (recovery DFU of the same build).
 *   - A store that cannot be read refuses every image (fail closed).
 *   - The floor is raised to an image's version only when that image is
 *     confirmed healthy (np_ota_confirm_boot), never at staging: a candidate
 *     that fails its boot trial must still be able to roll back to the bank
 *     it replaced. The floor never goes down.
 *
 * Enforced in both install verify paths in np_ota.c:
 *   np_ota_verify_scratch()            — the OTA app path and USB DFU (np_dfu.c)
 *   np_ota_handle_boot_verification()  — the bootloader's trial of a staged bank
 *
 * NOT enforced on the normal boot of an already-installed, already-confirmed
 * bank (np_main.c verify_bank_header). That bank passed this check when it was
 * installed. See docs/human-input/oi-priv-32-firmware-rollback-floor-and-dfu.md.
 */
#ifndef NP_ROLLBACK_H
#define NP_ROLLBACK_H

#include <stdbool.h>
#include "np_types.h"

/* Pure rule: is `candidate` admissible against `floor`? */
static inline bool np_rollback_version_ok(uint32_t candidate, uint32_t floor)
{
    return candidate >= floor;
}

/* Admit an image whose signature has already verified.
 * Returns NP_OK, NP_ERR_ROLLBACK (version below the floor), or
 * NP_ERR_ROLLBACK_STORE (the floor could not be read: refused, fail closed). */
np_status_t np_rollback_admit(uint32_t signed_version);

/* Raise the floor to `confirmed_version` if it is higher than the floor.
 * Never lowers it. Returns NP_OK, or NP_ERR_ROLLBACK_STORE if the store could
 * not be read or written (the floor is then unchanged, never lowered). */
np_status_t np_rollback_commit(uint32_t confirmed_version);

#endif /* NP_ROLLBACK_H */
