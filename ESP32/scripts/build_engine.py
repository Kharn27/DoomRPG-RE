Import("env")

import os
from os.path import join


project_dir = env.subst("$PROJECT_DIR")
build_dir = env.subst("$BUILD_DIR")
project_src_dir = join(project_dir, "src")
engine_dir = join(project_dir, "..", "src")
patched_dir = join(build_dir, "doomrpg_patched_sources")

os.makedirs(patched_dir, exist_ok=True)

env.Append(CPPPATH=[join(project_dir, "include"), project_src_dir, engine_dir])

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

# The source-tree SDL shim stores every texture as RGB565. That is acceptable
# for small diagnostic textures but impossible for original Doom RPG assets
# such as g.bmp (128x512): a single RGB565 copy would require 131072 contiguous
# bytes, more than the classic CYD can provide. Generate an ESP32-only shim that
# lets textures created from indexed BMP surfaces adopt the surface's 8-bpp
# pixel buffer and palette instead of allocating/converting a second RGB565
# copy. Conversion to RGB565 then happens per sampled pixel in SDL_RenderCopy().
esp32_sdl_source = join(project_src_dir, "esp32_sdl.cpp")
esp32_sdl_patched = join(patched_dir, "esp32_sdl.cpp")

with open(esp32_sdl_source, "r", encoding="utf-8") as source_file:
    esp32_sdl = source_file.read()

texture_struct_needle = '''struct SDL_Texture {
    int width;
    int height;
    int pitch;
    Uint16* pixels;
    Uint8 red;
    Uint8 green;
    Uint8 blue;
    bool hasColorKey;
    Uint16 colorKey;
};
'''
texture_struct_replacement = '''struct SDL_Texture {
    int width;
    int height;
    int pitch;
    Uint16* pixels;
    Uint8* indexedPixels;
    SDL_Color* palette;
    int paletteSize;
    bool indexed;
    Uint8 red;
    Uint8 green;
    Uint8 blue;
    bool hasColorKey;
    Uint16 colorKey;
};
'''

create_from_surface_needle = '''SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer* renderer, SDL_Surface* surface) {
    if (surface == nullptr || surface->format == nullptr || surface->format->palette == nullptr) {
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             surface->w, surface->h);
    if (texture == nullptr) return nullptr;
    texture->hasColorKey = surface->hasColorKey;
    const Uint8* source = static_cast<const Uint8*>(surface->pixels);
    for (int y = 0; y < surface->h; ++y) {
        for (int x = 0; x < surface->w; ++x) {
            const SDL_Color& color = surface->format->palette->colors[source[y * surface->pitch + x]];
            texture->pixels[y * surface->w + x] = rgb565(color.r, color.g, color.b);
        }
    }
    if (texture->hasColorKey) {
        texture->colorKey = rgb565((surface->colorKey >> 16) & 0xff,
                                   (surface->colorKey >> 8) & 0xff,
                                   surface->colorKey & 0xff);
    }
    return texture;
}
'''
create_from_surface_replacement = '''SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer*, SDL_Surface* surface) {
    if (surface == nullptr || surface->format == nullptr ||
        surface->format->palette == nullptr ||
        surface->format->palette->colors == nullptr || surface->pixels == nullptr ||
        surface->format->BitsPerPixel != 8 || surface->format->BytesPerPixel != 1) {
        setError("unsupported indexed surface");
        return nullptr;
    }

    SDL_Texture* texture = static_cast<SDL_Texture*>(calloc(1, sizeof(SDL_Texture)));
    if (texture == nullptr) {
        setError("out of memory creating indexed texture");
        return nullptr;
    }

    texture->width = surface->w;
    texture->height = surface->h;
    texture->pitch = surface->pitch;
    texture->indexedPixels = static_cast<Uint8*>(surface->pixels);
    texture->palette = surface->format->palette->colors;
    texture->paletteSize = surface->format->palette->ncolors;
    texture->indexed = true;
    texture->red = texture->green = texture->blue = 255;
    texture->hasColorKey = surface->hasColorKey;
    if (texture->hasColorKey) {
        texture->colorKey = rgb565((surface->colorKey >> 16) & 0xff,
                                   (surface->colorKey >> 8) & 0xff,
                                   surface->colorKey & 0xff);
    }

    // Transfer ownership to the texture. DoomRPG_createImage() immediately
    // frees the SDL_Surface after this call, so detaching avoids a second
    // width*height allocation and makes the transfer genuinely zero-copy.
    surface->pixels = nullptr;
    surface->format->palette->colors = nullptr;

    Serial.printf("[SDL] Adopt indexed texture %dx%d pixels=%u palette=%d\\n",
                  texture->width, texture->height,
                  static_cast<unsigned int>(texture->pitch * texture->height),
                  texture->paletteSize);
    return texture;
}
'''

