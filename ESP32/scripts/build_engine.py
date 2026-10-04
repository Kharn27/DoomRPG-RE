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
    '#include "esp_native_audio_intent.h"\n'
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

legacy_weapon_draw_needle = """	if (doomCanvas->state != ST_CAST) {
		Combat_drawWeapon(doomCanvas->combat, doomCanvas->shakeX, doomCanvas->shakeY - (doomCanvas->captureState == 2 ? 10 : 0));
	}
"""
legacy_weapon_draw_replacement = """	/* ESP32 resident gameplay owns first-person weapon presentation through
	 * EspNativeGameplayWeapon. DoomCanvas::combat is intentionally NULL. */
"""
legacy_weapon_draw_count = doom_canvas.count(legacy_weapon_draw_needle)
if legacy_weapon_draw_count != 1:
    raise RuntimeError(
        "Unexpected DoomCanvas legacy weapon draw shape; "
        "review retired ESP32 Combat ownership"
    )
doom_canvas = doom_canvas.replace(
    legacy_weapon_draw_needle, legacy_weapon_draw_replacement, 1
)

sound_enabled_needle = "			if (doomCanvas->doomRpg->sound->soundEnabled != 0) {"
sound_enabled_replacement = "			if (0) { /* ESP32 audio playback deferred; legacy soundEnabled was false */"
sound_enabled_count = doom_canvas.count(sound_enabled_needle)
sound_nextplay_needle = "\tdoomCanvas->doomRpg->sound->nextplay = 0;\n"
sound_nextplay_count = doom_canvas.count(sound_nextplay_needle)
if sound_enabled_count != 1 or sound_nextplay_count != 1:
    raise RuntimeError(
        "Unexpected DoomCanvas direct Sound_t field access shape; "
        "review retired ESP32 Sound ownership"
    )
doom_canvas = doom_canvas.replace(
    sound_enabled_needle, sound_enabled_replacement, 1
)
doom_canvas = doom_canvas.replace(sound_nextplay_needle, "", 1)

legacy_legals_load_needle = (
    '\tDoomRPG_createImage(doomCanvas->doomRpg, "g.bmp", false, '
    '&doomCanvas->imgLegals);\n'
)
legacy_legals_load_replacement = (
    '\t/* ESP32 native boot skips ST_LEGALS and paints MENU_MAIN directly; '
    'do not retain the 128x512 legacy legal strip. */\n'
)
legacy_legals_load_count = doom_canvas.count(legacy_legals_load_needle)
if legacy_legals_load_count != 1:
    raise RuntimeError(
        "Unexpected DoomCanvas legacy legal asset load shape; "
        "review native main-menu ownership"
    )
doom_canvas = doom_canvas.replace(
    legacy_legals_load_needle, legacy_legals_load_replacement, 1
)

legacy_hud_startup_needle = (
    "\tHud_startup(doomCanvas->hud, doomCanvas->largeStatus);\n"
)
legacy_hud_startup_replacement = """\t/* ESP32 native HUD owns visible HUD composition and fixed 20/80/20
\t * geometry. The inherited Hud_t owner is retired and remains NULL. */
"""
legacy_hud_startup_count = doom_canvas.count(legacy_hud_startup_needle)
if legacy_hud_startup_count != 1:
    raise RuntimeError(
        "Unexpected DoomCanvas legacy Hud_startup shape; "
        "review native HUD ownership"
    )
doom_canvas = doom_canvas.replace(
    legacy_hud_startup_needle, legacy_hud_startup_replacement, 1
)

legacy_hud_bind_needle = "\tdoomCanvas->hud = doomRpg->hud;\n"
legacy_hud_bind_replacement = """\t/* ESP32 native HUD state is independent of desktop Hud_t. */
\tdoomCanvas->hud = NULL;
"""
legacy_hud_bind_count = doom_canvas.count(legacy_hud_bind_needle)

legacy_hud_height_needle = (
    "\theight = (displayH - (doomRpg->hud->statusBarHeight) - "
    "(doomRpg->hud->statusTopBarHeight));\n"
)
legacy_hud_height_replacement = "\theight = displayH - 40;\n"
legacy_hud_height_count = doom_canvas.count(legacy_hud_height_needle)

legacy_hud_display_height_needle = (
    "\tdoomCanvas->displayRect.h = (doomRpg->hud->statusBarHeight + "
    "height + doomRpg->hud->statusTopBarHeight);\n"
)
legacy_hud_display_height_replacement = (
    "\tdoomCanvas->displayRect.h = 20 + height + 20;\n"
)
legacy_hud_display_height_count = doom_canvas.count(
    legacy_hud_display_height_needle
)

legacy_hud_screen_y_needle = (
    "\tdoomCanvas->screenRect.y = doomCanvas->displayRect.y + "
    "doomRpg->hud->statusTopBarHeight;\n"
)
legacy_hud_screen_y_replacement = (
    "\tdoomCanvas->screenRect.y = doomCanvas->displayRect.y + 20;\n"
)
legacy_hud_screen_y_count = doom_canvas.count(legacy_hud_screen_y_needle)

