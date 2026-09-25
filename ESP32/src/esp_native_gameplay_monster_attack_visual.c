#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_native_gameplay_controls.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_monster_attack_visual.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_player_view_state.h"

#define ATTACK_VISUAL_NO_SPRITE 0xffffU
#define ATTACK_VISUAL_PRIMARY_FRAME 1U
#define ATTACK_VISUAL_ALTERNATE_FRAME 5U
#define ATTACK_VISUAL_FRAME_MS 150U
#define ATTACK_VISUAL_SUBTYPE_COUNT 14U

/* Exact Combat monsterWpInfo table split into its two semantic fields.
 *
 * Legacy Combat_monsterSeq() toggles the attack/idle visual once per
 * (10 * delay) ms and decrements NUMSHOTS on every attack pose.  The separate
 * 150 ms frameTime controls projectile/sound timing, not when the sprite returns
 * to idle; every recovered delay is >= 200 ms, so the visual phase cadence is
 * simply the second monsterWpInfo field * 10.
 *
 * Keeping this table here makes attack presentation generic for every ordinary
 * monster subtype, including the three-shot families, without importing the
 * legacy Combat state machine or allocating per-enemy animation objects. */
static const uint8_t monsterShots[ATTACK_VISUAL_SUBTYPE_COUNT] = {
    1U, 1U, 3U, 1U, 3U, 1U, 3U, 1U, 1U, 1U, 1U, 1U, 1U, 3U
};
static const uint16_t monsterPhaseMs[ATTACK_VISUAL_SUBTYPE_COUNT] = {
    500U, 500U, 200U, 500U, 250U, 500U, 250U,
    300U, 500U, 500U, 500U, 500U, 500U, 250U
};

static EspNativeGameplayMonsterAttackVisualView attackVisual;
static uint8_t activeVisualFrame;
static uint8_t activeAttackFrame;
static uint8_t activeSequence;
static uint8_t activeTotalLoops;
static uint8_t activeCompletedLoops;
static uint16_t activePhaseMs;

static const char* reasonName(uint8_t reason) {
    switch ((EspNativeGameplayMonsterTurnReason)reason) {
    case ESP_NATIVE_GAMEPLAY_MONSTER_TURN_MOVE: return "MOVE";
    case ESP_NATIVE_GAMEPLAY_MONSTER_TURN_ROTATE: return "ROTATE";
    case ESP_NATIVE_GAMEPLAY_MONSTER_TURN_PLAYER_ATTACK: return "PLAYER_ATTACK";
    case ESP_NATIVE_GAMEPLAY_MONSTER_TURN_PASS_TURN: return "PASS_TURN";
    default: return "NONE";
    }
}

static int syncOwner(void) {
    const EspNativeGameplayMonsterTurnView* turn =
        EspNativeGameplayMonsterTurn_view();
    const EspNativeGameplayMonsterView* monsters =
        EspNativeGameplayMonsterState_view();

    if (turn == NULL || turn->active != 1U || turn->sourceArenaFNV1a == 0U ||
        monsters == NULL || monsters->records == NULL ||
        monsters->sourceArenaFNV1a != turn->sourceArenaFNV1a) {
        return 0;
    }

    if (attackVisual.active == 0U ||
        attackVisual.sourceArenaFNV1a != turn->sourceArenaFNV1a) {
        memset(&attackVisual, 0, sizeof(attackVisual));
        activeVisualFrame = 0U;
        activeAttackFrame = 0U;
        activeSequence = 0U;
        activeTotalLoops = 0U;
        activeCompletedLoops = 0U;
        activePhaseMs = 0U;
        attackVisual.sourceArenaFNV1a = turn->sourceArenaFNV1a;
        attackVisual.observedAttackProbes = turn->attackProbes;
        attackVisual.activeSpriteIndex = ATTACK_VISUAL_NO_SPRITE;
        attackVisual.active = 1U;
        printf("[MONSTERATKVIS] READY arena=%08x ownerBytes=%u source=turn-probe primaryFrame=%u alternateFrame=%u projectileFrameMs=%u sequence=generic-1or3-loop cadence=legacy-monsterWpInfo immutableSprite=yes fixedAnim=overlay gameplayRng=guarded projectile=deferred attackMessage=deferred sound=deferred\n",
               (unsigned int)attackVisual.sourceArenaFNV1a,
               (unsigned int)sizeof(attackVisual),
               (unsigned int)ATTACK_VISUAL_PRIMARY_FRAME,
               (unsigned int)ATTACK_VISUAL_ALTERNATE_FRAME,
               (unsigned int)ATTACK_VISUAL_FRAME_MS);
    }
    return 1;
}

static int settledPlayerView(const EspPlayerViewState* view) {
    return view != NULL && view->active == 1U &&
           view->viewX == view->destX &&
           view->viewY == view->destY &&
           view->viewAngle == view->destAngle;
}

