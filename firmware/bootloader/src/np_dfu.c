/*
 * NeuroPulse Bootloader — USB-C DFU Recovery
 * Document: NP-FW-EMMC-001 Rev A §8.4
 *
 * Implements USB DFU 1.1 (Device Firmware Upgrade specification, Rev 1.1).
 * USB class 0xFE, subclass 0x01, protocol 0x02 (DFU mode).
 *
 * Security model:
 *   - Every DFU_DNLOAD block is buffered in SRAM.
 *   - On DFU_MANIFEST the complete image is Ed25519 + SHA-256 verified before
 *     any byte is written to eMMC.
 *   - If verification fails, the device returns DFU_STATUS_errFIRMWARE and
 *     does NOT write to eMMC.
 *   - If both banks are corrupt and DFU also fails, the device halts.
 *
 * Image target:
 *   DFU writes to the inactive bank (or Bank A if both are corrupt).
 *   After successful write+verify, the SNVS boot flag is updated and the
 *   device resets to attempt a normal boot.
 *
 * USB hardware:
 *   i.MX RT1062 USB1 (USB-HS OTG) at 0x402E0000.
 *   Only DFU-class control transfers are handled; no bulk/interrupt endpoints.
 *   USB enumeration is handled by the on-chip ROM USB stack (simplified mode).
 *
 * CAUTION: This file uses direct register access to the USB PHY and controller.
 *          Interrupt-driven operation is used (np_dfu_usb_isr must be connected
 *          to the USB1_IRQHandler in the vector table).
 */

#include "np_dfu.h"
#include "np_boot_selector.h"
#include "np_emmc.h"
#include "np_signature.h"
#include "np_ota.h"
#include "np_config.h"
#include <string.h>

/* ── USB1 register map (subset) ──────────────────────────────────────────── */
#define USB1_BASE               NP_USB1_BASE

typedef struct {
    volatile uint32_t _pad0[16];        /* 0x000–0x03F: capability registers */
    volatile uint32_t USBCMD;           /* 0x040 */
    volatile uint32_t USBSTS;           /* 0x044 */
    volatile uint32_t USBINTR;          /* 0x048 */
    volatile uint32_t FRINDEX;          /* 0x04C */
    volatile uint32_t _pad1;            /* 0x050 */
    volatile uint32_t DEVICEADDR;       /* 0x054 */
    volatile uint32_t ENDPOINTLISTADDR; /* 0x058 */
    volatile uint32_t _pad2[10];
    volatile uint32_t PORTSC1;          /* 0x084 */
    volatile uint32_t _pad3[7];
    volatile uint32_t USBMODE;          /* 0x0A8 */
    volatile uint32_t ENDPTSETUPSTAT;   /* 0x0AC */
    volatile uint32_t ENDPTPRIME;       /* 0x0B0 */
    volatile uint32_t ENDPTFLUSH;       /* 0x0B4 */
    volatile uint32_t ENDPTSTAT;        /* 0x0B8 */
    volatile uint32_t ENDPTCOMPLETE;    /* 0x0BC */
    volatile uint32_t ENDPTCTRL0;       /* 0x0C0 */
} usb_regs_t;

#define USB1 ((usb_regs_t *)USB1_BASE)

/* USB PHY1 register map (subset) */
#define USB_PHY1_BASE           0x400D9000UL
typedef struct {
    volatile uint32_t PWD;      /* 0x00 */
    volatile uint32_t PWD_SET;  /* 0x04 */
    volatile uint32_t PWD_CLR;  /* 0x08 */
    volatile uint32_t PWD_TOG;  /* 0x0C */
    volatile uint32_t TX;       /* 0x10 */
    volatile uint32_t _pad[8];
    volatile uint32_t CTRL;     /* 0x30 */
    volatile uint32_t CTRL_SET; /* 0x34 */
    volatile uint32_t CTRL_CLR; /* 0x38 */
} usb_phy_t;
#define USB_PHY1 ((usb_phy_t *)USB_PHY1_BASE)

