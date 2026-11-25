#include "esp_perf_move_visibility.h"
#include "esp_native_bsp_visibility.h"
#include <stdio.h>
#include <stdint.h>

/* One synchronous render lease. Reuses the existing BSP visibility pass.
 * Multiple settled frames (e.g. door animation) are never candidates. */
static struct {
    uint16_t sprite, x, y;
    uint8_t armed, samples, classification;
} probe;
static uint32_t total, rejectedBoth, sourceOnly, destOnly, bothVisible, unknown;

void EspPerfMoveVisibility_begin(uint16_t sprite, uint16_t x, uint16_t y) {
    probe.sprite = sprite;
    probe.x = x;
    probe.y = y;
    probe.samples = 0;
    probe.classification = 4;
    probe.armed = 1;
}

void EspPerfMoveVisibility_observe(const EspNativeBspVisibilityState* state) {
    uint32_t oldLeaf = UINT32_MAX, newLeaf = UINT32_MAX;
    int oldVisible, newVisible;
    if (!probe.armed) return;
    if (probe.samples != UINT8_MAX) ++probe.samples;
    if (probe.samples != 1) return;
    oldVisible = EspNativeBspVisibility_positionVisible(
        state, probe.sprite, probe.x, probe.y, &oldLeaf);
    newVisible = EspNativeBspVisibility_mapSpriteVisibleReadOnly(
        state, probe.sprite, &newLeaf);
    if (oldLeaf == UINT32_MAX || newLeaf == UINT32_MAX) return;
    probe.classification = (uint8_t)((oldVisible ? 1 : 0) | (newVisible ? 2 : 0));
}

void EspPerfMoveVisibility_end(int rendered) {
    if (!probe.armed) return;
    probe.armed = 0;
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
            (unsigned)total, (unsigned)rejectedBoth, (unsigned)sourceOnly,
            (unsigned)destOnly, (unsigned)bothVisible, (unsigned)unknown);
    }
}
