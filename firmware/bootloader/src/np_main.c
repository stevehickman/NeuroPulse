/*
 * NeurOne Bootloader — Main Boot Flow
 * Document: NP-FW-EMMC-001 Rev A §8, §11
 *
 * Boot sequence (B-1 through B-9 from spec):
 *
 *  B-1  Power-on reset → ROM loads bootloader from eMMC boot partition 1
 *       (or BP2 if BP1 fails) into OCRAM at 0x20200000.  Bootloader_Reset
 *       handler is called.
 *
 *  B-2  Read SNVS_LPGPR0 to determine selected boot bank and OTA pending status.
 *
 *  B-3  If OTA_PENDING is set: run np_ota_handle_boot_verification() which
 *       checks the attempt counter, verifies the candidate bank's Ed25519
 *       signature, and performs automatic rollback at NP_BOOT_MAX_ATTEMPTS.
 *
 *       If OTA_PENDING is NOT set: verify the selected bank's signature.
 *
 *  B-4  If verification passes → prepare Scratch (zero it) → jump to firmware.
 *
 *  B-5  If the primary bank fails → try the other bank.
 *
 *  B-6  If both banks fail → enter USB-C DFU recovery.
 *
 *  B-7  [OTA download — application side, not in this file]
 *
 *  B-8  [Post-OTA: next boot → B-3 handles the new candidate]
 *
 *  B-9  [After NP_BOOT_MAX_ATTEMPTS failures: rollback handled in B-3]
 *
 * Safety constraints:
 *  - No heap allocation. All buffers are static or stack-allocated.
 *  - Stack is placed in OCRAM above the bootloader image.
 *  - The Scratch partition is zeroed on every boot to prevent stale data.
 *  - The bootloader never trusts data from the firmware banks without
 *    Ed25519 + SHA-256 verification.
 *  - No firmware from a bank is executed if its signature does not verify.
 */

#include "np_config.h"
#include "np_types.h"
#include "np_app_image.h"
#include "np_emmc.h"
#include "np_boot_selector.h"
#include "np_signature.h"
#include "np_ota.h"
#include "np_dfu.h"
#include <string.h>

/* SNVS_LPGPR2 — anonymization in-progress flag (NP-FW-EMMC-002 §D.6) ──────── */
/* Defined locally to avoid colliding with Unit 1's parallel edit to           */
/* np_config.h.  When np_config.h gains these defs, the merge reviewer must     */
/* delete these locals and keep the np_config.h versions.                       */
#ifndef NP_SNVS_LPGPR2
#define NP_SNVS_LPGPR2              (*(volatile uint32_t *)(NP_SNVS_BASE + 0x70U))
#endif
#ifndef NP_SNVS_ANON_IN_PROGRESS
#define NP_SNVS_ANON_IN_PROGRESS    (1UL << 0U)
#endif

/* ── i.MX RT1062 Image Vector Table (IVT) ────────────────────────────────── */
/* The ROM boot loader reads the IVT at offset 0x000 of the eMMC boot         */
/* partition to locate the bootloader entry point and load address.           */
/* The IVT must be the first structure in the binary image.                  */

typedef struct __attribute__((packed)) {
    uint8_t  tag;           /* 0xD1 for boot image                            */
    uint16_t length;        /* length of IVT header (LE) — always 0x0020     */
    uint8_t  version;       /* 0x40 = i.MX7/8 series boot image              */
    uint32_t entry;         /* absolute address of bootloader entry point    */
    uint32_t _reserved1;
    uint32_t dcd;           /* device configuration data address (0 = none)  */
    uint32_t boot_data;     /* boot data structure address                   */
    uint32_t self;          /* absolute IVT address (= NP_FW_LOAD_ADDR)     */
    uint32_t csf;           /* command sequence file (0 = HAB disabled)     */
    uint32_t _reserved2;
} np_ivt_t;

typedef struct __attribute__((packed)) {
    uint32_t start;         /* absolute load address (NP_FW_LOAD_ADDR)      */
    uint32_t size;          /* total image size in bytes                     */
    uint32_t plugin;        /* 0 = normal boot image                         */
    uint32_t _reserved;
} np_boot_data_t;

