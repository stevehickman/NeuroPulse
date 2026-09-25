/*
 * NeurOne Hub Control Program — Warranty Token (OI-WA-03)
 * Document: NP-FW-EMMC-002 §A.2, §A.3, §A.6; NP-APP-ROADMAP-001 §5
 *           (warrantyToken 4E455550-0010-…, READ 32 B)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * The 256-bit TRNG value that is the only link between a device's SHDR uploads
 * and its warranty registration (§A.1: a foreign key into the registration
 * system would make SHDR person-linked).  The app reads it over GATT and sends
 * it, not the serial number or an email, to the registration API (§A.4).
 *
 * ── Lifecycle ───────────────────────────────────────────────────────────────
 *   - Generated at first activation (§A.3), which here means the first time
 *     the app asks for it: np_warranty_token_get() provisions on first use.
 *   - Persisted in the Config partition as NP_CFG_FILE_WARRANTY_TOKEN, a
 *     REPLICATED np_cfg_store record: two enveloped copies in two metadata
 *     pairs, so one lost entry does not lose it.
 *   - Replaced only by a factory reset (§A.6 / §B.3 R-9, R-10 —
 *     np_factory_reset_hal_write_config_defaults()).
 *
 * ── The one rule that matters: never regenerate over a token you could not read
 * A token is created ONLY when the store reports it absent — neither copy
 * exists (NP_HUB_ERR_NOT_PRESENT).  A read error or an integrity failure is
 * returned to the caller and nothing is written: generating a fresh token then
 * would silently orphan the device's warranty registration and split its SHDR
 * history across two pseudonyms, and a transient eMMC error would be enough to
 * do it.  A device whose token is present but unreadable reads as "no token"
 * over GATT (the app keeps its interim token, OI-BLE-01) until it is serviced.
 *
 * ── Privacy ─────────────────────────────────────────────────────────────────
 * SHDR-class device identity (NP-FW-EMMC-002 §A.3: "Not stored in UHDR.  Not
 * derived from any user-identifying input").  No timestamp: the record stores
 * the device session count at generation, per §A.2.
 *
 * ── Concurrency ─────────────────────────────────────────────────────────────
 * Single caller: the BLE host task, through np_gatt_server's READ handler.
 * Provisioning blocks on a TRNG draw and one Config write, once per device
 * life.  Pure C, host-testable (np_gatt_server_tests).
 */

#ifndef NP_WARRANTY_TOKEN_H
#define NP_WARRANTY_TOKEN_H

#include <stddef.h>
#include <stdint.h>

#include "np_hub_types.h"

#define NP_WARRANTY_TOKEN_LEN        32u   /* 256-bit, §A.2 */

/* The persisted record, §A.2's np_warranty_token_t serialised little-endian:
 *   [0..31]  token
 *   [32..35] generated_at_session (u32 LE) — device session count, no timestamp
 *   [36..63] reserved, zero                                                    */
#define NP_WARRANTY_RECORD_LEN       64u
#define NP_WARRANTY_OFF_SESSION      32u
#define NP_WARRANTY_OFF_RESERVED     36u

/*
 * np_warranty_token_get — copy the device's warranty token into `out`,
 * provisioning it first if the device has never had one.
 *   NP_HUB_OK                   `out` holds the token
 *   NP_HUB_ERR_INVALID_ARG      out == NULL
 *   NP_HUB_ERR_STORE_IO         the stored token could not be read — nothing written
 *   NP_HUB_ERR_STORE_INTEGRITY  the stored record is present but refused
 *                               (damaged, reserved bytes set, or a degenerate
 *                               token) — nothing written
 *   NP_HUB_ERR_GENERIC          provisioning failed: the TRNG failed or drew a
 *                               degenerate value, or the write failed.  Nothing
 *                               is cached; the next call tries again.
 */
np_hub_status_t np_warranty_token_get(uint8_t out[NP_WARRANTY_TOKEN_LEN]);

/* Forget the RAM copy, as a reboot does.  Host tests only need it; the target
 * reboots after a factory reset (§B.3 R-12). */
void np_warranty_token_reset_cache(void);

/* ── Platform seam ───────────────────────────────────────────────────────────
 * Fill `buf` with `len` bytes from the i.MX RT1062 TRNG, with the NIST
 * SP 800-90B health tests §A.3 requires.  NP_HUB_OK only if every byte came
 * from a healthy source.  Trap-defined in firmware/platform/src/
 * np_platform_stub.c until the RNGB driver exists (OI-UHDRK-01's silicon). */
extern np_hub_status_t np_warranty_hal_trng_generate(uint8_t *buf, size_t len);

#endif /* NP_WARRANTY_TOKEN_H */
