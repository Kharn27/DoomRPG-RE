#include "esp_perf_move_visibility.h"
#include "esp_native_bsp_visibility.h"
#include "esp_native_ambient_sprite.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* Only the immediately following synchronous continuation may reuse a full
 * frame. A skipped frame can extend the original witness once after commit; never
 * renew its time budget. No heap or framebuffer. */
static struct {
    uint32_t leaves[ESP_NATIVE_BSP_VISIBILITY_LEAF_WORDS];
    uint32_t sampleMs, phaseWindow;
    uint16_t sprite, x, y, destX, destY;
    uint8_t armed, samples, classification, quiet, valid, reuseCount, pending;
} probe;
static uint32_t total, rejectedBoth, sourceOnly, destOnly, bothVisible, unknown;
static uint32_t continuations, skipped, fallbackQuiet, fallbackWitness,
    fallbackLeaf, fallbackAge, eligible;
static uint32_t blockerCounts[ESP_PERF_MOVE_BLOCK_COUNT], chained;

/* Previous move visibility is deliberately only a prediction in P2e.
 * No first-goal caller may consume it as a presentation lease. */
static EspPerfMoveVisibilityContext previousContext;
static uint8_t contextValid, firstPending, firstPredicted;
static uint32_t firstAttempts, firstCandidates, firstNoWitness, firstBlocked,
    firstContextMismatch, firstAge, firstLeaf, firstChecked, firstConfirmed,
    firstDisagreed, firstPixelChanged, firstSideEffects, firstQuietAfter,
    firstUnchanged, firstCurrentRejected, firstCurrentEqual;
static uint32_t firstPhaseRejected, continuationPhaseRejected, firstPhaseCrossed,
    firstChangedAcrossPhase, unrelatedFrames, firstPriorSample, firstPriorWindow;

void EspPerfMoveVisibility_rememberContext(const EspPerfMoveVisibilityContext* context) {
    contextValid = (uint8_t)(context != NULL);
    if (context != NULL) previousContext = *context;
}
int EspPerfMoveVisibility_firstShadowBegin(const EspPerfMoveVisibilityContext* context,
    uint32_t blockers, uint16_t sprite, uint16_t oldX, uint16_t oldY,
    uint16_t newX, uint16_t newY, uint32_t nowMs) {
    uint32_t oldLeaf = UINT32_MAX, newLeaf = UINT32_MAX;
    ++firstAttempts;
    firstPending = 1U;
    firstPredicted = 0U;
    if (blockers != 0U) { ++firstBlocked; return 0; }
    if (!probe.valid || !contextValid || context == NULL) {
        ++firstNoWitness; return 0;
    }
    if (memcmp(context, &previousContext, sizeof(*context)) != 0) {
        ++firstContextMismatch; return 0;
    }
    if (nowMs < probe.sampleMs || nowMs - probe.sampleMs >= 250U) {
        ++firstAge; return 0;
    }
    if (nowMs - probe.sampleMs >= probe.phaseWindow) {
        ++firstPhaseRejected; return 0;
    }
    if ((oldX != newX && oldY != newY) ||
        !EspNativeBspVisibility_positionLeaf(sprite, oldX, oldY, &oldLeaf) ||
        !EspNativeBspVisibility_positionLeaf(sprite, newX, newY, &newLeaf) ||
        oldLeaf != newLeaf || oldLeaf >= ESP_NATIVE_BSP_VISIBILITY_MAX_NODES ||
        (probe.leaves[oldLeaf >> 5] & (1U << (oldLeaf & 31U)))) {
        ++firstLeaf; return 0;
    }
    ++firstCandidates;
    firstPriorSample = probe.sampleMs;
    firstPriorWindow = probe.phaseWindow;
    firstPredicted = 1U;
    return 1;
}
void EspPerfMoveVisibility_firstShadowEnd(int rendered, uint32_t beforeFNV,
    uint32_t afterFNV, int sideEffects, int quietAfter) {
    if (!firstPending) return;
    firstPending = 0U;
    if (rendered && probe.samples == 1U && probe.classification == 0U) {
        ++firstCurrentRejected;
        if (beforeFNV != 0U && beforeFNV == afterFNV) ++firstCurrentEqual;
    }
    if (firstPredicted) {
        ++firstChecked;
        if (rendered && probe.samples == 1U && probe.classification == 0U)
            ++firstConfirmed;
        else ++firstDisagreed;
        if (beforeFNV == 0U || beforeFNV != afterFNV) ++firstPixelChanged;
        if (probe.samples == 1U && probe.sampleMs >= firstPriorSample &&
            probe.sampleMs - firstPriorSample >= firstPriorWindow) {
            ++firstPhaseCrossed;
            if (beforeFNV == 0U || beforeFNV != afterFNV)
                ++firstChangedAcrossPhase;
        }
        if (sideEffects || !probe.quiet) ++firstSideEffects;
        if (!quietAfter) ++firstQuietAfter;
        if (rendered && probe.samples == 1U && probe.classification == 0U &&
            beforeFNV != 0U && beforeFNV == afterFNV &&
            !sideEffects && probe.quiet && quietAfter) ++firstUnchanged;
    }
    if ((firstAttempts & 15U) == 0U) {
        printf("[PERFGAMEP2E] first=%u candidates=%u blocked=%u noWitness=%u context=%u age=%u leaf=%u checked=%u confirmed=%u disagreed=%u pixelChanged=%u sideEffects=%u busyAfter=%u unchanged=%u currentRejected=%u currentEqual=%u mode=shadow firstSkipped=0 cumulative=yes\n",
            (unsigned)firstAttempts,(unsigned)firstCandidates,(unsigned)firstBlocked,
            (unsigned)firstNoWitness,(unsigned)firstContextMismatch,(unsigned)firstAge,
            (unsigned)firstLeaf,(unsigned)firstChecked,(unsigned)firstConfirmed,
            (unsigned)firstDisagreed,(unsigned)firstPixelChanged,(unsigned)firstSideEffects,
            (unsigned)firstQuietAfter,(unsigned)firstUnchanged,
            (unsigned)firstCurrentRejected,(unsigned)firstCurrentEqual);
        printf("[PERFGAMEP2F] firstPhaseRejected=%u continuationPhaseRejected=%u firstPhaseCrossed=%u changedAcrossPhase=%u unrelatedFrames=%u cumulative=yes\n",
            (unsigned)firstPhaseRejected,(unsigned)continuationPhaseRejected,
            (unsigned)firstPhaseCrossed,(unsigned)firstChangedAcrossPhase,
            (unsigned)unrelatedFrames);
    }
}

