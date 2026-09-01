#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include <esp_heap_caps.h>
#include <esp_system.h>

#include "esp_asset_pack.h"
#include "esp_native_first_frame.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_session.h"
#include "esp_native_graphics_catalog.h"
#include "esp_native_resident_gameplay.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"

#define SESSION_STAGE_IDLE 0U
#define SESSION_STAGE_FIRST_FRAME 1U
#define SESSION_STAGE_HUD 2U
#define SESSION_STAGE_DEPENDENCIES 3U
#define SESSION_STAGE_SMALL_BEGIN 4U
#define SESSION_STAGE_SMALL_COLD 5U
#define SESSION_STAGE_SMALL_WARM 6U
#define SESSION_STAGE_LARGE_BEGIN 7U
#define SESSION_STAGE_LARGE_LEARN 8U
#define SESSION_STAGE_LARGE_WARM 9U
#define SESSION_STAGE_GAMEPLAY 10U
#define SESSION_STAGE_ACTIVE 11U
#define SESSION_STAGE_FAILED 255U

/* Hardware-budget guardrails for this A/B only. They are deliberately
 * advisory, not gameplay gates or architectural invariants. DMA-capable memory
 * overlaps the 8-bit heap on ESP32, so these figures must never be summed with
 * the capability snapshots as if they were independent pools. */
#define RESIDENT_OWNER_MAIN_BASELINE_BYTES 21160U
#define RAM_AUDIO_I2S_DMA_RESERVE_BYTES (16U * 1024U)
#define RAM_AUDIO_BUFFER_RESERVE_BYTES (8U * 1024U)
#define RAM_GENERAL_HEADROOM_RESERVE_BYTES (32U * 1024U)
#define RAM_TOTAL_8BIT_RESERVE_BYTES \
    (RAM_AUDIO_I2S_DMA_RESERVE_BYTES + RAM_AUDIO_BUFFER_RESERVE_BYTES + \
     RAM_GENERAL_HEADROOM_RESERVE_BYTES)
#define RAM_LARGEST_8BIT_TARGET_BYTES (16U * 1024U)
#define RAM_LARGEST_DMA_TARGET_BYTES (8U * 1024U)

typedef struct EspRamBudgetSnapshot_s {
    uint32_t heapFree;
    uint32_t heap8Free;
    uint32_t largest8;
    uint32_t dmaFree;
    uint32_t largestDma;
} EspRamBudgetSnapshot;

typedef struct EspNativeGameplaySessionState_s {
    EspNativeGameplaySessionConfig config;
    EspRamBudgetSnapshot gameplayBudgetBefore;
    uint8_t configured;
    uint8_t stage;
    uint8_t failed;
    uint8_t gameplayBudgetStarted;
} EspNativeGameplaySessionState;

static EspNativeGameplaySessionState sessionState;

