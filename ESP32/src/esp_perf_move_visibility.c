#include "esp_perf_move_visibility.h"
#include "esp_native_bsp_visibility.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* Only the immediately following synchronous continuation may reuse a full
 * frame. A skipped frame never seeds another reuse. No heap or framebuffer. */
static struct {
    uint32_t leaves[ESP_NATIVE_BSP_VISIBILITY_LEAF_WORDS];
    uint32_t sampleMs;
    uint16_t sprite, x, y, destX, destY;
    uint8_t armed, samples, classification, quiet, valid;
} probe;
static uint32_t total, rejectedBoth, sourceOnly, destOnly, bothVisible, unknown;
static uint32_t continuations, skipped, fallbackQuiet, fallbackWitness,
    fallbackLeaf, fallbackAge, eligible;

void EspPerfMoveVisibility_begin(uint16_t sprite, uint16_t x, uint16_t y,
    uint16_t destX, uint16_t destY, int quiet) {
    probe.sprite = sprite;
    probe.x = x;
    probe.y = y;
    probe.destX = destX;
    probe.destY = destY;
    probe.samples = 0;
    probe.classification = 4;
    probe.quiet = (uint8_t)(quiet != 0);
    probe.valid = 0;
    probe.armed = 1;
}
void EspPerfMoveVisibility_observe(const EspNativeBspVisibilityState* state,
    uint32_t sampleMs) {
    uint32_t oldLeaf = UINT32_MAX, newLeaf = UINT32_MAX;
    int oldVisible, newVisible;
    if (!probe.armed) return;
    if (probe.samples != UINT8_MAX) ++probe.samples;
    if (probe.samples != 1 || state == NULL) return;
    memcpy(probe.leaves, state->visibleLeaves, sizeof(probe.leaves));
    probe.sampleMs = sampleMs;
    oldVisible = EspNativeBspVisibility_positionVisible(
        state, probe.sprite, probe.x, probe.y, &oldLeaf);
    newVisible = EspNativeBspVisibility_mapSpriteVisibleReadOnly(
        state, probe.sprite, &newLeaf);
    if (oldLeaf == UINT32_MAX || newLeaf == UINT32_MAX) return;
    probe.classification = (uint8_t)((oldVisible ? 1 : 0) | (newVisible ? 2 : 0));
}
void EspPerfMoveVisibility_noteUnsafe(void) {
    if (probe.armed) probe.quiet = 0;
}
void EspPerfMoveVisibility_end(int rendered) {
    if (!probe.armed) return;
    probe.armed = 0;
    probe.valid = (uint8_t)(rendered == 1 && probe.samples == 1 &&
        probe.quiet && probe.classification != 4);
    ++total;
    if (!rendered || probe.samples != 1) ++unknown;
    else switch (probe.classification) {
    case 0: ++rejectedBoth; break;
    case 1: ++sourceOnly; break;
    case 2: ++destOnly; break;
    case 3: ++bothVisible; break;
    default: ++unknown; break;
    }
    if ((total & 15U) == 0U) {
        printf("[PERFGAMEP2A] renders=%u rejectedBoth=%u sourceOnly=%u destOnly=%u bothVisible=%u unknown=%u mode=shadow skipped=0 cumulative=yes\n",
            (unsigned)total,(unsigned)rejectedBoth,(unsigned)sourceOnly,
            (unsigned)destOnly,(unsigned)bothVisible,(unsigned)unknown);
        printf("[PERFGAMEP2B] continuation=%u eligible=%u skipped=%u fallbackQuiet=%u fallbackWitness=%u fallbackLeaf=%u fallbackAge=%u mode=%s cumulative=yes\n",
            (unsigned)continuations,(unsigned)eligible,(unsigned)skipped,
            (unsigned)fallbackQuiet,(unsigned)fallbackWitness,
            (unsigned)fallbackLeaf,(unsigned)fallbackAge,
            DOOMRPG_PERF_MOVE_REUSE ? "fast" : "shadow");
    }
}
int EspPerfMoveVisibility_tryReuse(int continuation, int quiet, uint16_t sprite,
    uint16_t oldX, uint16_t oldY, uint16_t newX, uint16_t newY, uint32_t nowMs) {
    const int valid = probe.valid;
    uint32_t oldLeaf = UINT32_MAX, newLeaf = UINT32_MAX;
    probe.valid = 0;
    if (!continuation) return 0;
    ++continuations;
    if (!quiet) { ++fallbackQuiet; return 0; }
    if (!valid || sprite != probe.sprite ||
        oldX != probe.destX || oldY != probe.destY) {
        ++fallbackWitness; return 0;
    }
    /* Same rejected convex BSP leaf: no admitted sprite or draw-order change.
     * Require a cardinal segment and a strictly bounded previous sample. */
    if ((oldX != newX && oldY != newY) ||
        !EspNativeBspVisibility_positionLeaf(sprite,oldX,oldY,&oldLeaf) ||
        !EspNativeBspVisibility_positionLeaf(sprite,newX,newY,&newLeaf) ||
        oldLeaf != newLeaf || oldLeaf >= ESP_NATIVE_BSP_VISIBILITY_MAX_NODES ||
        (probe.leaves[oldLeaf>>5] & (1U<<(oldLeaf&31U)))) {
        ++fallbackLeaf; return 0;
    }
    /* Ambient uses its existing due scheduler. Do not defer past the fastest
     * supported cadence even if no candidate currently arms that scheduler. */
    if (nowMs < probe.sampleMs || nowMs-probe.sampleMs >= 250U) {
        ++fallbackAge; return 0;
    }
    ++eligible;
    if (!DOOMRPG_PERF_MOVE_REUSE) return 0;
    ++skipped;
    printf("[PERFGAMEP2B] REUSE sprite=%u leaf=%u source=%u,%u dest=%u,%u skipped=%u continuation=%u mode=fast\n",
        (unsigned)sprite,(unsigned)oldLeaf,(unsigned)oldX,(unsigned)oldY,
        (unsigned)newX,(unsigned)newY,(unsigned)skipped,(unsigned)continuations);
    return 1;
}
