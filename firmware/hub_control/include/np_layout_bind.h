/*
 * NeurOne Hub Control Program — Binding the Helmet's Socket Layout
 * Document: NP-FW-HUB-001 Rev 34, REQ-FWHUB-64 (part a), OI-FWHUB-27
 *
 * ── What this does ───────────────────────────────────────────────────────────
 * Looks the helmet's hardware version up in the generated layout table (np_socket_layouts.h) and binds that
 * row's geometry to the module map (np_module_map_init()), so the hub works to the sockets this helmet has and
 * to no others.  A version the hub holds no layout for is REFUSED (the Principal, 2026-11-02): nothing is
 * bound, and np_layout_admit() refuses every session.
 *
 * ── When ─────────────────────────────────────────────────────────────────────
 * np_hub_control_app_main() binds once at bring-up, with np_safety_spi_get_hardware_version().  That is the
 * SWR-HV-07 DEFAULT (version 1) until the safety MCU's first valid report arrives, which cannot be before the
 * scheduler runs.  While version 1 is the only layout that is the whole truth.  The default has an end
 * (NP-REG-UPG-001 §7.8 item 5: removed in the change that adds version 2), so binding FOLLOWS THE REPORT:
 * task_module_detect calls np_layout_follow_report() each pass, under the same lease hold as a probe, and the
 * first valid report that names another version rebinds to it.  np_layout_admit() compares the bound row's
 * version with the version the helmet REPORTS, so until that pass runs (at most NP_DETECT_ACCESSORY_POLL_MS) a
 * session is refused, which fails closed.
 *
 * Pure C, no FreeRTOS: host-testable (np_socket_layouts_tests).  IEC 62304 Class B — SW-02 hub control.
 */

#ifndef NP_LAYOUT_BIND_H
#define NP_LAYOUT_BIND_H

#include <stdbool.h>
#include <stdint.h>

#include "np_hub_types.h"
#include "np_socket_layouts.h"

/*
 * Bind the layout of `hardware_version`.
 *   NP_HUB_OK                the row is bound: np_module_map_init() has taken its geometry and length.
 *   NP_HUB_ERR_NOT_PRESENT   the hub holds no layout for this version; nothing is bound, and any earlier
 *                            binding is forgotten here: np_layout_bound() is NULL and the module map is unbound
 *                            (np_module_map_unbind()), so no address is valid.
 *   other                    np_module_map_init() refused the row; nothing is bound, the map is unbound.
 */
np_hub_status_t np_layout_bind(uint8_t hardware_version);

/* The row bound by the last successful np_layout_bind(), or NULL. */
const np_socket_layout_t *np_layout_bound(void);

/*
 * Follow the safety MCU's report (OI-FWHUB-27): once a valid report has arrived, the bound layout is the
 * reported version's.
 *   report not received     nothing changes: the bring-up binding (the SWR-HV-07 default) stands.
 *   bound version == reported   nothing changes, and the module map is NOT touched, so its inventory survives.
 *   otherwise               np_layout_bind(reported_version): the status of that bind (NP_HUB_OK, or
 *                           NP_HUB_ERR_NOT_PRESENT when the hub has no layout for the version, in which case
 *                           nothing is bound and np_layout_admit() refuses).  *rebound_out, if given, says
 *                           whether a bind was made.
 * A rebind re-initialises the module map, which discards its inventory: the caller must hold the session lease
 * (np_lease_probe_begin()) so no session is loaded or running, and any inventory must be polled again.
 */
np_hub_status_t np_layout_follow_report(uint8_t reported_version, bool report_received, bool *rebound_out);

/*
 * Session admission: may a session run on this helmet?  `device_hw_version` is the version the safety MCU
 * reports (np_safety_spi_get_hardware_version()).
 *   NP_HUB_OK                       a layout is bound and it is this helmet's version's.
 *   NP_HUB_ERR_NO_LAYOUT            none is bound, or the bound one is another version's: the hub does not know
 *                                   which addresses this helmet has.
 * The status is NP_HUB_ERR_NO_LAYOUT (-29), added for this refusal (the Principal, 2026-11-02): it is not the
 * protocol-mismatch status, which would tell the person their protocol is for another helmet.
 */
np_hub_status_t np_layout_admit(uint8_t device_hw_version);

/*
 * Protocol address check (REQ-FWHUB-64): does every socket address a descriptor names exist on this helmet?
 * Every socket-mask command's set bits are checked against the bound geometry (np_module_map_address_valid());
 * slot-addressed commands name no sockets and are not looked at.  An address that is not valid refuses the whole
 * descriptor, and it is not counted as anything: this is a check of validity, not of how many bits are set.
 *   NP_HUB_OK                the descriptor names only valid addresses (or none).
 *   NP_HUB_ERR_INVALID_ARG   it names at least one that is not, or the helmet has no bound layout.
 * The status is the existing "malformed descriptor" one; a status of its own changes the wire contract.
 */
np_hub_status_t np_layout_check_protocol_addresses(const np_session_desc_t *desc);

#endif /* NP_LAYOUT_BIND_H */
