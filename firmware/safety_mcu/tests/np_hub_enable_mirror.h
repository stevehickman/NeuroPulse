/*
 * NeurOne — hub-side enable-word mirror (test support only)
 * See np_hub_enable_mirror.c for why this indirection exists.
 */

#ifndef NP_HUB_ENABLE_MIRROR_H
#define NP_HUB_ENABLE_MIRROR_H

#include <stdint.h>

typedef struct {
    const char *name;   /* modality name, without the NP_SAFETY_EN_ prefix */
    uint16_t    bit;    /* the hub's value for that enable bit */
} np_hub_enable_mirror_t;

extern const np_hub_enable_mirror_t NP_HUB_ENABLE_MIRROR[];
extern const unsigned               NP_HUB_ENABLE_MIRROR_COUNT;
extern const uint8_t                NP_HUB_CH_CLIN_STIM;
extern const uint8_t                NP_HUB_CH_TDCS;
extern const uint8_t                NP_HUB_CH_BES_TACS;
extern const uint8_t                NP_HUB_CH_VNS_HRV;
extern const uint8_t                NP_HUB_CH_CVNS;
extern const uint16_t               NP_HUB_BES_AREA_MCM2;
extern const uint16_t               NP_HUB_VNS_AREA_MCM2;
extern const uint16_t               NP_HUB_CVNS_AREA_MCM2;
extern const uint16_t               NP_HUB_EN_AUDIO;

#endif /* NP_HUB_ENABLE_MIRROR_H */
