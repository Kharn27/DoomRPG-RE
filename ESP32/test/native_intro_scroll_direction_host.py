#!/usr/bin/env python3
"""Exercise the real CYD intro starfield scroll helper as host C.

The Mars scene must scroll the opaque starfield AND its transparent line layer
rightward like both text story pages, without changing the ship trajectory.
The opaque layer must cover the 128-wide virtual view for every scroll phase.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / "ESP32/src/native_story_fit.c").read_text(encoding="utf-8")

start = source.index("static void drawRightwardAnimationBackground(")
end = source.index("\nstatic void drawTextGlyph(", start)
helper = source[start:end]

scene_start = source.index("const int bgOffset = elapsedAnim / 457;")
scene = source[scene_start:]
assert "scrollSpaceBG(doomCanvas, root, introState);" in source
assert "const int i = -((doomCanvas->time / 157) % 192);" in source
assert "left - i2" in source and "left - i3" in source
assert """drawRightwardAnimationBackground(doomCanvas,
                                          &introState->imgSpaceBG,
                                          left,
                                          top,
                                          bgOffset);""" in scene
assert """drawAnimationImage(doomCanvas,
                           &introState->imgLinesLayer,
                           left + linesOffset,""" in scene
assert "left - linesOffset" not in scene and "left - bgOffset" not in scene
assert "const int shipX = left + (elapsedAnim / 142);" in scene
assert "const int shipY = (DOOMRPG_CANVAS_CENTER_Y + 22) + (elapsedAnim / -333);" in scene
assert "const int linesOffset = elapsedAnim / 157;" in scene

prefix = r"""
#include <assert.h>
#include <stddef.h>
#define ESP32_STORY_VIRTUAL_SIZE 128
typedef struct { void* imgBitmap; int width; } Image_t;
typedef struct { int ignored; } DoomCanvas_t;
static struct { int x, y; } calls[140];
static int count;
static void drawBackgroundImage(DoomCanvas_t* c, Image_t* img, int x, int y) {
    (void)c;
    assert(img != NULL);
    assert(count < 140);
    calls[count].x = x;
    calls[count].y = y;
    ++count;
}
"""
suffix = r"""
int main(void) {
    DoomCanvas_t canvas = {0};
    Image_t image = {(void*)1, 192};
    const int widths[] = {64, 128, 192, 256};
    const int elapsed[] = {0, 156, 157, 456, 457, 9999, 10000};
    for (unsigned int w = 0; w < sizeof(widths)/sizeof(widths[0]); ++w) {
        image.width = widths[w];
        for (unsigned int t = 0; t < sizeof(elapsed)/sizeof(elapsed[0]); ++t) {
            const int phase = (elapsed[t] / 457) % image.width;
            count = 0;
            drawRightwardAnimationBackground(&canvas, &image, 0, 4,
                                             elapsed[t] / 457);
            assert(count >= 2);
            assert(calls[0].x == phase - image.width);
            for (int i = 0; i < count; ++i) {
                assert(calls[i].y == 4);
                if (i > 0) assert(calls[i].x - calls[i - 1].x == image.width);
            }
            for (int pixel = 0; pixel < 128; ++pixel) {
                int covered = 0;
                for (int i = 0; i < count; ++i) {
                    if (pixel >= calls[i].x && pixel < calls[i].x + image.width)
                        covered = 1;
                }
                assert(covered && "scrolling background exposed a gap");
            }
        }
    }
    image.width = 192;
    count = 0;
    drawRightwardAnimationBackground(&canvas, &image, 0, 0, 0);
    assert(calls[count - 1].x == 0);
    count = 0;
    drawRightwardAnimationBackground(&canvas, &image, 0, 0, 1);
    assert(calls[count - 1].x == 1 && "background must move RIGHT");
    image.width = 0;
    count = 0;
    drawRightwardAnimationBackground(&canvas, &image, 0, 0, 123);
    assert(count == 0);
    return 0;
}
"""

with tempfile.TemporaryDirectory(prefix="doom-intro-scroll-") as folder:
    c_path = Path(folder) / "intro_scroll.c"
    exe_path = Path(folder) / "intro_scroll"
    c_path.write_text(prefix + helper + suffix, encoding="utf-8")
    subprocess.run(
        ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
         str(c_path), "-o", str(exe_path)],
        check=True,
    )
    subprocess.run([str(exe_path)], check=True)

print("INTRO-SCROLL PASS real C helper: rightward stars + lines; "
      "seamless 128-wide cover; original ship/text pacing retained")
