#ifndef DOOMRPG_ESP32_RES60_LAYOUT_H
#define DOOMRPG_ESP32_RES60_LAYOUT_H

#include <stddef.h>
#include <stdint.h>

/* Permanent target geometry for the optional RES60 renderer. Never use
 * these dimensions to reinterpret the production 160x120 framebuffer. */
#define ESP_RES60_WIDTH 192U
#define ESP_RES60_HEIGHT 144U
#define ESP_RES60_HUD_TOP_ROWS 24U
#define ESP_RES60_WORLD_Y ESP_RES60_HUD_TOP_ROWS
#define ESP_RES60_WORLD_ROWS 96U
#define ESP_RES60_HUD_BOTTOM_Y (ESP_RES60_WORLD_Y + ESP_RES60_WORLD_ROWS)
#define ESP_RES60_HUD_BOTTOM_ROWS 24U
#define ESP_RES60_FRAMEBUFFER_BYTES (ESP_RES60_WIDTH * ESP_RES60_HEIGHT * sizeof(uint16_t))

#if ESP_RES60_HUD_TOP_ROWS + ESP_RES60_WORLD_ROWS + ESP_RES60_HUD_BOTTOM_ROWS != ESP_RES60_HEIGHT
#error "RES60 24+96+24 region contract must exactly fill 144 rows"
#endif

typedef enum EspRes60Region_e {
    ESP_RES60_OUTSIDE = 0,
    ESP_RES60_TOP_HUD = 1,
    ESP_RES60_WORLD = 2,
    ESP_RES60_BOTTOM_HUD = 3
} EspRes60Region;

static inline EspRes60Region EspRes60Layout_regionAt(int x, int y) {
    if (x < 0 || y < 0 || x >= (int)ESP_RES60_WIDTH ||
        y >= (int)ESP_RES60_HEIGHT) return ESP_RES60_OUTSIDE;
    if (y < (int)ESP_RES60_WORLD_Y) return ESP_RES60_TOP_HUD;
    if (y < (int)ESP_RES60_HUD_BOTTOM_Y) return ESP_RES60_WORLD;
    return ESP_RES60_BOTTOM_HUD;
}

/* Bounded, allocation-free diagnostic painter. This is NOT the 3D BSP
 * renderer; it proves native band ownership and 192-pixel row strides. */
static inline int EspRes60Layout_paintDiagnostic(uint16_t* framebuffer,
                                                 size_t framebufferBytes) {
    if (framebuffer == NULL || framebufferBytes != ESP_RES60_FRAMEBUFFER_BYTES)
        return 0;
    for (unsigned y = 0; y < ESP_RES60_HEIGHT; ++y) {
        for (unsigned x = 0; x < ESP_RES60_WIDTH; ++x) {
            uint16_t color;
            if (y < ESP_RES60_WORLD_Y) {
                color = ((x / 16U) & 1U) ? 0xc800U : 0xf800U;
                if (y == ESP_RES60_WORLD_Y - 1U) color = 0xffffU;
            } else if (y < ESP_RES60_HUD_BOTTOM_Y) {
                const unsigned wy = y - ESP_RES60_WORLD_Y;
                color = (((x / 12U) ^ (wy / 12U)) & 1U) ? 0x05d7U : 0x07ffU;
                if ((x % 24U) == 0U || (wy % 24U) == 0U) color = 0x001fU;
                if (x == 2U * wy || x + 2U * wy == ESP_RES60_WIDTH - 1U)
                    color = 0x0000U;
                if (x == ESP_RES60_WIDTH / 2U || wy == ESP_RES60_WORLD_ROWS / 2U)
                    color = 0xffffU;
            } else {
                color = ((x / 16U) & 1U) ? 0xfd20U : 0xffe0U;
                if (y == ESP_RES60_HUD_BOTTOM_Y) color = 0xffffU;
            }
            framebuffer[(size_t)y * ESP_RES60_WIDTH + x] = color;
        }
    }
    return 1;
}

/* Byte-order independent FNV-1a witness: low RGB565 byte before high byte. */
static inline uint32_t EspRes60Layout_fnv(const uint16_t* framebuffer,
                                          size_t framebufferBytes) {
    if (framebuffer == NULL || framebufferBytes != ESP_RES60_FRAMEBUFFER_BYTES)
        return 0U;
    uint32_t hash = UINT32_C(2166136261);
    for (size_t i = 0; i < ESP_RES60_WIDTH * ESP_RES60_HEIGHT; ++i) {
        const uint16_t pixel = framebuffer[i];
        hash = (hash ^ (uint8_t)pixel) * UINT32_C(16777619);
        hash = (hash ^ (uint8_t)(pixel >> 8)) * UINT32_C(16777619);
    }
    return hash;
}

#endif
