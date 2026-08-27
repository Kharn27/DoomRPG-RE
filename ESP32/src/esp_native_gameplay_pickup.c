#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_map_runtime.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_pickup.h"
#include "esp_player_view_state.h"

#define PICKUP_ENTITY_TYPE_WORLD_ITEM 3U
#define PICKUP_ENTITY_TYPE_INVENTORY 4U
#define PICKUP_ENTITY_TYPE_WEAPON 5U
#define PICKUP_ENTITY_TYPE_AMMO 6U
#define PICKUP_ENTITY_TYPE_AMMO_ALT 16U

typedef struct EspNativeGameplayPickupState_s {
    uint8_t* consumedBits;
    uint32_t consumedBytes;
    uint32_t spriteCount;
    uint16_t weapons;
    uint8_t targetMapId;
    uint8_t ready;
    uint8_t corpusLogged;
    uint8_t reserved;
} EspNativeGameplayPickupState;

static EspNativeGameplayPickupState pickup;

int __real_EspMapRuntime_getMapSprite(uint32_t index,
                                      EspMapSprite* outSprite);

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
    return pickup.consumedBits != NULL && spriteIndex < pickup.spriteCount &&
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

static int ensureOwner(uint8_t targetMapId) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspNativeGameplayHudState* hud = EspNativeGameplayHud_view();
    uint32_t bytes;
    uint8_t* next;

    if (runtime == NULL || hud == NULL || runtime->mapSpriteCount == 0U ||
        runtime->mapSpriteCount > UINT16_MAX) return 0;
    bytes = (runtime->mapSpriteCount + 7U) >> 3;

    if (!pickup.ready || pickup.targetMapId != targetMapId ||
        pickup.spriteCount != runtime->mapSpriteCount) {
        if (pickup.consumedBytes < bytes) {
            next = (uint8_t*)SDL_realloc(pickup.consumedBits, bytes);
            if (next == NULL) return 0;
            pickup.consumedBits = next;
            pickup.consumedBytes = bytes;
        }
        memset(pickup.consumedBits, 0, bytes);
        pickup.spriteCount = runtime->mapSpriteCount;
        pickup.targetMapId = targetMapId;
        pickup.weapons = (uint16_t)(1U << hud->model.weapon);
        pickup.ready = 1U;
        pickup.corpusLogged = 0U;
        printf("[PICKUP] OWNER map=%u sprites=%u consumedBytes=%u weapons=%04x allocation=lazy-gameplay\n",
               (unsigned int)targetMapId,
               (unsigned int)runtime->mapSpriteCount,
               (unsigned int)bytes,
               (unsigned int)pickup.weapons);
    }
    return 1;
}

void EspNativeGameplayPickup_reset(void) {
    if (pickup.consumedBits != NULL) SDL_free(pickup.consumedBits);
    memset(&pickup, 0, sizeof(pickup));
}

int __wrap_EspMapRuntime_getMapSprite(uint32_t index,
                                     EspMapSprite* outSprite) {
    if (!__real_EspMapRuntime_getMapSprite(index, outSprite)) return 0;
    if (outSprite != NULL && consumed(index)) {
        outSprite->info |= 0x00010000UL;
    }
    return 1;
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
    return 1;
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

void EspNativeGameplayPickup_onServiceMove(
    struct DoomRPG_s* doomRpgBase,
    const EspPlayerViewState* beforeView,
    const EspPlayerViewState* afterView) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspNativeGameplayHudState* hudConst;
    EspNativeGameplayHudState* hud;
    uint16_t beforeTile;
    uint16_t afterTile;
    uint16_t spriteIndex;
    uint16_t weaponsBefore;
    uint8_t subtype;
    uint8_t weaponBefore;
    int isNew;

    if (beforeView == NULL || afterView == NULL ||
        beforeView->active != 1U || afterView->active != 1U ||
        beforeView->targetMapId != afterView->targetMapId) return;
    beforeTile = tileForView(beforeView);
    afterTile = tileForView(afterView);
    if (beforeTile == UINT16_MAX || afterTile == UINT16_MAX ||
        beforeTile == afterTile) return;
    if (!ensureOwner(afterView->targetMapId)) {
        printf("[PICKUP] DEFER tile=%u reason=owner-not-ready\n",
               (unsigned int)afterTile);
        return;
    }
    if (!findWeaponOnTile(afterTile, &spriteIndex, &subtype)) return;
    if (subtype >= 12U) {
        printf("[PICKUP] DEFER tile=%u sprite=%u type=5 subtype=%u reason=weapon-range\n",
               (unsigned int)afterTile,
               (unsigned int)spriteIndex,
               (unsigned int)subtype);
        return;
    }

    hudConst = EspNativeGameplayHud_view();
    if (hudConst == NULL) return;
    hud = (EspNativeGameplayHudState*)(uintptr_t)hudConst;
    weaponBefore = hud->model.weapon;
    weaponsBefore = pickup.weapons;
    isNew = (pickup.weapons & (uint16_t)(1U << subtype)) == 0U;

    setConsumed(spriteIndex, 1);
    pickup.weapons |= (uint16_t)(1U << subtype);
    if (isNew) hud->model.weapon = subtype;

    printf("[PICKUP] WEAPON tile=%u sprite=%u subtype=%u new=%s weapons=%04x->%04x selected=%u->%u worldRemove=overlay ammoOwner=deferred legacyDialog=deferred\n",
           (unsigned int)afterTile,
           (unsigned int)spriteIndex,
           (unsigned int)subtype,
           isNew ? "yes" : "no",
           (unsigned int)weaponsBefore,
           (unsigned int)pickup.weapons,
           (unsigned int)weaponBefore,
           (unsigned int)hud->model.weapon);

    if (!rerender(doomRpg, afterView, "WEAPON-PICKUP")) {
        setConsumed(spriteIndex, 0);
        pickup.weapons = weaponsBefore;
        hud->model.weapon = weaponBefore;
        printf("[PICKUP] ROLLBACK tile=%u sprite=%u exactState=yes\n",
               (unsigned int)afterTile,
               (unsigned int)spriteIndex);
        (void)rerender(doomRpg, afterView, "WEAPON-PICKUP-ROLLBACK");
    }
}

void EspNativeGameplayPickup_logCorpus(void) {
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();
    uint32_t counts[17];
    uint32_t i;
    uint32_t pickupTotal = 0U;

    if (topology == NULL) return;
    if (!ensureOwner(0U == pickup.targetMapId ? 1U : pickup.targetMapId)) return;
    if (pickup.corpusLogged) return;
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

    printf("[PICKUPCORPUS] READY sprites=%u pickupEntities=%u weaponType5=%u worldType3=%u inventoryType4=%u ammoType6=%u ammoType16=%u\n",
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
