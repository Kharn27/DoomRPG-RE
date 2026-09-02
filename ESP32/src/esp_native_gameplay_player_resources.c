#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include <esp_heap_caps.h>

#include "esp_entity_def_type_catalog.h"
#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_combat_math.h"
#include "esp_native_gameplay_dialog.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_interaction_inventory.h"
#include "esp_native_gameplay_player_resources.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_gameplay_session.h"
#include "esp_native_resident_gameplay.h"
#include "esp_player_view_state.h"

#define RESOURCE_TYPE_WORLD_ITEM 3U
#define RESOURCE_TYPE_INVENTORY 4U
#define RESOURCE_TYPE_WEAPON 5U
#define RESOURCE_TYPE_AMMO 6U
#define RESOURCE_TYPE_AMMO_ALT 16U

#define RESOURCE_WORLD_HEALTH 20U
#define RESOURCE_WORLD_ARMOR 21U
#define RESOURCE_WORLD_CREDITS_1 22U
#define RESOURCE_WORLD_CREDITS_5 23U
#define RESOURCE_WORLD_KEY 24U
#define RESOURCE_INVENTORY_BASE 25U

#define RESOURCE_DEF_MASK 511U
#define RESOURCE_DEF_TILE_FLAG 0x00040000UL
#define RESOURCE_DEF_TILE_BASE 305U
#define RESOURCE_MAX_TOUCHES_PER_TILE 16U

#define RESOURCE_ACTION_NONE 0U
#define RESOURCE_ACTION_HEALTH 1U
#define RESOURCE_ACTION_ARMOR 2U
#define RESOURCE_ACTION_CREDITS 3U
#define RESOURCE_ACTION_KEY 4U
#define RESOURCE_ACTION_INVENTORY 5U
#define RESOURCE_ACTION_AMMO 6U
#define RESOURCE_ACTION_WEAPON 7U

typedef struct ResourceCandidate_s {
    int32_t parm;
    uint16_t spriteIndex;
    uint16_t defTile;
    uint16_t linkOrder;
    uint8_t type;
    uint8_t subtype;
} ResourceCandidate;

typedef struct ResourceApplied_s {
    uint32_t beforeValue;
    uint32_t afterValue;
    uint16_t spriteIndex;
    uint8_t type;
    uint8_t subtype;
    uint8_t action;
    uint8_t slot;
} ResourceApplied;

typedef struct ResourceOwner_s {
    EspNativeGameplayPlayerResourcesView view;
    uint8_t* consumedBits;
    uint32_t consumedCapacity;
    EspPlayerViewState pendingBefore;
    EspPlayerViewState pendingAfter;
    uint8_t corpusLogged;
} ResourceOwner;

static ResourceOwner resources;
static EspNativeGameplayHudState hudOverlay;

int __real_EspMapRuntime_getMapSprite(uint32_t index,
                                      EspMapSprite* outSprite);
const EspNativeGameplayHudState* __real_EspNativeGameplayHud_view(void);
EspPlayerViewMoveStatus __real_EspPlayerView_commitPreparedMove(
    const EspPlayerViewState* expectedBefore,
    const EspPlayerViewState* preparedAfter);
void __real_EspNativeGameplaySession_service(struct DoomRPG_s* doomRpg);
void __real_EspNativeGameplaySession_reset(void);

/* Historical bring-up owner is now dormant, but reset it defensively so a
 * branch/session transition can never retain stale compatibility state. */
void EspNativeGameplayPickup_reset(void);

static uint16_t tileForView(const EspPlayerViewState* view) {
    uint32_t x;
    uint32_t y;
    if (view == NULL || view->viewX < 0 || view->viewY < 0) return UINT16_MAX;
    x = (uint32_t)view->viewX >> 6;
    y = (uint32_t)view->viewY >> 6;
    if (x >= 32U || y >= 32U) return UINT16_MAX;
    return (uint16_t)(y * 32U + x);
}

