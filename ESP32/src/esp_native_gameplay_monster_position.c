#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_monster_position.h"
#include "esp_native_gameplay_monster_state.h"

#define POSITION_TILE_SIZE 64
#define POSITION_TILE_CENTER 32
#define POSITION_MAP_WIDTH 32U
#define POSITION_MAP_MAX_CENTER \
    (((POSITION_MAP_WIDTH - 1U) * POSITION_TILE_SIZE) + POSITION_TILE_CENTER)

static EspNativeGameplayMonsterPositionRecord* positionRecords;
static EspNativeGameplayMonsterPositionView positionView;
static EspNativeGameplayMonsterPositionSnapshot* pendingRestore;

_Static_assert(sizeof(EspNativeGameplayMonsterPositionRecord) == 8U,
               "native monster position record must remain 8 bytes");

static uint32_t fnvByte(uint32_t hash, uint8_t value) {
    hash ^= value;
    return hash * 16777619U;
}

static uint32_t fnv16(uint32_t hash, uint16_t value) {
    hash = fnvByte(hash, (uint8_t)(value & 0xffU));
    return fnvByte(hash, (uint8_t)((value >> 8) & 0xffU));
}

static uint32_t recordsFNV(void) {
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (positionRecords == NULL) return 0U;
    for (i = 0U; i < positionView.count; ++i) {
        const EspNativeGameplayMonsterPositionRecord* record = &positionRecords[i];
        hash = fnv16(hash, record->spriteIndex);
        hash = fnv16(hash, record->tileIndex);
        hash = fnv16(hash, record->worldX);
        hash = fnv16(hash, record->worldY);
    }
    return hash;
}

static uint32_t snapshotRecordsFNV(
    const EspNativeGameplayMonsterPositionSnapshot* snapshot) {
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (snapshot == NULL) return 0U;
    for (i = 0U; i < snapshot->count; ++i) {
        const EspNativeGameplayMonsterPositionRecord* record =
            &snapshot->records[i];
        hash = fnv16(hash, record->spriteIndex);
        hash = fnv16(hash, record->tileIndex);
        hash = fnv16(hash, record->worldX);
        hash = fnv16(hash, record->worldY);
    }
    return hash;
}

static int centeredCoordinate(int32_t value) {
    return value >= POSITION_TILE_CENTER &&
           value <= (int32_t)POSITION_MAP_MAX_CENTER &&
           (value & (POSITION_TILE_SIZE - 1)) == POSITION_TILE_CENTER;
}

static int tileIndexFor(int32_t x, int32_t y, uint16_t* outTile) {
    uint32_t tileX;
    uint32_t tileY;
    if (outTile == NULL || !centeredCoordinate(x) || !centeredCoordinate(y)) {
        return 0;
    }
    tileX = (uint32_t)x >> 6;
    tileY = (uint32_t)y >> 6;
    if (tileX >= POSITION_MAP_WIDTH || tileY >= POSITION_MAP_WIDTH) return 0;
    *outTile = (uint16_t)(tileY * POSITION_MAP_WIDTH + tileX);
    return 1;
}

static EspNativeGameplayMonsterPositionRecord* findMutable(uint16_t spriteIndex) {
    uint32_t i;
    if (positionRecords == NULL) return NULL;
    for (i = 0U; i < positionView.count; ++i) {
        if (positionRecords[i].spriteIndex == spriteIndex) return &positionRecords[i];
    }
    return NULL;
}

static void resetLivePosition(void) {
    if (positionRecords != NULL) heap_caps_free(positionRecords);
    positionRecords = NULL;
    memset(&positionView, 0, sizeof(positionView));
}

void EspNativeGameplayMonsterPosition_reset(void) {
    resetLivePosition();
    if (pendingRestore != NULL) heap_caps_free(pendingRestore);
    pendingRestore = NULL;
}

