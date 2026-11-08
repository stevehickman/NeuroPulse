/*
 * NeurOne Hub Control — np_runner_load() Host Tests
 *
 * The runner's load step is where a protocol is admitted or refused, and the order in which it refuses is part of the
 * safety argument: a refusal must leave NOTHING behind (no lease held, no descriptor installed, no event posted) and must
 * ask nothing of the safety MCU.  Until this suite nothing exercised that order; the admit functions were tested alone and
 * the order was "by inspection only" (NP-SW-001 SWR-HV-08 status, OI-UI-KMP-10).
 *
 * Linked REAL: np_session_runner.c (the unit under test), np_session_lease.c (the lease claim and release),
 * np_protocol.c (verify_and_parse, tier_admit, hw_version_admit), np_session_pause.c, np_ramp_plan.c, np_chan_decl.c.
 * Doubles: the Ed25519 verifier and serial HAL (as np_protocol_tests.c does: a real signature would have to be recomputed
 * per fixture, which tests the fixture builder), the safety MCU (what it reports, and a count of what it is ASKED),
 * the boot-admission flag, the event group, the FreeRTOS kernel calls, and every module/logging seam the run loop uses
 * (np_runner_load() reaches none of them).  What the descriptor holds is read through np_runner_progress(), the runner's
 * public snapshot, so no test reaches into the runner's static context.
 *
 * No hardware.  IEC 62304 Class B — SW-02 hub control.
 * Build with -DNP_BUILD_TESTS=ON (see firmware/hub_control/CMakeLists.txt).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "np_chan_decl.h"
#include "np_consumables.h"
#include "np_cvns_reenable.h"
#include "np_module_registry.h"
#include "np_pause_limit.h"
#include "np_pbm_socket_telem.h"
#include "np_socket_dispatch.h"
#include "np_hub_types.h"
#include "np_log_backend.h"
#include "np_protocol.h"
#include "np_ramp_plan.h"
#include "np_safety_spi.h"
#include "np_session_fitting.h"
#include "np_session_lease.h"
#include "np_session_log.h"
#include "np_session_progress.h"
#include "np_session_runner.h"
#include "np_stim_xcheck.h"
#include "np_sw02_platform_hal.h"

static int g_failures = 0;

static void check(int cond, const char *name)
{
    if (cond) {
        printf("PASS: %s\n", name);
    } else {
        printf("FAIL: %s\n", name);
        g_failures++;
    }
}

/* ── What the doubles report, and what they were asked ───────────────────────── */

static bool    g_admits_sessions;   /* np_log_backend_admits_sessions(): the boot's factory-reset check */
static uint8_t g_tier;              /* the unit's tier as the safety MCU established it */
static uint8_t g_hw_version;        /* the helmet hardware version from the MCU's verified record */
static bool    g_report_received;   /* the MCU has sent a valid tier/version report */
static np_hub_status_t g_layout_rc; /* np_layout_admit(): is a layout bound for this helmet's version (REQ-FWHUB-64) */
static uint8_t g_layout_arg;        /* the version np_runner_load() asked it about */
static np_hub_status_t g_addr_rc;   /* np_layout_check_protocol_addresses(): does the protocol name only valid addresses */
static int     g_addr_calls;

/* Everything np_runner_load() could ASK of the safety MCU or the rest of the hub.  A refusal must leave all of these
 * at zero: "before anything is requested of the safety MCU" is what the code comments promise. */
static unsigned g_spi_asks;
static unsigned g_event_sets;
static EventBits_t g_event_bits;
static unsigned g_sock_resets;
static unsigned g_pst_resets;

static void reset_doubles(void)
{
    g_admits_sessions = true;
    g_tier            = NP_TIER_T1;
    g_hw_version      = 1U;
    g_report_received = true;
    g_layout_rc       = NP_HUB_OK;
    g_layout_arg      = 0U;
    g_addr_rc         = NP_HUB_OK;
    g_addr_calls      = 0;
    g_spi_asks = g_event_sets = g_sock_resets = g_pst_resets = 0U;
    g_event_bits = 0U;
}

/* ── The Ed25519 verifier and the serial HAL (np_protocol.c's seams) ─────────── */

np_hub_status_t np_proto_hal_get_device_serial(uint8_t *buf, size_t len)
{
    (void)buf; (void)len;
    return NP_HUB_ERR_NOT_PRESENT;      /* the replay guard compares only when a serial reads back */
}

np_hub_status_t np_proto_hal_get_proto_pubkey(uint8_t *pub_key_out)
{
    memset(pub_key_out, 0, 32);
    return NP_HUB_OK;
}

static int g_verify_result;             /* 0 accepts, as np_ed25519_verify does */

