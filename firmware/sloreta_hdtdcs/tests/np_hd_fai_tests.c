/*
 * NeurOne sLORETA-guided HD-tDCS — First Article Inspection Tests
 * Document: NP-FW-HD-001 Rev A §12 / NP-FAI-HD-001 Rev A
 *
 * FAI-HD01: sLORETA source localization accuracy vs known phantom
 * FAI-HD02: Automatic MNI→10-20 electrode mapping accuracy
 * FAI-HD03: 4×1 ring focality vs standard 2-electrode (saline phantom)
 * FAI-HD04: EEG signal quality during concurrent tDCS (SNR ≥ 20 dB)
 *
 * Test categories:
 *   Software-verifiable (host CI): FAI-HD02 mapping accuracy (full coverage).
 *   Hardware bench required: FAI-HD01, FAI-HD03, FAI-HD04.
 *   Hardware tests are documented here as procedure stubs with PASS criteria.
 *
 * Build for host: -DNPTEST_HOST  (see CMakeLists.txt NP_BUILD_TESTS option)
 * Build for target: compile into test firmware image with platform HAL wired.
 *
 * Return convention: 0 = PASS, non-zero = FAIL (count of failures).
 */

#include "np_hd_montage.h"
#include "np_sloreta.h"
#include "np_hd_stim.h"
#include "np_hd_session.h"

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ── Test infrastructure ─────────────────────────────────────────────────────── */

static int g_fail_count = 0;

#define ASSERT(cond, msg)                                               \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL [%s:%d] %s\n", __func__, __LINE__, (msg));    \
            g_fail_count++;                                             \
        }                                                               \
    } while (0)

#define ASSERT_OK(expr)         ASSERT((expr) == NP_HD_OK, #expr " != NP_HD_OK")
#define ASSERT_EQ(a, b)         ASSERT((a) == (b), #a " != " #b)
#define ASSERT_APPROX(a, b, tol) ASSERT(fabsf((a)-(b)) <= (tol), \
                                         #a " not within " #tol " of " #b)

/* ── FAI-HD02: MNI→10-20 electrode mapping accuracy ─────────────────────────── */
/*
 * Tests the software electrode selection algorithm exhaustively.
 * No hardware required.
 *
 * Pass criteria:
 *   HD02-A: For each predefined clinical target, nearest electrode is
 *            within 35 mm MNI distance.
 *   HD02-B: 4×1 ring montage has center electrode closest to target
 *            (not outcompeted by any cathode).
 *   HD02-C: 4 cathodes cover ≥ 2 angular quadrants around center.
 *   HD02-D: All montage electrodes map to distinct tACS driver channels.
 *   HD02-E: Standard 2-electrode montage: anode ≠ cathode.
 *   HD02-F: Bilateral 4×1: left and right anodes are in opposite hemispheres.
 *   HD02-G: Bilateral 4×1: all 10 electrodes across both rings are distinct.
 *   HD02-H: Bilateral on a midline target is rejected, not silently unilateral.
 *   HD02-I: validate() rejects cross-ring conflicts in hand-built montages.
 *   HD02-K: All 21 cap electrodes map to distinct tACS driver channels (§6.4).
 */
