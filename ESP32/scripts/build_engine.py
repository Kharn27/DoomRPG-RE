Import("env")

import os
import re
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
    "DoomCanvas_drawImageSpecial",
    "DoomCanvas_drawString1",
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
if "DoomCanvas_run(" in doom_canvas_bridge_text or "DoomCanvas_loadMap(" in doom_canvas_bridge_text:
    raise RuntimeError(
        "Desktop DoomCanvas state-machine/map-loading ownership leaked into the ESP32 bridge"
    )
print(
    "[ESP32] Desktop DoomCanvas.c retired; "
    f"esp_legacy_doomcanvas_bridge.c owns {len(doom_canvas_bridge_exports)} source ABI exports"
)

# Render.c is no longer generated or compiled in any CYD profile. The
# permanent esp_render_geometry_primitives.c owns the seven linked geometry
# primitives for normal AND bringup. Startup/palette/mapping/teardown owners
# remain unchanged; retired desktop BSP/shape/texel functions are not admitted.
# Desktop reference parity is checked by host tests, not build-time rewriting.
print("[ESP32] Desktop Render.c retired; geometry=esp-render-geometry-primitives "
      "profiles=normal+trace+bringup generated=no")

# The ESP32 firmware no longer compiles a generated copy of desktop Game.c.
# Its only four linked compatibility roots are implemented permanently by
# ESP32/src/esp_legacy_game_bridge.c. Any new retained Game_* dependency must
# therefore fail at link time instead of silently reviving desktop gameplay.
print(
    "[ESP32] Desktop Game.c retired; "
    "esp_legacy_game_bridge.c owns Game_init/Game_loadConfig/"
    "Game_unloadMapData/Game_activate"
)

# DoomRPG_createImage() is the central image-loading path used by the game.
# Desktop SDL handles the original indexed BMP variants, while the deliberately
# small ESP32 SDL shim initially handled only 8-bpp BMPs. Generate an ESP32-only
# DoomRPG.c copy that routes those image loads through Esp32Bmp_LoadRW(). The
# ESP32 loader preserves indexed 1/4/8-bpp pixels in their native packed form so
# large mobile assets do not need an expanded one-byte-per-pixel allocation.
doom_rpg_source = join(engine_dir, "DoomRPG.c")
doom_rpg_patched = join(patched_dir, "DoomRPG.c")

with open(doom_rpg_source, "r", encoding="latin-1") as source_file:
    doom_rpg_source_text = source_file.read()

# Retire only the six ELF-linked pure math/binary bodies from the generated
# DoomRPG.c. Original source stays an executable specification; the ESP32
# provider is a permanent stateless TU. Preserve the original arithmetic
# verbatim (including signed 32-bit quirks) until an independently tested
# behavior change is justified. Any upstream drift fails the build.
import zlib

