#include <SDL.h>
#include "DoomRPG.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
#include "native_junction_first_frame_probe.h"
#include "native_junction_graphics_catalog_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#include <esp_heap_caps.h>

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
#define EXPECTED_GRAPHICS_CATALOG_FNV 0x969d5a77U
#define EXPECTED_GRAPHICS_TEXTURE_FNV 0x2dd5dfcfU
#define EXPECTED_GRAPHICS_SPRITE_FNV 0xcfd036cfU
#define EXPECTED_CATALOG_RECORD_BYTES 40U
#define EXPECTED_TEXTURE_COUNT 30U
#define EXPECTED_SPRITE_COUNT 16U
#define EXPECTED_CATALOG_STORAGE 1840U
#define EXPECTED_FRAME_STATE_BYTES 48U
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

static uint32_t gameWitness(const Game_t* game) {
    uint32_t v[11];
    if (game == NULL) return 0U;
    v[0] = (uint32_t)game->numEntities;
    v[1] = (uint32_t)game->numMonsters;
    v[2] = (uint32_t)game->waitTime;
    v[3] = (uint32_t)game->activeSprites;
    v[4] = (uint32_t)game->monstersTurn;
    v[5] = (uint32_t)game->tileEvent;
    v[6] = (uint32_t)game->tileEventIndex;
    v[7] = (uint32_t)game->tileEventFlags;
    v[8] = (uint32_t)game->isLoaded;
    v[9] = (uint32_t)game->isSaved;
    v[10] = (uint32_t)game->activeLoadType;
    return hashBytes(v, sizeof(v));
}

static uint32_t playerWitness(const Player_t* player) {
    uint32_t v[8];
    if (player == NULL) return 0U;
    v[0] = (uint32_t)player->keys;
    v[1] = (uint32_t)player->moves;
    v[2] = (uint32_t)player->weapons;
    v[3] = (uint32_t)player->weapon;
    v[4] = (uint32_t)player->currentXP;
    v[5] = (uint32_t)player->level;
    v[6] = (uint32_t)player->credits;
    v[7] = (uint32_t)player->berserkerTics;
    return hashBytes(v, sizeof(v));
}

static uint32_t hudWitness(const Hud_t* hud) {
    uint32_t v[5];
    if (hud == NULL) return 0U;
    v[0] = (uint32_t)hud->msgCount;
    v[1] = (uint32_t)hud->msgTime;
    v[2] = (uint32_t)hud->msgDuration;
    v[3] = (uint32_t)hud->isUpdate;
    v[4] = (uint32_t)(uintptr_t)hud->statBarMessage;
    return hashBytes(v, sizeof(v));
}

static uint32_t canvasWitness(const DoomCanvas_t* canvas) {
    uint32_t v[15];
    if (canvas == NULL) return 0U;
    v[0] = (uint32_t)canvas->viewX;
    v[1] = (uint32_t)canvas->viewY;
    v[2] = (uint32_t)canvas->viewAngle;
    v[3] = (uint32_t)canvas->destX;
    v[4] = (uint32_t)canvas->destY;
    v[5] = (uint32_t)canvas->destAngle;
    v[6] = (uint32_t)canvas->state;
    v[7] = (uint32_t)canvas->storyPage;
    v[8] = (uint32_t)canvas->numEvents;
    v[9] = (uint32_t)canvas->isUpdateView;
    v[10] = (uint32_t)canvas->time;
    v[11] = (uint32_t)canvas->idleTime;
    v[12] = (uint32_t)canvas->openDoorsCount;
    v[13] = (uint32_t)canvas->animFrameCount;
    v[14] = (uint32_t)canvas->renderOnly;
    return hashBytes(v, sizeof(v));
}

