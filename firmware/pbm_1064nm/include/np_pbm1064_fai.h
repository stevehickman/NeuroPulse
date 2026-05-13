/*
 * NeuroPulse 1064nm Smart Zone Module — FAI Test Declarations
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

#ifdef __cplusplus
}
#endif

#endif /* NP_PBM1064_FAI_H */
