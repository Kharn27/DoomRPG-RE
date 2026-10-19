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

# The desktop DoomCanvas.c translation unit is fully retired from the ESP32
# build. Its hardware-proven compatibility surface is implemented permanently
# by ESP32/src/esp_legacy_doomcanvas_bridge.c. Keeping the bridge in project
# sources means any future DoomCanvas_* dependency outside this explicit ABI
# fails at link time instead of silently reviving desktop state-machine code.
doom_canvas_bridge_source = join(project_src_dir, "esp_legacy_doomcanvas_bridge.c")
doom_canvas_bridge_exports = (
    "DoomCanvas_free",
    "DoomCanvas_setState",
    "DoomCanvas_startup",
    "DoomCanvas_init",
)
if not os.path.isfile(doom_canvas_bridge_source):
    raise RuntimeError("Missing permanent ESP32 DoomCanvas compatibility bridge")
with open(doom_canvas_bridge_source, "r", encoding="utf-8") as source_file:
    doom_canvas_bridge_text = source_file.read()
for export_name in doom_canvas_bridge_exports:
    if export_name + "(" not in doom_canvas_bridge_text:
        raise RuntimeError(
            "Missing DoomCanvas bridge export " + export_name +
            "; review the explicit ESP32 compatibility ABI"
        )
if ("static void EspDoomCanvas_drawImageSpecial(" not in doom_canvas_bridge_text or
        "void DoomCanvas_drawImageSpecial(" in doom_canvas_bridge_text):
    raise RuntimeError("DoomCanvas glyph blitter must be private, not public desktop ABI")
if ("void EspNativeCanvas_drawText(" not in doom_canvas_bridge_text or
        "const char* text," not in doom_canvas_bridge_text or
        "void DoomCanvas_drawString1(" in doom_canvas_bridge_text):
    raise RuntimeError("Legacy Canvas text ABI revived or native const-text owner missing")
if "DoomCanvas_run(" in doom_canvas_bridge_text or "DoomCanvas_loadMap(" in doom_canvas_bridge_text:
    raise RuntimeError(
        "Desktop DoomCanvas state-machine/map-loading ownership leaked into the ESP32 bridge"
    )
print(
    "[ESP32] Desktop DoomCanvas.c retired; "
    f"esp_legacy_doomcanvas_bridge.c owns {len(doom_canvas_bridge_exports)} source ABI exports; nativeText=1"
)

# Render.c is no longer generated or compiled in any CYD profile. The
# permanent esp_render_geometry_primitives.c owns the seven linked geometry
# primitives for normal AND bringup. Startup/palette/mapping/teardown owners
# remain unchanged; retired desktop BSP/shape/texel functions are not admitted.
# Desktop reference parity is checked by host tests, not build-time rewriting.
print("[ESP32] Desktop Render.c retired; geometry=esp-render-geometry-primitives "
      "profiles=normal+trace+bringup generated=no")

# No Game.c provider, generated or native bridge, is compiled on ESP32.
# Game_t is not allocated, the compatibility root pointer stays NULL, and
# Game_* calls are intentionally unavailable at link time.
print("[ESP32] Desktop Game.c retired; bridge=absent Game_init=retired "
      "gameObject=NULL originalC=uncompiled")

# The original DoomRPG.c is a behavioral reference, never a firmware or
# intermediate build input. Its linked functions already have native owners.
# Reference-body/CRC guards now live in host tests, not in an orphaned
# generated C translation unit.
# Remove an obsolete copy left by an earlier PlatformIO build so inspection
# cannot mistake it for an active input. Only the SDL shim is generated below.
retired_oracle = join(patched_dir, "DoomRPG.c")
if os.path.isfile(retired_oracle):
    os.remove(retired_oracle)
print("[ESP32] DoomRPG C oracle generation retired; reference=src/DoomRPG.c "
      "compiledPatchedC=0 nativeProviders=yes")