static int isResourceType(uint8_t type) {
    return type == RESOURCE_TYPE_WORLD_ITEM ||
           type == RESOURCE_TYPE_INVENTORY ||
           type == RESOURCE_TYPE_WEAPON ||
           type == RESOURCE_TYPE_AMMO ||
           type == RESOURCE_TYPE_AMMO_ALT;
}

static int consumed(uint32_t spriteIndex) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    return resources.view.active == 1U && runtime != NULL &&
           resources.view.sourceArenaFNV1a == runtime->arenaFNV1a &&
           resources.consumedBits != NULL &&
           spriteIndex < resources.view.spriteCount &&
           ((resources.consumedBits[spriteIndex >> 3] >>
             (spriteIndex & 7U)) & 1U) != 0U;
}

static void setConsumed(uint32_t spriteIndex, int value) {
    uint8_t mask;
    int was;
    if (resources.consumedBits == NULL ||
        spriteIndex >= resources.view.spriteCount) return;
    mask = (uint8_t)(1U << (spriteIndex & 7U));
    was = (resources.consumedBits[spriteIndex >> 3] & mask) != 0U;
    if (value) {
        resources.consumedBits[spriteIndex >> 3] |= mask;
        if (!was) ++resources.view.consumedCount;
    }
    else {
        resources.consumedBits[spriteIndex >> 3] &= (uint8_t)~mask;
        if (was && resources.view.consumedCount != 0U) {
            --resources.view.consumedCount;
        }
    }
}

int EspNativeGameplayPlayerResources_isConsumed(uint32_t spriteIndex) {
    return consumed(spriteIndex);
}

static void clearPending(void) {
    resources.view.pendingMove = 0U;
    memset(&resources.pendingBefore, 0, sizeof(resources.pendingBefore));
    memset(&resources.pendingAfter, 0, sizeof(resources.pendingAfter));
}

void EspNativeGameplayPlayerResources_reset(void) {
    if (resources.consumedBits != NULL) heap_caps_free(resources.consumedBits);
    memset(&resources, 0, sizeof(resources));
    memset(&hudOverlay, 0, sizeof(hudOverlay));
}

static int ensureOwner(uint8_t targetMapId) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint32_t bytes;
    uint8_t* next;

    if (runtime == NULL || runtime->arenaFNV1a == 0U ||
        runtime->mapSpriteCount == 0U || runtime->mapSpriteCount > UINT16_MAX ||
        targetMapId == 0U || !EspEntityDefTypeCatalog_isReady() ||
        !EspNativeGameplayPlayerState_ensure()) {
        return 0;
    }
    bytes = (runtime->mapSpriteCount + 7U) >> 3;

    if (resources.view.active == 0U ||
        resources.view.sourceArenaFNV1a != runtime->arenaFNV1a ||
        resources.view.targetMapId != targetMapId ||
        resources.view.spriteCount != runtime->mapSpriteCount) {
        if (resources.consumedCapacity < bytes) {
            next = (uint8_t*)heap_caps_realloc(resources.consumedBits,
                                               bytes,
                                               MALLOC_CAP_8BIT);
            if (next == NULL) return 0;
            resources.consumedBits = next;
            resources.consumedCapacity = bytes;
        }
        memset(resources.consumedBits, 0, bytes);
        resources.view.sourceArenaFNV1a = runtime->arenaFNV1a;
        resources.view.spriteCount = runtime->mapSpriteCount;
        resources.view.consumedBytes = bytes;
        resources.view.consumedCount = 0U;
        resources.view.playerFNV1a =
            EspNativeGameplayPlayerState_fingerprint();
        resources.view.targetMapId = targetMapId;
        resources.view.pendingMove = 0U;
        resources.view.active = 1U;
        resources.view.fatal = 0U;
        resources.corpusLogged = 0U;
        memset(&resources.pendingBefore, 0, sizeof(resources.pendingBefore));
        memset(&resources.pendingAfter, 0, sizeof(resources.pendingAfter));
        printf("[PLAYERRES] READY map=%u arena=%08x sprites=%u consumedBytes=%u playerBytes=%u playerFNV=%08x entityDefCache=%uB families=3/4/5/6/16\n",
               (unsigned int)targetMapId,
               (unsigned int)runtime->arenaFNV1a,
               (unsigned int)runtime->mapSpriteCount,
               (unsigned int)bytes,
               (unsigned int)sizeof(EspNativeGameplayPlayerState),
               (unsigned int)resources.view.playerFNV1a,
               (unsigned int)(ESP_ENTITY_DEF_TYPE_CATALOG_MAX_DEFINITIONS * 8U));
    }
    return 1;
}

