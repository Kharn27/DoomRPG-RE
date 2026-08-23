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
#include "esp_map_resident_lifecycle.h"
#include "esp_post_load_flag_cleanup_state.h"
#include "esp_post_load_initial_save_intent.h"
#include "native_junction_post_load_flag_cleanup_probe.h"
#include "native_junction_post_load_initial_save_intent_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_CLEANUP_BYTES 8U
#define EXPECTED_SAVE_INTENT_BYTES 24U
#define EXPECTED_SAVE_INTENT_FNV 0x0bf1a911U
#define EXPECTED_SNAPSHOT_FNV 0xbb714d80U
#define EXPECTED_RUNTIME_FNV 0xbc432a0fU
#define EXPECTED_MAP_FNV 0x8dba0bb4U
#define EXPECTED_SCRIPT_FNV 0xbc9b18ffU
#define EXPECTED_LINE_FNV 0x3658710dU
#define EXPECTED_TEXTURE_FNV 0x537319adU
#define EXPECTED_AUTOMAP_FNV 0xb699bd75U
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
    uint32_t v[17];
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
    v[13] = (uint32_t)game->newDestX;
    v[14] = (uint32_t)game->newDestY;
    v[15] = (uint32_t)game->newAngle;
    v[16] = hashBytes(game->newMapName, (uint32_t)sizeof(game->newMapName));
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

