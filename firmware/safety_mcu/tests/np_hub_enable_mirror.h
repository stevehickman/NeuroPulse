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

extern const np_hub_enable_mirror_t np_hub_enable_mirror[];
extern const unsigned               np_hub_enable_mirror_count;
extern const uint8_t                np_hub_ch_clin_stim;
extern const uint16_t               np_hub_en_audio;

#endif /* NP_HUB_ENABLE_MIRROR_H */