const EspNativeGameplayPlayerResourcesView*
EspNativeGameplayPlayerResources_view(void) {
    const EspPlayerViewState* view = EspPlayerView_view();
    if (view == NULL || view->active != 1U || !ensureOwner(view->targetMapId)) {
        return NULL;
    }
    resources.view.playerFNV1a = EspNativeGameplayPlayerState_fingerprint();
    return &resources.view;
}

static int rawDefTile(uint32_t spriteIndex, uint16_t* outDefTile) {
    EspMapSprite sprite;
    uint32_t lookup;
    if (outDefTile == NULL ||
        !__real_EspMapRuntime_getMapSprite(spriteIndex, &sprite)) return 0;
    lookup = sprite.info & RESOURCE_DEF_MASK;
    if ((sprite.info & RESOURCE_DEF_TILE_FLAG) != 0U) {
        lookup += RESOURCE_DEF_TILE_BASE;
    }
    if (lookup >= ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT) return 0;
    *outDefTile = (uint16_t)lookup;
    return 1;
}

static int collectCandidates(uint16_t tile,
                             ResourceCandidate out[RESOURCE_MAX_TOUCHES_PER_TILE],
                             uint8_t* outCount) {
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    uint32_t i;
    uint8_t count = 0U;

    if (outCount != NULL) *outCount = 0U;
    if (topology == NULL || out == NULL || outCount == NULL) return 0;

    for (i = 0U; i < topology->spriteCount; ++i) {
        uint8_t type;
        uint8_t subtype;
        uint8_t metaType;
        uint8_t metaSubtype;
        uint16_t linkState;
        uint16_t linkOrder;
        uint16_t defTile;
        int32_t parm;
        uint8_t pos;

        if (consumed(i) ||
            !EspMapSpriteTopology_getEntity(i, &type, &subtype,
                                            &linkState, &linkOrder) ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != tile ||
            !isResourceType(type)) {
            continue;
        }
        if (count >= RESOURCE_MAX_TOUCHES_PER_TILE) {
            printf("[PLAYERRES] DEFER tile=%u reason=touch-overflow max=%u mutation=no\n",
                   (unsigned int)tile,
                   (unsigned int)RESOURCE_MAX_TOUCHES_PER_TILE);
            return 0;
        }
        if (!rawDefTile(i, &defTile) ||
            !EspEntityDefTypeCatalog_getMetadata(defTile,
                                                 &metaType,
                                                 &metaSubtype,
                                                 &parm) ||
            metaType != type || metaSubtype != subtype) {
            printf("[PLAYERRES] DEFER tile=%u sprite=%u type=%u subtype=%u reason=entitydef-metadata mutation=no\n",
                   (unsigned int)tile,
                   (unsigned int)i,
                   (unsigned int)type,
                   (unsigned int)subtype);
            return 0;
        }

        pos = count;
        while (pos > 0U && linkOrder > out[pos - 1U].linkOrder) {
            out[pos] = out[pos - 1U];
            --pos;
        }
        out[pos].parm = parm;
        out[pos].spriteIndex = (uint16_t)i;
        out[pos].defTile = defTile;
        out[pos].linkOrder = linkOrder;
        out[pos].type = type;
        out[pos].subtype = subtype;
        ++count;
    }
    *outCount = count;
    return 1;
}

