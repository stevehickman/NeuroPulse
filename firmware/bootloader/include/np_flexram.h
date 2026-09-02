/*
 * NeurOne Bootloader — i.MX RT1062 FlexRAM Partition
 * Document: NP-SW-CI-001 §4.10 (closes OI-SWCI-39)
 *
 * ── What this file exists to fix ─────────────────────────────────────────────
 *
 * Until this module, NOTHING in this repository configured the FlexRAM
 * partition, and three files nonetheless asserted a memory map that depends on
 * it: bootloader_imxrt1062.ld (OCRAM 512 KiB at 0x20200000),
 * app_imxrt1062.ld (DTCM 128 KiB at 0x20000000, copied from the MCUX SDK's
 * m_data) and np_config.h.  The split was whatever the eFuses happened to say.
 *
 * A partition set by fuses is not thereby wrong — the point is that it was not
 * a NeurOne decision, was not written down, and could not be checked.  It is
 * the assumption every one of those three declarations rests on, and it was the
 * only one of them with no owner.
 *
 * ── The decision ─────────────────────────────────────────────────────────────
 *
 *   ITCM  0 banks  ·  DTCM 16 banks (512 KiB)  ·  FlexRAM OCRAM 0 banks
 *
 * Derived rather than chosen — the derivation, and why the two zeroes are not
 * waste, is in np_config.h next to the constants.
 *
 * ── Why the bootloader, and why first ────────────────────────────────────────
 *
 * The application's SDK Reset_Handler sets MSP from __isr_vector[0], which the
 * application linker script places at the top of DTCM, and then copies .data
 * into DTCM.  Both happen before its first C statement, so DTCM must already
 * exist at the size the application was linked for.  The bootloader is the only
 * code that runs earlier, so this is its job.
 *
 * Doing it as the FIRST thing in Bootloader_Reset() is not decoration.  The
 * property worth having is "no code runs before the memory map is established",
 * which is trivially checkable when the call is first and requires an argument
 * about every preceding statement otherwise.
 *
 * ── Why it is safe to do this while running ──────────────────────────────────
 *
 * Reassigning a bank discards its contents, so this is only safe if nothing
 * live is in FlexRAM.  For the bootloader that is a property of its linker
 * script rather than a promise: bootloader_imxrt1062.ld declares exactly ONE
 * memory region, OCRAM, and OCRAM is the dedicated OCRAM2 array at 0x20200000,
 * which is not part of FlexRAM.  Code, .data, .bss, .usb_qh, the staging
 * reservation and the stack are all inside it.  There is no second region a
 * section could have landed in, so nothing of the bootloader's can be in a bank
 * this function reassigns.
 *
 * The boot ROM is likewise not a hazard: it loaded this image into OCRAM2 and
 * has been left behind — Bootloader_Reset() does not return to it.
 */

#ifndef NP_FLEXRAM_H
#define NP_FLEXRAM_H

#include <stdint.h>

/* Returned by np_flexram_tcm_size_code() for a size the CM7_CFG*TCMSZ field
 * cannot encode.  Deliberately not a legal 4-bit code, so a caller that
 * forgets to check writes an obviously-wrong register rather than a plausible
 * one.  It is a compile-time-constant input in this bootloader, and the host
 * tests are what check it. */
#define NP_FLEXRAM_TCM_SIZE_INVALID  0xFFFFFFFFUL

/*
 * Build the IOMUXC_GPR17 FLEXRAM_BANK_CFG word for a bank split.
 *
 * Two bits per bank, bank n in bits [2n+1:2n]: 0b00 unused, 0b01 OCRAM,
 * 0b10 DTCM, 0b11 ITCM.  Banks are assigned in OCRAM, then DTCM, then ITCM
 * order from bank 0 upward — the same order as the MCUX SDK's
 * FLEXRAM_AllocateRam(), which is the reference this must not diverge from,
 * since a bank-config word with the fields in a different order produces a
 * device that boots into memory that is not where it was linked.
 *
 * Pure: no register access, so the host tests check the encoding directly
 * against a hand-computed oracle.  Returns 0 if the counts exceed
 * NP_FLEXRAM_BANK_COUNT — an all-unused word, which is inert rather than
 * plausible.
 */
uint32_t np_flexram_bank_cfg(uint8_t ocram_banks,
                             uint8_t dtcm_banks,
                             uint8_t itcm_banks);

/*
 * Encode a TCM window size for GPR14's CM7_CFGITCMSZ / CM7_CFGDTCMSZ.
 *
 * The field is a 4-bit code, size = 2^(code-1) KiB, valid for 4 KiB (3)
 * through 512 KiB (10); code 0 means the window is 0 KiB.  Anything else —
 * a non-power-of-two, 1 or 2 KiB, or more than 512 KiB — returns
 * NP_FLEXRAM_TCM_SIZE_INVALID.
 *
 * The window is sized to the banks actually backing it rather than opened to
 * the 512 KiB maximum.  A window wider than its backing turns a wild pointer
 * into a FlexRAM access error at an address that looks like ordinary RAM,
 * which is exactly the class of fault this bring-up cannot afford to make
 * ambiguous.
 */
uint32_t np_flexram_tcm_size_code(uint32_t size_bytes);

#ifndef NP_FLEXRAM_HOST_TEST
/*
 * Establish the partition in NP_FLEXRAM_*_BANKS.  Writes GPR17 (bank
 * assignment) and GPR14 (window sizes) first, then the single GPR16 write that
 * enables the DTCM interface and switches bank allocation from the eFuses to
 * GPR17 — staged, then committed.
 *
 * NOT built for host tests: it is three writes to absolute peripheral
 * addresses and there is nothing on a host for them to mean.  The two
 * functions above carry everything that can be checked without silicon, which
 * is the same division of labour np_app_image.c uses for its linker symbols.
 */
void np_flexram_apply_partition(void);
#endif /* NP_FLEXRAM_HOST_TEST */

#endif /* NP_FLEXRAM_H */
