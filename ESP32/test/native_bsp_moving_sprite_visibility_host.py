#!/usr/bin/env python3
"""Regression: committed monster movement must not reuse the spawn BSP leaf.

Compiles the actual production spriteLeaf/cachedSpriteLeaf/visibleReadOnly
function bodies with a three-node BSP and a projected map-sprite getter.
The negative mutant removes the live-position bypass and MUST fail.
Synthetic BSP only: hardware playtesting remains the final visual witness.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "esp_native_bsp_visibility.c").read_text()
CC = shlex.split(os.environ.get("CC", "cc"))


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def extract(signature):
    require(SOURCE.count(signature) == 1, "missing unique production body: " + signature)
    start = SOURCE.index(signature)
    brace = SOURCE.index("{", start)
    depth = 0
    for pos in range(brace, len(SOURCE)):
        if SOURCE[pos] == "{":
            depth += 1
        elif SOURCE[pos] == "}":
            depth -= 1
            if depth == 0:
                return SOURCE[start:pos + 1]
    raise AssertionError("unbalanced production C function: " + signature)


CACHE = extract("static int cachedSpriteLeaf(")
LEAF = extract("static int spriteLeaf(")
QUERY = extract("int EspNativeBspVisibility_mapSpriteVisibleReadOnly(")
POSITION_LEAF = extract("int EspNativeBspVisibility_positionLeaf(")
POSITION = extract("int EspNativeBspVisibility_positionVisible(")
DECLS = re.findall(
    r"static struct \{\s*const uint8_t\* arena;[\s\S]*?\} spriteLeafCache;",
    SOURCE,
)
require(len(DECLS) == 1, "expected exactly one real 2-KiB cache declaration")
require("esp_native_gameplay_monster_movement_publish.h" in SOURCE,
        "visibility must depend on the explicit native projection owner")
require("EspNativeGameplayMonsterMovementPublish_isProjected(" in CACHE,
        "moving-monster cache bypass missing")
require("spriteLeaf(runtime, &sprite, outLeaf)" in CACHE,
        "projected coordinates must reach the real BSP traversal")

PREFIX = r"""
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define MAX_BSP_DEPTH 64U
#define MAX_CACHED_SPRITE_LEAVES 1024U
#define UNCACHED_SPRITE_LEAF UINT16_MAX
#define ESP_NATIVE_BSP_VISIBILITY_MAX_NODES 512U

typedef struct {
    const uint8_t *arena;
    uint32_t arenaFNV1a, nodeCount, mapSpriteCount;
} EspMapRuntimeView;
typedef struct { uint16_t x, y; uint32_t info; } EspMapSprite;
typedef struct { uint32_t args1, args2; } EspMapNode;
typedef struct { uint32_t visibleLeaves[16]; } EspNativeBspVisibilityState;

static EspMapRuntimeView map;
static EspMapNode nodes[3];
static uint8_t arenaA, arenaB;
static int projected;
static uint16_t movedX = 64U, rawX = 64U;
static uint32_t spriteReads, nodeReads, projectionChecks;

