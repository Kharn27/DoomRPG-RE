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
#define EXPECTED_PIXELS 4341U
#define EXPECTED_CACHE_HITS 17U
#define EXPECTED_CACHE_MISSES 17U
#define EXPECTED_CEILING_RGB565 0xb5b6U
#define EXPECTED_FLOOR_RGB565 0x632cU
#define JUNCTION_TARGET_MAP 9U

static struct {
    int armed;
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

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static uint32_t framebufferHash(void) {
    const void* fb = Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    if (fb == NULL || bytes !=
        (size_t)DOOMRPG_LOGICAL_WIDTH * (size_t)DOOMRPG_LOGICAL_HEIGHT *
            sizeof(uint16_t)) return 0U;
    return hashBytes(fb, (uint32_t)bytes);
}

static uint32_t columnWitness(const Render_t* render) {
    if (render == NULL || render->columnScale == NULL ||
        render->screenWidth <= 0 || render->screenWidth > DOOMRPG_LOGICAL_WIDTH)
        return 0U;
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

static int catalogMenuDirect(uint32_t* outTextureFNV,
                             uint32_t* outSpriteFNV) {
    const EspNativeGraphicsCatalogView* v = EspNativeGraphicsCatalog_view();
    Render_t* render;
    uint32_t group;

    if (outTextureFNV != NULL) *outTextureFNV = 0U;
    if (outSpriteFNV != NULL) *outSpriteFNV = 0U;

    if (v == NULL || sizeof(EspNativeGraphicsCatalogRecord) !=
                         EXPECTED_CATALOG_RECORD_BYTES ||
        v->textureCount != EXPECTED_TEXTURE_COUNT ||
        v->spriteCount != EXPECTED_SPRITE_COUNT ||
        v->storageBytes != EXPECTED_CATALOG_STORAGE ||
        v->stateFNV1a == 0U) return 0;

    /* The caller supplies the legacy menu palette through the global Render
     * witness. Resolve it from the currently initialized engine object only in
     * the service function; this helper receives it through a temporary static
     * below to keep the structural checks together. */
    render = NULL;
    (void)render;

    if (outTextureFNV != NULL) {
        *outTextureFNV = hashBytes(
            v->textures,
            (uint32_t)v->textureCount * sizeof(EspNativeGraphicsCatalogRecord));
    }
    if (outSpriteFNV != NULL) {
        *outSpriteFNV = hashBytes(
            v->sprites,
            (uint32_t)v->spriteCount * sizeof(EspNativeGraphicsCatalogRecord));
    }

    for (group = 0U; group < 2U; ++group) {
        const EspNativeGraphicsCatalogRecord* records =
            group == 0U ? v->textures : v->sprites;
        uint16_t count = group == 0U ? v->textureCount : v->spriteCount;
        uint16_t i;
        for (i = 0U; i < count; ++i) {
            if (records[i].resourceId >= 256U ||
                (group == 0U && (records[i].sourceOffset & 1U) != 0U)) {
                return 0;
            }
        }
    }
    return 1;
}

static int catalogPaletteMatchesMenu(const EspNativeGraphicsCatalogView* v,
                                     const Render_t* render) {
    uint32_t group;
    if (v == NULL || render == NULL || render->mediaPalettes == NULL ||
        render->mediaPalettesLength <= 0) return 0;

    for (group = 0U; group < 2U; ++group) {
        const EspNativeGraphicsCatalogRecord* records =
            group == 0U ? v->textures : v->sprites;
        uint16_t count = group == 0U ? v->textureCount : v->spriteCount;
        uint16_t i;
        for (i = 0U; i < count; ++i) {
            uint32_t p;
            if ((uint32_t)records[i].paletteSourceOffset + 15U >=
                (uint32_t)render->mediaPalettesLength) return 0;
            for (p = 0U; p < ESP_NATIVE_GRAPHICS_PALETTE_COLORS; ++p) {
                if (records[i].paletteRgb565[p] !=
                    (uint16_t)render->mediaPalettes[
                        (uint32_t)records[i].paletteSourceOffset + p]) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

void Esp32JunctionFirstFrameCorrectedProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspNativeFirstFrame_reset();
}

int Esp32JunctionFirstFrameCorrectedProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionFirstFrameCorrectedProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    EspMapResidentSnapshot residentBefore, residentAfter;
    const EspMapLineStateView* lineState;
    const EspPlayerViewState* playerView;
    const EspNativeGraphicsCatalogView* catalog;
    const EspNativeFirstFrameState* frame;
    EspNativeFirstFrameState frameCopy;
    uint32_t textureFNV = 0U, spriteFNV = 0U;
    uint32_t frameBefore, frameAfter, stateFNV;
    uint32_t heapBefore, heapAfter, largestBefore, largestAfter;
    uint32_t gameBefore, gameAfter, playerBefore, playerAfter;
    uint32_t hudBefore, hudAfter, canvasBefore, canvasAfter;
    uint32_t renderBefore, renderAfter, columnBefore, columnAfter;
    uint32_t paletteBefore, paletteAfter;
    uint32_t catalogBeforeFNV, catalogAfterFNV;
    int nullRender, nullView, badMap, repeat, repeatAtomic;
    EspNativeFirstFrameStatus status;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionGraphicsCatalogProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[JUNCTIONFRAMEPROBE] ARMED corrected menu-equivalent RGB565 catalog; first visible Junction frame starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native Junction first gameplay frame (corrected RGB565) ===\n");
    printf("[JUNCTIONFRAMEPROBE] CONTRACT render the same hardware-proven deterministic Junction walls-only frame, but require source BGR565 palettes to be converted once into the exact RGB565 channel order used by the menu; framebuffer mutation/presentation are allowed, sprites/HUD/input/turn/gameplay remain deferred, legacy BSP/mappings/shapeData/mediaTexels stay absent and all transient wall texels/PAK handles must be released before PARK\n");

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL || doomRpg->render == NULL ||
        doomRpg->game == NULL || doomRpg->player == NULL || doomRpg->hud == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO || doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->doomCanvas->numEvents != 0 || doomRpg->game->numEntities != 0 ||
        doomRpg->game->numMonsters != 0 || !legacyGraphicsClear(doomRpg->render) ||
        doomRpg->render->framebuffer != (byte*)Esp32PlatformVideo_framebuffer() ||
        doomRpg->render->columnScale == NULL || doomRpg->render->screenWidth != 160 ||
        doomRpg->render->screenHeight != 80 || doomRpg->render->screenX != 0 ||
        doomRpg->render->screenY != 20 || EspAssetPack_isOpen() ||
        sizeof(EspNativeFirstFrameState) != EXPECTED_FRAME_STATE_BYTES ||
        EspNativeFirstFrame_isReady() || !playingCanonical() ||
        !playerViewCanonical() ||
        !EspMapResidentLifecycle_capture(&residentBefore) ||
        !residentCanonical(&residentBefore)) {
        printf("[JUNCTIONFRAMEPROBE] FAILED unsafe corrected-frame boundary\n");
        return;
    }

    catalog = EspNativeGraphicsCatalog_view();
    if (!catalogMenuDirect(&textureFNV, &spriteFNV) ||
        !catalogPaletteMatchesMenu(catalog, doomRpg->render)) {
        printf("[JUNCTIONFRAMEPROBE] FAILED corrected catalog/menu palette relation catalog=%p stateFNV=%08x\n",
               (const void*)catalog,
               (unsigned int)(catalog ? catalog->stateFNV1a : 0U));
        return;
    }

    lineState = EspMapLineState_view();
    playerView = EspPlayerView_view();
    if (lineState == NULL || lineState->stateFNV1a != EXPECTED_LINE_FNV ||
        lineState->openCount != 0U || playerView == NULL) {
        printf("[JUNCTIONFRAMEPROBE] FAILED mutable world gate lineFNV=%08x open=%u\n",
               (unsigned int)(lineState ? lineState->stateFNV1a : 0U),
               (unsigned int)(lineState ? lineState->openCount : 0U));
        return;
    }

    frameBefore = framebufferHash();
    heapBefore = heap8Free();
    largestBefore = largest8Block();
    gameBefore = hashBytes(doomRpg->game, sizeof(*doomRpg->game));
    playerBefore = hashBytes(doomRpg->player, sizeof(*doomRpg->player));
    hudBefore = hashBytes(doomRpg->hud, sizeof(*doomRpg->hud));
    canvasBefore = hashBytes(doomRpg->doomCanvas, sizeof(*doomRpg->doomCanvas));
    renderBefore = hashBytes(doomRpg->render, sizeof(*doomRpg->render));
    columnBefore = columnWitness(doomRpg->render);
    paletteBefore = hashBytes(doomRpg->render->mediaPalettes,
                              (uint32_t)doomRpg->render->mediaPalettesLength * 2U);
    catalogBeforeFNV = catalog->stateFNV1a;

    nullRender = EspNativeFirstFrame_route(NULL, playerView) ==
                 ESP_NATIVE_FIRST_FRAME_INVALID;
    nullView = EspNativeFirstFrame_route(doomRpg->render, NULL) ==
               ESP_NATIVE_FIRST_FRAME_INVALID;
    {
        EspPlayerViewState bad = *playerView;
        bad.targetMapId = 8U;
        badMap = EspNativeFirstFrame_route(doomRpg->render, &bad) ==
                 ESP_NATIVE_FIRST_FRAME_RENDER_FAILED;
    }

    if (framebufferHash() != frameBefore || EspNativeFirstFrame_isReady()) {
        printf("[JUNCTIONFRAMEPROBE] FAILED preflight mutated framebuffer/owner\n");
        return;
    }

    status = EspNativeFirstFrame_route(doomRpg->render, playerView);
    if (status != ESP_NATIVE_FIRST_FRAME_OK) {
        printf("[JUNCTIONFRAMEPROBE] FAILED corrected route status=%d frameNow=%08x packOpen=%d\n",
               (int)status, (unsigned int)framebufferHash(), EspAssetPack_isOpen());
        return;
    }

    frame = EspNativeFirstFrame_view();
    if (frame == NULL) return;
    frameCopy = *frame;
    stateFNV = hashBytes(frame, sizeof(*frame));

    repeat = EspNativeFirstFrame_route(doomRpg->render, playerView) ==
             ESP_NATIVE_FIRST_FRAME_ALREADY_ACTIVE;
    repeatAtomic = memcmp(frame, &frameCopy, sizeof(frameCopy)) == 0;

    frameAfter = framebufferHash();
    heapAfter = heap8Free();
    largestAfter = largest8Block();
    gameAfter = hashBytes(doomRpg->game, sizeof(*doomRpg->game));
    playerAfter = hashBytes(doomRpg->player, sizeof(*doomRpg->player));
    hudAfter = hashBytes(doomRpg->hud, sizeof(*doomRpg->hud));
    canvasAfter = hashBytes(doomRpg->doomCanvas, sizeof(*doomRpg->doomCanvas));
    renderAfter = hashBytes(doomRpg->render, sizeof(*doomRpg->render));
    columnAfter = columnWitness(doomRpg->render);
    paletteAfter = hashBytes(doomRpg->render->mediaPalettes,
                             (uint32_t)doomRpg->render->mediaPalettesLength * 2U);
    catalogAfterFNV = EspNativeGraphicsCatalog_view()->stateFNV1a;

    if (!EspMapResidentLifecycle_capture(&residentAfter) ||
        !residentCanonical(&residentAfter) ||
        frame->frameBeforeFNV != frameBefore || frame->frameAfterFNV != frameAfter ||
        frameBefore == frameAfter || frame->targetMapId != JUNCTION_TARGET_MAP ||
        frame->rendered != 1U || frame->presented != 1U || frame->active != 1U ||
        frame->leafNodes != EXPECTED_LEAVES ||
        frame->lineCandidates != EXPECTED_LINE_CANDIDATES ||
        frame->wallRequests != EXPECTED_WALL_REQUESTS ||
        frame->wallDraws != EXPECTED_WALL_DRAWS ||
        frame->spanCalls != EXPECTED_SPANS || frame->pixelsDrawn != EXPECTED_PIXELS ||
        frame->cacheHits != EXPECTED_CACHE_HITS ||
        frame->cacheMisses != EXPECTED_CACHE_MISSES ||
        frame->ceilingRgb565 != EXPECTED_CEILING_RGB565 ||
        frame->floorRgb565 != EXPECTED_FLOOR_RGB565 ||
        heapBefore != heapAfter || largestBefore != largestAfter ||
        gameBefore != gameAfter || playerBefore != playerAfter ||
        hudBefore != hudAfter || canvasBefore != canvasAfter ||
        renderBefore != renderAfter || columnBefore != columnAfter ||
        paletteBefore != paletteAfter || catalogBeforeFNV != catalogAfterFNV ||
        !legacyGraphicsClear(doomRpg->render) || EspAssetPack_isOpen() ||
        !repeat || !repeatAtomic ||
        !catalogPaletteMatchesMenu(EspNativeGraphicsCatalog_view(), doomRpg->render)) {
        printf("[JUNCTIONFRAMEPROBE] FAILED corrected integrity frameChanged=%d geometry=%u/%u/%u/%u/%u heap=%d largest=%d game=%d player=%d hud=%d canvas=%d render=%d column=%d palette=%d catalog=%d repeat=%d atomic=%d pack=%d\n",
               frameBefore != frameAfter,
               (unsigned int)frame->leafNodes,
               (unsigned int)frame->lineCandidates,
               (unsigned int)frame->wallDraws,
               (unsigned int)frame->spanCalls,
               (unsigned int)frame->pixelsDrawn,
               heapBefore == heapAfter, largestBefore == largestAfter,
               gameBefore == gameAfter, playerBefore == playerAfter,
               hudBefore == hudAfter, canvasBefore == canvasAfter,
               renderBefore == renderAfter, columnBefore == columnAfter,
               paletteBefore == paletteAfter,
               catalogBeforeFNV == catalogAfterFNV,
               repeat, repeatAtomic, EspAssetPack_isOpen());
        return;
    }

    printf("[JUNCTIONFRAME] READY stateBytes=%u stateFNV=%08x frame=%08x->%08x viewport=160x80@0,20 ceiling=%04x floor=%04x targetMap=9 rendered=1 presented=1 active=1\n",
           (unsigned int)sizeof(*frame), (unsigned int)stateFNV,
           (unsigned int)frame->frameBeforeFNV,
           (unsigned int)frame->frameAfterFNV,
           (unsigned int)frame->ceilingRgb565,
           (unsigned int)frame->floorRgb565);
    printf("[JUNCTIONFRAME] SEMANTIC nativeFrame=yes walls=yes paletteCorrected=yes paletteRelation=menu-direct spritesDeferred=yes hudDeferred=yes presentation=yes frameMutation=yes inputConsumed=no turnAdvanced=no gameplayDispatch=no legacyDoomCanvas_playingStateCalled=no legacyRender_renderCalled=no\n");
    printf("[JUNCTIONFRAME] RENDER leaves=%u lineCandidates=%u wallRequests=%u wallDraws=%u spans=%u pixels=%u cacheHits=%u cacheMisses=%u camera=%ld/%ld/%ld angle=%ld lineOpen=0\n",
           (unsigned int)frame->leafNodes,
           (unsigned int)frame->lineCandidates,
           (unsigned int)frame->wallRequests,
           (unsigned int)frame->wallDraws,
           (unsigned int)frame->spanCalls,
           (unsigned int)frame->pixelsDrawn,
           (unsigned int)frame->cacheHits,
           (unsigned int)frame->cacheMisses,
           (long)playerView->viewX, (long)playerView->viewY,
           (long)playerView->viewZ, (long)playerView->viewAngle);
    printf("[JUNCTIONFRAME] GFX catalogFNV=%08x->%08x textureFNV=%08x spriteFNV=%08x records=30/16 storage=1840 logicalToActual=yes paletteRelation=menu-direct packClosed=yes transientWallTexelsReleased=yes\n",
           (unsigned int)catalogBeforeFNV,
           (unsigned int)catalogAfterFNV,
           (unsigned int)textureFNV,
           (unsigned int)spriteFNV);
    printf("[JUNCTIONFRAME] FAILCLOSED nullRender=%d nullView=%d wrongMap=%d preFrameUnchanged=yes repeat=%d repeatAtomic=yes\n",
           nullRender, nullView, badMap, repeat);
    printf("[JUNCTIONFRAME] RESIDENT snapshotFNV=%08x->%08x unchanged=yes runtimeFNV=%08x mapFNV=%08x scriptFNV=%08x lineFNV=%08x textureStateFNV=%08x automapFNV=%08x topologyFNV=%08x payload=%u entities=%u enemies=%u destructibles=%u packClosed=yes\n",
           (unsigned int)hashBytes(&residentBefore, sizeof(residentBefore)),
           (unsigned int)hashBytes(&residentAfter, sizeof(residentAfter)),
           (unsigned int)residentAfter.runtimeFNV1a,
           (unsigned int)residentAfter.mapStateFNV1a,
           (unsigned int)residentAfter.scriptStateFNV1a,
           (unsigned int)residentAfter.lineStateFNV1a,
           (unsigned int)residentAfter.textureStateFNV1a,
           (unsigned int)residentAfter.automapStateFNV1a,
           (unsigned int)residentAfter.topologyFNV1a,
           (unsigned int)residentAfter.totalPayloadBytes,
           (unsigned int)residentAfter.entityCount,
           (unsigned int)residentAfter.enemyCount,
           (unsigned int)residentAfter.destructibleCount);
    printf("[JUNCTIONFRAME] RAM heap8=%u->%u delta=0 largest8=%u->%u delta=0 persistentFrameBytes=0 catalogPersistentBytes=1840\n",
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter);
    printf("[JUNCTIONFRAME] LEGACY gameFNV=%08x->%08x playerFNV=%08x->%08x hudFNV=%08x->%08x canvasFNV=%08x->%08x renderFNV=%08x->%08x columnFNV=%08x->%08x paletteFNV=%08x->%08x legacyState=9->9 legacyRuntimeClear=yes GameMutation=no PlayerMutation=no HudMutation=no DoomCanvasMutation=no RenderMutation=no ColumnMutation=no PaletteMutation=no\n",
           (unsigned int)gameBefore, (unsigned int)gameAfter,
           (unsigned int)playerBefore, (unsigned int)playerAfter,
           (unsigned int)hudBefore, (unsigned int)hudAfter,
           (unsigned int)canvasBefore, (unsigned int)canvasAfter,
           (unsigned int)renderBefore, (unsigned int)renderAfter,
           (unsigned int)columnBefore, (unsigned int)columnAfter,
           (unsigned int)paletteBefore, (unsigned int)paletteAfter);
    printf("[JUNCTIONFRAME] PARK legacyState=9 page=3 targetMap=9 junctionResident=yes nativeST_PLAYING=yes nativePlayingService=yes nativeGraphicsCatalog=yes nativeFirstFrame=yes firstFramePending=no spritesPending=yes hudPending=yes gameplayDispatchPending=yes initialSavePersistencePending=yes entities=0 monsters=0 noGameplay=yes\n");

    probeState.done = 1;
}
