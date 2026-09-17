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
#include "esp_map_resident_lifecycle.h"
#include "esp_map_runtime.h"
#include "esp_native_gameplay_combat_math.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_hub.h"
#include "esp_native_gameplay_input.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_gameplay_session.h"
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
constexpr uint8_t kMagic[8] = {'D', 'R', 'P', 'G', 'S', 'A', 'V', '1'};
constexpr uint16_t kVersion = 1U;
constexpr uint8_t kFamiliarAmmoType = 5U;
constexpr uint8_t kStatusSave = 0U;
constexpr uint8_t kStatusLoad = 1U;
constexpr uint8_t kStatusCount = 2U;
constexpr int kOverlayLeft = 91;
constexpr int kOverlayTop = 70;
constexpr int kOverlayRight = 158;
constexpr int kOverlayBottom = 98;

struct NativeSaveRecord {
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

static_assert(sizeof(NativeSaveRecord) <= 192U,
              "native save v1 must remain a tiny fixed record");

uint8_t statusCursor;
uint8_t lastOperation;
uint8_t lastOperationOk;

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

uint32_t recordCrc(const NativeSaveRecord& input) {
    NativeSaveRecord record = input;
    record.recordCrc32 = 0U;
    return crc32Bytes(reinterpret_cast<const uint8_t*>(&record), sizeof(record));
}

bool runtimeCoordinate(int32_t value) {
    return value >= 32 && value <= 2016 && (value & 63) == 32;
}

bool recordShapeValid(const NativeSaveRecord& record) {
    const EspPlayerViewState& view = record.view;
    if (memcmp(record.magic, kMagic, sizeof(kMagic)) != 0 ||
        record.version != kVersion ||
        record.recordBytes != sizeof(NativeSaveRecord) ||
        record.recordCrc32 == 0U || record.recordCrc32 != recordCrc(record) ||
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

bool readRecordPath(const char* path, NativeSaveRecord* outRecord) {
    File file;
    NativeSaveRecord record;
    size_t got;
    if (path == nullptr || outRecord == nullptr || !SD.exists(path)) return false;
    file = SD.open(path, FILE_READ);
    if (!file || file.size() != sizeof(record)) {
        if (file) file.close();
        return false;
    }
    memset(&record, 0, sizeof(record));
    got = file.read(reinterpret_cast<uint8_t*>(&record), sizeof(record));
    file.close();
    if (got != sizeof(record) || !recordShapeValid(record)) return false;
    *outRecord = record;
    return true;
}

bool readBestRecord(NativeSaveRecord* outRecord, bool* outRecoveredBackup) {
    if (outRecoveredBackup != nullptr) *outRecoveredBackup = false;
    if (readRecordPath(kSavePath, outRecord)) return true;
    if (!readRecordPath(kBackupPath, outRecord)) return false;
    if (outRecoveredBackup != nullptr) *outRecoveredBackup = true;
    return true;
}

bool writeExact(const char* path, const NativeSaveRecord& record) {
    File file = SD.open(path, FILE_WRITE);
    size_t wrote;
    if (!file) return false;
    wrote = file.write(reinterpret_cast<const uint8_t*>(&record), sizeof(record));
    file.flush();
    file.close();
    return wrote == sizeof(record);
}

bool commitRecordAtomic(const NativeSaveRecord& record) {
    NativeSaveRecord verify;
    bool movedOld = false;

    if (SD.exists(kTempPath)) (void)SD.remove(kTempPath);
    if (SD.exists(kBackupPath)) (void)SD.remove(kBackupPath);
    if (!writeExact(kTempPath, record) ||
        !readRecordPath(kTempPath, &verify) ||
        memcmp(&verify, &record, sizeof(record)) != 0) {
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

    if (SD.exists(kBackupPath)) (void)SD.remove(kBackupPath);
    memset(&verify, 0, sizeof(verify));
    return readRecordPath(kSavePath, &verify) &&
           memcmp(&verify, &record, sizeof(record)) == 0;
}

bool captureRecord(NativeSaveRecord* outRecord) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspPlayerViewState* view = EspPlayerView_view();
    NativeSaveRecord record;

    if (outRecord == nullptr || EspAssetPack_isOpen() ||
        runtime == nullptr || view == nullptr ||
        !EspMapResidentLifecycle_isReady()) {
        return false;
    }

    memset(&record, 0, sizeof(record));
    if (!EspNativeGameplayPlayerState_snapshot(&record.player)) return false;
    if (view->active != 1U || view->targetMapId == 0U ||
        runtime->sourceBytes == 0U || runtime->sourceCrc32 == 0U ||
        runtime->arenaFNV1a == 0U) {
        return false;
    }

    memcpy(record.magic, kMagic, sizeof(kMagic));
    record.version = kVersion;
    record.recordBytes = (uint16_t)sizeof(record);
    record.sourceBytes = runtime->sourceBytes;
    record.sourceCrc32 = runtime->sourceCrc32;
    record.runtimeFNV1a = runtime->arenaFNV1a;
    record.playerFNV1a = EspNativeGameplayPlayerState_fingerprint();
    record.targetMapId = view->targetMapId;
    record.gameplayLoadMapId = view->gameplayLoadMapId;
    record.loadType = view->loadType;
    record.view = *view;
    record.recordCrc32 = recordCrc(record);
    if (!recordShapeValid(record)) return false;
    *outRecord = record;
    return true;
}

bool saveNow(void) {
    NativeSaveRecord record;
    if (!captureRecord(&record) || !commitRecordAtomic(record)) {
        printf("[NATIVESAVE] SAVE-FAILED path=%s failClosed=yes\n", kLogPath);
        return false;
    }
    printf("[NATIVESAVE] SAVE path=%s version=%u bytes=%u map=%u gameplayLoadMapId=%u pos=%ld,%ld angle=%ld playerFNV=%08lx runtimeFNV=%08lx sourceBytes=%lu sourceCrc=%08lx recordCrc=%08lx atomic=temp+backup+rename world=fresh-rebuild\n",
           kLogPath,
           (unsigned int)record.version,
           (unsigned int)sizeof(record),
           (unsigned int)record.targetMapId,
           (unsigned int)record.gameplayLoadMapId,
           (long)record.view.viewX,
           (long)record.view.viewY,
           (long)record.view.viewAngle,
           (unsigned long)record.playerFNV1a,
           (unsigned long)record.runtimeFNV1a,
           (unsigned long)record.sourceBytes,
           (unsigned long)record.sourceCrc32,
           (unsigned long)record.recordCrc32);
    return true;
}

bool inventoryForRecord(const NativeSaveRecord& record,
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

bool restoreView(const NativeSaveRecord& record) {
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
    NativeSaveRecord record;
    EspBspInventory inventory;
    EspMapResidentSnapshot snapshot;
    EspNativeGameplaySessionConfig config;
    const EspMapRuntimeView* runtime;
    bool recoveredBackup = false;
    const char* name;
    EspMapResidentLifecycleStatus residentStatus;

    memset(&record, 0, sizeof(record));
    memset(&inventory, 0, sizeof(inventory));
    memset(&snapshot, 0, sizeof(snapshot));
    memset(&config, 0, sizeof(config));

    if (!readBestRecord(&record, &recoveredBackup)) {
        printf("[NATIVESAVE] LOAD-FAILED path=%s stage=READ reason=missing-or-invalid failClosed=yes\n",
               kLogPath);
        return false;
    }
    name = EspMapCatalog_nameForId(record.targetMapId);
    if (name == nullptr || name[0] == '\0') {
        printf("[NATIVESAVE] LOAD-FAILED path=%s stage=MAP_ID map=%u failClosed=yes\n",
               kLogPath, (unsigned int)record.targetMapId);
        return false;
    }

    /*
     * This first durable checkpoint deliberately rebuilds the saved BSP from
     * immutable PAK data before restoring pose/player state. Map-local mutable
     * overlays are therefore fresh in v1; the log says so explicitly.
     */
    EspNativeGameplaySession_reset();
    EspMapResidentLifecycle_resetAll();
    resetSpawnOwners();

    if (!inventoryForRecord(record, &inventory)) {
        printf("[NATIVESAVE] LOAD-FAILED path=%s stage=BSP_ID map=%u sourceBytes=%lu sourceCrc=%08lx failClosed=yes\n",
               kLogPath,
               (unsigned int)record.targetMapId,
               (unsigned long)record.sourceBytes,
               (unsigned long)record.sourceCrc32);
        return false;
    }

    residentStatus = EspMapResidentLifecycle_loadFromEmpty(
        name, &inventory, &snapshot);
    runtime = EspMapRuntime_view();
    if (residentStatus != ESP_MAP_RESIDENT_OK ||
        runtime == nullptr ||
        runtime->sourceBytes != record.sourceBytes ||
        runtime->sourceCrc32 != record.sourceCrc32 ||
        runtime->arenaFNV1a != record.runtimeFNV1a ||
        !EspMapResidentLifecycle_isReady()) {
        const unsigned long actualRuntime =
            runtime != nullptr ? (unsigned long)runtime->arenaFNV1a : 0UL;
        EspMapResidentLifecycle_resetAll();
        printf("[NATIVESAVE] LOAD-FAILED path=%s stage=RESIDENT status=%u map=%u expectedRuntime=%08lx actualRuntime=%08lx failClosed=yes\n",
               kLogPath,
               (unsigned int)residentStatus,
               (unsigned int)record.targetMapId,
               (unsigned long)record.runtimeFNV1a,
               actualRuntime);
        return false;
    }

    if (!EspNativeGameplayPlayerState_restore(&record.player) ||
        EspNativeGameplayPlayerState_fingerprint() != record.playerFNV1a ||
        !restoreView(record) ||
        !sessionConfigForPlayer(record.player, &config) ||
        !EspNativeGameplaySession_configure(&config)) {
        EspMapResidentLifecycle_resetAll();
        resetSpawnOwners();
        printf("[NATIVESAVE] LOAD-FAILED path=%s stage=RESTORE map=%u playerFNV=%08lx failClosed=yes\n",
               kLogPath,
               (unsigned int)record.targetMapId,
               (unsigned long)record.playerFNV1a);
        return false;
    }

    printf("[NATIVESAVE] LOAD path=%s version=%u bytes=%u map=%u gameplayLoadMapId=%u pos=%ld,%ld angle=%ld playerFNV=%08lx runtimeFNV=%08lx sourceBytes=%lu sourceCrc=%08lx backupRecovery=%s world=fresh-rebuild session=reprime-pending\n",
           kLogPath,
           (unsigned int)record.version,
           (unsigned int)sizeof(record),
           (unsigned int)record.targetMapId,
           (unsigned int)record.gameplayLoadMapId,
           (long)record.view.viewX,
           (long)record.view.viewY,
           (long)record.view.viewAngle,
           (unsigned long)record.playerFNV1a,
           (unsigned long)record.runtimeFNV1a,
           (unsigned long)record.sourceBytes,
           (unsigned long)record.sourceCrc32,
           recoveredBackup ? "yes" : "no");
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

            NativeSaveRecord probe;
            bool recoveredBackup = false;
            memset(&probe, 0, sizeof(probe));
            if (!readBestRecord(&probe, &recoveredBackup)) {
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
                printf("[NATIVESAVE] UI page=status rows=SAVE/LOAD slot=1 path=%s worldScope=fresh-rebuild\n",
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
