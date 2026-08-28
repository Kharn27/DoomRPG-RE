#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_dialog.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_interaction_inventory.h"
#include "esp_native_gameplay_pickup.h"
#include "esp_native_gameplay_session.h"
#include "esp_native_resident_gameplay.h"
#include "esp_player_view_state.h"

#define PICKUP_ENTITY_TYPE_WORLD_ITEM 3U
#define PICKUP_ENTITY_TYPE_INVENTORY 4U
#define PICKUP_ENTITY_TYPE_WEAPON 5U
#define PICKUP_ENTITY_TYPE_AMMO 6U
#define PICKUP_ENTITY_TYPE_AMMO_ALT 16U
#define PICKUP_HIDDEN_FLAG 0x00010000UL
#define PICKUP_WEAPON_LIMIT 12U

typedef struct EspNativeGameplayPickupState_s {
    uint8_t* consumedBits;
    uint32_t consumedBytes;
    uint32_t spriteCount;
    uint32_t arenaFNV;
    uint16_t knownWeapons;
    uint8_t targetMapId;
    uint8_t selectedWeapon;
    uint8_t selectedOverride;
    uint8_t ready;
    uint8_t corpusLogged;
    uint8_t pendingMove;
    uint8_t fatal;
    uint8_t reserved;
    EspPlayerViewState pendingBefore;
    EspPlayerViewState pendingAfter;
} EspNativeGameplayPickupState;

static EspNativeGameplayPickupState pickup;
static EspNativeGameplayHudState hudOverlay;

int __real_EspMapRuntime_getMapSprite(uint32_t index,
                                      EspMapSprite* outSprite);
const EspNativeGameplayHudState* __real_EspNativeGameplayHud_view(void);
EspPlayerViewMoveStatus __real_EspPlayerView_commitPreparedMove(
    const EspPlayerViewState* expectedBefore,
    const EspPlayerViewState* preparedAfter);
void __real_EspNativeGameplaySession_service(struct DoomRPG_s* doomRpg);
void __real_EspNativeGameplaySession_reset(void);

static uint16_t tileForView(const EspPlayerViewState* view) {
    uint32_t x;
    uint32_t y;
    if (view == NULL || view->viewX < 0 || view->viewY < 0) return UINT16_MAX;
    x = (uint32_t)view->viewX >> 6;
    y = (uint32_t)view->viewY >> 6;
    if (x >= 32U || y >= 32U) return UINT16_MAX;
    return (uint16_t)(y * 32U + x);
}

static int consumed(uint32_t spriteIndex) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    return pickup.ready == 1U && runtime != NULL && pickup.arenaFNV != 0U &&
           runtime->arenaFNV1a == pickup.arenaFNV &&
           pickup.consumedBits != NULL && spriteIndex < pickup.spriteCount &&
           ((pickup.consumedBits[spriteIndex >> 3] >>
             (spriteIndex & 7U)) & 1U) != 0U;
}

static void setConsumed(uint32_t spriteIndex, int value) {
    uint8_t mask;
    if (pickup.consumedBits == NULL || spriteIndex >= pickup.spriteCount) return;
    mask = (uint8_t)(1U << (spriteIndex & 7U));
    if (value) pickup.consumedBits[spriteIndex >> 3] |= mask;
    else pickup.consumedBits[spriteIndex >> 3] &= (uint8_t)~mask;
}

static void clearPendingMove(void) {
    pickup.pendingMove = 0U;
    memset(&pickup.pendingBefore, 0, sizeof(pickup.pendingBefore));
    memset(&pickup.pendingAfter, 0, sizeof(pickup.pendingAfter));
}