static uint32_t heap8(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static EspRamBudgetSnapshot ramBudgetSnapshot(void) {
    EspRamBudgetSnapshot snapshot;
    snapshot.heapFree = (uint32_t)esp_get_free_heap_size();
    snapshot.heap8Free = heap8();
    snapshot.largest8 = largest8();
    snapshot.dmaFree = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_DMA);
    snapshot.largestDma =
        (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    return snapshot;
}

static uint32_t consumedBytes(uint32_t before, uint32_t after) {
    return before >= after ? before - after : 0U;
}

static uint32_t reserveMargin(uint32_t freeBytes, uint32_t reserveBytes) {
    return freeBytes >= reserveBytes ? freeBytes - reserveBytes : 0U;
}

static void printRamBudgetSnapshot(const char* label,
                                   const EspRamBudgetSnapshot* snapshot) {
    if (label == NULL || snapshot == NULL) return;
    printf("[RAMBUDGET] %s heap=%u heap8=%u largest8=%u dma=%u largestDMA=%u\n",
           label,
           (unsigned int)snapshot->heapFree,
           (unsigned int)snapshot->heap8Free,
           (unsigned int)snapshot->largest8,
           (unsigned int)snapshot->dmaFree,
           (unsigned int)snapshot->largestDma);
}

static void failSessionAt(uint8_t stage, const char* reason) {
    if (EspAssetPack_isOpen()) EspAssetPack_close();
    sessionState.failed = 1U;
    sessionState.stage = SESSION_STAGE_FAILED;
    printf("[ENGINESESSION] FAILED stage=%u reason=%s resident=%u large=%u packOpen=%u\n",
           (unsigned int)stage,
           reason != NULL ? reason : "unknown",
           (unsigned int)EspAssetPack_isResident(),
           (unsigned int)EspAssetPack_isResidentLargeRangeEnabled(),
           (unsigned int)EspAssetPack_isOpen());
}

static void failSession(const char* reason) {
    failSessionAt(sessionState.stage, reason);
}

static int settledView(const EspPlayerViewState* view) {
    return view != NULL && view->active == 1U &&
           view->viewAngle == view->destAngle &&
           (view->viewAngle & 63) == 0;
}

static void printPackFrame(const char* label,
                           const EspNativeGameplayFrameStats* frame,
                           const EspAssetPackResidentStats* pack) {
    if (label == NULL || frame == NULL || pack == NULL) return;
    printf("[ENGINECACHE] %s physical=%u/%uB entry=%uH/%uM range=%uH/%uM/%uS/%uB cache=%u/%uB entries=%u/%u large=%u time=%u/%u/%u/%u total=%u frame=%08x\n",
           label,
           (unsigned int)pack->physicalReads,
           (unsigned int)pack->physicalBytes,
           (unsigned int)pack->entryCacheHits,
           (unsigned int)pack->entryCacheMisses,
           (unsigned int)pack->rangeCacheHits,
           (unsigned int)pack->rangeCacheMisses,
           (unsigned int)pack->rangeCacheStores,
           (unsigned int)pack->rangeCacheBypasses,
           (unsigned int)pack->rangeCacheBytesUsed,
           (unsigned int)pack->rangeCacheCapacityBytes,
           (unsigned int)pack->rangeCacheEntries,
           (unsigned int)pack->rangeCacheEntryCapacity,
           (unsigned int)pack->largeRangeEntries,
           (unsigned int)frame->worldMicros,
           (unsigned int)frame->spriteMicros,
           (unsigned int)frame->hudMicros,
           (unsigned int)frame->presentMicros,
           (unsigned int)frame->totalMicros,
           (unsigned int)frame->frameAfterFNV);
}

static int renderCacheWitness(struct DoomRPG_s* doomRpgBase,
                              const EspPlayerViewState* view,
                              const char* label,
                              EspNativeGameplayFrameStats* outFrame,
                              EspAssetPackResidentStats* outPack) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    if (doomRpg == NULL || doomRpg->render == NULL || !settledView(view) ||
        label == NULL || outFrame == NULL || outPack == NULL) {
        return 0;
    }
    memset(outFrame, 0, sizeof(*outFrame));
    memset(outPack, 0, sizeof(*outPack));
    EspAssetPack_residentResetStats();
    if (!EspNativeGameplayFrame_renderTurn(
            doomRpg->render, (uint8_t)view->viewAngle, outFrame)) {
        return 0;
    }
    EspAssetPack_residentGetStats(outPack);
    printPackFrame(label, outFrame, outPack);
    return outFrame->active && outFrame->finalPresented &&
           EspAssetPack_isResident() && !EspAssetPack_isOpen();
}

void EspNativeGameplaySession_reset(void) {
    EspNativeResidentGameplay_reset();
    EspNativeGameplayHud_reset();
    EspNativeFirstFrame_reset();
    EspNativeGraphicsCatalog_reset();

    if (EspAssetPack_isOpen()) EspAssetPack_close();
    if (!EspAssetPack_isOpen() && EspAssetPack_isResident()) {
        (void)EspAssetPack_residentEnd();
    }
    memset(&sessionState, 0, sizeof(sessionState));
}

int EspNativeGameplaySession_configure(
    const EspNativeGameplaySessionConfig* config) {
    if (config == NULL || sessionState.configured || sessionState.failed ||
        config->maxHealth == 0U || config->health > config->maxHealth ||
        config->armor > config->maxArmor) {
        return 0;
    }
    sessionState.config = *config;
    sessionState.configured = 1U;
    sessionState.stage = SESSION_STAGE_FIRST_FRAME;
    return 1;
}

int EspNativeGameplaySession_isActive(void) {
    return sessionState.stage == SESSION_STAGE_ACTIVE &&
           sessionState.failed == 0U &&
           EspNativeResidentGameplay_isActive();
}

