#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "Hud.h"
#include "Player.h"
#include "Render.h"

#include <esp_heap_caps.h>

#include "esp_asset_pack.h"
#include "esp_map_automap_state.h"
#include "esp_map_line_state.h"
#include "esp_map_line_texture_state.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "native_map1_change_map_probe.h"
#include "native_map1_show_hide_probe.h"
#include "native_map1_show_hide_probe_internal.h"

#define EXPECTED_ARENA_FNV 0xc3882516U
#define EXPECTED_MAP_STATE_FNV 0xcd99b98eU
#define EXPECTED_SCRIPT_FNV 0xf9e3d9dfU
#define EXPECTED_LINE_STATE_FNV 0xe5e74861U
#define EXPECTED_TEXTURE_STATE_FNV 0xf1fc1875U
#define EXPECTED_AUTOMAP_STATE_FNV 0x669b1aa7U
#define EXPECTED_LEGACY_NOTEBOOK_FNV 0x4d7705c5U
#define EXPECTED_TOPOLOGY_PAYLOAD_BYTES (344U * 7U)
#define EXPECTED_SHOW_RESULT_BYTES 26U
#define EXPECTED_HIDE_RESULT_BYTES 18U
#define MAX_TOPOLOGY_HEAP_OVERHEAD 128U
#define MIN_LARGEST_8BIT_BLOCK 32768U

typedef struct Esp32Map1ShowHideProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1ShowHideProbeState;

static Esp32Map1ShowHideProbeState probeState;

static uint32_t heap8Free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8Block(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static uint32_t hashByte(uint32_t hash, uint8_t value) {
    hash ^= value;
    return hash * 16777619U;
}

static uint32_t fnv1a32(const uint8_t* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (data == NULL && length != 0U) return 0U;
    for (i = 0U; i < length; ++i) hash = hashByte(hash, data[i]);
    return hash;
}

void Esp32Map1ShowHideProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspMapSpriteTopology_reset();
}

int Esp32Map1ShowHideProbe_isDone(void) {
    return probeState.done;
}