int np_ed25519_verify(const uint8_t *msg, size_t msg_len, const uint8_t *sig, const uint8_t *pub_key)
{
    (void)msg; (void)msg_len; (void)sig; (void)pub_key;
    return g_verify_result;
}

/* ── The safety MCU: what it reports ─────────────────────────────────────────── */

uint8_t np_safety_spi_get_tier(void)             { return g_tier; }
uint8_t np_safety_spi_get_hardware_version(void) { return g_hw_version; }

bool np_safety_spi_get_tier_report(uint8_t *reason_out, bool *refused_out)
{
    if (reason_out != NULL)  { *reason_out = 0U; }
    if (refused_out != NULL) { *refused_out = false; }
    return g_report_received;
}

/* ... and what it is asked.  Each is a request or a command, not a read. */
void np_safety_spi_request_enable(uint16_t channel_mask)          { (void)channel_mask; g_spi_asks++; }
void np_safety_spi_request_disable(uint16_t channel_mask)         { (void)channel_mask; g_spi_asks++; }
void np_safety_spi_disable_all(void)                              { g_spi_asks++; }
void np_safety_spi_set_geom_required(bool v)                      { (void)v; g_spi_asks++; }
void np_safety_spi_set_geom_required_tdcs(bool v)                 { (void)v; g_spi_asks++; }
void np_safety_spi_set_geom_required_bes(bool v)                  { (void)v; g_spi_asks++; }

/* ── The boot admission flag ─────────────────────────────────────────────────── */

bool np_log_backend_admits_sessions(void) { return g_admits_sessions; }
np_hub_status_t np_layout_admit(uint8_t device_hw_version) { g_layout_arg = device_hw_version; return g_layout_rc; }
np_hub_status_t np_layout_check_protocol_addresses(const np_session_desc_t *desc) { (void)desc; g_addr_calls++; return g_addr_rc; }

/* ── The kernel ──────────────────────────────────────────────────────────────── */

void np_freertos_assert_failed(const char *file, unsigned long line)
{
    printf("FAIL: configASSERT at %s:%lu\n", file, line);
    g_failures++;
}

static int g_mutex_token;               /* the lease's lock: held-state is the lease's own, not the kernel's */

QueueHandle_t xQueueCreateMutex(const uint8_t type)           { (void)type; return (QueueHandle_t)&g_mutex_token; }
BaseType_t xQueueSemaphoreTake(QueueHandle_t q, TickType_t t) { (void)q; (void)t; return pdTRUE; }
BaseType_t xQueueGenericSend(QueueHandle_t q, const void *item, TickType_t t, const BaseType_t pos)
{
    (void)q; (void)item; (void)t; (void)pos;
    return pdTRUE;
}

EventBits_t xEventGroupSetBits(EventGroupHandle_t g, const EventBits_t bits)
{
    (void)g;
    g_event_sets++;
    g_event_bits |= bits;
    return g_event_bits;
}

EventBits_t xEventGroupClearBits(EventGroupHandle_t g, const EventBits_t bits)
{
    (void)g;
    g_event_bits &= ~bits;
    return g_event_bits;
}

void       vPortEnterCritical(void) {}
void       vPortExitCritical(void)  {}
void       vTaskDelay(const TickType_t t) { (void)t; }
EventBits_t xEventGroupWaitBits(EventGroupHandle_t g, const EventBits_t bits, const BaseType_t clear,
                                const BaseType_t all, const TickType_t t)
{
    (void)g; (void)bits; (void)clear; (void)all; (void)t;   /* the run loop's sleep: the load never reaches it */
    return 0U;
}
TickType_t xTaskGetTickCount(void)  { return 0U; }

/* ── What np_runner_load() resets, counted ───────────────────────────────────── */

void np_sock_disp_reset(void)   { g_sock_resets++; }
void np_pst_session_reset(void) { g_pst_resets++; }

/* ── np_runner_progress(): the snapshot of the descriptor the runner holds ──── */

static np_progress_in_t g_last_progress;

void np_progress_build(const np_progress_in_t *in, uint8_t out[NP_PROGRESS_LEN])
{
    g_last_progress = *in;
    memset(out, 0, NP_PROGRESS_LEN);
}

/* ── Seams the run loop uses; np_runner_load() reaches none of them ──────────── */
/* Each is defined only so the runner links, and counts a call, because a call from np_runner_load() would be a change in
 * what it does that this suite should notice. */

static unsigned g_unexpected;

#define UNEXPECTED() (g_unexpected++)