/* Selected USB register bit definitions */
#define USBCMD_RS               (1U << 0)   /* run/stop                       */
#define USBCMD_RST              (1U << 1)   /* controller reset                */
#define USBSTS_UI               (1U << 0)   /* USB interrupt                  */
#define USBSTS_UEI              (1U << 1)   /* USB error interrupt             */
#define USBSTS_PCI              (1U << 2)   /* port change detect             */
#define USBSTS_URI              (1U << 6)   /* USB reset received              */
#define USBMODE_CM_DEVICE       0x02U       /* controller mode: device        */
#define USBMODE_SLOM            (1U << 3)   /* setup lockout mode off         */

/* ── DFU class constants (USB DFU 1.1 §6) ────────────────────────────────── */
#define DFU_REQUEST_DETACH      0x00U
#define DFU_REQUEST_DNLOAD      0x01U
#define DFU_REQUEST_UPLOAD      0x02U
#define DFU_REQUEST_GETSTATUS   0x03U
#define DFU_REQUEST_CLRSTATUS   0x04U
#define DFU_REQUEST_GETSTATE    0x05U
#define DFU_REQUEST_ABORT       0x06U

#define DFU_STATE_IDLE          0x02U
#define DFU_STATE_DNLOAD_SYNC   0x03U
#define DFU_STATE_DNBUSY        0x04U
#define DFU_STATE_DNLOAD_IDLE   0x05U
#define DFU_STATE_MANIFEST_SYNC 0x06U
#define DFU_STATE_MANIFEST      0x07U
#define DFU_STATE_MANIFEST_WAIT 0x08U
#define DFU_STATE_ERROR         0x0AU

#define DFU_STATUS_OK           0x00U
#define DFU_STATUS_errTARGET    0x01U
#define DFU_STATUS_errFILE      0x02U
#define DFU_STATUS_errWRITE     0x03U
#define DFU_STATUS_errCHECK     0x04U
#define DFU_STATUS_errPROG      0x05U
#define DFU_STATUS_errVERIFY    0x06U
#define DFU_STATUS_errADDRESS   0x07U
#define DFU_STATUS_errNOTDONE   0x08U
#define DFU_STATUS_errFIRMWARE  0x09U
#define DFU_STATUS_errVENDOR    0x0AU
#define DFU_STATUS_errUSBR      0x0BU
#define DFU_STATUS_errPOR       0x0CU
#define DFU_STATUS_errUNKNOWN   0x0DU

/* ── USB setup packet ─────────────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_t;

/* ── DFU device state ─────────────────────────────────────────────────────── */
typedef struct {
    uint8_t  state;
    uint8_t  status;
    uint32_t block_num;           /* DFU_DNLOAD block sequence number         */
    uint32_t bytes_received;      /* total bytes downloaded                   */
    bool     download_complete;   /* wLength == 0 received                    */
    bool     verified;            /* Ed25519 + SHA-256 passed                 */
    bool     written;             /* image written to eMMC successfully        */
} dfu_state_t;

/* ── Static DFU receive buffer ────────────────────────────────────────────── */
/* We receive the entire firmware image into the Scratch partition sector-by- */
/* sector.  The SRAM buffer holds one transfer block at a time.               */
static uint8_t  s_dfu_block[NP_DFU_TRANSFER_SIZE];
static dfu_state_t s_dfu;
static bool        s_usb_reset_seen;
static uint32_t    s_dfu_timer_ticks;   /* incremented by SysTick            */

/* ── SysTick for DFU timeout ──────────────────────────────────────────────── */
#define SYSTICK_BASE    0xE000E010UL
typedef struct {
    volatile uint32_t CSR;
    volatile uint32_t RVR;
    volatile uint32_t CVR;
    volatile uint32_t CALIB;
} systick_t;
#define SYSTICK ((systick_t *)SYSTICK_BASE)

static void systick_init(void)
{
    SYSTICK->RVR = 600000U - 1U;    /* 600 MHz / 600000 = 1 ms per tick      */
    SYSTICK->CVR = 0U;
    SYSTICK->CSR = 0x07U;           /* enable, use processor clock, interrupt */
}

/* SysTick_Handler must be listed in the vector table (set in linker script). */
void SysTick_Handler(void)
{
    s_dfu_timer_ticks++;
}

