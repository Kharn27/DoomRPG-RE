#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_map_runtime.h"
#include "native_committed_transition_probe.h"
#include "native_entrance_startup_route_probe.h"
#include "native_resident_handoff_probe.h"
#include "native_transition_preflight_final_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#include <esp_heap_caps.h>

#define EXPECTED_ENTRANCE_SNAPSHOT_FNV 0xb3811f3dU
#define EXPECTED_ENTRANCE_PAYLOAD_BYTES 17891U
#define EXPECTED_ENTRANCE_RUNTIME_BYTES 14095U
#define EXPECTED_ENTRANCE_SOURCE_BYTES 21823U
#define EXPECTED_ENTRANCE_SOURCE_CRC32 0x623f34e4U
#define EXPECTED_ENTRANCE_RUNTIME_FNV 0xc3882516U
#define EXPECTED_ENTRANCE_MAP_FNV 0xcd99b98eU
#define EXPECTED_ENTRANCE_SCRIPT_FNV 0xf9e3d9dfU
#define EXPECTED_ENTRANCE_LINE_FNV 0xe5e74861U
#define EXPECTED_ENTRANCE_TEXTURE_FNV 0xf1fc1875U
#define EXPECTED_ENTRANCE_AUTOMAP_FNV 0x669b1aa7U
#define EXPECTED_ENTRANCE_TOPOLOGY_FNV 0x3f321e43U

typedef struct Esp32EntranceStartupRouteProbeState_s {
    uint8_t attempted;
    uint8_t done;
} Esp32EntranceStartupRouteProbeState;

static Esp32EntranceStartupRouteProbeState probeState;

