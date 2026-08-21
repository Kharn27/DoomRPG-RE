#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "Game.h"
#include "Hud.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Player.h"
#include "Render.h"

#include <esp_heap_caps.h>

#include "esp_asset_pack.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_state.h"
#include "esp_map_strings.h"
#include "native_intro_clock.h"
#include "native_intro_dispose.h"
#include "native_intro_input.h"
#include "native_map1_access_probe.h"
#include "native_map1_bsp_pass1.h"
#include "native_map1_event_descriptor_probe.h"
#include "native_map1_event_filter_probe.h"
#include "native_map1_events_probe.h"
#include "native_map1_opcode_exec_probe.h"
#include "native_map1_runtime_load.h"
#include "native_map1_state_probe.h"
#include "native_map1_string_reader_probe.h"
#include "native_map1_ui_intent_probe.h"
#include "native_sprite_lru_cache.h"
#include "native_wall_lru_cache.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_ARENA_BYTES 14095U
#define EXPECTED_ARENA_FNV 0xc3882516U
#define EXPECTED_MAP_STATE_BYTES 1024U
#define EXPECTED_MAP_STATE_FNV 0xcd99b98eU
#define EXPECTED_SCRIPT_BYTES 81U
#define EXPECTED_SCRIPT_FNV 0xf9e3d9dfU
#define EXPECTED_STRING_COUNT 94U
#define EXPECTED_STRING_PAYLOAD_BYTES 7779U
#define EXPECTED_MAX_STRING_BYTES 313U
#define EXPECTED_STRING_SPAN_FNV 0x713188ebU
#define EXPECTED_INTRO_BSP_BYTES 21823U
#define EXPECTED_INTRO_BSP_CRC32 0x623f34e4U
#define READER_CAPACITY (EXPECTED_MAX_STRING_BYTES + 1U)
#define READER_STORAGE_BYTES (READER_CAPACITY + 2U)

typedef struct Esp32Map1StringReaderProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32Map1StringReaderProbeState;

typedef struct StringReadAudit_s {
    uint32_t spanFNV;
    uint32_t contentFNV;
    uint32_t count;
    uint32_t payloadBytes;
    uint32_t zeroLength;
    uint32_t cEmpty;
    uint32_t sourceNulBytes;
    uint32_t maxLength;
    uint32_t prefixMatches;
    uint32_t packPayloadReads;
    uint32_t guardChecks;
    uint32_t shortBufferRefused;
    uint32_t badRefRefused;
    uint32_t nullRefRefused;
    uint32_t closedPackRefused;
    uint32_t sampleFNV1;
    uint32_t sampleFNV25;
    uint32_t sampleFNV30;
    uint32_t sampleFNV85;
    EspMapStringRef maxRef;
    EspMapStringRef nonEmptyRef;
    uint8_t haveNonEmpty;
} StringReadAudit;

static Esp32Map1StringReaderProbeState probeState;

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

static uint32_t hashU16(uint32_t hash, uint16_t value) {
    hash = hashByte(hash, (uint8_t)(value & 0xffU));
    return hashByte(hash, (uint8_t)((value >> 8) & 0xffU));
}

static uint32_t fnv1a32(const uint8_t* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (data == NULL && length != 0U) return 0U;
    for (i = 0U; i < length; ++i) hash = hashByte(hash, data[i]);
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
    return fnv1a32(framebuffer, (uint32_t)bytes);
}

static int introResourcesAreReleased(const DoomCanvas_t* canvas) {
    return canvas != NULL &&
           canvas->imgSpaceBG.imgBitmap == NULL &&
           canvas->imgLinesLayer.imgBitmap == NULL &&
           canvas->imgPlanetLayer.imgBitmap == NULL &&
           canvas->imgSpaceship.imgBitmap == NULL &&
           canvas->storyText1[0] == NULL && canvas->storyText1[1] == NULL &&
           canvas->storyText2 == NULL;
}

static int legacyRuntimeIsClear(const Render_t* render) {
    return render != NULL &&
           render->nodes == NULL && render->lines == NULL &&
           render->mapSprites == NULL && render->tileEvents == NULL &&
           render->mapByteCode == NULL && render->mapStringsIDs == NULL &&
           render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL && render->mediaSpriteIds == NULL &&
           render->mapTextureTexels == NULL && render->mapSpriteTexels == NULL &&
           render->shapeData == NULL && render->mediaTexels == NULL &&
           render->ioBuffer == NULL &&
           !EspNativeWallCache_isActive() && !EspNativeSpriteCache_isActive();
}

