#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_entity_def_type_catalog.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_crate_state.h"
#include "esp_player_view_state.h"

#define CRATE_ENTITY_TYPE 12U
#define CRATE_ENTITY_SUBTYPE 2U
#define CRATE_RESOURCE_WORLD 3U
#define CRATE_RESOURCE_INVENTORY 4U
#define CRATE_RESOURCE_AMMO 6U

#define CRATE_DEF_MASK 511U
#define CRATE_DEF_TILE_FLAG 0x00040000UL
#define CRATE_DEF_TILE_BASE 305U
#define CRATE_SPRITE_INFO_DEF_CLEAR 0xfffffe00UL
#define CRATE_MAX_SPRITES 1024U

typedef struct CrateTransformRecord_s {
    uint16_t spriteIndex;
    uint16_t effectiveDefTile;
} CrateTransformRecord;

typedef struct CrateStateOwner_s {
    EspNativeGameplayCrateStateView view;
    CrateTransformRecord records[ESP_NATIVE_GAMEPLAY_CRATE_MAX_TRANSFORMS];
    uint8_t probeDone;
} CrateStateOwner;

/* Keep the owner out of startup .bss. Classic CYD mappings.bin inflation needs
 * the hardware-proven main DRAM boundary intact. This bounded owner exists only
 * while a native map/player context exists and is freed on map reset. */
static CrateStateOwner* crateState;

int __real_EspMapRuntime_getMapSprite(uint32_t index, EspMapSprite* outSprite);
int __real_EspMapSpriteTopology_getEntity(uint32_t spriteIndex,
                                          uint8_t* outType,
                                          uint8_t* outSubType,
                                          uint16_t* outLinkState,
                                          uint16_t* outLinkOrder);

static int rawDefinition(uint32_t spriteIndex,
                         uint16_t* outDefTile,
                         uint8_t* outType,
                         uint8_t* outSubtype,
                         int32_t* outParm) {
    EspMapSprite sprite;
    uint32_t lookup;
    if (!__real_EspMapRuntime_getMapSprite(spriteIndex, &sprite)) return 0;
    lookup = sprite.info & CRATE_DEF_MASK;
    if ((sprite.info & CRATE_DEF_TILE_FLAG) != 0U) {
        lookup += CRATE_DEF_TILE_BASE;
    }
    if (lookup >= ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT) return 0;
    if (!EspEntityDefTypeCatalog_getMetadata((uint16_t)lookup,
                                             outType, outSubtype, outParm)) {
        return 0;
    }
    if (outDefTile != NULL) *outDefTile = (uint16_t)lookup;
    return 1;
}

static int allowedDrop(uint8_t type, uint8_t subtype) {
    if (type == CRATE_RESOURCE_WORLD) {
        return subtype == 21U || subtype == 22U || subtype == 23U;
    }
    if (type == CRATE_RESOURCE_INVENTORY) return subtype == 25U;
    if (type == CRATE_RESOURCE_AMMO) return subtype <= 4U;
    return 0;
}

static int targetTile(uint8_t type, uint8_t subtype, uint16_t* outTile) {
    uint16_t tile = 0U;
    uint8_t metaType = 0U;
    uint8_t metaSubtype = 0U;
    int32_t parm = 0;
    if (outTile == NULL ||
        !EspEntityDefTypeCatalog_findTileIndex(type, subtype, &tile) ||
        tile > CRATE_DEF_MASK ||
        !EspEntityDefTypeCatalog_getMetadata(tile, &metaType, &metaSubtype, &parm) ||
        metaType != type || metaSubtype != subtype || !allowedDrop(type, subtype)) {
        return 0;
    }
    *outTile = tile;
    return 1;
}

static int targetsReady(uint16_t outTiles[9]) {
    uint8_t i;
    if (outTiles == NULL) return 0;
    if (!targetTile(3U, 23U, &outTiles[0]) ||
        !targetTile(3U, 22U, &outTiles[1]) ||
        !targetTile(4U, 25U, &outTiles[2]) ||
        !targetTile(3U, 21U, &outTiles[3])) {
        return 0;
    }
    for (i = 0U; i < 5U; ++i) {
        if (!targetTile(6U, i, &outTiles[4U + i])) return 0;
    }
    return 1;
}

