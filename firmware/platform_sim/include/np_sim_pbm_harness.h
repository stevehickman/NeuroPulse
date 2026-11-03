/* NP-SIM-FW-001 §8 step 1.  HOST ONLY. */
#ifndef NP_SIM_PBM_HARNESS_H
#define NP_SIM_PBM_HARNESS_H

void np_simh_init(void);          /* reset the bus, bind the hardware version 1 layout, clear dispatch */
void np_simh_line(char *line);    /* run one command line (modified in place) */

#include <stdbool.h>

/* The stimulation commands (np_sim_stim_harness.c).  np_simh_stim_line returns false if it does not own the command. */
void np_simh_stim_init(void);
void np_simh_stim_status(void);
bool np_simh_stim_line(const char *cmd, char *a, char *b, char *c, char *d, char *e);

/* The EEG front-end commands (np_sim_eeg_harness.c). */
void np_simh_eeg_init(void);
void np_simh_eeg_status(void);
bool np_simh_eeg_line(const char *cmd, char *a, char *b, char *c, char *d, char *e);

/* The auricular VNS commands (np_sim_vns_harness.c). */
void np_simh_vns_init(void);
void np_simh_vns_status(void);
bool np_simh_vns_line(const char *cmd, char *a, char *b, char *c, char *d, char *e);

/* The audio commands (np_sim_audio_harness.c). */
void np_simh_audio_init(void);
void np_simh_audio_status(void);
bool np_simh_audio_line(const char *cmd, char *a, char *b, char *c, char *d, char *e);

#endif
