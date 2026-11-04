/* Real production C pre-destruction spawn contract, host regression.
 * cc -std=c11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections
 * -IESP32/include ESP32/src/esp_player_spawn_state.c
 * ESP32/src/esp_map_catalog.c ESP32/test/map_spawn_preflight_host.c
 * -Wl,--gc-sections -o /tmp/spawn-preflight && /tmp/spawn-preflight
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_player_spawn_state.h"

static EspBspInventory inventory(void) {
    EspBspInventory i;
    memset(&i, 0, sizeof(i));
    i.sourceBytes = 1000U;
    i.consumedBytes = 1000U;
    i.structuralEndOffset = 1000U;
    i.sections.endOffset = 1000U;
    i.expectedCrc32 = 0x12345678U;
    i.crc32 = 0x12345678U;
    i.fnv1a32 = 0xabcdef01U;
    i.plan.persistentBytes = 400U;
    i.loadMapId = 2U; /* Junction: catalog ID and gameplay ID differ. */
    i.spawnIndex = 33U;
    i.spawnDirection = 192U;
    return i;
}

static EspMapCommittedTransitionState transition(void) {
    EspMapCommittedTransitionState s;
    memset(&s, 0, sizeof(s));
    s.targetSourceBytes = 1000U;
    s.targetSourceCrc32 = 0x12345678U;
    s.targetSourceFNV1a = 0xabcdef01U;
    s.targetGameplayLoadMapId = 2U;
    s.targetMapId = 9U;
    s.sourceMapId = 1U;
    s.phase = ESP_MAP_COMMITTED_TRANSITION_PHASE_READY;
    s.pendingConsumed = 1U;
    return s;
}

static void reject(EspMapCommittedTransitionState* s,
                   EspBspInventory* i,
                   EspPlayerSpawnStatus expected) {
    EspPlayerSpawnState out;
    memset(&out, 0xcd, sizeof(out));
    assert(EspPlayerSpawn_preflightCommitted(s, i, 0U, 0U, &out) ==
           expected);
    {
        EspPlayerSpawnState empty;
        memset(&empty, 0, sizeof(empty));
        assert(memcmp(&out, &empty, sizeof(out)) == 0);
    }
}

int main(void) {
    EspBspInventory i = inventory();
    EspMapCommittedTransitionState s = transition();
    EspPlayerSpawnState out;
    assert(EspPlayerSpawn_preflightCommitted(&s, &i, 0U, 0U, &out) ==
           ESP_PLAYER_SPAWN_OK);
    assert(out.active == 1U && out.targetMapId == 9U &&
           out.gameplayLoadMapId == 2U &&
           out.tileIndex == 33U && out.worldX == 96U &&
           out.worldY == 96U && out.angle == 192U &&
           out.spawnSource == ESP_PLAYER_SPAWN_SOURCE_HEADER &&
           out.overrideUsed == 0U);
    assert(s.phase == ESP_MAP_COMMITTED_TRANSITION_PHASE_READY &&
           s.committed == 0U); /* No mutation or target installation. */

    s.spawnParam = (128U << 10U) | (30U << 5U) | 4U;
    assert(EspPlayerSpawn_preflightCommitted(&s, &i, 0U, 0U, &out) ==
           ESP_PLAYER_SPAWN_OK);
    assert(out.tileX == 4U && out.tileY == 30U &&
           out.tileIndex == 964U && out.angle == 128U &&
           out.worldX == 288U && out.worldY == 1952U &&
           out.overrideUsed == 1U &&
           out.spawnSource == ESP_PLAYER_SPAWN_SOURCE_OVERRIDE);
    s.spawnParam = 0U;

    i.spawnIndex = 1024U;
    reject(&s, &i, ESP_PLAYER_SPAWN_SPAWN_INVALID);
    i = inventory();
    i.crc32 ^= 1U;
    reject(&s, &i, ESP_PLAYER_SPAWN_TARGET_MISMATCH);
    i = inventory();
    i.loadMapId = 9U;
    reject(&s, &i, ESP_PLAYER_SPAWN_TARGET_MISMATCH);
    i = inventory();
    i.trailingBytes = 1U;
    reject(&s, &i, ESP_PLAYER_SPAWN_TARGET_MISMATCH);
    i = inventory();
    i.plan.persistentBytes = 0U;
    reject(&s, &i, ESP_PLAYER_SPAWN_TARGET_MISMATCH);
    i = inventory();

    s.phase = ESP_MAP_COMMITTED_TRANSITION_PHASE_WAIT_STATS;
    reject(&s, &i, ESP_PLAYER_SPAWN_NOT_COMMITTED);
    s = transition();
    s.committed = 1U;
    reject(&s, &i, ESP_PLAYER_SPAWN_NOT_COMMITTED);
    s = transition();
    s.pendingConsumed = 0U;
    reject(&s, &i, ESP_PLAYER_SPAWN_NOT_COMMITTED);
    s = transition();
    assert(EspPlayerSpawn_preflightCommitted(&s, &i, 0U, 1U, &out) ==
           ESP_PLAYER_SPAWN_UNSUPPORTED_CONTEXT);
    assert(EspPlayerSpawn_preflightCommitted(&s, &i, 0U, 0U, NULL) ==
           ESP_PLAYER_SPAWN_INVALID);
    puts("native CHANGEMAP spawn preflight: header/override/identity/stats/rollback PASS");
    return 0;
}
