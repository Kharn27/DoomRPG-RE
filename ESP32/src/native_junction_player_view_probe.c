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
#include "esp_player_spawn_state.h"
#include "esp_player_view_state.h"
#include "native_junction_player_view_probe.h"
#include "native_junction_spawn_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_STATE_BYTES 44U
#define EXPECTED_REAL_FNV 0xd1131d18U
#define EXPECTED_OVERRIDE_FNV 0x9ed47d08U
#define EXPECTED_TARGET_SNAPSHOT_FNV 0xbc9071e9U
#define EXPECTED_OVERRIDE_PARAM 0x00030167UL

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

static int stateIsZero(const EspPlayerViewState* state) {
    EspPlayerViewState zero;
    if (state == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(state, &zero, sizeof(zero)) == 0;
}

static int realViewCanonical(const EspPlayerViewState* state) {
    return state != NULL && sizeof(*state) == EXPECTED_STATE_BYTES &&
           state->viewX == 992 && state->viewY == 1888 && state->viewZ == 36 &&
           state->viewAngle == 64 && state->destX == 992 &&
           state->destY == 1888 && state->destAngle == 64 &&
           state->viewZOld == 4 && state->targetMapId == 9U &&
           state->gameplayLoadMapId == 2U && state->loadType == 0U &&
           state->spawnApplied == 1U && state->hudRefreshPending == 1U &&
           state->facingRefreshPending == 1U &&
           state->playerSetupPending == 1U && state->tileEnterPending == 1U &&
           state->active == 1U && hashBytes(state, sizeof(*state)) == EXPECTED_REAL_FNV;
}

static int overrideViewCanonical(const EspPlayerViewState* state) {
    return state != NULL && state->viewX == 480 && state->viewY == 736 &&
           state->viewZ == 36 && state->viewAngle == 192 &&
           state->destX == 480 && state->destY == 736 &&
           state->destAngle == 192 && state->viewZOld == 4 &&
           state->targetMapId == 9U && state->gameplayLoadMapId == 2U &&
           state->loadType == 0U && state->spawnApplied == 1U &&
           state->hudRefreshPending == 1U && state->facingRefreshPending == 1U &&
           state->playerSetupPending == 1U && state->tileEnterPending == 1U &&
           state->active == 1U &&
           hashBytes(state, sizeof(*state)) == EXPECTED_OVERRIDE_FNV;
}

void Esp32JunctionPlayerViewProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspPlayerView_reset();
}

