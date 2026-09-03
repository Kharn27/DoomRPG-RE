#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"

#define ENTITY_DEF_RECORD_BYTES 24U
#define ENTITY_DEF_LOOKUP_LIMIT 817U
#define ENTITY_DEF_MAX_COUNT 1024U
#define ENTITY_SUBTYPE_MASK 0x7fU
#define ENTITY_SUBTYPE_HAS_DEF 0x80U
#define VISUAL_HIDDEN 0x80U

static uint8_t* topologyStorage;
static uint8_t* entityTypes;
static uint8_t* entitySubTypes;
static uint8_t* visualStates;
static uint8_t* linkStatesLE;
static uint8_t* linkOrdersLE;
static EspMapSpriteTopologyView topologyView;

static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void writeLe16(uint8_t* p, uint16_t value) {
    p[0] = (uint8_t)(value & 0xffU);
    p[1] = (uint8_t)((value >> 8) & 0xffU);
}

static uint32_t hashByte(uint32_t hash, uint8_t value) {
    hash ^= value;
    return hash * 16777619U;
}

static uint32_t stateHash(void) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (topologyStorage == NULL) return 0U;
    for (i = 0U; i < topologyView.storageBytes; ++i) {
        hash = hashByte(hash, topologyStorage[i]);
    }
    hash = hashByte(hash, (uint8_t)(topologyView.nextLinkOrder & 0xffU));
    return hashByte(hash,
                    (uint8_t)((topologyView.nextLinkOrder >> 8) & 0xffU));
}

static uint16_t linkStateAt(uint32_t index) {
    return readLe16(linkStatesLE + (index * 2U));
}

static void setLinkState(uint32_t index, uint16_t value) {
    writeLe16(linkStatesLE + (index * 2U), value);
}

static uint16_t linkOrderAt(uint32_t index) {
    return readLe16(linkOrdersLE + (index * 2U));
}

static void setLinkOrder(uint32_t index, uint16_t value) {
    writeLe16(linkOrdersLE + (index * 2U), value);
}

static void refreshView(void) {
    uint32_t i;
    uint16_t state;

    topologyView.entityCount = 0U;
    topologyView.linkedCount = 0U;
    topologyView.hiddenCount = 0U;
    topologyView.enemyCount = 0U;
    topologyView.destructibleCount = 0U;

    for (i = 0U; i < topologyView.spriteCount; ++i) {
        state = linkStateAt(i);
        if ((state & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) != 0U) {
            ++topologyView.entityCount;
            if ((state & ESP_MAP_SPRITE_TOPOLOGY_LINKED) != 0U) {
                ++topologyView.linkedCount;
            }
        }
        if ((visualStates[i] & VISUAL_HIDDEN) != 0U) {
            ++topologyView.hiddenCount;
        }
        if (entityTypes[i] == ESP_MAP_ENTITY_TYPE_ENEMY) {
            ++topologyView.enemyCount;
        }
        else if (entityTypes[i] == ESP_MAP_ENTITY_TYPE_DESTRUCTIBLE) {
            ++topologyView.destructibleCount;
        }
    }
    topologyView.stateFNV1a = stateHash();
}

static int sameDescriptor(const EspMapEventDescriptor* a,
                          const EspMapEventDescriptor* b) {
    return a != NULL && b != NULL &&
           a->value == b->value &&
           a->eventIndex == b->eventIndex &&
           a->tileIndex == b->tileIndex &&
           a->firstCommandIndex == b->firstCommandIndex &&
           a->commandEndIndex == b->commandEndIndex &&
           a->commandCount == b->commandCount &&
           a->initialState == b->initialState &&
           a->flags == b->flags;
}