static int guardedRender(DoomRPG_t* runtime,
                         const EspPlayerViewState* view,
                         EspNativeGameplayFrameStats* outFrame,
                         int* outRngExact) {
    Random_t before;
    int rendered;
    int exact;

    if (outFrame != NULL) memset(outFrame, 0, sizeof(*outFrame));
    if (outRngExact != NULL) *outRngExact = 0;
    if (runtime == NULL || runtime->render == NULL ||
        !settledPlayerView(view) || outFrame == NULL) {
        return 0;
    }

    before = runtime->random;
    rendered = EspNativeGameplayFrame_renderTurn(
        runtime->render, (uint8_t)view->viewAngle, outFrame);
    exact = memcmp(&runtime->random, &before, sizeof(before)) == 0;
    if (!exact) runtime->random = before;
    if (outRngExact != NULL) *outRngExact = exact;
    return rendered && exact;
}

static void clearSequence(void) {
    attackVisual.poseActive = 0U;
    attackVisual.activeProbe = 0U;
    attackVisual.clearAtMs = 0U;
    attackVisual.activeSpriteIndex = ATTACK_VISUAL_NO_SPRITE;
    activeVisualFrame = 0U;
    activeAttackFrame = 0U;
    activeSequence = 0U;
    activeTotalLoops = 0U;
    activeCompletedLoops = 0U;
    activePhaseMs = 0U;
}

static void serviceTimeline(DoomRPG_t* runtime) {
    const EspPlayerViewState* view;
    EspNativeGameplayFrameStats frame;
    uint32_t now;
    uint32_t probe;
    uint16_t spriteIndex;
    uint8_t attackFrame;
    uint8_t totalLoops;
    uint8_t completedLoops;
    uint16_t phaseMs;
    int wasAttackPose;
    int rngExact = 0;

    if (activeSequence == 0U || attackVisual.clearAtMs == 0U) return;
    now = DoomRPG_GetUpTimeMS();
    if ((int32_t)(now - attackVisual.clearAtMs) < 0) return;

    /* Touch feedback owns a bounded framebuffer snapshot.  A world redraw under
     * that lease could be resurrected when the snapshot restores, so all attack
     * timeline transitions retry after touch feedback releases ownership. */
    if (EspNativeGameplayControls_isActive()) return;

    view = EspPlayerView_view();
    if (runtime == NULL || runtime->render == NULL || !settledPlayerView(view)) {
        return;
    }

    probe = attackVisual.activeProbe;
    spriteIndex = attackVisual.activeSpriteIndex;
    attackFrame = activeAttackFrame;
    totalLoops = activeTotalLoops;
    completedLoops = activeCompletedLoops;
    phaseMs = activePhaseMs;
    wasAttackPose = attackVisual.poseActive != 0U;

    if (wasAttackPose) {
        /* Attack -> idle.  For the final shot this same redraw closes the
         * sequence; otherwise the idle phase lasts the exact recovered subtype
         * cadence before the next attack frame is presented. */
        attackVisual.poseActive = 0U;
        activeVisualFrame = 0U;
        if (!guardedRender(runtime, view, &frame, &rngExact)) {
            attackVisual.poseActive = 1U;
            activeVisualFrame = attackFrame;
            attackVisual.clearAtMs = now + 1U;
            ++attackVisual.expiryRetries;
            printf("[MONSTERATKVIS] STEP-RETRY probe=%u sprite=%u shot=%u/%u phase=attack->idle cause=%s rngExact=%s gameplayMutation=no\n",
                   (unsigned int)probe,
                   (unsigned int)spriteIndex,
                   (unsigned int)completedLoops,
                   (unsigned int)totalLoops,
                   rngExact ? "render-failed" : "render-touched-gameplay-rng",
                   rngExact ? "yes" : "NO");
            return;
        }

        if (completedLoops >= totalLoops) {
            attackVisual.completedProbe = probe;
            printf("[MONSTERATKVIS] COMPLETE probe=%u sprite=%u loops=%u visual=%u->idle phaseMs=%u frame=%08x presented=%u rngExact=yes gameplayMutation=no resolution=unblocked-after-animation\n",
                   (unsigned int)probe,
                   (unsigned int)spriteIndex,
                   (unsigned int)totalLoops,
                   (unsigned int)attackFrame,
                   (unsigned int)phaseMs,
                   (unsigned int)frame.frameAfterFNV,
                   (unsigned int)frame.finalPresented);
            clearSequence();
            return;
        }

        attackVisual.clearAtMs = DoomRPG_GetUpTimeMS() + phaseMs;
        printf("[MONSTERATKVIS] STEP probe=%u sprite=%u shot=%u/%u phase=idle nextShot=%u phaseMs=%u frame=%08x presented=%u rngExact=yes gameplayMutation=no\n",
               (unsigned int)probe,
               (unsigned int)spriteIndex,
               (unsigned int)completedLoops,
               (unsigned int)totalLoops,
               (unsigned int)(completedLoops + 1U),
               (unsigned int)phaseMs,
               (unsigned int)frame.frameAfterFNV,
               (unsigned int)frame.finalPresented);
        return;
    }

    /* Idle -> next attack shot. */
    attackVisual.poseActive = 1U;
    activeVisualFrame = attackFrame;
    ++activeCompletedLoops;
    if (!guardedRender(runtime, view, &frame, &rngExact)) {
        --activeCompletedLoops;
        attackVisual.poseActive = 0U;
        activeVisualFrame = 0U;
        attackVisual.clearAtMs = now + 1U;
        ++attackVisual.expiryRetries;
        printf("[MONSTERATKVIS] STEP-RETRY probe=%u sprite=%u shot=%u/%u phase=idle->attack cause=%s rngExact=%s gameplayMutation=no\n",
               (unsigned int)probe,
               (unsigned int)spriteIndex,
               (unsigned int)(completedLoops + 1U),
               (unsigned int)totalLoops,
               rngExact ? "render-failed" : "render-touched-gameplay-rng",
               rngExact ? "yes" : "NO");
        return;
    }

    attackVisual.clearAtMs = DoomRPG_GetUpTimeMS() + phaseMs;
    printf("[MONSTERATKVIS] STEP probe=%u sprite=%u shot=%u/%u phase=attack visual=%u phaseMs=%u frame=%08x presented=%u rngExact=yes gameplayMutation=no\n",
           (unsigned int)probe,
           (unsigned int)spriteIndex,
           (unsigned int)activeCompletedLoops,
           (unsigned int)totalLoops,
           (unsigned int)attackFrame,
           (unsigned int)phaseMs,
           (unsigned int)frame.frameAfterFNV,
           (unsigned int)frame.finalPresented);
}