# The source-tree SDL shim stores every texture as RGB565. That is acceptable
# for small diagnostic textures but impossible for original Doom RPG assets.
# Generate an ESP32-only shim whose BMP-derived textures adopt packed indexed
# surface pixels and palettes directly. 1/4/8-bpp palette indexes are decoded
# only when SDL_RenderCopy() samples a pixel, then converted to RGB565 straight
# into the shared 160x120 framebuffer.
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
    Uint8 indexedBitsPerPixel;
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
        surface->format->palette->colors == nullptr || surface->pixels == nullptr) {
        setError("unsupported indexed surface");
        return nullptr;
    }

    const Uint8 bitsPerPixel = surface->format->BitsPerPixel;
    if (bitsPerPixel != 1 && bitsPerPixel != 4 && bitsPerPixel != 8) {
        setError("unsupported indexed surface depth");
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
    texture->indexedBitsPerPixel = bitsPerPixel;
    texture->indexed = true;
    texture->red = texture->green = texture->blue = 255;
    texture->hasColorKey = surface->hasColorKey;
    if (texture->hasColorKey) {
        texture->colorKey = rgb565((surface->colorKey >> 16) & 0xff,
                                   (surface->colorKey >> 8) & 0xff,
                                   surface->colorKey & 0xff);
    }

    // Transfer ownership to the texture. DoomRPG_createImage() immediately
    // frees the SDL_Surface after this call, so detaching keeps this zero-copy.
    surface->pixels = nullptr;
    surface->format->palette->colors = nullptr;

    Serial.printf("[SDL] Adopt packed indexed texture %dx%d bpp=%u bytes=%u palette=%d\\n",
                  texture->width, texture->height,
                  texture->indexedBitsPerPixel,
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
                const Uint8* indexedRow =
                    texture->indexedPixels + sourceY * texture->pitch;
                Uint8 paletteIndex;
                if (texture->indexedBitsPerPixel == 8) {
                    paletteIndex = indexedRow[sourceX];
                }
                else if (texture->indexedBitsPerPixel == 4) {
                    const Uint8 packed = indexedRow[sourceX >> 1];
                    paletteIndex = (sourceX & 1) ? (packed & 0x0f) : (packed >> 4);
                }
                else {
                    paletteIndex =
                        (indexedRow[sourceX >> 3] >> (7 - (sourceX & 7))) & 0x01;
                }
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

print("[ESP32] SDL shim generated with packed zero-copy indexed BMP textures")

# The original desktop C is only a test reference: all firmware providers live
# in ESP32/src. The one generated source is the native packed SDL shim.
# The desktop original C source tree is no longer registered with SCons.
# All 21 original TUs were already excluded by the old '+<*.c>' source
# filter, so that registration compiled exactly zero objects. Make the
# boundary permanent instead of maintaining a fragile exclusion checklist:
# native sources live in ESP32/src; no desktop C copy is registered by SCons.
# No DoomRPG.c copy is generated. Host tests compare the original desktop
# source directly against native owners with pinned CRCs and behavior fixtures.
#
# An unexpected original source addition requires an explicit build ownership
# review; it must not silently be picked up by a future glob. Desktop and
# original reference code outside ESP32 are not modified by this guard.
retired_desktop_units = {
    "Combat.c",
    "CombatEntity.c",
    "DoomCanvas.c",
    "DoomRPG.c",
    "Entity.c",
    "EntityDef.c",
    "EntityMonster.c",
    "Game.c",
    "Hud.c",
    "Main.c",
    "Menu.c",
    "MenuItem.c",
    "MenuSystem.c",
    "ParticleSystem.c",
    "Player.c",
    "Render.c",
    "SDL_Video.c",
    "Sound.c",
    "Weapon.c",
    "Z_Zip.c",
    "Z_Zone.c",
}
present_desktop_units = {
    name for name in os.listdir(engine_dir) if name.endswith(".c")
}
if present_desktop_units != retired_desktop_units:
    raise RuntimeError(
        "Desktop original TU inventory changed; explicitly audit ESP32 "
        "native-vs-patched ownership before updating build_engine.py: "
        + repr(sorted(present_desktop_units ^ retired_desktop_units))
    )
print("[ESP32] Desktop original source registration retired; "
      "originalC=%u compiledOriginal=0 compiledPatchedC=0" %
      len(retired_desktop_units))

# No legacy C source is admitted: the singleton and remaining root functions
# are owned by ESP32/src/engine_stubs.c; the packed SDL shim alone is generated.

env.BuildSources(
    join(build_dir, "doomrpg_esp32_sdl_patched"),
    patched_dir,
    src_filter=["+<esp32_sdl.cpp>"],
)
