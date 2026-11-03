/*
 * NeurOne Firmware-in-the-Loop Simulator — T2 and accessory stubs (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 * HOST ONLY.  np_mod_t2_stubs.c has no seam: every function is a placeholder that refuses.  The simulator equivalent of a
 * module that refuses is to call it, and to say what it did and what it requested of the safety MCU, which is nothing.
 *
 *   stub <name> detect|init|control|telemetry|shutdown
 *
 * names: qeeg  tms  pbm_1170nm  clin_tacs  hd_tdcs  vibrotactile.   The reply is a `stub` event carrying the code and the
 * enables requested, so a T2 line that ever asked for one shows.
 */
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"

#include "np_hub_types.h"
#include "np_hub_config.h"
#include "np_safety_spi.h"

#include <string.h>

#define STUB_DECLS(n)                                                                               \
    extern np_hub_status_t np_mod_##n##_detect(uint8_t slot, np_hub_mod_type_t *type_out);          \
    extern np_hub_status_t np_mod_##n##_init(uint8_t slot);                                         \
    extern np_hub_status_t np_mod_##n##_control(uint8_t slot, const void *params, uint16_t len);    \
    extern np_hub_status_t np_mod_##n##_telemetry(uint8_t slot, np_telem_record_t *out);            \
    extern np_hub_status_t np_mod_##n##_shutdown(uint8_t slot);
STUB_DECLS(qeeg) STUB_DECLS(tms) STUB_DECLS(pbm_1170nm) STUB_DECLS(clin_tacs) STUB_DECLS(hd_tdcs) STUB_DECLS(vibrotactile)

typedef struct {
    const char *name;
    uint8_t     slot;
    np_hub_status_t (*detect)(uint8_t, np_hub_mod_type_t *);
    np_hub_status_t (*init)(uint8_t);
    np_hub_status_t (*control)(uint8_t, const void *, uint16_t);
    np_hub_status_t (*telemetry)(uint8_t, np_telem_record_t *);
    np_hub_status_t (*shutdown)(uint8_t);
} stub_t;

#define STUB_ROW(n, slot_id) { #n, slot_id, np_mod_##n##_detect, np_mod_##n##_init, np_mod_##n##_control, np_mod_##n##_telemetry, np_mod_##n##_shutdown }
static const stub_t K_STUBS[] = {
    STUB_ROW(qeeg,        NP_HUB_SLOT_QEEG),
    STUB_ROW(tms,         NP_HUB_SLOT_TMS),
    STUB_ROW(pbm_1170nm,  NP_HUB_SLOT_PBM_1170NM),
    STUB_ROW(clin_tacs,   NP_HUB_SLOT_CLIN_TACS),
    STUB_ROW(hd_tdcs,     NP_HUB_SLOT_HD_TDCS),
    STUB_ROW(vibrotactile, NP_HUB_SLOT_VIBROTACTILE),
};

bool np_simh_stubs_line(const char *cmd, char *a, char *b)
{
    if (strcmp(cmd, "stub") != 0 || a == NULL || b == NULL) { return false; }
    for (unsigned i = 0; i < sizeof K_STUBS / sizeof K_STUBS[0]; i++) {
        const stub_t *s = &K_STUBS[i];
        if (strcmp(a, s->name) != 0) { continue; }
        np_hub_status_t rc;
        np_hub_mod_type_t t = NP_MOD_NONE;
        np_telem_record_t rec;
        uint8_t params[8] = {0};
        memset(&rec, 0, sizeof rec);
        if (strcmp(b, "detect") == 0)         { rc = s->detect(s->slot, &t); }
        else if (strcmp(b, "init") == 0)      { rc = s->init(s->slot); }
        else if (strcmp(b, "control") == 0)   { rc = s->control(s->slot, params, (uint16_t)sizeof params); }
        else if (strcmp(b, "telemetry") == 0) { rc = s->telemetry(s->slot, &rec); }
        else if (strcmp(b, "shutdown") == 0)  { rc = s->shutdown(s->slot); }
        else { return false; }
        np_sim_emit("{\"ev\":\"stub\",\"name\":\"%s\",\"op\":\"%s\",\"code\":%d,\"mod_type\":%d,\"requested_enables\":%u}",
                    s->name, b, (int)rc, (int)t, (unsigned)np_safety_spi_get_requested_mask());
        return true;
    }
    return false;
}
