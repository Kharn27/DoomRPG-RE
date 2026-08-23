#include <string.h>

#include "esp_map_catalog.h"

static const char* const mapNames[ESP_MAP_CATALOG_COUNT] = {
    "/intro.bsp",
    "/level01.bsp",
    "/level02.bsp",
    "/level03.bsp",
    "/level04.bsp",
    "/level05.bsp",
    "/level06.bsp",
    "/level07.bsp",
    "/junction.bsp",
    "/junction_destroyed.bsp",
    "/items.bsp",
    "/reactor.bsp",
    "/endgame.bsp"
};

int EspMapCatalog_isValidId(uint8_t mapId) {
    return mapId >= ESP_MAP_CATALOG_FIRST_ID &&
           mapId <= ESP_MAP_CATALOG_LAST_ID;
}

const char* EspMapCatalog_nameForId(uint8_t mapId) {
    if (!EspMapCatalog_isValidId(mapId)) return NULL;
    return mapNames[(uint32_t)mapId - ESP_MAP_CATALOG_FIRST_ID];
}

int EspMapCatalog_idForName(const char* resourceName, uint8_t* outMapId) {
    uint32_t i;

    if (outMapId != NULL) *outMapId = 0U;
    if (resourceName == NULL || outMapId == NULL) return 0;

    for (i = 0U; i < ESP_MAP_CATALOG_COUNT; ++i) {
        if (strcmp(resourceName, mapNames[i]) == 0) {
            *outMapId = (uint8_t)(i + ESP_MAP_CATALOG_FIRST_ID);
            return 1;
        }
    }
    return 0;
}
