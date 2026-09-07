#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_entity_def_type_catalog.h"
#include "esp_native_gameplay_hub_content.h"

#define HUB_CONTENT_ENTITY_TYPE_ITEM 4U
#define HUB_CONTENT_ENTITY_TYPE_WEAPON 5U
#define HUB_CONTENT_ITEM_SUBTYPE_BASE 25U
#define HUB_CONTENT_AMMO_TYPES 6U

/* Exact Combat_init player weapon ammoType/ammoUsage fields for ids 0..11.
 * This table is presentation metadata only and does not own combat semantics. */
static const uint8_t weaponAmmoType[ESP_NATIVE_GAMEPLAY_HUB_CONTENT_WEAPONS] = {
    0U, 0U, 1U, 2U, 1U, 2U, 4U, 3U, 4U, 5U, 5U, 5U
};
static const uint8_t weaponAmmoUsage[ESP_NATIVE_GAMEPLAY_HUB_CONTENT_WEAPONS] = {
    0U, 1U, 1U, 1U, 3U, 2U, 3U, 1U, 15U, 0U, 0U, 0U
};

static uint32_t fnvUpdate(uint32_t hash, const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static int resolveName(uint8_t type,
                       uint8_t subtype,
                       uint16_t* outTileIndex,
                       char* outName,
                       uint32_t capacity) {
    uint16_t tileIndex;
    if (outTileIndex == NULL || outName == NULL || capacity < 2U ||
        !EspEntityDefTypeCatalog_findTileIndex(type, subtype, &tileIndex) ||
        !EspEntityDefTypeCatalog_readName(tileIndex, outName, capacity) ||
        outName[0] == '\0' || EspAssetPack_isOpen()) {
        return 0;
    }
    *outTileIndex = tileIndex;
    return 1;
}

const char* EspNativeGameplayHubContent_ammoLabel(uint8_t ammoType) {
    switch (ammoType) {
    case 0U: return "Hal. Cans";
    case 1U: return "Bullets";
    case 2U: return "Shells";
    case 3U: return "Rockets";
    case 4U: return "Cells";
    case 5U: return "--";
    default: return NULL;
    }
}

int EspNativeGameplayHubContent_snapshot(
    const EspNativeGameplayPlayerState* player,
    EspNativeGameplayHubContent* outContent) {
    uint16_t tileIndex;
    uint8_t slot;

    if (player == NULL || outContent == NULL || player->active != 1U ||
        player->weapon >= ESP_NATIVE_GAMEPLAY_HUB_CONTENT_WEAPONS ||
        !EspEntityDefTypeCatalog_isReady() || EspAssetPack_isOpen()) {
        return 0;
    }

    memset(outContent, 0, sizeof(*outContent));
    if (!resolveName(HUB_CONTENT_ENTITY_TYPE_WEAPON,
                     player->weapon,
                     &tileIndex,
                     outContent->weaponName,
                     sizeof(outContent->weaponName))) {
        return 0;
    }

    outContent->weaponAmmoType = weaponAmmoType[player->weapon];
    outContent->weaponAmmoUsage = weaponAmmoUsage[player->weapon];
    if (outContent->weaponAmmoType >= HUB_CONTENT_AMMO_TYPES ||
        EspNativeGameplayHubContent_ammoLabel(outContent->weaponAmmoType) == NULL) {
        return 0;
    }
    outContent->weaponAmmoValue = player->ammo[outContent->weaponAmmoType];

    outContent->firstItemSlot = 0xffU;
    for (slot = 0U; slot < ESP_NATIVE_GAMEPLAY_HUB_CONTENT_ITEMS; ++slot) {
        if (player->inventory[slot] == 0U) continue;
        if (!resolveName(HUB_CONTENT_ENTITY_TYPE_ITEM,
                         (uint8_t)(HUB_CONTENT_ITEM_SUBTYPE_BASE + slot),
                         &tileIndex,
                         outContent->itemName,
                         sizeof(outContent->itemName))) {
            return 0;
        }
        outContent->firstItemSlot = slot;
        outContent->firstItemCount = player->inventory[slot];
        outContent->hasItem = 1U;
        break;
    }

    return !EspAssetPack_isOpen();
}

int EspNativeGameplayHubContent_probeCatalog(void) {
    uint32_t hash = 2166136261U;
    uint16_t tileIndex;
    uint8_t subtype;
    uint8_t resolvedWeapons = 0U;
    uint8_t resolvedItems = 0U;
    char name[ESP_NATIVE_GAMEPLAY_HUB_CONTENT_NAME_BYTES];

    if (!EspEntityDefTypeCatalog_isReady() || EspAssetPack_isOpen()) return 0;

    for (subtype = 0U;
         subtype < ESP_NATIVE_GAMEPLAY_HUB_CONTENT_WEAPONS;
         ++subtype) {
        uint8_t family = (uint8_t)'W';
        memset(name, 0, sizeof(name));
        if (!resolveName(HUB_CONTENT_ENTITY_TYPE_WEAPON,
                         subtype,
                         &tileIndex,
                         name,
                         sizeof(name))) {
            printf("[HUBCONTENT] DEFER family=weapon subtype=%u reason=entity-def-name\n",
                   (unsigned int)subtype);
            return 0;
        }
        hash = fnvUpdate(hash, &family, sizeof(family));
        hash = fnvUpdate(hash, &subtype, sizeof(subtype));
        hash = fnvUpdate(hash, &tileIndex, sizeof(tileIndex));
        hash = fnvUpdate(hash, name, (uint32_t)strlen(name) + 1U);
        ++resolvedWeapons;
        printf("[HUBCONTENT] DEF family=weapon subtype=%u tile=%u name=\"%s\" ammoType=%u ammoUsage=%u\n",
               (unsigned int)subtype,
               (unsigned int)tileIndex,
               name,
               (unsigned int)weaponAmmoType[subtype],
               (unsigned int)weaponAmmoUsage[subtype]);
    }

    for (subtype = HUB_CONTENT_ITEM_SUBTYPE_BASE;
         subtype < HUB_CONTENT_ITEM_SUBTYPE_BASE +
                       ESP_NATIVE_GAMEPLAY_HUB_CONTENT_ITEMS;
         ++subtype) {
        uint8_t family = (uint8_t)'I';
        memset(name, 0, sizeof(name));
        if (!resolveName(HUB_CONTENT_ENTITY_TYPE_ITEM,
                         subtype,
                         &tileIndex,
                         name,
                         sizeof(name))) {
            printf("[HUBCONTENT] DEFER family=item subtype=%u reason=entity-def-name\n",
                   (unsigned int)subtype);
            return 0;
        }
        hash = fnvUpdate(hash, &family, sizeof(family));
        hash = fnvUpdate(hash, &subtype, sizeof(subtype));
        hash = fnvUpdate(hash, &tileIndex, sizeof(tileIndex));
        hash = fnvUpdate(hash, name, (uint32_t)strlen(name) + 1U);
        ++resolvedItems;
        printf("[HUBCONTENT] DEF family=item subtype=%u tile=%u name=\"%s\"\n",
               (unsigned int)subtype,
               (unsigned int)tileIndex,
               name);
    }

    if (resolvedWeapons != ESP_NATIVE_GAMEPLAY_HUB_CONTENT_WEAPONS ||
        resolvedItems != ESP_NATIVE_GAMEPLAY_HUB_CONTENT_ITEMS ||
        hash == 0U || EspAssetPack_isOpen()) {
        return 0;
    }

    printf("[HUBCONTENT] READY weapons=%u/%u items=%u/%u names=pak-on-demand persistentNameBytes=0 catalogFNV=%08x packClosed=yes mutation=no turn=no\n",
           (unsigned int)resolvedWeapons,
           (unsigned int)ESP_NATIVE_GAMEPLAY_HUB_CONTENT_WEAPONS,
           (unsigned int)resolvedItems,
           (unsigned int)ESP_NATIVE_GAMEPLAY_HUB_CONTENT_ITEMS,
           (unsigned int)hash);
    return 1;
}