static int boundaryIsSafe(const DoomRPG_t* doomRpg) {
    const DoomCanvas_t* canvas;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->menuSystem == NULL || doomRpg->hud == NULL ||
        doomRpg->player == NULL) return 0;

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    canvas = doomRpg->doomCanvas;

    return Esp32IntroDispose_isDone() && Esp32Map1BspPass1_isDone() &&
           Esp32Map1RuntimeLoad_isDone() && Esp32Map1AccessProbe_isDone() &&
           Esp32Map1StateProbe_isDone() && Esp32Map1EventsProbe_isDone() &&
           Esp32Map1EventDescriptorProbe_isDone() &&
           Esp32Map1EventFilterProbe_isDone() &&
           Esp32Map1OpcodeExecProbe_isDone() &&
           Esp32Map1UiIntentProbe_isDone() &&
           runtime != NULL && mapState != NULL && scriptState != NULL &&
           runtime->arenaBytes == EXPECTED_ARENA_BYTES &&
           runtime->arenaFNV1a == EXPECTED_ARENA_FNV &&
           runtime->sourceBytes == EXPECTED_INTRO_BSP_BYTES &&
           runtime->sourceCrc32 == EXPECTED_INTRO_BSP_CRC32 &&
           runtime->stringCount == EXPECTED_STRING_COUNT &&
           mapState->tileCount == EXPECTED_MAP_STATE_BYTES &&
           mapState->stateFNV1a == EXPECTED_MAP_STATE_FNV &&
           scriptState->storageBytes == EXPECTED_SCRIPT_BYTES &&
           !EspAssetPack_isOpen() && !Esp32IntroClock_isActive() &&
           !Esp32IntroInput_isActive() && doomRpg->menuSystem->menu == MENU_NONE &&
           canvas->state == ST_INTRO && canvas->storyPage == 3 &&
           canvas->storyTextPage == 0 && canvas->startupMap == MAP_INTRO &&
           introResourcesAreReleased(canvas) && legacyRuntimeIsClear(doomRpg->render) &&
           doomRpg->game->numEntities == 0 && doomRpg->game->numMonsters == 0;
}

static uint16_t readLe16(const uint8_t bytes[2]) {
    return (uint16_t)((uint16_t)bytes[0] |
                      ((uint16_t)bytes[1] << 8));
}

