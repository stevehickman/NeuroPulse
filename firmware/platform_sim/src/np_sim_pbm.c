/*
 * NeurOne Firmware-in-the-Loop Simulator — PBM socket seams (NP-SIM-FW-001 Rev 2 §3.2)
 * HOST ONLY; see np_sim_bus.h.
 *
 * Defines the socket-indexed PBM seams of np_sw02_platform_hal.h for T1-A
 * (base) tiles.  Each resolves its socket to a sim module; the module answers
 * from an injected response if one is armed, otherwise from the default model,
 * and the answer is reported with its source (REQ-SIMFW-04).
 *
 * Not defined here, and so not yet routed: the slot-indexed PBM seams (the
 * retired zone-slot path), the smart-tile register tunnel (np_pbm_hal_i2c_*)
 * and np_mod_pbm_hal_socket_selftest_pd1().  They are in np_sim_unrouted.c and
 * report themselves when reached.
 */
#include "np_sim_bus.h"

#include "np_sw02_platform_hal.h"

#include <stdio.h>

static void report(const np_sim_module_t *m, np_sim_op_t op, const char *args,
                   int code, const char *source)
{
    np_sim_emit("{\"ev\":\"hal\",\"t\":%u,\"id\":\"%s:%u\",\"op\":\"%s\",\"args\":%s,"
                "\"code\":%d,\"source\":\"%s\"}",
                (unsigned)np_sim_now_ms(), np_sim_kind_name(m->kind), (unsigned)m->index,
                np_sim_op_name(op), args, code, source);
}

/* Resolve a socket to a present module, reporting a miss.  A module the
 * firmware addressed but the scenario never added is the firmware talking to
 * an empty socket: NOT_PRESENT, reported. */
static np_sim_module_t *resolve(uint16_t socket_id, const char *op_name)
{
    np_sim_module_t *m = np_sim_find(socket_id);
    if (m == NULL || !m->present) {
        np_sim_emit("{\"ev\":\"hal\",\"t\":%u,\"id\":\"socket:%u\",\"op\":\"%s\","
                    "\"code\":%d,\"source\":\"absent\"}",
                    (unsigned)np_sim_now_ms(), (unsigned)socket_id, op_name,
                    (int)NP_HUB_ERR_NOT_PRESENT);
        return NULL;
    }
    return m;
}

np_hub_status_t np_mod_pbm_hal_socket_pwm_set(uint16_t socket_id,
                                              uint8_t cur_a, uint8_t cur_b,
                                              uint8_t freq_code, uint8_t duty)
{
    np_sim_module_t *m = resolve(socket_id, "socket_pwm_set");
    if (m == NULL) { return NP_HUB_ERR_NOT_PRESENT; }

    char args[96];
    (void)snprintf(args, sizeof args, "[%u,%u,%u,%u]", cur_a, cur_b, freq_code, duty);

    int code;
    if (np_sim_take_response(m, NP_SIM_OP_SOCKET_PWM_SET, &code)) {
        /* A failed write leaves the emitter as it was: the module's commanded
         * state is not updated. */
        report(m, NP_SIM_OP_SOCKET_PWM_SET, args, code, "injected");
        return (np_hub_status_t)code;
    }
    m->cur_a = cur_a; m->cur_b = cur_b; m->freq_code = freq_code; m->duty = duty;
    m->driven = (cur_a != 0U || cur_b != 0U);
    report(m, NP_SIM_OP_SOCKET_PWM_SET, args, (int)NP_HUB_OK, "default");
    return NP_HUB_OK;
}

np_hub_status_t np_mod_pbm_hal_socket_ntc_read(uint16_t socket_id, float *temp_c_out)
{
    np_sim_module_t *m = resolve(socket_id, "socket_ntc_read");
    if (m == NULL) { return NP_HUB_ERR_NOT_PRESENT; }

    int code;
    if (np_sim_take_response(m, NP_SIM_OP_SOCKET_NTC_READ, &code)) {
        report(m, NP_SIM_OP_SOCKET_NTC_READ, "[]", code, "injected");
        return (np_hub_status_t)code;
    }
    const np_sim_override_t *o = &m->ov[NP_SIM_OP_SOCKET_NTC_READ];
    const bool pinned = o->has_reading;
    *temp_c_out = pinned ? o->reading : m->ntc_c;

    char args[48];
    (void)snprintf(args, sizeof args, "[%.1f]", (double)*temp_c_out);
    report(m, NP_SIM_OP_SOCKET_NTC_READ, args, (int)NP_HUB_OK, pinned ? "injected" : "default");
    return NP_HUB_OK;
}
