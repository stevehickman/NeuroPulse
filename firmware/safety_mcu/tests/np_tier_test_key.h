/*
 * TEST-ONLY tier-authority public key for np_tier_identity_tests.
 *
 * Force-included (-include) into BOTH the unit under test and the test TU of
 * the keyed build, so np_tier_identity.c verifies against the same key the test
 * signs with.  Never reachable from the cross build: only the host-test target
 * in CMakeLists.txt names this file.
 *
 * Derived by crypto_ed25519_key_pair() from the seed 0xA0, 0xA1, … 0xBF, which
 * the test re-derives at start-up and checks against this constant — so a
 * key/seed mismatch fails a named test instead of failing every signature case
 * for an unexplained reason.
 */
#ifndef NP_TIER_TEST_KEY_H
#define NP_TIER_TEST_KEY_H

#define NP_TIER_AUTHORITY_PUBKEY_INIT { \
    0x4FU, 0xD0U, 0x99U, 0xCCU, 0xD4U, 0x7DU, 0x78U, 0x93U, \
    0xDFU, 0xE9U, 0xECU, 0x24U, 0x41U, 0x4EU, 0xCBU, 0x0DU, \
    0x9BU, 0x54U, 0x20U, 0x23U, 0x2AU, 0xADU, 0x30U, 0xD9U, \
    0x1CU, 0x46U, 0x5BU, 0xE3U, 0x3CU, 0xBEU, 0x65U, 0xC4U }

#define NP_TIER_TEST_SEED_FIRST  0xA0U

#endif /* NP_TIER_TEST_KEY_H */