void EspNativeGameplayMonsterAttackVisual_reset(void) {
    memset(&attackVisual, 0, sizeof(attackVisual));
    activeVisualFrame = 0U;
    activeAttackFrame = 0U;
    activeSequence = 0U;
    activeTotalLoops = 0U;
    activeCompletedLoops = 0U;
    activePhaseMs = 0U;
    attackVisual.activeSpriteIndex = ATTACK_VISUAL_NO_SPRITE;
}

const EspNativeGameplayMonsterAttackVisualView*
EspNativeGameplayMonsterAttackVisual_view(void) {
    return attackVisual.active == 1U ? &attackVisual : NULL;
}

int EspNativeGameplayMonsterAttackVisual_apply(uint32_t spriteIndex,
                                               uint8_t* ioVisualState) {
    if (ioVisualState == NULL || attackVisual.active != 1U ||
        attackVisual.poseActive != 1U || activeVisualFrame == 0U ||
        spriteIndex != attackVisual.activeSpriteIndex) {
        return 0;
    }
    *ioVisualState = (uint8_t)((*ioVisualState & 0xf0U) | activeVisualFrame);
    return 1;
}

int EspNativeGameplayMonsterAttackVisual_isPoseSprite(uint32_t spriteIndex) {
    return attackVisual.active == 1U && attackVisual.poseActive == 1U &&
           spriteIndex == attackVisual.activeSpriteIndex;
}

int EspNativeGameplayMonsterAttackVisual_isBusy(void) {
    return attackVisual.active == 1U && activeSequence != 0U;
}

int EspNativeGameplayMonsterAttackVisual_isProbeComplete(uint32_t probe) {
    return attackVisual.active == 1U && probe != 0U &&
           attackVisual.completedProbe == probe && activeSequence == 0U;
}

