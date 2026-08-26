#include <SDL.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_map_runtime.h"
#include "esp_native_bsp_visibility.h"
#include "esp_native_door_view_probe.h"
#include "esp_player_view_state.h"

#define ENTRANCE_TEXTURE_ID 7U

#define LINE_FLAG_TWO_SIDED 0x00000001UL
#define LINE_FLAG_AXIS_X 0x00000008UL
#define LINE_FLAG_AXIS_NEGATIVE 0x00000010UL
#define LINE_FLAG_Y_NUDGE 0x00000100UL
#define LINE_FLAG_X_NUDGE 0x00000200UL
#define LINE_FLAG_REVERSE_TEX 0x00008000UL

/* Diagnostic branch only. Keep the visibility witness out of loopTask stack;
 * the underlying builder already restores every borrowed Render scratch field. */
static EspNativeBspVisibilityState doorVisibility;
static uint8_t doorProbeBusy;

static void nudgedEndpoints(const EspMapLine* line,
                            int32_t* x1,
                            int32_t* y1,
                            int32_t* x2,
                            int32_t* y2) {
    *x1 = (int32_t)line->x1;
    *y1 = (int32_t)line->y1;
    *x2 = (int32_t)line->x2;
    *y2 = (int32_t)line->y2;

    if ((line->flags & LINE_FLAG_X_NUDGE) != 0U) {
        if ((line->flags & LINE_FLAG_AXIS_X) != 0U) {
            *x1 += 3;
            *x2 += 3;
        }
        else if ((line->flags & LINE_FLAG_AXIS_NEGATIVE) != 0U) {
            *x1 -= 3;
            *x2 -= 3;
        }
    }
    else if ((line->flags & LINE_FLAG_Y_NUDGE) != 0U) {
        if ((line->flags & LINE_FLAG_AXIS_X) != 0U) {
            *y1 += 3;
            *y2 += 3;
        }
        else if ((line->flags & LINE_FLAG_AXIS_NEGATIVE) != 0U) {
            *y1 -= 3;
            *y2 -= 3;
        }
    }
}

static int findOwningLeaf(const EspMapRuntimeView* runtime,
                          uint32_t lineIndex,
                          uint32_t* outLeaf,
                          int* outVisible) {
    uint32_t nodeIndex;

    if (runtime == NULL || outLeaf == NULL || outVisible == NULL) return 0;
    for (nodeIndex = 0U; nodeIndex < runtime->nodeCount; ++nodeIndex) {
        EspMapNode node;
        uint32_t lineStart;
        uint32_t lineCount;

        if (!EspMapRuntime_getNode(nodeIndex, &node)) return 0;
        if ((node.args1 & 0x30000U) != 0U) continue;

        lineStart = node.args2 & 0xffffU;
        lineCount = (node.args2 >> 16) & 0xffffU;
        if (lineIndex < lineStart || lineIndex >= lineStart + lineCount) continue;

        *outLeaf = nodeIndex;
        *outVisible =
            (doorVisibility.visibleLeaves[nodeIndex >> 5] &
             (1U << (nodeIndex & 31U))) != 0U;
        return 1;
    }
    return 0;
}

static const char* sideDecision(int64_t side, uint32_t flags) {
    if (side > 0) return "FRONT";
    if ((flags & LINE_FLAG_TWO_SIDED) != 0U) return "SWAP";
    return "BACKFACE_CULL";
}