static int auditStrings(const EspAssetPackEntry* entry, StringReadAudit* audit) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint8_t storage[READER_STORAGE_BYTES];
    char* text = (char*)&storage[1];
    uint8_t prefix[2];
    EspMapStringRef ref;
    EspMapStringRef badRef;
    size_t readLength;
    uint32_t spanHash = 2166136261U;
    uint32_t contentHash = 2166136261U;
    uint32_t i;
    uint32_t j;

    if (entry == NULL || audit == NULL || runtime == NULL ||
        runtime->stringCount != EXPECTED_STRING_COUNT) return 0;
    memset(audit, 0, sizeof(*audit));

    for (i = 0U; i < runtime->stringCount; ++i) {
        if (!EspMapStrings_getRef(i, &ref) || ref.index != i ||
            ref.sourceOffset < 2U ||
            !EspAssetPack_readRange(entry, (uint32_t)ref.sourceOffset - 2U,
                                    prefix, sizeof(prefix)) ||
            readLe16(prefix) != ref.length) return 0;
        ++audit->prefixMatches;

        storage[0] = 0xa5U;
        storage[READER_STORAGE_BYTES - 1U] = 0x5aU;
        memset(text, 0xcc, READER_CAPACITY);
        readLength = (size_t)-1;
        if (EspMapStrings_read(entry, &ref, text, READER_CAPACITY,
                               &readLength) != ESP_MAP_STRING_READ_OK ||
            readLength != ref.length || text[ref.length] != '\0' ||
            storage[0] != 0xa5U ||
            storage[READER_STORAGE_BYTES - 1U] != 0x5aU) return 0;
        ++audit->guardChecks;
        if (ref.length > 0U) ++audit->packPayloadReads;

        spanHash = hashU16(spanHash, ref.index);
        spanHash = hashU16(spanHash, ref.sourceOffset);
        spanHash = hashU16(spanHash, ref.length);
        contentHash = hashU16(contentHash, ref.index);
        contentHash = hashU16(contentHash, ref.length);
        for (j = 0U; j < ref.length; ++j) {
            const uint8_t value = (uint8_t)text[j];
            contentHash = hashByte(contentHash, value);
            if (value == 0U) ++audit->sourceNulBytes;
        }

        ++audit->count;
        audit->payloadBytes += ref.length;
        if (ref.length == 0U) ++audit->zeroLength;
        if (text[0] == '\0') ++audit->cEmpty;
        if (ref.length > audit->maxLength) {
            audit->maxLength = ref.length;
            audit->maxRef = ref;
        }
        if (!audit->haveNonEmpty && ref.length > 0U) {
            audit->nonEmptyRef = ref;
            audit->haveNonEmpty = 1U;
        }

        if (i == 1U) audit->sampleFNV1 = fnv1a32((const uint8_t*)text, ref.length);
        else if (i == 25U) audit->sampleFNV25 = fnv1a32((const uint8_t*)text, ref.length);
        else if (i == 30U) audit->sampleFNV30 = fnv1a32((const uint8_t*)text, ref.length);
        else if (i == 85U) audit->sampleFNV85 = fnv1a32((const uint8_t*)text, ref.length);
    }

    if (audit->maxLength == 0U || !audit->haveNonEmpty) return 0;

    storage[0] = 0xa5U;
    storage[READER_STORAGE_BYTES - 1U] = 0x5aU;
    readLength = 123U;
    if (EspMapStrings_read(entry, &audit->maxRef, text, audit->maxRef.length,
                           &readLength) != ESP_MAP_STRING_READ_BUFFER_TOO_SMALL ||
        readLength != 0U || storage[0] != 0xa5U ||
        storage[READER_STORAGE_BYTES - 1U] != 0x5aU) return 0;
    audit->shortBufferRefused = 1U;

    badRef = audit->nonEmptyRef;
    ++badRef.sourceOffset;
    readLength = 123U;
    if (EspMapStrings_read(entry, &badRef, text, READER_CAPACITY,
                           &readLength) != ESP_MAP_STRING_READ_INVALID ||
        readLength != 0U) return 0;
    audit->badRefRefused = 1U;

    readLength = 123U;
    if (EspMapStrings_read(entry, NULL, text, READER_CAPACITY,
                           &readLength) != ESP_MAP_STRING_READ_INVALID ||
        readLength != 0U) return 0;
    audit->nullRefRefused = 1U;

    audit->spanFNV = spanHash;
    audit->contentFNV = contentHash;
    return audit->count == EXPECTED_STRING_COUNT &&
           audit->payloadBytes == EXPECTED_STRING_PAYLOAD_BYTES &&
           audit->zeroLength == 1U && audit->maxLength == EXPECTED_MAX_STRING_BYTES &&
           audit->prefixMatches == EXPECTED_STRING_COUNT &&
           audit->guardChecks == EXPECTED_STRING_COUNT &&
           audit->packPayloadReads == EXPECTED_STRING_COUNT - audit->zeroLength &&
           audit->spanFNV == EXPECTED_STRING_SPAN_FNV &&
           audit->shortBufferRefused == 1U && audit->badRefRefused == 1U &&
           audit->nullRefRefused == 1U;
}

void Esp32Map1StringReaderProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

