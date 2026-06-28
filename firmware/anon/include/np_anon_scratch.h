/*
 * NeuroPulse Research Anonymization — Scratch Partition AES-256-CTR API
 * Document: NP-FW-EMMC-002 Rev A §D
 *
 * Encrypts every block written to the eMMC Scratch partition during a
 * research anonymization run, so that no decrypted UHDR data persists in
 * plaintext if the device loses power mid-pipeline.
 *
 * Threat model (NP-FW-EMMC-002 §D.1):
 *   - The anonymization pipeline decrypts selected UHDR elements, transforms
 *     them, and stages intermediate results in the Scratch partition.
 *   - The per-run AES-256-CTR key (k_scratch) and nonce are generated from the
 *     TRNG and held ONLY in SRAM.  They are never written to eMMC.
 *   - On normal completion the key is zeroized and the Scratch partition is
 *     SANITIZE'd.
 *   - On power loss the SRAM key is gone; even if the Scratch ciphertext is
 *     physically recovered it cannot be decrypted.  The bootloader additionally
 *     SANITIZEs Scratch on the next boot (NP_SNVS_ANON_IN_PROGRESS flag).
 *
 * Usage (single run):
 *   np_anon_scratch_ctx_t ctx;
 *   if (np_anon_scratch_init(&ctx) != NP_ANON_OK) { abort; }
 *   ... np_anon_scratch_write(&ctx, off, plaintext, len) ...
 *   ... np_anon_scratch_read(&ctx, off, out, len) ...
 *   np_anon_scratch_complete(&ctx);   // zeroise key, SANITIZE scratch
 *
 * No heap.  The caller owns the context (typically a stack or static instance).
 */

#ifndef NP_ANON_SCRATCH_H
#define NP_ANON_SCRATCH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "np_anon_types.h"
#include "np_anon_config.h"

/* ── Per-run encryption context ───────────────────────────────────────────── */
/*
 * k_scratch and nonce are SRAM-only secrets.  They MUST NOT be written to any
 * eMMC partition, logged, or copied to non-volatile storage.  They are zeroized
 * by np_anon_scratch_complete() with memset_explicit.
 */
typedef struct {
    uint8_t  k_scratch[NP_ANON_KEY_LEN];   /* AES-256-CTR key — SRAM only      */
    uint8_t  nonce[NP_ANON_NONCE_LEN];     /* CTR nonce — TRNG, unique per run */
    uint32_t session_id;                   /* anonymization session counter    */
    bool     key_valid;                    /* true between init and complete   */
} np_anon_scratch_ctx_t;

/* ── Lifecycle ───────────────────────────────────────────────────────────────
 *
 * np_anon_scratch_init
 *   Generate a fresh per-run key and nonce from the TRNG, set the
 *   NP_SNVS_ANON_IN_PROGRESS flag (before any scratch write so an interrupted
 *   run is always detectable), and mark the context valid.
 *
 *   On TRNG failure the partially-filled key material is zeroized and an error
 *   is returned; key_valid stays false.  The caller must not have its
 *   session_id reset — init does not touch session_id (it is bumped only on a
 *   successful complete) so a pre-zeroed/static context starts at session 0.
 *
 *   Returns NP_ANON_ERR_INVALID if ctx is NULL.
 *   Returns NP_ANON_ERR_TRNG    if the TRNG fails for key or nonce.
 *   Returns NP_ANON_OK          on success.
 */
np_anon_status_t np_anon_scratch_init(np_anon_scratch_ctx_t *ctx);

/*
 * np_anon_scratch_write
 *   Encrypt one block of plaintext under (k_scratch, nonce, block_offset) and
 *   write the ciphertext to the Scratch partition at block_offset.
 *
 *   block_offset is the Scratch-relative block index (0-based); it selects both
 *   the CTR counter base and the eMMC LBA (NP_SCRATCH_LBA_START + block_offset).
 *   len must be in (0, NP_ANON_SCRATCH_BLOCK_SIZE].
 *
 *   The ciphertext is staged in a stack buffer that is memset_explicit-cleared
 *   before return.
 *
 *   Returns NP_ANON_ERR_INVALID if ctx/plaintext is NULL, key_valid is false,
 *                               len is 0, len exceeds the block size, or
 *                               block_offset is outside the Scratch partition.
 *   Returns NP_ANON_ERR_ENCRYPT if AES-CTR fails.
 *   Returns NP_ANON_ERR_EMMC    if the eMMC write fails.
 *   Returns NP_ANON_OK          on success.
 */
