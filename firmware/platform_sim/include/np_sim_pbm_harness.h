/* NP-SIM-FW-001 §8 step 1.  HOST ONLY. */
#ifndef NP_SIM_PBM_HARNESS_H
#define NP_SIM_PBM_HARNESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "np_hub_types.h"

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


/* The safety MCU in the loop (np_sim_safety_harness.c, OI-SIMFW-09). */
void np_simh_safety_init(void);
void np_simh_safety_status(void);
bool np_simh_safety_line(const char *cmd, char *a, char *b, char *c);

/* The app's protocol upload, over the hub's own transport (OI-SIMFW-38).  np_sim_proto.c. */
void np_sim_proto_pubkey(uint8_t pub[32]);
np_hub_status_t np_sim_proto_serial(uint8_t *buf, size_t len);   /* the TEST device serial, for np_proto_hal_get_device_serial */
bool np_simh_proto_line(const char *cmd, char *a);
bool np_simh_gatt_line(const char *cmd, char *a, char *b);   /* np_sim_gatt.c */

/* The hub's own boot path and task set (np_sim_hub.c, OI-SIMFW-45, OI-SIMFW-50). */
bool np_simh_hub_line(const char *cmd, char *a);
bool np_sim_hub_booted(void);                 /* hub_boot ran: the hub's tasks beat the safety MCU and take the protocol */
np_hub_status_t np_sim_storage_up(void);      /* the part's storage, formatted new and mounted: what the boot path needs bound first (np_sim_gatt.c) */
void np_sim_transport_up(void);               /* np_transport_init() once, unless the hub's own boot has run it (np_sim_proto.c) */
void np_sim_gatt_mark_up(void);               /* the hub's own boot brought the service up, so gatt_init has nothing to do */

#endif
