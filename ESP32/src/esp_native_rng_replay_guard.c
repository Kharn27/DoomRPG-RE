#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "esp_native_rng_replay_guard.h"

#define RNG_REPLAY_GUARD_LEASE_MS 1000U

typedef struct EspNativeRngReplayGuard_s {
    Random_t preRefill;
    Random_t postRefill;
    Random_t* probeReservedRand;
    uint32_t validUntilMs;
    uint32_t realRefills;
    uint32_t replayedRefills;
    uint8_t valid;
    uint8_t probeReserved;
    uint8_t reserved[2];
} EspNativeRngReplayGuard;

static EspNativeRngReplayGuard rngReplayGuard;

static int leaseActive(uint32_t now) {
    return rngReplayGuard.valid != 0U &&
           rngReplayGuard.probeReserved == 0U &&
           (int32_t)(now - rngReplayGuard.validUntilMs) < 0;
}

static int atByteBoundary(const Random_t* rand) {
    return rand != NULL &&
           (rand->nextRand + (int)sizeof(byte)) >= RANDTABLESIZE;
}

static int atWordBoundary(const Random_t* rand) {
    return rand != NULL &&
           (rand->nextRand + (int)sizeof(uint32_t)) >= RANDTABLESIZE;
}

/*
 * Materialize the post-refill table only for the duration of a bounded native
 * probe. The hidden resetRand/_seed generator necessarily advances once when a
 * genuinely new table is produced, but the live Random_t is restored after the
 * probe and the exact post-refill table is then reserved indefinitely for that
 * same Random_t pointer + exact pre-refill state. The next real byte draw reuses
 * it instead of advancing the hidden generator again.
 *
 * If an ordinary rollback/replay lease already owns the exact same pre-refill
 * state, promote that cached post-refill table to the persistent probe
 * reservation instead of generating another table. This is important for the
 * ranged-ai>=217 movement path, where monster-turn may have crossed the boundary
 * immediately before movement replay.
 */
int EspNativeRngReplayGuard_beginProbeBoundary(Random_t* liveRandom,
                                               Random_t* outSaved,
                                               uint8_t* outPrepared) {
    uint32_t now;

    if (outPrepared != NULL) *outPrepared = 0U;
    if (liveRandom == NULL || outSaved == NULL || outPrepared == NULL) return 0;
    *outSaved = *liveRandom;
    if (!atByteBoundary(liveRandom)) return 1;

    now = DoomRPG_GetUpTimeMS();
    if (rngReplayGuard.probeReserved != 0U) {
        if (rngReplayGuard.probeReservedRand != liveRandom ||
            memcmp(liveRandom, &rngReplayGuard.preRefill,
                   sizeof(*liveRandom)) != 0) {
            printf("[RNGGUARD] PROBE-CONFLICT next=%d reservedPtrMatch=%s preExact=no action=fail-closed hiddenGenerator=untouched\n",
                   liveRandom->nextRand,
                   rngReplayGuard.probeReservedRand == liveRandom ? "yes" : "no");
            return 0;
        }
        *liveRandom = rngReplayGuard.postRefill;
        *outPrepared = 1U;
        printf("[RNGGUARD] PROBE-BORROW refill=%u next=127->0 source=persistent-reservation hiddenGenerator=untouched liveRestore=required\n",
               (unsigned int)rngReplayGuard.realRefills);
        return 1;
    }

    if (leaseActive(now) &&
        memcmp(liveRandom, &rngReplayGuard.preRefill,
               sizeof(*liveRandom)) == 0) {
        rngReplayGuard.probeReserved = 1U;
        rngReplayGuard.probeReservedRand = liveRandom;
        *liveRandom = rngReplayGuard.postRefill;
        *outPrepared = 1U;
        printf("[RNGGUARD] PROBE-PROMOTE refill=%u next=127->0 source=rollback-lease hiddenGenerator=untouched reservation=until-live-replay\n",
               (unsigned int)rngReplayGuard.realRefills);
        return 1;
    }

    rngReplayGuard.preRefill = *liveRandom;
    rngReplayGuard.postRefill = *liveRandom;
    DoomRPG_setRand(&rngReplayGuard.postRefill);
    rngReplayGuard.probeReservedRand = liveRandom;
    rngReplayGuard.validUntilMs = 0U;
    rngReplayGuard.valid = 1U;
    rngReplayGuard.probeReserved = 1U;
    ++rngReplayGuard.realRefills;
    *liveRandom = rngReplayGuard.postRefill;
    *outPrepared = 1U;
    printf("[RNGGUARD] PROBE-REFILL refill=%u next=127->0 hiddenGenerator=advanced-once reservation=until-live-replay liveRestore=required\n",
           (unsigned int)rngReplayGuard.realRefills);
    return 1;
}

