#ifndef DOOMRPG_ESP32_NATIVE_FONT_SD_H
#define DOOMRPG_ESP32_NATIVE_FONT_SD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Two original fonts and two exact-size story wallpapers can override PAK.
 * All optional indexed BMPs live under /doomrpg/img/ on SD.
 * Source IDs remain small, with the sole C++ SD owner of their handles. */
#define ESP_NATIVE_FONT_SD_STANDARD 1U
#define ESP_NATIVE_FONT_SD_LARGER 2U
#define ESP_NATIVE_WALLPAPER_SD_STARS 3U
#define ESP_NATIVE_WALLPAPER_SD_MARS 4U

/* The geometry is validated before any SD metadata becomes observable. */
static inline int EspNativeBmpSd_correctGeometry(uint8_t source, int width,
                                                  int height) {
    switch (source) {
    case ESP_NATIVE_FONT_SD_STANDARD: return width == 144 && height == 72;
    case ESP_NATIVE_FONT_SD_LARGER: return width == 208 && height == 102;
    case ESP_NATIVE_WALLPAPER_SD_STARS: return width == 192 && height == 128;
    case ESP_NATIVE_WALLPAPER_SD_MARS: return width == 128 && height == 128;
    default: return 0;
    }
}

static inline int EspNativeBmpSd_isFont(uint8_t source) {
    return source == ESP_NATIVE_FONT_SD_STANDARD ||
           source == ESP_NATIVE_FONT_SD_LARGER;
}

/* Return 0 on missing/disabled SD override, or an indexed BMP source ID. A successful
 * probe owns an open read-only File until rejected or the device restarts.
 * No ZIP, whole-file buffer or map-wide allocation is involved. */
uint8_t EspNativeFontSd_probe(const char* name, uint32_t* size);

/* Strict bounded offset read. A physical I/O failure invalidates this
 * override for the remainder of the boot; subsequent opens use the PAK. */
int EspNativeFontSd_read(uint8_t source, uint32_t offset,
                         void* destination, uint32_t bytes);

/* Called by the indexed BMP decoders for malformed files or wrong geometry. */
void EspNativeFontSd_reject(uint8_t source);

/* Close all optional BMP handles before any physical SD PAK/source-probe.
 * Reprobe good files after reopening; rejected files stay rejected until boot.
 * No retained full-image allocation is made by this owner. */
void EspNativeFontSd_suspend(void);

#ifdef __cplusplus
}
#endif

#endif