static int byteParm(int32_t parm, uint8_t* outValue) {
    if (outValue == NULL || parm < 0 || parm > 255) return 0;
    *outValue = (uint8_t)parm;
    return 1;
}

static int applyCandidate(const ResourceCandidate* candidate,
                          ResourceApplied* applied) {
    const EspNativeGameplayPlayerState* player;
    const EspNativeGameplayWeaponSpec* weapon;
    uint32_t before;
    uint32_t after;
    uint32_t mask;
    uint16_t weaponsBefore;
    uint8_t value;
    uint8_t added = 0U;
    uint8_t slot;
    int take = 0;

    if (candidate == NULL || applied == NULL ||
        !EspNativeGameplayPlayerState_ensure()) return -1;
    memset(applied, 0, sizeof(*applied));
    applied->spriteIndex = candidate->spriteIndex;
    applied->type = candidate->type;
    applied->subtype = candidate->subtype;
    applied->slot = 0xffU;
    player = EspNativeGameplayPlayerState_view();
    if (player == NULL) return -1;

    switch (candidate->type) {
    case RESOURCE_TYPE_WORLD_ITEM:
        switch (candidate->subtype) {
        case RESOURCE_WORLD_HEALTH:
            if (!byteParm(candidate->parm, &value)) return -1;
            applied->action = RESOURCE_ACTION_HEALTH;
            before = player->param1 & 0xffU;
            if (!EspNativeGameplayPlayerState_addHealth(value, &added)) break;
            after = EspNativeGameplayPlayerState_health();
            applied->beforeValue = before;
            applied->afterValue = after;
            take = 1;
            break;
        case RESOURCE_WORLD_ARMOR:
            if (!byteParm(candidate->parm, &value)) return -1;
            applied->action = RESOURCE_ACTION_ARMOR;
            before = (player->param1 >> 16) & 0xffU;
            if (!EspNativeGameplayPlayerState_addArmor(value, &added)) break;
            after = EspNativeGameplayPlayerState_armor();
            applied->beforeValue = before;
            applied->afterValue = after;
            take = 1;
            break;
        case RESOURCE_WORLD_CREDITS_1:
        case RESOURCE_WORLD_CREDITS_5:
            if (candidate->parm < 0) return -1;
            applied->action = RESOURCE_ACTION_CREDITS;
            before = player->credits;
            if (!EspNativeGameplayPlayerState_addCredits(
                    (uint32_t)candidate->parm)) return -1;
            player = EspNativeGameplayPlayerState_view();
            if (player == NULL) return -1;
            applied->beforeValue = before;
            applied->afterValue = player->credits;
            take = 1;
            break;
        case RESOURCE_WORLD_KEY:
            if (candidate->parm < 0 || candidate->parm >= 32) return -1;
            applied->action = RESOURCE_ACTION_KEY;
            mask = 1UL << (uint32_t)candidate->parm;
            before = player->keys;
            if (!EspNativeGameplayPlayerState_addKeys(mask)) return -1;
            player = EspNativeGameplayPlayerState_view();
            if (player == NULL) return -1;
            applied->beforeValue = before;
            applied->afterValue = player->keys;
            take = 1;
            break;
        default:
            /* Entity_touched() has no default mutation for type 3, but still
             * plays the pickup feedback and removes the entity. Preserve that
             * data-driven remove behavior rather than inventing a subtype gate. */
            applied->action = RESOURCE_ACTION_NONE;
            take = 1;
            break;
        }
        break;

    case RESOURCE_TYPE_INVENTORY:
        if (candidate->subtype < RESOURCE_INVENTORY_BASE ||
            candidate->subtype >= RESOURCE_INVENTORY_BASE +
                                  ESP_NATIVE_GAMEPLAY_PLAYER_INVENTORY_SLOTS) {
            return -1;
        }
        slot = (uint8_t)(candidate->subtype - RESOURCE_INVENTORY_BASE);
        applied->action = RESOURCE_ACTION_INVENTORY;
        applied->slot = slot;
        before = player->inventory[slot];
        if (!EspNativeGameplayPlayerState_addInventory(slot, 1U, &added)) break;
        player = EspNativeGameplayPlayerState_view();
        if (player == NULL) return -1;
        applied->beforeValue = before;
        applied->afterValue = player->inventory[slot];
        take = 1;
        break;

    case RESOURCE_TYPE_AMMO:
    case RESOURCE_TYPE_AMMO_ALT:
        if (candidate->subtype >= ESP_NATIVE_GAMEPLAY_PLAYER_AMMO_TYPES ||
            !byteParm(candidate->parm, &value)) return -1;
        applied->action = RESOURCE_ACTION_AMMO;
        applied->slot = candidate->subtype;
        before = player->ammo[candidate->subtype];
        if (!EspNativeGameplayPlayerState_addAmmo(candidate->subtype,
                                                  value, &added)) break;
        player = EspNativeGameplayPlayerState_view();
        if (player == NULL) return -1;
        applied->beforeValue = before;
        applied->afterValue = player->ammo[candidate->subtype];
        take = 1;
        break;

    case RESOURCE_TYPE_WEAPON:
        if (candidate->subtype >= ESP_NATIVE_GAMEPLAY_PLAYER_WEAPON_LIMIT ||
            !byteParm(candidate->parm, &value)) return -1;
        applied->action = RESOURCE_ACTION_WEAPON;
        applied->slot = candidate->subtype;
        weaponsBefore = player->weapons;
        applied->beforeValue =
            ((uint32_t)player->weapon << 16) | weaponsBefore;
        if ((weaponsBefore & (uint16_t)(1U << candidate->subtype)) == 0U) {
            if (!EspNativeGameplayPlayerState_adoptWeapon(candidate->subtype)) {
                return -1;
            }
        }
        weapon = EspNativeGameplayCombatMath_weapon(candidate->subtype);
        if (weapon != NULL && weapon->ammoUsage != 0U) {
            (void)EspNativeGameplayPlayerState_addAmmo(weapon->ammoType,
                                                       value, &added);
        }
        player = EspNativeGameplayPlayerState_view();
        if (player == NULL) return -1;
        applied->afterValue =
            ((uint32_t)player->weapon << 16) | player->weapons;
        /* Weapon entities are always removed by legacy Entity_touched(), even
         * when duplicate-weapon bonus ammo is already at maximum. */
        take = 1;
        break;

    default:
        return -1;
    }

    return take;
}

