/*
 * Host tests — the photoparoxysmal detector's channel roles as data (np_ppx_roles.h).
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "np_ppx_roles.h"

static int g_failures = 0;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

static np_ppx_montage_t montage(const np_ppx_site_t s[8])
{
    np_ppx_montage_t m;
    for (int i = 0; i < 8; i++) { m.channel_site[i] = s[i]; }
    return m;
}

/* @verifies REQ-FWHUB-69 */
static void test_the_t1_montage_has_no_occipital_channel_so_it_cannot_see(void)
{
    np_ppx_montage_t m;
    np_ppx_montage_t1(&m);
    np_ppx_roles_report_t r = np_ppx_roles_resolve(&m, NULL, 0U);
    check(r.roles.occipital_mask == 0U && r.n_occipital == 0U, "T1 as documented: no channel is occipital");
    check(!r.can_ever_see, "T1 as documented: the detector can never see, so the refusal stands");
    check(r.n_judging == 4U && r.roles.other_mask == 0xF0U, "T1: C3 C4 P3 P4 judge generalisation; Fp and F take no part");
    check(r.n_clinical_open == 2U, "T1: F3 and F4 are counted as open for Clinical, and do not judge");
}

/* @verifies REQ-FWHUB-69 */
static void test_a_montage_with_oz_and_central_parietal_sites_can_see(void)
{
    const np_ppx_site_t s[8] = { NP_PPX_SITE_OZ, NP_PPX_SITE_O1, NP_PPX_SITE_C3, NP_PPX_SITE_C4,
                                 NP_PPX_SITE_P3, NP_PPX_SITE_P4, NP_PPX_SITE_NONE, NP_PPX_SITE_FP1 };
    np_ppx_montage_t m = montage(s);
    np_ppx_roles_report_t r = np_ppx_roles_resolve(&m, NULL, 0U);
    check(r.roles.occipital_mask == 0x03U, "Oz and O1 are the occipital channels");
    check(r.roles.other_mask == 0x3CU, "C3 C4 P3 P4 judge; an unused channel and Fp1 do not");
    check(r.can_ever_see, "with an occipital channel and four judging channels the detector can see");
}

/* @verifies REQ-FWHUB-69 */
static void test_too_few_judging_channels_cannot_see(void)
{
    const np_ppx_site_t s[8] = { NP_PPX_SITE_OZ, NP_PPX_SITE_C3, NP_PPX_SITE_FP1, NP_PPX_SITE_FP2,
                                 NP_PPX_SITE_NONE, NP_PPX_SITE_NONE, NP_PPX_SITE_NONE, NP_PPX_SITE_NONE };
    np_ppx_montage_t m = montage(s);
    np_ppx_roles_report_t r = np_ppx_roles_resolve(&m, NULL, 0U);
    check(r.n_judging == 1U && !r.can_ever_see, "one judging channel is below min_other_channels: cannot ever see");
}

/* @verifies REQ-FWHUB-69 */
static void test_clinical_edits_the_table_and_nothing_else(void)
{
    /* Clinical rules that F3/F4 judge: the same montage resolves differently from a different table, with no code change. */
    uint8_t n = 0U;
    const np_ppx_site_role_t *shipped = np_ppx_role_table(&n);
    np_ppx_site_role_t edited[32];
    for (uint8_t i = 0U; i < n; i++) {
        edited[i] = shipped[i];
        if (edited[i].site == NP_PPX_SITE_F3 || edited[i].site == NP_PPX_SITE_F4) {
            edited[i].judges_generalised = true;
            edited[i].clinical_decides = false;
        }
    }
    np_ppx_montage_t m;
    np_ppx_montage_t1(&m);
    np_ppx_roles_report_t r = np_ppx_roles_resolve(&m, edited, n);
    check(r.n_judging == 6U && r.roles.other_mask == 0xFCU && r.n_clinical_open == 0U,
          "an edited table changes which channels judge, and closes the open rows");
    check(!r.can_ever_see, "and the T1 montage still has no occipital channel");
}

/* @verifies REQ-FWHUB-69 */
static void test_unknown_sites_and_no_montage_take_no_part(void)
{
    np_ppx_roles_report_t r = np_ppx_roles_resolve(NULL, NULL, 0U);
    check(r.roles.occipital_mask == 0U && r.roles.other_mask == 0U && !r.can_ever_see, "no montage: no roles, cannot see");
    const np_ppx_site_t s[8] = { NP_PPX_SITE_OZ, NP_PPX_SITE_C3, NP_PPX_SITE_C4, NP_PPX_SITE_NONE,
                                 NP_PPX_SITE_NONE, NP_PPX_SITE_NONE, NP_PPX_SITE_NONE, NP_PPX_SITE_NONE };
    np_ppx_montage_t m = montage(s);
    np_ppx_site_role_t only_oz[1] = { { NP_PPX_SITE_OZ, true, false, false } };
    r = np_ppx_roles_resolve(&m, only_oz, 1U);
    check(r.n_occipital == 1U && r.n_judging == 0U, "a site the table gives no role takes no part");
    /* A table row for "no site" must never give a role to the channels that carry none. */
    np_ppx_site_role_t with_none[2] = { { NP_PPX_SITE_NONE, false, true, false }, { NP_PPX_SITE_OZ, true, false, false } };
    r = np_ppx_roles_resolve(&m, with_none, 2U);
    check(r.n_judging == 0U && r.roles.other_mask == 0U, "a row for NONE gives the unused channels no role");
}

/* @verifies REQ-FWHUB-69 */
static void test_every_site_has_exactly_one_row_and_no_row_is_both_roles(void)
{
    uint8_t n = 0U;
    const np_ppx_site_role_t *t = np_ppx_role_table(&n);
    bool unique = true, exclusive = true;
    for (uint8_t i = 0U; i < n; i++) {
        if (t[i].occipital && t[i].judges_generalised) { exclusive = false; }
        for (uint8_t j = (uint8_t)(i + 1U); j < n; j++) { if (t[i].site == t[j].site) { unique = false; } }
    }
    check(unique, "no site appears twice in the shipped table");
    check(exclusive, "no site is both occipital and a generalisation judge: it would count twice");
    check(n == (uint8_t)(NP_PPX_SITE_COUNT - 1), "the shipped table has one row for every named site");
}

int main(void)
{
    test_the_t1_montage_has_no_occipital_channel_so_it_cannot_see();
    test_a_montage_with_oz_and_central_parietal_sites_can_see();
    test_too_few_judging_channels_cannot_see();
    test_clinical_edits_the_table_and_nothing_else();
    test_unknown_sites_and_no_montage_take_no_part();
    test_every_site_has_exactly_one_row_and_no_row_is_both_roles();
    printf("=== np_ppx_roles_tests: %d failure(s) ===\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