static const EspMapRuntimeView* EspMapRuntime_view(void) { return &map; }
int EspNativeGameplayMonsterMovementPublish_isProjected(uint16_t index) {
    ++projectionChecks;
    return index == 279U && projected;
}
static int EspMapRuntime_getMapSprite(uint32_t index, EspMapSprite* sprite) {
    if (sprite == NULL || index >= map.mapSpriteCount) return 0;
    ++spriteReads;
    *sprite = (EspMapSprite){ index == 279U && projected ? movedX : rawX, 64U, 0U };
    return 1;
}
static int EspMapRuntime_getNode(uint32_t index, EspMapNode* node) {
    if (node == NULL || index >= map.nodeCount) return 0;
    ++nodeReads;
    *node = nodes[index];
    return 1;
}
static void EspNativeSpriteVisual_coordinates(
    uint16_t x, uint16_t y, uint32_t info, int* outX, int* outY) {
    (void)info;
    *outX = x; *outY = y;
}
"""

CHECKS = r"""
static void selectLeaf(EspNativeBspVisibilityState* state, unsigned int leaf) {
    memset(state, 0, sizeof(*state));
    state->visibleLeaves[leaf >> 5] = 1UL << (leaf & 31U);
}
static void init(void) {
    memset(nodes, 0, sizeof(nodes));
    /* x > 96 -> leaf 1, x <= 96 -> leaf 2. */
    nodes[0].args1 = 0x00010000UL | 96U;
    nodes[0].args2 = (2UL << 16) | 1UL;
    map = (EspMapRuntimeView){ &arenaA, 0x12345678U, 3U, 320U };
    projected = 0;
    movedX = rawX = 64U;
    spriteReads = nodeReads = projectionChecks = 0U;
}
int main(void) {
    EspNativeBspVisibilityState visible;
    uint32_t leaf = UINT32_MAX, before;

    init();
    /* First renderer admission caches the immutable spawn leaf. */
    selectLeaf(&visible, 2U);
    assert(EspNativeBspVisibility_mapSpriteVisibleReadOnly(&visible,279U,&leaf));
    assert(leaf == 2U && spriteReads == 1U);
    assert(EspNativeBspVisibility_mapSpriteVisibleReadOnly(&visible,279U,&leaf));
    assert(spriteReads == 1U); /* untouched sprite is still cached */

    /* Door crossing: renderer sees projected x=128, combat/topology moved too.
     * The former immutable cache incorrectly returned leaf 2 here. */
    projected = 1; movedX = 128U;
    selectLeaf(&visible, 1U);
    assert(EspNativeBspVisibility_mapSpriteVisibleReadOnly(&visible,279U,&leaf));
    assert(leaf == 1U && spriteReads == 2U);
    assert(EspNativeBspVisibility_mapSpriteVisibleReadOnly(&visible,279U,&leaf));
    assert(leaf == 1U && spriteReads == 3U);

    /* Another committed movement / rollback must also take effect. */
    movedX = 64U;
    selectLeaf(&visible, 2U);
    assert(EspNativeBspVisibility_mapSpriteVisibleReadOnly(&visible,279U,&leaf));
    assert(leaf == 2U && spriteReads == 4U);

    /* Static objects never pay a repeated leaf traversal. */
    before = spriteReads;
    assert(EspNativeBspVisibility_mapSpriteVisibleReadOnly(&visible,12U,&leaf));
    assert(leaf == 2U && spriteReads == before + 1U);
    assert(EspNativeBspVisibility_mapSpriteVisibleReadOnly(&visible,12U,&leaf));
    assert(spriteReads == before + 1U);

    /* Deactivation: a sprite that never moved is once again cache-only. */
    projected = 0;
    before = spriteReads;
    assert(EspNativeBspVisibility_mapSpriteVisibleReadOnly(&visible,279U,&leaf));
    assert(leaf == 2U && spriteReads == before);

    /* Save/load rebuild of the same map replaces the cached arena identity. */
    map.arena = &arenaB; map.arenaFNV1a += 1U;
    rawX = 128U;
    selectLeaf(&visible, 1U);
    assert(EspNativeBspVisibility_mapSpriteVisibleReadOnly(&visible,279U,&leaf));
    assert(leaf == 1U && spriteReads == before + 1U);

    /* Large maps already bypass the bounded cache. */
    map.mapSpriteCount = 1100U;
    before = spriteReads;
    assert(EspNativeBspVisibility_mapSpriteVisibleReadOnly(&visible,1099U,&leaf));
    assert(leaf == 1U && spriteReads == before + 1U);
    assert(EspNativeBspVisibility_mapSpriteVisibleReadOnly(&visible,1099U,&leaf));
    assert(spriteReads == before + 2U);

    /* Explicit old position must not use the live projected leaf/cache. */
    assert(!EspNativeBspVisibility_positionVisible(&visible,279U,64U,64U,&leaf));
    assert(leaf == 2U);
    assert(EspNativeBspVisibility_positionVisible(&visible,279U,128U,64U,&leaf));
    assert(leaf == 1U);
    assert(!EspNativeBspVisibility_positionVisible(&visible,1100U,64U,64U,&leaf));
    assert(leaf == UINT32_MAX);

    /* Corrupt BSP child remains fail-closed, no stale leaf admitted. */
    map.arenaFNV1a += 1U;
    nodes[0].args2 = 0xffff0001UL;
    rawX = 64U; map.mapSpriteCount = 320U;
    leaf = 7U;
    assert(!EspNativeBspVisibility_mapSpriteVisibleReadOnly(&visible,279U,&leaf));
    assert(leaf == UINT32_MAX);
    assert(projectionChecks > 0U && nodeReads > 0U);
    puts("native_bsp_moving_sprite_visibility_host: PASS moving leaf, static cache, map reset, invalid BSP");
    return 0;
}
"""


def build_and_run(cache_body, directory, label):
    binary = directory / label
    source = directory / (label + ".c")
    source.write_text(
        PREFIX + "\n" + DECLS[0] + "\n" + LEAF + "\n" +
        cache_body + "\n" + QUERY + "\n" + POSITION_LEAF + "\n" + POSITION + "\n" + CHECKS
    )
    command = CC + ["-O2", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    str(source), "-o", str(binary)]
    compile_result = subprocess.run(command, capture_output=True, text=True)
    require(compile_result.returncode == 0,
            label + " compile failed: " + compile_result.stderr)
    return subprocess.run([str(binary)], capture_output=True, text=True)


def main():
    with tempfile.TemporaryDirectory(prefix="doomrpg-bsp-moving-monster-") as tmp:
        directory = Path(tmp)
        passed = build_and_run(CACHE, directory, "real")
        require(passed.returncode == 0, passed.stderr + passed.stdout)
        require("PASS moving leaf" in passed.stdout, passed.stdout)
        # Negative witness: reintroduce the former immutable-leaf behavior.
        marker = "(spriteIndex <= UINT16_MAX &&\n" \
                 "         EspNativeGameplayMonsterMovementPublish_isProjected(\n" \
                 "             (uint16_t)spriteIndex))"
        require(CACHE.count(marker) == 1, "mutant precondition changed")
        mutant = CACHE.replace(marker, "0")
        rejected = build_and_run(mutant, directory, "mutant")
        require(rejected.returncode != 0,
                "old stale-leaf implementation unexpectedly passed regression")
    print("PASS moving-monster BSP regression: production C + stale-cache mutant rejected")


if __name__ == "__main__":
    main()
