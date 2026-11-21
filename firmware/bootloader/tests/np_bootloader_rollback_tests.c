/*
 * NeurOne Bootloader — firmware anti-rollback, host tests (OI-PRIV-32, NP-PRIV-ANALYSIS-004 Finding M5)
 * Requirement: REQ-SW02-12 (NP-SW-001 §5.2.2)
 *
 * Runs the REAL np_ota.c install paths — np_ota_verify_scratch() (the OTA app
 * path and USB DFU's manifest step) and np_ota_handle_boot_verification() —
 * and the real np_signature.c Ed25519 verifier, against:
 *   - a sparse in-memory eMMC (np_emmc_* below),
 *   - an in-memory SNVS boot selector (np_boot_selector_* below),
 *   - an in-memory rollback-floor store (np_rollback_store_* below), standing
 *     in for the hardware store that has not been chosen.
 * Images are signed with Monocypher under the test key in
 * np_bootloader_test_key.h, the np_bootloader_app_image_tests pattern.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "np_types.h"
#include "np_config.h"
#include "np_emmc.h"
#include "np_boot_selector.h"
#include "np_ota.h"
#include "np_rollback.h"
#include "np_rollback_store.h"
#include "np_signature.h"
#include "monocypher-ed25519.h"

static int g_fail;
#define CHECK(c, m) do { if (c) { printf("PASS: %s\n", m); } else { printf("FAIL: %s (%s:%d)\n", m, __FILE__, __LINE__); g_fail++; } } while (0)

/* ── Sparse fake eMMC: unwritten sectors read as erased (0xFF) ────────────── */
#define FAKE_SECTORS 64U
static struct { int used; uint32_t lba; uint8_t d[NP_EMMC_SECTOR_SIZE]; } g_disk[FAKE_SECTORS];

static int find_sector(uint32_t lba)
{
    for (uint32_t i = 0U; i < FAKE_SECTORS; i++) {
        if (g_disk[i].used && g_disk[i].lba == lba) return (int)i;
    }
    return -1;
}

np_status_t np_emmc_init(void) { return NP_OK; }
np_status_t np_emmc_switch_partition(uint8_t part) { (void)part; return NP_OK; }

np_status_t np_emmc_read(uint32_t lba, void *buf, uint32_t count)
{
    uint8_t *p = buf;
    for (uint32_t s = 0U; s < count; s++) {
        int i = find_sector(lba + s);
        if (i < 0) memset(p + s * NP_EMMC_SECTOR_SIZE, 0xFF, NP_EMMC_SECTOR_SIZE);
        else memcpy(p + s * NP_EMMC_SECTOR_SIZE, g_disk[i].d, NP_EMMC_SECTOR_SIZE);
    }
    return NP_OK;
}

np_status_t np_emmc_write(uint32_t lba, const void *buf, uint32_t count)
{
    const uint8_t *p = buf;
    for (uint32_t s = 0U; s < count; s++) {
        int i = find_sector(lba + s);
        if (i < 0) {
            for (uint32_t k = 0U; k < FAKE_SECTORS; k++) { if (!g_disk[k].used) { i = (int)k; break; } }
            if (i < 0) return NP_ERR_EMMC_WRITE;
            g_disk[i].used = 1; g_disk[i].lba = lba + s;
        }
        memcpy(g_disk[i].d, p + s * NP_EMMC_SECTOR_SIZE, NP_EMMC_SECTOR_SIZE);
    }
    return NP_OK;
}

np_status_t np_emmc_write_verify(uint32_t lba, const void *buf, uint32_t count)
{
    return np_emmc_write(lba, buf, count);
}

np_status_t np_emmc_erase(uint32_t lba, uint32_t count)
{
    for (uint32_t i = 0U; i < FAKE_SECTORS; i++) {
        if (g_disk[i].used && g_disk[i].lba >= lba && (g_disk[i].lba - lba) < count) g_disk[i].used = 0;
    }
    return NP_OK;
}

/* ── Fake SNVS boot selector ──────────────────────────────────────────────── */
static np_bank_t g_bank;
static uint8_t   g_attempts;
static int       g_ota_pending;
static int       g_rolled_back;

