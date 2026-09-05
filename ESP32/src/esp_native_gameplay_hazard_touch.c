#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hazard_touch.h"
#include "esp_native_gameplay_player_state.h"

#define HAZARD_MAP_WIDTH 32U
#define HAZARD_TYPE_FIRE 10U
#define HAZARD_TYPE_STRONG 11U
#define HAZARD_RESOURCE_TYPE_WORLD 3U
#define HAZARD_RESOURCE_TYPE_INVENTORY 4U
#define HAZARD_RESOURCE_TYPE_WEAPON 5U
#define HAZARD_RESOURCE_TYPE_AMMO 6U
#define HAZARD_RESOURCE_TYPE_AMMO_ALT 16U
#define HAZARD_DAMAGE_FLASH_MS 500U
#define HAZARD_DOG_WEAPON_FIRST 9U
#define HAZARD_DOG_WEAPON_LAST 11U
#define HAZARD_DOG_AMMO_TYPE 5U

static uint16_t tileForView(const EspPlayerViewState* view) {
    uint32_t x;
    uint32_t y;
    if (view == NULL || view->active != 1U || view->viewX < 0 || view->viewY < 0) {
        return UINT16_MAX;
    }
    x = (uint32_t)view->viewX >> 6;
    y = (uint32_t)view->viewY >> 6;
    if (x >= HAZARD_MAP_WIDTH || y >= HAZARD_MAP_WIDTH) return UINT16_MAX;
    return (uint16_t)(y * HAZARD_MAP_WIDTH + x);
}

static int isResourceType(uint8_t type) {
    return type == HAZARD_RESOURCE_TYPE_WORLD ||
           type == HAZARD_RESOURCE_TYPE_INVENTORY ||
           type == HAZARD_RESOURCE_TYPE_WEAPON ||
           type == HAZARD_RESOURCE_TYPE_AMMO ||
           type == HAZARD_RESOURCE_TYPE_AMMO_ALT;
}

static int scanTile(uint16_t tile,
                    uint16_t* outFirstSprite,
                    uint8_t* outFirstType,
                    uint8_t* outHazards,
                    uint8_t* outResources,
                    uint16_t* outHealthDamage,
                    uint16_t* outArmorDamage) {
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    uint32_t i;
    uint8_t hazards = 0U;
    uint8_t resources = 0U;
    uint16_t healthDamage = 0U;
    uint16_t armorDamage = 0U;
    uint16_t firstSprite = UINT16_MAX;
    uint8_t firstType = 0U;

    if (outFirstSprite != NULL) *outFirstSprite = UINT16_MAX;
    if (outFirstType != NULL) *outFirstType = 0U;
    if (outHazards != NULL) *outHazards = 0U;
    if (outResources != NULL) *outResources = 0U;
    if (outHealthDamage != NULL) *outHealthDamage = 0U;
    if (outArmorDamage != NULL) *outArmorDamage = 0U;
    if (topology == NULL || topology->spriteCount > UINT16_MAX) return 0;

    for (i = 0U; i < topology->spriteCount; ++i) {
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        if (!EspMapSpriteTopology_getEntity(i, &type, &subtype,
                                            &linkState, &linkOrder)) {
            return 0;
        }
        (void)subtype;
        (void)linkOrder;
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != tile) {
            continue;
        }
        if (type == HAZARD_TYPE_FIRE || type == HAZARD_TYPE_STRONG) {
            if (hazards == 0U) {
                firstSprite = (uint16_t)i;
                firstType = type;
            }
            if (hazards == UINT8_MAX) return 0;
            ++hazards;
            if (type == HAZARD_TYPE_FIRE) {
                healthDamage += 1U;
                armorDamage += 2U;
            }
            else {
                healthDamage += 10U;
                armorDamage += 10U;
            }
        }
        else if (isResourceType(type)) {
            if (resources != UINT8_MAX) ++resources;
        }
    }

    if (outFirstSprite != NULL) *outFirstSprite = firstSprite;
    if (outFirstType != NULL) *outFirstType = firstType;
    if (outHazards != NULL) *outHazards = hazards;
    if (outResources != NULL) *outResources = resources;
    if (outHealthDamage != NULL) *outHealthDamage = healthDamage;
    if (outArmorDamage != NULL) *outArmorDamage = armorDamage;
    return 1;
}

