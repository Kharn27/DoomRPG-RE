/*
 * GAME-S1 #444: exercise the REAL compact topology snapshot/restore owner.
 * Host-only fixture: Sector 1 witnessed six type-2 humans and 65 monsters /
 * destructibles; the PAK/campaign sprite identity must be verified separately.
 * No runtime ZIP, no framebuffer or PSRAM assumptions.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/esp_map_sprite_topology.c"

static EspMapRuntimeView witnessRuntime;
static uint8_t witnessStorage[5U * 7U];

const EspMapRuntimeView* EspMapRuntime_view(void) {
    return &witnessRuntime;
}

static void setupFive(void) {
    uint32_t n = 5U;
    memset(&witnessRuntime, 0, sizeof(witnessRuntime));
    memset(witnessStorage, 0, sizeof(witnessStorage));
    memset(&topologyView, 0, sizeof(topologyView));
    witnessRuntime.arenaFNV1a = 0x58f1a921UL;
    witnessRuntime.mapSpriteCount = n;
    topologyStorage = witnessStorage;
    entityTypes = topologyStorage;
    entitySubTypes = entityTypes + n;
    visualStates = entitySubTypes + n;
    linkStatesLE = visualStates + n;
    linkOrdersLE = linkStatesLE + n * 2U;

    topologyView.storage = topologyStorage;
    topologyView.entityTypes = entityTypes;
    topologyView.entitySubTypes = entitySubTypes;
    topologyView.visualStates = visualStates;
    topologyView.linkStatesLE = linkStatesLE;
    topologyView.linkOrdersLE = linkOrdersLE;
    topologyView.spriteCount = n;
    topologyView.storageBytes = n * 7U;
    topologyView.nextLinkOrder = 20U;

    /* 0 enemy, 1 previous human now hidden/unlinked,
     * 2 destructible, 3 next human visible/linked, 4 other actor. */
    entityTypes[0] = ESP_MAP_ENTITY_TYPE_ENEMY;
    entityTypes[1] = ESP_MAP_ENTITY_TYPE_HUMAN;
    entityTypes[2] = ESP_MAP_ENTITY_TYPE_DESTRUCTIBLE;
    entityTypes[3] = ESP_MAP_ENTITY_TYPE_HUMAN;
    entityTypes[4] = 14U;
    entitySubTypes[0] = entitySubTypes[1] = 0x80U;
    entitySubTypes[2] = entitySubTypes[3] = 0x80U;
    entitySubTypes[4] = 0x80U;

    visualStates[0] = 1U;
    visualStates[1] = 0x80U;
    visualStates[2] = 3U;
    visualStates[3] = 1U;
    visualStates[4] = 5U;
    setLinkState(0, ESP_MAP_SPRITE_TOPOLOGY_EXISTS |
                    ESP_MAP_SPRITE_TOPOLOGY_HAS_SPRITE_ENT |
                    ESP_MAP_SPRITE_TOPOLOGY_ALIVE |
                    ESP_MAP_SPRITE_TOPOLOGY_LINKED | 100U);
    setLinkOrder(0, 1U);
    setLinkState(1, ESP_MAP_SPRITE_TOPOLOGY_EXISTS |
                    ESP_MAP_SPRITE_TOPOLOGY_HAS_SPRITE_ENT | 647U);
    setLinkOrder(1, 0U);
    setLinkState(2, ESP_MAP_SPRITE_TOPOLOGY_EXISTS |
                    ESP_MAP_SPRITE_TOPOLOGY_HAS_SPRITE_ENT |
                    ESP_MAP_SPRITE_TOPOLOGY_ALIVE |
                    ESP_MAP_SPRITE_TOPOLOGY_LINKED | 220U);
    setLinkOrder(2, 3U);
    setLinkState(3, ESP_MAP_SPRITE_TOPOLOGY_EXISTS |
                    ESP_MAP_SPRITE_TOPOLOGY_HAS_SPRITE_ENT |
                    ESP_MAP_SPRITE_TOPOLOGY_LINKED | 710U);
    setLinkOrder(3, 17U);
    setLinkState(4, ESP_MAP_SPRITE_TOPOLOGY_EXISTS |
                    ESP_MAP_SPRITE_TOPOLOGY_HAS_SPRITE_ENT |
                    ESP_MAP_SPRITE_TOPOLOGY_LINKED | 345U);
    setLinkOrder(4, 18U);
    refreshView();
    assert(EspMapSpriteTopology_isReady());
    assert(topologyView.enemyCount == 1U &&
           topologyView.destructibleCount == 1U &&
           topologyView.humanCount == 2U);
}

