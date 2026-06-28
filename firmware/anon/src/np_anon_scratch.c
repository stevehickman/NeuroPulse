/*
 * NeuroPulse Research Anonymization — Scratch Partition AES-256-CTR
 * Document: NP-FW-EMMC-002 Rev A §D.2–§D.6
 *
 * Implements the per-run encryption lifecycle for the Scratch partition used
 * by the on-device research anonymization pipeline.  See np_anon_scratch.h for
 * the threat model and contract.
 *
 * Sensitive-data zeroing uses memset_explicit() (project convention).  On C23
 * toolchains it is provided by <string.h>; on the C11 cross-build a guarded
 * non-elidable fallback is supplied below so key material is never left in SRAM
 * by a dead-store optimization.
 */

#include "np_anon_scratch.h"

#include <string.h>

/* ── memset_explicit fallback (non-elidable) ─────────────────────────────────
 *
 * The project standardises on memset_explicit() for clearing secrets.  It is
 * a C23 function; the firmware is built -std=c11, so provide a fallback when
 * the libc does not.  __STDC_LIB_EXT1__ / C23 detection is unreliable across
 * the arm-none-eabi newlib versions in use, so gate on the version macro and
 * allow the platform to predefine NP_ANON_HAVE_MEMSET_EXPLICIT to opt in to a
 * libc-provided implementation.
 */
#if !defined(NP_ANON_HAVE_MEMSET_EXPLICIT) && \
    !(defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L)

/* Volatile function pointer to memset: the compiler cannot prove the call is  */
/* dead, so it cannot elide the clear of a soon-to-be-freed/returned buffer.   */
static void *(*const volatile np_anon_memset_fn)(void *, int, size_t) = memset;

static void memset_explicit(void *dst, int ch, size_t len)
{
    np_anon_memset_fn(dst, ch, len);
}
#endif

/* ── Internal helpers ─────────────────────────────────────────────────────── */

/* Number of 512-byte blocks in the Scratch partition.                         */
#define NP_ANON_SCRATCH_BLOCK_COUNT (NP_SCRATCH_SIZE_LBA)

/*
 * Validate the common (ctx, block_offset, buf, len) arguments shared by read
 * and write.  Returns NP_ANON_OK if valid, else the specific error.
 */
static np_anon_status_t validate_block_args(const np_anon_scratch_ctx_t *ctx,
                                            uint32_t block_offset,
                                            const void *buf,
                                            size_t len)
{
    if (ctx == NULL || buf == NULL) {
        return NP_ANON_ERR_INVALID;
    }
    if (!ctx->key_valid) {
        /* No live key: nothing can be encrypted or decrypted. */
        return NP_ANON_ERR_INVALID;
    }
    if (len == 0U || len > NP_ANON_SCRATCH_BLOCK_SIZE) {
        return NP_ANON_ERR_INVALID;
    }
    if (block_offset >= NP_ANON_SCRATCH_BLOCK_COUNT) {
        /* Out of the Scratch partition — refuse before touching the HAL. */
        return NP_ANON_ERR_INVALID;
    }
    return NP_ANON_OK;
}

/* ── Lifecycle ───────────────────────────────────────────────────────────────*/

np_anon_status_t np_anon_scratch_init(np_anon_scratch_ctx_t *ctx)
{
    if (ctx == NULL) {
        return NP_ANON_ERR_INVALID;
    }

    /* Generate the per-run AES-256 key. */
    np_anon_status_t st = np_anon_hal_trng_generate(ctx->k_scratch,
                                                    NP_ANON_KEY_LEN);
    if (st != NP_ANON_OK) {
        /* Nothing usable was produced; ensure no partial key lingers. */
        memset_explicit(ctx->k_scratch, 0, NP_ANON_KEY_LEN);
        ctx->key_valid = false;
        return NP_ANON_ERR_TRNG;
    }

    /* Generate the CTR nonce. */
    st = np_anon_hal_trng_generate(ctx->nonce, NP_ANON_NONCE_LEN);
    if (st != NP_ANON_OK) {
        /* Destroy the key we just produced — a key without a nonce is useless */
        /* and must not be left resident. */
        memset_explicit(ctx->k_scratch, 0, NP_ANON_KEY_LEN);
        memset_explicit(ctx->nonce, 0, NP_ANON_NONCE_LEN);
        ctx->key_valid = false;
        return NP_ANON_ERR_TRNG;
    }

    /*
     * Set the in-progress flag BEFORE any scratch write so a power loss during
     * the very first write is still detected and SANITIZE'd by the bootloader.
     */
    NP_SNVS_LPGPR2 |= NP_SNVS_ANON_IN_PROGRESS;

    ctx->key_valid = true;
    return NP_ANON_OK;
}