static void prospectivePain(const EspNativeGameplayPlayerState* player,
                            uint16_t healthDamage,
                            uint16_t armorDamage,
                            uint8_t* outHealth,
                            uint8_t* outArmor) {
    int32_t health;
    int32_t armor;
    int32_t hpDamage;
    if (player == NULL || outHealth == NULL || outArmor == NULL) return;
    health = (int32_t)(player->param1 & 0xffU);
    armor = (int32_t)((player->param1 >> 16) & 0xffU);
    hpDamage = (int32_t)healthDamage;
    if (armor < (int32_t)armorDamage) {
        hpDamage += (int32_t)armorDamage - armor;
        armor = 0;
    }
    else {
        armor -= (int32_t)armorDamage;
    }
    health -= hpDamage;
    if (health < 0) health = 0;
    *outHealth = (uint8_t)health;
    *outArmor = (uint8_t)armor;
}

static int commitPain(const EspNativeGameplayPlayerState* before,
                      uint8_t healthAfter,
                      uint8_t armorAfter) {
    EspNativeGameplayPlayerState next;
    if (before == NULL || before->active != 1U) return 0;
    next = *before;
    next.param1 = (next.param1 & 0xff00ff00U) |
                  (uint32_t)healthAfter |
                  ((uint32_t)armorAfter << 16);
    return EspNativeGameplayPlayerState_restore(&next);
}

static int rerender(DoomRPG_t* doomRpg,
                    const EspPlayerViewState* view,
                    const char* reason) {
    EspNativeGameplayFrameStats frame;
    if (doomRpg == NULL || doomRpg->render == NULL || view == NULL ||
        view->viewAngle < 0 || view->viewAngle > 255) return 0;
    memset(&frame, 0, sizeof(frame));
    if (!EspNativeGameplayFrame_renderTurn(doomRpg->render,
                                           (uint8_t)view->viewAngle,
                                           &frame)) {
        printf("[HAZARD] RENDER-FAILED reason=%s angle=%d\n",
               reason != NULL ? reason : "touch",
               (int)view->viewAngle);
        return 0;
    }
    printf("[HAZARD] FRAME reason=%s frame=%08x totalUs=%u presented=%u\n",
           reason != NULL ? reason : "touch",
           (unsigned int)frame.frameAfterFNV,
           (unsigned int)frame.totalMicros,
           (unsigned int)frame.finalPresented);
    return frame.active == 1U && frame.finalPresented == 1U;
}

