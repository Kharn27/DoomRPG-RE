#pragma once

#include <Arduino.h>

struct TouchSample {
    uint16_t x;
    uint16_t y;
    uint16_t pressure;
};

class SoftXpt2046 {
public:
    SoftXpt2046(uint8_t mosi, uint8_t miso, uint8_t clock, uint8_t chipSelect,
                uint8_t irq);

    void begin();
    bool touched() const;
    bool read(TouchSample& sample);

private:
    static constexpr size_t kSampleCount = 5;

    uint8_t transfer(uint8_t value);
    uint16_t readAdc(uint8_t command);
    uint16_t readMedian(uint8_t command);
    static uint16_t median(uint16_t* values, size_t count);

    uint8_t mosi_;
    uint8_t miso_;
    uint8_t clock_;
    uint8_t chipSelect_;
    uint8_t irq_;
};