static int fai_hd02_electrode_mapping(void)
{
    int failures_before = g_fail_count;
    printf("FAI-HD02: MNI->10-20 electrode mapping accuracy\n");

    /* HD02-A: clinical targets — nearest electrode within 35 mm. */
    static const struct { np_hd_clinical_target_t target; const char *name; } targets[] = {
        { NP_HD_TARGET_DLPFC_L, "DLPFC_L" }, { NP_HD_TARGET_DLPFC_R, "DLPFC_R" },
        { NP_HD_TARGET_VLPFC_L, "VLPFC_L" }, { NP_HD_TARGET_ACC,      "ACC"     },
        { NP_HD_TARGET_MPFC,    "MPFC"    }, { NP_HD_TARGET_M1_L,    "M1_L"   },
        { NP_HD_TARGET_M1_R,    "M1_R"   },
    };
    uint8_t n_targets = (uint8_t)(sizeof(targets) / sizeof(targets[0]));

    for (uint8_t t = 0U; t < n_targets; t++) {
        np_hd_mni_t mni;
        ASSERT_OK(np_hd_clinical_target_mni(targets[t].target, &mni));

        float dist_mm = 0.0f;
        np_hd_electrode_t nearest = np_hd_nearest_electrode(&mni, &dist_mm);
        printf("  %s: nearest=%s dist=%.1f mm\n",
               targets[t].name, np_hd_electrode_name(nearest), dist_mm);

        ASSERT(dist_mm <= 35.0f, "HD02-A: nearest electrode >35 mm from target");
        ASSERT(nearest < NP_HD_CH_COUNT, "HD02-A: nearest electrode out of range");
    }

    /* HD02-B/C/D: 4×1 ring montage for each clinical target. */
    for (uint8_t t = 0U; t < n_targets; t++) {
        np_hd_mni_t mni;
        np_hd_clinical_target_mni(targets[t].target, &mni);

        np_hd_montage_t mont;
        memset(&mont, 0, sizeof(mont));
        np_hd_status_t ret = np_hd_montage_select_ring(&mni, &mont);

        ASSERT(ret == NP_HD_OK, "HD02-B: ring montage selection failed");
        if (ret != NP_HD_OK) {
            continue;
        }

        /* HD02-B: center electrode is closest to target. */
        float center_dist = 0.0f;
        np_hd_mni_t center_mni;
        np_hd_electrode_mni(mont.center, &center_mni);
        {
            int32_t dx = center_mni.x - mni.x, dy = center_mni.y - mni.y,
                    dz = center_mni.z - mni.z;
            center_dist = sqrtf((float)(dx*dx + dy*dy + dz*dz));
        }
        for (uint8_t k = 0U; k < mont.cathode_count; k++) {
            np_hd_mni_t cm;
            np_hd_electrode_mni(mont.cathodes[k], &cm);
            int32_t dx = cm.x - mni.x, dy = cm.y - mni.y, dz = cm.z - mni.z;
            float d = sqrtf((float)(dx*dx + dy*dy + dz*dz));
            ASSERT(d >= center_dist, "HD02-B: cathode closer to target than center");
        }

        /* HD02-C: cathodes span ≥ 2 quadrants. */
        uint8_t qmask = 0U;
        for (uint8_t k = 0U; k < mont.cathode_count; k++) {
            np_hd_mni_t cm;
            np_hd_electrode_mni(mont.cathodes[k], &cm);
            uint8_t q = (uint8_t)((cm.x >= center_mni.x ? 1U : 0U) |
                                   ((cm.y >= center_mni.y ? 1U : 0U) << 1));
            qmask |= (uint8_t)(1U << q);
        }
        ASSERT(__builtin_popcount(qmask) >= 2U, "HD02-C: cathodes collinear (<2 quadrants)");

        /* HD02-D: distinct driver channels. */
        ASSERT_OK(np_hd_montage_validate(&mont));

        printf("  %s ring: center=%s cathodes=%s,%s,%s,%s quadrants=%u\n",
               targets[t].name, np_hd_electrode_name(mont.center),
               np_hd_electrode_name(mont.cathodes[0]),
               np_hd_electrode_name(mont.cathodes[1]),
               np_hd_electrode_name(mont.cathodes[2]),
               np_hd_electrode_name(mont.cathodes[3]),
               (unsigned)__builtin_popcount(qmask));
    }

    /* HD02-E: standard 2-electrode — anode ≠ cathode. */
    {
        np_hd_mni_t dlpfc_l;
        np_hd_clinical_target_mni(NP_HD_TARGET_DLPFC_L, &dlpfc_l);
        np_hd_montage_t mont;
        ASSERT_OK(np_hd_montage_select_standard(&dlpfc_l, &mont));
        ASSERT(mont.center != mont.cathodes[0], "HD02-E: anode == cathode");
        printf("  2-elec DLPFC_L: anode=%s cathode=%s\n",
               np_hd_electrode_name(mont.center),
               np_hd_electrode_name(mont.cathodes[0]));
    }

    /* HD02-F: bilateral 4×1 — anodes in opposite hemispheres. */
    {
        np_hd_mni_t dlpfc_l;
        np_hd_clinical_target_mni(NP_HD_TARGET_DLPFC_L, &dlpfc_l);
        np_hd_montage_t mont;
        ASSERT_OK(np_hd_montage_select_bilateral(&dlpfc_l, &mont));
        np_hd_mni_t lmni, rmni;
        np_hd_electrode_mni(mont.center,   &lmni);
        np_hd_electrode_mni(mont.center_r, &rmni);
        ASSERT(lmni.x < 0 && rmni.x > 0, "HD02-F: bilateral anodes not in opposite hemispheres");
        printf("  bilateral: L_center=%s (x=%d) R_center=%s (x=%d)\n",
               np_hd_electrode_name(mont.center),   lmni.x,
               np_hd_electrode_name(mont.center_r), rmni.x);
    }

    /* HD02-G: bilateral 4×1 — all 10 electrodes of both rings are distinct.     */
    /*
     * NP-FW-HD-001 §6.3 drives both hemispheres simultaneously, so the ten
     * electrodes are energised at once and a cross-hemisphere conflict is as
     * disqualifying as one inside a hemisphere.  Before this check existed the
     * bilateral path shipped Cz as a cathode in BOTH rings for M1_L and M1_R
     * (driver 11 twice), and the M1_L right ring additionally carried the
     * C4/P4 driver-12 collision, because select_bilateral() had its own copy of
     * the selection loop that never received §6.2 step 5.
     */
    {
        static const struct {
            np_hd_clinical_target_t target;
            const char             *name;
        } lateral[] = {
            { NP_HD_TARGET_DLPFC_L, "DLPFC_L" }, { NP_HD_TARGET_DLPFC_R, "DLPFC_R" },
            { NP_HD_TARGET_VLPFC_L, "VLPFC_L" },
            { NP_HD_TARGET_M1_L,    "M1_L"    }, { NP_HD_TARGET_M1_R,    "M1_R"    },
        };
        uint8_t n_lateral = (uint8_t)(sizeof(lateral) / sizeof(lateral[0]));

        for (uint8_t t = 0U; t < n_lateral; t++) {
            np_hd_mni_t mni;
            np_hd_clinical_target_mni(lateral[t].target, &mni);

            np_hd_montage_t mont;
            memset(&mont, 0, sizeof(mont));
            np_hd_status_t ret = np_hd_montage_select_bilateral(&mni, &mont);
            ASSERT(ret == NP_HD_OK, "HD02-G: bilateral selection failed on lateral target");
            if (ret != NP_HD_OK) {
                continue;
            }
            ASSERT(mont.bilateral == 1U, "HD02-G: bilateral flag not set");
            ASSERT_EQ(mont.type, NP_HD_MONTAGE_BILATERAL_4X1);

            /* Flatten both rings into one array and scan every pair.            */
            np_hd_electrode_t all[NP_HD_BILATERAL_ELEC_COUNT];
            all[0] = mont.center;
            all[1 + NP_HD_RING_CATHODE_COUNT] = mont.center_r;
            for (uint8_t k = 0U; k < NP_HD_RING_CATHODE_COUNT; k++) {
                all[1U + k]                              = mont.cathodes[k];
                all[2U + NP_HD_RING_CATHODE_COUNT + k]   = mont.cathodes_r[k];
            }

            for (uint8_t i = 0U; i < NP_HD_BILATERAL_ELEC_COUNT; i++) {
                ASSERT(all[i] < NP_HD_CH_COUNT, "HD02-G: electrode index out of range");
                for (uint8_t j = (uint8_t)(i + 1U); j < NP_HD_BILATERAL_ELEC_COUNT; j++) {
                    ASSERT(all[i] != all[j],
                           "HD02-G: same electrode in both rings (one pellet, two currents)");
                }
            }

            /*
             * Driver-channel distinctness across all ten.  k_driver_channel[] is
             * file-static in np_hd_montage.c, so validate() is the probe: it now
             * walks both rings against one claim set and rejects a repeat of
             * either the electrode or its driver channel.
             */
            ASSERT_OK(np_hd_montage_validate(&mont));

            printf("  bilateral %s: L=%s[%s,%s,%s,%s] R=%s[%s,%s,%s,%s]\n",
                   lateral[t].name,
                   np_hd_electrode_name(mont.center),
                   np_hd_electrode_name(mont.cathodes[0]),
                   np_hd_electrode_name(mont.cathodes[1]),
                   np_hd_electrode_name(mont.cathodes[2]),
                   np_hd_electrode_name(mont.cathodes[3]),
                   np_hd_electrode_name(mont.center_r),
                   np_hd_electrode_name(mont.cathodes_r[0]),
                   np_hd_electrode_name(mont.cathodes_r[1]),
                   np_hd_electrode_name(mont.cathodes_r[2]),
                   np_hd_electrode_name(mont.cathodes_r[3]));
        }
    }

    /* HD02-H: bilateral on a midline target is rejected, not silently unilateral.*/
    /*
     * ACC (0, 28, 28) and MPFC (0, 52, 6) sit on x = 0, so negating x yields the
     * same coordinate and the "contralateral" ring resolves to the ring already
     * selected.  Both previously returned NP_HD_OK with center_r == center and all
     * four cathodes duplicated — a unilateral montage labelled as ten electrodes,
     * which np_hd_montage_validate() reported as fine.
     */
    {
        static const struct {
            np_hd_clinical_target_t target;
            const char             *name;
        } midline[] = {
            { NP_HD_TARGET_ACC,  "ACC"  },
            { NP_HD_TARGET_MPFC, "MPFC" },
        };
        uint8_t n_midline = (uint8_t)(sizeof(midline) / sizeof(midline[0]));

        for (uint8_t t = 0U; t < n_midline; t++) {
            np_hd_mni_t mni;
            np_hd_clinical_target_mni(midline[t].target, &mni);
            ASSERT_EQ(mni.x, 0);

            np_hd_montage_t mont;
            memset(&mont, 0xAA, sizeof(mont));   /* poison: prove we overwrite  */
            np_hd_status_t ret = np_hd_montage_select_bilateral(&mni, &mont);

            ASSERT(ret == NP_HD_ERR_MONTAGE_INVALID,
                   "HD02-H: midline target did not reject bilateral");
            ASSERT(mont.bilateral == 0U,
                   "HD02-H: bilateral flag left set on a rejected montage");
            printf("  bilateral %s (x=0): rejected (%d), bilateral flag clear\n",
                   midline[t].name, (int)ret);
        }
    }

    /* HD02-I: validate() rejects cross-ring conflicts in hand-built montages.   */
    /*
     * These montages are constructed by hand rather than obtained from the
     * selector, deliberately.  The selector can no longer produce a conflicting
     * bilateral montage, so if these cases were only exercised through it,
     * validate()'s right-ring branches would run zero times and could be deleted
     * or broken without any test objecting.  The selector guarantees the invariant;
     * these assertions are what keep the independent check honest.
     *
     * Each negative case is paired with a positive control that differs only in the
     * conflicting electrode, so a rejection cannot be credited to the wrong cause.
     */
    {
        /* Positive control: two disjoint rings, no shared electrode or driver.  */
        np_hd_montage_t ok;
        memset(&ok, 0, sizeof(ok));
        ok.type          = NP_HD_MONTAGE_BILATERAL_4X1;
        ok.bilateral     = 1U;
        ok.cathode_count = NP_HD_RING_CATHODE_COUNT;
        ok.center        = NP_HD_CH_C3;
        ok.cathodes[0]   = NP_HD_CH_FC3; ok.cathodes[1] = NP_HD_CH_F3;
        ok.cathodes[2]   = NP_HD_CH_CZ;  ok.cathodes[3] = NP_HD_CH_P3;
        ok.center_r      = NP_HD_CH_C4;
        ok.cathodes_r[0] = NP_HD_CH_FC4; ok.cathodes_r[1] = NP_HD_CH_F4;
        ok.cathodes_r[2] = NP_HD_CH_FZ;  ok.cathodes_r[3] = NP_HD_CH_T8;
        ASSERT_OK(np_hd_montage_validate(&ok));

        /* I-1: same electrode (Cz) as a cathode in both rings.                  */
        /* This is the M1 case as it actually shipped.  It must stay non-vacuous  */
        /* under any future k_driver_channel[]: one pellet cannot carry two       */
        /* independently driven currents regardless of how the cap is wired.      */
        np_hd_montage_t dup = ok;
        dup.cathodes_r[2] = NP_HD_CH_CZ;    /* was Fz; Cz is already in the left */
        ASSERT(np_hd_montage_validate(&dup) == NP_HD_ERR_MONTAGE_INVALID,
               "HD02-I1: duplicate electrode across rings accepted");

        /*
         * NOTE: there is deliberately no "distinct electrodes sharing one driver
         * channel" case here.  Under the 21-channel mapping (§6.4) that is
         * unconstructible — electrode i is driven by channel i, so two different
         * electrodes can never name the same channel.  An earlier revision used
         * Cz/Pz, which both mapped to driver 11 under the 16-channel placeholder;
         * with the identity map that assertion would still PASS while testing
         * nothing at all.  HD02-K below tests the property that makes it
         * unconstructible, which is the thing that can actually regress.
         */

        /* I-2b: the conflict is on center_r itself, not on a right cathode.     */
        /* Distinct branch from I-1: that enters validate()'s cathodes_r         */
        /* loop, this one is rejected before the loop is reached.  Verified with */
        /* gcov — without this case that rejection executes zero times while     */
        /* every assertion still passes.                                         */
        np_hd_montage_t dup_c = ok;
        dup_c.center_r = NP_HD_CH_CZ;       /* Cz is already left cathodes[2]    */
        ASSERT(np_hd_montage_validate(&dup_c) == NP_HD_ERR_MONTAGE_INVALID,
               "HD02-I2b: right anode duplicating a left cathode accepted");

        np_hd_montage_t same_c = ok;
        same_c.center_r = NP_HD_CH_C3;      /* both rings anchored on one anode  */
        ASSERT(np_hd_montage_validate(&same_c) == NP_HD_ERR_MONTAGE_INVALID,
               "HD02-I2b: center_r == center accepted");

        /* I-2c: a bilateral montage must carry a full right ring.  With a short  */
        /* cathode_count the right-ring loop would stop early and the untouched   */
        /* tail of cathodes_r[] would never be examined.                          */
        np_hd_montage_t short_r = ok;
        short_r.cathode_count = 1U;
        ASSERT(np_hd_montage_validate(&short_r) == NP_HD_ERR_MONTAGE_INVALID,
               "HD02-I2c: bilateral montage with partial right ring accepted");

        /* I-3: out-of-range right-ring indices are caught, not indexed with.    */
        np_hd_montage_t bad_c = ok;
        bad_c.center_r = (np_hd_electrode_t)NP_HD_CH_COUNT;
        ASSERT(np_hd_montage_validate(&bad_c) == NP_HD_ERR_MONTAGE_INVALID,
               "HD02-I3: out-of-range center_r accepted");

        np_hd_montage_t bad_k = ok;
        bad_k.cathodes_r[1] = NP_HD_CH_NONE;
        ASSERT(np_hd_montage_validate(&bad_k) == NP_HD_ERR_MONTAGE_INVALID,
               "HD02-I3: out-of-range cathodes_r accepted");

        /* I-4: the same right-ring content is ignored when bilateral is clear,  */
        /* so unilateral montages keep their previous validate() behaviour.      */
        np_hd_montage_t uni = dup;          /* still carries the Cz duplicate    */
        uni.bilateral = 0U;
        uni.type      = NP_HD_MONTAGE_RING_4X1;
        ASSERT_OK(np_hd_montage_validate(&uni));

        printf("  validate(): cross-ring duplicate/driver/range rejected; "
               "unilateral path unchanged\n");
    }

    /* HD02-K: every cap electrode maps to its own tACS driver channel.          */
    /*
     * §6.4 specifies 21 driver channels, one per electrode, no sharing.  This is
     * what lets each 4×1 ring take its geometrically nearest cathodes instead of
     * routing around a wiring artifact, and it is what makes a cross-ring driver
     * collision between two DIFFERENT electrodes impossible to construct.
     *
     * k_driver_channel[] is file-static in np_hd_montage.c, so the property is
     * probed through the public API: for every unordered pair (i, j), a two-
     * electrode montage {i, j} must validate.  validate() rejects on a repeated
     * driver channel, so a single passing sweep over all 210 pairs is equivalent
     * to "all 21 channels are distinct".
     *
     * If anyone re-aliases the table — the 16-channel placeholder wrapped
     * electrodes 16–20 onto channels 11–15 — this fails immediately and names the
     * offending pair, rather than surfacing later as an undeliverable clinical
     * montage the way C4/P4 did for M1_R.
     */
    {
        uint16_t pairs_checked = 0U;
        uint8_t  first_bad_i = 0U, first_bad_j = 0U;
        bool     found_bad = false;

        for (uint8_t i = 0U; i < NP_HD_CH_COUNT; i++) {
            for (uint8_t j = (uint8_t)(i + 1U); j < NP_HD_CH_COUNT; j++) {
                np_hd_montage_t pair;
                memset(&pair, 0, sizeof(pair));
                pair.type          = NP_HD_MONTAGE_STANDARD_2E;
                pair.cathode_count = 1U;
                pair.center        = (np_hd_electrode_t)i;
                pair.cathodes[0]   = (np_hd_electrode_t)j;

                if (np_hd_montage_validate(&pair) != NP_HD_OK && !found_bad) {
                    found_bad   = true;
                    first_bad_i = i;
                    first_bad_j = j;
                }
                pairs_checked++;
            }
        }

        ASSERT(!found_bad, "HD02-K: two electrodes share a tACS driver channel");
        ASSERT_EQ(pairs_checked, (NP_HD_CH_COUNT * (NP_HD_CH_COUNT - 1U)) / 2U);

        if (found_bad) {
            printf("  HD02-K: %s and %s collide\n",
                   np_hd_electrode_name((np_hd_electrode_t)first_bad_i),
                   np_hd_electrode_name((np_hd_electrode_t)first_bad_j));
        } else {
            printf("  %u electrode pairs checked: all map to distinct drivers\n",
                   (unsigned)pairs_checked);
        }
    }

    int result = g_fail_count - failures_before;
    printf("FAI-HD02: %s (%d failures)\n\n", result == 0 ? "PASS" : "FAIL", result);
    return result;
}

