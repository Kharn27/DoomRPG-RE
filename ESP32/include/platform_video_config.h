#ifndef DOOMRPG_ESP32_PLATFORM_VIDEO_CONFIG_H
#define DOOMRPG_ESP32_PLATFORM_VIDEO_CONFIG_H

/*
 * Doom RPG renders into a small RGB565 software framebuffer on the CYD.
 * 160x120 maps exactly to the physical 320x240 panel with a 2x nearest-
 * neighbour upscale in both axes.
 *
 * Keep these constants usable from both C engine stubs and C++ platform code.
 */
#define DOOMRPG_LOGICAL_WIDTH 160
#define DOOMRPG_LOGICAL_HEIGHT 120
#define DOOMRPG_PHYSICAL_WIDTH 320
#define DOOMRPG_PHYSICAL_HEIGHT 240
#define DOOMRPG_INTEGER_SCALE 2

/* RES60-P1 runs ONLY the video hardware probe: 160x120 gameplay layout,
 * 160x80 renderers, HUD and their strict guards are intentionally retained.
 * The 192x144 framebuffer is never passed to the 160x120 render engine. */
#ifndef DOOMRPG_RES60_P1_DIAGNOSTIC
#define DOOMRPG_RES60_P1_DIAGNOSTIC 0
#endif
#if DOOMRPG_RES60_P1_DIAGNOSTIC != 0 && DOOMRPG_RES60_P1_DIAGNOSTIC != 1
#error "RES60-P1 diagnostic flag must be 0 or 1"
#endif
/* RES60-P2 experiments with 24/96/24 ownership of the same 192x144
 * allocation. Never admit this flag into production 160x120 gameplay. */
#ifndef DOOMRPG_RES60_P2_LAYOUT
#define DOOMRPG_RES60_P2_LAYOUT 0
#endif
#if DOOMRPG_RES60_P2_LAYOUT != 0 && DOOMRPG_RES60_P2_LAYOUT != 1
#error "RES60-P2 layout flag must be 0 or 1"
#endif
#if DOOMRPG_RES60_P2_LAYOUT && !DOOMRPG_RES60_P1_DIAGNOSTIC
#error "RES60-P2 layout requires isolated RES60 video-only admission"
#endif
/* P3 reads original immutable MAP_INTRO geometry from native PAK for a
 * one-shot flat-shaded 192x96 perspective preview. It MUST NOT start the
 * legacy 160x80 engine or compile into the normal/inverted firmware. */
#ifndef DOOMRPG_RES60_P3_WORLD
#define DOOMRPG_RES60_P3_WORLD 0
#endif
#if DOOMRPG_RES60_P3_WORLD != 0 && DOOMRPG_RES60_P3_WORLD != 1
#error "RES60-P3 world flag must be 0 or 1"
#endif
#if DOOMRPG_RES60_P3_WORLD && (!DOOMRPG_RES60_P2_LAYOUT || !DOOMRPG_RES60_P1_DIAGNOSTIC)
#error "RES60-P3 requires isolated P1/P2 video-only framebuffer and layout"
#endif
/* RES60-P5: original assets into 192x144; 160x120 gameplay is still gated. */
#ifndef DOOMRPG_RES60_P5_TITLE
#define DOOMRPG_RES60_P5_TITLE 0
#endif
#if DOOMRPG_RES60_P5_TITLE != 0 && DOOMRPG_RES60_P5_TITLE != 1
#error "RES60-P5 must be 0 or 1"
#endif
#if DOOMRPG_RES60_P5_TITLE && (!DOOMRPG_RES60_P1_DIAGNOSTIC || !DOOMRPG_RES60_P2_LAYOUT || !DOOMRPG_RES60_P3_WORLD)
#error "RES60-P5 requires isolated P1/P2/P3"
#endif
/* P6: actual native MENU_MAIN graphics + calibrated two-tap zones in 192x144.
 * Starting the 160x120 game remains forbidden; START routes only to P3. */
