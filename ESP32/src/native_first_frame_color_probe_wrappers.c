#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_native_first_frame.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"

/*
 * Temporary first-frame-only hardware diagnostic.
 *
 * The previous R/B cancellation experiment changed the framebuffer hash but
 * produced no meaningful visual change on the real CYD, so it is deliberately
 * removed here.  This wrapper leaves every pixel untouched and measures the
 * exact RGB565 viewport already rendered/presented by EspNativeFirstFrame.
 * The resulting channel/luminance statistics distinguish an image that is
 * already washed out in RAM from one that only becomes washed out later in the
 * TFT presentation path.
 */

EspNativeFirstFrameStatus __real_EspNativeFirstFrame_route(
    struct Render_s* render,
    const struct EspPlayerViewState_s* playerView);
int __real_Esp32PlatformVideo_present(void);

int __wrap_Esp32PlatformVideo_present(void) {
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

EspNativeFirstFrameStatus __wrap_EspNativeFirstFrame_route(
    struct Render_s* render,
    const struct EspPlayerViewState_s* playerView) {
    const EspNativeFirstFrameStatus status =
        __real_EspNativeFirstFrame_route(render, playerView);

    if (status == ESP_NATIVE_FIRST_FRAME_OK) {
        measureViewport((const Render_t*)render);
    }
    return status;
}
