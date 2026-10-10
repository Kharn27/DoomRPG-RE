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
#include "esp_map_events.h"
#include "esp_map_line_checkpoint.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_combat_math.h"
#include "esp_native_gameplay_crate_state.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_hub.h"
#include "esp_native_gameplay_hub_theme.h"
#include "esp_native_gameplay_hub_touch_ui.h"
#include "esp_native_gameplay_input.h"
#include "esp_native_gameplay_monster_activation.h"
#include "esp_native_gameplay_monster_drop.h"
#include "esp_native_gameplay_monster_position.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_player_resources.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_gameplay_session.h"
#include "esp_native_gameplay_save_ui.h"
#include "esp_native_transition_presentation.h"
#include "esp_native_resident_gameplay.h"
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

constexpr char kLegacySavePath[] = "/DoomRPG-ESP32.sav";
constexpr char kLegacyBackupPath[] = "/DoomRPG-ESP32.sav.bak";
char kSavePath[48] = "/DoomRPG-ESP32-slot01.sav";
char kTempPath[52] = "/DoomRPG-ESP32-slot01.sav.tmp";
char kBackupPath[52] = "/DoomRPG-ESP32-slot01.sav.bak";
char kLogPath[52] = "/sd/DoomRPG-ESP32-slot01.sav";
uint8_t activeSlot = 1U;
uint8_t slotMode = 0U; /* 0=system, 1=manual SAVE, 2=manual LOAD */
uint8_t slotFocus = 1U;
uint8_t slotArmed = 0U;
uint8_t mainSlotReady = 0U;
const char* activeReadPath = kSavePath;
void selectSaveSlot(uint8_t slot) {
    if (slot < 1U || slot > 10U) return;
    activeSlot = slot;
    snprintf(kSavePath, sizeof(kSavePath), "/DoomRPG-ESP32-slot%02u.sav", (unsigned)slot);
    snprintf(kTempPath, sizeof(kTempPath), "/DoomRPG-ESP32-slot%02u.sav.tmp", (unsigned)slot);
    snprintf(kBackupPath, sizeof(kBackupPath), "/DoomRPG-ESP32-slot%02u.sav.bak", (unsigned)slot);
    snprintf(kLogPath, sizeof(kLogPath), "/sd/DoomRPG-ESP32-slot%02u.sav", (unsigned)slot);
}

constexpr uint8_t kMagicV1[8] = {'D', 'R', 'P', 'G', 'S', 'A', 'V', '1'};
constexpr uint8_t kMagicV2[8] = {'D', 'R', 'P', 'G', 'S', 'A', 'V', '2'};
constexpr uint8_t kMagicV3[8] = {'D', 'R', 'P', 'G', 'S', 'A', 'V', '3'};
constexpr uint8_t kMagicV4[8] = {'D', 'R', 'P', 'G', 'S', 'A', 'V', '4'};
constexpr uint8_t kMagicV5[8] = {'D', 'R', 'P', 'G', 'S', 'A', 'V', '5'};
constexpr uint8_t kMagicV6[8] = {'D', 'R', 'P', 'G', 'S', 'A', 'V', '6'};
constexpr uint8_t kMagicV7[8] = {'D', 'R', 'P', 'G', 'S', 'A', 'V', '7'};
constexpr uint8_t kMagicV8[8] = {'D', 'R', 'P', 'G', 'S', 'A', 'V', '8'};
constexpr uint8_t kMagicV9[8] = {'D', 'R', 'P', 'G', 'S', 'A', 'V', '9'};
constexpr uint8_t kMagicV10[8] = {'D', 'R', 'P', 'G', 'S', 'V', '1', '0'};
constexpr uint8_t kMagicV11[8] = {'D', 'R', 'P', 'G', 'S', 'V', '1', '1'};
constexpr uint16_t kVersionV1 = 1U;
constexpr uint16_t kVersionV2 = 2U;
constexpr uint16_t kVersionV3 = 3U;
constexpr uint16_t kVersionV4 = 4U;
constexpr uint16_t kVersionV5 = 5U;
constexpr uint16_t kVersionV6 = 6U;
constexpr uint16_t kVersionV7 = 7U;
constexpr uint16_t kVersionV8 = 8U;
constexpr uint16_t kVersionV9 = 9U;
constexpr uint16_t kVersionV10 = 10U;
constexpr uint16_t kVersionV11 = 11U;
constexpr uint8_t kFamiliarAmmoType = 5U;
constexpr uint8_t kStatusSave = ESP_NATIVE_SYS_SAVE;
constexpr uint8_t kStatusLoad = ESP_NATIVE_SYS_LOAD;
constexpr uint8_t kStatusExit = ESP_NATIVE_SYS_EXIT;
constexpr uint8_t kStatusCount = ESP_NATIVE_SYS_COUNT;
constexpr uint8_t kNoConfirmation = 0xffU;
constexpr int kPanelLeft = 8;
constexpr int kPanelTop = 36;
constexpr int kPanelRight = 151;
constexpr int kPanelBottom = 117;
constexpr int kButtonLeft = ESP_NATIVE_SYS_BUTTON_LEFT;
constexpr int kButtonRight = ESP_NATIVE_SYS_BUTTON_RIGHT;

int systemRowTop(uint8_t row) {
    return ESP_NATIVE_SYS_BUTTON_TOP + row *
        (ESP_NATIVE_SYS_BUTTON_HEIGHT + ESP_NATIVE_SYS_BUTTON_GAP);
}

const char* systemRowName(uint8_t row) {
    return row == kStatusSave ? "SAVE" : row == kStatusLoad ? "LOAD" : "EXIT";
}

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
    uint8_t hasMonsterSpatial;
    uint8_t hasMonsterDrops;
    EspPlayerLevelProgress levelProgress;
};

struct NativeSaveRecordV9Tail {
    EspNativeGameplayMonsterStateSnapshot monsters;
    EspMapSpriteTopologyMonsterSnapshot monsterTopology;
    EspNativeGameplayMonsterPositionSnapshot monsterPositions;
    EspNativeGameplayMonsterActivationSnapshot monsterActivation;
};

constexpr size_t kRecordBytesV6 =
    sizeof(NativeSaveRecordV5) +
    sizeof(EspNativeGameplayCrateTransformSnapshot);
constexpr size_t kRecordBytesV7 =
    kRecordBytesV6 + sizeof(EspMapAutomapSnapshot);
constexpr size_t kRecordBytesV8 =
    kRecordBytesV7 + sizeof(EspNativeGameplayMonsterStateSnapshot);

constexpr size_t kRecordBytesV9 =
    kRecordBytesV7 + sizeof(NativeSaveRecordV9Tail);
constexpr size_t kRecordBytesV10 = kRecordBytesV9 + sizeof(EspPlayerLevelProgress);
constexpr size_t kRecordBytesV11 =
    kRecordBytesV10 + sizeof(EspNativeGameplayMonsterDropSnapshot);
static_assert(sizeof(EspPlayerLevelProgress) == 16U && kRecordBytesV10 == 5460U,
              "v10 appends only the 16-byte level progress suffix");
static_assert(sizeof(EspNativeGameplayMonsterDropSnapshot) == 144U &&
                  kRecordBytesV11 == 5604U,
              "v11 appends only the 144-byte dynamic monster-drop snapshot");

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
static_assert(sizeof(EspMapSpriteTopologyMonsterSnapshot) == 816U,
              "monster-topology checkpoint must remain exactly 816 bytes");
static_assert(sizeof(EspNativeGameplayMonsterPositionSnapshot) == 812U,
              "monster-position checkpoint must remain exactly 812 bytes");
static_assert(sizeof(EspNativeGameplayMonsterActivationSnapshot) == 268U,
              "monster-activation checkpoint must remain exactly 268 bytes");
static_assert(sizeof(NativeSaveRecordV9Tail) == 3508U,
              "native save v9 tail must remain exactly 3508 bytes");
static_assert(kRecordBytesV9 == 5444U,
              "native save v9 must remain the bounded 5444-byte streamed record");
