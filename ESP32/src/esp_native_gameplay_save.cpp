#include <Arduino.h>
#include <SD.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_bsp_reader.h"
#include "esp_hud_post_load_clear_state.h"
#include "esp_hud_refresh_state.h"
#include "esp_map_catalog.h"
#include "esp_map_line_checkpoint.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_combat_math.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_hub.h"
#include "esp_native_gameplay_input.h"
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
constexpr uint16_t kVersionV1 = 1U;
constexpr uint16_t kVersionV2 = 2U;
constexpr uint16_t kVersionV3 = 3U;
constexpr uint16_t kVersionV4 = 4U;
constexpr uint16_t kVersionV5 = 5U;
constexpr uint8_t kFamiliarAmmoType = 5U;
constexpr uint8_t kStatusSave = 0U;
constexpr uint8_t kStatusLoad = 1U;
constexpr uint8_t kStatusCount = 2U;
constexpr int kOverlayLeft = 91;
constexpr int kOverlayTop = 70;
constexpr int kOverlayRight = 158;
constexpr int kOverlayBottom = 98;

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
};

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

bool writeExactV5(const char* path, const NativeSaveRecordV5& record) {
    File file = SD.open(path, FILE_WRITE);
    size_t wrote;
    if (!file) return false;
    wrote = file.write(reinterpret_cast<const uint8_t*>(&record), sizeof(record));
    file.flush();
    file.close();
    return wrote == sizeof(record);
}

bool readExactV5Matches(const char* path, const NativeSaveRecordV5& expected) {
    File file;
    const uint8_t* expectedBytes =
        reinterpret_cast<const uint8_t*>(&expected);
    uint8_t verify[64];
    size_t offset = 0U;

    if (path == nullptr || !SD.exists(path)) return false;
    file = SD.open(path, FILE_READ);
    if (!file || (size_t)file.size() != sizeof(expected)) {
        if (file) file.close();
        return false;
    }

    while (offset < sizeof(expected)) {
        size_t chunk = sizeof(expected) - offset;
        size_t got;
        if (chunk > sizeof(verify)) chunk = sizeof(verify);
        got = file.read(verify, chunk);
        if (got != chunk ||
            memcmp(verify, expectedBytes + offset, chunk) != 0) {
            file.close();
            return false;
        }
        offset += chunk;
    }
    file.close();
    return true;
}

bool commitRecordAtomic(const NativeSaveRecordV5& record) {
    bool movedOld = false;

    if (SD.exists(kTempPath)) (void)SD.remove(kTempPath);
    if (SD.exists(kBackupPath)) (void)SD.remove(kBackupPath);
    if (!writeExactV5(kTempPath, record) ||
        !readExactV5Matches(kTempPath, record)) {
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

    if (!readExactV5Matches(kSavePath, record)) {
        (void)SD.remove(kSavePath);
        if (movedOld && SD.exists(kBackupPath)) {
            (void)SD.rename(kBackupPath, kSavePath);
        }
        return false;
    }

    if (SD.exists(kBackupPath)) (void)SD.remove(kBackupPath);
    return true;
}

bool captureRecord(NativeSaveRecordV5* outRecord) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspPlayerViewState* view = EspPlayerView_view();

    if (outRecord == nullptr || EspAssetPack_isOpen() ||
        runtime == nullptr || view == nullptr ||
        !EspMapResidentLifecycle_isReady()) {
        return false;
    }

    NativeSaveRecordV5& record = *outRecord;
    memset(outRecord, 0, sizeof(*outRecord));
    if (!EspNativeGameplayPlayerState_snapshot(&record.core.player) ||
        !EspNativeGameplayPlayerResources_snapshot(&record.resources) ||
        !EspMapScriptState_snapshot(&record.script) ||
        !EspMapLineCheckpoint_snapshot(&record.lines) ||
        !EspNativeGameplayActionEngine_snapshotRemoved(&record.actionRemoved)) {
        return false;
    }
    if (view->active != 1U || view->targetMapId == 0U ||
        runtime->sourceBytes == 0U || runtime->sourceCrc32 == 0U ||
        runtime->arenaFNV1a == 0U) {
        return false;
    }

    memcpy(record.core.magic, kMagicV5, sizeof(kMagicV5));
    record.core.version = kVersionV5;
    record.core.recordBytes = (uint16_t)sizeof(record);
    record.core.sourceBytes = runtime->sourceBytes;
    record.core.sourceCrc32 = runtime->sourceCrc32;
    record.core.runtimeFNV1a = runtime->arenaFNV1a;
    record.core.playerFNV1a = EspNativeGameplayPlayerState_fingerprint();
    record.core.targetMapId = view->targetMapId;
    record.core.gameplayLoadMapId = view->gameplayLoadMapId;
    record.core.loadType = view->loadType;
    record.core.view = *view;
    record.core.recordCrc32 = recordCrcV5(record);
    return recordV5Valid(record);
}

