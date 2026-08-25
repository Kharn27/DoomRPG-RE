#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUD_DIRECTION_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_HUD_DIRECTION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EspNativeGameplayHudDirectionStats_s {
    uint32_t packReads;
    uint32_t bytesRead;
    uint32_t rowsRead;
    uint32_t pixelsWritten;
    uint8_t angle;
    uint8_t resourcesValidated;
    uint8_t rendered;
    uint8_t reserved;
} EspNativeGameplayHudDirectionStats;

/* Repaint only the bottom-HUD compass panel for one cardinal angle. The panel
 * background is reconstructed from the same 20x20 k.bmp tile phase used by the
 * hardware-proven full HUD, then o.bmp and the one-character a.bmp glyph are
 * redrawn. No HUD/game/player legacy owner is touched. */
int EspNativeGameplayHudDirection_render(
    uint8_t angle,
    EspNativeGameplayHudDirectionStats* outStats);

#ifdef __cplusplus
}
#endif

#endif