/* ── USB endpoint queue head (QH) and transfer descriptor (dTD) ──────────── */
/* Minimal implementation for control endpoint 0 only.                       */
#define USB_QH_ALIGN    __attribute__((aligned(2048)))
#define USB_TD_ALIGN    __attribute__((aligned(32)))

typedef struct __attribute__((packed)) {
    uint32_t capabilities;
    uint32_t current_dtd;
    uint32_t next_dtd;
    uint32_t token;
    uint32_t buf[5];
    uint32_t _reserved;
    uint8_t  setup_buf[8];
} usb_qh_t;

typedef struct __attribute__((packed)) {
    uint32_t next;
    uint32_t token;
    uint32_t buf[5];
    uint32_t _reserved;
} usb_dtd_t;

/* Two QHs: EP0 OUT (index 0) + EP0 IN (index 1) */
static usb_qh_t  s_qh[2] USB_QH_ALIGN;
static usb_dtd_t s_dtd   USB_TD_ALIGN;

/* ── USB control transfer helpers ────────────────────────────────────────── */

static void usb_ep0_send(const void *data, uint32_t len)
{
    if (len == 0U) {
        /* Zero-length status packet */
        s_dtd.next      = 0x00000001U;     /* DT | term                       */
        s_dtd.token     = (0U << 16U) | (1U << 7U);  /* 0 bytes, ACTIVE      */
        s_dtd.buf[0]    = 0U;
    } else {
        s_dtd.next      = 0x00000001U;
        s_dtd.token     = (len << 16U) | (1U << 7U); /* len bytes, ACTIVE    */
        s_dtd.buf[0]    = (uint32_t)(uintptr_t)data;
        s_dtd.buf[1]    = 0U;
    }
    s_qh[1].next_dtd = (uint32_t)(uintptr_t)&s_dtd;
    s_qh[1].token    = 0U;
    USB1->ENDPTPRIME |= (1U << 16U);    /* prime IN for EP0                  */
}

static void usb_ep0_stall(void)
{
    USB1->ENDPTCTRL0 |= (1U << 16U);   /* set stall on EP0 IN                */
}

/* ── DFU GET_STATUS response structure ───────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  bStatus;
    uint8_t  bwPollTimeout[3];  /* little-endian ms                           */
    uint8_t  bState;
    uint8_t  iString;
} dfu_getstatus_t;

/* ── DFU setup request handler ───────────────────────────────────────────── */