static int ensureOwner(uint8_t targetMapId) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspNativeGameplayHudState* hud = __real_EspNativeGameplayHud_view();
    uint32_t bytes;
    uint8_t* next;

    if (runtime == NULL || runtime->arenaFNV1a == 0U ||
        hud == NULL || hud->active != 1U || hud->painted != 1U ||
        targetMapId == 0U || hud->model.targetMapId != targetMapId ||
        runtime->mapSpriteCount == 0U || runtime->mapSpriteCount > UINT16_MAX) {
        return 0;
    }
    bytes = (runtime->mapSpriteCount + 7U) >> 3;

    if (!pickup.ready || pickup.targetMapId != targetMapId ||
        pickup.arenaFNV != runtime->arenaFNV1a ||
        pickup.spriteCount != runtime->mapSpriteCount) {
        if (pickup.consumedBytes < bytes) {
            next = (uint8_t*)heap_caps_realloc(pickup.consumedBits,
                                               bytes,
                                               MALLOC_CAP_8BIT);
            if (next == NULL) return 0;
            pickup.consumedBits = next;
            pickup.consumedBytes = bytes;
        }
        memset(pickup.consumedBits, 0, bytes);
        pickup.spriteCount = runtime->mapSpriteCount;
        pickup.arenaFNV = runtime->arenaFNV1a;
        pickup.targetMapId = targetMapId;
        pickup.knownWeapons = 0U;
        if (hud->model.weaponsPresent != 0U &&
            hud->model.weapon < PICKUP_WEAPON_LIMIT) {
            pickup.knownWeapons = (uint16_t)(1U << hud->model.weapon);
        }
        pickup.selectedWeapon = hud->model.weapon;
        pickup.selectedOverride = 0U;
        pickup.ready = 1U;
        pickup.corpusLogged = 0U;
        printf("[PICKUP] OWNER map=%u arena=%08x sprites=%u consumedBytes=%u knownWeapons=%04x selected=%u allocation=lazy-gameplay\n",
               (unsigned int)targetMapId,
               (unsigned int)pickup.arenaFNV,
               (unsigned int)runtime->mapSpriteCount,
               (unsigned int)bytes,
               (unsigned int)pickup.knownWeapons,
               (unsigned int)pickup.selectedWeapon);
    }
    return 1;
}

void EspNativeGameplayPickup_reset(void) {
    if (pickup.consumedBits != NULL) heap_caps_free(pickup.consumedBits);
    memset(&pickup, 0, sizeof(pickup));
    memset(&hudOverlay, 0, sizeof(hudOverlay));
}

int __wrap_EspMapRuntime_getMapSprite(uint32_t index,
                                     EspMapSprite* outSprite) {
    if (!__real_EspMapRuntime_getMapSprite(index, outSprite)) return 0;
    if (outSprite != NULL && consumed(index)) {
        outSprite->info |= PICKUP_HIDDEN_FLAG;
    }
    return 1;
}

const EspNativeGameplayHudState* __wrap_EspNativeGameplayHud_view(void) {
    const EspNativeGameplayHudState* base = __real_EspNativeGameplayHud_view();
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    if (base == NULL || runtime == NULL || pickup.ready != 1U ||
        pickup.arenaFNV != runtime->arenaFNV1a ||
        pickup.selectedOverride != 1U ||
        base->model.targetMapId != pickup.targetMapId ||
        pickup.selectedWeapon >= PICKUP_WEAPON_LIMIT) {
        return base;
    }
    hudOverlay = *base;
    hudOverlay.model.weapon = pickup.selectedWeapon;
    hudOverlay.model.weaponsPresent = 1U;
    return &hudOverlay;
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
        printf("[PICKUP] RENDER-FAILED reason=%s angle=%d\n",
               reason != NULL ? reason : "pickup",
               (int)view->viewAngle);
        return 0;
    }
    printf("[PICKUP] FRAME reason=%s frame=%08x totalUs=%u presented=%u\n",
           reason != NULL ? reason : "pickup",
           (unsigned int)frame.frameAfterFNV,
           (unsigned int)frame.totalMicros,
           (unsigned int)frame.finalPresented);
    return frame.active == 1U && frame.finalPresented == 1U;
}

static int findWeaponOnTile(uint16_t tile,
                            uint16_t* outSprite,
                            uint8_t* outSubtype) {
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    uint32_t i;
    if (outSprite != NULL) *outSprite = ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE;
    if (outSubtype != NULL) *outSubtype = 0U;
    if (topology == NULL || outSprite == NULL || outSubtype == NULL) return 0;

    for (i = 0U; i < topology->spriteCount; ++i) {
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t linkOrder;
        if (consumed(i) ||
            !EspMapSpriteTopology_getEntity(i, &type, &subtype,
                                            &linkState, &linkOrder)) {
            continue;
        }
        (void)linkOrder;
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) == 0U ||
            (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) != tile ||
            type != PICKUP_ENTITY_TYPE_WEAPON) {
            continue;
        }
        *outSprite = (uint16_t)i;
        *outSubtype = subtype;
        return 1;
    }
    return 0;
}