static uint32_t hudWitness(const Hud_t* hud) {
    uint32_t hash = 2166136261U;
    uint32_t i;
    const uint8_t* p;

    if (hud == NULL) return 0U;
    hash ^= (uint32_t)hud->msgCount; hash *= 16777619U;
    hash ^= (uint32_t)hud->msgTime; hash *= 16777619U;
    hash ^= (uint32_t)hud->msgDuration; hash *= 16777619U;
    hash ^= (uint32_t)hud->isUpdate; hash *= 16777619U;
    hash ^= (uint32_t)(uintptr_t)hud->statBarMessage; hash *= 16777619U;
    p = (const uint8_t*)hud->messages;
    for (i = 0U; i < (uint32_t)sizeof(hud->messages); ++i) {
        hash ^= p[i]; hash *= 16777619U;
    }
    p = (const uint8_t*)hud->logMessage;
    for (i = 0U; i < (uint32_t)sizeof(hud->logMessage); ++i) {
        hash ^= p[i]; hash *= 16777619U;
    }
    return hash;
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

static int residentCanonical(const EspMapResidentSnapshot* s) {
    return s != NULL && sizeof(*s) == 96U &&
           s->totalPayloadBytes == 10410U &&
           s->runtimeArenaBytes == 8867U && s->mapStateBytes == 1024U &&
           s->scriptStateBytes == 73U && s->lineStateBytes == 52U &&
           s->textureStateBytes == 26U && s->automapStateBytes == 32U &&
           s->topologyBytes == 336U &&
           s->runtimeFNV1a == EXPECTED_RUNTIME_FNV &&
           s->mapStateFNV1a == EXPECTED_MAP_FNV &&
           s->scriptStateFNV1a == EXPECTED_SCRIPT_FNV &&
           s->lineStateFNV1a == EXPECTED_LINE_FNV &&
           s->textureStateFNV1a == EXPECTED_TEXTURE_FNV &&
           s->automapStateFNV1a == EXPECTED_AUTOMAP_FNV &&
           s->topologyFNV1a == EXPECTED_TOPOLOGY_FNV &&
           s->entityCount == 30U && s->enemyCount == 0U &&
           s->destructibleCount == 3U &&
           hashBytes(s, sizeof(*s)) == EXPECTED_SNAPSHOT_FNV;
}

static int saveIntentCanonical(void) {
    const EspPostLoadInitialSaveIntentState* state =
        EspPostLoadInitialSaveIntent_view();
    return state != NULL && sizeof(*state) == EXPECTED_SAVE_INTENT_BYTES &&
           hashBytes(state, sizeof(*state)) == EXPECTED_SAVE_INTENT_FNV &&
           state->mapId == 9U && state->viewX == 992 && state->viewY == 1888 &&
           state->viewAngle == 64 && state->isLoadedBefore == 0U &&
           state->saveMode == 0U && state->saveRequired == 1U &&
           state->componentMask == ESP_POST_LOAD_SAVE_COMPONENT_ALL &&
           state->persistenceDeferred == 1U &&
           state->presentationDeferred == 1U && state->active == 1U;
}

static int stateIsZero(const EspPostLoadFlagCleanupState* state) {
    EspPostLoadFlagCleanupState zero;
    if (state == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(state, &zero, sizeof(zero)) == 0;
}

void Esp32JunctionPostLoadFlagCleanupProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspPostLoadFlagCleanup_reset();
}

int Esp32JunctionPostLoadFlagCleanupProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionPostLoadFlagCleanupProbe_service(struct DoomRPG_s* doomRpg) {
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentAfter;
    EspPostLoadInitialSaveIntentState badIntent;
    EspPostLoadFlagCleanupState scratch;
    EspPostLoadFlagCleanupState prepared;
    EspPostLoadFlagCleanupState beforeRepeat;
    const EspPostLoadFlagCleanupState* live;
    uint32_t heapBefore, heapAfter, largestBefore, largestAfter;
    uint32_t frameBefore, frameAfter, gameBefore, gameAfter;
    uint32_t playerBefore, playerAfter, hudBefore, hudAfter;
    uint32_t canvasBefore, canvasAfter, renderBefore, renderAfter;
    uint32_t saveIntentBefore, saveIntentAfter;
    uint8_t isLoadedBefore, isSavedBefore, activeLoadTypeBefore;
    int nullIntentGate, nullOutputGate, inactiveIntentGate, mapGate;
    int invalidLoadedGate, invalidSavedGate, invalidLoadTypeGate;
    int loadedMismatchGate, prepareAtomic, postActivePrepareGate;
    int repeatGate, repeatAtomic;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionPostLoadInitialSaveIntentProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[JUNCTIONFLAGCLEANUPPROBE] ARMED initial-save semantic intent complete; three scalar post-load Game flag clears start on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native Junction post-load flag cleanup ===\n");
    printf("[JUNCTIONFLAGCLEANUPPROBE] CONTRACT recover only contiguous legacy writes Game.isLoaded=false, Game.isSaved=false, Game.activeLoadType=0 after the hardware-proven initial-save intent; park one 8B pointer-free before/after owner, do not mutate legacy Game, do not clear queued events/particles, do not set isUpdateView/ST_PLAYING/idleTime, do not present and do not allocate\n");

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL || doomRpg->game == NULL ||
        doomRpg->render == NULL || doomRpg->player == NULL || doomRpg->hud == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO || doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->game->isLoaded != false || doomRpg->game->isSaved > 1 ||
        doomRpg->game->activeLoadType < 0 || doomRpg->game->activeLoadType > 2 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        EspAssetPack_isOpen() || !legacyRuntimeIsClear(doomRpg->render) ||
        sizeof(EspPostLoadFlagCleanupState) != EXPECTED_CLEANUP_BYTES ||
        !saveIntentCanonical() ||
        !EspMapResidentLifecycle_capture(&residentBefore) ||
        !residentCanonical(&residentBefore)) {
        printf("[JUNCTIONFLAGCLEANUPPROBE] FAILED unsafe save-intent boundary isLoaded=%d isSaved=%d activeLoadType=%d\n",
               doomRpg != NULL && doomRpg->game != NULL ? doomRpg->game->isLoaded : -1,
               doomRpg != NULL && doomRpg->game != NULL ? doomRpg->game->isSaved : -1,
               doomRpg != NULL && doomRpg->game != NULL ? doomRpg->game->activeLoadType : -1);
        return;
    }

    isLoadedBefore = (uint8_t)doomRpg->game->isLoaded;
    isSavedBefore = (uint8_t)doomRpg->game->isSaved;
    activeLoadTypeBefore = (uint8_t)doomRpg->game->activeLoadType;

    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    gameBefore = gameWitness(doomRpg->game);
    playerBefore = playerWitness(doomRpg->player);
    hudBefore = hudWitness(doomRpg->hud);
    canvasBefore = canvasWitness(doomRpg->doomCanvas);
    renderBefore = renderWitness(doomRpg->render);
    saveIntentBefore = hashBytes(EspPostLoadInitialSaveIntent_view(),
                                 sizeof(EspPostLoadInitialSaveIntentState));

    EspPostLoadFlagCleanup_reset();
    memset(&scratch, 0xa5, sizeof(scratch));
    nullIntentGate =
        EspPostLoadFlagCleanup_prepare(NULL, isLoadedBefore, isSavedBefore,
                                       activeLoadTypeBefore, &scratch) ==
            ESP_POST_LOAD_FLAG_CLEANUP_INVALID && stateIsZero(&scratch) &&
        !EspPostLoadFlagCleanup_isReady();

    nullOutputGate =
        EspPostLoadFlagCleanup_prepare(EspPostLoadInitialSaveIntent_view(),
                                       isLoadedBefore, isSavedBefore,
                                       activeLoadTypeBefore, NULL) ==
            ESP_POST_LOAD_FLAG_CLEANUP_INVALID &&
        !EspPostLoadFlagCleanup_isReady();

    badIntent = *EspPostLoadInitialSaveIntent_view();
    badIntent.active = 0U;
    memset(&scratch, 0xa5, sizeof(scratch));
    inactiveIntentGate =
        EspPostLoadFlagCleanup_prepare(&badIntent, isLoadedBefore, isSavedBefore,
                                       activeLoadTypeBefore, &scratch) ==
            ESP_POST_LOAD_FLAG_CLEANUP_SAVE_INTENT_INVALID &&
        stateIsZero(&scratch) && !EspPostLoadFlagCleanup_isReady();

    badIntent = *EspPostLoadInitialSaveIntent_view();
    badIntent.mapId = 8U;
    memset(&scratch, 0xa5, sizeof(scratch));
    mapGate = EspPostLoadFlagCleanup_prepare(
                  &badIntent, isLoadedBefore, isSavedBefore,
                  activeLoadTypeBefore, &scratch) ==
                  ESP_POST_LOAD_FLAG_CLEANUP_SAVE_INTENT_INVALID &&
              stateIsZero(&scratch) && !EspPostLoadFlagCleanup_isReady();

    memset(&scratch, 0xa5, sizeof(scratch));
    invalidLoadedGate =
        EspPostLoadFlagCleanup_prepare(EspPostLoadInitialSaveIntent_view(), 2U,
                                       isSavedBefore, activeLoadTypeBefore,
                                       &scratch) ==
            ESP_POST_LOAD_FLAG_CLEANUP_UNSUPPORTED_CONTEXT &&
        stateIsZero(&scratch) && !EspPostLoadFlagCleanup_isReady();

    memset(&scratch, 0xa5, sizeof(scratch));
    invalidSavedGate =
        EspPostLoadFlagCleanup_prepare(EspPostLoadInitialSaveIntent_view(),
                                       isLoadedBefore, 2U,
                                       activeLoadTypeBefore, &scratch) ==
            ESP_POST_LOAD_FLAG_CLEANUP_UNSUPPORTED_CONTEXT &&
        stateIsZero(&scratch) && !EspPostLoadFlagCleanup_isReady();

    memset(&scratch, 0xa5, sizeof(scratch));
    invalidLoadTypeGate =
        EspPostLoadFlagCleanup_prepare(EspPostLoadInitialSaveIntent_view(),
                                       isLoadedBefore, isSavedBefore, 3U,
                                       &scratch) ==
            ESP_POST_LOAD_FLAG_CLEANUP_UNSUPPORTED_CONTEXT &&
        stateIsZero(&scratch) && !EspPostLoadFlagCleanup_isReady();

    memset(&scratch, 0xa5, sizeof(scratch));
    loadedMismatchGate =
        EspPostLoadFlagCleanup_prepare(EspPostLoadInitialSaveIntent_view(), 1U,
                                       isSavedBefore, activeLoadTypeBefore,
                                       &scratch) ==
            ESP_POST_LOAD_FLAG_CLEANUP_UNSUPPORTED_CONTEXT &&
        stateIsZero(&scratch) && !EspPostLoadFlagCleanup_isReady();

    prepareAtomic = nullIntentGate && nullOutputGate && inactiveIntentGate &&
                    mapGate && invalidLoadedGate && invalidSavedGate &&
                    invalidLoadTypeGate && loadedMismatchGate &&
                    hashBytes(EspPostLoadInitialSaveIntent_view(),
                              sizeof(EspPostLoadInitialSaveIntentState)) ==
                        saveIntentBefore;

    memset(&prepared, 0, sizeof(prepared));
    if (!prepareAtomic ||
        EspPostLoadFlagCleanup_prepare(EspPostLoadInitialSaveIntent_view(),
                                       isLoadedBefore, isSavedBefore,
                                       activeLoadTypeBefore, &prepared) !=
            ESP_POST_LOAD_FLAG_CLEANUP_OK ||
        prepared.isLoadedBefore != isLoadedBefore ||
        prepared.isSavedBefore != isSavedBefore ||
        prepared.activeLoadTypeBefore != activeLoadTypeBefore ||
        prepared.isLoadedAfter != 0U || prepared.isSavedAfter != 0U ||
        prepared.activeLoadTypeAfter != 0U || prepared.targetMapId != 9U ||
        prepared.active != 1U || EspPostLoadFlagCleanup_isReady()) {
        printf("[JUNCTIONFLAGCLEANUPPROBE] FAILED pure cleanup prepare\n");
        return;
    }

    if (EspPostLoadFlagCleanup_route(isLoadedBefore, isSavedBefore,
                                     activeLoadTypeBefore) !=
            ESP_POST_LOAD_FLAG_CLEANUP_OK ||
        !EspPostLoadFlagCleanup_isReady() || EspPostLoadFlagCleanup_view() == NULL ||
        memcmp(&prepared, EspPostLoadFlagCleanup_view(), sizeof(prepared)) != 0) {
        printf("[JUNCTIONFLAGCLEANUPPROBE] FAILED cleanup route\n");
        return;
    }

    live = EspPostLoadFlagCleanup_view();
    beforeRepeat = *live;
    memset(&scratch, 0xa5, sizeof(scratch));
    postActivePrepareGate =
        EspPostLoadFlagCleanup_prepare(EspPostLoadInitialSaveIntent_view(),
                                       isLoadedBefore, isSavedBefore,
                                       activeLoadTypeBefore, &scratch) ==
            ESP_POST_LOAD_FLAG_CLEANUP_ALREADY_ACTIVE &&
        stateIsZero(&scratch);
    repeatGate =
        EspPostLoadFlagCleanup_route(isLoadedBefore, isSavedBefore,
                                     activeLoadTypeBefore) ==
        ESP_POST_LOAD_FLAG_CLEANUP_ALREADY_ACTIVE;
    repeatAtomic = EspPostLoadFlagCleanup_view() != NULL &&
                   memcmp(&beforeRepeat, EspPostLoadFlagCleanup_view(),
                          sizeof(beforeRepeat)) == 0;

    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfter = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameAfter = framebufferHash();
    gameAfter = gameWitness(doomRpg->game);
    playerAfter = playerWitness(doomRpg->player);
    hudAfter = hudWitness(doomRpg->hud);
    canvasAfter = canvasWitness(doomRpg->doomCanvas);
    renderAfter = renderWitness(doomRpg->render);
    saveIntentAfter = hashBytes(EspPostLoadInitialSaveIntent_view(),
                                sizeof(EspPostLoadInitialSaveIntentState));

    if (!postActivePrepareGate || !repeatGate || !repeatAtomic ||
        heapBefore != heapAfter || largestBefore != largestAfter ||
        frameBefore != frameAfter || gameBefore != gameAfter ||
        playerBefore != playerAfter || hudBefore != hudAfter ||
        canvasBefore != canvasAfter || renderBefore != renderAfter ||
        saveIntentBefore != saveIntentAfter ||
        saveIntentAfter != EXPECTED_SAVE_INTENT_FNV ||
        !EspMapResidentLifecycle_capture(&residentAfter) ||
        !residentCanonical(&residentAfter) ||
        memcmp(&residentBefore, &residentAfter, sizeof(residentBefore)) != 0 ||
        EspAssetPack_isOpen() || !legacyRuntimeIsClear(doomRpg->render) ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        doomRpg->doomCanvas->state != ST_INTRO) {
        printf("[JUNCTIONFLAGCLEANUPPROBE] FAILED integrity after cleanup owner\n");
        return;
    }

    live = EspPostLoadFlagCleanup_view();
    printf("[JUNCTIONFLAGCLEANUP] READY stateBytes=%u stateFNV=%08x isLoaded=%u->%u isSaved=%u->%u activeLoadType=%u->%u targetMap=%u active=%u\n",
           (unsigned)sizeof(*live), (unsigned)hashBytes(live, sizeof(*live)),
           (unsigned)live->isLoadedBefore, (unsigned)live->isLoadedAfter,
           (unsigned)live->isSavedBefore, (unsigned)live->isSavedAfter,
           (unsigned)live->activeLoadTypeBefore,
           (unsigned)live->activeLoadTypeAfter,
           (unsigned)live->targetMapId, (unsigned)live->active);
    printf("[JUNCTIONFLAGCLEANUP] SEMANTIC isLoadedCleared=yes isSavedCleared=yes activeLoadTypeCleared=yes legacyValues=%u/%u/%u->0/0/0 legacyMutation=no\n",
           (unsigned)isLoadedBefore, (unsigned)isSavedBefore,
           (unsigned)activeLoadTypeBefore);
    printf("[JUNCTIONFLAGCLEANUP] INPUT saveIntentBytes=%u saveIntentFNV=%08x unchanged=yes callerOrder=yes persistenceDebtPreserved=yes\n",
           (unsigned)sizeof(EspPostLoadInitialSaveIntentState),
           (unsigned)saveIntentAfter);
    printf("[JUNCTIONFLAGCLEANUP] FAILCLOSED nullIntent=%d nullOutput=%d inactiveIntent=%d targetMap=%d invalidLoaded=%d invalidSaved=%d invalidLoadType=%d loadedMismatch=%d prepareAtomic=%s postActivePrepare=%d repeat=%d repeatAtomic=%s\n",
           nullIntentGate, nullOutputGate, inactiveIntentGate, mapGate,
           invalidLoadedGate, invalidSavedGate, invalidLoadTypeGate,
           loadedMismatchGate, prepareAtomic ? "yes" : "no",
           postActivePrepareGate, repeatGate, repeatAtomic ? "yes" : "no");
    printf("[JUNCTIONFLAGCLEANUP] RESIDENT snapshotFNV=%08x->%08x unchanged=yes mapFNV=%08x automapFNV=%08x runtimeFNV=%08x scriptFNV=%08x lineFNV=%08x textureFNV=%08x topologyFNV=%08x payload=%u entities=%u enemies=%u destructibles=%u packClosed=yes\n",
           (unsigned)hashBytes(&residentBefore, sizeof(residentBefore)),
           (unsigned)hashBytes(&residentAfter, sizeof(residentAfter)),
           (unsigned)residentAfter.mapStateFNV1a,
           (unsigned)residentAfter.automapStateFNV1a,
           (unsigned)residentAfter.runtimeFNV1a,
           (unsigned)residentAfter.scriptStateFNV1a,
           (unsigned)residentAfter.lineStateFNV1a,
           (unsigned)residentAfter.textureStateFNV1a,
           (unsigned)residentAfter.topologyFNV1a,
           (unsigned)residentAfter.totalPayloadBytes,
           (unsigned)residentAfter.entityCount,
           (unsigned)residentAfter.enemyCount,
           (unsigned)residentAfter.destructibleCount);
    printf("[JUNCTIONFLAGCLEANUP] RAM heap8=%u->%u delta=%ld largest8=%u->%u delta=%ld persistentHeapBytes=0\n",
           (unsigned)heapBefore, (unsigned)heapAfter,
           (long)((int32_t)heapAfter - (int32_t)heapBefore),
           (unsigned)largestBefore, (unsigned)largestAfter,
           (long)((int32_t)largestAfter - (int32_t)largestBefore));
    printf("[JUNCTIONFLAGCLEANUP] LEGACY gameFNV=%08x->%08x playerFNV=%08x->%08x hudFNV=%08x->%08x canvasFNV=%08x->%08x renderFNV=%08x->%08x frameFNV=%08x->%08x legacyRuntimeClear=yes GameMutation=no PlayerMutation=no HudMutation=no DoomCanvasMutation=no RenderMutation=no\n",
           (unsigned)gameBefore, (unsigned)gameAfter,
           (unsigned)playerBefore, (unsigned)playerAfter,
           (unsigned)hudBefore, (unsigned)hudAfter,
           (unsigned)canvasBefore, (unsigned)canvasAfter,
           (unsigned)renderBefore, (unsigned)renderAfter,
           (unsigned)frameBefore, (unsigned)frameAfter);
    printf("[JUNCTIONFLAGCLEANUP] PARK state=%d page=%d targetMap=%u junctionResident=yes nativeInitialSaveIntent=yes nativePostLoadFlagCleanup=yes initialSavePersistencePending=yes flagCleanupPending=no eventParticleCleanupPending=yes isUpdateViewPending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned)live->targetMapId);

    probeState.done = 1;
}
