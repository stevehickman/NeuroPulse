/*
 * NeurOne Bootloader — eMMC Interface (USDHC2)
 * Document: NP-FW-EMMC-001 Rev 1 §3, §8
 */

#ifndef NP_EMMC_H
#define NP_EMMC_H

#include "np_types.h"

/* ── Public API ───────────────────────────────────────────────────────────── */

/* Initialize USDHC2 peripheral and enumerate the eMMC device.                */
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

/* Erase `count` sectors starting at `lba` (CMD35/36/38, argument 0).        */
/* A plain ERASE: it unmaps the range and is NOT a purge (OI-PRIV-27).       */
np_status_t np_emmc_erase(uint32_t lba, uint32_t count);

/* eMMC SANITIZE: CMD6 SWITCH writing 1 to EXT_CSD[165] SANITIZE_START, then */
/* CMD13 polling to TRAN.  Purges every unmapped block of the user area, so  */
/* it runs AFTER the erases it is meant to purge.  Fails if the part does    */
/* not advertise SANITIZE (EXT_CSD[231] bit 6), reports an error, or does    */
/* not finish within its bound.  Sequence: np_emmc_sanitize.h.               */
np_status_t np_emmc_sanitize(void);

/* Return LBA start of the requested bank in the user area.                   */
static inline uint32_t np_emmc_bank_lba(np_bank_t bank) {
    return (bank == NP_BANK_A) ? NP_BANK_A_LBA_START : NP_BANK_B_LBA_START;
}

#endif /* NP_EMMC_H */
