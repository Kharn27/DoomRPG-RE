#include <Arduino.h>
#include <SD.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_bsp_reader.h"
#include "esp_hud_post_load_clear_state.h"
#include "esp_hud_refresh_state.h"
#include "esp_map_automap_state.h"
#include "esp_map_catalog.h"
#include "esp_map_line_checkpoint.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_state.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_combat_math.h"
#include "esp_native_gameplay_crate_state.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_hub.h"
#include "esp_native_gameplay_hub_theme.h"
#include "esp_native_gameplay_input.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_player_resources.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_gameplay_session.h"
#include "esp_native_gameplay_save_ui.h"
#include "esp_player_facing_state.h"
#include "esp_player_finish_rotation_tile.h"
#include "esp_player_fresh_map_state.h"
#include "esp_player_initial_tile.h"
#include "esp_player_orientation_state.h"
#include "esp_player_spawn_state.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

namespace {

constexpr char kSavePath[] = "/DoomRPG-ESP32.sav";
constexpr char kTempPath[] = "/DoomRPG-ESP32.sav.tmp";
constexpr char kBackupPath[] = "/DoomRPG-ESP32.sav.bak";
constexpr char kLogPath[] = "/sd/DoomRPG-ESP32.sav";
constexpr uint8_t kMagicV1[8] = {'D', 'R', 'P', 'G', 'S', 'A', 'V', '1'};
constexpr uint8_t kMagicV2[8] = {'D', 'R', 'P', 'G', 'S', 'A', 'V', '2'};
constexpr uint8_t kMagicV3[8] = {'D', 'R', 'P', 'G', 'S', 'A', 'V', '3'};
constexpr uint8_t kMagicV4[8] = {'D', 'R', 'P', 'G', 'S', 'A', 'V', '4'};
constexpr uint8_t kMagicV5[8] = {'D', 'R', 'P', 'G', 'S', 'A', 'V', '5'};
constexpr uint8_t kMagicV6[8] = {'D', 'R', 'P', 'G', 'S', 'A', 'V', '6'};
constexpr uint8_t kMagicV7[8] = {'D', 'R', 'P', 'G', 'S', 'A', 'V', '7'};
constexpr uint8_t kMagicV8[8] = {'D', 'R', 'P', 'G', 'S', 'A', 'V', '8'};
constexpr uint16_t kVersionV1 = 1U;
constexpr uint16_t kVersionV2 = 2U;
constexpr uint16_t kVersionV3 = 3U;
constexpr uint16_t kVersionV4 = 4U;
constexpr uint16_t kVersionV5 = 5U;
constexpr uint16_t kVersionV6 = 6U;
constexpr uint16_t kVersionV7 = 7U;
constexpr uint16_t kVersionV8 = 8U;
constexpr uint8_t kFamiliarAmmoType = 5U;
constexpr uint8_t kStatusSave = 0U;
constexpr uint8_t kStatusLoad = 1U;
constexpr uint8_t kStatusCount = 2U;
constexpr uint8_t kNoConfirmation = 0xffU;
constexpr int kPanelLeft = 8;
constexpr int kPanelTop = 36;
constexpr int kPanelRight = 151;
constexpr int kPanelBottom = 98;
constexpr int kButtonLeft = 16;
constexpr int kButtonRight = 143;
constexpr int kSaveTop = 48;
constexpr int kSaveBottom = 68;
constexpr int kLoadTop = 74;
constexpr int kLoadBottom = 94;

/* Exact on-disk v1 prefix. Keep this byte-for-byte compatible with the
 * hardware-proven 132-byte DRPGSAV1 record so existing checkpoints remain
 * loadable after later versions are introduced. */
struct NativeSaveCore {
    uint8_t magic[8];
    uint16_t version;
    uint16_t recordBytes;
    uint32_t recordCrc32;
    uint32_t sourceBytes;
    uint32_t sourceCrc32;
    uint32_t runtimeFNV1a;
    uint32_t playerFNV1a;
    uint8_t targetMapId;
    uint8_t gameplayLoadMapId;
    uint8_t loadType;
    uint8_t reserved0;
    EspPlayerViewState view;
    EspNativeGameplayPlayerState player;
};

struct NativeSaveRecordV2 {
    NativeSaveCore core;
    EspNativeGameplayPlayerResourcesSnapshot resources;
};

struct NativeSaveRecordV3 {
    NativeSaveCore core;
    EspNativeGameplayPlayerResourcesSnapshot resources;
    EspMapScriptStateSnapshot script;
};

struct NativeSaveRecordV4 {
    NativeSaveCore core;
    EspNativeGameplayPlayerResourcesSnapshot resources;
    EspMapScriptStateSnapshot script;
    EspMapLineCheckpointSnapshot lines;
};

struct NativeSaveRecordV5 {
    NativeSaveCore core;
    EspNativeGameplayPlayerResourcesSnapshot resources;
    EspMapScriptStateSnapshot script;
    EspMapLineCheckpointSnapshot lines;
    EspNativeGameplayActionRemovedSnapshot actionRemoved;
};

struct LoadedSaveRecord {
    NativeSaveCore core;
    EspNativeGameplayPlayerResourcesSnapshot resources;
    EspMapScriptStateSnapshot script;
    EspMapLineCheckpointSnapshot lines;
    EspNativeGameplayActionRemovedSnapshot actionRemoved;
    uint16_t fileBytes;
    uint8_t hasResources;
    uint8_t hasScript;
    uint8_t hasLines;
    uint8_t hasActionRemoved;
    uint8_t hasAutomap;
    uint8_t reservedLoaded;
};

constexpr size_t kRecordBytesV6 =
    sizeof(NativeSaveRecordV5) +
    sizeof(EspNativeGameplayCrateTransformSnapshot);
constexpr size_t kRecordBytesV7 =
    kRecordBytesV6 + sizeof(EspMapAutomapSnapshot);
constexpr size_t kRecordBytesV8 =
    kRecordBytesV7 + sizeof(EspNativeGameplayMonsterStateSnapshot);

static_assert(sizeof(EspNativeGameplayCrateTransformSnapshot) == 176U,
              "crate transform checkpoint must remain exactly 176 bytes");
static_assert(sizeof(EspMapAutomapSnapshot) == 404U,
              "automap checkpoint must remain exactly 404 bytes");
static_assert(kRecordBytesV6 == 1532U,
              "native save v6 must remain the bounded 1532-byte streamed record");
static_assert(kRecordBytesV7 == 1936U,
              "native save v7 must remain the bounded 1936-byte streamed record");
static_assert(sizeof(EspNativeGameplayMonsterStateSnapshot) == 1612U,
              "monster-state checkpoint must remain exactly 1612 bytes");
static_assert(kRecordBytesV8 == 3548U,
              "native save v8 must remain the bounded 3548-byte streamed record");
static_assert(kRecordBytesV8 <= 0xffffU,
              "native save recordBytes field is uint16_t");

static_assert(sizeof(NativeSaveCore) == 132U,
              "native save v1 compatibility requires the proven 132-byte prefix");
static_assert(sizeof(NativeSaveRecordV2) ==
                  sizeof(NativeSaveCore) +
                      sizeof(EspNativeGameplayPlayerResourcesSnapshot),
              "native save v2 must remain a packed semantic prefix + section");
static_assert(sizeof(NativeSaveRecordV2) <= 320U,
              "native save v2 must remain a tiny fixed record");
static_assert(sizeof(NativeSaveRecordV3) ==
                  sizeof(NativeSaveRecordV2) + sizeof(EspMapScriptStateSnapshot),
              "native save v3 must append exactly one bounded script section");
static_assert(sizeof(NativeSaveRecordV3) <= 1536U,
              "native save v3 must remain a bounded compact record");
static_assert(sizeof(NativeSaveRecordV4) ==
                  sizeof(NativeSaveRecordV3) +
                      sizeof(EspMapLineCheckpointSnapshot),
              "native save v4 must append exactly one bounded line section");
static_assert(sizeof(NativeSaveRecordV4) <= 1536U,
              "native save v4 must remain a bounded compact record");
static_assert(sizeof(NativeSaveRecordV5) ==
                  sizeof(NativeSaveRecordV4) +
                      sizeof(EspNativeGameplayActionRemovedSnapshot),
              "native save v5 must append exactly one bounded action-removal section");
static_assert(sizeof(NativeSaveRecordV5) <= 1536U,
              "native save v5 must remain a bounded compact record");
static_assert(offsetof(LoadedSaveRecord, fileBytes) ==
                  sizeof(NativeSaveRecordV5),
              "loaded v5 semantic prefix must match exact on-disk v5 bytes");

uint8_t statusCursor;
uint8_t lastOperation;
uint8_t lastOperationOk;
uint8_t confirmationTarget = kNoConfirmation;

/*
 * V4 grew the bounded checkpoint record by the line-state section. Keeping a
 * LoadedSaveRecord automatic inside the HUB wrapper pushed loopTask over its
 * hardware stack canary before the real STATUS page renderer even returned.
 *
 * Save/load dispatch is serialized by the Arduino loop task, so one bounded
 * non-reentrant BSS read workspace is sufficient for probe, verification and
 * load. Large checkpoint payloads must not live in the HUB wrapper frame.
 */
union NativeSaveWorkspace {
    LoadedSaveRecord loaded;
    NativeSaveRecordV5 write;
};

NativeSaveWorkspace saveWorkspace;
static_assert(sizeof(NativeSaveWorkspace) <= 1536U,
              "native save shared workspace must stay small and bounded");

uint32_t crc32Bytes(const uint8_t* data, size_t bytes) {
    uint32_t crc = 0xffffffffU;
    size_t i;
    uint32_t bit;
    if (data == nullptr && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; ++bit) {
            const uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1) ^ (0xedb88320U & mask);
        }
    }
    return ~crc;
}