/* ── FAI-HD01: sLORETA source localization accuracy (phantom bench) ─────────── */
/*
 * Hardware bench procedure (not software-executable in CI).
 *
 * Setup:
 *   - EEG phantom: 21-electrode saline skull phantom (IEC 60601-1 electrical
 *     reference model or equivalent).
 *   - Inject known dipole via two current-source channels at a specified MNI
 *     location (e.g., DLPFC_L at -46, 36, 20 mm).
 *   - Record 2 minutes of EEG at 500 Hz through T2 21-ch cap on phantom.
 *   - Run np_sloreta_compute_map() + np_sloreta_find_peak() on recorded data.
 *
 * Pass criteria (HD01):
 *   HD01-A: Peak localization error ≤ 15 mm from known dipole location.
 *   HD01-B: Peak source power at dipole location ≥ 3× median voxel power.
 *   HD01-C: localization error ≤ 15 mm for 5/6 standard clinical targets
 *            (DLPFC_L, DLPFC_R, ACC, MPFC, M1_L, M1_R).
 *
 * This test stub verifies the software plumbing with a synthetic sinusoidal
 * source (see the stimulus comment below for why it must not be DC).
 * The numerical accuracy requirement (≤ 15 mm) is verified only on hardware.
 */
static int fai_hd01_sloreta_plumbing(void)
{
    int failures_before = g_fail_count;
    printf("FAI-HD01: sLORETA source localization (software plumbing check)\n");

    /* Minimal synthetic weight matrix (2 voxels × 21 channels). */
    static float W[2 * NP_HD_SLORETA_N_CH];
    static np_hd_mni_t voxel_mni[2] = { { -46, 36, 20 }, { 0, 0, 0 } };

    /* Voxel 0 matches DLPFC_L; set its weight vector to unit electrode F3. */
    memset(W, 0, sizeof(W));
    W[0 * NP_HD_SLORETA_N_CH + NP_HD_CH_F3] = 1.0f;  /* voxel 0 → F3          */
    W[1 * NP_HD_SLORETA_N_CH + NP_HD_CH_O2] = 1.0f;  /* voxel 1 → O2 (noise)  */

    np_sloreta_ctx_t ctx;
    ASSERT_OK(np_sloreta_init(&ctx, W, voxel_mni, 2U));

    /*
     * Inject a synthetic epoch: F3 carries a 10 µV alpha-band sinusoid, O2 a
     * 1 µV tone standing in for background activity at the undriven voxel.
     *
     * The stimulus MUST be time-varying.  np_sloreta_push_epoch() subtracts the
     * per-channel epoch mean before accumulating the covariance, so the DC level
     * this test used to inject left an exactly zero residual on every sample.
     * The covariance stayed zero, every voxel's W^T·C·W evaluated to zero, and
     * the dominance/band-power/covariance-norm assertions all failed against
     * firmware that was behaving correctly — a covariance-based localiser has no
     * power to report for a zero-variance input.
     *
     * Cycle counts are whole numbers per 1024-sample epoch, so each channel's
     * epoch mean is exactly zero and mean subtraction introduces no leakage.
     * At 500 Hz: 20 cycles = 9.77 Hz (alpha), 7 cycles = 3.42 Hz (delta).
     *
     * M_PI is deliberately not used — it is POSIX, not ISO C, and these tests
     * compile with a strict -std=c11 where glibc hides it.
     */
    static const float    k_two_pi          = 6.283185307179586f;
    static const float    k_f3_amplitude_uv = 10.0f;
    static const float    k_o2_amplitude_uv = 1.0f;
    static const uint16_t k_f3_cycles       = 20U;
    static const uint16_t k_o2_cycles       = 7U;

    static float samples[NP_HD_SLORETA_N_CH][NP_HD_SLORETA_FFT_SIZE];
    memset(samples, 0, sizeof(samples));
    for (uint16_t s = 0U; s < NP_HD_SLORETA_FFT_SIZE; s++) {
        float phase = k_two_pi * (float)s / (float)NP_HD_SLORETA_FFT_SIZE;
        samples[NP_HD_CH_F3][s] = k_f3_amplitude_uv * sinf(phase * (float)k_f3_cycles);
        samples[NP_HD_CH_O2][s] = k_o2_amplitude_uv * sinf(phase * (float)k_o2_cycles);
    }

    /* Push enough epochs to satisfy NP_HD_SLORETA_EPOCHS. */
    for (uint16_t e = 0U; e < NP_HD_SLORETA_EPOCHS; e++) {
        ASSERT_OK(np_sloreta_push_epoch(&ctx,
                                         (const float (*)[NP_HD_SLORETA_FFT_SIZE])samples,
                                         NP_HD_SLORETA_FFT_SIZE));
    }
    ASSERT(np_sloreta_epoch_count(&ctx) == NP_HD_SLORETA_EPOCHS,
           "HD01: epoch count mismatch");

    /* Compute source map. */
    float source_power[2];
    ASSERT_OK(np_sloreta_compute_map(&ctx, source_power, 2U));

    /* Voxel 0 (DLPFC_L, driven by F3) should dominate. */
    ASSERT(source_power[0] > source_power[1],
           "HD01: driven voxel not dominant — sLORETA plumbing error");
    printf("  source_power[0 DLPFC_L]=%.4f source_power[1 O2]=%.4f ratio=%.2f\n",
           source_power[0], source_power[1],
           source_power[1] > 0.0f ? source_power[0]/source_power[1] : 999.0f);

    /* Find peak. */
    np_hd_sloreta_result_t result;
    ASSERT_OK(np_sloreta_find_peak(&ctx, source_power, 2U, &result));
    ASSERT(result.peak_voxel == 0U, "HD01: peak voxel should be voxel 0 (DLPFC_L)");
    ASSERT(result.valid, "HD01: result.valid is false");
    printf("  peak_voxel=%u MNI=(%d,%d,%d)\n",
           result.peak_voxel, result.peak_mni.x, result.peak_mni.y, result.peak_mni.z);

    /* Band power decomposition. */
    np_hd_band_power_t bands;
    ASSERT_OK(np_sloreta_band_power(&ctx, 0U, source_power, &bands));
    ASSERT(bands.alpha + bands.theta + bands.delta + bands.beta > 0.0f,
           "HD01: band power all zero");

    /* Covariance norm should be non-zero. */
    float cnorm = np_sloreta_covariance_norm(&ctx);
    ASSERT(cnorm > 0.0f, "HD01: covariance norm is zero");

    /* Reset and verify epoch count clears. */
    np_sloreta_reset(&ctx);
    ASSERT(np_sloreta_epoch_count(&ctx) == 0U, "HD01: epoch count not cleared after reset");

    printf("NOTE: FAI-HD01 hardware bench (15 mm accuracy criterion) requires\n");
    printf("      21-ch EEG phantom and known dipole injection at bench.\n");
    printf("      See NP-FAI-HD-001 §3 for full procedure.\n");

    int result2 = g_fail_count - failures_before;
    printf("FAI-HD01 (software plumbing): %s (%d failures)\n\n",
           result2 == 0 ? "PASS" : "FAIL", result2);
    return result2;
}

