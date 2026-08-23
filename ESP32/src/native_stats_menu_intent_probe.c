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
#include "esp_map_sprite_topology.h"
#include "esp_stats_menu_intent.h"
#include "native_player_exit_state_probe.h"
#include "native_stats_menu_intent_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_STATS_FNV 0xbd41bcfaU
#define EXPECTED_LINE_FNV 0xe5e74861U
#define EXPECTED_TOPOLOGY_FNV 0x3f321e43U
#define EXPECTED_INTENT_FNV 0x96afe901U
#define EXPECTED_OVERALL_FNV 0xdeea91b4U
#define EXPECTED_ZERO_FNV 0x4b95f515U
#define EXPECTED_INTENT_BYTES 4U
#define MAP_INTRO_TARGET_MAP 9U

static struct {
    int armed;
    int attempted;
    int done;
} probeState;

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

static uint32_t transitionWitness(const DoomRPG_t* doomRpg) {
    uint32_t values[8];
    if (doomRpg == NULL || doomRpg->game == NULL ||
        doomRpg->doomCanvas == NULL || doomRpg->menu == NULL ||
        doomRpg->menuSystem == NULL) return 0U;
    values[0] = (uint32_t)doomRpg->game->changeMapParam;
    values[1] = (uint32_t)doomRpg->game->spawnParam;
    values[2] = (uint32_t)(uint16_t)doomRpg->menu->mapNameId;
    values[3] = (uint32_t)doomRpg->menuSystem->menu;
    values[4] = (uint32_t)doomRpg->doomCanvas->state;
    values[5] = (uint32_t)doomRpg->doomCanvas->storyPage;
    values[6] = (uint32_t)(uint16_t)doomRpg->doomCanvas->loadMapID;
    values[7] = (uint32_t)doomRpg->doomCanvas->loadType;
    return hashBytes(values, sizeof(values));
}

static uint32_t playerExitWitness(const Player_t* player) {
    uint32_t values[8];
    if (player == NULL) return 0U;
    values[0] = (uint32_t)player->totalTime;
    values[1] = (uint32_t)player->totalMoves;
    values[2] = (uint32_t)player->completedLevels;
    values[3] = (uint32_t)player->killedMonstersLevels;
    values[4] = (uint32_t)player->foundSecretsLevels;
    values[5] = (uint32_t)player->berserkerTics;
    values[6] = (uint32_t)(uintptr_t)player->dogFamiliar;
    values[7] = (uint32_t)player->moves;
    return hashBytes(values, sizeof(values));
}

static int legacyRuntimeIsClear(const Render_t* render) {
    return render != NULL && render->nodes == NULL && render->lines == NULL &&
           render->mapSprites == NULL && render->tileEvents == NULL &&
           render->mapByteCode == NULL && render->mapStringsIDs == NULL &&
           render->shapeData == NULL && render->mediaTexels == NULL &&
           render->ioBuffer == NULL;
}