uint32_t fnv1aBytes(const uint8_t* data, size_t bytes) {
    uint32_t hash = 2166136261U;
    size_t i;
    if (data == nullptr && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

uint32_t fnv1aAppend(uint32_t hash, const uint8_t* data, size_t bytes) {
    size_t i;
    if (data == nullptr && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

uint32_t automapSnapshotFNV(const EspMapAutomapSnapshot& automap) {
    uint32_t hash = 2166136261U;
    hash = fnv1aAppend(hash, automap.lineBits, automap.lineBitsetBytes);
    if (hash == 0U) return 0U;
    return fnv1aAppend(hash, automap.spriteBits, automap.spriteBitsetBytes);
}

uint32_t recordCrcBytes(const uint8_t* data, size_t bytes) {
    const size_t zeroOffset = offsetof(NativeSaveCore, recordCrc32);
    const size_t zeroEnd = zeroOffset + sizeof(uint32_t);
    uint32_t crc = 0xffffffffU;
    size_t i;
    uint32_t bit;

    if (data == nullptr || bytes < zeroEnd) return 0U;
    for (i = 0U; i < bytes; ++i) {
        const uint8_t value =
            (i >= zeroOffset && i < zeroEnd) ? 0U : data[i];
        crc ^= value;
        for (bit = 0U; bit < 8U; ++bit) {
            const uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1) ^ (0xedb88320U & mask);
        }
    }
    return ~crc;
}

uint32_t recordCrcV1(const NativeSaveCore& record) {
    return recordCrcBytes(reinterpret_cast<const uint8_t*>(&record),
                          sizeof(record));
}

uint32_t recordCrcV2(const NativeSaveRecordV2& record) {
    return recordCrcBytes(reinterpret_cast<const uint8_t*>(&record),
                          sizeof(record));
}

uint32_t recordCrcV3(const NativeSaveRecordV3& record) {
    return recordCrcBytes(reinterpret_cast<const uint8_t*>(&record),
                          sizeof(record));
}

uint32_t recordCrcV4(const NativeSaveRecordV4& record) {
    return recordCrcBytes(reinterpret_cast<const uint8_t*>(&record),
                          sizeof(record));
}

uint32_t recordCrcV5(const NativeSaveRecordV5& record) {
    return recordCrcBytes(reinterpret_cast<const uint8_t*>(&record),
                          sizeof(record));
}

uint32_t recordCrcV6(
    const NativeSaveRecordV5& prefix,
    const EspNativeGameplayCrateTransformSnapshot& crateTransforms) {
    const uint8_t* prefixBytes =
        reinterpret_cast<const uint8_t*>(&prefix);
    const uint8_t* crateBytes =
        reinterpret_cast<const uint8_t*>(&crateTransforms);
    const size_t zeroOffset = offsetof(NativeSaveCore, recordCrc32);
    const size_t zeroEnd = zeroOffset + sizeof(uint32_t);
    uint32_t crc = 0xffffffffU;
    size_t i;
    uint32_t bit;

    for (i = 0U; i < sizeof(prefix); ++i) {
        const uint8_t value =
            (i >= zeroOffset && i < zeroEnd) ? 0U : prefixBytes[i];
        crc ^= value;
        for (bit = 0U; bit < 8U; ++bit) {
            const uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1) ^ (0xedb88320U & mask);
        }
    }
    for (i = 0U; i < sizeof(crateTransforms); ++i) {
        crc ^= crateBytes[i];
        for (bit = 0U; bit < 8U; ++bit) {
            const uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1) ^ (0xedb88320U & mask);
        }
    }
    return ~crc;
}

uint32_t recordCrcV7(
    const NativeSaveRecordV5& prefix,
    const EspNativeGameplayCrateTransformSnapshot& crateTransforms,
    const EspMapAutomapSnapshot& automap) {
    const uint8_t* segments[3] = {
        reinterpret_cast<const uint8_t*>(&prefix),
        reinterpret_cast<const uint8_t*>(&crateTransforms),
        reinterpret_cast<const uint8_t*>(&automap)
    };
    const size_t sizes[3] = {
        sizeof(prefix), sizeof(crateTransforms), sizeof(automap)
    };
    const size_t zeroOffset = offsetof(NativeSaveCore, recordCrc32);
    const size_t zeroEnd = zeroOffset + sizeof(uint32_t);
    uint32_t crc = 0xffffffffU;
    uint8_t segment;
    size_t i;
    uint32_t bit;

    for (segment = 0U; segment < 3U; ++segment) {
        for (i = 0U; i < sizes[segment]; ++i) {
            uint8_t value = segments[segment][i];
            if (segment == 0U && i >= zeroOffset && i < zeroEnd) value = 0U;
            crc ^= value;
            for (bit = 0U; bit < 8U; ++bit) {
                const uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
                crc = (crc >> 1) ^ (0xedb88320U & mask);
            }
        }
    }
    return ~crc;
}

uint32_t recordCrcV8(
    const NativeSaveRecordV5& prefix,
    const EspNativeGameplayCrateTransformSnapshot& crateTransforms,
    const EspMapAutomapSnapshot& automap,
    const EspNativeGameplayMonsterStateSnapshot& monsters) {
    const uint8_t* segments[4] = {
        reinterpret_cast<const uint8_t*>(&prefix),
        reinterpret_cast<const uint8_t*>(&crateTransforms),
        reinterpret_cast<const uint8_t*>(&automap),
        reinterpret_cast<const uint8_t*>(&monsters)
    };
    const size_t sizes[4] = {
        sizeof(prefix), sizeof(crateTransforms), sizeof(automap),
        sizeof(monsters)
    };
    const size_t zeroOffset = offsetof(NativeSaveCore, recordCrc32);
    const size_t zeroEnd = zeroOffset + sizeof(uint32_t);
    uint32_t crc = 0xffffffffU;
    uint8_t segment;
    size_t i;
    uint32_t bit;

    for (segment = 0U; segment < 4U; ++segment) {
        for (i = 0U; i < sizes[segment]; ++i) {
            uint8_t value = segments[segment][i];
            if (segment == 0U && i >= zeroOffset && i < zeroEnd) value = 0U;
            crc ^= value;
            for (bit = 0U; bit < 8U; ++bit) {
                const uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
                crc = (crc >> 1) ^ (0xedb88320U & mask);
            }
        }
    }
    return ~crc;
}

uint32_t countBits(const uint8_t* bits, uint32_t bytes) {
    uint32_t count = 0U;
    uint32_t i;
    uint8_t value;
    if (bits == nullptr) return 0U;
    for (i = 0U; i < bytes; ++i) {
        value = bits[i];
        while (value != 0U) {
            count += (uint32_t)(value & 1U);
            value >>= 1U;
        }
    }
    return count;
}

bool runtimeCoordinate(int32_t value) {
    return value >= 32 && value <= 2016 && (value & 63) == 32;
}

bool coreShapeValid(const NativeSaveCore& record,
                    const uint8_t expectedMagic[8],
                    uint16_t expectedVersion,
                    uint16_t expectedBytes) {
    const EspPlayerViewState& view = record.view;
    if (memcmp(record.magic, expectedMagic, 8U) != 0 ||
        record.version != expectedVersion ||
        record.recordBytes != expectedBytes ||
        record.recordCrc32 == 0U ||
        record.reserved0 != 0U ||
        !EspMapCatalog_isValidId(record.targetMapId) ||
        record.gameplayLoadMapId == 0U || record.gameplayLoadMapId > 32U ||
        record.loadType != ESP_PLAYER_SPAWN_LOAD_FRESH_MAP ||
        record.sourceBytes == 0U || record.sourceCrc32 == 0U ||
        record.runtimeFNV1a == 0U || record.playerFNV1a == 0U ||
        record.player.active != 1U ||
        view.active != 1U || view.spawnApplied != 1U ||
        view.targetMapId != record.targetMapId ||
        view.gameplayLoadMapId != record.gameplayLoadMapId ||
        view.loadType != record.loadType ||
        view.hudRefreshPending != 0U ||
        view.facingRefreshPending != 0U ||
        view.playerSetupPending != 0U ||
        view.tileEnterPending != 0U ||
        !runtimeCoordinate(view.viewX) || !runtimeCoordinate(view.viewY) ||
        view.viewX != view.destX || view.viewY != view.destY ||
        view.viewAngle != view.destAngle ||
        view.viewAngle < 0 || view.viewAngle > 255 ||
        (view.viewAngle & 63) != 0 ||
        view.viewZ != (int32_t)ESP_PLAYER_SPAWN_VIEW_Z ||
        view.viewZOld != (int32_t)ESP_PLAYER_SPAWN_VIEW_Z_OLD) {
        return false;
    }
    return true;
}

bool resourceShapeValid(
    const EspNativeGameplayPlayerResourcesSnapshot& resources,
    const NativeSaveCore& core) {
    const uint32_t maxBytes =
        ESP_NATIVE_GAMEPLAY_PLAYER_RESOURCES_SNAPSHOT_MAX_BYTES;
    uint32_t expectedBytes;
    uint32_t validTailBits;
    uint8_t validTailMask;
    uint32_t i;

    if (resources.reserved0 != 0U ||
        resources.sourceArenaFNV1a != core.runtimeFNV1a ||
        resources.targetMapId != core.targetMapId ||
        resources.spriteCount == 0U || resources.spriteCount > maxBytes * 8U ||
        resources.consumedCount > resources.spriteCount) {
        return false;
    }
    expectedBytes = (resources.spriteCount + 7U) >> 3;
    if (expectedBytes == 0U || expectedBytes > maxBytes ||
        resources.consumedBytes != expectedBytes ||
        countBits(resources.consumedBits, expectedBytes) !=
            resources.consumedCount) {
        return false;
    }
    validTailBits = resources.spriteCount & 7U;
    if (validTailBits != 0U) {
        validTailMask = (uint8_t)((1U << validTailBits) - 1U);
        if ((resources.consumedBits[expectedBytes - 1U] &
             (uint8_t)~validTailMask) != 0U) {
            return false;
        }
    }
    for (i = expectedBytes; i < maxBytes; ++i) {
        if (resources.consumedBits[i] != 0U) return false;
    }
    return true;
}

bool scriptShapeValid(const EspMapScriptStateSnapshot& script,
                      const NativeSaveCore& core) {
    uint32_t expectedEventBytes;
    uint32_t expectedRemovedBytes;
    uint32_t expectedStorageBytes;
    uint32_t validTailBits;
    uint8_t validTailMask;
    uint32_t i;

    if (script.reserved0 != 0U ||
        script.sourceArenaFNV1a != core.runtimeFNV1a ||
        script.eventCount == 0U || script.byteCodeCount == 0U) {
        return false;
    }

    expectedEventBytes = (script.eventCount + 1U) >> 1;
    expectedRemovedBytes = (script.byteCodeCount + 7U) >> 3;
    expectedStorageBytes = expectedEventBytes + expectedRemovedBytes;
    if (expectedEventBytes == 0U || expectedRemovedBytes == 0U ||
        expectedStorageBytes == 0U ||
        expectedStorageBytes > ESP_MAP_SCRIPT_STATE_SNAPSHOT_MAX_BYTES ||
        expectedEventBytes > 0xffffU || expectedRemovedBytes > 0xffffU ||
        expectedStorageBytes > 0xffffU ||
        script.eventStateBytes != expectedEventBytes ||
        script.removedCommandBytes != expectedRemovedBytes ||
        script.storageBytes != expectedStorageBytes) {
        return false;
    }

    if ((script.eventCount & 1U) != 0U &&
        (script.storage[expectedEventBytes - 1U] & 0xf0U) != 0U) {
        return false;
    }

    validTailBits = script.byteCodeCount & 7U;
    if (validTailBits != 0U) {
        validTailMask = (uint8_t)((1U << validTailBits) - 1U);
        if ((script.storage[expectedStorageBytes - 1U] &
             (uint8_t)~validTailMask) != 0U) {
            return false;
        }
    }

    for (i = expectedStorageBytes;
         i < ESP_MAP_SCRIPT_STATE_SNAPSHOT_MAX_BYTES; ++i) {
        if (script.storage[i] != 0U) return false;
    }
    return true;
}

bool lineShapeValid(const EspMapLineCheckpointSnapshot& lines,
                    const NativeSaveCore& core) {
    return EspMapLineCheckpoint_shapeValid(&lines, core.runtimeFNV1a) != 0;
}

bool actionRemovedShapeValid(
    const EspNativeGameplayActionRemovedSnapshot& actionRemoved,
    const NativeSaveCore& core) {
    const uint32_t maxBytes =
        ESP_NATIVE_GAMEPLAY_ACTION_REMOVED_SNAPSHOT_MAX_BYTES;
    uint32_t expectedBytes;
    uint32_t validTailBits;
    uint8_t validTailMask;
    uint32_t i;

    if (actionRemoved.reserved0 != 0U ||
        actionRemoved.sourceArenaFNV1a != core.runtimeFNV1a ||
        actionRemoved.targetMapId != core.targetMapId ||
        actionRemoved.spriteCount == 0U ||
        actionRemoved.spriteCount > maxBytes * 8U ||
        actionRemoved.removedCount > actionRemoved.spriteCount) {
        return false;
    }
    expectedBytes = (actionRemoved.spriteCount + 7U) >> 3U;
    if (expectedBytes == 0U || expectedBytes > maxBytes ||
        actionRemoved.removedBytes != expectedBytes ||
        countBits(actionRemoved.removedBits, expectedBytes) !=
            actionRemoved.removedCount ||
        actionRemoved.stateFNV1a !=
            fnv1aBytes(actionRemoved.removedBits, expectedBytes)) {
        return false;
    }
    validTailBits = actionRemoved.spriteCount & 7U;
    if (validTailBits != 0U) {
        validTailMask = (uint8_t)((1U << validTailBits) - 1U);
        if ((actionRemoved.removedBits[expectedBytes - 1U] &
             (uint8_t)~validTailMask) != 0U) {
            return false;
        }
    }
    for (i = expectedBytes; i < maxBytes; ++i) {
        if (actionRemoved.removedBits[i] != 0U) return false;
    }
    return true;
}

bool recordV1Valid(const NativeSaveCore& record) {
    return coreShapeValid(record, kMagicV1, kVersionV1,
                          (uint16_t)sizeof(NativeSaveCore)) &&
           record.recordCrc32 == recordCrcV1(record);
}

bool recordV2Valid(const NativeSaveRecordV2& record) {
    return coreShapeValid(record.core, kMagicV2, kVersionV2,
                          (uint16_t)sizeof(NativeSaveRecordV2)) &&
           record.core.recordCrc32 == recordCrcV2(record) &&
           resourceShapeValid(record.resources, record.core);
}

bool recordV3Valid(const NativeSaveRecordV3& record) {
    return coreShapeValid(record.core, kMagicV3, kVersionV3,
                          (uint16_t)sizeof(NativeSaveRecordV3)) &&
           record.core.recordCrc32 == recordCrcV3(record) &&
           resourceShapeValid(record.resources, record.core) &&
           scriptShapeValid(record.script, record.core);
}

bool recordV4Valid(const NativeSaveRecordV4& record) {
    return coreShapeValid(record.core, kMagicV4, kVersionV4,
                          (uint16_t)sizeof(NativeSaveRecordV4)) &&
           record.core.recordCrc32 == recordCrcV4(record) &&
           resourceShapeValid(record.resources, record.core) &&
           scriptShapeValid(record.script, record.core) &&
           lineShapeValid(record.lines, record.core);
}

bool recordV5Valid(const NativeSaveRecordV5& record) {
    return coreShapeValid(record.core, kMagicV5, kVersionV5,
                          (uint16_t)sizeof(NativeSaveRecordV5)) &&
           record.core.recordCrc32 == recordCrcV5(record) &&
           resourceShapeValid(record.resources, record.core) &&
           scriptShapeValid(record.script, record.core) &&
           lineShapeValid(record.lines, record.core) &&
           actionRemovedShapeValid(record.actionRemoved, record.core);
}

bool loadedV5Valid(const LoadedSaveRecord& record) {
    return coreShapeValid(record.core, kMagicV5, kVersionV5,
                          (uint16_t)sizeof(NativeSaveRecordV5)) &&
           record.core.recordCrc32 ==
               recordCrcBytes(reinterpret_cast<const uint8_t*>(&record),
                              sizeof(NativeSaveRecordV5)) &&
           resourceShapeValid(record.resources, record.core) &&
           scriptShapeValid(record.script, record.core) &&
           lineShapeValid(record.lines, record.core) &&
           actionRemovedShapeValid(record.actionRemoved, record.core);
}

bool crateTransformsDisjoint(
    const EspNativeGameplayActionRemovedSnapshot& actionRemoved,
    const EspNativeGameplayCrateTransformSnapshot& crateTransforms) {
    uint32_t i;
    if (actionRemoved.spriteCount != crateTransforms.spriteCount ||
        actionRemoved.removedBytes != crateTransforms.transformedBytes) {
        return false;
    }
    for (i = 0U; i < actionRemoved.removedBytes; ++i) {
        if ((actionRemoved.removedBits[i] &
             crateTransforms.transformedBits[i]) != 0U) {
            return false;
        }
    }
    return true;
}

bool automapShapeValid(const EspMapAutomapSnapshot& automap,
                       const NativeSaveCore& core) {
    uint32_t expectedLineBytes;
    uint32_t expectedSpriteBytes;
    uint32_t validTailBits;
    uint8_t validTailMask;
    uint32_t i;

    if (automap.sourceArenaFNV1a != core.runtimeFNV1a ||
        automap.lineCount == 0U ||
        automap.lineCount > ESP_MAP_AUTOMAP_SNAPSHOT_MAX_OBJECTS ||
        automap.spriteCount == 0U ||
        automap.spriteCount > ESP_MAP_AUTOMAP_SNAPSHOT_MAX_OBJECTS ||
        automap.lineRevealedCount > automap.lineCount ||
        automap.spriteRevealedCount > automap.spriteCount) {
        return false;
    }

    expectedLineBytes = (automap.lineCount + 7U) >> 3U;
    expectedSpriteBytes = (automap.spriteCount + 7U) >> 3U;
    if (automap.lineBitsetBytes != expectedLineBytes ||
        automap.spriteBitsetBytes != expectedSpriteBytes ||
        expectedLineBytes == 0U ||
        expectedSpriteBytes == 0U ||
        expectedLineBytes > ESP_MAP_AUTOMAP_SNAPSHOT_MAX_BYTES ||
        expectedSpriteBytes > ESP_MAP_AUTOMAP_SNAPSHOT_MAX_BYTES ||
        countBits(automap.lineBits, expectedLineBytes) !=
            automap.lineRevealedCount ||
        countBits(automap.spriteBits, expectedSpriteBytes) !=
            automap.spriteRevealedCount) {
        return false;
    }

    validTailBits = automap.lineCount & 7U;
    if (validTailBits != 0U) {
        validTailMask = (uint8_t)((1U << validTailBits) - 1U);
        if ((automap.lineBits[expectedLineBytes - 1U] &
             (uint8_t)~validTailMask) != 0U) {
            return false;
        }
    }
    validTailBits = automap.spriteCount & 7U;
    if (validTailBits != 0U) {
        validTailMask = (uint8_t)((1U << validTailBits) - 1U);
        if ((automap.spriteBits[expectedSpriteBytes - 1U] &
             (uint8_t)~validTailMask) != 0U) {
            return false;
        }
    }

    for (i = expectedLineBytes;
         i < ESP_MAP_AUTOMAP_SNAPSHOT_MAX_BYTES; ++i) {
        if (automap.lineBits[i] != 0U) return false;
    }
    for (i = expectedSpriteBytes;
         i < ESP_MAP_AUTOMAP_SNAPSHOT_MAX_BYTES; ++i) {
        if (automap.spriteBits[i] != 0U) return false;
    }
    return true;
}

bool loadedV6Valid(
    const LoadedSaveRecord& record,
    const EspNativeGameplayCrateTransformSnapshot& crateTransforms) {
    return coreShapeValid(record.core, kMagicV6, kVersionV6,
                          (uint16_t)kRecordBytesV6) &&
           record.core.recordCrc32 ==
               recordCrcV6(
                   *reinterpret_cast<const NativeSaveRecordV5*>(&record),
                   crateTransforms) &&
           resourceShapeValid(record.resources, record.core) &&
           scriptShapeValid(record.script, record.core) &&
           lineShapeValid(record.lines, record.core) &&
           actionRemovedShapeValid(record.actionRemoved, record.core) &&
           EspNativeGameplayCrateState_snapshotFileShapeValid(
               &crateTransforms, record.core.runtimeFNV1a,
               record.core.targetMapId) &&
           crateTransformsDisjoint(record.actionRemoved, crateTransforms);
}

bool recordV6Valid(
    const NativeSaveRecordV5& prefix,
    const EspNativeGameplayCrateTransformSnapshot& crateTransforms) {
    return coreShapeValid(prefix.core, kMagicV6, kVersionV6,
                          (uint16_t)kRecordBytesV6) &&
           prefix.core.recordCrc32 == recordCrcV6(prefix, crateTransforms) &&
           resourceShapeValid(prefix.resources, prefix.core) &&
           scriptShapeValid(prefix.script, prefix.core) &&
           lineShapeValid(prefix.lines, prefix.core) &&
           actionRemovedShapeValid(prefix.actionRemoved, prefix.core) &&
           EspNativeGameplayCrateState_snapshotShapeValid(
               &crateTransforms, prefix.core.runtimeFNV1a,
               prefix.core.targetMapId) &&
           crateTransformsDisjoint(prefix.actionRemoved, crateTransforms);
}

bool loadedV7Valid(
    const LoadedSaveRecord& record,
    const EspNativeGameplayCrateTransformSnapshot& crateTransforms,
    const EspMapAutomapSnapshot& automap) {
    return coreShapeValid(record.core, kMagicV7, kVersionV7,
                          (uint16_t)kRecordBytesV7) &&
           record.core.recordCrc32 ==
               recordCrcV7(
                   *reinterpret_cast<const NativeSaveRecordV5*>(&record),
                   crateTransforms, automap) &&
           resourceShapeValid(record.resources, record.core) &&
           scriptShapeValid(record.script, record.core) &&
           lineShapeValid(record.lines, record.core) &&
           actionRemovedShapeValid(record.actionRemoved, record.core) &&
           EspNativeGameplayCrateState_snapshotFileShapeValid(
               &crateTransforms, record.core.runtimeFNV1a,
               record.core.targetMapId) &&
           crateTransformsDisjoint(record.actionRemoved, crateTransforms) &&
           automapShapeValid(automap, record.core);
}

bool recordV7Valid(
    const NativeSaveRecordV5& prefix,
    const EspNativeGameplayCrateTransformSnapshot& crateTransforms,
    const EspMapAutomapSnapshot& automap) {
    return coreShapeValid(prefix.core, kMagicV7, kVersionV7,
                          (uint16_t)kRecordBytesV7) &&
           prefix.core.recordCrc32 ==
               recordCrcV7(prefix, crateTransforms, automap) &&
           resourceShapeValid(prefix.resources, prefix.core) &&
           scriptShapeValid(prefix.script, prefix.core) &&
           lineShapeValid(prefix.lines, prefix.core) &&
           actionRemovedShapeValid(prefix.actionRemoved, prefix.core) &&
           EspNativeGameplayCrateState_snapshotShapeValid(
               &crateTransforms, prefix.core.runtimeFNV1a,
               prefix.core.targetMapId) &&
           crateTransformsDisjoint(prefix.actionRemoved, crateTransforms) &&
           automapShapeValid(automap, prefix.core);
}

bool loadedV8Valid(
    const LoadedSaveRecord& record,
    const EspNativeGameplayCrateTransformSnapshot& crateTransforms,
    const EspMapAutomapSnapshot& automap,
    const EspNativeGameplayMonsterStateSnapshot& monsters) {
    const bool coreOk =
        coreShapeValid(record.core, kMagicV8, kVersionV8,
                       (uint16_t)kRecordBytesV8);
    const uint32_t actualCrc =
        recordCrcV8(*reinterpret_cast<const NativeSaveRecordV5*>(&record),
                    crateTransforms, automap, monsters);
    const bool crcOk = record.core.recordCrc32 == actualCrc;
    const bool resourcesOk = resourceShapeValid(record.resources, record.core);
    const bool scriptOk = scriptShapeValid(record.script, record.core);
    const bool linesOk = lineShapeValid(record.lines, record.core);
    const bool removedOk =
        actionRemovedShapeValid(record.actionRemoved, record.core);
    const bool cratesOk =
        EspNativeGameplayCrateState_snapshotFileShapeValid(
            &crateTransforms, record.core.runtimeFNV1a,
            record.core.targetMapId) != 0;
    const bool disjointOk =
        crateTransformsDisjoint(record.actionRemoved, crateTransforms);
    const bool automapOk = automapShapeValid(automap, record.core);
    const bool monstersOk =
        EspNativeGameplayMonsterState_snapshotShapeValid(
            &monsters, record.core.runtimeFNV1a) != 0;
    const bool valid = coreOk && crcOk && resourcesOk && scriptOk && linesOk &&
                       removedOk && cratesOk && disjointOk && automapOk &&
                       monstersOk;
    if (!valid) {
        printf("[NATIVESAVE] V8-VALIDATE core=%u crc=%u storedCrc=%08x actualCrc=%08x resources=%u script=%u lines=%u removed=%u cratesFile=%u disjoint=%u automap=%u monsters=%u catalogResolution=deferred failClosed=yes\n",
               coreOk ? 1U : 0U,
               crcOk ? 1U : 0U,
               (unsigned int)record.core.recordCrc32,
               (unsigned int)actualCrc,
               resourcesOk ? 1U : 0U,
               scriptOk ? 1U : 0U,
               linesOk ? 1U : 0U,
               removedOk ? 1U : 0U,
               cratesOk ? 1U : 0U,
               disjointOk ? 1U : 0U,
               automapOk ? 1U : 0U,
               monstersOk ? 1U : 0U);
    }
    return valid;
}

bool recordV8Valid(
    const NativeSaveRecordV5& prefix,
    const EspNativeGameplayCrateTransformSnapshot& crateTransforms,
    const EspMapAutomapSnapshot& automap,
    const EspNativeGameplayMonsterStateSnapshot& monsters) {
    return coreShapeValid(prefix.core, kMagicV8, kVersionV8,
                          (uint16_t)kRecordBytesV8) &&
           prefix.core.recordCrc32 ==
               recordCrcV8(prefix, crateTransforms, automap, monsters) &&
           resourceShapeValid(prefix.resources, prefix.core) &&
           scriptShapeValid(prefix.script, prefix.core) &&
           lineShapeValid(prefix.lines, prefix.core) &&
           actionRemovedShapeValid(prefix.actionRemoved, prefix.core) &&
           EspNativeGameplayCrateState_snapshotShapeValid(
               &crateTransforms, prefix.core.runtimeFNV1a,
               prefix.core.targetMapId) &&
           crateTransformsDisjoint(prefix.actionRemoved, crateTransforms) &&
           automapShapeValid(automap, prefix.core) &&
           EspNativeGameplayMonsterState_snapshotShapeValid(
               &monsters, prefix.core.runtimeFNV1a);
}

bool readRecordPath(const char* path, LoadedSaveRecord* outRecord) {
    File file;
    size_t got;
    size_t fileBytes;

    if (path == nullptr || outRecord == nullptr || !SD.exists(path)) return false;
    file = SD.open(path, FILE_READ);
    if (!file) return false;
    fileBytes = (size_t)file.size();
    memset(outRecord, 0, sizeof(*outRecord));

    if (fileBytes == sizeof(NativeSaveCore)) {
        NativeSaveCore v1;
        memset(&v1, 0, sizeof(v1));
        got = file.read(reinterpret_cast<uint8_t*>(&v1), sizeof(v1));
        file.close();
        if (got != sizeof(v1) || !recordV1Valid(v1)) return false;
        outRecord->core = v1;
        outRecord->fileBytes = (uint16_t)sizeof(v1);
        return true;
    }

    if (fileBytes == sizeof(NativeSaveRecordV2)) {
        NativeSaveRecordV2 v2;
        memset(&v2, 0, sizeof(v2));
        got = file.read(reinterpret_cast<uint8_t*>(&v2), sizeof(v2));
        file.close();
        if (got != sizeof(v2) || !recordV2Valid(v2)) return false;
        outRecord->core = v2.core;
        outRecord->resources = v2.resources;
        outRecord->fileBytes = (uint16_t)sizeof(v2);
        outRecord->hasResources = 1U;
        return true;
    }

    if (fileBytes == sizeof(NativeSaveRecordV3)) {
        NativeSaveRecordV3 v3;
        memset(&v3, 0, sizeof(v3));
        got = file.read(reinterpret_cast<uint8_t*>(&v3), sizeof(v3));
        file.close();
        if (got != sizeof(v3) || !recordV3Valid(v3)) return false;
        outRecord->core = v3.core;
        outRecord->resources = v3.resources;
        outRecord->script = v3.script;
        outRecord->fileBytes = (uint16_t)sizeof(v3);
        outRecord->hasResources = 1U;
        outRecord->hasScript = 1U;
        return true;
    }

    if (fileBytes == sizeof(NativeSaveRecordV4)) {
        NativeSaveRecordV4 v4;
        memset(&v4, 0, sizeof(v4));
        got = file.read(reinterpret_cast<uint8_t*>(&v4), sizeof(v4));
        file.close();
        if (got != sizeof(v4) || !recordV4Valid(v4)) return false;
        outRecord->core = v4.core;
        outRecord->resources = v4.resources;
        outRecord->script = v4.script;
        outRecord->lines = v4.lines;
        outRecord->fileBytes = (uint16_t)sizeof(v4);
        outRecord->hasResources = 1U;
        outRecord->hasScript = 1U;
        outRecord->hasLines = 1U;
        return true;
    }

    if (fileBytes == sizeof(NativeSaveRecordV5)) {
        got = file.read(reinterpret_cast<uint8_t*>(outRecord),
                        sizeof(NativeSaveRecordV5));
        file.close();
        if (got != sizeof(NativeSaveRecordV5) || !loadedV5Valid(*outRecord)) {
            memset(outRecord, 0, sizeof(*outRecord));
            return false;
        }
        outRecord->fileBytes = (uint16_t)sizeof(NativeSaveRecordV5);
        outRecord->hasResources = 1U;
        outRecord->hasScript = 1U;
        outRecord->hasLines = 1U;
        outRecord->hasActionRemoved = 1U;
        return true;
    }

    if (fileBytes == kRecordBytesV6) {
        EspNativeGameplayCrateTransformSnapshot crateTransforms;
        memset(&crateTransforms, 0, sizeof(crateTransforms));
        got = file.read(reinterpret_cast<uint8_t*>(outRecord),
                        sizeof(NativeSaveRecordV5));
        if (got == sizeof(NativeSaveRecordV5)) {
            got = file.read(reinterpret_cast<uint8_t*>(&crateTransforms),
                            sizeof(crateTransforms));
        }
        file.close();
        if (got != sizeof(crateTransforms) ||
            !loadedV6Valid(*outRecord, crateTransforms)) {
            memset(outRecord, 0, sizeof(*outRecord));
            return false;
        }
        outRecord->fileBytes = (uint16_t)kRecordBytesV6;
        outRecord->hasResources = 1U;
        outRecord->hasScript = 1U;
        outRecord->hasLines = 1U;
        outRecord->hasActionRemoved = 1U;
        return true;
    }

    if (fileBytes == kRecordBytesV7) {
        EspNativeGameplayCrateTransformSnapshot crateTransforms;
        EspMapAutomapSnapshot automap;
        memset(&crateTransforms, 0, sizeof(crateTransforms));
        memset(&automap, 0, sizeof(automap));
        got = file.read(reinterpret_cast<uint8_t*>(outRecord),
                        sizeof(NativeSaveRecordV5));
        if (got == sizeof(NativeSaveRecordV5)) {
            got = file.read(reinterpret_cast<uint8_t*>(&crateTransforms),
                            sizeof(crateTransforms));
        }
        if (got == sizeof(crateTransforms)) {
            got = file.read(reinterpret_cast<uint8_t*>(&automap),
                            sizeof(automap));
        }
        file.close();
        if (got != sizeof(automap) ||
            !loadedV7Valid(*outRecord, crateTransforms, automap)) {
            memset(outRecord, 0, sizeof(*outRecord));
            return false;
        }
        outRecord->fileBytes = (uint16_t)kRecordBytesV7;
        outRecord->hasResources = 1U;
        outRecord->hasScript = 1U;
        outRecord->hasLines = 1U;
        outRecord->hasActionRemoved = 1U;
        outRecord->hasAutomap = 1U;
        return true;
    }

    if (fileBytes == kRecordBytesV8) {
        EspNativeGameplayCrateTransformSnapshot crateTransforms;
        EspMapAutomapSnapshot automap;
        EspNativeGameplayMonsterStateSnapshot* monsters;

        /*
         * Cold MENU_MAIN has substantially less contiguous 8-bit heap than a
         * resident gameplay session. The SD File handle itself owns temporary
         * heap, so reserving the 1612-byte V8 monster workspace while that
         * handle is open can spuriously turn a valid checkpoint into "No Save".
         *
         * We already know the exact file length here. Close the size-probe
         * handle first, reserve the bounded monster workspace, then reopen for
         * the exact read. Validation and on-disk format remain unchanged.
         */
        file.close();
        monsters =
            (EspNativeGameplayMonsterStateSnapshot*)malloc(sizeof(*monsters));
        if (monsters == nullptr) {
            printf("[NATIVESAVE] READABLE-V8 FAILED path=%s stage=monster-workspace bytes=%u failClosed=yes\n",
                   path,
                   (unsigned int)sizeof(*monsters));
            return false;
        }
        file = SD.open(path, FILE_READ);
        if (!file || (size_t)file.size() != kRecordBytesV8) {
            if (file) file.close();
            free(monsters);
            printf("[NATIVESAVE] READABLE-V8 FAILED path=%s stage=reopen expectedBytes=%u failClosed=yes\n",
                   path,
                   (unsigned int)kRecordBytesV8);
            return false;
        }

        memset(&crateTransforms, 0, sizeof(crateTransforms));
        memset(&automap, 0, sizeof(automap));
        memset(monsters, 0, sizeof(*monsters));
        got = file.read(reinterpret_cast<uint8_t*>(outRecord),
                        sizeof(NativeSaveRecordV5));
        if (got == sizeof(NativeSaveRecordV5)) {
            got = file.read(reinterpret_cast<uint8_t*>(&crateTransforms),
                            sizeof(crateTransforms));
        }
        if (got == sizeof(crateTransforms)) {
            got = file.read(reinterpret_cast<uint8_t*>(&automap),
                            sizeof(automap));
        }
        if (got == sizeof(automap)) {
            got = file.read(reinterpret_cast<uint8_t*>(monsters),
                            sizeof(*monsters));
        }
        file.close();
        const bool valid =
            got == sizeof(*monsters) &&
            loadedV8Valid(*outRecord, crateTransforms, automap, *monsters);
        printf("[NATIVESAVE] READABLE-V8 path=%s bytes=%u monsterWorkspace=%u allocation=before-reopen crateValidation=file-shape/catalog-deferred result=%s\n",
               path,
               (unsigned int)fileBytes,
               (unsigned int)sizeof(*monsters),
               valid ? "valid" : "invalid");
        free(monsters);
        if (!valid) {
            memset(outRecord, 0, sizeof(*outRecord));
            return false;
        }
        outRecord->fileBytes = (uint16_t)kRecordBytesV8;
        outRecord->hasResources = 1U;
        outRecord->hasScript = 1U;
        outRecord->hasLines = 1U;
        outRecord->hasActionRemoved = 1U;
        outRecord->hasAutomap = 1U;
        return true;
    }

    file.close();
    return false;
}

bool readBestRecord(LoadedSaveRecord* outRecord, bool* outRecoveredBackup) {
    if (outRecoveredBackup != nullptr) *outRecoveredBackup = false;
    if (readRecordPath(kSavePath, outRecord)) return true;
    if (!readRecordPath(kBackupPath, outRecord)) return false;
    if (outRecoveredBackup != nullptr) *outRecoveredBackup = true;
    return true;
}

bool writeExactV8(
    const char* path,
    const NativeSaveRecordV5& prefix,
    const EspNativeGameplayCrateTransformSnapshot& crateTransforms,
    const EspMapAutomapSnapshot& automap,
    const EspNativeGameplayMonsterStateSnapshot& monsters) {
    File file = SD.open(path, FILE_WRITE);
    size_t wrotePrefix;
    size_t wroteCrate;
    size_t wroteAutomap;
    size_t wroteMonsters;
    if (!file) return false;
    wrotePrefix = file.write(reinterpret_cast<const uint8_t*>(&prefix),
                             sizeof(prefix));
    wroteCrate = file.write(reinterpret_cast<const uint8_t*>(&crateTransforms),
                            sizeof(crateTransforms));
    wroteAutomap = file.write(reinterpret_cast<const uint8_t*>(&automap),
                              sizeof(automap));
    wroteMonsters = file.write(reinterpret_cast<const uint8_t*>(&monsters),
                               sizeof(monsters));
    file.flush();
    file.close();
    return wrotePrefix == sizeof(prefix) &&
           wroteCrate == sizeof(crateTransforms) &&
           wroteAutomap == sizeof(automap) &&
           wroteMonsters == sizeof(monsters);
}

bool readExactV8Matches(
    const char* path,
    const NativeSaveRecordV5& expectedPrefix,
    const EspNativeGameplayCrateTransformSnapshot& expectedCrate,
    const EspMapAutomapSnapshot& expectedAutomap,
    const EspNativeGameplayMonsterStateSnapshot& expectedMonsters) {
    File file;
    uint8_t verify[64];
    const uint8_t* segments[4] = {
        reinterpret_cast<const uint8_t*>(&expectedPrefix),
        reinterpret_cast<const uint8_t*>(&expectedCrate),
        reinterpret_cast<const uint8_t*>(&expectedAutomap),
        reinterpret_cast<const uint8_t*>(&expectedMonsters)
    };
    const size_t sizes[4] = {
        sizeof(expectedPrefix), sizeof(expectedCrate), sizeof(expectedAutomap),
        sizeof(expectedMonsters)
    };
    uint8_t segment;

    if (path == nullptr || !SD.exists(path)) return false;
    file = SD.open(path, FILE_READ);
    if (!file || (size_t)file.size() != kRecordBytesV8) {
        if (file) file.close();
        return false;
    }

    for (segment = 0U; segment < 4U; ++segment) {
        size_t offset = 0U;
        while (offset < sizes[segment]) {
            size_t chunk = sizes[segment] - offset;
            size_t got;
            if (chunk > sizeof(verify)) chunk = sizeof(verify);
            got = file.read(verify, chunk);
            if (got != chunk ||
                memcmp(verify, segments[segment] + offset, chunk) != 0) {
                file.close();
                return false;
            }
            offset += chunk;
        }
    }
    file.close();
    return true;
}

bool commitRecordAtomic(
    const NativeSaveRecordV5& prefix,
    const EspNativeGameplayCrateTransformSnapshot& crateTransforms,
    const EspMapAutomapSnapshot& automap,
    const EspNativeGameplayMonsterStateSnapshot& monsters) {
    bool movedOld = false;

    if (SD.exists(kTempPath)) (void)SD.remove(kTempPath);
    if (SD.exists(kBackupPath)) (void)SD.remove(kBackupPath);
    if (!writeExactV8(kTempPath, prefix, crateTransforms, automap, monsters) ||
        !readExactV8Matches(
            kTempPath, prefix, crateTransforms, automap, monsters)) {
        (void)SD.remove(kTempPath);
        return false;
    }

    if (SD.exists(kSavePath)) {
        if (!SD.rename(kSavePath, kBackupPath)) {
            (void)SD.remove(kTempPath);
            return false;
        }
        movedOld = true;
    }

    if (!SD.rename(kTempPath, kSavePath)) {
        if (movedOld && !SD.exists(kSavePath)) {
            (void)SD.rename(kBackupPath, kSavePath);
        }
        (void)SD.remove(kTempPath);
        return false;
    }

    if (!readExactV8Matches(
            kSavePath, prefix, crateTransforms, automap, monsters)) {
        (void)SD.remove(kSavePath);
        if (movedOld && SD.exists(kBackupPath)) {
            (void)SD.rename(kBackupPath, kSavePath);
        }
        return false;
    }

    if (SD.exists(kBackupPath)) (void)SD.remove(kBackupPath);
    return true;
}

bool readCrateSection(
    const char* path,
    const NativeSaveCore& core,
    EspNativeGameplayCrateTransformSnapshot* outSnapshot) {
    File file;
    size_t got;
    if (path == nullptr || outSnapshot == nullptr || !SD.exists(path) ||
        (core.version != kVersionV6 && core.version != kVersionV7 &&
         core.version != kVersionV8)) {
        return false;
    }
    const size_t expectedBytes =
        core.version == kVersionV8
            ? kRecordBytesV8
            : (core.version == kVersionV7 ? kRecordBytesV7 : kRecordBytesV6);
    file = SD.open(path, FILE_READ);
    if (!file || (size_t)file.size() != expectedBytes ||
        !file.seek(sizeof(NativeSaveRecordV5))) {
        if (file) file.close();
        return false;
    }
    memset(outSnapshot, 0, sizeof(*outSnapshot));
    got = file.read(reinterpret_cast<uint8_t*>(outSnapshot),
                    sizeof(*outSnapshot));
    file.close();
    return got == sizeof(*outSnapshot) &&
           EspNativeGameplayCrateState_snapshotShapeValid(
               outSnapshot, core.runtimeFNV1a, core.targetMapId);
}

bool restoreCrateSection(
    const char* path,
    const NativeSaveCore& core,
    uint16_t* outCount,
    uint32_t* outFNV) {
    EspNativeGameplayCrateTransformSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    if (!readCrateSection(path, core, &snapshot) ||
        !EspNativeGameplayCrateState_restore(&snapshot) ||
        EspNativeGameplayCrateState_fingerprint() != snapshot.stateFNV1a) {
        return false;
    }
    if (outCount != nullptr) *outCount = snapshot.transformedCount;
    if (outFNV != nullptr) *outFNV = snapshot.stateFNV1a;
    return true;
}

static uint8_t snapshotBit(const uint8_t* bits, uint32_t index) {
    return (uint8_t)((bits[index >> 3U] >> (index & 7U)) & 1U);
}

bool readAutomapSection(
    const char* path,
    const NativeSaveCore& core,
    EspMapAutomapSnapshot* outSnapshot) {
    File file;
    size_t got;
    if (path == nullptr || outSnapshot == nullptr || !SD.exists(path) ||
        (core.version != kVersionV7 && core.version != kVersionV8)) {
        return false;
    }
    const size_t expectedBytes =
        core.version == kVersionV8 ? kRecordBytesV8 : kRecordBytesV7;
    file = SD.open(path, FILE_READ);
    if (!file || (size_t)file.size() != expectedBytes ||
        !file.seek(kRecordBytesV6)) {
        if (file) file.close();
        return false;
    }
    memset(outSnapshot, 0, sizeof(*outSnapshot));
    got = file.read(reinterpret_cast<uint8_t*>(outSnapshot),
                    sizeof(*outSnapshot));
    file.close();
    return got == sizeof(*outSnapshot) &&
           automapShapeValid(*outSnapshot, core);
}

bool restoreAutomapSection(
    const char* path,
    const NativeSaveCore& core,
    uint16_t* outLineCount,
    uint16_t* outSpriteCount,
    uint16_t* outVisitedCount,
    uint32_t* outFNV) {
    EspMapAutomapSnapshot snapshot;
    const EspMapAutomapStateView* view;
    uint32_t expectedFNV;
    uint32_t i;
    uint16_t visitedCount = 0U;

    memset(&snapshot, 0, sizeof(snapshot));
    if (!readAutomapSection(path, core, &snapshot)) return false;
    expectedFNV = automapSnapshotFNV(snapshot);
    if (expectedFNV == 0U ||
        !EspMapAutomapState_restore(&snapshot)) {
        return false;
    }

    view = EspMapAutomapState_view();
    if (view == nullptr ||
        view->stateFNV1a != expectedFNV ||
        view->lineRevealedCount != snapshot.lineRevealedCount ||
        view->spriteRevealedCount != snapshot.spriteRevealedCount) {
        return false;
    }

    for (i = 0U; i < ESP_MAP_STATE_TILE_COUNT; ++i) {
        uint8_t flags;
        const uint8_t expectedVisited = snapshotBit(snapshot.visitedBits, i);
        if (!EspMapState_getTileFlags(i, &flags) ||
            (((flags & ESP_MAP_TILE_VISITED) != 0U) ? 1U : 0U) !=
                expectedVisited) {
            return false;
        }
        if (expectedVisited != 0U && visitedCount != UINT16_MAX) {
            ++visitedCount;
        }
    }

    if (outLineCount != nullptr)
        *outLineCount = snapshot.lineRevealedCount;
    if (outSpriteCount != nullptr)
        *outSpriteCount = snapshot.spriteRevealedCount;
    if (outVisitedCount != nullptr)
        *outVisitedCount = visitedCount;
    if (outFNV != nullptr) *outFNV = expectedFNV;
    return true;
}

bool stageV8MonsterSection(
    const char* path,
    const NativeSaveCore& core,
    uint16_t* outCount,
    uint32_t* outFNV) {
    File file;
    size_t got;
    EspNativeGameplayMonsterStateSnapshot* snapshot;

    if (path == nullptr || !SD.exists(path) ||
        core.version != kVersionV8) {
        return false;
    }
    snapshot = (EspNativeGameplayMonsterStateSnapshot*)malloc(
        sizeof(*snapshot));
    if (snapshot == nullptr) return false;
    memset(snapshot, 0, sizeof(*snapshot));

    file = SD.open(path, FILE_READ);
    if (!file || (size_t)file.size() != kRecordBytesV8 ||
        !file.seek(kRecordBytesV7)) {
        if (file) file.close();
        free(snapshot);
        return false;
    }
    got = file.read(reinterpret_cast<uint8_t*>(snapshot), sizeof(*snapshot));
    file.close();

    const bool ok =
        got == sizeof(*snapshot) &&
        EspNativeGameplayMonsterState_snapshotShapeValid(
            snapshot, core.runtimeFNV1a) &&
        EspNativeGameplayMonsterState_stageRestore(snapshot);
    if (ok) {
        if (outCount != nullptr) *outCount = snapshot->count;
        if (outFNV != nullptr) *outFNV = snapshot->stateFNV1a;
    }
    free(snapshot);
    return ok;
}

bool captureRecord(
    NativeSaveRecordV5* outPrefix,
    EspNativeGameplayCrateTransformSnapshot* outCrateTransforms,
    EspMapAutomapSnapshot* outAutomap,
    EspNativeGameplayMonsterStateSnapshot* outMonsters) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspPlayerViewState* view = EspPlayerView_view();

    if (outPrefix == nullptr || outCrateTransforms == nullptr ||
        outAutomap == nullptr || outMonsters == nullptr ||
        EspAssetPack_isOpen() || runtime == nullptr || view == nullptr ||
        !EspMapResidentLifecycle_isReady()) {
        return false;
    }

    NativeSaveRecordV5& record = *outPrefix;
    memset(outPrefix, 0, sizeof(*outPrefix));
    memset(outCrateTransforms, 0, sizeof(*outCrateTransforms));
    memset(outAutomap, 0, sizeof(*outAutomap));
    memset(outMonsters, 0, sizeof(*outMonsters));
    if (!EspNativeGameplayPlayerState_snapshot(&record.core.player) ||
        !EspNativeGameplayPlayerResources_snapshot(&record.resources) ||
        !EspMapScriptState_snapshot(&record.script) ||
        !EspMapLineCheckpoint_snapshot(&record.lines) ||
        !EspNativeGameplayActionEngine_snapshotRemoved(&record.actionRemoved) ||
        !EspNativeGameplayCrateState_snapshot(outCrateTransforms) ||
        !EspMapAutomapState_snapshot(outAutomap) ||
        !EspNativeGameplayMonsterState_snapshot(outMonsters)) {
        return false;
    }
    if (view->active != 1U || view->targetMapId == 0U ||
        runtime->sourceBytes == 0U || runtime->sourceCrc32 == 0U ||
        runtime->arenaFNV1a == 0U) {
        return false;
    }

    memcpy(record.core.magic, kMagicV8, sizeof(kMagicV8));
    record.core.version = kVersionV8;
    record.core.recordBytes = (uint16_t)kRecordBytesV8;
    record.core.sourceBytes = runtime->sourceBytes;
    record.core.sourceCrc32 = runtime->sourceCrc32;
    record.core.runtimeFNV1a = runtime->arenaFNV1a;
    record.core.playerFNV1a = EspNativeGameplayPlayerState_fingerprint();
    record.core.targetMapId = view->targetMapId;
    record.core.gameplayLoadMapId = view->gameplayLoadMapId;
    record.core.loadType = view->loadType;
    record.core.view = *view;
    record.core.recordCrc32 =
        recordCrcV8(record, *outCrateTransforms, *outAutomap, *outMonsters);
    return recordV8Valid(
        record, *outCrateTransforms, *outAutomap, *outMonsters);
}

