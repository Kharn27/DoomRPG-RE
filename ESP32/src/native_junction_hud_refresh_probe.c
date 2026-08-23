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
#include "esp_player_view_state.h"
#include "native_junction_hud_refresh_probe.h"
#include "native_junction_player_view_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_HUD_STATE_BYTES 8U
#define EXPECTED_HUD_STATE_FNV 0x6965ee06U
#define EXPECTED_VIEW_STATE_BYTES 44U
#define EXPECTED_VIEW_BEFORE_FNV 0xd1131d18U
#define EXPECTED_VIEW_AFTER_FNV 0xd17fa0d1U
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

static uint32_t playerWitness(const Player_t* player) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (player == NULL) return 0U;
    hash ^= (uint32_t)player->weapon;
    hash *= 16777619U;
    hash ^= (uint32_t)player->weapons;
    hash *= 16777619U;
    hash ^= (uint32_t)player->totalTime;
    hash *= 16777619U;
    hash ^= (uint32_t)player->totalMoves;
    hash *= 16777619U;
    for (i = 0U; i < (uint32_t)(sizeof(player->ammo) / sizeof(player->ammo[0])); ++i) {
        hash ^= (uint32_t)player->ammo[i];
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
           view->spawnApplied == 1U && view->hudRefreshPending == 1U &&
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
           view->playerSetupPending == 1U && view->tileEnterPending == 1U &&
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

static int hudStateZero(const EspHudRefreshState* state) {
    EspHudRefreshState zero;
    if (state == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(state, &zero, sizeof(zero)) == 0;
}

void Esp32JunctionHudRefreshProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspHudRefresh_reset();
}

int Esp32JunctionHudRefreshProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionHudRefreshProbe_service(struct DoomRPG_s* doomRpg) {
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentAfter;
    EspPlayerViewState viewBefore;
    EspPlayerViewState badView;
    EspHudRefreshState scratch;
    EspHudRefreshState prepared;
    EspHudRefreshState hudBeforeRepeat;
    const EspPlayerViewState* liveView;
    const EspHudRefreshState* liveHud;
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
    int resetProof;
    int nullViewGate;
    int nullOutputGate;
    int inactiveGate;
    int loadTypeGate;
    int missingHudGate;
    int missingFacingGate;
    int prepareAtomic;
    int repeatGate;
    int repeatAtomic;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionPlayerViewProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[JUNCTIONHUDPROBE] ARMED hardware-proven native player/view active; post-spawn HUD dirty routing starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native Junction post-spawn HUD refresh ===\n");
    printf("[JUNCTIONHUDPROBE] CONTRACT route recovered Hud.isUpdate=true into one 8B permanent native dirty owner and consume only player/view hudRefreshPending; preserve facing/Player_setup/tile-enter pending; first stale-vector facing write is not materialized and final facing stays deferred until correct post-tile finishRotation order; no legacy Hud/DoomCanvas/Render/Game/Player mutation, no presentation, no ST_PLAYING and no allocation\n");

    liveView = EspPlayerView_view();
    if (doomRpg == NULL || doomRpg->doomCanvas == NULL || doomRpg->game == NULL ||
        doomRpg->render == NULL || doomRpg->player == NULL || doomRpg->hud == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO || doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        !legacyRuntimeIsClear(doomRpg->render) || EspAssetPack_isOpen() ||
        sizeof(EspHudRefreshState) != EXPECTED_HUD_STATE_BYTES ||
        sizeof(EspPlayerViewState) != EXPECTED_VIEW_STATE_BYTES ||
        !viewBeforeCanonical(liveView) ||
        !EspMapResidentLifecycle_capture(&residentBefore) ||
        !snapshotCanonical(&residentBefore)) {
        printf("[JUNCTIONHUDPROBE] FAILED unsafe player/view boundary\n");
        probeState.done = 1;
        return;
    }

    viewBefore = *liveView;
    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    placementBefore = placementWitness(doomRpg);
    playerBefore = playerWitness(doomRpg->player);

    EspHudRefresh_reset();
    resetProof = !EspHudRefresh_isReady() && EspHudRefresh_view() == NULL;

    memset(&scratch, 0xa5, sizeof(scratch));
    nullViewGate =
        EspHudRefresh_preparePostSpawn(NULL, &scratch) == ESP_HUD_REFRESH_INVALID &&
        hudStateZero(&scratch);
    nullOutputGate =
        EspHudRefresh_preparePostSpawn(&viewBefore, NULL) == ESP_HUD_REFRESH_INVALID;

    badView = viewBefore;
    badView.active = 0U;
    memset(&scratch, 0xa5, sizeof(scratch));
    inactiveGate =
        EspHudRefresh_preparePostSpawn(&badView, &scratch) ==
            ESP_HUD_REFRESH_VIEW_INVALID &&
        hudStateZero(&scratch);

    badView = viewBefore;
    badView.loadType = 1U;
    memset(&scratch, 0xa5, sizeof(scratch));
    loadTypeGate =
        EspHudRefresh_preparePostSpawn(&badView, &scratch) ==
            ESP_HUD_REFRESH_UNSUPPORTED_CONTEXT &&
        hudStateZero(&scratch);

    badView = viewBefore;
    badView.hudRefreshPending = 0U;
    memset(&scratch, 0xa5, sizeof(scratch));
    missingHudGate =
        EspHudRefresh_preparePostSpawn(&badView, &scratch) ==
            ESP_HUD_REFRESH_UNSUPPORTED_CONTEXT &&
        hudStateZero(&scratch);

    badView = viewBefore;
    badView.facingRefreshPending = 0U;
    memset(&scratch, 0xa5, sizeof(scratch));
    missingFacingGate =
        EspHudRefresh_preparePostSpawn(&badView, &scratch) ==
            ESP_HUD_REFRESH_UNSUPPORTED_CONTEXT &&
        hudStateZero(&scratch);

    if (EspHudRefresh_preparePostSpawn(&viewBefore, &prepared) !=
            ESP_HUD_REFRESH_OK ||
        !hudCanonical(&prepared) ||
        !viewBeforeCanonical(EspPlayerView_view())) {
        printf("[JUNCTIONHUDPROBE] FAILED pure HUD refresh preparation\n");
        probeState.done = 1;
        return;
    }
    prepareAtomic = memcmp(&viewBefore, EspPlayerView_view(), sizeof(viewBefore)) == 0;

    if (EspHudRefresh_routePostSpawn() != ESP_HUD_REFRESH_OK ||
        !hudCanonical(EspHudRefresh_view()) ||
        !viewAfterCanonical(EspPlayerView_view())) {
        printf("[JUNCTIONHUDPROBE] FAILED live HUD refresh routing\n");
        probeState.done = 1;
        return;
    }

    liveHud = EspHudRefresh_view();
    hudBeforeRepeat = *liveHud;
    repeatGate = EspHudRefresh_routePostSpawn() == ESP_HUD_REFRESH_ALREADY_ACTIVE;
    repeatAtomic = EspHudRefresh_view() != NULL &&
                   memcmp(&hudBeforeRepeat, EspHudRefresh_view(),
                          sizeof(hudBeforeRepeat)) == 0 &&
                   viewAfterCanonical(EspPlayerView_view());

    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfter = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameAfter = framebufferHash();
    placementAfter = placementWitness(doomRpg);
    playerAfter = playerWitness(doomRpg->player);

    if (!EspMapResidentLifecycle_capture(&residentAfter) ||
        !snapshotCanonical(&residentAfter) ||
        memcmp(&residentBefore, &residentAfter, sizeof(residentBefore)) != 0 ||
        heapBefore != heapAfter || largestBefore != largestAfter ||
        frameBefore != frameAfter || placementBefore != placementAfter ||
        playerBefore != playerAfter || EspAssetPack_isOpen()) {
        printf("[JUNCTIONHUDPROBE] FAILED integrity after HUD routing\n");
        probeState.done = 1;
        return;
    }

    liveHud = EspHudRefresh_view();
    liveView = EspPlayerView_view();
    printf("[JUNCTIONHUD] READY stateBytes=%u stateFNV=%08x reason=POST_SPAWN refreshPending=%u routed=%u active=%u targetMap=%u gameplayLoadMapId=%u loadType=%u\n",
           (unsigned)sizeof(*liveHud),
           (unsigned)hashBytes(liveHud, sizeof(*liveHud)),
           (unsigned)liveHud->refreshPending, (unsigned)liveHud->routed,
           (unsigned)liveHud->active, (unsigned)liveHud->targetMapId,
           (unsigned)liveHud->gameplayLoadMapId, (unsigned)liveHud->loadType);
    printf("[JUNCTIONHUD] PLAYER viewBytes=%u beforeFNV=%08x afterFNV=%08x hudPending=%u facingPending=%u playerSetupPending=%u tileEnterPending=%u placementExact=yes\n",
           (unsigned)sizeof(*liveView), (unsigned)EXPECTED_VIEW_BEFORE_FNV,
           (unsigned)hashBytes(liveView, sizeof(*liveView)),
           (unsigned)liveView->hudRefreshPending,
           (unsigned)liveView->facingRefreshPending,
           (unsigned)liveView->playerSetupPending,
           (unsigned)liveView->tileEnterPending);
    printf("[JUNCTIONHUD] ORDER firstFacingTransient=yes finalFacingDeferred=yes finishRotationDeferred=yes playerSetupPending=yes tileEnterPending=yes\n");
    printf("[JUNCTIONHUD] FAILCLOSED nullView=%d nullOutput=%d inactive=%d loadType=%d missingHud=%d missingFacing=%d reset=%d prepareAtomic=%s repeat=%d repeatAtomic=%s\n",
           nullViewGate, nullOutputGate, inactiveGate, loadTypeGate,
           missingHudGate, missingFacingGate, resetProof,
           prepareAtomic ? "yes" : "no", repeatGate,
           repeatAtomic ? "yes" : "no");
    printf("[JUNCTIONHUD] RESIDENT snapshotFNV=%08x->%08x targetLeftResident=yes payload=%u entities=%u enemies=%u destructibles=%u packClosed=yes\n",
           (unsigned)hashBytes(&residentBefore, sizeof(residentBefore)),
           (unsigned)hashBytes(&residentAfter, sizeof(residentAfter)),
           (unsigned)residentAfter.totalPayloadBytes,
           (unsigned)residentAfter.entityCount,
           (unsigned)residentAfter.enemyCount,
           (unsigned)residentAfter.destructibleCount);
    printf("[JUNCTIONHUD] RAM heap8=%u->%u delta=%ld largest8=%u->%u delta=%ld persistentHeapBytes=0\n",
           (unsigned)heapBefore, (unsigned)heapAfter,
           (long)((int32_t)heapAfter - (int32_t)heapBefore),
           (unsigned)largestBefore, (unsigned)largestAfter,
           (long)((int32_t)largestAfter - (int32_t)largestBefore));
    printf("[JUNCTIONHUD] LEGACY placementFNV=%08x->%08x playerFNV=%08x->%08x frameFNV=%08x->%08x legacyRuntimeClear=yes DoomCanvasMutation=no GameMutation=no PlayerMutation=no RenderMutation=no HudMutation=no\n",
           (unsigned)placementBefore, (unsigned)placementAfter,
           (unsigned)playerBefore, (unsigned)playerAfter,
           (unsigned)frameBefore, (unsigned)frameAfter);
    printf("[JUNCTIONHUD] PARK state=%d page=%d mapSwapCommitted=yes targetMap=9 junctionResident=yes nativePlayerView=yes nativeHudRefresh=yes hudDirty=yes hudRouted=yes facingPending=yes playerSetupPending=yes tileEnterPending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage);

    probeState.done = 1;
}
