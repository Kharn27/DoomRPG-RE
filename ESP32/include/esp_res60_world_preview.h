#ifndef DOOMRPG_ESP32_RES60_WORLD_PREVIEW_H
#define DOOMRPG_ESP32_RES60_WORLD_PREVIEW_H

#include <stddef.h>
#include <stdint.h>
#include "esp_res60_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

/* P3: read-only native map-geometry projection. No legacy Render_t or
 * mapwide decompression. Geometry is ORIGINAL BSP line coordinates, not a
 * resampled 160x80 framebuffer. This is not the final textured renderer. */
#define ESP_RES60_PREVIEW_MAX_LINES 4096U
#define ESP_RES60_PREVIEW_SPRITE_SPAN 0x00000002UL
#define ESP_RES60_PREVIEW_OCCLUDER_ONLY 0x20000000UL

typedef struct EspRes60PreviewLine_s {
    uint16_t x1, y1, x2, y2;
    uint16_t texture;
    uint32_t flags;
} EspRes60PreviewLine;

typedef struct EspRes60PreviewCamera_s {
    uint16_t worldX;
    uint16_t worldY;
    uint8_t angle; /* original cardinal 0/64/128/192; fail closed otherwise */
    uint8_t viewZ; /* original spawn view height: 36 */
    uint16_t ceilingRgb565;
    uint16_t floorRgb565;
} EspRes60PreviewCamera;

typedef struct EspRes60PreviewStats_s {
    uint32_t preflightLines;
    uint32_t rayTests;
    uint32_t intersections;
    uint32_t visibleColumns;
    uint32_t frameFNV;
    uint32_t readerCalls;
    uint32_t skippedSpecialLines;
    uint32_t maxDepth;
    /* Diagnostic witnesses for an apparently uniform 192x96 image. */
    uint32_t minDepth;
    uint32_t centerDepth;
    uint32_t centerLineIndex; /* UINT32_MAX when center sees no wall */
    uint32_t fullHeightColumns;
    uint32_t wallPixels;
    uint32_t wedgeCandidates; /* P9 only: bounded 512-line directional shortlist */
    uint32_t wedgeRejected;
#if DOOMRPG_RES60_P11_PROFILE
    /* Inclusive stage timing: P3 preflight, ray tests, depth/UV,
     * flat framebuffer fill and raw FNV; micros() instrumentation has cost. */
    uint32_t preflightUs, bandUs, raysUs, geometryUs, fillUs, hashUs;
#endif
} EspRes60PreviewStats;

/* Read one compact immutable map line; no allocations/owner mutation. */
typedef int (*EspRes60PreviewReadLine)(uint32_t index,
                                     EspRes60PreviewLine* outLine, void* ctx);

#if DOOMRPG_RES60_P17_HEAP_SNAPSHOT
/* P17: bounded session-owned 6144B scratch, acquired explicitly ONCE after
 * framebuffer startup. No allocation in render; caller owns begin/end. */
int EspRes60Preview_snapshotBegin(void);
void EspRes60Preview_snapshotEnd(void);
const void* EspRes60Preview_snapshotAddress(void);
#if DOOMRPG_RES60_P29_BORROW_PLANE_SCRATCH
/* Exclusive synchronous loan; return before raycast or snapshot teardown. */
void* EspRes60Preview_borrowPlaneScratch(size_t* bytes);
void EspRes60Preview_returnPlaneScratch(void);
#endif
#endif

/* Full-surface fail-closed inputs; two-pass preflight prevents partial writes
 * on bad source geometry. No secondary framebuffer and no row-sized scratch.
 * The 192x96 region contains raycast *flat shaded* real map walls; no wall
 * textures, BSP order, sprites, doors, HUD art or gameplay state. */
int EspRes60Preview_render(uint16_t* framebuffer, size_t framebufferBytes,
                          const EspRes60PreviewCamera* camera,
                          uint32_t lineCount, EspRes60PreviewReadLine reader,
                          void* context, EspRes60PreviewStats* outStats);

#if DOOMRPG_RES60_P25_TEXTURED_PLANES
/* Same raycast; preserve the already-painted native floor/ceiling outside
 * wall spans. Caller must complete native plane rendering first. */
int EspRes60Preview_renderWithPlanes(uint16_t* framebuffer, size_t framebufferBytes,
                          const EspRes60PreviewCamera* camera,
                          uint32_t lineCount, EspRes60PreviewReadLine reader,
                          void* context, EspRes60PreviewStats* outStats);
#endif

#ifdef __cplusplus
}
#endif
#endif