#ifndef DOOMRPG_RES60_P6_MENU
#define DOOMRPG_RES60_P6_MENU 0
#endif
#if DOOMRPG_RES60_P6_MENU != 0 && DOOMRPG_RES60_P6_MENU != 1
#error "RES60-P6 must be 0 or 1"
#endif
#if DOOMRPG_RES60_P6_MENU && !DOOMRPG_RES60_P5_TITLE
#error "RES60-P6 requires original title P5 in isolated P1/P2/P3 profile"
#endif
/* P7: one read-only native 192x96 camera, four cardinal views, and
 * original skin turn/menu buttons. Strictly gated from 160x120 gameplay. */
#ifndef DOOMRPG_RES60_P7_NAV
#define DOOMRPG_RES60_P7_NAV 0
#endif
#if DOOMRPG_RES60_P7_NAV != 0 && DOOMRPG_RES60_P7_NAV != 1
#error "RES60-P7 must be 0 or 1"
#endif
#if DOOMRPG_RES60_P7_NAV && (!DOOMRPG_RES60_P6_MENU || !DOOMRPG_RES60_P3_WORLD)
#error "RES60-P7 requires P6 and isolated P3 world renderer"
#endif
/* P8: original PAK texture sampler in one 48x48 diagnostic inset.
 * Never claim the 192x96 walls are textured yet; P3 raw FNV preserved. */
#ifndef DOOMRPG_RES60_P8_TEXEL
#define DOOMRPG_RES60_P8_TEXEL 0
#endif
#if DOOMRPG_RES60_P8_TEXEL != 0 && DOOMRPG_RES60_P8_TEXEL != 1
#error "RES60-P8 must be 0 or 1"
#endif
#if DOOMRPG_RES60_P8_TEXEL && (!DOOMRPG_RES60_P7_NAV || !DOOMRPG_RES60_P3_WORLD)
#error "RES60-P8 requires P7 and original P3 world owner"
#endif
/* P9: 1KiB bounded directional broad-phase indices for original flat BSP.
 * Conservative half-plane rejection must not change any original P3 pixel. */
#ifndef DOOMRPG_RES60_P9_WEDGE
#define DOOMRPG_RES60_P9_WEDGE 0
#endif
#if DOOMRPG_RES60_P9_WEDGE != 0 && DOOMRPG_RES60_P9_WEDGE != 1
#error "RES60-P9 flag must be 0 or 1"
#endif
#if DOOMRPG_RES60_P9_WEDGE && (!DOOMRPG_RES60_P7_NAV || !DOOMRPG_RES60_P3_WORLD || !DOOMRPG_RES60_P8_TEXEL)
#error "RES60-P9 requires isolated P7/P8 cardinal BSP probe"
#endif
/* P10: single cached P8 wall material is mapped onto its actual P3/P9
 * projected wall columns; no second full frame or additional PAK reads. */
#ifndef DOOMRPG_RES60_P10_WALL_MATERIAL
#define DOOMRPG_RES60_P10_WALL_MATERIAL 0
#endif
#if DOOMRPG_RES60_P10_WALL_MATERIAL != 0 && DOOMRPG_RES60_P10_WALL_MATERIAL != 1
#error "RES60-P10 flag must be 0 or 1"
#endif
#if DOOMRPG_RES60_P10_WALL_MATERIAL && (!DOOMRPG_RES60_P9_WEDGE || !DOOMRPG_RES60_P8_TEXEL || !DOOMRPG_RES60_P3_WORLD)
#error "RES60-P10 requires isolated P8/P9 material and P3 world owner"
#endif
/* P11 is timing-only, not an optimization. Exactly the P10 RES60 profile
 * may opt in. No profile symbols or timers enter production 160x120. */
#ifndef DOOMRPG_RES60_P11_PROFILE
#define DOOMRPG_RES60_P11_PROFILE 0
#endif
#if DOOMRPG_RES60_P11_PROFILE != 0 && DOOMRPG_RES60_P11_PROFILE != 1
#error "RES60-P11 profile flag must be 0 or 1"
#endif
#if DOOMRPG_RES60_P11_PROFILE && (!DOOMRPG_RES60_P10_WALL_MATERIAL || !DOOMRPG_RES60_P9_WEDGE || !DOOMRPG_RES60_P1_DIAGNOSTIC)
#error "RES60-P11 requires isolated P10/P9 192x144 diagnostic profile"
#endif
/* P12: keep ONLY a verified compact scalar inventory snapshot between
 * cardinal preview rotations; original PAK entry CRC and rebuilt arena FNV
 * must still match on every warm rotation. Opt-in RES60 only. */