static uint32_t fnv1a(const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t frameFNV(void) {
    const void* framebuffer = Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    const size_t expected = (size_t)DOOMRPG_LOGICAL_WIDTH *
                            (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
    if (framebuffer == NULL || bytes != expected) return 0U;
    return fnv1a(framebuffer, (uint32_t)bytes);
}

static uint32_t heap8(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static int legacyRuntimeIsClear(const Render_t* render) {
    return render != NULL &&
           render->nodes == NULL &&
           render->lines == NULL &&
           render->mapSprites == NULL &&
           render->tileEvents == NULL &&
           render->mapByteCode == NULL &&
           render->mapStringsIDs == NULL &&
           render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL &&
           render->mediaSpriteIds == NULL &&
           render->mapTextureTexels == NULL &&
           render->mapSpriteTexels == NULL &&
           render->shapeData == NULL &&
           render->mediaTexels == NULL &&
           render->ioBuffer == NULL;
}

static int snapshotIsCanonicalEntrance(const EspMapResidentSnapshot* s) {
    return s != NULL && sizeof(*s) == 96U &&
           s->totalPayloadBytes == EXPECTED_ENTRANCE_PAYLOAD_BYTES &&
           s->runtimeArenaBytes == EXPECTED_ENTRANCE_RUNTIME_BYTES &&
           s->runtimeFNV1a == EXPECTED_ENTRANCE_RUNTIME_FNV &&
           s->mapStateFNV1a == EXPECTED_ENTRANCE_MAP_FNV &&
           s->scriptStateFNV1a == EXPECTED_ENTRANCE_SCRIPT_FNV &&
           s->lineStateFNV1a == EXPECTED_ENTRANCE_LINE_FNV &&
           s->textureStateFNV1a == EXPECTED_ENTRANCE_TEXTURE_FNV &&
           s->automapStateFNV1a == EXPECTED_ENTRANCE_AUTOMAP_FNV &&
           s->topologyFNV1a == EXPECTED_ENTRANCE_TOPOLOGY_FNV &&
           s->nodeCount == 223U && s->lineCount == 480U &&
           s->spriteCount == 344U && s->eventCount == 93U &&
           s->byteCodeCount == 265U && s->stringCount == 94U &&
           s->entityCount == 220U && s->enemyCount == 30U &&
           s->destructibleCount == 13U &&
           fnv1a(s, sizeof(*s)) == EXPECTED_ENTRANCE_SNAPSHOT_FNV;
}

void Esp32EntranceStartupRouteProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

void Esp32EntranceStartupRouteProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspMapRuntimeView* runtime;
    EspMapResidentSnapshot snapshot;
    const char* startupFile;
    uint32_t frameBefore;
    uint32_t frameAfter;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;

    if (probeState.done || probeState.attempted || doomRpg == NULL) return;
    if (!Esp32TransitionPreflightFinalProbe_isDone()) return;

    probeState.attempted = 1U;
    memset(&snapshot, 0, sizeof(snapshot));

    printf("\n=== Doom RPG ESP32-native Entrance startup route ===\n");
    printf("[ENTRANCEBOOT] CONTRACT recovered legacy startup is cinematic intro -> startupMap=1 -> /intro.bsp (Entrance). Historical target preflight may inspect /junction.bsp on disk, but resident handoff and committed transition are forbidden during startup. PARK before spawn/render; next milestone owns Entrance placement and first frame.\n");

    frameBefore = frameFNV();
    heapBefore = heap8();
    largestBefore = largest8();

    runtime = EspMapRuntime_view();
    startupFile = doomRpg->game != NULL && doomRpg->doomCanvas != NULL &&
                          doomRpg->doomCanvas->startupMap > 0
                      ? doomRpg->game->mapFiles[doomRpg->doomCanvas->startupMap - 1]
                      : NULL;

    if (doomRpg->doomCanvas == NULL || doomRpg->game == NULL ||
        doomRpg->render == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO ||
        doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->doomCanvas->startupMap != MAP_INTRO ||
        startupFile == NULL || SDL_strcasecmp(startupFile, "/intro.bsp") != 0 ||
        !EspMapResidentLifecycle_isReady() ||
        !EspMapResidentLifecycle_capture(&snapshot) ||
        !snapshotIsCanonicalEntrance(&snapshot) ||
        runtime == NULL || runtime->sourceBytes != EXPECTED_ENTRANCE_SOURCE_BYTES ||
        runtime->sourceCrc32 != EXPECTED_ENTRANCE_SOURCE_CRC32 ||
        runtime->arenaBytes != EXPECTED_ENTRANCE_RUNTIME_BYTES ||
        runtime->arenaFNV1a != EXPECTED_ENTRANCE_RUNTIME_FNV ||
        !legacyRuntimeIsClear(doomRpg->render) ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        EspAssetPack_isOpen() ||
        Esp32ResidentHandoffProbe_isDone() ||
        Esp32CommittedTransitionProbe_isDone()) {
        printf("[ENTRANCEBOOTPROBE] FAILED startup boundary state=%d page=%d startupMap=%d file=%s residentReady=%d snapshotFNV=%08x sourceBytes=%u sourceCRC=%08x arenaFNV=%08x handoffDone=%d committedDone=%d packOpen=%d legacyClear=%d entities=%d monsters=%d shapeData=%p mediaTexels=%p\n",
               doomRpg->doomCanvas != NULL ? doomRpg->doomCanvas->state : -1,
               doomRpg->doomCanvas != NULL ? doomRpg->doomCanvas->storyPage : -1,
               doomRpg->doomCanvas != NULL ? doomRpg->doomCanvas->startupMap : -1,
               startupFile != NULL ? startupFile : "<null>",
               EspMapResidentLifecycle_isReady(),
               (unsigned int)fnv1a(&snapshot, sizeof(snapshot)),
               runtime != NULL ? (unsigned int)runtime->sourceBytes : 0U,
               runtime != NULL ? (unsigned int)runtime->sourceCrc32 : 0U,
               runtime != NULL ? (unsigned int)runtime->arenaFNV1a : 0U,
               Esp32ResidentHandoffProbe_isDone(),
               Esp32CommittedTransitionProbe_isDone(),
               EspAssetPack_isOpen(),
               doomRpg->render != NULL ? legacyRuntimeIsClear(doomRpg->render) : 0,
               doomRpg->game != NULL ? doomRpg->game->numEntities : -1,
               doomRpg->game != NULL ? doomRpg->game->numMonsters : -1,
               doomRpg->render != NULL ? (void*)doomRpg->render->shapeData : NULL,
               doomRpg->render != NULL ? (void*)doomRpg->render->mediaTexels : NULL);
        probeState.done = 1U;
        return;
    }

    frameAfter = frameFNV();
    heapAfter = heap8();
    largestAfter = largest8();
    if (frameAfter != frameBefore || heapAfter != heapBefore ||
        largestAfter != largestBefore) {
        printf("[ENTRANCEBOOTPROBE] FAILED observer side effect frame=%08x->%08x heap8=%u->%u largest8=%u->%u\n",
               (unsigned int)frameBefore, (unsigned int)frameAfter,
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)largestBefore, (unsigned int)largestAfter);
        probeState.done = 1U;
        return;
    }

    printf("[ENTRANCEBOOT] READY resourceMapId=1 file=/intro.bsp name=Entrance startupMap=%d sourceBytes=%u crc32=%08x arena=%u arenaFNV=%08x snapshotBytes=%u snapshotFNV=%08x payload=%u spawnHeader=904 direction=64\n",
           doomRpg->doomCanvas->startupMap,
           (unsigned int)runtime->sourceBytes,
           (unsigned int)runtime->sourceCrc32,
           (unsigned int)runtime->arenaBytes,
           (unsigned int)runtime->arenaFNV1a,
           (unsigned int)sizeof(snapshot),
           (unsigned int)fnv1a(&snapshot, sizeof(snapshot)),
           (unsigned int)snapshot.totalPayloadBytes);
    printf("[ENTRANCEBOOT] OWNERS map=%08x script=%08x line=%08x texture=%08x automap=%08x topology=%08x nodes=%u lines=%u sprites=%u events=%u byteCodes=%u strings=%u entities=%u enemies=%u destructibles=%u\n",
           (unsigned int)snapshot.mapStateFNV1a,
           (unsigned int)snapshot.scriptStateFNV1a,
           (unsigned int)snapshot.lineStateFNV1a,
           (unsigned int)snapshot.textureStateFNV1a,
           (unsigned int)snapshot.automapStateFNV1a,
           (unsigned int)snapshot.topologyFNV1a,
           (unsigned int)snapshot.nodeCount,
           (unsigned int)snapshot.lineCount,
           (unsigned int)snapshot.spriteCount,
           (unsigned int)snapshot.eventCount,
           (unsigned int)snapshot.byteCodeCount,
           (unsigned int)snapshot.stringCount,
           (unsigned int)snapshot.entityCount,
           (unsigned int)snapshot.enemyCount,
           (unsigned int)snapshot.destructibleCount);
    printf("[ENTRANCEBOOT] ROUTE cinematicIntro=done entranceResident=yes targetPreflightOnly=yes residentHandoff=no committedTransition=no junctionResident=no junctionGameplay=no spawnDeferred=yes firstFrameDeferred=yes packClosed=yes legacyRuntimeClear=yes shapeData=%p mediaTexels=%p entities=%d monsters=%d\n",
           (void*)doomRpg->render->shapeData,
           (void*)doomRpg->render->mediaTexels,
           doomRpg->game->numEntities,
           doomRpg->game->numMonsters);
    printf("[ENTRANCEBOOT] RAM frame=%08x->%08x exact=yes heap8=%u->%u largest8=%u->%u allocation=none\n",
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter);
    printf("[ENTRANCEBOOT] PARK level=Entrance next=header-spawn-904+direction-64+first-native-frame\n");

    probeState.done = 1U;
}

int Esp32EntranceStartupRouteProbe_isDone(void) {
    return probeState.done != 0U;
}
