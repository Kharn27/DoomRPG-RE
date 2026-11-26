#include "platform_video_config.h"
#if DOOMRPG_RES60_P3_WORLD
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#if DOOMRPG_RES60_P17_HEAP_SNAPSHOT
#include <stdlib.h>
#endif

#include "esp_res60_world_preview.h"
#if DOOMRPG_RES60_P11_PROFILE
#include "esp_res60_profile.h"
#endif
#if DOOMRPG_RES60_P10_WALL_MATERIAL
#include "esp_res60_wall_material.h"
#endif
#if DOOMRPG_RES60_P9_WEDGE
/* Only one render owner, never an allocation or a duplicated map arena.
 * Compiled out of 160x120 production; 512 * 2B = 1024B BSS. */
#define RES60_P9_MAX_LINES 512U
#if !DOOMRPG_RES60_P15_COMPACT_SNAPSHOT
static uint16_t wedgeIndices[RES60_P9_MAX_LINES];
#endif
#if DOOMRPG_RES60_P14_LINE_SNAPSHOT
#if DOOMRPG_RES60_P15_COMPACT_SNAPSHOT
/* P15: original 9-bit source index, both ray-visible special flag bits,
 * original 16-bit coordinates and texture; no quantization or reordered ties.
 * This 12-byte snapshot replaces both the 16-byte P14 record and P9 index. */
enum {
    RES60_P15_SOURCE_MASK=0x01ffU,
    RES60_P15_SPRITE_BIT=0x0200U,
    RES60_P15_OCCLUDER_BIT=0x0400U
};
typedef struct EspRes60P15SnapshotLine_s {
    uint16_t x1,y1,x2,y2,texture;
    uint16_t sourceAndFlags;
} EspRes60P15SnapshotLine;
typedef char EspRes60P15_Line12[(sizeof(EspRes60P15SnapshotLine)==12U)?1:-1];
#if DOOMRPG_RES60_P17_HEAP_SNAPSHOT
static EspRes60P15SnapshotLine* candidateLines=NULL;
#else
static EspRes60P15SnapshotLine candidateLines[RES60_P9_MAX_LINES];
#endif
#else
/* P14 frozen: 8192B snapshot and 1024B P9 index. */
typedef char EspRes60P14_Line16[(sizeof(EspRes60PreviewLine)==16U)?1:-1];
static EspRes60PreviewLine candidateLines[RES60_P9_MAX_LINES];
#endif
#endif

#if DOOMRPG_RES60_P17_HEAP_SNAPSHOT
/* P17 allocates only from the ordinary INTERNAL/8BIT heap on classic
 * no-PSRAM CYD. Never allocate from the raycast, BSP or UV hot paths. */
int EspRes60Preview_snapshotBegin(void) {
    if (candidateLines != NULL) return 1;
    candidateLines=(EspRes60P15SnapshotLine*)malloc(
        sizeof(EspRes60P15SnapshotLine)*RES60_P9_MAX_LINES);
    return candidateLines != NULL;
}
void EspRes60Preview_snapshotEnd(void) {
    if (candidateLines != NULL) {
        free(candidateLines);
        candidateLines=NULL;
    }
}
const void* EspRes60Preview_snapshotAddress(void) {
    return candidateLines;
}
#endif

/* All 192 cardinal rays lie inside |lateral| <= 191/192 * forward.
 * If both endpoints are strictly outside the same half-plane, the
 * segment cannot intersect the convex view wedge. Conservative inclusive
 * boundaries preserve exact edge hits and nearest-wall tie ordering.
 * All products are bounded by 2048*192 in signed 32-bit integers. */
static int insideCardinalWedge(const EspRes60PreviewCamera* cam,
                               const EspRes60PreviewLine* line) {
    int32_t dx1=(int32_t)line->x1-cam->worldX;
    int32_t dx2=(int32_t)line->x2-cam->worldX;
    int32_t dy1=(int32_t)line->y1-cam->worldY;
    int32_t dy2=(int32_t)line->y2-cam->worldY;
    int32_t f1,f2,l1,l2;
    switch(cam->angle) {
        case 0U:   f1=dx1; f2=dx2; l1=dy1; l2=dy2; break;
        case 64U:  f1=-dy1; f2=-dy2; l1=dx1; l2=dx2; break;
        case 128U: f1=-dx1; f2=-dx2; l1=-dy1; l2=-dy2; break;
        default:   f1=dy1; f2=dy2; l1=-dx1; l2=-dx2; break;
    }
    if(f1<=0 && f2<=0) return 0;
    if(192*l1>191*f1 && 192*l2>191*f2) return 0;
    if(192*l1< -191*f1 && 192*l2< -191*f2) return 0;
    return 1;
}
#endif