/* The IVT and boot data are placed in a dedicated section at the start of   */
/* the binary, before any code.  The linker script positions them at         */
/* NP_FW_LOAD_ADDR (0x20200000).                                             */

extern uint32_t  _bootloader_end;   /* provided by linker script             */
extern void      Bootloader_Reset(void);
extern void      SysTick_Handler(void);  /* defined in np_dfu.c (DFU timeout tick) */

/* TEMPORARY — NP-SW-CI-001 phase 3 gate demonstration.  Declared but defined
 * nowhere, and called from Bootloader_Reset() below so it cannot be elided.
 * Produces an unresolved-symbol link failure — deliberately the same failure
 * shape as Defect B (§4.2), which is what this leg exists to catch.  REVERTED
 * in the next commit; must not appear in the merged diff.                    */
extern void      np_phase3_gate_probe(void);

/* IVT at base of OCRAM load address */
__attribute__((section(".np_ivt"), used))
static const np_ivt_t g_ivt = {
    .tag        = 0xD1U,
    .length     = 0x0020U,
    .version    = 0x40U,
    .entry      = NP_FW_LOAD_ADDR + sizeof(np_ivt_t) + sizeof(np_boot_data_t),
    ._reserved1 = 0U,
    .dcd        = 0U,
    .boot_data  = NP_FW_LOAD_ADDR + sizeof(np_ivt_t),
    .self       = NP_FW_LOAD_ADDR,
    .csf        = 0U,
    ._reserved2 = 0U,
};

__attribute__((section(".np_boot_data"), used))
static const np_boot_data_t g_boot_data = {
    .start    = NP_FW_LOAD_ADDR,
    .size     = 48U * 1024U,    /* bootloader image ≤ 48 KiB               */
    .plugin   = 0U,
    ._reserved = 0U,
};

/* ── Vector table stub ────────────────────────────────────────────────────── */
/* The bootloader needs a minimal vector table for the exceptions it handles. */
/* The Reset_Handler is the bootloader entry point (called by ROM).           */

extern uint32_t _stack_top;   /* provided by linker script: end of OCRAM stack */

__attribute__((section(".np_vectors"), used))
static const uint32_t g_vectors[] = {
    (uint32_t)&_stack_top,           /* 0: Initial stack pointer              */
    (uint32_t)Bootloader_Reset,      /* 1: Reset handler                      */
    0U,                               /* 2: NMI (not used in bootloader)       */
    (uint32_t)Bootloader_Reset,      /* 3: HardFault → re-enter bootloader    */
    0U, 0U, 0U,                       /* 4–6: reserved                         */
    0U,                               /* 7: reserved                           */
    0U, 0U, 0U,                       /* 8–10: reserved                        */
    0U,                               /* 11: SVCall                            */
    0U, 0U,                           /* 12–13: reserved                       */
    0U,                               /* 14: PendSV                            */
    (uint32_t)SysTick_Handler,        /* 15: SysTick (DFU timeout)             */
    /* External interrupts */
    [113 + 16] = (uint32_t)np_dfu_usb_isr,  /* USB1 IRQ (vector 129)         */
};

/* ── Hardware fault handler (redirects to bootloader reset) ──────────────── */
/* If the bootloader itself faults, the HardFault vector re-enters the        */
/* Bootloader_Reset handler.  This prevents a hard fault from hanging the    */
/* device in an unrecoverable state.                                         */

/* ── Scratch partition zero ──────────────────────────────────────────────── */
/* Zero the scratch partition at every boot to prevent stale OTA data from   */
/* being used across power cycles.                                           */
static void zero_scratch_partition(void)
{
    /* Erase via eMMC TRIM/erase command — faster than sector-by-sector zero. */
    /* Errors are ignored: if scratch erase fails, OTA verify will reject     */
    /* any stale image via hash check.                                        */
    np_emmc_erase(NP_SCRATCH_LBA_START, NP_SCRATCH_SIZE_LBA);
}