static int outcomeProbe(void) {
    static const struct {
        uint8_t first;
        uint8_t second;
        uint8_t secondValid;
        uint8_t expectedOutcome;
        uint8_t expectedType;
        uint8_t expectedSubtype;
    } cases[] = {
        {0U,   0U,   0U, ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRAPPED_REMOVE, 0U, 0U},
        {1U,   0U,   0U, ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRAPPED_REMOVE, 0U, 0U},
        {2U,   0U,   0U, ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM, 3U, 23U},
        {3U,   0U,   0U, ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM, 3U, 23U},
        {4U,   0U,   0U, ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM, 3U, 22U},
        {11U,  0U,   0U, ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM, 3U, 22U},
        {12U,  0U,   0U, ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM, 4U, 25U},
        {23U,  0U,   0U, ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM, 4U, 25U},
        {24U,  0U,   0U, ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM, 3U, 21U},
        {149U, 0U,   0U, ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM, 3U, 21U},
        {150U, 0U,   1U, ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM, 6U, 0U},
        {150U, 4U,   1U, ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM, 6U, 4U},
        {212U, 255U, 1U, ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM, 6U, 0U},
        {213U, 0U,   0U, ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_BREAK_REMOVE, 0U, 0U},
        {255U, 0U,   0U, ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_BREAK_REMOVE, 0U, 0U}
    };
    uint32_t i;
    for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        EspNativeGameplayCrateOutcome outcome =
            ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_INVALID;
        uint16_t tile = 0U;
        uint8_t type = 0U;
        uint8_t subtype = 0U;
        int32_t parm = 0;
        if (!EspNativeGameplayCrateState_resolveOutcome(
                cases[i].first, cases[i].second, cases[i].secondValid,
                &outcome, &tile) ||
            outcome != (EspNativeGameplayCrateOutcome)cases[i].expectedOutcome) {
            return 0;
        }
        if (outcome == ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM) {
            if (!EspEntityDefTypeCatalog_getMetadata(
                    tile, &type, &subtype, &parm) ||
                type != cases[i].expectedType ||
                subtype != cases[i].expectedSubtype) {
                return 0;
            }
        }
    }
    printf("[CRATEPROBE] READY cases=%u thresholds=0/2/4/12/24/150/213 ammoSecond=150..212 modulo5=yes rngConsumed=0 mutation=no\n",
           (unsigned int)(sizeof(cases) / sizeof(cases[0])));
    return 1;
}

static uint32_t crateSnapshotFNV(
    const EspNativeGameplayCrateTransformSnapshot* snapshot) {
    uint32_t hash = 2166136261U;
    uint32_t i;
    uint32_t codeBytes;
    if (snapshot == NULL) return 0U;
    for (i = 0U; i < snapshot->transformedBytes; ++i) {
        hash ^= snapshot->transformedBits[i];
        hash *= 16777619U;
    }
    codeBytes = (snapshot->transformedCount + 1U) >> 1U;
    for (i = 0U; i < codeBytes; ++i) {
        hash ^= snapshot->defCodes[i];
        hash *= 16777619U;
    }
    return hash;
}

static int targetCodeForTile(uint16_t tile, uint8_t* outCode) {
    uint16_t targets[9];
    uint8_t i;
    if (outCode == NULL || !targetsReady(targets)) return 0;
    for (i = 0U; i < 9U; ++i) {
        if (targets[i] == tile) {
            *outCode = (uint8_t)(i + 1U);
            return 1;
        }
    }
    return 0;
}

static int targetTileForCode(uint8_t code, uint16_t* outTile) {
    uint16_t targets[9];
    if (outTile == NULL || code < 1U || code > 9U ||
        !targetsReady(targets)) {
        return 0;
    }
    *outTile = targets[code - 1U];
    return 1;
}

static int findRecord(uint32_t spriteIndex) {
    uint16_t i;
    if (crateState == NULL) return -1;
    for (i = 0U; i < crateState->view.transformedCount; ++i) {
        if (crateState->records[i].spriteIndex == spriteIndex) return (int)i;
    }
    return -1;
}

void EspNativeGameplayCrateState_reset(void) {
    if (crateState != NULL) {
        free(crateState);
        crateState = NULL;
    }
}

