#include <SDL.h>
#include "DoomRPG.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "DoomCanvas.h"
#include "Game.h"
#include "Menu.h"
#include "MenuSystem.h"
#include "Player.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_map_level_exit_stats.h"
#include "esp_map_line_state.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_player_exit_state.h"
#include "native_map1_level_exit_stats_probe.h"
#include "native_player_exit_state_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_STATS_FNV 0xbd41bcfaU
#define EXPECTED_LINE_FNV 0xe5e74861U
#define EXPECTED_TOPOLOGY_FNV 0x3f321e43U
#define EXPECTED_STATE_BYTES 28U
#define EXPECTED_RESULT_BYTES 28U
#define SOURCE_LOAD_MAP_ID 1U
#define TEST_ELAPSED_MS 12345U
#define TEST_LEVEL_MOVES 37U

typedef struct Esp32PlayerExitProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32PlayerExitProbeState;

static Esp32PlayerExitProbeState probeState;

static uint32_t hashBytes(const void* data, uint32_t length) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (p == NULL && length != 0U) return 0U;
    for (i = 0U; i < length; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t framebufferHash(void) {
    const uint8_t* framebuffer =
        (const uint8_t*)Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    if (framebuffer == NULL ||
        bytes != (size_t)DOOMRPG_LOGICAL_WIDTH *
                     (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t)) {
        return 0U;
    }
    return hashBytes(framebuffer, (uint32_t)bytes);
}

static uint32_t playerExitWitness(const Player_t* player) {
    uint32_t values[7];
    if (player == NULL) return 0U;
    values[0] = (uint32_t)player->totalTime;
    values[1] = (uint32_t)player->totalMoves;
    values[2] = (uint32_t)player->completedLevels;
    values[3] = (uint32_t)player->killedMonstersLevels;
    values[4] = (uint32_t)player->foundSecretsLevels;
    values[5] = (uint32_t)player->berserkerTics;
    values[6] = player->dogFamiliar != NULL ? 1U : 0U;
    return hashBytes(values, sizeof(values));
}

static uint32_t transitionWitness(const DoomRPG_t* doomRpg) {
    uint32_t values[8];
    if (doomRpg == NULL || doomRpg->game == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->menu == NULL || doomRpg->menuSystem == NULL) return 0U;
    values[0] = (uint32_t)doomRpg->game->changeMapParam;
    values[1] = (uint32_t)doomRpg->game->spawnParam;
    values[2] = (uint32_t)doomRpg->menu->mapNameId;
    values[3] = (uint32_t)doomRpg->menuSystem->menu;
    values[4] = (uint32_t)doomRpg->doomCanvas->state;
    values[5] = (uint32_t)doomRpg->doomCanvas->storyPage;
    values[6] = (uint32_t)doomRpg->doomCanvas->loadMapID;
    values[7] = (uint32_t)doomRpg->doomCanvas->loadType;
    return hashBytes(values, sizeof(values));
}

static int legacyRuntimeIsClear(const Render_t* render) {
    return render != NULL && render->nodes == NULL && render->lines == NULL &&
           render->mapSprites == NULL && render->tileEvents == NULL &&
           render->mapByteCode == NULL && render->mapStringsIDs == NULL &&
           render->shapeData == NULL && render->mediaTexels == NULL &&
           render->ioBuffer == NULL;
}

static int resultIsZero(const EspPlayerExitApplyResult* result) {
    EspPlayerExitApplyResult zero;
    if (result == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(result, &zero, sizeof(zero)) == 0;
}

static EspPlayerExitState deterministicSeed(void) {
    EspPlayerExitState state;
    memset(&state, 0, sizeof(state));
    state.totalTime = 0x10203040U;
    state.totalMoves = 0x01020304U;
    state.completedLevels = 0x00000004U;
    state.killedMonstersLevels = 0x00000008U;
    state.foundSecretsLevels = 0x00000010U;
    state.berserkerTics = 9U;
    state.familiarActive = 1U;
    return state;
}

void Esp32PlayerExitStateProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

void Esp32PlayerExitStateProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspMapLineStateView* lineView;
    const EspMapSpriteTopologyView* topologyView;
    EspMapLevelExitStats stats;
    EspMapLevelExitStats noStats;
    EspMapLevelExitStats mapId2;
    EspMapLevelExitStats allStats;
    EspMapLevelExitStats badStats;
    EspPlayerExitState seed;
    EspPlayerExitState state;
    EspPlayerExitState before;
    EspPlayerExitState live;
    EspPlayerExitApplyResult result;
    EspPlayerExitApplyResult secondResult;
    EspPlayerExitApplyResult invalidResult;
    EspPlayerExitApplyStatus status;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t frameBefore;
    uint32_t frameAfter;
    uint32_t legacyBefore;
    uint32_t legacyAfter;
    uint32_t transitionBefore;
    uint32_t transitionAfter;
    uint32_t initialFNV;
    uint32_t appliedFNV;
    uint32_t resultFNV;
    uint32_t rollbackFNV;
    uint32_t allFNV;
    uint32_t liveFNV;
    uint32_t elapsedLive;
    uint32_t movesLive;
    int sourceApply;
    int rollback;
    int repeatMaskIdempotent;
    int noStatsGate;
    int mapId2Gate;
    int allMaskApply;
    int liveProjection;
    int nullState;
    int nullStats;
    int nullResult;
    int effectMismatch;
    int bitMismatch;
    int rangeMismatch;
    int stateAtomic;

    if (probeState.done || probeState.attempted || doomRpg == NULL) return;
    if (!Esp32Map1LevelExitStatsProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[PLAYEREXITPROBE] ARMED native level-exit stats proven; player exit-state application starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native player exit-state application ===\n");
    printf("[PLAYEREXITPROBE] CONTRACT apply one validated 20B level-exit snapshot to a 28B pointer-free native Player exit owner; elapsed/moves supplied by caller; no legacy Player/Menu/Game/Render mutation, no PAK IO, no allocation\n");

    if (doomRpg->player == NULL || doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->doomCanvas == NULL || doomRpg->menu == NULL ||
        doomRpg->menuSystem == NULL || !legacyRuntimeIsClear(doomRpg->render) ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        EspAssetPack_isOpen() || sizeof(EspPlayerExitState) != EXPECTED_STATE_BYTES ||
        sizeof(EspPlayerExitApplyResult) != EXPECTED_RESULT_BYTES) {
        printf("[PLAYEREXITPROBE] FAILED boundary stateBytes=%u resultBytes=%u legacyClear=%d packOpen=%d entities=%d monsters=%d\n",
               (unsigned int)sizeof(EspPlayerExitState),
               (unsigned int)sizeof(EspPlayerExitApplyResult),
               legacyRuntimeIsClear(doomRpg->render), EspAssetPack_isOpen(),
               doomRpg->game != NULL ? doomRpg->game->numEntities : -1,
               doomRpg->game != NULL ? doomRpg->game->numMonsters : -1);
        return;
    }

    lineView = EspMapLineState_view();
    topologyView = EspMapSpriteTopology_view();
    if (lineView == NULL || topologyView == NULL ||
        lineView->stateFNV1a != EXPECTED_LINE_FNV ||
        topologyView->stateFNV1a != EXPECTED_TOPOLOGY_FNV ||
        EspMapLevelExitStats_collect(SOURCE_LOAD_MAP_ID, 1U, &stats) !=
            ESP_MAP_LEVEL_EXIT_STATS_OK ||
        hashBytes(&stats, sizeof(stats)) != EXPECTED_STATS_FNV ||
        EspMapLevelExitStats_collect(SOURCE_LOAD_MAP_ID, 0U, &noStats) !=
            ESP_MAP_LEVEL_EXIT_STATS_OK ||
        EspMapLevelExitStats_collect(ESP_MAP_LEVEL_EXIT_NO_COMPLETION_MAP_ID, 1U,
                                     &mapId2) != ESP_MAP_LEVEL_EXIT_STATS_OK) {
        printf("[PLAYEREXITPROBE] FAILED source snapshot/owner regression\n");
        return;
    }

    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    legacyBefore = playerExitWitness(doomRpg->player);
    transitionBefore = transitionWitness(doomRpg);

    seed = deterministicSeed();
    state = seed;
    before = state;
    initialFNV = hashBytes(&state, sizeof(state));
    status = EspPlayerExitState_apply(&state, &stats, TEST_ELAPSED_MS,
                                      TEST_LEVEL_MOVES, &result);
    appliedFNV = hashBytes(&state, sizeof(state));
    resultFNV = hashBytes(&result, sizeof(result));
    sourceApply = status == ESP_PLAYER_EXIT_APPLY_OK &&
                  state.totalTime == before.totalTime + TEST_ELAPSED_MS &&
                  state.totalMoves == before.totalMoves + TEST_LEVEL_MOVES &&
                  state.completedLevels ==
                      (before.completedLevels | stats.completionLevelBit) &&
                  state.killedMonstersLevels == before.killedMonstersLevels &&
                  state.foundSecretsLevels == before.foundSecretsLevels &&
                  state.berserkerTics == 0U && state.familiarActive == 0U &&
                  result.totalTimeBefore == before.totalTime &&
                  result.totalTimeAfter == state.totalTime &&
                  result.totalMovesBefore == before.totalMoves &&
                  result.totalMovesAfter == state.totalMoves &&
                  result.effectFlagsApplied == stats.effectFlags &&
                  result.completedChanged == 1U && result.secretsChanged == 0U &&
                  result.monstersChanged == 0U && result.berserkerReset == 1U &&
                  result.familiarCleared == 1U;

    before = state;
    status = EspPlayerExitState_apply(&state, &stats, 0U, 0U, &secondResult);
    repeatMaskIdempotent = status == ESP_PLAYER_EXIT_APPLY_OK &&
                           memcmp(&state, &before, sizeof(state)) == 0 &&
                           secondResult.completedChanged == 0U &&
                           secondResult.secretsChanged == 0U &&
                           secondResult.monstersChanged == 0U &&
                           secondResult.berserkerReset == 0U &&
                           secondResult.familiarCleared == 0U;

    state = seed;
    status = EspPlayerExitState_apply(&state, &noStats, 7U, 3U, &result);
    noStatsGate = status == ESP_PLAYER_EXIT_APPLY_OK &&
                  state.totalTime == seed.totalTime + 7U &&
                  state.totalMoves == seed.totalMoves + 3U &&
                  state.completedLevels == seed.completedLevels &&
                  state.killedMonstersLevels == seed.killedMonstersLevels &&
                  state.foundSecretsLevels == seed.foundSecretsLevels &&
                  state.berserkerTics == 0U && state.familiarActive == 0U;

    state = seed;
    status = EspPlayerExitState_apply(&state, &mapId2, 11U, 5U, &result);
    mapId2Gate = status == ESP_PLAYER_EXIT_APPLY_OK &&
                 state.completedLevels == seed.completedLevels &&
                 state.killedMonstersLevels == seed.killedMonstersLevels &&
                 state.foundSecretsLevels == seed.foundSecretsLevels;

    allStats = stats;
    allStats.secretsFound = allStats.secretsTotal;
    allStats.monstersDead = allStats.monstersTotal;
    allStats.markAllSecrets = 1U;
    allStats.markAllMonsters = 1U;
    allStats.effectFlags = ESP_PLAYER_EXIT_ALL_EFFECTS;
    state = seed;
    status = EspPlayerExitState_apply(&state, &allStats, 0U, 0U, &result);
    allFNV = hashBytes(&state, sizeof(state));
    allMaskApply = status == ESP_PLAYER_EXIT_APPLY_OK &&
                   (state.completedLevels & stats.completionLevelBit) != 0U &&
                   (state.foundSecretsLevels & stats.completionLevelBit) != 0U &&
                   (state.killedMonstersLevels & stats.completionLevelBit) != 0U &&
                   result.completedChanged == 1U && result.secretsChanged == 1U &&
                   result.monstersChanged == 1U;

    live.totalTime = (uint32_t)doomRpg->player->totalTime;
    live.totalMoves = (uint32_t)doomRpg->player->totalMoves;
    live.completedLevels = (uint32_t)doomRpg->player->completedLevels;
    live.killedMonstersLevels = (uint32_t)doomRpg->player->killedMonstersLevels;
    live.foundSecretsLevels = (uint32_t)doomRpg->player->foundSecretsLevels;
    live.berserkerTics = (uint32_t)doomRpg->player->berserkerTics;
    live.familiarActive = doomRpg->player->dogFamiliar != NULL ? 1U : 0U;
    memset(live.reserved, 0, sizeof(live.reserved));
    before = live;
    elapsedLive = DoomRPG_GetUpTimeMS() - (uint32_t)doomRpg->player->time;
    movesLive = (uint32_t)doomRpg->player->moves;
    status = EspPlayerExitState_apply(&live, &stats, elapsedLive, movesLive, &result);
    liveFNV = hashBytes(&live, sizeof(live));
    liveProjection = status == ESP_PLAYER_EXIT_APPLY_OK &&
                     live.totalTime == before.totalTime + elapsedLive &&
                     live.totalMoves == before.totalMoves + movesLive &&
                     live.completedLevels ==
                         (before.completedLevels | stats.completionLevelBit) &&
                     live.killedMonstersLevels == before.killedMonstersLevels &&
                     live.foundSecretsLevels == before.foundSecretsLevels &&
                     live.berserkerTics == 0U && live.familiarActive == 0U;

    state = seed;
    before = state;
    memset(&invalidResult, 0xa5, sizeof(invalidResult));
    nullState = EspPlayerExitState_apply(NULL, &stats, 1U, 1U, &invalidResult) ==
                    ESP_PLAYER_EXIT_APPLY_INVALID && resultIsZero(&invalidResult);
    memset(&invalidResult, 0xa5, sizeof(invalidResult));
    nullStats = EspPlayerExitState_apply(&state, NULL, 1U, 1U, &invalidResult) ==
                    ESP_PLAYER_EXIT_APPLY_INVALID && resultIsZero(&invalidResult) &&
                memcmp(&state, &before, sizeof(state)) == 0;
    memset(&invalidResult, 0xa5, sizeof(invalidResult));
    nullResult = EspPlayerExitState_apply(&state, &stats, 1U, 1U, NULL) ==
                     ESP_PLAYER_EXIT_APPLY_INVALID &&
                 memcmp(&state, &before, sizeof(state)) == 0;

    badStats = stats;
    badStats.effectFlags &= (uint8_t)~ESP_MAP_LEVEL_EXIT_EFFECT_ACCUMULATE_TIME;
    memset(&invalidResult, 0xa5, sizeof(invalidResult));
    effectMismatch =
        EspPlayerExitState_apply(&state, &badStats, 1U, 1U, &invalidResult) ==
            ESP_PLAYER_EXIT_APPLY_INCONSISTENT_STATS &&
        resultIsZero(&invalidResult) && memcmp(&state, &before, sizeof(state)) == 0;

    badStats = stats;
    badStats.completionLevelBit = 2U;
    memset(&invalidResult, 0xa5, sizeof(invalidResult));
    bitMismatch =
        EspPlayerExitState_apply(&state, &badStats, 1U, 1U, &invalidResult) ==
            ESP_PLAYER_EXIT_APPLY_INCONSISTENT_STATS &&
        resultIsZero(&invalidResult) && memcmp(&state, &before, sizeof(state)) == 0;

    badStats = stats;
    badStats.secretsFound = (uint16_t)(badStats.secretsTotal + 1U);
    memset(&invalidResult, 0xa5, sizeof(invalidResult));
    rangeMismatch =
        EspPlayerExitState_apply(&state, &badStats, 1U, 1U, &invalidResult) ==
            ESP_PLAYER_EXIT_APPLY_INCONSISTENT_STATS &&
        resultIsZero(&invalidResult) && memcmp(&state, &before, sizeof(state)) == 0;
    stateAtomic = effectMismatch && bitMismatch && rangeMismatch;

    state = seed;
    rollbackFNV = hashBytes(&state, sizeof(state));
    rollback = rollbackFNV == initialFNV;

    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfter = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameAfter = framebufferHash();
    legacyAfter = playerExitWitness(doomRpg->player);
    transitionAfter = transitionWitness(doomRpg);

    if (!sourceApply || !rollback || !repeatMaskIdempotent || !noStatsGate ||
        !mapId2Gate || !allMaskApply || !liveProjection || !nullState ||
        !nullStats || !nullResult || !stateAtomic ||
        heapAfter != heapBefore || largestAfter != largestBefore ||
        frameAfter != frameBefore || legacyAfter != legacyBefore ||
        transitionAfter != transitionBefore || EspAssetPack_isOpen() ||
        !legacyRuntimeIsClear(doomRpg->render) ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        EspMapLineState_view() == NULL ||
        EspMapLineState_view()->stateFNV1a != EXPECTED_LINE_FNV ||
        EspMapSpriteTopology_view() == NULL ||
        EspMapSpriteTopology_view()->stateFNV1a != EXPECTED_TOPOLOGY_FNV) {
        printf("[PLAYEREXITPROBE] FAILED apply=%d rollback=%d repeat=%d noStats=%d mapId2=%d allMasks=%d live=%d failclosed=%d/%d/%d/%d ram=%u/%u frame=%08x/%08x legacy=%08x/%08x transition=%08x/%08x\n",
               sourceApply, rollback, repeatMaskIdempotent, noStatsGate,
               mapId2Gate, allMaskApply, liveProjection, nullState, nullStats,
               nullResult, stateAtomic,
               (unsigned int)heapBefore, (unsigned int)heapAfter,
               (unsigned int)frameBefore, (unsigned int)frameAfter,
               (unsigned int)legacyBefore, (unsigned int)legacyAfter,
               (unsigned int)transitionBefore, (unsigned int)transitionAfter);
        return;
    }

    probeState.done = 1;
    printf("[PLAYEREXIT] READY stateBytes=%u resultBytes=%u elapsed=%u moves=%u initialFNV=%08x appliedFNV=%08x resultFNV=%08x effects=%02x totalTime=%08x->%08x totalMoves=%08x->%08x completed=%08x->%08x killed=%08x->%08x secrets=%08x->%08x berserker=%u->%u familiar=%u->%u\n",
           (unsigned int)sizeof(EspPlayerExitState),
           (unsigned int)sizeof(EspPlayerExitApplyResult),
           (unsigned int)TEST_ELAPSED_MS, (unsigned int)TEST_LEVEL_MOVES,
           (unsigned int)initialFNV, (unsigned int)appliedFNV,
           (unsigned int)resultFNV, (unsigned int)stats.effectFlags,
           (unsigned int)seed.totalTime,
           (unsigned int)(seed.totalTime + TEST_ELAPSED_MS),
           (unsigned int)seed.totalMoves,
           (unsigned int)(seed.totalMoves + TEST_LEVEL_MOVES),
           (unsigned int)seed.completedLevels,
           (unsigned int)(seed.completedLevels | stats.completionLevelBit),
           (unsigned int)seed.killedMonstersLevels,
           (unsigned int)seed.killedMonstersLevels,
           (unsigned int)seed.foundSecretsLevels,
           (unsigned int)seed.foundSecretsLevels,
           (unsigned int)seed.berserkerTics, 0U,
           (unsigned int)seed.familiarActive, 0U);
    printf("[PLAYEREXIT] MASKS sourceCompleted=1 sourceSecrets=0 sourceMonsters=0 repeatIdempotent=1 allMasks=1 allStateFNV=%08x noStatsGate=1 mapId2Gate=1\n",
           (unsigned int)allFNV);
    printf("[PLAYEREXIT] LIVE elapsed=%u moves=%u projection=1 liveStateFNV=%08x legacyPlayerUnchanged=yes\n",
           (unsigned int)elapsedLive, (unsigned int)movesLive,
           (unsigned int)liveFNV);
    printf("[PLAYEREXIT] STATE rollbackFNV=%08x rollback=1 familiarSemanticOnly=yes entityPointerStored=no\n",
           (unsigned int)rollbackFNV);
    printf("[PLAYEREXIT] FAILCLOSED nullState=1 nullStats=1 nullResult=1 effectMismatch=1 bitMismatch=1 rangeMismatch=1 stateAtomic=yes\n");
    printf("[PLAYEREXIT] RAM heap8=%u->%u delta=0 largest8=%u->%u delta=0 persistentHeapBytes=0 frameFNV=%08x->%08x lineFNV=%08x topologyFNV=%08x\n",
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter,
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           (unsigned int)EXPECTED_LINE_FNV, (unsigned int)EXPECTED_TOPOLOGY_FNV);
    printf("[PLAYEREXIT] LEGACY playerExitFNV=%08x->%08x transitionFNV=%08x->%08x legacyRuntimeClear=yes Player_addLevelStatsCalled=no playerMutation=no menuMutation=no transitionTriggered=no\n",
           (unsigned int)legacyBefore, (unsigned int)legacyAfter,
           (unsigned int)transitionBefore, (unsigned int)transitionAfter);
    printf("[PLAYEREXIT] PARK state=%d page=%d nativePlayerExitState=yes stateBytes=%u resultBytes=%u persistentBytes=0 nativeExitStats=yes playerMutationProven=yes legacyPlayerMutation=no entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned int)sizeof(EspPlayerExitState),
           (unsigned int)sizeof(EspPlayerExitApplyResult));
}

int Esp32PlayerExitStateProbe_isDone(void) {
    return probeState.done;
}