bool saveNow(void) {
    NativeSaveRecordV5& record = saveWorkspace.write;
    EspNativeGameplayCrateTransformSnapshot crateTransforms;
    EspMapAutomapSnapshot automap;
    EspNativeGameplayMonsterStateSnapshot* monsters =
        (EspNativeGameplayMonsterStateSnapshot*)malloc(sizeof(*monsters));
    uint32_t scriptFNV;
    uint32_t openCount;
    uint32_t lockedCount;
    uint32_t texture10Count;
    uint32_t removedCount;
    uint32_t automapFNV;
    uint32_t automapVisitedCount;
    uint16_t monsterCount = 0U;
    uint32_t monsterFNV = 0U;

    if (monsters == nullptr) {
        printf("[NATIVESAVE] SAVE-FAILED path=%s version=8 stage=monster-workspace failClosed=yes\n",
               kLogPath);
        return false;
    }

    memset(&record, 0, sizeof(record));
    memset(&crateTransforms, 0, sizeof(crateTransforms));
    memset(&automap, 0, sizeof(automap));
    memset(monsters, 0, sizeof(*monsters));
    if (!captureRecord(&record, &crateTransforms, &automap, monsters) ||
        !commitRecordAtomic(record, crateTransforms, automap, *monsters)) {
        free(monsters);
        printf("[NATIVESAVE] SAVE-FAILED path=%s version=8 sections=resources+script+lines+action-removals+crate-transforms+automap+monster-state failClosed=yes\n",
               kLogPath);
        return false;
    }
    scriptFNV = fnv1aBytes(record.script.storage, record.script.storageBytes);
    openCount = countBits(record.lines.openBits, record.lines.bitsetBytes);
    lockedCount = countBits(record.lines.lockedBits,
                            record.lines.bitsetBytes);
    texture10Count = countBits(record.lines.texture10Bits,
                               record.lines.bitsetBytes);
    removedCount = countBits(record.actionRemoved.removedBits,
                             record.actionRemoved.removedBytes);
    automapFNV = automapSnapshotFNV(automap);
    automapVisitedCount =
        countBits(automap.visitedBits, ESP_MAP_AUTOMAP_SNAPSHOT_MAX_BYTES);
    monsterCount = monsters->count;
    monsterFNV = monsters->stateFNV1a;

    printf("[NATIVESAVE] SAVE path=%s version=%u bytes=%u map=%u gameplayLoadMapId=%u pos=%ld,%ld angle=%ld playerFNV=%08lx runtimeFNV=%08lx sourceBytes=%lu sourceCrc=%08lx recordCrc=%08lx resources=%u/%uB sprites=%u script=%lu/%lu/%uB scriptFNV=%08lx lines=%lu/%uB open=%lu locked=%lu texture10=%lu lineFNV=%08lx textureFNV=%08lx actionRemoved=%lu/%uB/%08lx crateTransforms=%u/%uB/%uB/%08lx automap=%uL/%uS/%luV/%08lx monsters=%u/%08lx atomic=temp+backup+rename world=resources+script+lines+action-removals+crate-transforms+automap+monster-state-restored+monster-position+activation-fresh\n",
           kLogPath,
           (unsigned int)record.core.version,
           (unsigned int)kRecordBytesV8,
           (unsigned int)record.core.targetMapId,
           (unsigned int)record.core.gameplayLoadMapId,
           (long)record.core.view.viewX,
           (long)record.core.view.viewY,
           (long)record.core.view.viewAngle,
           (unsigned long)record.core.playerFNV1a,
           (unsigned long)record.core.runtimeFNV1a,
           (unsigned long)record.core.sourceBytes,
           (unsigned long)record.core.sourceCrc32,
           (unsigned long)record.core.recordCrc32,
           (unsigned int)record.resources.consumedCount,
           (unsigned int)record.resources.consumedBytes,
           (unsigned int)record.resources.spriteCount,
           (unsigned long)record.script.eventCount,
           (unsigned long)record.script.byteCodeCount,
           (unsigned int)record.script.storageBytes,
           (unsigned long)scriptFNV,
           (unsigned long)record.lines.lineCount,
           (unsigned int)record.lines.bitsetBytes,
           (unsigned long)openCount,
           (unsigned long)lockedCount,
           (unsigned long)texture10Count,
           (unsigned long)record.lines.lineStateFNV1a,
           (unsigned long)record.lines.textureStateFNV1a,
           (unsigned long)removedCount,
           (unsigned int)record.actionRemoved.removedBytes,
           (unsigned long)record.actionRemoved.stateFNV1a,
           (unsigned int)crateTransforms.transformedCount,
           (unsigned int)crateTransforms.transformedBytes,
           (unsigned int)((crateTransforms.transformedCount + 1U) >> 1U),
           (unsigned long)crateTransforms.stateFNV1a,
           (unsigned int)automap.lineRevealedCount,
           (unsigned int)automap.spriteRevealedCount,
           (unsigned long)automapVisitedCount,
           (unsigned long)automapFNV,
           (unsigned int)monsterCount,
           (unsigned long)monsterFNV);
    free(monsters);
    return true;
}

