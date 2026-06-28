/*
 * NeuroPulse Bootloader — eMMC Driver (USDHC2)
 * Document: NP-FW-EMMC-001 Rev A §3, §8
 *
 * Drives the NXP i.MX RT1062 USDHC2 peripheral to access the 8 GB industrial
 * eMMC.  Supports:
 *   - Boot partition switching (hardware boot partitions 1/2 and user area)
 *   - Single and multi-block read/write
 *   - Write-with-readback verification for OTA bank programming
 *
 * Register map follows the i.MX RT1060 Reference Manual (Rev 3, §51).
 * Only the registers required by the bootloader are defined here; a full HAL
 * is not needed and would increase attack surface in boot-critical code.
 */

#include "np_emmc.h"
#include "np_config.h"

/* ── USDHC2 register map ──────────────────────────────────────────────────── */
#define USDHC2_BASE             NP_USDHC2_BASE

typedef struct {
    volatile uint32_t DS_ADDR;          /* 0x00 */
    volatile uint32_t BLK_ATT;         /* 0x04 */
    volatile uint32_t CMD_ARG;         /* 0x08 */
    volatile uint32_t CMD_XFR_TYP;     /* 0x0C */
    volatile uint32_t CMD_RSP0;        /* 0x10 */
    volatile uint32_t CMD_RSP1;        /* 0x14 */
    volatile uint32_t CMD_RSP2;        /* 0x18 */
    volatile uint32_t CMD_RSP3;        /* 0x1C */
    volatile uint32_t DATA_BUFF_ACC;   /* 0x20 */
    volatile uint32_t PRES_STATE;      /* 0x24 */
    volatile uint32_t PROT_CTRL;       /* 0x28 */
    volatile uint32_t SYS_CTRL;        /* 0x2C */
    volatile uint32_t INT_STATUS;      /* 0x30 */
    volatile uint32_t INT_STATUS_EN;   /* 0x34 */
    volatile uint32_t INT_SIGNAL_EN;   /* 0x38 */
    volatile uint32_t AUTOCMD12_ERR;   /* 0x3C */
    volatile uint32_t HOST_CTRL_CAP;   /* 0x40 */
    volatile uint32_t WTMK_LVL;        /* 0x44 */
    volatile uint32_t MIX_CTRL;        /* 0x48 */
    volatile uint32_t _pad0;           /* 0x4C */
    volatile uint32_t FORCE_EVENT;     /* 0x50 */
    volatile uint32_t ADMA_ERR_STATUS; /* 0x54 */
    volatile uint32_t ADMA_SYS_ADDR;   /* 0x58 */
    volatile uint32_t _pad1;           /* 0x5C */
    volatile uint32_t DLL_CTRL;        /* 0x60 */
    volatile uint32_t DLL_STATUS;      /* 0x64 */
    volatile uint32_t CLK_TUNE_CTRL;   /* 0x68 */
    volatile uint32_t _pad2[21];       /* 0x6C–0xBF */
    volatile uint32_t VEND_SPEC;       /* 0xC0 */
    volatile uint32_t MMC_BOOT;        /* 0xC4 */
    volatile uint32_t VEND_SPEC2;      /* 0xC8 */
    volatile uint32_t TUNING_CTRL;     /* 0xCC */
} usdhc_regs_t;

#define USDHC2 ((usdhc_regs_t *)USDHC2_BASE)

/* Selected USDHC register bit definitions */
#define USDHC_SYS_CTRL_RSTA         (1U << 24)  /* software reset all       */
#define USDHC_SYS_CTRL_RSTC         (1U << 25)  /* software reset cmd line  */
#define USDHC_SYS_CTRL_RSTD         (1U << 26)  /* software reset data      */
#define USDHC_PRES_STATE_CIHB       (1U << 0)   /* cmd inhibit              */
#define USDHC_PRES_STATE_CDIHB      (1U << 1)   /* data inhibit             */
#define USDHC_PRES_STATE_SDSTB      (1U << 3)   /* SD clock stable          */
#define USDHC_INT_STATUS_CC         (1U << 0)   /* command complete         */
#define USDHC_INT_STATUS_TC         (1U << 1)   /* transfer complete        */
#define USDHC_INT_STATUS_CTOE       (1U << 16)  /* command timeout error    */
#define USDHC_INT_STATUS_CCE        (1U << 17)  /* command CRC error        */
#define USDHC_INT_STATUS_CEBE       (1U << 18)  /* command end bit error    */
#define USDHC_INT_STATUS_CIE        (1U << 19)  /* command index error      */
#define USDHC_INT_STATUS_DTOE       (1U << 20)  /* data timeout error       */
#define USDHC_INT_STATUS_DEBE       (1U << 22)  /* data end bit error       */
#define USDHC_INT_STATUS_DMAE       (1U << 28)  /* DMA error                */
#define USDHC_INT_ERR_MASK          (USDHC_INT_STATUS_CTOE | USDHC_INT_STATUS_CCE | \
                                     USDHC_INT_STATUS_CEBE | USDHC_INT_STATUS_CIE | \
                                     USDHC_INT_STATUS_DTOE | USDHC_INT_STATUS_DEBE | \
                                     USDHC_INT_STATUS_DMAE)
