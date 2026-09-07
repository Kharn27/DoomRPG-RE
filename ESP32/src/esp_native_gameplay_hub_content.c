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
#define HUB_CONTENT_KEY_TYPES 4U

/* Exact Combat_init player weapon ammoType/ammoUsage fields for ids 0..11.
 * This table is presentation metadata only and does not own combat semantics. */
static const uint8_t weaponAmmoType[ESP_NATIVE_GAMEPLAY_HUB_CONTENT_WEAPONS] = {
    0U, 0U, 1U, 2U, 1U, 2U, 4U, 3U, 4U, 5U, 5U, 5U
};
static const uint8_t weaponAmmoUsage[ESP_NATIVE_GAMEPLAY_HUB_CONTENT_WEAPONS] = {
    0U, 1U, 1U, 1U, 3U, 2U, 3U, 1U, 15U, 0U, 0U, 0U
};
static const char* const keyNames[HUB_CONTENT_KEY_TYPES] = {
    "Green Key", "Yellow Key", "Blue Key", "Red Key"
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
    int packWasOpen;
    int ok;

    if (outTileIndex == NULL || outName == NULL || capacity < 2U ||
        !EspEntityDefTypeCatalog_findTileIndex(type, subtype, &tileIndex)) {
        return 0;
    }
    packWasOpen = EspAssetPack_isOpen();
    ok = packWasOpen
             ? EspEntityDefTypeCatalog_readNameFromOpenPack(
                   tileIndex, outName, capacity)
             : EspEntityDefTypeCatalog_readName(tileIndex, outName, capacity);
    if (!ok || outName[0] == '\0' || EspAssetPack_isOpen() != packWasOpen) {
        return 0;
    }
    *outTileIndex = tileIndex;
    return 1;
}

static int copyStatic(char* destination,
                      uint32_t capacity,
                      const char* source) {
    int written;
    if (destination == NULL || capacity < 2U || source == NULL) return 0;
    written = snprintf(destination, capacity, "%s", source);
    return written >= 0 && (uint32_t)written < capacity;
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
    int packWasOpen;

    if (player == NULL || outContent == NULL || player->active != 1U ||
        player->weapon >= ESP_NATIVE_GAMEPLAY_HUB_CONTENT_WEAPONS ||
        !EspEntityDefTypeCatalog_isReady()) {
        return 0;
    }
    packWasOpen = EspAssetPack_isOpen();

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

    return EspAssetPack_isOpen() == packWasOpen;
}

uint8_t EspNativeGameplayHubContent_inventoryEntryCount(
    const EspNativeGameplayPlayerState* player) {
    uint8_t count = 2U; /* Notebook + Credits are always present in legacy. */
    uint8_t i;

    if (player == NULL || player->active != 1U) return 0U;
    for (i = 0U; i < ESP_NATIVE_GAMEPLAY_HUB_CONTENT_WEAPONS; ++i) {
        if ((player->weapons & (1U << i)) != 0U) ++count;
    }
    for (i = 0U; i < ESP_NATIVE_GAMEPLAY_HUB_CONTENT_ITEMS; ++i) {
        if (player->inventory[i] != 0U) ++count;
    }
    for (i = 0U; i < HUB_CONTENT_KEY_TYPES; ++i) {
        if ((player->keys & (1UL << i)) != 0U) ++count;
    }
    return count <= ESP_NATIVE_GAMEPLAY_HUB_CONTENT_MAX_ENTRIES ? count : 0U;
}

const char* EspNativeGameplayHubContent_inventoryKindName(uint8_t kind) {
    switch (kind) {
    case ESP_NATIVE_GAMEPLAY_HUB_ENTRY_WEAPON: return "weapon";
    case ESP_NATIVE_GAMEPLAY_HUB_ENTRY_NOTEBOOK: return "notebook";
    case ESP_NATIVE_GAMEPLAY_HUB_ENTRY_ITEM: return "item";
    case ESP_NATIVE_GAMEPLAY_HUB_ENTRY_CREDITS: return "credits";
    case ESP_NATIVE_GAMEPLAY_HUB_ENTRY_KEY: return "key";
    default: return "unknown";
    }
}

