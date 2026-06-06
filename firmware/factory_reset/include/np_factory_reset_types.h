/* Document: NP-FW-EMMC-002 Rev A §B */
/*
 * NeuroPulse Device Factory Reset — Types and Status Codes
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
 */

#ifndef NP_FACTORY_RESET_TYPES_H
#define NP_FACTORY_RESET_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Status codes ─────────────────────────────────────────────────────────── */
/* NP_RESET_OK = 0; all error codes negative (project convention).            */
typedef enum {
    NP_RESET_OK                 =  0,   /* Reset completed successfully         */
    NP_RESET_ERR_INVALID_CMD    = -1,   /* R-2: command authenticity rejected   */
    NP_RESET_ERR_SANITIZE       = -2,   /* R-5/R-6/R-7: SANITIZE or zero failed  */
    NP_RESET_ERR_TRNG           = -3,   /* R-8/R-9: TRNG generation failed       */
    NP_RESET_ERR_CONFIG         = -4,   /* R-10: config defaults write failed    */
} np_reset_status_t;

/* ── Reset progress state ─────────────────────────────────────────────────── */
/* Tracks which of R-1..R-12 the reset sequence has reached.  Used for         */
/* diagnostics and to make the step ordering explicit and auditable.  The     */
/* power-loss resume path re-runs from R-5 regardless of which step was        */
/* interrupted, because R-5..R-10 are idempotent (SANITIZE/zero/re-derive).   */
typedef enum {
    NP_RESET_STATE_IDLE         = 0,    /* No reset in progress                 */
    NP_RESET_STATE_R1_VERIFIED  = 1,    /* R-1: caller verified Ed25519 command */
    NP_RESET_STATE_R2_AUTH      = 2,    /* R-2: module-level auth accepted      */
    NP_RESET_STATE_R3_FLAG_SET  = 3,    /* R-3: LPGPR1 in-progress flag set     */
    NP_RESET_STATE_R4_SUSPENDED = 4,    /* R-4: sessions suspended              */
    NP_RESET_STATE_R5_UHDR      = 5,    /* R-5: UHDR SANITIZE done              */
    NP_RESET_STATE_R6_SHDR      = 6,    /* R-6: SHDR zeroed                     */
    NP_RESET_STATE_R7_CONFIG    = 7,    /* R-7: Config zeroed                   */
    NP_RESET_STATE_R8_SALT      = 8,    /* R-8: new TRNG salt generated         */
    NP_RESET_STATE_R9_TOKEN     = 9,    /* R-9: new warranty token generated    */
    NP_RESET_STATE_R10_DEFAULTS = 10,   /* R-10: config defaults written        */
    NP_RESET_STATE_R11_FLAG_CLR = 11,   /* R-11: LPGPR1 in-progress flag cleared */
    NP_RESET_STATE_R12_REBOOT   = 12,   /* R-12: reboot requested               */
} np_reset_state_t;

#endif /* NP_FACTORY_RESET_TYPES_H */
