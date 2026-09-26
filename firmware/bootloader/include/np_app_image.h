/*
 * NeurOne Bootloader — Application Staging Area Limits
 * Document: NP-SW-CI-001 Rev 4 §4.3 (Defect C), phase 2
 *
 * The bootloader copies an application image out of its eMMC bank into the
 * OCRAM staging area and executes it in place.  Two facts about that area —
 * where it starts and how big it is — are owned by
 * linker/bootloader_imxrt1062.ld, which is the only place in the build that
 * knows the whole OCRAM map.  This header is how C obtains them.
 *
 * ── Why this file exists ─────────────────────────────────────────────────────
 *
 * It did not, and np_main.c recomputed the limit itself:
 *
 *     uint32_t remaining_ocram = NP_OCRAM_SIZE - (NP_APP_LOAD_ADDR - NP_FW_LOAD_ADDR);
 *
 * — 512K - 64K = 448K, the same wrong arithmetic as the linker script's inline
 * "512 - 64 = 448", derived independently and omitting the 8 KiB stack in the
 * same way.  In the linker script that was a loud link failure.  Here it was
 * silent: load_and_jump() accepted an image up to 448 KiB and np_emmc_read()
 * it to the staging base, so an image in the 440–448 KiB band overwrote the
 * 8 KiB stack the bootloader was executing on — after Ed25519 verification had
 * already passed, at the point where the boot record says the image is good.
 *
 * The fix is not two correct constants; it is one.  np_app_max_image_size()
 * reports what the linker actually reserved, so C cannot disagree with the
 * memory map by construction.
 */

#ifndef NP_APP_IMAGE_H
#define NP_APP_IMAGE_H

#include <stdint.h>

#include "np_types.h"

/*
 * Base address of the OCRAM application staging area (_app_staging_start).
 *
 * NOT built for host tests: on a host there is no linker script to read, so any
 * stand-in would let a test pass while the device binding was broken.  The
 * binding is verified on the artifact instead — `arm-none-eabi-nm` on the ELF,
 * asserting _app_staging_size == 0x6e000 and _app_staging_end == _stack_base.
 */
#ifndef NP_APP_IMAGE_HOST_TEST
uint8_t *np_app_staging_base(void);

/*
 * Largest application image the staging area can hold, in bytes
 * (_app_staging_size).  Currently 440 KiB — but that number is derived by the
 * linker from LENGTH(OCRAM) - _app_load_offset - _stack_size and is not
 * written down anywhere, here or there.
 */
uint32_t np_app_max_image_size(void);
#endif /* NP_APP_IMAGE_HOST_TEST */

/*
 * Is image_size loadable into a staging area of staging_size bytes?
 *
 * Returns NP_OK, or NP_ERR_IMAGE_TOO_LARGE for an image that is empty (nothing
 * to execute) or larger than the area (the copy would run past its end — which
 * for this layout means over the stack).
 *
 * Pure in both arguments and deliberately separate from np_app_max_image_size()
 * so the boundary is testable on a host with no linker script.  The limit it is
 * checked against is passed in, never recomputed.
 */
np_status_t np_app_image_size_check(uint32_t image_size, uint32_t staging_size);

/*
 * Is the image now sitting in the staging area the image the header signs?
 * NP-SW-CI-001 OI-SWCI-15.
 *
 * Called by load_and_jump() AFTER the copy out of the eMMC bank and BEFORE the
 * jump.  verify_bank_header() authenticates the header in the bank and
 * nothing more — it passes NULL image data, so no byte of the image itself was
 * ever hashed at boot.  A bit flip in the bank, a short or wrong eMMC read, or
 * a copy that landed somewhere else would all have reached the entry point
 * under a boot record that says "verified".  This closes that: the bytes that
 * are about to execute are hashed where they will execute, and the header that
 * names their hash is re-authenticated in the same call — so the header
 * load_and_jump() re-read is itself verified, not trusted because an earlier
 * read of the same sector was.
 *
 * Returns NP_OK, or the first failure: the staging size check above, then
 * np_signature_verify()'s header CRC / magic / size / image hash / Ed25519
 * results.  `staged` NULL is refused — the NULL-means-skip-the-hash convention
 * of np_signature_verify() must never be reachable from the jump path.
 */
np_status_t np_app_image_verify_staged(const np_image_header_t *hdr,
                                       const uint8_t *staged,
                                       uint32_t staging_size);

#endif /* NP_APP_IMAGE_H */