void   np_cons_on_session_end(uint32_t mods_active_mask)            { (void)mods_active_mask; UNEXPECTED(); }
size_t np_fitting_build(uint8_t hw, uint8_t *out, size_t cap)       { (void)hw; (void)out; (void)cap; UNEXPECTED(); return 0U; }
void   np_log_command(const np_session_cmd_t *cmd, uint32_t session_ms, bool accepted)
                                                                    { (void)cmd; (void)session_ms; (void)accepted; UNEXPECTED(); }
void   np_log_flush(void)                                           { UNEXPECTED(); }
void   np_log_session_end(const np_session_uhdr_record_t *u, const np_session_shdr_record_t *s)
                                                                    { (void)u; (void)s; UNEXPECTED(); }
void   np_log_session_fitting(const uint8_t *frame, size_t len)     { (void)frame; (void)len; UNEXPECTED(); }
void   np_log_session_pause(uint32_t session_ms, bool paused, uint16_t unrestored)
                                                                    { (void)session_ms; (void)paused; (void)unrestored; UNEXPECTED(); }
void   np_log_session_start(const np_session_uhdr_record_t *rec)    { (void)rec; UNEXPECTED(); }
void   np_log_shdr_fault(uint8_t slot, np_hub_mod_type_t type, uint8_t fault_code, uint32_t session_ms)
                                                                    { (void)slot; (void)type; (void)fault_code; (void)session_ms; UNEXPECTED(); }
void   np_log_shdr_pbm_session_counts(const np_pst_counts_t *counts) { (void)counts; UNEXPECTED(); }
void   np_log_telemetry(const np_telem_record_t *rec)               { (void)rec; UNEXPECTED(); }
void   np_progress_notify(void)                                     { UNEXPECTED(); }
void   np_hold_notify(void)                                         { UNEXPECTED(); }
void   np_cvns_reenable_session_reset(void)                         { UNEXPECTED(); }
void   np_stim_xcheck_reset(void)                                   { UNEXPECTED(); }
bool   np_stim_xcheck_observe(const np_telem_record_t *rec, uint32_t now_ms) { (void)rec; (void)now_ms; UNEXPECTED(); return false; }
uint32_t np_pause_limit_seconds(void)                               { UNEXPECTED(); return 0U; }
np_mod_entry_t *np_mod_reg_get(uint8_t slot)                        { (void)slot; UNEXPECTED(); return NULL; }
np_hub_status_t np_sock_disp_command(const np_session_cmd_t *cmd)   { (void)cmd; UNEXPECTED(); return NP_HUB_OK; }
void     np_sock_disp_process_stops(uint32_t now_ms)                { (void)now_ms; UNEXPECTED(); }
uint32_t np_sock_disp_next_stop_ms(uint32_t now_ms)                 { (void)now_ms; UNEXPECTED(); return 0U; }
void     np_sock_disp_stop_all(void)                                { UNEXPECTED(); }
void     np_sock_disp_pause(void)                                   { UNEXPECTED(); }
uint16_t np_sock_disp_resume(void)                                  { UNEXPECTED(); return 0U; }
void np_pst_dose_poll(uint32_t now_ms)                              { (void)now_ms; UNEXPECTED(); }
void np_pst_log_telemetry(uint32_t session_ms)                      { (void)session_ms; UNEXPECTED(); }
void np_pst_get_counts(np_pst_counts_t *out)                        { (void)out; UNEXPECTED(); }
np_hub_status_t np_safety_spi_send_channel_limits(const uint16_t *area_mcm2, uint8_t count)
                                                                    { (void)area_mcm2; (void)count; g_spi_asks++; return NP_HUB_OK; }
bool     np_mod_cvns_active(void)                                   { UNEXPECTED(); return false; }
void     np_mod_cvns_tick(uint32_t now_ms, uint32_t now_s)          { (void)now_ms; (void)now_s; UNEXPECTED(); }
void     np_mod_cvns_suspend(void)                                  { UNEXPECTED(); }
void     np_mod_cvns_ramp(uint32_t now_ms, np_ramp_report_t *out)   { (void)now_ms; (void)out; UNEXPECTED(); }
uint32_t np_mod_cvns_ramp_up_ms(void)                               { UNEXPECTED(); return 0U; }
uint32_t np_mod_cvns_hal_now_ms(void)                               { UNEXPECTED(); return 0U; }
uint32_t np_mod_cvns_hal_now_unix(void)                             { UNEXPECTED(); return 0U; }
void     np_mod_stim_tick(uint32_t wall_ms)                         { (void)wall_ms; UNEXPECTED(); }
bool     np_mod_stim_tdcs_pause(void)                               { UNEXPECTED(); return false; }
void     np_mod_stim_tdcs_ramp(np_ramp_report_t *out)               { (void)out; UNEXPECTED(); }
uint32_t np_mod_stim_tdcs_ramp_up_ms(const void *params, uint16_t len) { (void)params; (void)len; UNEXPECTED(); return 0U; }
np_hub_status_t np_safety_spi_send_channel_waveforms(const uint8_t *wave_class, const uint32_t *phase_us, uint8_t count)
                                                                    { (void)wave_class; (void)phase_us; (void)count; g_spi_asks++; return NP_HUB_OK; }

