Import("env")

import os
from os.path import join


project_dir = env.subst("$PROJECT_DIR")
build_dir = env.subst("$BUILD_DIR")
engine_dir = join(project_dir, "..", "src")
patched_dir = join(build_dir, "doomrpg_patched_sources")

os.makedirs(patched_dir, exist_ok=True)

env.Append(CPPPATH=[join(project_dir, "include"), engine_dir])

# DoomCanvas keeps the original desktop minimum display height of 128 pixels.
# The CYD render target is deliberately 160x120, so generate an ESP32-only
# copy with the minimum tied to the canonical platform video geometry.  The
# source-tree file remains untouched for desktop builds.
doom_canvas_source = join(engine_dir, "DoomCanvas.c")
doom_canvas_patched = join(patched_dir, "DoomCanvas.c")

with open(doom_canvas_source, "r", encoding="utf-8") as source_file:
    doom_canvas = source_file.read()

include_needle = '#include "SDL_Video.h"\n'
include_replacement = (
    '#include "SDL_Video.h"\n'
    '#include "platform_video_config.h"\n'
)

height_needle = (
    '\tif (doomCanvas->displayRect.h < 0x80) {\n'
    '\t\tdoomCanvas->displayRect.h = 0x80;\n'
    '\t}\n'
)
height_replacement = (
    '#ifdef DOOMRPG_ESP32\n'
    '\tif (doomCanvas->displayRect.h < DOOMRPG_LOGICAL_HEIGHT) {\n'
    '\t\tdoomCanvas->displayRect.h = DOOMRPG_LOGICAL_HEIGHT;\n'
    '\t}\n'
    '#else\n'
    '\tif (doomCanvas->displayRect.h < 0x80) {\n'
    '\t\tdoomCanvas->displayRect.h = 0x80;\n'
    '\t}\n'
    '#endif\n'
)

if doom_canvas.count(include_needle) != 1:
    raise RuntimeError("Unable to locate SDL_Video.h include in DoomCanvas.c")
if doom_canvas.count(height_needle) != 1:
    raise RuntimeError(
        "Unable to locate the 128-pixel DoomCanvas minimum-height block; "
        "review the ESP32 patch before building"
    )

doom_canvas = doom_canvas.replace(include_needle, include_replacement, 1)
doom_canvas = doom_canvas.replace(height_needle, height_replacement, 1)

with open(doom_canvas_patched, "w", encoding="utf-8", newline="\n") as patched_file:
    patched_file.write(doom_canvas)

print("[ESP32] DoomCanvas generated with 160x120-aware minimum height")

# The desktop entry point and its SDL/audio/ZIP implementations are replaced by
# the small ESP32 compatibility layer in this PlatformIO project. DoomCanvas.c
# is compiled from the generated ESP32-safe copy above.
env.BuildSources(
    join(build_dir, "doomrpg_engine"),
    engine_dir,
    src_filter=[
        "+<*.c>",
        "-<Main.c>",
        "-<SDL_Video.c>",
        "-<Sound.c>",
        "-<Z_Zone.c>",
        "-<DoomCanvas.c>",
    ],
)

env.BuildSources(
    join(build_dir, "doomrpg_engine_doomcanvas"),
    patched_dir,
    src_filter=["+<DoomCanvas.c>"],
)