int EspNativeGameplaySession_hasFailed(void) {
    return sessionState.failed != 0U;
}

void EspNativeGameplaySession_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspPlayerViewState* view;

    if (!sessionState.configured || sessionState.failed) return;
    if (doomRpg == NULL || doomRpg->render == NULL) {
        failSession("missing DoomRPG/render owner");
        return;
    }

    view = EspPlayerView_view();
    if (!settledView(view)) return;

    /* A level load pays cache priming exactly once. Gameplay input is armed only
     * after the PR #96/#97 hardware-proven order has completed:
     * small cold -> small warm -> enable 2048 B tail -> learn -> warm. */
    for (;;) {
        if (sessionState.stage == SESSION_STAGE_FIRST_FRAME) {
            EspNativeGraphicsCatalogStatus catalogStatus;
            EspNativeFirstFrameStatus frameStatus;
            const EspNativeFirstFrameState* frame;
            const EspNativeGraphicsCatalogView* catalog;

            if (!EspNativeGraphicsCatalog_isReady()) {
                catalogStatus = EspNativeGraphicsCatalog_buildFromRuntime();
                if (catalogStatus != ESP_NATIVE_GRAPHICS_CATALOG_OK &&
                    catalogStatus != ESP_NATIVE_GRAPHICS_CATALOG_ALREADY_ACTIVE) {
                    failSession("graphics catalog build");
                    return;
                }
            }
            catalog = EspNativeGraphicsCatalog_view();
            if (catalog == NULL) {
                failSession("graphics catalog missing");
                return;
            }

            printf("\n=== Doom RPG ESP32-native generic gameplay session ===\n");
            printf("[ENGINESESSION] CONTRACT current resident runtime + EspPlayerView select map/pose; generic graphics, HUD, small cache cold/warm, shared-payload 2048B learn/warm, then collision/input/gameplay. Historical Junction/Entrance probes are regression witnesses, never runtime prerequisites.\n");
            printf("[ENGINESESSION] CATALOG map=%u textures=%u sprites=%u storage=%u fnv=%08x\n",
                   (unsigned int)view->targetMapId,
                   (unsigned int)catalog->textureCount,
                   (unsigned int)catalog->spriteCount,
                   (unsigned int)catalog->storageBytes,
                   (unsigned int)catalog->stateFNV1a);

            frameStatus = EspNativeFirstFrame_route(doomRpg->render, view);
            if (frameStatus != ESP_NATIVE_FIRST_FRAME_OK) {
                failSession("first resident frame");
                return;
            }
            frame = EspNativeFirstFrame_view();
            if (frame == NULL || frame->rendered != 1U || frame->presented != 1U) {
                failSession("first frame publication");
                return;
            }
            printf("[ENGINESESSION] FIRST_FRAME map=%u angle=%u frame=%08x walls=%u pixels=%u presented=1\n",
                   (unsigned int)view->targetMapId,
                   (unsigned int)view->viewAngle,
                   (unsigned int)frame->frameAfterFNV,
                   (unsigned int)frame->wallDraws,
                   (unsigned int)frame->pixelsDrawn);
            sessionState.stage = SESSION_STAGE_HUD;
            continue;
        }

        if (sessionState.stage == SESSION_STAGE_HUD) {
            EspNativeGameplayHudModel model;
            EspNativeGameplayHudStats stats;
            EspNativeGameplayHudStatus status;

            memset(&model, 0, sizeof(model));
            memset(&stats, 0, sizeof(stats));
            model.targetMapId = view->targetMapId;
            model.gameplayLoadMapId = view->gameplayLoadMapId;
            model.loadType = view->loadType;
            model.health = sessionState.config.health;
            model.maxHealth = sessionState.config.maxHealth;
            model.armor = sessionState.config.armor;
            model.maxArmor = sessionState.config.maxArmor;
            model.ammo = sessionState.config.ammo;
            model.weapon = sessionState.config.weapon;
            model.ammoType = sessionState.config.ammoType;
            model.weaponsPresent = sessionState.config.weaponsPresent;
            model.destAngle = (uint8_t)view->destAngle;

            status = EspNativeGameplayHud_routeInitial(&model, &stats);
            if (status != ESP_NATIVE_GAMEPLAY_HUD_OK &&
                status != ESP_NATIVE_GAMEPLAY_HUD_ALREADY_ACTIVE) {
                failSession("initial HUD");
                return;
            }
            if (!EspNativeGameplayHud_isReady() ||
                !Esp32PlatformVideo_present()) {
                failSession("initial HUD present");
                return;
            }
            printf("[ENGINESESSION] HUD map=%u hp=%u/%u armor=%u/%u weapon=%u ammo=%u resources=%u pixels=%u reads=%u presented=1\n",
                   (unsigned int)view->targetMapId,
                   (unsigned int)model.health,
                   (unsigned int)model.maxHealth,
                   (unsigned int)model.armor,
                   (unsigned int)model.maxArmor,
                   (unsigned int)model.weapon,
                   (unsigned int)model.ammo,
                   (unsigned int)stats.resourcesValidated,
                   (unsigned int)stats.pixelsWritten,
                   (unsigned int)stats.packReads);
            sessionState.stage = SESSION_STAGE_DEPENDENCIES;
            continue;
        }

        if (sessionState.stage == SESSION_STAGE_DEPENDENCIES) {
            EspNativeGraphicsCatalogStatus status =
                EspNativeGraphicsCatalog_expandSpriteDependencies();
            const EspNativeGraphicsCatalogView* catalog =
                EspNativeGraphicsCatalog_view();
            if ((status != ESP_NATIVE_GRAPHICS_CATALOG_OK &&
                 status != ESP_NATIVE_GRAPHICS_CATALOG_ALREADY_ACTIVE) ||
                catalog == NULL) {
                failSession("sprite dependency closure");
                return;
            }
            printf("[ENGINESESSION] SPRITES dependencyStatus=%d catalogSprites=%u\n",
                   (int)status, (unsigned int)catalog->spriteCount);
            sessionState.stage = SESSION_STAGE_SMALL_BEGIN;
            continue;
        }

        if (sessionState.stage == SESSION_STAGE_SMALL_BEGIN) {
            EspAssetPackResidentStats owner;
            EspRamBudgetSnapshot before = ramBudgetSnapshot();
            EspRamBudgetSnapshot after;
            uint32_t configuredDelta;

            printRamBudgetSnapshot("CACHE_PRE", &before);
            if (EspAssetPack_isOpen() || EspAssetPack_isResident() ||
                !EspAssetPack_residentBegin() || EspAssetPack_isOpen() ||
                !EspAssetPack_isResident() ||
                EspAssetPack_isResidentLargeRangeEnabled()) {
                failSession("resident small-cache begin");
                return;
            }
            after = ramBudgetSnapshot();
            memset(&owner, 0, sizeof(owner));
            EspAssetPack_residentGetStats(&owner);
            configuredDelta =
                owner.ownerBytes >= RESIDENT_OWNER_MAIN_BASELINE_BYTES
                    ? owner.ownerBytes - RESIDENT_OWNER_MAIN_BASELINE_BYTES
                    : 0U;
            printRamBudgetSnapshot("CACHE_POST", &after);
            printf("[RAMBUDGET] CACHE_DELTA mainOwner=%u owner=%u configured=%u observedHeap=%u observed8=%u observedDMA=%u largest8=%u->%u largestDMA=%u->%u\n",
                   (unsigned int)RESIDENT_OWNER_MAIN_BASELINE_BYTES,
                   (unsigned int)owner.ownerBytes,
                   (unsigned int)configuredDelta,
                   (unsigned int)consumedBytes(before.heapFree, after.heapFree),
                   (unsigned int)consumedBytes(before.heap8Free, after.heap8Free),
                   (unsigned int)consumedBytes(before.dmaFree, after.dmaFree),
                   (unsigned int)before.largest8,
                   (unsigned int)after.largest8,
                   (unsigned int)before.largestDma,
                   (unsigned int)after.largestDma);
            printf("[ENGINECACHE] OWNER bytes=%u payload=%u entries=%u heap8=%u->%u largest8=%u->%u large=off physicalOpen=%u validate=%u\n",
                   (unsigned int)owner.ownerBytes,
                   (unsigned int)owner.rangeCacheCapacityBytes,
                   (unsigned int)owner.rangeCacheEntryCapacity,
                   (unsigned int)before.heap8Free,
                   (unsigned int)after.heap8Free,
                   (unsigned int)before.largest8,
                   (unsigned int)after.largest8,
                   (unsigned int)owner.physicalOpens,
                   (unsigned int)owner.validationPasses);
            sessionState.stage = SESSION_STAGE_SMALL_COLD;
            continue;
        }

        if (sessionState.stage == SESSION_STAGE_SMALL_COLD) {
            EspNativeGameplayFrameStats frame;
            EspAssetPackResidentStats pack;
            if (!renderCacheWitness(doomRpgBase, view, "SMALL-COLD",
                                    &frame, &pack) ||
                pack.rangeCacheStores == 0U ||
                EspAssetPack_isResidentLargeRangeEnabled()) {
                failSession("small-cache cold frame");
                return;
            }
            sessionState.stage = SESSION_STAGE_SMALL_WARM;
            continue;
        }

        if (sessionState.stage == SESSION_STAGE_SMALL_WARM) {
            EspNativeGameplayFrameStats frame;
            EspAssetPackResidentStats pack;
            if (!renderCacheWitness(doomRpgBase, view, "SMALL-WARM",
                                    &frame, &pack) ||
                pack.rangeCacheHits == 0U || pack.entryCacheHits == 0U ||
                EspAssetPack_isResidentLargeRangeEnabled()) {
                failSession("small-cache warm frame");
                return;
            }
            sessionState.stage = SESSION_STAGE_LARGE_BEGIN;
            continue;
        }

        if (sessionState.stage == SESSION_STAGE_LARGE_BEGIN) {
            EspAssetPackResidentStats pack;
            uint32_t heapBefore = heap8();
            uint32_t largestBefore = largest8();
            if (!EspAssetPack_residentLargeRangeBegin() ||
                !EspAssetPack_isResidentLargeRangeEnabled() ||
                EspAssetPack_isOpen() || heap8() != heapBefore ||
                largest8() != largestBefore) {
                failSession("large-cache activation");
                return;
            }
            memset(&pack, 0, sizeof(pack));
            EspAssetPack_residentGetStats(&pack);
            printf("[ENGINECACHE] LARGE_READY cache=%u/%uB entries=%u/%u largeEntries=%u ownerDelta=0 heap8=%u largest8=%u\n",
                   (unsigned int)pack.rangeCacheBytesUsed,
                   (unsigned int)pack.rangeCacheCapacityBytes,
                   (unsigned int)pack.rangeCacheEntries,
                   (unsigned int)pack.rangeCacheEntryCapacity,
                   (unsigned int)pack.largeRangeEntries,
                   (unsigned int)heapBefore,
                   (unsigned int)largestBefore);
            sessionState.stage = SESSION_STAGE_LARGE_LEARN;
            continue;
        }

        if (sessionState.stage == SESSION_STAGE_LARGE_LEARN) {
            EspNativeGameplayFrameStats frame;
            EspAssetPackResidentStats pack;
            if (!renderCacheWitness(doomRpgBase, view, "LARGE-LEARN",
                                    &frame, &pack) ||
                !EspAssetPack_isResidentLargeRangeEnabled() ||
                pack.rangeCacheStores == 0U || pack.largeRangeEntries == 0U) {
                failSession("large-cache learn frame");
                return;
            }
            sessionState.stage = SESSION_STAGE_LARGE_WARM;
            continue;
        }

        if (sessionState.stage == SESSION_STAGE_LARGE_WARM) {
            EspNativeGameplayFrameStats frame;
            EspAssetPackResidentStats pack;
            if (!renderCacheWitness(doomRpgBase, view, "LARGE-WARM",
                                    &frame, &pack) ||
                !EspAssetPack_isResidentLargeRangeEnabled() ||
                pack.rangeCacheHits == 0U || pack.entryCacheHits == 0U ||
                pack.largeRangeEntries == 0U) {
                failSession("large-cache warm frame");
                return;
            }
            printf("[ENGINECACHE] PRIMED map=%u angle=%u totalUs=%u largeEntries=%u heap8=%u largest8=%u next=collision+input\n",
                   (unsigned int)view->targetMapId,
                   (unsigned int)view->viewAngle,
                   (unsigned int)frame.totalMicros,
                   (unsigned int)pack.largeRangeEntries,
                   (unsigned int)heap8(),
                   (unsigned int)largest8());
            sessionState.stage = SESSION_STAGE_GAMEPLAY;
            continue;
        }

        if (sessionState.stage == SESSION_STAGE_GAMEPLAY) {
            if (!sessionState.gameplayBudgetStarted) {
                sessionState.gameplayBudgetBefore = ramBudgetSnapshot();
                sessionState.gameplayBudgetStarted = 1U;
                printRamBudgetSnapshot("LAZY_PRE",
                                       &sessionState.gameplayBudgetBefore);
            }

            EspNativeResidentGameplay_service(doomRpgBase);
            if (EspNativeResidentGameplay_isActive()) {
                EspAssetPackResidentStats pack;
                EspRamBudgetSnapshot after = ramBudgetSnapshot();
                const EspRamBudgetSnapshot* before =
                    &sessionState.gameplayBudgetBefore;
                const int reserveHealthy =
                    after.heap8Free >= RAM_TOTAL_8BIT_RESERVE_BYTES &&
                    after.largest8 >= RAM_LARGEST_8BIT_TARGET_BYTES &&
                    after.dmaFree >= RAM_AUDIO_I2S_DMA_RESERVE_BYTES &&
                    after.largestDma >= RAM_LARGEST_DMA_TARGET_BYTES;

                memset(&pack, 0, sizeof(pack));
                EspAssetPack_residentGetStats(&pack);
                printRamBudgetSnapshot("LAZY_POST", &after);
                printf("[RAMBUDGET] LAZY_DELTA heap=%u heap8=%u dma=%u largest8=%u->%u largestDMA=%u->%u cache=%u/%uB entries=%u/%u\n",
                       (unsigned int)consumedBytes(before->heapFree,
                                                   after.heapFree),
                       (unsigned int)consumedBytes(before->heap8Free,
                                                   after.heap8Free),
                       (unsigned int)consumedBytes(before->dmaFree,
                                                   after.dmaFree),
                       (unsigned int)before->largest8,
                       (unsigned int)after.largest8,
                       (unsigned int)before->largestDma,
                       (unsigned int)after.largestDma,
                       (unsigned int)pack.rangeCacheBytesUsed,
                       (unsigned int)pack.rangeCacheCapacityBytes,
                       (unsigned int)pack.rangeCacheEntries,
                       (unsigned int)pack.rangeCacheEntryCapacity);
                printf("[RAMBUDGET] RESERVE audioI2SDMA=%u audioBuffers=%u general=%u total8Target=%u largest8Target=%u largestDMATarget=%u margin8=%u marginDMA=%u advisory=%s\n",
                       (unsigned int)RAM_AUDIO_I2S_DMA_RESERVE_BYTES,
                       (unsigned int)RAM_AUDIO_BUFFER_RESERVE_BYTES,
                       (unsigned int)RAM_GENERAL_HEADROOM_RESERVE_BYTES,
                       (unsigned int)RAM_TOTAL_8BIT_RESERVE_BYTES,
                       (unsigned int)RAM_LARGEST_8BIT_TARGET_BYTES,
                       (unsigned int)RAM_LARGEST_DMA_TARGET_BYTES,
                       (unsigned int)reserveMargin(after.heap8Free,
                                                   RAM_TOTAL_8BIT_RESERVE_BYTES),
                       (unsigned int)reserveMargin(after.dmaFree,
                                                   RAM_AUDIO_I2S_DMA_RESERVE_BYTES),
                       reserveHealthy ? "HEADROOM_OK" : "REVIEW_HEADROOM");

                sessionState.stage = SESSION_STAGE_ACTIVE;
                printf("[ENGINESESSION] READY map=%u angle=%u residentCache=yes largeCache=yes touch=invisible-120ms TURN+MOVE=armed shapeData=%p mediaTexels=%p\n",
                       (unsigned int)view->targetMapId,
                       (unsigned int)view->viewAngle,
                       doomRpg->render != NULL ?
                           (void*)doomRpg->render->shapeData : NULL,
                       doomRpg->render != NULL ?
                           (void*)doomRpg->render->mediaTexels : NULL);
            }
            return;
        }

        if (sessionState.stage == SESSION_STAGE_ACTIVE) {
            EspNativeResidentGameplay_service(doomRpgBase);
            return;
        }

        failSession("invalid session stage");
        return;
    }
}
