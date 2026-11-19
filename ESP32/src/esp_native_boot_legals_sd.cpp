#include "esp_native_boot_legals_sd.h"
#include <Arduino.h>
#include <SD.h>

namespace {
File creditsFile;
}

extern "C" int EspNativeBootLegalsSd_open(uint32_t* fileSize) {
    if (fileSize != nullptr) *fileSize = 0U;
    if (creditsFile) creditsFile.close();
    creditsFile = SD.open(ESP_NATIVE_BOOT_LEGALS_SD_PATH, FILE_READ);
    if (!creditsFile || creditsFile.isDirectory()) {
        if (creditsFile) creditsFile.close();
        return 0;
    }
    const size_t length = creditsFile.size();
    if (length > UINT32_MAX) {
        creditsFile.close();
        return 0;
    }
    if (fileSize != nullptr) *fileSize = static_cast<uint32_t>(length);
    return 1;
}

extern "C" int EspNativeBootLegalsSd_read(uint32_t offset,
                                           void* dest, size_t length) {
    if (!creditsFile || dest == nullptr || length == 0U ||
        offset > creditsFile.size() ||
        length > creditsFile.size() - offset ||
        !creditsFile.seek(offset)) return 0;
    return creditsFile.read(static_cast<uint8_t*>(dest), length) == length;
}

extern "C" void EspNativeBootLegalsSd_close(void) {
    if (creditsFile) creditsFile.close();
}