int EspNativeGameplayMonsterPosition_ensure(void) {
    const EspNativeGameplayMonsterView* monsters = EspNativeGameplayMonsterState_view();
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    uint32_t i;

    if (monsters == NULL || monsters->records == NULL || monsters->count == 0U ||
        monsters->count > ESP_NATIVE_GAMEPLAY_MONSTER_MAX_COUNT ||
        runtime == NULL || topology == NULL || !EspMapRuntime_isLoaded() ||
        !EspMapSpriteTopology_isReady() ||
        monsters->sourceArenaFNV1a == 0U ||
        monsters->sourceArenaFNV1a != runtime->arenaFNV1a ||
        runtime->mapSpriteCount != topology->spriteCount) {
        return 0;
    }

    if (positionView.active == 1U && positionRecords != NULL &&
        positionView.sourceArenaFNV1a == runtime->arenaFNV1a &&
        positionView.count == monsters->count) {
        return 1;
    }

    if (pendingRestore != NULL) {
        if (!EspNativeGameplayMonsterPosition_snapshotShapeValid(
                pendingRestore, runtime->arenaFNV1a) ||
            pendingRestore->count != monsters->count) {
            EspNativeGameplayMonsterPosition_reset();
            printf("[MONSTERPOS] RESTORE-FAILED reason=shape-or-count\n");
            return 0;
        }

        resetLivePosition();
        positionRecords =
            (EspNativeGameplayMonsterPositionRecord*)heap_caps_malloc(
                monsters->count * sizeof(*positionRecords), MALLOC_CAP_8BIT);
        if (positionRecords == NULL) {
            EspNativeGameplayMonsterPosition_reset();
            printf("[MONSTERPOS] RESTORE-FAILED reason=allocation\n");
            return 0;
        }
        memcpy(positionRecords, pendingRestore->records,
               monsters->count * sizeof(*positionRecords));
        positionView.records = positionRecords;
        positionView.count = monsters->count;
        positionView.ownerBytes =
            monsters->count * (uint32_t)sizeof(*positionRecords);
        positionView.sourceArenaFNV1a = runtime->arenaFNV1a;

        for (i = 0U; i < monsters->count; ++i) {
            const EspNativeGameplayMonsterPositionRecord* restored =
                &positionRecords[i];
            const EspNativeGameplayMonsterRecord* monster =
                &monsters->records[i];
            uint8_t type;
            uint8_t subtype;
            uint16_t linkState;
            uint16_t linkOrder;

            if (restored->spriteIndex != monster->spriteIndex ||
                restored->spriteIndex >= runtime->mapSpriteCount ||
                !EspMapSpriteTopology_getEntity(
                    restored->spriteIndex, &type, &subtype,
                    &linkState, &linkOrder) ||
                type != ESP_MAP_ENTITY_TYPE_ENEMY ||
                subtype != monster->subtype ||
                (linkState & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) == 0U ||
                ((linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) != 0U &&
                 restored->tileIndex !=
                     (uint16_t)(linkState &
                                ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK))) {
                EspNativeGameplayMonsterPosition_reset();
                printf("[MONSTERPOS] RESTORE-FAILED reason=topology-identity index=%u sprite=%u\n",
                       (unsigned int)i,
                       (unsigned int)restored->spriteIndex);
                return 0;
            }
            (void)linkOrder;
        }

        positionView.stateFNV1a = recordsFNV();
        positionView.active = 1U;
        if (positionView.stateFNV1a != pendingRestore->stateFNV1a) {
            EspNativeGameplayMonsterPosition_reset();
            printf("[MONSTERPOS] RESTORE-FAILED reason=fingerprint\n");
            return 0;
        }
        heap_caps_free(pendingRestore);
        pendingRestore = NULL;
        printf("[MONSTERPOS] RESTORE arena=%08x monsters=%u ownerBytes=%u stateFNV=%08x source=checkpoint-v9 topologyExact=yes\n",
               (unsigned int)positionView.sourceArenaFNV1a,
               (unsigned int)positionView.count,
               (unsigned int)positionView.ownerBytes,
               (unsigned int)positionView.stateFNV1a);
        return 1;
    }

    EspNativeGameplayMonsterPosition_reset();
    positionRecords = (EspNativeGameplayMonsterPositionRecord*)heap_caps_malloc(
        monsters->count * sizeof(*positionRecords), MALLOC_CAP_8BIT);
    if (positionRecords == NULL) return 0;
    memset(positionRecords, 0, monsters->count * sizeof(*positionRecords));

    positionView.records = positionRecords;
    positionView.count = monsters->count;
    positionView.ownerBytes =
        monsters->count * (uint32_t)sizeof(*positionRecords);
    positionView.sourceArenaFNV1a = runtime->arenaFNV1a;

    for (i = 0U; i < monsters->count; ++i) {
        const EspNativeGameplayMonsterRecord* monster = &monsters->records[i];
        EspNativeGameplayMonsterPositionRecord* position = &positionRecords[i];
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        uint16_t tile;
        uint16_t canonicalTile;
        uint32_t tileX;
        uint32_t tileY;
        int32_t worldX;
        int32_t worldY;
        EspMapSprite sourceSprite;
        uint8_t hiddenSource = 0U;

        /*
         * Linked entities already have a canonical mutable topology tile and
         * that remains the authority after native relinks.
         *
         * A legacy hidden enemy (sprite.info bit 0x00010000), however, starts
         * EXISTS+ALIVE but deliberately UNLINKED. Topology therefore carries no
         * tile bits until EV_SHOW executes. EV_SHOW links that target using the
         * immutable raw BSP sprite tile. Seed MonsterPosition from that same raw
         * tile while the entity is still hidden so the later SHOW does not make
         * MonsterPosition and topology disagree and fail-close every movement
         * turn for the revealed monster.
         */
        if (monster->spriteIndex >= runtime->mapSpriteCount ||
            !EspMapRuntime_getMapSprite(monster->spriteIndex, &sourceSprite) ||
            !EspMapSpriteTopology_getEntity(monster->spriteIndex,
                                            &type, &subtype,
                                            &linkState, &linkOrder) ||
            type != ESP_MAP_ENTITY_TYPE_ENEMY || subtype != monster->subtype ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) == 0U) {
            EspNativeGameplayMonsterPosition_reset();
            return 0;
        }
        (void)linkOrder;
        hiddenSource =
            (uint8_t)((sourceSprite.info & 0x00010000UL) != 0U);
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U &&
            hiddenSource != 0U) {
            tileX = (uint32_t)sourceSprite.x >> 6U;
            tileY = (uint32_t)sourceSprite.y >> 6U;
            if (tileX >= POSITION_MAP_WIDTH || tileY >= POSITION_MAP_WIDTH) {
                EspNativeGameplayMonsterPosition_reset();
                return 0;
            }
            tile = (uint16_t)(tileY * POSITION_MAP_WIDTH + tileX);
        }
        else {
            tile =
                (uint16_t)(linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK);
            tileX = (uint32_t)tile % POSITION_MAP_WIDTH;
            tileY = (uint32_t)tile / POSITION_MAP_WIDTH;
        }
        worldX = (int32_t)(tileX * POSITION_TILE_SIZE + POSITION_TILE_CENTER);
        worldY = (int32_t)(tileY * POSITION_TILE_SIZE + POSITION_TILE_CENTER);
        if (!tileIndexFor(worldX, worldY, &canonicalTile) || canonicalTile != tile) {
            EspNativeGameplayMonsterPosition_reset();
            return 0;
        }

        position->spriteIndex = monster->spriteIndex;
        position->tileIndex = tile;
        position->worldX = (uint16_t)worldX;
        position->worldY = (uint16_t)worldY;
    }

    positionView.stateFNV1a = recordsFNV();
    positionView.active = 1U;
    printf("[MONSTERPOS] READY arena=%08x monsters=%u recordBytes=%u ownerBytes=%u stateFNV=%08x source=topology-linked+hidden-show-raw-tile centered=yes mutable=probe-transaction rendererPublish=deferred topologyRelink=deferred allocation=load-only\n",
           (unsigned int)positionView.sourceArenaFNV1a,
           (unsigned int)positionView.count,
           (unsigned int)sizeof(EspNativeGameplayMonsterPositionRecord),
           (unsigned int)positionView.ownerBytes,
           (unsigned int)positionView.stateFNV1a);
    return 1;
}

