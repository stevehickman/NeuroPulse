/*
 * What the device's own logger was asked to log, reported as it is logged (NP-SIM-FW-001 §31, REQ-SIMFW-04).
 *
 * Before the hub's logger was linked, the sim layer defined these five calls as reporters.  Now np_session_log.c is the device's own and
 * runs, and the linker's --wrap (CMakeLists.txt) sends each caller here first: the event the scenario suites count is emitted, then the
 * device's function is called with the same arguments.  Nothing is answered here and nothing in firmware/ is edited (REQ-SIMFW-03).
 */
#include "np_sim_bus.h"

#include "np_hub_types.h"
#include "np_log_hrv.h"
#include "np_pbm_socket_telem.h"
#include "np_session_log.h"

void __real_np_log_pbm_socket(const np_pst_socket_record_t *rec, uint32_t session_ms);
void __real_np_log_shdr_pbm_socket_health(const np_pst_health_record_t *rec);
void __real_np_log_shdr_fault(uint8_t slot, np_hub_mod_type_t type, uint8_t fault_code, uint32_t session_ms);
void __real_np_log_hrv_session(const np_hrv_session_record_t *rec, np_hrv_status_t reason);
void __real_np_log_shdr_zone_auth(uint8_t slot, np_hub_mod_type_t type, bool pass);

void __wrap_np_log_pbm_socket(const np_pst_socket_record_t *rec, uint32_t session_ms)
{
    np_sim_emit("{\"ev\":\"fw_log\",\"t\":%u,\"fn\":\"np_log_pbm_socket\",\"session_ms\":%u}", (unsigned)np_sim_now_ms(), (unsigned)session_ms);
    __real_np_log_pbm_socket(rec, session_ms);
}

void __wrap_np_log_shdr_pbm_socket_health(const np_pst_health_record_t *rec)
{
    np_sim_emit("{\"ev\":\"fw_log\",\"t\":%u,\"fn\":\"np_log_shdr_pbm_socket_health\"}", (unsigned)np_sim_now_ms());
    __real_np_log_shdr_pbm_socket_health(rec);
}

void __wrap_np_log_shdr_fault(uint8_t slot, np_hub_mod_type_t type, uint8_t fault_code, uint32_t session_ms)
{
    np_sim_emit("{\"ev\":\"fw_log\",\"t\":%u,\"fn\":\"np_log_shdr_fault\",\"slot\":%u,\"mod_type\":%d,\"fault_code\":%u}",
                (unsigned)np_sim_now_ms(), (unsigned)slot, (int)type, (unsigned)fault_code);
    __real_np_log_shdr_fault(slot, type, fault_code, session_ms);
}

void __wrap_np_log_hrv_session(const np_hrv_session_record_t *rec, np_hrv_status_t reason)
{
    np_sim_emit("{\"ev\":\"fw_log\",\"t\":%u,\"fn\":\"np_log_hrv_session\",\"has_record\":%s,\"reason\":%d}",
                (unsigned)np_sim_now_ms(), rec != NULL ? "true" : "false", (int)reason);
    __real_np_log_hrv_session(rec, reason);
}

void __wrap_np_log_shdr_zone_auth(uint8_t slot, np_hub_mod_type_t type, bool pass)
{
    np_sim_emit("{\"ev\":\"fw_log\",\"t\":%u,\"fn\":\"np_log_shdr_zone_auth\",\"slot\":%u,\"mod_type\":%d,\"pass\":%s}",
                (unsigned)np_sim_now_ms(), (unsigned)slot, (int)type, pass ? "true" : "false");
    __real_np_log_shdr_zone_auth(slot, type, pass);
}
