#include <SDL.h>
#include "DoomRPG.h"

#include <stddef.h>
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
#include "esp_hud_post_load_clear_state.h"
#include "esp_map_automap_state.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_map_runtime.h"
#include "esp_map_state.h"
#include "esp_player_facing_state.h"
#include "esp_player_view_state.h"
#include "esp_post_load_givemap_state.h"
#include "native_junction_post_load_givemap_probe.h"
#include "native_junction_post_load_hud_clear_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_GIVEMAP_STATE_BYTES 16U
#define EXPECTED_DIRECT_RESULT_BYTES 12U
#define EXPECTED_HUD_CLEAR_BYTES 8U
#define EXPECTED_HUD_CLEAR_FNV 0xb7383e18U
#define EXPECTED_VIEW_BYTES 44U
#define EXPECTED_VIEW_FNV 0xafcdcf74U
#define EXPECTED_FACING_BYTES 32U
#define EXPECTED_FACING_FNV 0x95aa1108U
#define EXPECTED_SNAPSHOT_BEFORE_FNV 0xbc9071e9U
#define EXPECTED_RUNTIME_FNV 0xbc432a0fU
#define EXPECTED_MAP_BEFORE_FNV 0xc5cdfc04U
#define EXPECTED_SCRIPT_FNV 0xbc9b18ffU
#define EXPECTED_LINE_FNV 0x3658710dU
#define EXPECTED_TEXTURE_FNV 0x537319adU
#define EXPECTED_AUTOMAP_BEFORE_FNV 0x0b2ae445U
#define EXPECTED_TOPOLOGY_FNV 0xd6e8df7dU

static struct {
    int armed;
    int attempted;
    int done;
} probeState;

