#include "platform_input.h"

#include "board_config.h"
#include "platform_touch_events.h"

#ifndef DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
#define DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY 0
#endif

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
#include "platform_video.h"
#endif

namespace {
constexpr uint32_t kTapReleaseDebounceMs = 50;
PlatformTapCallback gTapCallback = nullptr;
}

extern "C" void PlatformInput_setTapCallback(PlatformTapCallback callback) {
    gTapCallback = callback;
}

PlatformInput::PlatformInput(SoftXpt2046& touchscreen)
    : touchscreen_(touchscreen), tapDelivered_(false), releaseSince_(0) {}

void PlatformInput::begin() {
    touchscreen_.begin();
    tapDelivered_ = false;
    releaseSince_ = 0;
}

bool PlatformInput::touched() {
    const bool active = touchscreen_.touched();
    const uint32_t now = millis();

    if (active) {
        releaseSince_ = 0;

        /* Deliver semantic input immediately on the press edge. The legacy
         * Serial diagnostic reads coordinates only every ~80 ms; menu input
         * must not inherit that throttle or short taps could be missed.
         */
        if (!tapDelivered_) {
            PlatformTouchPoint point{};
            (void)readTouch(point);
        }
    }
    else if (tapDelivered_) {
        if (releaseSince_ == 0) {
            releaseSince_ = now;
        }
        else if ((now - releaseSince_) >= kTapReleaseDebounceMs) {
            tapDelivered_ = false;
            releaseSince_ = 0;
        }
    }

    return active;
}

bool PlatformInput::readTouch(PlatformTouchPoint& point) {
    TouchSample sample{};
    if (!touchscreen_.read(sample)) {
        return false;
    }

    point.rawX = sample.x;
    point.rawY = sample.y;
    point.pressure = sample.pressure;

    // The XPT2046 axes are tied to the physical panel and do not rotate with
    // TFT_eSPI. In landscape rotation 1, panel Y maps to screen X and panel X
    // maps to screen Y.
    point.x = mapAxis(sample.y, cyd::kTouchRawMinY, cyd::kTouchRawMaxY,
                      cyd::kScreenWidth - 1);
    point.y = mapAxis(sample.x, cyd::kTouchRawMinX, cyd::kTouchRawMaxX,
                      cyd::kScreenHeight - 1);

    if (!tapDelivered_) {
        tapDelivered_ = true;
        releaseSince_ = 0;

#if DOOMRPG_ESP32_TOUCH_HITBOX_OVERLAY
        /* Bring-up diagnostics observe the calibrated semantic press before any
         * menu gate can suppress ARM/CONFIRM delivery. This code is absent from
         * the normal esp32-cyd build at preprocessing time.
         */
        PlatformVideo_debugOverlayMarkTouch(point.x,
                                            point.y,
                                            point.pressure,
                                            point.rawX,
                                            point.rawY);
#endif

        if (gTapCallback != nullptr) {
            gTapCallback(point.x, point.y, point.pressure, point.rawX, point.rawY);
        }
    }

    return true;
}

int16_t PlatformInput::mapAxis(uint16_t raw, uint16_t rawMinimum,
                               uint16_t rawMaximum, int16_t screenMaximum) {
    const uint16_t clamped = constrain(raw, rawMinimum, rawMaximum);
    return static_cast<int16_t>(
        (static_cast<uint32_t>(clamped - rawMinimum) * screenMaximum) /
        (rawMaximum - rawMinimum));
}