np_bank_t np_boot_selector_get_bank(void) { return g_bank; }
np_bank_t np_boot_selector_other_bank(np_bank_t b) { return (b == NP_BANK_A) ? NP_BANK_B : NP_BANK_A; }
uint8_t   np_boot_selector_get_attempts(void) { return g_attempts; }
void      np_boot_selector_inc_attempts(void) { g_attempts++; }
void      np_boot_selector_clear_ota_pending(void) { g_ota_pending = 0; g_attempts = 0U; }
void      np_boot_selector_stage_ota(np_bank_t t) { g_bank = t; g_ota_pending = 1; g_attempts = 0U; }
void      np_boot_selector_confirm_boot(void) { g_ota_pending = 0; g_attempts = 0U; }
bool      np_boot_selector_ota_pending(void) { return g_ota_pending != 0; }
bool      np_boot_selector_dfu_forced(void) { return false; }
void      np_boot_selector_clear_dfu_forced(void) { }
void      np_boot_selector_rollback(np_bank_t stable) { g_bank = stable; g_ota_pending = 0; g_attempts = 0U; g_rolled_back = 1; }

/* ── Fake rollback-floor store (the hardware store is not chosen) ─────────── */
static uint32_t g_floor;
static int      g_store_readable;
static int      g_raises;

np_status_t np_rollback_store_read(uint32_t *floor)
{
    if (!g_store_readable) return NP_ERR_ROLLBACK_STORE;
    *floor = g_floor;
    return NP_OK;
}

np_status_t np_rollback_store_raise(uint32_t floor)
{
    if (!g_store_readable) return NP_ERR_ROLLBACK_STORE;
    g_raises++;
    if (floor > g_floor) g_floor = floor;   /* the contract: never lowered */
    return NP_OK;
}

/* ── Signed test images ───────────────────────────────────────────────────── */
#define IMG_LEN 700U   /* spans two sectors, so the streaming hash crosses one */
static uint8_t g_sk[64];
static uint8_t g_img[IMG_LEN];

static void le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8U); p[2] = (uint8_t)(v >> 16U); p[3] = (uint8_t)(v >> 24U);
}

static void make_header(np_image_header_t *h, uint32_t version)
{
    uint8_t msg[40];
    memset(h, 0, sizeof(*h));
    h->magic = NP_IMAGE_MAGIC;
    h->version = version;
    h->image_size = IMG_LEN;
    h->header_crc32 = np_crc32((const uint8_t *)h, 12U);
    np_sha256(g_img, IMG_LEN, h->image_sha256);
    memcpy(msg, h->image_sha256, 32U);
    le32(&msg[32], version);
    le32(&msg[36], IMG_LEN);
    crypto_ed25519_sign(h->signature, g_sk, msg, sizeof(msg));
}

/* Write header + image at `lba`, the layout of Scratch and of a bank. */
static void put_image(uint32_t lba, const np_image_header_t *h)
{
    uint8_t sec[NP_EMMC_SECTOR_SIZE];
    memset(sec, 0, sizeof(sec));
    memcpy(sec, h, sizeof(*h));
    np_emmc_write(lba, sec, 1U);
    for (uint32_t off = 0U, s = 1U; off < IMG_LEN; off += NP_EMMC_SECTOR_SIZE, s++) {
        uint32_t n = (IMG_LEN - off < NP_EMMC_SECTOR_SIZE) ? IMG_LEN - off : NP_EMMC_SECTOR_SIZE;
        memset(sec, 0, sizeof(sec));
        memcpy(sec, g_img + off, n);
        np_emmc_write(lba + s, sec, 1U);
    }
}

static void stage_scratch(uint32_t version)
{
    np_image_header_t h;
    make_header(&h, version);
    put_image(NP_SCRATCH_LBA_START, &h);
}

static void reset_world(uint32_t floor)
{
    memset(g_disk, 0, sizeof(g_disk));
    g_bank = NP_BANK_A; g_attempts = 0U; g_ota_pending = 0; g_rolled_back = 0;
    g_floor = floor; g_store_readable = 1; g_raises = 0;
}