bool inventoryForRecord(const NativeSaveCore& record,
                        EspBspInventory* outInventory) {
    const char* name;
    bool ok = false;
    if (outInventory == nullptr ||
        !EspMapCatalog_isValidId(record.targetMapId) ||
        EspAssetPack_isOpen()) {
        return false;
    }
    name = EspMapCatalog_nameForId(record.targetMapId);
    if (name == nullptr || name[0] == '\0') return false;
    memset(outInventory, 0, sizeof(*outInventory));
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) return false;
    if (EspBspReader_inventoryPackEntry(name, outInventory) &&
        outInventory->sourceBytes == record.sourceBytes &&
        outInventory->crc32 == record.sourceCrc32 &&
        outInventory->crc32 == outInventory->expectedCrc32 &&
        outInventory->consumedBytes == outInventory->sourceBytes &&
        outInventory->trailingBytes == 0U &&
        outInventory->plan.persistentBytes != 0U) {
        ok = true;
    }
    EspAssetPack_close();
    return ok && !EspAssetPack_isOpen();
}

void resetSpawnOwners(void) {
    EspPlayerView_reset();
    EspHudRefresh_reset();
    EspPlayerFreshMap_reset();
    EspPlayerInitialTile_reset();
    EspPlayerOrientation_reset();
    EspPlayerFinishRotationTile_reset();
    EspPlayerFacing_reset();
    EspHudPostLoadClear_reset();
    EspNativeGameplayDispatch_reset();
}