int EspNativeDoorViewProbe_log(struct Render_s* renderBase,
                               uint32_t viewportFNV) {
    Render_t* render = (Render_t*)renderBase;
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspPlayerViewState* view = EspPlayerView_view();
    uint32_t entranceCount = 0U;
    uint32_t visibleEntranceCount = 0U;
    uint32_t i;
    int sin_;
    int cos_;
    int32_t cameraX;
    int32_t cameraY;
    int ok = 0;

    if (doorProbeBusy || render == NULL || runtime == NULL || view == NULL ||
        view->active != 1U || runtime->nodeCount == 0U ||
        runtime->nodeCount > ESP_NATIVE_BSP_VISIBILITY_MAX_NODES ||
        runtime->lineCount == 0U) {
        printf("[DOORVIEW] SKIP busy=%u render=%s runtime=%s view=%s active=%u nodes=%u lines=%u maxNodes=%u\n",
               (unsigned int)doorProbeBusy,
               render != NULL ? "yes" : "no",
               runtime != NULL ? "yes" : "no",
               view != NULL ? "yes" : "no",
               view != NULL ? (unsigned int)view->active : 0U,
               runtime != NULL ? (unsigned int)runtime->nodeCount : 0U,
               runtime != NULL ? (unsigned int)runtime->lineCount : 0U,
               (unsigned int)ESP_NATIVE_BSP_VISIBILITY_MAX_NODES);
        return 0;
    }

    doorProbeBusy = 1U;
    memset(&doorVisibility, 0, sizeof(doorVisibility));
    if (!EspNativeBspVisibility_build(render, &doorVisibility)) goto done;

    sin_ = render->sinTable[view->viewAngle & 255];
    cos_ = render->sinTable[(view->viewAngle + 64) & 255];
    cameraX = view->viewX - ((16 * cos_) >> 16);
    cameraY = view->viewY + ((16 * sin_) >> 16);

    for (i = 0U; i < runtime->lineCount; ++i) {
        EspMapLine line;
        uint32_t leaf = UINT32_MAX;
        int leafVisible = 0;
        int32_t x1;
        int32_t y1;
        int32_t x2;
        int32_t y2;
        int32_t midX;
        int32_t midY;
        int32_t deltaX;
        int32_t deltaY;
        int64_t side;

        if (!EspMapRuntime_getLine(i, &line)) goto done;
        if (line.texture != ENTRANCE_TEXTURE_ID) continue;
        ++entranceCount;

        nudgedEndpoints(&line, &x1, &y1, &x2, &y2);
        side = ((int64_t)(x1 - cameraX) * (int64_t)(y2 - y1)) +
               ((int64_t)(y1 - cameraY) * (int64_t)(-(x2 - x1)));
        if (!findOwningLeaf(runtime, i, &leaf, &leafVisible)) goto done;
        if (leafVisible) ++visibleEntranceCount;

        midX = x1 + ((x2 - x1) / 2);
        midY = y1 + ((y2 - y1) / 2);
        deltaX = midX - cameraX;
        deltaY = midY - cameraY;

        printf("[DOORVIEW] LINE idx=%u leaf=%u visible=%s raw=%u,%u->%u,%u nudged=%d,%d->%d,%d flags=%08x side=%lld decision=%s reverseTex=%s midDelta=%d,%d\n",
               (unsigned int)i,
               (unsigned int)leaf,
               leafVisible ? "yes" : "no",
               (unsigned int)line.x1,
               (unsigned int)line.y1,
               (unsigned int)line.x2,
               (unsigned int)line.y2,
               (int)x1,
               (int)y1,
               (int)x2,
               (int)y2,
               (unsigned int)line.flags,
               (long long)side,
               sideDecision(side, line.flags),
               (line.flags & LINE_FLAG_REVERSE_TEX) != 0U ? "yes" : "no",
               (int)deltaX,
               (int)deltaY);
    }

    printf("[DOORVIEW] FRAME player=%d,%d,%d angle=%d camera=%d,%d viewport=%08x entrance=%u visibleEntrance=%u bspNodes=%u leaves=%u nodeCull=%u lines=%u backface=%u clip=%u occluders=%u spriteSpans=%u\n",
           (int)view->viewX,
           (int)view->viewY,
           (int)view->viewZ,
           (int)view->viewAngle,
           (int)cameraX,
           (int)cameraY,
           (unsigned int)viewportFNV,
           (unsigned int)entranceCount,
           (unsigned int)visibleEntranceCount,
           (unsigned int)doorVisibility.nodes,
           (unsigned int)doorVisibility.leaves,
           (unsigned int)doorVisibility.nodeCull,
           (unsigned int)doorVisibility.lines,
           (unsigned int)doorVisibility.backfaceCull,
           (unsigned int)doorVisibility.clipCull,
           (unsigned int)doorVisibility.occluders,
           (unsigned int)doorVisibility.spriteSpans);

    ok = entranceCount != 0U;

done:
    if (!ok) {
        printf("[DOORVIEW] FAIL runtime=%s view=%s nodes=%u lines=%u\n",
               runtime != NULL ? "yes" : "no",
               view != NULL ? "yes" : "no",
               runtime != NULL ? (unsigned int)runtime->nodeCount : 0U,
               runtime != NULL ? (unsigned int)runtime->lineCount : 0U);
    }
    doorProbeBusy = 0U;
    return ok;
}