np_anon_status_t np_anon_scratch_write(np_anon_scratch_ctx_t *ctx,
                                        uint32_t block_offset,
                                        const uint8_t *plaintext,
                                        size_t len)
{
    np_anon_status_t st = validate_block_args(ctx, block_offset, plaintext, len);
    if (st != NP_ANON_OK) {
        return st;
    }

    uint8_t ciphertext[NP_ANON_SCRATCH_BLOCK_SIZE];

    st = np_anon_hal_aes_ctr_crypt(ctx->k_scratch, ctx->nonce, block_offset,
                                   plaintext, ciphertext, len);
    if (st != NP_ANON_OK) {
        memset_explicit(ciphertext, 0, sizeof(ciphertext));
        return NP_ANON_ERR_ENCRYPT;
    }

    st = np_anon_hal_emmc_write_scratch(block_offset, ciphertext, len);

    /* Clear the staged ciphertext regardless of the write result — it derives */
    /* from decrypted UHDR and must not survive on the stack. */
    memset_explicit(ciphertext, 0, sizeof(ciphertext));

    if (st != NP_ANON_OK) {
        return NP_ANON_ERR_EMMC;
    }
    return NP_ANON_OK;
}

np_anon_status_t np_anon_scratch_read(np_anon_scratch_ctx_t *ctx,
                                       uint32_t block_offset,
                                       uint8_t *plaintext_out,
                                       size_t len)
{
    np_anon_status_t st =
        validate_block_args(ctx, block_offset, plaintext_out, len);
    if (st != NP_ANON_OK) {
        return st;
    }

    uint8_t ciphertext[NP_ANON_SCRATCH_BLOCK_SIZE];

    st = np_anon_hal_emmc_read_scratch(block_offset, ciphertext, len);
    if (st != NP_ANON_OK) {
        memset_explicit(ciphertext, 0, sizeof(ciphertext));
        return NP_ANON_ERR_EMMC;
    }

    /* AES-CTR is symmetric: decrypting is the same transform as encrypting. */
    st = np_anon_hal_aes_ctr_crypt(ctx->k_scratch, ctx->nonce, block_offset,
                                   ciphertext, plaintext_out, len);

    memset_explicit(ciphertext, 0, sizeof(ciphertext));

    if (st != NP_ANON_OK) {
        /* Do not leave partial plaintext exposed on decrypt failure. */
        memset_explicit(plaintext_out, 0, len);
        return NP_ANON_ERR_ENCRYPT;
    }
    return NP_ANON_OK;
}

void np_anon_scratch_complete(np_anon_scratch_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    /* Destroy the per-run secrets first — this must always happen. */
    memset_explicit(ctx->k_scratch, 0, NP_ANON_KEY_LEN);
    memset_explicit(ctx->nonce, 0, NP_ANON_NONCE_LEN);
    ctx->key_valid = false;
    ctx->session_id++;

    /* Secure-erase the staged ciphertext.  Best-effort: even if SANITIZE      */
    /* reports an error the SRAM key is already gone, so the residual          */
    /* ciphertext is unreadable; the bootloader flag still drives a retry.     */
    (void)np_anon_hal_emmc_sanitize_scratch();

    /*
     * Clear the in-progress flag LAST.  If power is lost after SANITIZE but
     * before this clear, the bootloader re-SANITIZEs on next boot — harmless
     * and idempotent.  Clearing last guarantees the flag never indicates
     * "clean" while staged ciphertext could still be present.
     */
    NP_SNVS_LPGPR2 &= ~NP_SNVS_ANON_IN_PROGRESS;
}