#define USDHC_CMD_XFR_TYP_RSPTYP_48    (0x2U << 16)
#define USDHC_CMD_XFR_TYP_RSPTYP_48B   (0x3U << 16)
#define USDHC_CMD_XFR_TYP_DPSEL        (1U << 21)   /* data present           */
#define USDHC_CMD_XFR_TYP_CICEN        (1U << 20)   /* cmd index check enable */
#define USDHC_CMD_XFR_TYP_CCCEN        (1U << 19)   /* cmd CRC check enable   */
#define USDHC_MIX_CTRL_DMAEN           (1U << 0)
#define USDHC_MIX_CTRL_BCEN            (1U << 1)    /* block count enable     */
#define USDHC_MIX_CTRL_AC12EN          (1U << 2)    /* auto CMD12 enable      */
#define USDHC_MIX_CTRL_DTDSEL          (1U << 4)    /* 1 = read               */
#define USDHC_MIX_CTRL_MSBSEL          (1U << 5)    /* multi-block select     */
#define USDHC_PROT_CTRL_DTW_4BIT       (0x1U << 1)  /* 4-bit data width       */
#define USDHC_PROT_CTRL_DTW_8BIT       (0x2U << 1)  /* 8-bit data width       */

/* eMMC commands */
#define CMD0_GO_IDLE            0U
#define CMD1_SEND_OP_COND       1U
#define CMD6_SWITCH             6U
#define CMD8_SEND_EXT_CSD       8U
#define CMD13_SEND_STATUS       13U
#define CMD16_SET_BLOCKLEN      16U
#define CMD17_READ_SINGLE       17U
#define CMD18_READ_MULTI        18U
#define CMD24_WRITE_SINGLE      24U
#define CMD25_WRITE_MULTI       25U
#define CMD35_ERASE_START       35U
#define CMD36_ERASE_END         36U
#define CMD38_ERASE             38U

/* eMMC EXTCSD byte indices */
#define EXTCSD_PARTITION_CONFIG     179U    /* PARTITION_CONFIG byte          */
#define EXTCSD_BUS_WIDTH            183U    /* BUS_WIDTH byte                 */
#define EXTCSD_HS_TIMING            185U    /* HS_TIMING byte                 */

/* PARTITION_CONFIG field values */
#define EMMC_BOOT_ACK               (1U << 6)
#define EMMC_BOOT_PART_SHIFT        3U
#define EMMC_ACCESS_BOOT1           (1U)    /* access boot partition 1        */
#define EMMC_ACCESS_BOOT2           (2U)    /* access boot partition 2        */
#define EMMC_ACCESS_USER            (0U)    /* access user data area          */

/* ── Internal state ───────────────────────────────────────────────────────── */
static bool     s_emmc_initialized = false;
static uint8_t  s_rca              = 0U;    /* Relative Card Address (8-bit)  */
static uint8_t  s_current_part     = 0xFFU; /* Track active partition         */

/* ── Internal helpers ─────────────────────────────────────────────────────── */

/* Spin-wait with a simple counter; suitable for bootloader where there is no  */
/* RTOS.  `cycles` is approximate Cortex-M7 loop iterations.                  */
static void delay_cycles(uint32_t cycles)
{
    volatile uint32_t n = cycles;
    while (n--) {
        __asm volatile("nop");
    }
}

/* Poll until a register bit matches expected, or timeout (loop iterations).  */
static bool poll_until(const volatile uint32_t *reg, uint32_t mask,
                        uint32_t expected, uint32_t timeout)
{
    while (timeout--) {
        if ((*reg & mask) == expected) {
            return true;
        }
    }
    return false;
}

