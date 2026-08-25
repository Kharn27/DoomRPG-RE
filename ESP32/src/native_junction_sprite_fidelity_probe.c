#include <SDL.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_first_frame.h"
#include "esp_player_view_state.h"
#include "native_junction_sprite_fidelity_probe.h"
#include "native_junction_sprite_overlay_probe.h"
#include "platform_video_config.h"

#define EXPECTED_BASE_FRAME_FNV 0x8910c2edU
#define EXPECTED_MODE7_FRAME_FNV 0x299506ebU
#define EXPECTED_MAP_SPRITES 48U
#define EXPECTED_WALK_NODES 39U
#define EXPECTED_WALK_LEAVES 12U
#define EXPECTED_WALK_NODE_CULL 8U
#define EXPECTED_WALK_LINES 62U
#define EXPECTED_WALK_BACKFACE 20U
#define EXPECTED_WALK_CLIP 8U
#define MAX_TRACKED_NODES 256U
#define MAX_BSP_DEPTH 64U
#define SCREEN_W 160
#define VISUAL_MASK 0x0001fe00UL
#define HIDDEN 0x00010000UL
#define TWO_SIDED 0x00000001UL
#define SPRITE_SPAN 0x00000002UL
#define AXIS_X 0x00000008UL
#define AXIS_NEG 0x00000010UL
#define Y_NUDGE 0x00000100UL
#define X_NUDGE 0x00000200UL
#define OCCLUDER 0x20000000UL

static struct {
    int preAttempted;
    int preDone;
    int postAttempted;
    int postDone;
} probeState;

typedef struct RenderScratch_s {
    int viewCos_;
    int viewSin_;
    int viewTransX;
    int viewSin;
    int viewCos;
    int viewTransY;
    int viewX;
    int viewY;
    int viewZ;
    int viewAngle;
    int lineCount;
    int lineRasterCount;
    int nodeCount;
    int nodeRasterCount;
    int spriteCount;
    int spriteRasterCount;
    int screenLeft;
    int screenTop;
    int screenRight;
    int screenBottom;
    int numLines;
    byte spanMode;
    short* pixels;
    Line_t tmpLine;
    int columnScale[SCREEN_W];
} RenderScratch;

typedef struct VisibleWalk_s {
    uint32_t leafBits[MAX_TRACKED_NODES / 32U];
    uint32_t nodes;
    uint32_t leaves;
    uint32_t nodeCull;
    uint32_t lines;
    uint32_t backface;
    uint32_t clip;
    uint32_t occluders;
    uint32_t spriteSpans;
} VisibleWalk;

