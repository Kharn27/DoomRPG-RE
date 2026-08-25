#include <SDL.h>
#include "DoomRPG.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Combat.h"
#include "CombatEntity.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "Hud.h"
#include "Player.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_hud_post_load_clear_state.h"
#include "esp_hud_refresh_state.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_graphics_catalog.h"
#include "esp_player_view_state.h"
#include "native_junction_gameplay_hud_probe.h"
#include "native_junction_sprite_fidelity_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

/* Keep ESP-IDF boolean macros after DoomRPG's legacy boolean enum. */
#include <esp_heap_caps.h>

#define EXPECTED_PRE_HUD_FRAME_FNV 0xb5218f24U
#define EXPECTED_VIEWPORT_FNV 0x9206eb24U
#define EXPECTED_TOPOLOGY_FNV 0xd6e8df7dU
#define EXPECTED_CLOSED_CATALOG_FNV 0x257444a5U
#define EXPECTED_PLAYER_VIEW_FNV 0xafcdcf74U

static struct {
    int armed;
    int attempted;
    int done;
} probeState;

static uint32_t fnvAppend(uint32_t hash, const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t fnv1a(const void* data, uint32_t bytes) {
    return fnvAppend(2166136261U, data, bytes);
}

static uint32_t frameFNV(const Render_t* render) {
    uint32_t bytes = DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT *
                     (uint32_t)sizeof(uint16_t);
    if (render == NULL || render->framebuffer == NULL ||
        Esp32PlatformVideo_framebuffer() != render->framebuffer ||
        Esp32PlatformVideo_framebufferSizeBytes() != bytes) {
        return 0U;
    }
    return fnv1a(render->framebuffer, bytes);
}

static uint32_t viewportFNV(const Render_t* render) {
    const uint16_t* framebuffer;
    int pitchPixels;
    uint32_t hash = 2166136261U;
    int y;

    if (render == NULL || render->framebuffer == NULL ||
        render->screenX != 0 || render->screenY != 20 ||
        render->screenWidth != 160 || render->screenHeight != 80) {
        return 0U;
    }
    framebuffer = (const uint16_t*)render->framebuffer;
    pitchPixels = render->pitch >> 1;
    for (y = 0; y < 80; ++y) {
        const uint16_t* row = framebuffer + (20 + y) * pitchPixels;
        hash = fnvAppend(hash, row, 160U * (uint32_t)sizeof(uint16_t));
    }
    return hash;
}

static uint32_t hudBandsFNV(const Render_t* render) {
    const uint16_t* framebuffer;
    int pitchPixels;
    uint32_t hash = 2166136261U;
    int y;

    if (render == NULL || render->framebuffer == NULL) return 0U;
    framebuffer = (const uint16_t*)render->framebuffer;
    pitchPixels = render->pitch >> 1;
    for (y = 0; y < 20; ++y) {
        hash = fnvAppend(hash, framebuffer + y * pitchPixels,
                         160U * (uint32_t)sizeof(uint16_t));
    }
    for (y = 100; y < 120; ++y) {
        hash = fnvAppend(hash, framebuffer + y * pitchPixels,
                         160U * (uint32_t)sizeof(uint16_t));
    }
    return hash;
}

static uint32_t heap8(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static int legacyGraphicsClear(const Render_t* render) {
    return render != NULL && render->lines == NULL && render->nodes == NULL &&
           render->mapSprites == NULL && render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL && render->mediaSpriteIds == NULL &&
           render->shapeData == NULL && render->mediaTexels == NULL &&
           render->mapTextureTexels == NULL && render->mapSpriteTexels == NULL;
}

static int playerViewCanonical(const EspPlayerViewState* view) {
    return view != NULL && sizeof(*view) == 44U &&
           view->viewX == 992 && view->viewY == 1888 && view->viewZ == 36 &&
           view->viewAngle == 64 && view->destX == 992 && view->destY == 1888 &&
           view->destAngle == 64 && view->viewZOld == 4 &&
           view->targetMapId == 9U && view->gameplayLoadMapId == 2U &&
           view->loadType == 0U && view->spawnApplied == 1U &&
           view->hudRefreshPending == 0U && view->facingRefreshPending == 0U &&
           view->playerSetupPending == 0U && view->tileEnterPending == 0U &&
           view->active == 1U && fnv1a(view, sizeof(*view)) == EXPECTED_PLAYER_VIEW_FNV;
}

static int dirtyBeforeCanonical(const EspHudRefreshState* dirty) {
    return dirty != NULL && sizeof(*dirty) == 8U &&
           dirty->reason == ESP_HUD_REFRESH_REASON_POST_SPAWN &&
           dirty->refreshPending == 1U && dirty->routed == 1U &&
           dirty->active == 1U && dirty->targetMapId == 9U &&
           dirty->gameplayLoadMapId == 2U && dirty->loadType == 0U &&
           dirty->reserved == 0U;
}

static int dirtyAfterCanonical(const EspHudRefreshState* dirty) {
    return dirty != NULL && sizeof(*dirty) == 8U &&
           dirty->reason == ESP_HUD_REFRESH_REASON_POST_SPAWN &&
           dirty->refreshPending == 0U && dirty->routed == 1U &&
           dirty->active == 1U && dirty->targetMapId == 9U &&
           dirty->gameplayLoadMapId == 2U && dirty->loadType == 0U &&
           dirty->reserved == 0U;
}

static int clearCanonical(const EspHudPostLoadClearState* clear) {
    return clear != NULL && sizeof(*clear) == 8U &&
           clear->targetMapId == 9U && clear->gameplayLoadMapId == 2U &&
           clear->loadType == 0U && clear->messageCount == 0U &&
           clear->statBarMessagePresent == 0U && clear->logMessageLength == 0U &&
           clear->cleared == 1U && clear->active == 1U;
}

static int modelCanonical(const EspNativeGameplayHudModel* model) {
    return model != NULL && model->targetMapId == 9U &&
           model->gameplayLoadMapId == 2U && model->loadType == 0U &&
           model->health == 30U && model->maxHealth == 30U &&
           model->armor == 0U && model->maxArmor == 20U &&
           model->ammo == 8U && model->weapon == 2U && model->ammoType == 1U &&
           model->weaponsPresent == 1U && model->destAngle == 64U &&
           model->damageActive == 0U && model->damageDir == 0U &&
           model->gotFace == 0U && model->messageCount == 0U &&
           model->statBarMessagePresent == 0U && model->logMessageLength == 0U;
}

static int stateCanonical(const EspNativeGameplayHudState* state) {
    return state != NULL && modelCanonical(&state->model) &&
           state->faceState == 0U && state->painted == 1U &&
           state->active == 1U && state->reserved == 0U;
}

static int stateZero(const EspNativeGameplayHudState* state) {
    EspNativeGameplayHudState zero;
    if (state == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(state, &zero, sizeof(zero)) == 0;
}

static int legacyInitialHudModel(const DoomRPG_t* doomRpg,
                                 EspNativeGameplayHudModel* out) {
    const Player_t* player;
    const Combat_t* combat;
    const Hud_t* hud;
    const EspPlayerViewState* view;

    if (out != NULL) memset(out, 0, sizeof(*out));
    if (doomRpg == NULL || out == NULL || doomRpg->player == NULL ||
        doomRpg->combat == NULL || doomRpg->hud == NULL) return 0;
    player = doomRpg->player;
    combat = doomRpg->combat;
    hud = doomRpg->hud;
    view = EspPlayerView_view();
    if (!playerViewCanonical(view) ||
        CombatEntity_getHealth((CombatEntity_t*)&player->ce) != 30 ||
        CombatEntity_getMaxHealth((CombatEntity_t*)&player->ce) != 30 ||
        CombatEntity_getArmor((CombatEntity_t*)&player->ce) != 0 ||
        CombatEntity_getMaxArmor((CombatEntity_t*)&player->ce) != 20 ||
        player->ammo[1] != 8U || player->weapon != 2 || player->weapons != 4 ||
        combat->weaponInfo[2].ammoType != 1 ||
        hud->damageTime != 0 || hud->damageCount != 0 || hud->gotFaceTime != 0 ||
        hud->msgCount != 0 || hud->statBarMessage != NULL || hud->logMessage[0] != '\0') {
        return 0;
    }

    out->targetMapId = 9U;
    out->gameplayLoadMapId = 2U;
    out->loadType = 0U;
    out->health = 30U;
    out->maxHealth = 30U;
    out->armor = 0U;
    out->maxArmor = 20U;
    out->ammo = 8U;
    out->weapon = 2U;
    out->ammoType = 1U;
    out->weaponsPresent = 1U;
    out->destAngle = view->destAngle;
    return modelCanonical(out);
}

void Esp32JunctionGameplayHudProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspNativeGameplayHud_reset();
}

int Esp32JunctionGameplayHudProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionGameplayHudProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    Render_t* render;
    EspNativeGameplayHudModel model;
    EspNativeGameplayHudModel badModel;
    EspNativeGameplayHudState scratch;
    EspNativeGameplayHudState prepared;
    EspNativeGameplayHudStats stats;
    EspNativeGameplayHudStats repeatStats;
    EspMapResidentSnapshot residentBefore;
    EspMapResidentSnapshot residentAfter;
    const EspNativeGraphicsCatalogView* catalog;
    const EspMapSpriteTopologyView* topology;
    uint32_t frameBefore;
    uint32_t frameAfter;
    uint32_t viewportBefore;
    uint32_t viewportAfter;
    uint32_t bandsBefore;
    uint32_t bandsAfter;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t hudBefore;
    uint32_t hudAfter;
    uint32_t playerBefore;
    uint32_t playerAfter;
    uint32_t gameBefore;
    uint32_t gameAfter;
    uint32_t canvasBefore;
    uint32_t canvasAfter;
    uint32_t renderBefore;
    uint32_t renderAfter;
    uint32_t stateFNV;
    uint32_t dirtyBeforeFNV;
    uint32_t dirtyAfterFNV;
    int nullGate;
    int badAngleGate;
    int messageGate;
    int prepareAtomic;
    int repeatGate;
    int repeatAtomic;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionSpriteFidelityProbe_postOverlayDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[JUNCTIONHUDPAINTPROBE] ARMED complete hardware-proven world+sprite+glow frame; native gameplay HUD paint starts on next loop service\n");
        return;
    }
    probeState.attempted = 1;

    printf("\n=== Doom RPG ESP32-native Junction initial gameplay HUD ===\n");
    printf("[JUNCTIONHUDPAINTPROBE] CONTRACT reproduce current-pose legacy Hud_drawTopBar/Hud_drawBottomBar as one native 160x120 painter: empty tiled top bar plus compact bottom bar 30HP/0 armor/pistol ammo8/normal face/N; assets a/k/l/m/o come only from DoomRPG-ESP32.pak via bounded indexed row reads; consume the existing 8B HUD dirty intent only after paint; preserve the 160x80 gameplay viewport, all native world owners and every legacy Game/Player/Hud/DoomCanvas/Render field; no input, turn, gameplay dispatch, legacy renderer or runtime ZIP\n");

    render = doomRpg != NULL ? doomRpg->render : NULL;
    catalog = EspNativeGraphicsCatalog_view();
    topology = EspMapSpriteTopology_view();
    frameBefore = frameFNV(render);
    viewportBefore = viewportFNV(render);
    bandsBefore = hudBandsFNV(render);

    if (doomRpg == NULL || doomRpg->game == NULL || doomRpg->player == NULL ||
        doomRpg->combat == NULL || doomRpg->hud == NULL ||
        doomRpg->doomCanvas == NULL || render == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO || doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        frameBefore != EXPECTED_PRE_HUD_FRAME_FNV ||
        viewportBefore != EXPECTED_VIEWPORT_FNV || bandsBefore == 0U ||
        !legacyGraphicsClear(render) || EspAssetPack_isOpen() ||
        !dirtyBeforeCanonical(EspHudRefresh_view()) ||
        !clearCanonical(EspHudPostLoadClear_view()) ||
        !playerViewCanonical(EspPlayerView_view()) ||
        catalog == NULL || catalog->stateFNV1a != EXPECTED_CLOSED_CATALOG_FNV ||
        catalog->textureCount != 30U || catalog->spriteCount != 17U ||
        catalog->storageBytes != 1880U ||
        topology == NULL || topology->stateFNV1a != EXPECTED_TOPOLOGY_FNV ||
        !legacyInitialHudModel(doomRpg, &model) ||
        !EspMapResidentLifecycle_capture(&residentBefore)) {
        printf("[JUNCTIONHUDPAINTPROBE] FAILED unsafe predecessor frame=%08x viewport=%08x dirty=%p clear=%p view=%p catalog=%08x topology=%08x entities=%d monsters=%d pack=%d legacyClear=%d\n",
               (unsigned int)frameBefore, (unsigned int)viewportBefore,
               (const void*)EspHudRefresh_view(),
               (const void*)EspHudPostLoadClear_view(),
               (const void*)EspPlayerView_view(),
               (unsigned int)(catalog ? catalog->stateFNV1a : 0U),
               (unsigned int)(topology ? topology->stateFNV1a : 0U),
               doomRpg && doomRpg->game ? doomRpg->game->numEntities : -1,
               doomRpg && doomRpg->game ? doomRpg->game->numMonsters : -1,
               EspAssetPack_isOpen(), legacyGraphicsClear(render));
        return;
    }

    memset(&scratch, 0xa5, sizeof(scratch));
    nullGate = EspNativeGameplayHud_prepareInitial(NULL, &scratch) ==
                   ESP_NATIVE_GAMEPLAY_HUD_INVALID &&
               stateZero(&scratch);
    badModel = model;
    badModel.destAngle = 1U;
    memset(&scratch, 0xa5, sizeof(scratch));
    badAngleGate = EspNativeGameplayHud_prepareInitial(&badModel, &scratch) ==
                       ESP_NATIVE_GAMEPLAY_HUD_UNSUPPORTED_CONTEXT &&
                   stateZero(&scratch);
    badModel = model;
    badModel.messageCount = 1U;
    memset(&scratch, 0xa5, sizeof(scratch));
    messageGate = EspNativeGameplayHud_prepareInitial(&badModel, &scratch) ==
                      ESP_NATIVE_GAMEPLAY_HUD_UNSUPPORTED_CONTEXT &&
                  stateZero(&scratch);
    if (EspNativeGameplayHud_prepareInitial(&model, &prepared) !=
            ESP_NATIVE_GAMEPLAY_HUD_OK ||
        !stateCanonical(&prepared) || EspNativeGameplayHud_isReady()) {
        printf("[JUNCTIONHUDPAINTPROBE] FAILED pure preparation gates=%d/%d/%d\n",
               nullGate, badAngleGate, messageGate);
        return;
    }
    prepareAtomic = frameFNV(render) == frameBefore &&
                    viewportFNV(render) == viewportBefore &&
                    dirtyBeforeCanonical(EspHudRefresh_view()) &&
                    !EspNativeGameplayHud_isReady();

    heapBefore = heap8();
    largestBefore = largest8();
    dirtyBeforeFNV = fnv1a(EspHudRefresh_peek(), sizeof(EspHudRefreshState));
    hudBefore = fnv1a(doomRpg->hud, sizeof(*doomRpg->hud));
    playerBefore = fnv1a(doomRpg->player, sizeof(*doomRpg->player));
    gameBefore = fnv1a(doomRpg->game, sizeof(*doomRpg->game));
    canvasBefore = fnv1a(doomRpg->doomCanvas, sizeof(*doomRpg->doomCanvas));
    renderBefore = fnv1a(doomRpg->render, sizeof(*doomRpg->render));

    memset(&stats, 0, sizeof(stats));
    if (EspNativeGameplayHud_routeInitial(&model, &stats) !=
            ESP_NATIVE_GAMEPLAY_HUD_OK) {
        printf("[JUNCTIONHUDPAINTPROBE] FAILED native route reads=%u bytes=%u rows=%u pixels=%u resources=%u pack=%d\n",
               (unsigned int)stats.packReads,
               (unsigned int)stats.bytesRead,
               (unsigned int)stats.rowsRead,
               (unsigned int)stats.pixelsWritten,
               (unsigned int)stats.resourcesValidated,
               EspAssetPack_isOpen());
        return;
    }

    heapAfter = heap8();
    largestAfter = largest8();
    frameAfter = frameFNV(render);
    viewportAfter = viewportFNV(render);
    bandsAfter = hudBandsFNV(render);
    dirtyAfterFNV = fnv1a(EspHudRefresh_peek(), sizeof(EspHudRefreshState));
    hudAfter = fnv1a(doomRpg->hud, sizeof(*doomRpg->hud));
    playerAfter = fnv1a(doomRpg->player, sizeof(*doomRpg->player));
    gameAfter = fnv1a(doomRpg->game, sizeof(*doomRpg->game));
    canvasAfter = fnv1a(doomRpg->doomCanvas, sizeof(*doomRpg->doomCanvas));
    renderAfter = fnv1a(doomRpg->render, sizeof(*doomRpg->render));
    stateFNV = fnv1a(EspNativeGameplayHud_view(), sizeof(EspNativeGameplayHudState));
    catalog = EspNativeGraphicsCatalog_view();
    topology = EspMapSpriteTopology_view();

    if (!nullGate || !badAngleGate || !messageGate || !prepareAtomic ||
        !stateCanonical(EspNativeGameplayHud_view()) || stateFNV == 0U ||
        !EspHudRefresh_isPaintConsumed() || EspHudRefresh_isReady() ||
        !dirtyAfterCanonical(EspHudRefresh_peek()) ||
        dirtyBeforeFNV == 0U || dirtyAfterFNV == 0U ||
        dirtyBeforeFNV == dirtyAfterFNV ||
        frameAfter == 0U || frameAfter == frameBefore ||
        viewportAfter != EXPECTED_VIEWPORT_FNV || viewportAfter != viewportBefore ||
        bandsAfter == 0U || bandsAfter == bandsBefore ||
        stats.resourcesValidated != 5U || stats.statusBarHeight != 20U ||
        stats.statusBarWidth == 0U || stats.iconWidth == 0U ||
        stats.iconHeight == 0U || stats.faceWidth == 0U ||
        stats.faceHeight == 0U || stats.faceState != 0U ||
        stats.packReads == 0U || stats.bytesRead == 0U ||
        stats.rowsRead == 0U || stats.pixelsWritten == 0U ||
        heapAfter != heapBefore || largestAfter != largestBefore ||
        EspAssetPack_isOpen() || !legacyGraphicsClear(render) ||
        hudAfter != hudBefore || playerAfter != playerBefore ||
        gameAfter != gameBefore || canvasAfter != canvasBefore ||
        renderAfter != renderBefore ||
        catalog == NULL || catalog->stateFNV1a != EXPECTED_CLOSED_CATALOG_FNV ||
        topology == NULL || topology->stateFNV1a != EXPECTED_TOPOLOGY_FNV ||
        !EspMapResidentLifecycle_capture(&residentAfter) ||
        memcmp(&residentBefore, &residentAfter, sizeof(residentBefore)) != 0 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0) {
        printf("[JUNCTIONHUDPAINTPROBE] FAILED post frame=%08x->%08x viewport=%08x->%08x bands=%08x->%08x state=%08x dirty=%08x->%08x gates=%d/%d/%d/%d resources=%u dims=%ux%u icon=%ux%u face=%ux%u/%u io=%uR/%uB/%urows/%upx heap=%u->%u largest=%u->%u legacy=%d/%d/%d/%d/%d pack=%d\n",
               (unsigned int)frameBefore, (unsigned int)frameAfter,
               (unsigned int)viewportBefore, (unsigned int)viewportAfter,
               (unsigned int)bandsBefore, (unsigned int)bandsAfter,
               (unsigned int)stateFNV,
               (unsigned int)dirtyBeforeFNV, (unsigned int)dirtyAfterFNV,
               nullGate, badAngleGate, messageGate, prepareAtomic,
               (unsigned int)stats.resourcesValidated,
               (unsigned int)stats.statusBarWidth,
               (unsigned int)stats.statusBarHeight,
               (unsigned int)stats.iconWidth,
               (unsigned int)stats.iconHeight,
               (unsigned int)stats.faceWidth,
               (unsigned int)stats.faceHeight,
               (unsigned int)stats.faceState,
               (unsigned int)stats.packReads,
               (unsigned int)stats.bytesRead,
               (unsigned int)stats.rowsRead,
               (unsigned int)stats.pixelsWritten,
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)largestBefore, (unsigned int)largestAfter,
               hudAfter == hudBefore, playerAfter == playerBefore,
               gameAfter == gameBefore, canvasAfter == canvasBefore,
               renderAfter == renderBefore, EspAssetPack_isOpen());
        return;
    }

    memset(&repeatStats, 0xa5, sizeof(repeatStats));
    repeatGate = EspNativeGameplayHud_routeInitial(&model, &repeatStats) ==
                 ESP_NATIVE_GAMEPLAY_HUD_ALREADY_ACTIVE;
    repeatAtomic = frameFNV(render) == frameAfter &&
                   viewportFNV(render) == viewportAfter &&
                   fnv1a(EspNativeGameplayHud_view(), sizeof(EspNativeGameplayHudState)) == stateFNV &&
                   dirtyAfterCanonical(EspHudRefresh_peek()) &&
                   !EspAssetPack_isOpen();
    if (!repeatGate || !repeatAtomic) {
        printf("[JUNCTIONHUDPAINTPROBE] FAILED repeat gate=%d atomic=%d frame=%08x/%08x pack=%d\n",
               repeatGate, repeatAtomic,
               (unsigned int)frameFNV(render), (unsigned int)frameAfter,
               EspAssetPack_isOpen());
        return;
    }

    if (!Esp32PlatformVideo_present()) {
        printf("[JUNCTIONHUDPAINTPROBE] FAILED present frame=%08x\n",
               (unsigned int)frameAfter);
        return;
    }

    printf("[JUNCTIONHUDPAINT] READY stateBytes=%u stateFNV=%08x frame=%08x->%08x viewport=%08x preserved=yes bands=%08x->%08x hp=%u/%u armor=%u/%u weapon=%u ammoType=%u ammo=%u face=%u dir=N\n",
           (unsigned int)sizeof(EspNativeGameplayHudState),
           (unsigned int)stateFNV,
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           (unsigned int)viewportAfter,
           (unsigned int)bandsBefore, (unsigned int)bandsAfter,
           (unsigned int)model.health, (unsigned int)model.maxHealth,
           (unsigned int)model.armor, (unsigned int)model.maxArmor,
           (unsigned int)model.weapon, (unsigned int)model.ammoType,
           (unsigned int)model.ammo, (unsigned int)stats.faceState);
    printf("[JUNCTIONHUDPAINT] ASSETS validated=%u bar=%ux%u icon=%ux%u face=%ux%u reads=%u bytes=%u rows=%u pixels=%u packClosed=yes\n",
           (unsigned int)stats.resourcesValidated,
           (unsigned int)stats.statusBarWidth,
           (unsigned int)stats.statusBarHeight,
           (unsigned int)stats.iconWidth,
           (unsigned int)stats.iconHeight,
           (unsigned int)stats.faceWidth,
           (unsigned int)stats.faceHeight,
           (unsigned int)stats.packReads,
           (unsigned int)stats.bytesRead,
           (unsigned int)stats.rowsRead,
           (unsigned int)stats.pixelsWritten);
    printf("[JUNCTIONHUDPAINT] MEMORY heap=%u->%u largest=%u->%u delta=0 dirty=%08x->%08x consumed=yes legacyHudStable=yes playerStable=yes gameStable=yes canvasStable=yes renderStable=yes residentStable=yes topology=%08x catalog=%08x\n",
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter,
           (unsigned int)dirtyBeforeFNV, (unsigned int)dirtyAfterFNV,
           (unsigned int)topology->stateFNV1a,
           (unsigned int)catalog->stateFNV1a);
    printf("[JUNCTIONHUDPAINT] PARK nativeHud=yes hudPending=no worldViewportPreserved=yes glowCompanions=yes gameplayDispatchPending=yes legacyState=%d entities=%d monsters=%d noGameplay=yes presented=1\n",
           doomRpg->doomCanvas->state,
           doomRpg->game->numEntities,
           doomRpg->game->numMonsters);
    probeState.done = 1;
}
