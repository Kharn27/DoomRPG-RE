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

# Render_renderSpriteObject() still contains desktop monster activation
# gating through retired Player_t/Game_t fields. ESP32 native monster activation
# owns this behavior, so generate a narrow copy with that stale block removed.
render_source = join(engine_dir, "Render.c")
render_patched = join(patched_dir, "Render.c")

with open(render_source, "r", encoding="latin-1") as source_file:
    render_source_text = source_file.read()

render_include_needle = '#include "Render.h"\n'
render_include_replacement = (
    '#include "Render.h"\n'
    '#include "esp_map_catalog.h"\n'
    '#include "platform_video_config.h"\n'
)
render_include_count = render_source_text.count(render_include_needle)
if render_include_count != 1:
    raise RuntimeError(
        "Unexpected Render.h include shape; review native map catalog bridge"
    )
render_source_text = render_source_text.replace(
    render_include_needle, render_include_replacement, 1
)

render_map_file_needle = (
    "render->doomRpg->game->mapFiles[render->mapNameID - 1]"
)
render_map_file_replacement = (
    "EspMapCatalog_nameForId((uint8_t)render->mapNameID)"
)
render_map_file_count = render_source_text.count(render_map_file_needle)
if render_map_file_count != 1:
    raise RuntimeError(
        "Unexpected Render_beginLoadMap mapFiles shape; "
        "review minimal Game shell map ownership"
    )
render_source_text = render_source_text.replace(
    render_map_file_needle, render_map_file_replacement, 1
)

render_canvas_geometry_needle = """\trender->clipRect.x = render->doomRpg->doomCanvas->displayRect.x;
\trender->clipRect.y = render->doomRpg->doomCanvas->displayRect.y;
\trender->clipRect.w = render->doomRpg->doomCanvas->displayRect.w;
\trender->clipRect.h = render->doomRpg->doomCanvas->displayRect.h;
"""
render_canvas_geometry_replacement = """\trender->clipRect.x = DOOMRPG_CANVAS_X;
\trender->clipRect.y = DOOMRPG_CANVAS_Y;
\trender->clipRect.w = DOOMRPG_CANVAS_WIDTH;
\trender->clipRect.h = DOOMRPG_CANVAS_HEIGHT;
"""
render_canvas_geometry_count = render_source_text.count(render_canvas_geometry_needle)
if render_canvas_geometry_count != 1:
    raise RuntimeError("Unexpected Render_startup Canvas geometry shape")
render_source_text = render_source_text.replace(
    render_canvas_geometry_needle, render_canvas_geometry_replacement, 1
)

render_canvas_shake_x_needle = "render->doomRpg->doomCanvas->shakeX"
render_canvas_shake_y_needle = "render->doomRpg->doomCanvas->shakeY"
render_canvas_shake_x_count = render_source_text.count(render_canvas_shake_x_needle)
render_canvas_shake_y_count = render_source_text.count(render_canvas_shake_y_needle)
if render_canvas_shake_x_count != 4 or render_canvas_shake_y_count != 4:
    raise RuntimeError(
        "Unexpected Render Canvas shake shape; review native shake ownership"
    )
render_source_text = render_source_text.replace(render_canvas_shake_x_needle, "0")
render_source_text = render_source_text.replace(render_canvas_shake_y_needle, "0")

render_legacy_activation_needle = """\tif (sprite->ent && sprite->ent->monster &&
\t\t!(sprite->ent->info & 0x80000) && !(sprite->info & 0x1000000) &&
\t\t!render->doomRpg->player->noclip && !render->doomRpg->game->disableAI) {
\t\tGame_activate(render->doomRpg->game, sprite->ent);
\t}

"""
render_legacy_activation_replacement = """\t/* ESP32 native monster activation owns visibility/activation state.
\t * Player_t is retired and Game_activate() is a fail-closed ABI no-op. */

"""
render_legacy_activation_count = render_source_text.count(
    render_legacy_activation_needle
)
if render_legacy_activation_count != 1:
    raise RuntimeError(
        "Unexpected Render_renderSpriteObject activation shape; "
        "review native monster activation ownership"
    )
render_source_text = render_source_text.replace(
    render_legacy_activation_needle,
    render_legacy_activation_replacement,
    1,
)

