#include <SDL.h>

#include <stdint.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_map_runtime.h"
#include "esp_native_bsp_visibility.h"
#include "esp_player_view_state.h"

#define MAX_BSP_DEPTH 64U
#define SCREEN_W 160
#define SCREEN_H 80

#define TWO_SIDED 0x00000001UL
#define SPRITE_SPAN 0x00000002UL
#define AXIS_X 0x00000008UL
#define AXIS_NEG 0x00000010UL
#define Y_NUDGE 0x00000100UL
#define X_NUDGE 0x00000200UL
#define OCCLUDER 0x20000000UL

typedef struct Scratch_s {
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
} Scratch;

static void saveScratch(Render_t* r, Scratch* s) {
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

static void restoreScratch(Render_t* r, const Scratch* s) {
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
        r->screenHeight != SCREEN_H || r->screenX != 0 || r->screenY != 20) {
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
    r->screenBottom = SCREEN_H;
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

static void sourceLine(const EspMapLine* src, Line_t* line) {
    memset(line, 0, sizeof(*line));
    line->vert1.x = src->x1;
    line->vert1.y = src->y1;
    line->vert2.x = src->x2;
    line->vert2.y = src->y2;
    line->flags = (int)src->flags;

    if ((src->flags & X_NUDGE) != 0U) {
        if ((src->flags & AXIS_X) != 0U) {
            line->vert1.x += 3;
            line->vert2.x += 3;
        }
        else if ((src->flags & AXIS_NEG) != 0U) {
            line->vert1.x -= 3;
            line->vert2.x -= 3;
        }
    }
    else if ((src->flags & Y_NUDGE) != 0U) {
        if ((src->flags & AXIS_X) != 0U) {
            line->vert1.y += 3;
            line->vert2.y += 3;
        }
        else if ((src->flags & AXIS_NEG) != 0U) {
            line->vert1.y -= 3;
            line->vert2.y -= 3;
        }
    }
}

static int depthColumns(Render_t* r, Line_t* line) {
    int dx = line->vert2.x - line->vert1.x;
    int step;
    int dDepth;
    int x;
    int x2;
    int depth;

    if (dx <= 0) return 1;
    step = (MAXINT / dx) << 1;
    dDepth = (int)DoomRPG_FixedMul(line->vert2.y - line->vert1.y, step);
    x = (line->vert1.x + 65535) >> 16;
    x2 = (line->vert2.x + 65535) >> 16;
    if (x < r->screenLeft) x = r->screenLeft;
    if (x2 > r->screenRight) x2 = r->screenRight;
    depth = line->vert1.y +
            DoomRPG_FixedMul((x << 16) - line->vert1.x, dDepth);

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

static int depthLine(Render_t* r,
                     uint32_t lineIndex,
                     EspNativeBspVisibilityState* state) {
    EspMapLine src;
    Line_t line;
    Vertex_t tmp;

    if (!EspMapRuntime_getLine(lineIndex, &src)) return 0;
    sourceLine(&src, &line);
    r->numLines = (int)lineIndex;
    ++state->lines;

    if (((line.vert1.x - r->viewX) * (line.vert2.y - line.vert1.y)) +
        ((line.vert1.y - r->viewY) * (-(line.vert2.x - line.vert1.x))) <= 0) {
        if ((line.flags & TWO_SIDED) == 0) {
            ++state->backfaceCull;
            return 1;
        }
        tmp = line.vert1;
        line.vert1 = line.vert2;
        line.vert2 = tmp;
    }

    Render_transform2DVerts(r, &line.vert1);
    Render_transform2DVerts(r, &line.vert2);
    if (!Render_clipLine(r, &line)) {
        ++state->clipCull;
        return 1;
    }
    Render_projectVertex(r, &line.vert1);
    Render_projectVertex(r, &line.vert2);

    if ((line.flags & OCCLUDER) != 0) {
        Render_occludeClippedLine(r, &line);
        ++state->occluders;
        return 1;
    }
    if ((line.flags & SPRITE_SPAN) != 0) {
        ++state->spriteSpans;
        return 1;
    }
    return depthColumns(r, &line);
}

static int walkNode(Render_t* r,
                    const EspMapRuntimeView* runtime,
                    uint32_t nodeIndex,
                    uint32_t depth,
                    EspNativeBspVisibilityState* state) {
    EspMapNode compact;
    Node_t node;
    uint32_t lineStart;
    uint32_t lineCount;
    uint32_t first;
    uint32_t second;
    uint32_t split;
    uint32_t i;

    if (r == NULL || runtime == NULL || state == NULL ||
        depth > MAX_BSP_DEPTH ||
        runtime->nodeCount > ESP_NATIVE_BSP_VISIBILITY_MAX_NODES ||
        nodeIndex >= runtime->nodeCount ||
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
    ++state->nodes;
    if (Render_cullBoundingBox(r, &node)) {
        ++state->nodeCull;
        return 1;
    }

    if ((compact.args1 & 0x30000U) == 0U) {
        lineStart = compact.args2 & 0xffffU;
        lineCount = (compact.args2 >> 16) & 0xffffU;
        if (lineStart > runtime->lineCount ||
            lineCount > runtime->lineCount - lineStart) {
            return 0;
        }
        state->visibleLeaves[nodeIndex >> 5] |= 1U << (nodeIndex & 31U);
        ++r->nodeRasterCount;
        ++state->leaves;
        r->lineCount += (int)lineCount;
        for (i = 0U; i < lineCount; ++i) {
            if (!depthLine(r, lineStart + i, state)) return 0;
        }
        return 1;
    }

    first = (compact.args2 >> 16) & 0xffffU;
    second = compact.args2 & 0xffffU;
    if (first >= runtime->nodeCount || second >= runtime->nodeCount) return 0;

    split = compact.args1 & 0xffffU;
    if (((compact.args1 & 0x20000U) == 0U || r->viewY <= (int)split) &&
        ((compact.args1 & 0x10000U) == 0U || r->viewX <= (int)split)) {
        return walkNode(r, runtime, first, depth + 1U, state) &&
               walkNode(r, runtime, second, depth + 1U, state);
    }
    return walkNode(r, runtime, second, depth + 1U, state) &&
           walkNode(r, runtime, first, depth + 1U, state);
}

static int spriteLeaf(const EspMapRuntimeView* runtime,
                      const EspMapSprite* sprite,
                      uint32_t* outLeaf) {
    uint32_t nodeIndex = 0U;
    uint32_t depth;

    if (runtime == NULL || sprite == NULL || outLeaf == NULL ||
        runtime->nodeCount == 0U ||
        runtime->nodeCount > ESP_NATIVE_BSP_VISIBILITY_MAX_NODES) {
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

int EspNativeBspVisibility_build(struct Render_s* renderBase,
                                 EspNativeBspVisibilityState* outState) {
    Render_t* render = (Render_t*)renderBase;
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspPlayerViewState* view = EspPlayerView_view();
    Scratch before;
    Scratch after;
    int ok = 0;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (render == NULL || outState == NULL || runtime == NULL || view == NULL ||
        render->framebuffer == NULL || render->columnScale == NULL ||
        render->screenWidth != SCREEN_W || render->screenHeight != SCREEN_H ||
        render->screenX != 0 || render->screenY != 20 ||
        !EspMapRuntime_isLoaded() || runtime->nodeCount == 0U ||
        runtime->nodeCount > ESP_NATIVE_BSP_VISIBILITY_MAX_NODES ||
        runtime->lineCount == 0U) {
        return 0;
    }

    saveScratch(render, &before);
    if (setupView(render, view) && walkNode(render, runtime, 0U, 0U, outState)) {
        memcpy(outState->columnScale, render->columnScale,
               sizeof(outState->columnScale));
        ok = 1;
    }
    restoreScratch(render, &before);
    saveScratch(render, &after);
    if (memcmp(&before, &after, sizeof(before)) != 0) ok = 0;
    if (!ok) memset(outState, 0, sizeof(*outState));
    return ok;
}

int EspNativeBspVisibility_mapSpriteVisible(
    const EspNativeBspVisibilityState* state,
    uint32_t mapSpriteIndex,
    uint32_t* outLeafIndex) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    EspMapSprite sprite;
    uint32_t leaf;

    if (outLeafIndex != NULL) *outLeafIndex = UINT32_MAX;
    if (state == NULL || runtime == NULL ||
        mapSpriteIndex >= runtime->mapSpriteCount ||
        !EspMapRuntime_getMapSprite(mapSpriteIndex, &sprite) ||
        !spriteLeaf(runtime, &sprite, &leaf) ||
        leaf >= ESP_NATIVE_BSP_VISIBILITY_MAX_NODES) {
        return 0;
    }
    if (outLeafIndex != NULL) *outLeafIndex = leaf;
    return (state->visibleLeaves[leaf >> 5] & (1U << (leaf & 31U))) != 0U;
}
