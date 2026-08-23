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
#include "esp_post_load_weapon_select_state.h"
#include "native_junction_post_load_givemap_probe.h"
#include "native_junction_post_load_weapon_select_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_WEAPON_STATE_BYTES 8U
#define EXPECTED_GIVEMAP_STATE_BYTES 16U
#define EXPECTED_GIVEMAP_STATE_FNV 0x448e587dU
#define EXPECTED_HUD_CLEAR_BYTES 8U
#define EXPECTED_HUD_CLEAR_FNV 0xb7383e18U
#define EXPECTED_VIEW_BYTES 44U
#define EXPECTED_VIEW_FNV 0xafcdcf74U
#define EXPECTED_FACING_BYTES 32U
#define EXPECTED_FACING_FNV 0x95aa1108U

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

static int stateIsZero(const EspPostLoadWeaponSelectState* state) {
    EspPostLoadWeaponSelectState zero;
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
    return sizeof(EspPostLoadGiveMapState) == EXPECTED_GIVEMAP_STATE_BYTES &&
           sizeof(EspHudPostLoadClearState) == EXPECTED_HUD_CLEAR_BYTES &&
           sizeof(EspPlayerViewState) == EXPECTED_VIEW_BYTES &&
           sizeof(EspPlayerFacingState) == EXPECTED_FACING_BYTES &&
           EspPostLoadGiveMap_view() != NULL &&
           EspHudPostLoadClear_view() != NULL && EspPlayerView_view() != NULL &&
           EspPlayerFacing_view() != NULL &&
           hashBytes(EspPostLoadGiveMap_view(), sizeof(EspPostLoadGiveMapState)) ==
               EXPECTED_GIVEMAP_STATE_FNV &&
           hashBytes(EspHudPostLoadClear_view(), sizeof(EspHudPostLoadClearState)) ==
               EXPECTED_HUD_CLEAR_FNV &&
           hashBytes(EspPlayerView_view(), sizeof(EspPlayerViewState)) ==
               EXPECTED_VIEW_FNV &&
           hashBytes(EspPlayerFacing_view(), sizeof(EspPlayerFacingState)) ==
               EXPECTED_FACING_FNV;
}

void Esp32JunctionPostLoadWeaponSelectProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspPostLoadWeaponSelect_reset();
}