# The two linked Render core roots now belong to the permanent
# ESP32 render startup bridge. Keep their source slice pinned so an original
# change must be audited instead of silently changing the native ABI.
render_core_begin = "Render_t* Render_init(Render_t* render, DoomRPG_t* doomRpg)\n{"
render_core_setup = "void Render_setup(Render_t* render, SDL_Rect* windowRect)\n{"
render_core_end = "void Render_freeRuntime(Render_t* render) {"
if any(render_source_text.count(anchor) != 1 for anchor in
       (render_core_begin, render_core_setup, render_core_end)):
    raise RuntimeError("Unexpected inherited Render core boundary")
render_core_slice = render_source_text[
    render_source_text.index(render_core_begin):
    render_source_text.index(render_core_end)]
import zlib
if zlib.crc32(render_core_slice.encode("latin-1")) != 0xb89f15f1:
    raise RuntimeError("Original Render_init/Render_setup changed: review native bridge")
render_source_text = render_source_text.replace(
    render_core_begin, "#if !defined(DOOMRPG_ESP32)\n" + render_core_begin, 1
)
render_source_text = render_source_text.replace(
    render_core_end,
    "#endif /* native ESP32 Render init/setup roots */\n\n" +
    render_core_end, 1
)
print("[ESP32] Render_init/Render_setup desktop roots retired; "
      "owner=esp-native-render-startup bridge=permanent")

# Render_startup now has a direct, single permanent ESP32 implementation.
# The original desktop startup allocated a second full RGB565 framebuffer
# and SDL texture, and its linker-wrapped entry was never executed here.
# Source signature/CRC pin keeps legacy specification drift explicit.
native_startup_begin = "int Render_startup(Render_t* render)\n{"
native_startup_end = "void Render_loadPalettes(Render_t* render)\n{"
if render_source_text.count(native_startup_begin) != 1 or \
   render_source_text.count(native_startup_end) != 1:
    raise RuntimeError("Original desktop Render_startup source boundary changed")
# The canvas-geometry patch above intentionally modifies the generated
# startup body before we exclude it. Fingerprint the unmodified source,
# while separately validating the patched definition boundary below.
with open(render_source, "r", encoding="latin-1") as original_render_file:
    original_render_for_startup = original_render_file.read()
if (original_render_for_startup.count(native_startup_begin) != 1 or
        original_render_for_startup.count(native_startup_end) != 1):
    raise RuntimeError("Desktop original Render_startup boundary changed")
native_startup_original_region = original_render_for_startup[
    original_render_for_startup.index(native_startup_begin):
    original_render_for_startup.index(native_startup_end)]
if zlib.crc32(native_startup_original_region.encode("latin-1")) != 0xf0d935e8:
    raise RuntimeError("Desktop Render_startup changed; audit native equivalent")
render_source_text = render_source_text.replace(
    native_startup_begin,
    "#if !defined(DOOMRPG_ESP32)\n" + native_startup_begin, 1)
render_source_text = render_source_text.replace(
    native_startup_end,
    "#endif /* Render_startup direct native ESP32 owner */\n\n" +
    native_startup_end, 1)
print("[ESP32] Desktop Render_startup unlinked; "
      "owner=esp-native-render-startup direct=yes linkerWrap=no")

# Render_freeRuntime production owner; preserve the original desktop/probe
# body while the permanent ESP32 teardown bridge owns the active ABI.
native_runtime_free_begin = "void Render_freeRuntime(Render_t* render) {"
native_runtime_free_end = "void Render_free(Render_t* render, boolean freePtr)\n{"
if (render_source_text.count(native_runtime_free_begin) != 1 or
        render_source_text.count(native_runtime_free_end) != 1):
    raise RuntimeError("Render_freeRuntime source boundary changed")
native_runtime_free_original = original_render_for_startup[
    original_render_for_startup.index(native_runtime_free_begin):
    original_render_for_startup.index(native_runtime_free_end)]
if render_source_text[
    render_source_text.index(native_runtime_free_begin):
    render_source_text.index(native_runtime_free_end)] != native_runtime_free_original:
    raise RuntimeError("Generated Render_freeRuntime diverged from desktop source")
render_source_text = render_source_text.replace(
    native_runtime_free_begin,
    "#if !defined(DOOMRPG_ESP32)\n" + native_runtime_free_begin, 1)