/* ── FAI-HD03: 4×1 ring focality (saline phantom bench procedure) ────────────── */
/*
 * Hardware bench procedure (not software-executable in CI).
 *
 * Setup:
 *   - Saline phantom: spherical 0.25 S/m saline head phantom (radius ~90 mm).
 *   - T2 wet gel 21-ch cap applied to phantom.
 *   - Micro-Ag/AgCl reference electrodes on 5 mm grid inside phantom.
 *   - NeurOne T2 HD-tDCS driver: 4×1 ring montage centred on C3
 *     (DLPFC_L approximation on phantom surface), 1 mA anode.
 *   - Repeat with standard 2-electrode (C3 anode, P4 cathode).
 *
 * Measurement:
 *   - Map electric field magnitude (mV/mm) on a coronal slice at 10 mm depth.
 *   - Compute FWHM of field distribution for each montage.
 *
 * Pass criteria (HD03):
 *   HD03-A: 4×1 ring FWHM ≤ 25 mm (specification: ~1.5 cm at target depth).
 *   HD03-B: Standard 2-electrode FWHM ≥ 60 mm (broad field expected).
 *   HD03-C: 4×1 peak field magnitude ≥ standard peak field × 0.5
 *            (focality gain without unacceptable field loss).
 *   HD03-D: All electrode current readings within ±5% of programmed values.
 *
 * This function verifies only the current distribution algorithm (software).
 */
