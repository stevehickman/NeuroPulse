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
#include "np_sim_pbm_harness.h"   /* np_sim_proto_pubkey */

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
#include "np_cvns_fault_summary.h"

#include "FreeRTOS.h"

#include <string.h>

/* ── log sinks ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────
 * The device's own logger (np_session_log.c) is linked (OI-SIMFW-45); the fw_log events the scenario suites read for np_log_shdr_fault,
 * np_log_shdr_zone_auth, np_log_hrv_session, np_log_pbm_socket and np_log_shdr_pbm_socket_health come from np_sim_log_wrap.c, which
 * emits the event and then calls the device's function. */

/* ── cervical VNS fillers ────────────────────────────────────────────────────────
 * The fault summary's persisted blob is RAM (a real device writes the UHDR partition); notifying a central is reported.
 * np_hub_cvns_reenable_confirm, np_hub_set_active_user and the session counter are the device's own now (np_hub_control_main.c,
 * np_session_log.c). */
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
/* np_transport_hal_enter/exit_critical: np_transport.c's own host model, built with NPTEST_HOST (CMakeLists.txt). */

/* The runner is linked (np_session_runner.c, OI-SIMFW-43), so np_runner_halt, np_runner_panic and the session lease are the device's own.
 * Without a loaded session np_runner_halt answers NP_HUB_ERR_NO_SESSION, as it did here. */

/* np_pbm_session.c (the smart-module session expander) is not linked (OI-SIMFW-53), so the two hooks np_hub_pbm_cal_bridge_install() gives it are taken
 * and dropped: the hub installs them at boot, and nothing in the twin asks the validator or the resolver a question. */
void np_pbm_session_set_uid_resolver(np_pbm_socket_uid_fn fn, void *ctx) { (void)fn; (void)ctx; }
void np_pbm_session_set_address_validator(np_pbm_socket_valid_fn fn, void *ctx) { (void)fn; (void)ctx; }

np_hub_status_t np_hexmap_nvram_read(uint8_t *buf, size_t len, size_t *read_len)
{ (void)buf; (void)len; if (read_len != NULL) { *read_len = 0; } return NP_HUB_ERR_NOT_PRESENT; }
np_hub_status_t np_hexmap_nvram_write(const uint8_t *buf, size_t len)
{ (void)buf; (void)len; return NP_HUB_OK; }

/* ── protocol identity: not exercised by a socket command ────────────────────── */

np_hub_status_t np_proto_hal_get_device_serial(uint8_t *buf, size_t len)
{ return np_sim_proto_serial(buf, len); }   /* the TEST device serial, np_sim_proto.c: the replay guard of np_protocol_verify_and_parse is live */
np_hub_status_t np_proto_hal_get_proto_pubkey(uint8_t *pub_key_out)
{ np_sim_proto_pubkey(pub_key_out); return NP_HUB_OK; }   /* the TEST protocol signing key's public half, np_sim_proto.c */

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