static int descriptorIsCanonical(const EspMapEventDescriptor* descriptor) {
    EspMapEventRef ref;
    EspMapEventDescriptor canonical;
    uint32_t value;

    if (descriptor == NULL ||
        !EspMapRuntime_getEvent(descriptor->eventIndex, &value)) return 0;

    ref.index = descriptor->eventIndex;
    ref.tileIndex = (uint16_t)(value & ESP_MAP_EVENT_TILE_MASK);
    ref.value = value;
    return EspMapEvents_describe(&ref, &canonical) &&
           sameDescriptor(descriptor, &canonical);
}

static int rawSpriteTile(const EspMapSprite* sprite, uint16_t* outTile) {
    uint32_t x;
    uint32_t y;

    if (sprite == NULL || outTile == NULL) return 0;
    x = (uint32_t)sprite->x >> 6;
    y = (uint32_t)sprite->y >> 6;
    if (x >= 32U || y >= 32U) return 0;
    *outTile = (uint16_t)((y * 32U) + x);
    return 1;
}

static int initialEntityTile(const EspMapSprite* sprite, uint16_t* outTile) {
    int32_t x;
    int32_t y;

    if (sprite == NULL || outTile == NULL) return 0;
    x = sprite->x;
    y = sprite->y;
    if ((sprite->info & 0x00200000UL) != 0U) x -= 64;
    else if ((sprite->info & 0x00100000UL) != 0U) y -= 64;
    else if ((sprite->info & 0x00080000UL) != 0U) y += 32;
    else if ((sprite->info & 0x00400000UL) != 0U) x += 32;

    x >>= 6;
    y >>= 6;
    if (x < 0 || x >= 32 || y < 0 || y >= 32) return 0;
    *outTile = (uint16_t)(((uint32_t)y * 32U) + (uint32_t)x);
    return 1;
}