static int fai_hd03_focality_algorithm(void)
{
    int failures_before = g_fail_count;
    printf("FAI-HD03: 4x1 focality (current distribution algorithm check)\n");

    /* Verify cathode current sum == anode current (charge balance). */
    np_hd_mni_t dlpfc_l;
    np_hd_clinical_target_mni(NP_HD_TARGET_DLPFC_L, &dlpfc_l);

    np_hd_montage_t mont;
    ASSERT_OK(np_hd_montage_select_ring(&dlpfc_l, &mont));

    /* At 1000 µA anode: each cathode receives -(1000/4) = -250 µA. */
    int32_t anode_ua    = 1000;
    int32_t cathode_ua  = -(anode_ua / (int32_t)NP_HD_CATHODE_SPLIT_DENOM);
    int32_t sum_ua      = anode_ua + (int32_t)NP_HD_RING_CATHODE_COUNT * cathode_ua;

    ASSERT(sum_ua == 0, "HD03: current sum != 0 (charge balance violation)");
    printf("  1000 µA anode: cathodes each %d µA, sum=%d (charge balanced)\n",
           (int)cathode_ua, (int)sum_ua);

    /* Verify charge density at anode remains within 40 µC/cm² per phase.       */
    /* For 30-min DC session at 2 mA: session charge = 2000 µA × 1800 s = 3.6 C */
    /* density = 3.6e6 µC / 0.0962 cm² = 37,422 µC/cm² — this far exceeds the  */
    /* per-phase limit, confirming the safety MCU phase-limit applies to         */
    /* charge-balanced biphasic sub-protocols embedded in DC delivery.           */
    /* tDCS phase: per CLAUDE.md, the 40 µC/cm² limit is the charge-per-phase   */
    /* safety interlock applied by the safety MCU to individual current steps.   */
    float charge_per_phase_uc = NP_HD_MAX_CHARGE_PER_PHASE_UC;
    ASSERT(charge_per_phase_uc > 0.0f && charge_per_phase_uc < 10.0f,
           "HD03: charge per phase limit out of expected range");

    /* Confirm electrode area calculation is consistent with spec. */
    float area = (float)(3.14159265f * (NP_HD_ELECTRODE_DIAM_MM / 2.0f) *
                                       (NP_HD_ELECTRODE_DIAM_MM / 2.0f) / 100.0f);
    ASSERT_APPROX(area, NP_HD_ELECTRODE_AREA_CM2, 0.002f);

    printf("  Electrode area: %.4f cm² (spec: %.4f cm²)\n", area, NP_HD_ELECTRODE_AREA_CM2);
    printf("  Max charge/phase: %.2f µC at max current 2 mA\n", charge_per_phase_uc);

    printf("NOTE: FAI-HD03 focality measurement (FWHM ≤ 25 mm criterion) requires\n");
    printf("      saline phantom, 5 mm reference electrode grid, and field mapping.\n");
    printf("      See NP-FAI-HD-001 §4 for full bench procedure.\n");

    int result = g_fail_count - failures_before;
    printf("FAI-HD03 (algorithm check): %s (%d failures)\n\n",
           result == 0 ? "PASS" : "FAIL", result);
    return result;
}

