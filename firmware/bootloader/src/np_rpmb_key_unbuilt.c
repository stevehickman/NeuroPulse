/*
 * NeurOne Bootloader — RPMB key source, device build, NOT BUILT (OI-PRIV-32, NP-PRIV-ANALYSIS-004)
 *
 * The key source is undecided (docs/human-input/oi-priv-32-rpmb-key-source.md:
 * derived on the i.MX RT1062 from a fused device secret through the DCP, a
 * random key fused at the factory, or a key NeurOne re-derives). Until the
 * principal chooses, this always fails, so the RPMB floor store cannot
 * authenticate, the floor reads as an error, and np_rollback_admit() refuses
 * every image: no OTA update and no DFU image is installed (fail closed,
 * REQ-RPMBK-01 (a)).
 *
 * It is deliberately not replaced by a fixed, all-zero or build-time key, in
 * any build, test mode or bench mode (REQ-RPMBK-01 (b)): anyone who knows the
 * key can write a lower floor, and a key programmed into an eMMC can never be
 * changed.
 */
#include "np_rpmb_key.h"

np_status_t np_rpmb_key_get(uint8_t key[32])
{
    if (key != NULL) {
        for (size_t i = 0U; i < 32U; i++) {
            key[i] = 0U;
        }
    }
    return NP_ERR_ROLLBACK_STORE;
}
