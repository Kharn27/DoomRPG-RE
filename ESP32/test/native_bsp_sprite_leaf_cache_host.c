/* The real compact BSP leaf query: exactly the same visibility membership
 * before/after caching, on arena changes and beyond the bounded cache cap.
 * Includes production C, while host mocks immutable map access only. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/esp_native_bsp_visibility.c"

static uint8_t immutableArena[4] = {0};
static EspMapRuntimeView view;
static EspMapSprite sprites[2];
static EspMapNode nodes[3];
static unsigned spriteReads;
static unsigned nodeReads;

const EspMapRuntimeView* EspMapRuntime_view(void) { return &view; }

int EspMapRuntime_getMapSprite(uint32_t index, EspMapSprite* out) {
    if (out == NULL || index >= view.mapSpriteCount) return 0;
    ++spriteReads;
    *out = sprites[index & 1U];
    return 1;
}

int EspMapRuntime_getNode(uint32_t index, EspMapNode* out) {
    if (out == NULL || index >= view.nodeCount) return 0;
    ++nodeReads;
    *out = nodes[index];
    return 1;
}

static void query(const EspNativeBspVisibilityState* state,
                  uint32_t index, int expectedVisible,
                  uint32_t expectedLeaf) {
    uint32_t leaf = UINT32_MAX;
    int visible = EspNativeBspVisibility_mapSpriteVisibleReadOnly(
        state, index, &leaf);
    assert(visible == expectedVisible);
    assert(leaf == expectedLeaf);
}

int main(void) {
    EspNativeBspVisibilityState visible = {0};
    unsigned readsBefore;
    unsigned nodesBefore;

    view.arena = immutableArena;
    view.arenaFNV1a = 0x110U;
    view.nodeCount = 3U;
    view.mapSpriteCount = 2U;

    nodes[0].args1 = 0x10000U | 100U; /* X split */
    nodes[0].args2 = (1U << 16) | 2U; /* left/right leaves */
    nodes[1].args1 = 0U;
    nodes[2].args1 = 0U;
    sprites[0] = (EspMapSprite){80U, 80U, 0U};
    sprites[1] = (EspMapSprite){120U, 80U, 0U};
    visible.visibleLeaves[1U >> 5] |= 1U << (1U & 31U);

    /* First query computes leaf; following queries use the same leaf. */
    query(&visible, 0U, 1, 1U);
    readsBefore = spriteReads;
    nodesBefore = nodeReads;
    query(&visible, 0U, 1, 1U);
    assert(spriteReads == readsBefore && nodeReads == nodesBefore);
    query(&visible, 1U, 0, 2U);
    visible.visibleLeaves[2U >> 5] |= 1U << (2U & 31U);
    query(&visible, 1U, 1, 2U);
    assert(spriteReads == readsBefore + 1U);

    /* An arena CRC/FNV change invalidates the index even if the allocator
     * reuses the exact same backing pointer. */
    sprites[0].x = 120U;
    view.arenaFNV1a += 1U;
    query(&visible, 0U, 1, 2U);
    assert(spriteReads == readsBefore + 2U);

    /* Maps larger than the 2KB sidecar still run the uncached source query. */
    view.mapSpriteCount = MAX_CACHED_SPRITE_LEAVES + 1U;
    readsBefore = spriteReads;
    query(&visible, 0U, 1, 2U);
    query(&visible, 0U, 1, 2U);
    assert(spriteReads == readsBefore + 2U);

    /* Corrupt child/node: fail closed with original sentinel semantics. */
    view.mapSpriteCount = 2U;
    ++view.arenaFNV1a;
    nodes[0].args2 = (1U << 16) | 1024U;
    query(&visible, 0U, 0, UINT32_MAX);
    puts("native_bsp_sprite_leaf_cache_host: PASS cached/uncached parity, live visibility, arena invalidate, oversize fallback, malformed fail-closed");
    return 0;
}
