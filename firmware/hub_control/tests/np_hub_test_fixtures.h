/*
 * np_hub_test_fixtures.h — constants shared by the hub host tests
 *
 * HOST TEST CODE ONLY.  One definition per fixture quantity: a test that needs
 * one includes this file and never redefines it.  The values here are the
 * tests' own, written out independently of the production headers on purpose
 * (NP_HEXMAP_* arithmetic is asserted AGAINST these, not derived from them).
 */

#ifndef NP_HUB_TEST_FIXTURES_H
#define NP_HUB_TEST_FIXTURES_H

#include <stddef.h>

/* The shipped lattice: 80 sockets (NP-HEX-ZM-001). */
#define N_SOCKETS   80u

/* The "NPMP" module-map blob — NP-SOUP-LFS-001 §5.3's arithmetic exactly:
 * blob(n) = HDR(8) + n * 175 + CRC(4), at the n = N_SOCKETS the Config
 * partition carries (14,012 bytes). */
#define BLOB_BYTES  (8U + (N_SOCKETS * 175U) + 4U)

/* The Config-partition medium the LFS tests mount.  Expands NP_LFS_CFG_*, so a
 * user includes np_lfs_instance.h first. */
#define MEDIA_BYTES ((size_t)NP_LFS_CFG_BLOCK_SIZE * NP_LFS_CFG_BLOCK_COUNT)

/* 32-byte self-checking journal record — NP-FW-NVRAM-001 §4.2 D-5. */
#define REC_SIZE    32U

/* The session-log file name the log scenarios append to. */
#define LOG_PATH    "session.log"

#endif /* NP_HUB_TEST_FIXTURES_H */