static uint32_t hashBytes(const void* data, uint32_t length) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (p == NULL && length != 0U) return 0U;
    for (i = 0U; i < length; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t framebufferHash(void) {
    const uint8_t* framebuffer =
        (const uint8_t*)Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();

    if (framebuffer == NULL ||
        bytes != (size_t)DOOMRPG_LOGICAL_WIDTH *
                     (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t)) {
        return 0U;
    }
    return hashBytes(framebuffer, (uint32_t)bytes);
}

static uint32_t gameWitness(const Game_t* game) {
    uint32_t v[13];
    if (game == NULL) return 0U;
    v[0] = (uint32_t)game->spawnParam;
    v[1] = (uint32_t)game->isLoaded;
    v[2] = (uint32_t)game->isSaved;
    v[3] = (uint32_t)game->activeLoadType;
    v[4] = (uint32_t)game->numEntities;
    v[5] = (uint32_t)game->numMonsters;
    v[6] = (uint32_t)game->skipAdvanceTurn;
    v[7] = (uint32_t)game->f658b;
    v[8] = (uint32_t)game->waitTime;
    v[9] = (uint32_t)game->tileEvent;
    v[10] = (uint32_t)game->tileEventIndex;
    v[11] = (uint32_t)game->tileEventFlags;
    v[12] = (uint32_t)game->saveTileEvent;
    return hashBytes(v, sizeof(v));
}

static uint32_t playerWitness(const Player_t* player) {
    uint32_t v[13];
    if (player == NULL) return 0U;
    v[0] = (uint32_t)player->keys;
    v[1] = (uint32_t)player->moves;
    v[2] = (uint32_t)player->xpGained;
    v[3] = (uint32_t)player->berserkerTics;
    v[4] = (uint32_t)player->disabledWeapons;
    v[5] = (uint32_t)player->weapons;
    v[6] = (uint32_t)player->weapon;
    v[7] = (uint32_t)player->currentXP;
    v[8] = (uint32_t)player->level;
    v[9] = (uint32_t)player->credits;
    v[10] = (uint32_t)player->completedLevels;
    v[11] = (uint32_t)(player->dogFamiliar != NULL);
    v[12] = (uint32_t)(player->facingEntity != NULL);
    return hashBytes(v, sizeof(v));
}

static uint32_t canvasWitness(const DoomCanvas_t* canvas) {
    uint32_t v[19];
    if (canvas == NULL) return 0U;
    v[0] = (uint32_t)canvas->viewX;
    v[1] = (uint32_t)canvas->viewY;
    v[2] = (uint32_t)canvas->viewZ;
    v[3] = (uint32_t)canvas->viewAngle;
    v[4] = (uint32_t)canvas->destX;
    v[5] = (uint32_t)canvas->destY;
    v[6] = (uint32_t)canvas->destAngle;
    v[7] = (uint32_t)canvas->viewSin;
    v[8] = (uint32_t)canvas->viewCos;
    v[9] = (uint32_t)canvas->viewStepX;
    v[10] = (uint32_t)canvas->viewStepY;
    v[11] = (uint32_t)canvas->loadType;
    v[12] = (uint32_t)canvas->state;
    v[13] = (uint32_t)canvas->storyPage;
    v[14] = (uint32_t)canvas->loadMapID;
    v[15] = (uint32_t)canvas->numEvents;
    v[16] = (uint32_t)canvas->isUpdateView;
    v[17] = (uint32_t)canvas->idleTime;
    v[18] = (uint32_t)canvas->time;
    return hashBytes(v, sizeof(v));
}

static uint32_t renderWitness(const Render_t* render) {
    uint32_t v[8];
    if (render == NULL) return 0U;
    v[0] = (uint32_t)render->sinTable[64];
    v[1] = (uint32_t)render->sinTable[128];
    v[2] = (uint32_t)render->viewZOld;
    v[3] = (uint32_t)render->numMapSprites;
    v[4] = (uint32_t)render->mapStringCount;
    v[5] = (uint32_t)render->viewX;
    v[6] = (uint32_t)render->viewY;
    v[7] = (uint32_t)render->viewAngle;
    return hashBytes(v, sizeof(v));
}

static int legacyRuntimeIsClear(const Render_t* render) {
    return render != NULL && render->nodes == NULL && render->lines == NULL &&
           render->mapSprites == NULL && render->tileEvents == NULL &&
           render->mapByteCode == NULL && render->mapStringsIDs == NULL &&
           render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL && render->mediaSpriteIds == NULL &&
           render->mapTextureTexels == NULL && render->mapSpriteTexels == NULL &&
           render->shapeData == NULL && render->mediaTexels == NULL &&
           render->ioBuffer == NULL;
}

static int residentBeforeCanonical(const EspMapResidentSnapshot* s) {
    return s != NULL && sizeof(*s) == 96U &&
           s->totalPayloadBytes == 10410U &&
           s->runtimeArenaBytes == 8867U && s->mapStateBytes == 1024U &&
           s->scriptStateBytes == 73U && s->lineStateBytes == 52U &&
           s->textureStateBytes == 26U && s->automapStateBytes == 32U &&
           s->topologyBytes == 336U && s->runtimeFNV1a == EXPECTED_RUNTIME_FNV &&
           s->mapStateFNV1a == EXPECTED_MAP_BEFORE_FNV &&
           s->scriptStateFNV1a == EXPECTED_SCRIPT_FNV &&
           s->lineStateFNV1a == EXPECTED_LINE_FNV &&
           s->textureStateFNV1a == EXPECTED_TEXTURE_FNV &&
           s->automapStateFNV1a == EXPECTED_AUTOMAP_BEFORE_FNV &&
           s->topologyFNV1a == EXPECTED_TOPOLOGY_FNV &&
           s->entityCount == 30U && s->enemyCount == 0U &&
           s->destructibleCount == 3U &&
           hashBytes(s, sizeof(*s)) == EXPECTED_SNAPSHOT_BEFORE_FNV;
}

static int residentAfterStructurallyValid(const EspMapResidentSnapshot* before,
                                          const EspMapResidentSnapshot* after) {
    return before != NULL && after != NULL &&
           after->runtimeArenaBytes == before->runtimeArenaBytes &&
           after->mapStateBytes == before->mapStateBytes &&
           after->scriptStateBytes == before->scriptStateBytes &&
           after->lineStateBytes == before->lineStateBytes &&
           after->textureStateBytes == before->textureStateBytes &&
           after->automapStateBytes == before->automapStateBytes &&
           after->topologyBytes == before->topologyBytes &&
           after->totalPayloadBytes == before->totalPayloadBytes &&
           after->runtimeFNV1a == before->runtimeFNV1a &&
           after->scriptStateFNV1a == before->scriptStateFNV1a &&
           after->lineStateFNV1a == before->lineStateFNV1a &&
           after->textureStateFNV1a == before->textureStateFNV1a &&
           after->topologyFNV1a == before->topologyFNV1a &&
           after->nodeCount == before->nodeCount &&
           after->lineCount == before->lineCount &&
           after->spriteCount == before->spriteCount &&
           after->eventCount == before->eventCount &&
           after->byteCodeCount == before->byteCodeCount &&
           after->stringCount == before->stringCount &&
           after->entityCount == before->entityCount &&
           after->enemyCount == before->enemyCount &&
           after->destructibleCount == before->destructibleCount;
}

static int worldFullyRevealed(const EspPostLoadGiveMapState* state) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    EspMapLine line;
    uint32_t i;
    uint32_t lineTargets = 0U;
    uint32_t spriteTargets = 0U;
    uint32_t entranceTargets = 0U;
    uint8_t revealed;
    uint8_t tileFlags;

    if (state == NULL || runtime == NULL) return 0;

    for (i = 0U; i < runtime->lineCount; ++i) {
        if (!EspMapRuntime_getLine(i, &line)) return 0;
        if ((line.flags & ESP_MAP_LINE_FLAG_NO_AUTOMAP) != 0U) continue;
        ++lineTargets;
        if (!EspMapAutomapState_getLineRevealed(i, &revealed) || revealed != 1U) {
            return 0;
        }
    }

    for (i = 0U; i < runtime->mapSpriteCount; ++i) {
        ++spriteTargets;
        if (!EspMapAutomapState_getSpriteRevealed(i, &revealed) || revealed != 1U) {
            return 0;
        }
    }

    for (i = 0U; i < ESP_MAP_STATE_TILE_COUNT; ++i) {
        if (!EspMapState_getTileFlags(i, &tileFlags)) return 0;
        if ((tileFlags & ESP_MAP_TILE_ENTRANCE) == 0U) continue;
        ++entranceTargets;
        if ((tileFlags & ESP_MAP_TILE_VISITED) == 0U) return 0;
    }

    return lineTargets == state->lineTargetCount &&
           spriteTargets == state->spriteTargetCount &&
           entranceTargets == state->entranceTargetCount;
}