int EspNativeGameplayCrateState_ensure(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspPlayerViewState* playerView = EspPlayerView_view();
    uint16_t targetTiles[9];
    uint32_t i;
    uint16_t crates = 0U;

    if (runtime == NULL || runtime->arenaFNV1a == 0U ||
        runtime->mapSpriteCount == 0U ||
        runtime->mapSpriteCount > CRATE_MAX_SPRITES ||
        playerView == NULL || playerView->active != 1U ||
        playerView->targetMapId == 0U ||
        !EspMapSpriteTopology_isReady() ||
        !EspEntityDefTypeCatalog_isReady()) {
        return 0;
    }
    if (crateState != NULL &&
        crateState->view.active != 0U &&
        crateState->view.sourceArenaFNV1a == runtime->arenaFNV1a &&
        crateState->view.spriteCount == runtime->mapSpriteCount &&
        crateState->view.targetMapId == playerView->targetMapId) {
        return crateState->view.fatal == 0U;
    }

    EspNativeGameplayCrateState_reset();
    crateState = (CrateStateOwner*)calloc(1U, sizeof(*crateState));
    if (crateState == NULL) {
        printf("[CRATESTATE] OOM ownerBytes=%u allocation=map-lazy failClosed=yes\n",
               (unsigned int)sizeof(CrateStateOwner));
        return 0;
    }
    crateState->view.sourceArenaFNV1a = runtime->arenaFNV1a;
    crateState->view.spriteCount = (uint16_t)runtime->mapSpriteCount;
    crateState->view.targetMapId = playerView->targetMapId;
    crateState->view.active = 1U;

    for (i = 0U; i < runtime->mapSpriteCount; ++i) {
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        if (!__real_EspMapSpriteTopology_getEntity(
                i, &type, &subtype, &linkState, &linkOrder)) {
            crateState->view.fatal = 1U;
            break;
        }
        (void)linkOrder;
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) != 0U &&
            type == CRATE_ENTITY_TYPE && subtype == CRATE_ENTITY_SUBTYPE) {
            EspMapSprite sprite;
            uint16_t defTile = 0U;
            int32_t parm = 0;
            uint8_t defType = 0U;
            uint8_t defSubtype = 0U;
            ++crates;
            if (!__real_EspMapRuntime_getMapSprite(i, &sprite) ||
                !rawDefinition(i, &defTile, &defType, &defSubtype, &parm) ||
                defType != CRATE_ENTITY_TYPE ||
                defSubtype != CRATE_ENTITY_SUBTYPE) {
                crateState->view.fatal = 1U;
                break;
            }
            printf("[CRATESTATE] WITNESS sprite=%u tile=%u pos=%d,%d defTile=%u parm=%08x weaponMask=%08x linked=%u order=%u\n",
                   (unsigned int)i,
                   (unsigned int)(linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK),
                   (int)sprite.x,
                   (int)sprite.y,
                   (unsigned int)defTile,
                   (unsigned int)parm,
                   (unsigned int)parm,
                   (unsigned int)((linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) != 0U),
                   (unsigned int)linkOrder);
        }
    }
    crateState->view.crateCount = crates;
    if (crates > ESP_NATIVE_GAMEPLAY_CRATE_MAX_TRANSFORMS ||
        !targetsReady(targetTiles)) {
        crateState->view.fatal = 1U;
    }

    printf("[CRATESTATE] READY arena=%08x sprites=%u crates=%u capacity=%u ownerBytes=%u targets=%s tiles=%u/%u/%u/%u ammo=%u/%u/%u/%u/%u persistence=deferred immutableBsp=yes allocation=map-lazy fatal=%u\n",
           (unsigned int)crateState->view.sourceArenaFNV1a,
           (unsigned int)crateState->view.spriteCount,
           (unsigned int)crateState->view.crateCount,
           (unsigned int)ESP_NATIVE_GAMEPLAY_CRATE_MAX_TRANSFORMS,
           (unsigned int)sizeof(*crateState),
           crateState->view.fatal == 0U ? "ready" : "NOT_READY",
           crateState->view.fatal == 0U ? (unsigned int)targetTiles[0] : 0U,
           crateState->view.fatal == 0U ? (unsigned int)targetTiles[1] : 0U,
           crateState->view.fatal == 0U ? (unsigned int)targetTiles[2] : 0U,
           crateState->view.fatal == 0U ? (unsigned int)targetTiles[3] : 0U,
           crateState->view.fatal == 0U ? (unsigned int)targetTiles[4] : 0U,
           crateState->view.fatal == 0U ? (unsigned int)targetTiles[5] : 0U,
           crateState->view.fatal == 0U ? (unsigned int)targetTiles[6] : 0U,
           crateState->view.fatal == 0U ? (unsigned int)targetTiles[7] : 0U,
           crateState->view.fatal == 0U ? (unsigned int)targetTiles[8] : 0U,
           (unsigned int)crateState->view.fatal);
    if (crateState->view.fatal == 0U && crateState->probeDone == 0U) {
        crateState->probeDone = 1U;
        if (!outcomeProbe()) {
            crateState->view.fatal = 1U;
            printf("[CRATEPROBE] FAILED thresholds-or-target-mapping mutation=no rngConsumed=0 failClosed=yes\n");
            return 0;
        }
    }
    return crateState->view.fatal == 0U;
}

