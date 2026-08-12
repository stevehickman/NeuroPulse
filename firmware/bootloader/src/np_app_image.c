/*
 * NeurOne Bootloader — Application Staging Area Limits
 * Document: NP-SW-CI-001 Rev 4 §4.3 (Defect C), phase 2
 *
 * See include/np_app_image.h for why the limit lives in the linker script.
 */

#include "np_app_image.h"

#ifndef NP_APP_IMAGE_HOST_TEST

/*
 * Provided by linker/bootloader_imxrt1062.ld.  These are linker symbols, so
 * their ADDRESS is the value — there is no object to load from, and taking &
 * is the whole read.  np_main.c already uses this idiom for _stack_top.
 *
 * _app_staging_size is an absolute symbol (LENGTH(OCRAM) - _app_load_offset -
 * _stack_size); _app_staging_start is a location inside .app_staging.
 */
extern uint32_t _app_staging_start;
extern uint32_t _app_staging_size;

uint8_t *np_app_staging_base(void)
{
    return (uint8_t *)(uintptr_t)&_app_staging_start;
}

uint32_t np_app_max_image_size(void)
{
    return (uint32_t)(uintptr_t)&_app_staging_size;
}

#endif /* NP_APP_IMAGE_HOST_TEST */

np_status_t np_app_image_size_check(uint32_t image_size, uint32_t staging_size)
{
    /* An empty image has no vector table and no entry point to jump to. */
    if (image_size == 0U) {
        return NP_ERR_IMAGE_TOO_LARGE;
    }

    /* Equality is accepted: an image of exactly staging_size bytes occupies
     * the area to its last byte and touches nothing beyond it.  The reservation
     * is a hard ceiling on application size, so refusing the exact fit would
     * quietly cost a byte of an already fully-allocated OCRAM. */
    if (image_size > staging_size) {
        return NP_ERR_IMAGE_TOO_LARGE;
    }

    return NP_OK;
}