legacy_hud_softkey_needle = (
    "\t\tHud_drawBarTiles(doomCanvas->doomRpg->hud, x, y, "
    "doomCanvas->clipRect.w, false);\n"
)
legacy_hud_softkey_replacement = (
    "\t\t/* ESP32 native UI owns bars/soft-key surfaces. */\n"
)
legacy_hud_softkey_count = doom_canvas.count(legacy_hud_softkey_needle)

legacy_hud_automap_state_needle = """\t\t\tdoomCanvas->doomRpg->hud->isUpdate = true;
\t\t\tHud_drawTopBar(doomCanvas->doomRpg->hud);
\t\t\tHud_drawBottomBar(doomCanvas->doomRpg->hud);
"""
legacy_hud_automap_state_replacement = """\t\t\t/* ESP32 native dialog/HUD composition repaints explicitly. */
"""
legacy_hud_automap_state_count = doom_canvas.count(
    legacy_hud_automap_state_needle
)

legacy_hud_dying_needle = "\t\tHud_drawBottomBar(doomCanvas->hud);\n"
legacy_hud_dying_replacement = (
    "\t\t/* ESP32 native player-death path owns HUD presentation. */\n"
)
legacy_hud_dying_count = doom_canvas.count(legacy_hud_dying_needle)

hud_object_patch_counts = [
    legacy_hud_bind_count,
    legacy_hud_height_count,
    legacy_hud_display_height_count,
    legacy_hud_screen_y_count,
    legacy_hud_softkey_count,
    legacy_hud_automap_state_count,
    legacy_hud_dying_count,
]
if any(count != 1 for count in hud_object_patch_counts):
    raise RuntimeError(
        "Unexpected DoomCanvas direct Hud_t usage shape; "
        "review retired ESP32 Hud ownership"
    )

doom_canvas = doom_canvas.replace(
    legacy_hud_bind_needle, legacy_hud_bind_replacement, 1
)
doom_canvas = doom_canvas.replace(
    legacy_hud_height_needle, legacy_hud_height_replacement, 1
)
doom_canvas = doom_canvas.replace(
    legacy_hud_display_height_needle,
    legacy_hud_display_height_replacement,
    1,
)
doom_canvas = doom_canvas.replace(
    legacy_hud_screen_y_needle, legacy_hud_screen_y_replacement, 1
)
doom_canvas = doom_canvas.replace(
    legacy_hud_softkey_needle, legacy_hud_softkey_replacement, 1
)
doom_canvas = doom_canvas.replace(
    legacy_hud_automap_state_needle,
    legacy_hud_automap_state_replacement,
    1,
)
doom_canvas = doom_canvas.replace(
    legacy_hud_dying_needle, legacy_hud_dying_replacement, 1
)

menu_play_needle = "MenuSystem_playSound(doomCanvas->menuSystem);"
menu_enter_sound_needle = "Sound_playSound(doomCanvas->doomRpg->sound, 5067, 0, 3);"
menu_play_count = doom_canvas.count(menu_play_needle)
menu_enter_sound_count = doom_canvas.count(menu_enter_sound_needle)
if menu_play_count != 1 or menu_enter_sound_count != 1:
    raise RuntimeError(
        "Unexpected DoomCanvas ST_MENU sound calls; review native audio intent patch"
    )
doom_canvas = doom_canvas.replace(
    menu_play_needle,
    "(void)EspNativeAudioIntent_publish(5042U, 0U, 3U);",
    1,
)
doom_canvas = doom_canvas.replace(
    menu_enter_sound_needle,
    "(void)EspNativeAudioIntent_publish(5067U, 0U, 3U);",
    1,
)

with open(doom_canvas_patched, "w", encoding="latin-1", newline="\n") as patched_file:
    patched_file.write(doom_canvas)

print(
    "[ESP32] DoomCanvas generated with 160x120-aware minimum height + "
    f"native menu audio intents + {legacy_weapon_draw_count} legacy Combat weapon draw retired + "
    f"{sound_enabled_count + sound_nextplay_count} direct Sound field access(es) retired + "
    f"{legacy_legals_load_count} legacy legal-strip load retired + "
    f"{legacy_hud_startup_count} legacy HUD startup retired + "
    f"{sum(hud_object_patch_counts)} direct Hud_t use(s) retired"
)

# Game_unloadMapData() still clears two fields on the inherited Combat_t during
# START/LOAD menu-runtime teardown. ESP32 deliberately keeps DoomRPG_t::combat
# NULL, so generate a narrow ESP32 Game.c copy that retires only those obsolete
# pointer resets. The desktop source remains the behavioral reference.
game_source = join(engine_dir, "Game.c")
game_patched = join(patched_dir, "Game.c")

with open(game_source, "r", encoding="latin-1") as source_file:
    game_source_text = source_file.read()