static_assert(kRecordBytesV11 <= 0xffffU,
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

/* Legacy Game.newMapName/newDestX/newDestY/newAngle equivalent. Unlike the
 * transition owner this survives EspNativeGameplaySession_reset(). It remains
 * intentionally separate from the V9 on-disk checkpoint until that format has
 * a dedicated route section. */
EspMapSaveRouteState transitionSaveRoute;

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

uint32_t checkpointFNVByte(uint32_t hash, uint8_t value) {
    hash ^= value;
    return hash * 16777619U;
}

uint32_t checkpointFNV16(uint32_t hash, uint16_t value) {
    hash = checkpointFNVByte(hash, (uint8_t)(value & 0xffU));
    return checkpointFNVByte(hash, (uint8_t)((value >> 8) & 0xffU));
}

uint32_t checkpointFNV32(uint32_t hash, uint32_t value) {
    hash = checkpointFNV16(hash, (uint16_t)(value & 0xffffU));
    return checkpointFNV16(hash, (uint16_t)((value >> 16) & 0xffffU));
}

uint32_t monsterStateSnapshotFNV(
    const EspNativeGameplayMonsterStateSnapshot& snapshot) {
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (snapshot.count > ESP_NATIVE_GAMEPLAY_MONSTER_MAX_COUNT) return 0U;
    for (i = 0U; i < snapshot.count; ++i) {
        const EspNativeGameplayMonsterRecord& record = snapshot.records[i];
        hash = checkpointFNV32(hash, record.param1);
        hash = checkpointFNV32(hash, record.param2);
        hash = checkpointFNV16(hash, record.spriteIndex);
        hash = checkpointFNV16(hash, record.defTile);
        hash = checkpointFNVByte(hash, record.subtype);
        hash = checkpointFNVByte(hash, record.mType);
        hash = checkpointFNVByte(hash, record.alternateAttack);
        hash = checkpointFNVByte(hash, record.alive);
    }
    return hash;
}

bool reconcileMonsterSnapshotWithTopology(
    EspNativeGameplayMonsterStateSnapshot* monsters,
    const EspMapSpriteTopologyMonsterSnapshot& topology,
    uint16_t* outReconciled) {
    uint32_t monsterIndex;
    uint32_t topologyIndex = 0U;
    uint16_t reconciled = 0U;

    if (outReconciled != nullptr) *outReconciled = 0U;
    if (monsters == nullptr ||
        topology.count < monsters->count ||
        topology.count > ESP_MAP_SPRITE_TOPOLOGY_MONSTER_SNAPSHOT_MAX) {
        return false;
    }

    for (monsterIndex = 0U; monsterIndex < monsters->count; ++monsterIndex) {
        EspNativeGameplayMonsterRecord& monster =
            monsters->records[monsterIndex];

        while (topologyIndex < topology.count &&
               topology.records[topologyIndex].spriteIndex <
                   monster.spriteIndex) {
            ++topologyIndex;
        }
        if (topologyIndex >= topology.count ||
            topology.records[topologyIndex].spriteIndex !=
                monster.spriteIndex) {
            return false;
        }

        /*
         * EV_SHOW blocker removal intentionally mutates compact topology first
         * and defers the legacy enemy gameplay consequences. For checkpoint
         * persistence, however, a topology-dead enemy must resume logically
         * dead as well or V9 would either reject the save or resurrect it on
         * LOAD. Reconcile only the snapshot copy: no XP/drop/sound/RNG is
         * synthesized and the live gameplay owner remains untouched.
         */
        if ((topology.records[topologyIndex].linkState &
             ESP_MAP_SPRITE_TOPOLOGY_ALIVE) == 0U &&
            monster.alive != 0U) {
            monster.alive = 0U;
            ++reconciled;
        }
    }

    monsters->stateFNV1a = monsterStateSnapshotFNV(*monsters);
    if (outReconciled != nullptr) *outReconciled = reconciled;
    return monsters->stateFNV1a != 0U &&
           EspNativeGameplayMonsterState_snapshotShapeValid(
               monsters, monsters->sourceArenaFNV1a);
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

uint32_t recordCrcV9(
    const NativeSaveRecordV5& prefix,
    const EspNativeGameplayCrateTransformSnapshot& crateTransforms,
    const EspMapAutomapSnapshot& automap,
    const NativeSaveRecordV9Tail& tail,
    const EspPlayerLevelProgress* progress = nullptr,
    const EspNativeGameplayMonsterDropSnapshot* monsterDrops = nullptr) {
    const uint8_t* segments[6] = {
        reinterpret_cast<const uint8_t*>(&prefix),
        reinterpret_cast<const uint8_t*>(&crateTransforms),
        reinterpret_cast<const uint8_t*>(&automap),
        reinterpret_cast<const uint8_t*>(&tail),
        reinterpret_cast<const uint8_t*>(progress),
        reinterpret_cast<const uint8_t*>(monsterDrops)
    };
    const size_t sizes[6] = {
        sizeof(prefix), sizeof(crateTransforms), sizeof(automap), sizeof(tail),
        progress != nullptr ? sizeof(*progress) : 0U,
        monsterDrops != nullptr ? sizeof(*monsterDrops) : 0U
    };
    const size_t zeroOffset = offsetof(NativeSaveCore, recordCrc32);
    const size_t zeroEnd = zeroOffset + sizeof(uint32_t);
    uint32_t crc = 0xffffffffU;
    uint8_t segment;
    size_t i;
    uint32_t bit;

    for (segment = 0U; segment < 6U; ++segment) {
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

bool monsterSpatialShapeValid(
    const NativeSaveRecordV9Tail& tail,
    const NativeSaveCore& core) {
    uint32_t i;
    uint32_t topologyCursor = 0U;

    if (!EspNativeGameplayMonsterState_snapshotShapeValid(
            &tail.monsters, core.runtimeFNV1a) ||
        !EspMapSpriteTopology_monsterSnapshotShapeValid(
            &tail.monsterTopology, core.runtimeFNV1a) ||
        !EspNativeGameplayMonsterPosition_snapshotShapeValid(
            &tail.monsterPositions, core.runtimeFNV1a) ||
        !EspNativeGameplayMonsterActivation_snapshotShapeValid(
            &tail.monsterActivation, core.runtimeFNV1a) ||
        tail.monsters.count != tail.monsterPositions.count ||
        tail.monsterTopology.count < tail.monsters.count) {
        return false;
    }

    for (i = 0U; i < tail.monsters.count; ++i) {
        const EspNativeGameplayMonsterRecord& monster =
            tail.monsters.records[i];
        const EspNativeGameplayMonsterPositionRecord& position =
            tail.monsterPositions.records[i];

        while (topologyCursor < tail.monsterTopology.count &&
               tail.monsterTopology.records[topologyCursor].spriteIndex <
                   monster.spriteIndex) {
            ++topologyCursor;
        }
        if (topologyCursor >= tail.monsterTopology.count ||
            tail.monsterTopology.records[topologyCursor].spriteIndex !=
                monster.spriteIndex) {
            printf("[NATIVESAVE] V9-SPATIAL-MISMATCH ordinal=%u sprite=%u reason=monster-topology-record-missing topologyCount=%u\n",
                   (unsigned int)i,
                   (unsigned int)monster.spriteIndex,
                   (unsigned int)tail.monsterTopology.count);
            return false;
        }

        const EspMapSpriteTopologyMonsterRecord& topology =
            tail.monsterTopology.records[topologyCursor];
        const bool topologyAlive =
            (topology.linkState & ESP_MAP_SPRITE_TOPOLOGY_ALIVE) != 0U;
        const bool topologyLinked =
            (topology.linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) != 0U;

        /*
         * MonsterState owns logical death. Ordinary combat can leave the raw
         * topology ALIVE bit set while the combat projection masks it, so
         * logical-dead + raw-alive is valid. The inverse is reconciled in the
         * checkpoint snapshot before this validator runs.
         */
        if (monster.spriteIndex != position.spriteIndex ||
            (monster.alive != 0U && !topologyAlive) ||
            (topologyLinked &&
             position.tileIndex !=
                 (uint16_t)(topology.linkState &
                            ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK))) {
            printf("[NATIVESAVE] V9-SPATIAL-MISMATCH ordinal=%u sprite=%u topoSprite=%u posSprite=%u monsterAlive=%u topologyAlive=%u topologyLinked=%u topoTile=%u posTile=%u\n",
                   (unsigned int)i,
                   (unsigned int)monster.spriteIndex,
                   (unsigned int)topology.spriteIndex,
                   (unsigned int)position.spriteIndex,
                   (unsigned int)monster.alive,
                   topologyAlive ? 1U : 0U,
                   topologyLinked ? 1U : 0U,
                   (unsigned int)(topology.linkState &
                                  ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK),
                   (unsigned int)position.tileIndex);
            return false;
        }
    }

    for (i = 0U; i < tail.monsterActivation.activeOrderCount; ++i) {
        const uint16_t activeSprite =
            tail.monsterActivation.activeOrder[i];
        uint32_t j;
        bool found = false;
        for (j = 0U; j < tail.monsters.count; ++j) {
            if (tail.monsters.records[j].spriteIndex == activeSprite) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

bool levelProgressShapeValid(const EspPlayerLevelProgress& progress,
                              const NativeSaveCore& core) {
    return progress.targetMapId == core.targetMapId &&
           progress.xpBaseline <= core.player.xpGained &&
           progress.complete <= 1U && progress.reserved[0] == 0U &&
           progress.reserved[1] == 0U;
}

bool loadedV9Valid(
    const LoadedSaveRecord& record,
    const EspNativeGameplayCrateTransformSnapshot& crateTransforms,
    const EspMapAutomapSnapshot& automap,
    const NativeSaveRecordV9Tail& tail,
    const EspNativeGameplayMonsterDropSnapshot* monsterDrops) {
    const bool v11 = record.core.version == kVersionV11;
    const bool v10 = record.core.version == kVersionV10;
    const bool coreOk = v11
        ? coreShapeValid(record.core, kMagicV11, kVersionV11,
                         (uint16_t)kRecordBytesV11) &&
          levelProgressShapeValid(record.levelProgress, record.core)
        : (v10
               ? coreShapeValid(record.core, kMagicV10, kVersionV10,
                                (uint16_t)kRecordBytesV10) &&
                 levelProgressShapeValid(record.levelProgress, record.core)
               : coreShapeValid(record.core, kMagicV9, kVersionV9,
                                (uint16_t)kRecordBytesV9));
    const bool dropsOk =
        !v11 ||
        (monsterDrops != nullptr &&
         EspNativeGameplayMonsterDrop_snapshotShapeValid(
             monsterDrops, record.core.runtimeFNV1a));
    const uint32_t actualCrc =
        recordCrcV9(*reinterpret_cast<const NativeSaveRecordV5*>(&record),
                    crateTransforms, automap, tail,
                    (v10 || v11) ? &record.levelProgress : nullptr,
                    v11 ? monsterDrops : nullptr);
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
    const bool spatialOk = monsterSpatialShapeValid(tail, record.core);
    const bool valid =
        coreOk && crcOk && resourcesOk && scriptOk && linesOk &&
        removedOk && cratesOk && disjointOk && automapOk && spatialOk &&
        dropsOk;

    if (!valid) {
        printf("[NATIVESAVE] SPATIAL-VALIDATE version=%u core=%u crc=%u storedCrc=%08x actualCrc=%08x resources=%u script=%u lines=%u removed=%u cratesFile=%u disjoint=%u automap=%u monsterSpatial=%u monsterDrops=%u failClosed=yes\n",
               (unsigned int)record.core.version,
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
               spatialOk ? 1U : 0U,
               dropsOk ? 1U : 0U);
    }
    return valid;
}

bool recordCurrentValid(
    const NativeSaveRecordV5& prefix,
    const EspNativeGameplayCrateTransformSnapshot& crateTransforms,
    const EspMapAutomapSnapshot& automap,
    const NativeSaveRecordV9Tail& tail,
    const EspPlayerLevelProgress& progress,
    const EspNativeGameplayMonsterDropSnapshot& monsterDrops) {
    return coreShapeValid(prefix.core, kMagicV11, kVersionV11,
                          (uint16_t)kRecordBytesV11) &&
           levelProgressShapeValid(progress, prefix.core) &&
           EspNativeGameplayMonsterDrop_snapshotShapeValid(
               &monsterDrops, prefix.core.runtimeFNV1a) &&
           prefix.core.recordCrc32 ==
               recordCrcV9(prefix, crateTransforms, automap, tail, &progress,
                           &monsterDrops) &&
           resourceShapeValid(prefix.resources, prefix.core) &&
           scriptShapeValid(prefix.script, prefix.core) &&
           lineShapeValid(prefix.lines, prefix.core) &&
           actionRemovedShapeValid(prefix.actionRemoved, prefix.core) &&
           EspNativeGameplayCrateState_snapshotShapeValid(
               &crateTransforms, prefix.core.runtimeFNV1a,
               prefix.core.targetMapId) &&
           crateTransformsDisjoint(prefix.actionRemoved, crateTransforms) &&
           automapShapeValid(automap, prefix.core) &&
           monsterSpatialShapeValid(tail, prefix.core);
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

    if (fileBytes == kRecordBytesV9 || fileBytes == kRecordBytesV10 ||
        fileBytes == kRecordBytesV11) {
        EspNativeGameplayCrateTransformSnapshot crateTransforms;
        EspMapAutomapSnapshot automap;
        EspNativeGameplayMonsterDropSnapshot monsterDrops;
        NativeSaveRecordV9Tail* tail;

        /*
         * Keep the cold MENU_MAIN probe bounded: close the SD size-probe
         * handle before reserving the 3508-byte V9 tail workspace, then reopen
         * for the exact read. The largest section remains smaller than the
         * already-observed classic-CYD contiguous 8-bit heap margin.
         */
        file.close();
        tail = (NativeSaveRecordV9Tail*)malloc(sizeof(*tail));
        if (tail == nullptr) {
            printf("[NATIVESAVE] READABLE-V9 FAILED path=%s stage=tail-workspace bytes=%u failClosed=yes\n",
                   path, (unsigned int)sizeof(*tail));
            return false;
        }
        file = SD.open(path, FILE_READ);
        if (!file || (size_t)file.size() != fileBytes) {
            if (file) file.close();
            free(tail);
            printf("[NATIVESAVE] READABLE-V9 FAILED path=%s stage=reopen expectedBytes=%u failClosed=yes\n",
                   path, (unsigned int)fileBytes);
            return false;
        }

        memset(&crateTransforms, 0, sizeof(crateTransforms));
        memset(&automap, 0, sizeof(automap));
        memset(&monsterDrops, 0, sizeof(monsterDrops));
        memset(tail, 0, sizeof(*tail));
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
            got = file.read(reinterpret_cast<uint8_t*>(tail),
                            sizeof(*tail));
        }
        bool sectionsRead = got == sizeof(*tail);
        if (sectionsRead &&
            (fileBytes == kRecordBytesV10 || fileBytes == kRecordBytesV11)) {
            sectionsRead = file.read(
                reinterpret_cast<uint8_t*>(&outRecord->levelProgress),
                sizeof(outRecord->levelProgress)) == sizeof(outRecord->levelProgress);
        }
        if (sectionsRead && fileBytes == kRecordBytesV11) {
            sectionsRead = file.read(
                reinterpret_cast<uint8_t*>(&monsterDrops),
                sizeof(monsterDrops)) == sizeof(monsterDrops);
        }
        file.close();

        const bool valid =
            sectionsRead &&
            outRecord->core.recordBytes == fileBytes &&
            loadedV9Valid(*outRecord, crateTransforms, automap, *tail,
                          fileBytes == kRecordBytesV11 ? &monsterDrops : nullptr);
        printf("[NATIVESAVE] READABLE-SPATIAL path=%s bytes=%u tailWorkspace=%u monsterState=%u monsterTopology=%u monsterPosition=%u monsterActivation=%u result=%s\n",
               path,
               (unsigned int)fileBytes,
               (unsigned int)sizeof(*tail),
               (unsigned int)sizeof(tail->monsters),
               (unsigned int)sizeof(tail->monsterTopology),
               (unsigned int)sizeof(tail->monsterPositions),
               (unsigned int)sizeof(tail->monsterActivation),
               valid ? "valid" : "invalid");
        free(tail);
        if (!valid) {
            memset(outRecord, 0, sizeof(*outRecord));
            return false;
        }
        outRecord->fileBytes = (uint16_t)fileBytes;
        outRecord->hasResources = 1U;
        outRecord->hasScript = 1U;
        outRecord->hasLines = 1U;
        outRecord->hasActionRemoved = 1U;
        outRecord->hasAutomap = 1U;
        outRecord->hasMonsterSpatial = 1U;
        outRecord->hasMonsterDrops =
            fileBytes == kRecordBytesV11 ? 1U : 0U;
        return true;
    }

    file.close();
    return false;
}

bool readBestRecord(LoadedSaveRecord* outRecord, bool* outRecoveredBackup) {
    if (outRecoveredBackup != nullptr) *outRecoveredBackup = false;
    if (readRecordPath(kSavePath, outRecord)) {
        activeReadPath = kSavePath;
        return true;
    }
    if (readRecordPath(kBackupPath, outRecord)) {
        activeReadPath = kBackupPath;
        if (outRecoveredBackup != nullptr) *outRecoveredBackup = true;
        return true;
    }
    /* The pre-multislot checkpoint remains untouched, readable as slot 1. */
    if (activeSlot == 1U && readRecordPath(kLegacySavePath, outRecord)) {
        activeReadPath = kLegacySavePath;
        return true;
    }
    if (activeSlot == 1U && readRecordPath(kLegacyBackupPath, outRecord)) {
        activeReadPath = kLegacyBackupPath;
        if (outRecoveredBackup != nullptr) *outRecoveredBackup = true;
        return true;
    }
    return false;
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

bool writeExactCurrent(
    const char* path,
    const NativeSaveRecordV5& prefix,
    const EspNativeGameplayCrateTransformSnapshot& crateTransforms,
    const EspMapAutomapSnapshot& automap,
    const NativeSaveRecordV9Tail& tail,
    const EspPlayerLevelProgress& progress,
    const EspNativeGameplayMonsterDropSnapshot& monsterDrops) {
    File file = SD.open(path, FILE_WRITE);
    size_t wrotePrefix;
    size_t wroteCrate;
    size_t wroteAutomap;
    size_t wroteTail;
    size_t wroteProgress;
    size_t wroteMonsterDrops;
    if (!file) return false;
    wrotePrefix = file.write(reinterpret_cast<const uint8_t*>(&prefix),
                             sizeof(prefix));
    wroteCrate = file.write(reinterpret_cast<const uint8_t*>(&crateTransforms),
                            sizeof(crateTransforms));
    wroteAutomap = file.write(reinterpret_cast<const uint8_t*>(&automap),
                              sizeof(automap));
    wroteTail = file.write(reinterpret_cast<const uint8_t*>(&tail),
                           sizeof(tail));
    wroteProgress = file.write(reinterpret_cast<const uint8_t*>(&progress),
                               sizeof(progress));
    wroteMonsterDrops = file.write(
        reinterpret_cast<const uint8_t*>(&monsterDrops),
        sizeof(monsterDrops));
    file.flush();
    file.close();
    return wrotePrefix == sizeof(prefix) &&
           wroteCrate == sizeof(crateTransforms) &&
           wroteAutomap == sizeof(automap) &&
           wroteTail == sizeof(tail) && wroteProgress == sizeof(progress) &&
           wroteMonsterDrops == sizeof(monsterDrops);
}

bool readExactCurrentMatches(
    const char* path,
    const NativeSaveRecordV5& expectedPrefix,
    const EspNativeGameplayCrateTransformSnapshot& expectedCrate,
    const EspMapAutomapSnapshot& expectedAutomap,
    const NativeSaveRecordV9Tail& expectedTail,
    const EspPlayerLevelProgress& expectedProgress,
    const EspNativeGameplayMonsterDropSnapshot& expectedMonsterDrops) {
    File file;
    uint8_t verify[64];
    const uint8_t* segments[6] = {
        reinterpret_cast<const uint8_t*>(&expectedPrefix),
        reinterpret_cast<const uint8_t*>(&expectedCrate),
        reinterpret_cast<const uint8_t*>(&expectedAutomap),
        reinterpret_cast<const uint8_t*>(&expectedTail),
        reinterpret_cast<const uint8_t*>(&expectedProgress),
        reinterpret_cast<const uint8_t*>(&expectedMonsterDrops)
    };
    const size_t sizes[6] = {
        sizeof(expectedPrefix), sizeof(expectedCrate), sizeof(expectedAutomap),
        sizeof(expectedTail), sizeof(expectedProgress),
        sizeof(expectedMonsterDrops)
    };
    uint8_t segment;

    if (path == nullptr || !SD.exists(path)) return false;
    file = SD.open(path, FILE_READ);
    if (!file || (size_t)file.size() != kRecordBytesV11) {
        if (file) file.close();
        return false;
    }

    for (segment = 0U; segment < 6U; ++segment) {
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

bool commitRecordAtomicCurrent(
    const NativeSaveRecordV5& prefix,
    const EspNativeGameplayCrateTransformSnapshot& crateTransforms,
    const EspMapAutomapSnapshot& automap,
    const NativeSaveRecordV9Tail& tail,
    const EspPlayerLevelProgress& progress,
    const EspNativeGameplayMonsterDropSnapshot& monsterDrops) {
    bool movedOld = false;

    if (SD.exists(kTempPath)) (void)SD.remove(kTempPath);
    if (SD.exists(kBackupPath)) (void)SD.remove(kBackupPath);
    if (!writeExactCurrent(kTempPath, prefix, crateTransforms, automap, tail,
                           progress, monsterDrops) ||
        !readExactCurrentMatches(
            kTempPath, prefix, crateTransforms, automap, tail, progress,
            monsterDrops)) {
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

    if (!readExactCurrentMatches(
            kSavePath, prefix, crateTransforms, automap, tail, progress,
            monsterDrops)) {
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
         core.version != kVersionV8 && core.version != kVersionV9 &&
         core.version != kVersionV10 && core.version != kVersionV11)) {
        return false;
    }
    const size_t expectedBytes =
        core.version == kVersionV11 ? kRecordBytesV11 : core.version == kVersionV10
            ? kRecordBytesV10 : core.version == kVersionV9
            ? kRecordBytesV9
            : (core.version == kVersionV8
                   ? kRecordBytesV8
                   : (core.version == kVersionV7
                          ? kRecordBytesV7
                          : kRecordBytesV6));
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
        (core.version != kVersionV7 && core.version != kVersionV8 &&
         core.version != kVersionV9 && core.version != kVersionV10 &&
         core.version != kVersionV11)) {
        return false;
    }
    const size_t expectedBytes =
        core.version == kVersionV11 ? kRecordBytesV11 : core.version == kVersionV10
            ? kRecordBytesV10 : core.version == kVersionV9
            ? kRecordBytesV9
            : (core.version == kVersionV8 ? kRecordBytesV8 : kRecordBytesV7);
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

bool recoverOneShotTopologyFromScript(
    const NativeSaveCore& core,
    uint16_t* outShowApplied,
    uint16_t* outShowAlreadyLinked,
    uint16_t* outHideApplied) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint32_t eventIndex;
    uint16_t showApplied = 0U;
    uint16_t showAlreadyLinked = 0U;
    uint16_t hideApplied = 0U;

    if (outShowApplied != nullptr) *outShowApplied = 0U;
    if (outShowAlreadyLinked != nullptr) *outShowAlreadyLinked = 0U;
    if (outHideApplied != nullptr) *outHideApplied = 0U;

    if ((core.version != kVersionV8 && core.version != kVersionV9 &&
         core.version != kVersionV10 && core.version != kVersionV11) ||
        runtime == nullptr ||
        runtime->arenaFNV1a != core.runtimeFNV1a ||
        !EspMapScriptState_isReady() ||
        !EspMapSpriteTopology_isReady()) {
        return false;
    }

    /*
     * V8 persisted the script removed-command bitmap but not mutable sprite
     * topology. A one-shot SHOW/HIDE with REMOVE set therefore carries enough
     * durable evidence that the command executed successfully before SAVE.
     * Replay only that narrow family, in map command order, against the freshly
     * rebuilt topology. This deliberately does not guess monster movement.
     */
    for (eventIndex = 0U; eventIndex < runtime->eventCount; ++eventIndex) {
        uint32_t rawEvent;
        EspMapEventRef ref;
        EspMapEventDescriptor descriptor;
        uint32_t offset;

        if (!EspMapRuntime_getEvent(eventIndex, &rawEvent)) return false;
        ref.index = (uint16_t)eventIndex;
        ref.tileIndex = (uint16_t)(rawEvent & ESP_MAP_EVENT_TILE_MASK);
        ref.value = rawEvent;
        if (!EspMapEvents_describe(&ref, &descriptor)) return false;

        for (offset = 0U; offset < descriptor.commandCount; ++offset) {
            const uint32_t global =
                (uint32_t)descriptor.firstCommandIndex + offset;
            EspMapByteCode command;
            uint8_t removed = 0U;

            if (!EspMapEvents_getCommand(&descriptor, offset, &command) ||
                !EspMapScriptState_isCommandRemoved(global, &removed)) {
                return false;
            }
            if (removed == 0U ||
                (command.arg2 &
                 ESP_MAP_SPRITE_TOPOLOGY_COMMAND_FLAG_REMOVE) == 0U ||
                (command.id != ESP_MAP_OPCODE_SHOW &&
                 command.id != ESP_MAP_OPCODE_HIDE)) {
                continue;
            }

            if (command.id == ESP_MAP_OPCODE_SHOW) {
                const uint16_t spriteIndex =
                    (uint16_t)(command.arg1 & 0xffffU);
                uint8_t type = 0U;
                uint8_t subtype = 0U;
                uint16_t linkState = 0U;
                uint16_t linkOrder = 0U;
                EspMapShowResult result;
                EspMapSpriteTopologyStatus status;

                if (!EspMapSpriteTopology_getEntity(
                        spriteIndex, &type, &subtype,
                        &linkState, &linkOrder)) {
                    return false;
                }
                (void)type;
                (void)subtype;
                (void)linkOrder;

                if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) != 0U) {
                    ++showAlreadyLinked;
                    printf("[NATIVESAVE] V8-TOPOLOGY SHOW global=%u event=%u off=%u sprite=%u action=already-linked\n",
                           (unsigned int)global,
                           (unsigned int)eventIndex,
                           (unsigned int)offset,
                           (unsigned int)spriteIndex);
                    continue;
                }

                memset(&result, 0, sizeof(result));
                status = EspMapSpriteTopology_applyShow(
                    &descriptor, offset, &result);
                if (status != ESP_MAP_SPRITE_TOPOLOGY_OK ||
                    result.removeCommandIfHandled == 0U) {
                    printf("[NATIVESAVE] V8-TOPOLOGY FAILED global=%u event=%u off=%u opcode=SHOW sprite=%u status=%u remove=%u failClosed=yes\n",
                           (unsigned int)global,
                           (unsigned int)eventIndex,
                           (unsigned int)offset,
                           (unsigned int)spriteIndex,
                           (unsigned int)status,
                           (unsigned int)result.removeCommandIfHandled);
                    return false;
                }
                ++showApplied;
                printf("[NATIVESAVE] V8-TOPOLOGY SHOW global=%u event=%u off=%u sprite=%u tile=%u linked=%u blockersRemoved=%u\n",
                       (unsigned int)global,
                       (unsigned int)eventIndex,
                       (unsigned int)offset,
                       (unsigned int)result.spriteIndex,
                       (unsigned int)result.tileIndex,
                       (unsigned int)result.targetLinkedAfter,
                       (unsigned int)result.blockersRemoved);
            }
            else {
                EspMapHideResult result;
                EspMapSpriteTopologyStatus status;
                memset(&result, 0, sizeof(result));
                status = EspMapSpriteTopology_applyHide(
                    &descriptor, offset, &result);
                if (status != ESP_MAP_SPRITE_TOPOLOGY_OK ||
                    result.removeCommandIfHandled == 0U) {
                    printf("[NATIVESAVE] V8-TOPOLOGY FAILED global=%u event=%u off=%u opcode=HIDE status=%u remove=%u failClosed=yes\n",
                           (unsigned int)global,
                           (unsigned int)eventIndex,
                           (unsigned int)offset,
                           (unsigned int)status,
                           (unsigned int)result.removeCommandIfHandled);
                    return false;
                }
                ++hideApplied;
                printf("[NATIVESAVE] V8-TOPOLOGY HIDE global=%u event=%u off=%u tile=%u hidden=%u\n",
                       (unsigned int)global,
                       (unsigned int)eventIndex,
                       (unsigned int)offset,
                       (unsigned int)result.tileIndex,
                       (unsigned int)result.hiddenEntityCount);
            }
        }
    }

    if (outShowApplied != nullptr) *outShowApplied = showApplied;
    if (outShowAlreadyLinked != nullptr)
        *outShowAlreadyLinked = showAlreadyLinked;
    if (outHideApplied != nullptr) *outHideApplied = hideApplied;

    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    printf("[NATIVESAVE] LEGACY-TOPOLOGY-REPLAY version=%u showApplied=%u showAlreadyLinked=%u hideApplied=%u topologyFNV=%08x linked=%u hidden=%u evidence=script-removed+remove-flag movement=not-guessed exactSpatial=no\n",
           (unsigned int)core.version,
           (unsigned int)showApplied,
           (unsigned int)showAlreadyLinked,
           (unsigned int)hideApplied,
           topology != nullptr ? (unsigned int)topology->stateFNV1a : 0U,
           topology != nullptr ? (unsigned int)topology->linkedCount : 0U,
           topology != nullptr ? (unsigned int)topology->hiddenCount : 0U);
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

bool restoreV9MonsterSpatialSections(
    const char* path,
    const NativeSaveCore& core,
    uint16_t* outCount,
    uint32_t* outMonsterFNV,
    uint32_t* outTopologyFNV,
    uint32_t* outPositionFNV,
    uint32_t* outActivationFNV) {
    File file;
    size_t got;
    NativeSaveRecordV9Tail* tail;
    bool ok = false;
    uint8_t legacyEnemyOnly = 0U;
    uint16_t replayShow = 0U;
    uint16_t replayAlreadyLinked = 0U;
    uint16_t replayHide = 0U;

    if (path == nullptr || !SD.exists(path) ||
        (core.version != kVersionV9 && core.version != kVersionV10 &&
         core.version != kVersionV11)) {
        return false;
    }

    tail = (NativeSaveRecordV9Tail*)malloc(sizeof(*tail));
    if (tail == nullptr) {
        printf("[NATIVESAVE] V9-SPATIAL-FAILED stage=workspace bytes=%u failClosed=yes\n",
               (unsigned int)sizeof(*tail));
        return false;
    }
    memset(tail, 0, sizeof(*tail));

    file = SD.open(path, FILE_READ);
    if (!file || (size_t)file.size() != core.recordBytes ||
        !file.seek(kRecordBytesV7)) {
        if (file) file.close();
        free(tail);
        return false;
    }
    got = file.read(reinterpret_cast<uint8_t*>(tail), sizeof(*tail));
    file.close();

    if (got == sizeof(*tail) &&
        monsterSpatialShapeValid(*tail, core)) {
        const EspMapSpriteTopologyView* liveTopology =
            EspMapSpriteTopology_view();

        if (liveTopology == nullptr) {
            ok = false;
        }
        else {
            legacyEnemyOnly =
                (uint8_t)(liveTopology->destructibleCount != 0U &&
                          tail->monsterTopology.count ==
                              liveTopology->enemyCount);
            if (legacyEnemyOnly != 0U &&
                !recoverOneShotTopologyFromScript(
                    core, &replayShow, &replayAlreadyLinked, &replayHide)) {
                ok = false;
            }
            else if (EspNativeGameplayMonsterState_stageRestore(
                         &tail->monsters) &&
                     EspMapSpriteTopology_restoreMonsterSnapshot(
                         &tail->monsterTopology) &&
                     EspNativeGameplayMonsterPosition_stageRestore(
                         &tail->monsterPositions) &&
                     EspNativeGameplayMonsterActivation_restoreSnapshot(
                         &tail->monsterActivation)) {
                ok = true;
            }
        }
    }

    if (ok) {
        if (outCount != nullptr) *outCount = tail->monsters.count;
        if (outMonsterFNV != nullptr)
            *outMonsterFNV = tail->monsters.stateFNV1a;
        if (outTopologyFNV != nullptr)
            *outTopologyFNV = tail->monsterTopology.stateFNV1a;
        if (outPositionFNV != nullptr)
            *outPositionFNV = tail->monsterPositions.stateFNV1a;
        if (outActivationFNV != nullptr)
            *outActivationFNV = tail->monsterActivation.stateFNV1a;
        printf("[NATIVESAVE] V9-SPATIAL-STAGE monsters=%u topologyTracked=%u monsterFNV=%08x topologyFNV=%08x positionFNV=%08x activationFNV=%08x scope=%s legacyReplay=%u/%u/%u exact=%s\n",
               (unsigned int)tail->monsters.count,
               (unsigned int)tail->monsterTopology.count,
               (unsigned int)tail->monsters.stateFNV1a,
               (unsigned int)tail->monsterTopology.stateFNV1a,
               (unsigned int)tail->monsterPositions.stateFNV1a,
               (unsigned int)tail->monsterActivation.stateFNV1a,
               legacyEnemyOnly != 0U
                   ? "enemy-only-legacy-v9+script-replay"
                   : "enemy+destructible-v9",
               (unsigned int)replayShow,
               (unsigned int)replayAlreadyLinked,
               (unsigned int)replayHide,
               legacyEnemyOnly != 0U ? "tracked-enemy+replayed-nonenemy"
                                     : "yes");
    }
    else {
        EspNativeGameplayMonsterPosition_reset();
        EspNativeGameplayMonsterActivation_reset();
        EspNativeGameplayMonsterState_reset();
        printf("[NATIVESAVE] V9-SPATIAL-FAILED stage=validate-or-restore failClosed=yes\n");
    }

    free(tail);
    return ok;
}

bool readMonsterDropSection(
    const char* path,
    const NativeSaveCore& core,
    EspNativeGameplayMonsterDropSnapshot* outSnapshot) {
    File file;
    size_t got;

    if (path == nullptr || outSnapshot == nullptr || !SD.exists(path) ||
        core.version != kVersionV11 ||
        core.recordBytes != (uint16_t)kRecordBytesV11) {
        return false;
    }
    file = SD.open(path, FILE_READ);
    if (!file || (size_t)file.size() != kRecordBytesV11 ||
        !file.seek(kRecordBytesV10)) {
        if (file) file.close();
        return false;
    }
    memset(outSnapshot, 0, sizeof(*outSnapshot));
    got = file.read(reinterpret_cast<uint8_t*>(outSnapshot),
                    sizeof(*outSnapshot));
    file.close();
    return got == sizeof(*outSnapshot) &&
           EspNativeGameplayMonsterDrop_snapshotShapeValid(
               outSnapshot, core.runtimeFNV1a);
}

bool restoreMonsterDropSection(
    const char* path,
    const NativeSaveCore& core,
    uint8_t* outVisible,
    uint32_t* outStateFNV,
    uint32_t* outSpawnSerial,
    uint8_t* outNextSlot) {
    EspNativeGameplayMonsterDropSnapshot snapshot;
    const EspNativeGameplayMonsterDropView* view;

    memset(&snapshot, 0, sizeof(snapshot));
    if (!readMonsterDropSection(path, core, &snapshot) ||
        !EspNativeGameplayMonsterDrop_restore(&snapshot)) {
        EspNativeGameplayMonsterDrop_reset();
        return false;
    }
    view = EspNativeGameplayMonsterDrop_view();
    if (view == nullptr || view->sourceArenaFNV1a != core.runtimeFNV1a ||
        view->spawnSerial != snapshot.spawnSerial ||
        view->nextSlot != snapshot.nextSlot ||
        EspNativeGameplayMonsterDrop_fingerprint() != snapshot.stateFNV1a) {
        EspNativeGameplayMonsterDrop_reset();
        return false;
    }
    if (outVisible != nullptr) *outVisible = view->visibleCount;
    if (outStateFNV != nullptr) *outStateFNV = snapshot.stateFNV1a;
    if (outSpawnSerial != nullptr) *outSpawnSerial = snapshot.spawnSerial;
    if (outNextSlot != nullptr) *outNextSlot = snapshot.nextSlot;
    printf("[MONSTERDROP] RESTORE version=11 arena=%08x serial=%u next=%u visible=%u stateFNV=%08x rng=untouched materialize=replay-no\n",
           (unsigned int)core.runtimeFNV1a,
           (unsigned int)snapshot.spawnSerial,
           (unsigned int)snapshot.nextSlot,
           (unsigned int)view->visibleCount,
           (unsigned int)snapshot.stateFNV1a);
    return true;
}

bool captureRecord(
    NativeSaveRecordV5* outPrefix,
    EspNativeGameplayCrateTransformSnapshot* outCrateTransforms,
    EspMapAutomapSnapshot* outAutomap,
    NativeSaveRecordV9Tail* outTail,
    EspPlayerLevelProgress* outProgress,
    EspNativeGameplayMonsterDropSnapshot* outMonsterDrops) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspPlayerViewState* view = EspPlayerView_view();

    if (outPrefix == nullptr || outCrateTransforms == nullptr ||
        outAutomap == nullptr || outTail == nullptr || outProgress == nullptr ||
        outMonsterDrops == nullptr || EspAssetPack_isOpen() || runtime == nullptr || view == nullptr ||
        !EspMapResidentLifecycle_isReady()) {
        return false;
    }

    NativeSaveRecordV5& record = *outPrefix;
    memset(outPrefix, 0, sizeof(*outPrefix));
    memset(outCrateTransforms, 0, sizeof(*outCrateTransforms));
    memset(outAutomap, 0, sizeof(*outAutomap));
    memset(outTail, 0, sizeof(*outTail));
    memset(outMonsterDrops, 0, sizeof(*outMonsterDrops));

    if (!EspNativeGameplayPlayerState_snapshot(&record.core.player)) {
        printf("[NATIVESAVE] V11-CAPTURE-FAILED stage=player-state\n");
        return false;
    }
    if (!EspPlayerFreshMap_snapshotProgress(millis(), outProgress)) {
        printf("[NATIVESAVE] V11-CAPTURE-FAILED stage=level-progress\n");
        return false;
    }
    if (!EspNativeGameplayPlayerResources_snapshot(&record.resources)) {
        printf("[NATIVESAVE] V11-CAPTURE-FAILED stage=resources\n");
        return false;
    }
    if (!EspMapScriptState_snapshot(&record.script)) {
        printf("[NATIVESAVE] V11-CAPTURE-FAILED stage=script\n");
        return false;
    }
    if (!EspMapLineCheckpoint_snapshot(&record.lines)) {
        printf("[NATIVESAVE] V11-CAPTURE-FAILED stage=lines\n");
        return false;
    }
    if (!EspNativeGameplayActionEngine_snapshotRemoved(
            &record.actionRemoved)) {
        printf("[NATIVESAVE] V11-CAPTURE-FAILED stage=action-removed\n");
        return false;
    }
    if (!EspNativeGameplayCrateState_snapshot(outCrateTransforms)) {
        printf("[NATIVESAVE] V11-CAPTURE-FAILED stage=crate-transforms\n");
        return false;
    }
    if (!EspMapAutomapState_snapshot(outAutomap)) {
        printf("[NATIVESAVE] V11-CAPTURE-FAILED stage=automap\n");
        return false;
    }
    if (!EspNativeGameplayMonsterState_snapshot(&outTail->monsters)) {
        printf("[NATIVESAVE] V11-CAPTURE-FAILED stage=monster-state\n");
        return false;
    }
    if (!EspMapSpriteTopology_snapshotMonsters(
            &outTail->monsterTopology)) {
        printf("[NATIVESAVE] V11-CAPTURE-FAILED stage=monster-topology\n");
        return false;
    }
    {
        uint16_t reconciledShowDeaths = 0U;
        if (!reconcileMonsterSnapshotWithTopology(
                &outTail->monsters, outTail->monsterTopology,
                &reconciledShowDeaths)) {
            printf("[NATIVESAVE] V11-CAPTURE-FAILED stage=monster-topology-reconcile\n");
            return false;
        }
        if (reconciledShowDeaths != 0U) {
            printf("[NATIVESAVE] V11-MONSTER-RECONCILE topologyDeadToLogicalDead=%u sideEffects=not-synthesized snapshotOnly=yes\n",
                   (unsigned int)reconciledShowDeaths);
        }
    }
    if (!EspNativeGameplayMonsterPosition_snapshot(
            &outTail->monsterPositions)) {
        printf("[NATIVESAVE] V11-CAPTURE-FAILED stage=monster-position\n");
        return false;
    }
    if (!EspNativeGameplayMonsterActivation_snapshot(
            &outTail->monsterActivation)) {
        printf("[NATIVESAVE] V11-CAPTURE-FAILED stage=monster-activation\n");
        return false;
    }
    if (!EspNativeGameplayMonsterDrop_snapshot(outMonsterDrops)) {
        printf("[NATIVESAVE] V11-CAPTURE-FAILED stage=monster-drops\n");
        return false;
    }

    memcpy(record.core.magic, kMagicV11, sizeof(kMagicV11));
    record.core.version = kVersionV11;
    record.core.recordBytes = (uint16_t)kRecordBytesV11;
    record.core.sourceBytes = runtime->sourceBytes;
    record.core.sourceCrc32 = runtime->sourceCrc32;
    record.core.runtimeFNV1a = runtime->arenaFNV1a;
    record.core.playerFNV1a = EspNativeGameplayPlayerState_fingerprint();
    record.core.targetMapId = view->targetMapId;
    record.core.gameplayLoadMapId = view->gameplayLoadMapId;
    record.core.loadType = view->loadType;
    record.core.view = *view;

    if (view->active != 1U || view->targetMapId == 0U ||
        runtime->sourceBytes == 0U || runtime->sourceCrc32 == 0U ||
        runtime->arenaFNV1a == 0U) {
        printf("[NATIVESAVE] V11-CAPTURE-FAILED stage=core-runtime-shape viewActive=%u map=%u sourceBytes=%u sourceCrc=%08x arena=%08x\n",
               (unsigned int)view->active,
               (unsigned int)view->targetMapId,
               (unsigned int)runtime->sourceBytes,
               (unsigned int)runtime->sourceCrc32,
               (unsigned int)runtime->arenaFNV1a);
        return false;
    }
    if (!monsterSpatialShapeValid(*outTail, record.core)) {
        printf("[NATIVESAVE] V11-CAPTURE-FAILED stage=monster-spatial-cross-check monsters=%u topology=%u positions=%u activation=%u\n",
               (unsigned int)outTail->monsters.count,
               (unsigned int)outTail->monsterTopology.count,
               (unsigned int)outTail->monsterPositions.count,
               (unsigned int)outTail->monsterActivation.activeOrderCount);
        return false;
    }

    record.core.recordCrc32 =
        recordCrcV9(record, *outCrateTransforms, *outAutomap, *outTail,
                    outProgress, outMonsterDrops);
    return recordCurrentValid(
        record, *outCrateTransforms, *outAutomap, *outTail, *outProgress,
        *outMonsterDrops);
}

bool saveNow(void) {
    NativeSaveRecordV5& record = saveWorkspace.write;
    EspNativeGameplayCrateTransformSnapshot crateTransforms;
    EspMapAutomapSnapshot automap;
    EspPlayerLevelProgress progress;
    EspNativeGameplayMonsterDropSnapshot monsterDrops;
    NativeSaveRecordV9Tail* tail =
        (NativeSaveRecordV9Tail*)malloc(sizeof(*tail));
    uint32_t scriptFNV;
    uint32_t openCount;
    uint32_t lockedCount;
    uint32_t texture10Count;
    uint32_t removedCount;
    uint32_t automapFNV;
    uint32_t automapVisitedCount;
    const EspMapSaveRouteState* returnRoute =
        EspNativeGameplaySave_transitionRoute();

    if (tail == nullptr) {
        printf("[NATIVESAVE] SAVE-FAILED path=%s version=11 stage=v9-tail-workspace bytes=%u failClosed=yes\n",
               kLogPath, (unsigned int)sizeof(*tail));
        return false;
    }

    memset(&record, 0, sizeof(record));
    memset(&crateTransforms, 0, sizeof(crateTransforms));
    memset(&automap, 0, sizeof(automap));
    memset(&monsterDrops, 0, sizeof(monsterDrops));
    memset(tail, 0, sizeof(*tail));

    if (!captureRecord(&record, &crateTransforms, &automap, tail, &progress,
                       &monsterDrops) ||
        !commitRecordAtomicCurrent(record, crateTransforms, automap, *tail,
                                   progress, monsterDrops)) {
        free(tail);
        printf("[NATIVESAVE] SAVE-FAILED path=%s version=11 sections=resources+script+lines+action-removals+crate-transforms+automap+monster-state+monster-topology+monster-position+monster-activation+monster-drops failClosed=yes\n",
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
    {
        const EspNativeGameplayMonsterDropView* dropView =
            EspNativeGameplayMonsterDrop_view();
        printf("[MONSTERDROP] SAVE version=11 arena=%08x serial=%u next=%u visible=%u stateFNV=%08x snapshotBytes=%u rng=untouched\n",
               (unsigned int)monsterDrops.sourceArenaFNV1a,
               (unsigned int)monsterDrops.spawnSerial,
               (unsigned int)monsterDrops.nextSlot,
               dropView != nullptr ? (unsigned int)dropView->visibleCount : 0U,
               (unsigned int)monsterDrops.stateFNV1a,
               (unsigned int)sizeof(monsterDrops));
    }

    printf("[LEVELPROGRESS] SAVE map=%u elapsedMs=%lu moves=%lu xpBaseline=%lu complete=%u suffixBytes=%u\n",
           (unsigned int)progress.targetMapId, (unsigned long)progress.elapsedMs,
           (unsigned long)progress.moves, (unsigned long)progress.xpBaseline,
           (unsigned int)progress.complete, (unsigned int)sizeof(progress));

    printf("[NATIVESAVE] SAVE path=%s version=%u bytes=%u map=%u gameplayLoadMapId=%u pos=%ld,%ld angle=%ld returnRoute=%s/%u,%u/%u playerFNV=%08lx runtimeFNV=%08lx sourceBytes=%lu sourceCrc=%08lx recordCrc=%08lx resources=%u/%uB sprites=%u script=%lu/%lu/%uB scriptFNV=%08lx lines=%lu/%uB open=%lu locked=%lu texture10=%lu lineFNV=%08lx textureFNV=%08lx actionRemoved=%lu/%uB/%08lx crateTransforms=%u/%uB/%uB/%08lx automap=%uL/%uS/%luV/%08lx monsters=%u/%08lx topology=%u/%08lx positions=%u/%08lx activation=%u/%08lx atomic=temp+backup+rename world=monster-spatial-exact-v9\n",
           kLogPath,
           (unsigned int)record.core.version,
           (unsigned int)record.core.recordBytes,
           (unsigned int)record.core.targetMapId,
           (unsigned int)record.core.gameplayLoadMapId,
           (long)record.core.view.viewX,
           (long)record.core.view.viewY,
           (long)record.core.view.viewAngle,
           returnRoute != nullptr ? returnRoute->mapName : "-",
           returnRoute != nullptr ? (unsigned int)returnRoute->destinationX : 0U,
           returnRoute != nullptr ? (unsigned int)returnRoute->destinationY : 0U,
           returnRoute != nullptr ? (unsigned int)returnRoute->angle : 0U,
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
           (unsigned int)tail->monsters.count,
           (unsigned long)tail->monsters.stateFNV1a,
           (unsigned int)tail->monsterTopology.count,
           (unsigned long)tail->monsterTopology.stateFNV1a,
           (unsigned int)tail->monsterPositions.count,
           (unsigned long)tail->monsterPositions.stateFNV1a,
           (unsigned int)tail->monsterActivation.activeOrderCount,
           (unsigned long)tail->monsterActivation.stateFNV1a);
    free(tail);
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
    bool loadingPresentation = false;
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
    uint16_t v8ShowApplied = 0U;
    uint16_t v8ShowAlreadyLinked = 0U;
    uint16_t v8HideApplied = 0U;
    uint32_t monsterTopologyFNV = 0U;
    uint32_t monsterPositionFNV = 0U;
    uint32_t monsterActivationFNV = 0U;
    uint8_t monsterDropVisible = 0U;
    uint32_t monsterDropFNV = 0U;
    uint32_t monsterDropSerial = 0U;
    uint8_t monsterDropNext = 0U;
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
    selectedPath = activeReadPath;
    name = EspMapCatalog_nameForId(record->targetMapId);
    if (name == nullptr || name[0] == '\0') {
        printf("[NATIVESAVE] LOAD-FAILED path=%s stage=MAP_ID map=%u failClosed=yes\n",
               kLogPath, (unsigned int)record->targetMapId);
        return false;
    }

    /*
     * The checkpoint restore is one pipeline for MENU_MAIN and in-game HUB.
     * Tear down any current gameplay/HUB owners before transition presentation
     * takes the shared framebuffer. No old-session cleanup may run after the
     * loading owner has begun.
     */
    EspNativeGameplaySession_reset();

    loadingPresentation =
        EspNativeTransitionPresentation_beginLoading(record->targetMapId) != 0;
    if (loadingPresentation) {
        EspNativeTransitionPresentation_checkpointProgress(10U, "CHECKPOINT");
        printf("[NATIVESAVE] LOAD-UI targetMap=%u presentation=active progress=10 source=checkpoint\n",
               (unsigned int)record->targetMapId);
    }
    else {
        printf("[NATIVESAVE] LOAD-UI targetMap=%u presentation=deferred gameplayLoad=continues\n",
               (unsigned int)record->targetMapId);
    }

    /* Rebuild immutable map/runtime first. V1 checkpoints stop at player+pose.
     * V2 adds the consumed player-resource overlay. V3 adds the compact native
     * script/event mutable owner. V4 adds the complete compact line family:
     * open/locked bits plus mutable 9/10 texture variants. V5 adds the compact
     * action-engine sprite-removal overlay. V6 appends canonical crate
     * transformed-definition state. V7 appends the compact Automap reveal
     * snapshot: lines, sprites and BIT_AM_VISITED tiles. V8 appends the exact
     * native logical monster records (HP/armor/stats/alternate attack/alive)
     * without rerolling their generation RNG. V9 closes the missing spatial
     * half: monster topology/linkage, exact positions and activation order.
     * V10 adds level-progress reporting state. V11 appends the exact compact
     * eight-slot dynamic monster-drop pool; restore never rerolls or respawns. */
    EspMapResidentLifecycle_resetAll();
    resetSpawnOwners();

    if (!inventoryForRecord(*record, &inventory)) {
        if (loadingPresentation) {
            EspNativeTransitionPresentation_abortLoading("BSP_ID");
        }
        printf("[NATIVESAVE] LOAD-FAILED path=%s stage=BSP_ID map=%u sourceBytes=%lu sourceCrc=%08lx failClosed=yes\n",
               kLogPath,
               (unsigned int)record->targetMapId,
               (unsigned long)record->sourceBytes,
               (unsigned long)record->sourceCrc32);
        return false;
    }
    if (loadingPresentation) {
        EspNativeTransitionPresentation_checkpointProgress(30U, "BSP");
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
        if (loadingPresentation) {
            EspNativeTransitionPresentation_abortLoading("RESIDENT");
        }
        printf("[NATIVESAVE] LOAD-FAILED path=%s stage=RESIDENT status=%u map=%u expectedRuntime=%08lx actualRuntime=%08lx failClosed=yes\n",
               kLogPath,
               (unsigned int)residentStatus,
               (unsigned int)record->targetMapId,
               (unsigned long)record->runtimeFNV1a,
               actualRuntime);
        return false;
    }
    if (loadingPresentation) {
        EspNativeTransitionPresentation_checkpointProgress(60U, "RUNTIME");
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
        (record->version >= kVersionV10 &&
         !EspPlayerFreshMap_restoreProgress(&loaded.levelProgress)) ||
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
          record->version == kVersionV8 ||
          record->version >= kVersionV9) &&
         !restoreCrateSection(selectedPath, *record,
                              &crateTransformCount,
                              &crateTransformFNV)) ||
        ((record->version == kVersionV7 ||
          record->version == kVersionV8 ||
          record->version >= kVersionV9) &&
         !restoreAutomapSection(selectedPath, *record,
                                &automapLineCount,
                                &automapSpriteCount,
                                &automapVisitedCount,
                                &automapFNV)) ||
        (record->version == kVersionV8 &&
         !recoverOneShotTopologyFromScript(
             *record, &v8ShowApplied, &v8ShowAlreadyLinked,
             &v8HideApplied)) ||
        (record->version == kVersionV8 &&
         !stageV8MonsterSection(selectedPath, *record,
                                &monsterCount, &monsterFNV)) ||
        (record->version >= kVersionV9 &&
         !restoreV9MonsterSpatialSections(
             selectedPath, *record,
             &monsterCount, &monsterFNV,
             &monsterTopologyFNV, &monsterPositionFNV,
             &monsterActivationFNV)) ||
        (record->version == kVersionV11 &&
         !restoreMonsterDropSection(
             selectedPath, *record, &monsterDropVisible, &monsterDropFNV,
             &monsterDropSerial, &monsterDropNext)) ||
        !sessionConfigForPlayer(record->player, &config) ||
        !EspNativeGameplaySession_configureResume(&config)) {
        resetFailedLoad();
        if (loadingPresentation) {
            EspNativeTransitionPresentation_abortLoading("RESTORE");
        }
        printf("[NATIVESAVE] LOAD-FAILED path=%s stage=RESTORE map=%u version=%u resources=%s script=%s lines=%s actionRemoved=%s crateTransforms=%s automap=%s monsters=%s monsterSpatial=%s monsterDrops=%s playerFNV=%08lx failClosed=yes\n",
               kLogPath,
               (unsigned int)record->targetMapId,
               (unsigned int)record->version,
               loaded.hasResources == 1U ? "required" : "legacy-none",
               loaded.hasScript == 1U ? "required" : "legacy-none",
               loaded.hasLines == 1U ? "required" : "legacy-none",
               loaded.hasActionRemoved == 1U ? "required" : "legacy-none",
               (record->version == kVersionV6 ||
                record->version == kVersionV7 ||
                record->version == kVersionV8 ||
                record->version >= kVersionV9)
                   ? "required"
                   : "legacy-none",
               (record->version == kVersionV7 ||
                record->version == kVersionV8 ||
                record->version >= kVersionV9)
                   ? "required"
                   : "legacy-none",
               (record->version == kVersionV8 ||
                record->version >= kVersionV9)
                   ? "required"
                   : "legacy-none",
               record->version >= kVersionV9 ? "required" : "legacy-none",
               record->version == kVersionV11 ? "required" : "legacy-empty",
               (unsigned long)record->playerFNV1a);
        return false;
    }

    if (record->version < kVersionV10) {
        EspPlayerFreshMap_resumeLegacy(record->targetMapId, record->player.xpGained);
    }
    if (record->version < kVersionV11) {
        EspNativeGameplayMonsterDrop_reset();
        printf("[NATIVESAVE] LEGACY-MONSTER-DROP-GAP version=%u dynamicDrops=fresh-empty warning=drop-pool-not-present-in-record rng=untouched\n",
               (unsigned int)record->version);
    }
    printf("[LEVELPROGRESS] LOAD map=%u history=%s timer=resume-on-gameplay\n",
           (unsigned int)record->targetMapId,
           record->version >= kVersionV10 && loaded.levelProgress.complete
               ? "full-level" : "since-load");
    if (loadingPresentation) {
        EspNativeTransitionPresentation_checkpointProgress(75U, "STATE");
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

    if (record->version == kVersionV8) {
        printf("[NATIVESAVE] LEGACY-MONSTER-SPATIAL-MIGRATION version=8 oneShotShow=%u alreadyLinked=%u oneShotHide=%u topology=recovered-from-script position=fresh-at-recovered-topology activation=fresh warning=historical-monster-movement-not-present-in-v8\n",
               (unsigned int)v8ShowApplied,
               (unsigned int)v8ShowAlreadyLinked,
               (unsigned int)v8HideApplied);
    }
    else if (record->version < kVersionV8) {
        printf("[NATIVESAVE] LEGACY-MONSTER-SPATIAL-GAP version=%u topology+position+activation=fresh warning=monster-visibility-and-position-cannot-be-restored-exactly-from-this-record\n",
               (unsigned int)record->version);
    }

    const char* worldSummary =
        record->version >= kVersionV11
            ? "resources+script+lines+action-removals+crate-transforms+automap+monster-state+topology+position+activation+monster-drops-restored-exact"
            : (record->version >= kVersionV9
                   ? "resources+script+lines+action-removals+crate-transforms+automap+monster-state+topology+position+activation-restored+monster-drops-fresh-empty"
                   : (record->version == kVersionV8
                          ? "resources+script+lines+action-removals+crate-transforms+automap+monster-state-restored+monster-spatial-fresh-lossy"
                          : (record->version == kVersionV7
                                 ? "resources+script+lines+action-removals+crate-transforms+automap-restored+monster-state+position+activation-fresh"
                                 : (record->version == kVersionV6
                                        ? "resources+script+lines+action-removals+crate-transforms-restored+automap+monster-state+position+activation-fresh"
                                        : "legacy-partial-world"))));

    printf("[NATIVESAVE] LOAD path=%s version=%u bytes=%u map=%u gameplayLoadMapId=%u pos=%ld,%ld angle=%ld playerFNV=%08lx runtimeFNV=%08lx sourceBytes=%lu sourceCrc=%08lx backupRecovery=%s resources=%s/%u/%uB script=%s/%lu/%lu/%uB/%08lx lines=%s/%lu/%uB/open%lu/locked%lu/tex10%lu/%08lx/%08lx actionRemoved=%s/%lu/%uB/%08lx crateTransforms=%s/%u/%08lx automap=%s/%uL/%uS/%uV/%08lx monsters=%s/%u/%08lx topology=%s/%08lx positions=%s/%08lx activation=%s/%08lx monsterDrops=%s/%u/%08lx/serial%u/next%u world=%s session=reprime-pending\n",
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
            record->version == kVersionV8 ||
            record->version >= kVersionV9)
               ? "restored" : "legacy-none",
           (unsigned int)crateTransformCount,
           (unsigned long)crateTransformFNV,
           (record->version == kVersionV7 ||
            record->version == kVersionV8 ||
            record->version >= kVersionV9)
               ? "restored" : "legacy-none",
           (unsigned int)automapLineCount,
           (unsigned int)automapSpriteCount,
           (unsigned int)automapVisitedCount,
           (unsigned long)automapFNV,
           (record->version == kVersionV8 ||
            record->version >= kVersionV9)
               ? "staged" : "legacy-none",
           (unsigned int)monsterCount,
           (unsigned long)monsterFNV,
           record->version >= kVersionV9 ? "restored" : "legacy-fresh",
           (unsigned long)monsterTopologyFNV,
           record->version >= kVersionV9 ? "staged" : "legacy-fresh",
           (unsigned long)monsterPositionFNV,
           record->version >= kVersionV9 ? "restored" : "legacy-fresh",
           (unsigned long)monsterActivationFNV,
           record->version == kVersionV11 ? "restored" : "legacy-empty",
           (unsigned int)monsterDropVisible,
           (unsigned long)monsterDropFNV,
           (unsigned int)monsterDropSerial,
           (unsigned int)monsterDropNext,
           worldSummary);
    if (loadingPresentation) {
        EspNativeTransitionPresentation_checkpointProgress(85U, "RESTORE");
        printf("[NATIVESAVE] LOAD-UI targetMap=%u progress=85 owner=retained-until-session-active gameplayPresents=blocked primeProgress=session-stages\n",
               (unsigned int)record->targetMapId);
    }
    return true;
}

/*
 * V1..V11 all start with the identical on-disk NativeSaveCore prefix.
 * Inspect only that small header to paint the menu; full CRC and complete
 * structural validation still belong exclusively to readBestRecord() on LOAD.
 * No checkpoint rewrite, full-save read, heap allocation or live-player query.
 */
bool readSlotPreviewCore(const char* path, NativeSaveCore* outCore) {
    File file;
    NativeSaveCore core;
    const uint8_t* expectedMagic = nullptr;
    size_t expectedBytes = 0U;
    size_t actualBytes;
    if (path == nullptr || outCore == nullptr || !SD.exists(path)) return false;
    file = SD.open(path, FILE_READ);
    if (!file) return false;
    actualBytes = (size_t)file.size();
    if (actualBytes < sizeof(core) ||
        file.read(reinterpret_cast<uint8_t*>(&core), sizeof(core)) !=
            sizeof(core)) {
        file.close();
        return false;
    }
    file.close();
    switch (core.version) {
    case kVersionV1: expectedMagic = kMagicV1; expectedBytes = sizeof(NativeSaveCore); break;
    case kVersionV2: expectedMagic = kMagicV2; expectedBytes = sizeof(NativeSaveRecordV2); break;
    case kVersionV3: expectedMagic = kMagicV3; expectedBytes = sizeof(NativeSaveRecordV3); break;
    case kVersionV4: expectedMagic = kMagicV4; expectedBytes = sizeof(NativeSaveRecordV4); break;
    case kVersionV5: expectedMagic = kMagicV5; expectedBytes = sizeof(NativeSaveRecordV5); break;
    case kVersionV6: expectedMagic = kMagicV6; expectedBytes = kRecordBytesV6; break;
    case kVersionV7: expectedMagic = kMagicV7; expectedBytes = kRecordBytesV7; break;
    case kVersionV8: expectedMagic = kMagicV8; expectedBytes = kRecordBytesV8; break;
    case kVersionV9: expectedMagic = kMagicV9; expectedBytes = kRecordBytesV9; break;
    case kVersionV10: expectedMagic = kMagicV10; expectedBytes = kRecordBytesV10; break;
    case kVersionV11: expectedMagic = kMagicV11; expectedBytes = kRecordBytesV11; break;
    default: return false;
    }
    if (actualBytes != expectedBytes ||
        !coreShapeValid(core, expectedMagic, core.version,
                        (uint16_t)expectedBytes) ||
        core.player.level == 0U) return false;
    *outCore = core;
    return true;
}

/* Compact labels fit the existing 107-pixel-wide, seven-pixel-tall row font. */
const char* slotMapCaption(uint8_t mapId, char* sector, size_t capacity) {
    if (!EspMapCatalog_isValidId(mapId)) return nullptr;
    switch (mapId) {
    case ESP_MAP_ID_INTRO: return "ENTRANCE";
    case ESP_MAP_ID_JUNCTION: return "JUNCTION";
    case 10U: return "JCT RUIN";
    case 11U: return "ITEMS";
    case 12U: return "REACTOR";
    case ESP_MAP_ID_END_GAME: return "ENDGAME";
    default:
        if (mapId >= 2U && mapId <= 8U &&
            sector != nullptr && capacity >= 9U) {
            snprintf(sector, capacity, "SECTOR %u", (unsigned)(mapId - 1U));
            return sector;
        }
        return nullptr;
    }
}

bool formatSlotCaption(uint8_t slot, bool armed, char* out, size_t capacity) {
    char path[48];
    char backup[52];
    char sector[12];
    NativeSaveCore core;
    bool occupied = false;
    const char* candidates[4];
    if (out == nullptr || capacity == 0U || slot < 1U || slot > 10U)
        return false;
    snprintf(path, sizeof(path), "/DoomRPG-ESP32-slot%02u.sav",
             (unsigned)slot);
    snprintf(backup, sizeof(backup), "/DoomRPG-ESP32-slot%02u.sav.bak",
             (unsigned)slot);
    candidates[0] = path;
    candidates[1] = backup;
    candidates[2] = slot == 1U ? kLegacySavePath : nullptr;
    candidates[3] = slot == 1U ? kLegacyBackupPath : nullptr;
    for (uint8_t i = 0U; i < 4U; ++i) {
        const char* candidate = candidates[i];
        if (candidate == nullptr || !SD.exists(candidate)) continue;
        occupied = true;
        if (!readSlotPreviewCore(candidate, &core)) continue;
        const char* mapName =
            slotMapCaption(core.targetMapId, sector, sizeof(sector));
        if (mapName == nullptr) continue;
        snprintf(out, capacity, "%02u %s LVL %u%s",
                 (unsigned)slot, mapName, (unsigned)core.player.level,
                 armed ? "?" : "");
        /* 18 glyphs * 6px minus 1 = 107px: the exact slot card width.
         * Keep the armed '?' and the map name readable at two/three digits. */
        if (strlen(out) > 18U) {
            snprintf(out, capacity, "%02u %s L%u%s",
                     (unsigned)slot, mapName, (unsigned)core.player.level,
                     armed ? "?" : "");
        }
        return true;
    }
    /* Unknown/corrupt headers keep their original occupied indicator. */
    snprintf(out, capacity, "%02u %s%s", (unsigned)slot,
             occupied ? "OCCUPIED" : "-- EMPTY --", armed ? "?" : "");
    return occupied;
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

void drawCenteredWord(uint16_t* fb, int centerX, int y, const char* text,
                      uint16_t color) {
    EspNativeGameplayHubTouchUi_drawCrispText(fb, text, centerX, y, color);
}

/* Two chunky industrial page keys, fixed pixels and no extra framebuffer. */
/* Two distinct page actions: PREV (upper), NEXT (lower).
 * The inactive direction is visibly disabled and does not consume state. */
void drawSlotNav(uint16_t* fb, int top, int bottom, bool next, bool enabled) {
    const int x0 = 118, x1 = 156;
    const int cx = 137, cy = (top + bottom) / 2;
    const uint16_t color = enabled ? ESP_HUB_COLOR_AMBER : ESP_HUB_COLOR_STEEL_DARK;
    fillRect(fb, x0, top, x1, bottom, enabled ? ESP_HUB_COLOR_PANEL_ALT : ESP_HUB_COLOR_PANEL);
    drawRect(fb, x0, top, x1, bottom, enabled ? ESP_HUB_COLOR_STEEL : ESP_HUB_COLOR_STEEL_DARK);
    /* Broad 2px chevron, pointing UP for previous, DOWN for next. */
    for (int i = 0; i < 7; ++i) {
        const int yy = next ? cy - 3 + i : cy + 3 - i;
        const int left = cx - 7 + i;
        const int right = cx + 7 - i;
        fillRect(fb, left, yy, left + 2, yy + 1, color);
        fillRect(fb, right - 2, yy, right, yy + 1, color);
    }
}

bool paintSaveOverlay(void) {
    const EspNativeGameplayHubView* hub = EspNativeGameplayHub_view();
    uint16_t* fb;
    const char* saveLabel = "SAVE";
    const char* loadLabel = "LOAD";
    const char* exitLabel = "EXIT TO MENU";
    uint16_t saveColor = ESP_HUB_COLOR_IVORY;
    uint16_t loadColor = ESP_HUB_COLOR_IVORY;
    const bool confirmExit = confirmationTarget == kStatusExit;
    const bool hasSave = slotMode == 0U &&
        EspNativeGameplaySave_hasReadableCheckpoint() != 0;
    size_t expected = (size_t)DOOMRPG_LOGICAL_WIDTH *
                      (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);

    if (hub == nullptr || hub->active != 1U ||
        hub->page != ESP_NATIVE_GAMEPLAY_HUB_PAGE_SYSTEM ||
        Esp32PlatformVideo_framebuffer() == nullptr ||
        Esp32PlatformVideo_framebufferSizeBytes() != expected) {
        return false;
    }

    fb = static_cast<uint16_t*>(Esp32PlatformVideo_framebuffer());
    if (slotMode != 0U) {
        const uint8_t first = slotFocus <= 5U ? 1U : 6U;
        char label[40];
        fillRect(fb, 2, 35, 157, 118, ESP_HUB_COLOR_BG);
        snprintf(label, sizeof(label), "%s - PAGE %u/2", slotMode == 1U ? "SAVE" : "LOAD",
                 first == 1U ? 1U : 2U);
        drawCenteredWord(fb, 60, 35, label, ESP_HUB_COLOR_IVORY);
        for (uint8_t row = 0U; row < 5U; ++row) {
            const uint8_t slot = first + row;
            const int top = 47 + row * 14;
            const bool focused = slot == slotFocus;
            const bool available = formatSlotCaption(
                slot, focused && slotArmed == slot, label, sizeof(label));
            fillRect(fb, 7, top, 113, top + 12,
                     focused ? ESP_HUB_COLOR_PANEL_ALT : ESP_HUB_COLOR_PANEL);
            drawRect(fb, 7, top, 113, top + 12,
                     focused ? ESP_HUB_COLOR_AMBER : ESP_HUB_COLOR_STEEL_DARK);
            drawCenteredWord(fb, 60, top + 3, label,
                             available ? (focused ? ESP_HUB_COLOR_GREEN : ESP_HUB_COLOR_IVORY)
                                       : ESP_HUB_COLOR_STEEL_DARK);
        }
        drawSlotNav(fb, 49, 78, false, first != 1U);
        drawSlotNav(fb, 83, 112, true, first == 1U);
        return Esp32PlatformVideo_present();
    }


    fillRect(fb, 2, 35, 157, 118, ESP_HUB_COLOR_BG);
    fillRect(fb, kPanelLeft, kPanelTop, kPanelRight, kPanelBottom,
             ESP_HUB_COLOR_PANEL);
    drawRect(fb, kPanelLeft, kPanelTop, kPanelRight, kPanelBottom,
             ESP_HUB_COLOR_STEEL);
    for (uint8_t row = 0U; row < kStatusCount; ++row) {
        const int top = systemRowTop(row);
        const int bottom = top + ESP_NATIVE_SYS_BUTTON_HEIGHT - 1;
        const bool selected = statusCursor == row;
        const uint16_t accent = row == kStatusExit && confirmExit
            ? ESP_HUB_COLOR_RED : ESP_HUB_COLOR_AMBER;
        fillRect(fb, kButtonLeft, top, kButtonRight, bottom,
                 selected ? ESP_HUB_COLOR_PANEL_ALT : ESP_HUB_COLOR_BG);
        drawRect(fb, kButtonLeft, top, kButtonRight, bottom,
                 selected ? accent : ESP_HUB_COLOR_STEEL_DARK);
        if (selected) fillRect(fb, kButtonLeft + 3, top + 4,
                              kButtonLeft + 5, bottom - 4, accent);
    }

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

    if (confirmExit) exitLabel = "EXIT TO MENU?";
    else if (lastOperation == 3U && !lastOperationOk) exitLabel = "EXIT FAILED";
    drawCenteredWord(fb, 80, systemRowTop(kStatusSave) + 8, saveLabel, saveColor);
    drawCenteredWord(fb, 80, systemRowTop(kStatusLoad) + 8, loadLabel, loadColor);
    drawCenteredWord(fb, 80, systemRowTop(kStatusExit) + (confirmExit ? 4 : 8),
                     exitLabel, confirmExit ? ESP_HUB_COLOR_AMBER : ESP_HUB_COLOR_IVORY);
    if (confirmExit) drawCenteredWord(fb, 80, systemRowTop(kStatusExit) + 14,
                                     "UNSAVED CHANGES LOST", ESP_HUB_COLOR_RED);
    return Esp32PlatformVideo_present();
}

}  // namespace

bool readableSaveExists(void);

extern "C" int EspNativeGameplaySave_mainSelectorActive(void) { return slotMode == 3U; }
extern "C" int EspNativeGameplaySave_mainSelectorReady(void) { return slotMode == 3U && mainSlotReady != 0U; }
extern "C" void EspNativeGameplaySave_mainSelectorFinish(void) {
    slotMode = 0U; slotArmed = 0U; mainSlotReady = 0U;
}
static void paintMainSlots(void) {
    uint16_t* fb = static_cast<uint16_t*>(Esp32PlatformVideo_framebuffer());
    if (!fb || Esp32PlatformVideo_framebufferSizeBytes() != 38400U) return;
    const uint8_t first = slotFocus <= 5U ? 1U : 6U;
    char text[40];
    fillRect(fb, 0, 0, 159, 119, ESP_HUB_COLOR_BG);
    snprintf(text, sizeof(text), "LOAD - PAGE %u/2", first == 1U ? 1U : 2U);
    drawCenteredWord(fb, 60, 15, text, ESP_HUB_COLOR_IVORY);
    for (uint8_t row = 0U; row < 5U; ++row) {
        const uint8_t slot = first + row;
        const int y = 32 + row * 14;
        const bool focused = slot == slotFocus;
        const bool occupied = formatSlotCaption(
            slot, focused && slotArmed == slot, text, sizeof(text));
        fillRect(fb, 7, y, 113, y + 12, focused ? ESP_HUB_COLOR_PANEL_ALT : ESP_HUB_COLOR_PANEL);
        drawRect(fb, 7, y, 113, y + 12, focused ? ESP_HUB_COLOR_AMBER : ESP_HUB_COLOR_STEEL_DARK);
        drawCenteredWord(fb, 60, y + 3, text,
                         occupied ? (focused ? ESP_HUB_COLOR_GREEN : ESP_HUB_COLOR_IVORY)
                                  : ESP_HUB_COLOR_STEEL_DARK);
    }
    drawSlotNav(fb, 34, 62, false, first != 1U);
    drawSlotNav(fb, 69, 98, true, first == 1U);
    fillRect(fb, 34, 105, 111, 118, ESP_HUB_COLOR_PANEL_ALT);
    drawRect(fb, 34, 105, 111, 118, ESP_HUB_COLOR_RED);
    drawCenteredWord(fb, 72, 108, "BACK", ESP_HUB_COLOR_IVORY);
    (void)Esp32PlatformVideo_present();
}
extern "C" int EspNativeGameplaySave_mainSelectorBegin(void) {
    slotMode = 3U; slotFocus = 1U; slotArmed = 0U; mainSlotReady = 0U;
    selectSaveSlot(1U);
    paintMainSlots();
    printf("[SAVESLOTS] MAIN-OPEN slots=10 pages=2\n");
    return 1;
}
/* 0 stay, 1 confirmed, -1 back */
extern "C" int EspNativeGameplaySave_mainSelectorTap(int x, int y) {
    if (slotMode != 3U) return 0;
    if (y >= 104) {
        if (x >= 34 && x <= 111) {
            EspNativeGameplaySave_mainSelectorFinish();
            return -1;
        }
        return 0;
    }
    if (x >= 118 && x <= 156 && y >= 34 && y <= 98) {
        const bool previous = y <= 62;
        const uint8_t page = slotFocus <= 5U ? 1U : 2U;
        if ((previous && page != 2U) || (!previous && (y < 69 || page != 1U)))
            return 0;
        slotFocus = previous ? 1U : 6U;
        slotArmed = 0U;
        selectSaveSlot(slotFocus);
        printf("[SAVESLOTS] MAIN-PAGE page=%u via=right-column\n", slotFocus <= 5U ? 1U : 2U);
        paintMainSlots();
        return 0;
    }
    if (x < 7 || x > 113 || y < 32 || y >= 102) return 0;
    const uint8_t row = (uint8_t)((y - 32) / 14);
    const uint8_t selected = (uint8_t)((slotFocus <= 5U ? 1U : 6U) + row);
    if (selected > 10U) return 0;
    if (selected != slotFocus) { slotFocus = selected; slotArmed = 0U; }
    selectSaveSlot(slotFocus);
    if (!readableSaveExists()) {
        slotArmed = 0U;
        printf("[SAVESLOTS] MAIN-EMPTY slot=%u\n", (unsigned)slotFocus);
    } else if (slotArmed == slotFocus) {
        mainSlotReady = 1U;
        printf("[SAVESLOTS] MAIN-CONFIRM slot=%u\n", (unsigned)slotFocus);
        return 1;
    } else {
        slotArmed = slotFocus;
        printf("[SAVESLOTS] MAIN-ARM slot=%u\n", (unsigned)slotFocus);
    }
    paintMainSlots();
    return 0;
}

extern "C" int EspNativeGameplaySave_slotSelectorActive(void) { return slotMode != 0U; }
extern "C" int EspNativeGameplaySave_touchSlot(int x, int y) {
    if (slotMode == 0U || y < 47 || y > 116) return 0;
    if (x >= 118 && x <= 156) {
        const bool previous = y >= 49 && y <= 78;
        const bool next = y >= 83 && y <= 112;
        const uint8_t page = slotFocus <= 5U ? 1U : 2U;
        if ((!previous && !next) || (previous && page == 1U) ||
            (next && page == 2U)) return 1;
        slotFocus = previous ? 1U : 6U;
        slotArmed = 0U;
        selectSaveSlot(slotFocus);
        (void)paintSaveOverlay();
        printf("[SAVESLOTS] PAGE page=%u via=right-column\n",
               slotFocus <= 5U ? 1U : 2U);
        return 1;
    }
    if (x < 7 || x > 113) return 1;
    const uint8_t row = (uint8_t)((y - 47) / 14);
    if (row >= 5U) return 1;
    const uint8_t selected = (uint8_t)((slotFocus <= 5U ? 1U : 6U) + row);
    if (selected != slotFocus) {
        slotFocus = selected;
        slotArmed = selected; /* first tap selects and arms, next confirms */
        selectSaveSlot(slotFocus);
        (void)paintSaveOverlay();
        printf("[SAVESLOTS] ARM mode=%s slot=%u source=touch\n",
               slotMode == 1U ? "SAVE" : "LOAD", (unsigned)slotFocus);
        return 1;
    }
    return 2; /* let SELECT arm/confirm */
}

extern "C" int EspNativeGameplaySave_adoptTransitionRoute(
    const EspMapSaveRouteState* route) {
    uint8_t mapId = 0U;
    size_t length;

    if (route == nullptr || !EspMapSaveRoute_isActive(route) ||
        route->mapNameLength == 0U ||
        route->mapNameLength >= ESP_MAP_SAVE_ROUTE_NAME_CAPACITY ||
        route->mapName[route->mapNameLength] != '\0') {
        return 0;
    }
    length = strlen(route->mapName);
    if (length != route->mapNameLength ||
        !EspMapCatalog_idForName(route->mapName, &mapId) ||
        !EspMapCatalog_isValidId(mapId)) {
        return 0;
    }

    transitionSaveRoute = *route;
    printf("[NATIVESAVE] ROUTE-ADOPT map=%s mapId=%u pos=%u,%u angle=%u sourceEvent=%u sourceCmd=%u lifetime=player-save-across-session-reset\n",
           transitionSaveRoute.mapName,
           (unsigned int)mapId,
           (unsigned int)transitionSaveRoute.destinationX,
           (unsigned int)transitionSaveRoute.destinationY,
           (unsigned int)transitionSaveRoute.angle,
           (unsigned int)transitionSaveRoute.sourceEventIndex,
           (unsigned int)transitionSaveRoute.globalCommandIndex);
    return 1;
}

extern "C" const EspMapSaveRouteState*
EspNativeGameplaySave_transitionRoute(void) {
    return EspMapSaveRoute_isActive(&transitionSaveRoute)
               ? &transitionSaveRoute
               : nullptr;
}

extern "C" void EspNativeGameplaySave_clearTransitionRoute(void) {
    EspMapSaveRoute_reset(&transitionSaveRoute);
}

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
    if (slotMode != 0U) {
        selectSaveSlot(slotFocus);
        return readableSaveExists() ? 1 : 0;
    }
    /* MENU_MAIN reads the first valid manual checkpoint, not only slot 1. */
    for (uint8_t slot = 1U; slot <= 10U; ++slot) {
        selectSaveSlot(slot);
        if (readableSaveExists()) return 1;
    }
    selectSaveSlot(1U);
    return 0;
}

extern "C" int EspNativeGameplaySave_loadCheckpoint(void) {
    if (slotMode == 0U && !EspNativeGameplaySave_hasReadableCheckpoint()) return 0;
    if (slotMode != 0U) selectSaveSlot(slotFocus);
    if (!loadNow()) return 0;
    /*
     * V1..V11 do not serialize the legacy return route. Never leak a route from
     * the replaced live session into the restored checkpoint session.
     */
    EspNativeGameplaySave_clearTransitionRoute();
    return 1;
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
        if (slotMode != 0U &&
            (action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD ||
             action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK)) {
            slotFocus = (uint8_t)(1U + ((slotFocus - 1U +
                (action == ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK ? 1U : 9U)) % 10U));
            slotArmed = 0U;
            selectSaveSlot(slotFocus);
            if (!paintSaveOverlay()) return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
            return ESP_NATIVE_GAMEPLAY_HUB_REDRAWN;
        }
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
                   systemRowName(statusCursor));
            return ESP_NATIVE_GAMEPLAY_HUB_REDRAWN;
        }

        if (action == ESP_NATIVE_GAMEPLAY_ACTION_SELECT) {
            if (slotMode == 0U && statusCursor != kStatusExit) {
                slotMode = statusCursor == kStatusSave ? 1U : 2U;
                slotFocus = 1U;
                slotArmed = 0U;
                selectSaveSlot(slotFocus);
                printf("[SAVESLOTS] OPEN mode=%s slots=10 page=1 legacy=slot1-read-only\n",
                       slotMode == 1U ? "SAVE" : "LOAD");
                if (!paintSaveOverlay()) return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
                return ESP_NATIVE_GAMEPLAY_HUB_REDRAWN;
            }
            if (slotMode != 0U) {
                selectSaveSlot(slotFocus);
                if (slotMode == 2U && !readableSaveExists()) {
                    slotArmed = 0U;
                    printf("[SAVESLOTS] EMPTY slot=%u load=blocked\n", (unsigned)slotFocus);
                    if (!paintSaveOverlay()) return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
                    return ESP_NATIVE_GAMEPLAY_HUB_IGNORED;
                }
                if (slotArmed != slotFocus) {
                    slotArmed = slotFocus;
                    printf("[SAVESLOTS] ARM mode=%s slot=%u\n", slotMode == 1U ? "SAVE" : "LOAD",
                           (unsigned)slotFocus);
                    if (!paintSaveOverlay()) return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
                    return ESP_NATIVE_GAMEPLAY_HUB_REDRAWN;
                }
                printf("[SAVESLOTS] CONFIRM mode=%s slot=%u\n",
                       slotMode == 1U ? "SAVE" : "LOAD", (unsigned)slotFocus);
                slotArmed = 0U;
                /* Slot already armed by a previous SELECT: no third confirm. */
                confirmationTarget = statusCursor;
            }
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
                       systemRowName(statusCursor));
                return ESP_NATIVE_GAMEPLAY_HUB_REDRAWN;
            }
            confirmationTarget = kNoConfirmation;

            if (statusCursor == kStatusExit) {
                lastOperation = 3U;
                lastOperationOk = EspNativeResidentGameplay_requestExitToMenu() ? 1U : 0U;
                if (!lastOperationOk) {
                    if (!paintSaveOverlay()) return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
                    return ESP_NATIVE_GAMEPLAY_HUB_IGNORED;
                }
                printf("[SYS] EXIT-CONFIRMED queued=yes saveWrite=no boundary=after-session\n");
                /* Do not return CLOSED: it would repaint the old gameplay.
                 * The loop performs teardown only after all wrappers return. */
                return ESP_NATIVE_GAMEPLAY_HUB_OK;
            }

            if (statusCursor == kStatusSave) {
                lastOperation = 1U;
                lastOperationOk = saveNow() ? 1U : 0U;
                if (!lastOperationOk) {
                    if (!paintSaveOverlay()) {
                        return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
                    }
                    return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
                }

                /* A successful confirmation is terminal for the HUB: restore
                 * its HUD underlay now and let resident gameplay redraw the
                 * settled world. The action-engine feedback lease is painted
                 * by that redraw and, on expiry, recomposes the current
                 * FORCE_MESSAGE/facing-label fallback instead of restoring a
                 * stale pre-HUB framebuffer snapshot. */
                status = __real_EspNativeGameplayHub_handleAction(
                    ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN);
                if (status != ESP_NATIVE_GAMEPLAY_HUB_CLOSED) return status;
                if (!EspNativeGameplayActionEngine_queueTextFeedback(
                        ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_STATUS_TEXT,
                        "Game saved", 0U)) {
                    printf("[NATIVESAVE] SAVE-FEEDBACK-DEFER text=\"Game saved\" reason=feedback-busy checkpoint=committed hub=closed\n");
                }
                else {
                    printf("[NATIVESAVE] SAVE-CLOSE result=success hub=closed feedback=\"Game saved\" duration=action-default fallback=status-then-facing turn=no\n");
                }
                slotMode = 0U;
                statusCursor = kStatusSave;
                lastOperation = 0U;
                lastOperationOk = 0U;
                return ESP_NATIVE_GAMEPLAY_HUB_CLOSED;
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
            slotMode = 0U;
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

    if (slotMode != 0U && action == ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN) {
        slotMode = 0U;
        slotArmed = 0U;
        if (!paintSaveOverlay()) return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
        return ESP_NATIVE_GAMEPLAY_HUB_REDRAWN;
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
                printf("[NATIVESAVE] UI page=system rows=SAVE/LOAD/EXIT path=%s confirmation=double-select saveVersion=11 worldScope=resources+script+lines+action-removals+crate-transforms+automap+monster-state+topology+position+activation+monster-drops-v11 legacyV1..V10=read-compatible exitSave=no\n",
                       kLogPath);
            }
            if ((status == ESP_NATIVE_GAMEPLAY_HUB_REDRAWN ||
                 status == ESP_NATIVE_GAMEPLAY_HUB_OK) &&
                !paintSaveOverlay()) {
                return ESP_NATIVE_GAMEPLAY_HUB_IO_FAILED;
            }
        }
        else if (after == nullptr || after->active == 0U ||
                 after->page != ESP_NATIVE_GAMEPLAY_HUB_PAGE_SYSTEM) {
            slotMode = 0U;
            statusCursor = kStatusSave;
            lastOperation = 0U;
            confirmationTarget = kNoConfirmation;
        }
    }
    return status;
}
