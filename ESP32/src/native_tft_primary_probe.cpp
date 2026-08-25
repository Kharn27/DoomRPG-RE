#include <Arduino.h>
#include <TFT_eSPI.h>

#include "board_config.h"
#include "esp_native_first_frame.h"
#include "platform_video_c_bridge.h"

namespace {

constexpr uint32_t kExpectedJunctionFrameFNV = 0x8910c2edU;
constexpr uint32_t kProbeHoldMs = 3000U;
bool attempted = false;

uint16_t swapRedBlue565(uint16_t color) {
    return static_cast<uint16_t>(((color & 0x001fU) << 11) |
                                 (color & 0x07e0U) |
                                 ((color & 0xf800U) >> 11));
}

void drawPrimaryPattern(TFT_eSPI& display, bool softwareRedBlueSwap) {
    uint16_t colors[4] = {
        static_cast<uint16_t>(0xf800U),
        static_cast<uint16_t>(0x07e0U),
        static_cast<uint16_t>(0x001fU),
        static_cast<uint16_t>(0x8410U),
    };

    if (softwareRedBlueSwap) {
        colors[0] = swapRedBlue565(colors[0]);
        colors[1] = swapRedBlue565(colors[1]);
        colors[2] = swapRedBlue565(colors[2]);
        colors[3] = swapRedBlue565(colors[3]);
    }

    const int stripeWidth = display.width() / 4;
    const int height = display.height();
    display.fillRect(0 * stripeWidth, 0, stripeWidth, height, colors[0]);
    display.fillRect(1 * stripeWidth, 0, stripeWidth, height, colors[1]);
    display.fillRect(2 * stripeWidth, 0, stripeWidth, height, colors[2]);
    display.fillRect(3 * stripeWidth, 0,
                     display.width() - 3 * stripeWidth, height, colors[3]);
}

}  // namespace

void Esp32TftPrimaryProbe_service() {
    if (attempted || !EspNativeFirstFrame_isReady()) {
        return;
    }
    attempted = true;

    const EspNativeFirstFrameState* frame = EspNativeFirstFrame_view();
    if (frame == nullptr || frame->frameAfterFNV != kExpectedJunctionFrameFNV ||
        frame->presented != 1U || frame->active != 1U) {
        Serial.printf("[VIDEOPRIMARY] REFUSED frame=%08x presented=%u active=%u\n",
                      static_cast<unsigned int>(frame ? frame->frameAfterFNV : 0U),
                      static_cast<unsigned int>(frame ? frame->presented : 0U),
                      static_cast<unsigned int>(frame ? frame->active : 0U));
        return;
    }

    /* This service is called only after the canonical first-frame route has
     * completed all of its predecessor, heap and framebuffer checks.  There is
     * deliberately no static constructor, task, queue or boot-time allocation.
     * The logical framebuffer is never modified by this visual-only probe. */
    TFT_eSPI diagnosticDisplay;
    diagnosticDisplay.begin();
    diagnosticDisplay.setRotation(cyd::kDisplayRotation);
    diagnosticDisplay.invertDisplay(true);
    diagnosticDisplay.setSwapBytes(true);

    Serial.println("[VIDEOPRIMARY] START framebuffer=8910c2ed untouched=yes invert=on; identify which screen is true RED|GREEN|BLUE|GRAY left-to-right");

    drawPrimaryPattern(diagnosticDisplay, false);
    Serial.printf("[VIDEOPRIMARY] PROFILE RAW source=RED|GREEN|BLUE|GRAY softwareRbSwap=no hold=%lums\n",
                  static_cast<unsigned long>(kProbeHoldMs));
    delay(kProbeHoldMs);

    drawPrimaryPattern(diagnosticDisplay, true);
    Serial.printf("[VIDEOPRIMARY] PROFILE RBSWAP source=RED|GREEN|BLUE|GRAY softwareRbSwap=yes hold=%lums\n",
                  static_cast<unsigned long>(kProbeHoldMs));
    delay(kProbeHoldMs);

    diagnosticDisplay.invertDisplay(true);
    diagnosticDisplay.setSwapBytes(true);
    const int restored = Esp32PlatformVideo_present();
    Serial.printf("[VIDEOPRIMARY] RESTORE gameplayFrame=yes result=%s framebuffer=%08x untouched=yes\n",
                  restored ? "OK" : "FAILED",
                  static_cast<unsigned int>(frame->frameAfterFNV));
}