static int processCommittedMove(struct DoomRPG_s* doomRpgBase,
                                const EspPlayerViewState* beforeView,
                                const EspPlayerViewState* afterView) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    uint16_t beforeTile;
    uint16_t afterTile;
    uint16_t spriteIndex;
    uint16_t weaponsBefore;
    uint8_t subtype;
    uint8_t selectedBefore;
    uint8_t overrideBefore;
    int newToNativeOwner;

    if (beforeView == NULL || afterView == NULL ||
        beforeView->active != 1U || afterView->active != 1U ||
        beforeView->targetMapId != afterView->targetMapId) return 1;
    beforeTile = tileForView(beforeView);
    afterTile = tileForView(afterView);
    if (beforeTile == UINT16_MAX || afterTile == UINT16_MAX ||
        beforeTile == afterTile) return 1;
    if (!ensureOwner(afterView->targetMapId)) {
        printf("[PICKUP] DEFER tile=%u reason=owner-not-ready\n",
               (unsigned int)afterTile);
        return 1;
    }
    if (!findWeaponOnTile(afterTile, &spriteIndex, &subtype)) return 1;
    if (subtype >= PICKUP_WEAPON_LIMIT) {
        printf("[PICKUP] DEFER tile=%u sprite=%u type=5 subtype=%u reason=weapon-range\n",
               (unsigned int)afterTile,
               (unsigned int)spriteIndex,
               (unsigned int)subtype);
        return 1;
    }

    weaponsBefore = pickup.knownWeapons;
    selectedBefore = pickup.selectedWeapon;
    overrideBefore = pickup.selectedOverride;
    newToNativeOwner =
        (pickup.knownWeapons & (uint16_t)(1U << subtype)) == 0U;

    setConsumed(spriteIndex, 1);
    pickup.knownWeapons |= (uint16_t)(1U << subtype);
    if (newToNativeOwner) {
        pickup.selectedWeapon = subtype;
        pickup.selectedOverride = 1U;
    }

    printf("[PICKUP] WEAPON tile=%u sprite=%u subtype=%u new=%s weapons=%04x->%04x selected=%u->%u worldRemove=overlay ammoOwner=deferred legacyDialog=deferred\n",
           (unsigned int)afterTile,
           (unsigned int)spriteIndex,
           (unsigned int)subtype,
           newToNativeOwner ? "yes" : "no",
           (unsigned int)weaponsBefore,
           (unsigned int)pickup.knownWeapons,
           (unsigned int)selectedBefore,
           (unsigned int)pickup.selectedWeapon);

    if (rerender(doomRpg, afterView, "WEAPON-PICKUP")) return 1;

    setConsumed(spriteIndex, 0);
    pickup.knownWeapons = weaponsBefore;
    pickup.selectedWeapon = selectedBefore;
    pickup.selectedOverride = overrideBefore;
    printf("[PICKUP] ROLLBACK tile=%u sprite=%u state=restored\n",
           (unsigned int)afterTile,
           (unsigned int)spriteIndex);
    if (!rerender(doomRpg, afterView, "WEAPON-PICKUP-ROLLBACK")) {
        printf("[PICKUP] FAILED tile=%u sprite=%u reason=rollback-render fatal=1\n",
               (unsigned int)afterTile,
               (unsigned int)spriteIndex);
        pickup.fatal = 1U;
        return 0;
    }
    return 1;
}

EspPlayerViewMoveStatus __wrap_EspPlayerView_commitPreparedMove(
    const EspPlayerViewState* expectedBefore,
    const EspPlayerViewState* preparedAfter) {
    EspPlayerViewMoveStatus status =
        __real_EspPlayerView_commitPreparedMove(expectedBefore, preparedAfter);

    if (status != ESP_PLAYER_VIEW_MOVE_OK || expectedBefore == NULL ||
        preparedAfter == NULL) {
        return status;
    }

    /* Dispatch rollback is the exact reverse commit of the pending move. Do not
     * publish that reverse pair as a second pickup candidate. */
    if (pickup.pendingMove == 1U &&
        memcmp(expectedBefore, &pickup.pendingAfter,
               sizeof(*expectedBefore)) == 0 &&
        memcmp(preparedAfter, &pickup.pendingBefore,
               sizeof(*preparedAfter)) == 0) {
        clearPendingMove();
        return status;
    }

    pickup.pendingBefore = *expectedBefore;
    pickup.pendingAfter = *preparedAfter;
    pickup.pendingMove = 1U;
    return status;
}

