#include <SDL.h>
#include "DoomRPG.h"

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
#include "esp_map_runtime.h"
#include "esp_native_graphics_catalog.h"
#include "esp_native_playing_service_state.h"
#include "esp_post_load_idle_time_state.h"
#include "native_junction_graphics_catalog_probe.h"
#include "native_junction_playing_service_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_SERVICE_BYTES 12U
#define EXPECTED_SERVICE_FNV 0x4c50b853U
#define EXPECTED_SNAPSHOT_FNV 0xbb714d80U
#define EXPECTED_RUNTIME_FNV 0xbc432a0fU
#define EXPECTED_MAP_FNV 0x8dba0bb4U
#define EXPECTED_SCRIPT_FNV 0xbc9b18ffU
#define EXPECTED_LINE_FNV 0x3658710dU
#define EXPECTED_TEXTURE_STATE_FNV 0x537319adU
#define EXPECTED_AUTOMAP_FNV 0xb699bd75U
#define EXPECTED_TOPOLOGY_FNV 0xd6e8df7dU
#define EXPECTED_IDLE_DELAY_MS 8000
#define EXPECTED_RECORD_BYTES 40U
#define MAX_ALLOCATOR_OVERHEAD 64U

#define PALETTE_RELATION_NONE 0
#define PALETTE_RELATION_NATIVE 1
#define PALETTE_RELATION_LEGACY_RB 2
#define PALETTE_RELATION_AMBIGUOUS 3

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

static uint16_t swapRedBlue565(uint16_t color) {
    return (uint16_t)(((color & 0x001fU) << 11) |
                      (color & 0x07e0U) |
                      ((color & 0xf800U) >> 11));
}

