/*
 * NeurOne Hub Control Program — the goggles' seam to the photoparoxysmal detector (REQ-FWHUB-67 / -69 / -72)
 */
#ifndef NP_MOD_VISUAL_H
#define NP_MOD_VISUAL_H

#include <stdbool.h>
#include "np_ppx_detect.h"

/* Cut the goggles now and hold the session paused (the panic's second step: stop or resume). Latches until the module is initialised. */
void np_mod_visual_ppx_halt(void);

/* Is the goggles' photic drive in the risk zone running now? */
bool np_mod_visual_photic_zone_active(void);

/* The detector's halt hook: halts only if the zone is being driven. Register with np_ppx_set_halt_fn(). */
void np_mod_visual_ppx_notify(np_ppx_verdict_t why);

#endif /* NP_MOD_VISUAL_H */
