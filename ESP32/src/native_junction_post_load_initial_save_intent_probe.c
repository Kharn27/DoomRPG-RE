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
#include "esp_map_resident_lifecycle.h"
#include "esp_player_facing_state.h"
#include "esp_player_view_state.h"
#include "esp_post_load_givemap_state.h"
#include "esp_post_load_initial_save_intent.h"
#include "esp_post_load_weapon_select_state.h"
#include "native_junction_post_load_initial_save_intent_probe.h"
#include "native_junction_post_load_weapon_select_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_SAVE_INTENT_BYTES 24U
#define EXPECTED_WEAPON_BYTES 8U
#define EXPECTED_WEAPON_FNV 0x699f3cf3U
#define EXPECTED_GIVEMAP_BYTES 16U
#define EXPECTED_GIVEMAP_FNV 0x448e587dU
#define EXPECTED_HUD_CLEAR_BYTES 8U
#define EXPECTED_HUD_CLEAR_FNV 0xb7383e18U
#define EXPECTED_VIEW_BYTES 44U
#define EXPECTED_VIEW_FNV 0xafcdcf74U
#define EXPECTED_FACING_BYTES 32U
#define EXPECTED_FACING_FNV 0x95aa1108U

#define EXPECTED_SAVE_MAP_ID 9U
#define EXPECTED_SAVE_VIEW_X 992
#define EXPECTED_SAVE_VIEW_Y 1888
#define EXPECTED_SAVE_VIEW_ANGLE 64

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

