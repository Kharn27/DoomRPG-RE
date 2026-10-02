#include <SDL.h>
#include <string.h>

#include "esp_native_text_format.h"

char* EspNativeText_buildDivider(char out[32], const char* text) {
    int i;
    int index;
    int len;
    int count;

    if (out == NULL || text == NULL) return out;

    out[0] = '\0';
    len = SDL_strlen(text);
    count = (15 - (len + 2)) / 2;
    index = 0;

    for (i = 0; i < count; ++i) {
        out[index++] = (char)0x80;
    }

    out[index] = ' ';
    strncpy(&out[index + 1], text, 32);
    index = index + 1 + len;
    out[index] = ' ';

    for (i = 0; i < count; ++i) {
        out[index + 1] = (char)0x80;
        ++index;
    }

    out[index + 1] = '\0';
    return out;
}
