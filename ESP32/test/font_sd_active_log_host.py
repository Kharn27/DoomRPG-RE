#!/usr/bin/env python3
"""Exercise the REAL FONTSD ACTIVE log gate with a host C compiler."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
src = (root / "src/esp_native_indexed_bmp.c").read_text()

needle = "static void logFontSdActiveOnce("
begin = src.index(needle)
brace = src.index("{", begin)
depth = 0
end = None
for i in range(brace, len(src)):
    if src[i] == "{":
        depth += 1
    elif src[i] == "}":
        depth -= 1
        if depth == 0:
            end = i + 1
            break
assert end is not None
body = src[begin:end]
assert "static uint8_t fontSdActiveLoggedMask;" in src
assert src.count("logFontSdActiveOnce(name, outBmp, sdSize, source);") == 2
assert src.count('printf("[FONTSD] ACTIVE') == 1
assert "EspNativeFontSd_reject(source)" in src
assert "EspNativeFontSd_probe(name, &sdSize)" in src
assert "EspNativeIndexedBmp_openSdFontStandalone(" in src
assert "EspNativeIndexedBmp_open(" in src

harness = r"""
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ESP_NATIVE_FONT_SD_STANDARD 1U
#define ESP_NATIVE_FONT_SD_LARGER 2U
typedef struct EspNativeIndexedBmp {
    uint8_t bitsPerPixel;
    uint16_t paletteCount;
} EspNativeIndexedBmp;

static unsigned count;
static int captureFontLog(const char* fmt, ...) {
    char buffer[200];
    va_list args;
    va_start(args,fmt);
    const int length=vsnprintf(buffer,sizeof(buffer),fmt,args);
    va_end(args);
    assert(length>0);
    assert(strstr(buffer,"[FONTSD] ACTIVE")!=NULL);
    assert(strstr(buffer,"source=SD")!=NULL);
    ++count;
    return length;
}
#define printf captureFontLog
static uint8_t fontSdActiveLoggedMask;
""" + body + r"""
#undef printf
int main(void) {
    EspNativeIndexedBmp small={4U,16U},large={4U,16U};
    for(unsigned i=0;i<200U;++i) {
        logFontSdActiveOnce("a.bmp",&small,5302U,1U);
    }
    assert(count==1U && fontSdActiveLoggedMask==1U);
    for(unsigned i=0;i<150U;++i) {
        logFontSdActiveOnce("larger_font.bmp",&large,12000U,2U);
    }
    assert(count==2U && fontSdActiveLoggedMask==3U);
    logFontSdActiveOnce(NULL,&small,5302U,1U);
    logFontSdActiveOnce("a.bmp",NULL,5302U,1U);
    logFontSdActiveOnce("bad",&small,5302U,0U);
    logFontSdActiveOnce("bad",&small,5302U,3U);
    assert(count==2U);
    puts("FONTSD ACTIVE once per SD font/boot, fallback diagnostic preserved: PASS");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doomrpg-fontsd-log-") as tmp:
    c = Path(tmp) / "main.c"
    binary = Path(tmp) / "main"
    c.write_text(harness)
    subprocess.run([
        "cc", "-O2", "-std=c11", "-Wall", "-Wextra", "-Werror",
        str(c), "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)