render_source_text = render_source_text.replace(
    native_runtime_free_end,
    "#endif /* native Render_freeRuntime production owner */\n\n" +
    native_runtime_free_end, 1)
print("[ESP32] Render_freeRuntime production=esp-native-bridge desktop=original")

# Render_free's native bridge implements exactly the inherited shell cleanup,
# with the PlatformVideo framebuffer detached before the legacy
# Render_freeRuntime() cleanup path. Prevent a duplicated desktop destructor.
native_free_begin = "void Render_free(Render_t* render, boolean freePtr)\n{"
native_free_next = "#if !defined(DOOMRPG_ESP32)\nint Render_startup(Render_t* render)\n{"
native_free_original_next = "int Render_startup(Render_t* render)\n{"
if (render_source_text.count(native_free_begin) != 1 or
        render_source_text.count(native_free_next) != 1):
    raise RuntimeError("Generated Render_free source boundary changed")
if (original_render_for_startup.count(native_free_begin) != 1 or
        original_render_for_startup.count(native_free_original_next) != 1):
    raise RuntimeError("Original desktop Render_free source boundary changed")
original_free_slice = original_render_for_startup[
    original_render_for_startup.index(native_free_begin):
    original_render_for_startup.index(native_free_original_next)]
if zlib.crc32(original_free_slice.encode("latin-1")) != 0x5c51ec08:
    raise RuntimeError("Original Render_free changed: audit native teardown owner")
render_source_text = render_source_text.replace(
    native_free_begin,
    "#if !defined(DOOMRPG_ESP32)\n" + native_free_begin, 1)
render_source_text = render_source_text.replace(
    native_free_next,
    "#endif /* Native direct Render_free owns platform framebuffer guard */\n\n" +
    native_free_next, 1)
print("[ESP32] Desktop Render_free unlinked; "
      "owner=esp-native-render-startup direct=yes linkerWrap=no "
      "sharedFB=PlatformVideo guarded=yes")

# Render palette, RGB565 and mappings roots have permanent native owners.
# The retained source slice is pinned to the exact legacy specification,
# with a strict function census and CRC32 to catch desktop-side drift.
palette_native_begin = "void Render_loadPalettes(Render_t* render)\n{"
palette_native_mapping = "boolean Render_loadMappings(Render_t* render)\n{"
palette_native_end = "boolean Render_beginLoadMap(Render_t* render, int mapNameID)\n{"
palette_native_symbols = (
    "void Render_loadPalettes(",
    "unsigned int Render_make565RGB(",
    "unsigned short Render_RGB888_To_RGB565(",
    "void Render_setGrayPalettes(",
    "boolean Render_loadMappings(",
)
if any(render_source_text.count(anchor) != 1 for anchor in
       (palette_native_begin, palette_native_mapping, palette_native_end)):
    raise RuntimeError("Unexpected original Render palette/mappings boundary")
if any(render_source_text.count(symbol) != 1 for symbol in palette_native_symbols):
    raise RuntimeError("Unexpected original Render palette/mappings export census")
palette_native_region = render_source_text[
    render_source_text.index(palette_native_begin):
    render_source_text.index(palette_native_end)]
if zlib.crc32(palette_native_region.encode("latin-1")) != 0x6f5b63d3:
    raise RuntimeError(
        "Original Render palette/mappings source changed: review native ABI"
    )
render_source_text = render_source_text.replace(
    palette_native_begin, "#if !defined(DOOMRPG_ESP32)\n" +
    palette_native_begin, 1
)
render_source_text = render_source_text.replace(
    palette_native_end,
    "#endif /* native ESP32 palette/mappings roots */\n\n" +
    palette_native_end, 1
)
print("[ESP32] Render palette/mappings desktop roots retired; "
      "exports=5 palette=esp-render-startup mappings=esp-config-mappings")

# The original Render_beginLoadMap* BSP parser is a desktop/bringup-only
# diagnostic path. Production already builds the immutable EspMapRuntime from
# the native PAK backing. Reject accidental calls instead of retaining its
# 1024-byte Render.mapFlags mirror (or accepting a legacy map-wide decoder).
render_legacy_map_begin = "boolean Render_beginLoadMap(Render_t* render, int mapNameID)\n{"
render_legacy_map_data = "boolean Render_beginLoadMapData(Render_t* render)\n{"
render_legacy_map_end = "boolean Render_loadBitShapes(Render_t* render)\n{"
if any(render_source_text.count(anchor) != 1 for anchor in
       (render_legacy_map_begin, render_legacy_map_data, render_legacy_map_end)):
    raise RuntimeError("Unexpected legacy Render BSP loader source shape")
