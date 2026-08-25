#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "esp_probe_log.h"

static int probeLogQuiet;

int __real_puts(const char* text);

static int mustSurface(const char* text) {
    if (text == NULL) return 0;
    return strstr(text, "FAILED") != NULL ||
           strstr(text, "ERROR") != NULL ||
           strstr(text, "REFUSED") != NULL ||
           strstr(text, "PANIC") != NULL ||
           strstr(text, "ASSERT") != NULL;
}

/*
 * Normal first-frame development no longer needs a transcript of already
 * hardware-proven menu/intro decode, animation, touch and heartbeat activity.
 * Keep the current native-frame tags visible and always surface failures.
 */
static int routineBootNoise(const char* text) {
    static const char* prefixes[] = {
        "[ALIVE]",
        "[VIDEO] Present",
        "[MENUTOUCH]",
        "[TOUCH]",
        "[MAINSTART]",
        "[ZIP]",
        "[BMP]",
        "[SDL] Adopt",
        "[INTROFIT]",
        "[INTRO1]",
        "[INTROCLK]",
        "[INTROIN]",
        "[INTRODISP]",
        "=== Doom RPG ESP32 real MENU_MAIN",
        "=== Doom RPG ESP32 bounded first ST_INTRO",
        "=== Doom RPG ESP32 bounded intro disposal"
    };
    unsigned int i;

    if (text == NULL) return 0;
    for (i = 0U; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        if (strncmp(text, prefixes[i], strlen(prefixes[i])) == 0) return 1;
    }
    return 0;
}

void EspProbeLog_setQuiet(int quiet) {
    probeLogQuiet = quiet ? 1 : 0;
}

int EspProbeLog_isQuiet(void) {
    return probeLogQuiet;
}

int __wrap_printf(const char* format, ...) {
    va_list args;
    int result;

    if ((probeLogQuiet || routineBootNoise(format)) && !mustSurface(format)) {
        return 0;
    }

    va_start(args, format);
    result = vprintf(format, args);
    va_end(args);
    return result;
}

int __wrap_puts(const char* text) {
    if ((probeLogQuiet || routineBootNoise(text)) && !mustSurface(text)) return 0;
    return __real_puts(text);
}
