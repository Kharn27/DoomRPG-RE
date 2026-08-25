#include <SDL.h>
#include "DoomRPG.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "DoomCanvas.h"
#include "Game.h"
#include "Hud.h"
#include "Player.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_map_line_state.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_native_first_frame.h"
#include "esp_native_graphics_catalog.h"
#include "esp_native_plane_renderer.h"
#include "esp_native_playing_service_state.h"
#include "esp_player_view_state.h"
#include "native_junction_first_frame_corrected_probe.h"
#include "native_junction_graphics_catalog_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_RESIDENT_FNV 0xbb714d80U
#define EXPECTED_RUNTIME_FNV 0xbc432a0fU
#define EXPECTED_MAP_FNV 0x8dba0bb4U
#define EXPECTED_SCRIPT_FNV 0xbc9b18ffU
#define EXPECTED_LINE_FNV 0x3658710dU
#define EXPECTED_TEXTURE_STATE_FNV 0x537319adU
#define EXPECTED_AUTOMAP_FNV 0xb699bd75U
#define EXPECTED_TOPOLOGY_FNV 0xd6e8df7dU
#define EXPECTED_PLAYING_SERVICE_FNV 0x4c50b853U
#define EXPECTED_PLAYER_VIEW_FNV 0xafcdcf74U
#define EXPECTED_CATALOG_FNV 0x969d5a77U
#define EXPECTED_TEXTURE_FNV 0x2dd5dfcfU
#define EXPECTED_SPRITE_FNV 0xcfd036cfU
#define EXPECTED_CATALOG_RECORD_BYTES 40U
#define EXPECTED_TEXTURE_COUNT 30U
#define EXPECTED_SPRITE_COUNT 16U
#define EXPECTED_CATALOG_STORAGE 1840U
#define EXPECTED_FRAME_STATE_BYTES 48U
#define EXPECTED_LEAVES 12U
#define EXPECTED_LINE_CANDIDATES 62U
#define EXPECTED_WALL_REQUESTS 34U
#define EXPECTED_WALL_DRAWS 34U
#define EXPECTED_SPANS 166U
#define EXPECTED_WALL_PIXELS 4341U
#define EXPECTED_WALL_CACHE_HITS 17U
#define EXPECTED_WALL_CACHE_MISSES 17U
#define EXPECTED_PLANE_ROWS 80U
#define EXPECTED_PLANE_PIXELS 12800U
#define JUNCTION_TARGET_MAP 9U

static struct {
    int attempted;
    int done;
} probeState;