int EspNativeGameplayHubContent_inventoryEntryAt(
    const EspNativeGameplayPlayerState* player,
    uint8_t entryIndex,
    EspNativeGameplayHubInventoryEntry* outEntry) {
    uint8_t cursor = 0U;
    uint8_t i;
    uint8_t count;
    uint16_t tileIndex;
    int packWasOpen;
    int written;

    if (player == NULL || outEntry == NULL || player->active != 1U ||
        !EspEntityDefTypeCatalog_isReady()) {
        return 0;
    }
    count = EspNativeGameplayHubContent_inventoryEntryCount(player);
    if (count == 0U || entryIndex >= count) return 0;
    packWasOpen = EspAssetPack_isOpen();
    memset(outEntry, 0, sizeof(*outEntry));

    for (i = 0U; i < ESP_NATIVE_GAMEPLAY_HUB_CONTENT_WEAPONS; ++i) {
        uint8_t ammoType;
        if ((player->weapons & (1U << i)) == 0U) continue;
        if (cursor++ != entryIndex) continue;

        if (!resolveName(HUB_CONTENT_ENTITY_TYPE_WEAPON,
                         i,
                         &tileIndex,
                         outEntry->name,
                         sizeof(outEntry->name))) {
            return 0;
        }
        outEntry->kind = ESP_NATIVE_GAMEPLAY_HUB_ENTRY_WEAPON;
        outEntry->sourceId = i;
        if (i == 0U) {
            if (!copyStatic(outEntry->value, sizeof(outEntry->value), "--")) {
                return 0;
            }
        }
        else {
            ammoType = weaponAmmoType[i];
            if (ammoType >= HUB_CONTENT_AMMO_TYPES) return 0;
            written = snprintf(outEntry->value,
                               sizeof(outEntry->value),
                               "%u",
                               (unsigned int)player->ammo[ammoType]);
            if (written < 0 || (uint32_t)written >= sizeof(outEntry->value)) {
                return 0;
            }
        }
        return EspAssetPack_isOpen() == packWasOpen;
    }

    if (cursor++ == entryIndex) {
        outEntry->kind = ESP_NATIVE_GAMEPLAY_HUB_ENTRY_NOTEBOOK;
        outEntry->sourceId = 0U;
        return copyStatic(outEntry->name, sizeof(outEntry->name), "Notebook") &&
               copyStatic(outEntry->value, sizeof(outEntry->value), "--") &&
               EspAssetPack_isOpen() == packWasOpen;
    }

    for (i = 0U; i < ESP_NATIVE_GAMEPLAY_HUB_CONTENT_ITEMS; ++i) {
        if (player->inventory[i] == 0U) continue;
        if (cursor++ != entryIndex) continue;

        if (!resolveName(HUB_CONTENT_ENTITY_TYPE_ITEM,
                         (uint8_t)(HUB_CONTENT_ITEM_SUBTYPE_BASE + i),
                         &tileIndex,
                         outEntry->name,
                         sizeof(outEntry->name))) {
            return 0;
        }
        outEntry->kind = ESP_NATIVE_GAMEPLAY_HUB_ENTRY_ITEM;
        outEntry->sourceId = i;
        written = snprintf(outEntry->value,
                           sizeof(outEntry->value),
                           "%u",
                           (unsigned int)player->inventory[i]);
        if (written < 0 || (uint32_t)written >= sizeof(outEntry->value)) {
            return 0;
        }
        return EspAssetPack_isOpen() == packWasOpen;
    }

    if (cursor++ == entryIndex) {
        outEntry->kind = ESP_NATIVE_GAMEPLAY_HUB_ENTRY_CREDITS;
        outEntry->sourceId = 0U;
        if (!copyStatic(outEntry->name, sizeof(outEntry->name), "Credits")) {
            return 0;
        }
        written = snprintf(outEntry->value,
                           sizeof(outEntry->value),
                           "%lu",
                           (unsigned long)player->credits);
        return written >= 0 && (uint32_t)written < sizeof(outEntry->value) &&
               EspAssetPack_isOpen() == packWasOpen;
    }

    for (i = 0U; i < HUB_CONTENT_KEY_TYPES; ++i) {
        if ((player->keys & (1UL << i)) == 0U) continue;
        if (cursor++ != entryIndex) continue;

        outEntry->kind = ESP_NATIVE_GAMEPLAY_HUB_ENTRY_KEY;
        outEntry->sourceId = i;
        return copyStatic(outEntry->name, sizeof(outEntry->name), keyNames[i]) &&
               copyStatic(outEntry->value, sizeof(outEntry->value), "--") &&
               EspAssetPack_isOpen() == packWasOpen;
    }

    return 0;
}

