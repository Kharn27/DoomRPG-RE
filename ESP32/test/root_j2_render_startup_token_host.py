#!/usr/bin/env python3
"""Root-J2: Render startup receives an explicit process token, not global doomRpg.

Git-blob exact inverse parity on three files and executable C probe of the
production EspRenderStartupBridge_start body (not a rewritten approximation).
"""
from pathlib import Path
import hashlib
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FILE_BLOBS = {
    "ESP32/include/esp_render_startup_bridge.h":
        "75416b3ca335c28435e04ca53988a853daaf1aa3",
    "ESP32/src/render_startup_bridge.c":
        "cf39d5ee4e1c9330f72eb555bc194cdee8f7f922",
    "ESP32/src/main.cpp":
        "2875f048bbde9a38c7782ddcb27632a4fb4c878a",
}
files = {path: (ROOT / path).read_text(encoding="utf-8")
         for path in FILE_BLOBS}

header = files["ESP32/include/esp_render_startup_bridge.h"]
source = files["ESP32/src/render_startup_bridge.c"]
boot = files["ESP32/src/main.cpp"]

assert "struct DoomRPG_s;" in header
assert "int EspRenderStartupBridge_start(struct DoomRPG_s* root, int preRenderReady);" in header
assert "int EspRenderStartupBridge_start(DoomRPG_t* root, int preRenderReady) {" in source
assert "extern DoomRPG_t* doomRpg;" not in source
assert "if (root == NULL || EspNativeRootOwners_live()->render == NULL ||" in source
assert "EspRenderStartupBridge_start(doomRpg, enginePreRenderReady ? 1 : 0) != 0;" in boot
assert boot.count("EspRenderStartupBridge_start(") == 1
assert source.count("EspRenderStartupBridge_start(") == 1

# Undo precisely the Root-J2 changes and prove untouched byte identity of
# the hardware-tested Root-J1 + SDL-F5 main as a Git blob.
changes = {
    "ESP32/include/esp_render_startup_bridge.h": [
        (" * engine layout / pre-render startup stage has succeeded. The caller passes\n"
         " * its process token explicitly; this owner never reads the global doomRpg.",
         " * engine layout / pre-render startup stage has succeeded."),
        ("struct DoomRPG_s;\n"
         "int EspRenderStartupBridge_start(struct DoomRPG_s* root, int preRenderReady);",
         "int EspRenderStartupBridge_start(int preRenderReady);"),
    ],
    "ESP32/src/render_startup_bridge.c": [
        ("#endif\n\n\n/* Render-shell compatibility slots",
         "#endif\n\n\nextern DoomRPG_t* doomRpg;\n\n/* Render-shell compatibility slots"),
        ("int EspRenderStartupBridge_start(DoomRPG_t* root, int preRenderReady) {",
         "int EspRenderStartupBridge_start(int preRenderReady) {"),
        ("if (root == NULL || EspNativeRootOwners_live()->render == NULL ||",
         "if (doomRpg == NULL || EspNativeRootOwners_live()->render == NULL ||"),
    ],
    "ESP32/src/main.cpp": [
        # AUDIO-A only adds two includes and two lifecycle/service calls.
        ('#include "esp_native_audio_midi.h"\n'
         '#include "esp_native_audio_volume.h"\n', ''),
        ("    EspNativeAudioVolume_loadAtBoot(sdReady ? 1 : 0);\n", ""),
        ("    EspNativeAudioMidi_service();\n", ""),
        # Root-J3 also changes the same boot TU; revert its independent call
        # first so Root-J2's original golden-blob parity remains exact.
        ("EspLegacyPrerenderStartup_start(doomRpg, engineLayoutReady ? 1 : 0) != 0;",
         "EspLegacyPrerenderStartup_start(engineLayoutReady ? 1 : 0) != 0;"),
        ("EspRenderStartupBridge_start(doomRpg, enginePreRenderReady ? 1 : 0) != 0;",
         "EspRenderStartupBridge_start(enginePreRenderReady ? 1 : 0) != 0;"),
    ],
}
for path, operations in changes.items():
    restored = files[path]
    for old, new in operations:
        assert restored.count(old) == 1, (path, old)
        restored = restored.replace(old, new, 1)
    binary = restored.encode("utf-8")
    digest = hashlib.sha1(
        b"blob " + str(len(binary)).encode() + b"\0" + binary
    ).hexdigest()
    assert digest == FILE_BLOBS[path], (path, digest, FILE_BLOBS[path])

# Extract the *production function*, including its actual fail-closed
# preflight, immutable shared framebuffer test and idempotent attempt latch.
start = source.index(
    "int EspRenderStartupBridge_start(DoomRPG_t* root, int preRenderReady) {"
)
brace = source.index("{", start)
depth, end = 0, None
for pos in range(brace, len(source)):
    if source[pos] == "{":
        depth += 1
    elif source[pos] == "}":
        depth -= 1
        if depth == 0:
            end = pos + 1
            break
