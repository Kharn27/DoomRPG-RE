#!/usr/bin/env python3
"""RES60-P3: real immutable BSP line projection, never resized 160x80 pixels."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
config = (ROOT / "ESP32/include/platform_video_config.h").read_text()
ini = (ROOT / "ESP32/platformio.ini").read_text()
main = (ROOT / "ESP32/src/main.cpp").read_text()
video = (ROOT / "ESP32/src/platform_video.cpp").read_text()
probe = (ROOT / "ESP32/src/esp_res60_world_probe.c").read_text()
projection = (ROOT / "ESP32/src/esp_res60_world_preview.c").read_text()

assert '#error "RES60-P3 requires isolated P1/P2 video-only framebuffer and layout"' in config
assert "-D DOOMRPG_RES60_P3_WORLD=1" in ini
assert "default_envs = esp32-cyd" in ini
assert "#define DOOMRPG_LOGICAL_WIDTH 160" in config
assert "#define DOOMRPG_LOGICAL_HEIGHT 120" in config
assert '#error "Classic CYD gameplay viewport must remain 160x80"' in config

setup = main[main.index("void setup()"):main.index("void loop()")]
p3 = setup.split("#if DOOMRPG_RES60_P3_WORLD", 1)[1].split(
    "#elif DOOMRPG_RES60_P2_LAYOUT", 1)[0]
assert "initializeSdCard();" in p3
assert "if (!sdReady)" in p3
assert "PlatformVideo_showRes60WorldPreview();" in p3
assert "EspEntranceMusic_" not in p3 and "initializeGameAssets" not in p3
assert "EspBspReader_inventoryPackEntry" in probe
assert "EspMapRuntime_loadPackEntry" in probe
assert "EspMapRuntime_getLine" in probe
assert "EspMapRuntime_reset();" in probe
assert "viewX -= 16" in probe and "viewY += 16" in probe
assert "viewX += 16" in probe and "viewY -= 16" in probe
assert "camera.worldX = (uint16_t)viewX;" in probe
assert "ESP_ASSET_PACK_DEFAULT_PATH" not in main[main.index("void setup()"):main.index("void loop()")]
assert "source=MAP_INTRO" in video
assert "textureSamples=0 sprites=0 engine=NOT_STARTED" in video
assert "framebuffer[((size_t)ESP_RES60_WORLD_Y + row) * ESP_RES60_WIDTH +" in projection
assert "EspRes60Layout_fnv(framebuffer, framebufferBytes)" in projection
assert "uint16_t outputRow[DOOMRPG_PHYSICAL_WIDTH];" in video

harness = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "platform_video_config.h"
#include "esp_res60_world_preview.h"
#include "esp_video_nearest.h"

_Static_assert(ESP_RES60_WIDTH == 192U, "192 genuinely rendered columns");
_Static_assert(ESP_RES60_WORLD_ROWS == 96U, "96 genuinely rendered world rows");
_Static_assert(ESP_RES60_FRAMEBUFFER_BYTES == 55296U, "one RGB565 framebuffer");
_Static_assert(DOOMRPG_LOGICAL_WIDTH == 160, "normal gameplay stays 160");
_Static_assert(DOOMRPG_VIEWPORT_HEIGHT == 80, "normal 3D stays 80");
#if DOOMRPG_RES60_P3_WORLD
_Static_assert(DOOMRPG_RES60_P2_LAYOUT && DOOMRPG_RES60_P1_DIAGNOSTIC,
               "P3 only in opt-in video-only build");
#endif

static const EspRes60PreviewLine square[] = {
    {192,32,192,224, 10,0}, {64,32,64,224, 20,0},
    {64,224,192,224, 30,0}, {64,32,192,32, 40,0}
};
/* A symmetric square alone cannot catch a cardinal camera reversal. */
static const EspRes60PreviewLine facingWalls[] = {
    {192,64,192,192,100,0}, /* angle 0:   +X */
    {64,64,192,64,101,0},   /* angle 64:  -Y (Doom's actual rotation) */
    {64,64,64,192,102,0},   /* angle 128: -X */
    {64,192,192,192,103,0}  /* angle 192: +Y */
};
static const EspRes60PreviewLine* activeLines = square;
static uint32_t activeCount = 4U;
static int badAt = -1;
static int reader(uint32_t index, EspRes60PreviewLine* out, void* ctx) {
    (void)ctx;
    if (index >= activeCount || out == NULL) return 0;
    if ((int)index == badAt) return 0;
    *out = activeLines[index];
    return 1;
}
int main(void) {
    uint16_t data[ESP_RES60_WIDTH * ESP_RES60_HEIGHT + 2U] = {0};
    uint16_t* frame = data + 1U;
    uint16_t before[ESP_RES60_WIDTH * ESP_RES60_HEIGHT] = {0};
    const size_t bytes = ESP_RES60_FRAMEBUFFER_BYTES;
    EspRes60PreviewCamera cam = {128U,128U,0U,36U,0x001fU,0x07e0U};
    EspRes60PreviewStats stats;
    data[0] = 0xfaceU;
    data[ESP_RES60_WIDTH * ESP_RES60_HEIGHT + 1U] = 0xbeefU;
    for (unsigned i=0;i<ESP_RES60_WIDTH*ESP_RES60_HEIGHT;++i) frame[i]=0xdeadU;
    memcpy(before,frame,bytes);
    assert(!EspRes60Preview_render(NULL,bytes,&cam,4U,reader,NULL,&stats));
    assert(!EspRes60Preview_render(frame,bytes-2U,&cam,4U,reader,NULL,&stats));
    assert(!EspRes60Preview_render(frame,bytes,&cam,0U,reader,NULL,&stats));
    assert(!EspRes60Preview_render(frame,bytes,&cam,
                                  ESP_RES60_PREVIEW_MAX_LINES+1U,reader,NULL,&stats));
    cam.angle = 33U;
    assert(!EspRes60Preview_render(frame,bytes,&cam,4U,reader,NULL,&stats));
    cam.angle = 0U;
    cam.viewZ = 37U;
    assert(!EspRes60Preview_render(frame,bytes,&cam,4U,reader,NULL,&stats));
    cam.viewZ = 36U;
    badAt = 2;
    assert(!EspRes60Preview_render(frame,bytes,&cam,4U,reader,NULL,&stats));
    badAt = -1;
    assert(memcmp(frame,before,bytes)==0); /* all preflight rejects touch no pixels */
    assert(EspRes60Preview_render(frame,bytes,&cam,4U,reader,NULL,&stats));
    assert(data[0] == 0xfaceU);
    assert(data[ESP_RES60_WIDTH*ESP_RES60_HEIGHT+1U] == 0xbeefU);
    assert(stats.preflightLines == 4U && stats.rayTests == 192U*4U);
    assert(stats.readerCalls == 4U + 192U*4U);
    assert(stats.visibleColumns == 192U);
    assert(stats.intersections >= 192U);
    assert(stats.frameFNV != 0U &&
           stats.frameFNV == EspRes60Layout_fnv(frame,bytes));
    assert(frame[23U*192U+0U] == 0xffffU);
    assert(frame[120U*192U+0U] == 0xffffU);
    assert(frame[24U*192U+96U] == cam.ceilingRgb565);
    assert(frame[72U*192U+96U] != cam.ceilingRgb565);
    assert(frame[72U*192U+96U] != cam.floorRgb565);
    assert(frame[0U] == 0xf800U);
    assert(frame[143U*192U] == 0xffe0U);

    uint32_t firstFNV=stats.frameFNV;
    for (uint8_t angle=0U; angle<4U; ++angle) {
        cam.angle = (uint8_t)(angle*64U);
        assert(EspRes60Preview_render(frame,bytes,&cam,4U,reader,NULL,&stats));
        assert(stats.visibleColumns == 192U && stats.rayTests == 768U);
    }
    cam.angle = 0U;
    assert(EspRes60Preview_render(frame,bytes,&cam,4U,reader,NULL,&stats));
    assert(stats.frameFNV == firstFNV);

    /* Four deliberately ASYMMETRIC single-wall camera tests: the wall
     * must be visible in the native forward direction but invisible when
     * the camera turns 180 degrees. These fail with the old +Y/-Y bug. */
    for (uint32_t cardinal = 0U; cardinal < 4U; ++cardinal) {
        activeLines = &facingWalls[cardinal];
        activeCount = 1U;
        cam.angle = (uint8_t)(cardinal * 64U);
        assert(EspRes60Preview_render(frame, bytes, &cam, 1U,
                                      reader, NULL, &stats));
        assert(stats.visibleColumns == 192U);
        assert(stats.intersections == 192U);
        assert(stats.rayTests == 192U);
        assert(stats.maxDepth == 64U);
        /* The same actual original segment behind the camera is invisible. */
        cam.angle = (uint8_t)((cardinal * 64U + 128U) & 255U);
        assert(!EspRes60Preview_render(frame, bytes, &cam, 1U,
                                       reader, NULL, &stats));
        assert(stats.visibleColumns == 0U);
    }
    activeLines = square;
    activeCount = 4U;
    assert(EspVideoNearest_source(319U,192U,320U)==191U);
    puts("RES60-P3 C11 PASS original-line geometry, 192x96, no staging, guards");
    return 0;
}
"""