void Esp32Map1StringReaderProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    EspAssetPackEntry entry = {0};
    StringReadAudit audit;
    uint8_t closedStorage[READER_CAPACITY];
    size_t closedLength;
    const char* mapFile;
    uint32_t heapBefore, heapOpen, heapAfter;
    uint32_t largestBefore, largestOpen, largestAfter;
    uint32_t frameBefore, frameAfter;
    uint32_t arenaBefore, arenaAfter;
    uint32_t mapStateBefore, mapStateAfter;
    uint32_t scriptBefore, scriptAfter;
    uint32_t notebookBefore, notebookAfter;
    uint32_t started, elapsed;
    char* statBarBefore;
    int skipAdvanceBefore, saveTileBefore, tileEventBefore;
    int tileEventIndexBefore, tileEventFlagsBefore;

    if (probeState.done || probeState.attempted || doomRpg == NULL) return;
    if (!Esp32Map1UiIntentProbe_isDone()) return;
    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[MAPTEXTPROBE] ARMED native UI intents proven; bounded pack-backed string reads start on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native MAP_INTRO bounded string reader ===\n");
    printf("[MAPTEXTPROBE] CONTRACT one resolved EspMapStringRef -> exact .pak range -> caller buffer <=314B incl terminator; 0 persistent bytes; no ZIP/UI/world/render mutation\n");

    if (!boundaryIsSafe(doomRpg)) {
        printf("[MAPTEXTPROBE] FAILED precondition heap8=%u largest8=%u packOpen=%d\n",
               (unsigned int)heap8Free(), (unsigned int)largest8Block(),
               EspAssetPack_isOpen());
        return;
    }

    mapFile = doomRpg->game->mapFiles[doomRpg->doomCanvas->startupMap - 1];
    if (mapFile == NULL || SDL_strcasecmp(mapFile, "/intro.bsp") != 0) {
        printf("[MAPTEXTPROBE] FAILED startup map resolves to '%s'\n",
               mapFile != NULL ? mapFile : "<null>");
        return;
    }

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    heapBefore = heap8Free();
    largestBefore = largest8Block();
    frameBefore = framebufferHash();
    arenaBefore = fnv1a32(runtime->arena, runtime->arenaBytes);
    mapStateBefore = fnv1a32(mapState->tileFlags, mapState->tileCount);
    scriptBefore = fnv1a32(scriptState->storage, scriptState->storageBytes);
    notebookBefore = fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                             (uint32_t)sizeof(doomRpg->player->NotebookString));
    statBarBefore = doomRpg->hud->statBarMessage;
    skipAdvanceBefore = doomRpg->game->skipAdvanceTurn;
    saveTileBefore = doomRpg->game->saveTileEvent;
    tileEventBefore = doomRpg->game->tileEvent;
    tileEventIndexBefore = doomRpg->game->tileEventIndex;
    tileEventFlagsBefore = doomRpg->game->tileEventFlags;
    started = DoomRPG_GetUpTimeMS();

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[MAPTEXTPROBE] FAILED open %s\n", ESP_ASSET_PACK_DEFAULT_PATH);
        return;
    }
    heapOpen = heap8Free();
    largestOpen = largest8Block();

    if (!EspAssetPack_findEntry(mapFile, &entry) ||
        entry.size != runtime->sourceBytes || entry.size != EXPECTED_INTRO_BSP_BYTES ||
        entry.crc32 != runtime->sourceCrc32 || entry.crc32 != EXPECTED_INTRO_BSP_CRC32 ||
        (entry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        !auditStrings(&entry, &audit)) {
        printf("[MAPTEXTPROBE] FAILED source/read audit entrySize=%u crc=%08x\n",
               (unsigned int)entry.size, (unsigned int)entry.crc32);
        EspAssetPack_close();
        return;
    }
    EspAssetPack_close();

    closedLength = 123U;
    if (EspMapStrings_read(&entry, &audit.nonEmptyRef, (char*)closedStorage,
                           sizeof(closedStorage), &closedLength) !=
            ESP_MAP_STRING_READ_IO_ERROR || closedLength != 0U) {
        printf("[MAPTEXTPROBE] FAILED closed-pack refusal\n");
        return;
    }
    audit.closedPackRefused = 1U;

    elapsed = DoomRPG_GetUpTimeMS() - started;
    heapAfter = heap8Free();
    largestAfter = largest8Block();
    frameAfter = framebufferHash();
    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    arenaAfter = runtime != NULL ? fnv1a32(runtime->arena, runtime->arenaBytes) : 0U;
    mapStateAfter = mapState != NULL ? fnv1a32(mapState->tileFlags, mapState->tileCount) : 0U;
    scriptAfter = scriptState != NULL ? fnv1a32(scriptState->storage, scriptState->storageBytes) : 0U;
    notebookAfter = fnv1a32((const uint8_t*)doomRpg->player->NotebookString,
                            (uint32_t)sizeof(doomRpg->player->NotebookString));

    if (EspAssetPack_isOpen() || !boundaryIsSafe(doomRpg) ||
        heapAfter != heapBefore || largestAfter != largestBefore ||
        frameAfter != frameBefore || arenaAfter != arenaBefore ||
        arenaAfter != EXPECTED_ARENA_FNV || mapStateAfter != mapStateBefore ||
        mapStateAfter != EXPECTED_MAP_STATE_FNV || scriptAfter != scriptBefore ||
        scriptAfter != EXPECTED_SCRIPT_FNV || notebookAfter != notebookBefore ||
        audit.closedPackRefused != 1U || doomRpg->hud->statBarMessage != statBarBefore ||
        doomRpg->game->skipAdvanceTurn != skipAdvanceBefore ||
        doomRpg->game->saveTileEvent != saveTileBefore ||
        doomRpg->game->tileEvent != tileEventBefore ||
        doomRpg->game->tileEventIndex != tileEventIndexBefore ||
        doomRpg->game->tileEventFlags != tileEventFlagsBefore) {
        printf("[MAPTEXTPROBE] FAILED postcondition heap8=%u->%u largest8=%u->%u frame=%08x->%08x arena=%08x->%08x mapState=%08x->%08x script=%08x->%08x notebook=%08x->%08x packOpen=%d\n",
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)largestBefore, (unsigned int)largestAfter,
               (unsigned int)frameBefore, (unsigned int)frameAfter,
               (unsigned int)arenaBefore, (unsigned int)arenaAfter,
               (unsigned int)mapStateBefore, (unsigned int)mapStateAfter,
               (unsigned int)scriptBefore, (unsigned int)scriptAfter,
               (unsigned int)notebookBefore, (unsigned int)notebookAfter,
               EspAssetPack_isOpen());
        return;
    }

    probeState.done = 1;
    printf("[MAPTEXT] READY strings=%u payload=%u zeroLength=%u cEmpty=%u sourceNulBytes=%u max=%u prefixMatches=%u spanFNV=%08x contentFNV=%08x\n",
           (unsigned int)audit.count, (unsigned int)audit.payloadBytes,
           (unsigned int)audit.zeroLength, (unsigned int)audit.cEmpty,
           (unsigned int)audit.sourceNulBytes, (unsigned int)audit.maxLength,
           (unsigned int)audit.prefixMatches, (unsigned int)audit.spanFNV,
           (unsigned int)audit.contentFNV);
    printf("[MAPTEXT] SAMPLES id1=%08x id25=%08x id30=%08x id85=%08x\n",
           (unsigned int)audit.sampleFNV1, (unsigned int)audit.sampleFNV25,
           (unsigned int)audit.sampleFNV30, (unsigned int)audit.sampleFNV85);
    printf("[MAPTEXT] FAILCLOSED shortBuffer=%u badRef=%u nullRef=%u closedPack=%u guards=%u/%u packPayloadReads=%u\n",
           (unsigned int)audit.shortBufferRefused, (unsigned int)audit.badRefRefused,
           (unsigned int)audit.nullRefRefused, (unsigned int)audit.closedPackRefused,
           (unsigned int)audit.guardChecks, (unsigned int)EXPECTED_STRING_COUNT,
           (unsigned int)audit.packPayloadReads);
    printf("[MAPTEXT] IO entry=%s size=%u crc32=%08x heapOpen=%u transientHeapCost=%d largestOpen=%u elapsed=%ums persistentBytes=0\n",
           mapFile, (unsigned int)entry.size, (unsigned int)entry.crc32,
           (unsigned int)heapOpen, (int)heapBefore - (int)heapOpen,
           (unsigned int)largestOpen, (unsigned int)elapsed);
    printf("[MAPTEXTPROBE] RAM heap8=%u->%u delta=0 largest8=%u->%u delta=0 frameFNV=%08x->%08x arenaFNV=%08x->%08x mapStateFNV=%08x->%08x scriptFNV=%08x->%08x notebookFNV=%08x->%08x\n",
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter,
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           (unsigned int)arenaBefore, (unsigned int)arenaAfter,
           (unsigned int)mapStateBefore, (unsigned int)mapStateAfter,
           (unsigned int)scriptBefore, (unsigned int)scriptAfter,
           (unsigned int)notebookBefore, (unsigned int)notebookAfter);
    printf("[MAPTEXTPROBE] PARK state=%d page=%d nativeArena=yes nativeTileState=yes nativeEventLookup=yes nativeEventDescriptor=yes nativeScriptState=yes nativeFilter=yes nativeOpcodeExec=yes nativeUiIntent=yes nativeStringReader=yes persistentBytes=0 legacyUiMutation=no worldMutation=no framebufferMutation=no entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           doomRpg->game->numEntities, doomRpg->game->numMonsters);
}

int Esp32Map1StringReaderProbe_isDone(void) {
    return probeState.done;
}