render_legacy_map_region = render_source_text[
    render_source_text.index(render_legacy_map_begin):
    render_source_text.index(render_legacy_map_end)]
if (render_source_text.count("render->mapFlags") != 13 or
        render_legacy_map_region.count("render->mapFlags") != 13):
    raise RuntimeError("Render.mapFlags gained an unreviewed source consumer")
render_source_text = render_source_text.replace(
    render_legacy_map_begin, "#if defined(DOOMRPG_ESP32_BRINGUP_PROBES)\n" + render_legacy_map_begin, 1)
render_source_text = render_source_text.replace(
    render_legacy_map_end,
    "#endif /* production legacy BSP loader rejection */\n\n" +
    render_legacy_map_end, 1)
# Fourth bounded ownership cut: map-wide custom/drop sprite pointer
# mirrors are only populated within this rejected legacy BSP loader.
# Native topology and native monster drops carry the production state.
for field, expected in (("customSprites", 1), ("dropSprites", 1),
                        ("firstDropSprite", 1)):
    token = "render->" + field
    if (render_source_text.count(token) != expected or
            render_legacy_map_region.count(token) != expected):
        raise RuntimeError("Unreviewed legacy Render sprite mirror: " + field)
print("[ESP32] Legacy Render BSP loader fail-closed in production; "
      "Render.mapFlags 1024-byte mirror retired (bringup/desktop unchanged)")

# Second Render ownership cut: the desktop/J2ME plane-cell array is
# absent from the normal classic-CYD layout. Production rendering uses the
# immutable native plane records and the native renderer. Legacy BSP load
# (already fail-closed above) and legacy world/plane draw must not silently
# reintroduce another 2048-cell map-wide mirror.
legacy_render_entry = "void Render_render(Render_t* render, int viewx, int viewy, int viewz, unsigned int viewangle)\n{"
legacy_render_next = "void Render_initColumnScale(Render_t* render)\n{"
legacy_plane_entry = "void Render_renderFloorAndCeilingBG(Render_t* render)\n{"
legacy_plane_next = "void Render_drawplane(Render_t* render, int x, int y, PlaneTextureRef_t* planeTextures, int cnt)\n{"
for needle in (legacy_render_entry, legacy_render_next,
               legacy_plane_entry, legacy_plane_next):
    if render_source_text.count(needle) != 1:
        raise RuntimeError("Unexpected legacy Render draw shape: " + needle[:60])
legacy_plane_use = re.findall(r"render->planeTextures\b", render_source_text)
if len(legacy_plane_use) != 4:
    raise RuntimeError("Unexpected legacy Render planeTextures use count")
legacy_loader_piece = render_source_text[
    render_source_text.index(render_legacy_map_data):
    render_source_text.index(render_legacy_map_end)]
legacy_draw_piece = render_source_text[
    render_source_text.index(legacy_plane_entry):
    render_source_text.index(legacy_plane_next)]
if (len(re.findall(r"render->planeTextures\b", legacy_loader_piece)) != 2 or
        len(re.findall(r"render->planeTextures\b", legacy_draw_piece)) != 2):
    raise RuntimeError("Render planeTextures read/write closure changed")
render_source_text = render_source_text.replace(
    legacy_render_entry, "#if defined(DOOMRPG_ESP32_BRINGUP_PROBES)\n" + legacy_render_entry, 1)
render_source_text = render_source_text.replace(
    legacy_render_next,
    "#endif /* production legacy world renderer rejection */\n\n" +
    legacy_render_next, 1)
render_source_text = render_source_text.replace(
    legacy_plane_entry, "#if defined(DOOMRPG_ESP32_BRINGUP_PROBES)\n" + legacy_plane_entry, 1)
render_source_text = render_source_text.replace(
    legacy_plane_next,
    "#endif /* production legacy plane renderer rejection */\n\n" +
    legacy_plane_next, 1)