static EspMapSpriteTopologyMonsterSnapshot oldV11(
    const EspMapSpriteTopologyMonsterSnapshot* current,
    int enemyOnly) {
    EspMapSpriteTopologyMonsterSnapshot old;
    memset(&old, 0, sizeof(old));
    old.sourceArenaFNV1a = current->sourceArenaFNV1a;
    old.recordBytes = (uint16_t)sizeof(EspMapSpriteTopologyMonsterRecord);
    old.nextLinkOrder = current->nextLinkOrder;
    old.records[0] = current->records[0];
    old.count = 1U;
    if (!enemyOnly) {
        old.records[1] = current->records[2];
        old.count = 2U;
    }
    old.stateFNV1a = monsterSnapshotFNV(&old);
    return old;
}

static void exerciseNewAndOld(void) {
    EspMapSpriteTopologyMonsterSnapshot snap;
    EspMapSpriteTopologyMonsterSnapshot old;
    uint16_t beforeOld;
    uint16_t beforeNext;
    uint8_t beforeVisual;

    assert(sizeof(snap) == 816U);
    setupFive();
    assert(EspMapSpriteTopology_snapshotMonsters(&snap));
    assert(snap.count == 4U && snap.records[0].spriteIndex == 0U &&
           snap.records[1].spriteIndex == 1U &&
           snap.records[2].spriteIndex == 2U &&
           snap.records[3].spriteIndex == 3U);
    assert(EspMapSpriteTopology_monsterSnapshotShapeValid(
        &snap, witnessRuntime.arenaFNV1a));

    /* Model LOAD rebuilding immutable BSP: old human reappears. Restore V11
     * full scope must hide it back, while preserving the new linked human. */
    setLinkState(1, ESP_MAP_SPRITE_TOPOLOGY_EXISTS |
                    ESP_MAP_SPRITE_TOPOLOGY_HAS_SPRITE_ENT |
                    ESP_MAP_SPRITE_TOPOLOGY_LINKED | 647U);
    setLinkOrder(1, 6U);
    visualStates[1] = 0U;
    setLinkState(3, ESP_MAP_SPRITE_TOPOLOGY_EXISTS |
                    ESP_MAP_SPRITE_TOPOLOGY_HAS_SPRITE_ENT);
    setLinkOrder(3, 0U);
    visualStates[3] = 0x80U;
    topologyView.nextLinkOrder = 22U;
    refreshView();
    assert(EspMapSpriteTopology_restoreMonsterSnapshot(&snap));
    assert((visualStates[1] & 0x80U) != 0U);
    assert((linkStateAt(1) & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U);
    assert(linkOrderAt(1) == 0U);
    assert((visualStates[3] & 0x80U) == 0U);
    assert((linkStateAt(3) & ESP_MAP_SPRITE_TOPOLOGY_LINKED) != 0U);
    assert(linkOrderAt(3) == 17U);
    assert(topologyView.nextLinkOrder == 20U);
    assert(topologyView.stateFNV1a == stateHash());

    /* Old V11 (enemy+destructible) must remain loadable. Human coverage
     * is irretrievably absent; restoration cannot invent historical state. */
    old = oldV11(&snap, 0);
    assert(EspMapSpriteTopology_monsterSnapshotShapeValid(
        &old, witnessRuntime.arenaFNV1a));
    setLinkState(1, ESP_MAP_SPRITE_TOPOLOGY_EXISTS |
                    ESP_MAP_SPRITE_TOPOLOGY_HAS_SPRITE_ENT |
                    ESP_MAP_SPRITE_TOPOLOGY_LINKED | 647U);
    setLinkOrder(1, 6U);
    visualStates[1] = 0U;
    refreshView();
    assert(EspMapSpriteTopology_restoreMonsterSnapshot(&old));
    assert((visualStates[1] & 0x80U) == 0U);
    assert((linkStateAt(1) & ESP_MAP_SPRITE_TOPOLOGY_LINKED) != 0U);
    old = oldV11(&snap, 1);
    assert(EspMapSpriteTopology_restoreMonsterSnapshot(&old));

    /* Structural tamper, even if checksum is recomputed, must fail before
     * applying a partial restore and must preserve all mutable bytes. */
    beforeOld = linkStateAt(1);
    beforeNext = topologyView.nextLinkOrder;
    beforeVisual = visualStates[1];
    snap.records[3].spriteIndex = 4U; /* type 14 cannot impersonate human */
    snap.stateFNV1a = monsterSnapshotFNV(&snap);
    assert(EspMapSpriteTopology_monsterSnapshotShapeValid(
        &snap, witnessRuntime.arenaFNV1a));
    assert(!EspMapSpriteTopology_restoreMonsterSnapshot(&snap));
    assert(linkStateAt(1) == beforeOld &&
           visualStates[1] == beforeVisual &&
           topologyView.nextLinkOrder == beforeNext);
    snap.records[3].spriteIndex = 3U;
    snap.stateFNV1a = monsterSnapshotFNV(&snap);
    ++snap.stateFNV1a; /* corrupted CRC/FNV */
    assert(!EspMapSpriteTopology_monsterSnapshotShapeValid(
        &snap, witnessRuntime.arenaFNV1a));
    assert(!EspMapSpriteTopology_restoreMonsterSnapshot(&snap));
}

static void exerciseCapacity(void) {
    static uint8_t large[101U * 7U];
    EspMapSpriteTopologyMonsterSnapshot snap;
    uint32_t i;
    memset(large, 0, sizeof(large));
    memset(&topologyView, 0, sizeof(topologyView));
    witnessRuntime.mapSpriteCount = 101U;
    topologyStorage = large;
    entityTypes = large;
    entitySubTypes = large + 101U;
    visualStates = entitySubTypes + 101U;
    linkStatesLE = visualStates + 101U;
    linkOrdersLE = linkStatesLE + 202U;
    topologyView.storage = large;
    topologyView.entityTypes = entityTypes;
    topologyView.entitySubTypes = entitySubTypes;
    topologyView.visualStates = visualStates;
    topologyView.linkStatesLE = linkStatesLE;
    topologyView.linkOrdersLE = linkOrdersLE;
    topologyView.spriteCount = 101U;
    topologyView.storageBytes = sizeof(large);
    topologyView.nextLinkOrder = 101U;
    for (i = 0U; i < 101U; ++i) {
        entityTypes[i] = (i < 65U ? ESP_MAP_ENTITY_TYPE_ENEMY :
                         (i < 98U ? ESP_MAP_ENTITY_TYPE_DESTRUCTIBLE :
                                    ESP_MAP_ENTITY_TYPE_HUMAN));
        setLinkState(i, ESP_MAP_SPRITE_TOPOLOGY_EXISTS |
                        ESP_MAP_SPRITE_TOPOLOGY_LINKED |
                        (uint16_t)i);
        setLinkOrder(i, (uint16_t)(i + 1U));
    }
    refreshView();
    assert(topologyView.enemyCount == 65U &&
           topologyView.destructibleCount == 33U &&
           topologyView.humanCount == 3U);
    assert(!EspMapSpriteTopology_snapshotMonsters(&snap));
}

int main(void) {
    exerciseNewAndOld();
    exerciseCapacity();
    puts("[GAME-S1-HUMANTOPO] PASS real C snapshot+restore full/legacy/tamper/capacity");
    return 0;
}
