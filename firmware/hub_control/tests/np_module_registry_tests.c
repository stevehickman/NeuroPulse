/*
 * NeurOne Hub Control — Module Registry Host Tests
 * Document: NP-FW-HUB-001 §3.2 (OI-FWHUB-16)
 *
 * The registry had no host test. That is how task_module_detect() came to call
 * the single-slot rescan for every accessory slot (7..18) while the rescan
 * refused any slot >= NP_HUB_ZONE_SLOT_COUNT (5) and the caller discarded the
 * return value: an accessory attached after boot — goggles, auricular VNS clip,
 * intranasal probe, cervical VNS, a T2 unit — was never registered, and only the
 * boot-time np_mod_reg_scan() ever saw one.
 *
 * What these tests pin:
 *   1. every accessory slot is re-probed and registered on hot-plug;
 *   2. an unchanged slot is NOT re-initialised — the intranasal and cervical VNS
 *      init() functions each write an SHDR auth record, so a rescan that
 *      re-ran init() every NP_DETECT_ACCESSORY_POLL_MS would flood SHDR;
 *   3. removal shuts the occupant down once and deregisters it;
 *   4. the retired zone slots 0-4 are refused without being probed;
 *   5. a failed init is not retried until the module is re-seated.
 *
 * The drivers are replaced by per-slot test doubles: every np_mod_*_ function
 * the probe table names routes to one table indexed by slot, so the registry
 * under test is the production np_module_registry.c, unmodified.
 *
 * No FreeRTOS, no hardware. IEC 62304 Class B — SW-02 hub control.
 *
 * Falsification (NP-CONV-001 §8): build against the pre-fix registry
 * (np_mod_reg_rescan_zone(uint8_t), NP-FW-HUB-001 Rev 7) with
 *   -DRESCAN_FN=np_mod_reg_rescan_zone
 * and this suite fails — the hot-plug, removal and failed-init cases all see
 * NP_HUB_ERR_INVALID_ARG and an unregistered slot.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "np_module_registry.h"

#ifndef RESCAN_FN
#define RESCAN_FN np_mod_reg_rescan_slot
#endif

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

/* ── Per-slot driver doubles ─────────────────────────────────────────────────── */

typedef struct {
    bool              present;       /* what detect() will report */
    np_hub_mod_type_t type;          /* type detect() reports when present */
    np_hub_status_t   init_rc;       /* what init() returns */
    unsigned          detect_calls;
    unsigned          init_calls;    /* == SHDR auth records for INS / CVNS */
    unsigned          shutdown_calls;
} slot_double_t;

static slot_double_t g_slot[NP_HUB_SLOT_MAX];

static np_hub_status_t dbl_detect(uint8_t slot, np_hub_mod_type_t *type_out)
{
    g_slot[slot].detect_calls++;
    if (!g_slot[slot].present) {
        return NP_HUB_ERR_NOT_PRESENT;
    }
    *type_out = g_slot[slot].type;
    return NP_HUB_OK;
}

static np_hub_status_t dbl_init(uint8_t slot)
{
    g_slot[slot].init_calls++;
    return g_slot[slot].init_rc;
}

static np_hub_status_t dbl_shutdown(uint8_t slot)
{
    g_slot[slot].shutdown_calls++;
    return NP_HUB_OK;
}

#define DRIVER_DOUBLES(pfx)                                                        \
    np_hub_status_t pfx##_detect(uint8_t s, np_hub_mod_type_t *t)                 \
        { return dbl_detect(s, t); }                                              \
    np_hub_status_t pfx##_init(uint8_t s) { return dbl_init(s); }                 \
    np_hub_status_t pfx##_control(uint8_t s, const void *p, uint16_t l)           \
        { (void)s; (void)p; (void)l; return NP_HUB_OK; }                          \
    np_hub_status_t pfx##_telemetry(uint8_t s, np_telem_record_t *o)              \
        { (void)s; (void)o; return NP_HUB_OK; }                                   \
    np_hub_status_t pfx##_shutdown(uint8_t s) { return dbl_shutdown(s); }