print("[ESP32] Legacy Render_render/plane BG fail-closed in production; "
      "Render.planeTextures 2048-byte mirror retired (bringup/desktop unchanged)")

# Every legacy producer/consumer must stay inside the audited closure.
for field, expected in (("planeTexturesCnt", 7), ("planeTextureIds", 4),
                        ("planeTexelOffsets", 2), ("planePaletteOffsets", 2)):
    if len(re.findall(r"render->" + field + r"\b", render_source_text)) != expected:
        raise RuntimeError("Unreviewed legacy Render plane field: " + field)
# Production has no legacy plane descriptor metadata; reject legacy helpers.
plane_draw = "void Render_drawplane(Render_t* render, int x, int y, PlaneTextureRef_t* planeTextures, int cnt)\n{"
plane_end = "void Render_renderBSP(Render_t* render)\n{"
if render_source_text.count(plane_draw) != 1 or render_source_text.count(plane_end) != 1:
    raise RuntimeError("Unexpected legacy Render plane helper source")
render_source_text = render_source_text = render_source_text.replace(
    plane_draw, "#if defined(DOOMRPG_ESP32_BRINGUP_PROBES)\n" + plane_draw, 1)
render_source_text = render_source_text.replace(plane_end, "#endif\n\n" + plane_end, 1)

# Fifth bounded cut: viewNodes was a 44-byte linked-list sentinel in
# the retired Render_renderBSP/Render_walkNode traversal. Native visibility
# uses compact immutable BSP data and never needs this mutable list.
# Preserve full desktop/bringup code and fail-close legacy production entry.
view_bsp_begin = "void Render_renderBSP(Render_t* render)\n{"
view_bsp_end = "void Render_renderBSPNoclip(Render_t* render)\n{"
view_walk_begin = "void Render_walkNode(Render_t* render, int i)\n{"
view_walk_end = "boolean Render_cullBoundingBox(Render_t* render, Node_t* node)\n{"
for anchor in (view_bsp_begin, view_bsp_end, view_walk_begin, view_walk_end):
    if render_source_text.count(anchor) != 1:
        raise RuntimeError("Unreviewed legacy BSP view-list source shape")
if render_source_text.count("render->viewNodes") != 6:
    raise RuntimeError("Unreviewed legacy Render.viewNodes consumer")
if (render_source_text[render_source_text.index(view_bsp_begin):
                       render_source_text.index(view_bsp_end)].count("render->viewNodes") != 3 or
        render_source_text[render_source_text.index(view_walk_begin):
                           render_source_text.index(view_walk_end)].count("render->viewNodes") != 3):
    raise RuntimeError("Legacy viewNodes consumer escaped its retired owner")
render_source_text = render_source_text.replace(
    view_bsp_begin, "#if defined(DOOMRPG_ESP32_BRINGUP_PROBES)\n" + view_bsp_begin, 1)
render_source_text = render_source_text.replace(
    view_bsp_end,
    "#endif /* production legacy BSP traversal rejection */\n\n" + view_bsp_end, 1)
render_source_text = render_source_text.replace(
    view_walk_begin, "#if defined(DOOMRPG_ESP32_BRINGUP_PROBES)\n" + view_walk_begin, 1)
render_source_text = render_source_text.replace(
    view_walk_end,
    "#endif /* production legacy BSP walk rejection */\n\n" + view_walk_end, 1)
print("[ESP32] Legacy BSP view-list traversal fail-closed; "
      "Render.viewNodes 44-byte sentinel retired from production")

# The normal firmware exports eight legacy fail-closed ABI endpoints
# from a permanent ESP32 C file; desktop originals remain bringup-only.
reject_path = join(project_src_dir, "esp_legacy_render_reject.c")
with open(reject_path, "r", encoding="utf-8") as reject_handle:
    reject_code = reject_handle.read()
reject_exports = ("Render_beginLoadMap", "Render_beginLoadMapData",
                  "Render_render", "Render_renderFloorAndCeilingBG",
                  "Render_drawplane", "Render_spanPlane",
                  "Render_renderBSP", "Render_walkNode")
if any(reject_code.count(name + "(") != 1 for name in reject_exports):
    raise RuntimeError("Native Render reject export census changed")
print("[ESP32] Legacy Render map/world/BSP rejects native-owned; "
      "exports=8 production=fail-closed bringup=desktop-original")

