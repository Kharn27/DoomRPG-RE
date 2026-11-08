Import("env")

import os
import shutil
from os.path import join


project_dir = env.subst("$PROJECT_DIR")
build_dir = env.subst("$BUILD_DIR")
project_src_dir = join(project_dir, "src")
engine_dir = join(project_dir, "..", "src")
patched_dir = join(build_dir, "doomrpg_patched_sources")

env.Append(CPPPATH=[join(project_dir, "include"), project_src_dir, engine_dir])

# The desktop DoomCanvas.c and its final free hook are retired. Glyph raster
# lives in the native text owner, not a legacy compatibility bridge.
native_canvas_text_source = join(project_src_dir, "esp_native_canvas_text.c")
retired_canvas_bridge_source = join(project_src_dir, "esp_legacy_doomcanvas_bridge.c")
if os.path.isfile(retired_canvas_bridge_source):
    raise RuntimeError("Retired desktop Canvas bridge source revived")
if not os.path.isfile(native_canvas_text_source):
    raise RuntimeError("Missing native Canvas text raster source")
with open(native_canvas_text_source, "r", encoding="utf-8") as source_file:
    native_canvas_text = source_file.read()
if ("static void EspDoomCanvas_drawImageSpecial(" not in native_canvas_text or
        "void DoomCanvas_drawImageSpecial(" in native_canvas_text):
    raise RuntimeError("DoomCanvas glyph blitter must be private, not public desktop ABI")
if ("void EspNativeCanvas_drawText(" not in native_canvas_text or
        "const char* text," not in native_canvas_text or
        "void DoomCanvas_drawString1(" in native_canvas_text):
    raise RuntimeError("Legacy Canvas text ABI revived or native const-text owner missing")
if "void DoomCanvas_free(" in native_canvas_text:
    raise RuntimeError("Retired Canvas destructor ABI revived in glyph owner")
native_layout_source = join(project_src_dir, "engine_stubs.c")
if not os.path.isfile(native_layout_source):
    raise RuntimeError("native engine layout owner missing")
with open(native_layout_source, "r", encoding="utf-8") as owner_file:
    native_layout_owner = owner_file.read()
if ("static DoomCanvas_t* EspNativeEngineRoot_initCanvas(" not in native_layout_owner or
        "EspNativeEngineRoot_initCanvas(NULL, doomRpg));" not in native_layout_owner or
        "static void EspNativeEngineRoot_releaseCanvas(" not in native_layout_owner or
        "EspNativeEngineRoot_releaseCanvas(doomrpg->doomCanvas);" not in native_layout_owner or
        "DoomCanvas_free(" in native_layout_owner or
        "DoomCanvas_init(" in native_canvas_text):
    raise RuntimeError("Retired Canvas lifecycle ABI revived or native root owner absent")
if ("static void EspNativeEngineLayout_startupCanvas(" not in native_layout_owner or
        "EspNativeEngineLayout_startupCanvas(canvas);" not in native_layout_owner or
        "DoomCanvas_startup(" in native_canvas_text):
    raise RuntimeError("Retired Canvas startup ABI revived or native layout owner absent")
native_menu_state_source = join(project_src_dir, "native_main_menu_model.c")
if not os.path.isfile(native_menu_state_source):
    raise RuntimeError("native menu state owner missing")
with open(native_menu_state_source, "r", encoding="utf-8") as owner_file:
    native_menu_state_owner = owner_file.read()
if ("void EspNativeMainMenu_transitionCanvasState(" not in native_menu_state_owner or
        "DoomCanvas_setState(" in native_canvas_text):
    raise RuntimeError("Retired desktop Canvas state ABI revived or native menu owner absent")
if "DoomCanvas_run(" in native_canvas_text or "DoomCanvas_loadMap(" in native_canvas_text:
    raise RuntimeError("Desktop Canvas state-machine/map ownership revived")
