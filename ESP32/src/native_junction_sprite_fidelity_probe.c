#include <SDL.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_native_first_frame.h"
#include "native_junction_sprite_fidelity_probe.h"
#include "native_junction_sprite_overlay_probe.h"
#include "platform_video_config.h"

#define EXPECTED_BASE_FRAME_FNV 0x8910c2edU

static struct {
    int preAttempted;
    int preDone;
    int postAttempted;
    int postDone;
} probeState;

static uint32_t fnvAppend(uint32_t hash, const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t frameFNV(const Render_t* render) {
    const uint32_t bytes = DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT *
                           (uint32_t)sizeof(uint16_t);
    if (render == NULL || render->framebuffer == NULL) return 0U;
    return fnvAppend(2166136261U, render->framebuffer, bytes);
}

static uint32_t viewportFNV(const Render_t* render) {
    const uint16_t* framebuffer;
    const uint32_t rowBytes = DOOMRPG_LOGICAL_WIDTH *
                              (uint32_t)sizeof(uint16_t);
    int pitchPixels;
    uint32_t hash = 2166136261U;
    int y;

    if (render == NULL || render->framebuffer == NULL ||
        render->screenWidth != DOOMRPG_LOGICAL_WIDTH ||
        render->screenHeight != 80 || render->screenX != 0 ||
        render->screenY < 0 || render->screenY + 80 > DOOMRPG_LOGICAL_HEIGHT) {
        return 0U;
    }

    framebuffer = (const uint16_t*)render->framebuffer;
    pitchPixels = render->pitch >> 1;
    for (y = 0; y < render->screenHeight; ++y) {
        const uint16_t* row = framebuffer +
            (render->screenY + y) * pitchPixels;
        hash = fnvAppend(hash, row, rowBytes);
    }
    return hash;
}

static void writeLe16(uint8_t* out, uint16_t value) {
    out[0] = (uint8_t)(value & 0xffU);
    out[1] = (uint8_t)((value >> 8) & 0xffU);
}

static void writeLe32(uint8_t* out, uint32_t value) {
    out[0] = (uint8_t)(value & 0xffU);
    out[1] = (uint8_t)((value >> 8) & 0xffU);
    out[2] = (uint8_t)((value >> 16) & 0xffU);
    out[3] = (uint8_t)((value >> 24) & 0xffU);
}

static uint8_t expand5(uint16_t value) {
    value &= 31U;
    return (uint8_t)((value << 3) | (value >> 2));
}

static uint8_t expand6(uint16_t value) {
    value &= 63U;
    return (uint8_t)((value << 2) | (value >> 4));
}

static int dumpSpriteViewportBmp(const Render_t* render, uint32_t* outFNV) {
    static const char* const path = "/sd/junction-sprite-viewport.bmp";
    enum {
        BMP_HEADER_BYTES = 54,
        BMP_WIDTH = 160,
        BMP_HEIGHT = 80,
        BMP_ROW_BYTES = BMP_WIDTH * 3,
        BMP_IMAGE_BYTES = BMP_ROW_BYTES * BMP_HEIGHT,
        BMP_FILE_BYTES = BMP_HEADER_BYTES + BMP_IMAGE_BYTES
    };
    uint8_t header[BMP_HEADER_BYTES] = {0};
    uint8_t row[BMP_ROW_BYTES];
    const uint16_t* framebuffer;
    int pitchPixels;
    FILE* file;
    int x;
    int y;

    if (outFNV != NULL) *outFNV = 0U;
    if (render == NULL || render->framebuffer == NULL ||
        render->screenWidth != BMP_WIDTH || render->screenHeight != BMP_HEIGHT ||
        render->screenX != 0 || render->screenY != 20) {
        return 0;
    }

    file = fopen(path, "wb");
    if (file == NULL) return 0;

    header[0] = 'B';
    header[1] = 'M';
    writeLe32(&header[2], BMP_FILE_BYTES);
    writeLe32(&header[10], BMP_HEADER_BYTES);
    writeLe32(&header[14], 40U);
    writeLe32(&header[18], BMP_WIDTH);
    writeLe32(&header[22], BMP_HEIGHT);
    writeLe16(&header[26], 1U);
    writeLe16(&header[28], 24U);
    writeLe32(&header[34], BMP_IMAGE_BYTES);
    writeLe32(&header[38], 2835U);
    writeLe32(&header[42], 2835U);

    if (fwrite(header, 1U, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        remove(path);
        return 0;
    }

    framebuffer = (const uint16_t*)render->framebuffer;
    pitchPixels = render->pitch >> 1;
    for (y = BMP_HEIGHT - 1; y >= 0; --y) {
        const uint16_t* source = framebuffer +
            (render->screenY + y) * pitchPixels;
        for (x = 0; x < BMP_WIDTH; ++x) {
            const uint16_t color = source[x];
            row[(x * 3) + 0] = expand5(color);
            row[(x * 3) + 1] = expand6(color >> 5);
            row[(x * 3) + 2] = expand5(color >> 11);
        }
        if (fwrite(row, 1U, sizeof(row), file) != sizeof(row)) {
            fclose(file);
            remove(path);
            return 0;
        }
    }

    if (fclose(file) != 0) {
        remove(path);
        return 0;
    }
    if (outFNV != NULL) *outFNV = viewportFNV(render);
    return outFNV == NULL || *outFNV != 0U;
}

void Esp32JunctionSpriteFidelityProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

int Esp32JunctionSpriteFidelityProbe_preOverlayDone(void) {
    return probeState.preDone;
}

int Esp32JunctionSpriteFidelityProbe_postOverlayDone(void) {
    return probeState.postDone;
}

void Esp32JunctionSpriteFidelityProbe_preOverlayService(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    Render_t* render;
    uint32_t frame;

    if (probeState.preDone || probeState.preAttempted) return;
    if (doomRpg == NULL || doomRpg->render == NULL ||
        !EspNativeFirstFrame_isReady()) {
        return;
    }
    probeState.preAttempted = 1;
    render = doomRpg->render;
    frame = frameFNV(render);

    if (frame != EXPECTED_BASE_FRAME_FNV) {
        printf("[SPRITEFIDELITY] FAILED pre boundary frame=%08x expected=%08x\n",
               (unsigned int)frame,
               (unsigned int)EXPECTED_BASE_FRAME_FNV);
        return;
    }

    /* The discovery walk that established Junction's exact hardware set
     * (21 candidates / 27 rejected, modes 14/7, candidate FNV 23ef1895) has
     * served its purpose. The renderer/probe now revalidate the 21/27 contract
     * from the same stateful BSP traversal, avoiding a second divergent walk. */
    probeState.preDone = 1;
}

void Esp32JunctionSpriteFidelityProbe_postOverlayService(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    Render_t* render;
    uint32_t frame;
    uint32_t viewport = 0U;

    if (probeState.postDone || probeState.postAttempted || !probeState.preDone ||
        !Esp32JunctionSpriteOverlayProbe_isDone()) {
        return;
    }
    probeState.postAttempted = 1;
    render = doomRpg != NULL ? doomRpg->render : NULL;
    frame = frameFNV(render);

    if (render == NULL || frame == 0U || frame == EXPECTED_BASE_FRAME_FNV ||
        !dumpSpriteViewportBmp(render, &viewport)) {
        printf("[JUNCTIONSPRITE] BMP FAILED frame=%08x base=%08x viewportFNV=%08x\n",
               (unsigned int)frame,
               (unsigned int)EXPECTED_BASE_FRAME_FNV,
               (unsigned int)viewport);
        return;
    }

    printf("[JUNCTIONSPRITE] BMP READY path=/junction-sprite-viewport.bmp size=38454 viewport=160x80@0,20 viewportFNV=%08x frame=%08x postParkDiagnostic=yes\n",
           (unsigned int)viewport, (unsigned int)frame);
    probeState.postDone = 1;
}
