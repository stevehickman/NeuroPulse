/*
 * np_hexmap_nvram.c — the module-map blob on the Config partition (OI-HEXMAP-01)
 * Document: NP-FW-NVRAM-001 Rev 11 §4.2, D-1, D-2; NP-SOUP-LFS-001 §5.3,
 *           §13.18.2 (REQ-LFS-02); NP-HEX-ZM-001 §4
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * The two functions np_module_map_persist() and np_module_map_restore() have
 * called since NP-HEX-ZM-001 §7, bound to npmp.bin through np_cfg_store.  The
 * file is NP_CFG_POLICY_REBUILD: a cache of facts the power-on poll will
 * re-answer, and the only Config file on an emission path.
 *
 * What each rule rests on, and where it is enforced:
 *
 *   atomicity   np_cfg_store_replace() is an in-place O_TRUNC rewrite that
 *               littlefs commits at close, so a power cut leaves the old blob
 *               or the new one (L-3, swept in np_cfg_store_tests).  Nothing
 *               here removes or renames (OI-LFS-08).
 *   integrity   every read is content-verified by np_module_map_blob_verify(),
 *               the same CRC load() checks (OI-LFS-09).  A write is verified
 *               first too, so this layer never stores a blob it would refuse
 *               to read back.
 *   staleness   NOT handled here and cannot be: an old blob with a valid CRC
 *               is indistinguishable from a current one at this layer.  That
 *               is REQ-LFS-02, and np_module_map enforces it — a restored
 *               record answers nothing until the poll confirms its UID.
 *
 * Until #340 binds a block device, np_cfg_store has no instance, every call
 * here returns NP_HUB_ERR_INVALID_ARG, and restore() leaves the inventory
 * empty.  That is the first-boot path, and it is correct.
 */

#include <stddef.h>
#include <stdint.h>

#include "np_cfg_store.h"
#include "np_module_map.h"

static bool npmp_verify(const uint8_t *buf, size_t len, void *ctx)
{
    (void)ctx;
    return np_module_map_blob_verify(buf, len);
}

np_hub_status_t np_hexmap_nvram_read(uint8_t *buf, size_t len, size_t *read_len)
{
    if (buf == NULL || read_len == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    *read_len = 0U;
    return np_cfg_store_read(NP_CFG_FILE_NPMP, buf, len, read_len,
                             npmp_verify, NULL);
}

np_hub_status_t np_hexmap_nvram_write(const uint8_t *buf, size_t len)
{
    if (!np_module_map_blob_verify(buf, len)) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    return np_cfg_store_replace(NP_CFG_FILE_NPMP, buf, len);
}