static int stateIsZero(const EspPostLoadInitialSaveIntentState* state) {
    EspPostLoadInitialSaveIntentState zero;
    if (state == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(state, &zero, sizeof(zero)) == 0;
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

static int precedingOwnersCanonical(void) {
    return sizeof(EspPostLoadWeaponSelectState) == EXPECTED_WEAPON_BYTES &&
           sizeof(EspPostLoadGiveMapState) == EXPECTED_GIVEMAP_BYTES &&
           sizeof(EspHudPostLoadClearState) == EXPECTED_HUD_CLEAR_BYTES &&
           sizeof(EspPlayerViewState) == EXPECTED_VIEW_BYTES &&
           sizeof(EspPlayerFacingState) == EXPECTED_FACING_BYTES &&
           EspPostLoadWeaponSelect_view() != NULL &&
           EspPostLoadGiveMap_view() != NULL &&
           EspHudPostLoadClear_view() != NULL && EspPlayerView_view() != NULL &&
           EspPlayerFacing_view() != NULL &&
           hashBytes(EspPostLoadWeaponSelect_view(),
                     sizeof(EspPostLoadWeaponSelectState)) == EXPECTED_WEAPON_FNV &&
           hashBytes(EspPostLoadGiveMap_view(), sizeof(EspPostLoadGiveMapState)) ==
               EXPECTED_GIVEMAP_FNV &&
           hashBytes(EspHudPostLoadClear_view(), sizeof(EspHudPostLoadClearState)) ==
               EXPECTED_HUD_CLEAR_FNV &&
           hashBytes(EspPlayerView_view(), sizeof(EspPlayerViewState)) ==
               EXPECTED_VIEW_FNV &&
           hashBytes(EspPlayerFacing_view(), sizeof(EspPlayerFacingState)) ==
               EXPECTED_FACING_FNV;
}

void Esp32JunctionPostLoadInitialSaveIntentProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspPostLoadInitialSaveIntent_reset();
}

int Esp32JunctionPostLoadInitialSaveIntentProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionPostLoadInitialSaveIntentProbe_service(
    struct DoomRPG_s* doomRpg) {
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentPrepared;
    EspMapResidentSnapshot residentAfter;
    EspMapResidentSnapshot residentAfterRepeat;
    EspPostLoadWeaponSelectState weaponBeforeOwner;
    EspPostLoadWeaponSelectState badWeapon;
    EspPlayerViewState viewBeforeOwner;
    EspPlayerViewState badView;
    EspPostLoadInitialSaveIntentState scratch;
    EspPostLoadInitialSaveIntentState prepared;
    EspPostLoadInitialSaveIntentState beforeRepeat;
    const EspPostLoadInitialSaveIntentState* liveState;
    EspPostLoadInitialSaveIntentStatus status;
    uint32_t heapBefore, heapAfter, largestBefore, largestAfter;
    uint32_t frameBefore, frameAfter, gameBefore, gameAfter;
    uint32_t playerBefore, playerAfter, canvasBefore, canvasAfter;
    uint32_t renderBefore, renderAfter, hudBefore, hudAfter;
    uint32_t weaponFNVBefore, weaponFNVAfter, giveMapFNVBefore, giveMapFNVAfter;
    uint32_t hudOwnerBefore, hudOwnerAfter, viewFNVBefore, viewFNVAfter;
    uint32_t facingFNVBefore, facingFNVAfter;
    int nullWeaponGate, nullViewGate, nullOutputGate, inactiveWeaponGate;
    int weaponMismatchGate, inactiveViewGate, viewPendingGate;
    int loadedContextGate, invalidLoadedGate, prepareAtomic;
    int postActivePrepareGate, repeatGate, repeatAtomic;
    int legacyRoutePresent;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionPostLoadWeaponSelectProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[JUNCTIONSAVEINTENTPROBE] ARMED post-load weapon self-select boundary complete; initial Game_saveState caller intent starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native Junction post-load initial save intent ===\n");
    printf("[JUNCTIONSAVEINTENTPROBE] CONTRACT recover only the fresh-Junction caller decision if (loadMapID != MAP_END_GAME && !Game.isLoaded) Game_saveState(game, loadMapID, viewX, viewY, viewAngle, false): source map/view arguments from the hardware-proven native PlayerView, capture Config+Player2+World+Player component intent in one 24B pointer-free owner; do not call legacy Game_saveState, do not show Saving UI, do not present, do not write Config/Player2/World/Player, keep actual persistence/load cleanup/ST_PLAYING deferred and do not allocate\n");

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL || doomRpg->game == NULL ||
        doomRpg->render == NULL || doomRpg->player == NULL || doomRpg->hud == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO || doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->game->isLoaded != false || doomRpg->game->numEntities != 0 ||
        doomRpg->game->numMonsters != 0 || EspAssetPack_isOpen() ||
        !legacyRuntimeIsClear(doomRpg->render) ||
        sizeof(EspPostLoadInitialSaveIntentState) != EXPECTED_SAVE_INTENT_BYTES ||
        !precedingOwnersCanonical() ||
        !EspMapResidentLifecycle_capture(&residentBefore) ||
        !residentCanonical(&residentBefore)) {
        printf("[JUNCTIONSAVEINTENTPROBE] FAILED unsafe post-weapon boundary isLoaded=%d\n",
               doomRpg != NULL && doomRpg->game != NULL
                   ? doomRpg->game->isLoaded
                   : -1);
        return;
    }

    weaponBeforeOwner = *EspPostLoadWeaponSelect_view();
    viewBeforeOwner = *EspPlayerView_view();
    weaponFNVBefore = hashBytes(&weaponBeforeOwner, sizeof(weaponBeforeOwner));
    giveMapFNVBefore =
        hashBytes(EspPostLoadGiveMap_view(), sizeof(EspPostLoadGiveMapState));
    hudOwnerBefore =
        hashBytes(EspHudPostLoadClear_view(), sizeof(EspHudPostLoadClearState));
    viewFNVBefore = hashBytes(&viewBeforeOwner, sizeof(viewBeforeOwner));
    facingFNVBefore =
        hashBytes(EspPlayerFacing_view(), sizeof(EspPlayerFacingState));

    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    gameBefore = gameWitness(doomRpg->game);
    playerBefore = playerWitness(doomRpg->player);
    canvasBefore = canvasWitness(doomRpg->doomCanvas);
    renderBefore = renderWitness(doomRpg->render);
    hudBefore = hashBytes(doomRpg->hud, sizeof(*doomRpg->hud));

    memset(&scratch, 0xa5, sizeof(scratch));
    nullWeaponGate =
        EspPostLoadInitialSaveIntent_prepare(NULL, &viewBeforeOwner, 0U, &scratch) ==
            ESP_POST_LOAD_INITIAL_SAVE_INTENT_INVALID &&
        stateIsZero(&scratch);
    memset(&scratch, 0xa5, sizeof(scratch));
    nullViewGate = EspPostLoadInitialSaveIntent_prepare(
                       &weaponBeforeOwner, NULL, 0U, &scratch) ==
                       ESP_POST_LOAD_INITIAL_SAVE_INTENT_INVALID &&
                   stateIsZero(&scratch);
    nullOutputGate = EspPostLoadInitialSaveIntent_prepare(
                         &weaponBeforeOwner, &viewBeforeOwner, 0U, NULL) ==
                     ESP_POST_LOAD_INITIAL_SAVE_INTENT_INVALID;

    badWeapon = weaponBeforeOwner;
    badWeapon.active = 0U;
    memset(&scratch, 0xa5, sizeof(scratch));
    inactiveWeaponGate = EspPostLoadInitialSaveIntent_prepare(
                             &badWeapon, &viewBeforeOwner, 0U, &scratch) ==
                             ESP_POST_LOAD_INITIAL_SAVE_INTENT_WEAPON_INVALID &&
                         stateIsZero(&scratch);

    badWeapon = weaponBeforeOwner;
    badWeapon.requestedWeapon = (uint8_t)(badWeapon.weaponBefore + 1U);
    memset(&scratch, 0xa5, sizeof(scratch));
    weaponMismatchGate = EspPostLoadInitialSaveIntent_prepare(
                             &badWeapon, &viewBeforeOwner, 0U, &scratch) ==
                             ESP_POST_LOAD_INITIAL_SAVE_INTENT_WEAPON_INVALID &&
                         stateIsZero(&scratch);

    badView = viewBeforeOwner;
    badView.active = 0U;
    memset(&scratch, 0xa5, sizeof(scratch));
    inactiveViewGate = EspPostLoadInitialSaveIntent_prepare(
                           &weaponBeforeOwner, &badView, 0U, &scratch) ==
                           ESP_POST_LOAD_INITIAL_SAVE_INTENT_VIEW_INVALID &&
                       stateIsZero(&scratch);

    badView = viewBeforeOwner;
    badView.facingRefreshPending = 1U;
    memset(&scratch, 0xa5, sizeof(scratch));
    viewPendingGate = EspPostLoadInitialSaveIntent_prepare(
                          &weaponBeforeOwner, &badView, 0U, &scratch) ==
                          ESP_POST_LOAD_INITIAL_SAVE_INTENT_VIEW_INVALID &&
                      stateIsZero(&scratch);

    memset(&scratch, 0xa5, sizeof(scratch));
    loadedContextGate = EspPostLoadInitialSaveIntent_prepare(
                            &weaponBeforeOwner, &viewBeforeOwner, 1U, &scratch) ==
                            ESP_POST_LOAD_INITIAL_SAVE_INTENT_LOADED_CONTEXT_DEFERRED &&
                        stateIsZero(&scratch);
    memset(&scratch, 0xa5, sizeof(scratch));
    invalidLoadedGate = EspPostLoadInitialSaveIntent_prepare(
                            &weaponBeforeOwner, &viewBeforeOwner, 2U, &scratch) ==
                            ESP_POST_LOAD_INITIAL_SAVE_INTENT_UNSUPPORTED_CONTEXT &&
                        stateIsZero(&scratch);

    memset(&prepared, 0, sizeof(prepared));
    status = EspPostLoadInitialSaveIntent_prepare(
        &weaponBeforeOwner, &viewBeforeOwner, 0U, &prepared);
    if (status != ESP_POST_LOAD_INITIAL_SAVE_INTENT_OK || prepared.active != 1U ||
        prepared.mapId != EXPECTED_SAVE_MAP_ID || prepared.isLoadedBefore != 0U ||
        prepared.saveMode != 0U || prepared.saveRequired != 1U ||
        prepared.componentMask != ESP_POST_LOAD_SAVE_COMPONENT_ALL ||
        prepared.persistenceDeferred != 1U ||
        prepared.presentationDeferred != 1U ||
        prepared.viewX != EXPECTED_SAVE_VIEW_X ||
        prepared.viewY != EXPECTED_SAVE_VIEW_Y ||
        prepared.viewAngle != EXPECTED_SAVE_VIEW_ANGLE ||
        EspPostLoadInitialSaveIntent_isReady() ||
        !EspMapResidentLifecycle_capture(&residentPrepared) ||
        memcmp(&residentBefore, &residentPrepared, sizeof(residentBefore)) != 0 ||
        !precedingOwnersCanonical() || EspAssetPack_isOpen()) {
        printf("[JUNCTIONSAVEINTENTPROBE] FAILED pure initial-save intent preparation status=%u\n",
               (unsigned)status);
        return;
    }
    prepareAtomic = 1;

    status = EspPostLoadInitialSaveIntent_route(0U);
    if (status != ESP_POST_LOAD_INITIAL_SAVE_INTENT_OK ||
        !EspPostLoadInitialSaveIntent_isReady() ||
        EspPostLoadInitialSaveIntent_view() == NULL ||
        memcmp(&prepared, EspPostLoadInitialSaveIntent_view(), sizeof(prepared)) != 0 ||
        !precedingOwnersCanonical() || EspAssetPack_isOpen() ||
        !EspMapResidentLifecycle_capture(&residentAfter) ||
        memcmp(&residentBefore, &residentAfter, sizeof(residentBefore)) != 0) {
        printf("[JUNCTIONSAVEINTENTPROBE] FAILED live initial-save intent route status=%u\n",
               (unsigned)status);
        return;
    }

    liveState = EspPostLoadInitialSaveIntent_view();
    postActivePrepareGate =
        EspPostLoadInitialSaveIntent_prepare(
            &weaponBeforeOwner, &viewBeforeOwner, 0U, &scratch) ==
        ESP_POST_LOAD_INITIAL_SAVE_INTENT_ALREADY_ACTIVE;
    beforeRepeat = *liveState;
    repeatGate = EspPostLoadInitialSaveIntent_route(0U) ==
                 ESP_POST_LOAD_INITIAL_SAVE_INTENT_ALREADY_ACTIVE;
    repeatAtomic = EspMapResidentLifecycle_capture(&residentAfterRepeat) &&
                   memcmp(&residentAfter, &residentAfterRepeat,
                          sizeof(residentAfter)) == 0 &&
                   memcmp(&beforeRepeat, EspPostLoadInitialSaveIntent_view(),
                          sizeof(beforeRepeat)) == 0;

    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfter = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameAfter = framebufferHash();
    gameAfter = gameWitness(doomRpg->game);
    playerAfter = playerWitness(doomRpg->player);
    canvasAfter = canvasWitness(doomRpg->doomCanvas);
    renderAfter = renderWitness(doomRpg->render);
    hudAfter = hashBytes(doomRpg->hud, sizeof(*doomRpg->hud));
    weaponFNVAfter = hashBytes(EspPostLoadWeaponSelect_view(),
                               sizeof(EspPostLoadWeaponSelectState));
    giveMapFNVAfter =
        hashBytes(EspPostLoadGiveMap_view(), sizeof(EspPostLoadGiveMapState));
    hudOwnerAfter =
        hashBytes(EspHudPostLoadClear_view(), sizeof(EspHudPostLoadClearState));
    viewFNVAfter = hashBytes(EspPlayerView_view(), sizeof(EspPlayerViewState));
    facingFNVAfter =
        hashBytes(EspPlayerFacing_view(), sizeof(EspPlayerFacingState));

    if (!nullWeaponGate || !nullViewGate || !nullOutputGate ||
        !inactiveWeaponGate || !weaponMismatchGate || !inactiveViewGate ||
        !viewPendingGate || !loadedContextGate || !invalidLoadedGate ||
        !prepareAtomic || !postActivePrepareGate || !repeatGate ||
        !repeatAtomic || heapBefore != heapAfter || largestBefore != largestAfter ||
        frameBefore != frameAfter || gameBefore != gameAfter ||
        playerBefore != playerAfter || canvasBefore != canvasAfter ||
        renderBefore != renderAfter || hudBefore != hudAfter ||
        weaponFNVBefore != weaponFNVAfter || giveMapFNVBefore != giveMapFNVAfter ||
        hudOwnerBefore != hudOwnerAfter || viewFNVBefore != viewFNVAfter ||
        facingFNVBefore != facingFNVAfter || EspAssetPack_isOpen() ||
        !legacyRuntimeIsClear(doomRpg->render) || doomRpg->game->isLoaded != false ||
        doomRpg->doomCanvas->state != ST_INTRO ||
        doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0) {
        printf("[JUNCTIONSAVEINTENTPROBE] FAILED integrity after initial-save intent route\n");
        return;
    }

    legacyRoutePresent = doomRpg->game->newMapName[0] != '\0';

    printf("[JUNCTIONSAVEINTENT] READY stateBytes=%u stateFNV=%08x mapId=%u view=%ld/%ld angle=%ld isLoadedBefore=%u saveMode=%u saveRequired=%u componentMask=%02x persistenceDeferred=%u presentationDeferred=%u active=%u\n",
           (unsigned)sizeof(*liveState),
           (unsigned)hashBytes(liveState, sizeof(*liveState)),
           (unsigned)liveState->mapId, (long)liveState->viewX,
           (long)liveState->viewY, (long)liveState->viewAngle,
           (unsigned)liveState->isLoadedBefore, (unsigned)liveState->saveMode,
           (unsigned)liveState->saveRequired, (unsigned)liveState->componentMask,
           (unsigned)liveState->persistenceDeferred,
           (unsigned)liveState->presentationDeferred,
           (unsigned)liveState->active);
    printf("[JUNCTIONSAVEINTENT] SEMANTIC mapNotEnd=yes notLoaded=yes config=yes player2=yes world=yes playerRoute=yes saveFileWrite=no savingUi=no presentation=no\n");
    printf("[JUNCTIONSAVEINTENT] INPUT weaponFNV=%08x giveMapFNV=%08x hudClearFNV=%08x viewFNV=%08x facingFNV=%08x unchanged=yes callerOrder=yes callMap=9 callView=992/1888 angle=64 source=nativePlayerView legacyIsLoaded=%d\n",
           (unsigned)weaponFNVAfter, (unsigned)giveMapFNVAfter,
           (unsigned)hudOwnerAfter, (unsigned)viewFNVAfter,
           (unsigned)facingFNVAfter, doomRpg->game->isLoaded);
    printf("[JUNCTIONSAVEINTENT] FAILCLOSED nullWeapon=%d nullView=%d nullOutput=%d inactiveWeapon=%d weaponMismatch=%d inactiveView=%d viewPending=%d loadedContextDeferred=%d invalidLoaded=%d prepareAtomic=%s postActivePrepare=%d repeat=%d repeatAtomic=%s\n",
           nullWeaponGate, nullViewGate, nullOutputGate, inactiveWeaponGate,
           weaponMismatchGate, inactiveViewGate, viewPendingGate,
           loadedContextGate, invalidLoadedGate,
           prepareAtomic ? "yes" : "no", postActivePrepareGate, repeatGate,
           repeatAtomic ? "yes" : "no");
    printf("[JUNCTIONSAVEINTENT] RESIDENT snapshotFNV=%08x->%08x unchanged=yes mapFNV=%08x automapFNV=%08x runtimeFNV=%08x scriptFNV=%08x lineFNV=%08x textureFNV=%08x topologyFNV=%08x payload=%u entities=%u enemies=%u destructibles=%u packClosed=yes\n",
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
           (unsigned)residentAfter.entityCount, (unsigned)residentAfter.enemyCount,
           (unsigned)residentAfter.destructibleCount);
    printf("[JUNCTIONSAVEINTENT] RAM heap8=%u->%u delta=%ld largest8=%u->%u delta=%ld persistentHeapBytes=0\n",
           (unsigned)heapBefore, (unsigned)heapAfter,
           (long)((int32_t)heapAfter - (int32_t)heapBefore),
           (unsigned)largestBefore, (unsigned)largestAfter,
           (long)((int32_t)largestAfter - (int32_t)largestBefore));
    printf("[JUNCTIONSAVEINTENT] LEGACY gameFNV=%08x->%08x playerFNV=%08x->%08x hudFNV=%08x->%08x canvasFNV=%08x->%08x renderFNV=%08x->%08x frameFNV=%08x->%08x legacyRuntimeClear=yes GameMutation=no PlayerMutation=no HudMutation=no DoomCanvasMutation=no RenderMutation=no legacyGame_saveStateCalled=no\n",
           (unsigned)gameBefore, (unsigned)gameAfter,
           (unsigned)playerBefore, (unsigned)playerAfter,
           (unsigned)hudBefore, (unsigned)hudAfter,
           (unsigned)canvasBefore, (unsigned)canvasAfter,
           (unsigned)renderBefore, (unsigned)renderAfter,
           (unsigned)frameBefore, (unsigned)frameAfter);
    printf("[JUNCTIONSAVEINTENT] DEFERRED legacyNewMapPresent=%s legacyNewDest=%d/%d legacyNewAngle=%d routePayloadOwned=no playerPersistence=no worldPersistence=no configPersistence=no\n",
           legacyRoutePresent ? "yes" : "no", doomRpg->game->newDestX,
           doomRpg->game->newDestY, doomRpg->game->newAngle);
    printf("[JUNCTIONSAVEINTENT] PARK state=%d page=%d targetMap=%u junctionResident=yes nativeWeaponSelfSelect=yes nativeInitialSaveIntent=yes initialSaveDecisionPending=no initialSavePersistencePending=yes initialSavePending=yes postLoadCleanupPending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned)liveState->mapId);

    /* New probes use done strictly as successful-completion/PARK. */
    probeState.done = 1;
}
