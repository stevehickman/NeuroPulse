/*
 * TEST-ONLY manufacturing root public key for np_bootloader_app_image_tests.
 *
 * Force-included (-include) into np_signature.c and the test TU of the host
 * build only, so the bootloader's own Ed25519 verifies against a key the test
 * holds the private half of.  Never reachable from the cross build: only the
 * host-test target in CMakeLists.txt names this file, and the cross build
 * links the placeholder in np_signature.c.
 *
 * Derived by Monocypher's crypto_ed25519_key_pair() from the seed 0xC0, 0xC1,
 * … 0xDF, which the test re-derives at start-up and checks against this
 * constant — so a key/seed mismatch fails one named assertion instead of every
 * signature case for an unexplained reason (the np_tier_test_key.h pattern).
 */
#ifndef NP_BOOTLOADER_TEST_KEY_H
#define NP_BOOTLOADER_TEST_KEY_H

#define NP_FW_PUBLIC_KEY_INIT { \
    0xDDU, 0xE3U, 0xBCU, 0xCEU, 0xC7U, 0xF3U, 0xA6U, 0x6AU, \
    0x11U, 0x15U, 0xF4U, 0x5DU, 0x72U, 0x0FU, 0x4DU, 0xC1U, \
    0x35U, 0xC3U, 0xAEU, 0x7CU, 0x4EU, 0x22U, 0xDCU, 0xA3U, \
    0x8FU, 0xDBU, 0x1EU, 0xFDU, 0x6AU, 0x49U, 0x5FU, 0xF8U }

#define NP_FW_TEST_SEED_FIRST  0xC0U

#endif /* NP_BOOTLOADER_TEST_KEY_H */
