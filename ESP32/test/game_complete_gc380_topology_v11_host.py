#!/usr/bin/env python3
"""GC380: execute ACTUAL C topology snapshot/restore owner, not a Python model.

V9/V10/V11 section remains exactly 816 bytes: old 100x8 and new 133x6.
Real 504-sprite Sector 5 *structural cardinality* from the independently
verified BSP corpus (85 enemies + 48 destructibles), with representative
sorted 9-bit sprite indices. An exact Sector 5 PAK-backed runtime execution
is still a separate CYD hardware validation; this is a deterministic host
owner/body oracle.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
INC = ROOT / "include"
source = (SRC / "esp_map_sprite_topology.c").read_text()
header = (INC / "esp_map_sprite_topology.h").read_text()
save = (SRC / "esp_native_gameplay_save.cpp").read_text()
workflow = ROOT.parent / ".github/workflows/esp32-cyd.yml"

SIGNATURES = (
    "static uint16_t readLe16(",
    "static void writeLe16(",
    "static uint32_t hashByte(",
    "static uint16_t linkStateAt(",
    "static void setLinkState(",
    "static uint16_t linkOrderAt(",
    "static void setLinkOrder(",
    "int EspMapSpriteTopology_monsterSnapshotRecordAt(",
    "static uint32_t monsterSnapshotFNV(",
    "int EspMapSpriteTopology_monsterSnapshotShapeValid(",
    "int EspMapSpriteTopology_snapshotMonsters(",
    "int EspMapSpriteTopology_restoreMonsterSnapshot(",
)

def extract(src, signature):
    assert src.count(signature) == 1, "missing/duplicate actual C " + signature
    start = src.index(signature)
    opening = src.index("{", start)
    depth = 0
    for index in range(opening, len(src)):
        if src[index] == "{":
            depth += 1
        elif src[index] == "}":
            depth -= 1
            if depth == 0:
                return src[start:index + 1]
    raise AssertionError("unterminated C body " + signature)

def scan():
    assert "ESP_MAP_SPRITE_TOPOLOGY_MONSTER_SNAPSHOT_MAX 100U" in header
    assert "ESP_MAP_SPRITE_TOPOLOGY_MONSTER_SNAPSHOT_COMPACT_MAX 133U" in header
    assert "ESP_MAP_SPRITE_TOPOLOGY_MONSTER_SNAPSHOT_COMPACT_BYTES 6U" in header
    assert 'sizeof(EspMapSpriteTopologyMonsterSnapshot) == 816U' in save
    assert 'kRecordBytesV11 == 5604U' in save
    assert 'sizeof(NativeSaveRecordV9Tail) == 3508U' in save
    assert "EspMapSpriteTopology_monsterSnapshotRecordAt(" in save
    assert save.count("EspMapSpriteTopology_monsterSnapshotRecordAt(") >= 2
    assert "tail.monsterTopology.records[topologyCursor]" not in save
    assert "topology.records[topologyIndex]" not in save
    assert "topology.count > ESP_MAP_SPRITE_TOPOLOGY_MONSTER_SNAPSHOT_COMPACT_MAX" in save
    assert 'V11-CAPTURE-FAILED stage=monster-topology' in save
    assert 'V9-SPATIAL-STAGE' in save
    assert 'recordCrcV9(' in save  # file-wide CRC still protects 816B raw bytes
    assert "game_complete_gc380_topology_v11_host.py" in workflow.read_text()
    for sig in SIGNATURES:
        extract(source, sig)

PREFIX = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "esp_map_sprite_topology.h"
#include "esp_map_runtime.h"

#define SPRITES 1100U
static uint8_t types[SPRITES], visual[SPRITES];
static uint8_t states[SPRITES * 2U], orders[SPRITES * 2U];
static uint8_t *entityTypes = types, *visualStates = visual;
static uint8_t *linkStatesLE = states, *linkOrdersLE = orders;
static EspMapSpriteTopologyView topologyView;
static EspMapRuntimeView runtime;
static int ready = 1;

const EspMapRuntimeView* EspMapRuntime_view(void) { return &runtime; }
int EspMapSpriteTopology_isReady(void) { return ready; }
static void refreshView(void) {
    uint32_t i;
    topologyView.enemyCount = topologyView.destructibleCount =
        topologyView.humanCount = 0U;
    for (i = 0U; i < topologyView.spriteCount; ++i) {
        if (entityTypes[i] == ESP_MAP_ENTITY_TYPE_ENEMY)
            ++topologyView.enemyCount;
        if (entityTypes[i] == ESP_MAP_ENTITY_TYPE_DESTRUCTIBLE)
            ++topologyView.destructibleCount;
        if (entityTypes[i] == ESP_MAP_ENTITY_TYPE_HUMAN)
            ++topologyView.humanCount;
    }
}
"""

