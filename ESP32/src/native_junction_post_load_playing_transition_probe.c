#include <SDL.h>
#include "DoomRPG.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "DoomCanvas.h"
#include "Game.h"
#include "Hud.h"
#include "ParticleSystem.h"
#include "Player.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_post_load_playing_transition_state.h"
#include "esp_post_load_view_invalidation_state.h"
#include "native_junction_post_load_playing_transition_probe.h"
#include "native_junction_post_load_view_invalidation_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_PLAYING_BYTES 12U
#define EXPECTED_VIEW_BYTES 4U
#define EXPECTED_VIEW_FNV 0x4561c3c1U
#define EXPECTED_SNAPSHOT_FNV 0xbb714d80U
#define EXPECTED_RUNTIME_FNV 0xbc432a0fU
#define EXPECTED_MAP_FNV 0x8dba0bb4U
#define EXPECTED_SCRIPT_FNV 0xbc9b18ffU
#define EXPECTED_LINE_FNV 0x3658710dU
#define EXPECTED_TEXTURE_FNV 0x537319adU
#define EXPECTED_AUTOMAP_FNV 0xb699bd75U
#define EXPECTED_TOPOLOGY_FNV 0xd6e8df7dU

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

static int stateIsZero(const EspPostLoadPlayingTransitionState* state) {
    EspPostLoadPlayingTransitionState zero;
    if (state == NULL) return 0;
    memset(&zero, 0, sizeof(zero));
    return memcmp(state, &zero, sizeof(zero)) == 0;
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

static uint32_t gameWitness(const Game_t* game) {
    uint32_t v[18];
    if (game == NULL) return 0U;
    v[0] = (uint32_t)game->spawnParam;
    v[1] = (uint32_t)game->isLoaded;
    v[2] = (uint32_t)game->isSaved;
    v[3] = (uint32_t)game->activeLoadType;
    v[4] = (uint32_t)game->numEntities;
    v[5] = (uint32_t)game->numMonsters;
    v[6] = (uint32_t)game->skipAdvanceTurn;
    v[7] = (uint32_t)game->f658b;
    v[8] = (uint32_t)game->waitTime;
    v[9] = (uint32_t)game->tileEvent;
    v[10] = (uint32_t)game->tileEventIndex;
    v[11] = (uint32_t)game->tileEventFlags;
    v[12] = (uint32_t)game->saveTileEvent;
    v[13] = (uint32_t)game->newDestX;
    v[14] = (uint32_t)game->newDestY;
    v[15] = (uint32_t)game->newAngle;
    v[16] = hashBytes(game->newMapName, (uint32_t)sizeof(game->newMapName));
    v[17] = (uint32_t)game->monstersTurn;
    return hashBytes(v, sizeof(v));
}

static uint32_t playerWitness(const Player_t* player) {
    uint32_t v[13];
    if (player == NULL) return 0U;
    v[0] = (uint32_t)player->keys;
    v[1] = (uint32_t)player->moves;
    v[2] = (uint32_t)player->xpGained;
    v[3] = (uint32_t)player->berserkerTics;
    v[4] = (uint32_t)player->disabledWeapons;
    v[5] = (uint32_t)player->weapons;
    v[6] = (uint32_t)player->weapon;
    v[7] = (uint32_t)player->currentXP;
    v[8] = (uint32_t)player->level;
    v[9] = (uint32_t)player->credits;
    v[10] = (uint32_t)player->completedLevels;
    v[11] = (uint32_t)(player->dogFamiliar != NULL);
    v[12] = (uint32_t)(player->facingEntity != NULL);
    return hashBytes(v, sizeof(v));
}

static uint32_t hudWitness(const Hud_t* hud) {
    uint32_t hash = 2166136261U;
    uint32_t i;
    const uint8_t* p;

    if (hud == NULL) return 0U;
    hash ^= (uint32_t)hud->msgCount; hash *= 16777619U;
    hash ^= (uint32_t)hud->msgTime; hash *= 16777619U;
    hash ^= (uint32_t)hud->msgDuration; hash *= 16777619U;
    hash ^= (uint32_t)hud->isUpdate; hash *= 16777619U;
    hash ^= (uint32_t)(uintptr_t)hud->statBarMessage; hash *= 16777619U;
    p = (const uint8_t*)hud->messages;
    for (i = 0U; i < (uint32_t)sizeof(hud->messages); ++i) {
        hash ^= p[i]; hash *= 16777619U;
    }
    p = (const uint8_t*)hud->logMessage;
    for (i = 0U; i < (uint32_t)sizeof(hud->logMessage); ++i) {
        hash ^= p[i]; hash *= 16777619U;
    }
    return hash;
}

static uint32_t canvasWitness(const DoomCanvas_t* canvas) {
    uint32_t v[24];
    uint32_t hash;
    if (canvas == NULL) return 0U;
    v[0] = (uint32_t)canvas->viewX;
    v[1] = (uint32_t)canvas->viewY;
    v[2] = (uint32_t)canvas->viewZ;
    v[3] = (uint32_t)canvas->viewAngle;
    v[4] = (uint32_t)canvas->destX;
    v[5] = (uint32_t)canvas->destY;
    v[6] = (uint32_t)canvas->destAngle;
    v[7] = (uint32_t)canvas->viewSin;
    v[8] = (uint32_t)canvas->viewCos;
    v[9] = (uint32_t)canvas->viewStepX;
    v[10] = (uint32_t)canvas->viewStepY;
    v[11] = (uint32_t)canvas->loadType;
    v[12] = (uint32_t)canvas->state;
    v[13] = (uint32_t)canvas->storyPage;
    v[14] = (uint32_t)canvas->loadMapID;
    v[15] = (uint32_t)canvas->numEvents;
    v[16] = (uint32_t)canvas->isUpdateView;
    v[17] = (uint32_t)canvas->idleTime;
    v[18] = (uint32_t)canvas->time;
    v[19] = (uint32_t)canvas->restoreSoftKeys;
    v[20] = (uint32_t)canvas->skipCheckState;
    v[21] = (uint32_t)canvas->displaySoftKeys;
    v[22] = hashBytes(canvas->softKeyLeft, (uint32_t)sizeof(canvas->softKeyLeft));
    v[23] = hashBytes(canvas->softKeyRight, (uint32_t)sizeof(canvas->softKeyRight));
    hash = hashBytes(v, sizeof(v));
    return hash;
}

static uint32_t eventQueueWitness(const DoomCanvas_t* canvas) {
    uint32_t hash;
    if (canvas == NULL) return 0U;
    hash = hashBytes(canvas->events, (uint32_t)sizeof(canvas->events));
    hash ^= (uint32_t)canvas->numEvents;
    hash *= 16777619U;
    return hash;
}

static uint32_t renderWitness(const Render_t* render) {
    uint32_t v[8];
    if (render == NULL) return 0U;
    v[0] = (uint32_t)render->sinTable[64];
    v[1] = (uint32_t)render->sinTable[128];
    v[2] = (uint32_t)render->viewZOld;
    v[3] = (uint32_t)render->numMapSprites;
    v[4] = (uint32_t)render->mapStringCount;
    v[5] = (uint32_t)render->viewX;
    v[6] = (uint32_t)render->viewY;
    v[7] = (uint32_t)render->viewAngle;
    return hashBytes(v, sizeof(v));
}

static uint32_t particleWitness(const ParticleSystem_t* particles) {
    return particles != NULL
               ? hashBytes(particles, (uint32_t)sizeof(*particles))
               : 0U;
}

static int legacyRuntimeIsClear(const Render_t* render) {
    return render != NULL && render->nodes == NULL && render->lines == NULL &&
           render->mapSprites == NULL && render->tileEvents == NULL &&
           render->mapByteCode == NULL && render->mapStringsIDs == NULL &&
           render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL && render->mediaSpriteIds == NULL &&
           render->mapTextureTexels == NULL && render->mapSpriteTexels == NULL &&
           render->shapeData == NULL && render->mediaTexels == NULL &&
           render->ioBuffer == NULL;
}

static int residentCanonical(const EspMapResidentSnapshot* s) {
    return s != NULL && sizeof(*s) == 96U &&
           s->totalPayloadBytes == 10410U &&
           s->runtimeArenaBytes == 8867U && s->mapStateBytes == 1024U &&
           s->scriptStateBytes == 73U && s->lineStateBytes == 52U &&
           s->textureStateBytes == 26U && s->automapStateBytes == 32U &&
           s->topologyBytes == 336U &&
           s->runtimeFNV1a == EXPECTED_RUNTIME_FNV &&
           s->mapStateFNV1a == EXPECTED_MAP_FNV &&
           s->scriptStateFNV1a == EXPECTED_SCRIPT_FNV &&
           s->lineStateFNV1a == EXPECTED_LINE_FNV &&
           s->textureStateFNV1a == EXPECTED_TEXTURE_FNV &&
           s->automapStateFNV1a == EXPECTED_AUTOMAP_FNV &&
           s->topologyFNV1a == EXPECTED_TOPOLOGY_FNV &&
           s->entityCount == 30U && s->enemyCount == 0U &&
           s->destructibleCount == 3U &&
           hashBytes(s, sizeof(*s)) == EXPECTED_SNAPSHOT_FNV;
}

static int viewInvalidationCanonical(void) {
    const EspPostLoadViewInvalidationState* state =
        EspPostLoadViewInvalidation_view();
    return state != NULL && sizeof(*state) == EXPECTED_VIEW_BYTES &&
           hashBytes(state, sizeof(*state)) == EXPECTED_VIEW_FNV &&
           state->isUpdateViewBefore == 1U && state->isUpdateViewAfter == 1U &&
           state->targetMapId == 9U && state->active == 1U;
}

static int poolIndex(const ParticleSystem_t* ps,
                     const ParticleNode_t* node,
                     uint32_t* outIndex) {
    uintptr_t first, end, value, delta;
    if (ps == NULL || node == NULL || outIndex == NULL) return 0;
    first = (uintptr_t)&ps->nodeListC[0];
    end = (uintptr_t)&ps->nodeListC[64];
    value = (uintptr_t)node;
    if (value < first || value >= end) return 0;
    delta = value - first;
    if ((delta % sizeof(ParticleNode_t)) != 0U) return 0;
    *outIndex = (uint32_t)(delta / sizeof(ParticleNode_t));
    return *outIndex < 64U;
}

static int markList(const ParticleSystem_t* ps,
                    const ParticleNode_t* sentinel,
                    uint64_t* seen,
                    int* outCount) {
    const ParticleNode_t* node;
    int count = 0;
    uint32_t index;
    uint64_t bit;

    if (ps == NULL || sentinel == NULL || seen == NULL || outCount == NULL ||
        sentinel->next == NULL || sentinel->prev == NULL) return 0;

    node = sentinel->next;
    while (node != sentinel) {
        if (++count > 64 || node->next == NULL || node->prev == NULL ||
            node->prev->next != node || node->next->prev != node ||
            !poolIndex(ps, node, &index)) return 0;
        bit = ((uint64_t)1U) << index;
        if ((*seen & bit) != 0U) return 0;
        *seen |= bit;
        node = node->next;
    }

    if (sentinel->next->prev != sentinel || sentinel->prev->next != sentinel)
        return 0;
    *outCount = count;
    return 1;
}

static int particleListsCanonical(const ParticleSystem_t* ps,
                                  int* activeCount,
                                  int* freeCount) {
    uint64_t seen = 0U;
    int active = 0, freeNodes = 0;

    if (ps == NULL || activeCount == NULL || freeCount == NULL ||
        ps->particleCount < 0 || ps->particleCount > 64) return 0;
    if (!markList(ps, &ps->nodeListA, &seen, &active)) return 0;
    if (!markList(ps, &ps->nodeListB, &seen, &freeNodes)) return 0;
    if (active + freeNodes != 64 || active != ps->particleCount ||
        seen != UINT64_MAX) return 0;

    *activeCount = active;
    *freeCount = freeNodes;
    return 1;
}

void Esp32JunctionPostLoadPlayingTransitionProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
    EspPostLoadPlayingTransition_reset();
}