int Esp32JunctionPostLoadWeaponSelectProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionPostLoadWeaponSelectProbe_service(struct DoomRPG_s* doomRpg) {
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentPrepared;
    EspMapResidentSnapshot residentAfter;
    EspMapResidentSnapshot residentAfterRepeat;
    EspPostLoadGiveMapState giveMapBefore;
    EspPostLoadGiveMapState badGiveMap;
    EspPostLoadWeaponSelectState scratch;
    EspPostLoadWeaponSelectState prepared;
    EspPostLoadWeaponSelectState beforeRepeat;
    const EspPostLoadWeaponSelectState* liveState;
    EspPostLoadWeaponSelectStatus status;
    uint32_t heapBefore, heapAfter, largestBefore, largestAfter;
    uint32_t frameBefore, frameAfter, gameBefore, gameAfter;
    uint32_t playerBefore, playerAfter, canvasBefore, canvasAfter;
    uint32_t renderBefore, renderAfter, hudBefore, hudAfter;
    uint32_t giveMapFNVBefore, giveMapFNVAfter;
    uint32_t hudOwnerBefore, hudOwnerAfter, viewBefore, viewAfter;
    uint32_t facingBefore, facingAfter;
    int weaponBefore, weaponAfter, updateViewBefore, updateViewAfter;
    int nullGiveMapGate, nullOutputGate, inactiveGiveMapGate;
    int targetMapGate, gameplayMapGate, loadTypeGate, countGate;
    int invalidWeaponGate, prepareAtomic, postActivePrepareGate;
    int repeatGate, repeatAtomic;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionPostLoadGiveMapProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[JUNCTIONWEAPONPROBE] ARMED hardware-proven direct Junction Game_givemap complete; post-load weapon self-select starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native Junction post-load weapon self-select ===\n");
    printf("[JUNCTIONWEAPONPROBE] CONTRACT recover only Player_selectWeapon(player, player->weapon) after hardware-proven Junction Game_givemap: requested weapon equals current weapon, so legacy DoomCanvas_updateViewTrue branch is not taken and the weapon assignment is identity; park one 8B pointer-free caller-order marker; do not call legacy Player_selectWeapon, do not mutate Player/DoomCanvas/Hud/Game/Render/native world, keep initial save/load cleanup/ST_PLAYING deferred, do not present and do not allocate\n");

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL || doomRpg->game == NULL ||
        doomRpg->render == NULL || doomRpg->player == NULL || doomRpg->hud == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO || doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        EspAssetPack_isOpen() || !legacyRuntimeIsClear(doomRpg->render) ||
        sizeof(EspPostLoadWeaponSelectState) != EXPECTED_WEAPON_STATE_BYTES ||
        !precedingOwnersCanonical() ||
        !EspMapResidentLifecycle_capture(&residentBefore) ||
        !residentCanonical(&residentBefore) || doomRpg->player->weapon < 0 ||
        doomRpg->player->weapon > 11) {
        printf("[JUNCTIONWEAPONPROBE] FAILED unsafe post-GIVEMAP boundary weapon=%d\n",
               doomRpg != NULL && doomRpg->player != NULL ? doomRpg->player->weapon : -1);
        probeState.done = 1;
        return;
    }

    giveMapBefore = *EspPostLoadGiveMap_view();
    giveMapFNVBefore = hashBytes(&giveMapBefore, sizeof(giveMapBefore));
    hudOwnerBefore = hashBytes(EspHudPostLoadClear_view(), sizeof(EspHudPostLoadClearState));
    viewBefore = hashBytes(EspPlayerView_view(), sizeof(EspPlayerViewState));
    facingBefore = hashBytes(EspPlayerFacing_view(), sizeof(EspPlayerFacingState));

    weaponBefore = doomRpg->player->weapon;
    updateViewBefore = doomRpg->doomCanvas->isUpdateView;
    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    gameBefore = gameWitness(doomRpg->game);
    playerBefore = playerWitness(doomRpg->player);
    canvasBefore = canvasWitness(doomRpg->doomCanvas);
    renderBefore = renderWitness(doomRpg->render);
    hudBefore = hashBytes(doomRpg->hud, sizeof(*doomRpg->hud));

    memset(&scratch, 0xa5, sizeof(scratch));
    nullGiveMapGate =
        EspPostLoadWeaponSelect_prepare(NULL, (uint8_t)weaponBefore, &scratch) ==
            ESP_POST_LOAD_WEAPON_SELECT_INVALID &&
        stateIsZero(&scratch);
    nullOutputGate =
        EspPostLoadWeaponSelect_prepare(&giveMapBefore, (uint8_t)weaponBefore, NULL) ==
        ESP_POST_LOAD_WEAPON_SELECT_INVALID;

    badGiveMap = giveMapBefore;
    badGiveMap.active = 0U;
    memset(&scratch, 0xa5, sizeof(scratch));
    inactiveGiveMapGate =
        EspPostLoadWeaponSelect_prepare(&badGiveMap, (uint8_t)weaponBefore, &scratch) ==
            ESP_POST_LOAD_WEAPON_SELECT_GIVEMAP_INVALID &&
        stateIsZero(&scratch);

    badGiveMap = giveMapBefore;
    badGiveMap.targetMapId = 1U;
    memset(&scratch, 0xa5, sizeof(scratch));
    targetMapGate =
        EspPostLoadWeaponSelect_prepare(&badGiveMap, (uint8_t)weaponBefore, &scratch) ==
            ESP_POST_LOAD_WEAPON_SELECT_UNSUPPORTED_CONTEXT &&
        stateIsZero(&scratch);

    badGiveMap = giveMapBefore;
    badGiveMap.gameplayLoadMapId = 1U;
    memset(&scratch, 0xa5, sizeof(scratch));
    gameplayMapGate =
        EspPostLoadWeaponSelect_prepare(&badGiveMap, (uint8_t)weaponBefore, &scratch) ==
            ESP_POST_LOAD_WEAPON_SELECT_UNSUPPORTED_CONTEXT &&
        stateIsZero(&scratch);

    badGiveMap = giveMapBefore;
    badGiveMap.loadType = 1U;
    memset(&scratch, 0xa5, sizeof(scratch));
    loadTypeGate =
        EspPostLoadWeaponSelect_prepare(&badGiveMap, (uint8_t)weaponBefore, &scratch) ==
            ESP_POST_LOAD_WEAPON_SELECT_UNSUPPORTED_CONTEXT &&
        stateIsZero(&scratch);

    badGiveMap = giveMapBefore;
    ++badGiveMap.lineTargetCount;
    memset(&scratch, 0xa5, sizeof(scratch));
    countGate =
        EspPostLoadWeaponSelect_prepare(&badGiveMap, (uint8_t)weaponBefore, &scratch) ==
            ESP_POST_LOAD_WEAPON_SELECT_UNSUPPORTED_CONTEXT &&
        stateIsZero(&scratch);

    memset(&scratch, 0xa5, sizeof(scratch));
    invalidWeaponGate =
        EspPostLoadWeaponSelect_prepare(&giveMapBefore, 12U, &scratch) ==
            ESP_POST_LOAD_WEAPON_SELECT_WEAPON_INVALID &&
        stateIsZero(&scratch);

    memset(&prepared, 0, sizeof(prepared));
    status = EspPostLoadWeaponSelect_prepare(
        &giveMapBefore, (uint8_t)weaponBefore, &prepared);
    if (status != ESP_POST_LOAD_WEAPON_SELECT_OK || prepared.active != 1U ||
        prepared.weaponBefore != (uint8_t)weaponBefore ||
        prepared.requestedWeapon != (uint8_t)weaponBefore ||
        prepared.weaponAfter != (uint8_t)weaponBefore ||
        prepared.viewInvalidationRequested != 0U || prepared.targetMapId != 9U ||
        prepared.gameplayLoadMapId != 2U || prepared.loadType != 0U ||
        EspPostLoadWeaponSelect_isReady() ||
        !EspMapResidentLifecycle_capture(&residentPrepared) ||
        memcmp(&residentBefore, &residentPrepared, sizeof(residentBefore)) != 0 ||
        !precedingOwnersCanonical() || EspAssetPack_isOpen()) {
        printf("[JUNCTIONWEAPONPROBE] FAILED pure weapon self-select preparation status=%u\n",
               (unsigned)status);
        probeState.done = 1;
        return;
    }
    prepareAtomic = 1;

    status = EspPostLoadWeaponSelect_route((uint8_t)weaponBefore);
    if (status != ESP_POST_LOAD_WEAPON_SELECT_OK ||
        !EspPostLoadWeaponSelect_isReady() ||
        EspPostLoadWeaponSelect_view() == NULL ||
        memcmp(&prepared, EspPostLoadWeaponSelect_view(), sizeof(prepared)) != 0 ||
        !precedingOwnersCanonical() || EspAssetPack_isOpen() ||
        !EspMapResidentLifecycle_capture(&residentAfter) ||
        memcmp(&residentBefore, &residentAfter, sizeof(residentBefore)) != 0) {
        printf("[JUNCTIONWEAPONPROBE] FAILED live weapon self-select route status=%u\n",
               (unsigned)status);
        probeState.done = 1;
        return;
    }

    liveState = EspPostLoadWeaponSelect_view();
    postActivePrepareGate =
        EspPostLoadWeaponSelect_prepare(&giveMapBefore, (uint8_t)weaponBefore, &scratch) ==
        ESP_POST_LOAD_WEAPON_SELECT_UNSUPPORTED_ORDER;
    beforeRepeat = *liveState;
    repeatGate =
        EspPostLoadWeaponSelect_route((uint8_t)weaponBefore) ==
        ESP_POST_LOAD_WEAPON_SELECT_ALREADY_ACTIVE;
    repeatAtomic = EspMapResidentLifecycle_capture(&residentAfterRepeat) &&
                   memcmp(&residentAfter, &residentAfterRepeat,
                          sizeof(residentAfter)) == 0 &&
                   memcmp(&beforeRepeat, EspPostLoadWeaponSelect_view(),
                          sizeof(beforeRepeat)) == 0;

    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfter = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameAfter = framebufferHash();
    gameAfter = gameWitness(doomRpg->game);
    playerAfter = playerWitness(doomRpg->player);
    canvasAfter = canvasWitness(doomRpg->doomCanvas);
    renderAfter = renderWitness(doomRpg->render);
    hudAfter = hashBytes(doomRpg->hud, sizeof(*doomRpg->hud));
    giveMapFNVAfter = hashBytes(EspPostLoadGiveMap_view(), sizeof(EspPostLoadGiveMapState));
    hudOwnerAfter = hashBytes(EspHudPostLoadClear_view(), sizeof(EspHudPostLoadClearState));
    viewAfter = hashBytes(EspPlayerView_view(), sizeof(EspPlayerViewState));
    facingAfter = hashBytes(EspPlayerFacing_view(), sizeof(EspPlayerFacingState));
    weaponAfter = doomRpg->player->weapon;
    updateViewAfter = doomRpg->doomCanvas->isUpdateView;

    if (!nullGiveMapGate || !nullOutputGate || !inactiveGiveMapGate ||
        !targetMapGate || !gameplayMapGate || !loadTypeGate || !countGate ||
        !invalidWeaponGate || !prepareAtomic || !postActivePrepareGate ||
        !repeatGate || !repeatAtomic || heapBefore != heapAfter ||
        largestBefore != largestAfter || frameBefore != frameAfter ||
        gameBefore != gameAfter || playerBefore != playerAfter ||
        canvasBefore != canvasAfter || renderBefore != renderAfter ||
        hudBefore != hudAfter || giveMapFNVBefore != giveMapFNVAfter ||
        hudOwnerBefore != hudOwnerAfter || viewBefore != viewAfter ||
        facingBefore != facingAfter || weaponBefore != weaponAfter ||
        updateViewBefore != updateViewAfter || EspAssetPack_isOpen() ||
        !legacyRuntimeIsClear(doomRpg->render) ||
        doomRpg->doomCanvas->state != ST_INTRO ||
        doomRpg->doomCanvas->storyPage != 3 || doomRpg->game->numEntities != 0 ||
        doomRpg->game->numMonsters != 0) {
        printf("[JUNCTIONWEAPONPROBE] FAILED integrity after weapon self-select route\n");
        probeState.done = 1;
        return;
    }

    printf("[JUNCTIONWEAPON] READY stateBytes=%u stateFNV=%08x weaponBefore=%u requestedWeapon=%u weaponAfter=%u viewInvalidationRequested=%u active=%u targetMap=%u gameplayLoadMapId=%u loadType=%u\n",
           (unsigned)sizeof(*liveState),
           (unsigned)hashBytes(liveState, sizeof(*liveState)),
           (unsigned)liveState->weaponBefore,
           (unsigned)liveState->requestedWeapon,
           (unsigned)liveState->weaponAfter,
           (unsigned)liveState->viewInvalidationRequested,
           (unsigned)liveState->active, (unsigned)liveState->targetMapId,
           (unsigned)liveState->gameplayLoadMapId, (unsigned)liveState->loadType);
    printf("[JUNCTIONWEAPON] SEMANTIC selfSelect=yes identityAssignment=yes updateViewBranchTaken=no legacyWeapon=%d->%d legacyIsUpdateView=%d->%d\n",
           weaponBefore, weaponAfter, updateViewBefore, updateViewAfter);
    printf("[JUNCTIONWEAPON] INPUT giveMapFNV=%08x hudClearFNV=%08x viewFNV=%08x facingFNV=%08x unchanged=yes callerOrder=yes\n",
           (unsigned)giveMapFNVAfter, (unsigned)hudOwnerAfter,
           (unsigned)viewAfter, (unsigned)facingAfter);
    printf("[JUNCTIONWEAPON] FAILCLOSED nullGiveMap=%d nullOutput=%d inactiveGiveMap=%d targetMap=%d gameplayMap=%d loadType=%d count=%d invalidWeapon=%d prepareAtomic=%s postActivePrepare=%d repeat=%d repeatAtomic=%s\n",
           nullGiveMapGate, nullOutputGate, inactiveGiveMapGate, targetMapGate,
           gameplayMapGate, loadTypeGate, countGate, invalidWeaponGate,
           prepareAtomic ? "yes" : "no", postActivePrepareGate, repeatGate,
           repeatAtomic ? "yes" : "no");
    printf("[JUNCTIONWEAPON] RESIDENT snapshotFNV=%08x->%08x unchanged=yes mapFNV=%08x automapFNV=%08x runtimeFNV=%08x scriptFNV=%08x lineFNV=%08x textureFNV=%08x topologyFNV=%08x payload=%u entities=%u enemies=%u destructibles=%u packClosed=yes\n",
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
    printf("[JUNCTIONWEAPON] RAM heap8=%u->%u delta=%ld largest8=%u->%u delta=%ld persistentHeapBytes=0\n",
           (unsigned)heapBefore, (unsigned)heapAfter,
           (long)((int32_t)heapAfter - (int32_t)heapBefore),
           (unsigned)largestBefore, (unsigned)largestAfter,
           (long)((int32_t)largestAfter - (int32_t)largestBefore));
    printf("[JUNCTIONWEAPON] LEGACY gameFNV=%08x->%08x playerFNV=%08x->%08x hudFNV=%08x->%08x canvasFNV=%08x->%08x renderFNV=%08x->%08x frameFNV=%08x->%08x legacyRuntimeClear=yes GameMutation=no PlayerMutation=no HudMutation=no DoomCanvasMutation=no RenderMutation=no legacyPlayer_selectWeaponCalled=no\n",
           (unsigned)gameBefore, (unsigned)gameAfter,
           (unsigned)playerBefore, (unsigned)playerAfter,
           (unsigned)hudBefore, (unsigned)hudAfter,
           (unsigned)canvasBefore, (unsigned)canvasAfter,
           (unsigned)renderBefore, (unsigned)renderAfter,
           (unsigned)frameBefore, (unsigned)frameAfter);
    printf("[JUNCTIONWEAPON] PARK state=%d page=%d targetMap=%u junctionResident=yes nativeHudClear=yes nativePostLoadGiveMap=yes nativeWeaponSelfSelect=yes weaponReselectPending=no initialSavePending=yes postLoadCleanupPending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned)liveState->targetMapId);

    probeState.done = 1;
}
