#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "Render.h"

#include "esp_native_first_frame.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

/*
 * Temporary first-frame-only hardware diagnostic.
 *
 * The hardware-proven sparse graphics catalog stores the direct CYD
 * framebuffer RGB565 words from palettes.bin. The current v2 wall/plane
 * rasterizers still apply a red/blue swap locally while resolving those
 * palettes. Rather than rewriting both rasterizers before proving the visible
 * consequence on hardware, this wrapper cancels that exact transform once,
 * immediately before the one first-frame presentation. Menu/intro and every
 * other PlatformVideo presentation remain untouched.
 *
 * If hardware proves this representation, the post-pass must be removed and
 * the redundant per-palette swaps deleted at their rasterizer sources.
 */

static int firstFrameRouteActive;
static int colorCorrectionApplied;
static uint32_t colorBeforeFNV;
static uint32_t colorAfterFNV;

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

static uint16_t swapRedBlue565(uint16_t color) {
    return (uint16_t)(((color & 0x001fU) << 11) |
                      (color & 0x07e0U) |
                      ((color & 0xf800U) >> 11));
}

int __wrap_Esp32PlatformVideo_present(void) {
    if (firstFrameRouteActive && !colorCorrectionApplied) {
        uint16_t* framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
        const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
        const size_t expected =
            (size_t)DOOMRPG_LOGICAL_WIDTH *
            (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
        size_t i;

        if (framebuffer == NULL || bytes != expected) {
            printf("[JUNCTIONFRAME] COLOR FAILED framebuffer=%p bytes=%u expected=%u\n",
                   (void*)framebuffer,
                   (unsigned int)bytes,
                   (unsigned int)expected);
            return 0;
        }

        colorBeforeFNV = fnv1a32(framebuffer, bytes);
        for (i = 0U; i < bytes / sizeof(uint16_t); ++i) {
            framebuffer[i] = swapRedBlue565(framebuffer[i]);
        }
        colorAfterFNV = fnv1a32(framebuffer, bytes);
        colorCorrectionApplied = 1;
    }

    return __real_Esp32PlatformVideo_present();
}

EspNativeFirstFrameStatus __wrap_EspNativeFirstFrame_route(
    struct Render_s* render,
    const struct EspPlayerViewState_s* playerView) {
    EspNativeFirstFrameStatus status;

    firstFrameRouteActive = 1;
    colorCorrectionApplied = 0;
    colorBeforeFNV = 0U;
    colorAfterFNV = 0U;

    status = __real_EspNativeFirstFrame_route(render, playerView);
    firstFrameRouteActive = 0;

    if (status == ESP_NATIVE_FIRST_FRAME_OK) {
        EspNativeFirstFrameState* state;
        if (!colorCorrectionApplied || colorBeforeFNV == 0U ||
            colorAfterFNV == 0U || colorBeforeFNV == colorAfterFNV) {
            printf("[JUNCTIONFRAME] COLOR FAILED applied=%d before=%08x after=%08x\n",
                   colorCorrectionApplied,
                   (unsigned int)colorBeforeFNV,
                   (unsigned int)colorAfterFNV);
            return ESP_NATIVE_FIRST_FRAME_PRESENT_FAILED;
        }

        state = (EspNativeFirstFrameState*)EspNativeFirstFrame_view();
        if (state == NULL || !state->active || !state->presented) {
            printf("[JUNCTIONFRAME] COLOR FAILED owner unavailable after present\n");
            return ESP_NATIVE_FIRST_FRAME_PRESENT_FAILED;
        }

        /* Keep the probe owner consistent with the pixels actually presented. */
        state->frameAfterFNV = colorAfterFNV;
        printf("[JUNCTIONFRAME] COLOR directCatalogRgb565=yes cancelLegacyRbSwap=yes frame=%08x->%08x\n",
               (unsigned int)colorBeforeFNV,
               (unsigned int)colorAfterFNV);
    }

    return status;
}
