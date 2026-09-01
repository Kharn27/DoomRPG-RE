#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_map_catalog.h"
#include "esp_map_events.h"
#include "esp_map_runtime.h"
#include "esp_map_strings.h"
#include "esp_native_gameplay_interaction_inventory.h"
#include "esp_player_view_state.h"

#define PROBE_OPCODE_CHANGEMAP 2U
#define PROBE_OPCODE_SAVEGAME 27U
#define PROBE_REMOVE_FLAG 0x00000200UL
#define PROBE_NAME_CAPACITY 48U

static uint32_t loggedArenaFNV;

void __real_EspNativeGameplayInteractionInventory_log(void);

static int readCurrentMapString(uint32_t stringIndex,
                                char* destination,
                                size_t capacity,
                                uint16_t* outLength) {
    const EspPlayerViewState* view = EspPlayerView_view();
    const char* sourceName;
    EspAssetPackEntry sourceEntry;
    EspMapStringRef ref;
    size_t length = 0U;
    int ok = 0;

    if (outLength != NULL) *outLength = 0U;
    if (destination == NULL || capacity == 0U || outLength == NULL ||
        view == NULL || view->active != 1U ||
        !EspMapCatalog_isValidId(view->targetMapId)) {
        return 0;
    }

    sourceName = EspMapCatalog_nameForId(view->targetMapId);
    memset(&sourceEntry, 0, sizeof(sourceEntry));
    memset(&ref, 0, sizeof(ref));
    destination[0] = '\0';

    if (sourceName == NULL ||
        !EspMapStrings_getRef(stringIndex, &ref) ||
        ref.length + 1U > capacity ||
        !EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        return 0;
    }

    if (EspAssetPack_findEntry(sourceName, &sourceEntry) &&
        EspMapStrings_read(&sourceEntry, &ref, destination, capacity, &length) ==
            ESP_MAP_STRING_READ_OK &&
        length == ref.length && length <= UINT16_MAX) {
        *outLength = (uint16_t)length;
        ok = 1;
    }
    EspAssetPack_close();
    return ok && !EspAssetPack_isOpen();
}

static void logTransitionCorpus(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint32_t eventIndex;

    if (runtime == NULL || runtime->arenaFNV1a == 0U ||
        runtime->arenaFNV1a == loggedArenaFNV) {
        return;
    }

    for (eventIndex = 0U; eventIndex < runtime->eventCount; ++eventIndex) {
        uint32_t rawEvent;
        EspMapEventRef ref;
        EspMapEventDescriptor descriptor;
        uint32_t offset;

        memset(&ref, 0, sizeof(ref));
        memset(&descriptor, 0, sizeof(descriptor));
        if (!EspMapRuntime_getEvent(eventIndex, &rawEvent)) return;
        ref.index = (uint16_t)eventIndex;
        ref.tileIndex = (uint16_t)(rawEvent & ESP_MAP_EVENT_TILE_MASK);
        ref.value = rawEvent;
        if (!EspMapEvents_describe(&ref, &descriptor)) return;

        for (offset = 0U; offset < descriptor.commandCount; ++offset) {
            EspMapByteCode command;
            uint32_t stringIndex;
            char mapName[PROBE_NAME_CAPACITY];
            uint16_t mapNameLength = 0U;
            uint8_t targetMapId = 0U;
            int stringOk;

            memset(&command, 0, sizeof(command));
            memset(mapName, 0, sizeof(mapName));
            if (!EspMapEvents_getCommand(&descriptor, offset, &command)) return;
            if (command.id != PROBE_OPCODE_CHANGEMAP &&
                command.id != PROBE_OPCODE_SAVEGAME) {
                continue;
            }

            stringIndex = command.arg1 & 0xffU;
            stringOk = readCurrentMapString(stringIndex, mapName,
                                            sizeof(mapName), &mapNameLength);
            if (stringOk) {
                (void)EspMapCatalog_idForName(mapName, &targetMapId);
            }

            if (command.id == PROBE_OPCODE_CHANGEMAP) {
                const uint32_t spawnParam = (command.arg1 << 1U) >> 9U;
                const uint8_t showStats =
                    (uint8_t)((command.arg1 >> 31U) & 1U);
                printf("[CHANGEMAPPROBE] event=%u tile=%u cmd=%u global=%u opcode=2 raw=%08x arg2=%08x string=%u name=%s nameLen=%u targetMap=%u showStats=%u spawnParam=%u remove=%u action=probe-only-fail-closed\n",
                       (unsigned int)descriptor.eventIndex,
                       (unsigned int)descriptor.tileIndex,
                       (unsigned int)offset,
                       (unsigned int)((uint32_t)descriptor.firstCommandIndex + offset),
                       (unsigned int)command.arg1,
                       (unsigned int)command.arg2,
                       (unsigned int)stringIndex,
                       stringOk ? mapName : "<unreadable>",
                       (unsigned int)mapNameLength,
                       (unsigned int)targetMapId,
                       (unsigned int)showStats,
                       (unsigned int)spawnParam,
                       (unsigned int)((command.arg2 & PROBE_REMOVE_FLAG) != 0U));
            }
            else {
                const uint32_t packed = command.arg1 >> 8U;
                const uint8_t rawX = (uint8_t)(packed & 0xffU);
                const uint8_t rawY = (uint8_t)((packed >> 8U) & 0xffU);
                const uint8_t angle = (uint8_t)((packed >> 16U) & 0xffU);
                printf("[CHANGEMAPPROBE] event=%u tile=%u cmd=%u global=%u opcode=27 raw=%08x arg2=%08x string=%u name=%s nameLen=%u targetMap=%u saveRaw=%u,%u angle=%u savePos=%u,%u remove=%u action=probe-only-fail-closed\n",
                       (unsigned int)descriptor.eventIndex,
                       (unsigned int)descriptor.tileIndex,
                       (unsigned int)offset,
                       (unsigned int)((uint32_t)descriptor.firstCommandIndex + offset),
                       (unsigned int)command.arg1,
                       (unsigned int)command.arg2,
                       (unsigned int)stringIndex,
                       stringOk ? mapName : "<unreadable>",
                       (unsigned int)mapNameLength,
                       (unsigned int)targetMapId,
                       (unsigned int)rawX,
                       (unsigned int)rawY,
                       (unsigned int)angle,
                       (unsigned int)(32U + ((uint32_t)rawX << 6U)),
                       (unsigned int)(32U + ((uint32_t)rawY << 6U)),
                       (unsigned int)((command.arg2 & PROBE_REMOVE_FLAG) != 0U));
            }
        }
    }

    loggedArenaFNV = runtime->arenaFNV1a;
}

void __wrap_EspNativeGameplayInteractionInventory_log(void) {
    __real_EspNativeGameplayInteractionInventory_log();
    logTransitionCorpus();
}