const EspNativeGameplayMonsterPositionView* EspNativeGameplayMonsterPosition_view(void) {
    return EspNativeGameplayMonsterPosition_ensure() ? &positionView : NULL;
}

const EspNativeGameplayMonsterPositionRecord* EspNativeGameplayMonsterPosition_find(
    uint16_t spriteIndex) {
    if (!EspNativeGameplayMonsterPosition_ensure()) return NULL;
    return findMutable(spriteIndex);
}

int EspNativeGameplayMonsterPosition_snapshotShapeValid(
    const EspNativeGameplayMonsterPositionSnapshot* snapshot,
    uint32_t expectedArenaFNV1a) {
    uint32_t i;

    if (snapshot == NULL || expectedArenaFNV1a == 0U ||
        snapshot->sourceArenaFNV1a != expectedArenaFNV1a ||
        snapshot->stateFNV1a == 0U || snapshot->count == 0U ||
        snapshot->count > ESP_NATIVE_GAMEPLAY_MONSTER_MAX_COUNT ||
        snapshot->recordBytes !=
            sizeof(EspNativeGameplayMonsterPositionRecord) ||
        snapshotRecordsFNV(snapshot) != snapshot->stateFNV1a) {
        return 0;
    }

    for (i = 0U; i < snapshot->count; ++i) {
        const EspNativeGameplayMonsterPositionRecord* record =
            &snapshot->records[i];
        uint16_t tile;
        if ((i != 0U &&
             record->spriteIndex <= snapshot->records[i - 1U].spriteIndex) ||
            !tileIndexFor((int32_t)record->worldX,
                          (int32_t)record->worldY, &tile) ||
            tile != record->tileIndex) {
            return 0;
        }
    }

    for (i = snapshot->count;
         i < ESP_NATIVE_GAMEPLAY_MONSTER_MAX_COUNT; ++i) {
        const uint8_t* bytes = (const uint8_t*)&snapshot->records[i];
        uint32_t j;
        for (j = 0U; j < sizeof(snapshot->records[i]); ++j) {
            if (bytes[j] != 0U) return 0;
        }
    }
    return 1;
}