const EspNativeGameplayCrateStateView* EspNativeGameplayCrateState_view(void) {
    return EspNativeGameplayCrateState_ensure() && crateState != NULL
               ? &crateState->view : NULL;
}

int EspNativeGameplayCrateState_snapshotShapeValid(
    const EspNativeGameplayCrateTransformSnapshot* snapshot,
    uint32_t expectedArenaFNV1a,
    uint8_t expectedTargetMapId) {
    uint32_t expectedBytes;
    uint32_t codeBytes;
    uint32_t validTailBits;
    uint8_t validTailMask;
    uint32_t setBits = 0U;
    uint32_t i;
    uint16_t ordinal = 0U;

    if (snapshot == NULL || expectedArenaFNV1a == 0U ||
        expectedTargetMapId == 0U ||
        snapshot->sourceArenaFNV1a != expectedArenaFNV1a ||
        snapshot->targetMapId != expectedTargetMapId ||
        snapshot->reserved0 != 0U ||
        snapshot->spriteCount == 0U ||
        snapshot->spriteCount >
            ESP_NATIVE_GAMEPLAY_CRATE_SNAPSHOT_MAX_SPRITE_BYTES * 8U ||
        snapshot->transformedCount > ESP_NATIVE_GAMEPLAY_CRATE_MAX_TRANSFORMS ||
        snapshot->transformedCount > snapshot->spriteCount) {
        return 0;
    }

    expectedBytes = (snapshot->spriteCount + 7U) >> 3U;
    if (expectedBytes == 0U ||
        expectedBytes > ESP_NATIVE_GAMEPLAY_CRATE_SNAPSHOT_MAX_SPRITE_BYTES ||
        snapshot->transformedBytes != expectedBytes) {
        return 0;
    }

    for (i = 0U; i < expectedBytes; ++i) {
        uint8_t value = snapshot->transformedBits[i];
        while (value != 0U) {
            setBits += (uint32_t)(value & 1U);
            value >>= 1U;
        }
    }
    if (setBits != snapshot->transformedCount) return 0;

    validTailBits = snapshot->spriteCount & 7U;
    if (validTailBits != 0U) {
        validTailMask = (uint8_t)((1U << validTailBits) - 1U);
        if ((snapshot->transformedBits[expectedBytes - 1U] &
             (uint8_t)~validTailMask) != 0U) {
            return 0;
        }
    }
    for (i = expectedBytes;
         i < ESP_NATIVE_GAMEPLAY_CRATE_SNAPSHOT_MAX_SPRITE_BYTES; ++i) {
        if (snapshot->transformedBits[i] != 0U) return 0;
    }

    codeBytes = (snapshot->transformedCount + 1U) >> 1U;
    if (codeBytes > ESP_NATIVE_GAMEPLAY_CRATE_SNAPSHOT_CODE_BYTES) return 0;
    for (i = 0U; i < snapshot->spriteCount; ++i) {
        uint8_t code;
        uint16_t tile;
        if ((snapshot->transformedBits[i >> 3U] &
             (uint8_t)(1U << (i & 7U))) == 0U) {
            continue;
        }
        code = (uint8_t)((ordinal & 1U) == 0U
                             ? (snapshot->defCodes[ordinal >> 1U] & 0x0fU)
                             : ((snapshot->defCodes[ordinal >> 1U] >> 4U) &
                                0x0fU));
        if (!targetTileForCode(code, &tile)) return 0;
        ++ordinal;
    }
    if (ordinal != snapshot->transformedCount) return 0;
    if ((snapshot->transformedCount & 1U) != 0U &&
        (snapshot->defCodes[codeBytes - 1U] & 0xf0U) != 0U) {
        return 0;
    }
    for (i = codeBytes; i < ESP_NATIVE_GAMEPLAY_CRATE_SNAPSHOT_CODE_BYTES; ++i) {
        if (snapshot->defCodes[i] != 0U) return 0;
    }
    return snapshot->stateFNV1a == crateSnapshotFNV(snapshot);
}