DRIVER_DOUBLES(np_mod_pbm)
DRIVER_DOUBLES(np_mod_intranasal)
DRIVER_DOUBLES(np_mod_eeg)
DRIVER_DOUBLES(np_mod_stim)
DRIVER_DOUBLES(np_mod_vns)
DRIVER_DOUBLES(np_mod_audio)
DRIVER_DOUBLES(np_mod_visual)
DRIVER_DOUBLES(np_mod_cvns)
DRIVER_DOUBLES(np_mod_qeeg)
DRIVER_DOUBLES(np_mod_tms)
DRIVER_DOUBLES(np_mod_pbm_1170nm)
DRIVER_DOUBLES(np_mod_clin_tacs)
DRIVER_DOUBLES(np_mod_hd_tdcs)
DRIVER_DOUBLES(np_mod_vibrotactile)

/* The type each accessory slot's driver reports. */
static const np_hub_mod_type_t k_slot_type[NP_HUB_SLOT_MAX] = {
    [NP_HUB_SLOT_ZONE_0]       = NP_MOD_PBM_BASE,
    [NP_HUB_SLOT_ZONE_1]       = NP_MOD_PBM_BASE,
    [NP_HUB_SLOT_ZONE_2]       = NP_MOD_PBM_BASE,
    [NP_HUB_SLOT_ZONE_3]       = NP_MOD_PBM_BASE,
    [NP_HUB_SLOT_ZONE_4]       = NP_MOD_PBM_BASE,
    [NP_HUB_SLOT_EEG]          = NP_MOD_EEG,
    [NP_HUB_SLOT_AUDIO]        = NP_MOD_AUDIO,
    [NP_HUB_SLOT_VISUAL]       = NP_MOD_VISUAL,
    [NP_HUB_SLOT_VNS_HRV]      = NP_MOD_VNS_HRV,
    [NP_HUB_SLOT_INTRANASAL]   = NP_MOD_INTRANASAL,
    [NP_HUB_SLOT_CVNS]         = NP_MOD_CVNS,
    [NP_HUB_SLOT_QEEG]         = NP_MOD_QEEG_21CH,
    [NP_HUB_SLOT_TMS]          = NP_MOD_TMS,
    [NP_HUB_SLOT_PBM_1170NM]   = NP_MOD_PBM_1170NM,
    [NP_HUB_SLOT_CLIN_TACS]    = NP_MOD_CLIN_TACS,
    [NP_HUB_SLOT_HD_TDCS]      = NP_MOD_HD_TDCS,
    [NP_HUB_SLOT_VIBROTACTILE] = NP_MOD_VIBROTACTILE,
    [NP_HUB_SLOT_BES_TACS]     = NP_MOD_BES_TACS,
    [NP_HUB_SLOT_TDCS]         = NP_MOD_TDCS,
};

/* Boot with the fixed hardware present and every accessory port empty, then
 * clear the counters so each test sees only what its rescans did. */
static void boot_empty(void)
{
    memset(g_slot, 0, sizeof(g_slot));
    for (uint8_t s = 0U; s < NP_HUB_SLOT_MAX; s++) {
        g_slot[s].type    = k_slot_type[s];
        g_slot[s].init_rc = NP_HUB_OK;
    }
    g_slot[NP_HUB_SLOT_EEG].present      = true;
    g_slot[NP_HUB_SLOT_AUDIO].present    = true;
    g_slot[NP_HUB_SLOT_BES_TACS].present = true;
    g_slot[NP_HUB_SLOT_TDCS].present     = true;

    np_mod_reg_init();
    (void)np_mod_reg_scan();

    for (uint8_t s = 0U; s < NP_HUB_SLOT_MAX; s++) {
        g_slot[s].detect_calls = g_slot[s].init_calls = g_slot[s].shutdown_calls = 0U;
    }
}

/* ── Tests ───────────────────────────────────────────────────────────────────── */