void resetFailedLoad(void) {
    EspNativeGameplaySession_reset();
    EspMapResidentLifecycle_resetAll();
    resetSpawnOwners();
}

bool restoreView(const NativeSaveCore& record) {
    EspPlayerSpawnState spawn;
    const EspPlayerViewState* live;
    uint32_t tileX = (uint32_t)record.view.viewX >> 6;
    uint32_t tileY = (uint32_t)record.view.viewY >> 6;
    uint32_t sourceParam;

    if (tileX >= ESP_PLAYER_SPAWN_MAP_WIDTH ||
        tileY >= ESP_PLAYER_SPAWN_MAP_WIDTH) {
        return false;
    }

    memset(&spawn, 0, sizeof(spawn));
    sourceParam = 0x80000000UL |
                  tileX |
                  (tileY << 5U) |
                  (((uint32_t)record.view.viewAngle & 255U) << 10U);
    spawn.sourceSpawnParam = sourceParam;
    spawn.tileIndex = (uint16_t)(tileY * ESP_PLAYER_SPAWN_MAP_WIDTH + tileX);
    spawn.worldX = (uint16_t)record.view.viewX;
    spawn.worldY = (uint16_t)record.view.viewY;
    spawn.tileX = (uint8_t)tileX;
    spawn.tileY = (uint8_t)tileY;
    spawn.angle = (uint8_t)record.view.viewAngle;
    spawn.viewZ = ESP_PLAYER_SPAWN_VIEW_Z;
    spawn.viewZOld = ESP_PLAYER_SPAWN_VIEW_Z_OLD;
    spawn.spawnSource = ESP_PLAYER_SPAWN_SOURCE_OVERRIDE;
    spawn.loadType = record.loadType;
    spawn.overrideUsed = 1U;
    spawn.facingRefreshPending = 1U;
    spawn.playerSetupPending = 1U;
    spawn.tileEnterPending = 1U;
    spawn.active = 1U;
    spawn.targetMapId = record.targetMapId;
    spawn.gameplayLoadMapId = record.gameplayLoadMapId;

    if (EspPlayerView_applySpawn(&spawn) != ESP_PLAYER_VIEW_APPLY_OK ||
        !EspPlayerView_consumeHudRefresh(record.targetMapId,
                                         record.gameplayLoadMapId,
                                         record.loadType) ||
        !EspPlayerView_consumePlayerSetup(record.targetMapId,
                                          record.gameplayLoadMapId,
                                          record.loadType) ||
        !EspPlayerView_consumeTileEnter(record.targetMapId,
                                        record.gameplayLoadMapId,
                                        record.loadType) ||
        !EspPlayerView_consumeFacing(record.targetMapId,
                                     record.gameplayLoadMapId,
                                     record.loadType)) {
        return false;
    }

    live = EspPlayerView_view();
    return live != nullptr &&
           memcmp(live, &record.view, sizeof(*live)) == 0;
}

