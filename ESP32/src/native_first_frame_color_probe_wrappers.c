#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_native_first_frame.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

/*
 * Temporary first-frame-only hardware diagnostic.
 *
 * The rendered Junction viewport has already proven dark in RAM while the real
 * CYD still looks visually washed out.  Leave the 160x80 gameplay viewport
 * untouched, but draw a small set of known RGB565 swatches in the otherwise
 * unused bottom 20 logical rows immediately before the first-frame present.
 * This gives a panel-side visual reference that is independent of Doom assets,
 * palettes and BSP rendering.  The viewport statistics remain read-only.
 */

static int firstFrameRouteActive;
static int swatchesApplied;

EspNativeFirstFrameStatus __real_EspNativeFirstFrame_route(
    struct Render_s* render,
    const struct EspPlayerViewState_s* playerView);
int __real_Esp32PlatformVideo_present(void);

static uint32_t fnv1a32(const void* data, size_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t hash = 2166136261U;
    size_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)((((uint16_t)r >> 3) << 11) |
                      (((uint16_t)g >> 2) << 5) |
                      ((uint16_t)b >> 3));
}

static void drawKnownSwatches(void) {
    static const uint16_t colors[] = {
        0x0000U, /* black */
        0x4208U, /* neutral gray 64 */
        0x8410U, /* neutral gray 128 */
        0xffffU, /* white */
        0xf800U, /* red */
        0x07e0U, /* green */
        0x001fU  /* blue */
    };
    uint16_t* framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    const size_t expected =
        (size_t)DOOMRPG_LOGICAL_WIDTH *
        (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
    const int swatchCount = (int)(sizeof(colors) / sizeof(colors[0]));
    const int y0 = 100;
    const int height = 20;
    const int width = DOOMRPG_LOGICAL_WIDTH / swatchCount;
    int i;
    int y;
    int x;

    if (framebuffer == NULL || bytes != expected) return;

    for (i = 0; i < swatchCount; ++i) {
        const int x0 = i * width;
        const int x1 = (i == swatchCount - 1)
                           ? DOOMRPG_LOGICAL_WIDTH
                           : (i + 1) * width;
        for (y = y0; y < y0 + height; ++y) {
            for (x = x0; x < x1; ++x) {
                framebuffer[y * DOOMRPG_LOGICAL_WIDTH + x] = colors[i];
            }
        }
    }
    swatchesApplied = 1;
}

int __wrap_Esp32PlatformVideo_present(void) {
    if (firstFrameRouteActive && !swatchesApplied) {
        drawKnownSwatches();
    }
    return __real_Esp32PlatformVideo_present();
}

static uint8_t expand5(uint16_t value) {
    value &= 31U;
    return (uint8_t)((value << 3) | (value >> 2));
}

static uint8_t expand6(uint16_t value) {
    value &= 63U;
    return (uint8_t)((value << 2) | (value >> 4));
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
        const uint16_t* row = framebuffer +
            (render->screenY + y) * pitchPixels + render->screenX;
        for (x = 0; x < render->screenWidth; ++x) {
            const uint16_t color = row[x];
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

    printf("[JUNCTIONFRAME] COLORSTATS viewport=%dx%d@%d,%d meanRGB=%u/%u/%u meanY=%u minY=%u maxY=%u binsY=0-63:%u 64-127:%u 128-191:%u 192-255:%u neutral=%u/%u framebufferViewportUntouched=yes\n",
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

EspNativeFirstFrameStatus __wrap_EspNativeFirstFrame_route(
    struct Render_s* renderBase,
    const struct EspPlayerViewState_s* playerView) {
    Render_t* render = (Render_t*)renderBase;
    EspNativeFirstFrameStatus status;

    firstFrameRouteActive = 1;
    swatchesApplied = 0;
    status = __real_EspNativeFirstFrame_route(renderBase, playerView);
    firstFrameRouteActive = 0;

    if (status == ESP_NATIVE_FIRST_FRAME_OK) {
        EspNativeFirstFrameState* state =
            (EspNativeFirstFrameState*)EspNativeFirstFrame_view();
        if (!swatchesApplied || state == NULL || render == NULL ||
            render->framebuffer == NULL) {
            printf("[JUNCTIONFRAME] SWATCH FAILED applied=%d owner=%p render=%p\n",
                   swatchesApplied, (void*)state, (void*)render);
            return ESP_NATIVE_FIRST_FRAME_PRESENT_FAILED;
        }

        state->frameAfterFNV = fnv1a32(
            render->framebuffer,
            (size_t)DOOMRPG_LOGICAL_WIDTH *
                (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t));
        printf("[JUNCTIONFRAME] SWATCH y=100..119 order=black,gray64,gray128,white,red,green,blue values=0000,4208,8410,ffff,f800,07e0,001f\n");
        measureViewport(render);
    }
    return status;
}