static void test_hot_plug_registers_every_accessory_slot(void)
{
    char name[128];
    for (uint8_t s = NP_HUB_SLOT_VISUAL; s < NP_HUB_SLOT_MAX; s++) {
        if (s == NP_HUB_SLOT_BES_TACS || s == NP_HUB_SLOT_TDCS) {
            continue;   /* fixed silicon — present from boot, covered below */
        }
        boot_empty();
        check(np_mod_reg_get(s) == NULL, "precondition: accessory slot empty at boot");

        g_slot[s].present = true;              /* accessory attached after boot */
        np_hub_status_t rc = RESCAN_FN(s);

        snprintf(name, sizeof name, "slot %u: rescan after hot-plug returns OK", s);
        check(rc == NP_HUB_OK, name);
        snprintf(name, sizeof name, "slot %u: detect() was called by the rescan", s);
        check(g_slot[s].detect_calls == 1U, name);
        snprintf(name, sizeof name, "slot %u: init() ran exactly once", s);
        check(g_slot[s].init_calls == 1U, name);
        np_mod_entry_t *e = np_mod_reg_get(s);
        snprintf(name, sizeof name, "slot %u: registered and resolvable via get()", s);
        check(e != NULL && e->type == k_slot_type[s] && e->slot == s, name);
    }
}

static void test_unchanged_slot_is_not_reinitialised(void)
{
    /* The SHDR flood case: the intranasal and cervical VNS init() functions each
     * write an SHDR auth record, and task_module_detect polls every 500 ms. */
    const uint8_t slots[] = { NP_HUB_SLOT_INTRANASAL, NP_HUB_SLOT_CVNS,
                              NP_HUB_SLOT_VISUAL, NP_HUB_SLOT_VNS_HRV };
    for (size_t i = 0U; i < sizeof slots; i++) {
        uint8_t s = slots[i];
        boot_empty();
        g_slot[s].present = true;
        (void)RESCAN_FN(s);
        for (int poll = 0; poll < 100; poll++) {
            (void)RESCAN_FN(s);
        }
        check(g_slot[s].detect_calls == 101U, "every poll re-probes the slot (detect)");
        check(g_slot[s].init_calls == 1U,
              "100 polls of a module still present: init() — and its SHDR auth record — ran once");
        check(g_slot[s].shutdown_calls == 0U, "a module still present is never shut down by a poll");
        check(np_mod_reg_get(s) != NULL, "module stays registered across polls");
    }
}

static void test_module_present_at_boot_is_not_reinitialised(void)
{
    boot_empty();
    g_slot[NP_HUB_SLOT_INTRANASAL].present = true;
    np_mod_reg_init();
    (void)np_mod_reg_scan();
    check(g_slot[NP_HUB_SLOT_INTRANASAL].init_calls == 1U, "boot scan initialised the probe");
    for (int poll = 0; poll < 10; poll++) {
        (void)RESCAN_FN(NP_HUB_SLOT_INTRANASAL);
    }
    check(g_slot[NP_HUB_SLOT_INTRANASAL].init_calls == 1U,
          "a module found by the boot scan is not re-initialised by idle polling");
}

static void test_removal_shuts_down_once_and_deregisters(void)
{
    boot_empty();
    uint8_t s = NP_HUB_SLOT_VNS_HRV;
    g_slot[s].present = true;
    (void)RESCAN_FN(s);

    g_slot[s].present = false;                  /* clip removed */
    np_hub_status_t rc = RESCAN_FN(s);
    check(rc == NP_HUB_ERR_NOT_PRESENT, "rescan after removal reports NOT_PRESENT");
    check(g_slot[s].shutdown_calls == 1U, "removed module's shutdown() ran once");
    check(np_mod_reg_get(s) == NULL, "removed module no longer resolvable");
    check(np_mod_reg_find(NP_MOD_VNS_HRV) == NULL, "removed module not found by type");

    for (int poll = 0; poll < 10; poll++) {
        (void)RESCAN_FN(s);
    }
    check(g_slot[s].shutdown_calls == 1U, "an empty slot is not shut down again on every poll");

    g_slot[s].present = true;                   /* re-seated */
    rc = RESCAN_FN(s);
    check(rc == NP_HUB_OK && np_mod_reg_get(s) != NULL, "re-seated module registers again");
    check(g_slot[s].init_calls == 2U, "re-seating runs init() again (one new auth record)");
}

static void test_type_change_reinitialises(void)
{
    /* No accessory port reports two types today; the rule is still "presence OR
     * type changed", so a swap that keeps the port occupied is not missed. */
    boot_empty();
    uint8_t s = NP_HUB_SLOT_CLIN_TACS;
    g_slot[s].present = true;
    (void)RESCAN_FN(s);
    g_slot[s].type = NP_MOD_HD_TDCS;
    (void)RESCAN_FN(s);
    np_mod_entry_t *e = np_mod_reg_get(s);
    check(g_slot[s].shutdown_calls == 1U, "type change shuts the previous occupant down");
    check(g_slot[s].init_calls == 2U, "type change initialises the new occupant");
    check(e != NULL && e->type == NP_MOD_HD_TDCS, "registry carries the new type");
}