#ifndef DOOMRPG_RES60_P12_INVENTORY_CACHE
#define DOOMRPG_RES60_P12_INVENTORY_CACHE 0
#endif
#if DOOMRPG_RES60_P12_INVENTORY_CACHE != 0 && DOOMRPG_RES60_P12_INVENTORY_CACHE != 1
#error "RES60-P12 inventory flag must be 0 or 1"
#endif
#if DOOMRPG_RES60_P12_INVENTORY_CACHE && (!DOOMRPG_RES60_P11_PROFILE || !DOOMRPG_RES60_P10_WALL_MATERIAL || !DOOMRPG_RES60_P1_DIAGNOSTIC)
#error "RES60-P12 requires isolated P11/P10 192x144 preview"
#endif
/* P13: optional session-owned compact immutable map, same sole arena,
 * only on the RES60 preview. The normal game already retains its map. */
#ifndef DOOMRPG_RES60_P13_MAP_LEASE
#define DOOMRPG_RES60_P13_MAP_LEASE 0
#endif
#if DOOMRPG_RES60_P13_MAP_LEASE != 0 && DOOMRPG_RES60_P13_MAP_LEASE != 1
#error "RES60-P13 map-lease flag must be 0 or 1"
#endif
#if DOOMRPG_RES60_P13_MAP_LEASE && (!DOOMRPG_RES60_P12_INVENTORY_CACHE || !DOOMRPG_RES60_P7_NAV || !DOOMRPG_RES60_P1_DIAGNOSTIC)
#error "RES60-P13 requires isolated P12/P7 192x144 preview"
#endif
/* P14: candidate BSP lines decoded once per frame into a single bounded
 * 8KiB static scratch after strict preflight; no second map or framebuffer.
 * Only P13 immutable RES60 preview may skip mid-ray source callbacks. */
#ifndef DOOMRPG_RES60_P14_LINE_SNAPSHOT
#define DOOMRPG_RES60_P14_LINE_SNAPSHOT 0
#endif
#if DOOMRPG_RES60_P14_LINE_SNAPSHOT != 0 && DOOMRPG_RES60_P14_LINE_SNAPSHOT != 1
#error "RES60-P14 line snapshot flag must be 0 or 1"
#endif
#if DOOMRPG_RES60_P14_LINE_SNAPSHOT && (!DOOMRPG_RES60_P13_MAP_LEASE || !DOOMRPG_RES60_P9_WEDGE || !DOOMRPG_RES60_P10_WALL_MATERIAL)
#error "RES60-P14 requires isolated P13/P9/P10 immutable-map preview"
#endif
/* P15: only immutable P14 preview can use compact candidates. */
#ifndef DOOMRPG_RES60_P15_COMPACT_SNAPSHOT
#define DOOMRPG_RES60_P15_COMPACT_SNAPSHOT 0
#endif
#if DOOMRPG_RES60_P15_COMPACT_SNAPSHOT != 0 && DOOMRPG_RES60_P15_COMPACT_SNAPSHOT != 1
#error "RES60-P15 compact snapshot flag must be 0 or 1"
#endif
#if DOOMRPG_RES60_P15_COMPACT_SNAPSHOT && (!DOOMRPG_RES60_P14_LINE_SNAPSHOT || !DOOMRPG_RES60_P13_MAP_LEASE || !DOOMRPG_RES60_P9_WEDGE || !DOOMRPG_RES60_P1_DIAGNOSTIC)
#error "RES60-P15 requires isolated P14/P13/P9 immutable preview"
#endif
/* P16: read-only heap region diagnostics, absolutely no gameplay/renderer
 * change and never compiled into production 160x120. */
#ifndef DOOMRPG_RES60_P16_HEAP_TOPOLOGY
#define DOOMRPG_RES60_P16_HEAP_TOPOLOGY 0
#endif
#if DOOMRPG_RES60_P16_HEAP_TOPOLOGY != 0 && DOOMRPG_RES60_P16_HEAP_TOPOLOGY != 1
#error "RES60-P16 heap topology flag must be 0 or 1"
#endif
#if DOOMRPG_RES60_P16_HEAP_TOPOLOGY && (!DOOMRPG_RES60_P15_COMPACT_SNAPSHOT || !DOOMRPG_RES60_P13_MAP_LEASE || !DOOMRPG_RES60_P11_PROFILE || !DOOMRPG_RES60_P1_DIAGNOSTIC)
#error "RES60-P16 requires isolated P15/P13/P11 immutable preview"
#endif
/* P17 controlled A/B: move only 6144B P15 snapshot BSS into a
 * single bounded owner allocated after the one real framebuffer.
 * Must never compile into production or implicitly allocate per render. */
