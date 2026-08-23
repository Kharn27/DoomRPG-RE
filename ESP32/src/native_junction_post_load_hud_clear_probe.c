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
#include "native_junction_facing_probe.h"
#include "native_junction_post_load_hud_clear_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_CLEAR_BYTES 8U
#define EXPECTED_VIEW_BYTES 44U
#define EXPECTED_VIEW_FNV 0xafcdcf74U
#define EXPECTED_FACING_BYTES 32U
#define EXPECTED_FACING_FNV 0x95aa1108U
#define EXPECTED_SNAPSHOT_FNV 0xbc9071e9U

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

static int residentCanonical(const EspMapResidentSnapshot* s) {
    return s != NULL && sizeof(*s) == 96U &&
           s->totalPayloadBytes == 10410U &&
           s->runtimeArenaBytes == 8867U && s->mapStateBytes == 1024U &&
           s->scriptStateBytes == 73U && s->lineStateBytes == 52U &&
           s->textureStateBytes == 26U && s->automapStateBytes == 32U &&
           s->topologyBytes == 336U && s->runtimeFNV1a == 0xbc432a0fU &&
           s->mapStateFNV1a == 0xc5cdfc04U &&
           s->scriptStateFNV1a == 0xbc9b18ffU &&
           s->lineStateFNV1a == 0x3658710dU &&
           s->textureStateFNV1a == 0x537319adU &&
           s->automapStateFNV1a == 0x0b2ae445U &&
           s->topologyFNV1a == 0xd6e8df7dU &&
           s->entityCount == 30U && s->enemyCount == 0U &&
           s->destructibleCount == 3U &&
           hashBytes(s, sizeof(*s)) == EXPECTED_SNAPSHOT_FNV;
}

static int viewCanonical(const EspPlayerViewState* v) {
    return v != NULL && sizeof(*v) == EXPECTED_VIEW_BYTES &&
           v->viewX == 992 && v->viewY == 1888 && v->viewZ == 36 &&
           v->viewAngle == 64 && v->destX == 992 && v->destY == 1888 &&
           v->destAngle == 64 && v->viewZOld == 4 &&
           v->targetMapId == 9U && v->gameplayLoadMapId == 2U &&
           v->loadType == 0U && v->spawnApplied == 1U &&
           v->hudRefreshPending == 0U && v->facingRefreshPending == 0U &&
           v->playerSetupPending == 0U && v->tileEnterPending == 0U &&
           v->active == 1U && hashBytes(v, sizeof(*v)) == EXPECTED_VIEW_FNV;
}

static int facingCanonical(const EspPlayerFacingState* f) {
    return f != NULL && sizeof(*f) == EXPECTED_FACING_BYTES &&
           f->traceStartX == 992 && f->traceStartY == 1857 &&
           f->traceEndX == 992 && f->traceEndY == 1665 &&
           f->legacyIdentity == 0U && f->hitIndex == ESP_PLAYER_FACING_NO_INDEX &&
           f->hitTile == ESP_PLAYER_FACING_NO_TILE && f->targetMapId == 9U &&
           f->gameplayLoadMapId == 2U && f->loadType == 0U &&
           f->kind == ESP_PLAYER_FACING_KIND_NONE && f->entityType == 0xffU &&
           f->entitySubType == 0xffU && f->traceEntityCount == 0U &&
           f->active == 1U && hashBytes(f, sizeof(*f)) == EXPECTED_FACING_FNV;
}

static int clearStateCanonical(const EspHudPostLoadClearState* s) {
    return s != NULL && sizeof(*s) == EXPECTED_CLEAR_BYTES &&
           s->targetMapId == 9U && s->gameplayLoadMapId == 2U &&
           s->loadType == 0U && s->messageCount == 0U &&
           s->statBarMessagePresent == 0U && s->logMessageLength == 0U &&
           s->cleared == 1U && s->active == 1U;
}

void Esp32JunctionPostLoadHudClearProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspHudPostLoadClear_reset();
}

int Esp32JunctionPostLoadHudClearProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionPostLoadHudClearProbe_service(struct DoomRPG_s* doomRpg) {
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentPrepared;
    EspMapResidentSnapshot residentAfter;
    EspPlayerViewState viewBefore;
    EspPlayerFacingState facingBefore;
    EspPlayerViewState badView;
    EspPlayerFacingState badFacing;
    EspHudPostLoadClearState scratch;
    EspHudPostLoadClearState prepared;
    EspHudPostLoadClearState clearBeforeRepeat;
    const EspHudPostLoadClearState* liveClear;
    EspHudPostLoadClearStatus status;
    uint32_t heapBefore, heapAfter, largestBefore, largestAfter;
    uint32_t frameBefore, frameAfter, gameBefore, gameAfter;
    uint32_t playerBefore, playerAfter, canvasBefore, canvasAfter;
    uint32_t renderBefore, renderAfter, hudBefore, hudAfter;
    int legacyMsgCountBefore, legacyMsgCountAfter;
    int legacyStatBarBefore, legacyStatBarAfter;
    int legacyLogFirstBefore, legacyLogFirstAfter;
    int nullViewGate, nullFacingGate, nullOutputGate, inactiveViewGate;
    int inactiveFacingGate, facingMismatchGate, loadTypeGate, orderGate;
    int prepareAtomic, repeatGate, repeatAtomic;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionFacingProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[JUNCTIONHUDCLEARPROBE] ARMED hardware-proven durable facing complete; post-load HUD message clear starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native Junction post-load HUD clear ===\n");
    printf("[JUNCTIONHUDCLEARPROBE] CONTRACT recover only the three caller writes immediately after DoomCanvas_finishRotation: Hud.msgCount=0, Hud.statBarMessage=NULL, Hud.logMessage[0]=0; park one 8B pointer-free semantic owner after hardware-proven durable facing; do not mutate legacy Hud, do not execute Junction Game_givemap, do not reselect weapon/save/clear load flags/free particles/set isUpdateView/enter ST_PLAYING/set idleTime, do not present and do not allocate\n");

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL || doomRpg->game == NULL ||
        doomRpg->render == NULL || doomRpg->player == NULL || doomRpg->hud == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO || doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        EspAssetPack_isOpen() || !legacyRuntimeIsClear(doomRpg->render) ||
        sizeof(EspHudPostLoadClearState) != EXPECTED_CLEAR_BYTES ||
        !viewCanonical(EspPlayerView_view()) ||
        !facingCanonical(EspPlayerFacing_view()) ||
        !EspMapResidentLifecycle_capture(&residentBefore) ||
        !residentCanonical(&residentBefore)) {
        printf("[JUNCTIONHUDCLEARPROBE] FAILED unsafe post-facing boundary\n");
        probeState.done = 1;
        return;
    }

    viewBefore = *EspPlayerView_view();
    facingBefore = *EspPlayerFacing_view();
    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    gameBefore = gameWitness(doomRpg->game);
    playerBefore = playerWitness(doomRpg->player);
    canvasBefore = canvasWitness(doomRpg->doomCanvas);
    renderBefore = renderWitness(doomRpg->render);
    hudBefore = hashBytes(doomRpg->hud, sizeof(*doomRpg->hud));
    legacyMsgCountBefore = doomRpg->hud->msgCount;
    legacyStatBarBefore = doomRpg->hud->statBarMessage != NULL;
    legacyLogFirstBefore = (uint8_t)doomRpg->hud->logMessage[0];

    memset(&scratch, 0xa5, sizeof(scratch));
    nullViewGate = EspHudPostLoadClear_prepare(NULL, &facingBefore, &scratch) ==
                   ESP_HUD_POST_LOAD_CLEAR_INVALID;
    nullFacingGate = EspHudPostLoadClear_prepare(&viewBefore, NULL, &scratch) ==
                     ESP_HUD_POST_LOAD_CLEAR_INVALID;
    nullOutputGate = EspHudPostLoadClear_prepare(&viewBefore, &facingBefore, NULL) ==
                     ESP_HUD_POST_LOAD_CLEAR_INVALID;

    badView = viewBefore;
    badView.active = 0U;
    inactiveViewGate = EspHudPostLoadClear_prepare(&badView, &facingBefore, &scratch) ==
                       ESP_HUD_POST_LOAD_CLEAR_VIEW_INVALID;

    badFacing = facingBefore;
    badFacing.active = 0U;
    inactiveFacingGate = EspHudPostLoadClear_prepare(&viewBefore, &badFacing, &scratch) ==
                         ESP_HUD_POST_LOAD_CLEAR_FACING_INVALID;

    badFacing = facingBefore;
    badFacing.targetMapId = 1U;
    facingMismatchGate = EspHudPostLoadClear_prepare(&viewBefore, &badFacing, &scratch) ==
                         ESP_HUD_POST_LOAD_CLEAR_FACING_INVALID;

    badView = viewBefore;
    badFacing = facingBefore;
    badView.loadType = 1U;
    badFacing.loadType = 1U;
    loadTypeGate = EspHudPostLoadClear_prepare(&badView, &badFacing, &scratch) ==
                   ESP_HUD_POST_LOAD_CLEAR_UNSUPPORTED_CONTEXT;

    badView = viewBefore;
    badView.facingRefreshPending = 1U;
    orderGate = EspHudPostLoadClear_prepare(&badView, &facingBefore, &scratch) ==
                ESP_HUD_POST_LOAD_CLEAR_UNSUPPORTED_ORDER;

    status = EspHudPostLoadClear_prepare(&viewBefore, &facingBefore, &prepared);
    if (status != ESP_HUD_POST_LOAD_CLEAR_OK || !clearStateCanonical(&prepared) ||
        EspHudPostLoadClear_isReady() ||
        memcmp(&viewBefore, EspPlayerView_view(), sizeof(viewBefore)) != 0 ||
        memcmp(&facingBefore, EspPlayerFacing_view(), sizeof(facingBefore)) != 0 ||
        EspAssetPack_isOpen() ||
        !EspMapResidentLifecycle_capture(&residentPrepared) ||
        memcmp(&residentBefore, &residentPrepared, sizeof(residentBefore)) != 0) {
        printf("[JUNCTIONHUDCLEARPROBE] FAILED pure post-load HUD clear preparation status=%u\n",
               (unsigned)status);
        probeState.done = 1;
        return;
    }
    prepareAtomic = 1;

    status = EspHudPostLoadClear_route();
    if (status != ESP_HUD_POST_LOAD_CLEAR_OK || !EspHudPostLoadClear_isReady() ||
        !clearStateCanonical(EspHudPostLoadClear_view()) ||
        !viewCanonical(EspPlayerView_view()) ||
        !facingCanonical(EspPlayerFacing_view()) || EspAssetPack_isOpen()) {
        printf("[JUNCTIONHUDCLEARPROBE] FAILED live post-load HUD clear route status=%u\n",
               (unsigned)status);
        probeState.done = 1;
        return;
    }

    clearBeforeRepeat = *EspHudPostLoadClear_view();
    repeatGate = EspHudPostLoadClear_route() == ESP_HUD_POST_LOAD_CLEAR_ALREADY_ACTIVE;
    repeatAtomic = memcmp(&clearBeforeRepeat, EspHudPostLoadClear_view(),
                          sizeof(clearBeforeRepeat)) == 0;

    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfter = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameAfter = framebufferHash();
    gameAfter = gameWitness(doomRpg->game);
    playerAfter = playerWitness(doomRpg->player);
    canvasAfter = canvasWitness(doomRpg->doomCanvas);
    renderAfter = renderWitness(doomRpg->render);
    hudAfter = hashBytes(doomRpg->hud, sizeof(*doomRpg->hud));
    legacyMsgCountAfter = doomRpg->hud->msgCount;
    legacyStatBarAfter = doomRpg->hud->statBarMessage != NULL;
    legacyLogFirstAfter = (uint8_t)doomRpg->hud->logMessage[0];

    if (!EspMapResidentLifecycle_capture(&residentAfter) ||
        memcmp(&residentBefore, &residentAfter, sizeof(residentBefore)) != 0 ||
        heapBefore != heapAfter || largestBefore != largestAfter ||
        frameBefore != frameAfter || gameBefore != gameAfter ||
        playerBefore != playerAfter || canvasBefore != canvasAfter ||
        renderBefore != renderAfter || hudBefore != hudAfter ||
        legacyMsgCountBefore != legacyMsgCountAfter ||
        legacyStatBarBefore != legacyStatBarAfter ||
        legacyLogFirstBefore != legacyLogFirstAfter ||
        memcmp(&viewBefore, EspPlayerView_view(), sizeof(viewBefore)) != 0 ||
        memcmp(&facingBefore, EspPlayerFacing_view(), sizeof(facingBefore)) != 0 ||
        EspAssetPack_isOpen() || !legacyRuntimeIsClear(doomRpg->render)) {
        printf("[JUNCTIONHUDCLEARPROBE] FAILED integrity after post-load HUD clear route\n");
        probeState.done = 1;
        return;
    }

    liveClear = EspHudPostLoadClear_view();
    printf("[JUNCTIONHUDCLEAR] READY stateBytes=%u stateFNV=%08x messageCount=%u statBarMessagePresent=%u logMessageLength=%u cleared=%u active=%u targetMap=%u gameplayLoadMapId=%u loadType=%u\n",
           (unsigned)sizeof(*liveClear),
           (unsigned)hashBytes(liveClear, sizeof(*liveClear)),
           (unsigned)liveClear->messageCount,
           (unsigned)liveClear->statBarMessagePresent,
           (unsigned)liveClear->logMessageLength,
           (unsigned)liveClear->cleared, (unsigned)liveClear->active,
           (unsigned)liveClear->targetMapId,
           (unsigned)liveClear->gameplayLoadMapId,
           (unsigned)liveClear->loadType);
    printf("[JUNCTIONHUDCLEAR] INPUT viewFNV=%08x facingFNV=%08x unchanged=yes finishRotationComplete=yes\n",
           (unsigned)hashBytes(EspPlayerView_view(), sizeof(EspPlayerViewState)),
           (unsigned)hashBytes(EspPlayerFacing_view(), sizeof(EspPlayerFacingState)));
    printf("[JUNCTIONHUDCLEAR] SEMANTIC legacyWrites=msgCount0+statBarNull+logEmpty represented=yes legacyHudUntouched=yes legacyMsgCount=%d->%d legacyStatBarPresent=%d->%d legacyLogFirst=%d->%d\n",
           legacyMsgCountBefore, legacyMsgCountAfter,
           legacyStatBarBefore, legacyStatBarAfter,
           legacyLogFirstBefore, legacyLogFirstAfter);
    printf("[JUNCTIONHUDCLEAR] FAILCLOSED nullView=%d nullFacing=%d nullOutput=%d inactiveView=%d inactiveFacing=%d facingMismatch=%d loadType=%d order=%d prepareAtomic=%s repeat=%d repeatAtomic=%s\n",
           nullViewGate, nullFacingGate, nullOutputGate, inactiveViewGate,
           inactiveFacingGate, facingMismatchGate, loadTypeGate, orderGate,
           prepareAtomic ? "yes" : "no", repeatGate,
           repeatAtomic ? "yes" : "no");
    printf("[JUNCTIONHUDCLEAR] RESIDENT snapshotFNV=%08x->%08x unchanged=yes automapFNV=%08x->%08x payload=%u entities=%u enemies=%u destructibles=%u packClosed=yes Game_givemapDeferred=yes\n",
           (unsigned)hashBytes(&residentBefore, sizeof(residentBefore)),
           (unsigned)hashBytes(&residentAfter, sizeof(residentAfter)),
           (unsigned)residentBefore.automapStateFNV1a,
           (unsigned)residentAfter.automapStateFNV1a,
           (unsigned)residentAfter.totalPayloadBytes,
           (unsigned)residentAfter.entityCount,
           (unsigned)residentAfter.enemyCount,
           (unsigned)residentAfter.destructibleCount);
    printf("[JUNCTIONHUDCLEAR] RAM heap8=%u->%u delta=%ld largest8=%u->%u delta=%ld persistentHeapBytes=0\n",
           (unsigned)heapBefore, (unsigned)heapAfter,
           (long)((int32_t)heapAfter - (int32_t)heapBefore),
           (unsigned)largestBefore, (unsigned)largestAfter,
           (long)((int32_t)largestAfter - (int32_t)largestBefore));
    printf("[JUNCTIONHUDCLEAR] LEGACY gameFNV=%08x->%08x playerFNV=%08x->%08x hudFNV=%08x->%08x canvasFNV=%08x->%08x renderFNV=%08x->%08x frameFNV=%08x->%08x legacyRuntimeClear=yes GameMutation=no PlayerMutation=no HudMutation=no DoomCanvasMutation=no RenderMutation=no\n",
           (unsigned)gameBefore, (unsigned)gameAfter,
           (unsigned)playerBefore, (unsigned)playerAfter,
           (unsigned)hudBefore, (unsigned)hudAfter,
           (unsigned)canvasBefore, (unsigned)canvasAfter,
           (unsigned)renderBefore, (unsigned)renderAfter,
           (unsigned)frameBefore, (unsigned)frameAfter);
    printf("[JUNCTIONHUDCLEAR] PARK state=%d page=%d targetMap=%u junctionResident=yes nativeFacing=yes nativeHudClear=yes finishRotationComplete=yes Game_givemapPending=yes weaponReselectPending=yes initialSavePending=yes postLoadCleanupPending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned)liveClear->targetMapId);

    probeState.done = 1;
}