/* ── Blob builder ─────────────────────────────────────────────────────────────
 * The same construction np_protocol_tests.c uses, at explicit byte offsets (not through the packed structs), so a change
 * to the layout is caught here and not mirrored. */

#define RUNNER_LOAD_BLOB_CAP 512

typedef struct {
    uint8_t buf[RUNNER_LOAD_BLOB_CAP];
    size_t  len;
} blob_t;

/* One command on a slot target, `mod_type` on `slot_id`.  `params` is a body the parser accepts for the module. */
static void blob_make(blob_t *b, uint8_t hw_version, uint32_t duration_ms, uint8_t mod_type, uint8_t slot_id,
                      const uint8_t *params, uint16_t params_len)
{
    memset(b, 0, sizeof *b);
    const uint32_t magic = NP_HUB_PROTO_MAGIC;
    const uint16_t version = NP_HUB_PROTO_VERSION;
    const uint32_t compiled_at = 1700000000UL;
    memcpy(b->buf + 0, &magic, 4);
    memcpy(b->buf + 4, &version, 2);
    b->buf[7] = 1U;                              /* cmd_count */
    memcpy(b->buf + 24, &compiled_at, 4);
    memcpy(b->buf + 60, &duration_ms, 4);
    b->buf[64] = hw_version;                     /* 65..67 reserved, zero */
    b->len = 68U;

    uint8_t *p = b->buf + b->len;
    const uint32_t start_ms = 0U;
    p[0] = mod_type;
    p[1] = slot_id;
    memcpy(p + 2, &start_ms, 4);
    memcpy(p + 6, &duration_ms, 4);
    memcpy(p + 10, &params_len, 2);
    p[12] = (uint8_t)NP_PROTO_TARGET_SLOT;
    p[13] = 0U;                                  /* target_len */
    b->len += 14U;
    memcpy(b->buf + b->len, params, params_len);
    b->len += params_len;
    memset(b->buf + b->len, 0, NP_HUB_PROTO_SIG_LEN);   /* the signature: the verifier double decides */
    b->len += NP_HUB_PROTO_SIG_LEN;
}

static const uint8_t EEG_PARAMS[5] = { 0xFF, 6, 2, 0, 3 };

/* A T1 protocol (EEG) for `hw_version`, `duration_ms` long. */
static void blob_t1(blob_t *b, uint8_t hw_version, uint32_t duration_ms)
{
    blob_make(b, hw_version, duration_ms, NP_MOD_EEG, NP_HUB_SLOT_EEG, EEG_PARAMS, sizeof EEG_PARAMS);
}

/* A protocol that uses a T2 modality (cervical VNS). */
static void blob_t2(blob_t *b, uint8_t hw_version, uint32_t duration_ms)
{
    blob_make(b, hw_version, duration_ms, NP_MOD_CVNS, NP_HUB_SLOT_CVNS, EEG_PARAMS, sizeof EEG_PARAMS);
}

/* ── Harness ──────────────────────────────────────────────────────────────────── */

static int g_events_token;

static void fresh(void)
{
    reset_doubles();
    g_verify_result = 0;
    np_runner_init((EventGroupHandle_t)&g_events_token);   /* a loaded session and a held lease are gone */
}

/* The duration of the descriptor the runner holds, through its public snapshot; 0 and no commands if none. */
static uint32_t held_duration_ms(uint8_t *cmds_out)
{
    uint8_t frame[NP_PROGRESS_LEN];
    np_runner_progress(frame);
    if (cmds_out != NULL) { *cmds_out = g_last_progress.cmd_count; }
    return g_last_progress.total_ms;
}

/* A refusal leaves nothing behind: no lease, no descriptor, no state, no event, nothing asked of anyone. */
static void expect_nothing_left_behind(const char *name)
{
    char msg[160];
    uint8_t cmds = 0xAA;
    snprintf(msg, sizeof msg, "%s: the lease is released", name);
    check(!np_lease_held(), msg);
    snprintf(msg, sizeof msg, "%s: no descriptor is installed", name);
    check(held_duration_ms(&cmds) == 0U && cmds == 0U, msg);
    snprintf(msg, sizeof msg, "%s: the runner is idle", name);
    check(np_runner_get_state() == NP_SESSION_IDLE, msg);
    snprintf(msg, sizeof msg, "%s: no session-start event was posted", name);
    check(g_event_sets == 0U && (g_event_bits & NP_EV_SESSION_START) == 0U, msg);
    snprintf(msg, sizeof msg, "%s: nothing was asked of the safety MCU", name);
    check(g_spi_asks == 0U, msg);
}

