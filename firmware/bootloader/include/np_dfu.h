/*
 * NeuroPulse Bootloader — USB-C DFU Recovery
 * Document: NP-FW-EMMC-001 Rev A §8.4
 *
 * DFU recovery is entered when:
 *  (a) Both firmware banks fail signature verification, OR
 *  (b) SNVS_LPGPR0.DFU_FORCED is set (app request or test jig), OR
 *  (c) NP_BOOT_MAX_ATTEMPTS exceeded and rollback bank also fails.
 *
 * Protocol: USB DFU 1.1 (USB class 0xFE, subclass 0x01).
 * The host DFU tool (neuropulse-dfu) authenticates via vendor command before
 * the device accepts any DFU_DNLOAD transfers.
 *
 * DFU writes directly to the inactive bank (or Bank A if both are corrupt).
 * Every received block is SHA-256 verified on completion before the bootloader
 * resets and attempts to boot the newly flashed bank.
 */

#ifndef NP_DFU_H
#define NP_DFU_H

#include "np_types.h"

/* ── Public API ───────────────────────────────────────────────────────────── */

/* Enter USB-C DFU recovery mode. This function does not return under normal  */
/* operation — it loops servicing USB until a valid firmware is received and  */
/* written, then resets the system to boot the new image.                     */
/* If NP_DFU_TIMEOUT_MS elapses with no USB connection, the function returns  */
/* NP_ERR_TIMEOUT so the caller can decide the next action (e.g., halt).      */
np_status_t np_dfu_enter(void);

/* USB interrupt service routine — must be called from USB1_IRQHandler.       */
void np_dfu_usb_isr(void);

#endif /* NP_DFU_H */