# The native CYD renderer still consumes a small set of legacy-named geometry
# primitives. They are pure Render scratch/math helpers: no desktop map owner,
# no mediaTexels/shapeData access, no world mutation. Keep the desktop/bringup
# reference bodies, but production must resolve these ABI exports from the
# permanent ESP32 source below rather than generated Render.c.
def extract_render_function(source, signature):
    start = source.find(signature)
    if start < 0:
        raise RuntimeError("Missing Render geometry function: " + signature)
    brace = source.find("{", start)
    if brace < 0:
        raise RuntimeError("Missing Render geometry function body: " + signature)
    depth = 0
    for pos in range(brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise RuntimeError("Unterminated Render geometry function: " + signature)

render_geometry_functions = (
    ("void Render_initColumnScale(Render_t* render)", 0xc33077ed),
    ("boolean Render_cullBoundingBox(Render_t* render, Node_t* node)", 0x2b5b02ac),
    ("void Render_transform2DVerts(Render_t* render, Vertex_t* vert)", 0x8c89a9a6),
    ("boolean Render_clipLine(Render_t* render, Line_t* line)", 0x67f70ded),
    ("void Render_clipVertex(Render_t* render, Vertex_t* vert, Line_t* line, int i, int i2)", 0x0a7ffb34),
    ("void Render_projectVertex(Render_t* render, Vertex_t* vert)", 0xd92b8789),
    ("void Render_occludeClippedLine(Render_t* render, Line_t* line)", 0x71aa6efe),
)
for signature, expected_crc in render_geometry_functions:
    original_body = extract_render_function(original_render_for_startup, signature)
    current_body = extract_render_function(render_source_text, signature)
    if current_body != original_body:
        raise RuntimeError(
            "Generated Render geometry drifted before ownership cut: " + signature
        )
    if zlib.crc32(original_body.encode("latin-1")) != expected_crc:
        raise RuntimeError(
            "Original Render geometry changed; review native equivalent: " + signature
        )
    render_source_text = render_source_text.replace(
        current_body,
        "#if defined(DOOMRPG_ESP32_BRINGUP_PROBES)\n" + current_body +
        "\n#endif /* native ESP32 Render geometry owner */",
        1,
    )

geometry_path = join(project_src_dir, "esp_render_geometry_primitives.c")
with open(geometry_path, "r", encoding="utf-8") as geometry_handle:
    geometry_code = geometry_handle.read()
if any(geometry_code.count(signature + "\n{") != 1
       for signature, _ in render_geometry_functions):
    raise RuntimeError("Native Render geometry export census changed")
print("[ESP32] Render geometry primitives native-owned; "
      "exports=7 production=esp-native bringup=desktop-original")

with open(render_patched, "w", encoding="latin-1", newline="\n") as patched_file:
    patched_file.write(render_source_text)

print(
    "[ESP32] Render generated with "
    f"{render_legacy_activation_count} legacy Game/Player monster activation block retired + "
    f"{render_map_file_count} legacy Game mapFiles lookup redirected + "
    f"{render_canvas_geometry_count} Canvas geometry mirror retired + "
    f"{render_canvas_shake_x_count + render_canvas_shake_y_count} Canvas shake reads fixed-zero"
)

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
# is retired entirely; DoomRPG.c and Render.c still use generated ESP32-safe
# copies while their remaining compatibility surfaces are migrated.
# The desktop original C source tree is no longer registered with SCons.
# All 21 original TUs were already excluded by the old '+<*.c>' source
# filter, so that registration compiled exactly zero objects. Make the
# boundary permanent instead of maintaining a fragile exclusion checklist:
# native sources live in ESP32/src; the *only* admitted legacy C copies are
# the two explicitly patched roots below (DoomRPG.c and Render.c).
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
      "originalC=%u compiledOriginal=0 patchedRoots=DoomRPG.c,Render.c" %
      len(retired_desktop_units))

env.BuildSources(
    join(build_dir, "doomrpg_engine_patched"),
    patched_dir,
    src_filter=[
        "+<DoomRPG.c>",
        "+<Render.c>",
    ],
)

env.BuildSources(
    join(build_dir, "doomrpg_esp32_sdl_patched"),
    patched_dir,
    src_filter=["+<esp32_sdl.cpp>"],
)