int EspNativeRngReplayGuard_endProbeBoundary(Random_t* liveRandom,
                                             const Random_t* saved,
                                             uint8_t prepared) {
    int exact;

    if (prepared == 0U) return 1;
    if (liveRandom == NULL || saved == NULL ||
        rngReplayGuard.probeReserved == 0U ||
        rngReplayGuard.probeReservedRand != liveRandom) {
        return 0;
    }

    exact = memcmp(liveRandom, &rngReplayGuard.postRefill,
                   sizeof(*liveRandom)) == 0;
    *liveRandom = *saved;
    printf("[RNGGUARD] PROBE-RESTORE refill=%u liveRandomExact=%s reservation=pending hiddenGenerator=advanced-once-total\n",
           (unsigned int)rngReplayGuard.realRefills,
           exact ? "yes" : "NO");
    return exact;
}

int EspNativeRngReplayGuard_commitProbeBoundary(Random_t* liveRandom,
                                                const Random_t* saved,
                                                uint8_t prepared,
                                                uint32_t consumedBytes) {
    uint32_t now;
    int expectedNext;

    if (prepared == 0U) return 1;
    if (liveRandom == NULL || saved == NULL || consumedBytes == 0U ||
        consumedBytes >= RANDTABLESIZE ||
        rngReplayGuard.probeReserved == 0U ||
        rngReplayGuard.probeReservedRand != liveRandom ||
        memcmp(saved, &rngReplayGuard.preRefill, sizeof(*saved)) != 0 ||
        memcmp(liveRandom->randTable, rngReplayGuard.postRefill.randTable,
               RANDTABLESIZE) != 0) {
        return 0;
    }

    expectedNext = rngReplayGuard.postRefill.nextRand + (int)consumedBytes;
    if (expectedNext >= RANDTABLESIZE || liveRandom->nextRand != expectedNext) {
        return 0;
    }

    now = DoomRPG_GetUpTimeMS();
    rngReplayGuard.probeReserved = 0U;
    rngReplayGuard.probeReservedRand = NULL;
    rngReplayGuard.validUntilMs = now + RNG_REPLAY_GUARD_LEASE_MS;
    rngReplayGuard.valid = 1U;
    printf("[RNGGUARD] PROBE-COMMIT refill=%u bytes=%u leaseMs=%u hiddenGenerator=advanced-once-total reservation=consumed rollbackReplay=armed sequenceExact=yes\n",
           (unsigned int)rngReplayGuard.realRefills,
           (unsigned int)consumedBytes,
           (unsigned int)RNG_REPLAY_GUARD_LEASE_MS);
    return 1;
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
 * stale ordinary rollback state from surviving unrelated gameplay evolution.
 * Probe reservations are stricter: they are tied to one exact live Random_t
 * pointer/state and persist until that live boundary is replayed. Consuming a
 * persistent reservation downgrades it to the ordinary rollback lease so a
 * speculative consumer can still roll Random_t back and have its immediate
 * live commit replay the exact same post-refill table.
 */
byte __wrap_DoomRPG_randNextByte(Random_t* rand) {
    int next;

    if (rand == NULL) return 0U;

    if (atByteBoundary(rand)) {
        uint32_t now = DoomRPG_GetUpTimeMS();

        if (rngReplayGuard.probeReserved != 0U) {
            if (rngReplayGuard.probeReservedRand == rand &&
                memcmp(rand, &rngReplayGuard.preRefill, sizeof(*rand)) == 0) {
                *rand = rngReplayGuard.postRefill;
                rngReplayGuard.probeReserved = 0U;
                rngReplayGuard.probeReservedRand = NULL;
                rngReplayGuard.validUntilMs = now + RNG_REPLAY_GUARD_LEASE_MS;
                rngReplayGuard.valid = 1U;
                ++rngReplayGuard.replayedRefills;
                printf("[RNGGUARD] PROBE-REPLAY refill=%u replay=%u leaseMs=%u next=127->0 hiddenGenerator=untouched reservation=consumed rollbackReplay=armed sequenceExact=yes\n",
                       (unsigned int)rngReplayGuard.realRefills,
                       (unsigned int)rngReplayGuard.replayedRefills,
                       (unsigned int)RNG_REPLAY_GUARD_LEASE_MS);
            }
            else {
                printf("[RNGGUARD] FATAL-RESERVATION-MISMATCH next=%d ptrMatch=%s sequenceExact=NO recovery=real-refill\n",
                       rand->nextRand,
                       rngReplayGuard.probeReservedRand == rand ? "yes" : "no");
                rngReplayGuard.probeReserved = 0U;
                rngReplayGuard.probeReservedRand = NULL;
                rngReplayGuard.valid = 0U;
                DoomRPG_setRand(rand);
                ++rngReplayGuard.realRefills;
            }
        }
        else if (leaseActive(now) &&
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
            DoomRPG_setRand(rand);
            rngReplayGuard.postRefill = *rand;
            rngReplayGuard.probeReservedRand = NULL;
            rngReplayGuard.validUntilMs = now + RNG_REPLAY_GUARD_LEASE_MS;
            rngReplayGuard.valid = 1U;
            rngReplayGuard.probeReserved = 0U;
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


/*
 * The inherited DoomRPG_randNextInt() treats Random_t::nextRand as an int
 * array index even though every RNG API advances it as a byte offset. That
 * multiplies the intended address by four: next==4 reads bytes 16..19 instead
 * of 4..7, and next>=32 reads beyond the 128-byte randTable entirely.
 *
 * Keep the recovered stream ownership and refill cadence, but interpret
 * nextRand consistently as a byte offset. Doom RPG BREW and the ESP32 target
 * are little-endian, so materialize the 32-bit word explicitly from the four
 * consecutive table bytes. The same replay guard used by byte draws protects
 * the hidden DoomRPG_setRand() generator when a transactional word draw crosses
 * its legacy refill boundary.
 */
int __wrap_DoomRPG_randNextInt(Random_t* rand) {
    int next;
    uint32_t word;

    if (rand == NULL) return 0;

    if (atWordBoundary(rand)) {
        uint32_t now = DoomRPG_GetUpTimeMS();
        int refillFrom = rand->nextRand;

        if (rngReplayGuard.probeReserved != 0U) {
            if (rngReplayGuard.probeReservedRand == rand &&
                memcmp(rand, &rngReplayGuard.preRefill, sizeof(*rand)) == 0) {
                *rand = rngReplayGuard.postRefill;
                rngReplayGuard.probeReserved = 0U;
                rngReplayGuard.probeReservedRand = NULL;
                rngReplayGuard.validUntilMs = now + RNG_REPLAY_GUARD_LEASE_MS;
                rngReplayGuard.valid = 1U;
                ++rngReplayGuard.replayedRefills;
                printf("[RNGGUARD] WORD-PROBE-REPLAY refill=%u replay=%u leaseMs=%u next=%d->0 bytes=4 hiddenGenerator=untouched reservation=consumed rollbackReplay=armed sequenceExact=yes\n",
                       (unsigned int)rngReplayGuard.realRefills,
                       (unsigned int)rngReplayGuard.replayedRefills,
                       (unsigned int)RNG_REPLAY_GUARD_LEASE_MS,
                       refillFrom);
            }
            else {
                printf("[RNGGUARD] WORD-FATAL-RESERVATION-MISMATCH next=%d ptrMatch=%s sequenceExact=NO recovery=real-refill\n",
                       rand->nextRand,
                       rngReplayGuard.probeReservedRand == rand ? "yes" : "no");
                rngReplayGuard.probeReserved = 0U;
                rngReplayGuard.probeReservedRand = NULL;
                rngReplayGuard.valid = 0U;
                DoomRPG_setRand(rand);
                ++rngReplayGuard.realRefills;
            }
        }
        else if (leaseActive(now) &&
                 memcmp(rand, &rngReplayGuard.preRefill, sizeof(*rand)) == 0) {
            *rand = rngReplayGuard.postRefill;
            ++rngReplayGuard.replayedRefills;
            printf("[RNGGUARD] WORD-REPLAY refill=%u replay=%u leaseMs=%u next=%d->0 bytes=4 hiddenGenerator=untouched rollbackSafe=yes\n",
                   (unsigned int)rngReplayGuard.realRefills,
                   (unsigned int)rngReplayGuard.replayedRefills,
                   (unsigned int)RNG_REPLAY_GUARD_LEASE_MS,
                   refillFrom);
        }
        else {
            rngReplayGuard.preRefill = *rand;
            DoomRPG_setRand(rand);
            rngReplayGuard.postRefill = *rand;
            rngReplayGuard.probeReservedRand = NULL;
            rngReplayGuard.validUntilMs = now + RNG_REPLAY_GUARD_LEASE_MS;
            rngReplayGuard.valid = 1U;
            rngReplayGuard.probeReserved = 0U;
            ++rngReplayGuard.realRefills;
            printf("[RNGGUARD] WORD-REFILL refill=%u leaseMs=%u next=%d->0 bytes=4 hiddenGenerator=advanced-once rollbackReplay=armed\n",
                   (unsigned int)rngReplayGuard.realRefills,
                   (unsigned int)RNG_REPLAY_GUARD_LEASE_MS,
                   refillFrom);
        }
    }

    next = rand->nextRand;
    rand->nextRand = next + (int)sizeof(uint32_t);
    word = (uint32_t)rand->randTable[next] |
           ((uint32_t)rand->randTable[next + 1] << 8) |
           ((uint32_t)rand->randTable[next + 2] << 16) |
           ((uint32_t)rand->randTable[next + 3] << 24);

    if (next >= (RANDTABLESIZE / (int)sizeof(uint32_t))) {
        printf("[RNGGUARD] WORD-OOB-AVOIDED next=%d correctedOffset=%d legacyOffset=%d value=%08x\n",
               next,
               next,
               next * (int)sizeof(uint32_t),
               (unsigned int)word);
    }

    return (int32_t)word;
}
