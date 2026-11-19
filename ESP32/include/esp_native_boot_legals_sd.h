#ifndef DOOMRPG_ESP32_NATIVE_BOOT_LEGALS_SD_H
#define DOOMRPG_ESP32_NATIVE_BOOT_LEGALS_SD_H
#include <stddef.h>
#include <stdint.h>
#define ESP_NATIVE_BOOT_LEGALS_SD_PATH "/doomrpg/img/credits.bmp"
#ifdef __cplusplus
extern "C" {
#endif
/* Optional removable-SD lease. Never used while the native PAK is open.
 * Each frame closes its file before PlatformVideo_present(). */
int EspNativeBootLegalsSd_open(uint32_t* fileSize);
int EspNativeBootLegalsSd_read(uint32_t offset, void* dest, size_t length);
void EspNativeBootLegalsSd_close(void);
#ifdef __cplusplus
}
#endif
#endif