/* The P3 single-frame oracle deliberately uses the original Doom RPG
 * perspective: 192 horizontal samples, camera wall height 64, eye at z=36,
 * near clip 4 world units. We raycast source BSP *line coordinates* with
 * 64-bit determinants for a bounded flat-shaded preview; the permanent
 * textured BSP and sprites remain gated until their own milestones. */
enum {
    PREVIEW_MAP_SIZE = 2048,
    PREVIEW_NEAR = 4,
    PREVIEW_FAR = 4096,
    PREVIEW_WALL_HEIGHT = 64
};

static int validLine(const EspRes60PreviewLine* line) {
    if (line == NULL ||
        line->x1 > PREVIEW_MAP_SIZE || line->x2 > PREVIEW_MAP_SIZE ||
        line->y1 > PREVIEW_MAP_SIZE || line->y2 > PREVIEW_MAP_SIZE) return 0;
    return 1;
}

static uint16_t flatWallColor(uint16_t texture, uint32_t depth) {
    /* Color identifies an ORIGINAL logical BSP texture ID, but is NOT a
     * decoded texture or a claim of final game color fidelity. */
    const uint32_t tier = texture % 4U;
    uint32_t r = 152U + tier * 20U;
    uint32_t g = 162U + ((tier + 2U) % 4U) * 16U;
    uint32_t b = 137U + ((tier + 1U) % 4U) * 24U;
    uint32_t brightness = depth > 1152U ? 86U :
        depth > 576U ? 124U : depth > 288U ? 170U : 236U;
    r = (r * brightness) >> 8U;
    g = (g * brightness) >> 8U;
    b = (b * brightness) >> 8U;
    return (uint16_t)(((r >> 3U) << 11U) |
                      ((g >> 2U) << 5U) | (b >> 3U));
}

static void rayForColumn(uint8_t angle, uint32_t column,
                         int32_t* outRayX, int32_t* outRayY) {
    /* Original Doom RPG Render transform, not conventional mathematical
     * counter-clockwise rotation:
     *   cameraDepth = dx*cos(angle) - dy*sin(angle)
     *   cameraSide  = dx*sin(angle) + dy*cos(angle)
     * Native viewAngle=64 therefore looks toward -Y, not +Y.
     * Horizontal 192-column rays use (depth, side)=(192, -191..191).
     * The P3 original version swapped forward signs for angles 64 and 192:
     * on Entrance it raycast the wall BEHIND the player at 48 world units
     * and filled all 96 viewport rows gray. */
    const int32_t lateral = (int32_t)(column * 2U) - 191;
    switch (angle) {
        case 0U:   *outRayX = 192;      *outRayY = lateral; break;
        case 64U:  *outRayX = lateral;  *outRayY = -192; break;
        case 128U: *outRayX = -192;     *outRayY = -lateral; break;
        default:   *outRayX = -lateral; *outRayY = 192; break;
    }
}

/* Exact rational 2D ray/segment intersection (both signs accepted) in
 * original 32x32 tile-grid world coordinates. No floats, division, heap,
 * texture buffer or pointer-heavy desktop map line. */