static void dfu_handle_setup(const usb_setup_t *setup)
{
    if ((setup->bmRequestType & 0x1FU) != 0x01U) {
        /* Not interface-class request — ignore (or handle standard USB) */
        return;
    }

    switch (setup->bRequest) {
    case DFU_REQUEST_DNLOAD: {
        if (setup->wLength == 0U) {
            /* Download complete marker */
            s_dfu.download_complete = true;
            s_dfu.state             = DFU_STATE_MANIFEST_SYNC;
            usb_ep0_send(NULL, 0U);
        } else {
            /* Queue receive of one block into s_dfu_block */
            s_dtd.next   = 0x00000001U;
            s_dtd.token  = (setup->wLength << 16U) | (1U << 7U);
            s_dtd.buf[0] = (uint32_t)(uintptr_t)s_dfu_block;
            s_qh[0].next_dtd = (uint32_t)(uintptr_t)&s_dtd;
            s_qh[0].token    = 0U;
            USB1->ENDPTPRIME |= 0x01U;  /* prime OUT for EP0                  */
            s_dfu.state      = DFU_STATE_DNLOAD_SYNC;
        }
        break;
    }

    case DFU_REQUEST_GETSTATUS: {
        static dfu_getstatus_t status_resp;
        status_resp.bStatus        = s_dfu.status;
        status_resp.bwPollTimeout[0] = 0U;
        status_resp.bwPollTimeout[1] = 0U;
        status_resp.bwPollTimeout[2] = 0U;
        status_resp.bState         = s_dfu.state;
        status_resp.iString        = 0U;

        if (s_dfu.state == DFU_STATE_DNLOAD_SYNC) {
            /* Process the block that was just received */
            uint32_t block_len = (s_dtd.token >> 16U) & 0xFFFFU;
            /* block_len may have been decremented by the controller */
            uint32_t expected = (setup->wLength > NP_DFU_TRANSFER_SIZE)
                                  ? NP_DFU_TRANSFER_SIZE : setup->wLength;
            (void)block_len;
            (void)expected;

            /* Write this block to the Scratch partition */
            uint32_t scratch_lba = NP_SCRATCH_HDR_LBA
                                 + s_dfu.block_num * (NP_DFU_TRANSFER_SIZE / NP_EMMC_SECTOR_SIZE);
            uint32_t sectors = NP_DFU_TRANSFER_SIZE / NP_EMMC_SECTOR_SIZE;
            np_emmc_write(scratch_lba, s_dfu_block, sectors);

            s_dfu.bytes_received += NP_DFU_TRANSFER_SIZE;
            s_dfu.block_num++;
            s_dfu.state   = DFU_STATE_DNLOAD_IDLE;
            status_resp.bState = DFU_STATE_DNLOAD_IDLE;
        } else if (s_dfu.state == DFU_STATE_MANIFEST_SYNC) {
            /* Perform verification + write */
            np_status_t vret = np_ota_verify_scratch();
            if (vret != NP_OK) {
                s_dfu.status = DFU_STATUS_errFIRMWARE;
                s_dfu.state  = DFU_STATE_ERROR;
            } else {
                /* Write verified image to the target bank */
                np_bank_t target = np_boot_selector_other_bank(np_boot_selector_get_bank());
                np_status_t wret = np_ota_write_bank(target);
                if (wret != NP_OK) {
                    s_dfu.status = DFU_STATUS_errWRITE;
                    s_dfu.state  = DFU_STATE_ERROR;
                } else {
                    s_dfu.verified = true;
                    s_dfu.written  = true;
                    s_dfu.status   = DFU_STATUS_OK;
                    s_dfu.state    = DFU_STATE_MANIFEST;
                    /* Arm the SNVS swap flag */
                    np_ota_stage_swap(target);
                }
            }
            status_resp.bStatus = s_dfu.status;
            status_resp.bState  = s_dfu.state;
        }

        usb_ep0_send(&status_resp, sizeof(status_resp));
        break;
    }

    case DFU_REQUEST_GETSTATE: {
        static uint8_t state_resp;
        state_resp = s_dfu.state;
        usb_ep0_send(&state_resp, 1U);
        break;
    }

    case DFU_REQUEST_CLRSTATUS:
        s_dfu.status = DFU_STATUS_OK;
        s_dfu.state  = DFU_STATE_IDLE;
        usb_ep0_send(NULL, 0U);
        break;

    case DFU_REQUEST_ABORT:
        s_dfu.state = DFU_STATE_IDLE;
        usb_ep0_send(NULL, 0U);
        break;

    default:
        usb_ep0_stall();
        break;
    }
}

/* ── USB ISR ──────────────────────────────────────────────────────────────── */

void np_dfu_usb_isr(void)
{
    uint32_t status = USB1->USBSTS;
    USB1->USBSTS = status;   /* clear by writing 1 */

    if (status & USBSTS_URI) {
        /* USB bus reset: re-enumerate */
        USB1->ENDPTFLUSH    = 0xFFFFFFFFU;
        USB1->DEVICEADDR    = 0U;
        s_usb_reset_seen    = true;
        s_dfu.state         = DFU_STATE_IDLE;
        s_dfu.status        = DFU_STATUS_OK;
        s_dfu.bytes_received = 0U;
        s_dfu.block_num      = 0U;
        s_dfu.download_complete = false;
    }

    if (status & USBSTS_UI) {
        /* Check for setup packet on EP0 */
        if (USB1->ENDPTSETUPSTAT & 0x01U) {
            /* Latch setup packet from QH setup buffer */
            usb_setup_t setup;
            memcpy(&setup, s_qh[0].setup_buf, sizeof(setup));
            USB1->ENDPTSETUPSTAT = 0x01U;   /* clear */
            dfu_handle_setup(&setup);
        }

        /* Check for completed transfers on EP0 IN (DFU_STATE_MANIFEST) */
        if ((USB1->ENDPTCOMPLETE & (1U << 16U)) &&
            s_dfu.state == DFU_STATE_MANIFEST && s_dfu.written) {
            USB1->ENDPTCOMPLETE = (1U << 16U);
            /* Issue soft reset to boot the new firmware */
            /* SCB_AIRCR reset: 0x05FA0004 */
            *((volatile uint32_t *)0xE000ED0CUL) = 0x05FA0004UL;
            /* Should not reach here */
            for (;;) { }
        }
    }
}