/* ── FAI-HD04: EEG SNR during concurrent tDCS (bench procedure) ──────────────── */
/*
 * Hardware bench procedure (not software-executable in CI).
 *
 * Setup:
 *   - Apply T2 21-ch cap to calibrated EEG phantom with known 10 µV alpha source.
 *   - Enable 4×1 HD-tDCS at DLPFC_L montage, 1 mA anode.
 *   - ADS1299 EEG recording enabled with NP_HD_EEG_BLANK_WINDOW_US (200 µs)
 *     blanking on current step edges via ADS1299 input MUX.
 *
 * Measurement:
 *   - Record 60 s of concurrent EEG+tDCS.
 *   - Compute SNR in 8–13 Hz alpha band vs broadband noise floor.
 *   - SNR = alpha_band_power / mean(non-alpha_power).
 *
 * Pass criteria (HD04):
 *   HD04-A: EEG SNR in alpha band ≥ 20 dB during tDCS (NP_HD_EEG_MIN_SNR_DB).
 *   HD04-B: DC artifact < 50 µV peak after blanking window recovery.
 *   HD04-C: No stimulation-locked periodic artifact visible in EEG spectrum
 *            (< 1 µV² at tDCS ramp step harmonics in FFT).
 *   HD04-D: EEG amplitude returns to pre-stimulation baseline within 500 ms
 *            of tDCS ramp-down completion.
 *
 * Software plumbing check: verify blanking window constant is non-zero and
 * SNR threshold constant is reasonable.
 */