#ifndef DOOMRPG_RES60_P17_HEAP_SNAPSHOT
#define DOOMRPG_RES60_P17_HEAP_SNAPSHOT 0
#endif
#if DOOMRPG_RES60_P17_HEAP_SNAPSHOT != 0 && DOOMRPG_RES60_P17_HEAP_SNAPSHOT != 1
#error "RES60-P17 heap snapshot flag must be 0 or 1"
#endif
#if DOOMRPG_RES60_P17_HEAP_SNAPSHOT && (!DOOMRPG_RES60_P16_HEAP_TOPOLOGY || !DOOMRPG_RES60_P15_COMPACT_SNAPSHOT || !DOOMRPG_RES60_P13_MAP_LEASE || !DOOMRPG_RES60_P1_DIAGNOSTIC)
#error "RES60-P17 requires isolated P16/P15/P13 preview"
#endif
/* P18: attempt to recover P16 raycast CPU with one frame-local alias to
 * P17's heap snapshot. No extra buffers, allocation, or mutation of BSP. */
#ifndef DOOMRPG_RES60_P18_LOCAL_SNAPSHOT
#define DOOMRPG_RES60_P18_LOCAL_SNAPSHOT 0
#endif
#if DOOMRPG_RES60_P18_LOCAL_SNAPSHOT != 0 && DOOMRPG_RES60_P18_LOCAL_SNAPSHOT != 1
#error "RES60-P18 local snapshot flag must be 0 or 1"
#endif
#if DOOMRPG_RES60_P18_LOCAL_SNAPSHOT && (!DOOMRPG_RES60_P17_HEAP_SNAPSHOT || !DOOMRPG_RES60_P16_HEAP_TOPOLOGY || !DOOMRPG_RES60_P15_COMPACT_SNAPSHOT || !DOOMRPG_RES60_P13_MAP_LEASE || !DOOMRPG_RES60_P1_DIAGNOSTIC)
#error "RES60-P18 requires isolated P17/P16/P15/P13 preview"
#endif
/* P20: reduce only warm RES60 laboratory Serial chatter. No raycast,
 * LCD, map, pointer, buffer, or production 160x120 changes. */
#ifndef DOOMRPG_RES60_P20_COMPACT_SERIAL
#define DOOMRPG_RES60_P20_COMPACT_SERIAL 0
#endif
#if DOOMRPG_RES60_P20_COMPACT_SERIAL != 0 && DOOMRPG_RES60_P20_COMPACT_SERIAL != 1
#error "RES60-P20 compact serial flag must be 0 or 1"
#endif
#if DOOMRPG_RES60_P20_COMPACT_SERIAL && (!DOOMRPG_RES60_P18_LOCAL_SNAPSHOT || !DOOMRPG_RES60_P13_MAP_LEASE || !DOOMRPG_RES60_P11_PROFILE || !DOOMRPG_RES60_P1_DIAGNOSTIC)
#error "RES60-P20 requires isolated P18/P13/P11 preview"
#endif
/* P21: reversible 32-bit bounded ray/segment determinant trial.
 * All coordinates have been preflight-validated in the 2048x2048 map;
 * nearest-hit rational comparisons remain 64-bit. */
#ifndef DOOMRPG_RES60_P21_INTERSECTION32
#define DOOMRPG_RES60_P21_INTERSECTION32 0
#endif
#if DOOMRPG_RES60_P21_INTERSECTION32 != 0 && DOOMRPG_RES60_P21_INTERSECTION32 != 1
#error "RES60-P21 intersection32 flag must be 0 or 1"
#endif
#if DOOMRPG_RES60_P21_INTERSECTION32 && (!DOOMRPG_RES60_P20_COMPACT_SERIAL || !DOOMRPG_RES60_P18_LOCAL_SNAPSHOT || !DOOMRPG_RES60_P15_COMPACT_SNAPSHOT || !DOOMRPG_RES60_P1_DIAGNOSTIC)
#error "RES60-P21 requires isolated P20/P18/P15 preview"
#endif
/* P23: native renderer column scratch may be sized for 192 solely in the
 * isolated RES60 build. This does NOT activate gameplay, change the classic
 * logical geometry, or admit the original sprites/HUD/plane renderer. */
