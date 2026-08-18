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
# copy with the minimum tied to the canonical platform video geometry. The
# source-tree file remains untouched for desktop builds.
#
# DoomCanvas.c is a legacy source file and contains non-UTF-8 bytes in comments
# (for example 0xF3). Latin-1 is used intentionally here because it maps every
# byte 1:1, so we can safely patch the ASCII code fragments without corrupting
# the rest of the original source text.
doom_canvas_source = join(engine_dir, "DoomCanvas.c")
doom_canvas_patched = join(patched_dir, "DoomCanvas.c")

with open(doom_canvas_source, "r", encoding="latin-1") as source_file:
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

with open(doom_canvas_patched, "w", encoding="latin-1", newline="\n") as patched_file:
    patched_file.write(doom_canvas)

print("[ESP32] DoomCanvas generated with 160x120-aware minimum height")

# DoomRPG_createImage() is the central image-loading path used by the game.
# Desktop SDL handles the original indexed BMP variants, while the deliberately
# small ESP32 SDL shim initially handled only 8-bpp BMPs. Generate an ESP32-only
# DoomRPG.c copy that routes those image loads through Esp32Bmp_LoadRW(), which
# expands uncompressed 1/4/8-bpp indexed BMP rows into the same 8-bpp indexed
# SDL_Surface representation expected by the rest of the engine.
doom_rpg_source = join(engine_dir, "DoomRPG.c")
doom_rpg_patched = join(patched_dir, "DoomRPG.c")

with open(doom_rpg_source, "r", encoding="latin-1") as source_file:
    doom_rpg_source_text = source_file.read()

zip_include_needle = '#include "Z_Zip.h"\n'
zip_include_replacement = (
    '#include "Z_Zip.h"\n'
    '#include "esp32_bmp.h"\n'
)
bmp_call_needle = "SDL_LoadBMP_RW("
bmp_call_count = doom_rpg_source_text.count(bmp_call_needle)

if doom_rpg_source_text.count(zip_include_needle) != 1:
    raise RuntimeError("Unable to locate Z_Zip.h include in DoomRPG.c")
if bmp_call_count == 0:
    raise RuntimeError(
        "Unable to locate SDL_LoadBMP_RW calls in DoomRPG.c; "
        "review the ESP32 image-loader patch before building"
    )

doom_rpg_source_text = doom_rpg_source_text.replace(
    zip_include_needle, zip_include_replacement, 1
)
doom_rpg_source_text = doom_rpg_source_text.replace(
    bmp_call_needle, "Esp32Bmp_LoadRW("
)

with open(doom_rpg_patched, "w", encoding="latin-1", newline="\n") as patched_file:
    patched_file.write(doom_rpg_source_text)

print(
    "[ESP32] DoomRPG generated with indexed BMP loader "
    f"({bmp_call_count} SDL_LoadBMP_RW call(s) redirected)"
)

# The desktop entry point and its SDL/audio/ZIP implementations are replaced by
# the small ESP32 compatibility layer in this PlatformIO project. DoomCanvas.c
# and DoomRPG.c are compiled from generated ESP32-safe copies above.
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
        "-<DoomRPG.c>",
    ],
)

env.BuildSources(
    join(build_dir, "doomrpg_engine_patched"),
    patched_dir,
    src_filter=[
        "+<DoomCanvas.c>",
        "+<DoomRPG.c>",
    ],
)