bool reprimeHudOwners(const NativeSaveCore& record) {
    const EspPlayerViewState* view = EspPlayerView_view();
    EspHudRefreshStatus refreshStatus;
    EspHudPostLoadClearStatus clearStatus;
    const EspHudRefreshState* refresh;
    const EspHudPostLoadClearState* clear;

    if (view == nullptr || memcmp(view, &record.view, sizeof(*view)) != 0) {
        return false;
    }

    refreshStatus = EspHudRefresh_restorePending(view);
    clearStatus = EspHudPostLoadClear_restoreSettled(view);
    refresh = EspHudRefresh_view();
    clear = EspHudPostLoadClear_view();

    if (refreshStatus != ESP_HUD_REFRESH_OK ||
        clearStatus != ESP_HUD_POST_LOAD_CLEAR_OK ||
        refresh == nullptr || clear == nullptr ||
        refresh->targetMapId != record.targetMapId ||
        refresh->gameplayLoadMapId != record.gameplayLoadMapId ||
        refresh->loadType != record.loadType ||
        clear->targetMapId != record.targetMapId ||
        clear->gameplayLoadMapId != record.gameplayLoadMapId ||
        clear->loadType != record.loadType) {
        printf("[NATIVESAVE] REPRIME-HUD-FAILED map=%u gameplayLoadMapId=%u refreshStatus=%u clearStatus=%u refresh=%s clear=%s failClosed=yes\n",
               (unsigned int)record.targetMapId,
               (unsigned int)record.gameplayLoadMapId,
               (unsigned int)refreshStatus,
               (unsigned int)clearStatus,
               refresh != nullptr ? "ready" : "missing",
               clear != nullptr ? "ready" : "missing");
        return false;
    }

    printf("[NATIVESAVE] REPRIME-HUD map=%u gameplayLoadMapId=%u angle=%ld refresh=pending clear=ready mutation=owners-only turn=no\n",
           (unsigned int)record.targetMapId,
           (unsigned int)record.gameplayLoadMapId,
           (long)view->viewAngle);
    return true;
}

bool sessionConfigForPlayer(const EspNativeGameplayPlayerState& player,
                            EspNativeGameplaySessionConfig* outConfig) {
    const EspNativeGameplayWeaponSpec* weaponSpec = nullptr;
    uint8_t ammoType;
    EspNativeGameplaySessionConfig config;

    if (outConfig == nullptr || player.active != 1U ||
        player.weapon >= ESP_NATIVE_GAMEPLAY_PLAYER_WEAPON_LIMIT) {
        return false;
    }

    if (player.weapon < ESP_NATIVE_GAMEPLAY_STANDARD_WEAPONS) {
        weaponSpec = EspNativeGameplayCombatMath_weapon(player.weapon);
        if (weaponSpec == nullptr ||
            weaponSpec->ammoType >= ESP_NATIVE_GAMEPLAY_PLAYER_AMMO_TYPES) {
            return false;
        }
        ammoType = weaponSpec->ammoType;
    }
    else {
        ammoType = kFamiliarAmmoType;
    }

    memset(&config, 0, sizeof(config));
    config.health = (uint8_t)(player.param1 & 0xffU);
    config.maxHealth = (uint8_t)((player.param1 >> 8) & 0xffU);
    config.armor = (uint8_t)((player.param1 >> 16) & 0xffU);
    config.maxArmor = (uint8_t)((player.param1 >> 24) & 0xffU);
    config.ammo = player.ammo[ammoType];
    config.weapon = player.weapon;
    config.ammoType = ammoType;
    config.weaponsPresent = player.weapons != 0U ? 1U : 0U;
    if (config.maxHealth == 0U || config.health > config.maxHealth ||
        config.armor > config.maxArmor) {
        return false;
    }
    *outConfig = config;
    return true;
}

