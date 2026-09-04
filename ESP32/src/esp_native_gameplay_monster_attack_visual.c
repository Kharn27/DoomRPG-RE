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
#define ATTACK_VISUAL_FRAME_MS 150U
#define ATTACK_VISUAL_SUBTYPE_COUNT 14U

/* Exact Combat monsterWpInfo NUMSHOTS field. Multi-loop presentation is kept
 * fail-closed here even though the already-owned retaliation math resolves all
 * loops transactionally. */
static const uint8_t monsterShots[ATTACK_VISUAL_SUBTYPE_COUNT] = {
    1U, 1U, 3U, 1U, 3U, 1U, 3U, 1U, 1U, 1U, 1U, 1U, 1U, 3U
};

static EspNativeGameplayMonsterAttackVisualView attackVisual;

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
        attackVisual.sourceArenaFNV1a = turn->sourceArenaFNV1a;
        attackVisual.observedAttackProbes = turn->attackProbes;
        attackVisual.activeSpriteIndex = ATTACK_VISUAL_NO_SPRITE;
        attackVisual.active = 1U;
        printf("[MONSTERATKVIS] READY arena=%08x ownerBytes=%u source=hardware-proven-turn-probe primaryFrame=%u leaseMs=%u singleLoop=yes immutableSprite=yes fixedAnim=overlay gameplayRng=guarded altFrame5=fail-closed multiLoop=fail-closed projectile=deferred attackMessage=deferred sound=deferred\n",
               (unsigned int)attackVisual.sourceArenaFNV1a,
               (unsigned int)sizeof(attackVisual),
               (unsigned int)ATTACK_VISUAL_PRIMARY_FRAME,
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

static void clearPose(void) {
    attackVisual.poseActive = 0U;
    attackVisual.activeProbe = 0U;
    attackVisual.clearAtMs = 0U;
    attackVisual.activeSpriteIndex = ATTACK_VISUAL_NO_SPRITE;
}

static void serviceExpiry(DoomRPG_t* runtime) {
    const EspPlayerViewState* view;
    EspNativeGameplayFrameStats frame;
    uint32_t now;
    uint32_t probe;
    uint16_t spriteIndex;
    int rngExact = 0;

    if (attackVisual.poseActive == 0U) return;
    now = DoomRPG_GetUpTimeMS();
    if ((int32_t)(now - attackVisual.clearAtMs) < 0) return;

    /* The touch-feedback owner keeps a short framebuffer snapshot. Repainting
     * underneath that lease would allow its restore path to resurrect the
     * attack pose, so retry after touch feedback releases ownership. */
    if (EspNativeGameplayControls_isActive()) return;

    view = EspPlayerView_view();
    if (runtime == NULL || runtime->render == NULL || !settledPlayerView(view)) {
        return;
    }

    probe = attackVisual.activeProbe;
    spriteIndex = attackVisual.activeSpriteIndex;
    clearPose();
    if (!guardedRender(runtime, view, &frame, &rngExact)) {
        attackVisual.activeProbe = probe;
        attackVisual.activeSpriteIndex = spriteIndex;
        attackVisual.clearAtMs = now + 1U;
        attackVisual.poseActive = 1U;
        ++attackVisual.expiryRetries;
        printf("[MONSTERATKVIS] EXPIRE-RETRY probe=%u sprite=%u reason=%s rngExact=%s recovery=next-service gameplayMutation=no\n",
               (unsigned int)probe,
               (unsigned int)spriteIndex,
               rngExact ? "render-failed" : "render-touched-gameplay-rng",
               rngExact ? "yes" : "NO");
        return;
    }

    printf("[MONSTERATKVIS] EXPIRE probe=%u sprite=%u visual=%u->idle leaseMs=%u frame=%08x presented=%u rngExact=yes gameplayMutation=no\n",
           (unsigned int)probe,
           (unsigned int)spriteIndex,
           (unsigned int)ATTACK_VISUAL_PRIMARY_FRAME,
           (unsigned int)ATTACK_VISUAL_FRAME_MS,
           (unsigned int)frame.frameAfterFNV,
           (unsigned int)frame.finalPresented);
}

void EspNativeGameplayMonsterAttackVisual_reset(void) {
    memset(&attackVisual, 0, sizeof(attackVisual));
    attackVisual.activeSpriteIndex = ATTACK_VISUAL_NO_SPRITE;
}

const EspNativeGameplayMonsterAttackVisualView*
EspNativeGameplayMonsterAttackVisual_view(void) {
    return attackVisual.active == 1U ? &attackVisual : NULL;
}

int EspNativeGameplayMonsterAttackVisual_apply(uint32_t spriteIndex,
                                               uint8_t* ioVisualState) {
    if (ioVisualState == NULL || attackVisual.active != 1U ||
        attackVisual.poseActive != 1U ||
        spriteIndex != attackVisual.activeSpriteIndex) {
        return 0;
    }
    *ioVisualState = (uint8_t)((*ioVisualState & 0xf0U) |
                               ATTACK_VISUAL_PRIMARY_FRAME);
    return 1;
}

int EspNativeGameplayMonsterAttackVisual_isPoseSprite(uint32_t spriteIndex) {
    return attackVisual.active == 1U && attackVisual.poseActive == 1U &&
           spriteIndex == attackVisual.activeSpriteIndex;
}

void EspNativeGameplayMonsterAttackVisual_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* runtime = (DoomRPG_t*)doomRpgBase;
    const EspNativeGameplayMonsterTurnView* turn;
    const EspNativeGameplayMonsterRecord* monster;
    const EspPlayerViewState* view;
    EspNativeGameplayFrameStats frame;
    EspNativeGameplayFrameStats recoveryFrame;
    uint32_t now;
    int rngExact = 0;
    int recoveryRngExact = 0;
    int recoveryRendered = 0;

    if (!syncOwner()) return;
    serviceExpiry(runtime);

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

    if (attackVisual.poseActive != 0U) {
        printf("[MONSTERATKVIS] DEFER probe=%u reason=%s sprite=%u cause=pose-overlap activeProbe=%u activeSprite=%u presentation=no gameplayMutation=no\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)turn->lastAttackerSpriteIndex,
               (unsigned int)attackVisual.activeProbe,
               (unsigned int)attackVisual.activeSpriteIndex);
        return;
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

    if (monster->alternateAttack != 0U) {
        ++attackVisual.deferredAlternate;
        printf("[MONSTERATKVIS] DEFER probe=%u reason=%s sprite=%u subtype=%u alt=%u cause=alternate-frame5 ownedFrame=1 presentation=no gameplayMutation=no\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)monster->spriteIndex,
               (unsigned int)monster->subtype,
               (unsigned int)monster->alternateAttack);
        return;
    }

    if (monsterShots[monster->subtype] != 1U) {
        ++attackVisual.deferredMultiLoop;
        printf("[MONSTERATKVIS] DEFER probe=%u reason=%s sprite=%u subtype=%u loops=%u cause=multi-loop-presentation ownedLoops=1 presentation=no gameplayMutation=no\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)monster->spriteIndex,
               (unsigned int)monster->subtype,
               (unsigned int)monsterShots[monster->subtype]);
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

    now = DoomRPG_GetUpTimeMS();
    attackVisual.activeProbe = turn->attackProbes;
    attackVisual.activeSpriteIndex = monster->spriteIndex;
    attackVisual.clearAtMs = now + ATTACK_VISUAL_FRAME_MS;
    attackVisual.poseActive = 1U;

    if (!guardedRender(runtime, view, &frame, &rngExact)) {
        clearPose();
        ++attackVisual.renderRollbacks;
        recoveryRendered = guardedRender(runtime, view, &recoveryFrame,
                                         &recoveryRngExact);
        printf("[MONSTERATKVIS] ROLLBACK probe=%u reason=%s sprite=%u subtype=%u cause=%s poseCleared=yes rngExact=%s recoveryRender=%s recoveryRngExact=%s gameplayMutation=no\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)monster->spriteIndex,
               (unsigned int)monster->subtype,
               rngExact ? "attack-frame-render-failed" :
                          "render-touched-gameplay-rng",
               rngExact ? "yes" : "NO",
               recoveryRendered ? "yes" : "NO",
               recoveryRngExact ? "yes" : "NO");
        return;
    }

    ++attackVisual.presentedAttacks;
    printf("[MONSTERATKVIS] ARM probe=%u reason=%s sprite=%u subtype=%u alt=0 loops=1 visual=%u fixedAnim=yes leaseMs=%u frame=%08x presented=%u rngExact=yes immutableSprite=yes retaliation=continues projectile=deferred attackMessage=deferred sound=deferred gameplayMutation=no\n",
           (unsigned int)turn->attackProbes,
           reasonName(turn->lastReason),
           (unsigned int)monster->spriteIndex,
           (unsigned int)monster->subtype,
           (unsigned int)ATTACK_VISUAL_PRIMARY_FRAME,
           (unsigned int)ATTACK_VISUAL_FRAME_MS,
           (unsigned int)frame.frameAfterFNV,
           (unsigned int)frame.finalPresented);
}