/* ── Tests ────────────────────────────────────────────────────────────────────── */

/* @verifies REQ-FWHUB-03 */
static void test_a_good_protocol_loads_and_holds_the_lease(void)
{
    fresh();
    blob_t b;
    blob_t1(&b, 1U, 60000U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_OK, "good: a T1 protocol for this helmet loads");
    check(np_lease_held(), "good: the lease is held from the load until the run ends");
    uint8_t cmds = 0U;
    check(held_duration_ms(&cmds) == 60000U && cmds == 1U, "good: the descriptor is installed");
    check(g_event_sets == 1U && (g_event_bits & NP_EV_SESSION_START) != 0U, "good: session-start is posted once");
    check(g_spi_asks == 0U, "good: loading asks nothing of the safety MCU (the run does)");
    check(g_unexpected == 0U, "good: loading reaches no module, logger or dispatch seam of the run loop");
    check(g_sock_resets == 1U && g_pst_resets == 1U, "good: the socket dispatch and the dose model are reset once");
}

/* @verifies REQ-FWHUB-03 */
static void test_a_second_blob_cannot_replace_a_loaded_descriptor(void)
{
    fresh();
    blob_t a, b;
    blob_t1(&a, 1U, 60000U);
    blob_t1(&b, 1U, 1234U);
    check(np_runner_load(a.buf, a.len) == NP_HUB_OK, "second: the first loads");
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_SESSION_ACTIVE, "second: the next is refused while the lease is held");
    check(held_duration_ms(NULL) == 60000U, "second: the loaded descriptor is the first's, not replaced");
    check(np_lease_held(), "second: the refusal does not release the first's lease");
    check(g_event_sets == 1U, "second: no second session-start event");
}

/* @verifies REQ-FWHUB-03 */
static void test_a_lease_held_by_someone_else_refuses_before_any_state_changes(void)
{
    fresh();
    check(np_lease_claim() == NP_HUB_OK, "held: setup, a session (or a claim) holds the lease");
    blob_t b;
    blob_t1(&b, 1U, 60000U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_SESSION_ACTIVE, "held: the load is refused");
    check(np_lease_held(), "held: the other holder keeps the lease");
    check(g_sock_resets == 0U && g_pst_resets == 0U, "held: the socket dispatch and the dose model are not reset");
    check(held_duration_ms(NULL) == 0U && g_event_sets == 0U && g_spi_asks == 0U, "held: nothing installed, posted or asked");
}

/* @verifies REQ-FWHUB-03 */
static void test_an_empty_or_missing_buffer_is_refused_without_touching_the_lease(void)
{
    fresh();
    blob_t b;
    blob_t1(&b, 1U, 60000U);
    check(np_runner_load(NULL, b.len) == NP_HUB_ERR_INVALID_ARG, "args: a NULL buffer is refused");
    check(np_runner_load(b.buf, 0U) == NP_HUB_ERR_INVALID_ARG, "args: a zero length is refused");
    expect_nothing_left_behind("args");
    check(g_sock_resets == 0U && g_pst_resets == 0U, "args: nothing was reset");
}

/* @verifies REQ-FWHUB-03 */
static void test_a_boot_that_refused_the_records_runs_no_session(void)
{
    fresh();
    g_admits_sessions = false;
    blob_t b;
    blob_t1(&b, 1U, 60000U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_BOOT_REFUSED, "boot: refused with BOOT_REFUSED");
    expect_nothing_left_behind("boot");
    check(g_sock_resets == 0U && g_pst_resets == 0U, "boot: refused before the lease is claimed or anything reset");
    /* Refused FIRST: even with the lease already held the answer is the boot's, not SESSION_ACTIVE. */
    fresh();
    g_admits_sessions = false;
    check(np_lease_claim() == NP_HUB_OK, "boot: setup, the lease is held");
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_BOOT_REFUSED, "boot: the boot refusal comes before the lease check");
}

/* @verifies REQ-FWHUB-03 */
static void test_a_bad_signature_is_refused_and_releases_the_lease(void)
{
    fresh();
    g_verify_result = 1;                         /* the verifier rejects */
    blob_t b;
    blob_t1(&b, 1U, 60000U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_BAD_SIGNATURE, "sig: refused with BAD_SIGNATURE");
    expect_nothing_left_behind("sig");
    /* ... and the next, good, protocol loads: the refusal did not wedge the lease. */
    g_verify_result = 0;
    check(np_runner_load(b.buf, b.len) == NP_HUB_OK, "sig: the next good load is not blocked by the refusal");
}