static int inputOwnersCanonical(void) {
    return sizeof(EspHudPostLoadClearState) == EXPECTED_HUD_CLEAR_BYTES &&
           sizeof(EspPlayerViewState) == EXPECTED_VIEW_BYTES &&
           sizeof(EspPlayerFacingState) == EXPECTED_FACING_BYTES &&
           EspHudPostLoadClear_view() != NULL && EspPlayerView_view() != NULL &&
           EspPlayerFacing_view() != NULL &&
           hashBytes(EspHudPostLoadClear_view(), sizeof(EspHudPostLoadClearState)) ==
               EXPECTED_HUD_CLEAR_FNV &&
           hashBytes(EspPlayerView_view(), sizeof(EspPlayerViewState)) ==
               EXPECTED_VIEW_FNV &&
           hashBytes(EspPlayerFacing_view(), sizeof(EspPlayerFacingState)) ==
               EXPECTED_FACING_FNV;
}

void Esp32JunctionPostLoadGiveMapProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspPostLoadGiveMap_reset();
}

int Esp32JunctionPostLoadGiveMapProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionPostLoadGiveMapProbe_service(struct DoomRPG_s* doomRpg) {
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentPrepared;
    EspMapResidentSnapshot residentAfter;
    EspMapResidentSnapshot residentAfterRepeat;
    EspHudPostLoadClearState hudBefore;
    EspHudPostLoadClearState badHud;
    EspPostLoadGiveMapState scratch;
    EspPostLoadGiveMapState prepared;
    EspPostLoadGiveMapState stateBeforeRepeat;
    const EspPostLoadGiveMapState* liveState;
    EspPostLoadGiveMapStatus status;
    EspMapGiveMapDirectResult directScratch;
    uint32_t heapBefore, heapAfter, largestBefore, largestAfter;
    uint32_t frameBefore, frameAfter, gameBefore, gameAfter;
    uint32_t playerBefore, playerAfter, canvasBefore, canvasAfter;
    uint32_t renderBefore, renderAfter, hudLegacyBefore, hudLegacyAfter;
    uint32_t viewBefore, viewAfter, facingBefore, facingAfter;
    uint32_t hudOwnerBefore, hudOwnerAfter;
    int nullHudGate, nullOutputGate, inactiveHudGate, unclearedGate;
    int targetMapGate, gameplayMapGate, loadTypeGate, plannerNullGate;
    int prepareAtomic, postActivePrepareGate, repeatGate, repeatAtomic;
    int targetCountsValid, mutationCountsValid, worldRevealed;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionPostLoadHudClearProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[JUNCTIONGIVEMAPPROBE] ARMED hardware-proven post-load HUD clear active; direct native Junction Game_givemap starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native Junction post-load Game_givemap ===\n");
    printf("[JUNCTIONGIVEMAPPROBE] CONTRACT recover only direct caller-side Junction Game_givemap after the hardware-proven HUD clear: reveal every non-0x20 line, every map sprite, and mark every BIT_AM_ENTRANCE tile BIT_AM_VISITED through shared native automap/map-state owners; park one 16B caller-order marker; never call legacy Game_givemap, never touch legacy Render/Hud/Player/Game/DoomCanvas, keep weapon reselection/save/load cleanup/ST_PLAYING deferred, do not present and do not allocate\n");

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL || doomRpg->game == NULL ||
        doomRpg->render == NULL || doomRpg->player == NULL || doomRpg->hud == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO || doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        EspAssetPack_isOpen() || !legacyRuntimeIsClear(doomRpg->render) ||
        sizeof(EspPostLoadGiveMapState) != EXPECTED_GIVEMAP_STATE_BYTES ||
        sizeof(EspMapGiveMapDirectResult) != EXPECTED_DIRECT_RESULT_BYTES ||
        !inputOwnersCanonical() || !EspMapResidentLifecycle_capture(&residentBefore) ||
        !residentBeforeCanonical(&residentBefore)) {
        printf("[JUNCTIONGIVEMAPPROBE] FAILED unsafe post-HUD-clear boundary\n");
        probeState.done = 1;
        return;
    }

    hudBefore = *EspHudPostLoadClear_view();
    hudOwnerBefore = hashBytes(&hudBefore, sizeof(hudBefore));
    viewBefore = hashBytes(EspPlayerView_view(), sizeof(EspPlayerViewState));
    facingBefore = hashBytes(EspPlayerFacing_view(), sizeof(EspPlayerFacingState));
    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    gameBefore = gameWitness(doomRpg->game);
    playerBefore = playerWitness(doomRpg->player);
    canvasBefore = canvasWitness(doomRpg->doomCanvas);
    renderBefore = renderWitness(doomRpg->render);
    hudLegacyBefore = hashBytes(doomRpg->hud, sizeof(*doomRpg->hud));

    memset(&scratch, 0xa5, sizeof(scratch));
    nullHudGate = EspPostLoadGiveMap_prepare(NULL, &scratch) ==
                  ESP_POST_LOAD_GIVEMAP_INVALID;
    nullOutputGate = EspPostLoadGiveMap_prepare(&hudBefore, NULL) ==
                     ESP_POST_LOAD_GIVEMAP_INVALID;

    badHud = hudBefore;
    badHud.active = 0U;
    inactiveHudGate = EspPostLoadGiveMap_prepare(&badHud, &scratch) ==
                      ESP_POST_LOAD_GIVEMAP_HUD_CLEAR_INVALID;
    badHud = hudBefore;
    badHud.cleared = 0U;
    unclearedGate = EspPostLoadGiveMap_prepare(&badHud, &scratch) ==
                    ESP_POST_LOAD_GIVEMAP_HUD_CLEAR_INVALID;
    badHud = hudBefore;
    badHud.targetMapId = 1U;
    targetMapGate = EspPostLoadGiveMap_prepare(&badHud, &scratch) ==
                    ESP_POST_LOAD_GIVEMAP_UNSUPPORTED_CONTEXT;
    badHud = hudBefore;
    badHud.gameplayLoadMapId = 1U;
    gameplayMapGate = EspPostLoadGiveMap_prepare(&badHud, &scratch) ==
                      ESP_POST_LOAD_GIVEMAP_UNSUPPORTED_CONTEXT;
    badHud = hudBefore;
    badHud.loadType = 1U;
    loadTypeGate = EspPostLoadGiveMap_prepare(&badHud, &scratch) ==
                   ESP_POST_LOAD_GIVEMAP_UNSUPPORTED_CONTEXT;
    plannerNullGate = EspMapAutomapState_planGiveMapDirect(NULL) ==
                      ESP_MAP_GIVEMAP_INVALID;

    memset(&prepared, 0, sizeof(prepared));
    status = EspPostLoadGiveMap_prepare(&hudBefore, &prepared);
    if (status != ESP_POST_LOAD_GIVEMAP_OK || prepared.active != 1U ||
        prepared.targetMapId != 9U || prepared.gameplayLoadMapId != 2U ||
        prepared.loadType != 0U || EspPostLoadGiveMap_isReady() ||
        !EspMapResidentLifecycle_capture(&residentPrepared) ||
        memcmp(&residentBefore, &residentPrepared, sizeof(residentBefore)) != 0 ||
        !inputOwnersCanonical() || EspAssetPack_isOpen()) {
        printf("[JUNCTIONGIVEMAPPROBE] FAILED pure direct givemap preparation status=%u\n",
               (unsigned)status);
        probeState.done = 1;
        return;
    }
    prepareAtomic = 1;

    targetCountsValid =
        prepared.lineTargetCount > 0U && prepared.spriteTargetCount > 0U &&
        prepared.entranceTargetCount > 0U &&
        prepared.lineTargetCount <= residentBefore.lineCount &&
        prepared.spriteTargetCount == residentBefore.spriteCount &&
        prepared.entranceTargetCount == EspMapState_view()->entranceCells;
    mutationCountsValid =
        prepared.linesMutated > 0U && prepared.spritesMutated > 0U &&
        prepared.tilesMutated > 0U &&
        prepared.linesMutated <= prepared.lineTargetCount &&
        prepared.spritesMutated <= prepared.spriteTargetCount &&
        prepared.tilesMutated <= prepared.entranceTargetCount;
    if (!targetCountsValid || !mutationCountsValid) {
        printf("[JUNCTIONGIVEMAPPROBE] FAILED unexpected direct givemap plan targets=%u/%u/%u mutations=%u/%u/%u\n",
               (unsigned)prepared.lineTargetCount,
               (unsigned)prepared.spriteTargetCount,
               (unsigned)prepared.entranceTargetCount,
               (unsigned)prepared.linesMutated,
               (unsigned)prepared.spritesMutated,
               (unsigned)prepared.tilesMutated);
        probeState.done = 1;
        return;
    }

    status = EspPostLoadGiveMap_route();
    if (status != ESP_POST_LOAD_GIVEMAP_OK || !EspPostLoadGiveMap_isReady() ||
        EspPostLoadGiveMap_view() == NULL ||
        memcmp(&prepared, EspPostLoadGiveMap_view(), sizeof(prepared)) != 0 ||
        !inputOwnersCanonical() || EspAssetPack_isOpen() ||
        !EspMapResidentLifecycle_capture(&residentAfter) ||
        !residentAfterStructurallyValid(&residentBefore, &residentAfter)) {
        printf("[JUNCTIONGIVEMAPPROBE] FAILED live direct givemap route status=%u\n",
               (unsigned)status);
        probeState.done = 1;
        return;
    }

    liveState = EspPostLoadGiveMap_view();
    worldRevealed = worldFullyRevealed(liveState);
    if (!worldRevealed || residentAfter.mapStateFNV1a == residentBefore.mapStateFNV1a ||
        residentAfter.automapStateFNV1a == residentBefore.automapStateFNV1a) {
        printf("[JUNCTIONGIVEMAPPROBE] FAILED native givemap world semantics\n");
        probeState.done = 1;
        return;
    }

    memset(&directScratch, 0, sizeof(directScratch));
    if (EspMapAutomapState_planGiveMapDirect(&directScratch) != ESP_MAP_GIVEMAP_OK ||
        directScratch.lineTargetCount != liveState->lineTargetCount ||
        directScratch.spriteTargetCount != liveState->spriteTargetCount ||
        directScratch.entranceTargetCount != liveState->entranceTargetCount ||
        directScratch.linesMutated != 0U || directScratch.spritesMutated != 0U ||
        directScratch.tilesMutated != 0U) {
        printf("[JUNCTIONGIVEMAPPROBE] FAILED idempotent direct givemap plan\n");
        probeState.done = 1;
        return;
    }

    postActivePrepareGate =
        EspPostLoadGiveMap_prepare(&hudBefore, &scratch) ==
        ESP_POST_LOAD_GIVEMAP_UNSUPPORTED_ORDER;
    stateBeforeRepeat = *liveState;
    repeatGate = EspPostLoadGiveMap_route() == ESP_POST_LOAD_GIVEMAP_ALREADY_ACTIVE;
    repeatAtomic = EspMapResidentLifecycle_capture(&residentAfterRepeat) &&
                   memcmp(&residentAfter, &residentAfterRepeat,
                          sizeof(residentAfter)) == 0 &&
                   memcmp(&stateBeforeRepeat, EspPostLoadGiveMap_view(),
                          sizeof(stateBeforeRepeat)) == 0;

    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfter = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameAfter = framebufferHash();
    gameAfter = gameWitness(doomRpg->game);
    playerAfter = playerWitness(doomRpg->player);
    canvasAfter = canvasWitness(doomRpg->doomCanvas);
    renderAfter = renderWitness(doomRpg->render);
    hudLegacyAfter = hashBytes(doomRpg->hud, sizeof(*doomRpg->hud));
    hudOwnerAfter = hashBytes(EspHudPostLoadClear_view(), sizeof(EspHudPostLoadClearState));
    viewAfter = hashBytes(EspPlayerView_view(), sizeof(EspPlayerViewState));
    facingAfter = hashBytes(EspPlayerFacing_view(), sizeof(EspPlayerFacingState));

    if (heapBefore != heapAfter || largestBefore != largestAfter ||
        frameBefore != frameAfter || gameBefore != gameAfter ||
        playerBefore != playerAfter || canvasBefore != canvasAfter ||
        renderBefore != renderAfter || hudLegacyBefore != hudLegacyAfter ||
        hudOwnerBefore != hudOwnerAfter || viewBefore != viewAfter ||
        facingBefore != facingAfter || EspAssetPack_isOpen() ||
        !legacyRuntimeIsClear(doomRpg->render) ||
        doomRpg->doomCanvas->state != ST_INTRO ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0) {
        printf("[JUNCTIONGIVEMAPPROBE] FAILED integrity after direct givemap route\n");
        probeState.done = 1;
        return;
    }

    printf("[JUNCTIONGIVEMAP] READY stateBytes=%u directResultBytes=%u stateFNV=%08x lineTargets=%u spriteTargets=%u entranceTargets=%u linesMutated=%u spritesMutated=%u tilesMutated=%u active=%u targetMap=%u gameplayLoadMapId=%u loadType=%u\n",
           (unsigned)sizeof(*liveState), (unsigned)sizeof(EspMapGiveMapDirectResult),
           (unsigned)hashBytes(liveState, sizeof(*liveState)),
           (unsigned)liveState->lineTargetCount,
           (unsigned)liveState->spriteTargetCount,
           (unsigned)liveState->entranceTargetCount,
           (unsigned)liveState->linesMutated,
           (unsigned)liveState->spritesMutated,
           (unsigned)liveState->tilesMutated,
           (unsigned)liveState->active, (unsigned)liveState->targetMapId,
           (unsigned)liveState->gameplayLoadMapId, (unsigned)liveState->loadType);
    printf("[JUNCTIONGIVEMAP] INPUT hudClearFNV=%08x viewFNV=%08x facingFNV=%08x unchanged=yes callerOrder=yes\n",
           (unsigned)hudOwnerAfter, (unsigned)viewAfter, (unsigned)facingAfter);
    printf("[JUNCTIONGIVEMAP] WORLD mapFNV=%08x->%08x automapFNV=%08x->%08x runtimeFNV=%08x scriptFNV=%08x lineFNV=%08x textureFNV=%08x topologyFNV=%08x allTargetsRevealed=%s idempotentPlan=yes nonTargetOwnersUnchanged=yes\n",
           (unsigned)residentBefore.mapStateFNV1a,
           (unsigned)residentAfter.mapStateFNV1a,
           (unsigned)residentBefore.automapStateFNV1a,
           (unsigned)residentAfter.automapStateFNV1a,
           (unsigned)residentAfter.runtimeFNV1a,
           (unsigned)residentAfter.scriptStateFNV1a,
           (unsigned)residentAfter.lineStateFNV1a,
           (unsigned)residentAfter.textureStateFNV1a,
           (unsigned)residentAfter.topologyFNV1a,
           worldRevealed ? "yes" : "no");
    printf("[JUNCTIONGIVEMAP] FAILCLOSED nullHud=%d nullOutput=%d inactiveHud=%d uncleared=%d targetMap=%d gameplayMap=%d loadType=%d plannerNull=%d prepareAtomic=%s postActivePrepare=%d repeat=%d repeatAtomic=%s\n",
           nullHudGate, nullOutputGate, inactiveHudGate, unclearedGate,
           targetMapGate, gameplayMapGate, loadTypeGate, plannerNullGate,
           prepareAtomic ? "yes" : "no", postActivePrepareGate, repeatGate,
           repeatAtomic ? "yes" : "no");
    printf("[JUNCTIONGIVEMAP] RESIDENT snapshotFNV=%08x->%08x payload=%u entities=%u enemies=%u destructibles=%u packClosed=yes worldMutationExpected=yes\n",
           (unsigned)hashBytes(&residentBefore, sizeof(residentBefore)),
           (unsigned)hashBytes(&residentAfter, sizeof(residentAfter)),
           (unsigned)residentAfter.totalPayloadBytes,
           (unsigned)residentAfter.entityCount,
           (unsigned)residentAfter.enemyCount,
           (unsigned)residentAfter.destructibleCount);
    printf("[JUNCTIONGIVEMAP] RAM heap8=%u->%u delta=%ld largest8=%u->%u delta=%ld persistentHeapBytes=0\n",
           (unsigned)heapBefore, (unsigned)heapAfter,
           (long)((int32_t)heapAfter - (int32_t)heapBefore),
           (unsigned)largestBefore, (unsigned)largestAfter,
           (long)((int32_t)largestAfter - (int32_t)largestBefore));
    printf("[JUNCTIONGIVEMAP] LEGACY gameFNV=%08x->%08x playerFNV=%08x->%08x hudFNV=%08x->%08x canvasFNV=%08x->%08x renderFNV=%08x->%08x frameFNV=%08x->%08x legacyRuntimeClear=yes GameMutation=no PlayerMutation=no HudMutation=no DoomCanvasMutation=no RenderMutation=no legacyGame_givemapCalled=no\n",
           (unsigned)gameBefore, (unsigned)gameAfter,
           (unsigned)playerBefore, (unsigned)playerAfter,
           (unsigned)hudLegacyBefore, (unsigned)hudLegacyAfter,
           (unsigned)canvasBefore, (unsigned)canvasAfter,
           (unsigned)renderBefore, (unsigned)renderAfter,
           (unsigned)frameBefore, (unsigned)frameAfter);
    printf("[JUNCTIONGIVEMAP] PARK state=%d page=%d targetMap=%u junctionResident=yes nativeHudClear=yes nativePostLoadGiveMap=yes Game_givemapPending=no weaponReselectPending=yes initialSavePending=yes postLoadCleanupPending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned)liveState->targetMapId);

    probeState.done = 1;
}