bool loadNow(void) {
    LoadedSaveRecord& loaded = saveWorkspace.loaded;
    const NativeSaveCore* record;
    EspBspInventory inventory;
    EspMapResidentSnapshot snapshot;
    EspNativeGameplaySessionConfig config;
    const EspMapRuntimeView* runtime;
    bool recoveredBackup = false;
    const char* name;
    EspMapResidentLifecycleStatus residentStatus;
    uint32_t expectedScriptFNV = 0U;
    uint32_t openCount = 0U;
    uint32_t lockedCount = 0U;
    uint32_t texture10Count = 0U;
    uint32_t actionRemovedCount = 0U;
    uint16_t crateTransformCount = 0U;
    uint32_t crateTransformFNV = 0U;
    uint16_t automapLineCount = 0U;
    uint16_t automapSpriteCount = 0U;
    uint16_t automapVisitedCount = 0U;
    uint32_t automapFNV = 0U;
    uint16_t monsterCount = 0U;
    uint32_t monsterFNV = 0U;
    const char* selectedPath = nullptr;

    memset(&loaded, 0, sizeof(loaded));
    memset(&inventory, 0, sizeof(inventory));
    memset(&snapshot, 0, sizeof(snapshot));
    memset(&config, 0, sizeof(config));

    if (!readBestRecord(&loaded, &recoveredBackup)) {
        printf("[NATIVESAVE] LOAD-FAILED path=%s stage=READ reason=missing-or-invalid failClosed=yes\n",
               kLogPath);
        return false;
    }
    record = &loaded.core;
    selectedPath = recoveredBackup ? kBackupPath : kSavePath;
    name = EspMapCatalog_nameForId(record->targetMapId);
    if (name == nullptr || name[0] == '\0') {
        printf("[NATIVESAVE] LOAD-FAILED path=%s stage=MAP_ID map=%u failClosed=yes\n",
               kLogPath, (unsigned int)record->targetMapId);
        return false;
    }

    /* Rebuild immutable map/runtime first. V1 checkpoints stop at player+pose.
     * V2 adds the consumed player-resource overlay. V3 adds the compact native
     * script/event mutable owner. V4 adds the complete compact line family:
     * open/locked bits plus mutable 9/10 texture variants. V5 adds the compact
     * action-engine sprite-removal overlay. V6 appends canonical crate
     * transformed-definition state. V7 appends the compact Automap reveal
     * snapshot: lines, sprites and BIT_AM_VISITED tiles. V8 appends the exact
     * native logical monster records (HP/armor/stats/alternate attack/alive)
     * without rerolling their generation RNG. Monster position/activation
     * remain deliberately fresh until their own bounded checkpoint. */
    EspNativeGameplaySession_reset();
    EspMapResidentLifecycle_resetAll();
    resetSpawnOwners();

    if (!inventoryForRecord(*record, &inventory)) {
        printf("[NATIVESAVE] LOAD-FAILED path=%s stage=BSP_ID map=%u sourceBytes=%lu sourceCrc=%08lx failClosed=yes\n",
               kLogPath,
               (unsigned int)record->targetMapId,
               (unsigned long)record->sourceBytes,
               (unsigned long)record->sourceCrc32);
        return false;
    }

    residentStatus = EspMapResidentLifecycle_loadFromEmpty(
        name, &inventory, &snapshot);
    runtime = EspMapRuntime_view();
    if (residentStatus != ESP_MAP_RESIDENT_OK ||
        runtime == nullptr ||
        runtime->sourceBytes != record->sourceBytes ||
        runtime->sourceCrc32 != record->sourceCrc32 ||
        runtime->arenaFNV1a != record->runtimeFNV1a ||
        !EspMapResidentLifecycle_isReady()) {
        const unsigned long actualRuntime =
            runtime != nullptr ? (unsigned long)runtime->arenaFNV1a : 0UL;
        resetFailedLoad();
        printf("[NATIVESAVE] LOAD-FAILED path=%s stage=RESIDENT status=%u map=%u expectedRuntime=%08lx actualRuntime=%08lx failClosed=yes\n",
               kLogPath,
               (unsigned int)residentStatus,
               (unsigned int)record->targetMapId,
               (unsigned long)record->runtimeFNV1a,
               actualRuntime);
        return false;
    }

    if (loaded.hasScript == 1U) {
        expectedScriptFNV = fnv1aBytes(loaded.script.storage,
                                       loaded.script.storageBytes);
    }
    if (loaded.hasLines == 1U) {
        openCount = countBits(loaded.lines.openBits, loaded.lines.bitsetBytes);
        lockedCount = countBits(loaded.lines.lockedBits,
                                loaded.lines.bitsetBytes);
        texture10Count = countBits(loaded.lines.texture10Bits,
                                   loaded.lines.bitsetBytes);
    }
    if (loaded.hasActionRemoved == 1U) {
        actionRemovedCount =
            countBits(loaded.actionRemoved.removedBits,
                      loaded.actionRemoved.removedBytes);
    }

    if (!EspNativeGameplayPlayerState_restore(&record->player) ||
        EspNativeGameplayPlayerState_fingerprint() != record->playerFNV1a ||
        !restoreView(*record) ||
        !reprimeHudOwners(*record) ||
        (loaded.hasResources == 1U &&
         !EspNativeGameplayPlayerResources_restore(&loaded.resources)) ||
        (loaded.hasScript == 1U &&
         (!EspMapScriptState_restore(&loaded.script) ||
          EspMapScriptState_fingerprint() != expectedScriptFNV)) ||
        (loaded.hasLines == 1U &&
         !EspMapLineCheckpoint_restore(&loaded.lines)) ||
        (loaded.hasActionRemoved == 1U &&
         (!EspNativeGameplayActionEngine_restoreRemoved(
              &loaded.actionRemoved) ||
          EspNativeGameplayActionEngine_removedFingerprint() !=
              loaded.actionRemoved.stateFNV1a)) ||
        ((record->version == kVersionV6 ||
          record->version == kVersionV7 ||
          record->version == kVersionV8) &&
         !restoreCrateSection(selectedPath, *record,
                              &crateTransformCount,
                              &crateTransformFNV)) ||
        ((record->version == kVersionV7 ||
          record->version == kVersionV8) &&
         !restoreAutomapSection(selectedPath, *record,
                                &automapLineCount,
                                &automapSpriteCount,
                                &automapVisitedCount,
                                &automapFNV)) ||
        (record->version == kVersionV8 &&
         !stageV8MonsterSection(selectedPath, *record,
                                &monsterCount, &monsterFNV)) ||
        !sessionConfigForPlayer(record->player, &config) ||
        !EspNativeGameplaySession_configureResume(&config)) {
        resetFailedLoad();
        printf("[NATIVESAVE] LOAD-FAILED path=%s stage=RESTORE map=%u version=%u resources=%s script=%s lines=%s actionRemoved=%s crateTransforms=%s automap=%s monsters=%s playerFNV=%08lx failClosed=yes\n",
               kLogPath,
               (unsigned int)record->targetMapId,
               (unsigned int)record->version,
               loaded.hasResources == 1U ? "required" : "legacy-none",
               loaded.hasScript == 1U ? "required" : "legacy-none",
               loaded.hasLines == 1U ? "required" : "legacy-none",
               loaded.hasActionRemoved == 1U ? "required" : "legacy-none",
               (record->version == kVersionV6 ||
                record->version == kVersionV7 ||
                record->version == kVersionV8)
                   ? "required"
                   : "legacy-none",
               (record->version == kVersionV7 ||
                record->version == kVersionV8)
                   ? "required"
                   : "legacy-none",
               record->version == kVersionV8 ? "required" : "legacy-none",
               (unsigned long)record->playerFNV1a);
        return false;
    }

    if (loaded.hasScript == 1U && loaded.hasLines == 0U) {
        printf("[NATIVESAVE] LEGACY-LINE-GAP version=%u lineState=fresh warning=script-may-reference-unpersisted-line-mutations\n",
               (unsigned int)record->version);
    }
    if (loaded.hasLines == 1U && loaded.hasActionRemoved == 0U) {
        printf("[NATIVESAVE] LEGACY-ACTION-GAP version=%u actionRemoved=fresh warning=fire-clears-and-future-action-removals-not-persisted\n",
               (unsigned int)record->version);
    }
    if (loaded.hasActionRemoved == 1U && record->version < kVersionV6) {
        printf("[NATIVESAVE] LEGACY-CRATE-GAP version=%u crateTransforms=fresh warning=transformed-crates-not-persisted\n",
               (unsigned int)record->version);
    }

    if (record->version < kVersionV7) {
        printf("[NATIVESAVE] LEGACY-AUTOMAP-GAP version=%u automap=fresh warning=reveal-state-not-present-in-record\n",
               (unsigned int)record->version);
    }

    if (record->version < kVersionV8) {
        printf("[NATIVESAVE] LEGACY-MONSTER-GAP version=%u monsterState=fresh warning=hp-alive-stats-not-present-in-record\n",
               (unsigned int)record->version);
    }

    const char* worldSummary =
        record->version == kVersionV8
            ? "resources+script+lines+action-removals+crate-transforms+automap+monster-state-restored+monster-position+activation-fresh"
            : (record->version == kVersionV7
                   ? "resources+script+lines+action-removals+crate-transforms+automap-restored+monster-state+position+activation-fresh"
                   : (record->version == kVersionV6
                          ? "resources+script+lines+action-removals+crate-transforms-restored+automap+monster-state+position+activation-fresh"
                          : "legacy-partial-world"));

    printf("[NATIVESAVE] LOAD path=%s version=%u bytes=%u map=%u gameplayLoadMapId=%u pos=%ld,%ld angle=%ld playerFNV=%08lx runtimeFNV=%08lx sourceBytes=%lu sourceCrc=%08lx backupRecovery=%s resources=%s/%u/%uB script=%s/%lu/%lu/%uB/%08lx lines=%s/%lu/%uB/open%lu/locked%lu/tex10%lu/%08lx/%08lx actionRemoved=%s/%lu/%uB/%08lx crateTransforms=%s/%u/%08lx automap=%s/%uL/%uS/%uV/%08lx monsters=%s/%u/%08lx world=%s session=reprime-pending\n",
           kLogPath,
           (unsigned int)record->version,
           (unsigned int)loaded.fileBytes,
           (unsigned int)record->targetMapId,
           (unsigned int)record->gameplayLoadMapId,
           (long)record->view.viewX,
           (long)record->view.viewY,
           (long)record->view.viewAngle,
           (unsigned long)record->playerFNV1a,
           (unsigned long)record->runtimeFNV1a,
           (unsigned long)record->sourceBytes,
           (unsigned long)record->sourceCrc32,
           recoveredBackup ? "yes" : "no",
           loaded.hasResources == 1U ? "restored" : "legacy-none",
           loaded.hasResources == 1U
               ? (unsigned int)loaded.resources.consumedCount : 0U,
           loaded.hasResources == 1U
               ? (unsigned int)loaded.resources.consumedBytes : 0U,
           loaded.hasScript == 1U ? "restored" : "legacy-none",
           loaded.hasScript == 1U
               ? (unsigned long)loaded.script.eventCount : 0UL,
           loaded.hasScript == 1U
               ? (unsigned long)loaded.script.byteCodeCount : 0UL,
           loaded.hasScript == 1U
               ? (unsigned int)loaded.script.storageBytes : 0U,
           (unsigned long)expectedScriptFNV,
           loaded.hasLines == 1U ? "restored" : "legacy-none",
           loaded.hasLines == 1U ? (unsigned long)loaded.lines.lineCount : 0UL,
           loaded.hasLines == 1U ? (unsigned int)loaded.lines.bitsetBytes : 0U,
           (unsigned long)openCount,
           (unsigned long)lockedCount,
           (unsigned long)texture10Count,
           loaded.hasLines == 1U
               ? (unsigned long)loaded.lines.lineStateFNV1a : 0UL,
           loaded.hasLines == 1U
               ? (unsigned long)loaded.lines.textureStateFNV1a : 0UL,
           loaded.hasActionRemoved == 1U ? "restored" : "legacy-none",
           (unsigned long)actionRemovedCount,
           loaded.hasActionRemoved == 1U
               ? (unsigned int)loaded.actionRemoved.removedBytes : 0U,
           loaded.hasActionRemoved == 1U
               ? (unsigned long)loaded.actionRemoved.stateFNV1a : 0UL,
           (record->version == kVersionV6 ||
            record->version == kVersionV7 ||
            record->version == kVersionV8)
               ? "restored" : "legacy-none",
           (unsigned int)crateTransformCount,
           (unsigned long)crateTransformFNV,
           (record->version == kVersionV7 ||
            record->version == kVersionV8)
               ? "restored" : "legacy-none",
           (unsigned int)automapLineCount,
           (unsigned int)automapSpriteCount,
           (unsigned int)automapVisitedCount,
           (unsigned long)automapFNV,
           record->version == kVersionV8 ? "staged" : "legacy-none",
           (unsigned int)monsterCount,
           (unsigned long)monsterFNV,
           worldSummary);
    return true;
}

void fillRect(uint16_t* fb, int left, int top, int right, int bottom,
              uint16_t color) {
    int x;
    int y;
    if (fb == nullptr) return;
    for (y = top; y <= bottom; ++y) {
        if (y < 0 || y >= DOOMRPG_LOGICAL_HEIGHT) continue;
        for (x = left; x <= right; ++x) {
            if (x < 0 || x >= DOOMRPG_LOGICAL_WIDTH) continue;
            fb[y * DOOMRPG_LOGICAL_WIDTH + x] = color;
        }
    }
}

void drawRect(uint16_t* fb, int left, int top, int right, int bottom,
              uint16_t color) {
    int x;
    int y;
    if (fb == nullptr) return;
    for (x = left; x <= right; ++x) {
        if (x >= 0 && x < DOOMRPG_LOGICAL_WIDTH) {
            if (top >= 0 && top < DOOMRPG_LOGICAL_HEIGHT)
                fb[top * DOOMRPG_LOGICAL_WIDTH + x] = color;
            if (bottom >= 0 && bottom < DOOMRPG_LOGICAL_HEIGHT)
                fb[bottom * DOOMRPG_LOGICAL_WIDTH + x] = color;
        }
    }
    for (y = top; y <= bottom; ++y) {
        if (y >= 0 && y < DOOMRPG_LOGICAL_HEIGHT) {
            if (left >= 0 && left < DOOMRPG_LOGICAL_WIDTH)
                fb[y * DOOMRPG_LOGICAL_WIDTH + left] = color;
            if (right >= 0 && right < DOOMRPG_LOGICAL_WIDTH)
                fb[y * DOOMRPG_LOGICAL_WIDTH + right] = color;
        }
    }
}