SUFFIX = r"""
static void prepare(uint16_t count) {
    uint32_t i;
    memset(types, 0xff, sizeof(types));
    memset(visual, 0, sizeof(visual));
    memset(states, 0, sizeof(states));
    memset(orders, 0, sizeof(orders));
    memset(&topologyView, 0, sizeof(topologyView));
    memset(&runtime, 0, sizeof(runtime));
    runtime.arenaFNV1a = 0x5ec70001U;
    runtime.mapSpriteCount = 504U;
    topologyView.spriteCount = 504U;
    topologyView.nextLinkOrder = count;
    for (i = 0U; i < count; ++i) {
        const uint32_t sprite = 3U * i;  /* 0..396: exceeds 8-bit ID */
        types[sprite] = i < 85U ? ESP_MAP_ENTITY_TYPE_ENEMY :
                                   ESP_MAP_ENTITY_TYPE_DESTRUCTIBLE;
        setLinkState(sprite, (uint16_t)(
            0x2000U | 0x0800U | 0x1000U | 0x0400U | (sprite & 1023U)));
        setLinkOrder(sprite, (uint16_t)(i + 1U));
        visual[sprite] = (uint8_t)(sprite ^ 0x5aU);
    }
    refreshView();
    assert(topologyView.enemyCount == (count < 85U ? count : 85U));
    assert(topologyView.destructibleCount == (count > 85U ? count - 85U : 0U));
}

static void clearMutable(void) {
    uint32_t i;
    for (i = 0U; i < topologyView.spriteCount; ++i) {
        if (entityTypes[i] == ESP_MAP_ENTITY_TYPE_ENEMY ||
            entityTypes[i] == ESP_MAP_ENTITY_TYPE_DESTRUCTIBLE ||
            entityTypes[i] == ESP_MAP_ENTITY_TYPE_HUMAN) {
            uint16_t state = linkStateAt(i);
            state &= (uint16_t)~(ESP_MAP_SPRITE_TOPOLOGY_LINKED |
                                ESP_MAP_SPRITE_TOPOLOGY_ALIVE);
            setLinkState(i, state);
            setLinkOrder(i, 0U);
            visualStates[i] ^= 0x80U;
        }
    }
    topologyView.nextLinkOrder = 0U;
    refreshView();
}

static uint32_t snapshotCompare(
    const EspMapSpriteTopologyMonsterSnapshot* a,
    const EspMapSpriteTopologyMonsterSnapshot* b) {
    assert(a->count == b->count && a->recordBytes == b->recordBytes);
    return memcmp(a, b, sizeof(*a)) == 0;
}

int main(void) {
    EspMapSpriteTopologyMonsterSnapshot snap, after, corrupt, old;
    EspMapSpriteTopologyMonsterRecord record;
    uint32_t i;

    assert(sizeof(EspMapSpriteTopologyMonsterRecord) == 8U);
    assert(sizeof(EspMapSpriteTopologyMonsterSnapshot) == 816U);
    assert(133U * 6U == 798U);
    assert(133U * 6U <= sizeof(snap.records));

    /* The 133 actual Sector 5 cardinality, 504 sprites, 85+48 categories.
     * Every tracked sprite (including dead/hidden) survives exact replay. */
    prepare(133U);
    assert(EspMapSpriteTopology_snapshotMonsters(&snap));
    assert(snap.count == 133U && snap.recordBytes == 6U);
    assert(snap.sourceArenaFNV1a == runtime.arenaFNV1a);
    assert(EspMapSpriteTopology_monsterSnapshotShapeValid(
        &snap, runtime.arenaFNV1a));
    assert(EspMapSpriteTopology_monsterSnapshotRecordAt(
        &snap, 132U, &record));
    assert(record.spriteIndex == 396U);
    assert(record.linkState == linkStateAt(396U));
    assert(record.linkOrder == 133U);
    assert(record.visualState == visualStates[396U]);
    assert(!EspMapSpriteTopology_monsterSnapshotRecordAt(
        &snap, 133U, &record));
    assert(((uint8_t*)snap.records)[798] == 0U &&
           ((uint8_t*)snap.records)[799] == 0U);

    clearMutable();
    assert(EspMapSpriteTopology_restoreMonsterSnapshot(&snap));
    assert(EspMapSpriteTopology_snapshotMonsters(&after));
    assert(snapshotCompare(&snap, &after));

    /* Save -> clear (EXIT) -> LOAD -> SAVE on same live owner. */
    clearMutable();
    assert(EspMapSpriteTopology_restoreMonsterSnapshot(&after));
    assert(EspMapSpriteTopology_snapshotMonsters(&corrupt));
    assert(snapshotCompare(&snap, &corrupt));

    /* Mutated corpse/hidden/de-linked state is exact, not recreated from
     * immutable BSP. Layout remains the same. */
    setLinkState(270U, 0x280eU);
    setLinkOrder(270U, 0U);
    visualStates[270U] = 0x84U;
    assert(EspMapSpriteTopology_snapshotMonsters(&snap));
    clearMutable();
    assert(EspMapSpriteTopology_restoreMonsterSnapshot(&snap));
    assert(linkStateAt(270U) == 0x280eU &&
           linkOrderAt(270U) == 0U && visualStates[270U] == 0x84U);
    assert(EspMapSpriteTopology_snapshotMonsters(&after));
    assert(snapshotCompare(&snap, &after));

    /* Rebuild the same clean Sector5 fixture for all adversarial checks. */
    prepare(133U);
    assert(EspMapSpriteTopology_snapshotMonsters(&snap));

    /* Invalid padding rejects even when semantic FNV is unchanged. */
    corrupt = snap;
    ((uint8_t*)corrupt.records)[799] = 7U;
    assert(!EspMapSpriteTopology_monsterSnapshotShapeValid(
        &corrupt, runtime.arenaFNV1a));

    /* Duplicate sprite indices and an omitted canonical tracked sprite
     * reject even after a malicious party recalculates semantic FNV. */
    corrupt = snap;
    ((uint8_t*)corrupt.records)[6U] = ((uint8_t*)corrupt.records)[0U];
    corrupt.stateFNV1a = monsterSnapshotFNV(&corrupt);
    assert(!EspMapSpriteTopology_monsterSnapshotShapeValid(
        &corrupt, runtime.arenaFNV1a));

    corrupt = snap;
    ((uint8_t*)corrupt.records)[(132U * 6U)] = 0xfdU;
    corrupt.stateFNV1a = monsterSnapshotFNV(&corrupt);
    assert(EspMapSpriteTopology_monsterSnapshotShapeValid(
        &corrupt, runtime.arenaFNV1a));
    assert(!EspMapSpriteTopology_restoreMonsterSnapshot(&corrupt));
    assert(linkStateAt(396U) == (uint16_t)(0x3c00U | (396U & 1023U)));

    corrupt = snap;
    corrupt.recordBytes = 7U;
    assert(!EspMapSpriteTopology_monsterSnapshotShapeValid(
        &corrupt, runtime.arenaFNV1a));
    corrupt = snap;
    corrupt.count = 134U;
    assert(!EspMapSpriteTopology_monsterSnapshotShapeValid(
        &corrupt, runtime.arenaFNV1a));
    corrupt = snap;
    corrupt.sourceArenaFNV1a ^= 1U;
    assert(!EspMapSpriteTopology_monsterSnapshotShapeValid(
        &corrupt, runtime.arenaFNV1a));
    corrupt = snap;
    ((uint8_t*)corrupt.records)[3U] = 0U;
    ((uint8_t*)corrupt.records)[4U] = 0U;
    corrupt.stateFNV1a = monsterSnapshotFNV(&corrupt);
    assert(!EspMapSpriteTopology_monsterSnapshotShapeValid(
        &corrupt, runtime.arenaFNV1a));
    corrupt = snap;
    ((uint8_t*)corrupt.records)[2U] &= (uint8_t)~0x20U;
    corrupt.stateFNV1a = monsterSnapshotFNV(&corrupt);
    assert(!EspMapSpriteTopology_monsterSnapshotShapeValid(
        &corrupt, runtime.arenaFNV1a));

    /* V9/V10/V11 old<=100 unchanged, including EVERY padding byte. */
    prepare(100U);
    assert(EspMapSpriteTopology_snapshotMonsters(&snap));
    assert(snap.count == 100U && snap.recordBytes == 8U);
    memset(&old, 0, sizeof(old));
    old.sourceArenaFNV1a = runtime.arenaFNV1a;
    old.count = 100U;
    old.recordBytes = (uint16_t)sizeof(EspMapSpriteTopologyMonsterRecord);
    old.nextLinkOrder = topologyView.nextLinkOrder;
    for (i = 0U; i < 100U; ++i) {
        old.records[i].spriteIndex = (uint16_t)(i * 3U);
        old.records[i].linkState = linkStateAt(i * 3U);
        old.records[i].linkOrder = linkOrderAt(i * 3U);
        old.records[i].visualState = visualStates[i * 3U];
    }
    old.stateFNV1a = monsterSnapshotFNV(&old);
    assert(snapshotCompare(&snap, &old));
    clearMutable();
    assert(EspMapSpriteTopology_restoreMonsterSnapshot(&old));
    assert(EspMapSpriteTopology_snapshotMonsters(&after));
    assert(snapshotCompare(&old, &after));

    /* Historical V9 enemy-only snapshot: only the 85 enemies restored;
     * destructible owner stays intact for legacy script replay. */
    prepare(133U);
    memset(&old, 0, sizeof(old));
    old.sourceArenaFNV1a = runtime.arenaFNV1a;
    old.count = 85U;
    old.recordBytes = 8U;
    old.nextLinkOrder = topologyView.nextLinkOrder;
    for (i = 0U; i < 85U; ++i) {
        uint32_t sprite = i * 3U;
        old.records[i].spriteIndex = (uint16_t)sprite;
        old.records[i].linkState = linkStateAt(sprite);
        old.records[i].linkOrder = linkOrderAt(sprite);
        old.records[i].visualState = visualStates[sprite];
    }
    old.stateFNV1a = monsterSnapshotFNV(&old);
    assert(EspMapSpriteTopology_monsterSnapshotShapeValid(
        &old, runtime.arenaFNV1a));
    setLinkState(390U, 0x280eU);  /* destructible mutation */
    setLinkOrder(390U, 0U);
    assert(EspMapSpriteTopology_restoreMonsterSnapshot(&old));
    assert(linkStateAt(390U) == 0x280eU);

    /* #463 regression: 133 entries with one HUMAN stay packed6 and exact.
     * Human SHOW/HIDE/visual states must never vanish on the GC380 path. */
    prepare(133U);
    types[396U] = ESP_MAP_ENTITY_TYPE_HUMAN;
    refreshView();
    assert(topologyView.enemyCount == 85U &&
           topologyView.destructibleCount == 47U &&
           topologyView.humanCount == 1U);
    assert(EspMapSpriteTopology_snapshotMonsters(&snap));
    assert(snap.count == 133U && snap.recordBytes == 6U);
    setLinkState(396U, 0x280eU);
    setLinkOrder(396U, 0U);
    visualStates[396U] = 0x91U;
    assert(EspMapSpriteTopology_snapshotMonsters(&snap));
    clearMutable();
    assert(EspMapSpriteTopology_restoreMonsterSnapshot(&snap));
    assert(linkStateAt(396U) == 0x280eU &&
           linkOrderAt(396U) == 0U && visualStates[396U] == 0x91U);
    assert(EspMapSpriteTopology_snapshotMonsters(&after));
    assert(snapshotCompare(&snap, &after));

    /* #463 regression: <=100 with human SHOW/HIDE remains legacy8. */
    prepare(71U);
    for (i = 65U; i < 71U; ++i)
        types[i * 3U] = ESP_MAP_ENTITY_TYPE_HUMAN;
    refreshView();
    assert(topologyView.enemyCount == 65U && topologyView.humanCount == 6U);
    assert(EspMapSpriteTopology_snapshotMonsters(&snap));
    assert(snap.count == 71U && snap.recordBytes == 8U);
    clearMutable();
    assert(EspMapSpriteTopology_restoreMonsterSnapshot(&snap));
    assert(EspMapSpriteTopology_snapshotMonsters(&after));
    assert(snapshotCompare(&snap, &after));

    /* Human inclusion beyond the representable 133 must fail closed. */
    prepare(133U);
    types[399U] = ESP_MAP_ENTITY_TYPE_HUMAN;
    setLinkState(399U, 0x3d8fU);
    setLinkOrder(399U, 134U);
    refreshView();
    assert(topologyView.humanCount == 1U);
    assert(!EspMapSpriteTopology_snapshotMonsters(&snap));

    /* 134 tracked fails closed rather than truncating to 133; no mutation. */
    prepare(134U);
    assert(!EspMapSpriteTopology_snapshotMonsters(&snap));

    /* An index beyond the 10-bit packed representation fails closed. */
    prepare(133U);
    types[396U] = 0xffU;
    types[1050U] = ESP_MAP_ENTITY_TYPE_DESTRUCTIBLE;
    setLinkState(1050U, 0x3c1aU);
    setLinkOrder(1050U, 133U);
    topologyView.spriteCount = 1100U;
    refreshView();
    assert(topologyView.enemyCount + topologyView.destructibleCount == 133U);
    assert(!EspMapSpriteTopology_snapshotMonsters(&snap));

    /* Without live runtime owner there is never a save commit. */
    prepare(133U);
    ready = 0;
    assert(!EspMapSpriteTopology_snapshotMonsters(&snap));
    ready = 1;

    puts("PASS GC380: real C topology snapshot/restore, 816B V11, 133 tracked, "
         "legacy8 byte exact, corruption and cap fail-closed");
    return 0;
}
"""

def main():
    scan()
    bodies = "\n\n".join(extract(source, sig) for sig in SIGNATURES)
    with tempfile.TemporaryDirectory(prefix="doom-gc380-") as tmp:
        directory = Path(tmp)
        path = directory / "actual_gc380.c"
        exe = directory / "actual_gc380"
        path.write_text(PREFIX + "\n\n" + bodies + "\n\n" + SUFFIX)
        subprocess.run(
            ["cc", "-std=c11", "-O0", "-g", "-Wall", "-Wextra", "-Werror",
             "-I", str(INC), str(path), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True, timeout=15)
    print("PASS GC380 source scan: true C snapshot/restore and C++ V11 ABI/spatial use")

if __name__ == "__main__":
    main()