EspNativeGameplayHazardTouchStatus EspNativeGameplayHazardTouch_processMove(
    struct DoomRPG_s* doomRpgBase,
    const EspPlayerViewState* beforeView,
    const EspPlayerViewState* afterView) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspNativeGameplayPlayerState* player;
    EspNativeGameplayPlayerState playerBefore;
    uint16_t beforeTile;
    uint16_t afterTile;
    uint16_t firstSprite;
    uint16_t healthDamage;
    uint16_t armorDamage;
    uint8_t firstType;
    uint8_t hazards;
    uint8_t resources;
    uint8_t healthBefore;
    uint8_t armorBefore;
    uint8_t healthAfter;
    uint8_t armorAfter;
    uint32_t playerFNVBefore;
    uint32_t playerFNVAfter;
    char message[24];
    int feedbackQueued = 0;

    /* This executor is called only from the committed-move service, which
     * snapshots before/after from one resident gameplay session. Keep its
     * entry contract local to the two active settled views instead of coupling
     * hazard damage to a map-owner field that the caller already validated. */
    if (beforeView == NULL || afterView == NULL ||
        beforeView->active != 1U || afterView->active != 1U) {
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_NONE;
    }
    beforeTile = tileForView(beforeView);
    afterTile = tileForView(afterView);
    if (beforeTile == UINT16_MAX || afterTile == UINT16_MAX ||
        beforeTile == afterTile) {
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_NONE;
    }
    if (!scanTile(afterTile, &firstSprite, &firstType, &hazards, &resources,
                  &healthDamage, &armorDamage)) {
        printf("[HAZARD] DEFER tile=%u reason=topology-query mutation=no turn=continues\n",
               (unsigned int)afterTile);
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED;
    }
    if (hazards == 0U) return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_NONE;
    if (resources != 0U) {
        printf("[HAZARD] DEFER tile=%u hazards=%u resources=%u reason=mixed-tiletouch-order-unowned mutation=no turn=continues\n",
               (unsigned int)afterTile,
               (unsigned int)hazards,
               (unsigned int)resources);
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED;
    }
    if (!EspNativeGameplayPlayerState_ensure() ||
        !EspNativeGameplayPlayerState_snapshot(&playerBefore)) {
        printf("[HAZARD] DEFER tile=%u reason=playerstate-not-ready mutation=no turn=continues\n",
               (unsigned int)afterTile);
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED;
    }
    player = EspNativeGameplayPlayerState_view();
    if (player == NULL) return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED;
    if (player->weapon >= HAZARD_DOG_WEAPON_FIRST &&
        player->weapon <= HAZARD_DOG_WEAPON_LAST &&
        player->ammo[HAZARD_DOG_AMMO_TYPE] != 0U) {
        printf("[HAZARD] DEFER tile=%u weapon=%u dogAmmo=%u reason=familiar-redirection-unowned mutation=no turn=continues\n",
               (unsigned int)afterTile,
               (unsigned int)player->weapon,
               (unsigned int)player->ammo[HAZARD_DOG_AMMO_TYPE]);
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED;
    }

    healthBefore = (uint8_t)(playerBefore.param1 & 0xffU);
    armorBefore = (uint8_t)((playerBefore.param1 >> 16) & 0xffU);
    prospectivePain(&playerBefore, healthDamage, armorDamage,
                    &healthAfter, &armorAfter);
    if (healthAfter == 0U) {
        printf("[HAZARD] DEFER tile=%u sprite=%u type=%u hazards=%u hp=%u->0 armor=%u->%u reason=player-lethal-transition-unowned mutation=no turn=continues\n",
               (unsigned int)afterTile,
               (unsigned int)firstSprite,
               (unsigned int)firstType,
               (unsigned int)hazards,
               (unsigned int)healthBefore,
               (unsigned int)armorBefore,
               (unsigned int)armorAfter);
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED;
    }

    playerFNVBefore = EspNativeGameplayPlayerState_fingerprint();
    if (!commitPain(&playerBefore, healthAfter, armorAfter)) {
        printf("[HAZARD] DEFER tile=%u reason=playerstate-commit mutation=no turn=continues\n",
               (unsigned int)afterTile);
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED;
    }
    playerFNVAfter = EspNativeGameplayPlayerState_fingerprint();

    memset(message, 0, sizeof(message));
    if (snprintf(message, sizeof(message), "%u damage!",
                 (unsigned int)(healthDamage + armorDamage)) <= 0 ||
        !EspNativeGameplayActionEngine_queueTextFeedback(
            ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DAMAGE,
            message,
            HAZARD_DAMAGE_FLASH_MS)) {
        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
        printf("[HAZARD] DEFER tile=%u reason=damage-feedback-not-ready playerRollback=yes mutation=no turn=continues\n",
               (unsigned int)afterTile);
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED;
    }
    feedbackQueued = 1;

    if (rerender(doomRpg, afterView, "HAZARD-TOUCH")) {
        printf("[HAZARD] COMMIT tile=%u sprite=%u type=%u hazards=%u rawDamage=%u+%u hp=%u->%u armor=%u->%u playerFNV=%08x->%08x message=\"%s\" secondary=\"%s\"-deferred flash=red-bb0000/500ms painFace=deferred shake=deferred sound=deferred lethal=fail-closed rollback=closed\n",
               (unsigned int)afterTile,
               (unsigned int)firstSprite,
               (unsigned int)firstType,
               (unsigned int)hazards,
               (unsigned int)healthDamage,
               (unsigned int)armorDamage,
               (unsigned int)healthBefore,
               (unsigned int)healthAfter,
               (unsigned int)armorBefore,
               (unsigned int)armorAfter,
               (unsigned int)playerFNVBefore,
               (unsigned int)playerFNVAfter,
               message,
               firstType == HAZARD_TYPE_STRONG ? "It really burns!!" : "It burns!");
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_COMMITTED;
    }

    if (feedbackQueued) {
        (void)EspNativeGameplayActionEngine_cancelQueuedFeedback(
            ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DAMAGE);
    }
    (void)EspNativeGameplayPlayerState_restore(&playerBefore);
    printf("[HAZARD] ROLLBACK tile=%u playerFNV=%08x exact=%s\n",
           (unsigned int)afterTile,
           (unsigned int)EspNativeGameplayPlayerState_fingerprint(),
           EspNativeGameplayPlayerState_fingerprint() == playerFNVBefore
               ? "yes" : "no");
    if (!rerender(doomRpg, afterView, "HAZARD-TOUCH-ROLLBACK")) {
        printf("[HAZARD] FAILED tile=%u reason=rollback-render fatal=1\n",
               (unsigned int)afterTile);
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_FATAL;
    }
    return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED;
}