static const char* actionName(uint8_t action) {
    switch (action) {
    case RESOURCE_ACTION_HEALTH: return "health";
    case RESOURCE_ACTION_ARMOR: return "armor";
    case RESOURCE_ACTION_CREDITS: return "credits";
    case RESOURCE_ACTION_KEY: return "key";
    case RESOURCE_ACTION_INVENTORY: return "inventory";
    case RESOURCE_ACTION_AMMO: return "ammo";
    case RESOURCE_ACTION_WEAPON: return "weapon";
    default: return "world-item";
    }
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
        printf("[PLAYERRES] RENDER-FAILED reason=%s angle=%d\n",
               reason != NULL ? reason : "pickup",
               (int)view->viewAngle);
        return 0;
    }
    printf("[PLAYERRES] FRAME reason=%s frame=%08x totalUs=%u presented=%u\n",
           reason != NULL ? reason : "pickup",
           (unsigned int)frame.frameAfterFNV,
           (unsigned int)frame.totalMicros,
           (unsigned int)frame.finalPresented);
    return frame.active == 1U && frame.finalPresented == 1U;
}

static int processCommittedMove(struct DoomRPG_s* doomRpgBase,
                                const EspPlayerViewState* beforeView,
                                const EspPlayerViewState* afterView) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    ResourceCandidate candidates[RESOURCE_MAX_TOUCHES_PER_TILE];
    ResourceApplied applied[RESOURCE_MAX_TOUCHES_PER_TILE];
    EspNativeGameplayPlayerState playerBefore;
    uint16_t beforeTile;
    uint16_t afterTile;
    uint8_t candidateCount = 0U;
    uint8_t appliedCount = 0U;
    uint8_t i;
    uint32_t playerFNVBefore;
    uint32_t playerFNVAfter;

    if (beforeView == NULL || afterView == NULL ||
        beforeView->active != 1U || afterView->active != 1U ||
        beforeView->targetMapId != afterView->targetMapId) return 1;
    beforeTile = tileForView(beforeView);
    afterTile = tileForView(afterView);
    if (beforeTile == UINT16_MAX || afterTile == UINT16_MAX ||
        beforeTile == afterTile) return 1;
    if (!ensureOwner(afterView->targetMapId)) {
        printf("[PLAYERRES] DEFER tile=%u reason=owner-not-ready mutation=no\n",
               (unsigned int)afterTile);
        return 1;
    }
    memset(candidates, 0, sizeof(candidates));
    memset(applied, 0, sizeof(applied));
    if (!collectCandidates(afterTile, candidates, &candidateCount)) return 1;
    if (candidateCount == 0U) return 1;
    if (!EspNativeGameplayPlayerState_snapshot(&playerBefore)) return 0;
    playerFNVBefore = EspNativeGameplayPlayerState_fingerprint();

    for (i = 0U; i < candidateCount; ++i) {
        int take = applyCandidate(&candidates[i], &applied[appliedCount]);
        if (take < 0) {
            (void)EspNativeGameplayPlayerState_restore(&playerBefore);
            while (appliedCount > 0U) {
                --appliedCount;
                setConsumed(applied[appliedCount].spriteIndex, 0);
            }
            printf("[PLAYERRES] DEFER tile=%u sprite=%u type=%u subtype=%u parm=%ld reason=unsupported-contract playerRollback=yes mutation=no\n",
                   (unsigned int)afterTile,
                   (unsigned int)candidates[i].spriteIndex,
                   (unsigned int)candidates[i].type,
                   (unsigned int)candidates[i].subtype,
                   (long)candidates[i].parm);
            return 1;
        }
        if (take == 0) {
            printf("[PLAYERRES] KEEP tile=%u sprite=%u type=%u subtype=%u parm=%ld reason=resource-at-maximum mutation=no\n",
                   (unsigned int)afterTile,
                   (unsigned int)candidates[i].spriteIndex,
                   (unsigned int)candidates[i].type,
                   (unsigned int)candidates[i].subtype,
                   (long)candidates[i].parm);
            continue;
        }
        setConsumed(candidates[i].spriteIndex, 1);
        printf("[PLAYERRES] PREPARE tile=%u sprite=%u defTile=%u type=%u subtype=%u parm=%ld action=%s value=%u->%u slot=%s%u worldRemove=hidden-overlay rollback=armed\n",
               (unsigned int)afterTile,
               (unsigned int)candidates[i].spriteIndex,
               (unsigned int)candidates[i].defTile,
               (unsigned int)candidates[i].type,
               (unsigned int)candidates[i].subtype,
               (long)candidates[i].parm,
               actionName(applied[appliedCount].action),
               (unsigned int)applied[appliedCount].beforeValue,
               (unsigned int)applied[appliedCount].afterValue,
               applied[appliedCount].slot == 0xffU ? "none/" : "index/",
               (unsigned int)(applied[appliedCount].slot == 0xffU
                                  ? 0U
                                  : applied[appliedCount].slot));
        ++appliedCount;
    }

    if (appliedCount == 0U) return 1;
    playerFNVAfter = EspNativeGameplayPlayerState_fingerprint();
    resources.view.playerFNV1a = playerFNVAfter;

    if (rerender(doomRpg, afterView, "RESOURCE-PICKUP")) {
        printf("[PLAYERRES] COMMIT tile=%u candidates=%u consumed=%u totalConsumed=%u playerFNV=%08x->%08x hp=%u/%u armor=%u/%u weapon=%u weapons=%04x ammo0=%u ammo1=%u ammo2=%u ammo3=%u ammo4=%u keys=%08x credits=%u sound=deferred message=deferred gotFace=deferred rollback=closed\n",
               (unsigned int)afterTile,
               (unsigned int)candidateCount,
               (unsigned int)appliedCount,
               (unsigned int)resources.view.consumedCount,
               (unsigned int)playerFNVBefore,
               (unsigned int)playerFNVAfter,
               (unsigned int)EspNativeGameplayPlayerState_health(),
               (unsigned int)EspNativeGameplayPlayerState_maxHealth(),
               (unsigned int)EspNativeGameplayPlayerState_armor(),
               (unsigned int)EspNativeGameplayPlayerState_maxArmor(),
               (unsigned int)EspNativeGameplayPlayerState_view()->weapon,
               (unsigned int)EspNativeGameplayPlayerState_weapons(),
               (unsigned int)EspNativeGameplayPlayerState_ammo(0U),
               (unsigned int)EspNativeGameplayPlayerState_ammo(1U),
               (unsigned int)EspNativeGameplayPlayerState_ammo(2U),
               (unsigned int)EspNativeGameplayPlayerState_ammo(3U),
               (unsigned int)EspNativeGameplayPlayerState_ammo(4U),
               (unsigned int)EspNativeGameplayPlayerState_view()->keys,
               (unsigned int)EspNativeGameplayPlayerState_view()->credits);
        return 1;
    }

    (void)EspNativeGameplayPlayerState_restore(&playerBefore);
    while (appliedCount > 0U) {
        --appliedCount;
        setConsumed(applied[appliedCount].spriteIndex, 0);
    }
    resources.view.playerFNV1a = EspNativeGameplayPlayerState_fingerprint();
    printf("[PLAYERRES] ROLLBACK tile=%u player=yes worldRemove=yes playerFNV=%08x exact=%s\n",
           (unsigned int)afterTile,
           (unsigned int)resources.view.playerFNV1a,
           resources.view.playerFNV1a == playerFNVBefore ? "yes" : "no");
    if (!rerender(doomRpg, afterView, "RESOURCE-PICKUP-ROLLBACK")) {
        printf("[PLAYERRES] FAILED tile=%u reason=rollback-render fatal=1\n",
               (unsigned int)afterTile);
        resources.view.fatal = 1U;
        return 0;
    }
    return 1;
}

