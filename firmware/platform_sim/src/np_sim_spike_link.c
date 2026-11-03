/*
 * NeurOne Firmware-in-the-Loop Simulator — spike-local link fillers
 * NP-SIM-FW-001 §8 step 1.  HOST ONLY.
 *
 * What the PBM socket path pulls in beyond the platform seams: the log sinks,
 * the lease probe, the hex-map NVRAM, the protocol identity seams and the
 * power governor.  These are SPIKE-LOCAL.  Each is a stand-in for something the
 * full simulator must provide properly, and each says which:
 *
 *   log sinks        become FW_LOG events (§3.5) — done here, they are real
 *   lease probe      first-party code (np_session_lease.c) once the runner links
 *   hex-map NVRAM    first-party code behind a file-backed block device (OI-SIMFW-05)
 *   protocol seams   device serial and key: test keys only (REQ-SIMFW-08)
 *   power governor   see np_pbm_power_admit below
 */
#include "np_sim_bus.h"

#include "np_hub_types.h"
#include "np_module_map.h"
#include "np_pbm_socket_telem.h"
#include "np_session_log.h"
#include "np_session_lease.h"
#include "np_sw02_platform_hal.h"
#include "np_socket_dispatch.h"
#include "np_pbm_session.h"
#include "np_log_hrv.h"
#include "np_session_runner.h"

#include "FreeRTOS.h"

#include <string.h>

/* ── log sinks: what the firmware logs, reported as it is logged ─────────────── */

void np_log_pbm_socket(const np_pst_socket_record_t *rec, uint32_t session_ms)
{
    (void)rec;
    np_sim_emit("{\"ev\":\"fw_log\",\"t\":%u,\"fn\":\"np_log_pbm_socket\",\"session_ms\":%u}",
                (unsigned)np_sim_now_ms(), (unsigned)session_ms);
}

void np_log_shdr_pbm_socket_health(const np_pst_health_record_t *rec)
{
    (void)rec;
    np_sim_emit("{\"ev\":\"fw_log\",\"t\":%u,\"fn\":\"np_log_shdr_pbm_socket_health\"}",
                (unsigned)np_sim_now_ms());
}

void np_log_shdr_fault(uint8_t slot, np_hub_mod_type_t type, uint8_t fault_code, uint32_t session_ms)
{
    (void)session_ms;
    np_sim_emit("{\"ev\":\"fw_log\",\"t\":%u,\"fn\":\"np_log_shdr_fault\",\"slot\":%u,"
                "\"mod_type\":%d,\"fault_code\":%u}",
                (unsigned)np_sim_now_ms(), (unsigned)slot, (int)type, (unsigned)fault_code);
}

/* The HRV session's finalised record, which the real library hands to the driver's end-of-session callback. */
void np_log_hrv_session(const np_hrv_session_record_t *rec, np_hrv_status_t reason)
{
    np_sim_emit("{\"ev\":\"fw_log\",\"t\":%u,\"fn\":\"np_log_hrv_session\",\"has_record\":%s,\"reason\":%d}",
                (unsigned)np_sim_now_ms(), rec != NULL ? "true" : "false", (int)reason);
}

/* The runner is not in the loop.  np_mod_visual_ppx_halt() asks it to abort the session; the simulator reports the
 * request and does nothing else, so a photoparoxysmal halt shows as what the driver did (LEDs off, enable dropped,
 * further commands refused) plus the abort it asked for.  A double of first-party code, listed as a gap. */
void np_runner_abort(np_abort_reason_t reason)
{
    np_sim_emit("{\"ev\":\"fw_log\",\"t\":%u,\"fn\":\"np_runner_abort\",\"reason\":%d}",
                (unsigned)np_sim_now_ms(), (int)reason);
}

/* ── no session lease, no persistence ─────────────────────────────────────────── */

bool np_lease_probe_begin(void) { return true; }
void np_lease_probe_end(void)   { }
void np_pbm_session_set_uid_resolver(np_pbm_socket_uid_fn fn, void *ctx) { (void)fn; (void)ctx; }

np_hub_status_t np_hexmap_nvram_read(uint8_t *buf, size_t len, size_t *read_len)
{ (void)buf; (void)len; if (read_len != NULL) { *read_len = 0; } return NP_HUB_ERR_NOT_PRESENT; }
np_hub_status_t np_hexmap_nvram_write(const uint8_t *buf, size_t len)
{ (void)buf; (void)len; return NP_HUB_OK; }

/* ── protocol identity: not exercised by a socket command ────────────────────── */

np_hub_status_t np_proto_hal_get_device_serial(uint8_t *buf, size_t len)
{ (void)buf; (void)len; return NP_HUB_ERR_NOT_PRESENT; }
np_hub_status_t np_proto_hal_get_proto_pubkey(uint8_t *pub_key_out)
{ memset(pub_key_out, 0, 32); return NP_HUB_OK; }

/* ── power governor ──────────────────────────────────────────────────────────────
 * The governor that ships refuses EVERY load (np_pbm_power_gov.c, OI-FWHUB-09 /
 * OI-HEXTILE-09), so against it no tile can be driven and the dispatch path the
 * simulator exists to exercise is closed.  The simulator links the production
 * governor under another name and admits here, once announcing that it has.
 * Whether a simulator should admit what the device refuses is a decision, not a
 * detail: NP-SIM-FW-001 OI-SIMFW-11. */
bool np_pbm_power_admit(const np_session_cmd_t *cmd, const np_sock_disp_socket_t *current)
{
    static bool announced;
    (void)cmd; (void)current;
    if (!announced) {
        announced = true;
        np_sim_emit("{\"ev\":\"sim_policy\",\"what\":\"power governor admits every load; "
                    "the production governor refuses every load (OI-FWHUB-09)\"}");
    }
    return true;
}

/* ── critical sections ────────────────────────────────────────────────────────────
 * np_safety_spi.c brackets its shared state with taskENTER_CRITICAL.  The spike
 * is single-threaded, so these are no-ops; with the real scheduler they are the
 * POSIX port's. */
void vPortEnterCritical(void) { }
void vPortExitCritical(void)  { }
UBaseType_t xPortSetInterruptMask(void)                { return 0; }
void        vPortClearInterruptMask(UBaseType_t mask)  { (void)mask; }