EspNativeGameplayHazardTouchStatus EspNativeGameplayHazardTouch_processPassTurn(
    EspNativeGameplayHazardPassTurnUndo* outUndo) {
    const EspPlayerViewState* view = EspPlayerView_view();
    const EspNativeGameplayPlayerState* player;
    EspNativeGameplayPlayerState playerBefore;
    uint16_t tile;
    uint16_t firstSprite;
    uint16_t healthDamage;
    uint16_t armorDamage;
    uint8_t firstType;
    uint8_t hazards;
    uint8_t resources;
    uint8_t healthBefore;
    uint8_t armorBefore;
    uint8_t healthAfter;
    uint8_t armorAfter;
    uint32_t playerFNVBefore;
    uint32_t playerFNVAfter;
    char message[24];

    if (outUndo != NULL) memset(outUndo, 0, sizeof(*outUndo));
    if (outUndo == NULL || view == NULL || view->active != 1U ||
        view->viewX != view->destX || view->viewY != view->destY ||
        view->viewAngle != view->destAngle) {
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED;
    }
    tile = tileForView(view);
    if (tile == UINT16_MAX) {
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED;
    }
    if (!scanTile(tile, &firstSprite, &firstType, &hazards, &resources,
                  &healthDamage, &armorDamage)) {
        printf("[HAZARDPASS] DEFER tile=%u reason=topology-query mutation=no monsterTurn=no\n",
               (unsigned int)tile);
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED;
    }
    /* Game_touchTile(..., false) calls Entity_touched only for type10/type11.
     * Resource entities on the same tile are deliberately ignored here. */
    (void)resources;
    if (hazards == 0U) return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_NONE;

    if (!EspNativeGameplayPlayerState_ensure() ||
        !EspNativeGameplayPlayerState_snapshot(&playerBefore)) {
        printf("[HAZARDPASS] DEFER tile=%u reason=playerstate-not-ready mutation=no monsterTurn=no\n",
               (unsigned int)tile);
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED;
    }
    player = EspNativeGameplayPlayerState_view();
    if (player == NULL) return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED;
    if (player->weapon >= HAZARD_DOG_WEAPON_FIRST &&
        player->weapon <= HAZARD_DOG_WEAPON_LAST &&
        player->ammo[HAZARD_DOG_AMMO_TYPE] != 0U) {
        printf("[HAZARDPASS] DEFER tile=%u weapon=%u dogAmmo=%u reason=familiar-redirection-unowned mutation=no monsterTurn=no\n",
               (unsigned int)tile,
               (unsigned int)player->weapon,
               (unsigned int)player->ammo[HAZARD_DOG_AMMO_TYPE]);
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED;
    }

    healthBefore = (uint8_t)(playerBefore.param1 & 0xffU);
    armorBefore = (uint8_t)((playerBefore.param1 >> 16) & 0xffU);
    prospectivePain(&playerBefore, healthDamage, armorDamage,
                    &healthAfter, &armorAfter);
    if (healthAfter == 0U) {
        printf("[HAZARDPASS] DEFER tile=%u sprite=%u type=%u hazards=%u hp=%u->0 armor=%u->%u reason=player-lethal-transition-unowned mutation=no monsterTurn=no\n",
               (unsigned int)tile,
               (unsigned int)firstSprite,
               (unsigned int)firstType,
               (unsigned int)hazards,
               (unsigned int)healthBefore,
               (unsigned int)armorBefore,
               (unsigned int)armorAfter);
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED;
    }

    playerFNVBefore = EspNativeGameplayPlayerState_fingerprint();
    if (!commitPain(&playerBefore, healthAfter, armorAfter)) {
        printf("[HAZARDPASS] DEFER tile=%u reason=playerstate-commit mutation=no monsterTurn=no\n",
               (unsigned int)tile);
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED;
    }
    playerFNVAfter = EspNativeGameplayPlayerState_fingerprint();

    memset(message, 0, sizeof(message));
    if (snprintf(message, sizeof(message), "%u damage!",
                 (unsigned int)(healthDamage + armorDamage)) <= 0 ||
        !EspNativeGameplayActionEngine_queueTextFeedback(
            ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DAMAGE,
            message,
            HAZARD_DAMAGE_FLASH_MS)) {
        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
        printf("[HAZARDPASS] DEFER tile=%u reason=damage-feedback-not-ready playerRollback=yes mutation=no monsterTurn=no\n",
               (unsigned int)tile);
        return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED;
    }

    outUndo->param1Before = playerBefore.param1;
    outUndo->playerFNVBefore = playerFNVBefore;
    outUndo->tileIndex = tile;
    outUndo->feedbackQueued = 1U;
    outUndo->committed = 1U;

    printf("[HAZARDPASS] COMMIT tile=%u sprite=%u type=%u hazards=%u rawDamage=%u+%u hp=%u->%u armor=%u->%u playerFNV=%08x->%08x message=\"%s\" passMessage=\"Turn passed.\"-legacy-superseded secondary=\"%s\"-deferred flash=red-bb0000/500ms render=feedback-owner-pending painFace=deferred shake=deferred sound=deferred lethal=fail-closed rollback=armed\n",
           (unsigned int)tile,
           (unsigned int)firstSprite,
           (unsigned int)firstType,
           (unsigned int)hazards,
           (unsigned int)healthDamage,
           (unsigned int)armorDamage,
           (unsigned int)healthBefore,
           (unsigned int)healthAfter,
           (unsigned int)armorBefore,
           (unsigned int)armorAfter,
           (unsigned int)playerFNVBefore,
           (unsigned int)playerFNVAfter,
           message,
           firstType == HAZARD_TYPE_STRONG ? "It really burns!!" : "It burns!");
    return ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_COMMITTED;
}

int EspNativeGameplayHazardTouch_rollbackPassTurn(
    const EspNativeGameplayHazardPassTurnUndo* undo) {
    EspNativeGameplayPlayerState current;
    uint32_t finalFNV;
    int feedbackExact = 1;
    int playerExact;

    if (undo == NULL || undo->committed == 0U ||
        !EspNativeGameplayPlayerState_snapshot(&current)) {
        return 0;
    }
    current.param1 = undo->param1Before;
    if (!EspNativeGameplayPlayerState_restore(&current)) return 0;
    if (undo->feedbackQueued != 0U) {
        feedbackExact = EspNativeGameplayActionEngine_cancelQueuedFeedback(
            ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DAMAGE);
    }
    finalFNV = EspNativeGameplayPlayerState_fingerprint();
    playerExact = finalFNV == undo->playerFNVBefore;
    printf("[HAZARDPASS] ROLLBACK tile=%u playerFNV=%08x expected=%08x exact=%s feedbackRollback=%s\n",
           (unsigned int)undo->tileIndex,
           (unsigned int)finalFNV,
           (unsigned int)undo->playerFNVBefore,
           playerExact ? "yes" : "NO",
           feedbackExact ? "yes" : "NO");
    return playerExact && feedbackExact;
}