int EspNativeGameplayCrateState_snapshot(
    EspNativeGameplayCrateTransformSnapshot* outSnapshot) {
    uint32_t i;
    uint16_t ordinal = 0U;
    if (outSnapshot == NULL || !EspNativeGameplayCrateState_ensure() ||
        crateState == NULL) {
        return 0;
    }
    memset(outSnapshot, 0, sizeof(*outSnapshot));
    outSnapshot->sourceArenaFNV1a = crateState->view.sourceArenaFNV1a;
    outSnapshot->spriteCount = crateState->view.spriteCount;
    outSnapshot->transformedCount = crateState->view.transformedCount;
    outSnapshot->transformedBytes =
        (uint16_t)((outSnapshot->spriteCount + 7U) >> 3U);
    outSnapshot->targetMapId = crateState->view.targetMapId;

    for (i = 0U; i < outSnapshot->spriteCount; ++i) {
        int found = findRecord(i);
        uint8_t code;
        if (found < 0) continue;
        if (!targetCodeForTile(crateState->records[found].effectiveDefTile,
                               &code)) {
            memset(outSnapshot, 0, sizeof(*outSnapshot));
            return 0;
        }
        outSnapshot->transformedBits[i >> 3U] |=
            (uint8_t)(1U << (i & 7U));
        if ((ordinal & 1U) == 0U) {
            outSnapshot->defCodes[ordinal >> 1U] = code;
        }
        else {
            outSnapshot->defCodes[ordinal >> 1U] |= (uint8_t)(code << 4U);
        }
        ++ordinal;
    }
    if (ordinal != outSnapshot->transformedCount) {
        memset(outSnapshot, 0, sizeof(*outSnapshot));
        return 0;
    }
    outSnapshot->stateFNV1a = crateSnapshotFNV(outSnapshot);
    return EspNativeGameplayCrateState_snapshotShapeValid(
        outSnapshot, outSnapshot->sourceArenaFNV1a,
        outSnapshot->targetMapId);
}