static void test_failed_init_not_retried_until_reseated(void)
{
    boot_empty();
    uint8_t s = NP_HUB_SLOT_CVNS;
    g_slot[s].present = true;
    g_slot[s].init_rc = NP_HUB_ERR_MOD_INIT;
    np_hub_status_t rc = RESCAN_FN(s);
    check(rc == NP_HUB_ERR_MOD_INIT, "failed init is reported to the caller");
    check(np_mod_reg_get(s) == NULL, "a module whose init failed is not resolvable (fail closed)");

    for (int poll = 0; poll < 10; poll++) {
        rc = RESCAN_FN(s);
    }
    check(g_slot[s].init_calls == 1U, "failed init is not retried every poll");
    check(rc == NP_HUB_ERR_MOD_INIT, "unchanged failed slot keeps reporting MOD_INIT");
    check(np_mod_reg_get(s) == NULL, "still not resolvable");

    g_slot[s].present = false;
    (void)RESCAN_FN(s);
    check(g_slot[s].shutdown_calls == 0U, "an uninitialised module is not shut down on removal");
    g_slot[s].present = true;
    g_slot[s].init_rc = NP_HUB_OK;
    rc = RESCAN_FN(s);
    check(rc == NP_HUB_OK && np_mod_reg_get(s) != NULL, "re-seating retries init");
    check(g_slot[s].init_calls == 2U, "exactly one retry, on the re-seat");
}

static void test_fixed_slots_are_harmless_to_rescan(void)
{
    const uint8_t slots[] = { NP_HUB_SLOT_EEG, NP_HUB_SLOT_AUDIO,
                              NP_HUB_SLOT_BES_TACS, NP_HUB_SLOT_TDCS };
    boot_empty();
    for (size_t i = 0U; i < sizeof slots; i++) {
        uint8_t s = slots[i];
        for (int poll = 0; poll < 10; poll++) {
            check(RESCAN_FN(s) == NP_HUB_OK, "fixed slot rescan returns OK");
        }
        /* EEG init re-runs ADS1299 self-calibration; stim init zeroes BOTH the
         * BES and tDCS state — neither may happen on an idle poll. */
        check(g_slot[s].init_calls == 0U, "fixed slot is not re-initialised by a rescan");
        check(g_slot[s].shutdown_calls == 0U, "fixed slot is not shut down by a rescan");
        check(np_mod_reg_get(s) != NULL, "fixed slot stays registered");
    }
}

static void test_retired_zone_slots_refused_unprobed(void)
{
    boot_empty();
    for (uint8_t s = 0U; s < NP_HUB_ZONE_SLOT_COUNT; s++) {
        g_slot[s].present = true;
        check(RESCAN_FN(s) == NP_HUB_ERR_INVALID_ARG,
              "retired zone slot rescan refused");
        check(g_slot[s].detect_calls == 0U && g_slot[s].init_calls == 0U,
              "retired zone slot is neither probed nor initialised");
        check(np_mod_reg_get(s) == NULL, "retired zone slot not registered by a rescan");
    }
}

static void test_out_of_range_refused(void)
{
    boot_empty();
    check(RESCAN_FN(NP_HUB_SLOT_MAX) == NP_HUB_ERR_INVALID_ARG, "slot MAX refused");
    check(RESCAN_FN(NP_HUB_SLOT_NONE) == NP_HUB_ERR_INVALID_ARG, "slot NONE refused");
}

int main(void)
{
    test_hot_plug_registers_every_accessory_slot();
    test_unchanged_slot_is_not_reinitialised();
    test_module_present_at_boot_is_not_reinitialised();
    test_removal_shuts_down_once_and_deregisters();
    test_type_change_reinitialises();
    test_failed_init_not_retried_until_reseated();
    test_fixed_slots_are_harmless_to_rescan();
    test_retired_zone_slots_refused_unprobed();
    test_out_of_range_refused();

    printf("\n%s: %d failure(s)\n", g_failures ? "FAILED" : "OK", g_failures);
    return g_failures ? 1 : 0;
}
