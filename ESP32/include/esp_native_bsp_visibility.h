#ifndef DOOMRPG_ESP32_NATIVE_BSP_VISIBILITY_H
#define DOOMRPG_ESP32_NATIVE_BSP_VISIBILITY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_BSP_VISIBILITY_COLUMNS 160U
#define ESP_NATIVE_BSP_VISIBILITY_MAX_NODES 256U
#define ESP_NATIVE_BSP_VISIBILITY_LEAF_WORDS \
    (ESP_NATIVE_BSP_VISIBILITY_MAX_NODES / 32U)

typedef struct EspNativeBspVisibilityState_s {
    uint32_t visibleLeaves[ESP_NATIVE_BSP_VISIBILITY_LEAF_WORDS];
    int32_t columnScale[ESP_NATIVE_BSP_VISIBILITY_COLUMNS];
    uint32_t nodes;
    uint32_t leaves;
    uint32_t nodeCull;
    uint32_t lines;
    uint32_t backfaceCull;
    uint32_t clipCull;
    uint32_t occluders;
    uint32_t spriteSpans;
} EspNativeBspVisibilityState;

struct Render_s;

/*
 * Reproduce the stateful compact BSP visibility/depth pass used by the native
 * gameplay renderer. The caller owns outState; this API allocates nothing,
 * touches no framebuffer pixels and restores all borrowed legacy Render
 * projection/depth scratch before returning.
 */
int EspNativeBspVisibility_build(struct Render_s* render,
                                 EspNativeBspVisibilityState* outState);

/* Resolve one compact map sprite through the legacy Render_relinkSprite BSP
 * ownership rule, then report whether its leaf was admitted by outState. */
int EspNativeBspVisibility_mapSpriteVisible(
    const EspNativeBspVisibilityState* state,
    uint32_t mapSpriteIndex,
    uint32_t* outLeafIndex);

#ifdef __cplusplus
}
#endif

#endif
