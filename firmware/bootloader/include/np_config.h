/*
 * NeurOne Bootloader — Hardware Configuration
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
 * Document: NP-FW-EMMC-001 Rev 1 §8.1
 *
 * All eMMC partition offsets are LBA-addressed (512-byte sectors).
 * Partition layout matches the 9-partition GPT defined in the spec.
 */

#ifndef NP_CONFIG_H
#define NP_CONFIG_H

#include <stdint.h>

/* ── i.MX RT1062 SRAM layout ──────────────────────────────────────────────── */
/* Bootloader executes from OCRAM loaded by ROM.                               */
/*                                                                             */
/* This is OCRAM2 — the DEDICATED 512 KiB block at 0x20200000.  It is not part */
/* of FlexRAM, so its base and size do not depend on the ITCM/DTCM/OCRAM       */
/* partition below, and reconfiguring that partition cannot move it or take    */
/* bytes out of it.  That is what makes it safe for the bootloader, which runs */
/* entirely from here, to establish the partition at all (NP-SW-CI-001 §4.10).  */
#define NP_OCRAM_BASE           0x20200000UL    /* OCRAM2 base (dedicated)     */
#define NP_OCRAM_SIZE           (512U * 1024U)  /* 512 KiB, partition-independent */
#define NP_SCRATCH_SRAM_BASE    0x20270000UL    /* ⚠ UNUSED — see OI-SWCI-44   */
#define NP_SCRATCH_SRAM_SIZE    (64U * 1024U)   /* ⚠ overlaps .app_staging     */

/* ── i.MX RT1062 FlexRAM partition (NP-SW-CI-001 §4.10, closes OI-SWCI-39) ─── */
/*                                                                             */
/* FlexRAM is a SEPARATE 512 KiB array from OCRAM2 above: 16 banks of 32 KiB   */
/* (FSL_FEATURE_FLEXRAM_INTERNAL_RAM_TOTAL_BANK_NUMBERS = 16,                  */
/* FSL_FEATURE_FLEXRAM_INTERNAL_RAM_BANK_SIZE = 32768, both in the vendored    */
/* firmware/vendor/mcux_sdk/devices/MIMXRT1062/MIMXRT1062_features.h).  Each   */
/* bank is assignable to ITCM, DTCM or the FlexRAM OCRAM aperture, by eFuse at */
/* reset or by IOMUXC_GPR17 afterwards.                                        */
/*                                                                             */
/* THE PARTITION IS DERIVED, NOT PREFERRED.  Only three consumers can exist,   */
/* and two of them have nothing to consume:                                    */
/*                                                                             */
/*   ITCM  (0x00000000)  no section in either linker script.  The bootloader   */
/*                       executes from OCRAM2 where the ROM put it, and the    */
/*                       application executes in place from the OCRAM2 staging */
/*                       area the bootloader copies it into — neither image    */
/*                       has any .text in ITCM, and putting one there would be */
/*                       a change to load_and_jump(), not to this constant.    */
/*   DTCM  (0x20000000)  the application's .data and its MSP stack.  The only  */
/*                       claimant.                                             */
/*   OCRAM (0x20280000)  the FlexRAM OCRAM aperture, which begins exactly      */
/*                       where OCRAM2 ends.  No section in either linker       */
/*                       script addresses it, and nothing needs it to: the     */
/*                       440 KiB OCRAM2 staging reservation is ~10% used by    */
/*                       the loadable image, so a future DMA or buffer pool    */
/*                       has room without taking a bank from DTCM.             */
/*                                                                             */
/* So every bank goes to the one region anything in this repository links      */
/* into.  The alternative — leaving banks unassigned — would shrink the        */
/* device's usable RAM to fit today's bring-up image, which is the reasoning   */
/* §4.8.5 rejected when it declined to shrink configTOTAL_HEAP_SIZE.           */
/*                                                                             */
/* Two consequences worth stating, because they are what closed OI-SWCI-39:    */
/*   - With zero FlexRAM OCRAM banks, OCRAM2 is the ONLY OCRAM, so             */
/*     NP_OCRAM_SIZE above is exact rather than merely conservative, and the   */
/*     MCUX linker script's 768 KiB (0x000C0000 at the same base) is revealed  */
/*     as the DEFAULT-FUSE composite — 512 KiB OCRAM2 + 256 KiB of FlexRAM —   */
/*     not a contradiction of it.  The two figures never described the same    */
/*     memory.                                                                 */
/*   - 0x20280000 and above is unmapped under this partition.  Nothing         */
/*     addresses it; the bootloader's initial SP is 0x20280000 exactly, and a  */
/*     full-descending stack decrements before its first store, so no access   */
/*     is ever made AT that address.                                           */
#define NP_FLEXRAM_BANK_SIZE    (32U * 1024U)
#define NP_FLEXRAM_BANK_COUNT   16U
#define NP_FLEXRAM_ITCM_BANKS   0U
#define NP_FLEXRAM_DTCM_BANKS   16U
#define NP_FLEXRAM_OCRAM_BANKS  0U