static void setup_key(void)
{
    uint8_t seed[32], pk[32];
    for (uint32_t i = 0U; i < 32U; i++) seed[i] = (uint8_t)(NP_FW_TEST_SEED_FIRST + i);
    crypto_ed25519_key_pair(g_sk, pk, seed);
    for (uint32_t i = 0U; i < IMG_LEN; i++) g_img[i] = (uint8_t)(i * 7U + 3U);
    if (memcmp(pk, NP_FW_PUBLIC_KEY, 32U) != 0) {
        printf("FAIL: test seed does not derive the key compiled into np_signature.c\n");
        g_fail++;
    }
}

/* ── Tests ────────────────────────────────────────────────────────────────── */

/* @verifies REQ-SW02-12#a */
static void test_rule_boundaries(void)
{
    CHECK(np_rollback_version_ok(5U, 5U), "a version equal to the floor is admitted (reinstall)");
    CHECK(np_rollback_version_ok(6U, 5U), "a version above the floor is admitted");
    CHECK(!np_rollback_version_ok(4U, 5U), "a version one below the floor is refused");
    CHECK(np_rollback_version_ok(0U, 0U), "floor 0 admits version 0");
    CHECK(!np_rollback_version_ok(0U, 0xFFFFFFFFU), "the top floor refuses version 0");
    CHECK(np_rollback_version_ok(0xFFFFFFFFU, 0xFFFFFFFFU), "the top version meets the top floor");
}

/* @verifies REQ-SW02-12#a */
static void test_scratch_path_refuses_older_signed_image(void)
{
    reset_world(0x00020000U);
    stage_scratch(0x00010005U);
    CHECK(np_ota_verify_scratch() == NP_ERR_ROLLBACK,
          "OTA/DFU verify: a correctly signed image below the floor is refused with NP_ERR_ROLLBACK");

    reset_world(0x00020000U);
    stage_scratch(0x00020000U);
    CHECK(np_ota_verify_scratch() == NP_OK, "OTA/DFU verify: a signed image at the floor is admitted");

    reset_world(0x00020000U);
    stage_scratch(0x00020001U);
    CHECK(np_ota_verify_scratch() == NP_OK, "OTA/DFU verify: a signed image above the floor is admitted");
}

/* @verifies REQ-SW02-12#a */
static void test_version_is_trusted_only_under_the_signature(void)
{
    /* Sign version 1, then raise the header's version past the floor and fix
     * the CRC: the version is covered by the signature, so this is refused as
     * a bad signature and never reaches the floor check as "new enough". */
    np_image_header_t h;
    reset_world(5U);
    make_header(&h, 1U);
    h.version = 99U;
    h.header_crc32 = np_crc32((const uint8_t *)&h, 12U);
    put_image(NP_SCRATCH_LBA_START, &h);
    CHECK(np_ota_verify_scratch() == NP_ERR_BAD_SIGNATURE,
          "a header whose version was raised after signing fails the signature, not the floor");
}

/* @verifies REQ-SW02-12#b */
static void test_unreadable_floor_refuses_every_image(void)
{
    reset_world(0U);
    g_store_readable = 0;
    stage_scratch(0xFFFFFFFFU);
    CHECK(np_ota_verify_scratch() == NP_ERR_ROLLBACK_STORE,
          "OTA/DFU verify: with the floor unreadable even the highest signed version is refused");
    CHECK(np_rollback_admit(0U) == NP_ERR_ROLLBACK_STORE, "admit fails closed on an unreadable store");
}

/* @verifies REQ-SW02-12#a */
static void test_boot_trial_rolls_back_a_staged_bank_below_the_floor(void)
{
    /* A bank written below the floor by any means (not through the scratch
     * check) is not booted: the bootloader's trial refuses it and rolls back. */
    np_image_header_t h;
    np_bank_t boot = NP_BANK_A;
    reset_world(10U);
    make_header(&h, 9U);
    put_image(np_emmc_bank_lba(NP_BANK_B), &h);
    np_boot_selector_stage_ota(NP_BANK_B);
    CHECK(np_ota_handle_boot_verification(&boot) == NP_ERR_ROLLBACK,
          "boot trial: a signed staged bank below the floor returns NP_ERR_ROLLBACK");
    CHECK(boot == NP_BANK_A && g_rolled_back && g_bank == NP_BANK_A,
          "boot trial: the selector is rolled back to the stable bank and it is the bank booted");

    reset_world(10U);
    make_header(&h, 10U);
    put_image(np_emmc_bank_lba(NP_BANK_B), &h);
    np_boot_selector_stage_ota(NP_BANK_B);
    boot = NP_BANK_A;
    CHECK(np_ota_handle_boot_verification(&boot) == NP_OK && boot == NP_BANK_B && !g_rolled_back,
          "boot trial: a signed staged bank at the floor is booted");
}

