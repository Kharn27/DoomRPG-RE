#ifndef DOOMRPG_ESP_AUDIO_LOADING_ROUTE_H
#define DOOMRPG_ESP_AUDIO_LOADING_ROUTE_H

#include <stdint.h>
#include "esp_map_catalog.h"

#ifdef __cplusplus
extern "C" {
#endif

/* L0: compile-time, allocation-free selection ONLY.
 * The loading screen is not yet an audio producer. An unconfigured map must
 * remain deferred instead of falling back to Entrance or the previous song. */
typedef enum EspAudioLoadingTrack_e {
    ESP_AUDIO_LOADING_TRACK_DEFERRED = 0,
    ESP_AUDIO_LOADING_TRACK_M_STALKS = 1
} EspAudioLoadingTrack;

static inline EspAudioLoadingTrack EspAudioLoadingRoute_forMap(uint8_t mapId) {
    return mapId == ESP_MAP_ID_INTRO
        ? ESP_AUDIO_LOADING_TRACK_M_STALKS
        : ESP_AUDIO_LOADING_TRACK_DEFERRED;
}

static inline const char* EspAudioLoadingRoute_trackName(
    EspAudioLoadingTrack track) {
    return track == ESP_AUDIO_LOADING_TRACK_M_STALKS
        ? "M_STALKS.wav" : "none";
}

#ifdef __cplusplus
}
#endif

#endif /* DOOMRPG_ESP_AUDIO_LOADING_ROUTE_H */