int EspNativeGameplayCrateState_restore(
    const EspNativeGameplayCrateTransformSnapshot* snapshot) {
    uint32_t i;
    uint16_t ordinal = 0U;
    uint16_t restoreCount = 0U;

    if (!EspNativeGameplayCrateState_ensure() || crateState == NULL ||
        snapshot == NULL ||
        !EspNativeGameplayCrateState_snapshotShapeValid(
            snapshot, crateState->view.sourceArenaFNV1a,
            crateState->view.targetMapId) ||
        snapshot->spriteCount != crateState->view.spriteCount ||
        snapshot->transformedCount > crateState->view.crateCount) {
        return 0;
    }

    /* Validate every source crate and target code before mutating the owner. */
    for (i = 0U; i < snapshot->spriteCount; ++i) {
        uint8_t sourceType;
        uint8_t sourceSubtype;
        int32_t sourceParm;
        uint8_t code;
        uint16_t targetTileValue;
        if ((snapshot->transformedBits[i >> 3U] &
             (uint8_t)(1U << (i & 7U))) == 0U) {
            continue;
        }
        code = (uint8_t)((ordinal & 1U) == 0U
                             ? (snapshot->defCodes[ordinal >> 1U] & 0x0fU)
                             : ((snapshot->defCodes[ordinal >> 1U] >> 4U) &
                                0x0fU));
        if (!rawDefinition(i, NULL, &sourceType, &sourceSubtype, &sourceParm) ||
            sourceType != CRATE_ENTITY_TYPE ||
            sourceSubtype != CRATE_ENTITY_SUBTYPE ||
            !targetTileForCode(code, &targetTileValue)) {
            return 0;
        }
        (void)sourceParm;
        (void)targetTileValue;
        ++ordinal;
    }
    if (ordinal != snapshot->transformedCount) return 0;

    memset(crateState->records, 0, sizeof(crateState->records));
    crateState->view.transformedCount = 0U;
    ordinal = 0U;
    for (i = 0U; i < snapshot->spriteCount; ++i) {
        uint8_t code;
        uint16_t targetTileValue;
        if ((snapshot->transformedBits[i >> 3U] &
             (uint8_t)(1U << (i & 7U))) == 0U) {
            continue;
        }
        code = (uint8_t)((ordinal & 1U) == 0U
                             ? (snapshot->defCodes[ordinal >> 1U] & 0x0fU)
                             : ((snapshot->defCodes[ordinal >> 1U] >> 4U) &
                                0x0fU));
        if (!targetTileForCode(code, &targetTileValue)) return 0;
        crateState->records[restoreCount].spriteIndex = (uint16_t)i;
        crateState->records[restoreCount].effectiveDefTile = targetTileValue;
        ++restoreCount;
        ++ordinal;
    }
    crateState->view.transformedCount = restoreCount;
    if (restoreCount != snapshot->transformedCount ||
        EspNativeGameplayCrateState_fingerprint() != snapshot->stateFNV1a) {
        return 0;
    }
    printf("[CRATECHECKPOINT] RESTORE arena=%08x map=%u sprites=%u transformed=%u bytes=%u codeBytes=%u stateFNV=%08x mutation=transform-overlay-only allocation=existing-owner\n",
           (unsigned int)snapshot->sourceArenaFNV1a,
           (unsigned int)snapshot->targetMapId,
           (unsigned int)snapshot->spriteCount,
           (unsigned int)snapshot->transformedCount,
           (unsigned int)snapshot->transformedBytes,
           (unsigned int)((snapshot->transformedCount + 1U) >> 1U),
           (unsigned int)snapshot->stateFNV1a);
    return 1;
}

uint32_t EspNativeGameplayCrateState_fingerprint(void) {
    EspNativeGameplayCrateTransformSnapshot snapshot;
    if (!EspNativeGameplayCrateState_snapshot(&snapshot)) return 0U;
    return snapshot.stateFNV1a;
}

int EspNativeGameplayCrateState_isTransformed(uint32_t spriteIndex) {
    if (!EspNativeGameplayCrateState_ensure() ||
        spriteIndex >= crateState->view.spriteCount) return 0;
    return findRecord(spriteIndex) >= 0;
}

int EspNativeGameplayCrateState_effectiveDefTile(uint32_t spriteIndex,
                                                 uint16_t* outDefTile) {
    int found;
    if (outDefTile == NULL || !EspNativeGameplayCrateState_ensure() ||
        spriteIndex >= crateState->view.spriteCount) return 0;
    found = findRecord(spriteIndex);
    if (found < 0) return 0;
    *outDefTile = crateState->records[found].effectiveDefTile;
    return 1;
}

int EspNativeGameplayCrateState_transform(uint16_t spriteIndex,
                                          uint16_t effectiveDefTile) {
    uint8_t sourceType;
    uint8_t sourceSubtype;
    uint8_t targetType;
    uint8_t targetSubtype;
    int32_t sourceParm;
    int32_t targetParm;
    uint16_t sourceTile;
    int found;

    if (!EspNativeGameplayCrateState_ensure() ||
        spriteIndex >= crateState->view.spriteCount ||
        effectiveDefTile > CRATE_DEF_MASK ||
        !rawDefinition(spriteIndex, &sourceTile, &sourceType, &sourceSubtype,
                       &sourceParm) ||
        sourceType != CRATE_ENTITY_TYPE || sourceSubtype != CRATE_ENTITY_SUBTYPE ||
        !EspEntityDefTypeCatalog_getMetadata(effectiveDefTile,
                                             &targetType, &targetSubtype,
                                             &targetParm) ||
        !allowedDrop(targetType, targetSubtype)) {
        return 0;
    }
    (void)sourceTile;
    (void)sourceParm;
    (void)targetParm;
    found = findRecord(spriteIndex);
    if (found >= 0) {
        return crateState->records[found].effectiveDefTile == effectiveDefTile;
    }
    if (crateState->view.transformedCount >= ESP_NATIVE_GAMEPLAY_CRATE_MAX_TRANSFORMS) {
        return 0;
    }
    crateState->records[crateState->view.transformedCount].spriteIndex = spriteIndex;
    crateState->records[crateState->view.transformedCount].effectiveDefTile =
        effectiveDefTile;
    ++crateState->view.transformedCount;
    return 1;
}