int Esp32JunctionPlayerViewProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionPlayerViewProbe_service(struct DoomRPG_s* doomRpg) {
    EspPlayerSpawnState spawn;
    EspPlayerSpawnState badSpawn;
    EspPlayerSpawnState overrideSpawn;
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentAfter;
    EspPlayerViewState beforeRepeat;
    const EspPlayerViewState* view;
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
    uint32_t realFNV;
    uint32_t overrideFNV;
    int nullSpawnGate;
    int inactiveGate;
    int geometryGate;
    int pendingGate;
    int repeatGate;
    int repeatAtomic;
    int resetProof;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionSpawnProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[JUNCTIONVIEWPROBE] ARMED hardware-proven Junction spawn projection ready; native player/view application starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native Junction player/view application ===\n");
    printf("[JUNCTIONVIEWPROBE] CONTRACT apply one validated 24B spawn projection into a 44B permanent pointer-free native player/view owner, including semantic Hud refresh pending; no legacy DoomCanvas/Render/Hud/Game/Player mutation, no facing query, no Player_setup, no tile-enter, no ST_PLAYING and no allocation\n");

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL || doomRpg->game == NULL ||
        doomRpg->render == NULL || doomRpg->player == NULL || doomRpg->hud == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO || doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        !legacyRuntimeIsClear(doomRpg->render) || EspAssetPack_isOpen() ||
        sizeof(EspPlayerViewState) != EXPECTED_STATE_BYTES ||
        !Esp32JunctionSpawnProbe_getState(&spawn) ||
        !EspMapResidentLifecycle_capture(&residentBefore) ||
        !snapshotCanonical(&residentBefore)) {
        printf("[JUNCTIONVIEWPROBE] FAILED unsafe Junction spawn boundary\n");
        probeState.done = 1;
        return;
    }

    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    placementBefore = placementWitness(doomRpg);
    playerBefore = playerWitness(doomRpg->player);

    EspPlayerView_reset();
    resetProof = !EspPlayerView_isReady() && EspPlayerView_view() == NULL;
    nullSpawnGate =
        EspPlayerView_applySpawn(NULL) == ESP_PLAYER_VIEW_APPLY_INVALID &&
        !EspPlayerView_isReady();

    badSpawn = spawn;
    badSpawn.active = 0U;
    inactiveGate =
        EspPlayerView_applySpawn(&badSpawn) == ESP_PLAYER_VIEW_APPLY_SPAWN_INVALID &&
        !EspPlayerView_isReady();

    badSpawn = spawn;
    badSpawn.worldX = (uint16_t)(badSpawn.worldX + 1U);
    geometryGate =
        EspPlayerView_applySpawn(&badSpawn) == ESP_PLAYER_VIEW_APPLY_SPAWN_INVALID &&
        !EspPlayerView_isReady();

    badSpawn = spawn;
    badSpawn.tileEnterPending = 0U;
    pendingGate =
        EspPlayerView_applySpawn(&badSpawn) == ESP_PLAYER_VIEW_APPLY_SPAWN_INVALID &&
        !EspPlayerView_isReady();

    if (EspPlayerView_applySpawn(&spawn) != ESP_PLAYER_VIEW_APPLY_OK ||
        !EspPlayerView_isReady() || !realViewCanonical(EspPlayerView_view())) {
        printf("[JUNCTIONVIEWPROBE] FAILED real player/view application\n");
        probeState.done = 1;
        return;
    }
    view = EspPlayerView_view();
    realFNV = hashBytes(view, sizeof(*view));
    beforeRepeat = *view;
    repeatGate = EspPlayerView_applySpawn(&spawn) == ESP_PLAYER_VIEW_APPLY_ALREADY_ACTIVE;
    repeatAtomic = EspPlayerView_view() != NULL &&
                   memcmp(&beforeRepeat, EspPlayerView_view(), sizeof(beforeRepeat)) == 0;

    EspPlayerView_reset();
    if (EspPlayerView_view() != NULL) {
        printf("[JUNCTIONVIEWPROBE] FAILED reset after real application\n");
        probeState.done = 1;
        return;
    }

    overrideSpawn = spawn;
    overrideSpawn.sourceSpawnParam = EXPECTED_OVERRIDE_PARAM;
    overrideSpawn.tileIndex = 359U;
    overrideSpawn.worldX = 480U;
    overrideSpawn.worldY = 736U;
    overrideSpawn.tileX = 7U;
    overrideSpawn.tileY = 11U;
    overrideSpawn.angle = 192U;
    overrideSpawn.spawnSource = ESP_PLAYER_SPAWN_SOURCE_OVERRIDE;
    overrideSpawn.overrideUsed = 1U;
    if (EspPlayerView_applySpawn(&overrideSpawn) != ESP_PLAYER_VIEW_APPLY_OK ||
        !overrideViewCanonical(EspPlayerView_view())) {
        printf("[JUNCTIONVIEWPROBE] FAILED override player/view application\n");
        probeState.done = 1;
        return;
    }
    overrideFNV = hashBytes(EspPlayerView_view(), sizeof(EspPlayerViewState));

    EspPlayerView_reset();
    if (EspPlayerView_applySpawn(&spawn) != ESP_PLAYER_VIEW_APPLY_OK ||
        !realViewCanonical(EspPlayerView_view())) {
        printf("[JUNCTIONVIEWPROBE] FAILED final real player/view park\n");
        probeState.done = 1;
        return;
    }

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
        printf("[JUNCTIONVIEWPROBE] FAILED integrity after application\n");
        probeState.done = 1;
        return;
    }

    view = EspPlayerView_view();
    printf("[JUNCTIONVIEW] READY stateBytes=%u stateFNV=%08x view=%ld/%ld/%ld angle=%ld dest=%ld/%ld angle=%ld viewZOld=%ld targetMap=%u gameplayLoadMapId=%u loadType=%u active=%u spawnApplied=%u\n",
           (unsigned)sizeof(*view), (unsigned)realFNV,
           (long)view->viewX, (long)view->viewY, (long)view->viewZ,
           (long)view->viewAngle, (long)view->destX, (long)view->destY,
           (long)view->destAngle, (long)view->viewZOld,
           (unsigned)view->targetMapId, (unsigned)view->gameplayLoadMapId,
           (unsigned)view->loadType, (unsigned)view->active,
           (unsigned)view->spawnApplied);
    printf("[JUNCTIONVIEW] FOLLOWUPS hudRefresh=%u facingRefresh=%u playerSetup=%u tileEnter=%u hudApplied=no facingApplied=no playerSetupApplied=no tileEnterApplied=no\n",
           (unsigned)view->hudRefreshPending,
           (unsigned)view->facingRefreshPending,
           (unsigned)view->playerSetupPending,
           (unsigned)view->tileEnterPending);
    printf("[JUNCTIONVIEW] OVERRIDE param=%08x view=480/736/36 angle=192 dest=480/736 angle=192 stateFNV=%08x sourceProjectionFNV=e0a5110b\n",
           (unsigned)EXPECTED_OVERRIDE_PARAM, (unsigned)overrideFNV);
    printf("[JUNCTIONVIEW] FAILCLOSED nullSpawn=%d inactive=%d badGeometry=%d badPending=%d repeat=%d repeatAtomic=%s reset=%d stateAtomic=%s\n",
           nullSpawnGate, inactiveGate, geometryGate, pendingGate, repeatGate,
           repeatAtomic ? "yes" : "no", resetProof,
           (nullSpawnGate && inactiveGate && geometryGate && pendingGate &&
            repeatGate && repeatAtomic && resetProof) ? "yes" : "no");
    printf("[JUNCTIONVIEW] RESIDENT snapshotFNV=%08x->%08x targetLeftResident=yes payload=%u entities=%u enemies=%u destructibles=%u packClosed=yes\n",
           (unsigned)hashBytes(&residentBefore, sizeof(residentBefore)),
           (unsigned)hashBytes(&residentAfter, sizeof(residentAfter)),
           (unsigned)residentAfter.totalPayloadBytes,
           (unsigned)residentAfter.entityCount,
           (unsigned)residentAfter.enemyCount,
           (unsigned)residentAfter.destructibleCount);
    printf("[JUNCTIONVIEW] RAM heap8=%u->%u delta=%ld largest8=%u->%u delta=%ld persistentHeapBytes=0\n",
           (unsigned)heapBefore, (unsigned)heapAfter,
           (long)((int32_t)heapAfter - (int32_t)heapBefore),
           (unsigned)largestBefore, (unsigned)largestAfter,
           (long)((int32_t)largestAfter - (int32_t)largestBefore));
    printf("[JUNCTIONVIEW] LEGACY placementFNV=%08x->%08x playerFNV=%08x->%08x frameFNV=%08x->%08x legacyRuntimeClear=yes DoomCanvasMutation=no GameMutation=no PlayerMutation=no RenderMutation=no HudMutation=no\n",
           (unsigned)placementBefore, (unsigned)placementAfter,
           (unsigned)playerBefore, (unsigned)playerAfter,
           (unsigned)frameBefore, (unsigned)frameAfter);
    printf("[JUNCTIONVIEW] PARK state=%d page=%d mapSwapCommitted=yes targetMap=9 junctionResident=yes nativeSpawnState=yes nativePlayerView=yes spawnAppliedNative=yes legacySpawnApplied=no hudRefreshPending=yes facingPending=yes playerSetupPending=yes tileEnterPending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage);

    probeState.done = 1;
}
