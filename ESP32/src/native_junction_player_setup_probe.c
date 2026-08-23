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
#include "esp_hud_refresh_state.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_player_fresh_map_state.h"
#include "esp_player_view_state.h"
#include "native_junction_hud_refresh_probe.h"
#include "native_junction_player_setup_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_SETUP_STATE_BYTES 24U
#define EXPECTED_SETUP_SEMANTIC_FNV 0x3b27c6a1U
#define EXPECTED_VIEW_STATE_BYTES 44U
#define EXPECTED_VIEW_BEFORE_FNV 0xd17fa0d1U
#define EXPECTED_VIEW_AFTER_FNV 0xc21fba3cU
#define EXPECTED_HUD_STATE_BYTES 8U
#define EXPECTED_HUD_STATE_FNV 0x6965ee06U
#define EXPECTED_TARGET_SNAPSHOT_FNV 0xbc9071e9U

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

static uint32_t setupSemanticFNV(const EspPlayerFreshMapState* state) {
    EspPlayerFreshMapState normalized;

    if (state == NULL) return 0U;
    normalized = *state;
    normalized.levelStartTimeMs = 0U;
    return hashBytes(&normalized, sizeof(normalized));
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

static uint32_t placementWitness(const DoomRPG_t* doomRpg) {
    uint32_t values[16];
    const DoomCanvas_t* canvas;
    const Game_t* game;
    const Render_t* render;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->game == NULL || doomRpg->render == NULL ||
        doomRpg->hud == NULL) return 0U;

    canvas = doomRpg->doomCanvas;
    game = doomRpg->game;
    render = doomRpg->render;
    values[0] = (uint32_t)game->spawnParam;
    values[1] = (uint32_t)game->isLoaded;
    values[2] = (uint32_t)canvas->viewX;
    values[3] = (uint32_t)canvas->viewY;
    values[4] = (uint32_t)canvas->viewZ;
    values[5] = (uint32_t)canvas->viewAngle;
    values[6] = (uint32_t)canvas->destX;
    values[7] = (uint32_t)canvas->destY;
    values[8] = (uint32_t)canvas->destAngle;
    values[9] = (uint32_t)(uint16_t)canvas->loadMapID;
    values[10] = (uint32_t)canvas->loadType;
    values[11] = (uint32_t)canvas->state;
    values[12] = (uint32_t)canvas->storyPage;
    values[13] = (uint32_t)render->viewZOld;
    values[14] = (uint32_t)doomRpg->hud->isUpdate;
    values[15] = (uint32_t)game->activeLoadType;
    return hashBytes(values, sizeof(values));
}