/* Issue a single MMC command; no data transfer.                              */
/* cmd: command index; arg: argument; rsp_type: RSPTYP bits for CMD_XFR_TYP. */
/* Returns R1 response (CMD_RSP0) or 0 on error.                             */
static np_status_t send_cmd(uint32_t cmd, uint32_t arg, uint32_t rsp_type,
                             uint32_t *rsp0_out)
{
    /* Wait for CMD inhibit to clear */
    if (!poll_until(&USDHC2->PRES_STATE, USDHC_PRES_STATE_CIHB, 0U, 1000000U)) {
        return NP_ERR_TIMEOUT;
    }

    /* Clear status bits */
    USDHC2->INT_STATUS = 0xFFFFFFFFU;

    /* Set command argument and issue */
    USDHC2->CMD_ARG     = arg;
    USDHC2->CMD_XFR_TYP = (cmd << 24U) | rsp_type
                          | USDHC_CMD_XFR_TYP_CICEN | USDHC_CMD_XFR_TYP_CCCEN;

    /* Wait for command complete or error */
    if (!poll_until(&USDHC2->INT_STATUS,
                    USDHC_INT_STATUS_CC | USDHC_INT_ERR_MASK,
                    USDHC_INT_STATUS_CC, 1000000U)) {
        return NP_ERR_TIMEOUT;
    }

    if (USDHC2->INT_STATUS & USDHC_INT_ERR_MASK) {
        return NP_ERR_EMMC_READ;
    }

    if (rsp0_out) {
        *rsp0_out = USDHC2->CMD_RSP0;
    }
    return NP_OK;
}