int EspNativeGameplayCrateState_rollbackTransform(uint16_t spriteIndex,
                                                  uint16_t effectiveDefTile) {
    int found;
    uint16_t last;
    if (!EspNativeGameplayCrateState_ensure()) return 0;
    found = findRecord(spriteIndex);
    if (found < 0 ||
        crateState->records[found].effectiveDefTile != effectiveDefTile ||
        crateState->view.transformedCount == 0U) {
        return 0;
    }
    last = (uint16_t)(crateState->view.transformedCount - 1U);
    if ((uint16_t)found != last) crateState->records[found] = crateState->records[last];
    memset(&crateState->records[last], 0, sizeof(crateState->records[last]));
    --crateState->view.transformedCount;
    return 1;
}

int EspNativeGameplayCrateState_applyMapSprite(uint32_t spriteIndex,
                                               EspMapSprite* ioSprite) {
    uint16_t defTile;
    if (ioSprite == NULL) return 0;
    if (!EspNativeGameplayCrateState_effectiveDefTile(spriteIndex, &defTile)) {
        return 1;
    }
    ioSprite->info = (ioSprite->info & CRATE_SPRITE_INFO_DEF_CLEAR) |
                     (uint32_t)defTile;
    return 1;
}

int EspNativeGameplayCrateState_applyEntity(uint32_t spriteIndex,
                                            uint8_t* ioType,
                                            uint8_t* ioSubtype,
                                            uint16_t* ioLinkState) {
    uint16_t defTile;
    uint8_t type;
    uint8_t subtype;
    int32_t parm;
    if (!EspNativeGameplayCrateState_effectiveDefTile(spriteIndex, &defTile)) {
        return 1;
    }
    if (!EspEntityDefTypeCatalog_getMetadata(defTile, &type, &subtype, &parm) ||
        !allowedDrop(type, subtype)) {
        return 0;
    }
    (void)parm;
    if (ioType != NULL) *ioType = type;
    if (ioSubtype != NULL) *ioSubtype = subtype;
    if (ioLinkState != NULL) {
        *ioLinkState &= (uint16_t)~ESP_MAP_SPRITE_TOPOLOGY_ALIVE;
    }
    return 1;
}

int EspNativeGameplayCrateState_requiresSecondRng(uint8_t first) {
    return first >= 150U && first < 213U;
}

int EspNativeGameplayCrateState_resolveOutcome(
    uint8_t first,
    uint8_t second,
    uint8_t secondValid,
    EspNativeGameplayCrateOutcome* outOutcome,
    uint16_t* outEffectiveDefTile) {
    uint8_t type = 0U;
    uint8_t subtype = 0U;
    uint16_t tile = 0U;

    if (outOutcome == NULL || outEffectiveDefTile == NULL ||
        !EspNativeGameplayCrateState_ensure()) return 0;
    *outOutcome = ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_INVALID;
    *outEffectiveDefTile = 0U;

    if (first < 2U) {
        if (secondValid != 0U) return 0;
        *outOutcome = ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRAPPED_REMOVE;
        return 1;
    }
    if (first < 4U) {
        type = 3U; subtype = 23U;
    }
    else if (first < 12U) {
        type = 3U; subtype = 22U;
    }
    else if (first < 24U) {
        type = 4U; subtype = 25U;
    }
    else if (first < 150U) {
        type = 3U; subtype = 21U;
    }
    else if (first < 213U) {
        if (secondValid == 0U) return 0;
        type = 6U;
        subtype = (uint8_t)(second % 5U);
    }
    else {
        if (secondValid != 0U) return 0;
        *outOutcome = ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_BREAK_REMOVE;
        return 1;
    }

    if (secondValid != 0U && !(first >= 150U && first < 213U)) return 0;
    if (!targetTile(type, subtype, &tile)) return 0;
    *outOutcome = ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_TRANSFORM;
    *outEffectiveDefTile = tile;
    return 1;
}
