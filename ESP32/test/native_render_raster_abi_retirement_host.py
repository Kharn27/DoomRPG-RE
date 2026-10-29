#!/usr/bin/env python3
"""Retirement guard for twenty-three desktop-only raster/background/geometry ABIs.

Classic CYD uses the compact native BSP/world/plane renderers. The old raster
reject-only TU has no remaining legitimate symbol, and must not be revived.
Desktop Render.c continues to define the original behavior/reference.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "ESP32"
HEADER = (ROOT / "src/Render.h").read_text(encoding="latin-1")
DESKTOP = (ROOT / "src/Render.c").read_text(encoding="latin-1")
REJECT_PATH = NATIVE / "src/esp_legacy_render_reject.c"

RETIRED = (
    "Render_render",
    "Render_renderFloorAndCeilingBG",
    "Render_drawplane",
    "Render_spanPlane",
    "Render_renderFloorAndCeilingSolidBG",
    "Render_getSpanMode",
    "Render_SpanMode0",
    "Render_SpanMode1",
    "Render_SpanMode2",
    "Render_SpanMode3",
    "Render_SpanMode4",
    "Render_SpanMode5",
    "Render_SpanMode6",
    "Render_SpanMode7",
    "Render_SpanMode8",
    "Render_SpanMode9",
    "Render_occludeClippedLine",
    "Render_projectVertex",
    "Render_transform2DVerts",
    "Render_initColumnScale",
    "Render_clipVertex",
    "Render_cullBoundingBox",
    "Render_clipLine",
)
LIVE_GEOMETRY = ()

assert not REJECT_PATH.exists(), "obsolete reject-only translation unit returned"
assert (NATIVE / "src/esp_native_plane_renderer.c").is_file()
geometry = (NATIVE / "src/esp_render_geometry_primitives.c").read_text(
    encoding="utf-8"
)

for name in RETIRED:
    # The original desktop implementation must remain authoritative.
    assert re.search(r"\b(?:void|boolean)\s+" + name + r"\s*\(", DESKTOP), name
    declaration = re.search(
        r"(?m)^[^\n]*\b" + name + r"\s*\([^;]*;", HEADER
    )
    assert declaration, "desktop declaration missing: " + name
    # The last still-open preprocessing guard is #ifndef DOOMRPG_ESP32.
    open_guard = HEADER.rfind("#ifndef DOOMRPG_ESP32", 0, declaration.start())
    last_close = HEADER.rfind("#endif", 0, declaration.start())
    assert open_guard > last_close, "ESP32-visible retired raster/span ABI: " + name
    assert HEADER.find("#endif", declaration.end()) >= 0, name

# The retired Render_t occlusion entrypoint is replaced by an explicit
# bounded column-scratch contract, consumed at both BSP render sites.
native_header = (NATIVE / "include/esp_render_geometry_primitives.h").read_text()
assert "Render_t" not in native_header and "Line_t" not in native_header
assert "EspNativeRenderGeometry_occludeProjectedSpan" in native_header
assert "EspNativeRenderGeometry_projectVertex" in native_header
assert "EspNativeRenderGeometry_clipLine" in native_header
assert re.search(r"\bint\s+EspNativeRenderGeometry_clipLine\s*\(", geometry)
assert not re.search(r"\bRender_clipLine\s*\(", geometry)
assert "EspNativeRenderGeometry_resetColumns" in native_header
assert re.search(r"\bEspNativeRenderGeometry_resetColumns\s*\(", geometry)
assert "EspNativeRenderGeometry_resetColumns" in (
    NATIVE / "src/esp_native_first_frame.c"
).read_text()
assert "EspNativeRenderGeometry_transformPoint" in native_header
assert re.search(r"\bEspNativeRenderGeometry_transformPoint\s*\(", geometry)
for owner in ("esp_native_first_frame.c", "esp_native_bsp_visibility.c",
              "esp_native_sprite_renderer.c"):
    assert "EspNativeRenderGeometry_transformPoint" in (NATIVE / "src" / owner).read_text()
assert re.search(r"\bEspNativeRenderGeometry_projectVertex\s*\(", geometry)
for owner in ("esp_native_first_frame.c", "esp_native_bsp_visibility.c",
              "esp_native_sprite_renderer.c"):
    assert "EspNativeRenderGeometry_projectVertex" in (NATIVE / "src" / owner).read_text()
assert re.search(r"\bEspNativeRenderGeometry_occludeProjectedSpan\s*\(", geometry)
for owner in ("esp_native_first_frame.c", "esp_native_bsp_visibility.c"):
    assert "EspNativeRenderGeometry_occludeProjectedSpan" in (NATIVE / "src" / owner).read_text()

# Native BSP culling borrows only scalar camera values and a live read-only
# column array. No caller-owned Line_t or renderer tmpLine is read/copied;
# all mutable line scratch remains inside the culling function.
assert "EspNativeRenderCullView" in native_header
assert "struct Render_s" not in native_header
assert "const int* columnScale;" in native_header
assert "struct Line_s* scratch" not in native_header
assert "int x1, int y1, int x2, int y2);" in native_header
assert "EspNativeRenderGeometry_cullBounds" in native_header
assert re.search(r"\bint\s+EspNativeRenderGeometry_cullBounds\s*\(", geometry)
assert re.search(
    r"EspNativeRenderGeometry_cullBounds\(const EspNativeRenderCullView\* view",
    geometry)
cull_body = geometry[geometry.index("int EspNativeRenderGeometry_cullBounds("):
                     geometry.index("void EspNativeRenderGeometry_transformPoint(")]
assert "Render_t" not in cull_body and "render->" not in cull_body
assert "Line_t localLine = {0};" in cull_body
assert "struct Line_s* scratch" not in cull_body
assert "view->columnScale[i]" in cull_body
for owner in ("esp_native_first_frame.c", "esp_native_bsp_visibility.c"):
    src = (NATIVE / "src" / owner).read_text()
    assert "EspNativeRenderCullView cullView;" in src, owner
    assert "EspNativeRenderGeometry_cullBounds(" in src, owner
    assert "cullScratch" not in src, owner
    assert "->tmpLine = savedTmpLine;" not in src, owner
    assert "savedTmpLine" not in src, owner
    assert ".columnScale = " in src, owner
pio = (NATIVE / "platformio.ini").read_text()
assert "--wrap=Render_cullBoundingBox" not in pio
assert "--wrap=Render_clipLine" not in pio
dynamic = (NATIVE / "src/esp_native_dynamic_line_render.c").read_text()
assert "EspNativeDynamicLineRender_clipLine" in dynamic
assert "__wrap_Render_clipLine" not in dynamic
assert "__real_Render_clipLine" not in dynamic
assert "return EspNativeRenderGeometry_clipLine(line);" in dynamic
assert "EspNativeDynamicLineRender_clipLine" in (
    NATIVE / "include/esp_native_dynamic_clip.h"
).read_text()
for owner in ("esp_native_first_frame.c", "esp_native_bsp_visibility.c",
              "esp_native_sprite_renderer.c"):
    src = (NATIVE / "src" / owner).read_text()
    assert "EspNativeDynamicLineRender_clipLine" in src
    assert not re.search(r"\bRender_clipLine\s*\(", src)

# The visibility query must not snapshot or restore the old Render tmpLine.
# The culler now creates its own zeroed line and all depth/projection lines
# are function-local, so the query's transactional scratch excludes tmpLine.
visibility = (NATIVE / "src/esp_native_bsp_visibility.c").read_text()
assert "Line_t tmpLine;" not in visibility
assert "->tmpLine" not in visibility

# Desktop-only span palette scratch belongs to Render_SpanMode*, not the
# native BSP/wall/sprite rasterizer (normal, bringup or trace).
assert re.search(
    r"#ifndef DOOMRPG_ESP32\s+unsigned short\* spanPalettes;"
    r"\s+unsigned short newSpanPalette\[16\];\s+#endif", HEADER
), "desktop span palette fields must be excluded from CYD"

# The native CYD uses one shared PlatformVideo framebuffer, never the
# desktop span callback or SDL streaming texture handle. The desktop
# Render.c / Render.h retain the unchanged callback and texture semantics.
assert re.search(r"#ifndef DOOMRPG_ESP32\s+span_t spanFunction;\s+#endif", HEADER)
assert re.search(r"#ifndef DOOMRPG_ESP32\s+SDL_Texture\* piDIB;\s+#endif", HEADER)
startup = (NATIVE / "src/render_startup_bridge.c").read_text()
root = (NATIVE / "src/engine_stubs.c").read_text()
assert "render->piDIB" not in startup
assert "render->piDIB" not in root
assert "SDL_DestroyTexture(render->piDIB)" not in root
assert "render->framebuffer = sharedFramebuffer;" in startup
assert "render->framebuffer = NULL;" in root

# The seven desktop raster gate flags are removed from the CYD layout.
# Preserve skipCull: native BSP geometry still reads it. Never accidentally
# delete active spanMode or the culling camera/column scratch.
assert re.search(
    r"#ifndef DOOMRPG_ESP32\s+boolean skipStretch;\s+boolean unk4;\s+#endif"
    r"\s+boolean skipCull;\s+#ifndef DOOMRPG_ESP32"
    r"\s+boolean skipBSP;\s+boolean skipLines;\s+boolean unk5;"
    r"\s+boolean skipSprites;\s+boolean skipViewNudge;\s+#endif", HEADER
), "retired raster gates must be desktop-only; skipCull must remain native"
assert "render->skipCull = 0;" in root

# PR237: only ten desktop memory ledgers disappear from the ESP32 shell;
# the live paletteMemory accounting and all geometry/pixel members remain.
ledger = (
    "mapMemory", "stringMem", "eventMem", "memory", "bitShapeMemory",
    "lineMem", "spriteMem", "nodeMem", "texelMemory", "mappingMemory",
)
for name in ledger:
    assert len(re.findall(r"\bint " + name + r";", HEADER)) == 1, name
assert re.search(
    r"#ifndef DOOMRPG_ESP32\s+int mapMemory;.*?int texelMemory;\s+#endif"
    r"\s+int paletteMemory;\s+#ifndef DOOMRPG_ESP32\s+int mappingMemory;\s+#endif",
    HEADER, re.S,
), "desktop ledgers must be conditional and native paletteMemory retained"
assert "render->paletteMemory = DoomRPG_freeMemory();" in (
    NATIVE / "src/render_startup_bridge.c"
).read_text()
for path in (NATIVE / "src").rglob("*"):
    if path.suffix not in (".c", ".cpp"):
        continue
    code = path.read_text(encoding="latin-1")
    code = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ", code, flags=re.S)
    for name in ledger:
        # SDL_RWops itself owns a distinct generic "memory" pointer.
        # This scan must not reject its legitimate non-Render member.
        receiver = r"\b(?:render|renderBase|r)\s*->" if name == "memory" else r"->"
        assert not re.search(receiver + r"\s*" + name + r"\b", code), (path, name)

# PR238: the three timings sampled inside desktop Render_render() are not
# native Render_t state. The native frame clock/profilers own their own values.
# Keep animFrameTime (initialized by the native constructor) and all six
# live per-frame raster counters / native geometry fields unchanged.
for field in ("currentFrameTime", "horizonFrameTime", "frameTime"):
    assert len(re.findall(r"\bint " + field + r";", HEADER)) == 1, field
    assert re.search(
        r"\brender->" + field + r"\s*=\s*DoomRPG_GetTimeMS\(\);",
        DESKTOP,
    ), ("desktop Render_render timestamp missing", field)
assert re.search(
    r"int animFrameTime;\s+/\* Desktop raster phase timestamps are not "
    r"native CYD state\..*?\*/\s+#ifndef DOOMRPG_ESP32"
    r"\s+int currentFrameTime;\s+int horizonFrameTime;"
    r"\s+int frameTime;\s+#endif\s+struct DoomRPG_s\* doomRpg;",
    HEADER, re.S,
), "desktop-only frame timestamps; keep animFrameTime native"
assert "render->animFrameTime = 0;" in root
for source_dir in (NATIVE / "src", NATIVE / "include"):
    for path in sorted(source_dir.rglob("*")):
        if path.suffix not in (".c", ".cpp", ".h"):
            continue
        code = path.read_text(encoding="latin-1")
        code = re.sub(
            r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
            " ", code, flags=re.S,
        )
        for field in ("currentFrameTime", "horizonFrameTime", "frameTime"):
            assert not re.search(
                r"\b(?:render|renderBase|rootRender)\s*->\s*" + field + r"\b",
                code,
            ), ("retired Render_t desktop frame timer referenced", path, field)

# PR238 stage B: five opaque desktop-only Render_t spares have no native
# reader or writer. In particular, the 16-bit and byte members may merely
# remove padding, so confirm the complete 32-bit Render_t size separately.
for type_, field in (
    ("int", "unk0"), ("int", "viewZOld"), ("short", "unk1"),
    ("int", "unk2"), ("byte", "unk3"),
):
    declaration = r"\b" + type_ + r"\s+" + field + r";"
    assert len(re.findall(declaration, HEADER)) == 1, field
    assert re.search(
        r"#ifndef DOOMRPG_ESP32\s+" + declaration + r"\s+#endif",
        HEADER,
    ), ("desktop-only opaque Render field must remain hidden on CYD", field)
    # Match Render pointer receivers only. PlayerView and spawn snapshot
    # legitimately use viewZOld as a separate native-owned member.
    # Strip comments and string literals before scanning native sources.
    for tree in (NATIVE / "src", NATIVE / "include"):
        for path in sorted(tree.rglob("*")):
            if path.suffix not in (".c", ".cpp", ".h"):
                continue
            code = path.read_text(encoding="latin-1")
            code = re.sub(
                r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                " ", code, flags=re.S,
            )
            assert not re.search(
                r"\b(?:render|renderBase|rootRender|r)\s*(?:->|\.)\s*"
                + field + r"\b", code,
            ), ("retired opaque Render field accessed by native owner", path, field)
assert "render->animFrameTime = 0;" in root
assert "render->skipCull = 0;" in root
assert "int lineRasterCount;" in HEADER and "int nodeRasterCount;" in HEADER
assert "int spriteRasterCount;" in HEADER

# The sprite renderer is the other owner of transactional frame scratch.
# Its clipping/projection lines are function-local too. No CYD Render_t
# member or native source may retain desktop tmpLine after this milestone.
sprite = (NATIVE / "src/esp_native_sprite_renderer.c").read_text()
assert "Line_t tmpLine;" not in sprite
assert "->tmpLine" not in sprite
assert "_Static_assert(sizeof(Scratch) <= 740U" in sprite
assert "sizeof(Render_t) == 1364U" in (
    NATIVE / "src/render_startup_bridge.c").read_text()
assert re.search(
    r"#if !defined\(DOOMRPG_ESP32\) \|\| defined\(DOOMRPG_ESP32_GEOMETRY_ORACLE\)"
    r"\s+struct Line_s tmpLine;\s+#endif", HEADER
), "desktop tmpLine must be absent from normal/bringup/trace native layout"
assert "_Static_assert(sizeof(Scratch) <= 740U" in visibility
assert "saveScratch(render, &before);" in visibility
assert "restoreScratch(render, &before);" in visibility
assert "if (memcmp(&before, &after, sizeof(before)) != 0)" in visibility

# Both native BSP traversal owners feed compact immutable node bounds directly
# to the culler. Explicit 16-bit signed conversions preserve legacy short
# widening, including any unusual source coordinates. No desktop Node_t copy.
for owner in ("esp_native_first_frame.c", "esp_native_bsp_visibility.c"):
    src = (NATIVE / "src" / owner).read_text()
    assert not re.search(r"\bNode_t\b", src), owner
    assert "EspMapNode compact;" in src, owner
    assert "EspMapRuntime_getNode(" in src, owner
    assert re.search(
        r"EspNativeRenderGeometry_cullBounds\(\s*(?:&cullView|cullView),"
        r"\s*\(short\)compact\.x1,\s*\(short\)compact\.y1,"
        r"\s*\(short\)compact\.x2,\s*\(short\)compact\.y2\)",
        src,
    ), owner

# Same-TU culling formerly called the unwrapped Render_clipLine directly.
# Preserve that pure internal clip; external wall/BSP-depth/sprite calls
# explicitly own the transient animated-door z adaptation.
assert "if (EspNativeRenderGeometry_clipLine(line))" in geometry
assert "__wrap_Render_cullBoundingBox" not in (
    NATIVE / "src/native_first_frame_render_wrappers.c"
).read_text()
assert "__real_Render_cullBoundingBox" not in (
    NATIVE / "src/native_first_frame_render_wrappers.c"
).read_text()

# Column reset formerly carried a hidden --wrap plane-render dependency.
# Guard the explicit predecessor ordering so the real-CYD WORLD_POST/planes=no
# regression cannot silently recur after differential geometry tests pass.
frame = (NATIVE / "src/esp_native_first_frame.c").read_text()
plane_wrapper = (NATIVE / "src/native_first_frame_render_wrappers.c").read_text()
plane_header = (NATIVE / "include/esp_native_plane_renderer.h").read_text()
pio = (NATIVE / "platformio.ini").read_text()
assert "--wrap=Render_initColumnScale" not in pio
assert "__wrap_Render_initColumnScale" not in plane_wrapper
assert "__real_Render_initColumnScale" not in plane_wrapper
assert "EspNativeWorldPlane_renderAtColumnReset" in plane_header
assert "EspNativeWorldPlane_renderAtColumnReset" in plane_wrapper
assert "EspNativePlaneRenderer_render(render)" in plane_wrapper
point = frame.index("EspNativeRenderGeometry_resetColumns(render->columnScale")
planes = frame.index("if (!EspNativeWorldPlane_renderAtColumnReset(render))", point)
bsp = frame.index("if (!walkNodes(work))", point)
assert point < planes < bsp, "world planes must render after column reset, before BSP"

# Internal interpolation must not expose the desktop ABI or depend on Render_t.
assert re.search(r"static\s+void\s+clipVertex\s*\(", geometry)
assert geometry.count("clipVertex(") == 8  # declaration + body + six calls

# Permanent native geometry ownership must survive this source retirement.
for name in LIVE_GEOMETRY:
    assert re.search(r"\b" + name + r"\b", HEADER), name
    assert re.search(r"\b" + name + r"\s*\(", geometry), name

# Check every production/bringup/trace C/C++ translation unit and header.
# Bare address-taking, macro expansions and calls are all forbidden.
for subtree in (NATIVE / "src", NATIVE / "include"):
    for path in sorted(subtree.rglob("*")):
        if path.suffix not in (".c", ".cpp", ".h"):
            continue
        source = path.read_text(encoding="latin-1")
        code = re.sub(
            r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
            " ", source, flags=re.S,
        )
        assert not re.search(r"(?:->|\.)tmpLine\b", code), (
            "retired desktop scratch member reference", path
        )
        for member in ("spanPalettes", "newSpanPalette", "spanFunction", "piDIB",
                       "skipStretch", "unk4", "skipBSP", "skipLines",
                       "unk5", "skipSprites", "skipViewNudge"):
            assert not re.search(r"(?:->|\.)" + member + r"\b", code), (
                "desktop-only span scratch accessed from native source", path, member
            )
        for name in RETIRED:
            assert not re.search(r"\b" + name + r"\b", code), (
                "retired desktop raster/span reference", path, name
            )


# PR236 B bounded hardware diagnosis: identify raw-flash I/O vs sprite work.
# No new reads, textures, heap buffers, or per-range logging.
profile_header = (NATIVE / "include/esp_asset_pack.h").read_text()
profile_pack = (NATIVE / "src/esp_asset_pack.cpp").read_text()
profile_frame = (NATIVE / "src/esp_native_gameplay_frame.c").read_text()
assert "EspAssetPack_mapFlashRuntimeReadMicros(void);" in profile_header
assert "const esp_err_t readResult =" in profile_pack
assert "esp_partition_read(mapFlash.partition, offset, destination, length)" in profile_pack
assert "mapFlashRuntimeReadMicros += (uint32_t)elapsed;" in profile_pack
assert "mapFlashRuntimeReadMicros = 0U;" in profile_pack
assert "flashReadBefore = EspAssetPack_mapFlashRuntimeReadMicros();" in profile_frame
assert "flashReadDuringSprites =" in profile_frame
assert "[SPRITEFLASH] sample=" in profile_frame
assert "spriteFlashProfileFrames < 16U" in profile_frame
assert profile_frame.count("#if defined(ARDUINO_ARCH_ESP32)") >= 4
assert "#endif\n    DRPG_LOGT(\"[SPRITEPROFILE]" in profile_frame

# PR236 second-stage bounded phase breakdown must not log per sprite.
sprite_phase = (NATIVE / "src/esp_native_sprite_renderer.c").read_text()
assert "spritePhaseSampleCount < 16U" in sprite_phase
assert "[SPRITEPHASE] sample=" in sprite_phase
assert "visibilityUs=%u orderUs=%u sourcesUs=%u drawUs=%u" in sprite_phase
assert "profileT1 = esp_timer_get_time();" in sprite_phase
assert "profileT2 = esp_timer_get_time();" in sprite_phase
assert "profileT3 = esp_timer_get_time();" in sprite_phase
assert "profileT4 = esp_timer_get_time();" in sprite_phase
assert "profileT5 = esp_timer_get_time();" in sprite_phase
assert "profileT6 = esp_timer_get_time();" in sprite_phase

# Third-stage probe splits the heavy map-wide scan by pure read-only calls.
# Hardware timings determine whether immutable BSP leaf indexing is warranted.
assert "typedef struct SpriteOrderProbe_s" in sprite_phase
assert "spritePhaseSampleCount < 16U ? &orderProbe : NULL" in sprite_phase
assert "recordUs += (uint32_t)(esp_timer_get_time()" in sprite_phase
assert "leafQueryUs += (uint32_t)(esp_timer_get_time()" in sprite_phase
assert "[SPRITEORDER] sample=" in sprite_phase
assert "postScanUs=%u candidates=%u" in sprite_phase
assert "orderScanDone = esp_timer_get_time();" in sprite_phase

# The immutable BSP leaf sidecar must be bounded, fail closed and never copy
# map-wide textures or mutable topology into the cache.
leaf_visibility = (NATIVE / "src/esp_native_bsp_visibility.c").read_text()
assert "#define MAX_CACHED_SPRITE_LEAVES 1024U" in leaf_visibility
assert "sizeof(spriteLeafCache.leaf) == 2048U" in leaf_visibility
assert "spriteLeafCache.arenaFNV1a != runtime->arenaFNV1a" in leaf_visibility
assert "spriteLeafCache.nodeCount != runtime->nodeCount" in leaf_visibility
assert "spriteLeafCache.spriteCount != runtime->mapSpriteCount" in leaf_visibility
assert "runtime->mapSpriteCount > MAX_CACHED_SPRITE_LEAVES" in leaf_visibility
assert "spriteLeaf(runtime, &sprite, outLeaf)" in leaf_visibility
assert "spriteLeafCache.leaf[spriteIndex] = (uint16_t)leaf;" in leaf_visibility
assert "!cachedSpriteLeaf(runtime, mapSpriteIndex, &leaf)" in leaf_visibility

print("PASS 23 retired Render ABIs; CYD Render_t omits seven raster flags + ten memory ledgers + three frame timestamps + five opaque desktop fields; BSP leaf cache retained")