int EspNativeGameplayMonsterPosition_snapshot(
    EspNativeGameplayMonsterPositionSnapshot* outSnapshot) {
    if (outSnapshot == NULL ||
        !EspNativeGameplayMonsterPosition_ensure() ||
        positionView.count == 0U ||
        positionView.count > ESP_NATIVE_GAMEPLAY_MONSTER_MAX_COUNT) {
        return 0;
    }
    memset(outSnapshot, 0, sizeof(*outSnapshot));
    outSnapshot->sourceArenaFNV1a = positionView.sourceArenaFNV1a;
    outSnapshot->count = (uint16_t)positionView.count;
    outSnapshot->recordBytes =
        (uint16_t)sizeof(EspNativeGameplayMonsterPositionRecord);
    memcpy(outSnapshot->records, positionRecords,
           positionView.count * sizeof(*positionRecords));
    outSnapshot->stateFNV1a = snapshotRecordsFNV(outSnapshot);
    return EspNativeGameplayMonsterPosition_snapshotShapeValid(
        outSnapshot, positionView.sourceArenaFNV1a);
}

int EspNativeGameplayMonsterPosition_stageRestore(
    const EspNativeGameplayMonsterPositionSnapshot* snapshot) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    EspNativeGameplayMonsterPositionSnapshot* staged;

    if (runtime == NULL || !EspMapRuntime_isLoaded() ||
        positionRecords != NULL || pendingRestore != NULL ||
        !EspNativeGameplayMonsterPosition_snapshotShapeValid(
            snapshot, runtime->arenaFNV1a)) {
        return 0;
    }

    staged = (EspNativeGameplayMonsterPositionSnapshot*)heap_caps_malloc(
        sizeof(*staged), MALLOC_CAP_8BIT);
    if (staged == NULL) return 0;
    memcpy(staged, snapshot, sizeof(*staged));
    pendingRestore = staged;
    printf("[MONSTERPOS] STAGE-RESTORE arena=%08x monsters=%u bytes=%u stateFNV=%08x allocation=transient-until-ensure\n",
           (unsigned int)staged->sourceArenaFNV1a,
           (unsigned int)staged->count,
           (unsigned int)sizeof(*staged),
           (unsigned int)staged->stateFNV1a);
    return 1;
}

