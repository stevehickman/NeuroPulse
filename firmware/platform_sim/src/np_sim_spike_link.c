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
 *   power governor   the production one (refuses every load); see np_pbm_power_admit below
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
#include "np_cvns_reenable.h"
#include "np_cardiac_sens.h"
#include "np_cvns_fault_summary.h"

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

/* ── cervical VNS fillers ────────────────────────────────────────────────────────
 * The fault summary's persisted blob is RAM (a real device writes the UHDR partition); notifying a central is reported.
 * np_hub_cvns_reenable_confirm and np_hub_set_active_user are np_hub_control_main.c's, which cannot be linked: the first calls
 * the real re-enable manager as the hub's does, the second records the tag.  np_log_session_count is the session counter the
 * fault record is ordered by; the simulator runs one session, so it is 1. */
static uint8_t g_cvfs_blob[256];
static size_t  g_cvfs_len;

np_hub_status_t np_cvfs_hal_load(uint8_t *buf, size_t cap, size_t *len_out)
{
    const size_t n = (g_cvfs_len < cap) ? g_cvfs_len : cap;
    memcpy(buf, g_cvfs_blob, n);
    *len_out = n;
    return NP_HUB_OK;
}
np_hub_status_t np_cvfs_hal_save(const uint8_t *buf, size_t len)
{
    if (len > sizeof g_cvfs_blob) { return NP_HUB_ERR_GENERIC; }
    memcpy(g_cvfs_blob, buf, len);
    g_cvfs_len = len;
    return NP_HUB_OK;
}
void np_cvfs_hal_notify(const uint8_t *frame, size_t len)
{
    (void)frame;
    np_sim_emit("{\"ev\":\"fw_log\",\"t\":%u,\"fn\":\"np_cvfs_hal_notify\",\"len\":%u}", (unsigned)np_sim_now_ms(), (unsigned)len);
}
np_hub_status_t np_hub_cvns_reenable_confirm(void) { return np_cvns_reenable_confirm(); }
np_hub_status_t np_hub_set_active_user(uint32_t user_tag)
{
    np_sim_emit("{\"ev\":\"fw_log\",\"t\":%u,\"fn\":\"np_hub_set_active_user\"}", (unsigned)np_sim_now_ms());
    (void)user_tag;
    return NP_HUB_OK;
}
uint32_t np_log_session_count(void) { return 1U; }
/* The owner's cardiac cutoff sensitivity (np_cardiac_sens.c, REQ-FWHUB-74) needs the Config store and the GATT server, which are not linked; level 0 is the cutoff as validated. */
uint8_t np_cardiac_sens_level(void) { return 0U; }
void np_transport_hal_enter_critical(void) { }
void np_transport_hal_exit_critical(void)  { }

/* The SHDR accessory-authentication record the intranasal driver writes at init: reported as it is logged. */
void np_log_shdr_zone_auth(uint8_t slot, np_hub_mod_type_t type, bool pass)
{
    np_sim_emit("{\"ev\":\"fw_log\",\"t\":%u,\"fn\":\"np_log_shdr_zone_auth\",\"slot\":%u,\"mod_type\":%d,\"pass\":%s}",
                (unsigned)np_sim_now_ms(), (unsigned)slot, (int)type, pass ? "true" : "false");
}

/* The runner is not in the loop.  np_mod_visual.c's halt_for() (the photoparoxysmal detector's halt) asks it to hold the
 * session PAUSED with the detector's cause; the simulator reports the request and does nothing else, so a halt shows as
 * what the driver did (LEDs off, enable dropped, further commands refused) plus the hold it asked for.  A double of
 * first-party code, listed as a gap.  (np_runner_abort, which the driver called before the detector commits, is no
 * longer reached from anything this layer links.) */
np_hub_status_t np_runner_halt(np_hold_cause_t cause)
{
    np_sim_emit("{\"ev\":\"fw_log\",\"t\":%u,\"fn\":\"np_runner_halt\",\"cause\":%d}",
                (unsigned)np_sim_now_ms(), (int)cause);
    return NP_HUB_ERR_NO_SESSION;
}

/* The panic control is the runner's too, and the runner is not in the loop: reported, not performed.  A double of
 * first-party code, listed as a gap. */
np_hub_status_t np_runner_panic(void)
{
    np_sim_emit("{\"ev\":\"fw_log\",\"t\":%u,\"fn\":\"np_runner_panic\"}", (unsigned)np_sim_now_ms());
    return NP_HUB_ERR_NO_SESSION;
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
 * The simulator mimics the device: np_pbm_power_admit() is the production governor
 * (np_pbm_power_gov.c, linked under another name only so this wrapper can sit in
 * front of it), and it refuses EVERY load until OI-FWHUB-09 replaces it.  So a PBM
 * drive over the socket, the bridge or the page is refused with NP_HUB_ERR_POWER_BUDGET,
 * exactly as the helmet refuses it (NP-SIM-FW-001 OI-SIMFW-11, decided).
 *
 * The one departure is a C-only seam, np_sim_governor_open_for_test(), that the host
 * test binaries call so the driver, rollback and fault paths behind the governor stay
 * coverable.  It is not a text command, so the bridge cannot reach it, and opening it
 * is announced once in the event stream (sim_policy). */
extern bool np_pbm_power_admit_production(const np_session_cmd_t *cmd, const np_sock_disp_socket_t *current);

static bool s_governor_open_for_test;

void np_sim_governor_open_for_test(bool open)
{
    s_governor_open_for_test = open;
    if (open) {
        np_sim_emit("{\"ev\":\"sim_policy\",\"what\":\"power governor opened by a host test; "
                    "the production governor refuses every load (OI-FWHUB-09)\"}");
    }
}

bool np_pbm_power_admit(const np_session_cmd_t *cmd, const np_sock_disp_socket_t *current)
{
    if (s_governor_open_for_test) { return true; }
    return np_pbm_power_admit_production(cmd, current);
}

/* ── critical sections ────────────────────────────────────────────────────────────
 * np_safety_spi.c brackets its shared state with taskENTER_CRITICAL.  The spike
 * is single-threaded, so these are no-ops; with the real scheduler they are the
 * POSIX port's. */
void vPortEnterCritical(void) { }
void vPortExitCritical(void)  { }
UBaseType_t xPortSetInterruptMask(void)                { return 0; }
void        vPortClearInterruptMask(UBaseType_t mask)  { (void)mask; }