static uint32_t hashBytes(const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t framebufferHash(void) {
    const void* fb = Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    if (fb == NULL || bytes !=
        (size_t)DOOMRPG_LOGICAL_WIDTH * (size_t)DOOMRPG_LOGICAL_HEIGHT *
            sizeof(uint16_t)) return 0U;
    return hashBytes(fb, (uint32_t)bytes);
}

static uint32_t columnHash(const Render_t* render) {
    if (render == NULL || render->columnScale == NULL ||
        render->screenWidth <= 0 || render->screenWidth > 160) return 0U;
    return hashBytes(render->columnScale,
                     (uint32_t)render->screenWidth * sizeof(int));
}

static int legacyGraphicsClear(const Render_t* render) {
    return render != NULL && render->lines == NULL && render->nodes == NULL &&
           render->mapSprites == NULL && render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL && render->mediaSpriteIds == NULL &&
           render->shapeData == NULL && render->mediaTexels == NULL &&
           render->mapTextureTexels == NULL && render->mapSpriteTexels == NULL;
}

static int residentCanonical(const EspMapResidentSnapshot* s) {
    return s != NULL && sizeof(*s) == 96U &&
           s->runtimeFNV1a == EXPECTED_RUNTIME_FNV &&
           s->mapStateFNV1a == EXPECTED_MAP_FNV &&
           s->scriptStateFNV1a == EXPECTED_SCRIPT_FNV &&
           s->lineStateFNV1a == EXPECTED_LINE_FNV &&
           s->textureStateFNV1a == EXPECTED_TEXTURE_STATE_FNV &&
           s->automapStateFNV1a == EXPECTED_AUTOMAP_FNV &&
           s->topologyFNV1a == EXPECTED_TOPOLOGY_FNV &&
           s->totalPayloadBytes == 10410U && s->entityCount == 30U &&
           s->enemyCount == 0U && s->destructibleCount == 3U &&
           hashBytes(s, sizeof(*s)) == EXPECTED_RESIDENT_FNV;
}

static int playingCanonical(void) {
    const EspNativePlayingServiceState* s = EspNativePlayingService_view();
    return s != NULL && sizeof(*s) == 12U &&
           hashBytes(s, sizeof(*s)) == EXPECTED_PLAYING_SERVICE_FNV &&
           s->nativeState == 3U && s->serviceOrdinal == 1U &&
           s->inputCountBefore == 0U && s->inputConsumed == 0U &&
           s->gameplayDispatched == 0U && s->renderIntent == 1U &&
           s->targetMapId == JUNCTION_TARGET_MAP && s->active == 1U;
}

static int playerViewCanonical(void) {
    const EspPlayerViewState* s = EspPlayerView_view();
    return s != NULL && sizeof(*s) == 44U &&
           hashBytes(s, sizeof(*s)) == EXPECTED_PLAYER_VIEW_FNV &&
           s->viewX == 992 && s->viewY == 1888 && s->viewZ == 36 &&
           s->viewAngle == 64 && s->destX == s->viewX &&
           s->destY == s->viewY && s->destAngle == s->viewAngle &&
           s->targetMapId == JUNCTION_TARGET_MAP &&
           s->hudRefreshPending == 0U && s->facingRefreshPending == 0U &&
           s->playerSetupPending == 0U && s->tileEnterPending == 0U &&
           s->active == 1U;
}

static int catalogCanonical(uint32_t* textureFNV, uint32_t* spriteFNV) {
    const EspNativeGraphicsCatalogView* v = EspNativeGraphicsCatalog_view();
    uint32_t t;
    uint32_t s;
    if (v == NULL || sizeof(EspNativeGraphicsCatalogRecord) !=
                         EXPECTED_CATALOG_RECORD_BYTES ||
        v->textureCount != EXPECTED_TEXTURE_COUNT ||
        v->spriteCount != EXPECTED_SPRITE_COUNT ||
        v->storageBytes != EXPECTED_CATALOG_STORAGE ||
        v->stateFNV1a != EXPECTED_CATALOG_FNV) return 0;
    t = hashBytes(v->textures,
                  (uint32_t)v->textureCount *
                      sizeof(EspNativeGraphicsCatalogRecord));
    s = hashBytes(v->sprites,
                  (uint32_t)v->spriteCount *
                      sizeof(EspNativeGraphicsCatalogRecord));
    if (textureFNV != NULL) *textureFNV = t;
    if (spriteFNV != NULL) *spriteFNV = s;
    return t == EXPECTED_TEXTURE_FNV && s == EXPECTED_SPRITE_FNV;
}

void Esp32JunctionFirstFrameCorrectedProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspNativeFirstFrame_reset();
    EspNativePlaneRenderer_reset();
}

int Esp32JunctionFirstFrameCorrectedProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionFirstFrameCorrectedProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    EspMapResidentSnapshot residentBefore, residentAfter;
    const EspMapLineStateView* lineState;
    const EspPlayerViewState* playerView;
    const EspNativeFirstFrameState* frame;
    const EspNativePlaneRenderStats* planes;
    EspNativeFirstFrameState frameCopy;
    uint32_t textureFNV = 0U, spriteFNV = 0U;
    uint32_t frameBefore, frameAfter, stateFNV;
    uint32_t heapBefore, heapAfter, largestBefore, largestAfter;
    uint32_t gameBefore, gameAfter, playerBefore, playerAfter;
    uint32_t hudBefore, hudAfter, canvasBefore, canvasAfter;
    uint32_t renderBefore, renderAfter, columnBefore, columnAfter;
    uint32_t paletteBefore, paletteAfter, catalogBefore, catalogAfter;
    int nullRender, nullView, wrongMap, repeat, repeatAtomic;
    int geometryOk, planeOk, integrityOk;
    EspNativeFirstFrameStatus status;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionGraphicsCatalogProbe_isDone()) return;
    probeState.attempted = 1;

    printf("\n=== Doom RPG ESP32-native Junction first gameplay frame v2 ===\n");

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->player == NULL || doomRpg->hud == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO ||
        doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->doomCanvas->numEvents != 0 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        !legacyGraphicsClear(doomRpg->render) ||
        doomRpg->render->framebuffer !=
            (byte*)Esp32PlatformVideo_framebuffer() ||
        doomRpg->render->screenWidth != 160 ||
        doomRpg->render->screenHeight != 80 ||
        doomRpg->render->screenX != 0 || doomRpg->render->screenY != 20 ||
        EspAssetPack_isOpen() || EspNativeFirstFrame_isReady() ||
        sizeof(EspNativeFirstFrameState) != EXPECTED_FRAME_STATE_BYTES ||
        !playingCanonical() || !playerViewCanonical() ||
        !catalogCanonical(&textureFNV, &spriteFNV) ||
        !EspMapResidentLifecycle_capture(&residentBefore) ||
        !residentCanonical(&residentBefore)) {
        printf("[JUNCTIONFRAME] FAILED unsafe boundary state=%d page=%d entities=%d monsters=%d legacyClear=%d playing=%d view=%d catalog=%d resident=%d\n",
               doomRpg && doomRpg->doomCanvas ? doomRpg->doomCanvas->state : -1,
               doomRpg && doomRpg->doomCanvas ? doomRpg->doomCanvas->storyPage : -1,
               doomRpg && doomRpg->game ? doomRpg->game->numEntities : -1,
               doomRpg && doomRpg->game ? doomRpg->game->numMonsters : -1,
               doomRpg && doomRpg->render ? legacyGraphicsClear(doomRpg->render) : 0,
               playingCanonical(), playerViewCanonical(),
               catalogCanonical(NULL, NULL),
               EspMapResidentLifecycle_capture(&residentAfter) &&
                   residentCanonical(&residentAfter));
        return;
    }

    lineState = EspMapLineState_view();
    playerView = EspPlayerView_view();
    if (lineState == NULL || lineState->stateFNV1a != EXPECTED_LINE_FNV ||
        lineState->openCount != 0U || playerView == NULL) {
        printf("[JUNCTIONFRAME] FAILED mutable-world gate lineFNV=%08x open=%u\n",
               (unsigned int)(lineState ? lineState->stateFNV1a : 0U),
               (unsigned int)(lineState ? lineState->openCount : 0U));
        return;
    }

    frameBefore = framebufferHash();
    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    gameBefore = hashBytes(doomRpg->game, sizeof(*doomRpg->game));
    playerBefore = hashBytes(doomRpg->player, sizeof(*doomRpg->player));
    hudBefore = hashBytes(doomRpg->hud, sizeof(*doomRpg->hud));
    canvasBefore = hashBytes(doomRpg->doomCanvas, sizeof(*doomRpg->doomCanvas));
    renderBefore = hashBytes(doomRpg->render, sizeof(*doomRpg->render));
    columnBefore = columnHash(doomRpg->render);
    paletteBefore = hashBytes(doomRpg->render->mediaPalettes,
                              (uint32_t)doomRpg->render->mediaPalettesLength * 2U);
    catalogBefore = EspNativeGraphicsCatalog_view()->stateFNV1a;

    nullRender = EspNativeFirstFrame_route(NULL, playerView) ==
                 ESP_NATIVE_FIRST_FRAME_INVALID;
    nullView = EspNativeFirstFrame_route(doomRpg->render, NULL) ==
               ESP_NATIVE_FIRST_FRAME_INVALID;
    {
        EspPlayerViewState bad = *playerView;
        bad.targetMapId = 8U;
        wrongMap = EspNativeFirstFrame_route(doomRpg->render, &bad) ==
                   ESP_NATIVE_FIRST_FRAME_RENDER_FAILED;
    }
    if (!nullRender || !nullView || !wrongMap ||
        framebufferHash() != frameBefore || EspNativeFirstFrame_isReady() ||
        EspNativePlaneRenderer_view() != NULL) {
        printf("[JUNCTIONFRAME] FAILED fail-closed preflight nullRender=%d nullView=%d wrongMap=%d\n",
               nullRender, nullView, wrongMap);
        return;
    }

    status = EspNativeFirstFrame_route(doomRpg->render, playerView);
    if (status != ESP_NATIVE_FIRST_FRAME_OK) {
        printf("[JUNCTIONFRAME] FAILED route status=%d frame=%08x packOpen=%d\n",
               (int)status, (unsigned int)framebufferHash(), EspAssetPack_isOpen());
        return;
    }

    frame = EspNativeFirstFrame_view();
    planes = EspNativePlaneRenderer_view();
    if (frame == NULL || planes == NULL) {
        printf("[JUNCTIONFRAME] FAILED missing frame/plane owner frame=%p planes=%p\n",
               (const void*)frame, (const void*)planes);
        return;
    }

    frameCopy = *frame;
    stateFNV = hashBytes(frame, sizeof(*frame));
    repeat = EspNativeFirstFrame_route(doomRpg->render, playerView) ==
             ESP_NATIVE_FIRST_FRAME_ALREADY_ACTIVE;
    repeatAtomic = memcmp(frame, &frameCopy, sizeof(frameCopy)) == 0;

    frameAfter = framebufferHash();
    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfter = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    gameAfter = hashBytes(doomRpg->game, sizeof(*doomRpg->game));
    playerAfter = hashBytes(doomRpg->player, sizeof(*doomRpg->player));
    hudAfter = hashBytes(doomRpg->hud, sizeof(*doomRpg->hud));
    canvasAfter = hashBytes(doomRpg->doomCanvas, sizeof(*doomRpg->doomCanvas));
    renderAfter = hashBytes(doomRpg->render, sizeof(*doomRpg->render));
    columnAfter = columnHash(doomRpg->render);
    paletteAfter = hashBytes(doomRpg->render->mediaPalettes,
                             (uint32_t)doomRpg->render->mediaPalettesLength * 2U);
    catalogAfter = EspNativeGraphicsCatalog_view()->stateFNV1a;

    if (!EspMapResidentLifecycle_capture(&residentAfter)) {
        printf("[JUNCTIONFRAME] FAILED resident capture after frame\n");
        return;
    }

    geometryOk = frame->leafNodes == EXPECTED_LEAVES &&
                 frame->lineCandidates == EXPECTED_LINE_CANDIDATES &&
                 frame->wallRequests == EXPECTED_WALL_REQUESTS &&
                 frame->wallDraws == EXPECTED_WALL_DRAWS &&
                 frame->spanCalls == EXPECTED_SPANS &&
                 frame->pixelsDrawn == EXPECTED_WALL_PIXELS &&
                 frame->cacheHits == EXPECTED_WALL_CACHE_HITS &&
                 frame->cacheMisses == EXPECTED_WALL_CACHE_MISSES;
    planeOk = planes->active == 1U && planes->rendered == 1U &&
              planes->rowsRendered == EXPECTED_PLANE_ROWS &&
              planes->pixelsRendered == EXPECTED_PLANE_PIXELS &&
              planes->uniqueLogicalTextures > 0U &&
              planes->uniqueLogicalTextures <= 24U &&
              planes->cacheMisses > 0U &&
              planes->texelReadBytes == planes->cacheMisses * 2048U;
    integrityOk = frameBefore != frameAfter && frameAfter == frame->frameAfterFNV &&
                  frameBefore == frame->frameBeforeFNV &&
                  heapBefore == heapAfter && largestBefore == largestAfter &&
                  gameBefore == gameAfter && playerBefore == playerAfter &&
                  hudBefore == hudAfter && canvasBefore == canvasAfter &&
                  renderBefore == renderAfter && columnBefore == columnAfter &&
                  paletteBefore == paletteAfter &&
                  catalogBefore == EXPECTED_CATALOG_FNV &&
                  catalogAfter == EXPECTED_CATALOG_FNV &&
                  memcmp(&residentBefore, &residentAfter,
                         sizeof(residentBefore)) == 0 &&
                  residentCanonical(&residentAfter) &&
                  legacyGraphicsClear(doomRpg->render) &&
                  !EspAssetPack_isOpen() && repeat && repeatAtomic &&
                  doomRpg->doomCanvas->state == ST_INTRO &&
                  doomRpg->game->numEntities == 0 &&
                  doomRpg->game->numMonsters == 0;

    if (!geometryOk || !planeOk || !integrityOk) {
        printf("[JUNCTIONFRAME] FAILED integrity geometry=%d planes=%d frameChanged=%d heap=%d largest=%d game=%d player=%d hud=%d canvas=%d render=%d column=%d palette=%d catalog=%08x/%08x resident=%d repeat=%d atomic=%d pack=%d\n",
               geometryOk, planeOk, frameBefore != frameAfter,
               heapBefore == heapAfter, largestBefore == largestAfter,
               gameBefore == gameAfter, playerBefore == playerAfter,
               hudBefore == hudAfter, canvasBefore == canvasAfter,
               renderBefore == renderAfter, columnBefore == columnAfter,
               paletteBefore == paletteAfter,
               (unsigned int)catalogBefore, (unsigned int)catalogAfter,
               memcmp(&residentBefore, &residentAfter,
                      sizeof(residentBefore)) == 0,
               repeat, repeatAtomic, EspAssetPack_isOpen());
        return;
    }

    printf("[JUNCTIONFRAME] READY stateBytes=%u stateFNV=%08x frame=%08x->%08x walls=%u spans=%u wallPixels=%u planes=%u planeTex=%u cache=%uH/%uM/%uE presented=%u\n",
           (unsigned int)sizeof(*frame), (unsigned int)stateFNV,
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           (unsigned int)frame->wallDraws, (unsigned int)frame->spanCalls,
           (unsigned int)frame->pixelsDrawn,
           (unsigned int)planes->pixelsRendered,
           (unsigned int)planes->uniqueLogicalTextures,
           (unsigned int)planes->cacheHits,
           (unsigned int)planes->cacheMisses,
           (unsigned int)planes->cacheEvictions,
           (unsigned int)frame->presented);
    printf("[JUNCTIONFRAME] GFX catalog=%08x texture=%08x sprite=%08x planeReads=%uB resident=%08x heapDelta=%d largestDelta=%d legacyRenderStable=yes packClosed=yes\n",
           (unsigned int)catalogAfter, (unsigned int)textureFNV,
           (unsigned int)spriteFNV, (unsigned int)planes->texelReadBytes,
           (unsigned int)hashBytes(&residentAfter, sizeof(residentAfter)),
           (int)heapBefore - (int)heapAfter,
           (int)largestBefore - (int)largestAfter);
    printf("[JUNCTIONFRAME] PARK nativeFirstFrame=yes texturedPlanes=yes firstFramePending=no spritesPending=yes hudPending=yes gameplayDispatchPending=yes legacyState=9 entities=0 monsters=0 noGameplay=yes\n");

    probeState.done = 1;
}