static uint32_t playerSetupWitness(const Player_t* player) {
    uint32_t values[16];
    uint32_t hash;
    uint32_t i;

    if (player == NULL) return 0U;
    values[0] = (uint32_t)player->time;
    values[1] = (uint32_t)player->moves;
    values[2] = (uint32_t)player->xpGained;
    values[3] = (uint32_t)player->berserkerTics;
    values[4] = (uint32_t)(player->dogFamiliar != NULL);
    values[5] = (uint32_t)player->disabledWeapons;
    values[6] = (uint32_t)player->weapons;
    values[7] = (uint32_t)player->weapon;
    values[8] = (uint32_t)player->totalTime;
    values[9] = (uint32_t)player->totalMoves;
    values[10] = (uint32_t)player->currentXP;
    values[11] = (uint32_t)player->level;
    values[12] = (uint32_t)player->keys;
    values[13] = (uint32_t)player->credits;
    values[14] = (uint32_t)player->completedLevels;
    values[15] = (uint32_t)player->disabledWeapons;

    hash = hashBytes(values, sizeof(values));
    for (i = 0U; i < (uint32_t)(sizeof(player->ammo) / sizeof(player->ammo[0])); ++i) {
        hash ^= (uint32_t)player->ammo[i];
        hash *= 16777619U;
    }
    for (i = 0U; i < (uint32_t)sizeof(player->NotebookString); ++i) {
        hash ^= (uint8_t)player->NotebookString[i];
        hash *= 16777619U;
    }
    return hash;
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

static int snapshotCanonical(const EspMapResidentSnapshot* snapshot) {
    return snapshot != NULL && sizeof(*snapshot) == 96U &&
           snapshot->totalPayloadBytes == 10410U &&
           snapshot->runtimeArenaBytes == 8867U &&
           snapshot->mapStateBytes == 1024U &&
           snapshot->scriptStateBytes == 73U &&
           snapshot->lineStateBytes == 52U &&
           snapshot->textureStateBytes == 26U &&
           snapshot->automapStateBytes == 32U &&
           snapshot->topologyBytes == 336U &&
           snapshot->runtimeFNV1a == 0xbc432a0fU &&
           snapshot->mapStateFNV1a == 0xc5cdfc04U &&
           snapshot->scriptStateFNV1a == 0xbc9b18ffU &&
           snapshot->lineStateFNV1a == 0x3658710dU &&
           snapshot->textureStateFNV1a == 0x537319adU &&
           snapshot->automapStateFNV1a == 0x0b2ae445U &&
           snapshot->topologyFNV1a == 0xd6e8df7dU &&
           snapshot->entityCount == 30U && snapshot->enemyCount == 0U &&
           snapshot->destructibleCount == 3U &&
           hashBytes(snapshot, sizeof(*snapshot)) == EXPECTED_TARGET_SNAPSHOT_FNV;
}

static int viewBeforeCanonical(const EspPlayerViewState* view) {
    return view != NULL && sizeof(*view) == EXPECTED_VIEW_STATE_BYTES &&
           view->viewX == 992 && view->viewY == 1888 && view->viewZ == 36 &&
           view->viewAngle == 64 && view->destX == 992 &&
           view->destY == 1888 && view->destAngle == 64 &&
           view->viewZOld == 4 && view->targetMapId == 9U &&
           view->gameplayLoadMapId == 2U && view->loadType == 0U &&
           view->spawnApplied == 1U && view->hudRefreshPending == 0U &&
           view->facingRefreshPending == 1U &&
           view->playerSetupPending == 1U && view->tileEnterPending == 1U &&
           view->active == 1U &&
           hashBytes(view, sizeof(*view)) == EXPECTED_VIEW_BEFORE_FNV;
}

static int viewAfterCanonical(const EspPlayerViewState* view) {
    return view != NULL && sizeof(*view) == EXPECTED_VIEW_STATE_BYTES &&
           view->viewX == 992 && view->viewY == 1888 && view->viewZ == 36 &&
           view->viewAngle == 64 && view->destX == 992 &&
           view->destY == 1888 && view->destAngle == 64 &&
           view->viewZOld == 4 && view->targetMapId == 9U &&
           view->gameplayLoadMapId == 2U && view->loadType == 0U &&
           view->spawnApplied == 1U && view->hudRefreshPending == 0U &&
           view->facingRefreshPending == 1U &&
           view->playerSetupPending == 0U && view->tileEnterPending == 1U &&
           view->active == 1U &&
           hashBytes(view, sizeof(*view)) == EXPECTED_VIEW_AFTER_FNV;
}

static int hudCanonical(const EspHudRefreshState* hud) {
    return hud != NULL && sizeof(*hud) == EXPECTED_HUD_STATE_BYTES &&
           hud->reason == ESP_HUD_REFRESH_REASON_POST_SPAWN &&
           hud->refreshPending == 1U && hud->routed == 1U && hud->active == 1U &&
           hud->targetMapId == 9U && hud->gameplayLoadMapId == 2U &&
           hud->loadType == 0U && hud->reserved == 0U &&
           hashBytes(hud, sizeof(*hud)) == EXPECTED_HUD_STATE_FNV;
}

static int setupCanonical(const EspPlayerFreshMapState* state,
                          uint32_t sampledNowMs) {
    return state != NULL && sizeof(*state) == EXPECTED_SETUP_STATE_BYTES &&
           state->levelStartTimeMs == sampledNowMs && state->moves == 0U &&
           state->xpGained == 0U && state->berserkerTics == 0U &&
           state->familiarActive == 0U && state->notebookEmpty == 1U &&
           state->weaponRestorePerformed == 0U && state->targetMapId == 9U &&
           state->gameplayLoadMapId == 2U && state->loadType == 0U &&
           state->setupApplied == 1U && state->active == 1U &&
           setupSemanticFNV(state) == EXPECTED_SETUP_SEMANTIC_FNV;
}

static int setupStateZero(const EspPlayerFreshMapState* state) {
    EspPlayerFreshMapState zero;
    if (state == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(state, &zero, sizeof(zero)) == 0;
}

void Esp32JunctionPlayerSetupProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspPlayerFreshMap_reset();
}

int Esp32JunctionPlayerSetupProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionPlayerSetupProbe_service(struct DoomRPG_s* doomRpg) {
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentAfter;
    EspPlayerViewState viewBefore;
    EspPlayerViewState badView;
    EspHudRefreshState hudBefore;
    EspHudRefreshState badHud;
    EspPlayerFreshMapState scratch;
    EspPlayerFreshMapState prepared;
    EspPlayerFreshMapState setupBeforeRepeat;
    EspPlayerViewState viewBeforeRepeat;
    const EspPlayerViewState* liveView;
    const EspHudRefreshState* liveHud;
    const EspPlayerFreshMapState* liveSetup;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t frameBefore;
    uint32_t frameAfter;
    uint32_t placementBefore;
    uint32_t placementAfter;
    uint32_t playerBefore;
    uint32_t playerAfter;
    uint32_t sampledNowMs;
    uint32_t setupFNV;
    uint32_t setupSemantic;
    uint32_t disabledWeapons;
    int resetProof;
    int nullViewGate;
    int nullHudGate;
    int nullOutputGate;
    int inactiveGate;
    int loadTypeGate;
    int hudPendingGate;
    int missingFacingGate;
    int missingSetupGate;
    int missingTileGate;
    int hudMismatchGate;
    int weaponRestoreGate;
    int prepareAtomic;
    int repeatGate;
    int repeatAtomic;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionHudRefreshProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[JUNCTIONSETUPPROBE] ARMED hardware-proven HUD routing active; native fresh-map Player_setup starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native Junction fresh-map Player_setup ===\n");
    printf("[JUNCTIONSETUPPROBE] CONTRACT own recovered fresh-map Player_setup fields in one 24B pointer-free native session state: sample level start time, reset moves/xpGained/berserker, clear familiar, represent empty notebook, consume only playerSetupPending; disabledWeapons!=0 remains fail-closed because weapon restore/view refresh is deferred; no legacy Player/Game/Hud/DoomCanvas/Render mutation, no tile-enter, no finishRotation/final facing, no ST_PLAYING and no allocation\n");

    liveView = EspPlayerView_view();
    liveHud = EspHudRefresh_view();
    if (doomRpg == NULL || doomRpg->doomCanvas == NULL || doomRpg->game == NULL ||
        doomRpg->render == NULL || doomRpg->player == NULL || doomRpg->hud == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO || doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        !legacyRuntimeIsClear(doomRpg->render) || EspAssetPack_isOpen() ||
        sizeof(EspPlayerFreshMapState) != EXPECTED_SETUP_STATE_BYTES ||
        sizeof(EspPlayerViewState) != EXPECTED_VIEW_STATE_BYTES ||
        sizeof(EspHudRefreshState) != EXPECTED_HUD_STATE_BYTES ||
        !viewBeforeCanonical(liveView) || !hudCanonical(liveHud) ||
        !EspMapResidentLifecycle_capture(&residentBefore) ||
        !snapshotCanonical(&residentBefore)) {
        printf("[JUNCTIONSETUPPROBE] FAILED unsafe post-HUD player boundary\n");
        probeState.done = 1;
        return;
    }

    disabledWeapons = (uint32_t)doomRpg->player->disabledWeapons;
    if (disabledWeapons != 0U) {
        printf("[JUNCTIONSETUPPROBE] FAILED real fresh path requires disabledWeapons=0 observed=%u\n",
               (unsigned)disabledWeapons);
        probeState.done = 1;
        return;
    }

    viewBefore = *liveView;
    hudBefore = *liveHud;
    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    placementBefore = placementWitness(doomRpg);
    playerBefore = playerSetupWitness(doomRpg->player);

    EspPlayerFreshMap_reset();
    resetProof = !EspPlayerFreshMap_isReady() && EspPlayerFreshMap_view() == NULL;

    memset(&scratch, 0xa5, sizeof(scratch));
    nullViewGate =
        EspPlayerFreshMap_prepare(NULL, &hudBefore, 123U, 0U, &scratch) ==
            ESP_PLAYER_FRESH_MAP_INVALID &&
        setupStateZero(&scratch);

    memset(&scratch, 0xa5, sizeof(scratch));
    nullHudGate =
        EspPlayerFreshMap_prepare(&viewBefore, NULL, 123U, 0U, &scratch) ==
            ESP_PLAYER_FRESH_MAP_INVALID &&
        setupStateZero(&scratch);

    nullOutputGate =
        EspPlayerFreshMap_prepare(&viewBefore, &hudBefore, 123U, 0U, NULL) ==
            ESP_PLAYER_FRESH_MAP_INVALID;

    badView = viewBefore;
    badView.active = 0U;
    memset(&scratch, 0xa5, sizeof(scratch));
    inactiveGate =
        EspPlayerFreshMap_prepare(&badView, &hudBefore, 123U, 0U, &scratch) ==
            ESP_PLAYER_FRESH_MAP_VIEW_INVALID &&
        setupStateZero(&scratch);

    badView = viewBefore;
    badView.loadType = 1U;
    memset(&scratch, 0xa5, sizeof(scratch));
    loadTypeGate =
        EspPlayerFreshMap_prepare(&badView, &hudBefore, 123U, 0U, &scratch) ==
            ESP_PLAYER_FRESH_MAP_UNSUPPORTED_CONTEXT &&
        setupStateZero(&scratch);

    badView = viewBefore;
    badView.hudRefreshPending = 1U;
    memset(&scratch, 0xa5, sizeof(scratch));
    hudPendingGate =
        EspPlayerFreshMap_prepare(&badView, &hudBefore, 123U, 0U, &scratch) ==
            ESP_PLAYER_FRESH_MAP_UNSUPPORTED_ORDER &&
        setupStateZero(&scratch);

    badView = viewBefore;
    badView.facingRefreshPending = 0U;
    memset(&scratch, 0xa5, sizeof(scratch));
    missingFacingGate =
        EspPlayerFreshMap_prepare(&badView, &hudBefore, 123U, 0U, &scratch) ==
            ESP_PLAYER_FRESH_MAP_UNSUPPORTED_ORDER &&
        setupStateZero(&scratch);

    badView = viewBefore;
    badView.playerSetupPending = 0U;
    memset(&scratch, 0xa5, sizeof(scratch));
    missingSetupGate =
        EspPlayerFreshMap_prepare(&badView, &hudBefore, 123U, 0U, &scratch) ==
            ESP_PLAYER_FRESH_MAP_UNSUPPORTED_ORDER &&
        setupStateZero(&scratch);

    badView = viewBefore;
    badView.tileEnterPending = 0U;
    memset(&scratch, 0xa5, sizeof(scratch));
    missingTileGate =
        EspPlayerFreshMap_prepare(&badView, &hudBefore, 123U, 0U, &scratch) ==
            ESP_PLAYER_FRESH_MAP_UNSUPPORTED_ORDER &&
        setupStateZero(&scratch);

    badHud = hudBefore;
    badHud.targetMapId = 1U;
    memset(&scratch, 0xa5, sizeof(scratch));
    hudMismatchGate =
        EspPlayerFreshMap_prepare(&viewBefore, &badHud, 123U, 0U, &scratch) ==
            ESP_PLAYER_FRESH_MAP_HUD_INVALID &&
        setupStateZero(&scratch);

    memset(&scratch, 0xa5, sizeof(scratch));
    weaponRestoreGate =
        EspPlayerFreshMap_prepare(&viewBefore, &hudBefore, 123U, 1U, &scratch) ==
            ESP_PLAYER_FRESH_MAP_WEAPON_RESTORE_DEFERRED &&
        setupStateZero(&scratch);

    if (EspPlayerFreshMap_prepare(&viewBefore, &hudBefore, 123U, 0U,
                                  &prepared) != ESP_PLAYER_FRESH_MAP_OK ||
        !setupCanonical(&prepared, 123U) ||
        !viewBeforeCanonical(EspPlayerView_view()) ||
        !hudCanonical(EspHudRefresh_view())) {
        printf("[JUNCTIONSETUPPROBE] FAILED pure Player_setup preparation\n");
        probeState.done = 1;
        return;
    }
    prepareAtomic =
        memcmp(&viewBefore, EspPlayerView_view(), sizeof(viewBefore)) == 0 &&
        memcmp(&hudBefore, EspHudRefresh_view(), sizeof(hudBefore)) == 0;

    sampledNowMs = (uint32_t)DoomRPG_GetUpTimeMS();
    if (EspPlayerFreshMap_route(sampledNowMs, disabledWeapons) !=
            ESP_PLAYER_FRESH_MAP_OK ||
        !setupCanonical(EspPlayerFreshMap_view(), sampledNowMs) ||
        !viewAfterCanonical(EspPlayerView_view()) ||
        !hudCanonical(EspHudRefresh_view())) {
        printf("[JUNCTIONSETUPPROBE] FAILED live Player_setup routing\n");
        probeState.done = 1;
        return;
    }

    liveSetup = EspPlayerFreshMap_view();
    setupFNV = hashBytes(liveSetup, sizeof(*liveSetup));
    setupSemantic = setupSemanticFNV(liveSetup);
    setupBeforeRepeat = *liveSetup;
    viewBeforeRepeat = *EspPlayerView_view();
    repeatGate =
        EspPlayerFreshMap_route(sampledNowMs + 1U, disabledWeapons) ==
            ESP_PLAYER_FRESH_MAP_ALREADY_ACTIVE;
    repeatAtomic = EspPlayerFreshMap_view() != NULL &&
                   EspPlayerView_view() != NULL &&
                   memcmp(&setupBeforeRepeat, EspPlayerFreshMap_view(),
                          sizeof(setupBeforeRepeat)) == 0 &&
                   memcmp(&viewBeforeRepeat, EspPlayerView_view(),
                          sizeof(viewBeforeRepeat)) == 0;

    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfter = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameAfter = framebufferHash();
    placementAfter = placementWitness(doomRpg);
    playerAfter = playerSetupWitness(doomRpg->player);

    if (!EspMapResidentLifecycle_capture(&residentAfter) ||
        !snapshotCanonical(&residentAfter) ||
        memcmp(&residentBefore, &residentAfter, sizeof(residentBefore)) != 0 ||
        heapBefore != heapAfter || largestBefore != largestAfter ||
        frameBefore != frameAfter || placementBefore != placementAfter ||
        playerBefore != playerAfter || EspAssetPack_isOpen() ||
        !hudCanonical(EspHudRefresh_view()) ||
        !viewAfterCanonical(EspPlayerView_view()) ||
        !setupCanonical(EspPlayerFreshMap_view(), sampledNowMs)) {
        printf("[JUNCTIONSETUPPROBE] FAILED integrity after Player_setup routing\n");
        probeState.done = 1;
        return;
    }

    liveSetup = EspPlayerFreshMap_view();
    liveView = EspPlayerView_view();
    printf("[JUNCTIONSETUP] READY stateBytes=%u stateFNV=%08x semanticFNV=%08x startMs=%u startExact=yes moves=%u xpGained=%u berserker=%u familiar=%u notebookEmpty=%u weaponRestore=%u targetMap=%u gameplayLoadMapId=%u loadType=%u active=%u setupApplied=%u\n",
           (unsigned)sizeof(*liveSetup), (unsigned)setupFNV,
           (unsigned)setupSemantic, (unsigned)liveSetup->levelStartTimeMs,
           (unsigned)liveSetup->moves, (unsigned)liveSetup->xpGained,
           (unsigned)liveSetup->berserkerTics,
           (unsigned)liveSetup->familiarActive,
           (unsigned)liveSetup->notebookEmpty,
           (unsigned)liveSetup->weaponRestorePerformed,
           (unsigned)liveSetup->targetMapId,
           (unsigned)liveSetup->gameplayLoadMapId,
           (unsigned)liveSetup->loadType, (unsigned)liveSetup->active,
           (unsigned)liveSetup->setupApplied);
    printf("[JUNCTIONSETUP] PLAYER viewBytes=%u beforeFNV=%08x afterFNV=%08x hudPending=%u facingPending=%u playerSetupPending=%u tileEnterPending=%u placementExact=yes\n",
           (unsigned)sizeof(*liveView), (unsigned)EXPECTED_VIEW_BEFORE_FNV,
           (unsigned)hashBytes(liveView, sizeof(*liveView)),
           (unsigned)liveView->hudRefreshPending,
           (unsigned)liveView->facingRefreshPending,
           (unsigned)liveView->playerSetupPending,
           (unsigned)liveView->tileEnterPending);
    printf("[JUNCTIONSETUP] ORDER hudOwned=yes firstFacingTransientUnowned=yes playerSetupApplied=yes tileEnterDeferred=yes finishRotationDeferred=yes finalFacingDeferred=yes disabledWeapons=%u\n",
           (unsigned)disabledWeapons);
    printf("[JUNCTIONSETUP] FAILCLOSED nullView=%d nullHud=%d nullOutput=%d inactive=%d loadType=%d hudPending=%d missingFacing=%d missingSetup=%d missingTile=%d hudMismatch=%d weaponRestore=%d reset=%d prepareAtomic=%s repeat=%d repeatAtomic=%s\n",
           nullViewGate, nullHudGate, nullOutputGate, inactiveGate,
           loadTypeGate, hudPendingGate, missingFacingGate, missingSetupGate,
           missingTileGate, hudMismatchGate, weaponRestoreGate, resetProof,
           prepareAtomic ? "yes" : "no", repeatGate,
           repeatAtomic ? "yes" : "no");
    printf("[JUNCTIONSETUP] RESIDENT snapshotFNV=%08x->%08x targetLeftResident=yes payload=%u entities=%u enemies=%u destructibles=%u packClosed=yes\n",
           (unsigned)hashBytes(&residentBefore, sizeof(residentBefore)),
           (unsigned)hashBytes(&residentAfter, sizeof(residentAfter)),
           (unsigned)residentAfter.totalPayloadBytes,
           (unsigned)residentAfter.entityCount,
           (unsigned)residentAfter.enemyCount,
           (unsigned)residentAfter.destructibleCount);
    printf("[JUNCTIONSETUP] RAM heap8=%u->%u delta=%ld largest8=%u->%u delta=%ld persistentHeapBytes=0\n",
           (unsigned)heapBefore, (unsigned)heapAfter,
           (long)((int32_t)heapAfter - (int32_t)heapBefore),
           (unsigned)largestBefore, (unsigned)largestAfter,
           (long)((int32_t)largestAfter - (int32_t)largestBefore));
    printf("[JUNCTIONSETUP] LEGACY placementFNV=%08x->%08x playerSetupFNV=%08x->%08x frameFNV=%08x->%08x legacyRuntimeClear=yes DoomCanvasMutation=no GameMutation=no PlayerMutation=no RenderMutation=no HudMutation=no\n",
           (unsigned)placementBefore, (unsigned)placementAfter,
           (unsigned)playerBefore, (unsigned)playerAfter,
           (unsigned)frameBefore, (unsigned)frameAfter);
    printf("[JUNCTIONSETUP] PARK state=%d page=%d mapSwapCommitted=yes targetMap=9 junctionResident=yes nativePlayerView=yes nativeHudRefresh=yes nativePlayerSetup=yes setupApplied=yes hudDirty=yes facingPending=yes playerSetupPending=no tileEnterPending=yes finishRotationPending=yes finalFacingPending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage);

    probeState.done = 1;
}