/* @verifies REQ-FWHUB-03 */
static void test_a_malformed_blob_is_refused_and_releases_the_lease(void)
{
    fresh();
    blob_t b;
    blob_t1(&b, 1U, 60000U);
    b.buf[0] ^= 0xFFU;                           /* the magic */
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_BAD_MAGIC, "bad blob: refused with BAD_MAGIC");
    expect_nothing_left_behind("bad magic");
    check(np_runner_load(b.buf, 10U) != NP_HUB_OK, "bad blob: a buffer shorter than a header is refused");
    expect_nothing_left_behind("short");
}

/* @verifies REQ-UPG-01#c,d */
static void test_a_t2_protocol_on_a_t1_unit_is_refused_f4_and_leaves_nothing(void)
{
    fresh();
    g_tier = NP_TIER_T1;
    blob_t b;
    blob_t2(&b, 1U, 60000U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_TIER_F4, "tier: a T2 protocol on a T1 unit is refused F4");
    expect_nothing_left_behind("tier F4");
    /* The descriptor was installed to be judged and then CLEARED: the snapshot must not show the T2 command. */
    check(held_duration_ms(NULL) == 0U, "tier F4: the refused descriptor is cleared, not left installed");
}

/* @verifies REQ-UPG-01#c,d */
static void test_a_t2_protocol_before_the_tier_is_known_is_unverified_not_f4(void)
{
    fresh();
    g_tier = NP_TIER_UNKNOWN;
    blob_t b;
    blob_t2(&b, 1U, 60000U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_TIER_UNVERIFIED, "tier: before the first report it is UNVERIFIED");
    expect_nothing_left_behind("tier unverified");
}

/* @verifies REQ-UPG-01#c,d */
static void test_a_t2_protocol_on_a_t2_unit_loads(void)
{
    fresh();
    g_tier = NP_TIER_T2;
    blob_t b;
    blob_t2(&b, 1U, 60000U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_OK, "tier: a T2 protocol on a T2 unit loads");
    check(np_lease_held(), "tier: and holds the lease");
}

/* @verifies REQ-UPG-01#b */
static void test_a_t1_protocol_loads_on_a_unit_whose_tier_is_not_yet_known(void)
{
    fresh();
    g_tier = NP_TIER_UNKNOWN;
    blob_t b;
    blob_t1(&b, 1U, 60000U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_OK, "tier: a T1 protocol needs no tier, so it loads while the tier is unknown");
}

/* @verifies REQ-FWHUB-62 SWR-HV-08 */
static void test_a_protocol_for_another_hardware_version_is_refused_and_leaves_nothing(void)
{
    fresh();
    g_hw_version = 1U;
    blob_t b;
    blob_t1(&b, 2U, 60000U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_HW_VERSION_MISMATCH, "hwver: a stamp of 2 on a helmet of 1 is refused");
    expect_nothing_left_behind("hwver mismatch");
    check(held_duration_ms(NULL) == 0U, "hwver mismatch: the refused descriptor is cleared, not left installed");
    /* Both ways: a stamp of 1 on a helmet of 2. */
    fresh();
    g_hw_version = 2U;
    blob_t1(&b, 1U, 60000U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_HW_VERSION_MISMATCH, "hwver: a stamp of 1 on a helmet of 2 is refused too");
    expect_nothing_left_behind("hwver mismatch, other way");
}

/* @verifies REQ-FWHUB-62 SWR-HV-08 */
static void test_a_matching_hardware_version_loads(void)
{
    fresh();
    g_hw_version = 2U;
    blob_t b;
    blob_t1(&b, 2U, 60000U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_OK, "hwver: a stamp that matches the helmet loads");
}

/* @verifies REQ-FWHUB-62 SWR-HV-08 */
static void test_before_the_first_report_the_helmet_version_is_unverified_and_nothing_loads(void)
{
    fresh();
    g_report_received = false;
    blob_t b;
    blob_t1(&b, 1U, 60000U);        /* the stamp equals the SWR-HV-07 default the helmet reports before any record */
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_HW_VERSION_UNVERIFIED,
          "hwver: with no report from the safety MCU even a matching stamp is refused UNVERIFIED");
    expect_nothing_left_behind("hwver unverified");
    g_report_received = true;
    check(np_runner_load(b.buf, b.len) == NP_HUB_OK, "hwver: once the report arrives the same blob loads (a second send can succeed)");
}

