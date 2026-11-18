#ifndef ESP_AUDIO_TRACK_VOLUME_H
#define ESP_AUDIO_TRACK_VOLUME_H

#include <stdint.h>
#include "esp_audio_loading_route.h"

/* Only M_DEAD.wav on Junction is 25% quieter. This is a local music-track
 * trim AFTER the existing ADPCM soft-knee, composed with the user's chosen
 * global MUSIC volume BEFORE publishing to the one SD music ring.
 * No effect on Intro, MENU, Entrance, SFX mixing or any DAC ISR work.
 * Keep this deterministic, bounded and allocation-free. */
#define ESP_AUDIO_JUNCTION_TRACK_VOLUME_PERCENT 75U

static inline uint8_t EspAudioTrackVolume_trackPercent(
    EspAudioLoadingTrack track) {
    return track==ESP_AUDIO_LOADING_TRACK_M_DEAD
        ? ESP_AUDIO_JUNCTION_TRACK_VOLUME_PERCENT : 100U;
}

static inline uint8_t EspAudioTrackVolume_effectivePercent(
    EspAudioLoadingTrack track, uint8_t masterPercent) {
    if (masterPercent>100U) return 0U; /* fail-closed */
    return (uint8_t)(((uint16_t)masterPercent*
        (uint16_t)EspAudioTrackVolume_trackPercent(track))/100U);
}

#endif /* ESP_AUDIO_TRACK_VOLUME_H */