void EspNativeGameplayMonsterAttackVisual_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* runtime = (DoomRPG_t*)doomRpgBase;
    const EspNativeGameplayMonsterTurnView* turn;
    const EspNativeGameplayMonsterRecord* monster;
    const EspPlayerViewState* view;
    EspNativeGameplayFrameStats frame;
    EspNativeGameplayFrameStats recoveryFrame;
    uint8_t visualFrame;
    int rngExact = 0;
    int recoveryRngExact = 0;
    int recoveryRendered = 0;

    if (!syncOwner()) return;
    serviceTimeline(runtime);

    turn = EspNativeGameplayMonsterTurn_view();
    if (turn == NULL || turn->attackProbes == attackVisual.observedAttackProbes) {
        return;
    }

    if (turn->attackProbes != attackVisual.observedAttackProbes + 1U) {
        printf("[MONSTERATKVIS] DEFER probes=%u->%u cause=probe-sequence-gap presentation=no gameplayMutation=no\n",
               (unsigned int)attackVisual.observedAttackProbes,
               (unsigned int)turn->attackProbes);
        attackVisual.observedAttackProbes = turn->attackProbes;
        return;
    }
    attackVisual.observedAttackProbes = turn->attackProbes;

    if (activeSequence != 0U) {
        printf("[MONSTERATKVIS] REPLACE probe=%u reason=%s sprite=%u priorProbe=%u priorSprite=%u priorShot=%u/%u cause=new-turn-probe presentation=continues gameplayMutation=no\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)turn->lastAttackerSpriteIndex,
               (unsigned int)attackVisual.activeProbe,
               (unsigned int)attackVisual.activeSpriteIndex,
               (unsigned int)activeCompletedLoops,
               (unsigned int)activeTotalLoops);
        clearSequence();
    }

    if (runtime == NULL || runtime->render == NULL ||
        turn->lastAttackerSpriteIndex == ATTACK_VISUAL_NO_SPRITE) {
        printf("[MONSTERATKVIS] DEFER probe=%u reason=%s cause=not-ready presentation=no gameplayMutation=no\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason));
        return;
    }

    monster = EspNativeGameplayMonsterState_find(turn->lastAttackerSpriteIndex);
    if (monster == NULL || monster->alive == 0U ||
        monster->subtype >= ATTACK_VISUAL_SUBTYPE_COUNT) {
        printf("[MONSTERATKVIS] DEFER probe=%u reason=%s sprite=%u cause=attacker-not-live presentation=no gameplayMutation=no\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)turn->lastAttackerSpriteIndex);
        return;
    }

    view = EspPlayerView_view();
    if (!settledPlayerView(view)) {
        printf("[MONSTERATKVIS] DEFER probe=%u reason=%s sprite=%u cause=unsettled-player-view presentation=no gameplayMutation=no\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)monster->spriteIndex);
        return;
    }

    visualFrame = monster->alternateAttack != 0U
                      ? ATTACK_VISUAL_ALTERNATE_FRAME
                      : ATTACK_VISUAL_PRIMARY_FRAME;
    attackVisual.completedProbe = 0U;
    attackVisual.activeProbe = turn->attackProbes;
    attackVisual.activeSpriteIndex = monster->spriteIndex;
    attackVisual.poseActive = 1U;
    activeVisualFrame = visualFrame;
    activeAttackFrame = visualFrame;
    activeSequence = 1U;
    activeTotalLoops = monsterShots[monster->subtype];
    activeCompletedLoops = 1U;
    activePhaseMs = monsterPhaseMs[monster->subtype];

    if (!guardedRender(runtime, view, &frame, &rngExact)) {
        clearSequence();
        ++attackVisual.renderRollbacks;
        recoveryRendered = guardedRender(runtime, view, &recoveryFrame,
                                         &recoveryRngExact);
        printf("[MONSTERATKVIS] ROLLBACK probe=%u reason=%s sprite=%u subtype=%u alt=%u visual=%u cause=%s poseCleared=yes rngExact=%s recoveryRender=%s recoveryRngExact=%s gameplayMutation=no\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)monster->spriteIndex,
               (unsigned int)monster->subtype,
               (unsigned int)monster->alternateAttack,
               (unsigned int)visualFrame,
               rngExact ? "attack-frame-render-failed" :
                          "render-touched-gameplay-rng",
               rngExact ? "yes" : "NO",
               recoveryRendered ? "yes" : "NO",
               recoveryRngExact ? "yes" : "NO");
        return;
    }

    /* Start the first visual phase only after physical presentation so render
     * cost cannot consume the recovered attack cadence.  Retaliation remains a
     * separate gameplay transaction and may commit while this presentation-only
     * sequence continues asynchronously. */
    attackVisual.clearAtMs = DoomRPG_GetUpTimeMS() + activePhaseMs;
    ++attackVisual.presentedAttacks;
    printf("[MONSTERATKVIS] ARM probe=%u reason=%s sprite=%u subtype=%u alt=%u loops=%u shot=1/%u phase=attack visual=%u fixedAnim=yes phaseMs=%u projectileFrameMs=%u frame=%08x presented=%u rngExact=yes immutableSprite=yes retaliation=continues projectile=deferred attackMessage=deferred sound=deferred gameplayMutation=no\n",
           (unsigned int)turn->attackProbes,
           reasonName(turn->lastReason),
           (unsigned int)monster->spriteIndex,
           (unsigned int)monster->subtype,
           (unsigned int)monster->alternateAttack,
           (unsigned int)activeTotalLoops,
           (unsigned int)activeTotalLoops,
           (unsigned int)visualFrame,
           (unsigned int)activePhaseMs,
           (unsigned int)ATTACK_VISUAL_FRAME_MS,
           (unsigned int)frame.frameAfterFNV,
           (unsigned int)frame.finalPresented);
}