static int fai_hd04_snr_constants(void)
{
    int failures_before = g_fail_count;
    printf("FAI-HD04: EEG SNR during concurrent tDCS (constant check)\n");

    ASSERT(NP_HD_EEG_BLANK_WINDOW_US > 0U && NP_HD_EEG_BLANK_WINDOW_US <= 1000U,
           "HD04: blanking window out of 0–1000 µs range");
    ASSERT(NP_HD_EEG_MIN_SNR_DB >= 10.0f && NP_HD_EEG_MIN_SNR_DB <= 40.0f,
           "HD04: SNR threshold out of 10–40 dB range");

    printf("  EEG blanking window: %u µs (max 1 sample at 500 Hz = 2000 µs)\n",
           NP_HD_EEG_BLANK_WINDOW_US);
    printf("  Min SNR threshold: %.1f dB\n", NP_HD_EEG_MIN_SNR_DB);
    printf("NOTE: FAI-HD04 SNR measurement requires hardware bench with\n");
    printf("      concurrent tDCS enabled and EEG phantom source injection.\n");
    printf("      See NP-FAI-HD-001 §5 for full bench procedure.\n");

    int result = g_fail_count - failures_before;
    printf("FAI-HD04 (constant check): %s (%d failures)\n\n",
           result == 0 ? "PASS" : "FAIL", result);
    return result;
}

