#include "soft_xpt2046.h"

namespace {

constexpr uint8_t kCommandX = 0xD0;
constexpr uint8_t kCommandY = 0x90;
constexpr uint8_t kCommandZ1 = 0xB0;
constexpr uint8_t kCommandZ2 = 0xC0;

}  // namespace

SoftXpt2046::SoftXpt2046(uint8_t mosi, uint8_t miso, uint8_t clock,
                         uint8_t chipSelect, uint8_t irq)
    : mosi_(mosi),
      miso_(miso),
      clock_(clock),
      chipSelect_(chipSelect),
      irq_(irq) {}

void SoftXpt2046::begin() {
    pinMode(mosi_, OUTPUT);
    pinMode(miso_, INPUT);
    pinMode(clock_, OUTPUT);
    pinMode(chipSelect_, OUTPUT);
    pinMode(irq_, INPUT);

    digitalWrite(mosi_, LOW);
    digitalWrite(clock_, LOW);
    digitalWrite(chipSelect_, HIGH);
}

bool SoftXpt2046::touched() const {
    return digitalRead(irq_) == LOW;
}

bool SoftXpt2046::read(TouchSample& sample) {
    if (!touched()) {
        return false;
    }

    sample.x = readMedian(kCommandX);
    sample.y = readMedian(kCommandY);

    const uint16_t z1 = readAdc(kCommandZ1);
    const uint16_t z2 = readAdc(kCommandZ2);
    sample.pressure = z1 + 4095U - z2;

    return touched();
}

uint8_t SoftXpt2046::transfer(uint8_t value) {
    uint8_t result = 0;

    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
        digitalWrite(mosi_, (value & mask) ? HIGH : LOW);
        delayMicroseconds(1);

        digitalWrite(clock_, HIGH);
        result <<= 1;
        if (digitalRead(miso_)) {
            result |= 1;
        }
        delayMicroseconds(1);
        digitalWrite(clock_, LOW);
    }

    return result;
}

uint16_t SoftXpt2046::readAdc(uint8_t command) {
    digitalWrite(chipSelect_, LOW);
    transfer(command);
    const uint16_t value =
        (static_cast<uint16_t>(transfer(0x00)) << 8) | transfer(0x00);
    digitalWrite(chipSelect_, HIGH);

    return (value >> 3) & 0x0FFF;
}

uint16_t SoftXpt2046::readMedian(uint8_t command) {
    uint16_t samples[kSampleCount];
    for (size_t i = 0; i < kSampleCount; ++i) {
        samples[i] = readAdc(command);
    }
    return median(samples, kSampleCount);
}

uint16_t SoftXpt2046::median(uint16_t* values, size_t count) {
    for (size_t i = 1; i < count; ++i) {
        const uint16_t value = values[i];
        size_t pos = i;
        while (pos > 0 && values[pos - 1] > value) {
            values[pos] = values[pos - 1];
            --pos;
        }
        values[pos] = value;
    }
    return values[count / 2];
}