const uint8_t* glyph(char c) {
    static const uint8_t A[7] = {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11};
    static const uint8_t C[7] = {0x0e,0x11,0x10,0x10,0x10,0x11,0x0e};
    static const uint8_t D[7] = {0x1e,0x11,0x11,0x11,0x11,0x11,0x1e};
    static const uint8_t E[7] = {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f};
    static const uint8_t F[7] = {0x1f,0x10,0x10,0x1e,0x10,0x10,0x10};
    static const uint8_t H[7] = {0x11,0x11,0x11,0x1f,0x11,0x11,0x11};
    static const uint8_t I[7] = {0x0e,0x04,0x04,0x04,0x04,0x04,0x0e};
    static const uint8_t K[7] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11};
    static const uint8_t L[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1f};
    static const uint8_t N[7] = {0x11,0x19,0x19,0x15,0x13,0x13,0x11};
    static const uint8_t O[7] = {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e};
    static const uint8_t P[7] = {0x1e,0x11,0x11,0x1e,0x10,0x10,0x10};
    static const uint8_t S[7] = {0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e};
    static const uint8_t T[7] = {0x1f,0x04,0x04,0x04,0x04,0x04,0x04};
    static const uint8_t V[7] = {0x11,0x11,0x11,0x11,0x11,0x0a,0x04};
    static const uint8_t ONE[7] = {0x04,0x0c,0x14,0x04,0x04,0x04,0x1f};
    static const uint8_t QUESTION[7] = {0x0e,0x11,0x01,0x02,0x04,0x00,0x04};
    switch (c) {
    case 'A': return A;
    case 'C': return C;
    case 'D': return D;
    case 'E': return E;
    case 'F': return F;
    case 'H': return H;
    case 'I': return I;
    case 'K': return K;
    case 'L': return L;
    case 'N': return N;
    case 'O': return O;
    case 'P': return P;
    case 'S': return S;
    case 'T': return T;
    case 'V': return V;
    case '1': return ONE;
    case '?': return QUESTION;
    default: return nullptr;
    }
}

void drawGlyph(uint16_t* fb, int x, int y, char c, uint16_t color) {
    const uint8_t* rows = glyph(c);
    int row;
    int col;
    if (rows == nullptr) return;
    for (row = 0; row < 7; ++row) {
        for (col = 0; col < 5; ++col) {
            if ((rows[row] & (uint8_t)(1U << (4 - col))) != 0U) {
                const int px = x + col;
                const int py = y + row;
                if (px >= 0 && px < DOOMRPG_LOGICAL_WIDTH &&
                    py >= 0 && py < DOOMRPG_LOGICAL_HEIGHT) {
                    fb[py * DOOMRPG_LOGICAL_WIDTH + px] = color;
                }
            }
        }
    }
}

void drawWord(uint16_t* fb, int x, int y, const char* text, uint16_t color) {
    if (fb == nullptr || text == nullptr) return;
    while (*text != '\0') {
        drawGlyph(fb, x, y, *text++, color);
        x += 6;
    }
}

void drawCenteredWord(uint16_t* fb, int centerX, int y, const char* text,
                      uint16_t color) {
    size_t length;
    if (text == nullptr) return;
    length = strlen(text);
    drawWord(fb, centerX - (int)(length * 6U - (length != 0U ? 1U : 0U)) / 2,
             y, text, color);
}

bool paintSaveOverlay(void) {
    const EspNativeGameplayHubView* hub = EspNativeGameplayHub_view();
    uint16_t* fb;
    const char* saveLabel = "SAVE";
    const char* loadLabel = "LOAD";
    uint16_t saveColor = ESP_HUB_COLOR_IVORY;
    uint16_t loadColor = ESP_HUB_COLOR_IVORY;
    const bool hasSave = EspNativeGameplaySave_hasReadableCheckpoint() != 0;
    size_t expected = (size_t)DOOMRPG_LOGICAL_WIDTH *
                      (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);

    if (hub == nullptr || hub->active != 1U ||
        hub->page != ESP_NATIVE_GAMEPLAY_HUB_PAGE_SYSTEM ||
        Esp32PlatformVideo_framebuffer() == nullptr ||
        Esp32PlatformVideo_framebufferSizeBytes() != expected) {
        return false;
    }

    fb = static_cast<uint16_t*>(Esp32PlatformVideo_framebuffer());

    fillRect(fb, 2, 35, 157, 98, ESP_HUB_COLOR_BG);
    fillRect(fb, kPanelLeft, kPanelTop, kPanelRight, kPanelBottom,
             ESP_HUB_COLOR_PANEL);
    drawRect(fb, kPanelLeft, kPanelTop, kPanelRight, kPanelBottom,
             ESP_HUB_COLOR_STEEL);
    drawCenteredWord(fb, 80, 38, "CHECKPOINT 1", ESP_HUB_COLOR_STEEL);

    fillRect(fb, kButtonLeft, kSaveTop, kButtonRight, kSaveBottom,
             statusCursor == kStatusSave ? ESP_HUB_COLOR_PANEL_ALT
                                         : ESP_HUB_COLOR_BG);
    fillRect(fb, kButtonLeft, kLoadTop, kButtonRight, kLoadBottom,
             statusCursor == kStatusLoad ? ESP_HUB_COLOR_PANEL_ALT
                                         : ESP_HUB_COLOR_BG);
    drawRect(fb, kButtonLeft, kSaveTop, kButtonRight, kSaveBottom,
             statusCursor == kStatusSave ? ESP_HUB_COLOR_AMBER
                                         : ESP_HUB_COLOR_STEEL_DARK);
    drawRect(fb, kButtonLeft, kLoadTop, kButtonRight, kLoadBottom,
             statusCursor == kStatusLoad ? ESP_HUB_COLOR_AMBER
                                         : ESP_HUB_COLOR_STEEL_DARK);
    fillRect(fb, kButtonLeft + 3,
             statusCursor == kStatusSave ? kSaveTop + 4 : kLoadTop + 4,
             kButtonLeft + 5,
             statusCursor == kStatusSave ? kSaveBottom - 4 : kLoadBottom - 4,
             ESP_HUB_COLOR_AMBER);

    if (confirmationTarget == kStatusSave) {
        saveLabel = "SAVE?";
        saveColor = ESP_HUB_COLOR_AMBER;
    }
    else if (lastOperation == 1U) {
        saveLabel = lastOperationOk != 0U ? "SAVED" : "FAILED";
        saveColor = lastOperationOk != 0U ? ESP_HUB_COLOR_GREEN
                                          : ESP_HUB_COLOR_RED;
    }
    if (!hasSave) {
        loadLabel = "NO SAVE";
        loadColor = ESP_HUB_COLOR_STEEL_DARK;
    }
    else if (confirmationTarget == kStatusLoad) {
        loadLabel = "LOAD?";
        loadColor = ESP_HUB_COLOR_AMBER;
    }
    else if (lastOperation == 2U && lastOperationOk == 0U) {
        loadLabel = "FAILED";
        loadColor = ESP_HUB_COLOR_RED;
    }

    drawCenteredWord(fb, 80, kSaveTop + 7, saveLabel, saveColor);
    drawCenteredWord(fb, 80, kLoadTop + 7, loadLabel, loadColor);
    return Esp32PlatformVideo_present();
}

}  // namespace

extern "C" uint8_t EspNativeGameplaySave_statusCursor(void) {
    return statusCursor;
}

#if defined(__GNUC__)
__attribute__((noinline))
#endif
bool readableSaveExists(void) {
    bool recoveredBackup = false;
    memset(&saveWorkspace.loaded, 0, sizeof(saveWorkspace.loaded));
    return readBestRecord(&saveWorkspace.loaded, &recoveredBackup);
}

extern "C" int EspNativeGameplaySave_hasReadableCheckpoint(void) {
    return readableSaveExists() ? 1 : 0;
}

extern "C" int EspNativeGameplaySave_loadCheckpoint(void) {
    return loadNow() ? 1 : 0;
}

extern "C" EspNativeGameplayHubStatus
__real_EspNativeGameplayHub_handleAction(uint8_t action);

extern "C" EspNativeGameplayHubStatus
__wrap_EspNativeGameplayHub_handleAction(uint8_t action) {
    const EspNativeGameplayHubView* before = EspNativeGameplayHub_view();
    const uint8_t beforePage = before != nullptr ? before->page : 0xffU;
    EspNativeGameplayHubStatus status;

    if (before != nullptr && before->active == 1U &&
        before->page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_SYSTEM) {
        if (action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD ||
            action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK) {
            if (action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD) {
                statusCursor =
                    (uint8_t)((statusCursor + kStatusCount - 1U) % kStatusCount);
            }
            else {
                statusCursor = (uint8_t)((statusCursor + 1U) % kStatusCount);
            }
            lastOperation = 0U;
            confirmationTarget = kNoConfirmation;
            if (!paintSaveOverlay()) return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
            printf("[NATIVESAVE] CURSOR page=system row=%u action=%s mutation=no turn=no\n",
                   (unsigned int)statusCursor,
                   statusCursor == kStatusSave ? "SAVE" : "LOAD");
            return ESP_NATIVE_GAMEPLAY_HUB_REDRAWN;
        }

        if (action == ESP_NATIVE_GAMEPLAY_ACTION_SELECT) {
            if (statusCursor == kStatusLoad &&
                !EspNativeGameplaySave_hasReadableCheckpoint()) {
                confirmationTarget = kNoConfirmation;
                lastOperation = 2U;
                lastOperationOk = 0U;
                printf("[NATIVESAVE] LOAD-DEFER path=%s reason=missing-or-invalid mutation=no\n",
                       kLogPath);
                if (!paintSaveOverlay()) return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
                return ESP_NATIVE_GAMEPLAY_HUB_IGNORED;
            }

            if (confirmationTarget != statusCursor) {
                confirmationTarget = statusCursor;
                lastOperation = 0U;
                if (!paintSaveOverlay()) return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
                printf("[NATIVESAVE] CONFIRM-ARM page=system row=%s mutation=no turn=no\n",
                       statusCursor == kStatusSave ? "SAVE" : "LOAD");
                return ESP_NATIVE_GAMEPLAY_HUB_REDRAWN;
            }
            confirmationTarget = kNoConfirmation;

            if (statusCursor == kStatusSave) {
                lastOperation = 1U;
                lastOperationOk = saveNow() ? 1U : 0U;
                if (!paintSaveOverlay()) return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
                return lastOperationOk ? ESP_NATIVE_GAMEPLAY_HUB_REDRAWN
                                       : ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
            }

            /*
             * Do not run the ordinary HUB close path before LOAD. That path
             * validates restoration of the old HUD/world, but a successful
             * checkpoint load replaces the entire gameplay session anyway.
             * loadNow() already owns EspNativeGameplaySession_reset(), whose
             * resident reset clears the HUB before rebuilding the checkpoint.
             *
             * Requiring the normal close first made LOAD fail with NOT_READY
             * whenever the redesigned HUB's expected HUD-band witness differed
             * from the current world HUD, even though the save itself had
             * already passed readableSaveExists().
             */
            printf("[NATIVESAVE] LOAD-BEGIN page=system row=LOAD sessionReplace=yes hubClose=session-reset\n");
            lastOperation = 2U;
            lastOperationOk =
                EspNativeGameplaySave_loadCheckpoint() ? 1U : 0U;
            statusCursor = kStatusSave;
            if (!lastOperationOk) {
                printf("[NATIVESAVE] LOAD-TERMINAL result=failed gameplaySession=reset failClosed=yes\n");
                return ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;
            }
            /*
             * Session reset/load has already cleared the HUB. Returning OK
             * (not CLOSED) prevents resident gameplay from redrawing the old
             * world after the session has been replaced. The next top-level
             * session tick starts the normal FIRST_FRAME -> HUD -> cache-prime
             * sequence.
             */
            return ESP_NATIVE_GAMEPLAY_HUB_OK;
        }
    }

    status = __real_EspNativeGameplayHub_handleAction(action);
    {
        const EspNativeGameplayHubView* after = EspNativeGameplayHub_view();
        if (after != nullptr && after->active == 1U &&
            after->page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_SYSTEM) {
            if (beforePage != ESP_NATIVE_GAMEPLAY_HUB_PAGE_SYSTEM) {
                statusCursor = kStatusSave;
                lastOperation = 0U;
                confirmationTarget = kNoConfirmation;
                printf("[NATIVESAVE] UI page=system rows=SAVE/LOAD slot=1 path=%s confirmation=double-select worldScope=resources+script+lines+action-removals+crate-transforms+automap+monster-state-v8+position-activation-fresh legacyV1..V7=read-only-compatible\n",
                       kLogPath);
            }
            if ((status == ESP_NATIVE_GAMEPLAY_HUB_REDRAWN ||
                 status == ESP_NATIVE_GAMEPLAY_HUB_OK) &&
                !paintSaveOverlay()) {
                return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
            }
        }
        else if (after == nullptr || after->active == 0U) {
            statusCursor = kStatusSave;
            lastOperation = 0U;
            confirmationTarget = kNoConfirmation;
        }
    }
    return status;
}