#define NP_ITCM_BASE            0x00000000UL
#define NP_ITCM_SIZE            (NP_FLEXRAM_ITCM_BANKS * NP_FLEXRAM_BANK_SIZE)
#define NP_DTCM_BASE            0x20000000UL
#define NP_DTCM_SIZE            (NP_FLEXRAM_DTCM_BANKS * NP_FLEXRAM_BANK_SIZE)

/* Base of the FlexRAM OCRAM aperture — immediately above OCRAM2.  Declared so */
/* the adjacency (NP_OCRAM_BASE + NP_OCRAM_SIZE == this) is a checked fact and */
/* not a coincidence two constants happen to have; see                         */
/* np_bootloader_flexram_tests.                                                */
#define NP_FLEXRAM_OCRAM_BASE   0x20280000UL

/* ── IOMUXC_GPR — the registers that establish the partition ─────────────── */
/*                                                                             */
/* Addresses rather than the SDK's IOMUXC_GPR_Type, because the bootloader is  */
/* a standalone CMake project that includes no vendored headers at all — the   */
/* same reason WDOG1_WCR and SNVS are raw addresses here and in np_main.c.     */
/* Offsets verified against the register layout typedef in the vendored        */
/* MIMXRT1062.h (GPR14 @ 0x38, GPR16 @ 0x40, GPR17 @ 0x44).                    */
/*                                                                             */
/* ⚠ The three FIELD definitions below are NOT in the vendored MCUX 2.16.0     */
/* header, and that is a fact about the header rather than about the silicon.  */
/* MIMXRT1062.h defines exactly two GPR16 fields (FLEXRAM_BANK_CFG_SEL at bit  */
/* 2, CM7_INIT_VTOR at 31:7) and no CM7_CFG*TCMSZ in GPR14 at all — bits 0, 1  */
/* of GPR16 and 23:16 of GPR14 are simply absent from it.  The values here     */
/* come from IMXRT1060RM and are corroborated by shipping RT1062 firmware      */
/* (Teensy 4.x startup writes GPR16 = 0x00200007 — bits 0,1,2 — and            */
/* GPR14 = 0x00AA0000, i.e. code 0xA in both 19:16 and 23:20 for 512 KiB       */
/* windows).  If a future SDK bump starts defining them, the duplicate must be */
/* deleted here rather than left to diverge silently: OI-SWCI-40's shape.      */
#define NP_IOMUXC_GPR_BASE      0x400AC000UL
#define NP_IOMUXC_GPR14         (*(volatile uint32_t *)(NP_IOMUXC_GPR_BASE + 0x38U))
#define NP_IOMUXC_GPR16         (*(volatile uint32_t *)(NP_IOMUXC_GPR_BASE + 0x40U))
#define NP_IOMUXC_GPR17         (*(volatile uint32_t *)(NP_IOMUXC_GPR_BASE + 0x44U))

#define NP_GPR16_INIT_ITCM_EN         (1UL << 0U)
#define NP_GPR16_INIT_DTCM_EN         (1UL << 1U)
#define NP_GPR16_FLEXRAM_BANK_CFG_SEL (1UL << 2U)