destroy_texture_needle = '''void SDL_DestroyTexture(SDL_Texture* texture) {
    if (texture == nullptr) return;
    free(texture->pixels);
    free(texture);
}
'''
destroy_texture_replacement = '''void SDL_DestroyTexture(SDL_Texture* texture) {
    if (texture == nullptr) return;
    free(texture->pixels);
    free(texture->indexedPixels);
    free(texture->palette);
    free(texture);
}
'''

update_texture_needle = '''int SDL_UpdateTexture(SDL_Texture* texture, const SDL_Rect* rect,
                      const void* pixels, int pitch) {
    if (texture == nullptr || pixels == nullptr) return -1;
    SDL_Rect area = rect == nullptr ? SDL_Rect{0, 0, texture->width, texture->height} : *rect;
'''
update_texture_replacement = '''int SDL_UpdateTexture(SDL_Texture* texture, const SDL_Rect* rect,
                      const void* pixels, int pitch) {
    if (texture == nullptr || pixels == nullptr || texture->indexed) return -1;
    SDL_Rect area = rect == nullptr ? SDL_Rect{0, 0, texture->width, texture->height} : *rect;
'''

render_sample_needle = '''            const Uint16 color = texture->pixels[sourceY * texture->width + sourceX];
            if (texture->hasColorKey && color == texture->colorKey) continue;
            rendererPixels[y * kLogicalWidth + x] = modulate565(color, texture);
'''
render_sample_replacement = '''            Uint16 color;
            if (texture->indexed) {
                const Uint8 paletteIndex =
                    texture->indexedPixels[sourceY * texture->pitch + sourceX];
                if (paletteIndex >= texture->paletteSize) continue;
                const SDL_Color& paletteColor = texture->palette[paletteIndex];
                color = rgb565(paletteColor.r, paletteColor.g, paletteColor.b);
            }
            else {
                color = texture->pixels[sourceY * texture->width + sourceX];
            }
            if (texture->hasColorKey && color == texture->colorKey) continue;
            rendererPixels[y * kLogicalWidth + x] = modulate565(color, texture);
'''

patches = [
    (texture_struct_needle, texture_struct_replacement, "SDL_Texture layout"),
    (create_from_surface_needle, create_from_surface_replacement,
     "SDL_CreateTextureFromSurface"),
    (destroy_texture_needle, destroy_texture_replacement, "SDL_DestroyTexture"),
    (update_texture_needle, update_texture_replacement, "SDL_UpdateTexture"),
    (render_sample_needle, render_sample_replacement, "SDL_RenderCopy sample"),
]

for needle, replacement, label in patches:
    if esp32_sdl.count(needle) != 1:
        raise RuntimeError(
            f"Unable to locate {label} in esp32_sdl.cpp; review the indexed "
            "texture patch before building"
        )
    esp32_sdl = esp32_sdl.replace(needle, replacement, 1)

with open(esp32_sdl_patched, "w", encoding="utf-8", newline="\n") as patched_file:
    patched_file.write(esp32_sdl)

print("[ESP32] SDL shim generated with zero-copy indexed BMP textures")

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

env.BuildSources(
    join(build_dir, "doomrpg_esp32_sdl_patched"),
    patched_dir,
    src_filter=["+<esp32_sdl.cpp>"],
)