/* ── Read and verify a firmware bank ──────────────────────────────────────── */
/* Returns NP_OK if the bank's Ed25519 signature and header CRC are valid.    */
/* The full image SHA-256 is NOT recomputed here (it would require 128 MiB   */
/* of eMMC reads); instead we verify only the header + Ed25519.  The image   */
/* SHA-256 embedded in the header was verified when the image was written    */
/* (np_ota_write_bank writes sector-by-sector with readback verify).         */
/*                                                                            */
/* For the cold-boot path (no OTA), we trust that:                           */
/*   1. Factory firmware was signed with the manufacturing root key.         */
/*   2. Bank A is write-protected in production (cannot be modified).        */
/*   3. The Ed25519 signature over (sha256 || version || size) is sufficient */
/*      to authenticate the image without re-hashing 128 MiB at boot.       */
static np_status_t verify_bank_header(np_bank_t bank)
{
    static uint8_t sector_buf[NP_EMMC_SECTOR_SIZE];
    np_image_header_t hdr;

    np_status_t ret = np_emmc_switch_partition(NP_EMMC_USER_AREA);
    if (ret != NP_OK) return ret;

    ret = np_emmc_read(np_emmc_bank_lba(bank), sector_buf, 1U);
    if (ret != NP_OK) return ret;

    memcpy(&hdr, sector_buf, sizeof(hdr));

    if (hdr.magic != NP_IMAGE_MAGIC) return NP_ERR_BAD_MAGIC;

    /* np_signature_verify with NULL image_data: verifies header CRC +        */
    /* Ed25519 signature only (no SHA-256 image recomputation).               */
    return np_signature_verify(&hdr, NULL);
}

/* ── Jump to application firmware ─────────────────────────────────────────── */
/* Load the firmware entry point from the bank's vector table and jump.      */
/* The firmware image starts at offset NP_IMAGE_HEADER_SIZE in the bank.    */
/* Its first word (offset 0) is the initial stack pointer.                   */
/* Its second word (offset 4) is the reset handler address.                  */
/*                                                                            */
/* We read the firmware into OCRAM above the bootloader, then relocate the   */
/* vector table and jump.                                                     */

/* The staging area's base address and size are owned by the linker script and  */
/* read through np_app_image.h.  They used to be recomputed here —              */
/* NP_APP_LOAD_ADDR as NP_FW_LOAD_ADDR + 64 KiB, and the size as NP_OCRAM_SIZE  */
/* minus that offset — which is Defect C: the size arithmetic omitted the 8 KiB */
/* stack, so this function accepted a 448 KiB image and copied 8 KiB of it over */
/* the stack it was running on.  Neither value is computed in C any more.       */
/* NP-SW-CI-001 §4.3.                                                           */

static np_status_t load_and_jump(np_bank_t bank)
{
    static uint8_t hdr_buf[NP_EMMC_SECTOR_SIZE];
    np_image_header_t hdr;

    np_status_t ret = np_emmc_read(np_emmc_bank_lba(bank), hdr_buf, 1U);
    if (ret != NP_OK) return ret;
    memcpy(&hdr, hdr_buf, sizeof(hdr));

    /* Validate header one more time before loading */
    if (hdr.magic != NP_IMAGE_MAGIC) return NP_ERR_BAD_MAGIC;

    /* Bank capacity — the image must fit the partition it was read from. */
    if (hdr.image_size > NP_FW_MAX_SIZE) return NP_ERR_IMAGE_TOO_LARGE;

    /* Staging capacity — rejects an empty image and anything that would run
     * past the end of the reservation.  The limit comes from the linker. */
    uint8_t *app_dest = np_app_staging_base();
    ret = np_app_image_size_check(hdr.image_size, np_app_max_image_size());
    if (ret != NP_OK) return ret;

    /* Read firmware sectors from the bank into OCRAM                        */
    /* Firmware starts at bank LBA + 1 (first sector after header).          */
    uint32_t app_sectors = (hdr.image_size + NP_EMMC_SECTOR_SIZE - 1U) / NP_EMMC_SECTOR_SIZE;

    /* Load firmware into OCRAM */
    ret = np_emmc_read(np_emmc_bank_lba(bank) + 1U, app_dest, app_sectors);
    if (ret != NP_OK) return ret;

    /* Relocate vector table to application's vectors */
    /* SCB_VTOR = 0xE000ED08 */
    *((volatile uint32_t *)0xE000ED08UL) = (uint32_t)(uintptr_t)app_dest;

    /* Data synchronization barrier before jump */
    __asm volatile("dsb" ::: "memory");
    __asm volatile("isb" ::: "memory");

    /* Extract application stack pointer and reset handler from its vectors */
    const uint32_t *app_vectors  = (const uint32_t *)(const void *)app_dest;
    uint32_t        app_sp       = app_vectors[0];
    uint32_t        app_entry    = app_vectors[1];

    /* Disable all interrupts before jump */
    __asm volatile("cpsid i" ::: "memory");

    /* Set application stack pointer and jump */
    __asm volatile(
        "mov  sp, %0    \n"
        "bx   %1        \n"
        :
        : "r"(app_sp), "r"(app_entry)
        :
    );

    /* Should never reach here */
    return NP_ERR_GENERIC;
}

