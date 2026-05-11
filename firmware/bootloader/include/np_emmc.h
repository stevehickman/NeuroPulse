/*
 * NeuroPulse Bootloader — eMMC Interface (USDHC2)
 * Document: NP-FW-EMMC-001 Rev A §3, §8
 */

#ifndef NP_EMMC_H
#define NP_EMMC_H

#include "np_types.h"

/* ── Public API ───────────────────────────────────────────────────────────── */

/* Initialise USDHC2 peripheral and enumerate the eMMC device.                */
/* Must be called once at boot before any read/write operations.              */
np_status_t np_emmc_init(void);

/* Switch the eMMC access window.                                              */
/*   part = NP_EMMC_BOOT_PART_1 | NP_EMMC_BOOT_PART_2 | NP_EMMC_USER_AREA   */
np_status_t np_emmc_switch_partition(uint8_t part);

/* Read `count` 512-byte sectors from `lba` into `buf`.                       */
np_status_t np_emmc_read(uint32_t lba, void *buf, uint32_t count);

/* Write `count` 512-byte sectors from `buf` to `lba`.                       */
np_status_t np_emmc_write(uint32_t lba, const void *buf, uint32_t count);

/* Read-back verify: write then immediately read and compare.                 */
/* Returns NP_OK only if every byte matches.                                  */
np_status_t np_emmc_write_verify(uint32_t lba, const void *buf, uint32_t count);

/* Erase `count` sectors starting at `lba` (sets to 0xFF).                   */
np_status_t np_emmc_erase(uint32_t lba, uint32_t count);

/* Return LBA start of the requested bank in the user area.                   */
static inline uint32_t np_emmc_bank_lba(np_bank_t bank) {
    return (bank == NP_BANK_A) ? NP_BANK_A_LBA_START : NP_BANK_B_LBA_START;
}

#endif /* NP_EMMC_H */