assert end is not None
production = source[start:end]
assert "renderStartupAttempted = 1;" in production
assert "renderStartupReady = 1;" in production
assert "render = EspNativeRootOwners_live()->render;" in production
assert "EspNativeRenderStartup_initialize(render)" in production
assert "render->framebuffer != expectedFramebuffer" in production

probe = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t byte;
typedef struct DoomRPG_s { unsigned char nativeProcessIdentity; } DoomRPG_t;
typedef struct Render_s {
    uint8_t* framebuffer;
    int pitch;
    short* mediaPalettes;
    int mediaPalettesLength;
} Render_t;
typedef struct {
    Render_t* render;
    void* doomCanvas;
} EspNativeRootOwners_t;
typedef struct { int rendererW, rendererH; } SDLVideo_t;

static int renderStartupAttempted = 0;
static int renderStartupReady = 0;
static uint8_t framebuffer[38400], wrongFramebuffer[38400];
static Render_t render;
static short palette[2];
static SDLVideo_t sdlVideo = {160, 120};
static int canvasIdentity = 1;
static EspNativeRootOwners_t owners = {&render, &canvasIdentity};
static DoomRPG_t rootA = {0x5a}, rootB = {0xa5};
static int allowedPreflight = 1, allowedInitialize = 1;
static int badFramebuffer = 0, badPitch = 0, badPalette = 0, badPaletteLength = 0;
static int preflights = 0, initializes = 0;
static EspNativeRootOwners_t* EspNativeRootOwners_live(void) { return &owners; }
static uint32_t heap8Free(void) { return 100000U; }
static uint32_t largest8Block(void) { return 61428U; }
static int preflightRenderResources(void) {
    ++preflights;
    return allowedPreflight;
}
static void* Esp32PlatformVideo_framebuffer(void) { return framebuffer; }
static uint32_t Esp32PlatformVideo_framebufferSizeBytes(void) { return 38400U; }
static int EspNativeRenderStartup_initialize(Render_t* dst) {
    ++initializes;
    if (!allowedInitialize) return 0;
    dst->framebuffer = badFramebuffer ? wrongFramebuffer : framebuffer;
    dst->pitch = badPitch ? 318 : 320;
    dst->mediaPalettes = badPalette ? NULL : palette;
    dst->mediaPalettesLength = badPaletteLength ? 0 : 2;
    return 1;
}
__PRODUCTION_STARTUP__

int main(void) {
    for (int i=0; i<4096; ++i) {
        const int mode = i % 12;
        DoomRPG_t* token = (i & 1) ? &rootA : &rootB;
        renderStartupAttempted = 0;
        renderStartupReady = 0;
        memset(&render, 0, sizeof(render));
        owners.render = &render;
        owners.doomCanvas = &canvasIdentity;
        allowedPreflight = allowedInitialize = 1;
        badFramebuffer = badPitch = badPalette = badPaletteLength = 0;
        preflights = initializes = 0;
        sdlVideo.rendererW = 160;
        sdlVideo.rendererH = 120;
        int preRenderReady = 1;
        switch (mode) {
          case 2: token = NULL; break;
          case 3: preRenderReady = 0; break;
          case 4: owners.render = NULL; break;
          case 5: owners.doomCanvas = NULL; break;
          case 6: allowedPreflight = 0; break;
          case 7: allowedInitialize = 0; break;
          case 8: badFramebuffer = 1; break;
          case 9: badPitch = 1; break;
          case 10: badPalette = 1; break;
          case 11: badPaletteLength = 1; break;
        }
        const int expected = mode <= 1;
        const int result = EspRenderStartupBridge_start(token, preRenderReady);
        assert(result == expected);
        assert(renderStartupAttempted == 1);
        assert(renderStartupReady == expected);
        assert(preflights == (mode != 2 && mode != 3 && mode != 4 && mode != 5));
        assert(initializes == (mode == 0 || mode == 1 || mode >= 7));
        // Successful or failed result must be cached exactly, irrespective
        // of subsequent token loss. No duplicate PAK loads or allocations.
        assert(EspRenderStartupBridge_start(NULL, 0) == expected);
        assert(preflights <= 1 && initializes <= 1);
    }
    puts("PASS Root-J2 production Render startup: 4096 cycles, two roots,");
    puts("     10 refusal/failure modes, process token NULL, single attempt");
    return 0;
}
'''.replace("__PRODUCTION_STARTUP__", production)

with tempfile.TemporaryDirectory(prefix="root-j2-render-") as tmp:
    src = Path(tmp) / "root_j2_render.c"
    executable = Path(tmp) / "root_j2_render"
    src.write_text(probe, encoding="utf-8")
    subprocess.run(
        ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
         str(src), "-o", str(executable)],
        capture_output=True, text=True, check=True
    )
    output = subprocess.run(
        [str(executable)], check=True, capture_output=True, text=True
    ).stdout
    assert "PASS Root-J2 production Render startup" in output

print("Root-J2 PASS: 3 files exact blob inverse parity;")
print("  production Render startup 4096 cycles, token explicit, fail closed")