static void logCorpus(void) {
    const EspPlayerViewState* view = EspPlayerView_view();
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    uint32_t counts[17];
    uint32_t i;
    uint32_t total = 0U;

    if (resources.corpusLogged || view == NULL || view->active != 1U ||
        topology == NULL || !ensureOwner(view->targetMapId)) return;
    memset(counts, 0, sizeof(counts));
    for (i = 0U; i < topology->spriteCount; ++i) {
        uint8_t type;
        uint8_t subtype;
        uint16_t state;
        uint16_t order;
        if (!EspMapSpriteTopology_getEntity(i, &type, &subtype,
                                            &state, &order)) continue;
        (void)subtype;
        (void)order;
        if ((state & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) == 0U) continue;
        if (type < 17U) ++counts[type];
        if (isResourceType(type)) ++total;
    }
    printf("[PLAYERRES] CORPUS map=%u arena=%08x pickups=%u type3=%u type4=%u type5=%u type6=%u type16=%u routes=all-generic playerOwner=shared\n",
           (unsigned int)view->targetMapId,
           (unsigned int)resources.view.sourceArenaFNV1a,
           (unsigned int)total,
           (unsigned int)counts[RESOURCE_TYPE_WORLD_ITEM],
           (unsigned int)counts[RESOURCE_TYPE_INVENTORY],
           (unsigned int)counts[RESOURCE_TYPE_WEAPON],
           (unsigned int)counts[RESOURCE_TYPE_AMMO],
           (unsigned int)counts[RESOURCE_TYPE_AMMO_ALT]);
    resources.corpusLogged = 1U;
}