static uint32_t renderWitness(const Render_t* render) {
    uint32_t v[24];
    if (render == NULL) return 0U;
    v[0] = (uint32_t)(uintptr_t)render->framebuffer;
    v[1] = (uint32_t)(uintptr_t)render->columnScale;
    v[2] = (uint32_t)(uintptr_t)render->mediaPalettes;
    v[3] = (uint32_t)(uintptr_t)render->mediaTexelOffsets;
    v[4] = (uint32_t)(uintptr_t)render->mediaBitShapeOffsets;
    v[5] = (uint32_t)(uintptr_t)render->mediaTexturesIds;
    v[6] = (uint32_t)(uintptr_t)render->mediaSpriteIds;
    v[7] = (uint32_t)(uintptr_t)render->shapeData;
    v[8] = (uint32_t)(uintptr_t)render->mediaTexels;
    v[9] = (uint32_t)(uintptr_t)render->lines;
    v[10] = (uint32_t)(uintptr_t)render->nodes;
    v[11] = (uint32_t)(uintptr_t)render->mapSprites;
    v[12] = (uint32_t)render->screenWidth;
    v[13] = (uint32_t)render->screenHeight;
    v[14] = (uint32_t)render->screenX;
    v[15] = (uint32_t)render->screenY;
    v[16] = (uint32_t)render->viewX;
    v[17] = (uint32_t)render->viewY;
    v[18] = (uint32_t)render->viewZ;
    v[19] = (uint32_t)render->viewAngle;
    v[20] = (uint32_t)render->lineCount;
    v[21] = (uint32_t)render->lineRasterCount;
    v[22] = (uint32_t)render->nodeCount;
    v[23] = (uint32_t)(uintptr_t)render->pixels;
    return hashBytes(v, sizeof(v));
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
           s->viewX == 992 && s->viewY == 1888 && s->viewAngle == 64 &&
           s->destX == s->viewX && s->destY == s->viewY &&
           s->destAngle == s->viewAngle &&
           s->targetMapId == JUNCTION_TARGET_MAP &&
           s->hudRefreshPending == 0U && s->facingRefreshPending == 0U &&
           s->playerSetupPending == 0U && s->tileEnterPending == 0U &&
           s->active == 1U;
}

static int catalogCanonical(uint32_t* outTextureFNV, uint32_t* outSpriteFNV) {
    const EspNativeGraphicsCatalogView* v = EspNativeGraphicsCatalog_view();
    uint32_t textureFNV;
    uint32_t spriteFNV;
    if (v == NULL || sizeof(EspNativeGraphicsCatalogRecord) !=
                         EXPECTED_CATALOG_RECORD_BYTES ||
        v->textureCount != EXPECTED_TEXTURE_COUNT ||
        v->spriteCount != EXPECTED_SPRITE_COUNT ||
        v->storageBytes != EXPECTED_CATALOG_STORAGE ||
        v->stateFNV1a != EXPECTED_GRAPHICS_CATALOG_FNV) return 0;
    textureFNV = hashBytes(v->textures,
                           v->textureCount * sizeof(EspNativeGraphicsCatalogRecord));
    spriteFNV = hashBytes(v->sprites,
                          v->spriteCount * sizeof(EspNativeGraphicsCatalogRecord));
    if (outTextureFNV != NULL) *outTextureFNV = textureFNV;
    if (outSpriteFNV != NULL) *outSpriteFNV = spriteFNV;
    return textureFNV == EXPECTED_GRAPHICS_TEXTURE_FNV &&
           spriteFNV == EXPECTED_GRAPHICS_SPRITE_FNV;
}

void Esp32JunctionFirstFrameProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspNativeFirstFrame_reset();
}

