/* NP-SIM-FW-001 §8 step 1.  HOST ONLY. */
#ifndef NP_SIM_PBM_HARNESS_H
#define NP_SIM_PBM_HARNESS_H

void np_simh_init(void);          /* reset the bus, bind the hardware version 1 layout, clear dispatch */
void np_simh_line(char *line);    /* run one command line (modified in place) */

#endif