static uint16_t findBlocker(uint16_t tileIndex, uint16_t excluded) {
    uint32_t i;
    uint16_t best = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    uint16_t bestOrder = 0U;
    uint16_t state;
    uint16_t order;
    uint8_t type;

    for (i = 0U; i < topologyView.spriteCount; ++i) {
        if (i == excluded) continue;
        state = linkStateAt(i);
        if ((state & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (state & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != tileIndex) continue;
        type = entityTypes[i];
        if (type != ESP_MAP_ENTITY_TYPE_ENEMY &&
            type != ESP_MAP_ENTITY_TYPE_DESTRUCTIBLE) continue;
        order = linkOrderAt(i);
        if (best == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE || order > bestOrder) {
            best = (uint16_t)i;
            bestOrder = order;
        }
    }
    return best;
}

static uint16_t findNextOnTile(uint16_t tileIndex, uint16_t belowOrder) {
    uint32_t i;
    uint16_t best = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    uint16_t bestOrder = 0U;
    uint16_t state;
    uint16_t order;

    for (i = 0U; i < topologyView.spriteCount; ++i) {
        state = linkStateAt(i);
        if ((state & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (state & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != tileIndex) continue;
        order = linkOrderAt(i);
        if (order >= belowOrder) continue;
        if (best == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE || order > bestOrder) {
            best = (uint16_t)i;
            bestOrder = order;
        }
    }
    return best;
}

static int blockerIsRandom(uint16_t spriteIndex) {
    uint16_t state;
    uint8_t subtype;

    if (spriteIndex == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE) return 0;
    state = linkStateAt(spriteIndex);
    if ((state & ESP_MAP_SPRITE_TOPOLOGY_ALIVE) == 0U ||
        entityTypes[spriteIndex] != ESP_MAP_ENTITY_TYPE_DESTRUCTIBLE) return 0;
    subtype = entitySubTypes[spriteIndex] & ENTITY_SUBTYPE_MASK;
    return subtype == ESP_MAP_ENTITY_SUBTYPE_CRATE;
}

static int removeBlocker(uint16_t spriteIndex,
                         uint8_t* removed,
                         uint8_t* noOp,
                         uint16_t* effectFlags) {
    uint16_t state;
    uint8_t type;

    if (spriteIndex == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE ||
        removed == NULL || noOp == NULL || effectFlags == NULL) return 0;

    state = linkStateAt(spriteIndex);
    if ((state & ESP_MAP_SPRITE_TOPOLOGY_ALIVE) == 0U) {
        ++(*noOp);
        return 1;
    }

    type = entityTypes[spriteIndex];
    if (type == ESP_MAP_ENTITY_TYPE_ENEMY) {
        /* Base topology consequence of Entity_died(): the enemy is unlinked
         * and its sprite enters the death frame. XP/sound/drop/AI side effects
         * remain explicitly deferred to later native gameplay owners. */
        visualStates[spriteIndex] =
            (uint8_t)((visualStates[spriteIndex] & 0xf0U) | 0x04U);
    }
    else if (type == ESP_MAP_ENTITY_TYPE_DESTRUCTIBLE) {
        /* Deterministic destructibles reach Game_remove(), which hides and
         * unlinks the map sprite. RNG-dependent crates are rejected in SHOW
         * preflight before any mutation. */
        visualStates[spriteIndex] |= VISUAL_HIDDEN;
    }
    else {
        return 0;
    }

    state &= (uint16_t)~(ESP_MAP_SPRITE_TOPOLOGY_ALIVE |
                         ESP_MAP_SPRITE_TOPOLOGY_LINKED);
    setLinkState(spriteIndex, state);
    setLinkOrder(spriteIndex, 0U);
    ++(*removed);
    *effectFlags |= ESP_MAP_SHOW_EFFECT_REMOVE_BLOCKER |
                    ESP_MAP_SHOW_EFFECT_DEFER_BLOCKER_GAMEPLAY;
    return 1;
}

void EspMapSpriteTopology_reset(void) {
    if (topologyStorage != NULL) heap_caps_free(topologyStorage);
    topologyStorage = NULL;
    entityTypes = NULL;
    entitySubTypes = NULL;
    visualStates = NULL;
    linkStatesLE = NULL;
    linkOrdersLE = NULL;
    memset(&topologyView, 0, sizeof(topologyView));
}

int EspMapSpriteTopology_buildFromRuntime(
    const EspAssetPackEntry* entityDefsEntry) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint8_t defTypes[ENTITY_DEF_LOOKUP_LIMIT];
    uint8_t defSubTypes[ENTITY_DEF_LOOKUP_LIMIT];
    uint8_t header[2];
    uint8_t record[ENTITY_DEF_RECORD_BYTES];
    uint32_t defCount;
    uint32_t sourceBytes;
    uint32_t storageBytes;
    uint32_t i;
    uint16_t tileIndex;
    uint8_t* cursor;
    uint32_t lookup;
    EspMapSprite sprite;

    if (runtime == NULL || entityDefsEntry == NULL ||
        (entityDefsEntry->flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        !EspAssetPack_isOpen() || runtime->mapSpriteCount == 0U ||
        runtime->mapSpriteCount > UINT16_MAX ||
        !EspAssetPack_readRange(entityDefsEntry, 0U, header, sizeof(header))) {
        return 0;
    }

    defCount = readLe16(header);
    if (defCount == 0U || defCount > ENTITY_DEF_MAX_COUNT) return 0;
    sourceBytes = 2U + (defCount * ENTITY_DEF_RECORD_BYTES);
    if (sourceBytes > entityDefsEntry->size) return 0;

    memset(defTypes, 0xff, sizeof(defTypes));
    memset(defSubTypes, 0xff, sizeof(defSubTypes));
    for (i = 0U; i < defCount; ++i) {
        if (!EspAssetPack_readRange(entityDefsEntry,
                                    2U + (i * ENTITY_DEF_RECORD_BYTES),
                                    record, sizeof(record))) return 0;
        tileIndex = readLe16(record);
        if (tileIndex < ENTITY_DEF_LOOKUP_LIMIT &&
            defTypes[tileIndex] == 0xffU) {
            defTypes[tileIndex] = record[2];
            defSubTypes[tileIndex] = record[3] & ENTITY_SUBTYPE_MASK;
        }
    }

    storageBytes = runtime->mapSpriteCount * 7U;
    if (topologyStorage == NULL || topologyView.storageBytes != storageBytes) {
        if (topologyStorage != NULL) heap_caps_free(topologyStorage);
        topologyStorage =
            (uint8_t*)heap_caps_malloc(storageBytes, MALLOC_CAP_8BIT);
        if (topologyStorage == NULL) {
            EspMapSpriteTopology_reset();
            return 0;
        }
    }
    memset(topologyStorage, 0, storageBytes);
    memset(&topologyView, 0, sizeof(topologyView));

    cursor = topologyStorage;
    entityTypes = cursor;
    cursor += runtime->mapSpriteCount;
    entitySubTypes = cursor;
    cursor += runtime->mapSpriteCount;
    visualStates = cursor;
    cursor += runtime->mapSpriteCount;
    linkStatesLE = cursor;
    cursor += runtime->mapSpriteCount * 2U;
    linkOrdersLE = cursor;

    topologyView.storage = topologyStorage;
    topologyView.entityTypes = entityTypes;
    topologyView.entitySubTypes = entitySubTypes;
    topologyView.visualStates = visualStates;
    topologyView.linkStatesLE = linkStatesLE;
    topologyView.linkOrdersLE = linkOrdersLE;
    topologyView.spriteCount = runtime->mapSpriteCount;
    topologyView.storageBytes = storageBytes;
    topologyView.entityDefCount = defCount;

    for (i = 0U; i < runtime->mapSpriteCount; ++i) {
        if (!EspMapRuntime_getMapSprite(i, &sprite)) {
            EspMapSpriteTopology_reset();
            return 0;
        }
        entityTypes[i] = 0xffU;
        entitySubTypes[i] = 0xffU;
        if ((sprite.info & 0x01000000UL) != 0U) continue;

        lookup = sprite.info & 511U;
        if ((sprite.info & 0x00040000UL) != 0U) lookup += 305U;
        if (lookup < ENTITY_DEF_LOOKUP_LIMIT && defTypes[lookup] != 0xffU) {
            entityTypes[i] = defTypes[lookup];
            entitySubTypes[i] =
                (uint8_t)((defSubTypes[lookup] & ENTITY_SUBTYPE_MASK) |
                          ENTITY_SUBTYPE_HAS_DEF);
        }
        else if ((sprite.info & 0x00020000UL) != 0U) {
            entityTypes[i] = (uint8_t)(lookup == 216U ? 15U : 14U);
            entitySubTypes[i] = 0U;
        }
    }

    if (!EspMapSpriteTopology_resetMutableFromRuntime()) {
        EspMapSpriteTopology_reset();
        return 0;
    }
    return 1;
}

int EspMapSpriteTopology_resetMutableFromRuntime(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint32_t i;
    uint16_t state;
    uint16_t tile;
    uint16_t order = 0U;
    uint8_t type;
    uint8_t subtype;
    int hasDef;
    EspMapSprite sprite;

    if (topologyStorage == NULL || runtime == NULL ||
        topologyView.spriteCount != runtime->mapSpriteCount ||
        topologyView.storageBytes != runtime->mapSpriteCount * 7U) return 0;

    for (i = 0U; i < runtime->mapSpriteCount; ++i) {
        if (!EspMapRuntime_getMapSprite(i, &sprite)) return 0;
        visualStates[i] = (uint8_t)((sprite.info >> 9) & 0xffU);
        setLinkOrder(i, 0U);
        state = 0U;
        type = entityTypes[i];
        subtype = entitySubTypes[i] & ENTITY_SUBTYPE_MASK;
        hasDef = (entitySubTypes[i] & ENTITY_SUBTYPE_HAS_DEF) != 0U;

        if (type != 0xffU) {
            state |= ESP_MAP_SPRITE_TOPOLOGY_EXISTS;
            if (hasDef) state |= ESP_MAP_SPRITE_TOPOLOGY_HAS_SPRITE_ENT;
            if (type == ESP_MAP_ENTITY_TYPE_ENEMY ||
                (type == ESP_MAP_ENTITY_TYPE_DESTRUCTIBLE && subtype != 4U)) {
                state |= ESP_MAP_SPRITE_TOPOLOGY_ALIVE;
            }
            if ((sprite.info & 0x00010000UL) == 0U) {
                if (!initialEntityTile(&sprite, &tile) || order == 0xffffU) {
                    return 0;
                }
                ++order;
                state |= ESP_MAP_SPRITE_TOPOLOGY_LINKED;
                state |= tile & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK;
                setLinkOrder(i, order);
            }
        }
        setLinkState(i, state);
    }

    topologyView.nextLinkOrder = order;
    refreshView();
    return 1;
}

int EspMapSpriteTopology_isReady(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    return topologyStorage != NULL && runtime != NULL &&
           topologyView.storage == topologyStorage &&
           topologyView.spriteCount == runtime->mapSpriteCount &&
           topologyView.storageBytes == runtime->mapSpriteCount * 7U;
}

const EspMapSpriteTopologyView* EspMapSpriteTopology_view(void) {
    return EspMapSpriteTopology_isReady() ? &topologyView : NULL;
}

int EspMapSpriteTopology_getVisualState(uint32_t spriteIndex,
                                        uint8_t* outVisualState) {
    if (!EspMapSpriteTopology_isReady() || outVisualState == NULL ||
        spriteIndex >= topologyView.spriteCount) return 0;
    *outVisualState = visualStates[spriteIndex];
    return 1;
}

int EspMapSpriteTopology_getEntity(uint32_t spriteIndex,
                                   uint8_t* outType,
                                   uint8_t* outSubType,
                                   uint16_t* outLinkState,
                                   uint16_t* outLinkOrder) {
    if (!EspMapSpriteTopology_isReady() || spriteIndex >= topologyView.spriteCount ||
        outType == NULL || outSubType == NULL || outLinkState == NULL ||
        outLinkOrder == NULL) return 0;
    *outType = entityTypes[spriteIndex];
    *outSubType = entitySubTypes[spriteIndex] & ENTITY_SUBTYPE_MASK;
    *outLinkState = linkStateAt(spriteIndex);
    *outLinkOrder = linkOrderAt(spriteIndex);
    return 1;
}

int EspMapSpriteTopology_prepareRelink(uint16_t spriteIndex,
                                      uint16_t destTile,
                                      EspMapSpriteTopologyRelink* outRelink) {
    uint16_t state;
    uint16_t order;

    if (outRelink != NULL) memset(outRelink, 0, sizeof(*outRelink));
    if (!EspMapSpriteTopology_isReady() || outRelink == NULL ||
        spriteIndex >= topologyView.spriteCount ||
        destTile > ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK ||
        topologyView.nextLinkOrder == 0xffffU) {
        return 0;
    }

    state = linkStateAt(spriteIndex);
    order = linkOrderAt(spriteIndex);
    if ((state & (ESP_MAP_SPRITE_TOPOLOGY_EXISTS |
                  ESP_MAP_SPRITE_TOPOLOGY_LINKED)) !=
            (ESP_MAP_SPRITE_TOPOLOGY_EXISTS |
             ESP_MAP_SPRITE_TOPOLOGY_LINKED) ||
        order == 0U ||
        (state & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) == destTile) {
        return 0;
    }

    outRelink->spriteIndex = spriteIndex;
    outRelink->sourceTile =
        (uint16_t)(state & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK);
    outRelink->destTile = destTile;
    outRelink->linkStateBefore = state;
    outRelink->linkStateAfter =
        (uint16_t)((state & (uint16_t)~ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) |
                   destTile | ESP_MAP_SPRITE_TOPOLOGY_LINKED);
    outRelink->linkOrderBefore = order;
    outRelink->linkOrderAfter = (uint16_t)(topologyView.nextLinkOrder + 1U);
    outRelink->nextLinkOrderBefore = topologyView.nextLinkOrder;
    outRelink->nextLinkOrderAfter = outRelink->linkOrderAfter;
    return 1;
}

int EspMapSpriteTopology_commitPreparedRelink(
    const EspMapSpriteTopologyRelink* relink) {
    if (!EspMapSpriteTopology_isReady() || relink == NULL ||
        relink->spriteIndex >= topologyView.spriteCount ||
        relink->sourceTile == relink->destTile ||
        relink->destTile > ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK ||
        relink->nextLinkOrderAfter != relink->linkOrderAfter ||
        relink->nextLinkOrderAfter !=
            (uint16_t)(relink->nextLinkOrderBefore + 1U) ||
        topologyView.nextLinkOrder != relink->nextLinkOrderBefore ||
        linkStateAt(relink->spriteIndex) != relink->linkStateBefore ||
        linkOrderAt(relink->spriteIndex) != relink->linkOrderBefore) {
        return 0;
    }

    setLinkState(relink->spriteIndex, relink->linkStateAfter);
    setLinkOrder(relink->spriteIndex, relink->linkOrderAfter);
    topologyView.nextLinkOrder = relink->nextLinkOrderAfter;
    refreshView();
    return 1;
}

int EspMapSpriteTopology_rollbackPreparedRelink(
    const EspMapSpriteTopologyRelink* relink) {
    if (!EspMapSpriteTopology_isReady() || relink == NULL ||
        relink->spriteIndex >= topologyView.spriteCount ||
        topologyView.nextLinkOrder != relink->nextLinkOrderAfter ||
        linkStateAt(relink->spriteIndex) != relink->linkStateAfter ||
        linkOrderAt(relink->spriteIndex) != relink->linkOrderAfter) {
        return 0;
    }

    setLinkState(relink->spriteIndex, relink->linkStateBefore);
    setLinkOrder(relink->spriteIndex, relink->linkOrderBefore);
    topologyView.nextLinkOrder = relink->nextLinkOrderBefore;
    refreshView();
    return 1;
}

EspMapSpriteTopologyStatus EspMapSpriteTopology_applyShow(
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapShowResult* outResult) {
    EspMapByteCode command;
    EspMapSprite sprite;
    uint32_t globalCommandIndex;
    uint32_t spriteIndex;
    uint16_t tileIndex;
    uint16_t blocker0;
    uint16_t blocker1 = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    uint16_t targetState;
    uint16_t blockerState;
    uint16_t effectFlags = 0U;
    uint8_t showFlags;
    uint8_t removed = 0U;
    uint8_t noOp = 0U;
    uint8_t hasEntity;

    if (outResult != NULL) memset(outResult, 0, sizeof(*outResult));
    if (!EspMapSpriteTopology_isReady()) return ESP_MAP_SPRITE_TOPOLOGY_NOT_READY;
    if (descriptor == NULL || outResult == NULL ||
        !descriptorIsCanonical(descriptor) ||
        commandOffset >= descriptor->commandCount || commandOffset > 0xffU ||
        !EspMapEvents_getCommand(descriptor, commandOffset, &command)) {
        return ESP_MAP_SPRITE_TOPOLOGY_INVALID;
    }
    if (command.id != ESP_MAP_OPCODE_SHOW) {
        return ESP_MAP_SPRITE_TOPOLOGY_UNSUPPORTED;
    }

    globalCommandIndex =
        (uint32_t)descriptor->firstCommandIndex + commandOffset;
    spriteIndex = command.arg1 & 0xffffU;
    if (globalCommandIndex > 0xffffU ||
        spriteIndex >= topologyView.spriteCount ||
        !EspMapRuntime_getMapSprite(spriteIndex, &sprite) ||
        !rawSpriteTile(&sprite, &tileIndex)) {
        return ESP_MAP_SPRITE_TOPOLOGY_OUT_OF_RANGE;
    }

    targetState = linkStateAt(spriteIndex);
    hasEntity = (uint8_t)((targetState &
                           ESP_MAP_SPRITE_TOPOLOGY_HAS_SPRITE_ENT) != 0U);
    if (hasEntity != 0U &&
        (targetState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) != 0U) {
        return ESP_MAP_SPRITE_TOPOLOGY_TARGET_ALREADY_LINKED;
    }
    if (hasEntity != 0U && topologyView.nextLinkOrder == 0xffffU) {
        return ESP_MAP_SPRITE_TOPOLOGY_ORDER_EXHAUSTED;
    }

    blocker0 = findBlocker(tileIndex, ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE);
    if (blocker0 != ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE) {
        if (blockerIsRandom(blocker0)) {
            return ESP_MAP_SPRITE_TOPOLOGY_RANDOM_BLOCKER;
        }
        blockerState = linkStateAt(blocker0);
        if ((blockerState & ESP_MAP_SPRITE_TOPOLOGY_ALIVE) != 0U) {
            blocker1 = findBlocker(tileIndex, blocker0);
        }
        else {
            /* Entity_died() no-ops, so the second lookup returns the same
             * still-linked blocker. */
            blocker1 = blocker0;
        }
        if (blocker1 != ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE &&
            blockerIsRandom(blocker1)) {
            return ESP_MAP_SPRITE_TOPOLOGY_RANDOM_BLOCKER;
        }
    }

    showFlags = (uint8_t)((command.arg1 >> 16) & 0xffU);
    outResult->sourceEventIndex = descriptor->eventIndex;
    outResult->globalCommandIndex = (uint16_t)globalCommandIndex;
    outResult->spriteIndex = (uint16_t)spriteIndex;
    outResult->tileIndex = tileIndex;
    outResult->blocker0SpriteIndex = blocker0;
    outResult->blocker1SpriteIndex = blocker1;
    outResult->sourceCommandOffset = (uint8_t)commandOffset;
    outResult->showFlags = showFlags;
    outResult->visualBefore = visualStates[spriteIndex];
    outResult->targetHasEntity = hasEntity;
    outResult->targetLinkedBefore =
        (uint8_t)((targetState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) != 0U);

    visualStates[spriteIndex] =
        (uint8_t)((visualStates[spriteIndex] & 0x70U) | showFlags);
    outResult->visualAfter = visualStates[spriteIndex];
    effectFlags |= ESP_MAP_SHOW_EFFECT_VISUAL_STATE;

    if (blocker0 != ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE) {
        ++outResult->blockersFound;
        if (!removeBlocker(blocker0, &removed, &noOp, &effectFlags)) {
            return ESP_MAP_SPRITE_TOPOLOGY_INVALID;
        }
    }
    if (blocker1 != ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE) {
        ++outResult->blockersFound;
        if (!removeBlocker(blocker1, &removed, &noOp, &effectFlags)) {
            return ESP_MAP_SPRITE_TOPOLOGY_INVALID;
        }
    }

    if (hasEntity != 0U) {
        ++topologyView.nextLinkOrder;
        targetState &= (uint16_t)~ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK;
        targetState |= tileIndex;
        targetState |= ESP_MAP_SPRITE_TOPOLOGY_LINKED;
        setLinkState(spriteIndex, targetState);
        setLinkOrder(spriteIndex, topologyView.nextLinkOrder);
        effectFlags |= ESP_MAP_SHOW_EFFECT_LINK_TARGET;
    }

    outResult->blockersRemoved = removed;
    outResult->blockerNoops = noOp;
    outResult->targetLinkedAfter =
        (uint8_t)((linkStateAt(spriteIndex) &
                   ESP_MAP_SPRITE_TOPOLOGY_LINKED) != 0U);
    outResult->legacyReturnValue = 1U;
    outResult->removeCommandIfHandled =
        (uint8_t)((command.arg2 &
                   ESP_MAP_SPRITE_TOPOLOGY_COMMAND_FLAG_REMOVE) != 0U);
    outResult->effectFlags = effectFlags;
    refreshView();
    return ESP_MAP_SPRITE_TOPOLOGY_OK;
}

EspMapSpriteTopologyStatus EspMapSpriteTopology_applyHide(
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapHideResult* outResult) {
    EspMapByteCode command;
    uint32_t globalCommandIndex;
    uint32_t x;
    uint32_t y;
    uint16_t tileIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    uint16_t current;
    uint16_t order;
    uint16_t state;
    uint16_t belowOrder = 0xffffU;
    uint8_t type;

    if (outResult != NULL) memset(outResult, 0, sizeof(*outResult));
    if (!EspMapSpriteTopology_isReady()) return ESP_MAP_SPRITE_TOPOLOGY_NOT_READY;
    if (descriptor == NULL || outResult == NULL ||
        !descriptorIsCanonical(descriptor) ||
        commandOffset >= descriptor->commandCount || commandOffset > 0xffU ||
        !EspMapEvents_getCommand(descriptor, commandOffset, &command)) {
        return ESP_MAP_SPRITE_TOPOLOGY_INVALID;
    }
    if (command.id != ESP_MAP_OPCODE_HIDE) {
        return ESP_MAP_SPRITE_TOPOLOGY_UNSUPPORTED;
    }

    globalCommandIndex =
        (uint32_t)descriptor->firstCommandIndex + commandOffset;
    if (globalCommandIndex > 0xffffU) return ESP_MAP_SPRITE_TOPOLOGY_INVALID;
    x = command.arg1 & 0xffU;
    y = (command.arg1 >> 8) & 0xffU;
    if (x < 32U && y < 32U) tileIndex = (uint16_t)((y * 32U) + x);

    outResult->sourceEventIndex = descriptor->eventIndex;
    outResult->globalCommandIndex = (uint16_t)globalCommandIndex;
    outResult->tileIndex = tileIndex;
    outResult->firstHiddenSpriteIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    outResult->lastHiddenSpriteIndex = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    outResult->sourceCommandOffset = (uint8_t)commandOffset;
    outResult->tileX = (uint8_t)x;
    outResult->tileY = (uint8_t)y;
    outResult->legacyReturnValue = 1U;
    outResult->removeCommandIfHandled =
        (uint8_t)((command.arg2 &
                   ESP_MAP_SPRITE_TOPOLOGY_COMMAND_FLAG_REMOVE) != 0U);

    if (tileIndex != ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE) {
        for (;;) {
            current = findNextOnTile(tileIndex, belowOrder);
            if (current == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE) break;
            order = linkOrderAt(current);
            if (order == 0U) return ESP_MAP_SPRITE_TOPOLOGY_INVALID;
            belowOrder = order;
            type = entityTypes[current];
            if (type == ESP_MAP_ENTITY_TYPE_ENEMY) continue;

            state = linkStateAt(current);
            visualStates[current] |= VISUAL_HIDDEN;
            state &= (uint16_t)~ESP_MAP_SPRITE_TOPOLOGY_LINKED;
            setLinkState(current, state);
            setLinkOrder(current, 0U);
            if (outResult->hiddenEntityCount == 0U) {
                outResult->firstHiddenSpriteIndex = current;
            }
            outResult->lastHiddenSpriteIndex = current;
            ++outResult->hiddenEntityCount;
        }
    }

    if (outResult->hiddenEntityCount != 0U) {
        outResult->effectFlags = ESP_MAP_HIDE_EFFECT_VISUAL_STATE |
                                 ESP_MAP_HIDE_EFFECT_UNLINK;
    }
    refreshView();
    return ESP_MAP_SPRITE_TOPOLOGY_OK;
}