def extract_doomrpg_function(source, signature):
    anchor = signature + "\n{"
    if source.count(anchor) != 1:
        raise RuntimeError("DoomRPG pure primitive signature drift: " + signature)
    start = source.index(anchor)
    depth = 0
    for pos in range(source.index("{", start), len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise RuntimeError("Unterminated DoomRPG primitive: " + signature)

pure_primitives = (
    ("fixed_t\tDoomRPG_FixedMul(fixed_t a, fixed_t b)", 0x383e7af6),
    ("fixed_t\tDoomRPG_FixedDiv(fixed_t a, fixed_t b)", 0x3cc430eb),
    ("short DoomRPG_shortAtNext(byte* data, int* posData)", 0x1af28fb4),
    ("short DoomRPG_shortAt(byte* data, int posData)", 0xe3c972b6),
    ("int DoomRPG_intAtNext(byte* data, int* posData)", 0x1fa2c02d),
    ("int DoomRPG_intAt(byte* data, int posData)", 0xd1998a2d),
)
pure_path = join(project_src_dir, "esp_doomrpg_primitives.c")
with open(pure_path, "r", encoding="latin-1") as pure_file:
    pure_native = pure_file.read()
for signature, expected_crc in pure_primitives:
    original_body = extract_doomrpg_function(doom_rpg_source_text, signature)
    if zlib.crc32(original_body.encode("latin-1")) != expected_crc:
        raise RuntimeError("DoomRPG primitive reference changed: " + signature)
    if extract_doomrpg_function(pure_native, signature) != original_body:
        raise RuntimeError("Native DoomRPG primitive differs from reference: " + signature)
    doom_rpg_source_text = doom_rpg_source_text.replace(
        original_body, "/* Stateless ESP32 primitive in esp_doomrpg_primitives.c */", 1
    )
print("[ESP32] DoomRPG pure primitives migrated=6 owner=esp_doomrpg_primitives "
      "source-bodies=removed")

doomrpg_display_x_needle = "doomrpg->doomCanvas->displayRect.x + "
doomrpg_display_y_needle = "doomrpg->doomCanvas->displayRect.y + "
doomrpg_display_x_count = doom_rpg_source_text.count(doomrpg_display_x_needle)
doomrpg_display_y_count = doom_rpg_source_text.count(doomrpg_display_y_needle)
if doomrpg_display_x_count != 7 or doomrpg_display_y_count != 7:
    raise RuntimeError("Unexpected DoomRPG drawing displayRect shape")
doom_rpg_source_text = doom_rpg_source_text.replace(doomrpg_display_x_needle, "")
doom_rpg_source_text = doom_rpg_source_text.replace(doomrpg_display_y_needle, "")

zip_include_needle = '#include "Z_Zip.h"\n'
zip_include_replacement = (
    '#include "esp32_bmp.h"\n'
    '#include "esp_legacy_asset_source.h"\n'
    '#include "esp_native_menu_storage.h"\n'
)
bmp_call_needle = "SDL_LoadBMP_RW("
bmp_call_count = doom_rpg_source_text.count(bmp_call_needle)
zip_read_needle = "readZipFileEntry(fileName, &zipFile, &fSize)"
zip_read_count = doom_rpg_source_text.count(zip_read_needle)
zip_close_needle = "\tcloseZipFile(&zipFile);\n"
zip_close_count = doom_rpg_source_text.count(zip_close_needle)
particle_free_needle = """\tif (doomrpg->particleSystem) {
\t\tParticleSystem_free(doomrpg->particleSystem, true);
\t}
\tdoomrpg->particleSystem = NULL;
"""
particle_free_replacement = """\t/* ESP32 native gameplay owns bounded gib effects; the inherited
\t * ParticleSystem object is never constructed. Keep the legacy field NULL
\t * without retaining the desktop ParticleSystem_free closure. */
\tdoomrpg->particleSystem = NULL;
"""
particle_free_count = doom_rpg_source_text.count(particle_free_needle)
player_free_needle = """\tif (doomrpg->player) {
\t\tSDL_memset(&doomrpg->player->ce, 0, sizeof(doomrpg->player->ce));
\t\tSDL_free(doomrpg->player);
\t}
\tdoomrpg->player = NULL;
"""
player_free_replacement = """\t/* ESP32 authoritative player state is native; Player_t is never constructed. */
\tdoomrpg->player = NULL;
"""
player_free_count = doom_rpg_source_text.count(player_free_needle)
entity_def_free_needle = """\tif (doomrpg->entityDef) {
\t\tEntityDef_free(doomrpg->entityDef, true);
\t}
\tdoomrpg->entityDef = NULL;
"""
entity_def_free_replacement = """\t/* ESP32 native resident maps use EspEntityDefTypeCatalog; the inherited
\t * EntityDefManager_t object is never constructed. */
\tdoomrpg->entityDef = NULL;
"""
entity_def_free_count = doom_rpg_source_text.count(entity_def_free_needle)
combat_free_needle = """	if (doomrpg->combat) {
		Combat_free(doomrpg->combat, true);
	}
	doomrpg->combat = NULL;
"""
combat_free_replacement = """	/* ESP32 native combat owners replace the inherited Combat_t object. */
	doomrpg->combat = NULL;
"""
combat_free_count = doom_rpg_source_text.count(combat_free_needle)
sound_free_needle = """	if (doomrpg->sound) {
		Sound_free(doomrpg->sound, true);
	}
	doomrpg->sound = NULL;
"""
sound_free_replacement = """	/* ESP32 audio playback is deferred and Sound_t is never constructed. */
	doomrpg->sound = NULL;
"""
sound_free_count = doom_rpg_source_text.count(sound_free_needle)
hud_free_needle = """\tif (doomrpg->hud) {
\t\tHud_free(doomrpg->hud, true);
\t}
\tdoomrpg->hud = NULL;
"""
hud_free_replacement = """\t/* ESP32 visible HUD is native and Hud_t is never constructed. */
\tdoomrpg->hud = NULL;
"""
hud_free_count = doom_rpg_source_text.count(hud_free_needle)
menu_free_needle = "\tif (doomrpg->menuSystem) {\n\t\tMenuSystem_free(doomrpg->menuSystem, true);\n\t}\n\tdoomrpg->menuSystem = NULL;\n"
menu_free_replacement = "\tif (doomrpg->menuSystem) {\n\t\tEspNativeMenuStorage_free(doomrpg->menuSystem, doomrpg, true);\n\t}\n\tdoomrpg->menuSystem = NULL;\n"
menu_free_count = doom_rpg_source_text.count(menu_free_needle)
game_storage_free_needle = """\tif (doomrpg->game) {
\t\tSDL_memset(&doomrpg->game->entityMonsters, 0, sizeof(doomrpg->game->entityMonsters));
\t\tSDL_memset(&doomrpg->game->entities, 0, sizeof(doomrpg->game->entities));
\t\tSDL_free(doomrpg->game);
\t}
\tdoomrpg->game = NULL;
"""
game_storage_free_replacement = """\tif (doomrpg->game) {
\t\t/* ESP32 Game_t is a minimal compatibility shell; native owners have
\t\t * already released world/session state. */
\t\tSDL_free(doomrpg->game);
\t}
\tdoomrpg->game = NULL;
"""
game_storage_free_count = doom_rpg_source_text.count(game_storage_free_needle)
game_memory_metric_needle = """\t\t\t\t\t\t\t\t\tdoomRpg->game->memory = DoomRPG_freeMemory() - mem;
"""
game_memory_metric_replacement = """\t\t\t\t\t\t\t\t\t/* ESP32 Game_t has no desktop allocation metric field. */
"""
game_memory_metric_count = doom_rpg_source_text.count(game_memory_metric_needle)
canvas_memory_metric_needle = """\t\tdoomRpg->doomCanvas->memory = DoomRPG_freeMemory() - mem;
"""
canvas_memory_metric_replacement = """\t\t/* ESP32 DoomCanvas_t has no retired desktop allocation metric field. */
"""
canvas_memory_metric_count = doom_rpg_source_text.count(canvas_memory_metric_needle)

if doom_rpg_source_text.count(zip_include_needle) != 1:
    raise RuntimeError("Unable to locate Z_Zip.h include in DoomRPG.c")
if bmp_call_count == 0:
    raise RuntimeError(
        "Unable to locate SDL_LoadBMP_RW calls in DoomRPG.c; "
        "review the ESP32 image-loader patch before building"
    )
if zip_read_count != 3 or zip_close_count != 1:
    raise RuntimeError(
        "Unexpected DoomRPG.c ZIP call shape; review native PAK source patch"
    )
if particle_free_count != 1:
    raise RuntimeError(
        "Unexpected DoomRPG.c ParticleSystem cleanup shape; "
        "review retired ESP32 particle ownership"
    )
if entity_def_free_count != 1:
    raise RuntimeError(
        "Unexpected DoomRPG.c EntityDef cleanup shape; "
        "review retired ESP32 EntityDef ownership"
    )
if player_free_count != 1:
    raise RuntimeError(
        "Unexpected DoomRPG.c Player cleanup shape; "
        "review retired ESP32 Player ownership"
    )
if combat_free_count != 1:
    raise RuntimeError(
        "Unexpected DoomRPG.c Combat cleanup shape; "
        "review retired ESP32 Combat ownership"
    )
if sound_free_count != 1:
    raise RuntimeError(
        "Unexpected DoomRPG.c Sound cleanup shape; "
        "review retired ESP32 Sound ownership"
    )
if hud_free_count != 1:
    raise RuntimeError(
        "Unexpected DoomRPG.c Hud cleanup shape; "
        "review retired ESP32 Hud ownership"
    )
if menu_free_count != 1:
    raise RuntimeError(
        "Unexpected DoomRPG.c MenuSystem cleanup shape; "
        "review native ESP32 menu storage ownership"
    )
if game_storage_free_count != 1:
    raise RuntimeError(
        "Unexpected DoomRPG.c legacy Game storage cleanup shape; "
        "review minimal ESP32 Game shell ownership"
    )
if game_memory_metric_count != 1:
    raise RuntimeError(
        "Unexpected DoomRPG.c Game allocation metric shape; "
        "review minimal ESP32 Game shell ownership"
    )
if canvas_memory_metric_count != 1:
    raise RuntimeError(
        "Unexpected DoomRPG.c DoomCanvas allocation metric shape; "
        "review compact ESP32 DoomCanvas ownership"
    )

doom_rpg_source_text = doom_rpg_source_text.replace(
    zip_include_needle, zip_include_replacement, 1
)
doom_rpg_source_text = doom_rpg_source_text.replace(
    bmp_call_needle, "Esp32Bmp_LoadRW("
)
doom_rpg_source_text = doom_rpg_source_text.replace(
    zip_read_needle, "EspLegacyAssetSource_readAlloc(fileName, &fSize)"
)
doom_rpg_source_text = doom_rpg_source_text.replace(zip_close_needle, "")
doom_rpg_source_text = doom_rpg_source_text.replace(
    particle_free_needle, particle_free_replacement, 1
)
doom_rpg_source_text = doom_rpg_source_text.replace(
    entity_def_free_needle, entity_def_free_replacement, 1
)
doom_rpg_source_text = doom_rpg_source_text.replace(
    player_free_needle, player_free_replacement, 1
)
doom_rpg_source_text = doom_rpg_source_text.replace(
    combat_free_needle, combat_free_replacement, 1
)
doom_rpg_source_text = doom_rpg_source_text.replace(
    sound_free_needle, sound_free_replacement, 1
)
doom_rpg_source_text = doom_rpg_source_text.replace(
    hud_free_needle, hud_free_replacement, 1
)
doom_rpg_source_text = doom_rpg_source_text.replace(
    menu_free_needle, menu_free_replacement, 1
)
doom_rpg_source_text = doom_rpg_source_text.replace(
    game_storage_free_needle, game_storage_free_replacement, 1
)
doom_rpg_source_text = doom_rpg_source_text.replace(
    game_memory_metric_needle, game_memory_metric_replacement, 1
)
doom_rpg_source_text = doom_rpg_source_text.replace(
    canvas_memory_metric_needle, canvas_memory_metric_replacement, 1
)

# Extract the three linked image/PAK methods to a permanent ESP32
# owner, after the legacy-to-PAK and indexed-BMP rewrites above. Keep their
# effective bodies exactly identical for this no-behavior-change transition.
# A source drift must be reviewed, not hidden by resurrecting desktop ZIP.
image_resource_methods = (
    ("void DoomRPG_createImage(DoomRPG_t* doomrpg, const char* resourceName, boolean isTransparentMask, Image_t* img)", 0xb853aa58),
    ("void DoomRPG_freeImage(DoomRPG_t* doomrpg, Image_t* image)", 0x34799bd6),
    ("byte *DoomRPG_fileOpenRead(DoomRPG_t* doomrpg, const char* resourceName)", 0xbf98b090),
)
image_owner_path = join(project_src_dir, "esp_doomrpg_image_resources.c")
with open(image_owner_path, "r", encoding="latin-1") as image_file:
    image_owner_source = image_file.read()
for signature, expected_crc in image_resource_methods:
    legacy_patched_body = extract_doomrpg_function(doom_rpg_source_text, signature)
    if zlib.crc32(legacy_patched_body.encode("latin-1")) != expected_crc:
        raise RuntimeError("DoomRPG image/PAK patched contract drift: " + signature)
    if extract_doomrpg_function(image_owner_source, signature) != legacy_patched_body:
        raise RuntimeError("ESP32 image resource owner parity drift: " + signature)
    doom_rpg_source_text = doom_rpg_source_text.replace(
        legacy_patched_body,
        "/* Native image/PAK owner: esp_doomrpg_image_resources.c */", 1
    )
print("[ESP32] DoomRPG image/resource provider moved=3 "
      "PAK=bounded indexedBMP=packed generatedBodies=removed")


# Linked presentation/clip ABI is owned by a permanent bounded native module,
# not by generated DoomRPG.c. The ESP32 displayRect stripping above is part
# of the already hardware-proven 160x120 contract, NOT a new coordinate rule.
# Unlike SDL's renderer, this owner does not allocate/own framebuffer pixels.
presentation_methods = (
    ("void DoomRPG_setClipFalse(DoomRPG_t* doomrpg)", 0x2dd78609),
    ("void DoomRPG_setClipTrue(DoomRPG_t* doomrpg, int x, int y, int w, int h)", 0xa02f2e6e),
    ("void DoomRPG_setColor(DoomRPG_t* doomrpg, int color)", 0x71b1064b),
    ("void DoomRPG_setFontColor(DoomRPG_t* doomrpg, int color)", 0xc7fa369c),
    ("void DoomRPG_fillRect(DoomRPG_t* doomrpg, int x, int y, int w, int h)", 0xbf15d43a),
    ("void DoomRPG_drawLine(DoomRPG_t* doomrpg, int x1, int y1, int x2, int y2)", 0x441e2ca2),
    ("void DoomRPG_flushGraphics(DoomRPG_t* doomrpg)", 0x7ca4dad8),
)
presentation_owner_path = join(project_src_dir, "esp_doomrpg_presentation.c")
with open(presentation_owner_path, "r", encoding="latin-1") as presentation_file:
    presentation_owner = presentation_file.read()
for signature, expected_crc in presentation_methods:
    adapted_body = extract_doomrpg_function(doom_rpg_source_text, signature)
    if zlib.crc32(adapted_body.encode("latin-1")) != expected_crc:
        raise RuntimeError("DoomRPG presentation contract drift: " + signature)
    if extract_doomrpg_function(presentation_owner, signature) != adapted_body:
        raise RuntimeError("ESP32 presentation owner ABI/body drift: " + signature)
    doom_rpg_source_text = doom_rpg_source_text.replace(
        adapted_body, "/* Native classic-CYD presentation owner */", 1
    )
for signature, _ in presentation_methods:
    if signature in doom_rpg_source_text:
        raise RuntimeError("Migrated presentation definition remains in DoomRPG.c: " + signature)
print("[ESP32] DoomRPG presentation/clip linked ABI moved=7 "
      "owner=esp_doomrpg_presentation generatedBodies=removed")

with open(doom_rpg_patched, "w", encoding="latin-1", newline="\n") as patched_file:
    patched_file.write(doom_rpg_source_text)

print(
    "[ESP32] DoomRPG generated with native PAK asset source + indexed BMP loader "
    f"({zip_read_count} ZIP read(s) retired, "
    f"{bmp_call_count} SDL_LoadBMP_RW call(s) redirected, "
    f"{particle_free_count} desktop ParticleSystem cleanup retired, "
    f"{entity_def_free_count} desktop EntityDef cleanup retired, "
    f"{player_free_count} desktop Player cleanup retired, "
    f"{combat_free_count} desktop Combat cleanup retired, "
    f"{sound_free_count} desktop Sound cleanup retired, "
    f"{hud_free_count} desktop Hud cleanup retired, "
    f"{menu_free_count} desktop MenuSystem cleanup redirected, "
    f"{game_storage_free_count} legacy Game storage cleanup retired, "
    f"{game_memory_metric_count} legacy Game allocation metric retired, "
    f"{canvas_memory_metric_count} legacy DoomCanvas allocation metric retired)"
)

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

# The desktop entry point and its SDL/audio/ZIP implementations are replaced by
# the small ESP32 compatibility layer in this PlatformIO project. DoomCanvas.c
# and Render.c are retired entirely; DoomRPG.c still uses a generated ESP32-safe
# copy while its remaining compatibility surface is migrated.
# The desktop original C source tree is no longer registered with SCons.
# All 21 original TUs were already excluded by the old '+<*.c>' source
# filter, so that registration compiled exactly zero objects. Make the
# boundary permanent instead of maintaining a fragile exclusion checklist:
# native sources live in ESP32/src; the *only* admitted legacy C copies are
# the single explicitly patched root below (DoomRPG.c).
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
      "originalC=%u compiledOriginal=0 patchedRoots=DoomRPG.c" %
      len(retired_desktop_units))

env.BuildSources(
    join(build_dir, "doomrpg_engine_patched"),
    patched_dir,
    src_filter=[
        "+<DoomRPG.c>",
    ],
)

env.BuildSources(
    join(build_dir, "doomrpg_esp32_sdl_patched"),
    patched_dir,
    src_filter=["+<esp32_sdl.cpp>"],
)