static const char* paletteRelationName(int relation) {
    switch (relation) {
        case PALETTE_RELATION_NATIVE: return "native-direct";
        case PALETTE_RELATION_LEGACY_RB: return "legacy-rb-swapped";
        case PALETTE_RELATION_AMBIGUOUS: return "ambiguous-symmetric";
        default: return "invalid";
    }
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

static int legacyMappingRuntimeClear(const Render_t* render) {
    return render != NULL &&
           render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL &&
           render->mediaSpriteIds == NULL &&
           render->shapeData == NULL && render->mediaTexels == NULL &&
           render->mapTextureTexels == NULL && render->mapSpriteTexels == NULL;
}

static int residentCanonical(const EspMapResidentSnapshot* s) {
    return s != NULL && sizeof(*s) == 96U &&
           s->totalPayloadBytes == 10410U &&
           s->runtimeFNV1a == EXPECTED_RUNTIME_FNV &&
           s->mapStateFNV1a == EXPECTED_MAP_FNV &&
           s->scriptStateFNV1a == EXPECTED_SCRIPT_FNV &&
           s->lineStateFNV1a == EXPECTED_LINE_FNV &&
           s->textureStateFNV1a == EXPECTED_TEXTURE_STATE_FNV &&
           s->automapStateFNV1a == EXPECTED_AUTOMAP_FNV &&
           s->topologyFNV1a == EXPECTED_TOPOLOGY_FNV &&
           s->entityCount == 30U && s->enemyCount == 0U &&
           s->destructibleCount == 3U &&
           hashBytes(s, sizeof(*s)) == EXPECTED_SNAPSHOT_FNV;
}

static int playingServiceCanonical(void) {
    const EspNativePlayingServiceState* state = EspNativePlayingService_view();
    return state != NULL && sizeof(*state) == EXPECTED_SERVICE_BYTES &&
           hashBytes(state, sizeof(*state)) == EXPECTED_SERVICE_FNV &&
           state->nativeState == 3U && state->serviceOrdinal == 1U &&
           state->inputCountBefore == 0U && state->inputConsumed == 0U &&
           state->gameplayDispatched == 0U && state->renderIntent == 1U &&
           state->renderDeferred == 1U && state->presentationDeferred == 1U &&
           state->hudIntent == 1U && state->targetMapId == 9U &&
           state->active == 1U;
}

static int idleCanonical(void) {
    const EspPostLoadIdleTimeState* idle = EspPostLoadIdleTime_view();
    return idle != NULL && idle->active == 1U && idle->targetMapId == 9U &&
           idle->timeBefore >= 0 && idle->idleTimeAfter >= idle->timeBefore &&
           (idle->idleTimeAfter - idle->timeBefore) == EXPECTED_IDLE_DELAY_MS;
}

static int catalogCoverageExact(const EspNativeGraphicsCatalogView* view) {
    uint32_t id;
    uint16_t textures = 0U;
    uint16_t sprites = 0U;
    if (view == NULL) return 0;

    for (id = 0U; id < 256U; ++id) {
        int textureRequired = EspMapRuntime_textureRequired(id) ||
                              EspMapRuntime_planeTextureUsed(id);
        int spriteRequired = EspMapRuntime_spriteRequired(id);
        const EspNativeGraphicsCatalogRecord* texture =
            EspNativeGraphicsCatalog_findTexture((uint16_t)id);
        const EspNativeGraphicsCatalogRecord* sprite =
            EspNativeGraphicsCatalog_findSprite((uint16_t)id);

        if ((texture != NULL) != textureRequired ||
            (sprite != NULL) != spriteRequired) {
            return 0;
        }
        if (texture != NULL) ++textures;
        if (sprite != NULL) ++sprites;
    }

    return textures == view->textureCount && sprites == view->spriteCount;
}

/*
 * palettes.bin raw words are the permanent framebuffer RGB565 representation.
 * Normal esp32-cyd startup currently leaves Render.mediaPalettes in the legacy
 * R/B-swapped representation because the historical bring-up normalization
 * suite is skipped. Accept either globally coherent relation so this probe is
 * valid both for normal firmware and for a diagnostic build that already ran
 * DoomRPG_prepareNativePalette(), without mutating the transitional table.
 */
static int catalogPaletteRelation(const EspNativeGraphicsCatalogView* view,
                                  const Render_t* render) {
    int directPossible = 1;
    int swappedPossible = 1;
    uint32_t group;

    if (view == NULL || render == NULL || render->mediaPalettes == NULL ||
        render->mediaPalettesLength <= 0) return PALETTE_RELATION_NONE;

    for (group = 0U; group < 2U; ++group) {
        const EspNativeGraphicsCatalogRecord* records =
            group == 0U ? view->textures : view->sprites;
        uint16_t count = group == 0U ? view->textureCount : view->spriteCount;
        uint16_t i;

        for (i = 0U; i < count; ++i) {
            uint32_t p;
            if ((uint32_t)records[i].paletteSourceOffset + 15U >=
                (uint32_t)render->mediaPalettesLength) {
                return PALETTE_RELATION_NONE;
            }
            if (group == 0U && (records[i].sourceOffset & 1U) != 0U) {
                return PALETTE_RELATION_NONE;
            }

            for (p = 0U; p < ESP_NATIVE_GRAPHICS_PALETTE_COLORS; ++p) {
                uint16_t nativeColor = records[i].paletteRgb565[p];
                uint16_t legacyColor = (uint16_t)render->mediaPalettes[
                    (uint32_t)records[i].paletteSourceOffset + p];
                if (nativeColor != legacyColor) directPossible = 0;
                if (nativeColor != swapRedBlue565(legacyColor)) {
                    swappedPossible = 0;
                }
                if (!directPossible && !swappedPossible) {
                    return PALETTE_RELATION_NONE;
                }
            }
        }
    }

    if (directPossible && swappedPossible) return PALETTE_RELATION_AMBIGUOUS;
    if (directPossible) return PALETTE_RELATION_NATIVE;
    if (swappedPossible) return PALETTE_RELATION_LEGACY_RB;
    return PALETTE_RELATION_NONE;
}

void Esp32JunctionGraphicsCatalogProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspNativeGraphicsCatalog_reset();
}

int Esp32JunctionGraphicsCatalogProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionGraphicsCatalogProbe_service(struct DoomRPG_s* doomRpg) {
    EspMapResidentSnapshot residentBefore, residentAfter;
    const EspNativeGraphicsCatalogView* view;
    uint32_t heapBefore, heapAfter, largestBefore, largestAfter, heapCost;
    uint32_t frameBefore, frameAfter;
    uint32_t gameBefore, gameAfter, playerBefore, playerAfter;
    uint32_t hudBefore, hudAfter, canvasBefore, canvasAfter;
    uint32_t renderBefore, renderAfter, paletteBefore, paletteAfter;
    uint32_t serviceBefore, serviceAfter, textureFNV, spriteFNV;
    uint32_t stateBeforeRepeat;
    uint16_t firstTexture, lastTexture, firstSprite, lastSprite;
    int preFindEmpty, coverageExact, paletteRelation;
    int repeatGate, repeatAtomic;
    EspNativeGraphicsCatalogStatus status;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionPlayingServiceProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[JUNCTIONGFXCATPROBE] ARMED first native PLAYING service complete; sparse graphics catalog starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native Junction sparse graphics catalog ===\n");
    printf("[JUNCTIONGFXCATPROBE] CONTRACT build only mappings/palettes for resident Junction resource IDs from mappings.bin + palettes.bin in DoomRPG-ESP32.pak; one compact immutable 40B record per required texture/sprite, no texel payload, keep legacy mapping arrays/shapeData/mediaTexels NULL, prove the raw-PAK native RGB565 relation without mutating legacy palettes/framebuffer/gameplay, close PAK before PARK\n");

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL || doomRpg->game == NULL ||
        doomRpg->render == NULL || doomRpg->player == NULL || doomRpg->hud == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO || doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->doomCanvas->numEvents != 0 || doomRpg->game->numEntities != 0 ||
        doomRpg->game->numMonsters != 0 || !legacyMappingRuntimeClear(doomRpg->render) ||
        doomRpg->render->mediaPalettes == NULL || doomRpg->render->mediaPalettesLength <= 0 ||
        EspAssetPack_isOpen() || sizeof(EspNativeGraphicsCatalogRecord) != EXPECTED_RECORD_BYTES ||
        !playingServiceCanonical() || !idleCanonical() ||
        !EspMapResidentLifecycle_capture(&residentBefore) || !residentCanonical(&residentBefore)) {
        printf("[JUNCTIONGFXCATPROBE] FAILED unsafe catalog boundary state=%d page=%d events=%d entities=%d monsters=%d mappingsClear=%d paletteEntries=%d service=%d idle=%d packOpen=%d\n",
               doomRpg && doomRpg->doomCanvas ? doomRpg->doomCanvas->state : -1,
               doomRpg && doomRpg->doomCanvas ? doomRpg->doomCanvas->storyPage : -1,
               doomRpg && doomRpg->doomCanvas ? doomRpg->doomCanvas->numEvents : -1,
               doomRpg && doomRpg->game ? doomRpg->game->numEntities : -1,
               doomRpg && doomRpg->game ? doomRpg->game->numMonsters : -1,
               doomRpg && doomRpg->render ? legacyMappingRuntimeClear(doomRpg->render) : 0,
               doomRpg && doomRpg->render ? doomRpg->render->mediaPalettesLength : -1,
               playingServiceCanonical(), idleCanonical(), EspAssetPack_isOpen());
        return;
    }

    preFindEmpty = EspNativeGraphicsCatalog_view() == NULL &&
                   EspNativeGraphicsCatalog_findTexture(0U) == NULL &&
                   EspNativeGraphicsCatalog_findSprite(0U) == NULL;
    if (!preFindEmpty) {
        printf("[JUNCTIONGFXCATPROBE] FAILED catalog unexpectedly live before build\n");
        return;
    }

    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    gameBefore = hashBytes(doomRpg->game, sizeof(*doomRpg->game));
    playerBefore = hashBytes(doomRpg->player, sizeof(*doomRpg->player));
    hudBefore = hashBytes(doomRpg->hud, sizeof(*doomRpg->hud));
    canvasBefore = hashBytes(doomRpg->doomCanvas, sizeof(*doomRpg->doomCanvas));
    renderBefore = hashBytes(doomRpg->render, sizeof(*doomRpg->render));
    paletteBefore = hashBytes(doomRpg->render->mediaPalettes,
                              (uint32_t)doomRpg->render->mediaPalettesLength * 2U);
    serviceBefore = hashBytes(EspNativePlayingService_view(),
                              sizeof(EspNativePlayingServiceState));

    status = EspNativeGraphicsCatalog_buildFromRuntime();
    view = EspNativeGraphicsCatalog_view();
    if (status != ESP_NATIVE_GRAPHICS_CATALOG_OK || view == NULL ||
        view->textureCount == 0U || view->spriteCount == 0U ||
        view->storageBytes != ((uint32_t)view->textureCount +
                              (uint32_t)view->spriteCount) * EXPECTED_RECORD_BYTES ||
        view->stateFNV1a == 0U || EspAssetPack_isOpen()) {
        printf("[JUNCTIONGFXCATPROBE] FAILED build status=%d textures=%u sprites=%u storage=%u fnv=%08x packOpen=%d\n",
               (int)status,
               view ? (unsigned)view->textureCount : 0U,
               view ? (unsigned)view->spriteCount : 0U,
               view ? (unsigned)view->storageBytes : 0U,
               view ? (unsigned)view->stateFNV1a : 0U,
               EspAssetPack_isOpen());
        return;
    }

    coverageExact = catalogCoverageExact(view);
    paletteRelation = catalogPaletteRelation(view, doomRpg->render);
    if (!coverageExact || paletteRelation == PALETTE_RELATION_NONE ||
        EspNativeGraphicsCatalog_findTexture(0xffffU) != NULL ||
        EspNativeGraphicsCatalog_findSprite(0xffffU) != NULL) {
        printf("[JUNCTIONGFXCATPROBE] FAILED semantic coverage=%d paletteRelation=%s missingTexture=%p missingSprite=%p\n",
               coverageExact, paletteRelationName(paletteRelation),
               (const void*)EspNativeGraphicsCatalog_findTexture(0xffffU),
               (const void*)EspNativeGraphicsCatalog_findSprite(0xffffU));
        return;
    }

    stateBeforeRepeat = view->stateFNV1a;
    repeatGate = EspNativeGraphicsCatalog_buildFromRuntime() ==
                 ESP_NATIVE_GRAPHICS_CATALOG_ALREADY_ACTIVE;
    view = EspNativeGraphicsCatalog_view();
    repeatAtomic = view != NULL && view->stateFNV1a == stateBeforeRepeat;
    if (!repeatGate || !repeatAtomic) {
        printf("[JUNCTIONGFXCATPROBE] FAILED repeat gate=%d atomic=%d\n",
               repeatGate, repeatAtomic);
        return;
    }

    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfter = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    heapCost = heapBefore >= heapAfter ? heapBefore - heapAfter : 0U;
    frameAfter = framebufferHash();
    gameAfter = hashBytes(doomRpg->game, sizeof(*doomRpg->game));
    playerAfter = hashBytes(doomRpg->player, sizeof(*doomRpg->player));
    hudAfter = hashBytes(doomRpg->hud, sizeof(*doomRpg->hud));
    canvasAfter = hashBytes(doomRpg->doomCanvas, sizeof(*doomRpg->doomCanvas));
    renderAfter = hashBytes(doomRpg->render, sizeof(*doomRpg->render));
    paletteAfter = hashBytes(doomRpg->render->mediaPalettes,
                             (uint32_t)doomRpg->render->mediaPalettesLength * 2U);
    serviceAfter = hashBytes(EspNativePlayingService_view(),
                             sizeof(EspNativePlayingServiceState));

    if (!EspMapResidentLifecycle_capture(&residentAfter) ||
        !residentCanonical(&residentAfter) ||
        hashBytes(&residentBefore, sizeof(residentBefore)) !=
            hashBytes(&residentAfter, sizeof(residentAfter)) ||
        frameBefore != frameAfter || gameBefore != gameAfter ||
        playerBefore != playerAfter || hudBefore != hudAfter ||
        canvasBefore != canvasAfter || renderBefore != renderAfter ||
        paletteBefore != paletteAfter || serviceBefore != serviceAfter ||
        serviceAfter != EXPECTED_SERVICE_FNV || EspAssetPack_isOpen() ||
        !legacyMappingRuntimeClear(doomRpg->render) ||
        heapCost < view->storageBytes ||
        heapCost > view->storageBytes + MAX_ALLOCATOR_OVERHEAD) {
        printf("[JUNCTIONGFXCATPROBE] FAILED integrity heapCost=%u storage=%u frame=%d game=%d player=%d hud=%d canvas=%d render=%d palette=%d service=%08x pack=%d mappingsClear=%d\n",
               (unsigned)heapCost, (unsigned)view->storageBytes,
               frameBefore == frameAfter, gameBefore == gameAfter,
               playerBefore == playerAfter, hudBefore == hudAfter,
               canvasBefore == canvasAfter, renderBefore == renderAfter,
               paletteBefore == paletteAfter, (unsigned)serviceAfter,
               EspAssetPack_isOpen(), legacyMappingRuntimeClear(doomRpg->render));
        return;
    }

    textureFNV = hashBytes(view->textures,
                           (uint32_t)view->textureCount * EXPECTED_RECORD_BYTES);
    spriteFNV = hashBytes(view->sprites,
                          (uint32_t)view->spriteCount * EXPECTED_RECORD_BYTES);
    firstTexture = view->textures[0].resourceId;
    lastTexture = view->textures[view->textureCount - 1U].resourceId;
    firstSprite = view->sprites[0].resourceId;
    lastSprite = view->sprites[view->spriteCount - 1U].resourceId;

    printf("[JUNCTIONGFXCAT] READY recordBytes=%u textureCount=%u spriteCount=%u storageBytes=%u heapCost=%u stateFNV=%08x textureFNV=%08x spriteFNV=%08x\n",
           (unsigned)sizeof(EspNativeGraphicsCatalogRecord),
           (unsigned)view->textureCount, (unsigned)view->spriteCount,
           (unsigned)view->storageBytes, (unsigned)heapCost,
           (unsigned)view->stateFNV1a, (unsigned)textureFNV,
           (unsigned)spriteFNV);
    printf("[JUNCTIONGFXCAT] SEMANTIC sparseCoverage=yes paletteRgb565Native=yes legacyPaletteRelation=%s textureRange=%u..%u spriteRange=%u..%u texelPayloadResident=no mapWideTexels=no nativeCatalogPersistent=yes\n",
           paletteRelationName(paletteRelation),
           (unsigned)firstTexture, (unsigned)lastTexture,
           (unsigned)firstSprite, (unsigned)lastSprite);
    printf("[JUNCTIONGFXCAT] INPUT playingServiceBytes=%u playingServiceFNV=%08x unchanged=yes residentSnapshot=%08x coverageExact=%s legacyPaletteFNV=%08x->%08x unchanged=yes\n",
           (unsigned)sizeof(EspNativePlayingServiceState),
           (unsigned)serviceAfter, (unsigned)EXPECTED_SNAPSHOT_FNV,
           coverageExact ? "yes" : "no",
           (unsigned)paletteBefore, (unsigned)paletteAfter);
    printf("[JUNCTIONGFXCAT] FAILCLOSED preFindEmpty=%d repeat=%d repeatAtomic=%s missingTexture=yes missingSprite=yes\n",
           preFindEmpty, repeatGate, repeatAtomic ? "yes" : "no");
    printf("[JUNCTIONGFXCAT] RESIDENT snapshotFNV=%08x->%08x unchanged=yes runtimeFNV=%08x mapFNV=%08x scriptFNV=%08x lineFNV=%08x textureStateFNV=%08x automapFNV=%08x topologyFNV=%08x payload=%u entities=%u enemies=%u destructibles=%u packClosed=yes\n",
           (unsigned)hashBytes(&residentBefore, sizeof(residentBefore)),
           (unsigned)hashBytes(&residentAfter, sizeof(residentAfter)),
           (unsigned)residentAfter.runtimeFNV1a,
           (unsigned)residentAfter.mapStateFNV1a,
           (unsigned)residentAfter.scriptStateFNV1a,
           (unsigned)residentAfter.lineStateFNV1a,
           (unsigned)residentAfter.textureStateFNV1a,
           (unsigned)residentAfter.automapStateFNV1a,
           (unsigned)residentAfter.topologyFNV1a,
           (unsigned)residentAfter.totalPayloadBytes,
           (unsigned)residentAfter.entityCount,
           (unsigned)residentAfter.enemyCount,
           (unsigned)residentAfter.destructibleCount);
    printf("[JUNCTIONGFXCAT] RAM heap8=%u->%u persistentDelta=%u largest8=%u->%u logicalCatalogBytes=%u allocatorOverhead=%u\n",
           (unsigned)heapBefore, (unsigned)heapAfter, (unsigned)heapCost,
           (unsigned)largestBefore, (unsigned)largestAfter,
           (unsigned)view->storageBytes,
           (unsigned)(heapCost - view->storageBytes));
    printf("[JUNCTIONGFXCAT] LEGACY gameFNV=%08x->%08x playerFNV=%08x->%08x hudFNV=%08x->%08x canvasFNV=%08x->%08x renderFNV=%08x->%08x frameFNV=%08x->%08x mediaTexelOffsets=NULL mediaBitShapeOffsets=NULL mediaTexturesIds=NULL mediaSpriteIds=NULL shapeData=NULL mediaTexels=NULL GameMutation=no PlayerMutation=no HudMutation=no DoomCanvasMutation=no RenderMutation=no FrameMutation=no\n",
           (unsigned)gameBefore, (unsigned)gameAfter,
           (unsigned)playerBefore, (unsigned)playerAfter,
           (unsigned)hudBefore, (unsigned)hudAfter,
           (unsigned)canvasBefore, (unsigned)canvasAfter,
           (unsigned)renderBefore, (unsigned)renderAfter,
           (unsigned)frameBefore, (unsigned)frameAfter);
    printf("[JUNCTIONGFXCAT] PARK legacyState=%d page=%d targetMap=9 junctionResident=yes nativeST_PLAYING=yes nativePlayingService=yes nativeGraphicsCatalog=yes graphicsCatalogPending=no firstFramePending=yes gameplayDispatchPending=yes rendererPending=yes initialSavePersistencePending=yes entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage);

    probeState.done = 1;
}