static void servicePendingMove(struct DoomRPG_s* doomRpg) {
    const EspPlayerViewState* live;
    EspPlayerViewState before;
    EspPlayerViewState after;

    if (resources.view.pendingMove != 1U) return;
    if (!EspNativeResidentGameplay_isActive()) {
        clearPending();
        return;
    }
    if (EspNativeGameplayDialog_isActive()) return;

    live = EspPlayerView_view();
    if (live == NULL ||
        memcmp(live, &resources.pendingAfter, sizeof(*live)) != 0) {
        printf("[PLAYERRES] DROP-PENDING reason=stale-view\n");
        clearPending();
        return;
    }
    before = resources.pendingBefore;
    after = resources.pendingAfter;
    clearPending();
    (void)processCommittedMove(doomRpg, &before, &after);
}

EspPlayerViewMoveStatus __wrap_EspPlayerView_commitPreparedMove(
    const EspPlayerViewState* expectedBefore,
    const EspPlayerViewState* preparedAfter) {
    EspPlayerViewMoveStatus status =
        __real_EspPlayerView_commitPreparedMove(expectedBefore, preparedAfter);

    if (status != ESP_PLAYER_VIEW_MOVE_OK || expectedBefore == NULL ||
        preparedAfter == NULL) return status;

    if (resources.view.pendingMove == 1U &&
        memcmp(expectedBefore, &resources.pendingAfter,
               sizeof(*expectedBefore)) == 0 &&
        memcmp(preparedAfter, &resources.pendingBefore,
               sizeof(*preparedAfter)) == 0) {
        clearPending();
        return status;
    }

    resources.pendingBefore = *expectedBefore;
    resources.pendingAfter = *preparedAfter;
    resources.view.pendingMove = 1U;
    return status;
}