game_combat_cleanup_needle = """	game->doomRpg->combat->curTarget = NULL;
	game->doomRpg->combat->curAttacker = NULL;
"""
game_combat_cleanup_replacement = """	/* ESP32 native combat has no Combat_t owner. The legacy target/attacker
	 * reset is obsolete; native combat owners reset independently. */
"""
game_combat_cleanup_count = game_source_text.count(game_combat_cleanup_needle)
if game_combat_cleanup_count != 1:
    raise RuntimeError(
        "Unexpected Game_unloadMapData Combat cleanup shape; "
        "review retired ESP32 Combat ownership"
    )
game_source_text = game_source_text.replace(
    game_combat_cleanup_needle, game_combat_cleanup_replacement, 1
)

game_sound_volume_needle = """\t\t\tintData = File_readInt(rw);
\t\t\tif (game) {
\t\t\t\tgame->doomRpg->sound->volume = intData;
\t\t\t}
"""
game_sound_volume_replacement = """\t\t\tintData = File_readInt(rw);
\t\t\t/* ESP32 audio playback is deferred; consume the legacy config
\t\t\t * volume field without retaining Sound_t. */
\t\t\t(void)intData;
"""
game_sound_volume_count = game_source_text.count(game_sound_volume_needle)
if game_sound_volume_count != 1:
    raise RuntimeError(
        "Unexpected Game_loadConfig Sound volume shape; "
        "review retired ESP32 Sound ownership"
    )
game_source_text = game_source_text.replace(
    game_sound_volume_needle, game_sound_volume_replacement, 1
)

with open(game_patched, "w", encoding="latin-1", newline="\n") as patched_file:
    patched_file.write(game_source_text)

print(
    "[ESP32] Game generated with "
    f"{game_combat_cleanup_count} legacy Combat cleanup reset retired + "
    f"{game_sound_volume_count} legacy Sound config field retired"
)

# Player_reset() still clears the inherited Hud_t message buffers. The native
# gameplay feedback/message owners reset independently, so generate an ESP32-only
# Player.c copy without those obsolete Hud_t writes.
player_source = join(engine_dir, "Player.c")
player_patched = join(patched_dir, "Player.c")

with open(player_source, "r", encoding="latin-1") as source_file:
    player_source_text = source_file.read()

player_hud_reset_needle = """\tplayer->doomRpg->hud->logMessage[0] = '\\0';
\tplayer->doomRpg->hud->msgCount = 0;
"""
player_hud_reset_replacement = """\t/* ESP32 native feedback/message owners do not retain Hud_t. */
"""
player_hud_reset_count = player_source_text.count(player_hud_reset_needle)
if player_hud_reset_count != 1:
    raise RuntimeError(
        "Unexpected Player_reset Hud_t cleanup shape; "
        "review retired ESP32 Hud ownership"
    )
player_source_text = player_source_text.replace(
    player_hud_reset_needle, player_hud_reset_replacement, 1
)

with open(player_patched, "w", encoding="latin-1", newline="\\n") as patched_file:
    patched_file.write(player_source_text)

print(
    "[ESP32] Player generated with "
    f"{player_hud_reset_count} legacy Hud reset block retired"
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

with open(doom_rpg_patched, "w", encoding="latin-1", newline="\n") as patched_file:
    patched_file.write(doom_rpg_source_text)

print(
    "[ESP32] DoomRPG generated with native PAK asset source + indexed BMP loader "
    f"({zip_read_count} ZIP read(s) retired, "
    f"{bmp_call_count} SDL_LoadBMP_RW call(s) redirected, "
    f"{particle_free_count} desktop ParticleSystem cleanup retired, "
    f"{entity_def_free_count} desktop EntityDef cleanup retired, "
    f"{combat_free_count} desktop Combat cleanup retired, "
    f"{sound_free_count} desktop Sound cleanup retired, "
    f"{hud_free_count} desktop Hud cleanup retired, "
    f"{menu_free_count} desktop MenuSystem cleanup redirected)"
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
# and DoomRPG.c are compiled from generated ESP32-safe copies above.
env.BuildSources(
    join(build_dir, "doomrpg_engine"),
    engine_dir,
    src_filter=[
        "+<*.c>",
        "-<Main.c>",
        "-<SDL_Video.c>",
        "-<Sound.c>",
        # Menu_t / MenuItem helpers / MenuSystem_t behavior and ParticleSystem_t
        # are retired ESP32 ownership surfaces. Exclude their desktop translation
        # units rather than relying on final-link garbage collection.
        "-<Menu.c>",
        "-<MenuItem.c>",
        "-<MenuSystem.c>",
        "-<ParticleSystem.c>",
        "-<EntityDef.c>",
        "-<Combat.c>",
        "-<Weapon.c>",
        "-<Hud.c>",
        "-<Game.c>",
        "-<Player.c>",
        "-<Z_Zone.c>",
        "-<Z_Zip.c>",
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
        "+<Game.c>",
        "+<Player.c>",
    ],
)

env.BuildSources(
    join(build_dir, "doomrpg_esp32_sdl_patched"),
    patched_dir,
    src_filter=["+<esp32_sdl.cpp>"],
)