with tempfile.TemporaryDirectory(prefix="res60-p3-") as temp:
    path = Path(temp) / "preview.c"
    binary = Path(temp) / "preview"
    path.write_text(harness)
    # Production builds must not even emit P3 world code.
    object_file = Path(temp) / "preview.o"
    subprocess.run([
        "cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-c",
        "-DDOOMRPG_RES60_P1_DIAGNOSTIC=0",
        "-DDOOMRPG_RES60_P2_LAYOUT=0",
        "-DDOOMRPG_RES60_P3_WORLD=0",
        "-I", str(ROOT / "ESP32/include"),
        str(ROOT / "ESP32/src/esp_res60_world_preview.c"),
        "-o", str(object_file)], check=True)
    symbols = subprocess.check_output(["nm", "-g", str(object_file)], text=True)
    assert "EspRes60Preview_render" not in symbols

    # Only the strict video-only P3 profile links/executes the real projector.
    subprocess.run([
        "cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-DDOOMRPG_RES60_P1_DIAGNOSTIC=1",
        "-DDOOMRPG_RES60_P2_LAYOUT=1",
        "-DDOOMRPG_RES60_P3_WORLD=1",
        "-I", str(ROOT / "ESP32/include"),
        str(ROOT / "ESP32/src/esp_res60_world_preview.c"),
        str(path), "-o", str(binary)],check=True)
    subprocess.run([str(binary)],check=True)

    # An accidental 60%-gameplay admission must fail at preprocess time.
    for p1,p2,p3 in ((0,0,1),(1,0,1),(0,1,1)):
        proc=subprocess.run([
            "cc","-std=c11",
            f"-DDOOMRPG_RES60_P1_DIAGNOSTIC={p1}",
            f"-DDOOMRPG_RES60_P2_LAYOUT={p2}",
            f"-DDOOMRPG_RES60_P3_WORLD={p3}",
            "-I",str(ROOT/"ESP32/include"),str(path),"-o",str(binary)],
            capture_output=True,text=True)
        assert proc.returncode != 0
        assert "requires isolated" in proc.stderr

print("RES60-P3 HOST PASS: genuine map-line projector and production isolation")