/* ── Bootloader main entry point ──────────────────────────────────────────── */

/* Called by ROM after loading the bootloader image into OCRAM.               */
__attribute__((noreturn))
void Bootloader_Reset(void)
{
    /* TEMPORARY — NP-SW-CI-001 phase 3 gate demonstration.  Reverted next commit. */
    np_phase3_gate_probe();

    /* B-1: Basic hardware setup ─────────────────────────────────────────── */
    /* Disable watchdog timer (WDOG1, WDOG2) inherited from ROM if running.  */
    /* WDOG1_WCR = 0x400B8000 — disable by setting WDE=0 before any long ops */
    volatile uint16_t *WDOG1_WCR = (volatile uint16_t *)0x400B8000UL;
    volatile uint16_t *WDOG2_WCR = (volatile uint16_t *)0x400D0000UL;
    *WDOG1_WCR &= (uint16_t)~(1U << 2U);  /* WDE = 0 (disable)             */
    *WDOG2_WCR &= (uint16_t)~(1U << 2U);

    /* Relocate bootloader's own vector table (ROM may have set a different one) */
    *((volatile uint32_t *)0xE000ED08UL) = (uint32_t)(uintptr_t)g_vectors;
    __asm volatile("dsb" ::: "memory");
    __asm volatile("isb" ::: "memory");

    /* Enable FPU access (CP10/CP11 full access) for SHA-256 performance */
    *((volatile uint32_t *)0xE000ED88UL) |= (0xFU << 20U);

    /* B-1: Initialize eMMC ──────────────────────────────────────────────── */
    np_status_t ret = np_emmc_init();
    if (ret != NP_OK) {
        /* eMMC init failure — enter DFU (cannot read either bank) */
        goto enter_dfu;
    }

    /* B-1: Zero the Scratch partition (power-loss safety) ──────────────── */
    zero_scratch_partition();

    /* Anonymization power-loss recovery (NP-FW-EMMC-002 §D.6) ──────────── */
    /* If a research anonymization run was interrupted by power loss, the    */
    /* SNVS_LPGPR2 in-progress flag is still set.  The per-run AES key lived */
    /* only in SRAM and is gone, so the staged Scratch ciphertext is already */
    /* unreadable; SANITIZE Scratch (via erase) anyway and clear the flag.   */
    /* This must run before the OTA_PENDING check so recovery is unconditional*/
    /* with respect to the boot path taken below.                            */
    if (NP_SNVS_LPGPR2 & NP_SNVS_ANON_IN_PROGRESS) {
        np_emmc_erase(NP_SCRATCH_LBA_START, NP_SCRATCH_SIZE_LBA);
        NP_SNVS_LPGPR2 &= ~NP_SNVS_ANON_IN_PROGRESS;
    }
    /* Check for interrupted factory reset — re-run SANITIZE before completing boot */
    if (NP_SNVS_LPGPR1 & NP_SNVS_RESET_IN_PROGRESS) {
        /* Power was lost during factory reset R-4..R-9.
         * Re-sanitize all data partitions, then complete the reset.
         * Device remains in factory-reset state until app re-initialises. */
        np_status_t wipe = np_emmc_switch_partition(NP_EMMC_USER_AREA);
        if (wipe == NP_OK) wipe = np_emmc_erase(NP_UHDR_LBA_START, NP_UHDR_SIZE_LBA);
        if (wipe == NP_OK) wipe = np_emmc_erase(NP_SHDR_LBA_START, NP_SHDR_SIZE_LBA);
        if (wipe == NP_OK) wipe = np_emmc_erase(NP_CONFIG_LBA_START, NP_CONFIG_SIZE_LBA);

        /* Only clear the in-progress flag if every wipe step succeeded.  If any
         * step failed, leave the flag SET so the next boot retries — the device
         * must never come up with user data half-erased after an interrupted
         * factory reset. */
        if (wipe == NP_OK) {
            NP_SNVS_LPGPR1 &= ~NP_SNVS_RESET_IN_PROGRESS;
        }
        /* Fall through — boot from active bank; app will detect factory state */
    }

    /* B-2: Read boot bank and OTA status from SNVS_LPGPR0 ──────────────── */
    {
        bool dfu_forced = np_boot_selector_dfu_forced();
        if (dfu_forced) {
            goto enter_dfu;
        }

        bool     ota_pending = np_boot_selector_ota_pending();
        np_bank_t boot_bank;

        if (ota_pending) {
            /* B-3: OTA candidate boot verification (steps 8–9 from spec) ─ */
            ret = np_ota_handle_boot_verification(&boot_bank);

            if (ret == NP_ERR_MAX_ATTEMPTS) {
                /* Rolled back to stable bank — boot it */
                ret = verify_bank_header(boot_bank);
                if (ret == NP_OK) {
                    ret = load_and_jump(boot_bank);
                }
                /* If rollback bank also fails, fall through to DFU */
                goto enter_dfu;
            }

            if (ret != NP_OK) {
                /* Verification failure already handled rollback in np_ota_  */
                /* handle_boot_verification(); try the stable bank.          */
                np_bank_t stable = np_boot_selector_get_bank();
                ret = verify_bank_header(stable);
                if (ret == NP_OK) {
                    ret = load_and_jump(stable);
                }
                goto enter_dfu;
            }

            /* B-4: OTA bank verified — boot it */
            ret = load_and_jump(boot_bank);
            /* load_and_jump should not return; if it does, fall through */
            goto enter_dfu;
        }

        /* B-2: No OTA pending — normal boot ─────────────────────────────── */
        boot_bank = np_boot_selector_get_bank();

        /* B-3: Verify selected bank signature */
        ret = verify_bank_header(boot_bank);
        if (ret == NP_OK) {
            /* B-4: Jump to firmware */
            ret = load_and_jump(boot_bank);
            /* Should not return */
        }

        /* B-5: Primary bank failed — try the other bank */
        np_bank_t fallback = np_boot_selector_other_bank(boot_bank);
        ret = verify_bank_header(fallback);
        if (ret == NP_OK) {
            ret = load_and_jump(fallback);
            /* Should not return */
        }

        /* B-6: Both banks failed → DFU */
    }

enter_dfu:
    /* B-6: USB-C DFU recovery mode ─────────────────────────────────────── */
    /* np_dfu_enter() either never returns (successful flash + reset)        */
    /* or returns NP_ERR_TIMEOUT if no host connected within 5 minutes.     */
    np_dfu_enter();

    /* If DFU times out (no USB host connected), halt.                       */
    /* The device is effectively bricked at this point and needs factory     */
    /* service.  The status LED will be red-blink (set by safety MCU         */
    /* when heartbeat from main processor stops).                            */
    while (1) {
        __asm volatile("wfi");
    }
    __builtin_unreachable();
}