/* @verifies REQ-FWHUB-64#a */
static void test_a_helmet_whose_version_has_no_bound_layout_loads_nothing(void)
{
    fresh();
    g_hw_version = 2U;
    g_layout_rc = NP_HUB_ERR_NO_LAYOUT;             /* the hub holds no layout for version 2 */
    blob_t b;
    blob_t1(&b, 2U, 60000U);                        /* the stamp matches the helmet: only the layout is missing */
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_NO_LAYOUT,
          "layout: a protocol that matches the helmet is refused, with the no-layout status, when none is bound for the helmet's version");
    check(NP_HUB_ERR_NO_LAYOUT != NP_HUB_ERR_HW_VERSION_MISMATCH && NP_HUB_ERR_NO_LAYOUT != NP_HUB_ERR_HW_VERSION_UNVERIFIED,
          "layout: the status is its own, neither a protocol mismatch nor a retry");
    check(g_layout_arg == 2U, "layout: the admission asked about the version the safety MCU reports");
    expect_nothing_left_behind("layout refused");
    check(held_duration_ms(NULL) == 0U, "layout refused: the descriptor is cleared, not left installed");
    g_layout_rc = NP_HUB_OK;
    check(np_runner_load(b.buf, b.len) == NP_HUB_OK, "layout: once a layout is bound the same blob loads");
}

/* @verifies REQ-FWHUB-64#a */
static void test_a_protocol_that_names_an_address_the_helmet_lacks_is_refused_whole(void)
{
    fresh();
    g_addr_rc = NP_HUB_ERR_BAD_ADDRESS;
    blob_t b;
    blob_t1(&b, 1U, 60000U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_BAD_ADDRESS,
          "address: a protocol naming an address the helmet does not have is refused with a status of its own");
    check(g_addr_calls == 1, "address: the check ran once");
    expect_nothing_left_behind("invalid address");
    check(held_duration_ms(NULL) == 0U, "invalid address: the descriptor is cleared, not left installed");
    g_addr_rc = NP_HUB_OK;
    check(np_runner_load(b.buf, b.len) == NP_HUB_OK, "address: the same blob loads once its addresses are valid");
}

/* @verifies REQ-FWHUB-64#a */
static void test_the_layout_refusal_comes_before_the_address_check(void)
{
    fresh();
    g_layout_rc = NP_HUB_ERR_NO_LAYOUT;
    g_addr_rc = NP_HUB_ERR_INVALID_ARG;
    blob_t b;
    blob_t1(&b, 1U, 60000U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_NO_LAYOUT,
          "order: with no layout bound the refusal is the layout's, because validity has nothing to be checked against");
    check(g_addr_calls == 0, "order: and the address check did not run");
}

/* @verifies REQ-FWHUB-64#a */
static void test_the_unverified_version_refusal_comes_before_the_layout_one(void)
{
    fresh();
    g_report_received = false;
    g_layout_rc = NP_HUB_ERR_NO_LAYOUT;
    blob_t b;
    blob_t1(&b, 1U, 60000U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_HW_VERSION_UNVERIFIED,
          "order: with no report the refusal is UNVERIFIED (retry), not the layout's");
}

/* @verifies REQ-UPG-01#c,d */
static void test_the_tier_refusal_comes_before_the_hardware_version_one(void)
{
    fresh();
    g_tier = NP_TIER_T1;
    g_hw_version = 1U;
    blob_t b;
    blob_t2(&b, 2U, 60000U);        /* wrong for the tier AND for the hardware version */
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_TIER_F4,
          "order: a protocol wrong in both ways is refused for its tier first");
}

/* @verifies REQ-FWHUB-03 */
static void test_a_refusal_after_a_good_load_does_not_disturb_it(void)
{
    fresh();
    blob_t a, bad;
    blob_t1(&a, 1U, 60000U);
    blob_t1(&bad, 2U, 5000U);
    check(np_runner_load(a.buf, a.len) == NP_HUB_OK, "keep: the first loads");
    check(np_runner_load(bad.buf, bad.len) == NP_HUB_ERR_SESSION_ACTIVE,
          "keep: a protocol for another version is refused SESSION_ACTIVE while one is loaded");
    check(held_duration_ms(NULL) == 60000U && np_lease_held(), "keep: the loaded session is untouched");
}

/* The goggles' parameter block for a photic drive at `mode`, `freq_hz`. */
static void blob_visual(blob_t *b, uint8_t hw_version, uint8_t mode, uint8_t freq_hz)
{
    np_mod_visual_params_t p;
    memset(&p, 0, sizeof p);
    p.mode = mode;
    p.freq_hz = freq_hz;
    p.duty_pct = 50U;
    blob_make(b, hw_version, 60000U, NP_MOD_VISUAL, NP_HUB_SLOT_VISUAL, (const uint8_t *)&p, sizeof p);
}

