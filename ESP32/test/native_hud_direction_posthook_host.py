#!/usr/bin/env python3
"""ARCH-RC5: real-source C HUD direction -> explicit player-HUD posthook parity.

Extract the actual five C helpers, compass owner and projection hook. Compile
them with bounded fake bitmap/PAK/platform services to validate the original
wrapper's success/failure and resource-order contract. These are host fixtures,
NOT pixel goldens or real-CYD execution. A hardware Serial run is mandatory.
"""
from __future__ import annotations

from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ESP = Path(__file__).resolve().parents[1]
OWNER = ESP / "src/esp_native_gameplay_hud_direction.c"
OVERLAY = ESP / "src/esp_native_gameplay_projection_overlay.c"
ABI = ESP / "include/esp_native_gameplay_projection_overlay.h"
INI = ESP / "platformio.ini"


def require(ok: bool, why: str) -> None:
    if not ok:
        raise AssertionError(why)


def extract(source: str, signature: str) -> str:
    require(source.count(signature) == 1, "missing/duplicate real body: " + signature)
    first = source.index(signature)
    opening = source.find("{", first)
    require(opening >= 0, "missing opening brace: " + signature)
    level = 0
    for i in range(opening, len(source)):
        if source[i] == "{":
            level += 1
        elif source[i] == "}":
            level -= 1
            if level == 0:
                return source[first:i + 1]
    raise AssertionError("unclosed real source: " + signature)


PREFIX = r'''
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "esp_native_gameplay_hud_direction.h"
#include "esp_native_gameplay_projection_overlay.h"
#define DOOMRPG_LOGICAL_WIDTH 160
#define DOOMRPG_LOGICAL_HEIGHT 120
#define ESP_ASSET_PACK_DEFAULT_PATH "/DoomRPG-ESP32.pak"
#define ESP_NATIVE_INDEXED_BMP_OK 0

struct EspNativeIndexedBmp { uint16_t width, height; };
struct EspNativeIndexedBmpStats {
    uint32_t packReads, bytesRead, rowsRead, pixelsWritten;
};
static uint16_t screen[160U * 120U];
static bool packOpen = false, openAllowed = true, framebufferAllowed = true;
static bool statusReady = false, statusPaintAllowed = true, postAllowed = true;
static bool mallocAllowed = true;
static int bmpFailAt = 0, blitFailAt = 0;
static unsigned int opens = 0, closes = 0, bmpCalls = 0, blitCalls = 0;
static unsigned int allocations = 0, releases = 0, liveScratch = 0;
static unsigned int postCalls = 0, postOrderFailures = 0;

static void* trackedMalloc(size_t n) {
    if (!mallocAllowed) return nullptr;
    void* p = std::malloc(n);
    if (p != nullptr) { ++allocations; ++liveScratch; }
    return p;
}
static void trackedFree(void* p) {
    if (p != nullptr) { ++releases; assert(liveScratch > 0U); --liveScratch; }
    std::free(p);
}
int EspAssetPack_isOpen() { return packOpen ? 1 : 0; }
int EspAssetPack_open(const char* path) {
    assert(strcmp(path, ESP_ASSET_PACK_DEFAULT_PATH) == 0);
    ++opens;
    if (packOpen || !openAllowed) return 0;
    packOpen = true;
    return 1;
}
void EspAssetPack_close() { assert(packOpen); ++closes; packOpen = false; }
void* Esp32PlatformVideo_framebuffer() {
    return framebufferAllowed ? screen : nullptr;
}
size_t Esp32PlatformVideo_framebufferSizeBytes() { return sizeof(screen); }
int EspNativeGameplayStatusMessage_isReady() { return statusReady ? 1 : 0; }
int EspNativeGameplayStatusMessage_paintIfDirty() {
    return statusPaintAllowed ? 1 : 0;
}
int EspNativeIndexedBmp_open(const char* name, EspNativeIndexedBmp* out,
                             EspNativeIndexedBmpStats* stats) {
    ++bmpCalls;
    if ((int)bmpCalls == bmpFailAt) return 1;
    if (strcmp(name, "k.bmp") == 0) { out->width=20U; out->height=20U; }
    else if (strcmp(name, "o.bmp") == 0) { out->width=16U; out->height=16U; }
    else if (strcmp(name, "a.bmp") == 0) { out->width=144U; out->height=72U; }
    else { assert(false); }
    ++stats->packReads;
    stats->bytesRead += 24U;
    stats->rowsRead += 2U;
    return ESP_NATIVE_INDEXED_BMP_OK;
}
int EspNativeIndexedBmp_blit(const EspNativeIndexedBmp* bmp,
                              uint16_t* framebuffer, int fbWidth, int fbHeight,
                              uint16_t sourceX, uint16_t sourceY,
                              uint16_t width, uint16_t height,
                              int16_t destinationX, int16_t destinationY,
                              uint8_t transparent,
                              EspNativeIndexedBmpStats* stats) {
    (void)bmp; (void)fbWidth; (void)fbHeight;
    (void)sourceX; (void)sourceY; (void)destinationX; (void)destinationY;
    (void)transparent;
    assert(framebuffer == screen && width != 0U && height != 0U);
    ++blitCalls;
    if ((int)blitCalls == blitFailAt) return 1;
    stats->pixelsWritten += (uint32_t)width * height;
    return ESP_NATIVE_INDEXED_BMP_OK;
}
static int repaintPlayerHud(uint8_t angle,
                             EspNativeGameplayHudDirectionStats* outStats) {
    ++postCalls;
    if (packOpen || liveScratch != 0U) {
        ++postOrderFailures;
        return 0;
    }
    assert(angle == 64U && outStats != nullptr);
    return postAllowed ? 1 : 0;
}
uint32_t EspNativeGameplayPlayerState_fingerprint() { return 0x11223344U; }
'''


