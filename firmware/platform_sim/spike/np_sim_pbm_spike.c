/*
 * NeurOne Firmware-in-the-Loop Simulator — PBM spike driver (NP-SIM-FW-001 §8 step 1)
 * HOST ONLY.
 *
 * Runs the REAL np_socket_dispatch, np_module_map, np_protocol, np_mod_pbm,
 * np_pbm_socket_telem and np_safety_spi sources against sim PBM modules, reading
 * commands from stdin as text lines and writing events to stdout as JSON lines.
 * The text form is deliberate: the firmware side parses no JSON.  The Node
 * bridge (simulator/server/) turns MODULE_* WebSocket messages into these lines.
 *
 * Commands (sockets are 0-based indexes):
 *   add <socket> <ntc_c>            put a T1-A tile in the socket
 *   drive <s>[,<s>...]              np_sock_disp_command: drive base tiles
 *   stop  <s>[,<s>...]              np_sock_disp_command: stop them
 *   fail <index> <op> <code> <n> [after]   next n calls of op return code (n<0: until cleared), after `after` normal calls
 *   reading <socket> <op> <value>   pin op's reading (ntc_read)
 *   absent <socket> | present <socket> | clear <socket>
 *   report <version>                the safety MCU reports the helmet's hardware version; the layout follows it
 *
 * Stimulation (BES/tACS and tDCS; np_sim_stim_harness.c lists them all):
 *   stim_add <pair> <kohm>          an electrode pair
 *   bes <pair> <waveform> <freq_mhz> <amp_ua> | tdcs <pair> <current_ua> <polarity> <ramp_s> <area_mcm2>
 *   stim_stop bes|tdcs | tdcs_pause | advance <ms> | sample bes|tdcs
 * Auricular VNS (np_sim_vns_harness.c lists them all):
 *   vns_add <ohm> | vns <side> <freq_mhz> <amp_ua> <pw_us> <ppg> <eeg_ref> <hrv_proto> | vns_stop | vns_sample
 * Cervical VNS (np_sim_cvns_harness.c lists them all; the scenario plays the safety MCU and the PPG ISR):
 *   cvns_add <l_kohm> <r_kohm> <unix_base> | cvns <side> <freq_mhz> <amp_ua> <pw_us> <ramp_s> <baseline_req> | cvns_mcu <mask> <status> | cvns_run <ms> <bpm>
 * Intranasal (np_sim_intranasal_harness.c) and the T2 stubs (np_sim_stubs_harness.c: stub <name> <op>):
 *   ins_add <auth> <contact> | ins <side> <freq> <duty> <cur660> <cur808> | ins_stop | ins_sample
 * Visual goggles (np_sim_visual_harness.c lists them all):
 *   vis_add <seated> <eye> <mpe> | vis <mode> <freq> <duty> <wl> <zlo> <zhi> <emdr> <mode_f> <shade> | vis_lift | vis_ppx
 * Audio (np_sim_audio_harness.c lists them all):
 *   audio_add <mesh_ohm> | audio <mode> <carrier_hz> <beat_mhz> <level_half_db> <bone> <adaptive> | audio_stop | audio_eeg_band <b>
 * EEG front end (np_sim_eeg_harness.c lists them all):
 *   eeg_add [id] | eeg_init | eeg <chmask> <gain> <notch> <ref_mode> <adaptive> | eeg_stop | eeg_sample | eeg_regs
 *   status                          every module, the dispatch table, requested enables
 *   quit
 */
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"

#include "np_hub_types.h"
#include "np_module_map.h"
#include "np_protocol.h"
#include "np_safety_spi.h"
#include "np_socket_dispatch.h"
#include "np_sw02_platform_hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    char line[256];
    np_simh_init();
    np_sim_emit("{\"ev\":\"ready\",\"spike\":\"pbm\",\"firmware\":\"np_socket_dispatch,np_module_map,"
                "np_protocol,np_mod_pbm,np_pbm_socket_telem,np_safety_spi,"
                "np_mod_stim,np_stim_xcheck,np_ramp_plan,np_mod_eeg,np_mod_vns,np_hrv_session,np_mod_audio,np_mod_visual,np_mod_intranasal,np_mod_t2_stubs,np_mod_cvns,np_cvns_library\"}");
    while (fgets(line, sizeof line, stdin) != NULL) { np_simh_line(line); }
    return 0;
}
