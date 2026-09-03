/*
 * NeurOne SW-02 Application — DTCM-resident data
 * Document: NP-SW-CI-001 §4.10 (closes OI-SWCI-42)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * The rationale for what lives in DTCM, and why the zeroing below is this
 * image's job rather than the vendored startup's, is in np_app_dtcm.c.
 */

#ifndef NP_APP_DTCM_H
#define NP_APP_DTCM_H

/*
 * Zero the .dtcm_bss region (__dtcm_bss_start__ .. __dtcm_bss_end__).
 *
 * MUST be the first statement of main().  The vendored SDK startup zeroes only
 * __bss_start__ .. __bss_end__ and cannot be taught about a second region
 * without patching a byte-exact vendored file, so anything placed in DTCM as
 * bss-class storage is uninitialised until this runs.
 *
 * Safe to call exactly once and only before the scheduler starts: it writes
 * the whole region, including ucHeap.
 */
void np_app_dtcm_bss_clear(void);

#endif /* NP_APP_DTCM_H */