#define NP_GPR14_CFGITCMSZ_SHIFT      16U
#define NP_GPR14_CFGITCMSZ_MASK       (0xFUL << NP_GPR14_CFGITCMSZ_SHIFT)
#define NP_GPR14_CFGDTCMSZ_SHIFT      20U
#define NP_GPR14_CFGDTCMSZ_MASK       (0xFUL << NP_GPR14_CFGDTCMSZ_SHIFT)

/* ── SNVS Low Power General Purpose Register 0 ───────────────────────────── */
/* Survives warm resets; holds boot bank flag and attempt counter.             */
#define NP_SNVS_BASE            0x400D4000UL
#define NP_SNVS_LPGPR0          (*(volatile uint32_t *)(NP_SNVS_BASE + 0x68U))

/* SNVS_LPGPR0 bit field layout:                                               */
/*   [1:0]  BOOT_BANK      0=Bank A, 1=Bank B                                 */
/*   [7:2]  BOOT_ATTEMPTS  incremented each boot of OTA candidate (max 3)     */
/*   [8]    OTA_PENDING    set by app before reset to trigger OTA swap verify  */
/*   [15:9] reserved                                                           */
/*   [16]   DFU_FORCED     set to force DFU recovery regardless of bank state  */
/*   [31:17] reserved                                                          */
#define NP_SNVS_BANK_MASK       0x00000003UL
#define NP_SNVS_BANK_A          0x00000000UL
#define NP_SNVS_BANK_B          0x00000001UL
#define NP_SNVS_ATTEMPTS_SHIFT  2U
#define NP_SNVS_ATTEMPTS_MASK   (0x3FU << NP_SNVS_ATTEMPTS_SHIFT)
#define NP_SNVS_OTA_PENDING     (1UL << 8U)
#define NP_SNVS_DFU_FORCED      (1UL << 16U)
#define NP_BOOT_MAX_ATTEMPTS    3U

/* ── SNVS_LPGPR1 — factory reset in-progress flag ────────────────────────── */
/* Set at factory-reset step R-3, cleared at R-11 (NP-FW-EMMC-002 Rev 1 §B).  */
/* If set on boot, a reset was interrupted by power loss; re-run SANITIZE.    */
#define NP_SNVS_LPGPR1          (*(volatile uint32_t *)(NP_SNVS_BASE + 0x6CU))
#define NP_SNVS_RESET_IN_PROGRESS  (1UL << 0U)

/* ── SNVS_LPGPR2 — anonymization in-progress flag ────────────────────────── */
/* Set while the research anonymization pipeline holds an encrypted extract in */
/* the Scratch partition (NP-FW-EMMC-002 Rev 1 §D).                           */
#define NP_SNVS_LPGPR2          (*(volatile uint32_t *)(NP_SNVS_BASE + 0x70U))
#define NP_SNVS_ANON_IN_PROGRESS   (1UL << 0U)

/* ── eMMC user-area partition map (512-byte LBA sectors) ─────────────────── */
/* All offsets are from LBA 0 of the eMMC user area.                          */
/* GPT header at LBA 1, entries LBA 2–33, first usable LBA 34.               */
/* Partitions are 1 MiB aligned (2048 sector granularity).                    */

#define NP_LBA_PER_MIB          2048UL          /* 512B sectors per MiB       */

/* Partition 1 — Firmware Bank A (128 MiB) */
#define NP_BANK_A_LBA_START     (1UL * NP_LBA_PER_MIB)     /* LBA  2048       */
#define NP_BANK_SIZE_LBA        (128UL * NP_LBA_PER_MIB)   /* 262144 sectors  */
#define NP_BANK_SIZE_BYTES      (128UL * 1024UL * 1024UL)

/* Partition 2 — Firmware Bank B (128 MiB) */
#define NP_BANK_B_LBA_START     (NP_BANK_A_LBA_START + NP_BANK_SIZE_LBA)

/* Partition 3 — Safety MCU FW staging (4 MiB) */
#define NP_SAFETY_MCU_LBA_START (NP_BANK_B_LBA_START + NP_BANK_SIZE_LBA)
#define NP_SAFETY_MCU_SIZE_LBA  (4UL * NP_LBA_PER_MIB)