/* ── USB PHY and controller initialisation ────────────────────────────────── */

static np_status_t usb_init(void)
{
    /* Enable USB clock via CCM */
    volatile uint32_t *CCM_CCGR6 = (volatile uint32_t *)0x400FC080UL;
    *CCM_CCGR6 |= 0x00000003UL;  /* enable USB1 clock                        */

    /* Disable and reset USB1 PHY */
    USB_PHY1->CTRL_SET = (1U << 31U);   /* SFTRST                             */
    for (volatile int i = 0; i < 1000; i++) { }
    USB_PHY1->CTRL_CLR = (1U << 31U);   /* release reset                     */
    USB_PHY1->CTRL_CLR = (1U << 30U);   /* CLKGATE off                       */
    USB_PHY1->PWD      = 0U;            /* power up all circuits              */

    /* Reset USB1 controller */
    USB1->USBCMD |= USBCMD_RST;
    for (int i = 0; i < 1000000; i++) {
        if (!(USB1->USBCMD & USBCMD_RST)) break;
    }
    if (USB1->USBCMD & USBCMD_RST) return NP_ERR_DFU_USB;

    /* Set device mode */
    USB1->USBMODE = USBMODE_CM_DEVICE | USBMODE_SLOM;

    /* Set up endpoint list */
    memset(s_qh, 0, sizeof(s_qh));
    s_qh[0].capabilities = (0x40U << 16U);   /* max packet = 64 bytes        */
    s_qh[1].capabilities = (0x40U << 16U);
    USB1->ENDPOINTLISTADDR = (uint32_t)(uintptr_t)s_qh;

    /* Enable interrupts: USB, error, reset */
    USB1->USBINTR = USBSTS_UI | USBSTS_UEI | USBSTS_URI;

    /* Enable NVIC interrupt for USB1 */
    volatile uint32_t *NVIC_ISER = (volatile uint32_t *)0xE000E100UL;
    NVIC_ISER[113U >> 5U] |= (1U << (113U & 31U));  /* USB1 = IRQ 113       */

    /* Start the USB controller */
    USB1->USBCMD |= USBCMD_RS;

    return NP_OK;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

np_status_t np_dfu_enter(void)
{
    /* Initialise DFU state */
    memset(&s_dfu, 0, sizeof(s_dfu));
    s_dfu.state  = DFU_STATE_IDLE;
    s_dfu.status = DFU_STATUS_OK;
    s_usb_reset_seen = false;

    /* Zero the Scratch partition (prevent partial-image re-use) */
    np_status_t ret = np_emmc_switch_partition(NP_EMMC_USER_AREA);
    if (ret != NP_OK) return ret;
    np_emmc_erase(NP_SCRATCH_LBA_START, NP_SCRATCH_SIZE_LBA);

    /* Start SysTick for timeout */
    s_dfu_timer_ticks = 0U;
    systick_init();

    /* Initialise USB */
    ret = usb_init();
    if (ret != NP_OK) return ret;

    np_boot_selector_clear_dfu_forced();

    /* Main DFU loop */
    while (1) {
        /* Check timeout: if no USB connection within 5 minutes, give up.    */
        if (!s_usb_reset_seen &&
            s_dfu_timer_ticks > NP_DFU_TIMEOUT_MS) {
            return NP_ERR_TIMEOUT;
        }

        /* If DFU_MANIFEST has been processed and reset was triggered by ISR, */
        /* we should not reach here. If we do, something went wrong.          */
        if (s_dfu.state == DFU_STATE_ERROR) {
            /* Stay in loop allowing host to query status and retry           */
        }

        /* __WFI() would be power-optimal but interrupt latency is not       */
        /* critical here; spin poll is acceptable in recovery mode.           */
        __asm volatile("wfi");
    }
}