np_anon_status_t np_anon_scratch_write(np_anon_scratch_ctx_t *ctx,
                                        uint32_t block_offset,
                                        const uint8_t *plaintext,
                                        size_t len);

/*
 * np_anon_scratch_read
 *   Read one ciphertext block from the Scratch partition at block_offset and
 *   decrypt it into plaintext_out.  AES-CTR is symmetric, so this is the same
 *   transform as write applied to the ciphertext.
 *
 *   len must be in (0, NP_ANON_SCRATCH_BLOCK_SIZE]; arguments are validated as
 *   in np_anon_scratch_write.  The ciphertext stack buffer is memset_explicit-
 *   cleared before return.
 *
 *   Returns the same status codes as np_anon_scratch_write.
 */
np_anon_status_t np_anon_scratch_read(np_anon_scratch_ctx_t *ctx,
                                       uint32_t block_offset,
                                       uint8_t *plaintext_out,
                                       size_t len);

/*
 * np_anon_scratch_complete
 *   Normal end-of-run teardown: zeroise k_scratch and nonce (memset_explicit),
 *   mark the context invalid, bump session_id, SANITIZE the Scratch partition,
 *   then clear the NP_SNVS_ANON_IN_PROGRESS flag (cleared last so an
 *   interrupted SANITIZE is still re-attempted by the bootloader).
 *
 *   Best-effort: secret destruction always happens even if SANITIZE reports an
 *   error; there is no recoverable failure path once a run ends, so this
 *   returns void.
 */
void np_anon_scratch_complete(np_anon_scratch_ctx_t *ctx);

/*
 * np_anon_scratch_resume_after_powerloss
 *   Called by the bootloader when NP_SNVS_ANON_IN_PROGRESS is set on boot.
 *   The SRAM key is already gone (power-off), so the staged Scratch ciphertext
 *   is unreadable; SANITIZE it regardless to remove the bytes, then clear the
 *   flag.  No context is required — there is no surviving key to destroy.
 */
void np_anon_scratch_resume_after_powerloss(void);

/* ── Platform HAL stubs ──────────────────────────────────────────────────────
 *
 * Implemented by the platform integration layer (NPTEST_HOST builds provide
 * no-op/echo stubs in np_anon_scratch.c).  Each returns NP_ANON_OK on success.
 */

/* OI-ANON-AES-01: fill buf with len bytes from the hardware TRNG (TRNG/RNGB). */
extern np_anon_status_t np_anon_hal_trng_generate(uint8_t *buf, size_t len);

/*
 * OI-ANON-AES-02: AES-256-CTR transform (symmetric — same call encrypts and
 * decrypts).  `in` and `out` may alias.  Uses the CAAM/DCP AES engine on target.
 *
 * Counter construction (CONFIDENTIALITY-CRITICAL): the 128-bit AES counter for
 * the first 16-byte sub-block of this scratch block is `nonce` with its low 32
 * bits set big-endian to
 *     block_offset * NP_ANON_AES_BLOCKS_PER_SCRATCH_BLOCK
 * and incremented by one per subsequent 16-byte sub-block.  Multiplying by the
 * blocks-per-scratch-block factor gives each scratch block a DISJOINT window of
 * counter values; using block_offset directly would overlap the keystream of
 * adjacent blocks (block N would reuse counters already consumed by block N-1),
 * which leaks plaintext via ciphertext XOR.  The HAL MUST apply this scaling.
 */
extern np_anon_status_t np_anon_hal_aes_ctr_crypt(const uint8_t *key,
                                                  const uint8_t *nonce,
                                                  uint32_t block_offset,
                                                  const uint8_t *in,
                                                  uint8_t *out,
                                                  size_t len);

/* OI-ANON-AES-03: write len bytes to Scratch at the given block index.       */
extern np_anon_status_t np_anon_hal_emmc_write_scratch(uint32_t block_offset,
                                                       const uint8_t *data,
                                                       size_t len);

/* OI-ANON-AES-04: read len bytes from Scratch at the given block index.      */
extern np_anon_status_t np_anon_hal_emmc_read_scratch(uint32_t block_offset,
                                                      uint8_t *data,
                                                      size_t len);

/* OI-ANON-AES-05: SANITIZE (secure-erase) the entire Scratch partition.      */
extern np_anon_status_t np_anon_hal_emmc_sanitize_scratch(void);

#endif /* NP_ANON_SCRATCH_H */
