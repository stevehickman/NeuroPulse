/*
 * NeurOne UHDR key module — internal helpers shared by np_uhdr_key.c and
 * np_uhdr_profile.c.  NOT a platform seam and NOT part of the public API:
 * nothing outside firmware/uhdr_key includes this header.
 *
 * Document: NP-FW-EMMC-002 §C.7 (attempt limit), §C.8 (profiles)
 */

#ifndef NP_UHDR_KEY_INTERNAL_H
#define NP_UHDR_KEY_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "np_uhdr_key_types.h"

/* Non-elidable zeroing of a secret buffer. */
void np_uhdr_secure_zero(void *dst, size_t len);

/*
 * §C.7 attempt limit, step 1: read the persisted count and refuse while the
 * back-off runs.  NP_UHDR_OK fills *failed and *now; NP_UHDR_ERR_CONFIG when
 * the count cannot be read (never read as zero); NP_UHDR_ERR_LOCKOUT while
 * the wait has not elapsed (no KDF, nothing charged).
 */
np_uhdr_status_t np_uhdr_attempt_gate(uint32_t *failed, uint64_t *now);

/*
 * §C.7 step 2: charge this attempt BEFORE the secret is tested.  A charge that
 * cannot be persisted returns NP_UHDR_ERR_CONFIG and the caller must not test.
 */
np_uhdr_status_t np_uhdr_attempt_charge(uint32_t failed, uint64_t now);

/*
 * The §C.7 back-off, for a caller that keeps its own count and reference (the
 * per-profile limit of §C.8, REQ-EMMC2-05): the wait still owed at uptime
 * `now` after `failed` failures, the last charged at uptime `ref_ms`.  Same
 * schedule (NP_UHDR_UNLOCK_*, UC-203) as the helmet-wide limit.
 */
uint64_t np_uhdr_backoff_remaining_ms(uint32_t failed, uint64_t ref_ms, uint64_t now);

/* §C.7 step 3: the secret authenticated, so clear the count (best effort). */
void np_uhdr_attempt_clear(void);

/*
 * The §C.2 wrapper-key derivation: hw_bind(Argon2id(credential, salt, params),
 * fused key).  Intermediate key zeroed; wkmd_out zeroed on failure.
 */
np_uhdr_status_t np_uhdr_derive_wkmd(const uint8_t *credential, size_t cred_len,
                                     const uint8_t *salt, uint32_t version,
                                     uint32_t m_cost, uint32_t t_cost,
                                     uint32_t parallelism, uint8_t *wkmd_out);

#endif /* NP_UHDR_KEY_INTERNAL_H */