SUFFIX = r'''
static void reset() {
    assert(liveScratch == 0U);
    packOpen = false;
    openAllowed = true;
    framebufferAllowed = true;
    statusReady = false;
    statusPaintAllowed = true;
    postAllowed = true;
    mallocAllowed = true;
    bmpFailAt = blitFailAt = 0;
    opens = closes = bmpCalls = blitCalls = 0;
    allocations = releases = liveScratch = 0;
    postCalls = postOrderFailures = 0;
}
static EspNativeGameplayHudDirectionStats stats;
static void invoke(uint8_t angle, int expected, unsigned int posts,
                   unsigned int expectedOpen = 1U,
                   unsigned int expectedClose = 1U) {
    memset(&stats, 0xa5, sizeof(stats));
    const int result = EspNativeGameplayHudDirection_render(angle, &stats);
    assert(result == expected);
    assert(postCalls == posts);
    assert(postOrderFailures == 0U);
    assert(opens == expectedOpen && closes == expectedClose);
    assert(liveScratch == 0U && allocations == releases);
    assert(!packOpen);
    if (posts != 0U) {
        assert(stats.rendered == 1U && stats.angle == angle);
        assert(stats.resourcesValidated == 3U);
        assert(stats.packReads == 3U && stats.pixelsWritten > 0U);
    } else {
        assert(stats.rendered == 0U && stats.angle == 0U);
    }
}
int main() {
    reset();
    invoke(64U, 1, 1U); // normal core + post, PAK closed before projection
    assert(bmpCalls == 3U && blitCalls > 2U);

    reset();
    postAllowed = false;
    invoke(64U, 0, 1U); // old wrapper's post-render error propagation

    reset();
    openAllowed = false;
    invoke(64U, 0, 0U, 1U, 0U); // failed native PAK open
    assert(allocations == 1U && releases == 1U);

    reset();
    bmpFailAt = 2;
    invoke(64U, 0, 0U); // mid-open error closes pack + frees scratch

    reset();
    blitFailAt = 1;
    invoke(64U, 0, 0U); // no repaint on partially rendered compass

    reset();
    statusReady = true;
    statusPaintAllowed = false;
    invoke(64U, 0, 0U, 0U, 0U); // top HUD dirty failure

    reset();
    framebufferAllowed = false;
    invoke(64U, 0, 0U, 0U, 0U); // no framebuffer

    reset();
    mallocAllowed = false;
    invoke(64U, 0, 0U, 0U, 0U); // allocation failure

    reset();
    invoke(37U, 0, 0U, 0U, 0U); // invalid direction refuses PAK

    reset();
    memset(&stats, 0xa5, sizeof(stats));
    assert(EspNativeGameplayHudDirection_render(64U, nullptr) == 0);
    assert(opens == 0U && postCalls == 0U && liveScratch == 0U);

    reset();
    packOpen = true;
    memset(&stats, 0xa5, sizeof(stats));
    assert(EspNativeGameplayHudDirection_render(64U, &stats) == 0);
    assert(packOpen && closes == 0U && opens == 0U && postCalls == 0U);
    assert(stats.rendered == 0U);

    reset();
    statusReady = true;
    invoke(64U, 1, 1U); // successful optional dirty paint

    return 0;
}
'''


def harness(owner: str, hook: str, original: str) -> str:
    hud = original
    macros = "\n".join(re.findall(r"(?m)^#define HUD_.*$", hud))
    scratch = extract(hud, "typedef struct HudDirectionScratch_s")
    helpers = [
        extract(hud, "static void mergeStats("),
        extract(hud, "static int openBmp("),
        extract(hud, "static int drawBmp("),
        extract(hud, "static char directionChar("),
        extract(hud, "static int clearCompass("),
    ]
    return "\n".join([PREFIX, macros, scratch] + helpers + [
        "#define malloc trackedMalloc", "#define free trackedFree",
        hook, owner, "#undef malloc", "#undef free", SUFFIX])


