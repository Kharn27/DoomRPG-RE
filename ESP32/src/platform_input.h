#pragma once

#include <Arduino.h>

#include "soft_xpt2046.h"

struct PlatformTouchPoint {
    int16_t x;
    int16_t y;
    uint16_t pressure;
    uint16_t rawX;
    uint16_t rawY;
};

class PlatformInput {
public:
    explicit PlatformInput(SoftXpt2046& touchscreen);

    void begin();
    bool touched();
    bool readTouch(PlatformTouchPoint& point);

private:
    static int16_t mapAxis(uint16_t raw, uint16_t rawMinimum,
                           uint16_t rawMaximum, int16_t screenMaximum);

    SoftXpt2046& touchscreen_;
    bool tapDelivered_;
    uint32_t releaseSince_;
};
