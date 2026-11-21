#include "esp_native_font_sd.h"
#include "esp_asset_pack.h"

#include <Arduino.h>
#include <SD.h>
#include <stdint.h>
#include <string.h>

namespace {

constexpr uint32_t kMinBmpBytes = 54U;
constexpr uint32_t kMaxFontBytes = 32U * 1024U;
/* 32 KiB is an input-size ceiling, NOT an allocation. It covers 1/4/8-bpp
 * 144x72 and 208x102 BMPs; 24-bpp art cannot be decoded by our indexed
 * importer and deliberately falls back to the original PAK. */
constexpr const char* kPaths[2] = {
    "/doomrpg/img/a.bmp",
    "/doomrpg/img/larger_font.bmp"
};

struct FontSlot {
    File file;
    uint32_t size = 0U;
    uint8_t state = 0U; /* 0=unprobed, 1=SD ready, 2=PAK fallback */
};

FontSlot fonts[2];

uint8_t sourceForName(const char* name) {
    if (name == nullptr) return 0U;
    if (strcmp(name, "a.bmp") == 0) return ESP_NATIVE_FONT_SD_STANDARD;
    if (strcmp(name, "larger_font.bmp") == 0) return ESP_NATIVE_FONT_SD_LARGER;
    return 0U;
}

FontSlot* slotFor(uint8_t source) {
    if (source < ESP_NATIVE_FONT_SD_STANDARD ||
        source > ESP_NATIVE_FONT_SD_LARGER) return nullptr;
    return &fonts[source - 1U];
}

} // namespace

/* The PAK owner calls this BEFORE opening any physical SD PAK/probe file.
 * Keep the reusable read-only font handles exclusive to safe SD windows. */
extern "C" void EspNativeFontSd_suspend(void) {
    for (FontSlot& slot : fonts) {
        if (slot->state == 1U) {
            if (slot->file) slot->file.close();
            slot->size = 0U;
            slot->state = 0U; /* Reprobe when the next flash-backed window opens. */
        }
    }
}

extern "C" uint8_t EspNativeFontSd_probe(const char* name, uint32_t* size) {
    if (size != nullptr) *size = 0U;
    const uint8_t source = sourceForName(name);
    FontSlot* const slot = slotFor(source);
    if (slot == nullptr) return 0U;
    if (!EspAssetPack_musicSdSafe()) {
        EspNativeFontSd_suspend();
        return 0U;
    }

    if (slot->state == 0U) {
        slot->state = 2U; /* Fail closed, including missing SD or bad metadata. */
        slot->file = SD.open(kPaths[source - 1U], FILE_READ);
        if (slot->file && !slot->file.isDirectory()) {
            const size_t fileSize = slot->file.size();
            if (fileSize >= kMinBmpBytes && fileSize <= kMaxFontBytes) {
                slot->size = static_cast<uint32_t>(fileSize);
                slot->state = 1U;
            }
        }
        if (slot->state != 1U) {
            if (slot->file) slot->file.close();
            Serial.printf("[FONTSD] FALLBACK name=%s reason=missing-or-size backing=PAK\n",
                          name);
        }
    }
    if (slot->state != 1U) return 0U;
    if (size != nullptr) *size = slot->size;
    return source;
}

extern "C" int EspNativeFontSd_read(uint8_t source, uint32_t offset,
                                     void* destination, uint32_t bytes) {
    FontSlot* const slot = slotFor(source);
    if (!EspAssetPack_musicSdSafe()) {
        EspNativeFontSd_suspend();
        return 0;
    }
    if (slot == nullptr || slot->state != 1U || !slot->file ||
        destination == nullptr || bytes == 0U ||
        offset > slot->size || bytes > slot->size - offset) {
        return 0;
    }
    if (!slot->file.seek(offset) ||
        slot->file.read(static_cast<uint8_t*>(destination),
                        static_cast<size_t>(bytes)) != bytes) {
        EspNativeFontSd_reject(source);
        return 0;
    }
    return 1;
}

extern "C" void EspNativeFontSd_reject(uint8_t source) {
    FontSlot* const slot = slotFor(source);
    if (slot == nullptr || slot->state == 2U) return;
    if (slot->file) slot->file.close();
    slot->size = 0U;
    slot->state = 2U;
    Serial.printf("[FONTSD] FALLBACK name=%s reason=invalid-or-io backing=PAK\n",
                  kPaths[source - 1U]);
}
