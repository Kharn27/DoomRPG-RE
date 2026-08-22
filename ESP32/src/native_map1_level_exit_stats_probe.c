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
#include "esp_map_automap_state.h"
#include "esp_map_events.h"
#include "esp_map_level_exit_stats.h"
#include "esp_map_line_state.h"
#include "esp_map_line_texture_state.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"
#include "native_map1_level_exit_stats_probe.h"
#include "native_map1_show_hide_final_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_ARENA_FNV 0xc3882516U
#define EXPECTED_MAP_STATE_FNV 0xcd99b98eU
#define EXPECTED_SCRIPT_FNV 0xf9e3d9dfU
#define EXPECTED_LINE_STATE_FNV 0xe5e74861U
#define EXPECTED_TEXTURE_STATE_FNV 0xf1fc1875U
#define EXPECTED_AUTOMAP_STATE_FNV 0x669b1aa7U
#define EXPECTED_TOPOLOGY_FNV 0x3f321e43U
#define EXPECTED_LINE_COUNT 480U
#define EXPECTED_SPRITE_COUNT 344U
#define EXPECTED_EVENT_COUNT 93U
#define EXPECTED_RESULT_BYTES 20U
#define MAP_INTRO_LOAD_MAP_ID 1U

#define BASE_EFFECTS \
    (ESP_MAP_LEVEL_EXIT_EFFECT_ACCUMULATE_TIME | \
     ESP_MAP_LEVEL_EXIT_EFFECT_ACCUMULATE_MOVES | \
     ESP_MAP_LEVEL_EXIT_EFFECT_RESET_BERSERKER | \
     ESP_MAP_LEVEL_EXIT_EFFECT_CLEAR_FAMILIAR)

typedef struct Esp32LevelExitProbeState_s {
    int armed;
    int attempted;
    int done;
} Esp32LevelExitProbeState;

static Esp32LevelExitProbeState probeState;

static uint32_t hashByte(uint32_t hash, uint8_t value) {
    hash ^= value;
    return hash * 16777619U;
}

static uint32_t hashBytes(const void* data, uint32_t length) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (bytes == NULL && length != 0U) return 0U;
    for (i = 0U; i < length; ++i) hash = hashByte(hash, bytes[i]);
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

static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint8_t bitAt(const uint8_t* bits, uint32_t index) {
    return (uint8_t)((bits[index >> 3] >> (index & 7U)) & 1U);
}

