#ifndef DOOMRPG_ESP32_NATIVE_LOADING_PROGRESS_SCALE_H
#define DOOMRPG_ESP32_NATIVE_LOADING_PROGRESS_SCALE_H

#include <stdint.h>
#include "esp_asset_pack.h"

/* Immediate CHANGEMAP: erase 0..20, copy 20..80, verify 80..100.
 * Deferred fresh Start/LOAD: preflash checkpoints 0..20, actual flash
 * erase/copy/verify 20..85, cache/handoff 90..100. No timed animation. */
static inline uint8_t EspNativeLoadingProgress_checkpointPercent(
    uint8_t percent, uint8_t deferred) {
    if (percent > 100U) percent = 100U;
    if (deferred && percent <= 85U) {
        return (uint8_t)(((uint32_t)percent * 20U) / 85U);
    }
    return percent;
}

static inline uint8_t EspNativeLoadingProgress_flashPercent(
    uint8_t phase, uint32_t completed, uint32_t total, uint8_t deferred) {
    uint32_t local, raw;
    if (total == 0U) return 0U;
    if (completed > total) completed = total;
    local = (uint32_t)(((uint64_t)completed * 100U) / total);
    switch (phase) {
    case ESP_ASSET_PACK_MAP_FLASH_PROGRESS_ERASE:
        raw = (local * 20U) / 100U;
        break;
    case ESP_ASSET_PACK_MAP_FLASH_PROGRESS_COPY:
        raw = 20U + (local * 60U) / 100U;
        break;
    case ESP_ASSET_PACK_MAP_FLASH_PROGRESS_VERIFY:
        raw = 80U + (local * 20U) / 100U;
        break;
    default:
        return 0U;
    }
    return deferred ? (uint8_t)(20U + (raw * 65U) / 100U)
                    : (uint8_t)raw;
}
#endif