bool saveNow(void) {
    NativeSaveRecordV5& record = saveWorkspace.write;
    uint32_t scriptFNV;
    uint32_t openCount;
    uint32_t lockedCount;
    uint32_t texture10Count;
    uint32_t removedCount;

    memset(&record, 0, sizeof(record));
    if (!captureRecord(&record) || !commitRecordAtomic(record)) {
        printf("[NATIVESAVE] SAVE-FAILED path=%s version=5 sections=resources+script+lines+action-removals failClosed=yes\n",
               kLogPath);
        return false;
    }
    scriptFNV = fnv1aBytes(record.script.storage, record.script.storageBytes);
    openCount = countBits(record.lines.openBits, record.lines.bitsetBytes);
    lockedCount = countBits(record.lines.lockedBits, record.lines.bitsetBytes);
    texture10Count = countBits(record.lines.texture10Bits,
                               record.lines.bitsetBytes);
    removedCount = countBits(record.actionRemoved.removedBits,
                             record.actionRemoved.removedBytes);
    printf("[NATIVESAVE] SAVE path=%s version=%u bytes=%u map=%u gameplayLoadMapId=%u pos=%ld,%ld angle=%ld playerFNV=%08lx runtimeFNV=%08lx sourceBytes=%lu sourceCrc=%08lx recordCrc=%08lx resources=%u/%uB sprites=%u script=%lu/%lu/%uB scriptFNV=%08lx lines=%lu/%uB open=%lu locked=%lu texture10=%lu lineFNV=%08lx textureFNV=%08lx actionRemoved=%lu/%uB/%08lx atomic=temp+backup+rename world=resources+script+lines+action-removals-restored+others-fresh\n",
           kLogPath,
           (unsigned int)record.core.version,
           (unsigned int)sizeof(record),
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
           (unsigned long)record.actionRemoved.stateFNV1a);
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
     * action-engine sprite-removal overlay (fires first). Every other world
     * family remains fresh until its own bounded persistence milestone. */
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
        !sessionConfigForPlayer(record->player, &config) ||
        !EspNativeGameplaySession_configureResume(&config)) {
        resetFailedLoad();
        printf("[NATIVESAVE] LOAD-FAILED path=%s stage=RESTORE map=%u version=%u resources=%s script=%s lines=%s actionRemoved=%s playerFNV=%08lx failClosed=yes\n",
               kLogPath,
               (unsigned int)record->targetMapId,
               (unsigned int)record->version,
               loaded.hasResources == 1U ? "required" : "legacy-none",
               loaded.hasScript == 1U ? "required" : "legacy-none",
               loaded.hasLines == 1U ? "required" : "legacy-none",
               loaded.hasActionRemoved == 1U ? "required" : "legacy-none",
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

    printf("[NATIVESAVE] LOAD path=%s version=%u bytes=%u map=%u gameplayLoadMapId=%u pos=%ld,%ld angle=%ld playerFNV=%08lx runtimeFNV=%08lx sourceBytes=%lu sourceCrc=%08lx backupRecovery=%s resources=%s/%u/%uB script=%s/%lu/%lu/%uB/%08lx lines=%s/%lu/%uB/open%lu/locked%lu/tex10%lu/%08lx/%08lx actionRemoved=%s/%lu/%uB/%08lx world=%s session=reprime-pending\n",
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
               ? (unsigned int)loaded.resources.consumedCount
               : 0U,
           loaded.hasResources == 1U
               ? (unsigned int)loaded.resources.consumedBytes
               : 0U,
           loaded.hasScript == 1U ? "restored" : "legacy-none",
           loaded.hasScript == 1U
               ? (unsigned long)loaded.script.eventCount
               : 0UL,
           loaded.hasScript == 1U
               ? (unsigned long)loaded.script.byteCodeCount
               : 0UL,
           loaded.hasScript == 1U
               ? (unsigned int)loaded.script.storageBytes
               : 0U,
           (unsigned long)expectedScriptFNV,
           loaded.hasLines == 1U ? "restored" : "legacy-none",
           loaded.hasLines == 1U ? (unsigned long)loaded.lines.lineCount : 0UL,
           loaded.hasLines == 1U ? (unsigned int)loaded.lines.bitsetBytes : 0U,
           (unsigned long)openCount,
           (unsigned long)lockedCount,
           (unsigned long)texture10Count,
           loaded.hasLines == 1U
               ? (unsigned long)loaded.lines.lineStateFNV1a
               : 0UL,
           loaded.hasLines == 1U
               ? (unsigned long)loaded.lines.textureStateFNV1a
               : 0UL,
           loaded.hasActionRemoved == 1U ? "restored" : "legacy-none",
           (unsigned long)actionRemovedCount,
           loaded.hasActionRemoved == 1U
               ? (unsigned int)loaded.actionRemoved.removedBytes
               : 0U,
           loaded.hasActionRemoved == 1U
               ? (unsigned long)loaded.actionRemoved.stateFNV1a
               : 0UL,
           loaded.hasActionRemoved == 1U
               ? "resources+script+lines+action-removals-restored+others-fresh"
               : (loaded.hasLines == 1U
                      ? "resources+script+lines-restored+action-removals+others-fresh"
                      : (loaded.hasScript == 1U
                             ? "resources+script-restored+lines+action-removals+others-fresh"
                             : (loaded.hasResources == 1U
                                    ? "resources-restored+script+lines+action-removals+others-fresh"
                                    : "fresh-rebuild-v1"))));
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
    static const uint8_t D[7] = {0x1e,0x11,0x11,0x11,0x11,0x11,0x1e};
    static const uint8_t E[7] = {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f};
    static const uint8_t L[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1f};
    static const uint8_t O[7] = {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e};
    static const uint8_t S[7] = {0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e};
    static const uint8_t V[7] = {0x11,0x11,0x11,0x11,0x11,0x0a,0x04};
    switch (c) {
    case 'A': return A;
    case 'D': return D;
    case 'E': return E;
    case 'L': return L;
    case 'O': return O;
    case 'S': return S;
    case 'V': return V;
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

bool paintSaveOverlay(void) {
    const EspNativeGameplayHubView* hub = EspNativeGameplayHub_view();
    uint16_t* fb;
    uint16_t saveColor;
    uint16_t loadColor;
    size_t expected = (size_t)DOOMRPG_LOGICAL_WIDTH *
                      (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);

    if (hub == nullptr || hub->active != 1U ||
        hub->page != ESP_NATIVE_GAMEPLAY_HUB_PAGE_STATUS ||
        Esp32PlatformVideo_framebuffer() == nullptr ||
        Esp32PlatformVideo_framebufferSizeBytes() != expected) {
        return false;
    }

    fb = static_cast<uint16_t*>(Esp32PlatformVideo_framebuffer());
    saveColor = statusCursor == kStatusSave ? 0x07e0U : 0xffffU;
    loadColor = statusCursor == kStatusLoad ? 0x07e0U : 0xffffU;
    fillRect(fb, kOverlayLeft, kOverlayTop, kOverlayRight, kOverlayBottom,
             0x0000U);
    drawRect(fb, kOverlayLeft, kOverlayTop, kOverlayRight, kOverlayBottom,
             0xffffU);
    if (statusCursor == kStatusSave) {
        fillRect(fb, kOverlayLeft + 3, 75, kOverlayLeft + 5, 81, 0x07e0U);
    }
    else {
        fillRect(fb, kOverlayLeft + 3, 87, kOverlayLeft + 5, 93, 0x07e0U);
    }
    drawWord(fb, kOverlayLeft + 10, 75, "SAVE", saveColor);
    drawWord(fb, kOverlayLeft + 10, 87, "LOAD", loadColor);
    if (lastOperation != 0U) {
        const uint16_t opColor = lastOperationOk ? 0x07e0U : 0xf800U;
        const int y = lastOperation == 1U ? 77 : 89;
        fillRect(fb, kOverlayRight - 7, y, kOverlayRight - 3, y + 4, opColor);
    }
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

extern "C" EspNativeGameplayHubStatus
__real_EspNativeGameplayHub_handleAction(uint8_t action);

extern "C" EspNativeGameplayHubStatus
__wrap_EspNativeGameplayHub_handleAction(uint8_t action) {
    const EspNativeGameplayHubView* before = EspNativeGameplayHub_view();
    const uint8_t beforePage = before != nullptr ? before->page : 0xffU;
    EspNativeGameplayHubStatus status;

    if (before != nullptr && before->active == 1U &&
        before->page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_STATUS) {
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
            if (!paintSaveOverlay()) return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
            printf("[NATIVESAVE] CURSOR page=status row=%u action=%s mutation=no turn=no\n",
                   (unsigned int)statusCursor,
                   statusCursor == kStatusSave ? "SAVE" : "LOAD");
            return ESP_NATIVE_GAMEPLAY_HUB_REDRAWN;
        }

        if (action == ESP_NATIVE_GAMEPLAY_ACTION_SELECT) {
            if (statusCursor == kStatusSave) {
                lastOperation = 1U;
                lastOperationOk = saveNow() ? 1U : 0U;
                if (!paintSaveOverlay()) return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
                return lastOperationOk ? ESP_NATIVE_GAMEPLAY_HUB_REDRAWN
                                       : ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
            }

            if (!readableSaveExists()) {
                lastOperation = 2U;
                lastOperationOk = 0U;
                printf("[NATIVESAVE] LOAD-DEFER path=%s reason=missing-or-invalid mutation=no\n",
                       kLogPath);
                if (!paintSaveOverlay()) return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
                return ESP_NATIVE_GAMEPLAY_HUB_IGNORED;
            }

            status = __real_EspNativeGameplayHub_handleAction(
                ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN);
            if (status != ESP_NATIVE_GAMEPLAY_HUB_CLOSED) return status;

            lastOperation = 2U;
            lastOperationOk = loadNow() ? 1U : 0U;
            statusCursor = kStatusSave;
            if (!lastOperationOk) {
                printf("[NATIVESAVE] LOAD-TERMINAL result=failed gameplaySession=reset failClosed=yes\n");
                return ESP_NATIVE_GAMEPLAY_HUB_NOT_READY;
            }
            /*
             * The real HUB is already closed, but returning OK (not CLOSED)
             * prevents resident gameplay from redrawing the old world after the
             * session has been replaced. The next top-level session tick starts
             * the normal FIRST_FRAME -> HUD -> cache prime sequence.
             */
            return ESP_NATIVE_GAMEPLAY_HUB_OK;
        }
    }

    status = __real_EspNativeGameplayHub_handleAction(action);
    {
        const EspNativeGameplayHubView* after = EspNativeGameplayHub_view();
        if (after != nullptr && after->active == 1U &&
            after->page == ESP_NATIVE_GAMEPLAY_HUB_PAGE_STATUS) {
            if (beforePage != ESP_NATIVE_GAMEPLAY_HUB_PAGE_STATUS) {
                statusCursor = kStatusSave;
                lastOperation = 0U;
                printf("[NATIVESAVE] UI page=status rows=SAVE/LOAD slot=1 path=%s worldScope=resources+script+lines+action-removals-v5+others-fresh legacyV1V2V3V4=read-only-compatible\n",
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
        }
    }
    return status;
}
