#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"

#define RNG_REPLAY_GUARD_LEASE_MS 1000U

typedef struct EspNativeRngReplayGuard_s {
    Random_t preRefill;
    Random_t postRefill;
    uint32_t validUntilMs;
    uint32_t realRefills;
    uint32_t replayedRefills;
    uint8_t valid;
    uint8_t reserved[3];
} EspNativeRngReplayGuard;

static EspNativeRngReplayGuard rngReplayGuard;

void __real_DoomRPG_setRand(Random_t* rand);

static int leaseActive(uint32_t now) {
    return rngReplayGuard.valid != 0U &&
           (int32_t)(now - rngReplayGuard.validUntilMs) < 0;
}

/*
 * The legacy byte RNG has hidden refill state outside Random_t: DoomRPG_setRand()
 * advances the file-static resetRand/_seed generator in DoomRPG.c. Native
 * transactions historically snapshotted Random_t only, so a speculative draw
 * crossing nextRand==127 could regenerate the table, roll Random_t back, then
 * regenerate a different table during the live replay.
 *
 * Keep normal byte sequencing identical, but make an immediate exact refill
 * replay idempotent. The first boundary crossing calls the real generator and
 * caches pre/post Random_t. If a rollback restores that exact pre-refill state,
 * the next matching boundary crossing reuses the cached post-refill table and
 * does not advance hidden generator state a second time. A short lease prevents
 * stale state from surviving unrelated gameplay/session evolution.
 */
byte __wrap_DoomRPG_randNextByte(Random_t* rand) {
    int next;

    if (rand == NULL) return 0U;

    if ((rand->nextRand + (int)sizeof(byte)) >= RANDTABLESIZE) {
        uint32_t now = DoomRPG_GetUpTimeMS();

        if (leaseActive(now) &&
            memcmp(rand, &rngReplayGuard.preRefill, sizeof(*rand)) == 0) {
            *rand = rngReplayGuard.postRefill;
            ++rngReplayGuard.replayedRefills;
            printf("[RNGGUARD] REPLAY refill=%u replay=%u leaseMs=%u next=127->0 hiddenGenerator=untouched rollbackSafe=yes\n",
                   (unsigned int)rngReplayGuard.realRefills,
                   (unsigned int)rngReplayGuard.replayedRefills,
                   (unsigned int)RNG_REPLAY_GUARD_LEASE_MS);
        }
        else {
            rngReplayGuard.preRefill = *rand;
            __real_DoomRPG_setRand(rand);
            rngReplayGuard.postRefill = *rand;
            rngReplayGuard.validUntilMs = now + RNG_REPLAY_GUARD_LEASE_MS;
            rngReplayGuard.valid = 1U;
            ++rngReplayGuard.realRefills;
            printf("[RNGGUARD] REFILL refill=%u leaseMs=%u next=127->0 hiddenGenerator=advanced-once rollbackReplay=armed\n",
                   (unsigned int)rngReplayGuard.realRefills,
                   (unsigned int)RNG_REPLAY_GUARD_LEASE_MS);
        }
    }

    next = rand->nextRand;
    rand->nextRand = next + (int)sizeof(byte);
    return rand->randTable[next];
}
