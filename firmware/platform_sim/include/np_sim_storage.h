/*
 * NeurOne Firmware-in-the-Loop Simulator — the hub's partitions, stored in files (NP-SIM-FW-001 §33, OI-SIMFW-51, REQ-SIMFW-25)
 * HOST ONLY.
 *
 * Every partition the twin holds (Config, SHDR, UHDR) is a littlefs instance, in the device's own geometry (np_lfs_instance.c for Config,
 * np_lfs_log_instance.c for the two logs), over a block device whose medium is a FILE on the host (REQ-SIMFW-21 (a)).  The file is made in
 * the system temporary directory under a random name, readable and writable by its owner only (mode 0600, (d)), and is unlinked the moment
 * it is created, so it has no name and cannot outlive the process whatever ends it ((b)).  It is sparse: the UHDR partition is 6,903 MiB and
 * the SHDR partition 512 MiB, and only the blocks littlefs writes take disk.
 *
 * What the file holds is SYNTHETIC (REQ-SIMFW-08): whatever the hub's own code writes to a twin that no person wears.  UHDR is plaintext
 * here; the device encrypts it at rest (AES-256-XTS under a key NeurOne never holds), and the twin holds no key (OI-SIMFW-52).
 *
 * The log partitions' file layer (np_log_hal_part_open, _append, _sync, _close) is the twin's: the device's board layer does not implement it
 * yet (firmware/platform/ traps), so the twin gives the seam the behaviour its contract states (np_log_backend.h: an exclusive create of one
 * session file on UHDR, one append-mode file on SHDR).  Nothing here changes a firmware source (REQ-SIMFW-03).
 *
 * Not modelled: the eMMC's timing, wear and power loss, a flush to the disk (a sync moves no bytes to it), encryption at rest, and a save or
 * a retention period for the file (REQ-SIMFW-21 (c) and (e) to (o), which `OI-SIMFW-05` still owns).
 */
#ifndef NP_SIM_STORAGE_H
#define NP_SIM_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "lfs.h"
#include "np_hub_types.h"
#include "np_log_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { NP_SIM_STORE_CONFIG = 0, NP_SIM_STORE_UHDR = 1, NP_SIM_STORE_SHDR = 2, NP_SIM_STORE_COUNT } np_sim_store_t;

typedef struct {
    bool     up;               /* the partition's instance is formatted and mounted      */
    uint64_t apparent_bytes;   /* the backing file's length: the partition's size        */
    uint64_t allocated_bytes;  /* what the host actually allocated for it (sparse)       */
    unsigned nlink;            /* 0: the file has no name                                */
    unsigned mode;             /* permission bits of the backing file                    */
} np_sim_store_info_t;

/* The Config medium: opens the backing file and sets the block device functions of `cfg` (context, read, prog, erase, sync, lock, unlock).
 * The caller applies the device's geometry first (np_lfs_config_apply) and validates after.  NP_HUB_ERR_STORE_IO if no file can be made. */
np_hub_status_t np_sim_storage_config_device(struct lfs_config *cfg, uint64_t bytes);

/* What a partition is backed by; false for a partition not brought up yet. */
bool np_sim_storage_info(np_sim_store_t which, np_sim_store_info_t *out);

/* UHDR session files, by name (the session counter as 20 digits) and size, in littlefs's directory order; the number found.  Only closed files'
 * sizes are exact (littlefs reports a file open for write at its last sync). */
unsigned np_sim_storage_uhdr_sessions(uint64_t *counters, uint32_t *sizes, unsigned max);

/* The size of the SHDR log file, and `len` bytes of it from `off`; false if the partition is not up or the file is absent. */
bool np_sim_storage_shdr_size(uint32_t *size);

/* Read bytes of a closed UHDR session file (never one the logger has open: two handles on one file is what the Config store's rule forbids). */
bool np_sim_storage_uhdr_read(uint64_t counter, uint32_t off, void *buf, uint32_t len, uint32_t *got);

/* Unmount and mount a log partition again: littlefs's RAM state is lost and what comes back is what is in the file.  False if it cannot. */
bool np_sim_storage_remount_log(np_sim_store_t which);

#ifdef __cplusplus
}
#endif

#endif /* NP_SIM_STORAGE_H */
