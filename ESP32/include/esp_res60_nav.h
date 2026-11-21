#ifndef DOOMRPG_ESP32_RES60_NAV_H
#define DOOMRPG_ESP32_RES60_NAV_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* P7 only: isolated read-only camera rotation and three original-skin
 * controls in the 24px bottom band. NOT player movement or gameplay. */
typedef enum EspRes60NavTap_e {
    ESP_RES60_NAV_NONE = 0,
    ESP_RES60_NAV_LEFT = 1,
    ESP_RES60_NAV_MENU = 2,
    ESP_RES60_NAV_RIGHT = 3
} EspRes60NavTap;
EspRes60NavTap EspRes60Nav_hitPhysical(int16_t physicalX, int16_t physicalY);
int EspRes60Nav_applyTurn(uint8_t current, EspRes60NavTap tap,
                         uint8_t* next);
int EspRes60Nav_resolveAngle(uint8_t spawnAngle, uint8_t turns,
                            uint8_t* angle);
int EspRes60Nav_paint(uint16_t* fb, size_t bytes,
                      uint8_t spawnAngle, uint8_t turns);
#ifdef __cplusplus
}
#endif
#endif