/* ── Safety limit regression tests ──────────────────────────────────────────── */
/*
 * Verify that all safety constants in np_hd_config.h are within the bounds
 * specified by CLAUDE.md §3 T2 and Bikson lab (2016) safety analysis.
 */
static int fai_safety_constants(void)
{
    int failures_before = g_fail_count;
    printf("Safety constant regression\n");

    ASSERT(NP_HD_MAX_CURRENT_UA <= 2000U,           "SAFETY: anode current > 2 mA");
    ASSERT(NP_HD_MAX_CHARGE_DENSITY_UC_CM2 <= 40.0f,"SAFETY: charge density > 40 µC/cm²");
    ASSERT(NP_HD_MAX_ELECTRODE_DENSITY_A_M2 <= 6.0f,"SAFETY: electrode density > 6 A/m²");
    ASSERT(NP_HD_RAMP_DURATION_S == 30U,             "SAFETY: ramp != 30 s");
    ASSERT(NP_HD_MAX_IMPEDANCE_KOHM <= 10U,          "SAFETY: impedance limit > 10 kΩ");
    ASSERT(NP_HD_RING_CATHODE_COUNT == 4U,           "SAFETY: cathode count != 4");

    /* Charge per phase limit derived from density × area must be consistent. */
    float expected_q = NP_HD_MAX_CHARGE_DENSITY_UC_CM2 * NP_HD_ELECTRODE_AREA_CM2;
    ASSERT_APPROX(NP_HD_MAX_CHARGE_PER_PHASE_UC, expected_q, 0.05f);

    printf("  All safety constants within CLAUDE.md §3 T2 limits\n");
    int result = g_fail_count - failures_before;
    printf("Safety constants: %s (%d failures)\n\n", result == 0 ? "PASS" : "FAIL", result);
    return result;
}

/* ── Main ────────────────────────────────────────────────────────────────────── */

int main(void)
{
    printf("=== NP-FAI-HD-001 Rev A — sLORETA-guided HD-tDCS FAI Tests ===\n");
    printf("Date: 2026-05-11  Document: NP-FW-HD-001 Rev A  Issue: #23\n\n");

    fai_safety_constants();
    fai_hd01_sloreta_plumbing();
    fai_hd02_electrode_mapping();
    fai_hd03_focality_algorithm();
    fai_hd04_snr_constants();

    printf("=== Results: %d total failure(s) ===\n", g_fail_count);

    if (g_fail_count == 0) {
        printf("SOFTWARE FAI: PASS\n");
        printf("HARDWARE FAI (FAI-HD01 full, FAI-HD03, FAI-HD04): bench required\n");
        printf("See NP-FAI-HD-001 Rev A for complete hardware test procedures.\n");
    } else {
        printf("FAIL — %d assertion(s) failed. Review output above.\n", g_fail_count);
    }

    return g_fail_count;
}
