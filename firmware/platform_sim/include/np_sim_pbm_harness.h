/* NP-SIM-FW-001 Rev 2 §8 step 1.  HOST ONLY. */
#ifndef NP_SIM_PBM_HARNESS_H
#define NP_SIM_PBM_HARNESS_H

#define NP_SIMH_SOCKETS 80U

void np_simh_init(void);          /* reset the bus, bind the 80-socket map, clear dispatch */
void np_simh_line(char *line);    /* run one command line (modified in place) */

#endif
