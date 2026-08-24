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

void EspProbeLog_setQuiet(int quiet) {
    probeLogQuiet = quiet ? 1 : 0;
}

int EspProbeLog_isQuiet(void) {
    return probeLogQuiet;
}

int __wrap_printf(const char* format, ...) {
    va_list args;
    int result;

    if (probeLogQuiet && !mustSurface(format)) return 0;

    va_start(args, format);
    result = vprintf(format, args);
    va_end(args);
    return result;
}

int __wrap_puts(const char* text) {
    if (probeLogQuiet && !mustSurface(text)) return 0;
    return __real_puts(text);
}
