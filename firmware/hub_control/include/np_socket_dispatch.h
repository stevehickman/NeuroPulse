/*
 * NeurOne Hub Control Program — Socket-Indexed Dispatch Registry
 * Document: NP-FW-HUB-001 Rev 2 §3.3, §5.6 (closes OI-FWHUB-01's dispatch gap)
 *
 * ── What this module is ──────────────────────────────────────────────────────
 *
 * The missing half of OI-HUB-SOCKET-01. The parser accepts
 * NP_PROTO_TARGET_SOCKET_MASK commands and np_module_map resolves sockets, but
 * until this module the only control path was np_module_registry, which is
 * indexed by SLOT, and slots 0-4 (the retired zone slots) are rejected by the
 * parser. Transcranial PBM therefore had no dispatchable path at all.
 *
 * This module is the socket-indexed counterpart: one record per socket in the
 * full 7-bit domain (NP_HEXMAP_MAX_SOCKETS), holding what the socket is being
 * driven with and when it auto-stops. The session runner routes every
 * socket-addressed command here and every slot-addressed command to the slot
 * registry, and never lets one fall back to the other (RISK-FWHUB-01).
 *
 * ── Admission, in order — every step all-or-nothing ──────────────────────────
 *
 *   1. mod_type must be socket-addressable: NP_MOD_PBM_BASE or NP_MOD_PBM_SMART.
 *      Nothing else occupies a lattice socket as a DRIVEN emitter today, and
 *      electrode modalities must not be admitted here until NP-FW-MMSOCK-001
 *      §3.6's geometry gap (OI-MMSOCK-02) is closed — see that document for
 *      which modalities may share a socket and which must not.
 *   2. The mask must select ≥1 socket.
 *   3. A STOP (params_len == 0) is admitted unconditionally past this point:
 *      stopping is always safe and must never be refused.
 *   4. The params block must be exactly the size its mod_type defines.
 *   5. Placement: every selected socket must hold a module carrying every
 *      emitter the params ask to light (np_module_map_check_placement). A
 *      socket that cannot honour the command fails the WHOLE command — a
 *      partially-honoured dose is not recoverable, and UHDR would record as
 *      delivered a dose that went to fewer sockets than the protocol named.
 *   6. Power: np_pbm_power_admit() must admit the resulting load
 *      (OI-HEXTILE-09). See that function's note — the production governor
 *      refuses every load until OI-HEXTILE-09 is designed.
 *   7. Drive every socket. If any socket's driver fails, every socket this
 *      command started is stopped again before returning the error.
 *
 * Only after step 7 succeeds does this module request NP_SAFETY_EN_PBM_CRANIAL,
 * and it drops that request only when NO socket remains active — one command's
 * stop must not cut the lattice gate under another command's still-running
 * sockets (which would leave UHDR recording those as delivered). The one
 * exception is a socket whose STOP fails: firmware has then lost control of an
 * emitter, so every socket is stopped and the cranial enable is released at
 * once — the whole-lattice hardware cut is the backstop for exactly that case.
 *
 * No FreeRTOS dependency: host-testable (np_socket_dispatch_tests).
 * IEC 62304 Class B — SW-02 hub control. Class C safety remains with the
 * safety MCU's NP_SAFETY_EN_PBM_CRANIAL bit, in series with everything here.
 */

#ifndef NP_SOCKET_DISPATCH_H
#define NP_SOCKET_DISPATCH_H

#include <stdbool.h>
#include <stdint.h>

#include "np_hub_types.h"
#include "np_module_map.h"   /* NP_HEXMAP_MAX_SOCKETS */

/* Largest params block any socket-addressable mod_type defines
 * (np_mod_pbm_smart_params_t, 9 bytes since OI-HEXTILE-25 moved it to mW/cm²). */
#define NP_SOCK_DISP_PARAMS_MAX   9U

/* Read-only per-socket view — what the power governor sees. */
typedef struct {
    bool              active;
    np_hub_mod_type_t mod_type;      /* NP_MOD_PBM_BASE / NP_MOD_PBM_SMART        */
    uint8_t           params_len;
    uint8_t           params[NP_SOCK_DISP_PARAMS_MAX];
    uint32_t          stop_at_ms;    /* 0 = no auto-stop pending (runner sentinel) */
} np_sock_disp_socket_t;

/*
 * np_pbm_power_admit — may the lattice run `cmd` on top of `current`?
 *
 * `current` is the full per-socket state before the command (index = socket
 * index, NP_HEXMAP_MAX_SOCKETS entries); `cmd` is a verified, placement-checked
 * DRIVE command (never a stop). Return true to admit.
 *
 * Defined in np_pbm_power_gov.c. The governor NP-HW-HEXTILE-001 §9.3 requires
 * ("the compiler and the session runner both need a power-budget check against
 * the negotiated USB-C PD contract") does not exist yet and cannot be written:
 * it must be denominated in watts against the PD contract (OI-HEXTILE-09), and
 * its input is undefined for `frequency: 0Hz` + duty (OI-SESPWR-03). Until it
 * is, the production definition REFUSES EVERY LOAD, so the dispatch path is
 * complete but closed by default — the same shape as the StudyDescriptorVerifier
 * that ships refusing everything (CLAUDE.md §6.3).
 */
bool np_pbm_power_admit(const np_session_cmd_t      *cmd,
                        const np_sock_disp_socket_t *current);

/* ── Driver seam (modules/np_mod_pbm.c, registry idiom — no per-module header) ─
 * socket_id is INDEX space (0-based, as np_hex_addr_t.socket_id). */
np_hub_status_t np_mod_pbm_socket_drive(uint16_t          socket_id,
                                        np_hub_mod_type_t mod_type,
                                        const void       *params,
                                        uint16_t          len);
np_hub_status_t np_mod_pbm_socket_stop(uint16_t socket_id);

/* ── API ──────────────────────────────────────────────────────────────────── */

/* Clear every socket record. Called at session load; drives no hardware. */
void np_sock_disp_reset(void);

/*
 * Admit and execute one socket-addressed command (see the header note).
 *
 * Returns NP_HUB_OK when every addressed socket was driven (or stopped);
 * otherwise NO socket is left driven by this command and:
 *   NP_HUB_ERR_INVALID_ARG   — not socket-addressable, empty mask, wrong params size
 *   NP_HUB_ERR_NOT_PRESENT   — placement failed for ≥1 socket
 *   NP_HUB_ERR_POWER_BUDGET  — refused by np_pbm_power_admit()
 *   NP_HUB_ERR_MOD_FAULT     — a socket driver failed; the command was rolled back
 *
 * Requests / releases NP_SAFETY_EN_PBM_CRANIAL itself (see header note).
 */
np_hub_status_t np_sock_disp_command(const np_session_cmd_t *cmd);

/* Stop every socket whose stop_at_ms ≤ now_ms. */
void np_sock_disp_process_stops(uint32_t now_ms);

/* Earliest pending stop strictly after now_ms, or 0 if none. */
uint32_t np_sock_disp_next_stop_ms(uint32_t now_ms);

/* Stop every active socket and release the cranial enable (session end). */
void np_sock_disp_stop_all(void);

/* Number of sockets currently driven. */
uint16_t np_sock_disp_active_count(void);

/* Read-only view of one socket's record; NULL if out of domain. */
const np_sock_disp_socket_t *np_sock_disp_socket(uint16_t socket_id);

#endif /* NP_SOCKET_DISPATCH_H */