print("[ESP32] Desktop DoomCanvas.c and lifecycle bridge retired; "
      "desktopCanvasABI=0 nativeCanvasText=1 nativeRootLifecycle=1")

# Render.c is no longer generated or compiled in any CYD profile. The
# permanent esp_render_geometry_primitives.c owns the seven linked geometry
# primitives for normal AND bringup. Startup/palette/mapping/teardown owners
# remain unchanged; retired desktop BSP/shape/texel functions are not admitted.
# Desktop reference parity is checked by host tests, not build-time rewriting.
print("[ESP32] Desktop Render.c retired; geometry=esp-render-geometry-primitives "
      "profiles=normal+trace+bringup generated=no")

# The former production Render reject TU is now empty of legitimate owners.
# Reject its return in every firmware profile: real BSP/plane/map operations
# belong to compact native modules, not desktop-shaped safety stubs.
if os.path.isfile(join(project_src_dir, "esp_legacy_render_reject.c")):
    raise RuntimeError("Legacy Render reject translation unit revived")
print("[ESP32] Legacy Render reject TU retired nativeBSP+planes=owned")

# Render_setup is private to native layout; guard the exclusive ownership.
with open(join(project_src_dir, "render_startup_bridge.c"), "r", encoding="utf-8") as f:
    native_render_startup = f.read()
if ("void Render_setup(" in native_render_startup or
        "static void EspNativeEngineLayout_setupRender(" not in native_layout_owner or
        "EspNativeEngineLayout_setupRender(doomRpg->render, &screenRect);" not in native_layout_owner or
        "Render_setup(doomRpg->render" in native_layout_owner):
    raise RuntimeError("Retired Render_setup ABI revived or private native layout absent")
print("[ESP32] Render_setup retired nativeLayout=private")

# Legacy Render_startup is now implementation-private inside the native owner;
# the source-body parity and error-path semantics are tested using real C.
if ("int Render_startup(Render_t* render) {" in native_render_startup or
        "static int EspNativeRenderStartup_initialize(Render_t* render) {" not in native_render_startup or
        "result = EspNativeRenderStartup_initialize(render);" not in native_render_startup or
        "result = Render_startup(render);" in native_render_startup):
    raise RuntimeError("Render_startup desktop ABI revived or native initializer absent")
print("[ESP32] Render_startup retired nativeRenderStartup=private")

# Render_free is private to the fatal process-root teardown. The legacy
# cleanup slots now have a separate native-named two-caller API.
if ("void Render_free(Render_t* render, boolean freePtr)" in native_render_startup or
        "static void EspNativeEngineRoot_releaseRender(Render_t* render, boolean freePtr)" not in native_layout_owner or
        "EspNativeEngineRoot_releaseRender(doomrpg->render, true);" not in native_layout_owner or
        "Render_free(doomrpg->render" in native_layout_owner):
    raise RuntimeError("Retired Render_free ABI revived or private root teardown absent")
print("[ESP32] Render_free retired nativeRenderTeardown=private runtimeSlots=separate")

# Two native callers share compatibility Render cleanup. Reject the desktop
# Render_freeRuntime ABI without accidentally privatizing MENU_MAIN START.
if ("void Render_freeRuntime(Render_t* render)" in native_render_startup or
        "void EspNativeRenderRuntime_releaseCompatibility(Render_t* render)" not in native_render_startup or
        "EspNativeRenderRuntime_releaseCompatibility(render);" not in native_layout_owner or
        "Render_freeRuntime(render);" in native_layout_owner):
    raise RuntimeError("Retired Render_freeRuntime ABI revived or native dual-lifetime cleanup absent")
print("[ESP32] Render_freeRuntime retired nativeRenderRuntimeCleanup=shared-native-two-callers")