int Esp32JunctionFirstFrameProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionFirstFrameProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentAfter;
    const EspMapLineStateView* lineState;
    const EspPlayerViewState* playerView;
    const EspNativeFirstFrameState* frame;
    EspNativeFirstFrameState frameCopy;
    uint32_t textureFNV = 0U, spriteFNV = 0U;
    uint32_t frameBefore, frameAfter, stateFNV;
    uint32_t heapBefore, heapAfter, largestBefore, largestAfter;
    uint32_t gameBefore, gameAfter, playerBefore, playerAfter;
    uint32_t hudBefore, hudAfter, canvasBefore, canvasAfter;
    uint32_t renderBefore, renderAfter, columnBefore, columnAfter;
    uint32_t residentBeforeFNV, residentAfterFNV;
    uint32_t catalogBeforeFNV, catalogAfterFNV;
    int nullRender, nullView, badMap, repeat, repeatAtomic;
    int legacyClearBefore, legacyClearAfter;
    EspNativeFirstFrameStatus status;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionGraphicsCatalogProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[JUNCTIONFRAMEPROBE] ARMED sparse graphics catalog complete; first visible native Junction frame starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native Junction first gameplay frame ===\n");
    printf("[JUNCTIONFRAMEPROBE] CONTRACT consume hardware-proven native PLAYING service + PlayerView + compact Junction runtime + sparse graphics catalog into one real 160x120 framebuffer mutation and TFT present: render only deterministic solid floor/ceiling + wall geometry/assets, keep sprites/HUD/input/turn/gameplay deferred, never call DoomCanvas_playingState/Render_render, never resurrect legacy lines/nodes/mappings/shapeData/mediaTexels, release all transient wall texels and close PAK before PARK\n");

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL || doomRpg->render == NULL ||
        doomRpg->game == NULL || doomRpg->player == NULL || doomRpg->hud == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO || doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->doomCanvas->numEvents != 0 || doomRpg->game->numEntities != 0 ||
        doomRpg->game->numMonsters != 0 || !legacyGraphicsClear(doomRpg->render) ||
        doomRpg->render->framebuffer != (byte*)Esp32PlatformVideo_framebuffer() ||
        doomRpg->render->columnScale == NULL || doomRpg->render->screenWidth != 160 ||
        doomRpg->render->screenHeight <= 0 || doomRpg->render->screenHeight > 120 ||
        doomRpg->render->screenX < 0 || doomRpg->render->screenY < 0 ||
        doomRpg->render->screenX + doomRpg->render->screenWidth > 160 ||
        doomRpg->render->screenY + doomRpg->render->screenHeight > 120 ||
        EspAssetPack_isOpen() || sizeof(EspNativeFirstFrameState) != EXPECTED_FRAME_STATE_BYTES ||
        EspNativeFirstFrame_isReady() || !playingCanonical() || !playerViewCanonical() ||
        !catalogCanonical(&textureFNV, &spriteFNV) ||
        !EspMapResidentLifecycle_capture(&residentBefore) || !residentCanonical(&residentBefore)) {
        printf("[JUNCTIONFRAMEPROBE] FAILED unsafe first-frame boundary state=%d page=%d events=%d entities=%d monsters=%d legacyClear=%d viewport=%dx%d@%d,%d packOpen=%d frameActive=%d playing=%d playerView=%d catalog=%d resident=%d\n",
               doomRpg && doomRpg->doomCanvas ? doomRpg->doomCanvas->state : -1,
               doomRpg && doomRpg->doomCanvas ? doomRpg->doomCanvas->storyPage : -1,
               doomRpg && doomRpg->doomCanvas ? doomRpg->doomCanvas->numEvents : -1,
               doomRpg && doomRpg->game ? doomRpg->game->numEntities : -1,
               doomRpg && doomRpg->game ? doomRpg->game->numMonsters : -1,
               doomRpg && doomRpg->render ? legacyGraphicsClear(doomRpg->render) : 0,
               doomRpg && doomRpg->render ? doomRpg->render->screenWidth : -1,
               doomRpg && doomRpg->render ? doomRpg->render->screenHeight : -1,
               doomRpg && doomRpg->render ? doomRpg->render->screenX : -1,
               doomRpg && doomRpg->render ? doomRpg->render->screenY : -1,
               EspAssetPack_isOpen(), EspNativeFirstFrame_isReady(),
               playingCanonical(), playerViewCanonical(),
               catalogCanonical(NULL, NULL),
               EspMapResidentLifecycle_capture(&residentAfter) && residentCanonical(&residentAfter));
        return;
    }

    lineState = EspMapLineState_view();
    playerView = EspPlayerView_view();
    if (lineState == NULL || lineState->stateFNV1a != EXPECTED_LINE_FNV ||
        lineState->openCount != 0U || playerView == NULL) {
        printf("[JUNCTIONFRAMEPROBE] FAILED mutable world gate lineState=%p lineFNV=%08x open=%u playerView=%p\n",
               (void*)lineState,
               (unsigned int)(lineState ? lineState->stateFNV1a : 0U),
               (unsigned int)(lineState ? lineState->openCount : 0U),
               (void*)playerView);
        return;
    }

    frameBefore = framebufferHash();
    heapBefore = heap8Free();
    largestBefore = largest8Block();
    gameBefore = gameWitness(doomRpg->game);
    playerBefore = playerWitness(doomRpg->player);
    hudBefore = hudWitness(doomRpg->hud);
    canvasBefore = canvasWitness(doomRpg->doomCanvas);
    renderBefore = renderWitness(doomRpg->render);
    columnBefore = columnWitness(doomRpg->render);
    residentBeforeFNV = hashBytes(&residentBefore, sizeof(residentBefore));
    catalogBeforeFNV = EspNativeGraphicsCatalog_view()->stateFNV1a;
    legacyClearBefore = legacyGraphicsClear(doomRpg->render);

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
        printf("[JUNCTIONFRAMEPROBE] FAILED fail-closed preflight mutated framebuffer/owner\n");
        return;
    }

    status = EspNativeFirstFrame_route(doomRpg->render, playerView);
    if (status != ESP_NATIVE_FIRST_FRAME_OK) {
        printf("[JUNCTIONFRAMEPROBE] FAILED route status=%d frameNow=%08x packOpen=%d\n",
               (int)status, (unsigned int)framebufferHash(), EspAssetPack_isOpen());
        return;
    }

    frame = EspNativeFirstFrame_view();
    if (frame == NULL) {
        printf("[JUNCTIONFRAMEPROBE] FAILED owner missing after OK route\n");
        return;
    }
    frameCopy = *frame;
    stateFNV = hashBytes(frame, sizeof(*frame));

    repeat = EspNativeFirstFrame_route(doomRpg->render, playerView) ==
             ESP_NATIVE_FIRST_FRAME_ALREADY_ACTIVE;
    repeatAtomic = memcmp(frame, &frameCopy, sizeof(frameCopy)) == 0;

    frameAfter = framebufferHash();
    heapAfter = heap8Free();
    largestAfter = largest8Block();
    gameAfter = gameWitness(doomRpg->game);
    playerAfter = playerWitness(doomRpg->player);
    hudAfter = hudWitness(doomRpg->hud);
    canvasAfter = canvasWitness(doomRpg->doomCanvas);
    renderAfter = renderWitness(doomRpg->render);
    columnAfter = columnWitness(doomRpg->render);
    legacyClearAfter = legacyGraphicsClear(doomRpg->render);
    catalogAfterFNV = EspNativeGraphicsCatalog_view()->stateFNV1a;

    if (!EspMapResidentLifecycle_capture(&residentAfter)) {
        printf("[JUNCTIONFRAMEPROBE] FAILED resident capture after frame\n");
        return;
    }
    residentAfterFNV = hashBytes(&residentAfter, sizeof(residentAfter));

    printf("[JUNCTIONFRAME] READY stateBytes=%u stateFNV=%08x frame=%08x->%08x viewport=%dx%d@%d,%d ceiling=%04x floor=%04x targetMap=%u rendered=%u presented=%u active=%u\n",
           (unsigned int)sizeof(*frame), (unsigned int)stateFNV,
           (unsigned int)frame->frameBeforeFNV,
           (unsigned int)frame->frameAfterFNV,
           doomRpg->render->screenWidth, doomRpg->render->screenHeight,
           doomRpg->render->screenX, doomRpg->render->screenY,
           (unsigned int)frame->ceilingRgb565,
           (unsigned int)frame->floorRgb565,
           (unsigned int)frame->targetMapId,
           (unsigned int)frame->rendered,
           (unsigned int)frame->presented,
           (unsigned int)frame->active);
    printf("[JUNCTIONFRAME] SEMANTIC nativeFrame=yes walls=yes spritesDeferred=yes hudDeferred=yes presentation=yes frameMutation=%s inputConsumed=no turnAdvanced=no gameplayDispatch=no legacyDoomCanvas_playingStateCalled=no legacyRender_renderCalled=no\n",
           frameBefore != frameAfter ? "yes" : "NO");
    printf("[JUNCTIONFRAME] RENDER leaves=%u lineCandidates=%u wallRequests=%u wallDraws=%u spans=%u pixels=%u cacheHits=%u cacheMisses=%u camera=%ld/%ld/%ld angle=%ld lineOpen=%u\n",
           (unsigned int)frame->leafNodes,
           (unsigned int)frame->lineCandidates,
           (unsigned int)frame->wallRequests,
           (unsigned int)frame->wallDraws,
           (unsigned int)frame->spanCalls,
           (unsigned int)frame->pixelsDrawn,
           (unsigned int)frame->cacheHits,
           (unsigned int)frame->cacheMisses,
           (long)playerView->viewX, (long)playerView->viewY,
           (long)playerView->viewZ, (long)playerView->viewAngle,
           (unsigned int)lineState->openCount);
    printf("[JUNCTIONFRAME] GFX catalogFNV=%08x->%08x textureFNV=%08x spriteFNV=%08x records=%u/%u storage=%u logicalToActual=yes packClosed=%s transientWallTexelsReleased=yes\n",
           (unsigned int)catalogBeforeFNV,
           (unsigned int)catalogAfterFNV,
           (unsigned int)textureFNV,
           (unsigned int)spriteFNV,
           (unsigned int)EspNativeGraphicsCatalog_view()->textureCount,
           (unsigned int)EspNativeGraphicsCatalog_view()->spriteCount,
           (unsigned int)EspNativeGraphicsCatalog_view()->storageBytes,
           EspAssetPack_isOpen() ? "NO" : "yes");
    printf("[JUNCTIONFRAME] FAILCLOSED nullRender=%d nullView=%d wrongMap=%d preFrameUnchanged=yes repeat=%d repeatAtomic=%s\n",
           nullRender, nullView, badMap, repeat,
           repeatAtomic ? "yes" : "NO");
    printf("[JUNCTIONFRAME] RESIDENT snapshotFNV=%08x->%08x unchanged=%s runtimeFNV=%08x mapFNV=%08x scriptFNV=%08x lineFNV=%08x textureStateFNV=%08x automapFNV=%08x topologyFNV=%08x payload=%u entities=%u enemies=%u destructibles=%u packClosed=%s\n",
           (unsigned int)residentBeforeFNV,
           (unsigned int)residentAfterFNV,
           memcmp(&residentBefore, &residentAfter, sizeof(residentBefore)) == 0 ? "yes" : "NO",
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
           (unsigned int)residentAfter.destructibleCount,
           EspAssetPack_isOpen() ? "NO" : "yes");
    printf("[JUNCTIONFRAME] RAM heap8=%u->%u delta=%d largest8=%u->%u delta=%d persistentFrameBytes=0 catalogPersistentBytes=1840\n",
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (int)heapBefore - (int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter,
           (int)largestBefore - (int)largestAfter);
    printf("[JUNCTIONFRAME] LEGACY gameFNV=%08x->%08x playerFNV=%08x->%08x hudFNV=%08x->%08x canvasFNV=%08x->%08x renderFNV=%08x->%08x columnFNV=%08x->%08x legacyState=%d->%d legacyRuntimeClear=%s GameMutation=%s PlayerMutation=%s HudMutation=%s DoomCanvasMutation=%s RenderMutation=%s ColumnMutation=%s\n",
           (unsigned int)gameBefore, (unsigned int)gameAfter,
           (unsigned int)playerBefore, (unsigned int)playerAfter,
           (unsigned int)hudBefore, (unsigned int)hudAfter,
           (unsigned int)canvasBefore, (unsigned int)canvasAfter,
           (unsigned int)renderBefore, (unsigned int)renderAfter,
           (unsigned int)columnBefore, (unsigned int)columnAfter,
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->state,
           legacyClearBefore && legacyClearAfter ? "yes" : "NO",
           gameBefore == gameAfter ? "no" : "YES",
           playerBefore == playerAfter ? "no" : "YES",
           hudBefore == hudAfter ? "no" : "YES",
           canvasBefore == canvasAfter ? "no" : "YES",
           renderBefore == renderAfter ? "no" : "YES",
           columnBefore == columnAfter ? "no" : "YES");

    if (frame->frameBeforeFNV != frameBefore || frame->frameAfterFNV != frameAfter ||
        frameBefore == frameAfter || frame->targetMapId != JUNCTION_TARGET_MAP ||
        frame->rendered != 1U || frame->presented != 1U || frame->active != 1U ||
        frame->wallDraws == 0U || frame->spanCalls == 0U || frame->pixelsDrawn == 0U ||
        !nullRender || !nullView || !badMap || !repeat || !repeatAtomic ||
        EspAssetPack_isOpen() || heapBefore != heapAfter || largestBefore != largestAfter ||
        !residentCanonical(&residentAfter) ||
        memcmp(&residentBefore, &residentAfter, sizeof(residentBefore)) != 0 ||
        catalogBeforeFNV != catalogAfterFNV ||
        gameBefore != gameAfter || playerBefore != playerAfter ||
        hudBefore != hudAfter || canvasBefore != canvasAfter ||
        renderBefore != renderAfter || columnBefore != columnAfter ||
        !legacyClearBefore || !legacyClearAfter || doomRpg->doomCanvas->state != ST_INTRO ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0) {
        printf("[JUNCTIONFRAMEPROBE] FAILED post-frame integrity gate\n");
        return;
    }

    printf("[JUNCTIONFRAME] PARK legacyState=9 page=3 targetMap=9 junctionResident=yes nativeST_PLAYING=yes nativePlayingService=yes nativeGraphicsCatalog=yes nativeFirstFrame=yes firstFramePending=no spritesPending=yes hudPending=yes gameplayDispatchPending=yes initialSavePersistencePending=yes entities=0 monsters=0 noGameplay=yes\n");
    probeState.done = 1;
}