/* Partition 4 — Config/Calibration (16 MiB, LittleFS) */
#define NP_CONFIG_LBA_START     (NP_SAFETY_MCU_LBA_START + NP_SAFETY_MCU_SIZE_LBA)
#define NP_CONFIG_SIZE_LBA      (16UL * NP_LBA_PER_MIB)

/* Partition 5 — SHDR (512 MiB) */
#define NP_SHDR_LBA_START       (NP_CONFIG_LBA_START + NP_CONFIG_SIZE_LBA)
#define NP_SHDR_SIZE_LBA        (512UL * NP_LBA_PER_MIB)

/* Partition 6 — UHDR (6903 MiB, AES-256-XTS) */
#define NP_UHDR_LBA_START       (NP_SHDR_LBA_START + NP_SHDR_SIZE_LBA)
#define NP_UHDR_SIZE_LBA        (6903UL * NP_LBA_PER_MIB)

/* Partition 7 — Scratch (500 MiB, zeroed on boot, OTA staging workspace) */
#define NP_SCRATCH_LBA_START    (NP_UHDR_LBA_START + NP_UHDR_SIZE_LBA)
#define NP_SCRATCH_SIZE_LBA     (500UL * NP_LBA_PER_MIB)

/* ── Firmware image header layout ────────────────────────────────────────── */
/* Each bank starts with NP_IMAGE_HEADER at offset 0.                         */
/* Firmware vector table + code follows at offset NP_IMAGE_HEADER_SIZE.       */
#define NP_IMAGE_MAGIC          0x4E504657UL    /* "NPFW"                      */
#define NP_IMAGE_HEADER_SIZE    256U            /* Aligned to 256 bytes        */
#define NP_FW_LOAD_ADDR         0x20200000UL    /* OCRAM load target           */
#define NP_FW_MAX_SIZE          (NP_BANK_SIZE_BYTES - NP_IMAGE_HEADER_SIZE)

/* ── Ed25519 public key (manufacturing root — provisioned at test) ────────── */
/* 32-byte curve25519 public key embedded in bootloader at build time.        */
/* Replace with production key before tooling sign-off.                       */
#define NP_ED25519_PUBKEY_SIZE  32U
#define NP_ED25519_SIG_SIZE     64U
#define NP_SHA256_SIZE          32U
#define NP_SHA512_SIZE          64U

/* ── USDHC peripheral (eMMC on USDHC2) ──────────────────────────────────── */
#define NP_USDHC2_BASE          0x402C4000UL
#define NP_EMMC_SECTOR_SIZE     512U
#define NP_EMMC_BOOT_PART_1     1U              /* EXTCSD[179] value for BP1  */
#define NP_EMMC_BOOT_PART_2     2U              /* EXTCSD[179] value for BP2  */
#define NP_EMMC_USER_AREA       0U              /* EXTCSD[179] value for UDA  */

/* ── USB OTG1 (USB-C DFU recovery) ──────────────────────────────────────── */
#define NP_USB1_BASE            0x402E0000UL
#define NP_DFU_VENDOR_ID        0x3A9BU         /* NeurOne USB VID          */
#define NP_DFU_PRODUCT_ID       0x0001U         /* Bootloader DFU PID          */
#define NP_DFU_TRANSFER_SIZE    4096U           /* wTransferSize bytes         */
#define NP_DFU_TIMEOUT_MS       300000U         /* 5-minute DFU session window */

/* ── Watchdog / boot confirmation ────────────────────────────────────────── */
/* Application must call np_ota_confirm_boot() within this window after boot. */
/* If the app does not confirm, the next reset treats this as a boot failure. */
/* This constant is stored in Config partition, not SNVS (survives SNVS wipe).*/
#define NP_OTA_CONFIRM_TIMEOUT_S 30U

/* ── CRC32 polynomial ─────────────────────────────────────────────────────── */
#define NP_CRC32_POLY           0xEDB88320UL    /* IEEE 802.3 reflected        */

#endif /* NP_CONFIG_H */