int EspNativeGameplayHubContent_probeInventoryList(void) {
    EspNativeGameplayPlayerState synthetic;
    EspNativeGameplayHubInventoryEntry entry;
    uint32_t hash = 2166136261U;
    uint8_t count;
    uint8_t i;
    int packWasOpen;

    if (!EspEntityDefTypeCatalog_isReady()) return 0;
    packWasOpen = EspAssetPack_isOpen();
    memset(&synthetic, 0, sizeof(synthetic));
    synthetic.active = 1U;
    synthetic.weapon = 2U;
    synthetic.weapons = 0x0fffU;
    synthetic.ammo[0] = 10U;
    synthetic.ammo[1] = 20U;
    synthetic.ammo[2] = 30U;
    synthetic.ammo[3] = 40U;
    synthetic.ammo[4] = 50U;
    synthetic.ammo[5] = 60U;
    synthetic.inventory[0] = 1U;
    synthetic.inventory[1] = 2U;
    synthetic.inventory[2] = 3U;
    synthetic.inventory[3] = 4U;
    synthetic.inventory[4] = 5U;
    synthetic.keys = 0x0fU;
    synthetic.credits = 123U;

    count = EspNativeGameplayHubContent_inventoryEntryCount(&synthetic);
    if (count != ESP_NATIVE_GAMEPLAY_HUB_CONTENT_MAX_ENTRIES) {
        printf("[HUBLIST] DEFER reason=max-entry-count actual=%u expected=%u\n",
               (unsigned int)count,
               (unsigned int)ESP_NATIVE_GAMEPLAY_HUB_CONTENT_MAX_ENTRIES);
        return 0;
    }

    for (i = 0U; i < count; ++i) {
        uint8_t expectedKind;
        uint8_t expectedSource;
        memset(&entry, 0, sizeof(entry));
        if (!EspNativeGameplayHubContent_inventoryEntryAt(&synthetic, i, &entry)) {
            printf("[HUBLIST] DEFER entry=%u reason=projection\n",
                   (unsigned int)i);
            return 0;
        }

        if (i < 12U) {
            expectedKind = ESP_NATIVE_GAMEPLAY_HUB_ENTRY_WEAPON;
            expectedSource = i;
        }
        else if (i == 12U) {
            expectedKind = ESP_NATIVE_GAMEPLAY_HUB_ENTRY_NOTEBOOK;
            expectedSource = 0U;
        }
        else if (i < 18U) {
            expectedKind = ESP_NATIVE_GAMEPLAY_HUB_ENTRY_ITEM;
            expectedSource = (uint8_t)(i - 13U);
        }
        else if (i == 18U) {
            expectedKind = ESP_NATIVE_GAMEPLAY_HUB_ENTRY_CREDITS;
            expectedSource = 0U;
        }
        else {
            expectedKind = ESP_NATIVE_GAMEPLAY_HUB_ENTRY_KEY;
            expectedSource = (uint8_t)(i - 19U);
        }
        if (entry.kind != expectedKind || entry.sourceId != expectedSource) {
            printf("[HUBLIST] DEFER entry=%u reason=legacy-order kind=%u/%u source=%u/%u\n",
                   (unsigned int)i,
                   (unsigned int)entry.kind,
                   (unsigned int)expectedKind,
                   (unsigned int)entry.sourceId,
                   (unsigned int)expectedSource);
            return 0;
        }

        hash = fnvUpdate(hash, &entry.kind, sizeof(entry.kind));
        hash = fnvUpdate(hash, &entry.sourceId, sizeof(entry.sourceId));
        hash = fnvUpdate(hash, entry.name, (uint32_t)strlen(entry.name) + 1U);
        hash = fnvUpdate(hash, entry.value, (uint32_t)strlen(entry.value) + 1U);
        printf("[HUBLIST] PROBE entry=%u kind=%s source=%u name=\"%s\" value=\"%s\"\n",
               (unsigned int)i,
               EspNativeGameplayHubContent_inventoryKindName(entry.kind),
               (unsigned int)entry.sourceId,
               entry.name,
               entry.value);
    }

    if (hash == 0U || EspAssetPack_isOpen() != packWasOpen) return 0;
    printf("[HUBLIST] READY entries=%u/%u order=legacy-content persistentListBytes=0 transientEntryBytes=%u listFNV=%08x packOwnership=preserved-%s mutation=no turn=no\n",
           (unsigned int)count,
           (unsigned int)ESP_NATIVE_GAMEPLAY_HUB_CONTENT_MAX_ENTRIES,
           (unsigned int)sizeof(entry),
           (unsigned int)hash,
           packWasOpen ? "open" : "closed");
    return 1;
}

int EspNativeGameplayHubContent_probeCatalog(void) {
    uint32_t hash = 2166136261U;
    uint16_t tileIndex;
    uint8_t subtype;
    uint8_t resolvedWeapons = 0U;
    uint8_t resolvedItems = 0U;
    char name[ESP_NATIVE_GAMEPLAY_HUB_CONTENT_NAME_BYTES];
    int packWasOpen;

    if (!EspEntityDefTypeCatalog_isReady()) return 0;
    packWasOpen = EspAssetPack_isOpen();

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
        hash == 0U || EspAssetPack_isOpen() != packWasOpen) {
        return 0;
    }

    printf("[HUBCONTENT] READY weapons=%u/%u items=%u/%u names=pak-on-demand persistentNameBytes=0 catalogFNV=%08x packOwnership=preserved-%s mutation=no turn=no\n",
           (unsigned int)resolvedWeapons,
           (unsigned int)ESP_NATIVE_GAMEPLAY_HUB_CONTENT_WEAPONS,
           (unsigned int)resolvedItems,
           (unsigned int)ESP_NATIVE_GAMEPLAY_HUB_CONTENT_ITEMS,
           (unsigned int)hash,
           packWasOpen ? "open" : "closed");
    return 1;
}
