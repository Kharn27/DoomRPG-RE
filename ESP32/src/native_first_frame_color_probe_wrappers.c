#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "DoomRPG.h"
#include "Render.h"

#include "esp_native_first_frame.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

/*
 * Temporary first-frame fidelity diagnostics.
 *
 * The renderer contract owns only the logical RGB565 framebuffer.  The BMP
 * dump runs after PARK because first-use stdio/SD may retain a small VFS/libc
 * allocation.  A second, equally temporary presentation probe is armed only
 * while EspNativeFirstFrame_route() is inside its real TFT present.  It shows
 * the exact same restored framebuffer through four reversible variants so one
 * hardware flash can distinguish panel inversion, byte-order and R/B-order
 * faults without changing any gameplay/render owner or canonical FNV.
 */

EspNativeFirstFrameStatus __real_EspNativeFirstFrame_route(
    struct Render_s* render,
    const struct EspPlayerViewState_s* playerView);
int __real_Esp32PlatformVideo_present(void);

static Render_t* pendingDumpRender;
static Render_t* presentationRender;
static int presentationArmed;
static int dumpAttempted;
static int dumpSucceeded;

static uint32_t fnvAppend(uint32_t hash, const void* data, size_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    size_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint8_t expand5(uint16_t value) {
    value &= 31U;
    return (uint8_t)((value << 3) | (value >> 2));
}

static uint8_t expand6(uint16_t value) {
    value &= 63U;
    return (uint8_t)((value << 2) | (value >> 4));
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

static uint32_t viewportFNV(const Render_t* render) {
    const uint16_t* framebuffer;
    const size_t rowBytes =
        (size_t)DOOMRPG_LOGICAL_WIDTH * sizeof(uint16_t);
    int pitchPixels;
    uint32_t hash = 2166136261U;
    int y;

    if (render == NULL || render->framebuffer == NULL ||
        render->screenWidth != DOOMRPG_LOGICAL_WIDTH ||
        render->screenHeight <= 0 || render->screenX != 0 ||
        render->screenY < 0 ||
        render->screenY + render->screenHeight > DOOMRPG_LOGICAL_HEIGHT) {
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

static int dumpViewportBmp(const Render_t* render, uint32_t* outFNV) {
    static const char* const path = "/sd/junction-viewport.bmp";
    enum {
        BMP_HEADER_BYTES = 54,
        BMP_WIDTH = DOOMRPG_LOGICAL_WIDTH,
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
    int y;
    int x;

    if (outFNV != NULL) *outFNV = 0U;
    if (render == NULL || render->framebuffer == NULL ||
        render->screenWidth != BMP_WIDTH ||
        render->screenHeight != BMP_HEIGHT || render->screenX != 0 ||
        render->screenY < 0 ||
        render->screenY + BMP_HEIGHT > DOOMRPG_LOGICAL_HEIGHT) {
        printf("[JUNCTIONFRAME] BMP FAILED viewport=%dx%d@%d,%d\n",
               render != NULL ? render->screenWidth : -1,
               render != NULL ? render->screenHeight : -1,
               render != NULL ? render->screenX : -1,
               render != NULL ? render->screenY : -1);
        return 0;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        printf("[JUNCTIONFRAME] BMP FAILED open path=/junction-viewport.bmp\n");
        return 0;
    }

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
        printf("[JUNCTIONFRAME] BMP FAILED header write\n");
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
            printf("[JUNCTIONFRAME] BMP FAILED pixel write row=%d\n", y);
            return 0;
        }
    }

    if (fclose(file) != 0) {
        remove(path);
        printf("[JUNCTIONFRAME] BMP FAILED close\n");
        return 0;
    }

    if (outFNV != NULL) *outFNV = viewportFNV(render);
    printf("[JUNCTIONFRAME] BMP READY path=/junction-viewport.bmp size=%u viewport=%dx%d@%d,%d viewportFNV=%08x postParkDiagnostic=yes\n",
           (unsigned int)BMP_FILE_BYTES,
           render->screenWidth, render->screenHeight,
           render->screenX, render->screenY,
           (unsigned int)(outFNV != NULL ? *outFNV : 0U));
    return 1;
}

static void measureViewport(const Render_t* render) {
    const uint16_t* framebuffer;
    uint64_t sumR = 0U;
    uint64_t sumG = 0U;
    uint64_t sumB = 0U;
    uint64_t sumY = 0U;
    uint32_t bins[4] = {0U, 0U, 0U, 0U};
    uint32_t neutral = 0U;
    uint32_t count = 0U;
    uint8_t minY = 255U;
    uint8_t maxY = 0U;
    int pitchPixels;
    int x;
    int y;

    if (render == NULL || render->framebuffer == NULL ||
        render->screenWidth <= 0 || render->screenHeight <= 0 ||
        render->screenX < 0 || render->screenY < 0) {
        printf("[JUNCTIONFRAME] COLORSTATS FAILED invalid render viewport\n");
        return;
    }

    framebuffer = (const uint16_t*)render->framebuffer;
    pitchPixels = render->pitch >> 1;

    for (y = 0; y < render->screenHeight; ++y) {
        const uint16_t* source = framebuffer +
            (render->screenY + y) * pitchPixels + render->screenX;
        for (x = 0; x < render->screenWidth; ++x) {
            const uint16_t color = source[x];
            const uint8_t r = expand5(color >> 11);
            const uint8_t g = expand6(color >> 5);
            const uint8_t b = expand5(color);
            const uint8_t lum = (uint8_t)(((uint32_t)77U * r +
                                           (uint32_t)150U * g +
                                           (uint32_t)29U * b) >> 8);
            const int rg = (int)r - (int)g;
            const int rb = (int)r - (int)b;
            const int gb = (int)g - (int)b;

            sumR += r;
            sumG += g;
            sumB += b;
            sumY += lum;
            ++bins[lum >> 6];
            if (lum < minY) minY = lum;
            if (lum > maxY) maxY = lum;
            if (rg >= -4 && rg <= 4 && rb >= -4 && rb <= 4 &&
                gb >= -4 && gb <= 4) {
                ++neutral;
            }
            ++count;
        }
    }

    if (count == 0U) {
        printf("[JUNCTIONFRAME] COLORSTATS FAILED empty viewport\n");
        return;
    }

    printf("[JUNCTIONFRAME] COLORSTATS viewport=%dx%d@%d,%d meanRGB=%u/%u/%u meanY=%u minY=%u maxY=%u binsY=0-63:%u 64-127:%u 128-191:%u 192-255:%u neutral=%u/%u framebufferUntouched=yes\n",
           render->screenWidth, render->screenHeight,
           render->screenX, render->screenY,
           (unsigned int)(sumR / count),
           (unsigned int)(sumG / count),
           (unsigned int)(sumB / count),
           (unsigned int)(sumY / count),
           (unsigned int)minY,
           (unsigned int)maxY,
           (unsigned int)bins[0],
           (unsigned int)bins[1],
           (unsigned int)bins[2],
           (unsigned int)bins[3],
           (unsigned int)neutral,
           (unsigned int)count);
}

static int presentationBufferValid(const Render_t* render) {
    return render != NULL && render->framebuffer != NULL &&
           render->framebuffer == (byte*)Esp32PlatformVideo_framebuffer() &&
           Esp32PlatformVideo_framebufferSizeBytes() ==
               (size_t)DOOMRPG_LOGICAL_WIDTH *
               (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
}

static void transformInvert(uint16_t* pixels, size_t count) {
    size_t i;
    for (i = 0U; i < count; ++i) pixels[i] ^= 0xffffU;
}

static void transformByteSwap(uint16_t* pixels, size_t count) {
    size_t i;
    for (i = 0U; i < count; ++i) {
        const uint16_t color = pixels[i];
        pixels[i] = (uint16_t)((color << 8) | (color >> 8));
    }
}

static void transformRedBlueSwap(uint16_t* pixels, size_t count) {
    size_t i;
    for (i = 0U; i < count; ++i) {
        const uint16_t color = pixels[i];
        pixels[i] = (uint16_t)(((color & 0x001fU) << 11) |
                               (color & 0x07e0U) |
                               ((color & 0xf800U) >> 11));
    }
}

static int showReversibleProfile(const char* name,
                                 void (*transform)(uint16_t*, size_t),
                                 uint16_t* pixels,
                                 size_t count) {
    int result;
    printf("[VIDEOCAL] PROFILE %s hold=2500ms framebufferRestored=yes\n", name);
    if (transform != NULL) transform(pixels, count);
    result = __real_Esp32PlatformVideo_present();
    if (transform != NULL) transform(pixels, count);
    vTaskDelay(pdMS_TO_TICKS(2500));
    return result;
}

int __wrap_Esp32PlatformVideo_present(void) {
    uint16_t* pixels;
    size_t count;
    int ok = 1;

    if (!presentationArmed || !presentationBufferValid(presentationRender)) {
        return __real_Esp32PlatformVideo_present();
    }

    pixels = (uint16_t*)presentationRender->framebuffer;
    count = (size_t)DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT;

    printf("[VIDEOCAL] START same logical frame; choose closest profile by eye\n");
    ok &= showReversibleProfile("A CURRENT", NULL, pixels, count);
    ok &= showReversibleProfile("B PRE-INVERT", transformInvert, pixels, count);
    ok &= showReversibleProfile("C PRE-BYTESWAP", transformByteSwap, pixels, count);
    ok &= showReversibleProfile("D PRE-RBSWAP", transformRedBlueSwap, pixels, count);

    printf("[VIDEOCAL] RESTORE A CURRENT final=yes\n");
    ok &= __real_Esp32PlatformVideo_present();
    return ok;
}

void Esp32FirstFrameDiagnostic_reset(void) {
    pendingDumpRender = NULL;
    presentationRender = NULL;
    presentationArmed = 0;
    dumpAttempted = 0;
    dumpSucceeded = 0;
}

int Esp32FirstFrameDiagnostic_exportBmp(void) {
    uint32_t dumpFNV = 0U;

    if (dumpAttempted) return dumpSucceeded;
    dumpAttempted = 1;
    if (pendingDumpRender == NULL) {
        printf("[JUNCTIONFRAME] BMP FAILED no validated frame pending\n");
        return 0;
    }

    dumpSucceeded = dumpViewportBmp(pendingDumpRender, &dumpFNV) &&
                    dumpFNV != 0U;
    return dumpSucceeded;
}

EspNativeFirstFrameStatus __wrap_EspNativeFirstFrame_route(
    struct Render_s* renderBase,
    const struct EspPlayerViewState_s* playerView) {
    Render_t* render = (Render_t*)renderBase;
    EspNativeFirstFrameStatus status;

    presentationRender = render;
    presentationArmed = 1;
    status = __real_EspNativeFirstFrame_route(renderBase, playerView);
    presentationArmed = 0;
    presentationRender = NULL;

    if (status == ESP_NATIVE_FIRST_FRAME_OK) {
        measureViewport(render);
        pendingDumpRender = render;
    }
    return status;
}