static void servicePendingMove(struct DoomRPG_s* doomRpg) {
    const EspPlayerViewState* live;
    EspPlayerViewState before;
    EspPlayerViewState after;

    if (pickup.pendingMove != 1U) return;
    if (!EspNativeResidentGameplay_isActive()) {
        clearPendingMove();
        return;
    }
    if (EspNativeGameplayDialog_isActive()) return;

    live = EspPlayerView_view();
    if (live == NULL ||
        memcmp(live, &pickup.pendingAfter, sizeof(*live)) != 0) {
        printf("[PICKUP] DROP-PENDING reason=stale-view\n");
        clearPendingMove();
        return;
    }

    before = pickup.pendingBefore;
    after = pickup.pendingAfter;
    clearPendingMove();
    (void)processCommittedMove(doomRpg, &before, &after);
}

void EspNativeGameplayPickup_logCorpus(void) {
    const EspPlayerViewState* view = EspPlayerView_view();
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    uint32_t counts[17];
    uint32_t i;
    uint32_t pickupTotal = 0U;

    if (view == NULL || view->active != 1U || topology == NULL ||
        !ensureOwner(view->targetMapId) || pickup.corpusLogged) return;
    memset(counts, 0, sizeof(counts));

    for (i = 0U; i < topology->spriteCount; ++i) {
        uint8_t type;
        uint8_t subtype;
        uint16_t linkState;
        uint16_t order;
        if (!EspMapSpriteTopology_getEntity(i, &type, &subtype,
                                            &linkState, &order)) continue;
        (void)subtype;
        (void)order;
        if ((linkState & ESP_MAP_SPRITE_TOPOLOGY_EXISTS) == 0U) continue;
        if (type < 17U) ++counts[type];
        if (type == PICKUP_ENTITY_TYPE_WORLD_ITEM ||
            type == PICKUP_ENTITY_TYPE_INVENTORY ||
            type == PICKUP_ENTITY_TYPE_WEAPON ||
            type == PICKUP_ENTITY_TYPE_AMMO ||
            type == PICKUP_ENTITY_TYPE_AMMO_ALT) {
            ++pickupTotal;
        }
    }

    printf("[PICKUPCORPUS] READY map=%u arena=%08x sprites=%u pickupEntities=%u weaponType5=%u worldType3=%u inventoryType4=%u ammoType6=%u ammoType16=%u\n",
           (unsigned int)view->targetMapId,
           (unsigned int)pickup.arenaFNV,
           (unsigned int)topology->spriteCount,
           (unsigned int)pickupTotal,
           (unsigned int)counts[PICKUP_ENTITY_TYPE_WEAPON],
           (unsigned int)counts[PICKUP_ENTITY_TYPE_WORLD_ITEM],
           (unsigned int)counts[PICKUP_ENTITY_TYPE_INVENTORY],
           (unsigned int)counts[PICKUP_ENTITY_TYPE_AMMO],
           (unsigned int)counts[PICKUP_ENTITY_TYPE_AMMO_ALT]);
    printf("[PICKUPCORPUS] ROUTES type5=owned/remove+select type3=DEFERRED-player-stats type4=DEFERRED-inventory type6/16=DEFERRED-ammo\n");
    pickup.corpusLogged = 1U;
}

void __wrap_EspNativeGameplaySession_reset(void) {
    EspNativeGameplayPickup_reset();
    __real_EspNativeGameplaySession_reset();
}

void __wrap_EspNativeGameplaySession_service(struct DoomRPG_s* doomRpg) {
    if (pickup.fatal != 0U) return;
    __real_EspNativeGameplaySession_service(doomRpg);
    EspNativeGameplayPickup_logCorpus();
    EspNativeGameplayInteractionInventory_log();
    servicePendingMove(doomRpg);
}