/* @verifies REQ-FWHUB-66 */
static void test_photic_driving_in_the_risk_zone_is_refused_at_upload_and_leaves_nothing(void)
{
    fresh();
    blob_t b;
    blob_visual(&b, 1U, 0U, 20U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_PHOTIC_ZONE,
          "photic: a goggles command at 20 Hz is refused at load, with a status of its own");
    expect_nothing_left_behind("photic");
    blob_visual(&b, 1U, 3U, 3U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_PHOTIC_ZONE, "photic: the combined mode at the lower bound is refused");
    expect_nothing_left_behind("photic combined");
    blob_visual(&b, 1U, 0U, 30U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_ERR_PHOTIC_ZONE, "photic: the upper bound is refused");
    expect_nothing_left_behind("photic upper");
    blob_visual(&b, 1U, 0U, 40U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_OK, "photic: 40 Hz loads");
    fresh();
    blob_visual(&b, 1U, 1U, 20U);
    check(np_runner_load(b.buf, b.len) == NP_HUB_OK, "photic: EMDR does not drive at freq_hz, so it loads");
}

/* @verifies REQ-FWHUB-66 */
static void test_every_refusal_np_runner_load_can_give_has_a_reason_the_apps_name(void)
{
    /* The reasons the apps group into a cause of their own (common/npps-core/src/library.rs, helmet_cause).  A status that
     * np_runner_load() can return and that is not here would reach a person as "some other reason". */
    static const int NAMED_REASONS[] = { 2, 4, 5, 6, 7, 8, 9, 13, 19, 20, 25, 27, 28, 29, 30, 31 };
    fresh();
    blob_t b;
    int seen[8]; unsigned n = 0U;
    blob_visual(&b, 1U, 0U, 20U);
    seen[n++] = -(int)np_runner_load(b.buf, b.len);
    fresh(); g_addr_rc = NP_HUB_ERR_BAD_ADDRESS; blob_t1(&b, 1U, 60000U);
    seen[n++] = -(int)np_runner_load(b.buf, b.len);
    fresh(); g_admits_sessions = false; blob_t1(&b, 1U, 60000U);
    seen[n++] = -(int)np_runner_load(b.buf, b.len);
    fresh(); blob_t2(&b, 1U, 60000U);
    seen[n++] = -(int)np_runner_load(b.buf, b.len);
    bool all_named = true;
    for (unsigned i = 0U; i < n; i++) {
        bool found = false;
        for (unsigned k = 0U; k < sizeof NAMED_REASONS / sizeof NAMED_REASONS[0]; k++) { if (NAMED_REASONS[k] == seen[i]) { found = true; } }
        if (!found) { all_named = false; }
    }
    check(all_named, "reasons: the refusals np_runner_load gives here are all ones the apps name");
}

int main(void)
{
    test_a_good_protocol_loads_and_holds_the_lease();
    test_a_second_blob_cannot_replace_a_loaded_descriptor();
    test_a_lease_held_by_someone_else_refuses_before_any_state_changes();
    test_an_empty_or_missing_buffer_is_refused_without_touching_the_lease();
    test_a_boot_that_refused_the_records_runs_no_session();
    test_a_bad_signature_is_refused_and_releases_the_lease();
    test_a_malformed_blob_is_refused_and_releases_the_lease();
    test_a_t2_protocol_on_a_t1_unit_is_refused_f4_and_leaves_nothing();
    test_a_t2_protocol_before_the_tier_is_known_is_unverified_not_f4();
    test_a_t2_protocol_on_a_t2_unit_loads();
    test_a_t1_protocol_loads_on_a_unit_whose_tier_is_not_yet_known();
    test_a_protocol_for_another_hardware_version_is_refused_and_leaves_nothing();
    test_a_matching_hardware_version_loads();
    test_before_the_first_report_the_helmet_version_is_unverified_and_nothing_loads();
    test_a_helmet_whose_version_has_no_bound_layout_loads_nothing();
    test_a_protocol_that_names_an_address_the_helmet_lacks_is_refused_whole();
    test_the_layout_refusal_comes_before_the_address_check();
    test_the_unverified_version_refusal_comes_before_the_layout_one();
    test_the_tier_refusal_comes_before_the_hardware_version_one();
    test_a_refusal_after_a_good_load_does_not_disturb_it();
    test_photic_driving_in_the_risk_zone_is_refused_at_upload_and_leaves_nothing();
    test_every_refusal_np_runner_load_can_give_has_a_reason_the_apps_name();

    if (g_unexpected != 0U) {
        printf("FAIL: np_runner_load() reached %u run-loop seam call(s)\n", g_unexpected);
        g_failures++;
    }
    printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
