/*
 * NeuroPulse OTA State — Host Smoke Tests
 * Document: NP-FW-EMMC-001 Rev A §8.3
 *
 * Tests the host-compilable np_ota_state logic: CRC32 computation, magic
 * validation, boot-attempt counter, and rollback threshold detection.
 *
 * NOT hardware verification — real eMMC read/write, SNVS flag, and bank-swap
 * paths require bench testing on the i.MX RT1062 target.
 */

#include "np_ota_state.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>

static int g_failures = 0;

static void check(int cond, const char *name)
{
    if (cond) {
        printf("PASS: %s\n", name);
    } else {
        printf("FAIL: %s\n", name);
        g_failures++;
    }
}

/* Helper: build a valid state with a predictable SHA-256 */
static np_ota_state_t make_valid_state(uint8_t bank)
{
    np_ota_state_t s;
    uint8_t sha[NP_OTA_SHA256_SIZE];
    for (int i = 0; i < (int)NP_OTA_SHA256_SIZE; i++) {
        sha[i] = (uint8_t)(0xA0 + i);
    }
    np_ota_state_init(&s, bank, sha);
    return s;
}

int main(void)
{
    /* ── 1: Valid state passes validation ──────────────────────────────── */
    {
        np_ota_state_t s = make_valid_state(1);
        check(np_ota_state_validate(&s) == NP_OTA_STATE_OK,
              "valid state Bank B passes validation");
    }
    {
        np_ota_state_t s = make_valid_state(0);
        check(np_ota_state_validate(&s) == NP_OTA_STATE_OK,
              "valid state Bank A passes validation");
    }

    /* ── 2: Wrong magic fails ─────────────────────────────────────────── */
    {
        np_ota_state_t s = make_valid_state(1);
        s.magic = 0xDEADBEEFUL;
        check(np_ota_state_validate(&s) == NP_OTA_STATE_BAD_MAGIC,
              "corrupted magic detected");
    }

    /* ── 3: Corrupt CRC fails ─────────────────────────────────────────── */
    {
        np_ota_state_t s = make_valid_state(1);
        s.crc32 ^= 0x00000001UL;  /* flip one bit */
        check(np_ota_state_validate(&s) == NP_OTA_STATE_BAD_CRC,
              "single-bit CRC corruption detected");
    }
    {
        np_ota_state_t s = make_valid_state(1);
        s.staged_sha256[0] ^= 0xFFU;  /* corrupt hash but don't update CRC */
        check(np_ota_state_validate(&s) == NP_OTA_STATE_BAD_CRC,
              "modified SHA-256 without CRC update detected");
    }

    /* ── 4: Invalid bank value fails ──────────────────────────────────── */
    {
        np_ota_state_t s = make_valid_state(1);
        s.target_bank = 2U;
        s.crc32 = np_ota_state_crc32(&s);  /* re-compute CRC with invalid bank */
        check(np_ota_state_validate(&s) == NP_OTA_STATE_INVALID_BANK,
              "bank value 2 rejected");
    }

    /* ── 5: Boot attempt counting ─────────────────────────────────────── */
    {
        np_ota_state_t s = make_valid_state(1);
        check(s.boot_attempts == 0, "init: boot_attempts == 0");

        bool ok1 = np_ota_state_increment_attempts(&s);
        check(s.boot_attempts == 1, "after increment 1: boot_attempts == 1");
        check(ok1 == true,          "increment 1: still below max, returns true");
        check(np_ota_state_validate(&s) == NP_OTA_STATE_OK,
              "attempt 1 state still valid");

        bool ok2 = np_ota_state_increment_attempts(&s);
        check(s.boot_attempts == 2, "after increment 2: boot_attempts == 2");
        check(ok2 == true,          "increment 2: still below max, returns true");
        check(np_ota_state_validate(&s) == NP_OTA_STATE_OK,
              "attempt 2 state still valid");

        bool ok3 = np_ota_state_increment_attempts(&s);
        check(s.boot_attempts == 3,  "after increment 3: boot_attempts == 3");
        check(ok3 == false,          "increment 3: at max, returns false (rollback needed)");
        check(np_ota_state_validate(&s) == NP_OTA_STATE_MAX_ATTEMPTS,
              "attempt 3 triggers MAX_ATTEMPTS");
    }

    /* ── 6: CRC is recomputed after increment_attempts ───────────────── */
    {
        np_ota_state_t s = make_valid_state(1);
        uint32_t crc_before = s.crc32;
        np_ota_state_increment_attempts(&s);
        /* CRC must change since boot_attempts changed */
        check(s.crc32 != crc_before, "CRC updated after increment_attempts");
        /* Validate still passes (CRC was recomputed correctly) */
        check(np_ota_state_validate(&s) == NP_OTA_STATE_OK,
              "state valid after increment_attempts recomputes CRC");
    }

    /* ── 7: Zero-filled record fails (not a valid state) ─────────────── */
    {
        np_ota_state_t s;
        memset(&s, 0, sizeof(s));
        check(np_ota_state_validate(&s) == NP_OTA_STATE_BAD_MAGIC,
              "zero-filled record rejected (bad magic)");
    }

    /* ── 8: 0xFF-filled record fails ─────────────────────────────────── */
    {
        np_ota_state_t s;
        memset(&s, 0xFF, sizeof(s));
        check(np_ota_state_validate(&s) == NP_OTA_STATE_BAD_MAGIC,
              "0xFF-filled record rejected (bad magic)");
    }

    /* ── 9: CRC is order-dependent (different SHA → different CRC) ───── */
    {
        np_ota_state_t s1 = make_valid_state(1);
        uint8_t sha2[NP_OTA_SHA256_SIZE];
        for (int i = 0; i < (int)NP_OTA_SHA256_SIZE; i++) {
            sha2[i] = (uint8_t)(0xB0 + i);  /* different from make_valid_state */
        }
        np_ota_state_t s2;
        np_ota_state_init(&s2, 1, sha2);
        check(s1.crc32 != s2.crc32,
              "different SHA-256 inputs produce different CRC values");
    }

    printf("\n%s (%d failure(s))\n",
           g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
           g_failures);
    return g_failures == 0 ? 0 : 1;
}