/* @verifies REQ-SW02-12#b */
static void test_boot_trial_with_unreadable_floor_rolls_back(void)
{
    np_image_header_t h;
    np_bank_t boot = NP_BANK_A;
    reset_world(0U);
    g_store_readable = 0;
    make_header(&h, 3U);
    put_image(np_emmc_bank_lba(NP_BANK_B), &h);
    np_boot_selector_stage_ota(NP_BANK_B);
    CHECK(np_ota_handle_boot_verification(&boot) == NP_ERR_ROLLBACK_STORE && boot == NP_BANK_A && g_rolled_back,
          "boot trial: an unreadable floor refuses the candidate and rolls back");
}

/* @verifies REQ-SW02-12#c */
static void test_floor_rises_at_confirmation_not_at_staging(void)
{
    reset_world(4U);
    stage_scratch(7U);
    CHECK(np_ota_verify_scratch() == NP_OK, "a newer image verifies");
    CHECK(np_ota_write_bank(NP_BANK_B) == NP_OK, "it is written to the other bank");
    CHECK(np_ota_stage_swap(NP_BANK_B) == NP_OK, "the swap is staged");
    CHECK(g_floor == 4U && g_raises == 0, "staging does not raise the floor, so a failed trial can still roll back");

    np_bank_t boot = NP_BANK_A;
    CHECK(np_ota_handle_boot_verification(&boot) == NP_OK && boot == NP_BANK_B, "the candidate passes its trial");
    CHECK(g_floor == 4U, "the trial does not raise the floor");

    CHECK(np_ota_confirm_boot() == NP_OK, "confirmation succeeds");
    CHECK(g_floor == 7U, "confirmation raises the floor to the confirmed image's signed version");

    /* The bank it replaced is now below the floor: it cannot be reinstalled. */
    stage_scratch(4U);
    CHECK(np_ota_verify_scratch() == NP_ERR_ROLLBACK, "the replaced release is refused once the newer one is confirmed");
}

/* @verifies REQ-SW02-12#c */
static void test_floor_is_never_lowered(void)
{
    np_image_header_t h;
    reset_world(20U);
    CHECK(np_rollback_commit(19U) == NP_OK && g_floor == 20U && g_raises == 0,
          "committing a version below the floor leaves it unchanged and writes nothing");
    CHECK(np_rollback_commit(20U) == NP_OK && g_floor == 20U && g_raises == 0,
          "committing the floor itself writes nothing");
    CHECK(np_rollback_commit(21U) == NP_OK && g_floor == 21U && g_raises == 1,
          "committing a higher version raises it");

    /* Confirm on a running bank whose (signed) version is below the floor. */
    reset_world(20U);
    make_header(&h, 12U);
    put_image(np_emmc_bank_lba(NP_BANK_A), &h);
    g_bank = NP_BANK_A;
    (void)np_ota_confirm_boot();
    CHECK(g_floor == 20U, "confirming an image below the floor does not lower it");

    /* An unreadable store leaves the floor as it was and reports the error. */
    reset_world(20U);
    make_header(&h, 30U);
    put_image(np_emmc_bank_lba(NP_BANK_A), &h);
    g_store_readable = 0;
    CHECK(np_ota_confirm_boot() == NP_ERR_ROLLBACK_STORE && g_floor == 20U,
          "a store that cannot be read is reported at confirmation and the floor is unchanged");
}

int main(void)
{
    setup_key();
    test_rule_boundaries();
    test_scratch_path_refuses_older_signed_image();
    test_version_is_trusted_only_under_the_signature();
    test_unreadable_floor_refuses_every_image();
    test_boot_trial_rolls_back_a_staged_bank_below_the_floor();
    test_boot_trial_with_unreadable_floor_rolls_back();
    test_floor_rises_at_confirmation_not_at_staging();
    test_floor_is_never_lowered();
    if (g_fail != 0) { printf("FAILED (%d)\n", g_fail); return 1; }
    printf("OK\n");
    return 0;
}
