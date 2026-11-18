#ifndef DOOMRPG_ESP_AUDIO_LOADING_ROUTE_H
#define DOOMRPG_ESP_AUDIO_LOADING_ROUTE_H

#include <stdint.h>
#include "esp_map_catalog.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Allocation-free map soundtrack selection. Loading never opens SD; maps
 * without an explicitly assigned track remain deferred (including map 10). */
typedef enum EspAudioLoadingTrack_e {
    ESP_AUDIO_LOADING_TRACK_DEFERRED = 0,
    ESP_AUDIO_LOADING_TRACK_M_STALKS = 1,
    ESP_AUDIO_LOADING_TRACK_M_DEAD = 2
} EspAudioLoadingTrack;

static inline EspAudioLoadingTrack EspAudioLoadingRoute_forMap(uint8_t mapId) {
    switch (mapId) {
        case ESP_MAP_ID_INTRO: return ESP_AUDIO_LOADING_TRACK_M_STALKS;
        case ESP_MAP_ID_JUNCTION: return ESP_AUDIO_LOADING_TRACK_M_DEAD;
        default: return ESP_AUDIO_LOADING_TRACK_DEFERRED;
    }
}

/* BSP header loadMapId is NOT the native catalogue/map ID. This is the
 * separate, recovered per-BSP gameplay load discriminator. Real Junction
 * /junction.bsp declares loadMapId=2 while catalog map ID is 9.
 * Select only explicitly validated (catalog, BSP header) pairs; never
 * accept an unknown map or silently infer loadMapId from catalog ID. */
static inline EspAudioLoadingTrack EspAudioLoadingRoute_forView(
    uint8_t catalogMapId, uint8_t bspLoadMapId) {
    if (catalogMapId == ESP_MAP_ID_INTRO && bspLoadMapId == 1U)
        return ESP_AUDIO_LOADING_TRACK_M_STALKS;
    if (catalogMapId == ESP_MAP_ID_JUNCTION && bspLoadMapId == 2U)
        return ESP_AUDIO_LOADING_TRACK_M_DEAD;
    return ESP_AUDIO_LOADING_TRACK_DEFERRED;
}

static inline const char* EspAudioLoadingRoute_trackName(
    EspAudioLoadingTrack track) {
    switch (track) {
        case ESP_AUDIO_LOADING_TRACK_M_STALKS: return "M_STALKS.wav";
        case ESP_AUDIO_LOADING_TRACK_M_DEAD: return "M_DEAD.wav";
        default: return "none";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* DOOMRPG_ESP_AUDIO_LOADING_ROUTE_H */