static int rayHit(const EspRes60PreviewCamera* cam,
                  const EspRes60PreviewLine* line,
                  int32_t rayX, int32_t rayY,
                  int64_t* depthNum, int64_t* depthDen
#if DOOMRPG_RES60_P10_WALL_MATERIAL
                  , int64_t* segmentParamNum
#endif
                  ) {
#if DOOMRPG_RES60_P21_INTERSECTION32
    /* P21 numeric bound (C11 int32, proved for all admitted values):
     * endpoints in [0,2048], camera in [0,2047], cardinal rays
     * |rayX|+|rayY|<=383. Thus |sx,sy,vx,vy|<=2048,
     * |num|<=2*2048^2=8388608, |den,u|<=383*2048=784384.
     * num*192<=1610612736 fits signed 32 bit; FAR*den<=3212836864
     * fits UNSIGNED 32 bit. Nearest-hit cross-products remain 64 bit.
     * Do not enable for arbitrary map/ray formats without new bounds. */
    const int32_t sx = (int32_t)line->x2 - (int32_t)line->x1;
    const int32_t sy = (int32_t)line->y2 - (int32_t)line->y1;
    const int32_t vx = (int32_t)line->x1 - (int32_t)cam->worldX;
    const int32_t vy = (int32_t)line->y1 - (int32_t)cam->worldY;
    int32_t den = rayX * sy - rayY * sx;
    int32_t num = vx * sy - vy * sx;
    int32_t u = vx * rayY - vy * rayX;
#else
    const int64_t sx = (int64_t)line->x2 - line->x1;
    const int64_t sy = (int64_t)line->y2 - line->y1;
    const int64_t vx = (int64_t)line->x1 - cam->worldX;
    const int64_t vy = (int64_t)line->y1 - cam->worldY;
    int64_t den = (int64_t)rayX * sy - (int64_t)rayY * sx;
    int64_t num = vx * sy - vy * sx;
    int64_t u = vx * rayY - vy * rayX;
#endif
    if (den == 0) return 0;
    if (den < 0) {
        den = -den;
        num = -num;
        u = -u;
    }
#if DOOMRPG_RES60_P21_INTERSECTION32
    if (num <= 0 || u < 0 || u > den ||
        (uint32_t)num * 192U < (uint32_t)PREVIEW_NEAR * (uint32_t)den ||
        (uint32_t)num * 192U > (uint32_t)PREVIEW_FAR * (uint32_t)den)
        return 0;
#else
    if (num <= 0 || u < 0 || u > den ||
        num * 192 < PREVIEW_NEAR * den ||
        num * 192 > PREVIEW_FAR * den) return 0;
#endif
    *depthNum = num;
    *depthDen = den;
#if DOOMRPG_RES60_P10_WALL_MATERIAL
    *segmentParamNum = u; /* exact segment fraction u/den; already computed */
#endif
    return 1;
}

static void fillTopBottom(uint16_t* fb) {
    const size_t bottom = (size_t)ESP_RES60_HUD_BOTTOM_Y * ESP_RES60_WIDTH;
    for (uint32_t y = 0U; y < ESP_RES60_HUD_TOP_ROWS; ++y) {
        for (uint32_t x = 0U; x < ESP_RES60_WIDTH; ++x) {
            const uint16_t color = ((x / 16U) & 1U) ? 0xc800U : 0xf800U;
            fb[(size_t)y * ESP_RES60_WIDTH + x] =
                y == ESP_RES60_HUD_TOP_ROWS - 1U ? 0xffffU : color;
        }
    }
    for (uint32_t y = 0U; y < ESP_RES60_HUD_BOTTOM_ROWS; ++y) {
        for (uint32_t x = 0U; x < ESP_RES60_WIDTH; ++x) {
            const uint16_t color = ((x / 16U) & 1U) ? 0xfd20U : 0xffe0U;
            fb[bottom + (size_t)y * ESP_RES60_WIDTH + x] =
                y == 0U ? 0xffffU : color;
        }
    }
}