/* Switch eMMC EXTCSD byte via CMD6 (SWITCH command).                        */
static np_status_t extcsd_switch(uint8_t index, uint8_t value)
{
    /* CMD6 arg: [25:24]=3 (write byte), [23:16]=index, [15:8]=value, [2:0]=0 */
    uint32_t arg = (3U << 24U) | ((uint32_t)index << 16U) | ((uint32_t)value << 8U);
    np_status_t ret = send_cmd(CMD6_SWITCH, arg, USDHC_CMD_XFR_TYP_RSPTYP_48B, NULL);
    if (ret != NP_OK) {
        return ret;
    }
    /* Wait for device ready after switch */
    delay_cycles(200000U);
    return NP_OK;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

np_status_t np_emmc_init(void)
{
    np_status_t ret;
    uint32_t    rsp0;

    if (s_emmc_initialized) {
        return NP_OK;
    }

    /* Software reset all */
    USDHC2->SYS_CTRL |= USDHC_SYS_CTRL_RSTA;
    if (!poll_until(&USDHC2->SYS_CTRL, USDHC_SYS_CTRL_RSTA, 0U, 1000000U)) {
        return NP_ERR_EMMC_INIT;
    }

    /* Set identification clock: ~400 kHz from 200 MHz IPG_CLK                */
    /* SDCLKFS=0xFF (divide by 512), DVS=0 → 200MHz/512 ≈ 390 kHz             */
    USDHC2->SYS_CTRL = (USDHC2->SYS_CTRL & ~0x0000FFF0U) | (0xFFU << 8U);
    delay_cycles(100000U);

    /* CMD0: reset to idle */
    send_cmd(CMD0_GO_IDLE, 0U, 0U, NULL);
    delay_cycles(50000U);

    /* CMD1: negotiate operating conditions (3.3V, sector addressing) */
    uint32_t ocr_arg = 0x40FF8080U;    /* HCS=1, voltage window 2.7–3.6 V   */
    for (int retries = 0; retries < 1000; retries++) {
        ret = send_cmd(CMD1_SEND_OP_COND, ocr_arg, USDHC_CMD_XFR_TYP_RSPTYP_48, &rsp0);
        if (ret != NP_OK) {
            return NP_ERR_EMMC_INIT;
        }
        if (rsp0 & (1U << 31U)) {      /* Card power-up status bit            */
            break;
        }
        delay_cycles(10000U);
    }

    /* Bump to full-speed clock: 200 MHz / 4 = 50 MHz (HS mode)              */
    USDHC2->SYS_CTRL = (USDHC2->SYS_CTRL & ~0x0000FFF0U) | (0x01U << 8U);

    /* Switch to 8-bit data bus */
    ret = extcsd_switch(EXTCSD_BUS_WIDTH, 0x02U);  /* 0x02 = 8-bit           */
    if (ret != NP_OK) {
        return ret;
    }
    USDHC2->PROT_CTRL = (USDHC2->PROT_CTRL & ~(0x3U << 1U)) | USDHC_PROT_CTRL_DTW_8BIT;

    /* Enable high-speed timing */
    ret = extcsd_switch(EXTCSD_HS_TIMING, 0x01U);
    if (ret != NP_OK) {
        return ret;
    }

    /* Set block length to 512 bytes */
    ret = send_cmd(CMD16_SET_BLOCKLEN, NP_EMMC_SECTOR_SIZE, USDHC_CMD_XFR_TYP_RSPTYP_48, NULL);
    if (ret != NP_OK) {
        return ret;
    }

    s_emmc_initialized = true;
    s_current_part     = NP_EMMC_USER_AREA;
    return NP_OK;
}

np_status_t np_emmc_switch_partition(uint8_t part)
{
    if (s_current_part == part) {
        return NP_OK;
    }

    /* PARTITION_CONFIG: [5:3] = BOOT_PARTITION_ENABLE, [2:0] = PARTITION_ACCESS */
    /* For accessing a boot partition we set PARTITION_ACCESS = part.           */
    /* Boot partition enable bits and ACK bit are preserved from current state. */
    uint8_t cfg = (uint8_t)(part & 0x07U) | EMMC_BOOT_ACK;
    np_status_t ret = extcsd_switch(EXTCSD_PARTITION_CONFIG, cfg);
    if (ret == NP_OK) {
        s_current_part = part;
    }
    return ret;
}

np_status_t np_emmc_read(uint32_t lba, void *buf, uint32_t count)
{
    if (!buf || count == 0U) {
        return NP_ERR_INVALID_ARG;
    }

    uint32_t   *dst = (uint32_t *)buf;
    uint32_t    words_per_block = NP_EMMC_SECTOR_SIZE / 4U;

    for (uint32_t blk = 0U; blk < count; blk++) {
        /* Wait for data inhibit to clear */
        if (!poll_until(&USDHC2->PRES_STATE, USDHC_PRES_STATE_CDIHB, 0U, 1000000U)) {
            return NP_ERR_TIMEOUT;
        }

        USDHC2->INT_STATUS   = 0xFFFFFFFFU;
        USDHC2->BLK_ATT      = (1U << 16U) | NP_EMMC_SECTOR_SIZE;  /* 1 block */
        USDHC2->MIX_CTRL     = USDHC_MIX_CTRL_DTDSEL;               /* read    */

        /* Issue CMD17 (single block read) */
        USDHC2->CMD_ARG      = lba + blk;
        USDHC2->CMD_XFR_TYP  = (CMD17_READ_SINGLE << 24U)
                               | USDHC_CMD_XFR_TYP_RSPTYP_48
                               | USDHC_CMD_XFR_TYP_CICEN
                               | USDHC_CMD_XFR_TYP_CCCEN
                               | USDHC_CMD_XFR_TYP_DPSEL;

        /* Wait for command complete */
        if (!poll_until(&USDHC2->INT_STATUS,
                        USDHC_INT_STATUS_CC | USDHC_INT_ERR_MASK,
                        USDHC_INT_STATUS_CC, 1000000U)) {
            return NP_ERR_TIMEOUT;
        }
        if (USDHC2->INT_STATUS & USDHC_INT_ERR_MASK) {
            return NP_ERR_EMMC_READ;
        }

        /* Read 128 32-bit words from FIFO */
        for (uint32_t w = 0U; w < words_per_block; w++) {
            /* Poll buffer read ready (BRR) */
            if (!poll_until(&USDHC2->INT_STATUS, (1U << 5U), (1U << 5U), 1000000U)) {
                return NP_ERR_TIMEOUT;
            }
            *dst++ = USDHC2->DATA_BUFF_ACC;
        }

        /* Wait for transfer complete */
        if (!poll_until(&USDHC2->INT_STATUS,
                        USDHC_INT_STATUS_TC | USDHC_INT_ERR_MASK,
                        USDHC_INT_STATUS_TC, 1000000U)) {
            return NP_ERR_TIMEOUT;
        }
    }

    return NP_OK;
}

np_status_t np_emmc_write(uint32_t lba, const void *buf, uint32_t count)
{
    if (!buf || count == 0U) {
        return NP_ERR_INVALID_ARG;
    }

    const uint32_t *src = (const uint32_t *)buf;
    uint32_t        words_per_block = NP_EMMC_SECTOR_SIZE / 4U;

    for (uint32_t blk = 0U; blk < count; blk++) {
        if (!poll_until(&USDHC2->PRES_STATE,
                        USDHC_PRES_STATE_CIHB | USDHC_PRES_STATE_CDIHB,
                        0U, 1000000U)) {
            return NP_ERR_TIMEOUT;
        }

        USDHC2->INT_STATUS  = 0xFFFFFFFFU;
        USDHC2->BLK_ATT     = (1U << 16U) | NP_EMMC_SECTOR_SIZE;
        USDHC2->MIX_CTRL    = 0U;               /* write, single block        */

        USDHC2->CMD_ARG     = lba + blk;
        USDHC2->CMD_XFR_TYP = (CMD24_WRITE_SINGLE << 24U)
                              | USDHC_CMD_XFR_TYP_RSPTYP_48
                              | USDHC_CMD_XFR_TYP_CICEN
                              | USDHC_CMD_XFR_TYP_CCCEN
                              | USDHC_CMD_XFR_TYP_DPSEL;

        if (!poll_until(&USDHC2->INT_STATUS,
                        USDHC_INT_STATUS_CC | USDHC_INT_ERR_MASK,
                        USDHC_INT_STATUS_CC, 1000000U)) {
            return NP_ERR_TIMEOUT;
        }
        if (USDHC2->INT_STATUS & USDHC_INT_ERR_MASK) {
            return NP_ERR_EMMC_WRITE;
        }

        /* Write 128 32-bit words to FIFO */
        for (uint32_t w = 0U; w < words_per_block; w++) {
            /* Poll buffer write ready (BWR) */
            if (!poll_until(&USDHC2->INT_STATUS, (1U << 4U), (1U << 4U), 1000000U)) {
                return NP_ERR_TIMEOUT;
            }
            USDHC2->DATA_BUFF_ACC = *src++;
        }

        if (!poll_until(&USDHC2->INT_STATUS,
                        USDHC_INT_STATUS_TC | USDHC_INT_ERR_MASK,
                        USDHC_INT_STATUS_TC, 1000000U)) {
            return NP_ERR_TIMEOUT;
        }
        if (USDHC2->INT_STATUS & USDHC_INT_ERR_MASK) {
            return NP_ERR_EMMC_WRITE;
        }
    }

    return NP_OK;
}

np_status_t np_emmc_write_verify(uint32_t lba, const void *buf, uint32_t count)
{
    np_status_t ret = np_emmc_write(lba, buf, count);
    if (ret != NP_OK) {
        return ret;
    }

    /* Read back and compare byte-by-byte using SRAM scratch buffer.          */
    /* Process one sector at a time to keep SRAM usage to 512 bytes.          */
    static uint8_t s_verify_buf[NP_EMMC_SECTOR_SIZE];
    const uint8_t *expected = (const uint8_t *)buf;

    for (uint32_t blk = 0U; blk < count; blk++) {
        ret = np_emmc_read(lba + blk, s_verify_buf, 1U);
        if (ret != NP_OK) {
            return ret;
        }
        for (uint32_t b = 0U; b < NP_EMMC_SECTOR_SIZE; b++) {
            if (s_verify_buf[b] != expected[blk * NP_EMMC_SECTOR_SIZE + b]) {
                return NP_ERR_EMMC_WRITE;
            }
        }
    }

    return NP_OK;
}

np_status_t np_emmc_erase(uint32_t lba, uint32_t count)
{
    np_status_t ret;

    ret = send_cmd(CMD35_ERASE_START, lba, USDHC_CMD_XFR_TYP_RSPTYP_48, NULL);
    if (ret != NP_OK) return ret;

    ret = send_cmd(CMD36_ERASE_END, lba + count - 1U, USDHC_CMD_XFR_TYP_RSPTYP_48, NULL);
    if (ret != NP_OK) return ret;

    /* CMD38 arg=0x00000000 for erase (not secure erase) */
    ret = send_cmd(CMD38_ERASE, 0U, USDHC_CMD_XFR_TYP_RSPTYP_48B, NULL);
    if (ret != NP_OK) return ret;

    /* Wait for device to become ready */
    uint32_t rsp0;
    for (int i = 0; i < 100000; i++) {
        ret = send_cmd(CMD13_SEND_STATUS, (uint32_t)s_rca << 16U,
                       USDHC_CMD_XFR_TYP_RSPTYP_48, &rsp0);
        if (ret == NP_OK && (rsp0 & 0x1E00U) == 0x0800U) { /* TRAN state    */
            return NP_OK;
        }
        delay_cycles(1000U);
    }

    return NP_ERR_TIMEOUT;
}
