/* NP-SIM-FW-001 §8 step 1.  HOST ONLY. */
#ifndef NP_SIM_PBM_HARNESS_H
#define NP_SIM_PBM_HARNESS_H

#include <stdbool.h>

/* Host tests only, never a text command: open the power governor so the driver paths behind it can run.  The
 * default is the production governor, which refuses every load (OI-SIMFW-11).  np_simh_init() closes it again. */
void np_sim_governor_open_for_test(bool open);
void np_simh_init(void);          /* reset the bus, bind the hardware version 1 layout, clear dispatch */
void np_simh_line(char *line);    /* run one command line (modified in place) */

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

/* The visual-goggle commands (np_sim_visual_harness.c). */
void np_simh_visual_init(void);
void np_simh_visual_status(void);
bool np_simh_visual_line(const char *cmd, char *a, char *b, char *c, char *d, char *e);

/* The intranasal Y-probe commands (np_sim_intranasal_harness.c). */
void np_simh_ins_init(void);
void np_simh_ins_status(void);
bool np_simh_ins_line(const char *cmd, char *a, char *b, char *c, char *d, char *e);

/* The T2 and accessory stubs (np_sim_stubs_harness.c). */
bool np_simh_stubs_line(const char *cmd, char *a, char *b);

/* The cervical VNS commands (np_sim_cvns_harness.c). */
void np_simh_cvns_init(void);
void np_simh_cvns_status(void);
bool np_simh_cvns_line(const char *cmd, char *a, char *b, char *c, char *d, char *e);

#endif