def run(source: str, expect_ok: bool) -> None:
    compiler = shutil.which("c++") or shutil.which("g++") or shutil.which("clang++")
    require(compiler is not None, "host C++ compiler unavailable")
    with tempfile.TemporaryDirectory(prefix="arch-rc5-") as folder:
        src = Path(folder) / "real.cpp"
        exe = Path(folder) / "real"
        src.write_text(source)
        build = subprocess.run(
            [compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror",
             "-I", str(ESP / "include"), str(src), "-o", str(exe)],
            capture_output=True, text=True)
        require(build.returncode == 0, "real-source host compile failed: " + build.stderr)
        result = subprocess.run([str(exe)], capture_output=True, text=True)
        require((result.returncode == 0) == expect_ok,
                f"host parity/adversarial mismatch expected={expect_ok} "
                f"rc={result.returncode} stderr={result.stderr}")


def main() -> None:
    hud = OWNER.read_text()
    overlay = OVERLAY.read_text()
    header = ABI.read_text()
    ini = INI.read_text()
    owner = extract(hud, "int EspNativeGameplayHudDirection_render(")
    hook = extract(overlay,
                   "int EspNativeGameplayProjectionOverlay_afterHudDirectionRender(")
    post = "EspNativeGameplayProjectionOverlay_afterHudDirectionRender("
    require(post in header and 'extern "C"' in header, "missing permanent C ABI")
    require('#include "esp_native_gameplay_projection_overlay.h"' in hud and
            '#include "esp_native_gameplay_projection_overlay.h"' in overlay,
            "owner/hook missing shared ABI include")
    require(owner.index("if (EspAssetPack_isOpen()) EspAssetPack_close();") <
            owner.index("free(scratch);") <
            owner.index("if (!ok) return 0;") <
            owner.index(post), "teardown/post ordering changed")
    require("if (!repaintPlayerHud(angle, outStats))" in hook and
            "[PLAYERHUD] REFRESH-FAILED" in hook,
            "legacy HUD projection or error reporting removed")
    require(re.search(r"(?m)^\s*-Wl,--wrap=EspNativeGameplayHudDirection_render\s*$",
                      ini) is None, "retired linker interposition returned")
    require("__wrap_EspNativeGameplayHudDirection_render" not in overlay and
            "__real_EspNativeGameplayHudDirection_render" not in overlay,
            "retired linker owner or alias survives")
    run(harness(owner, hook, hud), True)

    owner_bad = [
        owner.replace("return " + post + "\n        angle, outStats);", "return 1;"),
        owner.replace("    free(scratch);\n", ""),
        owner.replace("    if (!ok) return 0;\n", ""),
        owner.replace("    if (EspAssetPack_isOpen()) EspAssetPack_close();\n", ""),
        owner.replace("    if (ok) *outStats = *stats;\n", ""),
        owner.replace("    if (outStats != NULL) memset(outStats, 0, sizeof(*outStats));\n", ""),
        owner.replace("c == '\\0' || EspAssetPack_isOpen()",
                      "false || EspAssetPack_isOpen()"),
        owner.replace("c == '\\0' || EspAssetPack_isOpen()",
                      "c == '\\0' || false"),
        owner.replace("    if (!ok) return 0;\n",
                      "    if (!ok) return 0;\n").replace(
                          "    free(scratch);\n",
                          "    free(scratch);\n    if (ok) (void)" + post +
                          "\n        angle, outStats);\n"),
    ]
    hook_bad = [
        hook.replace("if (!repaintPlayerHud(angle, outStats))",
                     "if (false && !repaintPlayerHud(angle, outStats))"),
        hook.replace("        return 0;\n", "        return 1;\n", 1),
        hook.replace("    return 1;\n", "    return 0;\n"),
    ]
    # Keep every negative fixture different from the extracted sources, even
    # when upstream formatting changes. Do not allow a silently inert mutant.
    for i, changed in enumerate(owner_bad, 1):
        require(changed != owner, f"inert owner negative fixture {i}")
        try:
            run(harness(changed, hook, hud), False)
        except AssertionError as error:
            raise AssertionError(f"owner negative fixture {i}: {error}") from error
    for i, changed in enumerate(hook_bad, 1):
        require(changed != hook, f"inert hook negative fixture {i}")
        try:
            run(harness(owner, changed, hud), False)
        except AssertionError as error:
            raise AssertionError(f"hook negative fixture {i}: {error}") from error
    print("PASS ARCH-RC5 real-source C++: 12 HUD direction parity/error "
          "cases and 12 adverse post/cleanup/guard mutations; no pixel golden")


if __name__ == "__main__":
    main()