void Esp32Map1ShowHideProbe_service(struct DoomRPG_s* doomRpgOpaque) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgOpaque;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    const EspMapAutomapStateView* automapState;
    const EspMapSpriteTopologyView* topology;
    EspAssetPackEntry entityDefsEntry;
    Esp32ShowHideTopologyAudit topologyAudit;
    Esp32ShowHideCorpusAudit audit;
    uint32_t heapBefore;
    uint32_t heapOpen;
    uint32_t heapAfterBuild;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestOpen;
    uint32_t largestAfterBuild;
    uint32_t largestAfter;
    uint32_t frameBefore;
    uint32_t frameAfter;
    uint32_t arenaBefore;
    uint32_t arenaAfter;
    uint32_t mapStateBefore;
    uint32_t mapStateAfter;
    uint32_t scriptBefore;
    uint32_t scriptAfter;
    uint32_t automapBefore;
    uint32_t automapAfter;
    uint32_t notebookBefore;
    uint32_t notebookAfter;
    uint32_t keysBefore;
    uint32_t keysAfter;
    uint32_t hudBefore;
    uint32_t hudAfter;
    uint32_t passwordBefore;
    uint32_t passwordAfter;
    uint32_t continuationBefore;
    uint32_t continuationAfter;
    uint32_t legacyTopologyBefore;
    uint32_t legacyTopologyAfter;
    uint32_t persistentHeapCost;
    uint32_t startedMs;
    uint32_t elapsedMs;

    if (probeState.done || probeState.attempted || doomRpg == NULL) return;
    if (!Esp32Map1ChangeMapProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[MAPSHOWHIDEPROBE] ARMED native CHANGEMAP intent proven; final EV_SHOW/EV_HIDE sprite-topology ownership starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native MAP_INTRO SHOW/HIDE sprite topology ===\n");
    printf("[MAPSHOWHIDEPROBE] CONTRACT build 7B/map-sprite compact classification+visual+tile/link/order owner from immutable BSP + /entities.db; execute only EV_SHOW/EV_HIDE topology/visual mutations, defer blocker gameplay side-effects, reject RNG crate blocker; no legacy Entity/Render/world mutation\n");

    if (!Esp32ShowHideProbe_boundaryIsSafe(doomRpg)) {
        printf("[MAPSHOWHIDEPROBE] FAILED unsafe precondition\n");
        return;
    }

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    automapState = EspMapAutomapState_view();
    if (runtime == NULL || mapState == NULL || scriptState == NULL ||
        automapState == NULL) {
        printf("[MAPSHOWHIDEPROBE] FAILED native prerequisites unavailable\n");
        return;
    }

    heapBefore = heap8Free();
    largestBefore = largest8Block();
    frameBefore = Esp32ShowHideProbe_framebufferHash();
    arenaBefore = runtime->arenaFNV1a;
    mapStateBefore = mapState->stateFNV1a;
    scriptBefore = fnv1a32(scriptState->storage, scriptState->storageBytes);
    automapBefore = automapState->stateFNV1a;
    notebookBefore = fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                              (uint32_t)sizeof(doomRpg->player->NotebookString));
    keysBefore = (uint32_t)doomRpg->player->keys;
    hudBefore = Esp32ShowHideProbe_hudHash(doomRpg->hud);
    passwordBefore = Esp32ShowHideProbe_passwordHash(doomRpg->doomCanvas);
    continuationBefore = Esp32ShowHideProbe_continuationHash(doomRpg->game);
    legacyTopologyBefore = Esp32ShowHideProbe_legacyTopologyHash(doomRpg->game);
    startedMs = DoomRPG_GetUpTimeMS();

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[MAPSHOWHIDEPROBE] FAILED open %s\n", ESP_ASSET_PACK_DEFAULT_PATH);
        return;
    }
    heapOpen = heap8Free();
    largestOpen = largest8Block();
    if (!EspAssetPack_findEntry("/entities.db", &entityDefsEntry) ||
        (entityDefsEntry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        !EspMapSpriteTopology_buildFromRuntime(&entityDefsEntry)) {
        EspAssetPack_close();
        printf("[MAPSHOWHIDEPROBE] FAILED native topology build\n");
        return;
    }
    EspAssetPack_close();
    heapAfterBuild = heap8Free();
    largestAfterBuild = largest8Block();

    topology = EspMapSpriteTopology_view();
    if (topology == NULL ||
        topology->storageBytes != EXPECTED_TOPOLOGY_PAYLOAD_BYTES ||
        sizeof(EspMapShowResult) != EXPECTED_SHOW_RESULT_BYTES ||
        sizeof(EspMapHideResult) != EXPECTED_HIDE_RESULT_BYTES ||
        !Esp32ShowHideProbe_auditInitial(doomRpg, &topologyAudit) ||
        !Esp32ShowHideProbe_auditCorpus(&audit)) {
        printf("[MAPSHOWHIDEPROBE] FAILED topology/corpus audit\n");
        return;
    }

    heapAfter = heap8Free();
    largestAfter = largest8Block();
    elapsedMs = DoomRPG_GetUpTimeMS() - startedMs;
    persistentHeapCost =
        heapBefore >= heapAfterBuild ? heapBefore - heapAfterBuild : 0U;

    frameAfter = Esp32ShowHideProbe_framebufferHash();
    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    automapState = EspMapAutomapState_view();
    topology = EspMapSpriteTopology_view();
    arenaAfter = runtime != NULL ? runtime->arenaFNV1a : 0U;
    mapStateAfter = mapState != NULL ? mapState->stateFNV1a : 0U;
    scriptAfter = scriptState != NULL
                      ? fnv1a32(scriptState->storage, scriptState->storageBytes)
                      : 0U;
    automapAfter = automapState != NULL ? automapState->stateFNV1a : 0U;
    notebookAfter = fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                             (uint32_t)sizeof(doomRpg->player->NotebookString));
    keysAfter = (uint32_t)doomRpg->player->keys;
    hudAfter = Esp32ShowHideProbe_hudHash(doomRpg->hud);
    passwordAfter = Esp32ShowHideProbe_passwordHash(doomRpg->doomCanvas);
    continuationAfter = Esp32ShowHideProbe_continuationHash(doomRpg->game);
    legacyTopologyAfter = Esp32ShowHideProbe_legacyTopologyHash(doomRpg->game);

    if (topology == NULL || topology->stateFNV1a != audit.initialStateFNV ||
        topology->stateFNV1a != topologyAudit.topologyFNV ||
        persistentHeapCost < EXPECTED_TOPOLOGY_PAYLOAD_BYTES ||
        persistentHeapCost >
            EXPECTED_TOPOLOGY_PAYLOAD_BYTES + MAX_TOPOLOGY_HEAP_OVERHEAD ||
        heapAfter != heapAfterBuild || largestAfter != largestAfterBuild ||
        largestAfter < MIN_LARGEST_8BIT_BLOCK ||
        frameAfter != frameBefore || arenaAfter != arenaBefore ||
        arenaAfter != EXPECTED_ARENA_FNV ||
        mapStateAfter != mapStateBefore || mapStateAfter != EXPECTED_MAP_STATE_FNV ||
        scriptAfter != scriptBefore || scriptAfter != EXPECTED_SCRIPT_FNV ||
        automapAfter != automapBefore ||
        automapAfter != EXPECTED_AUTOMAP_STATE_FNV ||
        EspMapLineState_view() == NULL ||
        EspMapLineState_view()->stateFNV1a != EXPECTED_LINE_STATE_FNV ||
        EspMapLineTextureState_view() == NULL ||
        EspMapLineTextureState_view()->stateFNV1a != EXPECTED_TEXTURE_STATE_FNV ||
        notebookAfter != notebookBefore ||
        notebookAfter != EXPECTED_LEGACY_NOTEBOOK_FNV || keysAfter != keysBefore ||
        hudAfter != hudBefore || passwordAfter != passwordBefore ||
        continuationAfter != continuationBefore ||
        legacyTopologyAfter != legacyTopologyBefore || EspAssetPack_isOpen() ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0) {
        printf("[MAPSHOWHIDEPROBE] FAILED integrity regression\n");
        return;
    }

    printf("[MAPTOPOLOGY] READY sprites=%u storageBytes=%u defCount=%u entities=%u hasDef=%u fallback=%u linked=%u hiddenSprites=%u hiddenEntities=%u enemies=%u destructibles=%u nextOrder=%u stateFNV=%08x\n",
           (unsigned int)topology->spriteCount,
           (unsigned int)topology->storageBytes,
           (unsigned int)topology->entityDefCount,
           (unsigned int)topologyAudit.entityCount,
           (unsigned int)topologyAudit.hasDefCount,
           (unsigned int)topologyAudit.fallbackCount,
           (unsigned int)topologyAudit.linkedCount,
           (unsigned int)topology->hiddenCount,
           (unsigned int)topologyAudit.hiddenEntityCount,
           (unsigned int)topologyAudit.enemyCount,
           (unsigned int)topologyAudit.destructibleCount,
           (unsigned int)topology->nextLinkOrder,
           (unsigned int)topology->stateFNV1a);
    printf("[MAPSHOWHIDE] READY refs=%u show=%u hide=%u removable=%u stateExecRefused=%u showMutated=%u hideMutated=%u hideNoMutation=%u showTargetEnt=%u showTargetNoEnt=%u blockersFound=%u blockersRemoved=%u blockerNoops=%u deferredDeaths=%u hideEntities=%u showResultBytes=%u hideResultBytes=%u showResultFNV=%08x hideResultFNV=%08x showStateFNV=%08x hideStateFNV=%08x elapsed=%ums\n",
           (unsigned int)audit.refs,
           (unsigned int)audit.showRefs,
           (unsigned int)audit.hideRefs,
           (unsigned int)audit.removableRefs,
           (unsigned int)audit.stateExecutorRefused,
           (unsigned int)audit.showMutated,
           (unsigned int)audit.hideMutated,
           (unsigned int)audit.hideNoMutation,
           (unsigned int)audit.showTargetEntities,
           (unsigned int)audit.showTargetNoEntity,
           (unsigned int)audit.showBlockersFound,
           (unsigned int)audit.showBlockersRemoved,
           (unsigned int)audit.showBlockerNoops,
           (unsigned int)audit.showDeferredDeaths,
           (unsigned int)audit.hideEntitiesTotal,
           (unsigned int)sizeof(EspMapShowResult),
           (unsigned int)sizeof(EspMapHideResult),
           (unsigned int)audit.showResultFNV,
           (unsigned int)audit.hideResultFNV,
           (unsigned int)audit.showStateFNV,
           (unsigned int)audit.hideStateFNV,
           (unsigned int)elapsedMs);
    printf("[MAPSHOW] SAMPLE cmd=%u event=%u off=%u sprite=%u flags=%02x tile=%u visual=%02x->%02x targetEnt=%u linked=%u->%u blockers=%u removed=%u noops=%u blocker0=%u blocker1=%u effects=%04x handled=%u removeIfHandled=%u\n",
           (unsigned int)audit.showResult.globalCommandIndex,
           (unsigned int)audit.showResult.sourceEventIndex,
           (unsigned int)audit.showResult.sourceCommandOffset,
           (unsigned int)audit.showResult.spriteIndex,
           (unsigned int)audit.showResult.showFlags,
           (unsigned int)audit.showResult.tileIndex,
           (unsigned int)audit.showResult.visualBefore,
           (unsigned int)audit.showResult.visualAfter,
           (unsigned int)audit.showResult.targetHasEntity,
           (unsigned int)audit.showResult.targetLinkedBefore,
           (unsigned int)audit.showResult.targetLinkedAfter,
           (unsigned int)audit.showResult.blockersFound,
           (unsigned int)audit.showResult.blockersRemoved,
           (unsigned int)audit.showResult.blockerNoops,
           (unsigned int)audit.showResult.blocker0SpriteIndex,
           (unsigned int)audit.showResult.blocker1SpriteIndex,
           (unsigned int)audit.showResult.effectFlags,
           (unsigned int)audit.showResult.legacyReturnValue,
           (unsigned int)audit.showResult.removeCommandIfHandled);
    printf("[MAPHIDE] SAMPLE cmd=%u event=%u off=%u tile=%u,%u index=%u hidden=%u first=%u last=%u effects=%02x handled=%u removeIfHandled=%u\n",
           (unsigned int)audit.hideResult.globalCommandIndex,
           (unsigned int)audit.hideResult.sourceEventIndex,
           (unsigned int)audit.hideResult.sourceCommandOffset,
           (unsigned int)audit.hideResult.tileX,
           (unsigned int)audit.hideResult.tileY,
           (unsigned int)audit.hideResult.tileIndex,
           (unsigned int)audit.hideResult.hiddenEntityCount,
           (unsigned int)audit.hideResult.firstHiddenSpriteIndex,
           (unsigned int)audit.hideResult.lastHiddenSpriteIndex,
           (unsigned int)audit.hideResult.effectFlags,
           (unsigned int)audit.hideResult.legacyReturnValue,
           (unsigned int)audit.hideResult.removeCommandIfHandled);
    printf("[MAPSHOWHIDE] STATE initialFNV=%08x rollback=%u/%u showRepeatGuard=%u hideIdempotent=%u reset=%u worldRestored=yes\n",
           (unsigned int)audit.initialStateFNV,
           (unsigned int)audit.rollbackProofs,
           (unsigned int)audit.refs,
           (unsigned int)audit.showRepeatGuard,
           (unsigned int)audit.hideIdempotent,
           (unsigned int)audit.resetProof);
    printf("[MAPSHOWHIDE] FAILCLOSED unsupported=%u badOffset=%u badDescriptor=%u nullDescriptor=%u nullResult=%u randomCrate=guarded targetRelink=guarded stateAtomic=yes\n",
           (unsigned int)audit.unsupportedRefused,
           (unsigned int)audit.badOffsetRefused,
           (unsigned int)audit.badDescriptorRefused,
           (unsigned int)audit.nullDescriptorRefused,
           (unsigned int)audit.nullResultRefused);
    printf("[MAPSHOWHIDEPROBE] IO entityDefs=/entities.db size=%u crc32=%08x heapOpen=%u transientPackCost=%u largestOpen=%u packIO=yes buildOnly=yes executorPackIO=no\n",
           (unsigned int)entityDefsEntry.size,
           (unsigned int)entityDefsEntry.crc32,
           (unsigned int)heapOpen,
           (unsigned int)(heapBefore >= heapOpen ? heapBefore - heapOpen : 0U),
           (unsigned int)largestOpen);
    printf("[MAPSHOWHIDEPROBE] RAM heap8=%u->%u persistentHeapCost=%u payload=%u allocatorOverhead=%u largest8=%u->%u frameFNV=%08x->%08x arenaFNV=%08x->%08x mapStateFNV=%08x->%08x scriptFNV=%08x->%08x automapFNV=%08x->%08x\n",
           (unsigned int)heapBefore,
           (unsigned int)heapAfter,
           (unsigned int)persistentHeapCost,
           (unsigned int)EXPECTED_TOPOLOGY_PAYLOAD_BYTES,
           (unsigned int)(persistentHeapCost - EXPECTED_TOPOLOGY_PAYLOAD_BYTES),
           (unsigned int)largestBefore,
           (unsigned int)largestAfter,
           (unsigned int)frameBefore,
           (unsigned int)frameAfter,
           (unsigned int)arenaBefore,
           (unsigned int)arenaAfter,
           (unsigned int)mapStateBefore,
           (unsigned int)mapStateAfter,
           (unsigned int)scriptBefore,
           (unsigned int)scriptAfter,
           (unsigned int)automapBefore,
           (unsigned int)automapAfter);
    printf("[MAPSHOWHIDEPROBE] LEGACY notebookFNV=%08x->%08x keys=%08x->%08x hudFNV=%08x->%08x passwordCanvasFNV=%08x->%08x continuationFNV=%08x->%08x entityTopologyFNV=%08x->%08x legacyRuntimeClear=yes\n",
           (unsigned int)notebookBefore,
           (unsigned int)notebookAfter,
           (unsigned int)keysBefore,
           (unsigned int)keysAfter,
           (unsigned int)hudBefore,
           (unsigned int)hudAfter,
           (unsigned int)passwordBefore,
           (unsigned int)passwordAfter,
           (unsigned int)continuationBefore,
           (unsigned int)continuationAfter,
           (unsigned int)legacyTopologyBefore,
           (unsigned int)legacyTopologyAfter);
    printf("[MAPSHOWHIDEPROBE] PARK state=%d page=%d nativeArena=yes nativeTileState=yes nativeEventLookup=yes nativeEventDescriptor=yes nativeScriptState=yes nativeFilter=yes nativeOpcodeExec=yes nativeUiIntent=yes nativeStringReader=yes nativeStatusMessageOwner=yes nativeDialogOwner=yes nativeNotebookOwner=yes nativeKeyGate=yes nativePasswordOwner=yes nativeLineState=yes nativeDoorExec=yes nativeLineTextureState=yes nativeUnlockExec=yes nativeAutomapState=yes nativeGiveMapExec=yes nativeSaveRoute=yes nativeChangeMapIntent=yes nativeSpriteTopology=yes nativeShowHideExec=yes topologyBytes=%u showResultBytes=%u hideResultBytes=%u worldMutationProven=yes worldRestored=yes legacyEntityMutation=no framebufferMutation=no entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state,
           doomRpg->doomCanvas->storyPage,
           (unsigned int)topology->storageBytes,
           (unsigned int)sizeof(EspMapShowResult),
           (unsigned int)sizeof(EspMapHideResult),
           doomRpg->game->numEntities,
           doomRpg->game->numMonsters);

    probeState.done = 1;
}