#ifndef DOOMRPG_RES60_P23_NATIVE_RASTER_GEOMETRY
#define DOOMRPG_RES60_P23_NATIVE_RASTER_GEOMETRY 0
#endif
#if DOOMRPG_RES60_P23_NATIVE_RASTER_GEOMETRY != 0 && DOOMRPG_RES60_P23_NATIVE_RASTER_GEOMETRY != 1
#error "RES60-P23 raster geometry flag must be 0 or 1"
#endif
#if DOOMRPG_RES60_P23_NATIVE_RASTER_GEOMETRY && (!DOOMRPG_RES60_P21_INTERSECTION32 || !DOOMRPG_RES60_P18_LOCAL_SNAPSHOT || !DOOMRPG_RES60_P1_DIAGNOSTIC)
#error "RES60-P23 requires isolated P21/P18/P1 preview"
#endif
#if DOOMRPG_RES60_P1_DIAGNOSTIC
#define DOOMRPG_VIDEO_BUFFER_WIDTH 192
#define DOOMRPG_VIDEO_BUFFER_HEIGHT 144
#else
#define DOOMRPG_VIDEO_BUFFER_WIDTH DOOMRPG_LOGICAL_WIDTH
#define DOOMRPG_VIDEO_BUFFER_HEIGHT DOOMRPG_LOGICAL_HEIGHT
#endif
#if DOOMRPG_RES60_P1_DIAGNOSTIC && \
    (DOOMRPG_VIDEO_BUFFER_WIDTH * 5 != DOOMRPG_PHYSICAL_WIDTH * 3 || \
     DOOMRPG_VIDEO_BUFFER_HEIGHT * 5 != DOOMRPG_PHYSICAL_HEIGHT * 3)
#error "RES60-P1 buffer must be exactly 192x144 on a 320x240 CYD"
#endif

/* Permanent classic-CYD logical layout. DoomCanvas does not own mutable
 * geometry on ESP32; the hardware target is fixed at 160x120 logical. */
#define DOOMRPG_CANVAS_X 0
#define DOOMRPG_CANVAS_Y 0
#define DOOMRPG_CANVAS_WIDTH DOOMRPG_LOGICAL_WIDTH
#define DOOMRPG_CANVAS_HEIGHT DOOMRPG_LOGICAL_HEIGHT
#define DOOMRPG_CANVAS_CENTER_X (DOOMRPG_LOGICAL_WIDTH / 2)
#define DOOMRPG_CANVAS_CENTER_Y (DOOMRPG_LOGICAL_HEIGHT / 2)
#define DOOMRPG_HUD_TOP_HEIGHT 20
#define DOOMRPG_HUD_BOTTOM_HEIGHT 20
#define DOOMRPG_VIEWPORT_X 0
#define DOOMRPG_VIEWPORT_Y DOOMRPG_HUD_TOP_HEIGHT
#define DOOMRPG_VIEWPORT_WIDTH DOOMRPG_LOGICAL_WIDTH
#define DOOMRPG_VIEWPORT_HEIGHT \
    (DOOMRPG_LOGICAL_HEIGHT - DOOMRPG_HUD_TOP_HEIGHT - DOOMRPG_HUD_BOTTOM_HEIGHT)

#if DOOMRPG_VIEWPORT_HEIGHT != 80
#error "Classic CYD gameplay viewport must remain 160x80"
#endif

#if DOOMRPG_LOGICAL_WIDTH * DOOMRPG_INTEGER_SCALE != DOOMRPG_PHYSICAL_WIDTH
#error "Doom RPG CYD horizontal scale must remain integer"
#endif

#if DOOMRPG_LOGICAL_HEIGHT * DOOMRPG_INTEGER_SCALE != DOOMRPG_PHYSICAL_HEIGHT
#error "Doom RPG CYD vertical scale must remain integer"
#endif

#endif