static int intentIsZero(const EspStatsMenuIntent* intent) {
    EspStatsMenuIntent zero;
    if (intent == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(intent, &zero, sizeof(zero)) == 0;
}

void Esp32StatsMenuIntentProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

int Esp32StatsMenuIntentProbe_isDone(void) {
    return probeState.done;
}

void Esp32StatsMenuIntentProbe_service(struct DoomRPG_s* doomRpg) {
    const EspMapLineStateView* lineState;
    const EspMapSpriteTopologyView* topology;
    EspMapLevelExitStats stats;
    EspStatsMenuIntent intent;
    EspStatsMenuIntent repeat;
    EspStatsMenuIntent overall;
    EspStatsMenuIntent noStats;
    EspStatsMenuIntent invalid;
    EspStatsMenuIntent resetProof;
    EspStatsMenuIntentStatus status;
    EspStatsMenuIntentStatus noStatsStatus;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t frameBefore;
    uint32_t frameAfter;
    uint32_t transitionBefore;
    uint32_t transitionAfter;
    uint32_t playerBefore;
    uint32_t playerAfter;
    uint32_t statsFNV;
    uint32_t intentFNV;
    uint32_t overallFNV;
    uint32_t zeroFNV;
    int target0;
    int target14;
    int showStats2;
    int nullIntent;
    int resetOk;
    int repeatExact;
    int safeBoundary;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32PlayerExitStateProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[STATSMENUPROBE] ARMED native player exit-state proven; stats-menu intent starts on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native stats-menu intent ===\n");
    printf("[STATSMENUPROBE] CONTRACT project CHANGEMAP showStats target into one 4B pointer-free LEVEL/OVERALL menu intent + pending-consume semantic; no legacy Menu/Game/Render mutation, no map load, no PAK IO, no allocation\n");

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL ||
        doomRpg->render == NULL || doomRpg->game == NULL ||
        doomRpg->menu == NULL || doomRpg->menuSystem == NULL ||
        doomRpg->player == NULL) {
        printf("[STATSMENUPROBE] FAILED missing legacy witness objects\n");
        probeState.done = 1;
        return;
    }

    lineState = EspMapLineState_view();
    topology = EspMapSpriteTopology_view();
    safeBoundary = doomRpg->doomCanvas->state == ST_INTRO &&
                   doomRpg->doomCanvas->storyPage == 3 &&
                   legacyRuntimeIsClear(doomRpg->render) &&
                   doomRpg->game->numEntities == 0 &&
                   doomRpg->game->numMonsters == 0 &&
                   !EspAssetPack_isOpen() &&
                   lineState != NULL && topology != NULL &&
                   lineState->stateFNV1a == EXPECTED_LINE_FNV &&
                   topology->stateFNV1a == EXPECTED_TOPOLOGY_FNV &&
                   MAP_JUNCTION == MAP_INTRO_TARGET_MAP &&
                   MAP_END_GAME == ESP_STATS_MENU_END_GAME_MAP_ID &&
                   MENU_MAP_STATS != MENU_MAP_STATS_OVERALL;
    if (!safeBoundary) {
        printf("[STATSMENUPROBE] FAILED unsafe boundary\n");
        probeState.done = 1;
        return;
    }

    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    transitionBefore = transitionWitness(doomRpg);
    playerBefore = playerExitWitness(doomRpg->player);

    memset(&stats, 0, sizeof(stats));
    if (EspMapLevelExitStats_collect(1U, 1U, &stats) !=
            ESP_MAP_LEVEL_EXIT_STATS_OK ||
        stats.showStats != 1U || stats.completionLevelBit != 1U ||
        stats.secretsFound != 0U || stats.secretsTotal != 6U ||
        stats.monstersDead != 0U || stats.monstersTotal != 30U ||
        stats.effectFlags != 0x1fU) {
        printf("[STATSMENUPROBE] FAILED inherited exit-stats contract\n");
        probeState.done = 1;
        return;
    }
    statsFNV = hashBytes(&stats, sizeof(stats));

    status = EspStatsMenuIntent_prepare(MAP_INTRO_TARGET_MAP,
                                        stats.showStats, &intent);
    intentFNV = hashBytes(&intent, sizeof(intent));
    if (status != ESP_STATS_MENU_INTENT_OK ||
        sizeof(intent) != EXPECTED_INTENT_BYTES ||
        intent.targetMapId != MAP_INTRO_TARGET_MAP ||
        intent.menuKind != ESP_STATS_MENU_KIND_LEVEL ||
        intent.active != 1U || intent.consumePending != 1U ||
        statsFNV != EXPECTED_STATS_FNV || intentFNV != EXPECTED_INTENT_FNV) {
        printf("[STATSMENUPROBE] FAILED source intent\n");
        probeState.done = 1;
        return;
    }

    memset(&repeat, 0xa5, sizeof(repeat));
    repeatExact = EspStatsMenuIntent_prepare(MAP_INTRO_TARGET_MAP, 1U, &repeat) ==
                      ESP_STATS_MENU_INTENT_OK &&
                  memcmp(&repeat, &intent, sizeof(intent)) == 0;

    memset(&overall, 0, sizeof(overall));
    overallFNV = 0U;
    if (EspStatsMenuIntent_prepare(ESP_STATS_MENU_END_GAME_MAP_ID, 1U,
                                   &overall) != ESP_STATS_MENU_INTENT_OK ||
        overall.menuKind != ESP_STATS_MENU_KIND_OVERALL ||
        overall.targetMapId != ESP_STATS_MENU_END_GAME_MAP_ID ||
        overall.active != 1U || overall.consumePending != 1U) {
        printf("[STATSMENUPROBE] FAILED overall variant\n");
        probeState.done = 1;
        return;
    }
    overallFNV = hashBytes(&overall, sizeof(overall));

    memset(&noStats, 0xa5, sizeof(noStats));
    noStatsStatus = EspStatsMenuIntent_prepare(MAP_INTRO_TARGET_MAP, 0U,
                                               &noStats);
    zeroFNV = hashBytes(&noStats, sizeof(noStats));

    memset(&invalid, 0xa5, sizeof(invalid));
    target0 = EspStatsMenuIntent_prepare(0U, 1U, &invalid) ==
                  ESP_STATS_MENU_INTENT_INVALID && intentIsZero(&invalid);
    memset(&invalid, 0xa5, sizeof(invalid));
    target14 = EspStatsMenuIntent_prepare(14U, 1U, &invalid) ==
                   ESP_STATS_MENU_INTENT_INVALID && intentIsZero(&invalid);
    memset(&invalid, 0xa5, sizeof(invalid));
    showStats2 = EspStatsMenuIntent_prepare(MAP_INTRO_TARGET_MAP, 2U, &invalid) ==
                     ESP_STATS_MENU_INTENT_INVALID && intentIsZero(&invalid);
    nullIntent = EspStatsMenuIntent_prepare(MAP_INTRO_TARGET_MAP, 1U, NULL) ==
                     ESP_STATS_MENU_INTENT_INVALID;
    memset(&resetProof, 0xa5, sizeof(resetProof));
    EspStatsMenuIntent_reset(&resetProof);
    resetOk = intentIsZero(&resetProof);

    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfter = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameAfter = framebufferHash();
    transitionAfter = transitionWitness(doomRpg);
    playerAfter = playerExitWitness(doomRpg->player);

    if (!repeatExact || overallFNV != EXPECTED_OVERALL_FNV ||
        noStatsStatus != ESP_STATS_MENU_INTENT_NOT_APPLICABLE ||
        !intentIsZero(&noStats) || zeroFNV != EXPECTED_ZERO_FNV ||
        !target0 || !target14 || !showStats2 || !nullIntent || !resetOk ||
        heapAfter != heapBefore || largestAfter != largestBefore ||
        frameAfter != frameBefore || transitionAfter != transitionBefore ||
        playerAfter != playerBefore || EspAssetPack_isOpen() ||
        lineState->stateFNV1a != EXPECTED_LINE_FNV ||
        topology->stateFNV1a != EXPECTED_TOPOLOGY_FNV) {
        printf("[STATSMENUPROBE] FAILED variant/failclosed/integrity audit\n");
        probeState.done = 1;
        return;
    }

    printf("[STATSMENU] READY intentBytes=%u targetMap=%u menuKind=%u active=%u consumePending=%u sourceStatsFNV=%08x intentFNV=%08x legacyMenuId=%d\n",
           (unsigned int)sizeof(intent), (unsigned int)intent.targetMapId,
           (unsigned int)intent.menuKind, (unsigned int)intent.active,
           (unsigned int)intent.consumePending, (unsigned int)statsFNV,
           (unsigned int)intentFNV, MENU_MAP_STATS);
    printf("[STATSMENU] VARIANTS endGameTarget=%u endGameKind=%u endGameFNV=%08x legacyOverallId=%d noStatsStatus=%u noStatsZero=1 zeroFNV=%08x repeatExact=1\n",
           (unsigned int)overall.targetMapId, (unsigned int)overall.menuKind,
           (unsigned int)overallFNV, MENU_MAP_STATS_OVERALL,
           (unsigned int)noStatsStatus, (unsigned int)zeroFNV);
    printf("[STATSMENU] FAILCLOSED target0=%d target14=%d showStats2=%d nullIntent=%d reset=%d stateAtomic=yes\n",
           target0, target14, showStats2, nullIntent, resetOk);
    printf("[STATSMENU] RAM heap8=%u->%u delta=%d largest8=%u->%u delta=%d persistentHeapBytes=0 frameFNV=%08x->%08x lineFNV=%08x topologyFNV=%08x\n",
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (int)heapBefore - (int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter,
           (int)largestBefore - (int)largestAfter,
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           (unsigned int)lineState->stateFNV1a,
           (unsigned int)topology->stateFNV1a);
    printf("[STATSMENU] LEGACY playerExitFNV=%08x->%08x transitionFNV=%08x->%08x legacyRuntimeClear=yes menuMutation=no Game_changeMapCalled=no mapLoad=no\n",
           (unsigned int)playerBefore, (unsigned int)playerAfter,
           (unsigned int)transitionBefore, (unsigned int)transitionAfter);
    printf("[STATSMENU] PARK state=%d page=%d nativeStatsMenuIntent=yes intentBytes=%u targetMap=%u menuKind=LEVEL consumePendingSemantic=yes persistentBytes=0 nativePlayerExitState=yes legacyMenuMutation=no transitionTriggered=no entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned int)sizeof(intent), (unsigned int)intent.targetMapId,
           doomRpg->game->numEntities, doomRpg->game->numMonsters);

    probeState.done = 1;
}
