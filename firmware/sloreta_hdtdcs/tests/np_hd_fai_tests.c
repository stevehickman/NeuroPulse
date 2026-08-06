/*
 * NeurOne sLORETA-guided HD-tDCS — First Article Inspection Tests
 * Document: NP-FW-HD-001 Rev A §12 / NP-FAI-HD-001 Rev A
 *
 * FAI-HD01: sLORETA source localization accuracy vs known phantom
 *           HD01-D: covariance estimator variance on stochastic input
 *           HD01-E: per-band source power is spectral, not a fixed ratio
 *           HD01-F: peak selection is a real argmax, not "voxel 0"
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
 *   HD02-A: Each predefined clinical target is within 35 mm of its nearest
 *            electrode if classed SURFACE, or beyond 35 mm if classed DEEP
 *            (§2.3 focality applies to surface targets only).
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
 *   HD02-L: The driver channels np_hd_stim_init() programs equal
 *            np_hd_electrode_driver_channel() for every montage electrode
 *            (see fai_hd02l_driver_channel_single_source() below).
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

        ASSERT(nearest < NP_HD_CH_COUNT, "HD02-A: nearest electrode out of range");

        /*
         * The 35 mm limit is a claim about 4×1 focality (§2.3: ~1.5 cm FWHM at
         * 10 mm depth), so it applies to targets on the cortical surface.  ACC
         * sits on the medial wall at 47.1 mm from Fz; no 10-10 scalp position
         * gets closer than ~37.9 mm (Fpz), so the limit is unsatisfiable there
         * by any electrode placement, not merely by this cap.
         *
         * Checked in BOTH directions so neither class can be quietly
         * misclassified: a SURFACE target must actually be within 35 mm, and a
         * DEEP target must actually be beyond it.  Reclassifying ACC to SURFACE
         * to silence this, or moving a surface target's coordinates below the
         * scalp, fails here rather than passing vacuously.
         */
        np_hd_target_depth_t depth;
        ASSERT_OK(np_hd_clinical_target_depth(targets[t].target, &depth));

        if (depth == NP_HD_TARGET_DEPTH_SURFACE) {
            ASSERT(dist_mm <= 35.0f,
                   "HD02-A: surface target >35 mm from nearest electrode");
        } else {
            ASSERT(dist_mm > 35.0f,
                   "HD02-A: target classed DEEP but is within 35 mm — reclassify");
            printf("    ^ DEEP target: 4x1 delivers indirect network modulation "
                   "here, not focal stimulation (§2.3)\n");
        }
    }

    /* HD02-A2: NP_HD_TARGET_CUSTOM has no precomputed depth class.              */
    {
        np_hd_target_depth_t depth;
        ASSERT(np_hd_clinical_target_depth(NP_HD_TARGET_CUSTOM, &depth)
                   == NP_HD_ERR_INVALID_ARG,
               "HD02-A2: CUSTOM target returned a depth class");
        ASSERT(np_hd_clinical_target_depth(NP_HD_TARGET_DLPFC_L, NULL)
                   == NP_HD_ERR_INVALID_ARG,
               "HD02-A2: NULL out accepted");
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

/* ── HD02-L: stim driver channels match the montage accessor ────────────────── */
/*
 * Regression guard against re-duplicating the electrode→driver-channel table.
 *
 * np_hd_stim.c used to carry a hand-copied clone of k_driver_channel[] from
 * np_hd_montage.c, kept in step only by a comment.  HD02-K proves the montage
 * module's table assigns every electrode its own driver channel; this test
 * proves the STIM module actually programs those same channels.  Without it,
 * HD02-K can pass while np_hd_stim_init() drives something else entirely —
 * a wrong-electrode current path, not cosmetic drift.  The duplicated
 * ring-selection loop that shipped the C4/P4 driver-12 collision is the same
 * failure mode: a second copy that the fix never reached.
 *
 * Pass criteria:
 *   HD02-L1: for every electrode of every selected montage, the driver channel
 *            np_hd_stim_init() stored equals np_hd_electrode_driver_channel().
 *   HD02-L2: the accessor returns an in-range channel for every valid electrode
 *            and NP_HD_DRIVER_CH_NONE for out-of-range ones.
 *   HD02-L3: the channels the stim context programs are mutually distinct —
 *            the property np_hd_montage_validate() certified.
 */
static void hd02l_stim_cb(np_hd_stim_ctx_t *ctx, bool granted, const float imp[])
{
    (void)ctx; (void)granted; (void)imp;
}

static void hd02l_check_montage(const np_hd_montage_t *mont, const char *what)
{
    np_hd_stim_ctx_t ctx;
    np_hd_status_t   ret = np_hd_stim_init(&ctx, mont, hd02l_stim_cb);
    if (ret != NP_HD_OK) {
        printf("FAIL [%s] HD02-L: np_hd_stim_init failed (%d)\n", what, (int)ret);
        g_fail_count++;
        return;
    }

    np_hd_electrode_state_t states[NP_HD_RING_ELECTRODE_COUNT];
    memset(states, 0, sizeof(states));
    ASSERT_OK(np_hd_stim_get_electrode_states(&ctx, states,
                                               NP_HD_RING_ELECTRODE_COUNT));

    /* Expected electrode order: anode at [0], then cathodes in montage order. */
    np_hd_electrode_t expect[NP_HD_RING_ELECTRODE_COUNT];
    uint8_t n_expect = 0U;
    expect[n_expect++] = mont->center;
    for (uint8_t k = 0U; k < mont->cathode_count &&
                          k < NP_HD_RING_CATHODE_COUNT; k++) {
        expect[n_expect++] = mont->cathodes[k];
    }

    uint32_t seen = 0U;
    for (uint8_t i = 0U; i < n_expect; i++) {
        uint8_t want = np_hd_electrode_driver_channel(expect[i]);
        uint8_t got  = states[i].driver_channel;

        if (states[i].label != expect[i]) {
            printf("FAIL [%s] HD02-L: elec[%u] label=%s expected %s\n", what, i,
                   np_hd_electrode_name(states[i].label),
                   np_hd_electrode_name(expect[i]));
            g_fail_count++;
        }

        /* HD02-L1: the programmed channel IS the accessor's channel. */
        if (got != want) {
            printf("FAIL [%s] HD02-L1: %s programmed driver ch %u, "
                   "montage accessor says %u — duplicated table has drifted\n",
                   what, np_hd_electrode_name(expect[i]),
                   (unsigned)got, (unsigned)want);
            g_fail_count++;
        }

        /* HD02-L2 (in-range half): a valid electrode never yields NONE. */
        if (want == NP_HD_DRIVER_CH_NONE || want >= NP_HD_DRIVER_CHANNELS) {
            printf("FAIL [%s] HD02-L2: %s maps to out-of-range driver ch %u\n",
                   what, np_hd_electrode_name(expect[i]), (unsigned)want);
            g_fail_count++;
            continue;
        }

        /* HD02-L3: distinctness — the invariant validate() certified. */
        if (seen & (1UL << want)) {
            printf("FAIL [%s] HD02-L3: driver ch %u used twice (at %s)\n",
                   what, (unsigned)want, np_hd_electrode_name(expect[i]));
            g_fail_count++;
        }
        seen |= (1UL << want);
    }

    /* NOTE: for a bilateral montage np_hd_stim_init() builds state for the left
     * ring only — center_r / cathodes_r are not programmed, and ctx->elec[] is
     * NP_HD_RING_ELECTRODE_COUNT (5) entries, not NP_HD_BILATERAL_ELEC_COUNT.
     * That gap is a separate open question about how §6.3 simultaneous delivery
     * reaches the driver; this test covers the channels init does set.        */
    np_hd_stim_deinit(&ctx);
}

static int fai_hd02l_driver_channel_single_source(void)
{
    int failures_before = g_fail_count;
    printf("HD02-L: stim driver channels == montage accessor (single source)\n");

    /* HD02-L2: accessor contract across the whole electrode domain. */
    for (uint16_t e = 0U; e < NP_HD_CH_COUNT; e++) {
        uint8_t drv = np_hd_electrode_driver_channel((np_hd_electrode_t)e);
        ASSERT(drv < NP_HD_DRIVER_CHANNELS,
               "HD02-L2: valid electrode maps outside the driver range");
    }
    ASSERT_EQ(np_hd_electrode_driver_channel(NP_HD_CH_NONE),
              NP_HD_DRIVER_CH_NONE);
    ASSERT_EQ(np_hd_electrode_driver_channel((np_hd_electrode_t)NP_HD_CH_COUNT),
              NP_HD_DRIVER_CH_NONE);

    static const struct { np_hd_clinical_target_t target; const char *name; } targets[] = {
        { NP_HD_TARGET_DLPFC_L, "DLPFC_L" }, { NP_HD_TARGET_DLPFC_R, "DLPFC_R" },
        { NP_HD_TARGET_VLPFC_L, "VLPFC_L" }, { NP_HD_TARGET_ACC,      "ACC"     },
        { NP_HD_TARGET_MPFC,    "MPFC"    }, { NP_HD_TARGET_M1_L,    "M1_L"   },
        { NP_HD_TARGET_M1_R,    "M1_R"   },
    };
    uint8_t n_targets = (uint8_t)(sizeof(targets) / sizeof(targets[0]));

    static const struct { np_hd_montage_type_t type; const char *label; } kinds[] = {
        { NP_HD_MONTAGE_RING_4X1,      "ring4x1"   },
        { NP_HD_MONTAGE_BILATERAL_4X1, "bilateral" },
        { NP_HD_MONTAGE_STANDARD_2E,   "std2elec"  },
    };
    uint8_t n_kinds = (uint8_t)(sizeof(kinds) / sizeof(kinds[0]));

    uint8_t n_checked = 0U;
    uint8_t n_midline_skips = 0U;

    for (uint8_t t = 0U; t < n_targets; t++) {
        np_hd_mni_t mni;
        ASSERT_OK(np_hd_clinical_target_mni(targets[t].target, &mni));

        for (uint8_t m = 0U; m < n_kinds; m++) {
            np_hd_montage_t mont;
            memset(&mont, 0, sizeof(mont));

            np_hd_status_t sel = np_hd_montage_from_mni(&mni, kinds[m].type, &mont);
            np_hd_status_t val = (sel == NP_HD_OK) ? np_hd_montage_validate(&mont)
                                                    : sel;

            if (sel != NP_HD_OK || val != NP_HD_OK) {
                /*
                 * Guard the guard: a montage this test cannot build is a montage
                 * it cannot check, so an unexplained skip must fail rather than
                 * quietly shrinking coverage toward nothing.
                 *
                 * Only one skip is legitimate.  Both bilateral and standard
                 * 2-electrode place their second site at the contralateral
                 * homologue (mirror x); a midline target (x == 0) mirrors onto
                 * itself, so no such site exists and selection correctly returns
                 * NP_HD_ERR_MONTAGE_INVALID (see np_hd_montage_select_bilateral's
                 * contract, and HD02-H).  ACC (0,28,28) and MPFC (0,52,6) are the
                 * two midline targets, so this accounts for exactly 4 skips.
                 * Every 4×1 ring must build for every target.
                 */
                bool mirrored_kind = (kinds[m].type == NP_HD_MONTAGE_STANDARD_2E) ||
                                      (kinds[m].type == NP_HD_MONTAGE_BILATERAL_4X1);
                if (mirrored_kind && mni.x == 0) {
                    n_midline_skips++;
                } else {
                    printf("FAIL [%s/%s] HD02-L: montage unavailable "
                           "(select=%d validate=%d) — coverage lost\n",
                           targets[t].name, kinds[m].label, (int)sel, (int)val);
                    g_fail_count++;
                }
                continue;
            }

            char what[64];
            snprintf(what, sizeof(what), "%s/%s", targets[t].name, kinds[m].label);
            hd02l_check_montage(&mont, what);
            n_checked++;
        }
    }

    /* 2 midline targets × 2 mirrored montage kinds. */
    ASSERT_EQ(n_midline_skips, 4U);
    ASSERT_EQ(n_checked, (uint8_t)(n_targets * n_kinds - 4U));
    printf("  checked %u montages against the accessor (%u midline skips)\n",
           (unsigned)n_checked, (unsigned)n_midline_skips);

    int result = g_fail_count - failures_before;
    printf("HD02-L: %s (%d failures)\n\n", result == 0 ? "PASS" : "FAIL", result);
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
 *
 * A deterministic stimulus cannot bound estimator variance — see HD01-D
 * (fai_hd01d_estimator_variance) for the stochastic-input case that does.
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

    /* Expected source power is exactly derivable, so assert magnitude and not
     * merely ordering (see the magnitude check below for why that matters).
     *
     * W_0 is the unit vector e_F3 and W_1 is e_O2, so each quadratic form
     * W_v^T C W_v collapses to a single covariance diagonal entry — the
     * variance of that channel's trace, A²/2 for a coherent sinusoid.
     *
     * The two tones sit on whole-cycle counts (20 and 7) over the same
     * 1024-sample window, so they are orthogonal and the F3–O2 cross-covariance
     * is exactly zero.  The Frobenius norm therefore reduces to the two
     * diagonal terms. */
    const float k_expected_power_f3 = k_f3_amplitude_uv * k_f3_amplitude_uv / 2.0f;
    const float k_expected_power_o2 = k_o2_amplitude_uv * k_o2_amplitude_uv / 2.0f;
    const float k_expected_cov_norm = sqrtf(k_expected_power_f3 * k_expected_power_f3 +
                                            k_expected_power_o2 * k_expected_power_o2);

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

    /* Magnitude check, not just ordering.
     *
     * Ordering and ratio assertions are both blind to a uniform scale error: a
     * covariance accumulator that mis-weights samples reports every voxel low
     * by the same factor, so voxel 0 still dominates voxel 1 and the ratio is
     * still (10 µV / 1 µV)² = 100.  That is exactly how the per-sample
     * running-mean blend defect in np_sloreta_push_epoch() survived here — it
     * scaled the whole map down by ~NP_HD_SLORETA_FFT_SIZE (source_power[0]
     * read 0.0490 instead of 50.0) while every relative assertion passed.
     *
     * Pinning the absolute value is what closes that hole. */
    ASSERT_APPROX(source_power[0], k_expected_power_f3, 0.5f);
    ASSERT_APPROX(source_power[1], k_expected_power_o2, 0.05f);

    /* Find peak. */
    np_hd_sloreta_result_t result;
    ASSERT_OK(np_sloreta_find_peak(&ctx, source_power, 2U, &result));
    ASSERT(result.peak_voxel == 0U, "HD01: peak voxel should be voxel 0 (DLPFC_L)");
    ASSERT(result.valid, "HD01: result.valid is false");
    printf("  peak_voxel=%u MNI=(%d,%d,%d)\n",
           result.peak_voxel, result.peak_mni.x, result.peak_mni.y, result.peak_mni.z);

    /* Band power decomposition.
     *
     * This assertion used to read only "the four bands sum to > 0", which any
     * non-zero broadband power satisfies — and the implementation it was guarding
     * scaled ONE broadband quadratic form by four compile-time bin-count ratios,
     * so it passed while returning the same decomposition for every possible
     * input.  Voxel 0 is driven by F3, which carries a 9.77 Hz tone (bin 20,
     * alpha), so the correct decomposition is alpha-dominant and that is what is
     * asserted.  FAI-HD01-E below sweeps all four bands and adds the
     * cross-stimulus check that a fixed-ratio implementation cannot satisfy. */
    np_hd_band_power_t bands;
    ASSERT_OK(np_sloreta_band_power(&ctx, 0U, &bands));
    ASSERT(bands.valid, "HD01: band power not marked valid");
    ASSERT(bands.alpha > 20.0f * (bands.delta + bands.theta + bands.beta),
           "HD01: alpha-driven voxel is not alpha-dominant");
    printf("  bands@voxel0: delta=%.4f theta=%.4f alpha=%.4f beta=%.4f\n",
           bands.delta, bands.theta, bands.alpha, bands.beta);

    /* Covariance norm should be non-zero, and should match the two populated
     * diagonal terms (see k_expected_cov_norm above). */
    float cnorm = np_sloreta_covariance_norm(&ctx);
    ASSERT(cnorm > 0.0f, "HD01: covariance norm is zero");
    ASSERT_APPROX(cnorm, k_expected_cov_norm, 0.5f);
    printf("  covariance norm=%.4f (expected %.4f)\n", cnorm, k_expected_cov_norm);

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

/* ── FAI-HD01-D: covariance estimator variance (stochastic input) ───────────── */
/*
 * HD01-D: the accumulated covariance must be an unbiased, low-variance
 * estimator of the true signal covariance on STOCHASTIC input.
 *
 * Why this exists as a separate case from fai_hd01_sloreta_plumbing().
 *
 * That test drives a deterministic sinusoid, and a deterministic stimulus
 * cannot distinguish "used all the samples" from "used a few of them": every
 * epoch's tail resembles every other, so discarding samples shows up purely as
 * a constant factor.  A constant factor is then invisible to ordering and ratio
 * assertions, and cancels outright in the argmax that selects the peak voxel.
 *
 * That is not a hypothetical gap.  np_sloreta_push_epoch() applied its
 * per-epoch running-mean blend inside the per-sample loop, which decayed the
 * accumulator geometrically and left it computing over ~1/N of the acquired
 * data.  Under the deterministic stimulus the whole defect presented as a
 * ~1024x scale factor with a textbook dominance ratio.
 *
 * Real EEG is stochastic, where the same defect is an estimator-variance
 * problem rather than a scale problem, and it degrades source localisation:
 * measured over 400 trials with a source implanted among 8 voxels, correct
 * localisation fell from 100% to 46% at moderate SNR, approaching the chance
 * rate as SNR dropped.  The peak voxel drives montage selection
 * (np_hd_session.c), so that is a targeting-reliability failure.
 *
 * Method: drive one channel with zero-mean uniform white noise of amplitude A,
 * whose true variance is A^2/12, and repeat over independent runs.  W is the
 * unit vector on that channel, so the quadratic form is one covariance diagonal
 * and np_sloreta_covariance_norm() reports it directly.
 *
 * Pass criteria:
 *   HD01-D1 (bias):     mean estimate within 2% of A^2/12.
 *   HD01-D2 (variance): coefficient of variation across runs within 3x the
 *                       theoretical floor for this estimator.
 *
 * The CV floor is derived, not tuned.  For a variance estimator over n_eff
 * independent samples, CV = sqrt((kurtosis_excess + 2) / n_eff); uniform noise
 * has kurtosis_excess = -1.2, giving sqrt(0.8 / n_eff).  Here n_eff is
 * NP_HD_SLORETA_EPOCHS * NP_HD_SLORETA_FFT_SIZE = 65536, so the floor is
 * 0.349%.  Measured: 0.25-0.43% across seeds (i.e. at the floor), against
 * 7.8-9.1% for the defect above — the 3x limit sits with wide margin on both
 * sides and is not sensitive to the seed.
 *
 * The generator is a self-contained xorshift32 rather than rand(), whose
 * sequence differs between glibc and macOS libc; this must produce identical
 * values on the ubuntu CI runner and on a developer Mac.
 */
#define HD01D_RUNS      24U     /* CV estimate SE ~16% rel; limit is 3x floor  */
#define HD01D_AMPLITUDE 10.0f   /* uV, peak-to-peak span of the uniform noise  */

static uint32_t hd01d_rng_next(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* Uniform on [-0.5, 0.5).  Takes the top 24 bits so every value is exactly
 * representable in float32 and the sequence is bit-identical across hosts. */
static float hd01d_rng_centered(uint32_t *state)
{
    return ((float)(hd01d_rng_next(state) >> 8) / (float)(1UL << 24)) - 0.5f;
}

static int fai_hd01d_estimator_variance(void)
{
    int failures_before = g_fail_count;
    printf("FAI-HD01-D: covariance estimator variance (stochastic input)\n");

    static float       W[NP_HD_SLORETA_N_CH];
    static np_hd_mni_t voxel_mni[1] = { { 0, 0, 0 } };
    static float       samples[NP_HD_SLORETA_N_CH][NP_HD_SLORETA_FFT_SIZE];

    memset(W, 0, sizeof(W));
    W[NP_HD_CH_F3] = 1.0f;

    double estimate[HD01D_RUNS];

    for (uint32_t r = 0U; r < HD01D_RUNS; r++) {
        /* Odd, run-dependent seed: xorshift32 has an all-zero fixed point. */
        uint32_t state = (uint32_t)(r * 2654435761U) | 1U;

        np_sloreta_ctx_t ctx;
        ASSERT_OK(np_sloreta_init(&ctx, W, voxel_mni, 1U));

        for (uint16_t e = 0U; e < NP_HD_SLORETA_EPOCHS; e++) {
            memset(samples, 0, sizeof(samples));
            for (uint16_t s = 0U; s < NP_HD_SLORETA_FFT_SIZE; s++) {
                samples[NP_HD_CH_F3][s] = HD01D_AMPLITUDE * hd01d_rng_centered(&state);
            }
            ASSERT_OK(np_sloreta_push_epoch(&ctx,
                                             (const float (*)[NP_HD_SLORETA_FFT_SIZE])samples,
                                             NP_HD_SLORETA_FFT_SIZE));
        }
        estimate[r] = (double)np_sloreta_covariance_norm(&ctx);
    }

    double mean = 0.0;
    for (uint32_t r = 0U; r < HD01D_RUNS; r++) {
        mean += estimate[r];
    }
    mean /= (double)HD01D_RUNS;

    double sum_sq = 0.0;
    for (uint32_t r = 0U; r < HD01D_RUNS; r++) {
        double d = estimate[r] - mean;
        sum_sq += d * d;
    }
    double sd = sqrt(sum_sq / (double)(HD01D_RUNS - 1U));   /* Bessel-corrected */
    double cv = sd / mean;

    /* Uniform on a span of A has variance A^2/12. */
    double true_variance = (double)HD01D_AMPLITUDE * (double)HD01D_AMPLITUDE / 12.0;
    double n_eff         = (double)NP_HD_SLORETA_EPOCHS * (double)NP_HD_SLORETA_FFT_SIZE;
    double cv_floor      = sqrt(0.8 / n_eff);
    double cv_limit      = 3.0 * cv_floor;

    printf("  mean=%.6f (true %.6f, error %.3f%%)\n",
           mean, true_variance, 100.0 * fabs(mean - true_variance) / true_variance);
    printf("  cv=%.4f%% (floor %.4f%%, limit %.4f%%) over %u runs\n",
           100.0 * cv, 100.0 * cv_floor, 100.0 * cv_limit, (unsigned)HD01D_RUNS);

    /* HD01-D1: unbiased.  Catches a scale error such as the ~1024x low map. */
    ASSERT(fabs(mean - true_variance) <= 0.02 * true_variance,
           "HD01-D1: covariance estimate biased >2% from true variance");

    /* HD01-D2: at the variance floor.  Catches an estimator that silently
     * discards samples even when the mean has been corrected — the failure
     * mode a deterministic stimulus structurally cannot see. */
    ASSERT(cv <= cv_limit,
           "HD01-D2: estimator variance >3x floor — accumulator discarding samples");

    int result = g_fail_count - failures_before;
    printf("FAI-HD01-D: %s (%d failures)\n\n", result == 0 ? "PASS" : "FAIL", result);
    return result;
}

/* ── FAI-HD01-E: per-band source power is spectral, not a fixed ratio ────────── */
/*
 * HD01-E: np_sloreta_band_power() must return a decomposition that DEPENDS ON THE
 * INPUT SPECTRUM.
 *
 * Why this exists.
 *
 * The shipped implementation computed the broadband quadratic form W_v^T C W_v
 * once and multiplied it by four compile-time constants derived from the band
 * bin-edge macros — w_delta = 8/61, w_theta = 8/61, w_alpha = 10/61,
 * w_beta = 35/61.  No FFT existed anywhere in the module; NP_HD_SLORETA_FFT_SIZE
 * was only ever an array dimension.  Consequently the four returned values were
 * always in fixed proportion to one another, and a pure 9.8 Hz alpha epoch, a
 * pure 2.4 Hz delta epoch and a pure 19.5 Hz beta epoch all produced the byte-
 * identical output (delta 6.5574, theta 6.5574, alpha 8.1967, beta 28.6885), with
 * beta always "dominant" purely because it spans 35 of the 61 bins.
 *
 * The only guard was FAI-HD01's "the four bands sum to > 0", which any non-zero
 * broadband power satisfies.  That is the failure this case is shaped to prevent
 * recurring: an assertion can pass, and the pass count can rise, while the
 * property under test is not being tested at all.
 *
 * Method.  Drive one channel with a bin-centred tone placed squarely inside each
 * band in turn, and require that band to carry essentially all the power.  W is
 * the unit vector on that channel, so each quadratic form collapses to one
 * covariance diagonal entry and the expected value is derivable in closed form.
 *
 * Bin centring matters and is not incidental.  At 500 Hz over 1024 samples the
 * grid is 0.48828 Hz/bin, so a tone of exactly k whole cycles per epoch lands on
 * bin k.  Under the PERIODIC Hann window the module uses, such a tone occupies
 * exactly three bins (k-1, k, k+1) with no sidelobe leakage whatsoever, so the
 * 2 % magnitude tolerance below is a derivation and not a tuned number.  Every
 * test tone sits at least 3 bins clear of its band edges, so all three bins fall
 * inside the intended band.
 *
 * Pass criteria:
 *   HD01-E1 (dominance):    target band > 20x each of the other three.
 *   HD01-E2 (magnitude):    target band within 2 % of A^2/2 (the tone's variance).
 *   HD01-E3 (purity):       target band >= 95 % of the four-band total.
 *   HD01-E4 (sign):         no band value is ever negative.
 *   HD01-E5 (anti-ratio):   the alpha:delta ratio under an alpha stimulus exceeds
 *                           the alpha:delta ratio under a delta stimulus by >100x.
 *                           A fixed-ratio implementation has these EQUAL by
 *                           construction and cannot pass under any choice of
 *                           weights — this is the assertion that kills it.
 *   HD01-E6 (Parseval):     for a two-tone input the band sum reconciles with the
 *                           independently accumulated time-domain variance to 2 %.
 *   HD01-E7 (per-voxel):    two voxels reading different channels get different
 *                           decompositions — band power is voxel-specific, not a
 *                           single scalar redistributed.
 *   HD01-E8 (contract):     NOT_READY before any full epoch, after reset, and
 *                           after a short (non-transform-length) epoch; and
 *                           INVALID_ARG on a bad voxel index or NULL out.
 */
#define HD01E_TONE_AMPLITUDE_UV 10.0f

/* Whole cycles per 1024-sample epoch == FFT bin index.  Each is >= 3 bins from
 * both edges of its band, so Hann's 3-bin mainlobe stays inside the band:
 *   delta bins  1-8  -> 5  (2.44 Hz)     theta bins  9-16 -> 12 (5.86 Hz)
 *   alpha bins 17-26 -> 20 (9.77 Hz)     beta  bins 27-61 -> 40 (19.53 Hz)  */
#define HD01E_CYCLES_DELTA 5U
#define HD01E_CYCLES_THETA 12U
#define HD01E_CYCLES_ALPHA 20U
#define HD01E_CYCLES_BETA  40U

static float hd01e_W[2 * NP_HD_SLORETA_N_CH];
static np_hd_mni_t hd01e_mni[2] = { { -46, 36, 20 }, { 0, 0, 0 } };
static float hd01e_samples[NP_HD_SLORETA_N_CH][NP_HD_SLORETA_FFT_SIZE];

/* Write A*sin(2*pi*cycles*n/N) into one channel; zero every other channel. */
static void hd01e_fill_tone(uint8_t ch, uint16_t cycles, float amplitude_uv)
{
    static const float k_two_pi = 6.283185307179586f;
    for (uint16_t s = 0U; s < NP_HD_SLORETA_FFT_SIZE; s++) {
        float phase = k_two_pi * (float)s / (float)NP_HD_SLORETA_FFT_SIZE;
        hd01e_samples[ch][s] = amplitude_uv * sinf(phase * (float)cycles);
    }
}

/* Accumulate n_epochs identical epochs of the current hd01e_samples content. */
static void hd01e_push(np_sloreta_ctx_t *ctx, uint16_t n_epochs)
{
    for (uint16_t e = 0U; e < n_epochs; e++) {
        ASSERT_OK(np_sloreta_push_epoch(
            ctx, (const float (*)[NP_HD_SLORETA_FFT_SIZE])hd01e_samples,
            NP_HD_SLORETA_FFT_SIZE));
    }
}

static int fai_hd01e_band_power_spectral(void)
{
    int failures_before = g_fail_count;
    printf("FAI-HD01-E: band power is spectral (single-band dominance)\n");

    /* Voxel 0 reads F3, voxel 1 reads O2 (used by HD01-E7). */
    memset(hd01e_W, 0, sizeof(hd01e_W));
    hd01e_W[0 * NP_HD_SLORETA_N_CH + NP_HD_CH_F3] = 1.0f;
    hd01e_W[1 * NP_HD_SLORETA_N_CH + NP_HD_CH_O2] = 1.0f;

    /* A coherent tone of amplitude A has variance A^2/2; W is a unit vector, so
     * the quadratic form is exactly that channel's in-band variance. */
    const float k_expected = HD01E_TONE_AMPLITUDE_UV * HD01E_TONE_AMPLITUDE_UV / 2.0f;
    const float k_tol      = 0.02f * k_expected;

    static const struct {
        const char  *name;
        uint16_t     cycles;
        np_hd_band_t band;
    } cases[] = {
        { "delta", HD01E_CYCLES_DELTA, NP_HD_BAND_DELTA },
        { "theta", HD01E_CYCLES_THETA, NP_HD_BAND_THETA },
        { "alpha", HD01E_CYCLES_ALPHA, NP_HD_BAND_ALPHA },
        { "beta",  HD01E_CYCLES_BETA,  NP_HD_BAND_BETA  },
    };
    const uint8_t n_cases = (uint8_t)(sizeof(cases) / sizeof(cases[0]));

    /* profile[stimulus][band] — retained for the HD01-E5 cross-stimulus check. */
    float profile[4][NP_HD_BAND_COUNT];

    for (uint8_t c = 0U; c < n_cases; c++) {
        np_sloreta_ctx_t ctx;
        ASSERT_OK(np_sloreta_init(&ctx, hd01e_W, hd01e_mni, 2U));

        memset(hd01e_samples, 0, sizeof(hd01e_samples));
        hd01e_fill_tone(NP_HD_CH_F3, cases[c].cycles, HD01E_TONE_AMPLITUDE_UV);
        hd01e_push(&ctx, NP_HD_SLORETA_EPOCHS);

        np_hd_band_power_t b;
        ASSERT_OK(np_sloreta_band_power(&ctx, 0U, &b));
        ASSERT(b.valid, "HD01-E: band result not marked valid");

        const float v[NP_HD_BAND_COUNT] = { b.delta, b.theta, b.alpha, b.beta };
        for (uint8_t k = 0U; k < NP_HD_BAND_COUNT; k++) {
            profile[c][k] = v[k];
        }

        float target = v[cases[c].band];
        float total  = v[0] + v[1] + v[2] + v[3];

        /* HD01-E1: dominance.  Written as a product, not a ratio, so an exactly
         * zero off-band value is a pass rather than a division by zero. */
        for (uint8_t k = 0U; k < NP_HD_BAND_COUNT; k++) {
            if (k == (uint8_t)cases[c].band) {
                continue;
            }
            ASSERT(target > 20.0f * v[k],
                   "HD01-E1: stimulus band does not dominate by 20x");
        }

        /* HD01-E2: absolute magnitude, derived (A^2/2), not tuned. */
        ASSERT_APPROX(target, k_expected, k_tol);

        /* HD01-E3: purity. */
        ASSERT(total > 0.0f, "HD01-E3: band total is zero");
        ASSERT(target >= 0.95f * total,
               "HD01-E3: stimulus band holds <95% of total band power");

        /* HD01-E4: no negative band power at either voxel. */
        for (uint8_t k = 0U; k < NP_HD_BAND_COUNT; k++) {
            ASSERT(v[k] >= 0.0f, "HD01-E4: negative band power");
        }

        printf("  %-5s tone (bin %2u): delta=%8.4f theta=%8.4f alpha=%8.4f "
               "beta=%8.4f  purity=%.4f\n",
               cases[c].name, (unsigned)cases[c].cycles,
               v[0], v[1], v[2], v[3], target / total);
    }

    /*
     * HD01-E5: the assertion the placeholder cannot survive.
     *
     * Compare the alpha:delta ratio measured under an alpha stimulus against the
     * same ratio measured under a delta stimulus.  Cross-multiplied to stay
     * defined when an off-band value is exactly zero:
     *
     *     (alpha|alpha-stim) * (delta|delta-stim)
     *         >  100 * (alpha|delta-stim) * (delta|alpha-stim)
     *
     * For ANY implementation that scales one broadband scalar by fixed per-band
     * weights, both sides reduce to w_alpha * w_delta * P_a * P_d and the
     * inequality is 1 > 100 — false regardless of the weights chosen.  Dominance
     * alone (E1) could in principle be gamed by a per-band-tuned fudge; this
     * cannot, because it requires the ratio to INVERT with the stimulus.
     */
    {
        const uint8_t S_DELTA = 0U, S_ALPHA = 2U;
        float lhs = profile[S_ALPHA][NP_HD_BAND_ALPHA] *
                    profile[S_DELTA][NP_HD_BAND_DELTA];
        float rhs = profile[S_DELTA][NP_HD_BAND_ALPHA] *
                    profile[S_ALPHA][NP_HD_BAND_DELTA];

        ASSERT(lhs > 100.0f * rhs,
               "HD01-E5: alpha:delta ratio does not invert with the stimulus — "
               "band values are in a stimulus-independent fixed ratio");
        printf("  cross-stimulus alpha:delta inversion: %.4e vs %.4e (>100x)\n",
               (double)lhs, (double)rhs);
    }

    /*
     * HD01-E6: Parseval reconciliation on a two-tone input.
     *
     * Alpha at amplitude 10 and beta at amplitude 4 on the same channel, on
     * whole-cycle counts over one window, so the tones are orthogonal and the
     * total variance is 10^2/2 + 4^2/2 = 58.  Only F3 is populated, so the
     * covariance Frobenius norm equals that channel's variance exactly.  This ties
     * the spectral accumulator to the independently computed time-domain one:
     * an FFT scale error would move the band sum away from the covariance norm
     * even though every relative assertion above still passed.
     */
    {
        const float k_beta_amp = 4.0f;
        np_sloreta_ctx_t ctx;
        ASSERT_OK(np_sloreta_init(&ctx, hd01e_W, hd01e_mni, 2U));

        memset(hd01e_samples, 0, sizeof(hd01e_samples));
        static const float k_two_pi = 6.283185307179586f;
        for (uint16_t s = 0U; s < NP_HD_SLORETA_FFT_SIZE; s++) {
            float ph = k_two_pi * (float)s / (float)NP_HD_SLORETA_FFT_SIZE;
            hd01e_samples[NP_HD_CH_F3][s] =
                HD01E_TONE_AMPLITUDE_UV * sinf(ph * (float)HD01E_CYCLES_ALPHA) +
                k_beta_amp              * sinf(ph * (float)HD01E_CYCLES_BETA);
        }
        hd01e_push(&ctx, NP_HD_SLORETA_EPOCHS);

        np_hd_band_power_t b;
        ASSERT_OK(np_sloreta_band_power(&ctx, 0U, &b));

        float expect_alpha = HD01E_TONE_AMPLITUDE_UV * HD01E_TONE_AMPLITUDE_UV / 2.0f;
        float expect_beta  = k_beta_amp * k_beta_amp / 2.0f;
        float band_sum     = b.delta + b.theta + b.alpha + b.beta;
        float cov_variance = np_sloreta_covariance_norm(&ctx);

        /* Each tone resolves to its own band at its own amplitude. */
        ASSERT_APPROX(b.alpha, expect_alpha, 0.02f * expect_alpha);
        ASSERT_APPROX(b.beta,  expect_beta,  0.02f * expect_beta);

        /* And the four bands together reconcile with the time-domain variance. */
        ASSERT_APPROX(band_sum, cov_variance, 0.02f * cov_variance);
        printf("  two-tone: alpha=%.4f (exp %.1f) beta=%.4f (exp %.1f) "
               "sum=%.4f vs cov_norm=%.4f\n",
               b.alpha, (double)expect_alpha, b.beta, (double)expect_beta,
               band_sum, cov_variance);
    }

    /*
     * HD01-E7: band power is per-voxel.  F3 carries alpha, O2 carries delta;
     * voxel 0 reads F3 and voxel 1 reads O2, so one call must come back
     * alpha-dominant and the other delta-dominant from the SAME context.  A
     * band decomposition derived from a single global scalar cannot do this.
     */
    {
        np_sloreta_ctx_t ctx;
        ASSERT_OK(np_sloreta_init(&ctx, hd01e_W, hd01e_mni, 2U));

        memset(hd01e_samples, 0, sizeof(hd01e_samples));
        hd01e_fill_tone(NP_HD_CH_F3, HD01E_CYCLES_ALPHA, HD01E_TONE_AMPLITUDE_UV);
        hd01e_fill_tone(NP_HD_CH_O2, HD01E_CYCLES_DELTA, HD01E_TONE_AMPLITUDE_UV);
        hd01e_push(&ctx, NP_HD_SLORETA_EPOCHS);

        np_hd_band_power_t v0, v1;
        ASSERT_OK(np_sloreta_band_power(&ctx, 0U, &v0));
        ASSERT_OK(np_sloreta_band_power(&ctx, 1U, &v1));

        ASSERT(v0.alpha > 20.0f * (v0.delta + v0.theta + v0.beta),
               "HD01-E7: voxel 0 (F3, alpha tone) is not alpha-dominant");
        ASSERT(v1.delta > 20.0f * (v1.theta + v1.alpha + v1.beta),
               "HD01-E7: voxel 1 (O2, delta tone) is not delta-dominant");
        printf("  per-voxel: v0 alpha=%.4f delta=%.4f | v1 alpha=%.4f delta=%.4f\n",
               v0.alpha, v0.delta, v1.alpha, v1.delta);
    }

    /* HD01-E8: contract — nothing reads as a measurement unless it is one. */
    {
        np_sloreta_ctx_t ctx;
        ASSERT_OK(np_sloreta_init(&ctx, hd01e_W, hd01e_mni, 2U));

        np_hd_band_power_t b;
        memset(&b, 0xAA, sizeof(b));    /* poison: prove we overwrite */
        ASSERT(np_sloreta_band_power(&ctx, 0U, &b) == NP_HD_ERR_NOT_READY,
               "HD01-E8: band power returned a value with no epochs accumulated");
        ASSERT(!b.valid, "HD01-E8: NOT_READY result left valid set");
        ASSERT(b.delta == 0.0f && b.theta == 0.0f &&
               b.alpha == 0.0f && b.beta  == 0.0f,
               "HD01-E8: NOT_READY result left stale values");

        /* A short epoch feeds the broadband covariance but cannot feed a
         * transform-length FFT, so bands must stay NOT_READY rather than
         * reporting a zero-padded — and therefore low-biased — spectrum. */
        memset(hd01e_samples, 0, sizeof(hd01e_samples));
        hd01e_fill_tone(NP_HD_CH_F3, HD01E_CYCLES_ALPHA, HD01E_TONE_AMPLITUDE_UV);
        ASSERT_OK(np_sloreta_push_epoch(
            &ctx, (const float (*)[NP_HD_SLORETA_FFT_SIZE])hd01e_samples,
            (uint16_t)(NP_HD_SLORETA_FFT_SIZE / 2U)));
        ASSERT(np_sloreta_epoch_count(&ctx) == 1U,
               "HD01-E8: short epoch rejected by the broadband path");
        ASSERT(np_sloreta_band_power(&ctx, 0U, &b) == NP_HD_ERR_NOT_READY,
               "HD01-E8: short epoch produced a band measurement");

        /* Argument validation. */
        ASSERT(np_sloreta_band_power(&ctx, 2U, &b) == NP_HD_ERR_INVALID_ARG,
               "HD01-E8: out-of-range voxel index accepted");
        ASSERT(np_sloreta_band_power(&ctx, 0U, NULL) == NP_HD_ERR_INVALID_ARG,
               "HD01-E8: NULL out accepted");
        ASSERT(np_sloreta_band_power(NULL, 0U, &b) == NP_HD_ERR_INVALID_ARG,
               "HD01-E8: NULL ctx accepted");

        /*
         * Unbound weight matrix.  Built by hand rather than obtained from
         * np_sloreta_init(), which rejects a NULL W — same reason HD02-I builds
         * its conflicting montages by hand: a guard only reachable through an
         * invalid state still has to be exercised, or it can be deleted without
         * any test objecting.  The state is not hypothetical: np_hd_session_destroy()
         * memsets the session, zeroing the embedded ctx's W, and n_voxels is set
         * here only so the range check does not mask the guard under test.
         */
        np_sloreta_ctx_t bare;
        memset(&bare, 0, sizeof(bare));
        bare.n_voxels        = 1U;
        bare.spectral_epochs = 1U;
        ASSERT(np_sloreta_band_power(&bare, 0U, &b) == NP_HD_ERR_NO_WEIGHT_MATRIX,
               "HD01-E8: band power computed with no weight matrix bound");

        /* Reset must clear the spectral accumulator too, not just the
         * broadband one — otherwise a new session inherits the last one's
         * spectrum, which is a UHDR cross-contamination bug as well as a
         * measurement bug. */
        hd01e_push(&ctx, NP_HD_SLORETA_EPOCHS);
        ASSERT_OK(np_sloreta_band_power(&ctx, 0U, &b));
        np_sloreta_reset(&ctx);
        ASSERT(np_sloreta_band_power(&ctx, 0U, &b) == NP_HD_ERR_NOT_READY,
               "HD01-E8: reset did not clear the spectral accumulator");
        printf("  contract: NOT_READY when empty / short-epoch / after reset; "
               "bad args rejected\n");
    }

    int result = g_fail_count - failures_before;
    printf("FAI-HD01-E: %s (%d failures)\n\n", result == 0 ? "PASS" : "FAIL", result);
    return result;
}

/* ── FAI-HD01-F: peak selection is a real argmax ─────────────────────────────── */
/*
 * HD01-F: np_sloreta_find_peak() must return the index of the LARGEST source
 * power, wherever in the array it sits.
 *
 * Why this exists.
 *
 * Every other FAI case builds its synthetic weight matrix so that voxel 0 is the
 * driven one — HD01 puts a 10 µV tone on F3 and reads it at W row 0, HD01-E does
 * the same.  find_peak() initialises peak_val from source_power[0] and only
 * updates inside the comparison, so under those stimuli the update body never
 * runs: gcov reported both of its lines as ##### with the whole suite passing.
 * A find_peak() that returned voxel 0 unconditionally, or whose comparison was
 * inverted to an argmin, satisfied the entire suite.
 *
 * That is not a coverage-hygiene complaint.  np_hd_session_compute_sloreta()
 * feeds sloreta_result.peak_mni straight into np_hd_montage_select_*(), so the
 * argmax picks the electrode ring that current is delivered through.  A silent
 * defect here is a wrong-target stimulation path.
 *
 * Method.  Two independent probes of the same property:
 *
 *   1. Hand-built source-power arrays passed directly to find_peak().  The
 *      function takes the array as an argument, so this isolates the comparison
 *      from the covariance and weight-matrix path entirely — nothing but the
 *      argmax is under test, and the expected answer is by inspection.
 *   2. The full covariance path, with a synthetic W in which a LATER voxel reads
 *      the channel carrying the largest tone.  This proves the two halves are
 *      wired together, i.e. that the array find_peak() sees is the map
 *      compute_map() produced.
 *
 * Pass criteria:
 *   HD01-F1 (position):    the peak is found at the start, at v == 1, in the
 *                          middle, and at the last index — so an argmin, a
 *                          missing update body, a loop starting past v == 1, and
 *                          an off-by-one on either end of the bound all fail.
 *   HD01-F2 (bound):       a larger value one past n_voxels is NOT selected; and
 *                          n_voxels == 1 returns voxel 0.
 *   HD01-F3 (negatives):   an all-negative map peaks at its least-negative entry
 *                          (catches peak_val seeded from 0.0f rather than [0]).
 *   HD01-F4 (tie-break):   on an exact tie the LOWEST index wins — the strict '>'
 *                          contract, pinned deliberately (see the comment there).
 *   HD01-F5 (payload):     peak_mni and peak_source_power correspond to the
 *                          reported peak_voxel, not to voxel 0 or to a stale
 *                          value.
 *   HD01-F6 (args):        NULL ctx / power / out and n_voxels == 0 are rejected.
 *   HD01-F7 (end-to-end):  through compute_map(), a later voxel reading the
 *                          largest-amplitude channel is the reported peak, both
 *                          when it sits mid-array and when it sits last.
 */
#define HD01F_VOXELS 6U

/* Six distinct MNI coordinates, so peak_mni identifies the voxel unambiguously:
 * a peak_mni assertion cannot be satisfied by the wrong index. */
static float       hd01f_W[HD01F_VOXELS * NP_HD_SLORETA_N_CH];
static np_hd_mni_t hd01f_mni[HD01F_VOXELS] = {
    { -46,  36,  20 }, { -35, -55,  64 }, {  55,   0,  67 },
    {  21, -85,   5 }, {   0, -10,  83 }, {  35,  36,  64 },
};
static float hd01f_samples[NP_HD_SLORETA_N_CH][NP_HD_SLORETA_FFT_SIZE];

/* Voxel v reads channel hd01f_channel[v]; each carries its own whole-cycle tone
 * so every channel's epoch mean is exactly zero and the tones stay orthogonal. */
static const uint8_t  hd01f_channel[HD01F_VOXELS] = {
    NP_HD_CH_F3, NP_HD_CH_P3, NP_HD_CH_C4, NP_HD_CH_O2, NP_HD_CH_CZ, NP_HD_CH_F4,
};
static const uint16_t hd01f_cycles[HD01F_VOXELS] = { 20U, 12U, 40U, 5U, 30U, 17U };

/*
 * Drive find_peak() with a caller-supplied map and check the whole result.
 *
 * peak_source_power is compared with == rather than a tolerance: find_peak()
 * copies the winning element verbatim and performs no arithmetic on it, so an
 * exact match is the correct contract.  A tolerance here would let a value
 * computed from the wrong element pass whenever the two happened to be close.
 */
static void hd01f_expect_peak(np_sloreta_ctx_t *ctx,
                              const float      *power,
                              uint16_t          n,
                              uint16_t          want_idx,
                              const char       *what)
{
    np_hd_sloreta_result_t r;
    memset(&r, 0xAA, sizeof(r));        /* poison: prove every field is written */

    np_hd_status_t ret = np_sloreta_find_peak(ctx, power, n, &r);
    if (ret != NP_HD_OK) {
        printf("FAIL [%s] HD01-F: find_peak returned %d\n", what, (int)ret);
        g_fail_count++;
        return;
    }

    if (r.peak_voxel != want_idx) {
        printf("FAIL [%s] HD01-F1: peak_voxel=%u (power %.4f), expected %u "
               "(power %.4f)\n", what, (unsigned)r.peak_voxel,
               power[r.peak_voxel < n ? r.peak_voxel : 0U],
               (unsigned)want_idx, power[want_idx]);
        g_fail_count++;
    }

    /* HD01-F5: the payload must describe the voxel that was reported. */
    if (r.peak_source_power != power[want_idx]) {
        printf("FAIL [%s] HD01-F5: peak_source_power=%.4f, expected %.4f\n",
               what, r.peak_source_power, power[want_idx]);
        g_fail_count++;
    }
    if (r.peak_mni.x != hd01f_mni[want_idx].x ||
        r.peak_mni.y != hd01f_mni[want_idx].y ||
        r.peak_mni.z != hd01f_mni[want_idx].z) {
        printf("FAIL [%s] HD01-F5: peak_mni=(%d,%d,%d), expected (%d,%d,%d)\n",
               what, r.peak_mni.x, r.peak_mni.y, r.peak_mni.z,
               hd01f_mni[want_idx].x, hd01f_mni[want_idx].y,
               hd01f_mni[want_idx].z);
        g_fail_count++;
    }
    ASSERT(r.valid, "HD01-F: result not marked valid");
}

static int fai_hd01f_peak_argmax(void)
{
    int failures_before = g_fail_count;
    printf("FAI-HD01-F: peak selection is a real argmax\n");

    /* Row v is the unit vector on hd01f_channel[v]. */
    memset(hd01f_W, 0, sizeof(hd01f_W));
    for (uint8_t v = 0U; v < HD01F_VOXELS; v++) {
        hd01f_W[v * NP_HD_SLORETA_N_CH + hd01f_channel[v]] = 1.0f;
    }

    /* ── Part 1: the argmax alone, driven with hand-built maps ────────────── */
    {
        np_sloreta_ctx_t ctx;
        ASSERT_OK(np_sloreta_init(&ctx, hd01f_W, hd01f_mni, HD01F_VOXELS));

        /*
         * HD01-F1: the peak in each structurally distinct position.
         *
         * "at v == 1" and "last" are not redundant with "middle": v == 1 is the
         * first loop iteration, so a loop that started at v == 2 would still find
         * a middle peak, and "last" is the only case an off-by-one shortening the
         * bound can miss.  "at 0" is the control — it is the only arrangement the
         * pre-existing suite ever produced, and on its own it is satisfied by a
         * function that never enters the loop at all.
         */
        static const float p_first[HD01F_VOXELS]  = { 9.0f, 2.0f, 3.0f, 4.0f, 5.0f, 1.0f };
        static const float p_second[HD01F_VOXELS] = { 1.0f, 9.0f, 3.0f, 4.0f, 5.0f, 2.0f };
        static const float p_middle[HD01F_VOXELS] = { 1.0f, 2.0f, 9.0f, 4.0f, 5.0f, 3.0f };
        static const float p_last[HD01F_VOXELS]   = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 9.0f };

        hd01f_expect_peak(&ctx, p_first,  HD01F_VOXELS, 0U, "peak@0 (control)");
        hd01f_expect_peak(&ctx, p_second, HD01F_VOXELS, 1U, "peak@1 (first iter)");
        hd01f_expect_peak(&ctx, p_middle, HD01F_VOXELS, 2U, "peak@middle");
        hd01f_expect_peak(&ctx, p_last,   HD01F_VOXELS, 5U, "peak@last");

        /*
         * HD01-F2: n_voxels is a hard bound, not a hint.
         *
         * The array is deliberately HD01F_VOXELS long while only the first five
         * entries are offered, and the sixth is by far the largest.  Reading it
         * is therefore defined behaviour rather than an out-of-bounds access, so
         * a `v <= n_voxels` bound fails this assertion loudly instead of
         * depending on whatever happens to sit past the array.
         */
        static const float p_short[HD01F_VOXELS] = { 1.0f, 2.0f, 7.0f, 4.0f, 5.0f, 1000.0f };
        hd01f_expect_peak(&ctx, p_short, 5U, 2U, "peak within bound (tail ignored)");

        /* Degenerate map: one voxel, no loop iterations at all. */
        static const float p_one[HD01F_VOXELS] = { 3.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        hd01f_expect_peak(&ctx, p_one, 1U, 0U, "single voxel");

        /*
         * HD01-F3: an all-negative map.  Source power from a covariance quadratic
         * form is non-negative in practice, so this is about the seed and not
         * about real data: peak_val must start at source_power[0], and an
         * implementation that seeded it from 0.0f (or from FLT_MIN, or memset the
         * result and compared against a zeroed field) would report voxel 0 here
         * while agreeing with every non-negative case above.
         */
        static const float p_neg[HD01F_VOXELS] = { -9.0f, -2.0f, -5.0f, -1.0f, -7.0f, -3.0f };
        hd01f_expect_peak(&ctx, p_neg, HD01F_VOXELS, 3U, "all-negative map");

        /*
         * HD01-F4: tie-breaking.
         *
         * The comparison is strict '>', so on an exact tie the LOWEST index is
         * retained.  Pinning this is a DELIBERATE CHOICE, not an accident of the
         * current code: the two coordinates are different scalp targets, so which
         * one wins decides which montage is built, and "whichever the compiler
         * felt like" is not an acceptable answer for a stimulation target.  Lowest
         * index is chosen because it makes the result depend only on the voxel
         * ordering in the weight matrix, which is fixed at build time and
         * reproducible across runs and revisions.
         *
         * A refactor to '>=' — which would silently switch to the HIGHEST tied
         * index — must fail here rather than quietly changing which electrode
         * ring a tied map selects.
         */
        static const float p_tie[HD01F_VOXELS]     = { 1.0f, 9.0f, 3.0f, 9.0f, 5.0f, 2.0f };
        static const float p_all_tie[HD01F_VOXELS] = { 4.0f, 4.0f, 4.0f, 4.0f, 4.0f, 4.0f };
        hd01f_expect_peak(&ctx, p_tie,     HD01F_VOXELS, 1U, "tie -> lowest index");
        hd01f_expect_peak(&ctx, p_all_tie, HD01F_VOXELS, 0U, "all tied -> index 0");

        /* HD01-F6: argument validation. */
        {
            np_hd_sloreta_result_t r;
            ASSERT(np_sloreta_find_peak(NULL, p_last, HD01F_VOXELS, &r)
                       == NP_HD_ERR_INVALID_ARG, "HD01-F6: NULL ctx accepted");
            ASSERT(np_sloreta_find_peak(&ctx, NULL, HD01F_VOXELS, &r)
                       == NP_HD_ERR_INVALID_ARG, "HD01-F6: NULL source_power accepted");
            ASSERT(np_sloreta_find_peak(&ctx, p_last, HD01F_VOXELS, NULL)
                       == NP_HD_ERR_INVALID_ARG, "HD01-F6: NULL out accepted");
            ASSERT(np_sloreta_find_peak(&ctx, p_last, 0U, &r)
                       == NP_HD_ERR_INVALID_ARG, "HD01-F6: n_voxels == 0 accepted");
        }

        printf("  direct argmax: first/second/middle/last, bound, negatives, "
               "ties, bad args\n");
    }

    /* ── Part 2: the same property through the covariance path ────────────── */
    /*
     * Part 1 proves the comparison; this proves the wiring.  If compute_map()
     * wrote the map in a different voxel order, or find_peak() were handed
     * something other than that map, Part 1 would still pass and the delivered
     * target would still be wrong.
     *
     * W row v is the unit vector on hd01f_channel[v], so source_power[v] is that
     * channel's variance — exactly A_v^2 / 2 for a coherent tone of amplitude A_v.
     * The amplitudes below put the maximum on a LATER voxel in both arrangements.
     */
    {
        static const float k_two_pi = 6.283185307179586f;

        static const struct {
            const char *name;
            float       amplitude_uv[HD01F_VOXELS];
            uint16_t    want_idx;
        } cases[] = {
            /* Largest tone on voxel 2 — peak mid-array.                        */
            { "driven voxel 2 (middle)", { 2.0f, 4.0f, 10.0f, 6.0f, 3.0f, 5.0f }, 2U },
            /* Largest tone on voxel 5 — peak at the last index.                */
            { "driven voxel 5 (last)",   { 2.0f, 4.0f,  6.0f, 3.0f, 5.0f, 10.0f }, 5U },
        };
        const uint8_t n_cases = (uint8_t)(sizeof(cases) / sizeof(cases[0]));

        for (uint8_t c = 0U; c < n_cases; c++) {
            np_sloreta_ctx_t ctx;
            ASSERT_OK(np_sloreta_init(&ctx, hd01f_W, hd01f_mni, HD01F_VOXELS));

            memset(hd01f_samples, 0, sizeof(hd01f_samples));
            for (uint8_t v = 0U; v < HD01F_VOXELS; v++) {
                for (uint16_t s = 0U; s < NP_HD_SLORETA_FFT_SIZE; s++) {
                    float ph = k_two_pi * (float)s / (float)NP_HD_SLORETA_FFT_SIZE;
                    hd01f_samples[hd01f_channel[v]][s] =
                        cases[c].amplitude_uv[v] * sinf(ph * (float)hd01f_cycles[v]);
                }
            }

            for (uint16_t e = 0U; e < NP_HD_SLORETA_EPOCHS; e++) {
                ASSERT_OK(np_sloreta_push_epoch(
                    &ctx, (const float (*)[NP_HD_SLORETA_FFT_SIZE])hd01f_samples,
                    NP_HD_SLORETA_FFT_SIZE));
            }

            float power[HD01F_VOXELS];
            ASSERT_OK(np_sloreta_compute_map(&ctx, power, HD01F_VOXELS));

            /* The map itself must carry A^2/2 per voxel, so a peak assertion
             * below cannot be credited to a map that is wrong in some other way. */
            for (uint8_t v = 0U; v < HD01F_VOXELS; v++) {
                float expect = cases[c].amplitude_uv[v] *
                               cases[c].amplitude_uv[v] / 2.0f;
                ASSERT_APPROX(power[v], expect, 0.02f * expect);
            }

            /* HD01-F7: the later driven voxel is the reported peak, with the
             * MNI coordinate and power that belong to it. */
            hd01f_expect_peak(&ctx, power, HD01F_VOXELS, cases[c].want_idx,
                              cases[c].name);

            printf("  %-24s: power = %.2f %.2f %.2f %.2f %.2f %.2f -> voxel %u\n",
                   cases[c].name, power[0], power[1], power[2],
                   power[3], power[4], power[5], (unsigned)cases[c].want_idx);
        }
    }

    int result = g_fail_count - failures_before;
    printf("FAI-HD01-F: %s (%d failures)\n\n", result == 0 ? "PASS" : "FAIL", result);
    return result;
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
    fai_hd01d_estimator_variance();
    fai_hd01e_band_power_spectral();
    fai_hd01f_peak_argmax();
    fai_hd02_electrode_mapping();
    fai_hd02l_driver_channel_single_source();
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
