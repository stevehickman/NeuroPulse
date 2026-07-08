/*
 * NeurOne 1064nm Smart Zone Module — FAI Test Declarations
 * Document: NP-FW-PBM1064-001 Rev A §11
 *
 * FAI-SM-01, -02, -03, -05, -09, -10, -11: software-passable; implemented here.
 * FAI-SM-04, -06, -07, -08: hardware bench required (return PENDING status).
 */

#ifndef NP_PBM1064_FAI_H
#define NP_PBM1064_FAI_H

#include <stdint.h>
#include <stdbool.h>
#include "np_pbm1064_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Run all software-passable FAI tests and print results via np_fai_log(). */
void np_pbm1064_fai_run_all(void);

/* Individual test entry points (callable from test harness). */
np_pbm1064_fai_result_t np_pbm1064_fai_sm01(void); /* ADC detection, all 5 slots */
np_pbm1064_fai_result_t np_pbm1064_fai_sm02(void); /* I2C probe ACK, 50-cycle    */
np_pbm1064_fai_result_t np_pbm1064_fai_sm03(void); /* Base module compat          */
np_pbm1064_fai_result_t np_pbm1064_fai_sm04(void); /* HW bench — returns PENDING  */
np_pbm1064_fai_result_t np_pbm1064_fai_sm05(void); /* Duty cycle ceiling 25%      */
np_pbm1064_fai_result_t np_pbm1064_fai_sm06(void); /* HW bench — returns PENDING  */
np_pbm1064_fai_result_t np_pbm1064_fai_sm07(void); /* HW bench — returns PENDING  */
np_pbm1064_fai_result_t np_pbm1064_fai_sm08(void); /* HW bench — returns PENDING  */
np_pbm1064_fai_result_t np_pbm1064_fai_sm09(void); /* Thermal throttle cascade    */
np_pbm1064_fai_result_t np_pbm1064_fai_sm10(void); /* T2 combined state machine   */
np_pbm1064_fai_result_t np_pbm1064_fai_sm11(void); /* UHDR/SHDR data routing      */

/*
 * T2 combined session FAI tests (NP-SES-1064-001 §6; software-passable).
 *
 * FAI-T2-01: Combined session descriptor validation
 * FAI-T2-02: Full 4-step thermal throttle cascade (1170nm → CH_C → CH_B → CH_A)
 * FAI-T2-03: Combined UHDR record completeness (1064nm + 1170nm dose + sLORETA)
 * FAI-T2-04: 1170nm abort path — abort disables both 1170nm and 1064nm
 */
np_pbm1064_fai_result_t np_pbm1064_fai_t2_01(void);
np_pbm1064_fai_result_t np_pbm1064_fai_t2_02(void);
np_pbm1064_fai_result_t np_pbm1064_fai_t2_03(void);
np_pbm1064_fai_result_t np_pbm1064_fai_t2_04(void);

#ifdef __cplusplus
}
#endif

#endif /* NP_PBM1064_FAI_H */