static int statsIsZero(const EspMapLevelExitStats* stats) {
    EspMapLevelExitStats zero;
    if (stats == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(stats, &zero, sizeof(zero)) == 0;
}

static int legacyRuntimeIsClear(const Render_t* render) {
    return render != NULL && render->nodes == NULL && render->lines == NULL &&
           render->mapSprites == NULL && render->tileEvents == NULL &&
           render->mapByteCode == NULL && render->mapStringsIDs == NULL &&
           render->shapeData == NULL && render->mediaTexels == NULL &&
           render->ioBuffer == NULL;
}

static uint32_t playerStatsWitness(const Player_t* player) {
    uint32_t values[9];
    if (player == NULL) return 0U;
    values[0] = (uint32_t)player->time;
    values[1] = (uint32_t)player->totalTime;
    values[2] = (uint32_t)player->moves;
    values[3] = (uint32_t)player->totalMoves;
    values[4] = (uint32_t)player->completedLevels;
    values[5] = (uint32_t)player->foundSecretsLevels;
    values[6] = (uint32_t)player->killedMonstersLevels;
    values[7] = (uint32_t)player->berserkerTics;
    values[8] = (uint32_t)(uintptr_t)player->dogFamiliar;
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

static int referenceCounts(uint16_t* secretsFound,
                           uint16_t* secretsTotal,
                           uint16_t* monstersDead,
                           uint16_t* monstersTotal,
                           uint16_t* firstSecretLine) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspMapLineStateView* lines = EspMapLineState_view();
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    EspMapLine line;
    uint32_t i;
    uint16_t state;
    uint8_t type;

    if (secretsFound == NULL || secretsTotal == NULL || monstersDead == NULL ||
        monstersTotal == NULL || firstSecretLine == NULL || runtime == NULL ||
        lines == NULL || topology == NULL || lines->openBits == NULL ||
        topology->entityTypes == NULL || topology->linkStatesLE == NULL) return 0;

    *secretsFound = 0U;
    *secretsTotal = 0U;
    *monstersDead = 0U;
    *monstersTotal = 0U;
    *firstSecretLine = 0xffffU;

    for (i = 0U; i < runtime->lineCount; ++i) {
        if (!EspMapRuntime_getLine(i, &line)) return 0;
        if ((line.flags & ESP_MAP_LEVEL_EXIT_LINE_FLAG_SECRET) == 0U) continue;
        if (*firstSecretLine == 0xffffU) *firstSecretLine = (uint16_t)i;
        ++(*secretsTotal);
        if (bitAt(lines->openBits, i) != 0U) ++(*secretsFound);
    }

    for (i = 0U; i < topology->spriteCount; ++i) {
        type = topology->entityTypes[i];
        state = readLe16(topology->linkStatesLE + (i * 2U));
        if (type != ESP_MAP_ENTITY_TYPE_ENEMY ||
            (state & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) == 0U) continue;
        ++(*monstersTotal);
        if ((state & ESP_MAP_SPRITE_TOPOLOGY_ALIVE) == 0U) ++(*monstersDead);
    }
    return 1;
}

static int descriptorByIndex(uint32_t eventIndex,
                             EspMapEventDescriptor* descriptor) {
    EspMapEventRef ref;
    uint32_t value;
    if (descriptor == NULL || eventIndex >= EXPECTED_EVENT_COUNT ||
        !EspMapRuntime_getEvent(eventIndex, &value)) return 0;
    ref.index = (uint16_t)eventIndex;
    ref.tileIndex = (uint16_t)(value & ESP_MAP_EVENT_TILE_MASK);
    ref.value = value;
    return EspMapEvents_describe(&ref, descriptor);
}

static int proveShowSensitivity(const EspMapLevelExitStats* source,
                                uint32_t* outCommand,
                                uint32_t* outEvent,
                                uint32_t* outOffset,
                                uint32_t* outEnemyRemoved,
                                uint32_t* outAfterFNV,
                                uint32_t* outStatsFNV) {
    EspMapEventDescriptor descriptor;
    EspMapByteCode command;
    EspMapShowResult show;
    EspMapLevelExitStats mutated;
    const EspMapSpriteTopologyView* topology;
    uint32_t eventIndex;
    uint32_t offset;
    uint32_t enemyRemoved;
    uint16_t state;
    uint16_t order;
    uint8_t type;
    uint8_t subtype;
    uint16_t blockers[2];
    uint32_t i;

    if (source == NULL || outCommand == NULL || outEvent == NULL ||
        outOffset == NULL || outEnemyRemoved == NULL || outAfterFNV == NULL ||
        outStatsFNV == NULL) return 0;

    for (eventIndex = 0U; eventIndex < EXPECTED_EVENT_COUNT; ++eventIndex) {
        if (!descriptorByIndex(eventIndex, &descriptor)) return 0;
        for (offset = 0U; offset < descriptor.commandCount; ++offset) {
            if (!EspMapEvents_getCommand(&descriptor, offset, &command)) return 0;
            if (command.id != ESP_MAP_OPCODE_SHOW) continue;
            if (!EspMapSpriteTopology_resetMutableFromRuntime()) return 0;
            memset(&show, 0, sizeof(show));
            if (EspMapSpriteTopology_applyShow(&descriptor, offset, &show) !=
                ESP_MAP_SPRITE_TOPOLOGY_OK) return 0;
            if ((show.effectFlags & ESP_MAP_SHOW_EFFECT_DEFER_BLOCKER_GAMEPLAY) == 0U ||
                show.blockersRemoved == 0U || show.blockerNoops != 0U) continue;

            blockers[0] = show.blocker0SpriteIndex;
            blockers[1] = show.blocker1SpriteIndex;
            enemyRemoved = 0U;
            for (i = 0U; i < 2U; ++i) {
                if (blockers[i] == ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE) continue;
                if (i == 1U && blockers[1] == blockers[0]) continue;
                if (!EspMapSpriteTopology_getEntity(blockers[i], &type, &subtype,
                                                    &state, &order)) return 0;
                (void)subtype;
                (void)order;
                if (type == ESP_MAP_ENTITY_TYPE_ENEMY &&
                    (state & ESP_MAP_SPRITE_TOPOLOGY_ALIVE) == 0U) {
                    ++enemyRemoved;
                }
            }

            if (EspMapLevelExitStats_collect(MAP_INTRO_LOAD_MAP_ID, 1U,
                                             &mutated) !=
                    ESP_MAP_LEVEL_EXIT_STATS_OK ||
                mutated.secretsFound != source->secretsFound ||
                mutated.secretsTotal != source->secretsTotal ||
                mutated.monstersTotal != source->monstersTotal ||
                mutated.monstersDead !=
                    (uint16_t)(source->monstersDead + enemyRemoved)) {
                return 0;
            }
            topology = EspMapSpriteTopology_view();
            if (topology == NULL) return 0;
            *outCommand = (uint32_t)descriptor.firstCommandIndex + offset;
            *outEvent = eventIndex;
            *outOffset = offset;
            *outEnemyRemoved = enemyRemoved;
            *outAfterFNV = topology->stateFNV1a;
            *outStatsFNV = hashBytes(&mutated, sizeof(mutated));
            return 1;
        }
    }
    return 0;
}

void Esp32Map1LevelExitStatsProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

int Esp32Map1LevelExitStatsProbe_isDone(void) {
    return probeState.done;
}

void Esp32Map1LevelExitStatsProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspMapRuntimeView* runtime;
    const EspMapLineStateView* lineState;
    const EspMapLineTextureStateView* textureState;
    const EspMapScriptStateView* scriptState;
    const EspMapAutomapStateView* automapState;
    const EspMapStateView* mapState;
    const EspMapSpriteTopologyView* topology;
    EspMapLevelExitStats source;
    EspMapLevelExitStats noStats;
    EspMapLevelExitStats noCompletion;
    EspMapLevelExitStats toggled;
    EspMapLevelExitStats invalid;
    uint16_t refSecretsFound, refSecretsTotal, refMonstersDead, refMonstersTotal;
    uint16_t secretLine;
    uint8_t secretOpen;
    uint32_t sourceFNV;
    uint32_t noStatsFNV;
    uint32_t noCompletionFNV;
    uint32_t showCommand, showEvent, showOffset, showEnemyRemoved;
    uint32_t showAfterFNV, showStatsFNV;
    uint32_t lineInitialFNV, lineToggledFNV;
    uint32_t heapBefore, heapAfter, largestBefore, largestAfter;
    uint32_t frameBefore, frameAfter;
    uint32_t playerBefore, playerAfter, transitionBefore, transitionAfter;
    uint32_t arenaBefore, mapBefore, scriptBefore, automapBefore;
    uint32_t topologyBefore;
    uint32_t expectedEffects;
    uint32_t secretSensitivity = 0U;
    uint32_t elapsed;
    uint32_t started;

    if (probeState.done || probeState.attempted || doomRpg == NULL ||
        !Esp32Map1ShowHideFinalProbe_isDone()) return;
    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[MAPEXITSTATS] ARMED all MAP_INTRO opcode families owned; native level-exit stats snapshot starts on next loop service\n");
        return;
    }
    probeState.attempted = 1;

    printf("\n=== Doom RPG ESP32-native MAP_INTRO level-exit stats snapshot ===\n");
    printf("[MAPEXITSTATS] CONTRACT reproduce map-derived Player_addLevelStats semantics from native line+sprite owners into one 20B caller-owned value; no Player/Menu/Game/Render mutation, no PAK IO, no allocation\n");

    runtime = EspMapRuntime_view();
    lineState = EspMapLineState_view();
    textureState = EspMapLineTextureState_view();
    scriptState = EspMapScriptState_view();
    automapState = EspMapAutomapState_view();
    mapState = EspMapState_view();
    topology = EspMapSpriteTopology_view();
    if (runtime == NULL || lineState == NULL || textureState == NULL ||
        scriptState == NULL || automapState == NULL || mapState == NULL ||
        topology == NULL || runtime->lineCount != EXPECTED_LINE_COUNT ||
        runtime->mapSpriteCount != EXPECTED_SPRITE_COUNT ||
        runtime->eventCount != EXPECTED_EVENT_COUNT ||
        runtime->arenaFNV1a != EXPECTED_ARENA_FNV ||
        mapState->stateFNV1a != EXPECTED_MAP_STATE_FNV ||
        hashBytes(scriptState->storage, scriptState->storageBytes) != EXPECTED_SCRIPT_FNV ||
        lineState->stateFNV1a != EXPECTED_LINE_STATE_FNV ||
        textureState->stateFNV1a != EXPECTED_TEXTURE_STATE_FNV ||
        automapState->stateFNV1a != EXPECTED_AUTOMAP_STATE_FNV ||
        topology->stateFNV1a != EXPECTED_TOPOLOGY_FNV ||
        topology->enemyCount != 30U || sizeof(EspMapLevelExitStats) != EXPECTED_RESULT_BYTES ||
        EspAssetPack_isOpen() || !legacyRuntimeIsClear(doomRpg->render) ||
        doomRpg->game == NULL || doomRpg->player == NULL ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        doomRpg->doomCanvas == NULL || doomRpg->doomCanvas->state != ST_INTRO ||
        doomRpg->doomCanvas->storyPage != 3) {
        printf("[MAPEXITSTATS] FAILED unsafe precondition\n");
        return;
    }

    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    playerBefore = playerStatsWitness(doomRpg->player);
    transitionBefore = transitionWitness(doomRpg);
    arenaBefore = runtime->arenaFNV1a;
    mapBefore = mapState->stateFNV1a;
    scriptBefore = hashBytes(scriptState->storage, scriptState->storageBytes);
    automapBefore = automapState->stateFNV1a;
    topologyBefore = topology->stateFNV1a;
    lineInitialFNV = lineState->stateFNV1a;
    started = DoomRPG_GetUpTimeMS();

    if (EspMapLevelExitStats_collect(MAP_INTRO_LOAD_MAP_ID, 1U, &source) !=
            ESP_MAP_LEVEL_EXIT_STATS_OK ||
        !referenceCounts(&refSecretsFound, &refSecretsTotal,
                         &refMonstersDead, &refMonstersTotal, &secretLine) ||
        source.secretsFound != refSecretsFound ||
        source.secretsTotal != refSecretsTotal ||
        source.monstersDead != refMonstersDead ||
        source.monstersTotal != refMonstersTotal ||
        source.monstersTotal != topology->enemyCount ||
        source.loadMapId != MAP_INTRO_LOAD_MAP_ID || source.showStats != 1U ||
        source.completionLevelBit != 1U || source.markCompleted != 1U) {
        printf("[MAPEXITSTATS] FAILED source/reference mismatch\n");
        return;
    }

    expectedEffects = BASE_EFFECTS | ESP_MAP_LEVEL_EXIT_EFFECT_MARK_COMPLETED;
    if (source.markAllSecrets != 0U) {
        expectedEffects |= ESP_MAP_LEVEL_EXIT_EFFECT_MARK_ALL_SECRETS;
    }
    if (source.markAllMonsters != 0U) {
        expectedEffects |= ESP_MAP_LEVEL_EXIT_EFFECT_MARK_ALL_MONSTERS;
    }
    if (source.markAllSecrets != (uint8_t)(source.secretsFound == source.secretsTotal) ||
        source.markAllMonsters != (uint8_t)(source.monstersDead == source.monstersTotal) ||
        source.effectFlags != (uint8_t)expectedEffects) {
        printf("[MAPEXITSTATS] FAILED completion projection\n");
        return;
    }
    sourceFNV = hashBytes(&source, sizeof(source));

    if (EspMapLevelExitStats_collect(MAP_INTRO_LOAD_MAP_ID, 0U, &noStats) !=
            ESP_MAP_LEVEL_EXIT_STATS_OK ||
        noStats.completionLevelBit != 0U || noStats.markCompleted != 0U ||
        noStats.markAllSecrets != 0U || noStats.markAllMonsters != 0U ||
        noStats.effectFlags != BASE_EFFECTS ||
        noStats.secretsFound != source.secretsFound ||
        noStats.secretsTotal != source.secretsTotal ||
        noStats.monstersDead != source.monstersDead ||
        noStats.monstersTotal != source.monstersTotal) {
        printf("[MAPEXITSTATS] FAILED showStats=0 gate\n");
        return;
    }
    noStatsFNV = hashBytes(&noStats, sizeof(noStats));

    if (EspMapLevelExitStats_collect(ESP_MAP_LEVEL_EXIT_NO_COMPLETION_MAP_ID, 1U,
                                     &noCompletion) !=
            ESP_MAP_LEVEL_EXIT_STATS_OK ||
        noCompletion.completionLevelBit != 0U ||
        noCompletion.markCompleted != 0U || noCompletion.markAllSecrets != 0U ||
        noCompletion.markAllMonsters != 0U || noCompletion.effectFlags != BASE_EFFECTS) {
        printf("[MAPEXITSTATS] FAILED loadMapId=2 gate\n");
        return;
    }
    noCompletionFNV = hashBytes(&noCompletion, sizeof(noCompletion));

    if (!proveShowSensitivity(&source, &showCommand, &showEvent, &showOffset,
                              &showEnemyRemoved, &showAfterFNV, &showStatsFNV) ||
        !EspMapSpriteTopology_resetMutableFromRuntime()) {
        printf("[MAPEXITSTATS] FAILED SHOW/death sensitivity\n");
        return;
    }
    topology = EspMapSpriteTopology_view();
    if (topology == NULL || topology->stateFNV1a != topologyBefore) {
        printf("[MAPEXITSTATS] FAILED topology rollback\n");
        return;
    }

    if (secretLine != 0xffffU) {
        if (!EspMapLineState_getOpen(secretLine, &secretOpen) ||
            !EspMapLineState_setOpen(secretLine, (uint8_t)(secretOpen == 0U))) {
            printf("[MAPEXITSTATS] FAILED secret-line toggle\n");
            return;
        }
        lineState = EspMapLineState_view();
        if (lineState == NULL) return;
        lineToggledFNV = lineState->stateFNV1a;
        if (EspMapLevelExitStats_collect(MAP_INTRO_LOAD_MAP_ID, 1U, &toggled) !=
                ESP_MAP_LEVEL_EXIT_STATS_OK ||
            toggled.secretsTotal != source.secretsTotal ||
            toggled.monstersDead != source.monstersDead ||
            toggled.monstersTotal != source.monstersTotal ||
            toggled.secretsFound !=
                (uint16_t)(secretOpen != 0U ? source.secretsFound - 1U
                                            : source.secretsFound + 1U) ||
            !EspMapLineState_setOpen(secretLine, secretOpen)) {
            printf("[MAPEXITSTATS] FAILED secret sensitivity\n");
            return;
        }
        lineState = EspMapLineState_view();
        if (lineState == NULL || lineState->stateFNV1a != lineInitialFNV) {
            printf("[MAPEXITSTATS] FAILED line rollback\n");
            return;
        }
        secretSensitivity = 1U;
    }
    else {
        lineToggledFNV = lineInitialFNV;
        secretSensitivity = 2U;
    }

    memset(&invalid, 0xa5, sizeof(invalid));
    if (EspMapLevelExitStats_collect(0U, 1U, &invalid) !=
            ESP_MAP_LEVEL_EXIT_STATS_INVALID || !statsIsZero(&invalid)) {
        printf("[MAPEXITSTATS] FAILED mapId0 failclosed\n");
        return;
    }
    memset(&invalid, 0xa5, sizeof(invalid));
    if (EspMapLevelExitStats_collect(33U, 1U, &invalid) !=
            ESP_MAP_LEVEL_EXIT_STATS_INVALID || !statsIsZero(&invalid)) {
        printf("[MAPEXITSTATS] FAILED mapId33 failclosed\n");
        return;
    }
    memset(&invalid, 0xa5, sizeof(invalid));
    if (EspMapLevelExitStats_collect(1U, 2U, &invalid) !=
            ESP_MAP_LEVEL_EXIT_STATS_INVALID || !statsIsZero(&invalid) ||
        EspMapLevelExitStats_collect(1U, 1U, NULL) !=
            ESP_MAP_LEVEL_EXIT_STATS_INVALID) {
        printf("[MAPEXITSTATS] FAILED argument failclosed\n");
        return;
    }

    elapsed = DoomRPG_GetUpTimeMS() - started;
    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfter = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameAfter = framebufferHash();
    playerAfter = playerStatsWitness(doomRpg->player);
    transitionAfter = transitionWitness(doomRpg);
    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    automapState = EspMapAutomapState_view();
    lineState = EspMapLineState_view();
    topology = EspMapSpriteTopology_view();

    if (heapAfter != heapBefore || largestAfter != largestBefore ||
        frameAfter != frameBefore || playerAfter != playerBefore ||
        transitionAfter != transitionBefore || runtime == NULL ||
        runtime->arenaFNV1a != arenaBefore || runtime->arenaFNV1a != EXPECTED_ARENA_FNV ||
        mapState == NULL || mapState->stateFNV1a != mapBefore ||
        mapState->stateFNV1a != EXPECTED_MAP_STATE_FNV || scriptState == NULL ||
        hashBytes(scriptState->storage, scriptState->storageBytes) != scriptBefore ||
        automapState == NULL || automapState->stateFNV1a != automapBefore ||
        lineState == NULL || lineState->stateFNV1a != lineInitialFNV ||
        topology == NULL || topology->stateFNV1a != topologyBefore ||
        EspAssetPack_isOpen() || !legacyRuntimeIsClear(doomRpg->render) ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0) {
        printf("[MAPEXITSTATS] FAILED integrity regression\n");
        return;
    }

    printf("[MAPEXITSTATS] READY resultBytes=%u loadMapId=%u showStats=%u secrets=%u/%u monsters=%u/%u markCompleted=%u markAllSecrets=%u markAllMonsters=%u completionBit=%08x effects=%02x statsFNV=%08x elapsed=%ums\n",
           (unsigned int)sizeof(EspMapLevelExitStats),
           (unsigned int)source.loadMapId, (unsigned int)source.showStats,
           (unsigned int)source.secretsFound, (unsigned int)source.secretsTotal,
           (unsigned int)source.monstersDead, (unsigned int)source.monstersTotal,
           (unsigned int)source.markCompleted,
           (unsigned int)source.markAllSecrets,
           (unsigned int)source.markAllMonsters,
           (unsigned int)source.completionLevelBit,
           (unsigned int)source.effectFlags,
           (unsigned int)sourceFNV, (unsigned int)elapsed);
    printf("[MAPEXITSTATS] GATES noStatsFNV=%08x noCompletionMapFNV=%08x baseEffects=%02x showStats0=yes loadMapId2=yes equalityOnZero=legacy\n",
           (unsigned int)noStatsFNV, (unsigned int)noCompletionFNV,
           (unsigned int)BASE_EFFECTS);
    printf("[MAPEXITSTATS] SHOWSENS cmd=%u event=%u off=%u enemyBlockersRemoved=%u topologyFNV=%08x->%08x statsFNV=%08x rollback=%08x\n",
           (unsigned int)showCommand, (unsigned int)showEvent,
           (unsigned int)showOffset, (unsigned int)showEnemyRemoved,
           (unsigned int)topologyBefore, (unsigned int)showAfterFNV,
           (unsigned int)showStatsFNV, (unsigned int)topology->stateFNV1a);
    printf("[MAPEXITSTATS] SECRETSENS line=%u initialOpen=%u proof=%u lineFNV=%08x->%08x->%08x\n",
           secretLine != 0xffffU ? (unsigned int)secretLine : 65535U,
           secretLine != 0xffffU ? (unsigned int)secretOpen : 0U,
           (unsigned int)secretSensitivity,
           (unsigned int)lineInitialFNV, (unsigned int)lineToggledFNV,
           (unsigned int)lineState->stateFNV1a);
    printf("[MAPEXITSTATS] FAILCLOSED mapId0=1 mapId33=1 showStats2=1 nullResult=1 stateAtomic=yes\n");
    printf("[MAPEXITSTATS] RAM heap8=%u->%u delta=0 largest8=%u->%u delta=0 persistentHeapBytes=0 frameFNV=%08x->%08x arenaFNV=%08x->%08x lineFNV=%08x->%08x topologyFNV=%08x->%08x\n",
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter,
           (unsigned int)frameBefore, (unsigned int)frameAfter,
           (unsigned int)arenaBefore, (unsigned int)runtime->arenaFNV1a,
           (unsigned int)lineInitialFNV, (unsigned int)lineState->stateFNV1a,
           (unsigned int)topologyBefore, (unsigned int)topology->stateFNV1a);
    printf("[MAPEXITSTATS] LEGACY playerStatsFNV=%08x->%08x transitionFNV=%08x->%08x legacyRuntimeClear=yes Player_addLevelStatsCalled=no menuMutation=no transitionTriggered=no\n",
           (unsigned int)playerBefore, (unsigned int)playerAfter,
           (unsigned int)transitionBefore, (unsigned int)transitionAfter);
    printf("[MAPEXITSTATS] PARK state=%d page=%d nativeExitStats=yes resultBytes=%u persistentBytes=0 allMapIntroOpcodeFamiliesOwned=yes playerMutation=no menuMutation=no worldRestored=yes entities=%d monsters=%d noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage,
           (unsigned int)sizeof(EspMapLevelExitStats),
           doomRpg->game->numEntities, doomRpg->game->numMonsters);

    probeState.done = 1;
}