static uint32_t fnvAppend(uint32_t hash, const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t frameFNV(const Render_t* render) {
    const uint32_t bytes = DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT *
                           (uint32_t)sizeof(uint16_t);
    if (render == NULL || render->framebuffer == NULL) return 0U;
    return fnvAppend(2166136261U, render->framebuffer, bytes);
}

static uint32_t viewportFNV(const Render_t* render) {
    const uint16_t* framebuffer;
    const uint32_t rowBytes = DOOMRPG_LOGICAL_WIDTH *
                              (uint32_t)sizeof(uint16_t);
    int pitchPixels;
    uint32_t hash = 2166136261U;
    int y;

    if (render == NULL || render->framebuffer == NULL ||
        render->screenWidth != DOOMRPG_LOGICAL_WIDTH ||
        render->screenHeight != 80 || render->screenX != 0 ||
        render->screenY < 0 || render->screenY + 80 > DOOMRPG_LOGICAL_HEIGHT) {
        return 0U;
    }

    framebuffer = (const uint16_t*)render->framebuffer;
    pitchPixels = render->pitch >> 1;
    for (y = 0; y < render->screenHeight; ++y) {
        const uint16_t* row = framebuffer +
            (render->screenY + y) * pitchPixels;
        hash = fnvAppend(hash, row, rowBytes);
    }
    return hash;
}

static void saveScratch(const Render_t* r, RenderScratch* s) {
    memset(s, 0, sizeof(*s));
    s->viewCos_ = r->viewCos_;
    s->viewSin_ = r->viewSin_;
    s->viewTransX = r->viewTransX;
    s->viewSin = r->viewSin;
    s->viewCos = r->viewCos;
    s->viewTransY = r->viewTransY;
    s->viewX = r->viewX;
    s->viewY = r->viewY;
    s->viewZ = r->viewZ;
    s->viewAngle = r->viewAngle;
    s->lineCount = r->lineCount;
    s->lineRasterCount = r->lineRasterCount;
    s->nodeCount = r->nodeCount;
    s->nodeRasterCount = r->nodeRasterCount;
    s->spriteCount = r->spriteCount;
    s->spriteRasterCount = r->spriteRasterCount;
    s->screenLeft = r->screenLeft;
    s->screenTop = r->screenTop;
    s->screenRight = r->screenRight;
    s->screenBottom = r->screenBottom;
    s->numLines = r->numLines;
    s->spanMode = r->spanMode;
    s->pixels = r->pixels;
    s->tmpLine = r->tmpLine;
    memcpy(s->columnScale, r->columnScale, sizeof(s->columnScale));
}

static void restoreScratch(Render_t* r, const RenderScratch* s) {
    r->viewCos_ = s->viewCos_;
    r->viewSin_ = s->viewSin_;
    r->viewTransX = s->viewTransX;
    r->viewSin = s->viewSin;
    r->viewCos = s->viewCos;
    r->viewTransY = s->viewTransY;
    r->viewX = s->viewX;
    r->viewY = s->viewY;
    r->viewZ = s->viewZ;
    r->viewAngle = s->viewAngle;
    r->lineCount = s->lineCount;
    r->lineRasterCount = s->lineRasterCount;
    r->nodeCount = s->nodeCount;
    r->nodeRasterCount = s->nodeRasterCount;
    r->spriteCount = s->spriteCount;
    r->spriteRasterCount = s->spriteRasterCount;
    r->screenLeft = s->screenLeft;
    r->screenTop = s->screenTop;
    r->screenRight = s->screenRight;
    r->screenBottom = s->screenBottom;
    r->numLines = s->numLines;
    r->spanMode = s->spanMode;
    r->pixels = s->pixels;
    r->tmpLine = s->tmpLine;
    memcpy(r->columnScale, s->columnScale, sizeof(s->columnScale));
}

static int setupView(Render_t* r, const EspPlayerViewState* v) {
    int sin_;
    int cos_;
    int vx;
    int vy;
    int x;

    if (r == NULL || v == NULL || r->framebuffer == NULL ||
        r->columnScale == NULL || r->screenWidth != SCREEN_W ||
        r->screenHeight != 80 || r->screenX != 0 || r->screenY != 20) {
        return 0;
    }

    sin_ = r->sinTable[v->viewAngle & 255];
    cos_ = r->sinTable[(v->viewAngle + 64) & 255];
    vx = v->viewX - ((16 * cos_) >> 16);
    vy = v->viewY + ((16 * sin_) >> 16);

    r->viewX = vx;
    r->viewY = vy;
    r->viewZ = v->viewZ;
    r->viewCos_ = cos_;
    r->viewSin_ = -sin_;
    r->viewTransX = -((vx * r->viewCos_) + (vy * r->viewSin_));
    r->viewSin = sin_;
    r->viewCos = cos_;
    r->viewTransY = -((vx * r->viewSin) + (vy * r->viewCos));
    r->viewAngle = v->viewAngle;
    r->pixels = (short*)&r->framebuffer[r->pitch * r->screenY];
    r->screenLeft = 0;
    r->screenTop = 0;
    r->screenRight = SCREEN_W;
    r->screenBottom = 80;
    r->lineCount = 0;
    r->lineRasterCount = 0;
    r->nodeCount = 0;
    r->nodeRasterCount = 0;
    r->spriteCount = 0;
    r->spriteRasterCount = 0;
    r->spanMode = 0;

    for (x = 0; x < SCREEN_W; ++x) r->columnScale[x] = MAXINT;
    return 1;
}

static void sourceLine(const EspMapLine* src, Line_t* l) {
    memset(l, 0, sizeof(*l));
    l->vert1.x = src->x1;
    l->vert1.y = src->y1;
    l->vert2.x = src->x2;
    l->vert2.y = src->y2;
    l->flags = (int)src->flags;

    if ((src->flags & X_NUDGE) != 0U) {
        if ((src->flags & AXIS_X) != 0U) {
            l->vert1.x += 3;
            l->vert2.x += 3;
        }
        else if ((src->flags & AXIS_NEG) != 0U) {
            l->vert1.x -= 3;
            l->vert2.x -= 3;
        }
    }
    else if ((src->flags & Y_NUDGE) != 0U) {
        if ((src->flags & AXIS_X) != 0U) {
            l->vert1.y += 3;
            l->vert2.y += 3;
        }
        else if ((src->flags & AXIS_NEG) != 0U) {
            l->vert1.y -= 3;
            l->vert2.y -= 3;
        }
    }
}

static int depthColumns(Render_t* r, Line_t* l) {
    int dx = l->vert2.x - l->vert1.x;
    int step;
    int dDepth;
    int x;
    int x2;
    int depth;

    if (dx <= 0) return 1;
    step = (MAXINT / dx) << 1;
    dDepth = (int)DoomRPG_FixedMul(l->vert2.y - l->vert1.y, step);
    x = (l->vert1.x + 65535) >> 16;
    x2 = (l->vert2.x + 65535) >> 16;
    if (x < r->screenLeft) x = r->screenLeft;
    if (x2 > r->screenRight) x2 = r->screenRight;
    depth = l->vert1.y + DoomRPG_FixedMul((x << 16) - l->vert1.x, dDepth);

    while (x < x2) {
        int scale;
        if (depth <= 0) return 0;
        scale = (0x40000000 / depth) << 2;
        depth += dDepth;
        if (r->columnScale[x] >= scale) r->columnScale[x] = scale;
        ++x;
    }
    return 1;
}

static int depthLine(Render_t* r, uint32_t lineIndex, VisibleWalk* w) {
    EspMapLine src;
    Line_t l;
    Vertex_t tmp;

    if (!EspMapRuntime_getLine(lineIndex, &src)) return 0;
    sourceLine(&src, &l);
    r->numLines = (int)lineIndex;
    ++w->lines;

    if (((l.vert1.x - r->viewX) * (l.vert2.y - l.vert1.y)) +
        ((l.vert1.y - r->viewY) * (-(l.vert2.x - l.vert1.x))) <= 0) {
        if ((l.flags & TWO_SIDED) == 0) {
            ++w->backface;
            return 1;
        }
        tmp = l.vert1;
        l.vert1 = l.vert2;
        l.vert2 = tmp;
    }

    Render_transform2DVerts(r, &l.vert1);
    Render_transform2DVerts(r, &l.vert2);
    if (!Render_clipLine(r, &l)) {
        ++w->clip;
        return 1;
    }
    Render_projectVertex(r, &l.vert1);
    Render_projectVertex(r, &l.vert2);

    if ((l.flags & OCCLUDER) != 0) {
        Render_occludeClippedLine(r, &l);
        ++w->occluders;
        return 1;
    }
    if ((l.flags & SPRITE_SPAN) != 0) {
        ++w->spriteSpans;
        return 1;
    }
    return depthColumns(r, &l);
}

static int walkDepthNode(Render_t* r,
                         const EspMapRuntimeView* rt,
                         uint32_t nodeIndex,
                         uint32_t depth,
                         VisibleWalk* w) {
    EspMapNode compact;
    Node_t node;
    uint32_t lineStart;
    uint32_t lineCount;
    uint32_t first;
    uint32_t second;
    uint32_t split;
    uint32_t i;

    if (r == NULL || rt == NULL || w == NULL || depth > MAX_BSP_DEPTH ||
        nodeIndex >= rt->nodeCount || nodeIndex >= MAX_TRACKED_NODES ||
        !EspMapRuntime_getNode(nodeIndex, &compact)) {
        return 0;
    }

    memset(&node, 0, sizeof(node));
    node.x1 = (short)compact.x1;
    node.y1 = (short)compact.y1;
    node.x2 = (short)compact.x2;
    node.y2 = (short)compact.y2;
    node.args1 = (int)compact.args1;
    node.args2 = (int)compact.args2;

    ++r->nodeCount;
    ++w->nodes;
    if (Render_cullBoundingBox(r, &node)) {
        ++w->nodeCull;
        return 1;
    }

    if ((compact.args1 & 0x30000U) == 0U) {
        lineStart = compact.args2 & 0xffffU;
        lineCount = (compact.args2 >> 16) & 0xffffU;
        if (lineStart > rt->lineCount || lineCount > rt->lineCount - lineStart) {
            return 0;
        }
        w->leafBits[nodeIndex >> 5] |= 1U << (nodeIndex & 31U);
        ++r->nodeRasterCount;
        ++w->leaves;
        r->lineCount += (int)lineCount;
        for (i = 0U; i < lineCount; ++i) {
            if (!depthLine(r, lineStart + i, w)) return 0;
        }
        return 1;
    }

    first = (compact.args2 >> 16) & 0xffffU;
    second = compact.args2 & 0xffffU;
    if (first >= rt->nodeCount || second >= rt->nodeCount) return 0;

    split = compact.args1 & 0xffffU;
    if (((compact.args1 & 0x20000U) == 0U || r->viewY <= (int)split) &&
        ((compact.args1 & 0x10000U) == 0U || r->viewX <= (int)split)) {
        return walkDepthNode(r, rt, first, depth + 1U, w) &&
               walkDepthNode(r, rt, second, depth + 1U, w);
    }
    return walkDepthNode(r, rt, second, depth + 1U, w) &&
           walkDepthNode(r, rt, first, depth + 1U, w);
}

static int spriteLeaf(const EspMapRuntimeView* runtime,
                      const EspMapSprite* sprite,
                      uint32_t* outLeaf) {
    uint32_t nodeIndex = 0U;
    uint32_t depth;

    if (runtime == NULL || sprite == NULL || outLeaf == NULL ||
        runtime->nodeCount == 0U || runtime->nodeCount > MAX_TRACKED_NODES) {
        return 0;
    }

    for (depth = 0U; depth <= MAX_BSP_DEPTH; ++depth) {
        EspMapNode node;
        uint32_t split;
        uint32_t child;

        if (nodeIndex >= runtime->nodeCount ||
            !EspMapRuntime_getNode(nodeIndex, &node)) {
            return 0;
        }
        if ((node.args1 & 0x30000U) == 0U) {
            *outLeaf = nodeIndex;
            return 1;
        }

        split = node.args1 & 0xffffU;
        if ((node.args1 & 0x10000U) != 0U) {
            child = sprite->x > split
                        ? (node.args2 & 0xffffU)
                        : ((node.args2 >> 16) & 0xffffU);
        }
        else if ((node.args1 & 0x20000U) != 0U) {
            child = sprite->y > split
                        ? (node.args2 & 0xffffU)
                        : ((node.args2 >> 16) & 0xffffU);
        }
        else {
            return 0;
        }
        if (child >= runtime->nodeCount) return 0;
        nodeIndex = child;
    }
    return 0;
}

static int isMode7(uint16_t logical) {
    return logical == 136U || logical == 137U || logical == 144U;
}

static void writeLe16(uint8_t* out, uint16_t value) {
    out[0] = (uint8_t)(value & 0xffU);
    out[1] = (uint8_t)((value >> 8) & 0xffU);
}

static void writeLe32(uint8_t* out, uint32_t value) {
    out[0] = (uint8_t)(value & 0xffU);
    out[1] = (uint8_t)((value >> 8) & 0xffU);
    out[2] = (uint8_t)((value >> 16) & 0xffU);
    out[3] = (uint8_t)((value >> 24) & 0xffU);
}

static uint8_t expand5(uint16_t value) {
    value &= 31U;
    return (uint8_t)((value << 3) | (value >> 2));
}

static uint8_t expand6(uint16_t value) {
    value &= 63U;
    return (uint8_t)((value << 2) | (value >> 4));
}

static int dumpSpriteViewportBmp(const Render_t* render, uint32_t* outFNV) {
    static const char* const path = "/sd/junction-sprite-viewport.bmp";
    enum {
        BMP_HEADER_BYTES = 54,
        BMP_WIDTH = 160,
        BMP_HEIGHT = 80,
        BMP_ROW_BYTES = BMP_WIDTH * 3,
        BMP_IMAGE_BYTES = BMP_ROW_BYTES * BMP_HEIGHT,
        BMP_FILE_BYTES = BMP_HEADER_BYTES + BMP_IMAGE_BYTES
    };
    uint8_t header[BMP_HEADER_BYTES] = {0};
    uint8_t row[BMP_ROW_BYTES];
    const uint16_t* framebuffer;
    int pitchPixels;
    FILE* file;
    int x;
    int y;

    if (outFNV != NULL) *outFNV = 0U;
    if (render == NULL || render->framebuffer == NULL ||
        render->screenWidth != BMP_WIDTH || render->screenHeight != BMP_HEIGHT ||
        render->screenX != 0 || render->screenY != 20) {
        return 0;
    }

    file = fopen(path, "wb");
    if (file == NULL) return 0;

    header[0] = 'B';
    header[1] = 'M';
    writeLe32(&header[2], BMP_FILE_BYTES);
    writeLe32(&header[10], BMP_HEADER_BYTES);
    writeLe32(&header[14], 40U);
    writeLe32(&header[18], BMP_WIDTH);
    writeLe32(&header[22], BMP_HEIGHT);
    writeLe16(&header[26], 1U);
    writeLe16(&header[28], 24U);
    writeLe32(&header[34], BMP_IMAGE_BYTES);
    writeLe32(&header[38], 2835U);
    writeLe32(&header[42], 2835U);

    if (fwrite(header, 1U, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        remove(path);
        return 0;
    }

    framebuffer = (const uint16_t*)render->framebuffer;
    pitchPixels = render->pitch >> 1;
    for (y = BMP_HEIGHT - 1; y >= 0; --y) {
        const uint16_t* source = framebuffer +
            (render->screenY + y) * pitchPixels;
        for (x = 0; x < BMP_WIDTH; ++x) {
            const uint16_t color = source[x];
            row[(x * 3) + 0] = expand5(color);
            row[(x * 3) + 1] = expand6(color >> 5);
            row[(x * 3) + 2] = expand5(color >> 11);
        }
        if (fwrite(row, 1U, sizeof(row), file) != sizeof(row)) {
            fclose(file);
            remove(path);
            return 0;
        }
    }

    if (fclose(file) != 0) {
        remove(path);
        return 0;
    }
    if (outFNV != NULL) *outFNV = viewportFNV(render);
    return outFNV == NULL || *outFNV != 0U;
}

void Esp32JunctionSpriteFidelityProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

int Esp32JunctionSpriteFidelityProbe_preOverlayDone(void) {
    return probeState.preDone;
}

int Esp32JunctionSpriteFidelityProbe_postOverlayDone(void) {
    return probeState.postDone;
}

void Esp32JunctionSpriteFidelityProbe_preOverlayService(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    Render_t* render;
    const EspMapRuntimeView* runtime;
    const EspPlayerViewState* view;
    RenderScratch saved;
    RenderScratch restored;
    VisibleWalk walk;
    uint32_t frameBefore;
    uint32_t frameAfter;
    uint32_t candidateFNV = 2166136261U;
    uint32_t candidates = 0U;
    uint32_t rejected = 0U;
    uint32_t hidden = 0U;
    uint32_t candidateMode0 = 0U;
    uint32_t candidateMode7 = 0U;
    uint32_t i;
    int ok = 0;

    if (probeState.preDone || probeState.preAttempted) return;
    if (doomRpg == NULL || doomRpg->render == NULL ||
        !EspMapRuntime_isLoaded() || !EspMapSpriteTopology_isReady() ||
        !EspNativeFirstFrame_isReady()) {
        return;
    }
    probeState.preAttempted = 1;

    render = doomRpg->render;
    runtime = EspMapRuntime_view();
    view = EspPlayerView_view();
    frameBefore = frameFNV(render);
    memset(&walk, 0, sizeof(walk));

    printf("\n=== Doom RPG ESP32-native Junction BSP view-sprite census ===\n");
    printf("[SPRITEVIEW] CONTRACT reproduce legacy Render_relinkSprite leaf ownership plus the exact stateful native first-frame depth/cull walk; read-only diagnostic before sprite overlay\n");

    if (runtime == NULL || view == NULL || runtime->mapSpriteCount != EXPECTED_MAP_SPRITES ||
        runtime->nodeCount == 0U || runtime->nodeCount > MAX_TRACKED_NODES ||
        render->columnScale == NULL || frameBefore != EXPECTED_BASE_FRAME_FNV) {
        printf("[SPRITEVIEW] FAILED boundary frame=%08x sprites=%u nodes=%u\n",
               (unsigned int)frameBefore,
               (unsigned int)(runtime ? runtime->mapSpriteCount : 0U),
               (unsigned int)(runtime ? runtime->nodeCount : 0U));
        return;
    }

    saveScratch(render, &saved);
    if (!setupView(render, view) ||
        !walkDepthNode(render, runtime, 0U, 0U, &walk)) {
        goto restore;
    }

    for (i = 0U; i < runtime->mapSpriteCount; ++i) {
        EspMapSprite sprite;
        uint8_t visualState;
        uint32_t info;
        uint32_t logical;
        uint32_t leaf;
        int admitted;

        if (!EspMapRuntime_getMapSprite(i, &sprite) ||
            !EspMapSpriteTopology_getVisualState(i, &visualState) ||
            !spriteLeaf(runtime, &sprite, &leaf)) {
            goto restore;
        }
        info = (sprite.info & ~VISUAL_MASK) | ((uint32_t)visualState << 9);
        logical = info & 511U;
        if ((info & HIDDEN) != 0U) {
            ++hidden;
            continue;
        }

        admitted = (walk.leafBits[leaf >> 5] & (1U << (leaf & 31U))) != 0U;
        if (!admitted) {
            ++rejected;
            continue;
        }

        ++candidates;
        if (isMode7((uint16_t)logical)) ++candidateMode7;
        else ++candidateMode0;
        candidateFNV = fnvAppend(candidateFNV, &i, sizeof(i));
        candidateFNV = fnvAppend(candidateFNV, &leaf, sizeof(leaf));
        candidateFNV = fnvAppend(candidateFNV, &logical, sizeof(logical));
        printf("[SPRITEVIEW] ITEM i=%u leaf=%u logical=%u mode=%u pos=%u,%u\n",
               (unsigned int)i, (unsigned int)leaf,
               (unsigned int)logical,
               isMode7((uint16_t)logical) ? 7U : 0U,
               (unsigned int)sprite.x, (unsigned int)sprite.y);
    }
    ok = 1;

restore:
    restoreScratch(render, &saved);
    saveScratch(render, &restored);
    frameAfter = frameFNV(render);
    if (memcmp(&saved, &restored, sizeof(saved)) != 0) ok = 0;

    if (!ok || frameAfter != frameBefore ||
        walk.nodes != EXPECTED_WALK_NODES ||
        walk.leaves != EXPECTED_WALK_LEAVES ||
        walk.nodeCull != EXPECTED_WALK_NODE_CULL ||
        walk.lines != EXPECTED_WALK_LINES ||
        walk.backface != EXPECTED_WALK_BACKFACE ||
        walk.clip != EXPECTED_WALK_CLIP ||
        candidates + rejected + hidden != EXPECTED_MAP_SPRITES ||
        candidates == 0U) {
        printf("[SPRITEVIEW] FAILED walk=%u/%u/%u lines=%u backface=%u clip=%u candidates=%u rejected=%u hidden=%u frame=%08x->%08x\n",
               (unsigned int)walk.nodes, (unsigned int)walk.leaves,
               (unsigned int)walk.nodeCull, (unsigned int)walk.lines,
               (unsigned int)walk.backface, (unsigned int)walk.clip,
               (unsigned int)candidates, (unsigned int)rejected,
               (unsigned int)hidden, (unsigned int)frameBefore,
               (unsigned int)frameAfter);
        return;
    }

    printf("[SPRITEVIEW] READY walk=nodes:%u leaves:%u nodeCull:%u lines:%u backface:%u clip:%u mapSprites=%u candidates=%u bspRejected=%u hidden=%u modes=0:%u/7:%u candidateFNV=%08x framebuffer=%08x untouched=yes renderScratchStable=yes\n",
           (unsigned int)walk.nodes, (unsigned int)walk.leaves,
           (unsigned int)walk.nodeCull, (unsigned int)walk.lines,
           (unsigned int)walk.backface, (unsigned int)walk.clip,
           (unsigned int)runtime->mapSpriteCount,
           (unsigned int)candidates, (unsigned int)rejected,
           (unsigned int)hidden, (unsigned int)candidateMode0,
           (unsigned int)candidateMode7, (unsigned int)candidateFNV,
           (unsigned int)frameAfter);
    probeState.preDone = 1;
}

void Esp32JunctionSpriteFidelityProbe_postOverlayService(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    Render_t* render;
    uint32_t frame;
    uint32_t viewport = 0U;

    if (probeState.postDone || probeState.postAttempted || !probeState.preDone ||
        !Esp32JunctionSpriteOverlayProbe_isDone()) {
        return;
    }
    probeState.postAttempted = 1;
    render = doomRpg != NULL ? doomRpg->render : NULL;
    frame = frameFNV(render);

    if (render == NULL || frame != EXPECTED_MODE7_FRAME_FNV ||
        !dumpSpriteViewportBmp(render, &viewport)) {
        printf("[JUNCTIONSPRITE] BMP FAILED frame=%08x expected=%08x viewportFNV=%08x\n",
               (unsigned int)frame, (unsigned int)EXPECTED_MODE7_FRAME_FNV,
               (unsigned int)viewport);
        return;
    }

    printf("[JUNCTIONSPRITE] BMP READY path=/junction-sprite-viewport.bmp size=38454 viewport=160x80@0,20 viewportFNV=%08x frame=%08x postParkDiagnostic=yes\n",
           (unsigned int)viewport, (unsigned int)frame);
    probeState.postDone = 1;
}