static int renderPreview(uint16_t* framebuffer, size_t framebufferBytes,
                          const EspRes60PreviewCamera* camera,
                          uint32_t lineCount, EspRes60PreviewReadLine reader,
                          void* context, EspRes60PreviewStats* outStats,
                          int texturedBackground) {
    EspRes60PreviewStats stats;
    uint32_t index;
    uint32_t column;
    memset(&stats, 0, sizeof(stats));
    stats.centerLineIndex = UINT32_MAX;
    if (outStats != NULL) *outStats = stats;
#if DOOMRPG_RES60_P17_HEAP_SNAPSHOT
    /* P18 pins P17's sole owner pointer for the duration of the synchronous
     * frame. The optional read callback cannot change ownership mid-frame;
     * invalidation before writes is fail-closed, never a hot-ray allocation. */
#if DOOMRPG_RES60_P18_LOCAL_SNAPSHOT
    EspRes60P15SnapshotLine* const frameSnapshot=candidateLines;
    if (frameSnapshot == NULL) return 0;
#else
    /* P17 baseline remains available as the untouched A/B oracle. */
    if (candidateLines == NULL) return 0;
#endif
#endif
#if DOOMRPG_RES60_P10_WALL_MATERIAL
    /* Invalidate the entire previous frame before any input admission. */
    EspRes60WallMaterial_reset();
#endif
    if (framebuffer == NULL || camera == NULL || reader == NULL ||
        outStats == NULL || framebufferBytes != ESP_RES60_FRAMEBUFFER_BYTES ||
        lineCount == 0U || lineCount > ESP_RES60_PREVIEW_MAX_LINES ||
        camera->worldX >= PREVIEW_MAP_SIZE ||
        camera->worldY >= PREVIEW_MAP_SIZE ||
        camera->viewZ != 36U ||
        (camera->angle & 63U) != 0U) return 0;
#if DOOMRPG_RES60_P9_WEDGE
    /* P9 only admits MAP_INTRO-scale maps; no 8KiB stack/heap index array. */
    if(lineCount>RES60_P9_MAX_LINES) return 0;
    uint32_t candidateCount=0U;
#endif

    /* Validate every source line before ANY framebuffer write. A corrupted
     * compact map fails closed rather than leaving an untrusted half-frame. */
#if DOOMRPG_RES60_P11_PROFILE
    uint32_t phaseAt=EspRes60Profile_nowUs();
#endif
    for (index = 0U; index < lineCount; ++index) {
        EspRes60PreviewLine line;
        memset(&line, 0, sizeof(line));
        if (!reader(index, &line, context) || !validLine(&line)) return 0;
#if DOOMRPG_RES60_P18_LOCAL_SNAPSHOT
        if (candidateLines != frameSnapshot) return 0;
#endif
        ++stats.preflightLines;
        ++stats.readerCalls;
#if DOOMRPG_RES60_P9_WEDGE
        if(insideCardinalWedge(camera,&line)) {
#if DOOMRPG_RES60_P15_COMPACT_SNAPSHOT
            /* lineCount<=512 was checked before preflight. The original
             * index fits without truncation; other BSP flags were unused. */
            const uint16_t sourceAndFlags=(uint16_t)(index |
                ((line.flags & ESP_RES60_PREVIEW_SPRITE_SPAN) ?
                    RES60_P15_SPRITE_BIT : 0U) |
                ((line.flags & ESP_RES60_PREVIEW_OCCLUDER_ONLY) ?
                    RES60_P15_OCCLUDER_BIT : 0U));
#if DOOMRPG_RES60_P18_LOCAL_SNAPSHOT
            frameSnapshot[candidateCount]=(EspRes60P15SnapshotLine){
                line.x1,line.y1,line.x2,line.y2,line.texture,sourceAndFlags};
#else
            candidateLines[candidateCount]=(EspRes60P15SnapshotLine){
                line.x1,line.y1,line.x2,line.y2,line.texture,sourceAndFlags};
#endif
#else
            wedgeIndices[candidateCount]=(uint16_t)index;
#if DOOMRPG_RES60_P14_LINE_SNAPSHOT
            candidateLines[candidateCount]=line;
#endif
#endif
            ++candidateCount;
        } else {
            ++stats.wedgeRejected;
        }
#endif
    }
#if DOOMRPG_RES60_P9_WEDGE
    stats.wedgeCandidates=candidateCount;
#endif

#if DOOMRPG_RES60_P11_PROFILE
    stats.preflightUs=EspRes60Profile_nowUs()-phaseAt;
    phaseAt=EspRes60Profile_nowUs();
#endif
    fillTopBottom(framebuffer);
#if DOOMRPG_RES60_P11_PROFILE
    stats.bandUs=EspRes60Profile_nowUs()-phaseAt;
#endif
    for (column = 0U; column < ESP_RES60_WIDTH; ++column) {
        int32_t rayX = 0;
        int32_t rayY = 0;
        int64_t bestNum = 0, bestDen = 1;
        uint16_t nearestTexture = 0U;
        uint32_t bestLineIndex = UINT32_MAX;
        uint32_t depth = 0U;
        int hit = 0;
#if DOOMRPG_RES60_P10_WALL_MATERIAL
        EspRes60PreviewLine nearestLine = {0};
        int64_t bestSegmentParamNum=0;
#endif
        rayForColumn(camera->angle, column, &rayX, &rayY);
#if DOOMRPG_RES60_P11_PROFILE
        uint32_t columnAt=EspRes60Profile_nowUs();
#endif
#if DOOMRPG_RES60_P9_WEDGE
        for (index = 0U; index < candidateCount; ++index) {
#else
        for (index = 0U; index < lineCount; ++index) {
#endif
            uint32_t lineIndex=index;
#if DOOMRPG_RES60_P15_COMPACT_SNAPSHOT
#if DOOMRPG_RES60_P18_LOCAL_SNAPSHOT
            const EspRes60P15SnapshotLine* entry=&frameSnapshot[index];
#else
            const EspRes60P15SnapshotLine* entry=&candidateLines[index];
#endif
            lineIndex=(uint32_t)(entry->sourceAndFlags & RES60_P15_SOURCE_MASK);
            const EspRes60PreviewLine line={
                entry->x1,entry->y1,entry->x2,entry->y2,entry->texture,
                ((entry->sourceAndFlags & RES60_P15_SPRITE_BIT) ?
                    ESP_RES60_PREVIEW_SPRITE_SPAN : 0U) |
                ((entry->sourceAndFlags & RES60_P15_OCCLUDER_BIT) ?
                    ESP_RES60_PREVIEW_OCCLUDER_ONLY : 0U)};
#elif DOOMRPG_RES60_P9_WEDGE
            lineIndex=wedgeIndices[index];
#endif
#if DOOMRPG_RES60_P14_LINE_SNAPSHOT && !DOOMRPG_RES60_P15_COMPACT_SNAPSHOT
            /* P13 guarantees immutable arena validity for the whole frame;
             * preflight already checked every source line. The snapshot is
             * indexed in original P9 order, including ties/special flags. */
            const EspRes60PreviewLine line=candidateLines[index];
#elif !DOOMRPG_RES60_P15_COMPACT_SNAPSHOT
            EspRes60PreviewLine line;
#endif
            int64_t num, den;
#if DOOMRPG_RES60_P10_WALL_MATERIAL
            int64_t paramNum;
#endif
#if !DOOMRPG_RES60_P14_LINE_SNAPSHOT
            memset(&line, 0, sizeof(line));
            if (!reader(lineIndex, &line, context) || !validLine(&line)) {
                /* Admission changed mid-frame. This is a read-only
                 * diagnostic and aborts without presenting partial pixels. */
                *outStats = stats;
                return 0;
            }
            ++stats.readerCalls;
#endif
            ++stats.rayTests;
            if ((line.flags & (ESP_RES60_PREVIEW_SPRITE_SPAN |
                                ESP_RES60_PREVIEW_OCCLUDER_ONLY)) != 0U) {
                ++stats.skippedSpecialLines;
                continue;
            }
            if (!rayHit(camera, &line, rayX, rayY, &num, &den
#if DOOMRPG_RES60_P10_WALL_MATERIAL
                        , &paramNum
#endif
                        )) continue;
            ++stats.intersections;
            if (!hit || num * bestDen < bestNum * den) {
                bestNum = num;
                bestDen = den;
                nearestTexture = line.texture;
                bestLineIndex = lineIndex;
#if DOOMRPG_RES60_P10_WALL_MATERIAL
                nearestLine = line;
                bestSegmentParamNum=paramNum;
#endif
                hit = 1;
            }
        }

#if DOOMRPG_RES60_P11_PROFILE
        stats.raysUs+=EspRes60Profile_nowUs()-columnAt;
        columnAt=EspRes60Profile_nowUs();
#endif
        if (hit) {
            ++stats.visibleColumns;
            depth = (uint32_t)((bestNum * 192) / bestDen);
            if (depth == 0U) depth = PREVIEW_NEAR;
            if (depth > stats.maxDepth) stats.maxDepth = depth;
            if (stats.minDepth == 0U || depth < stats.minDepth)
                stats.minDepth = depth;
        }
        if (column == ESP_RES60_WIDTH / 2U) {
            stats.centerDepth = depth;
            stats.centerLineIndex = bestLineIndex;
        }
        /* Source Doom RPG 64-high walls, 36-high camera eye, 192-wide
         * perspective and the new native halfViewport=48. */
        const int32_t top = hit ? (int32_t)(ESP_RES60_WORLD_ROWS / 2U) -
            (int32_t)(((int64_t)(PREVIEW_WALL_HEIGHT - camera->viewZ) *
                        ESP_RES60_WIDTH) / (2 * (int64_t)depth)) : 0;
        const int32_t wallH = hit ? (int32_t)(((int64_t)PREVIEW_WALL_HEIGHT *
                        ESP_RES60_WIDTH) / (2 * (int64_t)depth)) : 0;
        const int32_t bottom = top + wallH;
#if DOOMRPG_RES60_P10_WALL_MATERIAL
        if (hit) {
            /* P10 diagnostic UV: exact winning segment fraction already
             * provided by rayHit. One 64-bit division instead of two per
             * column, no duplicate raycast or additional owner/cache. */
            const int32_t dx=(int32_t)nearestLine.x2-nearestLine.x1;
            const int32_t dy=(int32_t)nearestLine.y2-nearestLine.y1;
            const int32_t ax=dx<0 ? -dx : dx;
            const int32_t ay=dy<0 ? -dy : dy;
            const int32_t dominant=ax>=ay ? dx : dy;
            const int64_t sourceAlong=
                ((int64_t)dominant*bestSegmentParamNum)/bestDen;
            int32_t phase=(int32_t)(sourceAlong%64);
            if(phase<0)phase+=64;
            if(!EspRes60WallMaterial_record(column,nearestTexture,top,
                                              wallH,(uint8_t)phase)) {
                *outStats=stats;
                return 0;
            }
        }
#endif
        const uint16_t wallColor = hit ? flatWallColor(nearestTexture, depth) : 0U;
        if (hit && top <= 0 && bottom >= (int32_t)ESP_RES60_WORLD_ROWS)
            ++stats.fullHeightColumns;
#if DOOMRPG_RES60_P11_PROFILE
        stats.geometryUs+=EspRes60Profile_nowUs()-columnAt;
        columnAt=EspRes60Profile_nowUs();
#endif
        for (uint32_t row = 0U; row < ESP_RES60_WORLD_ROWS; ++row) {
            const uint16_t bg = row < ESP_RES60_WORLD_ROWS / 2U
                ? camera->ceilingRgb565 : camera->floorRgb565;
            const int wall = hit && (int32_t)row >= top &&
                             (int32_t)row < bottom;
            if (wall || !texturedBackground)
                framebuffer[((size_t)ESP_RES60_WORLD_Y + row) * ESP_RES60_WIDTH +
                            column] = wall ? wallColor : bg;
            if (wall) ++stats.wallPixels;
        }
#if DOOMRPG_RES60_P11_PROFILE
        stats.fillUs+=EspRes60Profile_nowUs()-columnAt;
#endif
    }
#if DOOMRPG_RES60_P11_PROFILE
    phaseAt=EspRes60Profile_nowUs();
#endif
    stats.frameFNV = EspRes60Layout_fnv(framebuffer, framebufferBytes);
#if DOOMRPG_RES60_P11_PROFILE
    stats.hashUs=EspRes60Profile_nowUs()-phaseAt;
#endif
    if (stats.frameFNV == 0U || stats.visibleColumns == 0U) {
        *outStats = stats;
        return 0;
    }
    *outStats = stats;
#if DOOMRPG_RES60_P10_WALL_MATERIAL
    EspRes60WallMaterial_publish();
#endif
    return 1;
}

int EspRes60Preview_render(uint16_t* framebuffer, size_t framebufferBytes,
                          const EspRes60PreviewCamera* camera,
                          uint32_t lineCount, EspRes60PreviewReadLine reader,
                          void* context, EspRes60PreviewStats* outStats) {
    return renderPreview(framebuffer, framebufferBytes, camera, lineCount,
                         reader, context, outStats, 0);
}
#if DOOMRPG_RES60_P25_TEXTURED_PLANES
int EspRes60Preview_renderWithPlanes(uint16_t* framebuffer, size_t framebufferBytes,
                          const EspRes60PreviewCamera* camera,
                          uint32_t lineCount, EspRes60PreviewReadLine reader,
                          void* context, EspRes60PreviewStats* outStats) {
    return renderPreview(framebuffer, framebufferBytes, camera, lineCount,
                         reader, context, outStats, 1);
}
#endif

#endif /* DOOMRPG_RES60_P3_WORLD: production 50% keeps no new world code */