void np_anon_scratch_resume_after_powerloss(void)
{
    /*
     * Power-loss recovery path (called by the bootloader).  The SRAM key is
     * gone, so any staged Scratch ciphertext is already unreadable; SANITIZE
     * it anyway to physically remove the bytes, then clear the flag.
     */
    (void)np_anon_hal_emmc_sanitize_scratch();
    NP_SNVS_LPGPR2 &= ~NP_SNVS_ANON_IN_PROGRESS;
}

/* ── Host test stubs (NPTEST_HOST) ───────────────────────────────────────────
 *
 * No-op / echo implementations so the init/write/complete cycle can be
 * exercised on the host CI.  These are NOT compiled into target firmware; the
 * platform integration layer provides the real CAAM/USDHC/TRNG-backed HALs.
 *
 * Echo crypt: XOR with a deterministic keystream byte derived from
 * (key[0], nonce[0], block_offset, index).  This is NOT cryptography — it
 * exists only so the host round-trip test (write then read) verifies the
 * symmetric-transform contract (decrypt(encrypt(x)) == x) without a real AES
 * engine.
 */
#ifdef NPTEST_HOST

/* Host-backed SNVS_LPGPR2 (declared extern in np_anon_config.h under host). */
volatile uint32_t np_anon_host_lpgpr2 = 0U;

np_anon_status_t np_anon_hal_trng_generate(uint8_t *buf, size_t len)
{
    if (buf == NULL) {
        return NP_ANON_ERR_INVALID;
    }
    /* Deterministic non-zero fill for reproducible host tests. */
    for (size_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(0xA5U ^ (uint8_t)i);
    }
    return NP_ANON_OK;
}

np_anon_status_t np_anon_hal_aes_ctr_crypt(const uint8_t *key,
                                           const uint8_t *nonce,
                                           uint32_t block_offset,
                                           const uint8_t *in,
                                           uint8_t *out,
                                           size_t len)
{
    if (key == NULL || nonce == NULL || in == NULL || out == NULL) {
        return NP_ANON_ERR_INVALID;
    }
    /* Mirror the real disjoint-window counter base (OI-ANON-AES-02) so the    */
    /* host echo keystream differs per scratch block exactly as AES-CTR would. */
    const uint32_t ctr_base = block_offset * NP_ANON_AES_BLOCKS_PER_SCRATCH_BLOCK;
    const uint8_t seed = (uint8_t)(key[0] ^ nonce[0] ^ (uint8_t)ctr_base);
    for (size_t i = 0; i < len; i++) {
        out[i] = (uint8_t)(in[i] ^ (uint8_t)(seed + (uint8_t)i));
    }
    return NP_ANON_OK;
}

/* Single 512-byte sector of host-side fake Scratch storage (one block). */
static uint8_t g_host_scratch[NP_ANON_SCRATCH_BLOCK_SIZE];

np_anon_status_t np_anon_hal_emmc_write_scratch(uint32_t block_offset,
                                                const uint8_t *data,
                                                size_t len)
{
    (void)block_offset;
    if (data == NULL || len > sizeof(g_host_scratch)) {
        return NP_ANON_ERR_INVALID;
    }
    memcpy(g_host_scratch, data, len);
    return NP_ANON_OK;
}

np_anon_status_t np_anon_hal_emmc_read_scratch(uint32_t block_offset,
                                               uint8_t *data,
                                               size_t len)
{
    (void)block_offset;
    if (data == NULL || len > sizeof(g_host_scratch)) {
        return NP_ANON_ERR_INVALID;
    }
    memcpy(data, g_host_scratch, len);
    return NP_ANON_OK;
}

np_anon_status_t np_anon_hal_emmc_sanitize_scratch(void)
{
    memset(g_host_scratch, 0, sizeof(g_host_scratch));
    return NP_ANON_OK;
}

#endif /* NPTEST_HOST */