# The palette loader is only a private Render startup phase. The existing
# RGB565 conversion helpers remain independent public compatibility APIs.
if ("void Render_loadPalettes(Render_t* render)" in native_render_startup or
        "static void EspNativeRenderStartup_loadPalettes(Render_t* render)" not in native_render_startup or
        "EspNativeRenderStartup_loadPalettes(render);" not in native_render_startup or
        "Render_loadPalettes(render);" in native_render_startup):
    raise RuntimeError("Retired Render_loadPalettes ABI revived or native startup owner absent")
print("[ESP32] Render_loadPalettes retired nativePaletteLoader=private")

# The legacy Render_setGrayPalettes mutator has no native owner or caller.
# Native grayscale effects, if ever required, must own their own small color
# state and must not mutate map-wide texture arrays from desktop Render.
if "void Render_setGrayPalettes(Render_t* render)" in native_render_startup:
    raise RuntimeError("Unused Render_setGrayPalettes legacy ABI revived")
print("[ESP32] Render_setGrayPalettes retired nativeGrayPaletteOwner=none")

# No Render_loadMappings compatibility symbol may remain: CONFIGMAP must call
# the existing native PAK/framebuffer-scratch mapping owner directly.
native_mapping_path = join(project_src_dir, "esp_legacy_config_mappings_startup.c")
# Oracle-retirement host tests intentionally emulate a minimal project_src_dir
# without native TUs; full PlatformIO builds always own this source file. The
# dedicated native_render_mappings_retirement_host.py independently verifies
# the actual repository sources, including this direct caller contract.
if os.path.isfile(native_mapping_path):
    with open(native_mapping_path, "r", encoding="utf-8") as f:
        native_mapping_owner = f.read()
    if ("boolean Render_loadMappings(Render_t* render)" in native_mapping_owner or
            "int EspLegacyMappings_load(struct Render_s* renderBase)" not in native_mapping_owner or
            "mappingsResult = EspLegacyMappings_load(render) ? true : false;" not in native_mapping_owner or
            "mappingsResult = Render_loadMappings(render);" in native_mapping_owner):
        raise RuntimeError("Retired Render_loadMappings ABI revived or direct native mapping owner missing")
print("[ESP32] Render_loadMappings retired nativeMappings=direct-PAK-owned")

# No Game.c provider, generated or native bridge, is compiled on ESP32.
# Game_t is not allocated, the compatibility root pointer stays NULL, and
# Game_* calls are intentionally unavailable at link time.
print("[ESP32] Desktop Game.c retired; bridge=absent Game_init=retired "
      "gameObject=NULL originalC=uncompiled")

# The original DoomRPG.c is a behavioral reference, never a firmware or
# intermediate build input. Its linked functions already have native owners.
# Reference-body/CRC guards now live in host tests, not in an orphaned
# generated C translation unit.
# Remove stale generated inputs/objects from an older, incremental .pio build.
# Neither desktop source nor SDL shim is ever generated by this pre-script.
for stale_name in ("DoomRPG.c", "esp32_sdl.cpp"):
    stale_source = join(patched_dir, stale_name)
    if os.path.isfile(stale_source):
        os.remove(stale_source)
stale_sdl_objects = join(build_dir, "doomrpg_esp32_sdl_patched")
if os.path.isdir(stale_sdl_objects):
    shutil.rmtree(stale_sdl_objects)
print("[ESP32] DoomRPG C oracle generation retired; reference=src/DoomRPG.c "
      "compiledPatchedC=0 nativeProviders=yes")

# SDL-A: the compact, indexed shim is a permanent ESP32/src translation unit.
# No Python textual patch, generated source or BuildSources registration.
if not os.path.isfile(join(project_src_dir, "esp32_sdl.cpp")):
    raise RuntimeError("Missing permanent SDL shim source")
print("[ESP32] SDL shim permanent; source=ESP32/src/esp32_sdl.cpp generated=no")

# The original desktop C is only a test reference: all firmware providers live
# in ESP32/src. The one generated source is the native packed SDL shim.
# The desktop original C source tree is not registered with SCons.
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

# No legacy C source is admitted: singleton/root owners stay in ESP32/src.
