#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Render.h"
#include "native_palette.h"

#define SPRITE_172_PALETTE_OFFSET 1616

static uint16_t swapRedBlue565(uint16_t color) {
    return (uint16_t)(((color & 0x001FU) << 11) |
                      (color & 0x07E0U) |
                      ((color & 0xF800U) >> 11));
}

int DoomRPG_prepareNativePalette(struct Render_s* renderBase) {
    Render_t* render = (Render_t*)renderBase;
    uint16_t before[4];
    uint16_t after[4];
    int i;

    printf("\n=== Doom RPG ESP32-native palette normalization ===\n");

    if (render == NULL || render->mediaPalettes == NULL ||
        render->mediaPalettesLength <= 0) {
        printf("[PALETTE] FAILED renderer palette unavailable\n");
        return 0;
    }

    if (SPRITE_172_PALETTE_OFFSET + 3 >= render->mediaPalettesLength) {
        printf("[PALETTE] FAILED sprite-172 palette offset outside table entries=%d\n",
               render->mediaPalettesLength);
        return 0;
    }

    for (i = 0; i < 4; ++i) {
        before[i] = (uint16_t)render->mediaPalettes[SPRITE_172_PALETTE_OFFSET + i];
    }

    for (i = 0; i < render->mediaPalettesLength; ++i) {
        uint16_t legacyColor = (uint16_t)render->mediaPalettes[i];
        render->mediaPalettes[i] = (short)swapRedBlue565(legacyColor);
    }

    for (i = 0; i < 4; ++i) {
        after[i] = (uint16_t)render->mediaPalettes[SPRITE_172_PALETTE_OFFSET + i];
    }

    printf("[PALETTE] Normalized %d entries legacy R/B order -> framebuffer RGB565\n",
           render->mediaPalettesLength);
    printf("[PALETTE] sprite172 offset=%d first4 before=%04x,%04x,%04x,%04x after=%04x,%04x,%04x,%04x\n",
           SPRITE_172_PALETTE_OFFSET,
           (unsigned int)before[0],
           (unsigned int)before[1],
           (unsigned int)before[2],
           (unsigned int)before[3],
           (unsigned int)after[0],
           (unsigned int)after[1],
           (unsigned int)after[2],
           (unsigned int)after[3]);
    printf("[PALETTE] READY native consumers now see canonical RGB565\n");
    return 1;
}
