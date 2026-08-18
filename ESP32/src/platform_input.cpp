#include "platform_input.h"

#include "board_config.h"

PlatformInput::PlatformInput(SoftXpt2046& touchscreen)
    : touchscreen_(touchscreen) {}

void PlatformInput::begin() {
    touchscreen_.begin();
}

bool PlatformInput::touched() const {
    return touchscreen_.touched();
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
    return true;
}

int16_t PlatformInput::mapAxis(uint16_t raw, uint16_t rawMinimum,
                               uint16_t rawMaximum, int16_t screenMaximum) {
    const uint16_t clamped = constrain(raw, rawMinimum, rawMaximum);
    return static_cast<int16_t>(
        (static_cast<uint32_t>(clamped - rawMinimum) * screenMaximum) /
        (rawMaximum - rawMinimum));
}