void EspPerfMoveVisibility_invalidate(void) {
    probe.armed = 0U;
    probe.valid = 0U;
    probe.pending = 0U;
    contextValid = 0U;
    firstPending = 0U;
}

void EspPerfMoveVisibility_commitReuse(int quiet) {
    if (!probe.pending) return;
    probe.pending = 0U;
    probe.valid = (uint8_t)(quiet && probe.reuseCount < 2U);
}

void EspPerfMoveVisibility_begin(uint16_t sprite, uint16_t x, uint16_t y,
    uint16_t destX, uint16_t destY, int quiet) {
    probe.sprite = sprite;
    probe.x = x;
    probe.y = y;
    probe.destX = destX;
    probe.destY = destY;
    probe.samples = 0;
    probe.phaseWindow = UINT32_MAX;
    probe.reuseCount = 0U;
    probe.pending = 0U;
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
void EspPerfMoveVisibility_frameStarted(int preview) {
    if (probe.armed) {
        if (preview) EspPerfMoveVisibility_noteUnsafe();
        return;
    }
    if (probe.valid || contextValid) ++unrelatedFrames;
    EspPerfMoveVisibility_invalidate();
}

void EspPerfMoveVisibility_noteAmbientPhase(uint32_t info, uint8_t type,
    uint8_t subtype, uint16_t index, uint32_t sampleMs) {
    uint32_t cadence, stagger, shifted, remaining;
    if (!probe.armed) return;
    if (probe.samples != 1U || sampleMs != probe.sampleMs) {
        EspPerfMoveVisibility_noteUnsafe();
        return;
    }
    if (!EspNativeAmbientSprite_eligible(info, type)) return;
    /* Same unsigned arithmetic as EspNativeAmbientSprite_frame().
     * Include non-rasterized candidates: a new frame can have new bounds. */
    stagger = (uint32_t)(index & 255U) * 8U;
    if (type == 1U) {
        cadence = EspNativeAmbientSprite_rapid(type, subtype)
            ? AMBIENT_RAPID_MS : AMBIENT_SLOW_MS;
        if (EspNativeAmbientSprite_rapid(type, subtype))
            stagger = (uint32_t)(index & 255U);
    } else if (type == 2U) {
        cadence = 6000U;
        stagger = (uint32_t)(index & 255U) * 256U;
    } else cadence = 250U;
    shifted = sampleMs + stagger;
    remaining = cadence - shifted % cadence;
    /* A wrap in the staggered clock may change the visual before the
     * normal phase boundary; choose the earlier conservative deadline. */
    if (UINT32_MAX - shifted < remaining)
        remaining = UINT32_MAX - shifted + 1U;
    if (remaining < probe.phaseWindow) probe.phaseWindow = remaining;
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
        printf("[PERFGAMEP2C] chained=%u blockers(world/automap/hub/touch/modal/door/attack/pain/indicator/present/pak/moveEvent/feedback/fx/pickup/ambient)=", (unsigned)chained);
        for (uint32_t i=0U; i<ESP_PERF_MOVE_BLOCK_COUNT; ++i)
            printf("%s%u", i ? "/" : "", (unsigned)blockerCounts[i]);
        printf(" cumulative=yes\n");
    }
}
int EspPerfMoveVisibility_tryReuse(int continuation, uint32_t blockers, uint16_t sprite,
    uint16_t oldX, uint16_t oldY, uint16_t newX, uint16_t newY, uint32_t nowMs) {
    const int valid = probe.valid;
    uint32_t oldLeaf = UINT32_MAX, newLeaf = UINT32_MAX;
    probe.valid = 0;
    probe.pending = 0U;
    if (!continuation) return 0;
    ++continuations;
    if (blockers != 0U) {
        ++fallbackQuiet;
        for (uint32_t i=0U; i<ESP_PERF_MOVE_BLOCK_COUNT; ++i)
            if (blockers & (1U<<i)) ++blockerCounts[i];
        return 0;
    }
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
    if (nowMs - probe.sampleMs >= probe.phaseWindow) {
        ++continuationPhaseRejected;
        return 0;
    }
    ++eligible;
    if (!DOOMRPG_PERF_MOVE_REUSE) return 0;
    ++skipped;
    if (probe.reuseCount != 0U) ++chained;
    ++probe.reuseCount;
    probe.destX = newX;
    probe.destY = newY;
    probe.pending = 1U;
    printf("[PERFGAMEP2B] REUSE sprite=%u leaf=%u source=%u,%u dest=%u,%u skipped=%u continuation=%u mode=fast\n",
        (unsigned)sprite,(unsigned)oldLeaf,(unsigned)oldX,(unsigned)oldY,
        (unsigned)newX,(unsigned)newY,(unsigned)skipped,(unsigned)continuations);
    return 1;
}