int EspNativeGameplayMonsterPosition_prepareCardinalMove(
    uint16_t spriteIndex,
    int32_t deltaX,
    int32_t deltaY,
    EspNativeGameplayMonsterPositionRecord* outBefore,
    EspNativeGameplayMonsterPositionRecord* outAfter) {
    EspNativeGameplayMonsterPositionRecord* live;
    int32_t nextX;
    int32_t nextY;
    uint16_t nextTile;

    if (outBefore != NULL) memset(outBefore, 0, sizeof(*outBefore));
    if (outAfter != NULL) memset(outAfter, 0, sizeof(*outAfter));
    if (outBefore == NULL || outAfter == NULL ||
        !EspNativeGameplayMonsterPosition_ensure() ||
        !((deltaX == POSITION_TILE_SIZE && deltaY == 0) ||
          (deltaX == -POSITION_TILE_SIZE && deltaY == 0) ||
          (deltaX == 0 && deltaY == POSITION_TILE_SIZE) ||
          (deltaX == 0 && deltaY == -POSITION_TILE_SIZE))) {
        return 0;
    }

    live = findMutable(spriteIndex);
    if (live == NULL) return 0;
    nextX = (int32_t)live->worldX + deltaX;
    nextY = (int32_t)live->worldY + deltaY;
    if (!tileIndexFor(nextX, nextY, &nextTile)) return 0;

    *outBefore = *live;
    *outAfter = *live;
    outAfter->worldX = (uint16_t)nextX;
    outAfter->worldY = (uint16_t)nextY;
    outAfter->tileIndex = nextTile;
    return 1;
}

int EspNativeGameplayMonsterPosition_commitPrepared(
    const EspNativeGameplayMonsterPositionRecord* expectedBefore,
    const EspNativeGameplayMonsterPositionRecord* preparedAfter) {
    EspNativeGameplayMonsterPositionRecord* live;
    uint16_t expectedTile;

    if (expectedBefore == NULL || preparedAfter == NULL ||
        expectedBefore->spriteIndex != preparedAfter->spriteIndex ||
        !tileIndexFor((int32_t)preparedAfter->worldX,
                      (int32_t)preparedAfter->worldY, &expectedTile) ||
        expectedTile != preparedAfter->tileIndex ||
        !EspNativeGameplayMonsterPosition_ensure()) {
        return 0;
    }
    live = findMutable(expectedBefore->spriteIndex);
    if (live == NULL || memcmp(live, expectedBefore, sizeof(*live)) != 0) return 0;
    *live = *preparedAfter;
    positionView.stateFNV1a = recordsFNV();
    return 1;
}

int EspNativeGameplayMonsterPosition_rollbackPrepared(
    const EspNativeGameplayMonsterPositionRecord* expectedAfter,
    const EspNativeGameplayMonsterPositionRecord* restoreBefore) {
    EspNativeGameplayMonsterPositionRecord* live;
    uint16_t restoreTile;

    if (expectedAfter == NULL || restoreBefore == NULL ||
        expectedAfter->spriteIndex != restoreBefore->spriteIndex ||
        !tileIndexFor((int32_t)restoreBefore->worldX,
                      (int32_t)restoreBefore->worldY, &restoreTile) ||
        restoreTile != restoreBefore->tileIndex ||
        !EspNativeGameplayMonsterPosition_ensure()) {
        return 0;
    }
    live = findMutable(expectedAfter->spriteIndex);
    if (live == NULL || memcmp(live, expectedAfter, sizeof(*live)) != 0) return 0;
    *live = *restoreBefore;
    positionView.stateFNV1a = recordsFNV();
    return 1;
}

uint32_t EspNativeGameplayMonsterPosition_fingerprint(void) {
    return EspNativeGameplayMonsterPosition_ensure()
               ? positionView.stateFNV1a
               : 0U;
}