const EspNativeGameplayHudState* __wrap_EspNativeGameplayHud_view(void) {
    const EspNativeGameplayHudState* base = __real_EspNativeGameplayHud_view();
    const EspNativeGameplayPlayerState* player;
    const EspNativeGameplayWeaponSpec* weapon;

    if (base == NULL || base->active != 1U || base->painted != 1U ||
        !EspNativeGameplayPlayerState_ensure()) return base;
    player = EspNativeGameplayPlayerState_view();
    if (player == NULL) return base;

    hudOverlay = *base;
    hudOverlay.model.health = (uint8_t)(player->param1 & 0xffU);
    hudOverlay.model.maxHealth = (uint8_t)((player->param1 >> 8) & 0xffU);
    hudOverlay.model.armor = (uint8_t)((player->param1 >> 16) & 0xffU);
    hudOverlay.model.maxArmor = (uint8_t)((player->param1 >> 24) & 0xffU);
    hudOverlay.model.weapon = player->weapon;
    hudOverlay.model.weaponsPresent = player->weapons != 0U ? 1U : 0U;

    weapon = EspNativeGameplayCombatMath_weapon(player->weapon);
    if (weapon != NULL && weapon->ammoType < ESP_NATIVE_GAMEPLAY_PLAYER_AMMO_TYPES) {
        hudOverlay.model.ammoType = weapon->ammoType;
        hudOverlay.model.ammo = player->ammo[weapon->ammoType];
    }
    return &hudOverlay;
}

void __wrap_EspNativeGameplaySession_reset(void) {
    EspNativeGameplayActionEngine_reset();
    EspNativeGameplayPlayerResources_reset();
    EspNativeGameplayPickup_reset();
    __real_EspNativeGameplaySession_reset();
}

void __wrap_EspNativeGameplaySession_service(struct DoomRPG_s* doomRpg) {
    if (resources.view.fatal != 0U) return;
    __real_EspNativeGameplaySession_service(doomRpg);
    logCorpus();
    EspNativeGameplayInteractionInventory_log();
    servicePendingMove(doomRpg);
    if (!EspNativeGameplayActionEngine_service(doomRpg)) {
        printf("[ACTIONENGINE] FAILED reason=service fatal=1\n");
        resources.view.fatal = 1U;
    }
}