int Esp32JunctionPostLoadPlayingTransitionProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionPostLoadPlayingTransitionProbe_service(
    struct DoomRPG_s* doomRpg) {
    EspMapResidentSnapshot residentBefore, residentAfter;
    EspPostLoadViewInvalidationState badView;
    EspPostLoadPlayingTransitionState scratch, prepared, beforeRepeat;
    const EspPostLoadPlayingTransitionState* live;
    ParticleSystem_t* particles;
    uint32_t heapBefore, heapAfter, largestBefore, largestAfter;
    uint32_t frameBefore, frameAfter, gameBefore, gameAfter;
    uint32_t playerBefore, playerAfter, hudBefore, hudAfter;
    uint32_t canvasBefore, canvasAfter, renderBefore, renderAfter;
    uint32_t eventQueueBefore, eventQueueAfter;
    uint32_t particleBefore, particleAfter;
    uint32_t viewBefore, viewAfter;
    uint8_t monstersTurnBefore, displaySoftKeysBefore;
    uint8_t restoreSoftKeysBefore, skipCheckStateBefore;
    int activeBefore, freeBefore, activeAfter, freeAfter;
    int nullViewGate, nullOutputGate, inactiveViewGate, mapGate;
    int invalidMonstersGate, invalidDisplayGate, invalidRestoreGate;
    int invalidSkipGate, prepareAtomic, postActivePrepareGate;
    int repeatGate, repeatAtomic;
    uint8_t expectedSoftKeyIntent, expectedSoftKeyVisible;
    EspPostLoadPlayingTransitionStatus status;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionPostLoadViewInvalidationProbe_isDone()) return;

    if (!probeState.armed) {
        probeState.armed = 1;
        printf("[JUNCTIONPLAYINGPROBE] ARMED post-load view invalidation complete; native ST_INTRO -> ST_PLAYING transition semantics start on next loop service\n");
        return;
    }

    probeState.attempted = 1;
    printf("\n=== Doom RPG ESP32-native Junction ST_PLAYING transition ===\n");
    printf("[JUNCTIONPLAYINGPROBE] CONTRACT recover the full ST_INTRO->ST_PLAYING semantics of legacy DoomCanvas_setState(ST_PLAYING) after hardware-proven view invalidation: own state 9->3, restoreSoftKeys result, skipCheckState=true and conditional Menu/Map soft-key intent in one 12B pointer-free state; do not call legacy DoomCanvas_setState/drawSoftKeys, do not mutate legacy Game/DoomCanvas/Hud/Render/ParticleSystem, do not render/present, keep idleTime deferred and do not allocate\n");

    if (doomRpg == NULL || doomRpg->doomCanvas == NULL || doomRpg->game == NULL ||
        doomRpg->render == NULL || doomRpg->player == NULL || doomRpg->hud == NULL ||
        doomRpg->particleSystem == NULL ||
        doomRpg->doomCanvas->state != ST_INTRO ||
        doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->doomCanvas->numEvents != 0 ||
        doomRpg->doomCanvas->isUpdateView != true ||
        doomRpg->doomCanvas->displaySoftKeys > 1 ||
        doomRpg->doomCanvas->restoreSoftKeys > 1 ||
        doomRpg->doomCanvas->skipCheckState > 1 ||
        doomRpg->game->monstersTurn > 1 ||
        doomRpg->particleSystem->particleCount != 0 ||
        doomRpg->game->numEntities != 0 || doomRpg->game->numMonsters != 0 ||
        EspAssetPack_isOpen() || !legacyRuntimeIsClear(doomRpg->render) ||
        sizeof(EspPostLoadPlayingTransitionState) != EXPECTED_PLAYING_BYTES ||
        !viewInvalidationCanonical() ||
        !EspMapResidentLifecycle_capture(&residentBefore) ||
        !residentCanonical(&residentBefore)) {
        printf("[JUNCTIONPLAYINGPROBE] FAILED unsafe view boundary state=%d monstersTurn=%d displaySoftKeys=%d restoreSoftKeys=%d skipCheckState=%d\n",
               doomRpg != NULL && doomRpg->doomCanvas != NULL
                   ? doomRpg->doomCanvas->state : -1,
               doomRpg != NULL && doomRpg->game != NULL
                   ? doomRpg->game->monstersTurn : -1,
               doomRpg != NULL && doomRpg->doomCanvas != NULL
                   ? doomRpg->doomCanvas->displaySoftKeys : -1,
               doomRpg != NULL && doomRpg->doomCanvas != NULL
                   ? doomRpg->doomCanvas->restoreSoftKeys : -1,
               doomRpg != NULL && doomRpg->doomCanvas != NULL
                   ? doomRpg->doomCanvas->skipCheckState : -1);
        return;
    }

    particles = doomRpg->particleSystem;
    if (!particleListsCanonical(particles, &activeBefore, &freeBefore) ||
        activeBefore != 0 || freeBefore != 64) {
        printf("[JUNCTIONPLAYINGPROBE] FAILED particle topology not canonical active=%d free=%d\n",
               activeBefore, freeBefore);
        return;
    }

    monstersTurnBefore = (uint8_t)doomRpg->game->monstersTurn;
    displaySoftKeysBefore = (uint8_t)doomRpg->doomCanvas->displaySoftKeys;
    restoreSoftKeysBefore = (uint8_t)doomRpg->doomCanvas->restoreSoftKeys;
    skipCheckStateBefore = (uint8_t)doomRpg->doomCanvas->skipCheckState;
    expectedSoftKeyIntent = monstersTurnBefore == 0U
                                ? ESP_POST_LOAD_PLAYING_SOFTKEY_MENU_MAP
                                : ESP_POST_LOAD_PLAYING_SOFTKEY_NONE;
    expectedSoftKeyVisible =
        (monstersTurnBefore == 0U && displaySoftKeysBefore != 0U) ? 1U : 0U;

    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestBefore =
        (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameBefore = framebufferHash();
    gameBefore = gameWitness(doomRpg->game);
    playerBefore = playerWitness(doomRpg->player);
    hudBefore = hudWitness(doomRpg->hud);
    canvasBefore = canvasWitness(doomRpg->doomCanvas);
    renderBefore = renderWitness(doomRpg->render);
    eventQueueBefore = eventQueueWitness(doomRpg->doomCanvas);
    particleBefore = particleWitness(particles);
    viewBefore = hashBytes(EspPostLoadViewInvalidation_view(),
                           sizeof(EspPostLoadViewInvalidationState));

    memset(&scratch, 0xa5, sizeof(scratch));
    nullViewGate =
        EspPostLoadPlayingTransition_prepare(
            NULL, monstersTurnBefore, displaySoftKeysBefore,
            restoreSoftKeysBefore, skipCheckStateBefore, &scratch) ==
            ESP_POST_LOAD_PLAYING_TRANSITION_INVALID &&
        stateIsZero(&scratch);

    nullOutputGate =
        EspPostLoadPlayingTransition_prepare(
            EspPostLoadViewInvalidation_view(), monstersTurnBefore,
            displaySoftKeysBefore, restoreSoftKeysBefore, skipCheckStateBefore,
            NULL) == ESP_POST_LOAD_PLAYING_TRANSITION_INVALID;

    badView = *EspPostLoadViewInvalidation_view();
    badView.active = 0U;
    memset(&scratch, 0xa5, sizeof(scratch));
    inactiveViewGate =
        EspPostLoadPlayingTransition_prepare(
            &badView, monstersTurnBefore, displaySoftKeysBefore,
            restoreSoftKeysBefore, skipCheckStateBefore, &scratch) ==
            ESP_POST_LOAD_PLAYING_TRANSITION_VIEW_INVALID &&
        stateIsZero(&scratch);

    badView = *EspPostLoadViewInvalidation_view();
    badView.targetMapId = 8U;
    memset(&scratch, 0xa5, sizeof(scratch));
    mapGate = EspPostLoadPlayingTransition_prepare(
                  &badView, monstersTurnBefore, displaySoftKeysBefore,
                  restoreSoftKeysBefore, skipCheckStateBefore, &scratch) ==
                  ESP_POST_LOAD_PLAYING_TRANSITION_VIEW_INVALID &&
              stateIsZero(&scratch);

    memset(&scratch, 0xa5, sizeof(scratch));
    invalidMonstersGate =
        EspPostLoadPlayingTransition_prepare(
            EspPostLoadViewInvalidation_view(), 2U, displaySoftKeysBefore,
            restoreSoftKeysBefore, skipCheckStateBefore, &scratch) ==
            ESP_POST_LOAD_PLAYING_TRANSITION_UNSUPPORTED_CONTEXT &&
        stateIsZero(&scratch);

    memset(&scratch, 0xa5, sizeof(scratch));
    invalidDisplayGate =
        EspPostLoadPlayingTransition_prepare(
            EspPostLoadViewInvalidation_view(), monstersTurnBefore, 2U,
            restoreSoftKeysBefore, skipCheckStateBefore, &scratch) ==
            ESP_POST_LOAD_PLAYING_TRANSITION_UNSUPPORTED_CONTEXT &&
        stateIsZero(&scratch);

    memset(&scratch, 0xa5, sizeof(scratch));
    invalidRestoreGate =
        EspPostLoadPlayingTransition_prepare(
            EspPostLoadViewInvalidation_view(), monstersTurnBefore,
            displaySoftKeysBefore, 2U, skipCheckStateBefore, &scratch) ==
            ESP_POST_LOAD_PLAYING_TRANSITION_UNSUPPORTED_CONTEXT &&
        stateIsZero(&scratch);

    memset(&scratch, 0xa5, sizeof(scratch));
    invalidSkipGate =
        EspPostLoadPlayingTransition_prepare(
            EspPostLoadViewInvalidation_view(), monstersTurnBefore,
            displaySoftKeysBefore, restoreSoftKeysBefore, 2U, &scratch) ==
            ESP_POST_LOAD_PLAYING_TRANSITION_UNSUPPORTED_CONTEXT &&
        stateIsZero(&scratch);

    memset(&prepared, 0xa5, sizeof(prepared));
    status = EspPostLoadPlayingTransition_prepare(
        EspPostLoadViewInvalidation_view(), monstersTurnBefore,
        displaySoftKeysBefore, restoreSoftKeysBefore, skipCheckStateBefore,
        &prepared);
    prepareAtomic =
        status == ESP_POST_LOAD_PLAYING_TRANSITION_OK &&
        !EspPostLoadPlayingTransition_isReady() &&
        prepared.stateBefore == ESP_POST_LOAD_PLAYING_STATE_INTRO &&
        prepared.stateAfter == ESP_POST_LOAD_PLAYING_STATE_PLAYING &&
        prepared.monstersTurnBefore == monstersTurnBefore &&
        prepared.displaySoftKeysBefore == displaySoftKeysBefore &&
        prepared.restoreSoftKeysBefore == restoreSoftKeysBefore &&
        prepared.restoreSoftKeysAfter == expectedSoftKeyVisible &&
        prepared.skipCheckStateBefore == skipCheckStateBefore &&
        prepared.skipCheckStateAfter == 1U &&
        prepared.softKeyIntent == expectedSoftKeyIntent &&
        prepared.softKeyPresentationDeferred == expectedSoftKeyVisible &&
        prepared.targetMapId == 9U && prepared.active == 1U;

    if (!nullViewGate || !nullOutputGate || !inactiveViewGate || !mapGate ||
        !invalidMonstersGate || !invalidDisplayGate || !invalidRestoreGate ||
        !invalidSkipGate || !prepareAtomic) {
        printf("[JUNCTIONPLAYINGPROBE] FAILED fail-closed/preparation gates nullView=%d nullOutput=%d inactive=%d map=%d invalidMonsters=%d invalidDisplay=%d invalidRestore=%d invalidSkip=%d prepareAtomic=%d\n",
               nullViewGate, nullOutputGate, inactiveViewGate, mapGate,
               invalidMonstersGate, invalidDisplayGate, invalidRestoreGate,
               invalidSkipGate, prepareAtomic);
        return;
    }

    status = EspPostLoadPlayingTransition_route(
        monstersTurnBefore, displaySoftKeysBefore, restoreSoftKeysBefore,
        skipCheckStateBefore);
    live = EspPostLoadPlayingTransition_view();
    if (status != ESP_POST_LOAD_PLAYING_TRANSITION_OK || live == NULL ||
        memcmp(live, &prepared, sizeof(prepared)) != 0) {
        printf("[JUNCTIONPLAYINGPROBE] FAILED route status=%d live=%p\n",
               (int)status, (const void*)live);
        return;
    }

    beforeRepeat = *live;
    memset(&scratch, 0xa5, sizeof(scratch));
    postActivePrepareGate =
        EspPostLoadPlayingTransition_prepare(
            EspPostLoadViewInvalidation_view(), monstersTurnBefore,
            displaySoftKeysBefore, restoreSoftKeysBefore, skipCheckStateBefore,
            &scratch) == ESP_POST_LOAD_PLAYING_TRANSITION_ALREADY_ACTIVE &&
        stateIsZero(&scratch);
    repeatGate =
        EspPostLoadPlayingTransition_route(
            monstersTurnBefore, displaySoftKeysBefore, restoreSoftKeysBefore,
            skipCheckStateBefore) == ESP_POST_LOAD_PLAYING_TRANSITION_ALREADY_ACTIVE;
    live = EspPostLoadPlayingTransition_view();
    repeatAtomic = live != NULL &&
                   memcmp(live, &beforeRepeat, sizeof(beforeRepeat)) == 0;

    if (!postActivePrepareGate || !repeatGate || !repeatAtomic) {
        printf("[JUNCTIONPLAYINGPROBE] FAILED repeat gates postActive=%d repeat=%d atomic=%d\n",
               postActivePrepareGate, repeatGate, repeatAtomic);
        return;
    }

    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largestAfter =
        (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    frameAfter = framebufferHash();
    gameAfter = gameWitness(doomRpg->game);
    playerAfter = playerWitness(doomRpg->player);
    hudAfter = hudWitness(doomRpg->hud);
    canvasAfter = canvasWitness(doomRpg->doomCanvas);
    renderAfter = renderWitness(doomRpg->render);
    eventQueueAfter = eventQueueWitness(doomRpg->doomCanvas);
    particleAfter = particleWitness(particles);
    viewAfter = hashBytes(EspPostLoadViewInvalidation_view(),
                          sizeof(EspPostLoadViewInvalidationState));

    if (!EspMapResidentLifecycle_capture(&residentAfter) ||
        !residentCanonical(&residentAfter) ||
        hashBytes(&residentBefore, sizeof(residentBefore)) !=
            hashBytes(&residentAfter, sizeof(residentAfter)) ||
        !particleListsCanonical(particles, &activeAfter, &freeAfter) ||
        activeAfter != 0 || freeAfter != 64 ||
        heapBefore != heapAfter || largestBefore != largestAfter ||
        frameBefore != frameAfter || gameBefore != gameAfter ||
        playerBefore != playerAfter || hudBefore != hudAfter ||
        canvasBefore != canvasAfter || renderBefore != renderAfter ||
        eventQueueBefore != eventQueueAfter || particleBefore != particleAfter ||
        viewBefore != viewAfter || viewAfter != EXPECTED_VIEW_FNV ||
        doomRpg->doomCanvas->state != ST_INTRO ||
        doomRpg->doomCanvas->storyPage != 3 ||
        doomRpg->doomCanvas->numEvents != 0 ||
        doomRpg->doomCanvas->isUpdateView != true ||
        doomRpg->doomCanvas->displaySoftKeys != displaySoftKeysBefore ||
        doomRpg->doomCanvas->restoreSoftKeys != restoreSoftKeysBefore ||
        doomRpg->doomCanvas->skipCheckState != skipCheckStateBefore ||
        doomRpg->game->monstersTurn != monstersTurnBefore ||
        particles->particleCount != 0 || doomRpg->game->numEntities != 0 ||
        doomRpg->game->numMonsters != 0 || EspAssetPack_isOpen() ||
        !legacyRuntimeIsClear(doomRpg->render)) {
        printf("[JUNCTIONPLAYINGPROBE] FAILED integrity after native semantic transition\n");
        return;
    }

    printf("[JUNCTIONPLAYING] READY stateBytes=%u stateFNV=%08x state=%u->%u monstersTurn=%u displaySoftKeys=%u restoreSoftKeys=%u->%u skipCheckState=%u->%u softKeyIntent=%u softKeyPresentationDeferred=%u targetMap=%u active=%u\n",
           (unsigned)sizeof(*live),
           (unsigned)hashBytes(live, sizeof(*live)),
           (unsigned)live->stateBefore, (unsigned)live->stateAfter,
           (unsigned)live->monstersTurnBefore,
           (unsigned)live->displaySoftKeysBefore,
           (unsigned)live->restoreSoftKeysBefore,
           (unsigned)live->restoreSoftKeysAfter,
           (unsigned)live->skipCheckStateBefore,
           (unsigned)live->skipCheckStateAfter,
           (unsigned)live->softKeyIntent,
           (unsigned)live->softKeyPresentationDeferred,
           (unsigned)live->targetMapId, (unsigned)live->active);
    printf("[JUNCTIONPLAYING] SEMANTIC nativeST_PLAYING=yes legacyST_PLAYING=no stateTransition=yes softKeyCallRequested=%s softKeyVisible=%s softKeyLabels=%s restoreSoftKeysResult=%u skipCheckStateResult=%u legacyDoomCanvas_setStateCalled=no legacyDoomCanvas_drawSoftKeysCalled=no rendering=no presentation=no\n",
           expectedSoftKeyIntent == ESP_POST_LOAD_PLAYING_SOFTKEY_MENU_MAP
               ? "yes" : "no",
           expectedSoftKeyVisible != 0U ? "yes" : "no",
           expectedSoftKeyIntent == ESP_POST_LOAD_PLAYING_SOFTKEY_MENU_MAP
               ? "Menu/Map" : "none",
           (unsigned)live->restoreSoftKeysAfter,
           (unsigned)live->skipCheckStateAfter);
    printf("[JUNCTIONPLAYING] INPUT viewInvalidationBytes=%u viewInvalidationFNV=%08x unchanged=%s callerOrder=yes isUpdateView=1->1 particleTopologyCanonical=yes activeList=%d freeList=%d totalPool=64\n",
           (unsigned)sizeof(EspPostLoadViewInvalidationState),
           (unsigned)viewAfter, viewBefore == viewAfter ? "yes" : "no",
           activeAfter, freeAfter);
    printf("[JUNCTIONPLAYING] FAILCLOSED nullView=%d nullOutput=%d inactiveView=%d targetMap=%d invalidMonstersTurn=%d invalidDisplaySoftKeys=%d invalidRestoreSoftKeys=%d invalidSkipCheckState=%d prepareAtomic=%s postActivePrepare=%d repeat=%d repeatAtomic=%s\n",
           nullViewGate, nullOutputGate, inactiveViewGate, mapGate,
           invalidMonstersGate, invalidDisplayGate, invalidRestoreGate,
           invalidSkipGate, prepareAtomic ? "yes" : "no",
           postActivePrepareGate, repeatGate, repeatAtomic ? "yes" : "no");
    printf("[JUNCTIONPLAYING] RESIDENT snapshotFNV=%08x->%08x unchanged=yes mapFNV=%08x automapFNV=%08x runtimeFNV=%08x scriptFNV=%08x lineFNV=%08x textureFNV=%08x topologyFNV=%08x payload=%u entities=%u enemies=%u destructibles=%u packClosed=yes\n",
           (unsigned)hashBytes(&residentBefore, sizeof(residentBefore)),
           (unsigned)hashBytes(&residentAfter, sizeof(residentAfter)),
           (unsigned)residentAfter.mapStateFNV1a,
           (unsigned)residentAfter.automapStateFNV1a,
           (unsigned)residentAfter.runtimeFNV1a,
           (unsigned)residentAfter.scriptStateFNV1a,
           (unsigned)residentAfter.lineStateFNV1a,
           (unsigned)residentAfter.textureStateFNV1a,
           (unsigned)residentAfter.topologyFNV1a,
           (unsigned)residentAfter.totalPayloadBytes,
           (unsigned)residentAfter.entityCount,
           (unsigned)residentAfter.enemyCount,
           (unsigned)residentAfter.destructibleCount);
    printf("[JUNCTIONPLAYING] RAM heap8=%u->%u delta=%d largest8=%u->%u delta=%d persistentHeapBytes=0\n",
           (unsigned)heapBefore, (unsigned)heapAfter,
           (int)heapAfter - (int)heapBefore,
           (unsigned)largestBefore, (unsigned)largestAfter,
           (int)largestAfter - (int)largestBefore);
    printf("[JUNCTIONPLAYING] LEGACY gameFNV=%08x->%08x playerFNV=%08x->%08x hudFNV=%08x->%08x canvasFNV=%08x->%08x renderFNV=%08x->%08x frameFNV=%08x->%08x eventQueueFNV=%08x->%08x particleFNV=%08x->%08x legacyState=%d->%d monstersTurn=%u->%u displaySoftKeys=%u->%u restoreSoftKeys=%u->%u skipCheckState=%u->%u legacyRuntimeClear=yes GameMutation=no PlayerMutation=no HudMutation=no DoomCanvasMutation=no RenderMutation=no ParticleSystemMutation=no legacyDoomCanvas_setStateCalled=no legacyDoomCanvas_drawSoftKeysCalled=no\n",
           (unsigned)gameBefore, (unsigned)gameAfter,
           (unsigned)playerBefore, (unsigned)playerAfter,
           (unsigned)hudBefore, (unsigned)hudAfter,
           (unsigned)canvasBefore, (unsigned)canvasAfter,
           (unsigned)renderBefore, (unsigned)renderAfter,
           (unsigned)frameBefore, (unsigned)frameAfter,
           (unsigned)eventQueueBefore, (unsigned)eventQueueAfter,
           (unsigned)particleBefore, (unsigned)particleAfter,
           ST_INTRO, doomRpg->doomCanvas->state,
           (unsigned)monstersTurnBefore,
           (unsigned)doomRpg->game->monstersTurn,
           (unsigned)displaySoftKeysBefore,
           (unsigned)doomRpg->doomCanvas->displaySoftKeys,
           (unsigned)restoreSoftKeysBefore,
           (unsigned)doomRpg->doomCanvas->restoreSoftKeys,
           (unsigned)skipCheckStateBefore,
           (unsigned)doomRpg->doomCanvas->skipCheckState);
    printf("[JUNCTIONPLAYING] PARK legacyState=%d page=%d targetMap=9 junctionResident=yes nativeViewInvalidation=yes nativeST_PLAYING=yes legacyST_PLAYING=no initialSavePersistencePending=yes ST_PLAYINGPending=no idleTimePending=yes rendererPending=yes entities=0 monsters=0 noGameplay=yes\n",
           doomRpg->doomCanvas->state, doomRpg->doomCanvas->storyPage);

    probeState.done = 1;
}
