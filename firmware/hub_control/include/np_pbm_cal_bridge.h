/*
 * NeurOne Hub Control — PBM calibration bridge (OI-HUB-C06)
 * Document: NP-FW-PBM1064-001 Rev 4 §6.6; NP-HW-HUB-001 Rev 3 §9.5
 *
 * Joins the two halves of UID-keyed dose-metering calibration:
 *
 *   firmware/pbm  asks "what is THIS module's calibration?", by UID,
 *                        through two injected callbacks.
 *   np_module_map        holds the answer, in its UID-keyed NVRAM record.
 *
 * The coupling is a bridge rather than a header include because hub_control
 * links pbm and not the reverse; pulling np_module_map.h into the PBM
 * library would invert that edge. This translation unit is the one place that
 * sees both, so it is the one place that knows about both.
 *
 * WHY UID AND NOT SOCKET: K_PD1/K_PD2/K_ratio_nom characterise a physical
 * module's LEDs and photodiodes, measured on that module at manufacture. Under
 * hex tiling tiles are universal and swappable, so keying by socket applies the
 * previous occupant's coefficients after a swap — invisibly, because cal_source
 * would still read NP_CAL_FACTORY. There is deliberately no socket-keyed entry
 * point in this file for anyone to reach for.
 *
 * IEC 62304 Class B (SW-02 hub control).
 */

#ifndef NP_PBM_CAL_BRIDGE_H
#define NP_PBM_CAL_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Install the module-map-backed calibration provider and socket→UID resolver
 * into firmware/pbm.
 *
 * Call once, AFTER np_module_map_init() (and after np_module_map_restore(), if
 * the inventory is being restored from NVRAM) and BEFORE the first
 * np_pbm_session_start(). Until it is called, the PBM library resolves
 * every socket to the zero UID and loads firmware defaults — the behaviour that
 * predates OI-HUB-C06 — so an un-bridged build is degraded, never wrong.
 *
 * np_module_map itself has no production call site yet (it is wired in with
 * OI-HEXMAP-02), which is why this bridge is not invoked from
 * np_hub_control_app_main() in this revision. It is exercised by
 * np_pbm_cal_bridge_tests.
 */
void np_hub_pbm_cal_bridge_install(void);

/*
 * Remove the bridge, restoring the defaults-only behaviour. Exists for tests
 * and for a controlled teardown; production has no reason to call it.
 */
void np_hub_pbm_cal_bridge_remove(void);

#ifdef __cplusplus
}
#endif

#endif /* NP_PBM_CAL_BRIDGE_H */
