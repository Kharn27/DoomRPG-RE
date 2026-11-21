#ifndef DOOMRPG_ESP32_NATIVE_FONT_SD_H
#define DOOMRPG_ESP32_NATIVE_FONT_SD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Only these two immutable font identities may override the native PAK.
 * SD paths are /doomrpg/img/a.bmp and /doomrpg/img/larger_font.bmp.
 * A source ID is not an open File pointer: it stays small and safe in C
 * metadata, and the sole C++ SD owner keeps the backing handle. */
#define ESP_NATIVE_FONT_SD_STANDARD 1U
#define ESP_NATIVE_FONT_SD_LARGER 2U

/* Return 0 on missing/disabled SD override, or a font source ID. A successful
 * probe owns an open read-only File until rejected or the device restarts.
 * No ZIP, whole-file buffer or map-wide allocation is involved. */
uint8_t EspNativeFontSd_probe(const char* name, uint32_t* size);

/* Strict bounded offset read. A physical I/O failure invalidates this
 * override for the remainder of the boot; subsequent opens use the PAK. */
int EspNativeFontSd_read(uint8_t source, uint32_t offset,
                         void* destination, uint32_t bytes);

/* Called by the indexed BMP decoders when a present file is malformed or
 * its dimensions differ from the original sprite atlas. */
void EspNativeFontSd_reject(uint8_t source);

/* Close live font handles before any physical PAK/source-probe SD.open.
 * Missing/invalid decisions remain cached; good fonts can be reprobed after
 * the PAK returns to flash or fully closes. No retained image allocation. */
void EspNativeFontSd_suspend(void);

#ifdef __cplusplus
}
#endif

#endif
